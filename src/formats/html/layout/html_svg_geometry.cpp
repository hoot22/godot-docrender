#include "html_svg_geometry.h"

#include "formats/html/backend/litehtml_container.h"
#include "formats/html/markup/html_markup.h"

#include <litehtml/document.h>
#include <litehtml/element.h>
#include <litehtml/html.h>

#include <algorithm>
#include <array>
#include <cmath>

using namespace godot;
using namespace litehtml;

namespace dockit::svg {
	using Point = css::SVGPathPoint;
	using Builder = css::SVGPathBuilder;
	using Segment = css::SVGPathSegment;

	static const element* find_id(const element& p_node, const std::string_view p_id) {
		if (p_id == p_node.get_attr("id", "")) return &p_node;
		for (const auto& child : p_node.children()) {
			if (const auto* target = find_id(*child, p_id)) return target;
		}
		return nullptr;
	}

	const element* resolve_fragment(const element& p_root, std::string_view p_url) {
		if (p_url.size() < 2 || p_url.front() != '#') return nullptr;
		const auto id = decode_html_fragment(String::utf8(p_url.data() + 1, p_url.size() - 1)).utf8();
		return find_id(p_root, std::string_view(id.get_data(), id.length()));
	}

	std::shared_ptr<document> reference_document(const element& p_target, std::string_view p_url, const std::string& p_css_base) {
		auto source = p_target.get_document();
		if (source && !p_url.empty() && p_url.front() != '#') {
			auto* container = dynamic_cast<LiteHTMLContainer*>(source->container());
			return container ? container->get_svg_clip_document(p_url, p_css_base.c_str()) : nullptr;
		}
		return source;
	}

	bool is_svg(const element& p_node) {
		for (auto parent = p_node.parent(); parent; parent = parent->parent()) {
			const std::string_view tag = parent->get_tagName();
			if (tag == "svg") return true;
			if (tag == "foreignobject") return false;
		}
		return false;
	}

	static bool is_space(char p_char) {
		return p_char == ' ' || p_char == '\t' || p_char == '\r' || p_char == '\n';
	}

	static void skip_space(const char*& r_text) {
		while (is_space(*r_text)) ++r_text;
	}

	static bool read_number(const char*& r_text, double& r_value) {
		const char first = *r_text;
		if (first != '+' && first != '-' && first != '.' && !(first >= '0' && first <= '9')) return false;
		char* end = nullptr;
		r_value = litehtml::t_strtod(r_text, &end);
		if (end == r_text || !std::isfinite(r_value)) return false;
		r_text = end;
		return true;
	}

	static bool skip_separator(const char*& r_text) {
		const char* start = r_text;
		skip_space(r_text);
		if (*r_text == ',') {
			++r_text;
			skip_space(r_text);
		}
		return r_text != start;
	}

	double length_attribute(const element& p_node, const char* p_name, double p_basis, double p_default) {
		const char* attribute = p_node.get_attr(p_name);
		if (!attribute) return p_default;
		const auto tokens = tokenize(attribute);
		const css_token* value = nullptr;
		for (const auto& token : tokens) {
			if (token.type == dockit::css::WHITESPACE) continue;
			if (value) return p_default;
			value = &token;
		}
		if (!value) return p_default;
		css_length length;
		if (value->type == dockit::css::NUMBER && std::isfinite(value->n.number)) return value->n.number;
		if (!length.from_token(*value, f_length_percentage | f_math)) return p_default;
		p_node.get_document()->cvt_units(length, p_node.css().get_font_metrics(), 0);
		return length.calc_percent(p_basis);
	}

	static std::optional<Transform2D> transform_function(std::string_view p_name, const std::array<double, 6>& p_args, size_t p_count) {
		const double rad = std::acos(-1.0) / 180;
		if (p_name == "matrix" && p_count == 6) {
			return Transform2D(Vector2(p_args[0], p_args[1]), Vector2(p_args[2], p_args[3]), Vector2(p_args[4], p_args[5]));
		}
		if (p_name == "translate" && (p_count == 1 || p_count == 2)) {
			return Transform2D(0, Vector2(p_args[0], p_args[1]));
		}
		if (p_name == "scale" && (p_count == 1 || p_count == 2)) {
			return Transform2D(Vector2(p_args[0], 0), Vector2(0, p_count == 2 ? p_args[1] : p_args[0]), Vector2());
		}
		if (p_name == "rotate" && (p_count == 1 || p_count == 3)) {
			Transform2D rotation(p_args[0] * rad, Vector2());
			const Vector2 center(p_args[1], p_args[2]);
			rotation[2] = center - rotation.basis_xform(center);
			return rotation;
		}
		if (p_name == "skewX" && p_count == 1)
			return Transform2D(Vector2(1, 0), Vector2(std::tan(p_args[0] * rad), 1), Vector2());
		if (p_name == "skewY" && p_count == 1)
			return Transform2D(Vector2(1, std::tan(p_args[0] * rad)), Vector2(0, 1), Vector2());
		return std::nullopt;
	}

	Transform2D transform_attribute(const element& p_node) {
		const char* text = p_node.get_attr("transform", "");
		Transform2D matrix;
		skip_space(text);
		while (*text) {
			const char* start = text;
			while ((*text >= 'a' && *text <= 'z') || (*text >= 'A' && *text <= 'Z')) ++text;
			const std::string_view name(start, text - start);
			skip_space(text);
			if (*text != '(') return {};
			++text;
			skip_space(text);
			std::array<double, 6> args{};
			size_t count = 0;
			while (*text != ')') {
				if (count == args.size() || !read_number(text, args[count++])) return {};
				skip_space(text);
				if (*text == ',') {
					++text;
					skip_space(text);
					if (*text == ')') return {};
				}
			}
			++text;
			const auto step = transform_function(name, args, count);
			if (!step) return {};
			matrix *= *step;
			skip_separator(text);
		}
		return matrix.is_finite() ? matrix : Transform2D();
	}

	static void line(Builder& r_path, Point p_end) {
		r_path.append({ Segment::Kind::line, p_end });
	}

	static void arc(Builder& r_path, Point p_end, Point p_radii) {
		Segment segment{ Segment::Kind::arc, p_end };
		segment.radii = p_radii;
		segment.sweep = true;
		r_path.append(segment);
	}

	static css::SVGPath rect_path(const element& p_shape, const Vector2& p_basis) {
		const double width = length_attribute(p_shape, "width", p_basis.x);
		const double height = length_attribute(p_shape, "height", p_basis.y);
		if (width <= 0 || height <= 0) return {};
		const double x = length_attribute(p_shape, "x", p_basis.x);
		const double y = length_attribute(p_shape, "y", p_basis.y);
		const double specified_rx = length_attribute(p_shape, "rx", p_basis.x, -1);
		const double specified_ry = length_attribute(p_shape, "ry", p_basis.y, -1);
		double rx = std::clamp(specified_rx < 0 ? specified_ry : specified_rx, 0.0, width * 0.5);
		double ry = std::clamp(specified_ry < 0 ? specified_rx : specified_ry, 0.0, height * 0.5);
		if (rx == 0 || ry == 0) rx = ry = 0;
		const Point radii{rx, ry};
		Builder path;
		path.move_to({x + rx, y});
		line(path, {x + width - rx, y});
		if (rx > 0) arc(path, {x + width, y + ry}, radii);
		line(path, {x + width, y + height - ry});
		if (rx > 0) arc(path, {x + width - rx, y + height}, radii);
		line(path, {x + rx, y + height});
		if (rx > 0) arc(path, {x, y + height - ry}, radii);
		line(path, {x, y + ry});
		if (rx > 0) arc(path, {x + rx, y}, radii);
		path.close();
		return path.take_path();
	}

	static css::SVGPath ellipse_path(const element& p_shape, const Vector2& p_basis, bool p_circle) {
		Point radii;
		if (p_circle) {
			const double radius = length_attribute(p_shape, "r", std::hypot(p_basis.x, p_basis.y) / std::sqrt(2));
			radii = {radius, radius};
		} else {
			radii.x = length_attribute(p_shape, "rx", p_basis.x);
			radii.y = length_attribute(p_shape, "ry", p_basis.y);
		}
		if (radii.x <= 0 || radii.y <= 0) return {};
		const double x = length_attribute(p_shape, "cx", p_basis.x);
		const double y = length_attribute(p_shape, "cy", p_basis.y);
		Builder path;
		path.move_to({x + radii.x, y});
		arc(path, {x - radii.x, y}, radii);
		arc(path, {x + radii.x, y}, radii);
		path.close();
		return path.take_path();
	}

	static css::SVGPath points_path(const element& p_shape, bool p_closed) {
		const char* text = p_shape.get_attr("points", "");
		skip_space(text);
		Builder path;
		while (*text) {
			Point point;
			if (!read_number(text, point.x) || !skip_separator(text) || !read_number(text, point.y)) break;
			if (path.empty()) path.move_to(point);
			else line(path, point);
			if (!skip_separator(text)) break;
		}
		if (p_closed) path.close();
		return path.take_path();
	}

	css::SVGPath shape_path(const element& p_shape, const Vector2& p_basis, std::string_view p_tag) {
		if (p_tag == "path") return css::parse_svg_geometry_path(p_shape.get_attr("d", ""));
		if (p_tag == "rect") return rect_path(p_shape, p_basis);
		if (p_tag == "circle" || p_tag == "ellipse") return ellipse_path(p_shape, p_basis, p_tag == "circle");
		if (p_tag == "polygon" || p_tag == "polyline") return points_path(p_shape, p_tag == "polygon");
		if (p_tag == "line") {
			Builder path;
			path.move_to({ length_attribute(p_shape, "x1", p_basis.x), length_attribute(p_shape, "y1", p_basis.y) });
			line(path, { length_attribute(p_shape, "x2", p_basis.x), length_attribute(p_shape, "y2", p_basis.y) });
			return path.take_path();
		}
		return {};
	}

	PathMeasure::PathMeasure(const css::SVGPath& p_path, const Transform2D& p_transform, double p_tolerance, bool p_reverse) {
		const double scale = std::max(1.0, std::hypot(double(p_transform[0].length()), double(p_transform[1].length())));
		double distance = 0;
		size_t nonempty = 0;
		for (size_t i = 0; i < p_path.size(); ++i) {
			const auto& subpath = p_path[p_reverse ? p_path.size() - 1 - i : i];
			if (subpath.segments.empty()) continue;
			auto points = css::flatten_svg_subpath(subpath, p_tolerance / scale);
			if (subpath.closed && !(points.front() == points.back())) points.push_back(points.front());
			if (p_reverse) std::reverse(points.begin(), points.end());
			const size_t first_segment = segments.size();
			Vector2 from = p_transform.xform(Vector2(points.front().x, points.front().y));
			for (size_t next = 1; next < points.size(); ++next) {
				const Vector2 to = p_transform.xform(Vector2(points[next].x, points[next].y));
				const Vector2 delta = to - from;
				const double span = std::hypot(double(delta.x), double(delta.y));
				if (!std::isfinite(distance + span)) {
					segments.clear();
					closed = false;
					return;
				}
				if (distance + span > distance) {
					segments.push_back({ from, to, delta / span, distance, distance + span });
					distance += span;
				}
				from = to;
			}
			if (segments.size() > first_segment) closed = ++nonempty == 1 && subpath.closed;
		}
	}

	PathMeasure::Sample PathMeasure::sample(double p_distance) const {
		if (segments.empty()) return {};
		if (closed) {
			p_distance = std::fmod(p_distance, length());
			if (p_distance < 0) p_distance += length();
		} else {
			p_distance = std::clamp(p_distance, 0.0, length());
		}
		const auto next = std::upper_bound(segments.begin(), segments.end(), p_distance,
			[](double distance, const Segment& segment) { return distance < segment.last_distance; });
		if (next == segments.end()) return { segments.back().end, segments.back().tangent };
		const double offset = p_distance - next->first_distance;
		return { next->start + next->tangent * offset, next->tangent };
	}
}
