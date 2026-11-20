#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include <litehtml/background.h>

namespace dockit {
	class HTMLResources;

	godot::Ref<godot::Texture2D> rasterize_cross_fade(const litehtml::background_layer& p_tile,
		const litehtml::css_cross_fade& p_mix, const litehtml::string& p_base, const litehtml::web_color& p_current,
		int p_direction, litehtml::image_rendering p_sampling, HTMLResources& p_resources);
}
