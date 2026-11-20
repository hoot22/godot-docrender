#pragma once

#include "html_polygon_clip.h"

#include <godot_cpp/variant/transform2d.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace litehtml {
	class element;
}

namespace dockit {
	std::optional<HTMLClipPolygon> resolve_svg_clip(const litehtml::element& p_target,
		std::string_view p_url, const std::string& p_css_base, const godot::Rect2& p_box,
		const godot::Vector2& p_viewport, const godot::Transform2D& p_visual_matrix); // 将 SVG 裁剪引用转换为文档坐标下的填充轮廓
}
