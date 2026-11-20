#pragma once

#include "css_borders.h"
#include "css_position.h"
#include "css_shape_path.h"
#include <array>

namespace litehtml {
	enum class css_clip_shape { box, inset, rect, xywh, circle, ellipse, polygon, path, shape, reference };
	enum class css_shape_box { margin, border, padding, content };

	struct css_clip_path {
		css_clip_shape shape = css_clip_shape::box;
		css_shape_box box = css_shape_box::border;
		std::array<css_length, 4> dimensions{};
		css_border_radius radii;
		css_image_position center;
		bool center_explicit = false;
		std::vector<std::array<css_length, 2>> vertices;
		css_length polygon_radius;
		std::shared_ptr<const dockit::css::SVGPath> svg_path;
		std::optional<dockit::css::CSSShapePath> shape_path;
		std::string url;
		std::string base_url;
		bool even_odd = false;
	};
	using css_clip_path_ref = std::shared_ptr<const css_clip_path>;

	std::optional<css_shape_box> parse_css_shape_box(const css_token& token);
	std::optional<css_clip_path_ref> parse_css_clip_path(const css_token_vector& tokens, const std::string& base_url); // 解析 CSS 裁剪路径与参考盒
}
