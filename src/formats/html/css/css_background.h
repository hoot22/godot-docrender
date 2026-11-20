// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LH_BACKGROUND_H
#define LH_BACKGROUND_H

#include "css_types.h"
#include "css_length.h"
#include "css_position.h"
#include "css_web_color.h"
#include "css_borders.h"
#include "css_blend_mode.h"
#include "css_gradient.h"

#include <functional>

namespace litehtml
{
	class element;
	class render_item;
	class document_container;
	class background_layer
	{
	public:
		// border_box defines draw boundary. Everything must be drawn inside this rectangle only.
		position border_box;
		// border_radius defines radius of the border_box.
		border_radiuses border_radius;
		// clip_box defines clipping rectangle. Works like border_box. Container must set additional clipping.
		position clip_box;
		// origin_box defines origin rectangle.
		position origin_box;
		background_attachment attachment;
		background_repeat_pair repeat;
		litehtml::size tile_spacing; // Additional distance between tile origins
		// is_root is true for root element. Container can use this flag to apply background to the top window.
		bool is_root;
		css_image_orientation image_orientation = css_image_orientation::from_image;
		bool flip_image_x = false;
		bool fill_image_tile = false;

		background_layer() :
				attachment(background_attachment_scroll),
				is_root(false)
				{}

		class image
		{
		public:
			string url;
			string base_url;
		};

		struct color_point
		{
			float offset;
			web_color color;
			optional<float> hint;
			color_point() { offset = 0.0; }
			color_point(float _offset, web_color _color) : offset(_offset), color(_color) {}
		};

		class color
		{
		public:
			web_color color;
		};

		class gradient_base
		{
		public:
			vector<color_point>  color_points;
			color_space_t        color_space       = color_space_none;
			hue_interpolation_t  hue_interpolation = hue_interpolation_none;

			void color_points_transparent_fix();
			bool prepare_color_points(float len, string_id grad_type, const vector<gradient::color_stop>& colors, const web_color& current_color);
		};

		class linear_gradient : public gradient_base
		{
		public:
			pointF start;
			pointF end;
		};

		class radial_gradient : public gradient_base
		{
		public:
			pointF position;
			pointF radius;
		};

		class conic_gradient : public gradient_base
		{
		public:
			pointF position; // position is the center of the conic gradient
			float angle = 0; // angle is the angle of the gradient in degrees, starting from 0 at the top and going clockwise
			float radius = 0; // radius is the distance from the center to the farthest corner of the background box
		};
	};

	class background
	{
	public:
		enum layer_type
		{
			type_none,
			type_color,
			type_solid_image,
			type_cross_fade,
			type_image,
			type_linear_gradient,
			type_radial_gradient,
			type_conic_gradient,
		};

		vector<image>			m_image;
		string					m_baseurl;
		web_color				m_color;
		web_color				m_current_color;
		int					m_direction = static_cast<int>(css_direction::ltr);
		int_vector				m_attachment;
		vector<css_axis_position> m_position_x;
		vector<css_axis_position> m_position_y;
		size_vector				m_size;
		vector<background_repeat> m_repeat_x;
		vector<background_repeat> m_repeat_y;
		int_vector				m_clip;
		int_vector				m_origin;
		int_vector				m_blend_modes;

		bool is_empty() const
		{
			if(m_color.to_srgb(m_current_color).alpha != 0)
				return false;
			if(m_image.empty())
				return true;
			for(const auto& img : m_image)
			{
				if(!img.is_empty()) return false;
			}
			return true;
		}
		int get_layers_number() const
		{
			if(m_color.to_srgb(m_current_color).alpha != 0)
			{
				return (int) m_image.size() + 1;
			}
			return (int) m_image.size();
		}
		bool get_layer(int idx, position pos, const element* el, const std::shared_ptr<render_item>& ri, background_layer& layer) const;
		layer_type get_layer_type(int idx) const;
		std::unique_ptr<background_layer::image> get_image_layer(int idx) const;
		std::unique_ptr<background_layer::color> get_color_layer(int idx) const;
		std::unique_ptr<background_layer::linear_gradient> get_linear_gradient_layer(int idx, const background_layer& layer) const;
		std::unique_ptr<background_layer::radial_gradient> get_radial_gradient_layer(int idx, const background_layer& layer) const;
		std::unique_ptr<background_layer::conic_gradient> get_conic_gradient_layer(int idx, const background_layer& layer) const;
		void draw_layers(uint_ptr hdc, const position& box, const element* el, const std::shared_ptr<render_item>& ri,
			document_container& container, const std::function<void(background_layer&)>& prepare) const;
		void draw_layer(uint_ptr hdc, int idx, const background_layer& layer, document_container* container) const;

	private:
		css_blend_mode blend_mode(int idx) const;
	};

	enum class css_mask_mode { match_source, alpha, luminance };
	enum class css_mask_composite { add, subtract, intersect, exclude };

	struct css_mask {
		background layers;
		int_vector modes;
		int_vector composites;
		bool active = false;
	};
}

#endif  // LH_BACKGROUND_H
