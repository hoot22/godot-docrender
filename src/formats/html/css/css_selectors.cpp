// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_selectors.h"

#include "css_parser.h"
#include "css_syntax_util.h"

#include <charconv>
#include <set>

namespace dockit::css {

using namespace detail;
using std::make_shared;
using string_vector = vector<string>;

void Selector::calc_specificity()
{
	m_specificity = selector_specificity();
	if (m_right.m_tag != "*")
		m_specificity += {0, 0, 0, 1};

	for (const auto& attr : m_right.m_attrs)
	{
		if (attr.type == select_nesting)
		{
			if (attr.nesting) m_specificity += attr.nesting->specificity;
		}
		else if (attr.type == select_id)
			m_specificity += {0, 1, 0, 0};
		else if (attr.type == select_pseudo_element)
			m_specificity += {0, 0, 0, 1};
		else if (attr.type == select_pseudo_class && attr.name == "where")
			continue;
		else if (attr.type == select_pseudo_class &&
			is_one_of(attr.name, "is", "not", "nth-child", "nth-last-child", "has"))
		{
			if (is_one_of(attr.name, "nth-child", "nth-last-child"))
				m_specificity += {0, 0, 1, 0};

			selector_specificity maximum;
			for (const auto& argument : attr.selector_list)
			{
				argument->calc_specificity();
				maximum = std::max(maximum, argument->m_specificity);
			}
			m_specificity += maximum;
		}
		else
			m_specificity += {0, 0, 1, 0};
	}
	if (m_left)
	{
		m_left->calc_specificity();
		m_specificity += m_left->m_specificity;
	}
}

unsigned Selector::state_dependencies() const
{
	unsigned dependencies = 0;
	for (auto part = this; part; part = part->m_left.get()) {
		for (const auto& attribute : part->m_right.m_attrs) {
			if (attribute.type == select_pseudo_class) {
				if (attribute.name == "hover") dependencies |= selector_depends_on_hover;
				if (attribute.name == "valid" || attribute.name == "invalid") dependencies |= selector_depends_on_validity;
			}
			if (attribute.nesting) dependencies |= attribute.nesting->state_dependencies;
			for (const auto& argument : attribute.selector_list) dependencies |= argument->state_dependencies();
		}
	}
	return dependencies;
}

// https://www.w3.org/TR/selectors-4/#type-nmsp
// <ns-prefix> = [ <ident-token> | '*' ]? '|'     https://www.w3.org/TR/selectors-4/#typedef-ns-prefix
string parse_ns_prefix(const css_token_vector& tokens, int& index)
{
	const auto& a = at(tokens, index);
	const auto& b = at(tokens, index + 1);

	if (a.ch == '|')
	{
		index++;
		return "";
	}

	if ((a.type == IDENT || a.ch == '*') && b.ch == '|')
	{
		index += 2;
		return a.type == IDENT ? a.name : "*";
	}

	return "";
}

struct wq_name
{
	string prefix;
	string name;
};

// <wq-name> = <ns-prefix>? <ident-token>
// Whitespace is forbidden between any of the components of a <wq-name>.
wq_name parse_wq_name(const css_token_vector& tokens, int& index)
{
	int start = index;
	string prefix = parse_ns_prefix(tokens, index);

	auto tok = at(tokens, index);
	if (tok.type == IDENT)
	{
		index++;
		return { prefix, tok.name };
	}
	// restore index to before <ns-prefix> if failed to parse <ident-token>
	index = start;

	// handle situation when a name is erroneously parsed as prefix, eg. [x|=a]
	tok = at(tokens, index);
	if (tok.type == IDENT)
	{
		index++;
		return { "", tok.name };
	}

	return {};
}

// https://www.w3.org/TR/selectors-4/#typedef-type-selector
// <type-selector> = <wq-name> | <ns-prefix>? '*'
// <wq-name> = <ns-prefix>? <ident-token>
// So:
// <type-selector> = <ns-prefix>? [ <ident-token> | '*' ]
wq_name parse_type_selector(const css_token_vector& tokens, int& index)
{
	int start = index;
	string prefix = parse_ns_prefix(tokens, index);

	const auto& tok = at(tokens, index);
	if (tok.type == IDENT || tok.ch == '*')
	{
		index++;
		string name = tok.type == IDENT ? tok.name : "*";
		// type selector is always ASCII-case-insensitive for HTML documents, regardless of document mode (quirks/no quirks)
		return { lowcase(prefix), lowcase(name) };
	}
	// restore index to before <ns-prefix> if failed to parse <ident-token> or '*'
	index = start;
	return {};
}

// <attr-matcher> = [ '~' | '|' | '^' | '$' | '*' ]? '='
bool parse_attr_matcher(const css_token_vector& tokens, int& index, attr_matcher& matcher)
{
	const auto& a = at(tokens, index);
	const auto& b = at(tokens, index + 1);

	if (a.ch == '=')
	{
		index++;
		matcher = attribute_equals;
		return true;
	}

	if (!(is_one_of(a.ch, '~', '|', '^', '$', '*') && b.ch == '='))
		return false;

	index += 2;
	matcher = (attr_matcher)a.ch;
	return true;
}

// https://www.w3.org/TR/selectors-4/#typedef-attribute-selector
// <attribute-selector> = '[' <wq-name> ']' |
//                        '[' <wq-name> <attr-matcher> [ <string-token> | <ident-token> ] <attr-modifier>? ']'
// <attr-matcher> = [ '~' | '|' | '^' | '$' | '*' ]? '='
// <attr-modifier> = i | s
AttributeSelector parse_attribute_selector(const css_token& block)
{
	AttributeSelector selector;

	const css_token_vector& tokens = block.value;
	int index = 0;

	// <wq-name>
	skip_whitespace(tokens, index);
	wq_name wq_name = parse_wq_name(tokens, index);
	if (wq_name.name == "") return {};

	// attribute name in attribute selector is ASCII case-insensitive for HTML documents, regardless of document mode (quirks/no quirks)
	auto prefix = lowcase(wq_name.prefix);
	auto name   = lowcase(wq_name.name);

	skip_whitespace(tokens, index);
	if (index == (int)tokens.size()) // [name]
	{
		selector.type    = select_attr;
		selector.prefix  = prefix;
		selector.name    = name;
		selector.matcher = attribute_exists;
		return selector;
	}

	// <attr-matcher>
	skip_whitespace(tokens, index);
	attr_matcher matcher;
	if (!parse_attr_matcher(tokens, index, matcher))
		return {};

	// <string-token> | <ident-token>
	skip_whitespace(tokens, index);
	const css_token& value = at(tokens, index);
	if (value.type != STRING && value.type != IDENT)
		return {};
	index++;

	// <attr-modifier>?
	skip_whitespace(tokens, index);
	char modifier = 0;
	const css_token& tok = at(tokens, index);
	if (tok.type == IDENT)
	{
		if      (tok.ident() == "s") modifier = 's';
		else if (tok.ident() == "i") modifier = 'i';
		else return {}; // junk at the end of attribute selector
		index++;
	}

	skip_whitespace(tokens, index);
	if (index != (int)tokens.size())
		return {}; // junk at the end of attribute selector

	// https://html.spec.whatwg.org/multipage/semantics-other.html#case-sensitivity-of-selectors
	// Attribute selectors on an HTML element in an HTML document must treat the values
	// of attributes with the following names as ASCII case-insensitive (unless s modifier is specified):
	static string_vector special_attributes = {
		"accept",
		"accept-charset",
		"align",
		"alink",
		"axis",
		"bgcolor",
		"charset",
		"checked",
		"clear",
		"codetype",
		"color",
		"compact",
		"declare",
		"defer",
		"dir",
		"direction",
		"disabled",
		"enctype",
		"face",
		"frame",
		"hreflang",
		"http-equiv",
		"lang",
		"language",
		"link",
		"media",
		"method",
		"multiple",
		"nohref",
		"noresize",
		"noshade",
		"nowrap",
		"readonly",
		"rel",
		"rev",
		"rules",
		"scope",
		"scrolling",
		"selected",
		"shape",
		"target",
		"text",
		"type",
		"valign",
		"valuetype",
		"vlink",
	};

	selector.type     = select_attr;
	selector.prefix   = prefix;
	selector.name     = name;
	selector.matcher  = matcher;
	selector.caseless_match = modifier == 'i' || (!modifier && std::find(special_attributes.begin(), special_attributes.end(), name) != special_attributes.end());
	selector.value    = selector.caseless_match ? lowcase(value.str) : value.str;
	return selector;
}

struct an_b
{
	int a, b;
	bool valid;
	an_b() : a(), b(), valid(false) {}
	an_b(int a, int b) : a(a), b(b), valid(true) {}
};

// https://www.w3.org/TR/css-syntax-3/#anb-syntax
static bool parse_an_b_integer(std::string_view text, int& result)
{
	if (!text.empty() && text.front() == '+') text.remove_prefix(1);
	if (text.empty()) return false;
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
	return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size();
}

static bool parse_an_b_number(const css_token& token, int& result)
{
	return token.type == NUMBER && token.n.number_type == css_number_integer &&
		parse_an_b_integer(token.repr, result);
}

static bool is_signed_integer(const css_token& token)
{
	return token.type == NUMBER && !token.repr.empty() && is_one_of(token.repr.front(), '+', '-');
}

static an_b parse_an_b(css_token_vector tokens)
{
	remove_whitespace(tokens);
	if (tokens.empty()) return {};

	size_t index = 0;
	const bool leading_plus = tokens.front().type == '+';
	if (leading_plus)
	{
		if (tokens.size() < 2 || tokens[1].preceded_by_space || tokens[1].type != IDENT) return {};
		++index;
	}
	const auto& first = tokens[index++];
	int a = 1, b = 0;
	string unit;
	if (first.type == IDENT)
	{
		unit = first.ident();
		if (!leading_plus && tokens.size() == 1)
		{
			if (unit == "even") return {2, 0};
			if (unit == "odd") return {2, 1};
		}
		if (!unit.empty() && unit.front() == '-')
		{
			if (leading_plus) return {};
			a = -1;
			unit.erase(0, 1);
		}
	}
	else if (first.type == DIMENSION && first.n.number_type == css_number_integer)
	{
		unit = lowcase(first.unit);
		const auto number_end = first.repr.find_first_not_of("0123456789", !first.repr.empty() && is_one_of(first.repr.front(), '+', '-') ? 1 : 0);
		if (!parse_an_b_integer(std::string_view(first.repr).substr(0, number_end), a)) return {};
	}
	else
	{
		return tokens.size() == 1 && parse_an_b_number(first, b) ? an_b(0, b) : an_b();
	}

	if (unit.empty() || unit.front() != 'n') return {};
	const size_t remaining = tokens.size() - index;
	if (unit.size() > 2)
	{
		if (unit[1] != '-' || !std::all_of(unit.begin() + 2, unit.end(), [](char c) { return c >= '0' && c <= '9'; }))
			return {};
		return remaining == 0 && parse_an_b_integer(std::string_view(unit).substr(1), b) ? an_b(a, b) : an_b();
	}
	if (unit == "n-")
	{
		if (remaining != 1 || is_signed_integer(tokens[index])) return {};
		const auto& digits = tokens[index];
		return digits.type == NUMBER && digits.n.number_type == css_number_integer && parse_an_b_integer("-" + digits.repr, b) ?
			an_b(a, b) : an_b();
	}
	if (unit != "n") return {};
	if (remaining == 0) return {a, 0};
	if (remaining == 1 && is_signed_integer(tokens[index]) && parse_an_b_number(tokens[index], b)) return {a, b};
	if (remaining != 2 || !is_one_of(tokens[index].type, '+', '-')) return {};
	const auto& digits = tokens[index + 1];
	if (digits.type != NUMBER || digits.n.number_type != css_number_integer || is_signed_integer(digits)) return {};
	return parse_an_b_integer((tokens[index].type == '-' ? "-" : "") + digits.repr, b) ? an_b(a, b) : an_b();
}

int find_of_keyword(const css_token_vector& tokens)
{
	for (int i = 0; i < (int)tokens.size(); i++)
	{
		if (tokens[i].ident() == "of")
			return i;
	}
	return -1;
}

// https://www.w3.org/TR/selectors-4/#the-nth-child-pseudo
AttributeSelector parse_nth_child(const css_token& token, bool of_keyword, SelectorMode mode, int options)
{
	AttributeSelector selector(select_pseudo_class, lowcase(token.name));
	const auto& tokens = token.value;
	const int of_index = of_keyword ? find_of_keyword(tokens) : -1;
	if (of_index >= 0)
	{
		const int nested = (options & (strict_arguments | forbid_has)) | forbid_pseudo_elements;
		selector.selector_list = parse_selector_list(slice(tokens, of_index + 1), nested | strict_mode, mode);
		if (selector.selector_list.empty()) return {};
	}

	const an_b value = parse_an_b(of_index < 0 ? tokens : css_token_vector(tokens.begin(), tokens.begin() + of_index));
	if (!value.valid) return {};
	selector.a = value.a;
	selector.b = value.b;
	return selector;
}

AttributeSelector parse_function_pseudo_class(const css_token& token, SelectorMode mode, int options)
{
	string name = lowcase(token.name);
	if (name == "nth-child" || name == "nth-last-child")
	{
		return parse_nth_child(token, true, mode, options);
	}
	else if (name == "nth-of-type" || name == "nth-last-of-type")
	{
		return parse_nth_child(token, false, mode, options);
	}
	else if (is_one_of(name, "is", "where", "not", "has"))
	{
		if (name == "has" && (options & forbid_has))
			return {};

		const bool forgiving = is_one_of(name, "is", "where") && !(options & strict_arguments);
		int nested = (options & (strict_arguments | forbid_has)) | forbid_pseudo_elements;
		if (forgiving) nested |= forgiving_mode;
		if (name == "has") nested |= relative_selectors | forbid_has;

		AttributeSelector selector(select_pseudo_class, name);
		selector.selector_list = parse_selector_list(token.value, nested, mode);
		if (!forgiving && selector.selector_list.empty()) return {};
		return selector;
	}
	else if (name == "lang") // https://www.w3.org/TR/selectors-4/#the-lang-pseudo
	{
		AttributeSelector selector(select_pseudo_class, name);
		for (auto range : parse_comma_separated_list(token.value))
		{
			remove_whitespace(range);
			if (range.size() != 1 || !is_one_of(range[0].type, IDENT, STRING)) return {};
			selector.language_ranges.push_back(lowcase(range[0].str));
		}
		return selector;
	}
	else if (name == "dir") // https://www.w3.org/TR/selectors-4/#the-dir-pseudo
	{
		css_token_vector arguments = token.value;
		remove_whitespace(arguments);
		if (arguments.size() != 1 || arguments[0].type != IDENT) return {};
		const string direction = lowcase(arguments[0].ident());
		if (!is_one_of(direction, "ltr", "rtl")) return {};
		AttributeSelector selector(select_pseudo_class, name);
		selector.value = direction;
		return selector;
	}
	return {};
}

// simple = non-functional (without parentheses)
bool is_supported_simple_pseudo_class(const string& name)
{
	static std::set<string> supported_simple_pseudo_classes =
	{
		// Location Pseudo-classes  https://www.w3.org/TR/selectors-4/#location
		"any-link", "link", "visited", "local-link", "target", "target-within", "scope",
		// User Action Pseudo-classes  https://www.w3.org/TR/selectors-4/#useraction-pseudos
		"hover", "active", "focus", "focus-visible", "focus-within",
		"enabled", "disabled", "checked", "placeholder-shown", "open", "modal", "indeterminate",
		"required", "optional", "valid", "invalid", "read-only", "read-write",
		// Tree-Structural pseudo-classes  https://www.w3.org/TR/selectors-4/#structural-pseudos
		"root", "empty", "first-child", "last-child", "only-child", "first-of-type", "last-of-type", "only-of-type",
	};
	return supported_simple_pseudo_classes.count(lowcase(name)) == 1;
}

// https://www.w3.org/TR/selectors-4/#typedef-pseudo-class-selector
// <pseudo-class-selector> = ':' <ident-token> |
//                           ':' <function-token> <any-value> ')'
//                           where <ident-token> is not before, after, first-line or first-letter
AttributeSelector parse_pseudo_class(const css_token_vector& tokens, int& index, SelectorMode mode, int options)
{
	const auto& a = at(tokens, index);
	const auto& b = at(tokens, index + 1);

	if (a.ch != ':')
		return {};

	if (b.type == IDENT)
	{
		// unsupported pseudo-classes must be treated as invalid:  https://www.w3.org/TR/selectors-4/#w3c-partial
		if (!is_supported_simple_pseudo_class(b.ident()))
			return {};

		index += 2;
		return { select_pseudo_class, b.ident() };
	}
	if (b.type == CV_FUNCTION)
	{
		AttributeSelector sel = parse_function_pseudo_class(b, mode, options);
		if (sel) index += 2;
		return sel;
	}
	return {};
}

// https://www.w3.org/TR/selectors-4/#typedef-subclass-selector
// <subclass-selector> = <id-selector> | <class-selector> | <attribute-selector> | <pseudo-class-selector>
// <id-selector> = <hash-token> with hash_type == ID
// <class-selector> = '.' <ident-token>
AttributeSelector parse_subclass_selector(const css_token_vector& tokens, int& index, SelectorMode mode, int options)
{
	AttributeSelector selector;

	const auto& tok0 = at(tokens, index);
	const auto& tok1 = at(tokens, index + 1);

	switch (tok0.type)
	{
	case HASH:
		if (tok0.hash_type == css_hash_id)
		{
			index++;
			selector.type = select_id;
			string name = tok0.name;
			// ids are matched ASCII case-insensitively in quirks mode
			if (mode == SelectorMode::quirks) name = lowcase(name);
			selector.name = name;
			return selector;
		}
		return {};

	case '.':
		if (tok1.type == IDENT)
		{
			index += 2;
			selector.type = select_class;
			string name = tok1.name;
			// class names are matched ASCII case-insensitively in quirks mode
			if (mode == SelectorMode::quirks) name = lowcase(name);
			selector.name = name;
			return selector;
		}
		return {};

	case SQUARE_BLOCK:
		selector = parse_attribute_selector(tok0);
		if (selector) index++;
		return selector;

	case AMPERSAND:
		++index;
		return { select_nesting, "&" };

	default:
		return parse_pseudo_class(tokens, index, mode, options);
	}
}

// simple = non-functional (without parentheses)
bool is_supported_simple_pseudo_element(const string& name)
{
	return is_one_of(lowcase(name),
		// Typographic Pseudo-elements  https://www.w3.org/TR/css-pseudo-4/#typographic-pseudos
		//"first-line", "first-letter",
		// Highlight Pseudo-elements  https://www.w3.org/TR/css-pseudo-4/#highlight-pseudos
		//"selection",
		// Tree-Abiding Pseudo-elements  https://www.w3.org/TR/css-pseudo-4/#treelike
		"before", "after", "marker", "placeholder", "selection", "backdrop"
	);
}

AttributeSelector parse_pseudo_element(const css_token_vector& tokens, int& index)
{
	const auto& a = at(tokens, index);
	const auto& b = at(tokens, index + 1);
	const auto& c = at(tokens, index + 2);

	if (a.ch != ':')
		return {};
	if (b.ch != ':' && b.type != IDENT)
		return {};

	if (b.type == IDENT) // legacy syntax with one ':'  https://www.w3.org/TR/selectors-4/#single-colon-pseudos
	{
		if (!is_one_of(b.ident(), "before", "after")) // first-line/letter are not supported
			return {};

		index += 2;
		return {select_pseudo_element, b.ident()};
	}

	if (c.type == IDENT) // normal syntax with '::'
	{
		if (!is_supported_simple_pseudo_element(c.ident()))
			return {};

		index += 3;
		return {select_pseudo_element, c.ident()};
	}

	return {};
}

// https://www.w3.org/TR/selectors-4/#typedef-compound-selector
// <compound-selector> = [ <type-selector>? <subclass-selector>*
//                         [ <pseudo-element-selector> <pseudo-class-selector>* ]* ]!
// NOTE: This grammar allows pseudo-classes to go before #id and .class and [attr].
// Whitespace is forbidden:
// * Between any of the top-level components of a <compound-selector>
CompoundSelector::ptr parse_compound_selector(const css_token_vector& tokens, int& index, SelectorMode mode, int options)
{
	auto selector = make_shared<CompoundSelector>();

	// <type-selector>?
	wq_name wq_name = parse_type_selector(tokens, index);
	selector->m_prefix = wq_name.prefix;
	selector->m_tag    = wq_name.name;
	selector->m_has_type_selector = !wq_name.name.empty();

	// <subclass-selector>*
	while (AttributeSelector sel = parse_subclass_selector(tokens, index, mode, options))
		selector->m_attrs.push_back(sel);

	// The supported pseudo-elements have no implemented sub-pseudo-elements.
	if (auto sel = parse_pseudo_element(tokens, index))
	{
		selector->m_attrs.push_back(sel);

		while ((sel = parse_pseudo_class(tokens, index, mode, options)))
			selector->m_attrs.push_back(sel);
	}

	// [..]!  "must produce at least one value"  https://www.w3.org/TR/css-values-4/#mult-req
	if (selector->m_tag == "" && selector->m_attrs.empty())
		return nullptr;

	if (selector->m_tag == "")
		selector->m_tag = "*";

	return selector;
}

// <combinator> = '>' | '+' | '~' | [ '|' '|' ]
// <whitespace> combinator is also handled here
// parse_combinator consumes all leading and trailing whitespace
// column combinator || is at-risk https://www.w3.org/TR/selectors-4/  and not implemented in Chrome/Firefox  https://caniuse.com/mdn-Selectors_column
int parse_combinator(const css_token_vector& tokens, int& index)
{
	bool ws = skip_whitespace(tokens, index);

	const css_token& tok = at(tokens, index);
	if (is_one_of(tok.ch, '>', '+', '~'))
//	if (tok.ch in ${'>', '+', '~'})
	{
		index++;
		skip_whitespace(tokens, index);
		return tok.ch;
	}

	return ws ? ' ' : 0;
}

Selector::ptr parse_complex_selector(const css_token_vector& tokens, SelectorMode mode, int options)
{
	int index = 0;
	skip_whitespace(tokens, index);
	const int leading = options & relative_selectors ? parse_combinator(tokens, index) : 0;
	auto sel = parse_compound_selector(tokens, index, mode, options);
	if (!sel) return nullptr;

	auto selector = make_shared<Selector>();
	if (leading) selector->m_anchor_combinator = static_cast<css_combinator>(leading);
	selector->m_right = *sel;

	// NOTE: all the whitespace is handled by parse_combinator, that's why skip_whitespace is never called in the loop
	// NOTE: parse_complex_selector is different from most other parse_xxx functions in that it's required
	//       to parse all input tokens, it doesn't just parse as much as possible.
	while (true)
	{
		int combinator = parse_combinator(tokens, index);
		if (index == (int)tokens.size())
			// combinator == 0 means index already was at the end before the call to parse_combinator
			return !combinator || combinator == ' ' ? selector : nullptr;
		if (!combinator) // not the end and combinator failed to parse
			return nullptr;
		// None of the supported pseudo-elements can be followed by a combinator.
		if (std::any_of(sel->m_attrs.begin(), sel->m_attrs.end(), [](const auto& attribute) {
			return attribute.type == select_pseudo_element;
		}))
			return nullptr;

		// otherwise: index is not at the end, combinator is good and tokens[index] is not whitespace
		// it means if parse_compound_selector fails it's an error
		sel = parse_compound_selector(tokens, index, mode, options);
		if (!sel)
			return nullptr;

		auto new_selector = make_shared<Selector>();
		new_selector->m_left = selector;
		new_selector->m_right = *sel;
		new_selector->m_combinator = (css_combinator)combinator;
		selector = new_selector;
	}
}

// Check for a selector of the given type in any compound.
static bool has_selector(const Selector& selector, attr_select_type type)
{
	for (const auto& sel : selector.m_right.m_attrs)
	{
		if (sel.type == type)
			return true;
	}

	if (selector.m_left)
		return has_selector(*selector.m_left, type);

	return false;
}

NestingSelectorList::NestingSelectorList(Selector::vector p_selectors) : selectors(std::move(p_selectors))
{
	for (const auto& selector : selectors) {
		selector->calc_specificity();
		specificity = std::max(specificity, selector->m_specificity);
		if (!has_selector(*selector, select_pseudo_element)) {
			element_selectors.push_back(selector);
			state_dependencies |= selector->state_dependencies();
		}
	}
}

static bool contains_anchor(const css_token_vector& tokens, bool scoped)
{
	for (size_t index = 0; index < tokens.size(); ++index) {
		const auto& token = tokens[index];
		if (token.type == AMPERSAND || (scoped && token.type == ':' && index + 1 < tokens.size() &&
			lowcase(tokens[index + 1].ident()) == "scope")) return true;
		if (token.is_component_value() && contains_anchor(token.value, scoped)) return true;
	}
	return false;
}

static void bind_nesting(Selector& selector, const std::shared_ptr<const NestingSelectorList>& parent)
{
	for (auto part = &selector; part; part = part->m_left.get()) {
		for (auto& attribute : part->m_right.m_attrs) {
			if (attribute.type == select_nesting) attribute.nesting = parent;
			for (const auto& argument : attribute.selector_list) bind_nesting(*argument, parent);
		}
	}
}

static Selector::vector parse_anchored_selector_list(const css_token_vector& tokens, SelectorMode mode,
	const std::shared_ptr<const NestingSelectorList>& parent, int options, bool scoped)
{
	Selector::vector result;
	for (auto& branch : parse_comma_separated_list(tokens)) {
		int first = 0;
		skip_whitespace(branch, first);
		if (first == static_cast<int>(branch.size())) return {};
		const auto leading = at(branch, first).type;
		if (is_one_of(leading, '>', '+', '~') || !contains_anchor(branch, scoped)) {
			branch.insert(branch.begin(), {css_token(AMPERSAND), css_token(WHITESPACE)});
		}
		auto parsed = parse_complex_selector(branch, mode, options);
		if (!parsed || ((options & forbid_pseudo_elements) && has_selector(*parsed, select_pseudo_element))) return {};
		bind_nesting(*parsed, parent);
		result.push_back(std::move(parsed));
	}
	return result;
}

Selector::vector parse_nested_selector_list(const css_token_vector& tokens, SelectorMode mode,
	const std::shared_ptr<const NestingSelectorList>& parent, int options)
{
	return parse_anchored_selector_list(tokens, mode, parent, options, false);
}

Selector::vector parse_scoped_selector_list(const css_token_vector& tokens, SelectorMode mode, int options)
{
	return parse_anchored_selector_list(tokens, mode, {}, options, true);
}

// https://www.w3.org/TR/css-syntax-3/#parse-comma-list
// https://www.w3.org/TR/selectors-4/#selector-list
// https://www.w3.org/TR/selectors-4/#forgiving-selector
// Parse comma-separated list of complex selectors.
Selector::vector parse_selector_list(const css_token_vector& tokens, int options, SelectorMode mode)
{
	// NOTE: this is unnecessary: "If input contains only <whitespace-token>s, return an empty list."

	vector<css_token_vector> list_of_lists = parse_comma_separated_list(tokens);
	Selector::vector result;

	for (const auto& list: list_of_lists)
	{
		Selector::ptr selector = parse_complex_selector(list, mode, options);

		// if selector is failed to parse or not allowed by the options
		if (!selector ||
			((options & forbid_pseudo_elements) && has_selector(*selector, select_pseudo_element)))
		{
			// in forgiving mode, ignore the bad selector
			if (options & forgiving_mode)
				continue;

			// in strict mode, entire selector-list fails to parse because of one bad selector
			return {};
		}

		result.push_back(selector);
	}

	return result;
}

static bool supports_selector_namespaces(const Selector& selector)
{
	// Named prefixes require a namespace declaration, which litehtml does not process.
	for (auto part = &selector; part; part = part->m_left.get())
	{
		if (part->m_right.m_prefix != "" && part->m_right.m_prefix != "*")
			return false;
		for (const auto& attr : part->m_right.m_attrs)
		{
			if (attr.prefix != "" && attr.prefix != "*")
				return false;
			for (const auto& argument : attr.selector_list)
			{
				if (!supports_selector_namespaces(*argument))
					return false;
			}
		}
	}
	return true;
}

bool supports_css_selector(const css_token_vector& tokens, SelectorMode mode)
{
	const auto selector = parse_complex_selector(tokens, mode, strict_arguments);
	return selector && supports_selector_namespaces(*selector);
}

bool has_balanced_selector_tokens(const css_token_vector& tokens) {
	std::vector<css_token_type> closing;
	for (const auto& token : tokens) {
		switch (token.type) {
		case LEFT_BRACKET:
			closing.push_back(RIGHT_BRACKET);
			break;
		case FUNCTION:
		case LEFT_PAREN:
			closing.push_back(RIGHT_PAREN);
			break;
		case RIGHT_BRACKET:
		case RIGHT_PAREN:
			if (closing.empty() || closing.back() != token.type) {
				return false;
			}
			closing.pop_back();
			break;
		case BAD_STRING:
		case BAD_URL:
			return false;
		default:
			break;
		}
	}
	return closing.empty();
}

} // namespace dockit::css
