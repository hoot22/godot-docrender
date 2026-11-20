#pragma once

#include "css_property_value.h"

#include <variant>
#include <vector>

namespace dockit::css {
	class ComputedStyle;
	using AnimatedValue = std::variant<float, litehtml::web_color, litehtml::css_length, litehtml::css_transform_list,
		litehtml::css_text_indent, litehtml::css_font_size_adjust>;
	using AnimatedValues = std::vector<AnimatedValue>;

	enum class AnimatedRange { unrestricted, non_negative, unit_interval, font_weight };
	struct AnimatedProperty {
		litehtml::string_id name;
		AnimatedValue (*read)(const ComputedStyle&);
		AnimatedRange range = AnimatedRange::non_negative;
		bool distinguish_number = false;
	};

	const std::vector<AnimatedProperty>& animated_properties();
	AnimatedValues read_animated_values(const ComputedStyle& style);
	bool equal_animated_value(const AnimatedProperty& property, const AnimatedValue& a, const AnimatedValue& b);
	bool can_interpolate_animated_value(const AnimatedProperty& property, const AnimatedValue& a, const AnimatedValue& b);
	AnimatedValue interpolate_animated_value(const AnimatedProperty& property, const AnimatedValue& a,
		const AnimatedValue& b, double progress, bool allow_discrete);
}
