#include "html_shaped_text.h"

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		bool word_separator(char32_t p_character) {
			switch (p_character) {
				case U' ': case 0x00a0: case 0x1361: case 0x10100: case 0x10101: case 0x1039f: case 0x1091f:
					return true;
				default:
					return false;
			}
		}

		bool cursive_letter(char32_t p_character) {
			// Keep horizontal joining scripts connected when the shaper cannot elongate them.
			static constexpr std::pair<char32_t, char32_t> ranges[] = {
				{ 0x0620, 0x064a }, { 0x066e, 0x06d3 }, { 0x06ee, 0x06ef }, { 0x06fa, 0x06fc },
				{ 0x0710, 0x072f }, { 0x074d, 0x077f }, { 0x07ca, 0x07ea }, { 0x0840, 0x0858 },
				{ 0x0860, 0x086a }, { 0x0870, 0x088e }, { 0x08a0, 0x08c9 },
				{ 0x10ac0, 0x10ae4 }, { 0x10d00, 0x10d23 }, { 0x10ec2, 0x10ec7 },
				{ 0x1e900, 0x1e943 },
			};
			return std::any_of(std::begin(ranges), std::end(ranges), [p_character](const auto& range) {
				return p_character >= range.first && p_character <= range.second;
			});
		}

		bool has_script_gaps(char32_t p_character) {
			static constexpr std::pair<char32_t, char32_t> ranges[] = {
				{ 0x0e01, 0x0e3a }, { 0x0e40, 0x0e4e }, { 0x0e81, 0x0ede }, { 0x1000, 0x109f },
				{ 0x1100, 0x11ff }, { 0x1780, 0x17d3 }, { 0x1950, 0x197f }, { 0x1980, 0x19df },
				{ 0x1a00, 0x1aaf }, { 0x3041, 0x30ff }, { 0x3105, 0x312f }, { 0x3131, 0x318e },
				{ 0x31a0, 0x31bf }, { 0x31f0, 0x31ff }, { 0x3400, 0x4dbf }, { 0x4e00, 0x9fff },
				{ 0xa960, 0xa97c }, { 0xa9e0, 0xa9ff }, { 0xaa60, 0xaa7f }, { 0xac00, 0xd7fb },
				{ 0xf900, 0xfaff }, { 0xff66, 0xffdc }, { 0x1aff0, 0x1b16f }, { 0x20000, 0x323af },
			};
			const auto* range = std::lower_bound(std::begin(ranges), std::end(ranges), p_character,
				[](const auto& p_range, char32_t p_value) { return p_range.second < p_value; });
			return range != std::end(ranges) && range->first <= p_character;
		}

		bool bopomofo(char32_t p_character) {
			return (p_character >= 0x3105 && p_character <= 0x312f) || (p_character >= 0x31a0 && p_character <= 0x31bf);
		}

		bool justify_words(text_justify p_method) {
			return p_method == text_justify_auto || p_method == text_justify_inter_word;
		}

		struct ShapedCluster {
			int64_t first_glyph;
			int64_t end_glyph;
			float advance;
			char32_t character;
			int separators;
		};

		template <typename Callback>
		void visit_clusters(const TypedArray<Dictionary>& p_glyphs, const String& p_text, int64_t p_offset,
			bool p_count_words, Callback p_visit) {
			for (int64_t first = 0; first < p_glyphs.size();) {
				const Dictionary leading = p_glyphs[first];
				const int64_t count = leading["count"];
				const int64_t last = first + std::clamp<int64_t>(count, 1, p_glyphs.size() - first);
				const int64_t flags = leading["flags"];
				float advance = 0.0f;
				for (int64_t index = first; index < last; ++index) {
					const Dictionary glyph = p_glyphs[index];
					advance += float(glyph["advance"]) * int32_t(glyph["repeat"]);
				}
				const int64_t start = int64_t(leading["start"]) - p_offset;
				const bool visible = advance > 0.0f && !(flags & TextServer::GRAPHEME_IS_VIRTUAL) &&
					start >= 0 && start < p_text.length();
				int separators = 0;
				if (visible && p_count_words) {
					const int64_t end = int64_t(leading["end"]) - p_offset;
					for (int64_t index = start; index < std::min(end, p_text.length()); ++index) {
						if (word_separator(p_text[index])) ++separators;
					}
				}
				p_visit(ShapedCluster{ first, last, advance, visible ? p_text[start] : char32_t(0), separators });
				first = last;
			}
		}
	}

	bool has_text_justification_gap(text_justify p_method, char32_t p_left, char32_t p_right) {
		if (!p_left || !p_right || (cursive_letter(p_left) && cursive_letter(p_right))) return false;
		if (p_method == text_justify_inter_character) return true;
		if (p_method != text_justify_auto && p_method != text_justify_ruby) return false;
		if (word_separator(p_left) || word_separator(p_right)) return false;
		if (p_method == text_justify_ruby && bopomofo(p_left) && bopomofo(p_right)) return false;
		return has_script_gaps(p_left) || has_script_gaps(p_right);
	}

	text_justify_info HTMLTextSpacing::get_justification_info(const HTMLTextLayout& p_layout, const String& p_text, text_justify p_method) {
		text_justify_info result;
		if (p_method == text_justify_none || p_text.is_empty()) return result;
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		visit_clusters(server->shaped_text_get_glyphs(p_layout.get_rid()), p_text, p_layout.text_start(*server.ptr()),
			justify_words(p_method), [&](const ShapedCluster& p_cluster) {
			if (!p_cluster.character) return;
			if (!result.first) result.first = p_cluster.character;
			result.count += p_cluster.separators + int(has_text_justification_gap(p_method, result.last, p_cluster.character));
			result.last = p_cluster.character;
		});
		return result;
	}

	std::shared_ptr<const HTMLTextSpacing> HTMLTextSpacing::create(const HTMLTextLayout& p_layout, const String& p_text,
		float p_letters, float p_words, text_justify_adjustment p_justification) {
		if (p_justification.spacing == 0.0f && p_words == 0.0f && (p_letters == 0.0f || p_text.length() < 2)) return {};
		if (p_letters == 0.0f && p_justification.spacing == 0.0f) {
			int64_t index = 0;
			while (index < p_text.length() && !word_separator(p_text[index])) ++index;
			if (index == p_text.length()) return {};
		}
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		const auto native = server->shaped_text_get_glyphs(p_layout.get_rid());
		if (native.is_empty()) return {};
		auto result = std::make_shared<HTMLTextSpacing>();
		result->glyph_offsets.resize(native.size());
		result->boundaries.push_back({});
		float unspaced = 0.0f;
		float spaced = 0.0f;
		float previous_advance = 0.0f;
		char32_t previous_character = 0;
		bool changed = false;
		const float justify_word_spacing = justify_words(p_justification.method) ? p_justification.spacing : 0.0f;
		visit_clusters(native, p_text, p_layout.text_start(*server.ptr()), p_words != 0.0f || justify_word_spacing != 0.0f,
			[&](const ShapedCluster& p_cluster) {
			const float advance = p_cluster.advance;
			const float word_spacing = p_cluster.separators ? std::max(-advance, p_words * p_cluster.separators) : 0.0f;
			const float expanded_space = word_spacing + justify_word_spacing * p_cluster.separators;
			const char32_t character = p_cluster.character;
			float tracking = 0.0f;
			if (character && previous_character) {
				if (p_letters != 0.0f && !(cursive_letter(previous_character) && cursive_letter(character))) {
					tracking = std::max(-std::min(previous_advance, advance + word_spacing), p_letters);
				}
				if (p_justification.spacing != 0.0f && has_text_justification_gap(p_justification.method, previous_character, character)) {
					tracking += p_justification.spacing;
				}
				result->boundaries.back().y += tracking * 0.5f;
			}
			if (character) {
				previous_character = character;
				previous_advance = advance + word_spacing;
			}
			changed |= tracking != 0.0f || expanded_space != 0.0f;
			spaced += tracking;
			const float delta = spaced - unspaced + expanded_space * 0.5f;
			float cursor = unspaced + delta;
			for (int64_t index = p_cluster.first_glyph; index < p_cluster.end_glyph; ++index) {
				const Dictionary glyph = native[index];
				result->glyph_offsets[index] = delta;
				const RID font = glyph["font_rid"];
				const int64_t glyph_index = glyph["index"];
				const int32_t size = glyph["font_size"];
				const int32_t repeat = glyph["repeat"];
				const int64_t glyph_flags = glyph["flags"];
				const float glyph_advance = glyph["advance"];
				const auto offset = Vector2(glyph["offset"]);
				const bool missing = !font.is_valid() && !(glyph_flags & TextServer::GRAPHEME_IS_VIRTUAL);
				for (int32_t copy = 0; copy < repeat; ++copy) {
					if (glyph_index != 0 && size > 0 && (font.is_valid() || missing)) {
						result->glyphs.push_back({ font, Vector2(cursor, 0) + offset, glyph_index, size, missing });
					}
					cursor += glyph_advance;
				}
			}
			unspaced += advance;
			spaced += advance + expanded_space;
			if (advance > 0.0f) result->boundaries.push_back(Vector2(unspaced, spaced));
		});
		if (!changed || !std::isfinite(spaced)) return {};
		result->width = spaced;
		return result;
	}

	float HTMLTextSpacing::map(float p_x, bool p_inverse) const {
		const auto key = [p_inverse](const Vector2& p_point) { return p_inverse ? p_point.y : p_point.x; };
		const auto value = [p_inverse](const Vector2& p_point) { return p_inverse ? p_point.x : p_point.y; };
		if (p_x <= key(boundaries.front())) return value(boundaries.front()) + p_x - key(boundaries.front());
		const auto next = std::upper_bound(boundaries.begin(), boundaries.end(), p_x,
			[&](float x, const Vector2& point) { return x < key(point); });
		const auto& previous = *(next - 1);
		if (next == boundaries.end()) return value(previous) + p_x - key(previous);
		const float fraction = (p_x - key(previous)) / (key(*next) - key(previous));
		return value(previous) + fraction * (value(*next) - value(previous));
	}

	void HTMLTextSpacing::draw(const TextServer& p_server, const RID& p_canvas, const Vector2& p_baseline, const Color& p_color) const {
		for (const auto& glyph : glyphs) {
			const Vector2 origin = p_baseline + glyph.position;
			if (glyph.missing) p_server.draw_hex_code_box(p_canvas, glyph.size, origin, glyph.index, p_color);
			else p_server.font_draw_glyph(glyph.font, p_canvas, glyph.size, origin, glyph.index, p_color);
		}
	}
}
