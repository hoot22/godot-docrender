#pragma once

#include <optional>
#include <string>
#include <vector>

namespace dockit::css {
	struct SVGPathPoint {
		double x = 0;
		double y = 0;

		SVGPathPoint operator+(SVGPathPoint p_other) const { return {x + p_other.x, y + p_other.y}; }
		SVGPathPoint operator-(SVGPathPoint p_other) const { return {x - p_other.x, y - p_other.y}; }
		SVGPathPoint operator*(double p_factor) const { return {x * p_factor, y * p_factor}; }
		bool operator==(SVGPathPoint p_other) const { return x == p_other.x && y == p_other.y; }
	};

	struct SVGPathSegment {
		enum class Kind { line, quadratic, cubic, arc };
		Kind kind = Kind::line;
		SVGPathPoint end;
		SVGPathPoint control_1;
		SVGPathPoint control_2;
		SVGPathPoint radii;
		double rotation = 0;
		bool large_arc = false;
		bool sweep = false;
	};

	struct SVGSubpath {
		SVGPathPoint start;
		std::vector<SVGPathSegment> segments;
		bool closed = false;
	};

	using SVGPath = std::vector<SVGSubpath>;
	using SVGContours = std::vector<std::vector<SVGPathPoint>>;

	class SVGPathBuilder {
		SVGPath subpaths;
		SVGPathPoint current;
		bool open = false;

	public:
		bool empty() const { return subpaths.empty(); }
		SVGPathPoint position() const { return current; }
		SVGPathPoint reflected_control(SVGPathSegment::Kind p_kind) const;	// 获取平滑曲线的隐式控制点
		bool move_to(SVGPathPoint p_position);								// 从绝对坐标开始新轮廓
		bool append(const SVGPathSegment& p_segment);						// 添加已解析的路径段
		void close();														// 闭合当前轮廓并恢复起点
		SVGPath take_path();												// 移交构造完成的路径
	};

	std::optional<SVGPath> parse_svg_path(const std::string& p_data);		// 解析 SVG 路径命令并解析为绝对坐标
	SVGPath parse_svg_geometry_path(const std::string& p_data);				// 按 SVG 属性规则保留首个语法错误前的有效路径
	std::vector<SVGPathPoint> flatten_svg_subpath(
		const SVGSubpath& p_subpath,
		double p_tolerance
	);																		// 展平单条子路径
	SVGContours flatten_svg_path(const SVGPath& p_path, double p_tolerance);// 按误差容限将曲线转成独立轮廓
}
