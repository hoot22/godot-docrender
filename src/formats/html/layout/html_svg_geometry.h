#pragma once

#include "formats/html/css/css_svg_path.h"

#include <godot_cpp/variant/transform2d.hpp>

#include <string_view>
#include <memory>

namespace litehtml {
	class element;
	class document;
}

namespace dockit::svg {
	const litehtml::element* resolve_fragment(const litehtml::element& p_root, std::string_view p_url); // 在当前文档解析片段引用
	std::shared_ptr<litehtml::document> reference_document(const litehtml::element& p_target,
		std::string_view p_url, const std::string& p_css_base); // 取得片段引用所属的文档
	bool is_svg(const litehtml::element& p_node); // 检查节点是否位于 SVG 图形树
	double length_attribute(const litehtml::element& p_node, const char* p_name, double p_basis, double p_default = 0);
	godot::Transform2D transform_attribute(const litehtml::element& p_node); // 读取 SVG 变换列表
	css::SVGPath shape_path(const litehtml::element& p_shape, const godot::Vector2& p_basis, std::string_view p_tag); // 返回 SVG 图形的等效路径

	class PathMeasure {
		struct Segment {
			godot::Vector2 start;
			godot::Vector2 end;
			godot::Vector2 tangent;
			double first_distance;
			double last_distance;
		};
		std::vector<Segment> segments;
		bool closed = false;

	public:
		struct Sample {
			godot::Vector2 point;
			godot::Vector2 tangent;
		};

		PathMeasure(const css::SVGPath& p_path, const godot::Transform2D& p_transform, double p_tolerance, bool p_reverse);
		double length() const { return segments.empty() ? 0 : segments.back().last_distance; }
		bool is_closed() const { return closed; }
		Sample sample(double p_distance) const; // 在给定距离取位置与切线，闭合路径允许回绕
		godot::Vector2 end() const { return segments.empty() ? godot::Vector2() : segments.back().end; }
	};
}
