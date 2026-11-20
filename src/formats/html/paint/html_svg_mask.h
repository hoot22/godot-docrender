#pragma once

#include "formats/html/css/css_background.h"

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include <optional>
#include <string_view>

namespace dockit {
	class HTMLPlaneTransform;

	struct HTMLSVGMask {
		godot::Ref<godot::Texture2D> texture;
		godot::Rect2 box;
		bool luminance = false;
		bool linear_rgb = false;
	};

	// An empty raster represents a recognized mask source with no coverage.
	std::optional<HTMLSVGMask> rasterize_svg_mask(const litehtml::element& p_target, std::string_view p_url,
		const std::string& p_css_base, const godot::Rect2& p_target_box, const godot::Rect2& p_ink,
		const godot::Vector2& p_viewport, const HTMLPlaneTransform& p_matrix, litehtml::css_mask_mode p_mode);
}
