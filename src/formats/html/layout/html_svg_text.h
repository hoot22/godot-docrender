#pragma once

#include "html_polygon_clip.h"

namespace litehtml {
	class element;
}

namespace dockit {
	HTMLClipOutline resolve_svg_text_outline(const litehtml::element& p_text, const godot::Vector2& p_viewport,
		double p_tolerance); // 计算 SVG 水平文本块的可见字形轮廓
}
