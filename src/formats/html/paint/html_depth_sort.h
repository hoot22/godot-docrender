#pragma once

#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <vector>

namespace dockit {
	struct HTMLDepthLayer {
		godot::Rect2 bounds;
		godot::Vector3 depth;
	};

	struct HTMLDepthSlice {
		std::size_t layer;
		std::vector<godot::Vector2> polygon;
	};

	// Input order is CSS paint order. Output polygons are ordered back to front.
	std::vector<HTMLDepthSlice> sort_html_depth_layers(const std::vector<HTMLDepthLayer>& p_layers);

	inline godot::real_t html_depth_at(const godot::Vector3& p_plane, const godot::Vector2& p_point) {
		return p_plane.dot(godot::Vector3(p_point.x, p_point.y, 1));
	}
}
