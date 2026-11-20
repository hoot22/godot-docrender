#pragma once

#include "css_parser.h"
#include "css_transition.h"

#include <map>

namespace litehtml {
	class css_value_parser;
	class style;

	using css_animation_names = std::vector<std::optional<std::string>>;

	struct css_animation {
		double duration = 0;
		double delay = 0;
		css_easing easing;
		double iterations = 1;
		int direction = 0;
		int fill = 0;
		bool paused = false;
	};

	struct css_animation_styles {
		css_animation_names names{std::nullopt};
		css_transition_times durations{0};
		css_transition_times delays{0};
		css_transition_easings easings{{}};
		css_transition_times iterations{1};
		std::vector<int> directions{0};
		std::vector<int> fills{0};
		std::vector<int> play_states{0};

		css_animation at(std::size_t index) const;
	};

	struct css_keyframes {
		std::map<double, std::shared_ptr<const style>> frames;
	};

	const std::array<string_id, 8>& css_animation_property_names();
	void parse_css_animation(css_value_parser& parser, string_id name, const css_token_vector& value, bool important);
}

namespace dockit::css {
	struct KeyframeRule {
		std::string name;
		std::map<double, raw_declaration::vector> frames;
	};

	std::optional<KeyframeRule> parse_keyframe_rule(const css_token_vector& prelude, const css_token_vector& body);
}
