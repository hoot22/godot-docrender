#pragma once

#include "css_token_compat.h"

#include <cstdint>
#include <map>
#include <optional>
#include <utility>

namespace litehtml {
	using css_unicode_ranges = std::vector<std::pair<char32_t, char32_t>>;
	using css_font_features = std::map<uint32_t, uint32_t>;
	using css_font_variations = std::map<uint32_t, float>;
	constexpr float default_oblique_angle = 14.0f;

	struct css_font_style;
	// Advance past one font-style value, including its optional oblique angle.
	bool parse_css_font_style(const css_token_vector& tokens, int& index, css_font_style& result);
	std::optional<float> parse_css_font_width(const css_token& token, bool keywords_only = false);
	std::optional<css_font_features> parse_css_font_features(const css_token_vector& tokens);
	std::optional<css_font_variations> parse_css_font_variations(const css_token_vector& tokens);

	struct css_font_face_source {
		string value;
		bool local = false;
		bool needs_variations = false;

		bool operator==(const css_font_face_source& other) const {
			return value == other.value && local == other.local && needs_variations == other.needs_variations;
		}
	};

	struct css_font_metric_override {
		std::optional<double> horizontal;
		std::optional<double> vertical;

		bool operator==(const css_font_metric_override& other) const {
			return horizontal == other.horizontal && vertical == other.vertical;
		}
	};

	struct css_font_metric_overrides {
		css_font_metric_override ascent;
		css_font_metric_override descent;
		css_font_metric_override line_gap;

		bool operator==(const css_font_metric_overrides& other) const {
			return ascent == other.ascent && descent == other.descent && line_gap == other.line_gap;
		}
	};

	struct css_font_face {
		string family;
		string base_url;
		std::vector<css_font_face_source> sources;
		css_unicode_ranges unicode_ranges;
		font_style style = font_style_normal;
		float min_angle = 0.0f;
		float max_angle = 0.0f;
		float min_weight = 400.0f;
		float max_weight = 400.0f;
		std::optional<std::pair<float, float>> width_range;
		css_font_features features;
		css_font_variations variations;
		css_font_metric_overrides metrics;
		double size_adjust = 1.0;

		bool operator==(const css_font_face& other) const {
			return family == other.family && base_url == other.base_url && sources == other.sources &&
				unicode_ranges == other.unicode_ranges &&
				style == other.style && min_angle == other.min_angle && max_angle == other.max_angle &&
				min_weight == other.min_weight && max_weight == other.max_weight && width_range == other.width_range &&
				features == other.features && variations == other.variations && metrics == other.metrics && size_adjust == other.size_adjust;
		}
	};

	std::optional<css_font_face> parse_font_face_rule(const css_token_vector& prelude,
		const css_token_vector& block, const string& base_url);
	bool supports_font_format(const string& format);
	bool supports_font_tech(const string& tech);
}
