#pragma once

#include "css_parser.h"
#include "css_property_value.h"

#include <godot_cpp/variant/string.hpp>

namespace dockit::css {
	class ComputedStyle;
	std::string property_name(const godot::String& name);
	raw_declaration::vector inline_declarations(const godot::String& text);
	std::string serialize_declarations(const raw_declaration::vector& declarations);
	std::string serialize_rule(const raw_rule& rule);
	raw_rule::ptr parse_cssom_rule(const godot::String& text);
	std::string serialize_value(litehtml::string_id name, const litehtml::property_value& value);
	std::string serialize_computed(litehtml::string_id name, const ComputedStyle& style);
	bool is_box_shorthand(litehtml::string_id name);
	std::string serialize_box_shorthand(std::vector<std::string> values);
}
