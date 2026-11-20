// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include <litehtml/html.h>
#include "formats/html/css/css_math.h"
#include <litehtml/media_query.h>
#include <litehtml/css_parser.h>
#include <cassert>
#include <cmath>
#include <litehtml/document.h>
#include <litehtml/document_container.h>
#include <litehtml/html_tag.h>
#include "css_variables.h"


namespace litehtml
{

namespace {

std::optional<css_token_vector> read_custom_value(const html_tag* tag, string_id name)
{
	css_token_vector tokens;
	if (tag && tag->get_custom_property(name, tokens)) return tokens;
	return std::nullopt;
}

std::optional<css_token_vector> substitute_query_variables(const css_token_vector& tokens, const html_tag& container)
{
	const css_inherited_variable read = [&](string_id name, css_token_vector& value) {
		return container.get_custom_property(name, value);
	};
	const css_custom_rollback keep = [](string_id, const property_value& value, css_wide_keyword) -> const property_value& {
		return value;
	};
	const auto doc = container.get_document();
	const auto registered = doc ? doc->registered_properties() : css_registered_properties{};
	return css_variable_resolver(read, {}, registered, doc ? doc->container() : nullptr, keep).substitute(tokens);
}

bool equal_custom_tokens(const css_token_vector& a, const css_token_vector& b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i) {
		const auto& left = a[i];
		const auto& right = b[i];
		if (left.type != right.type || left.str != right.str ||
			(i != 0 && left.preceded_by_space != right.preceded_by_space)) return false;
		if (is_one_of(left.type, NUMBER, DIMENSION, PERCENTAGE) &&
			(left.n.number != right.n.number || left.n.number_type != right.n.number_type)) return false;
		if (left.type == HASH && left.hash_type != right.hash_type) return false;
		if (left.is_component_value() && !equal_custom_tokens(left.value, right.value)) return false;
	}
	return true;
}

} // namespace

bool style_query_feature::check(const html_tag& container) const
{
	const auto actual = read_custom_value(&container, name);
	if (!value) return actual.has_value();

	auto expected = substitute_query_variables(*value, container);
	if (expected) {
		switch (read_css_wide_keyword(*expected)) {
		case css_wide_keyword::revert:
		case css_wide_keyword::revert_layer:
			return false;
		case css_wide_keyword::initial:
			expected.reset();
			break;
		case css_wide_keyword::inherit:
		case css_wide_keyword::unset:
			expected = read_custom_value(dynamic_cast<const html_tag*>(container.parent().get()), name);
			break;
		default:
			break;
		}
	}
	if (!expected || !actual) return expected.has_value() == actual.has_value();
	return equal_custom_tokens(*actual, *expected);
}

bool eval_op(float x, short op, float value)
{
	const float epsilon = 0.00001f;
	if (abs(x - value) < epsilon)
	{
		if (is_one_of(op, '=', u'⩾', u'⩽')) return true;
		if (op == u'≠') return false;
	}

	switch (op)
	{
	case  '<': return x < value;
	case u'⩽': return x <= value;
	case  '>': return x > value;
	case u'⩾': return x >= value;
	case  '=': return x == value;
	case u'≠': return x != value;
	default:   return false;
	}
}

trilean media_feature::compare(float x, const media_features& features, const query_length_environment* lengths) const
{
	const auto lower = resolve_threshold(first, features, lengths);
	if (!lower) return Unknown;
	if (!op2) return static_cast<trilean>(eval_op(x, op, *lower));
	const auto upper = resolve_threshold(second, features, lengths);
	if (!upper) return Unknown;
	return static_cast<trilean>(eval_op(*lower, op, x) && eval_op(x, op2, *upper));
}

trilean media_feature::check(const media_features& feat, const query_length_environment* lengths) const
{
	const auto matches = [&](float value) { return compare(value, feat, lengths); };
	if (name == "inline-size") return matches(feat.width);
	if (name == "block-size") return matches(feat.height);
	if (name == "prefers-color-scheme")
		return matches(feat.preferred_color_scheme == color_scheme::dark ? _id("dark") : _id("light"));
	if (name == "prefers-reduced-motion")
		return matches(feat.prefers_reduced_motion ? _id("reduce") : _id("no-preference"));
	if (name == "prefers-contrast")
		return matches(feat.prefers_contrast_more ? _id("more") : _id("no-preference"));
	if (name == "hover") return matches(feat.hover ? _id("hover") : _id("none"));
	if (name == "any-hover") return matches(feat.any_hover ? _id("hover") : _id("none"));
	if (name == "pointer") return matches(feat.primary_pointer ? feat.primary_pointer : _id("none"));
	if (name == "any-pointer") {
		if (feat.any_pointer_fine || feat.any_pointer_coarse) {
			trilean result = False;
			if (feat.any_pointer_fine) result = result || matches(_id("fine"));
			if (feat.any_pointer_coarse) result = result || matches(_id("coarse"));
			return result;
		}
		return matches(_id("none"));
	}
	switch (_id(name))
	{
	case _width_:
		return matches(feat.width);
	case _height_:
		return matches(feat.height);
	case _device_width_:
		return matches(feat.device_width);
	case _device_height_:
		return matches(feat.device_height);
	case _orientation_:
		return matches(feat.height >= feat.width ? _portrait_ : _landscape_);
	case _aspect_ratio_:
		// The standard calls 1/0 "a degenerate ratio" https://drafts.csswg.org/css-values-4/#ratio-value,
		// but it doesn't specify exactly how it behaves in this context: https://drafts.csswg.org/mediaqueries-5/#aspect-ratio.
		// Chrome/Firefox work as expected with 1/0, for example when both width and height are nonzero
		// (aspect-ratio < 1/0) evaluates to true. But they behave the same for 0/0, which is unexpected
		// (0/0 is NaN, so any comparisons should evaluate to false).
		// 0/1 is also degenerate according to the standard.
		return feat.height != 0 ? matches(float(feat.width) / feat.height) : False;
	case _device_aspect_ratio_:
		return feat.device_height != 0 ? matches(float(feat.device_width) / feat.device_height) : False;
	case _color_:
		return matches(feat.color);
	case _color_index_:
		return matches(feat.color_index);
	case _monochrome_:
		return matches(feat.monochrome);
	case _resolution_:
		return matches(feat.resolution);
	default:
		assert(0); // must never happen, unknown media features are handled in parse_media_feature
		return False;
	}
}

trilean media_condition::check(const media_features& features, const query_length_environment* lengths) const
{
	if (op == _not_)
		return not m_conditions[0].check(features, lengths);

	if (op == _and_)
	{
		// https://drafts.csswg.org/mediaqueries/#evaluating
		// The result is true if the <media-in-parens> child term and all of the <media-in-parens>
		// children of the <media-and> child terms are true, false if at least one of these
		// <media-in-parens> terms are false, and unknown otherwise.
		trilean result = True;
		for (const auto& condition : m_conditions)
		{
			result = result && condition.check(features, lengths);
			if (result == False) return result; // no need to check further
		}
		return result;
	}
	if (op == _or_)
	{
		// The result is false if the <media-in-parens> child term and all of the <media-in-parens>
		// children of the <media-or> child terms are false, true if at least one of these
		// <media-in-parens> terms are true, and unknown otherwise.
		trilean result = False;
		for (const auto& condition : m_conditions)
		{
			result = result || condition.check(features, lengths);
			if (result == True) return result; // no need to check further
		}
		return result;
	}
	return False;
}

trilean media_in_parens::check(const media_features& features, const query_length_environment* lengths) const
{
	if (is<media_condition>()) return get<media_condition>().check(features, lengths);
	if (is<media_feature>()) return get<media_feature>().check(features, lengths);
	if (is<style_query_feature>() && lengths && lengths->container)
		return (trilean)get<style_query_feature>().check(*lengths->container);
	// <general-enclosed>  https://drafts.csswg.org/mediaqueries/#evaluating
	return Unknown;
}

trilean media_query::check(const media_features& features) const
{
	trilean result;
	// https://drafts.csswg.org/mediaqueries/#media-types
	// User agents must recognize the following media types as valid, but must make them match nothing.
	if (m_media_type >= media_type_first_deprecated)
		result = False;
	else if (m_media_type == media_type_unknown)
		result = False;
	else if (m_media_type == media_type_all)
		result = True;
	else
		result = (trilean)(m_media_type == features.type);

	if (result == True)
	{
		for (const auto& condition : m_conditions)
		{
			result = result && condition.check(features);
			if (result == False) break; // no need to check further
		}
	}

	if (m_not) result = not result;

	return result;
}

// https://drafts.csswg.org/mediaqueries/#mq-list
bool media_query_list::check(const media_features& features) const
{
	if (empty()) return true; // An empty media query list evaluates to true.

	trilean result = False;
	for (const auto& query : m_queries)
	{
		result = result || query.check(features);
		if (result == True) break; // no need to check further
	}

	// https://drafts.csswg.org/mediaqueries/#evaluating
	// If the result ... is used in any context that expects a two-valued boolean, “unknown” must be converted to “false”.
	return result == True;
}

// nested @media rules: https://drafts.csswg.org/css-conditional-3/#processing
// all of them must be true for style rules to apply
bool media_query_list_list::apply_media_features(const media_features& features)
{
	bool apply = true;

	for (const auto& mq_list: m_media_query_lists)
	{
		if (!mq_list.check(features))
		{
			apply = false;
			break;
		}
	}

	bool ret = (apply != m_is_used);
	m_is_used = apply;
	return ret;
}


bool parse_media_query(const css_token_vector& tokens, media_query& mquery, document::ptr doc);

static css_token_vector normalize_media_tokens(const css_token_vector& tokens)
{
	auto keep_whitespace = [](auto left_token, auto right_token)
	{
		return is_one_of(left_token.ch, '<', '>') && right_token.ch == '=';
	};
	return normalize(tokens, f_componentize | f_remove_whitespace, keep_whitespace);
}

// https://drafts.csswg.org/mediaqueries-5/#typedef-media-query-list
media_query_list parse_media_query_list(const css_token_vector& _tokens, document::ptr doc)
{
	const auto tokens = normalize_media_tokens(_tokens);
	// this needs special treatment because empty media query list evaluates to true
	if (tokens.empty()) return {};

	media_query_list result;
	auto list_of_lists = parse_comma_separated_list(tokens);
	for (const auto& list : list_of_lists)
	{
		media_query query;
		parse_media_query(list, query, doc);
		// Note: appending even if media query failed to parse, as per standard.
		result.m_queries.push_back(query);
	}
	return result;
}

media_query_list parse_media_query_list(const string& str, shared_ptr<document> doc)
{
	auto tokens = normalize(str);
	return parse_media_query_list(tokens, doc);
}

enum class condition_grammar { media, container, style };

bool parse_media_condition(const css_token_vector& tokens, int& index, bool or_allowed, media_condition& condition,
	document::ptr doc, condition_grammar grammar = condition_grammar::media);

// <media-query> = <media-condition> | [ not | only ]? <media-type> [ and <media-condition-without-or> ]?
bool parse_media_query(const css_token_vector& tokens, media_query& mquery, document::ptr doc)
{
	if (tokens.empty()) return false;
	int index = 0;
	auto end = [&]() { return index == (int)tokens.size(); };

	media_condition condition;
	if (parse_media_condition(tokens, index, true, condition, doc) && end())
	{
		mquery.m_not = false;
		mquery.m_media_type = media_type_all;
		mquery.m_conditions = {condition};
		return true;
	}

	string ident = tokens[0].ident();
	bool _not = false;
	if (ident == "not") index++, _not = true;
	else if (ident == "only") index++; // ignored  https://drafts.csswg.org/mediaqueries-5/#mq-only

	// <media-type> = <ident> except only, not, and, or, and layer
	ident = at(tokens, index).ident();
	if (is_one_of(ident, "", "only", "not", "and", "or", "layer"))
		return false;
	int idx = value_index(ident, media_type_strings);
	int media_type = idx == -1 ? media_type_unknown : idx + 1;
	index++;

	if (at(tokens, index).ident() == "and")
	{
		index++;
		if (!parse_media_condition(tokens, index, false, condition, doc) || !end())
			return false;
		mquery.m_conditions = {condition};
	}
	if (!end()) return false;
	mquery.m_not = _not;
	mquery.m_media_type = (litehtml::media_type) media_type;
	return true;
}

bool parse_media_in_parens(const css_token& token, media_in_parens& media_in_parens, document::ptr doc, condition_grammar grammar);

// <media-condition>            = <media-not> | <media-in-parens> [ <media-and>* | <media-or>* ]
// <media-condition-without-or> = <media-not> | <media-in-parens> <media-and>*
// <media-not> = not <media-in-parens>
bool parse_media_condition(const css_token_vector& tokens, int& index, bool _or_allowed, media_condition& condition,
	document::ptr doc, condition_grammar grammar)
{
	media_in_parens media_in_parens;
	if (at(tokens, index).ident() == "not")
	{
		if (!parse_media_in_parens(at(tokens, index + 1), media_in_parens, doc, grammar)) return false;
		condition.op = _not_;
		condition.m_conditions = {media_in_parens};
		index += 2;
		return true;
	}

	if (!parse_media_in_parens(at(tokens, index), media_in_parens, doc, grammar)) return false;
	condition.m_conditions = {media_in_parens};
	index++;

	bool or_allowed = _or_allowed;
	bool and_allowed = true;
	while (true)
	{
		string ident = at(tokens, index).ident();
		if (ident == "and" && and_allowed) condition.op = _and_, or_allowed = false;
		else if (ident == "or" && or_allowed) condition.op = _or_, and_allowed = false;
		else return true;
		index++;

		if (!parse_media_in_parens(at(tokens, index), media_in_parens, doc, grammar)) return false;
		condition.m_conditions.push_back(media_in_parens);
		index++;
	}
}

bool parse_media_feature(const css_token& token, media_feature& media_feature, document::ptr doc, bool container);

bool matches_media_condition(const css_token_vector& tokens, const document::ptr& doc, const media_features& features)
{
	const auto normalized = normalize_media_tokens(tokens);
	int index = 0;
	media_condition condition;
	return parse_media_condition(normalized, index, true, condition, doc) &&
		index == static_cast<int>(normalized.size()) && condition.check(features) == True;
}

bool is_container_name(const css_token& token)
{
	return token.type == IDENT && !is_one_of(token.ident(), "none", "and", "not", "or",
		"default", "inherit", "initial", "unset", "revert", "revert-layer");
}

static unsigned required_query_axes(const media_condition& condition)
{
	unsigned axes = 0;
	for (const auto& part : condition.m_conditions)
	{
		if (part.is<media_condition>()) axes |= required_query_axes(part.get<media_condition>());
		else if (part.is<unknown>()) axes |= query_unknown;
		else if (part.is<media_feature>())
		{
			const auto& name = part.get<media_feature>().name;
			if (is_one_of(name, "width", "inline-size")) axes |= query_inline;
			else if (is_one_of(name, "height", "block-size")) axes |= query_block;
			else axes |= query_inline | query_block;
		}
	}
	return axes;
}

std::optional<vector<container_condition>> parse_container_conditions(const css_token_vector& input, const document::ptr& doc)
{
	const auto lists = parse_comma_separated_list(normalize_media_tokens(input));
	vector<container_condition> conditions;
	for (const auto& tokens : lists)
	{
		if (tokens.empty()) return std::nullopt;
		container_condition entry;
		int index = 0;
		if (is_container_name(tokens.front())) entry.name = tokens[index++].name;
		const auto end = static_cast<int>(tokens.size());
		if (index != end)
		{
			entry.condition.emplace();
			if (!parse_media_condition(tokens, index, true, *entry.condition, doc, condition_grammar::container) || index != end)
				return std::nullopt;
			entry.axes = required_query_axes(*entry.condition);
		}
		conditions.push_back(std::move(entry));
	}
	if (conditions.empty()) return std::nullopt;
	return conditions;
}

static std::optional<style_query_feature> parse_style_feature(const css_token_vector& tokens)
{
	if (tokens.empty() || tokens.front().type != IDENT) return {};
	const auto& name = tokens.front().name;
	if (name.size() <= 2 || name.compare(0, 2, "--") != 0) return {};
	if (tokens.size() == 1) return style_query_feature{_id(name), std::nullopt};
	if (tokens[1].ch != ':') return {};
	auto declaration = css_parser(tokens).consume_declaration();
	const auto& value = declaration.value;
	if ((!value.empty() && !is_declaration_value(value)) ||
		std::any_of(value.begin(), value.end(), [](const css_token& token) { return is_one_of(token.ch, '<', '>', '='); }) ||
		inspect_css_variables(value) == css_variable_syntax::invalid) return {};
	return style_query_feature{_id(name), std::move(declaration.value)};
}

// https://drafts.csswg.org/mediaqueries-5/#typedef-media-in-parens
// <media-in-parens> = ( <media-condition> ) | <media-feature> | <general-enclosed>
// <general-enclosed> = [ <function-token> <any-value>? ) ] | [ ( <any-value>? ) ]
bool parse_media_in_parens(const css_token& token, media_in_parens& result, document::ptr doc, condition_grammar grammar)
{
	if (token.type == CV_FUNCTION)
	{
		if (!token.value.empty() && !is_any_value(token.value))
			return false;
		if (grammar == condition_grammar::container && lowcase(token.name) == "style") {
			if (auto feature = parse_style_feature(token.value)) {
				result = std::move(*feature);
				return true;
			}
			int index = 0;
			media_condition condition;
			if (parse_media_condition(token.value, index, true, condition, doc, condition_grammar::style) &&
				index == static_cast<int>(token.value.size())) {
				result = std::move(condition);
				return true;
			}
		}
		result = unknown();
		return true;
	}

	if (token.type != ROUND_BLOCK) return false;
	const css_token_vector& tokens = token.value;

	int index = 0;
	media_condition condition;
	if (parse_media_condition(tokens, index, true, condition, doc, grammar) && index == static_cast<int>(tokens.size())) {
		result = std::move(condition);
		return true;
	}
	if (grammar == condition_grammar::style) {
		if (auto feature = parse_style_feature(tokens)) {
			result = std::move(*feature);
			return true;
		}
	} else {
		media_feature feature;
		if (parse_media_feature(token, feature, doc, grammar == condition_grammar::container)) {
			result = std::move(feature);
			return true;
		}
	}
	if (!tokens.empty() && !is_any_value(tokens)) return false;
	result = unknown();
	return true;
}

bool parse_mf_value(const css_token_vector& tokens, int& index, css_token val[2]);
bool parse_mf_range(const css_token_vector& tokens, media_feature& media_feature, document::ptr doc, bool container);

// https://drafts.csswg.org/mediaqueries/#mq-ranges
// Every media feature defines its “type” as either “range” or “discrete” in its definition table.
// The only significant difference between the two types is that “range” media features can be evaluated in a range context and accept “min-” and “max-” prefixes on their name.

// https://drafts.csswg.org/mediaqueries/#mq-min-max
// “Discrete” type properties do not accept “min-” or “max-” prefixes. Adding such a prefix to a “discrete” type media feature simply results in an unknown feature name.
// For example, (min-grid: 1) is invalid, because grid is a “discrete” media feature, and so doesn’t accept the prefixes. (Even though the grid media feature appears to be numeric, as it accepts the values 0 and 1.)

struct mf_info
{
	string_id			type		= empty_id; // range, discrete
	string_id			value_type	= empty_id; // length, ratio, resolution, integer, keyword
	vector<string_id>	keywords    = {};       // default value is specified here to get rid of gcc warning "missing initializer for member"

	operator bool() { return type != empty_id; }
};

static const std::map<string, mf_info> container_features = {
	{ "width", { _range_, _length_ } },
	{ "height", { _range_, _length_ } },
	{ "inline-size", { _range_, _length_ } },
	{ "block-size", { _range_, _length_ } },
	{ "aspect-ratio", { _range_, _ratio_ } },
	{ "orientation", { _discrete_, _keyword_, { _portrait_, _landscape_ } } },
};

static const std::map<string, mf_info>& supported_media_features()
{
	static const std::map<string, mf_info> features = {
	////////////////////////////////////////////////
	// 4. Viewport/Page Dimensions Media Features
	////////////////////////////////////////////////

	// https://drafts.csswg.org/mediaqueries/#width
	// For continuous media, this is the width of the viewport.
	{"width", {_range_, _length_}},
	{"height", {_range_, _length_}},

	// https://drafts.csswg.org/mediaqueries/#aspect-ratio
	// width/height
	{"aspect-ratio", {_range_, _ratio_}},

	// https://drafts.csswg.org/mediaqueries/#orientation
	{"orientation", {_discrete_, _keyword_, {_portrait_, _landscape_}}},
	{"prefers-color-scheme", {_discrete_, _keyword_, {_id("light"), _id("dark")}}},
	{"prefers-reduced-motion", {_discrete_, _keyword_, {_id("no-preference"), _id("reduce")}}},
	{"prefers-contrast", {_discrete_, _keyword_, {_id("no-preference"), _id("more"), _id("less"), _id("custom")}}},
	{"hover", {_discrete_, _keyword_, {_id("none"), _id("hover")}}},
	{"any-hover", {_discrete_, _keyword_, {_id("none"), _id("hover")}}},
	{"pointer", {_discrete_, _keyword_, {_id("none"), _id("coarse"), _id("fine")}}},
	{"any-pointer", {_discrete_, _keyword_, {_id("none"), _id("coarse"), _id("fine")}}},

	////////////////////////////////////////////////
	// 5. Display Quality Media Features
	////////////////////////////////////////////////

	// https://drafts.csswg.org/mediaqueries/#resolution
	// https://developer.mozilla.org/en-US/docs/Web/CSS/@media/resolution
	{"resolution", {_range_, _resolution_}},

	////////////////////////////////////////////////
	// 6. Color Media Features
	////////////////////////////////////////////////

	// https://drafts.csswg.org/mediaqueries/#color
	{"color", {_range_, _integer_}},

	// https://drafts.csswg.org/mediaqueries/#color-index
	{"color-index", {_range_, _integer_}},

	// https://drafts.csswg.org/mediaqueries/#monochrome
	{"monochrome", {_range_, _integer_}},


	////////////////////////////////////////////////
	// Deprecated Media Features
	////////////////////////////////////////////////

	// https://drafts.csswg.org/mediaqueries/#device-width
	{"device-width", {_range_, _length_}},
	{"device-height", {_range_, _length_}},

	// https://drafts.csswg.org/mediaqueries/#device-aspect-ratio
	{"device-aspect-ratio", {_range_, _ratio_}},
	};
	return features;
}

static std::optional<media_feature::threshold> parse_threshold(const mf_info& info, const css_token val[2], bool defer_variables)
{
	if (defer_variables) {
		css_token_vector tokens{val[0]};
		if (val[1].type != 0) {
			if (info.value_type != _ratio_) return std::nullopt;
			tokens.emplace_back(static_cast<css_token_type>('/'));
			tokens.push_back(val[1]);
		}
		const auto variables = inspect_css_variables(tokens);
		if (variables == css_variable_syntax::valid) return media_feature::threshold(std::move(tokens));
		if (variables == css_variable_syntax::invalid) return std::nullopt;
	}

	switch (info.value_type)
	{
	case _integer_:
		if (val[0].type == NUMBER && val[0].n.number_type == css_number_integer && val[1].type == 0)
			return val[0].n.number;
		break;

	case _length_:
	{
		if (val[1].type != 0) break;
		css_length parsed;
		if (parsed.from_token(val[0], f_length | f_math) && std::isfinite(parsed.val())) return parsed;
		break;
	}

	case _resolution_: // https://drafts.csswg.org/css-values-4/#resolution
		if (val[1].type != 0) break;
		if (const auto resolution = parse_css_resolution(val[0]))
			return static_cast<float>(*resolution * CSS_DPI_PER_DPPX);
		if (val[0].ident() == "infinite") return static_cast<float>(INFINITY);
		break;

	case _ratio_: // https://drafts.csswg.org/css-values-4/#ratio  <ratio> = <number [0,∞]> [ / <number [0,∞]> ]?
	{
		const auto number = [](const css_token& token) -> std::optional<double> {
			if (token.type == NUMBER) return token.n.number;
			return evaluate_css_scalar_math(token, css_math_scalar::number);
		};
		const auto numerator = number(val[0]);
		const auto denominator = val[1].type == 0 ? std::optional(1.0) : number(val[1]);
		if (numerator && denominator && *numerator >= 0 && *denominator >= 0)
			return static_cast<float>(*numerator / *denominator); // A zero denominator can produce an infinite ratio.
		break;
	}

	case _keyword_:
	{
		const auto ident = _id(val[0].ident());
		if (val[1].type == 0 && contains(info.keywords, ident)) return static_cast<float>(ident);
		break;
	}

	default:
		break;
	}
	return std::nullopt;
}

std::optional<float> media_feature::resolve_threshold(const threshold& value, const media_features& features,
	const query_length_environment* lengths) const
{
	if (value.is<float>()) return value.get<float>();
	const auto doc = length_document.lock();
	if (!doc) return std::nullopt;
	const css_length_context context = lengths ? lengths->units : css_length_context{ &length_font, &features };
	const auto& font = lengths ? lengths->font : length_font;
	if (value.is<css_length>()) return doc->to_pixels(value.get<css_length>(), font, 0, context);
	if (!lengths || !lengths->container) return std::nullopt;

	const auto substituted = substitute_query_variables(value.get<css_token_vector>(), *lengths->container);
	if (!substituted) return std::nullopt;
	int index = 0;
	css_token tokens[2];
	if (!parse_mf_value(*substituted, index, tokens) || index != static_cast<int>(substituted->size())) return std::nullopt;
	const auto parsed = parse_threshold(container_features.at(name), tokens, false);
	if (!parsed) return std::nullopt;
	if (parsed->is<css_length>()) return doc->to_pixels(parsed->get<css_length>(), font, 0, context);
	return parsed->get<float>();
}

bool media_feature::verify_and_convert_units(string_id syntax,
	css_token val[2], css_token val2[2], document::ptr doc, bool container)
{
	const auto& features = container ? container_features : supported_media_features();
	const auto convert = [&](const mf_info& info, const css_token tokens[2], threshold& target)
	{
		auto parsed = parse_threshold(info, tokens, container);
		if (!parsed) return false;
		if (!parsed->is<float>())
		{
			if (!doc) return false;
			if (length_document.expired())
			{
				length_document = doc;
				length_font = doc->initial_font_metrics();
			}
		}
		target = std::move(*parsed);
		return true;
	};

	// https://drafts.csswg.org/mediaqueries/#mq-boolean-context
	if (syntax == _boolean_) // (name)
	{
		// Attempting to evaluate a min/max prefixed media feature in a boolean context is invalid and a syntax error.
		auto mf_info = at(features, name);
		if (!mf_info) return false;
		first = mf_info.value_type == _keyword_ ? static_cast<float>(_none_) : 0.0f;
		op = u'≠';
		return true;
	}
	else if (syntax == _plain_) // ({min-,max-,}name: value)
	{
		if (is_one_of(name.substr(0, 4), "min-", "max-"))
		{
			string real_name = name.substr(4);
			auto mf_info = at(features, real_name);
			if (!mf_info || mf_info.type == _discrete_)
				return false;
			if (!convert(mf_info, val, first))
				return false;
			op = name.substr(0, 4) == "min-" ? u'⩾' : u'⩽';
			name = real_name;
			return true;
		}
		else
		{
			auto mf_info = at(features, name);
			if (!mf_info) return false;
			if (!convert(mf_info, val, first))
				return false;
			op = '=';
			return true;
		}
	}
	else // range syntax
	{
		auto mf_info = at(features, name);
		if (!mf_info || mf_info.type == _discrete_)
			return false;
		return convert(mf_info, val, first) && (!val2 || convert(mf_info, val2, second));
	}
}

// <media-feature> = ( [ <mf-plain> | <mf-boolean> | <mf-range> ] )
bool parse_media_feature(const css_token& token, media_feature& result, document::ptr doc, bool container)
{
	if (token.type != ROUND_BLOCK || token.value.empty()) return false;
	const css_token_vector& tokens = token.value;

	if (tokens.size() == 1)
	{
		media_feature mf = {tokens[0].ident()};
		if (!mf.verify_and_convert_units(_boolean_, nullptr, nullptr, doc, container)) return false;
		result = mf;
		return true;
	}

	if (tokens[0].type == IDENT && tokens[1].ch == ':')
	{
		css_token val[2];
		int index = 2;
		if (!parse_mf_value(tokens, index, val) || index != (int)tokens.size())
			return false;

		media_feature mf = {tokens[0].ident()};
		if (!mf.verify_and_convert_units(_plain_, val, nullptr, doc, container)) return false;
		result = mf;
		return true;
	}

	return parse_mf_range(tokens, result, doc, container);
}

// <mf-value> includes math functions that evaluate to a supported feature value.
// <ratio> = <number [0,∞]> [ / <number [0,∞]> ]?
bool parse_mf_value(const css_token_vector& tokens, int& index, css_token val[2])
{
	const css_token& a = at(tokens, index);
	const css_token& b = at(tokens, index + 1);
	const css_token& c = at(tokens, index + 2);

	if (!is_one_of(a.type, NUMBER, DIMENSION, IDENT, CV_FUNCTION)) return false;

	const auto ratio_part = [](const css_token& token) {
		return (token.type == NUMBER && token.n.number >= 0) || token.type == CV_FUNCTION;
	};
	if (ratio_part(a) && b.ch == '/' && ratio_part(c))
	{
		val[0] = a;
		val[1] = c;
		index += 3;
	}
	else
	{
		val[0] = a;
		val[1] = css_token();
		index++;
	}
	return true;
}

short mirror(short op)
{
	if (op ==  '<') return  '>';
	if (op ==  '>') return  '<';
	if (op == u'⩽') return u'⩾';
	if (op == u'⩾') return u'⩽';
	return op;
}

// <mf-range> = <mf-name> <mf-comparison> <mf-value>
//            | <mf-value> <mf-comparison> <mf-name>
//            | <mf-value> <mf-lt> <mf-name> <mf-lt> <mf-value>
//            | <mf-value> <mf-gt> <mf-name> <mf-gt> <mf-value>
// <mf-lt> = '<' '='?
// <mf-gt> = '>' '='?
// <mf-eq> = '='
// <mf-comparison> = <mf-lt> | <mf-gt> | <mf-eq>
bool parse_mf_range(const css_token_vector& tokens, media_feature& result, document::ptr doc, bool container)
{
	if (tokens.size() < 3) return false;

	int index;
	string name;
	auto mf_name = [&]()
	{
		if (at(tokens, index).type != IDENT) return false;
		name = at(tokens, index).ident();
		// special case for (infinite = resolution)
		// resolution is the only range media feature that can accept a keyword
		if (name == "infinite") return false;
		index++;
		return true;
	};
	auto mf_value = [&](css_token _val[2])
	{
		return parse_mf_value(tokens, index, _val);
	};
	auto mf_lt_gt = [&](char lg, short& _op)
	{
		const css_token& tok = at(tokens, index);
		const css_token& tok1 = at(tokens, index + 1);

		if (tok.ch != lg) return false;

		if (tok1.ch == '=')
			index+=2, _op = lg == '<' ? u'⩽' : u'⩾';
		else
			index++,  _op = lg;
		return true;
	};
	auto mf_lt = [&](short& _op) { return mf_lt_gt('<', _op); };
	auto mf_gt = [&](short& _op) { return mf_lt_gt('>', _op); };
	auto mf_comparison = [&](short& _op)
	{
		const css_token& tok = at(tokens, index);

		if (tok.ch == '=')
		{
			index++;
			_op = '=';
			return true;
		}

		return mf_lt(_op) || mf_gt(_op);
	};
	auto start = [&]() { index = 0; return true; };
	auto end = [&]() { return index == (int)tokens.size(); };

	short op;
	css_token val[2];
	// using lambda to avoid warning "assignment within conditional expression"
	auto reverse = [](short& _op) { _op = mirror(_op); return true; };
	if ((start() && mf_name() && mf_comparison(op) && mf_value(val) && end()) ||
		(start() && mf_value(val) && mf_comparison(op) && mf_name() && end() && reverse(op)))
	{
		media_feature mf = {name};
		mf.op = op;
		if (!mf.verify_and_convert_units(_range_, val, nullptr, doc, container)) return false;
		result = mf;
		return true;
	}
	short op2;
	css_token val2[2];
	if ((start() && mf_value(val) && mf_lt(op) && mf_name() && mf_lt(op2) && mf_value(val2) && end()) ||
		(start() && mf_value(val) && mf_gt(op) && mf_name() && mf_gt(op2) && mf_value(val2) && end()))
	{
		media_feature mf = {name};
		mf.op = op;
		mf.op2 = op2;
		if (!mf.verify_and_convert_units(_range_, val, val2, doc, container)) return false;
		result = mf;
		return true;
	}
	return false;
}

} // namespace litehtml
