#include "litehtml_fonts.h"

#include "formats/html/resources/html_resources.h"
#include "html_system_fonts.h"

#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/system_font.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include "gen/html/html_default_font.gen.h"

#include <cstring>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <tuple>
#include <utility>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		constexpr int64_t WIDTH_AXIS = 0x77647468; // OpenType tag "wdth".
		constexpr uint32_t OPTICAL_SIZE_AXIS = 0x6f70737a; // OpenType tag "opsz".
		constexpr uint32_t PETITE_CAPS = 0x70636170; // OpenType tag "pcap".
		constexpr uint32_t UPPERCASE_PETITE_CAPS = 0x63327063; // OpenType tag "c2pc".

		struct BidiText {
			String text;
			int64_t prefix = 0;
			int64_t suffix = 0;
		};

		BidiText apply_unicode_bidi(const String& p_text, bool p_right_to_left, css_unicode_bidi p_mode) {
			if (p_text.is_empty() || p_mode == css_unicode_bidi::normal) return { p_text };
			const char32_t override_start = p_right_to_left ? 0x202e : 0x202d;
			const char32_t embed_start = p_right_to_left ? 0x202b : 0x202a;
			const char32_t isolate_start = p_right_to_left ? 0x2067 : 0x2066;
			String prefix;
			String suffix;
			switch (p_mode) {
				case css_unicode_bidi::embed:
					prefix = String::chr(embed_start);
					suffix = String::chr(0x202c);
					break;
				case css_unicode_bidi::bidi_override:
					prefix = String::chr(override_start);
					suffix = String::chr(0x202c);
					break;
				case css_unicode_bidi::isolate:
					prefix = String::chr(isolate_start);
					suffix = String::chr(0x2069);
					break;
				case css_unicode_bidi::isolate_override:
					prefix = String::chr(isolate_start) + String::chr(override_start);
					suffix = String::chr(0x202c) + String::chr(0x2069);
					break;
				case css_unicode_bidi::plaintext:
					prefix = String::chr(0x2068);
					suffix = String::chr(0x2069);
					break;
				case css_unicode_bidi::normal:
					return { p_text };
			}
			return { prefix + p_text + suffix, prefix.length(), suffix.length() };
		}

#ifdef WINDOWS_ENABLED
		enum class HanScript { none, simplified, traditional };

		HanScript chinese_script(const String& p_language) {
			const auto parts = p_language.to_lower().split("-");
			if (parts.is_empty()) return HanScript::none;
			const String primary = parts[0];
			if (primary != "zh" && primary != "cmn" && primary != "yue") return HanScript::none;
			// Chinese tags can also use the older zh-cmn and zh-yue forms.
			const bool has_extlang = primary == "zh" && parts.size() > 1 && (parts[1] == "cmn" || parts[1] == "yue");
			const bool cantonese = primary == "yue" || (has_extlang && parts[1] == "yue");
			// Explicit script subtags take precedence over the region.
			for (int64_t i = has_extlang ? 2 : 1; i < parts.size() && parts[i].length() != 1; ++i) {
				if (parts[i] == "hans") return HanScript::simplified;
				if (parts[i] == "hant") return HanScript::traditional;
				if (parts[i] == "tw" || parts[i] == "hk" || parts[i] == "mo") return HanScript::traditional;
				if (parts[i].length() == 4) return HanScript::none;
				if (parts[i].length() == 2) break;
			}
			return cantonese ? HanScript::traditional : HanScript::simplified;
		}

		const char* chinese_generic_fonts(HanScript p_script, const String& p_generic) {
			if (p_script == HanScript::none) return nullptr;
			const bool traditional = p_script == HanScript::traditional;
			if (p_generic == "serif")
				return traditional ? "Noto Serif TC|Noto Serif CJK TC|PMingLiU" : "Noto Serif SC|Noto Serif CJK SC|SimSun";
			if (p_generic == "monospace") return traditional ? "MingLiU" : "NSimSun";
			if (p_generic == "cursive") return traditional ? "DFKai-SB" : "KaiTi";
			if (p_generic == "standard" || p_generic == "sans-serif")
				return traditional ? "Noto Sans TC|Noto Sans CJK TC|Microsoft JhengHei" : "Noto Sans SC|Noto Sans CJK SC|Microsoft YaHei";
			return nullptr;
		}

		// Limit the Windows generic preferences to Han and East Asian punctuation.
		const css_unicode_ranges CHINESE_FONT_RANGES = {
			{0x2e80, 0x2fff}, {0x3000, 0x303f}, {0x31c0, 0x31ef}, {0x3400, 0x4dbf},
			{0x4e00, 0x9fff}, {0xf900, 0xfaff}, {0xfe10, 0xfe1f}, {0xfe30, 0xfe4f},
			{0xff01, 0xff60}, {0xffe0, 0xffe6}, {0x20000, 0x2fa1f}, {0x30000, 0x3ffff},
		};
#endif

		int32_t font_pixel_size(pixel_t p_size) {
			const double size = std::round(static_cast<double>(p_size));
			return std::isfinite(size) && size <= std::numeric_limits<int32_t>::max()
				? static_cast<int32_t>(std::max(1.0, size)) : LiteHTMLFonts::DEFAULT_SIZE;
		}

		int32_t adjusted_font_size(double p_size, double p_ratio) {
			if (p_ratio == 0.0) return 0;
			if (p_ratio == 1.0) return font_pixel_size(p_size);
			// TextServer stores font sizes in a signed 26.6 coordinate.
			constexpr double limit = std::numeric_limits<int32_t>::max() / 64;
			return font_pixel_size(std::min(p_size * p_ratio, limit));
		}

		float resolve_font_metric(const std::optional<double>& p_ratio, float p_native, double p_size) {
			if (!p_ratio) return p_native;
			// Keep sums of author-provided metrics within the layout coordinate range.
			constexpr double limit = std::numeric_limits<int32_t>::max() / 4.0;
			return static_cast<float>(std::min(*p_ratio * std::min(p_size, std::numeric_limits<double>::max()), limit));
		}

		bool is_shaping_control(char32_t p_character) {
			return p_character == 0x200c || p_character == 0x200d ||
				(p_character >= 0x202a && p_character <= 0x202e) ||
				(p_character >= 0x2066 && p_character <= 0x2069) ||
				(p_character >= 0xfe00 && p_character <= 0xfe0f) ||
				(p_character >= 0xe0100 && p_character <= 0xe01ef);
		}

		float glyph_height(const Ref<TextServer>& p_server, const RID& p_font, int32_t p_size, char32_t p_char, float p_fallback) {
			const int64_t glyph = p_server->font_get_glyph_index(p_font, p_size, p_char, 0);
			if (glyph == 0) {
				return p_fallback;
			}
			const Dictionary outline = p_server->font_get_glyph_contours(p_font, p_size, glyph);
			const PackedVector3Array points = outline.get("points", PackedVector3Array());
			float height = 0.0f;
			for (int64_t index = 0; index < points.size(); ++index) {
				height = std::max(height, -points[index].y);
			}
			return std::isfinite(height) && height > 0.0f ? height : p_fallback;
		}

		std::optional<double> measure_font_aspect(const Ref<TextServer>& p_server, const RID& p_face,
			css_font_size_adjust::Metric p_metric) {
			using Metric = css_font_size_adjust::Metric;
			if (!p_face.is_valid()) return {};
			// Measure an outline at a size large enough to limit integer hinting error.
			constexpr int sample_size = 1024;
			float measure = 0.0f;
			if (p_metric == Metric::ex_height || p_metric == Metric::cap_height) {
				const char32_t character = p_metric == Metric::ex_height ? U'x' : U'H';
				measure = glyph_height(p_server, p_face, sample_size, character, 0.0f);
			} else {
				const char32_t character = p_metric == Metric::ch_width ? U'0' : U'\u6c34';
				const auto glyph = p_server->font_get_glyph_index(p_face, sample_size, character, 0);
				if (!glyph) return {};
				const auto advance = p_server->font_get_glyph_advance(p_face, sample_size, glyph);
				measure = p_metric == Metric::ic_height ? std::abs(advance.y) : advance.x;
			}
			if (!std::isfinite(measure) || measure <= 0) return {};
			return measure / double(sample_size);
		}

		std::optional<double> measure_font_aspect(const Ref<TextServer>& p_server, const Ref<Font>& p_font,
			css_font_size_adjust::Metric p_metric) {
			const auto rids = p_font->get_rids();
			return rids.is_empty() ? std::nullopt : measure_font_aspect(p_server, RID(rids[0]), p_metric);
		}

		struct CaseFoldRange {
			char32_t first;
			char32_t last;
			int32_t offset;
			int step;
		};

		struct CaseFoldExpansion {
			char32_t codepoint;
			const char32_t* folded;
		};

		#include "unicode_case_folding.inc"

		struct CodepointRange {
			char32_t first;
			char32_t last;
		};

		#include "unicode_emphasis_skip.inc"

		bool skips_emphasis(char32_t p_value) {
			const auto* range = std::lower_bound(std::begin(EMPHASIS_SKIP_RANGES), std::end(EMPHASIS_SKIP_RANGES), p_value,
				[](const CodepointRange& p_range, char32_t p_codepoint) { return p_range.last < p_codepoint; });
			return range != std::end(EMPHASIS_SKIP_RANGES) && range->first <= p_value;
		}

		String fold_font_family(const String& p_name) {
			std::u32string folded;
			folded.reserve(p_name.length());
			for (int64_t index = 0; index < p_name.length(); ++index) {
				const char32_t codepoint = p_name[index];
				const auto* range = std::lower_bound(std::begin(CASE_FOLD_RANGES), std::end(CASE_FOLD_RANGES), codepoint,
					[](const CaseFoldRange& p_range, char32_t p_value) { return p_range.last < p_value; });
				if (range != std::end(CASE_FOLD_RANGES) && codepoint >= range->first && (codepoint - range->first) % range->step == 0) {
					folded += static_cast<char32_t>(static_cast<int32_t>(codepoint) + range->offset);
					continue;
				}
				const auto* expansion = std::lower_bound(std::begin(CASE_FOLD_EXPANSIONS), std::end(CASE_FOLD_EXPANSIONS), codepoint,
					[](const CaseFoldExpansion& p_expansion, char32_t p_value) { return p_expansion.codepoint < p_value; });
				if (expansion != std::end(CASE_FOLD_EXPANSIONS) && expansion->codepoint == codepoint) {
					folded += expansion->folded;
				}
				else {
					folded += codepoint;
				}
			}
			return String(folded.c_str());
		}

		std::pair<int, float> font_weight_rank(float p_requested, float p_selected) {
			const float distance = std::abs(p_requested - p_selected);
			if (p_requested < 400.0f) return { p_selected > p_requested, distance };
			if (p_requested > 500.0f) return { p_selected < p_requested, distance };
			if (p_selected >= p_requested && p_selected <= 500.0f) return { 0, distance };
			return { p_selected < p_requested ? 1 : 2, distance };
		}

		std::pair<bool, float> font_width_rank(float p_requested, float p_selected) {
			const bool wrong_side = p_requested <= 100.0f ? p_selected > p_requested : p_selected < p_requested;
			return { wrong_side, std::abs(p_requested - p_selected) };
		}

		std::pair<int, float> font_style_rank(const css_font_style& p_requested, const css_font_face& p_face,
			float p_selected_angle) {
			if (p_requested.kind == font_style_italic) {
				if (p_face.style == font_style_italic) return { 0, 0.0f };
				if (p_selected_angle >= 11.0f) return { 1, p_selected_angle - 11.0f };
				if (p_selected_angle > 0.0f) return { 2, 11.0f - p_selected_angle };
				return { p_face.style == font_style_normal ? 3 : 4, -p_selected_angle };
			}
			if (p_face.style == font_style_italic) {
				return { p_requested.kind == font_style_normal ? 2 : 3, 0.0f };
			}
			const float angle = p_requested.angle;
			if (angle == 0.0f && p_selected_angle == 0.0f) return { 0, 0.0f };
			const float direction = angle < 0.0f ? -1.0f : 1.0f;
			const float preferred = p_selected_angle * direction;
			const float requested = std::abs(angle);
			if (preferred == requested) return { 0, 0.0f };
			const float distance = std::abs(preferred - requested);
			if (preferred <= 0.0f && requested != 0.0f) return { 4, distance };
			if (preferred < 0.0f) return { 4, distance };
			const bool outward = requested >= 11.0f;
			return { (preferred > requested) == outward ? 1 : 2, distance };
		}

		Transform2D oblique_transform(float p_degrees) {
			constexpr double radians_per_degree = 3.14159265358979323846 / 180.0;
			// The tangent is undefined at either endpoint of the CSS angle range.
			const float shear = static_cast<float>(std::tan(std::clamp(p_degrees, -89.0f, 89.0f) * radians_per_degree));
			// TextServer passes the columns to FreeType as its y-up outline matrix rows.
			return Transform2D(1.0f, shear, 0.0f, 1.0f, 0.0f, 0.0f);
		}

		void merge_axis_settings(Dictionary& r_axes, const Dictionary& p_supported, const css_font_variations& p_settings) {
			if (p_settings.empty()) return;
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			for (const Variant& key : r_axes.keys()) {
				if (key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) continue;
				const auto tag = static_cast<uint32_t>(server->name_to_tag(key));
				if (p_settings.count(tag) && p_supported.has(int64_t(tag))) r_axes.erase(key);
			}
			for (const auto& [tag, value] : p_settings) {
				if (p_supported.has(int64_t(tag))) r_axes[int64_t(tag)] = value;
			}
		}

		bool wants_optical_size(const font_description& p_description) {
			return p_description.optical_sizing == font_optical_sizing_auto && !p_description.variations.count(OPTICAL_SIZE_AXIS);
		}

		void apply_optical_size(Dictionary& r_axes, const Dictionary& p_supported, const font_description& p_description, double p_ratio = 1.0) {
			constexpr float points_per_css_pixel = 72.0f / 96.0f;
			merge_axis_settings(r_axes, p_supported, { { OPTICAL_SIZE_AXIS, adjusted_font_size(p_description.size, p_ratio) * points_per_css_pixel } });
		}

		bool has_feature_settings(const font_description& p_description) {
			return p_description.kerning != font_kerning_auto || !p_description.features.empty() ||
				!p_description.variant_features.empty() || p_description.alternates.historical_forms || p_description.letter_spacing != 0;
		}

		Dictionary font_features(const Ref<Font>& p_font, const css_font_features& p_face,
			const css_font_features& p_named, const font_description& p_description) {
			Dictionary features = p_font->get_opentype_features();
			features = features.duplicate();
			constexpr uint32_t kern = 0x6b65726e;
			constexpr uint32_t palt = 0x70616c74;
			constexpr uint32_t apkn = 0x61706b6e;
			css_font_features overrides = p_face;
			if (p_description.kerning != font_kerning_auto) {
				const bool enable = p_description.kerning == font_kerning_normal;
				overrides[kern] = enable;
				if (enable) {
					const Dictionary supported = p_font->get_supported_feature_list();
					if (supported.has(int64_t(palt))) {
						overrides[palt] = 1;
						if (supported.has(int64_t(apkn))) overrides[apkn] = 1;
					}
				}
				else overrides[apkn] = 0;
			}
			for (const auto& [tag, value] : p_description.variant_features) overrides[tag] = value;
			if (p_description.variant_features.count(PETITE_CAPS)) {
				const Dictionary supported = p_font->get_supported_feature_list();
				const bool all_petite = p_description.variant_features.count(UPPERCASE_PETITE_CAPS);
				if (!supported.has(int64_t(PETITE_CAPS)) || (all_petite && !supported.has(int64_t(UPPERCASE_PETITE_CAPS)))) {
					overrides[PETITE_CAPS] = 0;
					overrides[0x736d6370] = 1; // smcp
					if (all_petite) {
						overrides[UPPERCASE_PETITE_CAPS] = 0;
						overrides[0x63327363] = 1; // c2sc
					}
				}
			}
			if (p_description.alternates.historical_forms) overrides[0x68697374] = 1; // hist
			for (const auto& [tag, value] : p_named) overrides[tag] = value;
			if (p_description.letter_spacing != 0) {
				for (const uint32_t optional : { 0x6c696761u, 0x636c6967u, 0x646c6967u, 0x686c6967u }) { // liga, clig, dlig, hlig
					overrides[optional] = 0;
				}
			}
			for (const auto& [tag, value] : p_description.features) overrides[tag] = value;
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			for (const Variant& key : features.keys()) {
				if (key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) continue;
				const auto tag = static_cast<uint32_t>(server->name_to_tag(key));
				if (overrides.count(tag)) features.erase(key);
			}
			for (const auto& [tag, value] : overrides) features[int64_t(tag)] = int64_t(value);
			return features;
		}

		Ref<Font> combine_fonts(TypedArray<Font> p_fonts, const Ref<Font>& p_feature_source) {
			const Ref<Font> primary = p_fonts[0];
			if (p_fonts.size() == 1) return primary;
			const Ref<FontVariation> primary_variation = primary;
			Ref<FontVariation> combined;
			if (primary_variation.is_valid()) {
				combined = primary_variation->duplicate();
			}
			else {
				combined.instantiate();
				combined->set_base_font(primary);
			}
			p_fonts.remove_at(0);
			const TypedArray<Font> fallbacks = primary->get_fallbacks();
			for (int64_t index = 0; index < fallbacks.size(); ++index) {
				const Ref<Font> fallback = fallbacks[index];
				if (fallback.is_valid() && !p_fonts.has(fallback)) p_fonts.append(fallback);
			}
			combined->set_fallbacks(p_fonts);
			if (p_feature_source.is_valid()) combined->set_opentype_features(p_feature_source->get_opentype_features());
			return combined;
		}

	} // namespace

	LiteHTMLFonts::LiteHTMLFonts(const HTMLFontConfig& p_fonts, HTMLResources& p_resources) :
		fonts(p_fonts), resources(p_resources), default_font(p_fonts.text) {
		if (default_font.is_null()) {
			PackedByteArray bytes;
			bytes.resize(sizeof(html_default_font));
			std::memcpy(bytes.ptrw(), html_default_font, sizeof(html_default_font));
			Ref<FontFile> bundled;
			bundled.instantiate();
			bundled->set_data(bytes);
			default_font = bundled;
		}
	}

	const char* LiteHTMLFonts::default_family() const {
		return fonts.text.is_valid() ? "sans-serif" : "serif";
	}

	int32_t LiteHTMLFonts::default_fixed_size() const {
		return fonts.text.is_valid() ? DEFAULT_SIZE : 13;
	}

	bool LiteHTMLFonts::set_font_rules(std::vector<css_font_face> p_faces,
		std::vector<css_font_feature_values_rule> p_feature_values) {
		if (font_faces == p_faces && font_feature_rules == p_feature_values) return false;
		font_faces = std::move(p_faces);
		font_feature_rules = std::move(p_feature_values);
		font_stacks.clear();
		faces_by_family.clear();
		for (size_t index = 0; index < font_faces.size(); ++index) {
			const String name = fold_font_family(String::utf8(font_faces[index].family.c_str()));
			auto* faces = faces_by_family.getptr(name);
			if (!faces) faces = &faces_by_family.insert(name, {})->value;
			faces->push_back(index);
		}
		feature_names_by_family.clear();
		for (const auto& rule : font_feature_rules) {
			for (const auto& family : rule.families) {
				const String name = fold_font_family(String::utf8(family.c_str()));
				auto* definitions = feature_names_by_family.getptr(name);
				if (!definitions) definitions = &feature_names_by_family.insert(name, {})->value;
				for (size_t type = 0; type < rule.names.size(); ++type) {
					for (const auto& [alias, values] : rule.names[type]) (*definitions)[type].insert_or_assign(alias, values);
				}
			}
		}
		return true;
	}

	css_font_features LiteHTMLFonts::resolve_feature_names(const String& p_folded_family, const css_font_alternates& p_alternates) const {
		if (p_alternates.names.empty()) return {};
		const auto* definitions = feature_names_by_family.getptr(p_folded_family);
		return definitions ? resolve_font_feature_names(p_alternates, *definitions) : css_font_features{};
	}

	bool LiteHTMLFonts::FontChoice::covers(char32_t p_character) const {
		if (!ranges.empty()) {
			const auto range = std::lower_bound(ranges.begin(), ranges.end(), p_character,
				[](const auto& p_range, char32_t p_value) { return p_range.second < p_value; });
			if (range == ranges.end() || range->first > p_character) return false;
		}
		return font->has_char(p_character);
	}

	const LiteHTMLFonts::FontChoice* LiteHTMLFonts::FontStack::match(const String& p_text, int64_t p_start, int64_t p_end,
		bool p_ignore_joiners) const {
		for (const auto& choice : choices) {
			int64_t index = p_start;
			while (index < p_end && ((p_ignore_joiners && is_shaping_control(p_text[index])) || choice.covers(p_text[index]))) ++index;
			if (index == p_end) return &choice;
		}
		return nullptr;
	}

	void LiteHTMLFonts::FontStack::add_text(TextLine& r_line, const String& p_text, double p_size,
		std::vector<ShapingRun>* r_runs) const {
		const int32_t base_size = font_pixel_size(p_size);
		if (choices.empty() || p_text.is_empty()) {
			r_line.add_string(p_text, unrestricted, base_size, language);
			return;
		}
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		const auto breaks = server->string_get_character_breaks(p_text, language);
		int64_t start = 0;
		int64_t run_start = 0;
		Ref<Font> run;
		const FontChoice* run_choice = nullptr;
		int32_t run_size = base_size;
		const auto add_zero = [&](int64_t p_start, int64_t p_end) {
			// A zero-sized face still occupies the original source offsets in TextServer.
			int64_t offset = p_start;
			if (!breaks.is_empty()) {
				const auto* end = breaks.ptr() + breaks.size();
				for (auto* next = std::upper_bound(breaks.ptr(), end, offset); next != end && *next < p_end; ++next) {
					r_line.add_object(offset, {}, INLINE_ALIGNMENT_TOP, *next - offset);
					offset = *next;
				}
			}
			r_line.add_object(offset, {}, INLINE_ALIGNMENT_TOP, p_end - offset);
		};
		const auto add_system_fallback = [&](int64_t p_end) {
			const String value = p_text.substr(run_start, p_end - run_start);
			// Let TextServer choose installed fallback faces with the original language and shaping context.
			Ref<TextLine> probe;
			probe.instantiate();
			probe->add_string(value, unrestricted, base_size, language);
			const auto glyphs = server->shaped_text_sort_logical(probe->get_rid());
			const Dictionary features = unrestricted->get_opentype_features();
			const auto emit = [&](int64_t p_first, int64_t p_last, const RID& p_face) {
				if (p_last <= p_first) return;
				const String span = value.substr(p_first, p_last - p_first);
				const auto aspect = measure_font_aspect(server, p_face, size_adjust_metric);
				if (!aspect) {
					r_line.add_string(span, unrestricted, base_size, language);
					return;
				}
				const int32_t size = adjusted_font_size(p_size, *size_adjust_reference / *aspect);
				if (size == 0) {
					add_zero(run_start + p_first, run_start + p_last);
				} else {
					TypedArray<RID> faces;
					faces.append(p_face);
					if (!server->shaped_text_add_string(r_line.get_rid(), span, faces, size, features, language)) {
						r_line.add_string(span, unrestricted, base_size, language);
					}
				}
			};
			int64_t first = 0;
			int64_t covered = 0;
			RID face;
			for (const Dictionary& glyph : glyphs) {
				const int64_t from = glyph["start"];
				const int64_t to = glyph["end"];
				if (from < covered || to <= from || to > value.length()) continue;
				const RID next = glyph["font_rid"];
				if (from != covered || next != face) {
					emit(first, covered, face);
					emit(covered, from, {});
					first = from;
					face = next;
				}
				covered = to;
			}
			emit(first, covered, face);
			emit(covered, value.length(), {});
		};
		const auto flush = [&](int64_t p_end) {
			if (run.is_null() || p_end <= run_start) return;
			if (r_runs) r_runs->push_back({run_start, run_choice});
			if (run_size == 0) add_zero(run_start, p_end);
			else if (!run_choice && size_adjust_reference) add_system_fallback(p_end);
			else r_line.add_string(p_text.substr(run_start, p_end - run_start), run, run_size, language);
		};
		const auto append = [&](int64_t p_first, const FontChoice* p_choice) {
			const auto& font = p_choice ? p_choice->font : unrestricted;
			const int32_t next_size = p_choice ? adjusted_font_size(p_size, p_choice->size_adjust) : base_size;
		if (run != font || run_size != next_size || ((r_runs || size_adjust_reference) && run_choice != p_choice)) {
				flush(p_first);
				run_start = p_first;
				run = font;
				run_choice = p_choice;
				run_size = next_size;
			}
		};
		for (const auto end : breaks) {
			if (end <= start || end > p_text.length()) continue;
			if (const auto* choice = match(p_text, start, end)) {
				append(start, choice);
			}
			else {
				// Leave variation selectors and joined emoji sequences to the shaper.
				bool needs_sequence = false;
				for (int64_t index = start; index < end && !needs_sequence; ++index) {
					needs_sequence = is_shaping_control(p_text[index]);
				}
				if (needs_sequence) {
					append(start, has_size_adjust ? match(p_text, start, end, true) : nullptr);
				}
				else {
					for (int64_t index = start; index < end; ++index) {
						append(index, match(p_text, index, index + 1));
					}
				}
			}
			start = end;
		}
		if (start < p_text.length()) {
			append(start, match(p_text, start, p_text.length()));
		}
		flush(p_text.length());
	}

	std::optional<float> LiteHTMLFonts::FontStack::char_advance(char32_t p_character, double p_size) const {
		for (const auto& choice : choices) {
			if (!choice.covers(p_character)) continue;
			const auto size = adjusted_font_size(p_size, choice.size_adjust);
			return size > 0 ? choice.font->get_char_size(p_character, size).x : 0.0f;
		}
		if (size_adjust_reference) {
			Ref<TextLine> line;
			line.instantiate();
			add_text(*line.ptr(), String::chr(p_character), p_size);
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			for (const Dictionary& glyph : server->shaped_text_get_glyphs(line->get_rid())) {
				const RID face = glyph["font_rid"];
				const int flags = glyph["flags"];
				if (face.is_valid() || (flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT)) return line->get_line_width();
			}
			return {};
		}
		const float advance = unrestricted->get_char_size(p_character, font_pixel_size(p_size)).x;
		return advance > 0 ? std::optional<float>(advance) : std::nullopt;
	}

	HTMLTextLayout LiteHTMLFonts::FontData::shape(const String& p_text) {
		if (const auto* cached = lines.getptr(p_text)) {
			return *cached;
		}
		Ref<TextLine> line;
		line.instantiate();
		const BidiText shaped = apply_unicode_bidi(p_text, description.right_to_left, description.unicode_bidi);
		line->set_direction(description.unicode_bidi == css_unicode_bidi::plaintext ? TextServer::DIRECTION_AUTO :
			(description.right_to_left ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR));
		std::vector<FontStack::ShapingRun> runs;
		const bool needs_run_metrics = stack->has_size_adjust && description.use_font_line_gap;
		stack->add_text(*line.ptr(), shaped.text, description.size, needs_run_metrics ? &runs : nullptr);
		HTMLTextLayout layout{ line, ascent, shaped.prefix, shaped.suffix };
		layout.descent = descent;
		if (!runs.empty()) adjust_normal_line_metrics(layout, runs);
		layout.spacing = HTMLTextSpacing::create(layout, p_text, description.letter_spacing, description.word_spacing);
		lines.insert(p_text, layout);
		return layout;
	}

	void LiteHTMLFonts::FontData::adjust_normal_line_metrics(HTMLTextLayout& r_layout,
		const std::vector<FontStack::ShapingRun>& p_runs) const {
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		const auto glyphs = server->shaped_text_sort_logical(r_layout.get_rid());
		const auto primary_rids = stack->primary->get_rids();
		const FontStack::ShapingRun* last_run = nullptr;
		RID last_face;
		float top = 0.0f;
		float bottom = 0.0f;
		std::vector<HTMLTextRunMetrics> metrics;
		for (const Dictionary& glyph : glyphs) {
			const RID face = glyph["font_rid"];
			if (!face.is_valid()) continue;
			const int64_t start = glyph["start"];
			const auto end = std::upper_bound(p_runs.begin(), p_runs.end(), start,
				[](int64_t p_offset, const FontStack::ShapingRun& p_run) { return p_offset < p_run.start; });
			if (end == p_runs.begin()) continue;
			const auto& run = *std::prev(end);
			if (last_face != face || last_run != &run) {
				last_face = face;
				last_run = &run;
				const auto* choice = run.choice;
				if (choice && choice->font == stack->primary && choice->size_adjust == stack->primary_size_adjust &&
					choice->metrics == stack->primary_metrics && primary_rids.has(face)) {
					top = r_layout.ascent;
					bottom = r_layout.descent;
				} else {
					const int64_t glyph_size = glyph["font_size"];
					top = server->font_get_ascent(face, glyph_size) + server->font_get_spacing(face, TextServer::SPACING_TOP);
					bottom = server->font_get_descent(face, glyph_size) + server->font_get_spacing(face, TextServer::SPACING_BOTTOM);
					if (choice && choice->font->get_rids().has(face)) {
						const double size = description.size * choice->size_adjust;
						const float half_gap = resolve_font_metric(choice->metrics.line_gap.horizontal, 0, size) / 2;
						top = resolve_font_metric(choice->metrics.ascent.horizontal, top, size) + half_gap;
						bottom = resolve_font_metric(choice->metrics.descent.horizontal, bottom, size) + half_gap;
					}
				}
			}
			if (top <= r_layout.ascent && bottom <= r_layout.descent) continue;
			const int64_t glyph_end = glyph["end"];
			if (glyph_end <= start) continue;
			// Several glyphs can share one cluster. Merge their extents for range queries.
			if (!metrics.empty() && (start < metrics.back().end ||
				(start == metrics.back().end && top == metrics.back().ascent && bottom == metrics.back().descent))) {
				auto& previous = metrics.back();
				previous.end = std::max(previous.end, glyph_end);
				previous.ascent = std::max(previous.ascent, top);
				previous.descent = std::max(previous.descent, bottom);
			} else {
				metrics.push_back({start, glyph_end, top, bottom});
			}
		}
		r_layout.set_run_metrics(std::move(metrics));
	}

	HTMLTextLayout LiteHTMLFonts::FontData::shape_punctuation(const String& p_text, punctuation_trim p_trim) {
		auto& cache = punctuation_lines[static_cast<size_t>(p_trim) - 1];
		if (const auto* cached = cache.getptr(p_text)) return *cached;
		auto layout = shape(p_text);
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		const auto glyphs = server->shaped_text_get_glyphs(layout.get_rid());
		const auto trimmed = [&]() -> HTMLTextLayout {
			if (glyphs.size() != 1) return layout;
			const Dictionary glyph = glyphs[0];
			const RID font = glyph["font_rid"];
			const int64_t index = glyph["index"];
			const int64_t font_size = glyph["font_size"];
			if (!font.is_valid() || index <= 0 || font_size <= 0) return layout;

			constexpr uint32_t halt = 0x68616c74;
			const auto* choice = stack->match(p_text, 0, p_text.length());
			const auto& chosen = choice ? choice->font : stack->unrestricted;
			Dictionary features = chosen->get_opentype_features().duplicate();
			// Explicit font features already determine this glyph's advance.
			if (features.has(int64_t(halt)) || features.has("halt")) return layout;
			if (server->font_supported_feature_list(font).has(int64_t(halt))) {
				features[halt] = 1;
				Ref<TextLine> line;
				line.instantiate();
				const BidiText shaped = apply_unicode_bidi(p_text, description.right_to_left, description.unicode_bidi);
				line->set_direction(description.unicode_bidi == css_unicode_bidi::plaintext ? TextServer::DIRECTION_AUTO :
					(description.right_to_left ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR));
				if (server->shaped_text_add_string(line->get_rid(), shaped.text, chosen->get_rids(), font_size, features, stack->language)) {
					HTMLTextLayout candidate{ line, layout.ascent, shaped.prefix, shaped.suffix };
					candidate.descent = layout.descent;
					if (candidate.width() > 0 && candidate.width() < layout.width()) return candidate;
				}
			}

			const auto han_advance = [&](char32_t p_character) {
				const auto id = server->font_get_glyph_index(font, font_size, p_character, 0);
				return id > 0 ? server->font_get_glyph_advance(font, font_size, id).x : real_t(0);
			};
			const float full = layout.width();
			const float ideograph = han_advance(U'水');
			if (ideograph <= 0 || std::abs(full - ideograph) > 0.5f ||
				std::abs(han_advance(U'卜') - ideograph) > 0.5f || std::abs(han_advance(U'一') - ideograph) > 0.5f) return layout;
			const Dictionary contours = server->font_get_glyph_contours(font, font_size, index);
			const PackedVector3Array points = contours.get("points", PackedVector3Array());
			if (points.is_empty()) return layout;
			const Vector2 offset = glyph["offset"];
			float left = std::numeric_limits<float>::infinity();
			float right = -left;
			for (const Vector3& point : points) {
				left = std::min(left, float(point.x + offset.x));
				right = std::max(right, float(point.x + offset.x));
			}
			const float trim_left = p_trim == punctuation_trim::left ? full / 2 : p_trim == punctuation_trim::center ? full / 4 : 0;
			const float trim_right = p_trim == punctuation_trim::right ? full / 2 : p_trim == punctuation_trim::center ? full / 4 : 0;
			if ((trim_left == 0 || left >= trim_left - 0.5f) && (trim_right == 0 || right <= full - trim_right + 0.5f)) {
				layout.trim_inline_advance(trim_left, trim_right);
			}
			return layout;
		}();
		cache.insert(p_text, trimmed);
		return trimmed;
	}

	const std::vector<float>* LiteHTMLFonts::FontData::get_emphasis_centers(const String& p_text, const HTMLTextLayout& p_layout) {
		if (emphasis.is_null()) return nullptr;
		if (!p_layout.is_subrange()) {
			if (const auto* cached = emphasis_centers.getptr(p_text)) return cached->empty() ? nullptr : cached;
		}
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		const auto line_rid = p_layout.get_rid();
		const auto offset = p_layout.text_start(*server.ptr());
		const auto breaks = server->string_get_character_breaks(p_text);
		std::vector<float> points;
		int32_t start = 0;
		for (const int32_t end : breaks) {
			if (end <= start || end > p_text.length()) continue;
			// A separator with combining marks is eligible when the cluster has visible content.
			bool eligible = false;
			for (int32_t index = start; index < end && !eligible; ++index) {
				eligible = !skips_emphasis(p_text[index]);
			}
			if (eligible) {
				const auto ranges = server->shaped_text_get_selection(line_rid, start + offset, end + offset);
				if (!ranges.is_empty()) {
					float left = ranges[0].x;
					float right = ranges[0].y;
					for (const Vector2& range : ranges) {
						left = std::min(left, range.x);
						right = std::max(right, range.y);
					}
					if (right > left) {
						points.push_back((left + right) / 2.0f);
					}
				}
			}
			start = end;
		}
		if (p_layout.is_subrange()) {
			fragment_emphasis_centers = std::move(points);
			return fragment_emphasis_centers.empty() ? nullptr : &fragment_emphasis_centers;
		}
		const auto& cached = emphasis_centers.insert(p_text, std::move(points))->value;
		return cached.empty() ? nullptr : &cached;
	}

	Ref<Font> LiteHTMLFonts::find_installed_font(const String& p_family, int p_weight, bool p_italic, float p_width) {
		if (p_family.is_empty()) {
			return {};
		}
		const String folded = fold_font_family(p_family);
		const int stretch = static_cast<int>(std::round(std::clamp(p_width, 50.0f, 200.0f)));
		const String key = vformat("%d/%d/%d/%s", p_weight, int(p_italic), stretch, folded);
		if (const auto* cached = installed_fonts.getptr(key)) {
			return *cached;
		}
		OS* os = OS::get_singleton();
		// Godot aliases these names on some platforms. A quoted CSS family must be a real family.
		const bool platform_alias = folded == "serif" || folded == "sans-serif" || folded == "monospace" ||
			folded == "cursive" || folded == "fantasy";
		if (!installed_font_names) {
			installed_font_names.emplace();
			for (const String& name : os->get_system_fonts()) {
				const String name_key = fold_font_family(name);
				if (!installed_font_names->has(name_key)) {
					installed_font_names->insert(name_key, name);
				}
			}
		}
		const String* listed_family = installed_font_names->getptr(folded);
		const bool exact_lookup = !platform_alias && os->get_name() == "Windows";
		Ref<Font> result;
		if (listed_family || !platform_alias) {
			Ref<SystemFont> font;
			font.instantiate();
			font->set_font_weight(std::clamp(p_weight, 100, 999));
			font->set_font_stretch(stretch);
			font->set_font_italic(p_italic);
			// TextServer tries every face in the CSS stack before its system fallback.
			font->set_allow_system_fallback(true);
			PackedStringArray names;
			names.append(listed_family ? *listed_family : p_family);
			font->set_font_names(names);
			if (font->get_face_count() > 0 && (exact_lookup || (listed_family && !platform_alias) ||
				folded == fold_font_family(font->get_font_name()))) {
				result = font;
			}
		}
		// Font data keeps its own references to the faces it uses.
		if (installed_fonts.size() >= 512) installed_fonts.clear();
		installed_fonts.insert(key, result);
		return result;
	}

	Ref<FontFile> LiteHTMLFonts::find_local_face(const String& p_name) {
		const String key = fold_font_family(p_name);
		if (const auto* cached = local_faces.getptr(key)) return *cached;
		const auto font = load_system_font_face(p_name);
		local_faces.insert(key, font);
		return font;
	}

	void LiteHTMLFonts::append_authored_fonts(std::vector<FontChoice>& r_choices, const std::vector<size_t>& p_faces,
		const String& p_folded_family, const font_description& p_description, bool& r_size_dependent) {
		const auto named_features = resolve_feature_names(p_folded_family, p_description.alternates);
		struct Candidate {
			const css_font_face* face;
			float weight;
			float angle;
			std::pair<bool, float> width_rank;
			std::pair<int, float> style_rank;
			std::pair<int, float> weight_rank;
		};
		std::vector<Candidate> matches;
		matches.reserve(p_faces.size());
		for (auto index = p_faces.rbegin(); index != p_faces.rend(); ++index) {
			const auto& face = font_faces[*index];
			const float weight = std::clamp<float>(p_description.weight, face.min_weight, face.max_weight);
			const float angle = std::clamp(p_description.style.kind == font_style_italic ? 11.0f : p_description.style.angle,
				face.min_angle, face.max_angle);
			const auto [min_width, max_width] = face.width_range.value_or(std::make_pair(100.0f, 100.0f));
			const float width = std::clamp(p_description.width, min_width, max_width);
			matches.push_back({ &face, weight, angle, font_width_rank(p_description.width, width),
				font_style_rank(p_description.style, face, angle),
				font_weight_rank(p_description.weight, weight) });
		}
		const auto rank = [](const Candidate& p_candidate) {
			return std::tie(p_candidate.width_rank, p_candidate.style_rank, p_candidate.weight_rank);
		};
		std::stable_sort(matches.begin(), matches.end(), [&](const Candidate& a, const Candidate& b) {
			return rank(a) < rank(b);
		});
		const Candidate* best = nullptr;
		for (const auto& candidate : matches) {
			if (best && rank(candidate) != rank(*best)) break;
			const auto& face = *candidate.face;
			for (const auto& source : face.sources) {
				const Ref<FontFile> font = source.local ? find_local_face(String::utf8(source.value.c_str())) :
					resources.load_font(source.value.c_str(), face.base_url.c_str());
				if (font.is_null()) continue;
				constexpr int64_t weight_axis = 0x77676874; // OpenType tag "wght".
				constexpr int64_t slant_axis = 0x736c6e74; // OpenType tag "slnt".
				constexpr int64_t italic_axis = 0x6974616c; // OpenType tag "ital".
				const Dictionary axes_supported = font->get_supported_variation_list();
				if (source.needs_variations && axes_supported.is_empty()) continue;
				const bool variable_weight = axes_supported.has(weight_axis);
				Dictionary axes;
				if (variable_weight) axes[weight_axis] = candidate.weight;
				if (axes_supported.has(WIDTH_AXIS)) {
					float width = p_description.width;
					if (face.width_range) width = std::clamp(width, face.width_range->first, face.width_range->second);
					axes[WIDTH_AXIS] = width;
				}
				const bool oblique_face = face.style == font_style_oblique;
				const bool use_slant = axes_supported.has(slant_axis) && (oblique_face ||
					(p_description.style.kind == font_style_oblique && face.style == font_style_normal));
				if (use_slant) axes[slant_axis] = -(oblique_face ? candidate.angle : p_description.style.angle);
				if (face.style == font_style_italic && axes_supported.has(italic_axis)) axes[italic_axis] = 1;
				merge_axis_settings(axes, axes_supported, face.variations);
				if (wants_optical_size(p_description) && axes_supported.has(int64_t(OPTICAL_SIZE_AXIS))) {
					// A numeric adjustment normalizes the face independently of its authored multiplier.
					const auto ratio = p_description.size_adjust.mode == css_font_size_adjust::Mode::number ? 1.0 : face.size_adjust;
					apply_optical_size(axes, axes_supported, p_description, ratio);
					r_size_dependent = true;
				}
				merge_axis_settings(axes, axes_supported, p_description.variations);
				const bool face_has_features = !face.features.empty() || !named_features.empty();
				const bool has_features = face_has_features || has_feature_settings(p_description);
				const bool embolden = !variable_weight && p_description.weight >= 600 &&
					candidate.weight < 600.0f && font->get_font_weight() < 600;
				const float angle = p_description.style.kind == font_style_italic ? default_oblique_angle : p_description.style.angle;
				const bool slant = angle != 0.0f && !use_slant && face.style == font_style_normal &&
					!(font->get_font_style() & TextServer::FONT_ITALIC);
				Ref<Font> selected = font;
				const int64_t face_index = font->get_face_index(0);
				if (!axes.is_empty() || has_features || embolden || slant || face_index != 0) {
					Ref<FontVariation> variation;
					variation.instantiate();
					variation->set_base_font(font);
					variation->set_variation_face_index(face_index);
					variation->set_variation_opentype(axes);
					if (has_features) variation->set_opentype_features(font_features(font, face.features, named_features, p_description));
					if (embolden) variation->set_variation_embolden(0.6f);
					if (slant) variation->set_variation_transform(oblique_transform(angle));
					selected = variation;
				}
				if (!best) best = &candidate;
				r_choices.push_back({ selected, face.unicode_ranges, face_has_features, face.metrics, face.size_adjust });
				break;
			}
		}
	}

	LiteHTMLFonts::FontStack LiteHTMLFonts::resolve_font_stack(const font_description& p_description) {
		const bool bold = p_description.weight >= 600;
		const int installed_weight = static_cast<int>(std::round(p_description.weight));
		const bool italic = p_description.style.kind == font_style_italic;
		const bool oblique = p_description.style.kind == font_style_oblique;
		const float angle = italic ? default_oblique_angle : p_description.style.angle;
		const bool adjust_optical = wants_optical_size(p_description);
		const String language = String::utf8(p_description.language.c_str());
		bool size_dependent = false;
		bool has_named_features = false;
		const auto with_style = [&](const Ref<Font>& p_font, bool p_synthesize = true, const String& p_family = {}) -> Ref<Font> {
			const bool embolden = p_synthesize && bold && p_font->get_font_weight() < 600;
			const bool slant = p_synthesize && angle != 0.0f && !(p_font->get_font_style() & TextServer::FONT_ITALIC);
			const Ref<SystemFont> system_font = p_font;
			const bool adjust_width = p_description.width != 100.0f && system_font.is_null();
			const Dictionary supported_axes = adjust_width || adjust_optical || (slant && oblique) || !p_description.variations.empty() ?
				p_font->get_supported_variation_list() : Dictionary();
			const bool vary_width = adjust_width && supported_axes.has(WIDTH_AXIS);
			const bool vary_optical = adjust_optical && supported_axes.has(int64_t(OPTICAL_SIZE_AXIS));
			const bool vary_explicit = std::any_of(p_description.variations.begin(), p_description.variations.end(),
				[&](const auto& setting) { return supported_axes.has(int64_t(setting.first)); });
			css_font_features named_features;
			if (!p_description.alternates.names.empty() && !feature_names_by_family.is_empty()) {
				const String family = p_family.is_empty() ? fold_font_family(p_font->get_font_name()) : p_family;
				named_features = resolve_feature_names(family, p_description.alternates);
				has_named_features |= !named_features.empty();
			}
			const bool has_features = has_feature_settings(p_description) || !named_features.empty();
			if (!embolden && !slant && !vary_width && !vary_optical && !vary_explicit && !has_features) {
				return p_font;
			}
			const Ref<FontVariation> previous = p_font;
			Ref<FontVariation> variation;
			if (previous.is_valid()) {
				variation = previous->duplicate();
			}
			else {
				variation.instantiate();
				variation->set_base_font(p_font);
				const Ref<FontFile> file = p_font;
				if (file.is_valid()) variation->set_variation_face_index(file->get_face_index(0));
			}
			Dictionary axes = variation->get_variation_opentype().duplicate();
			if (vary_width) axes[WIDTH_AXIS] = p_description.width;
			if (embolden) {
				variation->set_variation_embolden(0.6f);
			}
			if (slant) {
				constexpr int64_t slant_axis = 0x736c6e74;
				if (oblique && supported_axes.has(slant_axis)) {
					axes[slant_axis] = -angle;
				}
				else {
					variation->set_variation_transform(oblique_transform(angle));
				}
			}
			if (vary_optical) {
				apply_optical_size(axes, supported_axes, p_description);
				size_dependent = true;
			}
			if (vary_explicit) merge_axis_settings(axes, supported_axes, p_description.variations);
			if (has_features) variation->set_opentype_features(font_features(p_font, {}, named_features, p_description));
			variation->set_variation_opentype(axes);
			variation->set_fallbacks(p_font->get_fallbacks());
			return variation;
		};
		const String text_name = fold_font_family(default_font->get_font_name());
		const Ref<Font>& themed_variant = bold && italic ? fonts.bold_italic : (bold ? fonts.bold : fonts.italic);
		const Ref<Font> text_font = (bold || italic) && themed_variant.is_valid() ?
			with_style(themed_variant, false, text_name) : with_style(default_font, true, text_name);
		std::vector<FontChoice> choices;
		const auto append = [&](const Ref<Font>& p_font) {
			if (p_font.is_valid()) choices.push_back({ p_font, {} });
		};
		const auto append_installed = [&](const char* p_names, const css_unicode_ranges& p_ranges = {}) {
			for (const String& name : String(p_names).split("|")) {
				const auto font = find_installed_font(name, installed_weight, italic, p_description.width);
				if (font.is_valid()) {
					choices.push_back({ with_style(font, true, fold_font_family(name)), p_ranges });
					return true;
				}
			}
			return false;
		};
		constexpr const char* serif_fonts = "Times New Roman|Noto Serif|DejaVu Serif|Liberation Serif|Times|Droid Serif";
		const String code_name = fonts.code.is_valid() ? fold_font_family(fonts.code->get_font_name()) : String();
		const String block_name = fonts.code_block.is_valid() ? fold_font_family(fonts.code_block->get_font_name()) : String();
#ifdef WINDOWS_ENABLED
		const auto han_script = chinese_script(language);
		const bool localize_whole_font = p_description.language_is_declared && fonts.text.is_null();
#endif
		for (const auto& family : p_description.families) {
			const String name = String::utf8(family.name.c_str());
			if (!family.generic) {
				const String folded = fold_font_family(name);
				if (folded.is_empty()) {
					continue;
				}
				if (const auto* authored = faces_by_family.getptr(folded)) {
					append_authored_fonts(choices, *authored, folded, p_description, size_dependent);
				}
				else if (folded == "dockit-code-block" && fonts.code_block.is_valid()) {
					append(with_style(fonts.code_block, true, folded));
				}
				else if (folded == text_name) {
					append(text_font);
				}
				else if (folded == code_name) {
					append(with_style(fonts.code));
				}
				else if (folded == block_name) {
					append(with_style(fonts.code_block));
				}
				else if (const auto font = find_installed_font(name, installed_weight, italic, p_description.width); font.is_valid()) {
					append(with_style(font, true, folded));
				}
				continue;
			}
#ifdef WINDOWS_ENABLED
			if (const char* candidates = chinese_generic_fonts(han_script, name)) {
				// A standalone generic resolves to a whole font. If it is absent,
				// try the rest of the author's list before the document default.
				if (localize_whole_font) {
					append_installed(candidates);
					continue;
				}
				// Untagged text and Markdown keep their usual Latin fonts.
				append_installed(candidates, CHINESE_FONT_RANGES);
			}
#endif
			if (name == "sans-serif") {
				if (fonts.text.is_valid() || !append_installed("Arial|Liberation Sans|Helvetica|Noto Sans|DejaVu Sans")) append(text_font);
			}
			else if (name == "serif") {
				append_installed(serif_fonts);
			}
			else if (name == "system-ui" || name == "ui-sans-serif") {
				append_installed("Segoe UI|.AppleSystemUIFont|Helvetica Neue|Roboto|Cantarell|Noto Sans");
			}
			else if (name == "monospace" && fonts.code.is_valid()) {
				append(with_style(fonts.code));
			}
			else if (name == "monospace" && fonts.text.is_null() && OS::get_singleton()->get_name() == "Windows") {
				append_installed("Consolas|Courier New");
			}
			else {
				static constexpr std::pair<const char*, const char*> generic_fonts[] = {
				{ "monospace", "Courier New|Menlo|Noto Sans Mono|DejaVu Sans Mono|Liberation Mono|Roboto Mono|Droid Sans Mono" },
					{ "cursive", "Comic Sans MS|Apple Chancery|URW Chancery L|Dancing Script" },
					{ "fantasy", "Gabriola|Papyrus|Impact|Copperplate" },
					{ "math", "Cambria Math|STIX Two Math|Latin Modern Math|DejaVu Math TeX Gyre|Noto Sans Math" },
					{ "ui-serif", "New York" },
					{ "ui-monospace", "Cascadia Mono|SF Mono" },
					{ "ui-rounded", "SF Pro Rounded" },
					{ "generic(fangsong)", "FangSong|STFangsong|仿宋" },
					{ "generic(kai)", "KaiTi|STKaiti|Kaiti TC|楷体" },
					{ "generic(khmer-mul)", "Khmer OS Muol|Khmer OS Muol Light" },
					{ "generic(nastaliq)", "Noto Nastaliq Urdu|Jameel Noori Nastaleeq|Urdu Typesetting" },
				};
				for (const auto& [generic, candidates] : generic_fonts) {
					if (name == generic) {
						append_installed(candidates);
						break;
					}
				}
			}
		}
		if (choices.empty() && fonts.text.is_null()) {
#ifdef WINDOWS_ENABLED
			if (const char* candidates = chinese_generic_fonts(han_script, "standard")) {
				if (localize_whole_font) append_installed(candidates);
				else if (p_description.families.empty()) append_installed(candidates, CHINESE_FONT_RANGES);
			}
#endif
			if (choices.empty() || !choices.front().ranges.empty()) append_installed(serif_fonts);
		}
#ifdef WINDOWS_ENABLED
		if (localize_whole_font) {
			// Keep named and generic CSS families ahead of the browser's Han fallback.
			// Restrict it to Han and punctuation so it cannot replace Latin glyphs.
			if (const char* candidates = chinese_generic_fonts(han_script, "standard")) {
				append_installed(candidates, CHINESE_FONT_RANGES);
			}
		}
#endif
		append(text_font);
		// Font-relative metrics use the first face whose range includes space.
		const auto primary = std::find_if(choices.begin(), choices.end(), [](const FontChoice& choice) {
			return choice.ranges.empty() || std::any_of(choice.ranges.begin(), choice.ranges.end(), [](const auto& range) {
				return range.first <= U' ' && U' ' <= range.second;
			});
		});
		const auto& adjust = p_description.size_adjust;
		std::optional<double> reference;
		if (adjust.mode != css_font_size_adjust::Mode::none) {
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			if (adjust.mode == css_font_size_adjust::Mode::number) reference = adjust.value;
			else if (const auto aspect = measure_font_aspect(server, primary->font, adjust.metric)) {
				reference = std::min(*aspect * primary->size_adjust, std::numeric_limits<double>::max());
			}
			if (reference) {
				for (auto& choice : choices) {
					if (const auto aspect = measure_font_aspect(server, choice.font, adjust.metric)) {
						// The descriptor multiplies both the glyph size and the aspect being normalized.
						choice.size_adjust = std::min(*reference / *aspect, std::numeric_limits<double>::max());
					}
				}
			}
		}
		TypedArray<Font> unrestricted;
		bool has_size_adjust = reference.has_value();
		bool restricted = false;
		bool has_face_features = false;
		for (const auto& choice : choices) {
			has_face_features |= choice.has_face_features;
			has_size_adjust |= choice.size_adjust != 1.0;
			// TextServer can only apply one size per span, so scaled faces are matched explicitly.
			if (choice.ranges.empty() && choice.size_adjust == 1.0) {
				if (!unrestricted.has(choice.font)) unrestricted.append(choice.font);
			}
			else {
				restricted = true;
			}
		}
		// TextLine applies a span's feature dictionary to every font in its fallback list.
		// Split runs when feature defaults, kerning, or petite-cap fallback varies by face.
		const bool per_face_features = has_face_features || has_named_features || p_description.kerning == font_kerning_normal ||
			p_description.variant_features.count(PETITE_CAPS);
		if (unrestricted.is_empty()) unrestricted.append(text_font);
		FontStack stack{ primary->font, combine_fonts(unrestricted, per_face_features ? text_font : Ref<Font>()),
			{}, language, size_dependent, primary->metrics, primary->size_adjust, has_size_adjust, reference, adjust.metric };
		if (restricted || per_face_features || has_size_adjust) stack.choices = std::move(choices);
		return stack;
	}

	uint_ptr LiteHTMLFonts::create_font(const font_description& p_description, font_metrics* r_metrics) {
		const int32_t size = font_pixel_size(p_description.size);
		String key = String::num(p_description.weight, 9) + vformat("/%d/", int(p_description.style.kind));
		key += String::num(p_description.style.angle, 9);
		key += String("/") + String::num(p_description.width, 9);
		for (const auto& family : p_description.families) {
			const String name = String::utf8(family.name.c_str());
			key += vformat("/%d/%d/%s", int(family.generic), name.length(), name);
		}
		const String language = String::utf8(p_description.language.c_str());
		key += vformat("/lang=%d:%s", language.length(), language);
		if (p_description.language_is_declared) key += "/declared-lang";
		key += String::utf8(p_description.opentype_key().c_str());
		String size_key;
		const auto* cached = font_stacks.getptr(key);
		if (!cached) {
			size_key = key + "/size=" + String::num(p_description.size, 9);
			cached = font_stacks.getptr(size_key);
		}
		const auto stack = cached ? *cached : std::make_shared<const FontStack>(resolve_font_stack(p_description));
		if (!cached) {
			if (font_stacks.size() >= 512) font_stacks.clear();
			font_stacks.insert(stack->size_dependent ? size_key : key, stack);
		}
		const auto& font = stack->primary;
		r_metrics->size_adjust_reference = stack->size_adjust_reference;
		const auto primary_size = adjusted_font_size(p_description.size, stack->primary_size_adjust);

		const float base_ascent = primary_size > 0 ? font->get_ascent(primary_size) : 0.0f;
		const float base_descent = primary_size > 0 ? font->get_descent(primary_size) : 0.0f;
		const auto& overrides = stack->primary_metrics;
		const double effective_size = p_description.size * stack->primary_size_adjust;
		const float half_gap = p_description.use_font_line_gap ? resolve_font_metric(overrides.line_gap.horizontal, 0, effective_size) / 2 : 0;
		float ascent = resolve_font_metric(overrides.ascent.horizontal, base_ascent, effective_size) + half_gap;
		float descent = resolve_font_metric(overrides.descent.horizontal, base_descent, effective_size) + half_gap;
		r_metrics->half_line_gap = half_gap;
		r_metrics->has_metric_overrides = overrides.ascent.horizontal || overrides.descent.horizontal || half_gap != 0;
		r_metrics->emphasis_extent = 0;
		Ref<TextLine> emphasis;
		if (!p_description.emphasis_style.empty()) {
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			const String value = String::utf8(p_description.emphasis_style.c_str(), p_description.emphasis_style.size());
			const auto breaks = server->string_get_character_breaks(value, stack->language);
			const String mark = breaks.is_empty() ? value : value.substr(0, breaks[0]);
			const auto* choice = mark.is_empty() ? nullptr : stack->match(mark, 0, mark.length());
			const int mark_size = choice && choice->size_adjust != 1.0 ?
				adjusted_font_size(p_description.size / 2.0, choice->size_adjust) : std::max(1, size / 2);
			if (!mark.is_empty() && mark_size > 0) {
				const Ref<Font>& mark_font = choice ? choice->font : stack->unrestricted;
				const Ref<FontVariation> mark_variation = mark_font;
				Ref<FontVariation> variant;
				if (mark_variation.is_valid()) {
					variant = mark_variation->duplicate();
				}
				else {
					variant.instantiate();
					variant->set_base_font(mark_font);
					variant->set_fallbacks(mark_font->get_fallbacks());
				}
				Dictionary features = mark_font->get_opentype_features().duplicate();
				features["ruby"] = 1;
				variant->set_opentype_features(features);
				emphasis.instantiate();
				emphasis->add_string(mark, variant, mark_size, stack->language);
				const bool under = p_description.emphasis_position & text_emphasis_position_under;
				const int adjacent_decoration = under ? text_decoration_line_underline : text_decoration_line_overline;
				r_metrics->emphasis_extent = emphasis->get_size().y + ((p_description.decoration_line & adjacent_decoration) ? 2.0f : 0.0f);
				if (under) descent += r_metrics->emphasis_extent;
				else ascent += r_metrics->emphasis_extent;
			}
		}
		auto* data = new FontData{ stack, size, primary_size, ascent, descent, p_description, emphasis, {}, {} };
		r_metrics->font_size = size;
		r_metrics->ascent = ascent;
		r_metrics->descent = descent;
		const float underline_position = primary_size > 0 ? font->get_underline_position(primary_size) : 0.0f;
		r_metrics->underline_position = std::isfinite(underline_position) ? underline_position : 0.0f;
		r_metrics->height = r_metrics->ascent + r_metrics->descent;
		const auto rids = font->get_rids();
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		r_metrics->x_height = primary_size * 0.5f;
		r_metrics->cap_height = base_ascent;
		if (primary_size > 0 && !rids.is_empty() && server.is_valid()) {
			const RID primary = rids[0];
			r_metrics->x_height = glyph_height(server, primary, primary_size, U'x', r_metrics->x_height);
			r_metrics->cap_height = glyph_height(server, primary, primary_size, U'H', r_metrics->cap_height);
		}
		r_metrics->ch_width = stack->char_advance(U'0', p_description.size).value_or(primary_size * 0.5f);
		r_metrics->ic_width = stack->char_advance(U'\u6c34', p_description.size).value_or(static_cast<float>(primary_size));
		r_metrics->draw_spaces = p_description.decoration_line != text_decoration_line_none;
		r_metrics->sub_shift = primary_size * 0.2f;
		r_metrics->super_shift = primary_size * 0.35f;
		return reinterpret_cast<uint_ptr>(data);
	}

	void LiteHTMLFonts::delete_font(uint_ptr p_font) {
		delete reinterpret_cast<FontData*>(p_font);
	}

	pixel_t LiteHTMLFonts::text_width(const char* p_text, uint_ptr p_font) {
		return reinterpret_cast<FontData*>(p_font)->shape(String::utf8(p_text)).width();
	}

	pixel_t LiteHTMLFonts::measure_text(const String& p_text, uint_ptr p_font, font_metrics& r_metrics) {
		auto& font = *reinterpret_cast<FontData*>(p_font);
		const auto layout = font.shape(p_text);
		r_metrics.ascent = layout.ascent;
		r_metrics.descent = layout.descent;
		r_metrics.height = layout.ascent + layout.descent;
		return layout.width();
	}

	std::vector<size_t> LiteHTMLFonts::get_bidi_visual_order(const String& p_text, uint_ptr p_font,
		bool p_right_to_left, const std::vector<std::pair<int64_t, int64_t>>& p_ranges) {
		std::vector<size_t> order(p_ranges.size());
		for (size_t index = 0; index < order.size(); ++index) order[index] = index;
		if (!p_font || p_text.is_empty() || p_ranges.size() < 2) return order;

		auto& font = *reinterpret_cast<FontData*>(p_font);
		Ref<TextLine> line;
		line.instantiate();
		line->set_direction(p_right_to_left ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
		font.stack->add_text(*line.ptr(), p_text, font.description.size);

		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		if (server.is_null()) return order;
		std::vector<int64_t> first_glyph(p_ranges.size(), std::numeric_limits<int64_t>::max());
		int64_t glyph_index = 0;
		for (const Dictionary& glyph : server->shaped_text_get_glyphs(line->get_rid())) {
			const int64_t glyph_start = glyph["start"];
			const int64_t glyph_end = glyph["end"];
			if (glyph_end > glyph_start) {
				auto range = std::lower_bound(p_ranges.begin(), p_ranges.end(), glyph_start,
					[](const auto& p_range, int64_t p_position) { return p_range.second <= p_position; });
				for (; range != p_ranges.end() && range->first < glyph_end; ++range) {
					if (range->second > glyph_start) {
						const auto index = static_cast<size_t>(range - p_ranges.begin());
						first_glyph[index] = std::min(first_glyph[index], glyph_index);
					}
				}
			}
			++glyph_index;
		}
		std::stable_sort(order.begin(), order.end(), [&](size_t p_left, size_t p_right) {
			return first_glyph[p_left] < first_glyph[p_right];
		});
		return order;
	}

	pixel_t LiteHTMLFonts::trimmed_punctuation_width(const char* p_text, uint_ptr p_font, punctuation_trim p_trim) {
		return reinterpret_cast<FontData*>(p_font)->shape_punctuation(String::utf8(p_text), p_trim).width();
	}

	HTMLTextLayout LiteHTMLFonts::shape_text(uint_ptr p_font, const String& p_text) {
		return reinterpret_cast<FontData*>(p_font)->shape(p_text == "\t" ? String(" ") : p_text);
	}

	HTMLTextLayout LiteHTMLFonts::shape_text_for_outline(uint_ptr p_font, const String& p_text,
		bool p_right_to_left, float& r_scale) {
		const auto& font = *reinterpret_cast<const FontData*>(p_font);
		if (font.description.size <= 0) return {};
		const int size = std::max(font.size, 32);
		r_scale = font.description.size / size;
		Ref<TextLine> line;
		line.instantiate();
		const BidiText shaped = apply_unicode_bidi(p_text, p_right_to_left, font.description.unicode_bidi);
		line->set_direction(font.description.unicode_bidi == css_unicode_bidi::plaintext ? TextServer::DIRECTION_AUTO :
			(p_right_to_left ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR));
		font.stack->add_text(*line.ptr(), shaped.text, size);
		HTMLTextLayout layout{ line, 0, shaped.prefix, shaped.suffix };
		layout.spacing = HTMLTextSpacing::create(layout, p_text,
			font.description.letter_spacing / r_scale, font.description.word_spacing / r_scale);
		return layout;
	}

	HTMLShapedText LiteHTMLFonts::shape_styled_text(uint_ptr p_font, const String& p_run, text_justify_adjustment p_justification,
		float p_advance, int64_t p_start, int64_t p_end, punctuation_trim p_trim) {
		FontData& font = *reinterpret_cast<FontData*>(p_font);
		const String text = p_end < 0 ? p_run : p_run.substr(p_start, p_end - p_start);
		HTMLTextLayout layout = shape_text(p_font, p_run);
		const auto& description = font.description;
		if (p_trim != punctuation_trim::none) {
			layout = font.shape_punctuation(text, p_trim);
		}
		else if (p_end >= 0) {
			layout = layout.subrange(p_start, p_end);
			if (!layout.is_valid()) layout = shape_text(p_font, text);
			layout.spacing = HTMLTextSpacing::create(layout, text, description.letter_spacing,
				description.word_spacing, p_justification);
		}
		else if (text == "\t" || text == " ") {
			const float native_width = layout.native_width();
			if (p_advance != layout.width()) layout.spacing = HTMLTextSpacing::create(layout, " ", 0, p_advance - native_width);
		}
		else if (p_justification.spacing > 0.0f) {
			layout.spacing = HTMLTextSpacing::create(layout, text, description.letter_spacing,
				description.word_spacing, p_justification);
		}
		float thickness = 0.0f;
		if (description.decoration_line != text_decoration_line_none) {
			const auto& specified = description.decoration_thickness;
			thickness = layout.underline_thickness();
			if (!specified.is_predefined()) {
				thickness = specified.calc_percent(font.size);
			}
			else if (specified.predef() == text_decoration_thickness_from_font && font.primary_size > 0) {
				const float metric = font.stack->primary->get_underline_thickness(font.primary_size);
				if (std::isfinite(metric) && metric > 0.0f) {
					thickness = metric;
				}
			}
			thickness = std::max(1.0f, thickness);
		}
		return { layout, description.decoration_line, description.decoration_style,
			description.decoration_color, thickness, font.emphasis, font.get_emphasis_centers(text, layout),
			description.emphasis_color, (description.emphasis_position & text_emphasis_position_under) != 0 };
	}
} // namespace dockit
