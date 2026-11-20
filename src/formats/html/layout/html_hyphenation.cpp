#include "html_hyphenation.h"

#include "html_hyphenation_dictionary.h"
#include "html_line_break.h"

#include "formats/html/css/css_litehtml_dom.h"

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <litehtml/document_container.h>
#include <litehtml/el_text.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		bool counts_for_hyphenation(char32_t p_character) {
			struct Range { char32_t first; char32_t last; };
			static constexpr Range ignored[] = {
				#include "unicode_hyphenation_skip.inc"
			};
			const auto* range = std::lower_bound(std::begin(ignored), std::end(ignored), p_character,
				[](const Range& p_range, char32_t p_value) { return p_range.last < p_value; });
			return range == std::end(ignored) || p_character < range->first;
		}

		enum class WordClass {
			other, letter, hebrew, number, katakana, ignored, connector,
			letter_punctuation, number_punctuation, shared_punctuation, hebrew_quote,
		};

		WordClass word_class(char32_t p_character) {
			struct Range { char32_t first; char32_t last; WordClass kind; };
			static constexpr Range classes[] = {
				#include "unicode_hyphenation_word_break.inc"
			};
			const auto* range = std::lower_bound(std::begin(classes), std::end(classes), p_character,
				[](const Range& p_range, char32_t p_value) { return p_range.last < p_value; });
			return range != std::end(classes) && range->first <= p_character ? range->kind : WordClass::other;
		}

		struct WordSpan {
			int64_t start;
			int64_t end;
		};

		bool joins_inside_word(const std::u32string& p_text, WordSpan p_left, WordSpan p_right) {
			if (p_left.end >= p_right.start) return false;
			WordClass punctuation = WordClass::ignored;
			for (int64_t index = p_left.end; index < p_right.start; ++index) {
				const auto type = word_class(p_text[index]);
				if (type == WordClass::ignored) continue;
				if (punctuation != WordClass::ignored && (punctuation != WordClass::connector || type != WordClass::connector)) return false;
				punctuation = type;
			}
			if (punctuation == WordClass::ignored || punctuation == WordClass::other) return false;

			WordClass before = WordClass::ignored;
			for (int64_t index = p_left.end; index > p_left.start && before == WordClass::ignored;) before = word_class(p_text[--index]);
			WordClass after = WordClass::ignored;
			for (int64_t index = p_right.start; index < p_right.end && after == WordClass::ignored; ++index) after = word_class(p_text[index]);
			const auto letter = [](WordClass kind) { return kind == WordClass::letter || kind == WordClass::hebrew; };
			switch (punctuation) {
				case WordClass::connector: {
					const auto adjoining = [&](WordClass kind) { return letter(kind) || kind == WordClass::number || kind == WordClass::katakana; };
					return adjoining(before) && adjoining(after);
				}
				case WordClass::letter_punctuation:
					return letter(before) && letter(after);
				case WordClass::number_punctuation:
					return before == WordClass::number && after == WordClass::number;
				case WordClass::shared_punctuation:
					return (letter(before) && letter(after)) || (before == WordClass::number && after == WordClass::number);
				case WordClass::hebrew_quote:
					return before == WordClass::hebrew && after == WordClass::hebrew;
				default:
					return false;
			}
		}

		std::vector<WordSpan> hyphenation_words(const std::u32string& p_text, const PackedInt32Array& p_boundaries) {
			std::vector<WordSpan> words;
			for (int64_t index = 0; index + 1 < p_boundaries.size(); index += 2) {
				const WordSpan next{ p_boundaries[index], p_boundaries[index + 1] };
				if (next.start < 0 || next.end <= next.start || next.end > static_cast<int64_t>(p_text.size())) continue;
				// TextServer imposes punctuation breaks on the ICU result. Rejoin only the gaps allowed by UAX #29.
				if (!words.empty() && joins_inside_word(p_text, words.back(), next)) words.back().end = next.end;
				else words.push_back(next);
			}
			return words;
		}

		struct SoftHyphen {
			el_text* node;
			int64_t offset;
		};

		struct SourceSpan {
			render_item* item;
			int64_t start;
			int64_t end;
		};

		bool can_hyphenate(const css_properties& p_style) {
			return p_style.get_hyphens() != hyphens_none && p_style.get_text_wrap_mode() == text_wrap_wrap &&
				p_style.get_word_break() != word_break_break_all && p_style.get_line_break() != line_break_anywhere;
		}

		const HyphenationPatternData* dictionaries_for_word(const std::vector<SourceSpan>& p_spans, WordSpan p_word) {
			const HyphenationPatternData* patterns = nullptr;
			bool auto_enabled = false;
			for (const auto& span : p_spans) {
				if (span.end <= p_word.start) continue;
				if (span.start >= p_word.end) break;
				const auto& source = *span.item->src_el();
				const char* language = get_element_language(source);
				const auto* candidate = language ? hyphenation_patterns_for_language(language) : nullptr;
				if (!candidate || (patterns && patterns != candidate)) return nullptr;
				patterns = candidate;
				auto_enabled |= source.css().get_hyphens() == hyphens_auto && can_hyphenate(source.css());
			}
			return auto_enabled ? patterns : nullptr;
		}

		pixel_t span_width(const String& p_spelling, const std::vector<SourceSpan>& p_spans, int64_t p_begin, int64_t p_end) {
			pixel_t width = 0;
			for (const auto& span : p_spans) {
				const auto start = std::max(span.start, p_begin);
				const auto end = std::min(span.end, p_end);
				const auto& source = *span.item->src_el();
				if (start < end && source.css().get_font()) {
					const auto text = p_spelling.substr(start, end - start).utf8();
					width += source.get_document()->container()->text_width(text.get_data(), source.css().get_font());
				}
			}
			return width;
		}

		void add_dictionary_breaks(const HyphenationPatternData& p_patterns, WordSpan p_word,
			const std::u32string& p_text, const String& p_spelling, const std::vector<SourceSpan>& p_spans,
			const PackedInt32Array& p_graphemes, const std::vector<SoftHyphen>& p_markers, size_t p_first_marker,
			size_t p_past_last_marker, pixel_t p_line_width, HTMLAutomaticHyphens& r_automatic) {
			const bool fallback = p_first_marker != p_past_last_marker;
			if (fallback && p_line_width <= 0) return;
			const auto cuts = automatic_hyphenation_points(p_patterns,
				std::u32string_view(p_text.data() + p_word.start, p_word.end - p_word.start));
			if (cuts.empty()) return;
			std::vector<SoftHyphen> authored;
			for (auto index = p_first_marker; index < p_past_last_marker; ++index) {
				const auto& marker = p_markers[index];
				if (marker.node->allows_hyphenation() && can_hyphenate(marker.node->css())) authored.push_back(marker);
			}
			size_t span = 0;
			size_t interval = 0;
			int64_t measured_end = -1;
			bool segment_overflows = false;
			for (const auto offset : cuts) {
				const auto point = p_word.start + offset;
				if (p_graphemes.is_empty() || !std::binary_search(p_graphemes.ptr(), p_graphemes.ptr() + p_graphemes.size(), point)) continue;
				while (span < p_spans.size() && point > p_spans[span].end) ++span;
				if (span == p_spans.size() || point <= p_spans[span].start) continue;
				const auto& source = *p_spans[span].item->src_el();
				if (source.css().get_hyphens() != hyphens_auto || !can_hyphenate(source.css())) continue;
				const auto owner = source.parent();
				const auto prefix = std::count_if(p_text.begin() + p_word.start, p_text.begin() + point, counts_for_hyphenation);
				const auto suffix = std::count_if(p_text.begin() + point, p_text.begin() + p_word.end, counts_for_hyphenation);
				if (!owner || !owner->css().get_hyphenation_chars().allows(prefix, suffix)) continue;
				if (fallback) {
					while (interval < authored.size() && authored[interval].offset < point) ++interval;
					if (interval < authored.size() && authored[interval].offset == point) continue;
					const auto end = interval == authored.size() ? p_word.end : authored[interval].offset;
					if (end != measured_end) {
						const auto start = interval == 0 ? p_word.start : authored[interval - 1].offset;
						const auto suffix_width = interval == authored.size() ? 0 : measure_hyphen(*authored[interval].node);
						segment_overflows = span_width(p_spelling, p_spans, start, end) + suffix_width > p_line_width;
						measured_end = end;
					}
					if (!segment_overflows) continue;
				}
				r_automatic[p_spans[span].item].push_back({ point - p_spans[span].start, fallback });
			}
		}
	}

	void prepare_hyphenation(const std::vector<render_item*>& p_text, pixel_t p_line_width, HTMLAutomaticHyphens& r_automatic) {
		std::u32string text;
		std::vector<SoftHyphen> markers;
		std::vector<SourceSpan> spans;
		for (auto* item : p_text) {
			auto* node = static_cast<el_text*>(item->src_el().get());
			if (is_soft_hyphen(*node)) {
				node->set_hyphenation_allowed(false);
				markers.push_back({ node, static_cast<int64_t>(text.size()) });
			}
			else {
				const String content = String::utf8(node->get_rendered_text().c_str());
				if (content.is_empty()) continue;
				const auto start = static_cast<int64_t>(text.size());
				text.append(content.ptr(), content.length());
				spans.push_back({ item, start, static_cast<int64_t>(text.size()) });
			}
		}
		if (text.empty()) return;
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		if (server.is_null()) return;
		// Segment the visible spelling, with U+00AD left out of the input. The marker offsets still address this string.
		const char* language = get_element_language(*p_text.front()->src_el());
		const String locale = language ? String::utf8(language).replace("-", "_") : String();
		const String spelling(text.c_str());
		const auto words = hyphenation_words(text, server->string_get_word_breaks(spelling, locale));
		const auto graphemes = server->string_get_character_breaks(spelling);
		size_t marker = 0;
		for (const auto& [start, end] : words) {
			while (marker < markers.size() && markers[marker].offset <= start) ++marker;
			const auto first_marker = marker;
			const auto total = marker < markers.size() && markers[marker].offset < end ?
				std::count_if(text.begin() + start, text.begin() + end, counts_for_hyphenation) : 0;
			int64_t preceding = 0;
			auto cursor = text.begin() + start;
			for (; marker < markers.size() && markers[marker].offset < end; ++marker) {
				const auto& candidate = markers[marker];
				const auto cut = text.begin() + candidate.offset;
				preceding += std::count_if(cursor, cut, counts_for_hyphenation);
				cursor = cut;
				if (!graphemes.is_empty() && !std::binary_search(graphemes.ptr(), graphemes.ptr() + graphemes.size(), candidate.offset)) continue;
				const auto owner = candidate.node->parent();
				if (owner) candidate.node->set_hyphenation_allowed(owner->css().get_hyphenation_chars().allows(preceding, total - preceding));
			}
			const WordSpan word{ start, end };
			if (const auto* patterns = dictionaries_for_word(spans, word)) {
				add_dictionary_breaks(*patterns, word, text, spelling, spans, graphemes, markers, first_marker, marker, p_line_width, r_automatic);
			}
		}
	}
}
