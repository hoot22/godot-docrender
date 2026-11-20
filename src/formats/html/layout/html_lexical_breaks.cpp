#include "html_lexical_breaks.h"

#include "html_hyphenation.h"
#include "html_inline_text_fragment.h"
#include "html_line_break.h"

#include "formats/html/css/css_litehtml_dom.h"

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <litehtml/iterators.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <string>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		enum class DictionaryScript { thai, lao, khmer, myanmar, unknown };

		struct DictionaryProbe {
			const char* locale;
			const char* text;
		};

		constexpr DictionaryProbe DICTIONARIES[] = {
			{ "th", "ภาษาไทยสวยงามมาก" },
			{ "lo", "ພາສາລາວເປັນພາສາ" },
			{ "km", "ភាសាខ្មែរជាភាសាមួយ" },
			{ "my", "မြန်မာဘာသာစကား" },
		};

		DictionaryScript dictionary_script(char32_t p_character) {
			if (p_character >= 0x0e00 && p_character <= 0x0e7f) return DictionaryScript::thai;
			if (p_character >= 0x0e80 && p_character <= 0x0eff) return DictionaryScript::lao;
			if (p_character >= 0x1780 && p_character <= 0x17ff) return DictionaryScript::khmer;
			if ((p_character >= 0x1000 && p_character <= 0x109f) ||
				(p_character >= 0xa9e0 && p_character <= 0xa9ff) ||
				(p_character >= 0xaa60 && p_character <= 0xaa7f)) return DictionaryScript::myanmar;
			return DictionaryScript::unknown;
		}

		class LexicalRuns {
			struct Span {
				const render_item* renderer;
				int64_t offset;
				int64_t start;
				int64_t end;
			};

			Ref<TextServer> server = TextServerManager::get_singleton()->get_primary_interface();
			std::array<std::optional<bool>, std::size(DICTIONARIES)> supported;
			HTMLLexicalBreaks breaks;
			std::u32string text;
			std::vector<Span> spans;
			DictionaryScript script = DictionaryScript::unknown;
			String language;

			bool has_dictionary() {
				if (script == DictionaryScript::unknown || !server->has_feature(TextServer::FEATURE_BREAK_ITERATORS)) return false;
				const auto index = static_cast<size_t>(script);
				auto& known = supported[index];
				if (!known) {
					const auto& probe = DICTIONARIES[index];
					known = server->string_get_word_breaks(String::utf8(probe.text), probe.locale).size() > 2;
				}
				return *known;
			}

		public:
			void finish_run() {
				if (text.size() > 1 && server.is_valid()) {
					const String run(text.c_str());
					const auto graphemes = server->string_get_character_breaks(run);
					PackedInt32Array cuts = graphemes;
					int64_t first = 0;
					int64_t step = 1;
					if (has_dictionary()) {
						const String primary = DICTIONARIES[static_cast<size_t>(script)].locale;
						const String locale = language.get_slice("-", 0) == primary ? language.replace("-", "_") : primary;
						cuts = server->string_get_word_breaks(run, locale);
						first = 1;
						step = 2;
					}
					size_t span = 0;
					for (int64_t index = first; index < cuts.size(); index += step) {
						const int64_t cut = cuts[index];
						if (cut <= 0 || cut >= run.length() || graphemes.is_empty() ||
							!std::binary_search(graphemes.ptr(), graphemes.ptr() + graphemes.size(), cut)) continue;
						while (span < spans.size() && cut >= spans[span].end) ++span;
						if (span == spans.size()) break;
						const auto& source = spans[span];
						const auto& style = source.renderer->css();
						if (style.get_word_break() != word_break_manual && style.get_word_break() != word_break_break_all &&
							style.get_line_break() != line_break_anywhere && style.get_text_wrap_mode() == text_wrap_wrap) {
							breaks[source.renderer].push_back(source.offset + cut - source.start);
						}
					}
				}
				text.clear();
				spans.clear();
			}

			void append(const render_item& p_renderer, const el_text& p_text) {
				const auto& payload = p_text.get_rendered_text();
				if (std::none_of(payload.begin(), payload.end(), [](unsigned char c) { return c >= 0x80; })) {
					finish_run();
					return;
				}
				const String content = String::utf8(payload.c_str());
				const char* tag = get_element_language(p_text);
				const String declared = tag ? String::utf8(tag).to_lower().replace("_", "-") : String();
				for (int64_t offset = 0; offset < content.length(); ++offset) {
					const char32_t character = content[offset];
					if (is_lexical_character(character)) {
						const auto next = dictionary_script(character);
						if (!text.empty() && (next != script || language != declared)) finish_run();
						script = next;
						language = declared;
					}
					else if (text.empty() || !extends_lexical_character(character)) {
						finish_run();
						continue;
					}
					const int64_t position = static_cast<int64_t>(text.size());
					if (spans.empty() || spans.back().renderer != &p_renderer) {
						spans.push_back({ &p_renderer, offset, position, position + 1 });
					}
					else spans.back().end = position + 1;
					text.push_back(character);
				}
			}

			HTMLLexicalBreaks complete() {
				finish_run();
				return std::move(breaks);
			}
		};
	}

	HTMLInlineWordBreaks prepare_inline_word_breaks(const std::shared_ptr<render_item>& p_root, pixel_t p_line_width) {
		LexicalRuns runs;
		HTMLInlineWordBreaks result;
		std::vector<render_item*> pieces;
		bool needs_hyphenation = false;
		const auto finish_word = [&] {
			if (needs_hyphenation) prepare_hyphenation(pieces, p_line_width, result.automatic_hyphens);
			pieces.clear();
			needs_hyphenation = false;
		};
		go_inside_inline descendants;
		inline_selector items;
		elements_iterator iterator(false, &descendants, &items);
		const render_item* previous = nullptr;
		iterator.process(p_root, [&](const std::shared_ptr<render_item>& p_item, iterator_item_type) {
			auto* current = p_item.get();
			if (const auto* fragment = dynamic_cast<const HTMLInlineTextFragment*>(current->src_el().get())) {
				current = fragment->original_renderer().get();
			}
			if (current == previous) return;
			previous = current;
			const auto& source = *current->src_el();
			if (source.is_float() || !source.in_normal_flow()) return;
			auto* text = dynamic_cast<el_text*>(current->src_el().get());
			if (text && !source.is_break() && !is_explicit_wrap_point(source)) runs.append(*current, *text);
			else runs.finish_run();
			if (text && !source.is_break() && !source.is_space() && !source.is_other_space_separator() &&
				!source.is_glue_space() && !is_explicit_wrap_point(source)) {
				pieces.push_back(current);
				needs_hyphenation |= is_soft_hyphen(source) || source.css().get_hyphens() == hyphens_auto;
			}
			else if (source.tag() != _id("wbr")) finish_word();
		});
		finish_word();
		result.lexical = runs.complete();
		return result;
	}
}
