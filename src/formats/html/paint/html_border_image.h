#pragma once

#include "formats/html/css/css_border_image.h"

#include <godot_cpp/variant/rect2.hpp>
#include <litehtml/borders.h>

#include <vector>

namespace dockit {
	struct HTMLBorderImagePatch {
		godot::Rect2 source;
		godot::Rect2 destination;
		godot::Vector2 origin;
		godot::Vector2 tile;
		godot::Vector2 period;
	};

	godot::Rect2 get_border_image_area(const litehtml::css_border_image& p_style,
		const godot::Rect2& p_box, const litehtml::borders& p_borders);
	std::vector<HTMLBorderImagePatch> make_border_image_patches(const litehtml::css_border_image& p_style,
		const litehtml::borders& p_borders, const godot::Rect2& p_area, const godot::Vector2& p_source_size,
		const godot::Vector2& p_natural_size, const godot::Vector2& p_texture_size, bool p_flip_x);
}
