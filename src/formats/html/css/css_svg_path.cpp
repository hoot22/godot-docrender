#include "css_svg_path.h"

#include <litehtml/html.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace dockit::css {
	namespace {
		using Point = SVGPathPoint;
		using Segment = SVGPathSegment;

		bool path_whitespace(char p_char) {
			return p_char == ' ' || p_char == '\t' || p_char == '\n' || p_char == '\r';
		}

		bool finite(Point p_point) {
			return std::isfinite(p_point.x) && std::isfinite(p_point.y);
		}

		class SVGPathParser {
			const std::string& source;
			size_t offset = 0;
			SVGPathBuilder path;

			char peek() const { return offset < source.size() ? source[offset] : '\0'; }

			void skip_space() {
				while (path_whitespace(peek())) ++offset;
			}

			bool comma_space() {
				const size_t before = offset;
				skip_space();
				if (peek() == ',') {
					++offset;
					skip_space();
				}
				return offset != before;
			}

			bool read_number(double& r_value) {
				const char first = peek();
				if (first != '+' && first != '-' && first != '.' && !(first >= '0' && first <= '9')) return false;
				const char* start = source.c_str() + offset;
				char* end = nullptr;
				r_value = litehtml::t_strtod(start, &end);
				if (end == start || !std::isfinite(r_value)) return false;
				offset += static_cast<size_t>(end - start);
				return true;
			}

			bool read_arguments(char p_command, int p_count, std::array<double, 7>& r_values) {
				for (int i = 0; i < p_count; ++i) {
					const bool separated = i > 0 && comma_space();
					if (p_command == 'A' && (i == 3 || i == 4)) {
						if ((i == 3 && !separated) || (peek() != '0' && peek() != '1')) return false;
						r_values[i] = source[offset++] - '0';
					} else {
						if (p_command == 'A' && i < 2 && (peek() == '-' || peek() == '+')) return false;
						if (!read_number(r_values[i])) return false;
					}
				}
				return true;
			}

			bool append_segment(char p_command, bool p_relative, const std::array<double, 7>& p_values) {
				const Point current = path.position();
				const Point origin = p_relative ? current : Point{};
				const auto point = [&](int index) { return origin + Point{p_values[index], p_values[index + 1]}; };
				Segment segment;
				switch (p_command) {
					case 'L': segment.end = point(0); break;
					case 'H': segment.end = {origin.x + p_values[0], current.y}; break;
					case 'V': segment.end = {current.x, origin.y + p_values[0]}; break;
					case 'C':
						segment.kind = Segment::Kind::cubic;
						segment.control_1 = point(0);
						segment.control_2 = point(2);
						segment.end = point(4);
						break;
					case 'S':
						segment.kind = Segment::Kind::cubic;
						segment.control_1 = path.reflected_control(segment.kind);
						segment.control_2 = point(0);
						segment.end = point(2);
						break;
					case 'Q':
						segment.kind = Segment::Kind::quadratic;
						segment.control_1 = point(0);
						segment.end = point(2);
						break;
					case 'T':
						segment.kind = Segment::Kind::quadratic;
						segment.control_1 = path.reflected_control(segment.kind);
						segment.end = point(0);
						break;
					case 'A':
						segment.kind = Segment::Kind::arc;
						segment.radii = {p_values[0], p_values[1]};
						segment.rotation = p_values[2];
						segment.large_arc = p_values[3] != 0;
						segment.sweep = p_values[4] != 0;
						segment.end = point(5);
						break;
				}
				return path.append(segment);
			}

		public:
			explicit SVGPathParser(const std::string& p_source) : source(p_source) {}

			std::optional<SVGPath> parse(bool p_keep_prefix = false) {
				const auto fail = [&]() -> std::optional<SVGPath> {
					return p_keep_prefix && !path.empty() ? std::optional<SVGPath>(path.take_path()) : std::nullopt;
				};
				char command = 0;
				bool relative = false;
				for (;;) {
					skip_space();
					if (offset == source.size()) break;
					const char next = peek();
					if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z')) {
						relative = next >= 'a' && next <= 'z';
						command = relative ? static_cast<char>(next - 'a' + 'A') : next;
						++offset;
						skip_space();
					} else if (command != 0) {
						comma_space();
					}
					if (path.empty() && command != 'M') return fail();
					int count = 0;
					switch (command) {
						case 'M': case 'L': case 'T': count = 2; break;
						case 'H': case 'V': count = 1; break;
						case 'Q': case 'S': count = 4; break;
						case 'C': count = 6; break;
						case 'A': count = 7; break;
						case 'Z':
							path.close();
							command = 0;
							continue;
						default: return fail();
					}
					std::array<double, 7> values{};
					if (!read_arguments(command, count, values)) return fail();
					if (command == 'M') {
						const Point point{values[0], values[1]};
						if (!path.move_to(relative ? path.position() + point : point)) return fail();
						command = 'L';
					} else if (!append_segment(command, relative, values)) {
						return fail();
					}
				}
				if (path.empty()) return std::nullopt;
				return path.take_path();
			}
		};

		double distance_to_segment(Point p_point, Point p_start, Point p_end) {
			const Point chord = p_end - p_start;
			const double length = std::hypot(chord.x, chord.y);
			const Point offset = p_point - p_start;
			if (length == 0) return std::hypot(offset.x, offset.y);
			const Point direction = chord * (1 / length);
			const double projection = std::clamp(offset.x * direction.x + offset.y * direction.y, 0.0, length);
			const Point perpendicular = offset - direction * projection;
			return std::hypot(perpendicular.x, perpendicular.y);
		}

		void flatten_cubic(std::vector<Point>& r_points, Point p_0, Point p_1, Point p_2, Point p_3,
			double p_tolerance, int p_depth = 0) {
			if (p_depth == 12 || (distance_to_segment(p_1, p_0, p_3) <= p_tolerance &&
				distance_to_segment(p_2, p_0, p_3) <= p_tolerance)) {
				r_points.push_back(p_3);
				return;
			}
			const Point a = p_0 * 0.5 + p_1 * 0.5;
			const Point b = p_1 * 0.5 + p_2 * 0.5;
			const Point c = p_2 * 0.5 + p_3 * 0.5;
			const Point d = a * 0.5 + b * 0.5;
			const Point e = b * 0.5 + c * 0.5;
			const Point middle = d * 0.5 + e * 0.5;
			flatten_cubic(r_points, p_0, a, d, middle, p_tolerance, p_depth + 1);
			flatten_cubic(r_points, middle, e, c, p_3, p_tolerance, p_depth + 1);
		}

		void flatten_arc(std::vector<Point>& r_points, Point p_start, const Segment& p_arc, double p_tolerance) {
			const Point end = p_arc.end;
			if (p_start == end) return;
			double rx = p_arc.radii.x;
			double ry = p_arc.radii.y;
			if (rx == 0 || ry == 0) {
				r_points.push_back(end);
				return;
			}
			// Convert the SVG endpoint parameters to the center and angle form.
			constexpr double pi = 3.14159265358979323846;
			const double rotation = std::remainder(p_arc.rotation, 360.0) * pi / 180;
			const double c = std::cos(rotation);
			const double s = std::sin(rotation);
			const Point delta = p_start * 0.5 - end * 0.5;
			const Point local{c * delta.x + s * delta.y, c * delta.y - s * delta.x};
			const double required = std::hypot(local.x / rx, local.y / ry);
			if (required > 1) {
				rx *= required;
				ry *= required;
			}
			const Point unit{local.x / rx, local.y / ry};
			const double chord = std::hypot(unit.x, unit.y);
			if (!std::isfinite(required) || !std::isfinite(rx) || !std::isfinite(ry) || chord == 0) {
				r_points.push_back(end);
				return;
			}
			const double sign = p_arc.large_arc == p_arc.sweep ? -1 : 1;
			const double distance = sign * std::sqrt(std::max(0.0, 1 - chord * chord));
			const Point mid{(unit.y / chord) * distance, (-unit.x / chord) * distance};
			const Point center = p_start * 0.5 + end * 0.5 +
				Point{c * (rx * mid.x) - s * (ry * mid.y), s * (rx * mid.x) + c * (ry * mid.y)};
			const Point a = unit - mid;
			const Point b = unit * -1 - mid;
			const double first = std::atan2(a.y, a.x);
			double sweep = std::atan2(a.x * b.y - a.y * b.x, a.x * b.x + a.y * b.y);
			if (p_arc.sweep && sweep < 0) sweep += 2 * pi;
			if (!p_arc.sweep && sweep > 0) sweep -= 2 * pi;
			const double step = 4 * std::asin(std::sqrt(std::min(0.5, p_tolerance / std::max(rx, ry) / 2)));
			const int pieces = static_cast<int>(std::clamp(std::ceil(std::abs(sweep) / std::max(step, 1e-9)), 1.0, 4096.0));
			for (int i = 1; i < pieces; ++i) {
				const double angle = first + sweep * i / pieces;
				const double x = rx * std::cos(angle);
				const double y = ry * std::sin(angle);
				r_points.push_back(center + Point{c * x - s * y, s * x + c * y});
			}
			r_points.push_back(end);
		}
	}

	SVGPathPoint SVGPathBuilder::reflected_control(SVGPathSegment::Kind p_kind) const {
		if (!open || subpaths.back().segments.empty()) return current;
		const auto& previous = subpaths.back().segments.back();
		if (previous.kind != p_kind) return current;
		const auto control = p_kind == Segment::Kind::cubic ? previous.control_2 : previous.control_1;
		return current * 2 - control;
	}

	bool SVGPathBuilder::move_to(SVGPathPoint p_position) {
		if (!finite(p_position)) return false;
		current = p_position;
		subpaths.push_back({current, {}});
		open = true;
		return true;
	}

	bool SVGPathBuilder::append(const SVGPathSegment& p_segment) {
		if (!finite(p_segment.end) || !finite(p_segment.control_1) || !finite(p_segment.control_2) ||
			!finite(p_segment.radii) || !std::isfinite(p_segment.rotation)) return false;
		if (!open) move_to(current);
		subpaths.back().segments.push_back(p_segment);
		current = p_segment.end;
		return true;
	}

	void SVGPathBuilder::close() {
		if (empty()) return;
		subpaths.back().closed = true;
		current = subpaths.back().start;
		open = false;
	}

	SVGPath SVGPathBuilder::take_path() {
		return std::move(subpaths);
	}

	std::optional<SVGPath> parse_svg_path(const std::string& p_data) {
		return SVGPathParser(p_data).parse();
	}

	SVGPath parse_svg_geometry_path(const std::string& p_data) {
		return SVGPathParser(p_data).parse(true).value_or(SVGPath());
	}

	std::vector<SVGPathPoint> flatten_svg_subpath(const SVGSubpath& p_subpath, double p_tolerance) {
		Point current = p_subpath.start;
		std::vector<Point> points{current};
		for (const auto& segment : p_subpath.segments) {
			switch (segment.kind) {
				case Segment::Kind::line:
					points.push_back(segment.end);
					break;
				case Segment::Kind::quadratic: {
					const Point a = current * (1.0 / 3) + segment.control_1 * (2.0 / 3);
					const Point b = segment.end * (1.0 / 3) + segment.control_1 * (2.0 / 3);
					flatten_cubic(points, current, a, b, segment.end, p_tolerance);
					break;
				}
				case Segment::Kind::cubic:
					flatten_cubic(points, current, segment.control_1, segment.control_2, segment.end, p_tolerance);
					break;
				case Segment::Kind::arc:
					flatten_arc(points, current, segment, p_tolerance);
					break;
			}
			current = segment.end;
		}
		return points;
	}

	SVGContours flatten_svg_path(const SVGPath& p_path, double p_tolerance) {
		SVGContours result;
		for (const auto& subpath : p_path) {
			if (!subpath.segments.empty()) result.push_back(flatten_svg_subpath(subpath, p_tolerance));
		}
		return result;
	}
}
