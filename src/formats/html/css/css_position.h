// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LH_CSS_POSITION_H
#define LH_CSS_POSITION_H

#include "css_length.h"

namespace litehtml
{
	enum class object_fit { fill, contain, cover, none, scale_down, cover_scale_down };

	enum class css_position_edge { near, far, flow_start, flow_end };

	struct css_axis_position
	{
		css_length offset;
		css_position_edge edge = css_position_edge::near;

		void resolve_flow_direction(bool reverse)
		{
			if (edge == css_position_edge::flow_start)
				edge = reverse ? css_position_edge::far : css_position_edge::near;
			else if (edge == css_position_edge::flow_end)
				edge = reverse ? css_position_edge::near : css_position_edge::far;
		}

		pixel_t resolve(pixel_t available) const
		{
			const auto distance = offset.calc_percent(available);
			return edge == css_position_edge::far ? available - distance : distance;
		}
	};

	struct css_image_position
	{
		css_axis_position x{ css_length(50, css_units_percentage) };
		css_axis_position y{ css_length(50, css_units_percentage) };
	};

	struct css_position
	{
		css_length	x;
		css_length	y;
		css_length	width;
		css_length	height;
	};

	struct css_size
	{
		css_length	width;
		css_length	height;

		css_size() = default;
		css_size(css_length width, css_length height) : width(width), height(height) {}
	};

	using size_vector = std::vector<css_size>;
}

#endif  // LH_CSS_POSITION_H
