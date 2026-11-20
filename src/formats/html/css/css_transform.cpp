#include "formats/html/css/css_transform.h"
#include "formats/html/css/css_math.h"

#include <litehtml/html.h>

#include <algorithm>
#include <cmath>

namespace litehtml {
	namespace {
		using kind = css_transform_function::kind;

		bool parse_length(const css_token& p_token, css_length& r_value, bool p_percentage) {
			if (is_one_of(p_token.type, NUMBER, PERCENTAGE, DIMENSION) && !std::isfinite(p_token.n.number)) {
				return false;
			}
			return r_value.from_token(p_token, (p_percentage ? f_length_percentage : f_length) | f_math);
		}

		bool parse_angle(const css_token& p_token, double& r_angle) {
			const auto angle = parse_css_angle(p_token, true, false);
			if (!angle) return false;
			r_angle = *angle;
			return true;
		}

		bool parse_number(const css_token& p_token, double& r_number) {
			if (p_token.type == CV_FUNCTION) {
				const auto number = evaluate_css_scalar_math(p_token, css_math_scalar::number);
				if (!number || !std::isfinite(*number)) return false;
				r_number = *number;
				return true;
			}
			if (p_token.type != NUMBER || !std::isfinite(p_token.n.number)) {
				return false;
			}
			r_number = p_token.n.number;
			return true;
		}

		bool parse_scale(const css_token& p_token, double& r_number) {
			if (p_token.type == PERCENTAGE && std::isfinite(p_token.n.number)) {
				r_number = p_token.n.number / 100;
				return true;
			}
			if (p_token.type == CV_FUNCTION) {
				const auto value = evaluate_css_scalar_math(p_token, css_math_scalar::number_or_percentage);
				if (!value || !std::isfinite(*value)) return false;
				r_number = *value;
				return true;
			}
			return parse_number(p_token, r_number);
		}

		void normalize_rotation(css_transform_function& r_transform) {
			auto& [x, y, z] = r_transform.axis;
			const double largest = std::max({ std::abs(x), std::abs(y), std::abs(z) });
			if (largest == 0) {
				r_transform.axis = { 0, 0, 1 };
				r_transform.first = 0;
				return;
			}
			x /= largest;
			y /= largest;
			z /= largest;
			const double length = std::hypot(x, y, z);
			x /= length;
			y /= length;
			z /= length;
		}

		bool parse_rotation(const css_token_vector& p_tokens, css_transform_function& r_function) {
			const size_t count = p_tokens.size();
			if (!is_one_of(count, size_t(1), size_t(2), size_t(4))) return false;
			const auto first = parse_css_angle(p_tokens.front());
			const auto angle = first ? first : parse_css_angle(p_tokens.back());
			if (!angle) return false;
			r_function.first = *angle;
			if (count == 1) return true;
			const size_t axis = first ? 1 : 0;
			if (count == 2) {
				const auto name = p_tokens[axis].ident();
				if (name == "x") r_function.axis = { 1, 0, 0 };
				else if (name == "y") r_function.axis = { 0, 1, 0 };
				else if (name != "z") return false;
				return true;
			}
			for (size_t i = 0; i < r_function.axis.size(); ++i) {
				if (!parse_number(p_tokens[axis + i], r_function.axis[i])) return false;
			}
			normalize_rotation(r_function);
			return true;
		}

		bool comma_arguments(const css_token_vector& p_args, size_t p_count) {
			if (p_args.size() != 2 * p_count - 1) return false;
			for (size_t i = 1; i < p_args.size(); i += 2) {
				if (p_args[i].type != COMMA) return false;
			}
			return true;
		}

		bool parse_matrix(const css_token_vector& p_args, bool p_spatial, css_transform_function& r_function) {
			const size_t count = p_spatial ? 16 : 6;
			if (!comma_arguments(p_args, count)) return false;
			r_function.type = p_spatial ? kind::matrix3d : kind::matrix;
			for (size_t i = 0; i < count; ++i) {
				if (!parse_number(p_args[2 * i], r_function.matrix[i])) return false;
			}
			return true;
		}

		bool parse_transform_function(std::string_view p_name, const css_token_vector& p_args, css_transform_function& r_function) {
			if (is_one_of(p_name, "matrix", "matrix3d")) return parse_matrix(p_args, p_name == "matrix3d", r_function);
			if (p_name == "perspective") {
				r_function.type = kind::perspective;
				return p_args.size() == 1 && r_function.z.from_token(p_args.front(), f_length | f_positive | f_math, "none");
			}
			if (p_name == "rotate3d") {
				r_function.type = kind::rotate;
				if (!comma_arguments(p_args, 4) || !parse_angle(p_args[6], r_function.first)) return false;
				for (size_t i = 0; i < r_function.axis.size(); ++i) {
					if (!parse_number(p_args[2 * i], r_function.axis[i])) return false;
				}
				normalize_rotation(r_function);
				return true;
			}
			if (p_name == "translate3d") {
				r_function.type = kind::translate;
				return comma_arguments(p_args, 3) && parse_length(p_args[0], r_function.x, true) &&
					parse_length(p_args[2], r_function.y, true) && parse_length(p_args[4], r_function.z, false);
			}
			if (p_name == "scale3d") {
				r_function.type = kind::scale;
				return comma_arguments(p_args, 3) && parse_scale(p_args[0], r_function.first) &&
					parse_scale(p_args[2], r_function.second) && parse_scale(p_args[4], r_function.third);
			}
			const bool two = comma_arguments(p_args, 2);
			if (p_args.size() != 1 && !two) return false;
			if (two && !is_one_of(p_name, "translate", "scale", "skew")) return false;

			if (is_one_of(p_name, "translate", "translatex", "translatey", "translatez")) {
				r_function.type = kind::translate;
				if (p_name == "translatez") return parse_length(p_args[0], r_function.z, false);
				return parse_length(p_args[0], p_name == "translatey" ? r_function.y : r_function.x, true) &&
					(!two || parse_length(p_args[2], r_function.y, true));
			}
			if (is_one_of(p_name, "rotate", "rotatex", "rotatey", "rotatez")) {
				r_function.type = kind::rotate;
				if (p_name == "rotatex") r_function.axis = { 1, 0, 0 };
				else if (p_name == "rotatey") r_function.axis = { 0, 1, 0 };
				return parse_angle(p_args[0], r_function.first);
			}
			if (is_one_of(p_name, "skew", "skewx", "skewy")) {
				r_function.type = kind::skew;
				return parse_angle(p_args[0], p_name == "skewy" ? r_function.second : r_function.first) &&
					(!two || parse_angle(p_args[2], r_function.second));
			}
			if (is_one_of(p_name, "scale", "scalex", "scaley", "scalez")) {
				r_function.type = kind::scale;
				r_function.first = r_function.second = 1;
				if (p_name == "scalez") return parse_scale(p_args[0], r_function.third);
				if (!parse_scale(p_args[0], p_name == "scaley" ? r_function.second : r_function.first)) return false;
				if (p_name == "scale") {
					r_function.second = r_function.first;
					return !two || parse_scale(p_args[2], r_function.second);
				}
				return true;
			}
			return false;
		}

		enum class origin_axis { either, horizontal, vertical, length, invalid };

		origin_axis parse_origin_component(const css_token& p_token, css_length& r_length) {
			const auto name = p_token.ident();
			if (name == "center") return origin_axis::either;
			if (is_one_of(name, "left", "right")) {
				r_length.set_value(name == "left" ? 0 : 100, css_units_percentage);
				return origin_axis::horizontal;
			}
			if (is_one_of(name, "top", "bottom")) {
				r_length.set_value(name == "top" ? 0 : 100, css_units_percentage);
				return origin_axis::vertical;
			}
			return parse_length(p_token, r_length, true) ? origin_axis::length : origin_axis::invalid;
		}
	}

	bool parse_css_transform(const css_token_vector& p_tokens, css_transform_list& r_value) {
		if (p_tokens.size() == 1 && p_tokens.front().ident() == "none") {
			return true;
		}
		for (const auto& token : p_tokens) {
			if (token.type != CV_FUNCTION) return false;
			css_transform_function function{};
			if (!parse_transform_function(lowcase(token.name), token.value, function)) return false;
			r_value.push_back(std::move(function));
		}
		return !r_value.empty();
	}

	bool parse_css_transform_origin(const css_token_vector& p_tokens, css_transform_origin& r_value) {
		if (p_tokens.empty() || p_tokens.size() > 3) return false;
		if (p_tokens.size() == 3 && !parse_length(p_tokens[2], r_value.z, false)) return false;
		const auto first = parse_origin_component(p_tokens[0], r_value.x);
		if (first == origin_axis::invalid) return false;
		if (p_tokens.size() == 1) {
			if (first == origin_axis::vertical) std::swap(r_value.x, r_value.y);
			return true;
		}
		const auto second = parse_origin_component(p_tokens[1], r_value.y);
		if (second == origin_axis::invalid) return false;
		if (first == origin_axis::vertical || second == origin_axis::horizontal) {
			if (first == origin_axis::horizontal || second == origin_axis::vertical ||
				first == origin_axis::length || second == origin_axis::length) return false;
			std::swap(r_value.x, r_value.y);
		}
		return true;
	}

	bool parse_css_individual_transform(kind p_kind, const css_token_vector& p_tokens, css_individual_transform& r_value) {
		if (p_tokens.size() == 1 && p_tokens.front().ident() == "none") {
			r_value.reset();
			return true;
		}
		if (p_tokens.empty()) return false;
		css_transform_function function{};
		function.type = p_kind;
		if (p_kind == kind::rotate) {
			if (!parse_rotation(p_tokens, function)) return false;
		} else if (p_kind == kind::translate) {
			if (p_tokens.size() > 3 || !parse_length(p_tokens[0], function.x, true) ||
				(p_tokens.size() >= 2 && !parse_length(p_tokens[1], function.y, true))) return false;
			if (p_tokens.size() == 3) {
				if (!parse_length(p_tokens[2], function.z, false)) return false;
			}
		} else if (p_kind == kind::scale) {
			if (p_tokens.size() > 3 || !parse_scale(p_tokens[0], function.first)) return false;
			function.second = function.first;
			if (p_tokens.size() >= 2 && !parse_scale(p_tokens[1], function.second)) return false;
			if (p_tokens.size() == 3) {
				if (!parse_scale(p_tokens[2], function.third)) return false;
			}
		} else {
			return false;
		}
		r_value = std::move(function);
		return true;
	}
}
