#include "css_transition.h"
#include "css_value_parser.h"

#include <litehtml/css_parser.h>
#include <litehtml/html.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace litehtml {
	namespace {
		struct transition_ids {
			string_id shorthand = _id("transition");
			string_id property = _id("transition-property");
			string_id duration = _id("transition-duration");
			string_id delay = _id("transition-delay");
			string_id easing = _id("transition-timing-function");
			string_id behavior = _id("transition-behavior");
		};
		const transition_ids& ids() {
			static const transition_ids names;
			return names;
		}

		std::optional<std::string> property_name(const css_token& token) {
			if (token.type != IDENT) return std::nullopt;
			const auto name = token.ident();
			if (is_one_of(name, "inherit", "initial", "unset", "revert", "revert-layer", "default"))
				return std::nullopt;
			return token.name.compare(0, 2, "--") == 0 ? token.name : name;
		}

		std::optional<int> behavior_value(const css_token& token) {
			const auto name = token.ident();
			if (name == "normal") return 0;
			if (name == "allow-discrete") return 1;
			return std::nullopt;
		}

		std::optional<double> number(const css_token& token) {
			return token.type == NUMBER && std::isfinite(token.n.number) ?
				std::optional<double>(token.n.number) : std::nullopt;
		}

		std::optional<css_easing> parse_linear(const vector<css_token_vector>& args) {
			if (args.size() < 2) return std::nullopt;
			struct stop { std::optional<double> input; double output; };
			std::vector<stop> stops;
			for (const auto& argument : args) {
				std::optional<double> output;
				std::vector<double> positions;
				for (const auto& token : argument) {
					if (const auto value = number(token); value && !output) output = value;
					else if (token.type == PERCENTAGE && std::isfinite(token.n.number) && positions.size() < 2)
						positions.push_back(token.n.number / 100.0);
					else return std::nullopt;
				}
				if (!output) return std::nullopt;
				if (positions.empty()) stops.push_back({std::nullopt, *output});
				else for (const auto position : positions) stops.push_back({position, *output});
			}
			if (!stops.front().input) stops.front().input = 0.0;
			if (!stops.back().input) stops.back().input = 1.0;
			for (std::size_t left = 0; left + 1 < stops.size();) {
				auto right = left + 1;
				while (!stops[right].input) ++right;
				const auto from = *stops[left].input;
				const auto to = std::max(from, *stops[right].input);
				for (auto index = left + 1; index <= right; ++index)
					stops[index].input = from + (to - from) * double(index - left) / double(right - left);
				left = right;
			}
			css_easing result;
			result.type = css_easing::kind::linear;
			for (const auto& stop : stops) result.stops.emplace_back(*stop.input, stop.output);
			return result;
		}

	}

	std::optional<double> parse_css_time(const css_token& token) {
		if (token.type != DIMENSION || !std::isfinite(token.n.number)) return std::nullopt;
		const auto unit = lowcase(token.unit);
		if (unit == "s") return token.n.number;
		if (unit == "ms") return token.n.number / 1000.0;
		return std::nullopt;
	}

	std::optional<css_easing> parse_css_easing(const css_token& token) {
			css_easing result;
			const auto name = token.ident();
			if (name == "ease") return result;
			if (name == "linear") result.type = css_easing::kind::linear;
			else if (name == "ease-in") result.control = {0.42, 0, 1, 1};
			else if (name == "ease-out") result.control = {0, 0, 0.58, 1};
			else if (name == "ease-in-out") result.control = {0.42, 0, 0.58, 1};
			else if (is_one_of(name, "step-start", "step-end")) {
				result.type = css_easing::kind::steps;
				result.position = name == "step-start" ? css_easing::step_position::start : css_easing::step_position::end;
			} else if (token.type == CV_FUNCTION) {
				const auto args = parse_comma_separated_list(token.value);
				const auto function = lowcase(token.name);
				if (function == "linear") return parse_linear(args);
				if (std::any_of(args.begin(), args.end(), [](const auto& arg) { return arg.size() != 1; }))
					return std::nullopt;
				if (function == "cubic-bezier" && args.size() == 4) {
					for (std::size_t index = 0; index < 4; ++index) {
						const auto value = number(args[index].front());
						if (!value || (index % 2 == 0 && (*value < 0 || *value > 1))) return std::nullopt;
						result.control[index] = *value;
					}
				} else if (function == "steps" && (args.size() == 1 || args.size() == 2)) {
					const auto steps = number(args.front().front());
					if (!steps || args.front().front().n.number_type != css_number_integer ||
						*steps < 1 || *steps >= std::numeric_limits<int>::max())
						return std::nullopt;
					result.type = css_easing::kind::steps;
					result.steps = static_cast<int>(*steps);
					if (args.size() == 2) {
						const auto jump = args[1].front().ident();
						using position = css_easing::step_position;
						if (is_one_of(jump, "start", "jump-start")) result.position = position::start;
						else if (is_one_of(jump, "end", "jump-end")) result.position = position::end;
						else if (jump == "jump-none" && result.steps > 1) result.position = position::none;
						else if (jump == "jump-both") result.position = position::both;
						else return std::nullopt;
					}
				} else return std::nullopt;
			} else return std::nullopt;
			return result;
	}

	double css_easing::sample(double progress, bool before) const {
		if (type == kind::steps) {
			const int jumps = steps + (position == step_position::both) - (position == step_position::none);
			const double scaled = progress * steps;
			double current = std::floor(scaled) +
				(position == step_position::both || position == step_position::start);
			if (before && scaled == std::floor(scaled)) --current;
			return std::clamp(current, 0.0, double(jumps)) / jumps;
		}
		if (type == kind::linear) {
			if (stops.empty()) return progress;
			auto upper = std::upper_bound(stops.begin(), stops.end(), progress,
				[](double value, const auto& stop) { return value < stop.first; });
			if (upper == stops.begin()) ++upper;
			if (upper == stops.end()) --upper;
			const auto& [x1, y1] = *std::prev(upper);
			const auto& [x2, y2] = *upper;
			return x1 == x2 ? (progress < x1 ? y1 : y2) : y1 + (y2 - y1) * (progress - x1) / (x2 - x1);
		}
		if (progress <= 0 || progress >= 1) return progress;
		const auto bezier = [](double parameter, double first, double second) {
			const auto one_minus = 1.0 - parameter;
			return 3 * parameter * one_minus * (one_minus * first + parameter * second) + parameter * parameter * parameter;
		};
		double lower = 0;
		double upper = 1;
		for (int i = 0; i < 24; ++i) {
			const auto middle = (lower + upper) / 2;
			if (bezier(middle, control[0], control[2]) < progress) lower = middle;
			else upper = middle;
		}
		return bezier((lower + upper) / 2, control[1], control[3]);
	}

	std::optional<css_transition> css_transition_styles::match(string_id property, bool rtl) const {
		const auto targets = [&](const auto& visit, string_id name) -> bool {
			name = css_canonical_property(name);
			if (name == property || css_map_logical_property(name, rtl) == property) return true;
			const auto& expanded = css_longhands(name);
			return std::any_of(expanded.begin(), expanded.end(), [&](string_id longhand) {
				return visit(visit, longhand);
			});
		};
		for (std::size_t i = properties.size(); i-- > 0;) {
			if (properties[i] == "all" || targets(targets, _id(properties[i]))) {
				return css_transition{durations[i % durations.size()], delays[i % delays.size()],
					easings[i % easings.size()], behaviors[i % behaviors.size()] != 0};
			}
		}
		return std::nullopt;
	}

	void parse_css_transition(css_value_parser& parser, string_id name, const css_token_vector& value, bool important) {
		const auto& fields = ids();
		const bool shorthand = name == fields.shorthand;
		css_transition_styles lists;
		lists.properties.clear();
		lists.durations.clear();
		lists.delays.clear();
		lists.easings.clear();
		lists.behaviors.clear();
		const auto groups = parse_comma_separated_list(value);
		if (groups.empty()) return;
		for (const auto& group : groups) {
			if (group.empty() || (!shorthand && group.size() != 1)) return;
			std::optional<std::string> property;
			std::optional<double> duration, delay;
			std::optional<css_easing> easing;
			std::optional<int> behavior;
			for (const auto& token : group) {
				if (const auto time = parse_css_time(token); time && (shorthand || name == fields.duration || name == fields.delay)) {
					if (!duration && name != fields.delay) {
						if (*time < 0) return;
						duration = time;
					} else if (!delay) delay = time;
					else return;
				} else if (const auto timing = parse_css_easing(token); timing && !easing && (shorthand || name == fields.easing)) {
					easing = timing;
				} else if (const auto mode = behavior_value(token); mode && !behavior && (shorthand || name == fields.behavior)) {
					behavior = mode;
				} else if (const auto target = property_name(token); target && !property && (shorthand || name == fields.property)) {
					property = target;
				} else return;
			}
			if (property && *property == "none" && groups.size() != 1) return;
			lists.properties.push_back(property.value_or("all"));
			lists.durations.push_back(duration.value_or(0));
			lists.delays.push_back(delay.value_or(0));
			lists.easings.push_back(easing.value_or(css_easing{}));
			lists.behaviors.push_back(behavior.value_or(0));
		}
		if (shorthand || name == fields.property) parser.add_parsed_property(fields.property, property_value(lists.properties, important));
		if (shorthand || name == fields.duration) parser.add_parsed_property(fields.duration, property_value(lists.durations, important));
		if (shorthand || name == fields.delay) parser.add_parsed_property(fields.delay, property_value(lists.delays, important));
		if (shorthand || name == fields.easing) parser.add_parsed_property(fields.easing, property_value(lists.easings, important));
		if (shorthand || name == fields.behavior) parser.add_parsed_property(fields.behavior, property_value(lists.behaviors, important));
	}
}
