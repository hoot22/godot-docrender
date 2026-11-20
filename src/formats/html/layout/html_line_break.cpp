#include "html_line_break.h"
#include "html_inline_text_fragment.h"
#include "html_punctuation_spacing.h"

#include "formats/html/css/css_litehtml_dom.h"
#include "formats/html/backend/litehtml_container.h"

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <litehtml/document.h>
#include <litehtml/el_text.h>
#include <litehtml/html.h>

#include <algorithm>
#include <iterator>
#include <string_view>

using namespace godot;
using namespace litehtml;

namespace {
	constexpr char32_t ZERO_WIDTH_SPACE = 0x200b;
	constexpr char32_t SOFT_HYPHEN = 0xad;
	constexpr std::u32string_view OPEN_PUNCTUATION = U"〈《「『【〔〖〘〚（［｛｟｢";
	constexpr std::u32string_view CLOSE_PUNCTUATION = U"〉》」』】〕〗〙〛）］｝｠｣、。，．｡､";
	constexpr std::u32string_view ITERATION_MARKS = U"々〻ゝゞヽヾ";
	constexpr std::u32string_view CJK_HYPHENS = U"〜゠";
	constexpr std::u32string_view LOOSE_HYPHENS = U"‐–";
	constexpr std::u32string_view CENTERED_PUNCTUATION = U"・：；･‼⁇⁈⁉！？";

	enum class LineGroup {
		other, ideograph, conditional_kana, inseparable, east_asian_prefix, east_asian_suffix,
		prefix, suffix, word_unit, southeast_asian, forbid_after, forbid_before, quotation, glue, combining, inseparable_pair,
	};

	LineGroup unicode_line_group(char32_t p_character) {
		struct Range {
			char32_t first;
			char32_t last;
			LineGroup group;
		};
		static constexpr Range ranges[] = {
			#include "unicode_line_break.inc"
		};
		const auto* match = std::lower_bound(std::begin(ranges), std::end(ranges), p_character,
			[](const Range& p_range, char32_t p_value) { return p_range.last < p_value; });
		return match != std::end(ranges) && match->first <= p_character ? match->group : LineGroup::other;
	}

	bool includes(std::u32string_view p_group, char32_t p_character) {
		return p_group.find(p_character) != std::u32string_view::npos;
	}

	bool cjk_letter(char32_t p_character) {
		static constexpr std::pair<char32_t, char32_t> ranges[] = {
			{ 0x1100, 0x11ff }, { 0x3041, 0x3096 }, { 0x30a1, 0x30fa }, { 0x3105, 0x312f },
			{ 0x3131, 0x318e }, { 0x31a0, 0x31bf }, { 0x31f0, 0x31ff }, { 0x3400, 0x4dbf },
			{ 0x4e00, 0x9fff }, { 0xa960, 0xa97c }, { 0xac00, 0xd7fb }, { 0xf900, 0xfaff },
			{ 0xff66, 0xff9d }, { 0x1aff0, 0x1b16f }, { 0x20000, 0x323af },
		};
		const auto* range = std::lower_bound(std::begin(ranges), std::end(ranges), p_character,
			[](const auto& p_range, char32_t p_value) { return p_range.second < p_value; });
		return range != std::end(ranges) && range->first <= p_character;
	}

	bool isolate_at_grapheme(char32_t p_character) {
		if (p_character < 0x80) return false;
		if (cjk_letter(p_character) || is_fullwidth_spacing_punctuation(p_character) || includes(OPEN_PUNCTUATION, p_character) ||
			includes(CLOSE_PUNCTUATION, p_character) || includes(ITERATION_MARKS, p_character) ||
			includes(CJK_HYPHENS, p_character) || includes(LOOSE_HYPHENS, p_character) ||
			includes(CENTERED_PUNCTUATION, p_character) || p_character == ZERO_WIDTH_SPACE) return true;
		const auto group = unicode_line_group(p_character);
		return group == LineGroup::conditional_kana || group == LineGroup::inseparable ||
			group == LineGroup::east_asian_prefix || group == LineGroup::east_asian_suffix;
	}

	bool uses_chinese_or_japanese_rules(const element& p_element) {
		const char* attribute = get_element_language(p_element);
		if (!attribute || !*attribute) return false;
		const string language = lowcase(attribute);
		std::string_view remaining(language);
		size_t end = remaining.find('-');
		const auto primary = remaining.substr(0, end);
		while (end != std::string_view::npos) {
			remaining.remove_prefix(end + 1);
			end = remaining.find('-');
			const auto subtag = remaining.substr(0, end);
			const bool letters = std::all_of(subtag.begin(), subtag.end(), [](char c) { return c >= 'a' && c <= 'z'; });
			if (letters && subtag.size() == 4) {
				return subtag == "hant" || subtag == "hans" || subtag == "hani" || subtag == "hanb" || subtag == "bopo" ||
					subtag == "jpan" || subtag == "hrkt" || subtag == "hira" || subtag == "kana";
			}
			// A script follows the language and any extlang, before the region or extensions.
			if (!letters || subtag.size() != 3) break;
		}
		return primary == "ja" || primary == "zh" || primary == "cmn" || primary == "yue" ||
			primary == "nan" || primary == "hak" || primary == "wuu";
	}

	bool word_character(char32_t p_character) {
		if (p_character < 128) {
			return (p_character >= U'A' && p_character <= U'Z') ||
				(p_character >= U'a' && p_character <= U'z') ||
				(p_character >= U'0' && p_character <= U'9') || p_character == U'_';
		}
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		return cjk_letter(p_character) || (server.is_valid() && server->is_valid_letter(p_character));
	}

	char32_t front_character(std::string_view p_text) {
		if (p_text.empty()) return 0;
		const auto first = static_cast<unsigned char>(p_text.front());
		if (first < 128) return first;
		const int length = first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
		if (p_text.size() < static_cast<size_t>(length)) return 0;
		return String::utf8(p_text.data(), length)[0];
	}

	char32_t back_character(std::string_view p_text) {
		while (!p_text.empty()) {
			size_t start = p_text.size() - 1;
			while (start > 0 && (static_cast<unsigned char>(p_text[start]) & 0xc0) == 0x80) --start;
			const char32_t character = front_character(p_text.substr(start));
			const bool extends_cjk_cluster = (character >= 0xfe00 && character <= 0xfe0f) ||
				(character >= 0xe0100 && character <= 0xe01ef) || character == 0x3099 || character == 0x309a;
			const bool combining = character >= 0x80 && unicode_line_group(character) == LineGroup::combining;
			if ((!extends_cjk_cluster && !combining) || start == 0) return character;
			p_text = p_text.substr(0, start);
		}
		return 0;
	}

	bool separator(char32_t p_character) {
		return p_character == U' ' || p_character == U'\t' || p_character == U'\n' || p_character == U'\r' ||
			is_unicode_forced_break(p_character) || is_css_space_separator(p_character);
	}
}

bool litehtml::is_explicit_wrap_point(const element& p_element) {
	static const auto wbr = _id("wbr");
	if (p_element.tag() == wbr) return true;
	const auto* text = dynamic_cast<const el_text*>(&p_element);
	return text && text->get_source_text() == "\xe2\x80\x8b";
}

bool litehtml::is_soft_hyphen(const element& p_element) {
	const auto* text = dynamic_cast<const el_text*>(&p_element);
	return text && text->get_source_text() == "\xc2\xad";
}

bool litehtml::is_hyphenation_fallback(const element& p_element) {
	const auto* fragment = dynamic_cast<const dockit::HTMLInlineTextFragment*>(&p_element);
	return fragment && fragment->is_hyphenation_fallback();
}

pixel_t litehtml::measure_hyphen(const element& p_element) {
	const auto owner = p_element.parent();
	if (!owner || !owner->css().get_font()) return 0;
	const auto& style = owner->css();
	return p_element.get_document()->container()->text_width(style.get_hyphenate_character().c_str(), style.get_font());
}

bool litehtml::is_lexical_character(char32_t p_character) {
	return p_character >= 0x80 && unicode_line_group(p_character) == LineGroup::southeast_asian;
}

bool litehtml::extends_lexical_character(char32_t p_character) {
	return p_character == 0x200d || p_character == 0x200c ||
		(p_character >= 0x80 && unicode_line_group(p_character) == LineGroup::combining);
}

bool litehtml::is_grapheme_boundary(const element& p_left, const element& p_right) {
	const auto* left = dynamic_cast<const el_text*>(&p_left);
	const auto* right = dynamic_cast<const el_text*>(&p_right);
	if (!left || !right) return true;
	const auto& left_text = left->get_rendered_text();
	const auto& right_text = right->get_rendered_text();
	if (left_text.empty() || right_text.empty()) return false;
	if (static_cast<unsigned char>(left_text.back()) < 0x80 &&
		static_cast<unsigned char>(right_text.front()) < 0x80) return true;
	const auto* left_fragment = dynamic_cast<const dockit::HTMLInlineTextFragment*>(left);
	const auto* right_fragment = dynamic_cast<const dockit::HTMLInlineTextFragment*>(right);
	if (left_fragment && right_fragment && left_fragment->original_renderer() == right_fragment->original_renderer()) return true;
	const auto server = TextServerManager::get_singleton()->get_primary_interface();
	if (server.is_null()) return false;
	const String prefix = String::utf8(left_text.c_str());
	const auto breaks = server->string_get_character_breaks(prefix + String::utf8(right_text.c_str()));
	if (breaks.is_empty()) return false;
	const auto* boundary = std::lower_bound(breaks.ptr(), breaks.ptr() + breaks.size(), prefix.length());
	return boundary != breaks.ptr() + breaks.size() && *boundary == prefix.length();
}

bool litehtml::allows_text_boundary_wrap(const element& p_left, const element& p_right, const css_properties& p_style) {
	const auto* left = dynamic_cast<const el_text*>(&p_left);
	const auto* right = dynamic_cast<const el_text*>(&p_right);
	if (!left || !right) return true;

	const auto word_break_mode = p_style.get_word_break();
	const bool break_all = word_break_mode == word_break_break_all;
	if (break_all && !is_grapheme_boundary(p_left, p_right)) return false;
	const char32_t before = back_character(left->get_rendered_text());
	const char32_t after = front_character(right->get_rendered_text());
	if (includes(OPEN_PUNCTUATION, before) || includes(CLOSE_PUNCTUATION, after) ||
		before == 0x2060 || after == 0x2060 || before == 0xfeff || after == 0xfeff) return false;

	const auto mode = p_style.get_line_break();
	const bool loose = mode == line_break_loose;
	const auto left_group = break_all || before >= 0x80 ? unicode_line_group(before) : LineGroup::other;
	const auto right_group = break_all || after >= 0x80 ? unicode_line_group(after) : LineGroup::other;
	const auto is_word_unit = [](LineGroup group) {
		return group == LineGroup::word_unit || group == LineGroup::southeast_asian;
	};
	if (break_all && (left_group == LineGroup::forbid_after || left_group == LineGroup::quotation ||
		(right_group == LineGroup::quotation && !p_left.is_space()) ||
		left_group == LineGroup::glue || right_group == LineGroup::glue || before == 0x200d ||
		(left_group == LineGroup::inseparable_pair && right_group == LineGroup::inseparable_pair))) return false;

	if (includes(CJK_HYPHENS, after)) return mode != line_break_strict && uses_chinese_or_japanese_rules(p_right);
	if (includes(LOOSE_HYPHENS, after)) {
		return loose && (left_group == LineGroup::ideograph || (break_all && is_word_unit(left_group)));
	}
	if (right_group == LineGroup::conditional_kana || includes(ITERATION_MARKS, after)) return loose;
	if (right_group == LineGroup::inseparable && !p_left.is_space() && !p_left.is_other_space_separator()) {
		return loose && left_group == LineGroup::inseparable;
	}
	if (includes(CENTERED_PUNCTUATION, after) || right_group == LineGroup::east_asian_suffix) {
		return loose && uses_chinese_or_japanese_rules(p_right);
	}
	if (break_all && right_group == LineGroup::forbid_before) return false;
	if (left_group == LineGroup::east_asian_prefix) return loose && uses_chinese_or_japanese_rules(p_left);

	if (break_all) {
		const bool left_ideograph = is_word_unit(left_group) || left_group == LineGroup::ideograph || cjk_letter(before);
		const bool right_ideograph = is_word_unit(right_group) || right_group == LineGroup::ideograph || cjk_letter(after);
		if ((left_group == LineGroup::prefix && right_ideograph) || (left_ideograph && right_group == LineGroup::suffix)) return false;
		if (left_ideograph || right_ideograph) return true;
	}
	else {
		const auto* fragment = dynamic_cast<const dockit::HTMLInlineTextFragment*>(right);
		if (fragment && fragment->has_lexical_break_before()) return word_break_mode != word_break_manual;
		if ((right_group == LineGroup::southeast_asian && extends_lexical_character(before)) ||
			(left_group == LineGroup::southeast_asian && extends_lexical_character(after))) return false;
	}
	if (cjk_letter(before) || cjk_letter(after)) {
		return word_break_mode != word_break_keep_all || !word_character(before) || !word_character(after);
	}
	// Keep quotation marks and interpuncts that are also used inside Latin words attached to their text.
	constexpr std::u32string_view WORD_PUNCTUATION = U"‘’“”·‧";
	if ((includes(WORD_PUNCTUATION, before) && word_character(after)) ||
		(includes(WORD_PUNCTUATION, after) && word_character(before))) return false;
	return !(left_group == LineGroup::southeast_asian || word_character(before)) ||
		!(right_group == LineGroup::southeast_asian || word_character(after));
}

void dockit::LiteHTMLContainer::split_text(const char* p_text, const std::function<void(const char*)>& p_word,
	const std::function<void(const char*)>& p_space) {
	const String text = String::utf8(p_text);
	if (text.is_empty()) return;
	const bool needs_clusters = std::any_of(text.ptr(), text.ptr() + text.length(), isolate_at_grapheme);
	PackedInt32Array breaks;
	if (needs_clusters) {
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		if (server.is_valid()) breaks = server->string_get_character_breaks(text);
	}
	const auto emit = [&](int64_t p_start, int64_t p_end, const std::function<void(const char*)>& p_callback) {
		if (p_start == p_end) return;
		const CharString utf8 = text.substr(p_start, p_end - p_start).utf8();
		p_callback(utf8.get_data());
	};
	int64_t start = 0;
	int64_t next_break = 0;
	for (int64_t index = 0; index < text.length();) {
		const char32_t character = text[index];
		const bool space = separator(character);
		const bool soft_hyphen = character == SOFT_HYPHEN;
		const bool isolate = soft_hyphen || isolate_at_grapheme(character);
		int64_t end = index + 1;
		if (isolate && !space && character != ZERO_WIDTH_SPACE && !soft_hyphen) {
			while (next_break < breaks.size() && breaks[next_break] <= index) ++next_break;
			if (next_break < breaks.size()) end = breaks[next_break];
		}
		if (space || isolate) {
			emit(start, index, p_word);
			emit(index, end, space ? p_space : p_word);
			start = end;
		}
		index = end;
	}
	emit(start, text.length(), p_word);
}
