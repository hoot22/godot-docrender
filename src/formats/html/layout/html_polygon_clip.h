#pragma once

#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>

#include <vector>

namespace dockit {
	class HTMLPlaneTransform;
	struct HTMLClipPolygon;

	struct HTMLClipOutline {
		std::vector<std::vector<godot::Vector2>> contours;
		bool even_odd = false;
		std::vector<HTMLClipPolygon> clips; // 与当前轮廓的填充区域求交集

		template<class Visitor>
		void for_each_edge(Visitor&& p_visit) const {
			for (const auto& vertices : contours) {
				if (vertices.size() < 3) continue;
				auto a = vertices.back();
				for (const auto& b : vertices) {
					p_visit(a, b);
					a = b;
				}
			}
		}

		bool contains_point(const godot::Vector2& p_point) const; // 按填充规则检查多边形命中
		godot::Rect2 bounds() const; // 返回包含当前轮廓及嵌套裁剪的保守边界
	};

	struct HTMLClipPolygon {
		std::vector<HTMLClipOutline> outlines; // 各轮廓独立填充后取并集
		std::vector<HTMLClipPolygon> clips; // 与组合后的整个填充区域求交集

		godot::Rect2 bounds() const; // 获取非退化轮廓的外接矩形
		bool contains_point(const godot::Vector2& p_point) const; // 检查是否命中任一填充轮廓
		HTMLClipPolygon transformed(const HTMLPlaneTransform& p_matrix) const; // 将所有嵌套轮廓映射到另一坐标系
	};

	void round_clip_polygon_vertices(std::vector<godot::Vector2>& r_vertices, double p_radius, double p_tolerance); // 将顶点替换为两侧相切的圆弧
	godot::Rect2 intersect_clip_polygons(const std::vector<HTMLClipPolygon>& p_polygons); // 求多个填充区域的交集外接矩形
}
