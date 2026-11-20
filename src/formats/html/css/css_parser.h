// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#pragma once

#include "css_tokenizer.h"

#include <functional>
#include <memory>

namespace dockit::css {

using std::shared_ptr;

// https://www.w3.org/TR/cssom-1/#css-declarations
struct raw_declaration
{
	using vector = std::vector<raw_declaration>;

	string name; // property name
	css_token_vector value = {}; // default value is specified here to get rid of gcc warning "missing initializer for member"
	bool important = false;

	operator bool() const { return name != ""; }
};

// intermediate half-parsed rule that is used internally by the parser
class raw_rule
{
public:
	using ptr = shared_ptr<raw_rule>;
	using vector = std::vector<ptr>;

	enum rule_type { qualified, at, declaration_list };

	raw_rule(rule_type type, string name = "") : type(type), name(name) {}

	rule_type type;
	// An at-rule has a name, a prelude consisting of a list of component values, and an optional block consisting of a simple {} block.
	string name;
	// https://www.w3.org/TR/css-syntax-3/#qualified-rule
	// A qualified rule has a prelude consisting of a list of component values, and a block consisting of a simple {} block.
	// Note: Most qualified rules will be style rules, where the prelude is a selector and the block a list of declarations.
	css_token_vector prelude;
	css_token block;
	raw_declaration::vector declarations;
};

class css_parser
{
	css_token_vector m_tokens;
	int m_index = 0;

	css_token next_token();
	css_token peek_token();

public:
	css_parser() {}
	css_parser(const css_token_vector& tokens) : m_tokens(tokens) {}

	// This overload accepts decoded input. Callers control byte decoding.
	static raw_rule::vector parse_stylesheet(const string& input, bool top_level);
	static raw_rule::vector parse_stylesheet(const css_token_vector& input, bool top_level);
	raw_rule::vector		consume_list_of_rules(bool top_level);
	raw_rule::ptr			consume_qualified_rule();
	raw_rule::ptr			consume_at_rule();
	css_token				consume_simple_block(char opening_bracket);
	css_token				consume_component_value();
	css_token				consume_function(const string& name);

	raw_declaration			consume_declaration();
	raw_rule::vector		consume_block_contents();
	void					consume_style_block_contents(/*out*/ raw_declaration::vector& decls, /*out*/ raw_rule::vector& rules);
};

using keep_whitespace_fn = std::function<bool (const css_token& left, const css_token& right)>;
void remove_whitespace(css_token_vector& tokens, keep_whitespace_fn keep_whitespace = 0);

enum {
	f_componentize = 1,
	f_remove_whitespace = 2
};
css_token_vector normalize(string input, int options = 0, keep_whitespace_fn keep_whitespace = {});
css_token_vector normalize(css_token_vector input, int options = 0, keep_whitespace_fn keep_whitespace = {});

vector<css_token_vector> parse_comma_separated_list(const css_token_vector& tokens);
bool is_declaration_value(const css_token_vector& tokens, int index = 0);
bool is_any_value(const css_token_vector& tokens);
bool skip_whitespace(const css_token_vector& tokens, int& index);

} // namespace dockit::css
