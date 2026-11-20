#include "html_punctuation_spacing.h"

#include "html_line_break.h"
#include "html_text_autospace.h"
#include "formats/html/css/css_litehtml_dom.h"

#include <litehtml/document_container.h>
#include <litehtml/el_text.h>
#include <litehtml/line_box.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <string_view>

namespace litehtml {
	namespace {
		using Kind = spacing_punctuation;
		constexpr std::u32string_view WIDE_OPEN = U"〈《「『【〔〖〘〚（［｛｟‘“";
		constexpr std::u32string_view WIDE_CLOSE = U"〉》」』】〕〗〙〛）］｝｠’”";
		constexpr std::u32string_view MIDDLE = U"·‧・";
		constexpr std::u32string_view WIDE_COLONS = U"：；";
		constexpr std::u32string_view WIDE_DOTS = U"、。，．";
		constexpr std::u32string_view NARROW_OPEN = U"([{༺༼᚛‚„⁅⁽₍⌈⌊〈❨❪❬❮❰❲❴⟅⟦⟨⟪⟬⟮⦃⦅⦇⦉⦋⦍⦏⦑⦓⦕⦗⧘⧚⧼⸢⸤⸦⸨⹕⹗⹙⹛﴿︗︵︷︹︻︽︿﹁﹃﹇﹙﹛﹝｢";
		constexpr std::u32string_view NARROW_CLOSE = U")]}༻༽᚜⁆⁾₎⌉⌋〉❩❫❭❯❱❳❵⟆⟧⟩⟫⟭⟯⦄⦆⦈⦊⦌⦎⦐⦒⦔⦖⦘⧙⧛⧽⸣⸥⸧⸩⹖⹘⹚⹜﴾︘︶︸︺︼︾﹀﹂﹄﹈﹚﹜﹞｣";

		bool includes(std::u32string_view p_values, char32_t p_value) {
			return p_values.find(p_value) != std::u32string_view::npos;
		}

		Kind punctuation_kind(char32_t p_character, const element& p_owner) {
			if (includes(WIDE_OPEN, p_character)) return Kind::opening;
			if (includes(WIDE_CLOSE, p_character)) return Kind::closing;
			if (includes(MIDDLE, p_character)) return Kind::middle;
			if (p_character == 0x3000) return Kind::ideographic_space;
			if (includes(WIDE_COLONS, p_character) || includes(WIDE_DOTS, p_character)) {
				static const string_vector traditional{ "zh-Hant", "zh-TW", "zh-HK", "zh-MO", "yue" };
				static const string_vector chinese{ "zh", "cmn" };
				const bool trad = matches_element_language(p_owner, traditional);
				return (trad || (includes(WIDE_COLONS, p_character) && !matches_element_language(p_owner, chinese))) ?
					Kind::middle : Kind::closing;
			}
			if (includes(NARROW_OPEN, p_character)) return Kind::narrow_opening;
			if (includes(NARROW_CLOSE, p_character)) return Kind::narrow_closing;
			return Kind::other;
		}

		bool skips_for_spacing(const line_box_item& p_item) {
			return p_item.get_el()->skip() || p_item.width() == 0;
		}
	}

	bool is_fullwidth_spacing_punctuation(char32_t p_character) {
		return includes(WIDE_OPEN, p_character) || includes(WIDE_CLOSE, p_character) || includes(MIDDLE, p_character) ||
			includes(WIDE_COLONS, p_character) || includes(WIDE_DOTS, p_character);
	}

	bool is_punctuation_variation_selector(char32_t p_character) {
		return (p_character >= 0xfe00 && p_character <= 0xfe0f) ||
			(p_character >= 0xe0100 && p_character <= 0xe01ef);
	}

	punctuation_spacing get_punctuation_spacing(const element& p_element, bool p_trailing, pixel_t p_untrimmed_width) {
		const auto* text = dynamic_cast<const el_text*>(&p_element);
		if (!text || p_untrimmed_width <= 0 || text->get_rendered_text().empty()) return {};
		const auto owner = text->parent();
		if (!owner) return {};
		const auto& source = text->get_rendered_text();
		const char byte = p_trailing ? source.back() : source.front();
		// Outside punctuation and U+3000, ASCII boundaries are by far the common case.
		if (static_cast<unsigned char>(byte) < 128 && !includes(p_trailing ? NARROW_OPEN : NARROW_CLOSE, byte)) return {};
		const auto content = godot::String::utf8(source.c_str());
		if (content.is_empty()) return {};
		int64_t boundary = p_trailing ? content.length() - 1 : 0;
		while (boundary > 0 && is_punctuation_variation_selector(content[boundary])) --boundary;
		const auto kind = punctuation_kind(content[boundary], *owner);
		const auto& style = owner->css();
		punctuation_spacing result;
		result.kind = kind;
		result.flags = style.get_text_spacing_trim();
		result.font_size = style.get_font_metrics().font_size;
		const bool single_glyph = std::all_of(content.ptr() + 1, content.ptr() + content.length(), is_punctuation_variation_selector);
		if (result.flags && single_glyph && style.get_font()) {
			result.trim = kind == Kind::opening ? punctuation_trim::left :
				kind == Kind::closing ? punctuation_trim::right :
				kind == Kind::middle && (result.flags & text_spacing_all) ? punctuation_trim::center : punctuation_trim::none;
			if (result.trim != punctuation_trim::none) {
				const auto width = owner->get_document()->container()->trimmed_punctuation_width(source.c_str(), style.get_font(), result.trim);
				result.reduction = std::clamp(p_untrimmed_width - width, pixel_t(0), p_untrimmed_width);
			}
		}
		return result;
	}

	bool punctuation_spacing::at_start(bool p_paragraph_start) const {
		return kind == Kind::opening &&
			((flags & text_spacing_start) || ((flags & text_spacing_first) && !p_paragraph_start));
	}

	bool punctuation_spacing::before(const punctuation_spacing& p_next) const {
		return kind == Kind::closing && (flags & text_spacing_adjacent) &&
			(is_one_of(p_next.kind, Kind::closing, Kind::middle, Kind::ideographic_space, Kind::narrow_closing) ||
				(p_next.kind == Kind::opening && p_next.font_size > font_size));
	}

	bool punctuation_spacing::after(const punctuation_spacing& p_previous) const {
		return kind == Kind::opening && (flags & text_spacing_adjacent) &&
			(is_one_of(p_previous.kind, Kind::opening, Kind::middle, Kind::ideographic_space, Kind::narrow_opening) ||
				(p_previous.kind == Kind::closing && p_previous.font_size >= font_size));
	}

	line_box::punctuation_update line_box::punctuation_before(const line_box_item& p_item) const {
		punctuation_update result;
		if (p_item.get_type() != line_box_item::type_text_part || skips_for_spacing(p_item)) return result;
		const auto& source = *p_item.get_el()->src_el();
		if (source.is_break() || source.is_white_space()) return result;
		const auto incoming = get_punctuation_spacing(source, false, p_item.untrimmed_width());
		bool separates = false;
		bool at_start = true;
		for (auto part = m_items.rbegin(); part != m_items.rend(); ++part) {
			auto& earlier = **part;
			if (earlier.get_type() != line_box_item::type_text_part) {
				if (earlier.get_type() != line_box_item::type_inline_continue) {
					separates |= separates_autospace(*earlier.get_el(), earlier.get_type() == line_box_item::type_inline_start, m_right - m_left);
				}
				continue;
			}
			if (skips_for_spacing(earlier)) continue;
			at_start = false;
			if (!separates) {
				const auto previous = get_punctuation_spacing(*earlier.get_el()->src_el(), true, earlier.untrimmed_width());
				if (previous.before(incoming) && previous.reduction > 0) {
					result.previous = &earlier;
					result.previous_reduction = previous.reduction;
				}
				if (incoming.after(previous)) result.next_reduction = incoming.reduction;
			}
			break;
		}
		if ((incoming.flags & text_spacing_all) || (at_start && !separates && incoming.at_start(m_paragraph_start))) {
			result.next_reduction = incoming.reduction;
		}
		if (incoming.kind == Kind::closing && (incoming.flags & (text_spacing_end | text_spacing_allow_end))) {
			result.possible_end_reduction = incoming.reduction;
		}
		result.next_trim = incoming.trim;
		return result;
	}

	void line_box::apply_punctuation_trim(line_box_item& p_item, punctuation_trim p_trim, pixel_t p_reduction) {
		const auto old_width = p_item.width();
		p_item.set_punctuation_reduction(p_trim, p_reduction);
		const auto change = p_item.width() - old_width;
		if (change == 0) return;
		m_width += change;
		for (auto part = m_items.rbegin(); part != m_items.rend(); ++part) {
			if (part->get() == &p_item) break;
			(*part)->pos().x += change;
		}
	}

	void line_box::trim_line_end(pixel_t p_hanging_width) {
		for (auto part = m_items.rbegin(); part != m_items.rend(); ++part) {
			auto& item = **part;
			if (item.get_type() != line_box_item::type_text_part || skips_for_spacing(item)) continue;
			const auto& source = *item.get_el()->src_el();
			if (source.is_break() || source.is_white_space() || is_hanging_space(source)) continue;
			const auto punctuation = get_punctuation_spacing(source, true, item.untrimmed_width());
			if (punctuation.kind == Kind::closing && !(punctuation.flags & text_spacing_all)) {
				const bool overflows = m_left + m_width + item.punctuation_reduction() - p_hanging_width > m_right;
				const bool trims = (punctuation.flags & text_spacing_end) || ((punctuation.flags & text_spacing_allow_end) && overflows);
				apply_punctuation_trim(item, punctuation.trim, trims ? punctuation.reduction : 0);
			}
			break;
		}
	}
}
