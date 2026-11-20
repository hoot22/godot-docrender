#pragma once

namespace litehtml {
	enum class css_blend_mode {
		normal, multiply, screen, overlay, darken, lighten, color_dodge, color_burn,
		hard_light, soft_light, difference, exclusion, hue, saturation, color, luminosity, plus_lighter,
	};

#define DOCKIT_CSS_BLEND_MODES \
		"normal;multiply;screen;overlay;darken;lighten;color-dodge;color-burn;" \
		"hard-light;soft-light;difference;exclusion;hue;saturation;color;luminosity"
	inline constexpr const char* css_blend_mode_keywords = DOCKIT_CSS_BLEND_MODES;
	inline constexpr const char* css_mix_blend_mode_keywords = DOCKIT_CSS_BLEND_MODES ";plus-lighter";
#undef DOCKIT_CSS_BLEND_MODES
}
