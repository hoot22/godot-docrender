#pragma once

#include "css_length.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace litehtml {
	struct css_math_node;
	using css_math_node_ptr = std::shared_ptr<const css_math_node>;

	struct css_math_expression {
		css_math_node_ptr root;
		bool has_percentage;
		bool non_negative;
		bool is_number;

		double evaluate(double p_basis) const;
		string to_string() const;
	};

	std::shared_ptr<const css_math_expression> parse_css_math(const css_token& p_token, int p_options);
	enum class css_math_scalar { number, percentage, number_or_percentage, angle, number_or_angle };
	using css_math_input_names = std::array<std::string_view, 4>;
	using css_math_input_values = std::array<double, 4>;
	css_math_node_ptr parse_css_scalar_math(const css_token& p_token, css_math_scalar p_type,
		const css_math_input_names* p_names = nullptr);
	double evaluate_css_scalar_math(const css_math_node& p_node, css_math_scalar p_type,
		double p_percentage_basis, const css_math_input_values* p_inputs = nullptr);
	// Evaluate a context-free calculation. Angles are returned in radians.
	std::optional<double> evaluate_css_scalar_math(const css_token& p_token, css_math_scalar p_type,
		double p_percentage_basis = 1.0);
	// Parse an angle in radians, optionally normalizing it to a signed half turn.
	std::optional<double> parse_css_angle(const css_token& p_token, bool p_allow_unitless_zero = false, bool p_normalize = true);
	inline constexpr double CSS_DPI_PER_DPPX = 96.0;
	// Parse a nonnegative resolution in canonical dppx units.
	std::optional<double> parse_css_resolution(const css_token& p_token);
	std::shared_ptr<const css_math_expression> compute_css_math(const css_math_expression& p_value,
		const std::function<pixel_t(const css_length&)>& p_resolve_length);
	css_length interpolate_css_length(const css_length& from, const css_length& to, double progress, bool non_negative = false);
}
