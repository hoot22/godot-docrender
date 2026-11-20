// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#pragma once

#include "css_tokenizer.h"

#include <limits>
#include <memory>
#include <tuple>
#include <utility>

namespace dockit::css {

struct selector_specificity {
	int a = 0;
	int b = 0;
	int c = 0;
	int d = 0;

	void operator+=(const selector_specificity& other) {
		const auto sum = [](int left, int right) {
			constexpr int limit = std::numeric_limits<int>::max();
			return right > limit - left ? limit : left + right;
		};
		a = sum(a, other.a);
		b = sum(b, other.b);
		c = sum(c, other.c);
		d = sum(d, other.d);
	}
	bool operator==(const selector_specificity& other) const { return columns() == other.columns(); }
	bool operator<(const selector_specificity& other) const { return columns() < other.columns(); }
	bool operator>(const selector_specificity& other) const { return other < *this; }

private:
	std::tuple<int, int, int, int> columns() const { return {a, b, c, d}; }
};

enum attr_select_type
{
	select_class,
	select_id,
	select_attr,
	select_pseudo_class,
	select_pseudo_element,
	select_nesting,
};

// https://www.w3.org/TR/selectors-4/#attribute-selectors
enum attr_matcher : char
{
	attribute_exists                    = 0,
	attribute_equals                    = '=',
	attribute_contains_string           = '*', // *=
	attribute_contains_word             = '~', // ~=
	attribute_starts_with_string        = '^', // ^=
	attribute_starts_with_string_hyphen = '|', // |=
	attribute_ends_with_string          = '$', // $=
};

struct Selector;
struct NestingSelectorList;

struct AttributeSelector {
	attr_select_type type = select_class;
	string prefix;
	string name;
	string value;
	vector<string> language_ranges;
	attr_matcher matcher{};
	bool caseless_match = false;
	vector<std::shared_ptr<Selector>> selector_list;
	std::shared_ptr<const NestingSelectorList> nesting;
	int a = 0;
	int b = 0;

	AttributeSelector() = default;
	AttributeSelector(attr_select_type type, string name) : type(type), name(std::move(name)) {}
	explicit operator bool() const { return !name.empty(); }
};

struct CompoundSelector {
	using ptr = std::shared_ptr<CompoundSelector>;

	string m_prefix;
	string m_tag;
	vector<AttributeSelector> m_attrs;
	bool m_has_type_selector = false;
};

enum css_combinator
{
	combinator_descendant        = ' ',
	combinator_child             = '>',
	combinator_adjacent_sibling  = '+',
	combinator_general_sibling   = '~'
};

enum selector_state_dependency : unsigned {
	selector_depends_on_hover = 1 << 0,
	selector_depends_on_validity = 1 << 1,
};

struct Selector {
	using ptr = std::shared_ptr<Selector>;
	using vector = std::vector<ptr>;

	selector_specificity m_specificity;
	ptr m_left;
	CompoundSelector m_right;
	css_combinator m_combinator = combinator_descendant;
	css_combinator m_anchor_combinator = combinator_descendant;

	void calc_specificity();
	unsigned state_dependencies() const;
};

struct NestingSelectorList {
	explicit NestingSelectorList(Selector::vector p_selectors);

	Selector::vector selectors;
	Selector::vector element_selectors;
	selector_specificity specificity;
	unsigned state_dependencies = 0;
};

enum class SelectorMode { standards, quirks };

enum {
	strict_mode = 0,
	forgiving_mode = 1,
	forbid_pseudo_elements = 1 << 1,
	strict_arguments = 1 << 2,
	relative_selectors = 1 << 3,
	forbid_has = 1 << 4,
};

Selector::ptr parse_complex_selector(const css_token_vector& tokens, SelectorMode mode, int options);
Selector::vector parse_selector_list(const css_token_vector& tokens, int options, SelectorMode mode);
Selector::vector parse_nested_selector_list(const css_token_vector& tokens, SelectorMode mode,
	const std::shared_ptr<const NestingSelectorList>& parent, int options = strict_mode);
Selector::vector parse_scoped_selector_list(const css_token_vector& tokens, SelectorMode mode, int options = strict_mode);
bool has_balanced_selector_tokens(const css_token_vector& tokens);
bool supports_css_selector(const css_token_vector& tokens, SelectorMode mode);

} // namespace dockit::css
