#include "css_animation.h"

#include "css_syntax_util.h"
#include "css_value_parser.h"

#include <cmath>
#include <limits>

namespace {
	using namespace litehtml;
	using namespace dockit::css::detail;

	std::optional<std::string> keyframes_name(const css_token& token) {
		if (token.type == STRING) return token.str;
		if (token.type == IDENT && !is_one_of(token.ident(), "default", "none", "inherit",
			"initial", "unset", "revert", "revert-layer")) return token.name;
		return std::nullopt;
	}

	std::optional<double> offset_value(const css_token& token) {
		if (token.ident() == "from") return 0;
		if (token.ident() == "to") return 1;
		if (token.type != PERCENTAGE || !std::isfinite(token.n.number) || token.n.number < 0 || token.n.number > 100)
			return std::nullopt;
		return token.n.number / 100;
	}
}

namespace litehtml {
	css_animation css_animation_styles::at(std::size_t index) const {
		return {durations[index % durations.size()], delays[index % delays.size()], easings[index % easings.size()],
			iterations[index % iterations.size()], directions[index % directions.size()], fills[index % fills.size()],
			play_states[index % play_states.size()] != 0};
	}

	const std::array<string_id, 8>& css_animation_property_names() {
		static const std::array names{_id("animation-name"), _id("animation-duration"), _id("animation-delay"),
			_id("animation-timing-function"), _id("animation-iteration-count"), _id("animation-direction"),
			_id("animation-fill-mode"), _id("animation-play-state")};
		return names;
	}

	void parse_css_animation(css_value_parser& parser, string_id name, const css_token_vector& value, bool important) {
		const auto& fields = css_animation_property_names();
		const bool shorthand = name == _id("animation");
		const auto accepts = [&](std::size_t index) { return shorthand || name == fields[index]; };
		css_animation_styles lists{{}, {}, {}, {}, {}, {}, {}, {}};
		const auto groups = dockit::css::parse_comma_separated_list(value);
		if (groups.empty()) return;
		for (const auto& group : groups) {
			if (group.empty() || (!shorthand && group.size() != 1)) return;
			std::optional<std::string> keyframes;
			bool has_name = false;
			std::optional<double> duration, delay, iterations;
			std::optional<css_easing> easing;
			std::optional<int> direction, fill, state;
			for (const auto& token : group) {
				const auto keyword = token.ident();
				if (const auto time = parse_css_time(token); time && (accepts(1) || accepts(2))) {
					if (accepts(1) && !duration) {
						if (*time < 0) return;
						duration = time;
					} else if (!delay && accepts(2)) delay = time;
					else return;
				} else if (const auto timing = parse_css_easing(token); timing && accepts(3) && !easing) {
					easing = timing;
				} else if (accepts(4) && !iterations && keyword == "infinite") {
					iterations = std::numeric_limits<double>::infinity();
				} else if (accepts(4) && !iterations && token.type == NUMBER &&
						std::isfinite(token.n.number) && token.n.number >= 0) {
					iterations = token.n.number;
				} else if (const int mode = value_index(keyword, "normal;reverse;alternate;alternate-reverse");
						mode >= 0 && accepts(5) && !direction) {
					direction = mode;
				} else if (const int mode = value_index(keyword, "none;forwards;backwards;both");
						mode >= 0 && accepts(6) && !fill) {
					fill = mode;
				} else if (const int mode = value_index(keyword, "running;paused");
						mode >= 0 && accepts(7) && !state) {
					state = mode;
				} else if (accepts(0) && !has_name) {
					keyframes = keyframes_name(token);
					if (!keyframes && keyword != "none") return;
					has_name = true;
				} else return;
			}
			lists.names.push_back(std::move(keyframes));
			lists.durations.push_back(duration.value_or(0));
			lists.delays.push_back(delay.value_or(0));
			lists.easings.push_back(easing.value_or(css_easing{}));
			lists.iterations.push_back(iterations.value_or(1));
			lists.directions.push_back(direction.value_or(0));
			lists.fills.push_back(fill.value_or(0));
			lists.play_states.push_back(state.value_or(0));
		}
		const auto write = [&](std::size_t index, const auto& items) {
			if (accepts(index)) parser.add_parsed_property(fields[index], property_value(items, important));
		};
		write(0, lists.names);
		write(1, lists.durations);
		write(2, lists.delays);
		write(3, lists.easings);
		write(4, lists.iterations);
		write(5, lists.directions);
		write(6, lists.fills);
		write(7, lists.play_states);
	}
}

namespace dockit::css {
	std::optional<KeyframeRule> parse_keyframe_rule(const css_token_vector& prelude, const css_token_vector& body) {
		const auto name_tokens = normalize(prelude, f_remove_whitespace);
		if (name_tokens.size() != 1) return std::nullopt;
		const auto name = keyframes_name(name_tokens.front());
		if (!name) return std::nullopt;
		KeyframeRule result{*name, {}};
		for (const auto& rule : css_parser::parse_stylesheet(body, false)) {
			if (rule->type != raw_rule::qualified) continue;
			std::vector<double> offsets;
			const auto groups = parse_comma_separated_list(normalize(rule->prelude, f_remove_whitespace));
			for (const auto& group : groups) {
				const auto offset = group.size() == 1 ? offset_value(group.front()) : std::nullopt;
				if (!offset) break;
				offsets.push_back(*offset);
			}
			if (offsets.empty() || offsets.size() != groups.size()) continue;
			raw_declaration::vector declarations;
			raw_rule::vector nested_rules;
			css_parser parser(rule->block.value);
			parser.consume_style_block_contents(declarations, nested_rules);
			declarations.erase(std::remove_if(declarations.begin(), declarations.end(), [](const auto& declaration) {
				const auto name = detail::lowcase(declaration.name);
				return declaration.important || name == "animation" ||
					(name.compare(0, 10, "animation-") == 0 && name != "animation-timing-function");
			}), declarations.end());
			for (const auto offset : offsets) {
				auto& frame = result.frames[offset];
				frame.insert(frame.end(), declarations.begin(), declarations.end());
			}
		}
		return result;
	}
}
