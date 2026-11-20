#pragma once

#include "css_position.h"
#include "css_svg_path.h"

#include <array>

namespace dockit::css {
	struct ComputeContext;

	struct ShapeControlPoint {
		enum class Anchor { origin, start, end };
		litehtml::css_image_position position;
		Anchor anchor = Anchor::origin;
	};

	struct ShapeCommand {
		enum class Kind { move, line, horizontal, vertical, quadratic, cubic, smooth_quadratic, smooth_cubic, arc, close };
		Kind kind = Kind::line;
		bool relative = false;
		litehtml::css_image_position end;
		std::array<ShapeControlPoint, 2> controls;
		std::array<litehtml::css_length, 2> radii;
		bool single_radius = true;
		bool large_arc = false;
		bool sweep = false;
		double rotation_degrees = 0;
	};

	struct CSSShapePath {
		litehtml::css_image_position start;
		std::vector<ShapeCommand> commands;
		bool even_odd = false;
	};

	std::optional<CSSShapePath> parse_shape_path(const css_token_vector& p_tokens); // 解析 shape() 的起点、命令和填充规则
	void compute_shape_path(CSSShapePath& r_path, const ComputeContext& p_context,
		const litehtml::font_metrics& p_font, bool p_rtl); // 计算相对长度和逻辑边，保留布局百分比
	std::optional<SVGPath> resolve_shape_path(const CSSShapePath& p_path,
		litehtml::pixel_t p_width, litehtml::pixel_t p_height); // 根据参考盒尺寸解析路径段
}
