#include "css_shape_path.h"

#include "css_compute_context.h"
#include "css_math.h"
#include "css_parser.h"
#include "css_position_values.h"
#include "css_syntax_util.h"

#include <cmath>

namespace dockit::css {
	namespace {
		using Kind = ShapeCommand::Kind;
		using Anchor = ShapeControlPoint::Anchor;
		using detail::at;
		using detail::is_one_of;

		constexpr int length_flags = f_length_percentage | f_math;

		bool parse_pair(const css_token_vector& p_tokens, int& r_index, css_image_position& r_point) {
			if (!r_point.x.offset.from_token(at(p_tokens, r_index), length_flags) ||
				!r_point.y.offset.from_token(at(p_tokens, r_index + 1), length_flags)) return false;
			r_index += 2;
			return true;
		}

		bool parse_control(const css_token_vector& p_tokens, int p_begin, int p_end, bool p_relative,
			ShapeControlPoint& r_control) {
			int index = p_begin;
			if (p_end - p_begin == 4 && at(p_tokens, p_begin + 2).ident() == "from") {
				const auto anchor = at(p_tokens, p_begin + 3).ident();
				if (anchor == "start") r_control.anchor = Anchor::start;
				else if (anchor == "end") r_control.anchor = Anchor::end;
				else if (anchor == "origin") r_control.anchor = Anchor::origin;
				else return false;
				return parse_pair(p_tokens, index, r_control.position);
			}
			if (p_relative) {
				r_control.anchor = Anchor::start;
				return p_end - p_begin == 2 && parse_pair(p_tokens, index, r_control.position);
			}
			// The following slash, when present, ends the first control point.
			return parse_image_position(p_tokens, index, r_control.position) && index == p_end;
		}

		bool parse_arc_options(const css_token_vector& p_tokens, int p_index, ShapeCommand& r_command) {
			bool has_radii = false;
			bool has_rotation = false;
			bool has_sweep = false;
			bool has_size = false;
			const int end = static_cast<int>(p_tokens.size());
			while (p_index < end) {
				const auto keyword = p_tokens[p_index++].ident();
				if (keyword == "of" && !has_radii) {
					has_radii = true;
					if (!r_command.radii[0].from_token(at(p_tokens, p_index++), length_flags)) return false;
					if (r_command.radii[1].from_token(at(p_tokens, p_index), length_flags)) {
						r_command.single_radius = false;
						++p_index;
					}
				} else if (keyword == "rotate" && !has_rotation) {
					has_rotation = true;
					const auto angle = parse_css_angle(at(p_tokens, p_index++));
					if (!angle) return false;
					constexpr double degrees_per_radian = 57.295779513082320876;
					r_command.rotation_degrees = *angle * degrees_per_radian;
				} else if (is_one_of(keyword, "cw", "ccw") && !has_sweep) {
					has_sweep = true;
					r_command.sweep = keyword == "cw";
				} else if (is_one_of(keyword, "large", "small") && !has_size) {
					has_size = true;
					r_command.large_arc = keyword == "large";
				} else {
					return false;
				}
			}
			return has_radii;
		}

		std::optional<ShapeCommand> parse_command(const css_token_vector& p_tokens) {
			ShapeCommand result;
			const auto command = at(p_tokens, 0).ident();
			const int end = static_cast<int>(p_tokens.size());
			if (command == "close") {
				result.kind = Kind::close;
				return end == 1 ? std::optional(result) : std::nullopt;
			}
			const auto mode = at(p_tokens, 1).ident();
			if (!is_one_of(mode, "to", "by")) return std::nullopt;
			result.relative = mode == "by";
			int index = 2;

			if (is_one_of(command, "hline", "vline")) {
				if (end != 3) return std::nullopt;
				const bool horizontal = command == "hline";
				result.kind = horizontal ? Kind::horizontal : Kind::vertical;
				auto& axis = horizontal ? result.end.x : result.end.y;
				if (result.relative) {
					if (!axis.offset.from_token(p_tokens[2], length_flags)) return std::nullopt;
				} else {
					const auto position = parse_axis_position({p_tokens[2]}, horizontal, false);
					if (!position) return std::nullopt;
					axis = *position;
				}
				return result;
			}
			if (result.relative ? !parse_pair(p_tokens, index, result.end) :
				!parse_image_position(p_tokens, index, result.end)) return std::nullopt;
			if (is_one_of(command, "move", "line")) {
				result.kind = command == "move" ? Kind::move : Kind::line;
				return index == end ? std::optional(result) : std::nullopt;
			}
			if (command == "arc") {
				result.kind = Kind::arc;
				return parse_arc_options(p_tokens, index, result) ? std::optional(result) : std::nullopt;
			}
			if (!is_one_of(command, "curve", "smooth")) return std::nullopt;
			if (command == "smooth" && index == end) {
				result.kind = Kind::smooth_quadratic;
				return result;
			}
			if (at(p_tokens, index++).ident() != "with") return std::nullopt;
			int split = index;
			while (split < end && p_tokens[split].ch != '/') ++split;
			if (!parse_control(p_tokens, index, split, result.relative, result.controls[0])) return std::nullopt;
			if (command == "smooth") {
				result.kind = Kind::smooth_cubic;
				return split == end ? std::optional(result) : std::nullopt;
			}
			if (split == end) {
				result.kind = Kind::quadratic;
			} else {
				if (!parse_control(p_tokens, split + 1, end, result.relative, result.controls[1])) return std::nullopt;
				result.kind = Kind::cubic;
			}
			return result;
		}
	}

	std::optional<CSSShapePath> parse_shape_path(const css_token_vector& p_tokens) {
		const auto parts = parse_comma_separated_list(p_tokens);
		if (parts.size() < 2 || parts.front().empty()) return std::nullopt;
		const auto& header = parts.front();
		const auto fill = header.front().ident();
		int index = is_one_of(fill, "nonzero", "evenodd") ? 1 : 0;
		CSSShapePath result;
		result.even_odd = fill == "evenodd";
		if (at(header, index++).ident() != "from" ||
			!parse_image_position(header, index, result.start) || index != static_cast<int>(header.size())) return std::nullopt;
		for (size_t part = 1; part < parts.size(); ++part) {
			auto command = parse_command(parts[part]);
			if (!command) return std::nullopt;
			result.commands.push_back(std::move(*command));
		}
		return result;
	}

	void compute_shape_path(CSSShapePath& r_path, const ComputeContext& p_context,
		const font_metrics& p_font, bool p_rtl) {
		const auto position = [&](css_image_position& r_point) {
			r_point.x.resolve_flow_direction(p_rtl);
			r_point.y.resolve_flow_direction(false);
			p_context.cvt_units(r_point.x.offset, p_font, 0);
			p_context.cvt_units(r_point.y.offset, p_font, 0);
		};
		position(r_path.start);
		for (auto& command : r_path.commands) {
			position(command.end);
			for (auto& control : command.controls) position(control.position);
			for (auto& radius : command.radii) p_context.cvt_units(radius, p_font, 0);
		}
	}

	std::optional<SVGPath> resolve_shape_path(const CSSShapePath& p_path, pixel_t p_width, pixel_t p_height) {
		using Segment = SVGPathSegment;
		SVGPathBuilder outline;
		const auto position = [&](const css_image_position& p_position) -> SVGPathPoint {
			return {p_position.x.resolve(p_width), p_position.y.resolve(p_height)};
		};
		if (!outline.move_to(position(p_path.start))) return std::nullopt;
		for (const auto& command : p_path.commands) {
			if (command.kind == Kind::close) {
				outline.close();
				continue;
			}
			const auto start = outline.position();
			auto end = position(command.end);
			if (command.relative) end = start + end;
			if (command.kind == Kind::horizontal) end.y = start.y;
			if (command.kind == Kind::vertical) end.x = start.x;
			if (command.kind == Kind::move) {
				if (!outline.move_to(end)) return std::nullopt;
				continue;
			}

			const auto control = [&](size_t p_index) {
				const auto& point = command.controls[p_index];
				auto value = position(point.position);
				if (point.anchor == Anchor::start) value = start + value;
				if (point.anchor == Anchor::end) value = end + value;
				return value;
			};
			Segment segment;
			segment.end = end;
			switch (command.kind) {
				case Kind::quadratic:
				case Kind::smooth_quadratic:
					segment.kind = Segment::Kind::quadratic;
					segment.control_1 = command.kind == Kind::quadratic ? control(0) : outline.reflected_control(segment.kind);
					break;
				case Kind::cubic:
				case Kind::smooth_cubic:
					segment.kind = Segment::Kind::cubic;
					segment.control_1 = command.kind == Kind::cubic ? control(0) : outline.reflected_control(segment.kind);
					segment.control_2 = control(command.kind == Kind::cubic ? 1 : 0);
					break;
				case Kind::arc:
					segment.kind = Segment::Kind::arc;
					if (command.single_radius) {
						const auto size = static_cast<pixel_t>(std::hypot(p_width, p_height) / std::sqrt(2.0));
						const auto radius = std::abs(command.radii[0].calc_percent(size));
						segment.radii = {radius, radius};
					} else {
						segment.radii = {std::abs(command.radii[0].calc_percent(p_width)),
							std::abs(command.radii[1].calc_percent(p_height))};
					}
					segment.rotation = command.rotation_degrees;
					segment.sweep = command.sweep;
					segment.large_arc = command.large_arc;
					break;
				default:
					break;
			}
			if (!outline.append(segment)) return std::nullopt;
		}
		return outline.take_path();
	}
}
