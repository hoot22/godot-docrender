#pragma once

#include "css_clip_path.h"

namespace litehtml {
	struct css_offset_position {
		enum class mode { normal, automatic, explicit_position };
		mode kind = mode::automatic;
		css_image_position position;
	};

	struct css_offset_rotation {
		bool follows_path = true;
		double angle = 0;
	};

	struct css_offset_ray {
		enum class size { closest_side, farthest_side, closest_corner, farthest_corner, sides };
		double angle = 0;
		size extent = size::closest_side;
		bool contain = false;
		std::optional<css_image_position> origin;
	};

	struct css_offset_path {
		css_clip_path_ref shape;
		std::optional<css_offset_ray> ray;
		css_shape_box box = css_shape_box::border;

		bool enabled() const { return shape || ray.has_value(); }
	};

	struct css_motion_path {
		css_offset_path path;
		css_offset_position position{ css_offset_position::mode::normal, {} };
		css_offset_position anchor;
		css_length distance;
		css_offset_rotation rotation;

		bool enabled() const { return path.enabled(); }
	};

	std::optional<css_offset_position> parse_offset_position(const css_token_vector& p_tokens, bool p_accept_normal);
	std::optional<css_offset_rotation> parse_offset_rotation(const css_token_vector& p_tokens);
	std::optional<css_offset_path> parse_offset_path(const css_token_vector& p_tokens, const std::string& p_base);
	std::optional<css_motion_path> parse_motion_shorthand(const css_token_vector& p_tokens, const std::string& p_base);
}
