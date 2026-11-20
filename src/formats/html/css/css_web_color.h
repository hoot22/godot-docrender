// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LH_WEB_COLOR_H
#define LH_WEB_COLOR_H

#include "css_token_compat.h"
#include "css_types.h"
#include <memory>
#include <optional>

namespace litehtml
{
	class document_container;
	struct css_color_data;
	enum class system_color : uint8_t {
		none, canvas, canvas_text, field, field_text, button_face, button_text, button_border,
		gray_text, link_text, active_text, visited_text, highlight, highlight_text, accent, accent_text,
		button_hover, button_active, border_hover, border_active, disabled_face, disabled_border, disabled_text,
		focus_ring, gauge_track, gauge_border, scrollbar_track, scrollbar_thumb, scrollbar_pressed,
		menu_face, menu_border, menu_hover, list_selected, list_hover
	};

	struct web_color
	{
		byte red   = 0;
		byte green = 0;
		byte blue  = 0;
		byte alpha = 255;
		bool is_current_color = false;
		system_color system = system_color::none;
		color_scheme scheme = color_scheme::light;
		// Share floating-point samples and deferred color expressions across inherited styles.
		std::shared_ptr<const css_color_data> css_data;
		std::optional<color_scheme> computed_scheme;

		bool needs_current_color() const;
		web_color compute_for_scheme(color_scheme used_scheme) const;
		web_color resolve(const web_color& current) const;
		web_color to_srgb(const web_color& current = black) const;

		static const web_color transparent;
		static const web_color black;
		static const web_color white;
		static const web_color current_color;

		web_color() {}
		web_color(byte r, byte g, byte b, byte a = 255) : red(r), green(g), blue(b), alpha(a) {}
		web_color(bool is_current_color) : is_current_color(is_current_color) {}
		explicit web_color(system_color value) : system(value) {}

		bool operator==(web_color color) const { return system == color.system && red == color.red && green == color.green && blue == color.blue && alpha == color.alpha; }
		bool operator!=(web_color color) const { return !(*this == color); }

		web_color darken(double fraction) const;
		string to_string() const;
	};
	web_color system_color_value(system_color value, color_scheme scheme);

	bool parse_color(const css_token& token, web_color& color, document_container* container);
	bool parse_color_at_depth(const css_token& token, web_color& color, document_container* container, unsigned depth);
	bool equal_css_colors(const web_color& first, const web_color& second);
	web_color interpolate_css_color(const web_color& from, const web_color& to, double progress);
}

#endif  // LH_WEB_COLOR_H
