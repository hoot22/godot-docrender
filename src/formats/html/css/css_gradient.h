// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LITEHTML_GRADIENT_H
#define LITEHTML_GRADIENT_H

#include "css_length.h"
#include "css_string_id.h"
#include "css_types.h"
#include "css_web_color.h"
#include <memory>
#include <optional>

namespace litehtml
{
	class document_container;
	enum gradient_side
	{
		gradient_side_none = 0,
		gradient_side_left = 0x01,
		gradient_side_right = 0x02,
		gradient_side_top = 0x04,
		gradient_side_bottom = 0x08,
		gradient_side_x_center = 0x10,
		gradient_side_y_center = 0x20,
		gradient_side_x_length = 0x40,
		gradient_side_y_length = 0x80,
	};
	static_assert(gradient_side_left == (1 << background_position_left), "parse_bg_position is also used to parse radial gradient position");

	enum radial_shape_t
	{
		radial_shape_none,
		radial_shape_circle,
		radial_shape_ellipse,
	};

#define radial_extent_strings "closest-corner;closest-side;farthest-corner;farthest-side"
	enum radial_extent_t
	{
		radial_extent_none,
		radial_extent_closest_corner,
		radial_extent_closest_side,
		radial_extent_farthest_corner,
		radial_extent_farthest_side,
	};

#define color_space_strings \
			"srgb;"\
			"srgb-linear;"\
			"display-p3;"\
			"a98-rgb;"\
			"prophoto-rgb;"\
			"rec2020;"\
			"lab;"\
			"oklab;"\
			"xyz;"\
			"xyz-d50;"\
			"xyz-d65;"\
			"hsl;"\
			"hwb;"\
			"lch;"\
			"oklch"

	enum color_space_t
	{
		color_space_none,

		// rectangular-color-space
		color_space_srgb,
		color_space_srgb_linear,
		color_space_display_p3,
		color_space_a98_rgb,
		color_space_prophoto_rgb,
		color_space_rec2020,
		color_space_lab,
		color_space_oklab,
		color_space_xyz,
		color_space_xyz_d50,
		color_space_xyz_d65,

		// polar-color-space
		color_space_hsl, color_space_polar_start = color_space_hsl,
		color_space_hwb,
		color_space_lch,
		color_space_oklch,
	};

#define hue_interpolation_strings "shorter;longer;increasing;decreasing"
	enum hue_interpolation_t
	{
		hue_interpolation_none,
		hue_interpolation_shorter,
		hue_interpolation_longer,
		hue_interpolation_increasing,
		hue_interpolation_decreasing
	};

	class gradient
	{
	public:
		// https://drafts.csswg.org/css-images-4/#typedef-color-stop-list
		// color_stop represents <linear-color-stop> | <linear-color-hint> | <angular-color-stop> | <angular-color-hint>
		// <linear-color-stop>  = <color> <length-percentage>{1,2}?
		// <linear-color-hint>  = <length-percentage>
		// <angular-color-stop> = <color> <angle-percentage>{1,2}?
		// <angular-color-hint> = <angle-percentage>
		// If two lengths/angles are specified they create two color-stops with the same color.
		class color_stop
		{
		public:
			bool is_color_hint = false;
			web_color color;
			optional<css_length> length;
			optional<float> angle;

			color_stop() {}
			color_stop(web_color color)                    : color(color)                        {}
			color_stop(web_color color, css_length length) : color(color), length(length)        {}
			color_stop(web_color color, float angle)       : color(color), angle(angle)          {}
			color_stop(css_length length)                  : is_color_hint(true), length(length) {}
			color_stop(float angle)                        : is_color_hint(true), angle(angle)   {}
		};

		string_id m_type;
		uint32_t m_side;
		float angle;
		vector<color_stop> m_colors;
		css_length position_x;
		css_length position_y;
		radial_shape_t radial_shape;
		radial_extent_t radial_extent;
		css_length radial_radius_x;
		css_length radial_radius_y;
		float conic_from_angle;
		color_space_t color_space;
		hue_interpolation_t hue_interpolation;


		explicit gradient(string_id type = empty_id)
		{
			m_type = type;

			// linear gradient:       m_side default is gradient_side_none (use angle)
			// radial,conic gradient: m_side default is gradient_side_x_center | gradient_side_y_center (see parse_gradient)
			m_side = gradient_side_none;
			// linear gradient angle, used when m_side == gradient_side_none
			angle = 180; // to bottom
			// radial,conic position (together with m_side)
			position_x.predef(0);
			position_y.predef(0);

			// actual default depends on the number of radii provided, see parse_radial_gradient_shape_size_position_interpolation
			radial_shape = radial_shape_ellipse;

			// <radial-size>  https://drafts.csswg.org/css-images-3/#valdef-radial-gradient-radial-size
			// if radius is specified radial_extent will be set to none, see parse_radial_size
			radial_extent = radial_extent_farthest_corner;
			radial_radius_x.predef(0);
			radial_radius_y.predef(0);

			conic_from_angle = 0;

			// If the host syntax does not define what color space interpolation should take place in, it defaults to Oklab.
			color_space = color_space_oklab;
			// Unless otherwise specified, if no specific hue interpolation algorithm is selected by the host syntax, the default is shorter.
			hue_interpolation = hue_interpolation_shorter;
		}

		bool is_empty() const
		{
			return m_type == empty_id || m_colors.empty();
		}

		bool is_linear() const { return m_type == _linear_gradient_ || m_type == _repeating_linear_gradient_; }
		bool is_radial() const { return m_type == _radial_gradient_ || m_type == _repeating_radial_gradient_; }
		bool is_conic() const { return m_type == _conic_gradient_ || m_type == _repeating_conic_gradient_; }

		static gradient transparent;
	};

	struct css_image_set;
	struct css_cross_fade;
	struct css_light_dark_image;
	enum class css_direction { ltr, rtl };
	inline constexpr char direction_keywords[] = "ltr;rtl";

	class image
	{
	public:
		enum type
		{
			type_none,
			type_url,
			type_solid,
			type_gradient,
			type_set,
			type_cross_fade,
			type_light_dark,
		};
		type type;
		string url;
		gradient m_gradient;
		std::shared_ptr<const css_image_set> m_set;
		std::shared_ptr<const css_cross_fade> m_cross_fade;
		std::shared_ptr<const css_light_dark_image> m_light_dark;
		double density = 1.0;
		css_image_orientation orientation = css_image_orientation::from_image;
		bool from_image_set = false;
		optional<double> candidate_density;
		std::optional<web_color> fallback_color;
		std::optional<css_direction> direction;

		image() : type(type_none) {}

		bool is_empty() const
		{
			switch (type)
			{
			case type_none:
				return true;
			case type_url:
				return url.empty() && !fallback_color;
			case type_solid:
				return false;
			case type_gradient:
				return m_gradient.is_empty();
			case type_set:
			case type_cross_fade:
			case type_light_dark:
				return false;
			}
			return true;
		}
	};

	using css_image_ptr = std::shared_ptr<const image>;

	bool parse_gradient(const css_token& token, gradient& gradient, document_container* container);
}

#endif //LITEHTML_GRADIENT_H
