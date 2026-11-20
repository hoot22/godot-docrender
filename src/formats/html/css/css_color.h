#pragma once

#include <array>

namespace dockit::css {
	struct css_token;
}

namespace litehtml {
	using dockit::css::css_token;
	class document_container;
	struct web_color;

	enum class css_color_space {
		srgb, srgb_linear, display_p3, display_p3_linear, a98_rgb, prophoto_rgb, rec2020,
		xyz_d50, xyz_d65, lab, lch, oklab, oklch, hsl, hwb,
	};

	web_color make_css_color(css_color_space space, std::array<double, 4> components, unsigned missing = 0);
	std::array<double, 4> css_color_components(const web_color& color, css_color_space space);
	bool parse_absolute_color(const css_token& token, web_color& color);
	bool parse_relative_color(const css_token& token, web_color& color, document_container* container, unsigned depth);
	bool parse_color_mix(const css_token& token, web_color& color, document_container* container, unsigned depth);
	bool parse_adaptive_color(const css_token& token, web_color& color, document_container* container, unsigned depth);
}
