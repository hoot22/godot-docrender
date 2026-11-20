#include "html_glyph_outline.h"

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

using namespace godot;

namespace dockit {
	namespace {
		using Point = css::SVGPathPoint;
		using Segment = css::SVGPathSegment;

		bool append_contour(css::SVGPathBuilder& r_path, const PackedVector3Array& p_points, int64_t p_first, int64_t p_last) {
			constexpr auto on_curve = TextServer::CONTOUR_CURVE_TAG_ON;
			constexpr auto conic = TextServer::CONTOUR_CURVE_TAG_OFF_CONIC;
			constexpr auto cubic = TextServer::CONTOUR_CURVE_TAG_OFF_CUBIC;
			const auto point = [&](int64_t i) { return Point{p_points[i].x, p_points[i].y}; };
			const auto tag = [&](int64_t i) { return static_cast<int>(p_points[i].z); };

			Point origin = point(p_first);
			if (tag(p_first) == on_curve) {
				++p_first;
			} else if (tag(p_first) != conic) {
				return false;
			} else if (tag(p_last) == on_curve) {
				origin = point(p_last--);
			} else if (tag(p_last) == conic) {
				origin = (origin + point(p_last)) * 0.5;
			} else {
				return false;
			}
			if (!r_path.move_to(origin)) return false;
			for (int64_t i = p_first; i <= p_last;) {
				Segment segment;
				const int kind = tag(i);
				const Point current = point(i++);
				if (kind == on_curve) {
					segment.end = current;
				} else if (kind == conic) {
					segment.kind = Segment::Kind::quadratic;
					segment.control_1 = current;
					if (i > p_last) segment.end = origin;
					else if (tag(i) == on_curve) segment.end = point(i++);
					else if (tag(i) == conic) segment.end = (current + point(i)) * 0.5;
					else return false;
				} else if (kind == cubic && i <= p_last && tag(i) == cubic) {
					segment.kind = Segment::Kind::cubic;
					segment.control_1 = current;
					segment.control_2 = point(i++);
					if (i > p_last) segment.end = origin;
					else if (tag(i) == on_curve) segment.end = point(i++);
					else return false;
				} else {
					return false;
				}
				if (!r_path.append(segment)) return false;
			}
			r_path.close();
			return true;
		}
	}

	css::SVGPath read_glyph_outline(const TextServer& p_server, const RID& p_font, int p_size, int64_t p_index, bool& r_reverse) {
		const auto outline = p_server.font_get_glyph_contours(p_font, p_size, p_index);
		r_reverse = outline.get("orientation", false);
		const PackedVector3Array points = outline.get("points", PackedVector3Array());
		const PackedInt32Array contours = outline.get("contours", PackedInt32Array());
		css::SVGPathBuilder path;
		int64_t first = 0;
		for (const int32_t last : contours) {
			if (last < first || last >= points.size() || !append_contour(path, points, first, last)) return {};
			first = static_cast<int64_t>(last) + 1;
		}
		return path.take_path();
	}
}
