#pragma once

#include "css_tokenizer.h"
#include "stylesheet.h"
#include "formats/html/css/css_parser.h"

namespace litehtml {

using dockit::css::raw_rule;
using dockit::css::raw_declaration;
using dockit::css::keep_whitespace_fn;
using dockit::css::remove_whitespace;
using dockit::css::normalize;
using dockit::css::parse_comma_separated_list;
using dockit::css::is_declaration_value;
using dockit::css::is_any_value;
using dockit::css::skip_whitespace;

inline constexpr auto f_componentize = dockit::css::f_componentize;
inline constexpr auto f_remove_whitespace = dockit::css::f_remove_whitespace;

class css_parser : public dockit::css::css_parser {
public:
	using dockit::css::css_parser::css_parser;
	using dockit::css::css_parser::parse_stylesheet;

	static raw_rule::vector parse_stylesheet(const string& input, bool top_level);
};

} // namespace litehtml
