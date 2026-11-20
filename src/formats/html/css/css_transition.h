#pragma once

#include "css_token_compat.h"
#include "css_string_id.h"

#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace litehtml {

	struct css_easing {
		enum class kind { bezier, steps, linear } type = kind::bezier;
		enum class step_position { start, end, none, both } position = step_position::end;
		std::array<double, 4> control{0.25, 0.1, 0.25, 1.0};
		int steps = 1;
		std::vector<std::pair<double, double>> stops;

		double sample(double progress, bool before = false) const;
	};

	std::optional<double> parse_css_time(const css_token& token);
	std::optional<css_easing> parse_css_easing(const css_token& token);

	using css_transition_times = std::vector<double>;
	using css_transition_easings = std::vector<css_easing>;

	struct css_transition {
		double duration;
		double delay;
		css_easing easing;
		bool allow_discrete;
	};

	struct css_transition_styles {
		std::vector<std::string> properties{"all"};
		css_transition_times durations{0.0};
		css_transition_times delays{0.0};
		css_transition_easings easings{{}};
		std::vector<int> behaviors{0};

		std::optional<css_transition> match(string_id property, bool rtl) const;
	};

	class css_value_parser;
	void parse_css_transition(css_value_parser& parser, string_id name, const css_token_vector& value, bool important);
}
