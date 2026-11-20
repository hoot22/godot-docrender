// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_value_parser.h"
#include "css_image_value.h"
#include "css_position_values.h"
#include <litehtml/html.h>

#include <litehtml/css_parser.h>

#include <cmath>
#include <limits>

namespace litehtml {

namespace {
	std::optional<style_display> parse_display(const css_token_vector& tokens) {
		if (tokens.size() == 1 && tokens.front().type == IDENT) {
			const auto& name = tokens.front().ident();
			if (name == "flow") return display_block;
			const int index = value_index(name, style_display_strings, -1);
			if (index >= 0 && index != display_inline_text) return static_cast<style_display>(index);
			return std::nullopt;
		}
		if (tokens.size() != 2) return std::nullopt;
		const auto outside = [](const css_token& token) {
			return token.type == IDENT && is_one_of(token.ident(), "block", "inline");
		};
		const bool first_outside = outside(tokens.front());
		if (first_outside == outside(tokens.back())) return std::nullopt;
		const bool inline_level = (first_outside ? tokens.front() : tokens.back()).ident() == "inline";
		const auto& inside = (first_outside ? tokens.back() : tokens.front()).ident();
		if (inside == "flow") return inline_level ? display_inline : display_block;
		if (inside == "flow-root") return inline_level ? display_inline_block : display_flow_root;
		if (inside == "flex") return inline_level ? display_inline_flex : display_flex;
		if (inside == "grid") return inline_level ? display_inline_grid : display_grid;
		if (inside == "table") return inline_level ? display_inline_table : display_table;
		if (inside == "list-item" && !inline_level) return display_list_item;
		return std::nullopt;
	}

	bool is_counter_name(const css_token& token) {
		return token.type == IDENT &&
			!is_one_of(token.ident(), "none", "inherit", "initial", "unset", "revert", "revert-layer", "default");
	}

	std::optional<css_counter_changes> parse_counters(const css_token_vector& tokens, int default_value) {
		css_counter_changes result;
		if (tokens.size() == 1 && tokens.front().ident() == "none") {
			return result;
		}
		for (size_t index = 0; index < tokens.size();) {
			const auto& name = tokens[index++];
			if (!is_counter_name(name)) return std::nullopt;
			int value = default_value;
			if (index < tokens.size() && tokens[index].type == NUMBER) {
				const auto number = parse_counter_integer(tokens[index++]);
				if (!number) return std::nullopt;
				value = *number;
			}
			result.push_back({ _id(name.name), value });
		}
		return result;
	}

	std::optional<css_content_item> parse_content_item(const css_token& token, document_container* container) {
		using kind = css_content_kind;
		if (token.type == STRING) return css_content_item{ kind::text, token.str, {} };
		if (token.type == IDENT) {
			const auto ident = token.ident();
			if (ident == "open-quote") return css_content_item{ kind::open_quote, {}, {} };
			if (ident == "close-quote") return css_content_item{ kind::close_quote, {}, {} };
			if (ident == "no-open-quote") return css_content_item{ kind::no_open_quote, {}, {} };
			if (ident == "no-close-quote") return css_content_item{ kind::no_close_quote, {}, {} };
			return std::nullopt;
		}

		if (auto image = parse_css_image(token, container); image && !image->is_empty())
			return css_content_item{kind::image, {}, {}, {}, std::make_shared<const litehtml::image>(std::move(*image))};
		if (token.type != CV_FUNCTION || token.value.empty()) return std::nullopt;

		const auto function = lowcase(token.name);
		const auto args = parse_comma_separated_list(token.value);
		if (args[0].empty()) return std::nullopt;
		if (function == "attr") {
			const auto& name = args[0];
			if (name.size() > 2 || name[0].type != IDENT ||
				(name.size() == 2 && (name[1].ident() != "raw-string" || !name[1].preceded_by_space))) {
				return std::nullopt;
			}
			if (args.size() > 2 || (args.size() == 2 && !args[1].empty() && (args[1].size() != 1 || args[1][0].type != STRING))) {
				return std::nullopt;
			}
			const auto fallback = args.size() == 2 && !args[1].empty() ? args[1][0].str : string();
			return css_content_item{ kind::attribute, lowcase(name[0].name), fallback };
		}
		if (function != "counter" && function != "counters") return std::nullopt;

		const bool plural = function == "counters";
		const size_t style_index = plural ? 2 : 1;
		if (args[0].size() != 1 || !is_counter_name(args[0][0]) || args.size() > style_index + 1 ||
			(plural && (args.size() < 2 || args[1].size() != 1 || args[1][0].type != STRING))) {
			return std::nullopt;
		}
		css_counter_style_reference style{ "decimal", {} };
		if (args.size() > style_index) {
			const auto& style_arg = args[style_index];
			if (style_arg.size() != 1) return std::nullopt;
			auto parsed = parse_counter_style_reference(style_arg[0], true);
			if (!parsed) return std::nullopt;
			style = std::move(*parsed);
		}
		return css_content_item{ plural ? kind::counters : kind::counter, args[0][0].name, plural ? args[1][0].str : string(), std::move(style) };
	}

	std::optional<css_generated_content> parse_generated_content(const css_token_vector& tokens, const string& baseurl,
		document_container* container) {
		css_generated_content result;
		if (tokens.size() == 1 && tokens[0].ident() == "normal") return result;
		result.normal = false;
		if (tokens.size() == 1 && tokens[0].ident() == "none") return result;
		result.base_url = baseurl;
		bool alternative = false;
		for (const auto& token : tokens) {
			if (token.type == '/') {
				if (alternative || result.items.empty()) return std::nullopt;
				alternative = true;
				continue;
			}
			auto item = parse_content_item(token, container);
			if (!item) return std::nullopt;
			if (alternative) {
				using kind = css_content_kind;
				if (!is_one_of(item->kind, kind::text, kind::attribute, kind::counter, kind::counters))
					return std::nullopt;
				result.alternative.push_back(std::move(*item));
			} else {
				result.items.push_back(std::move(*item));
			}
		}
		if (alternative && result.alternative.empty()) return std::nullopt;
		return result;
	}

	std::optional<css_quotes> parse_quotes(const css_token_vector& tokens) {
		css_quotes result;
		if (tokens.size() == 1) {
			const auto ident = tokens[0].ident();
			if (ident == "auto") return result;
			if (ident == "none") {
				result.kind = css_quotes_kind::none;
				return result;
			}
			if (ident == "match-parent") {
				result.kind = css_quotes_kind::match_parent;
				return result;
			}
		}
		if (tokens.empty() || tokens.size() % 2 != 0) return std::nullopt;
		result.kind = css_quotes_kind::pairs;
		for (const auto& token : tokens) {
			if (token.type != STRING) return std::nullopt;
			result.pairs.push_back(token.str);
		}
		return result;
	}

}

void css_value_parser::parse_layout(string_id name, const css_token_vector& value, const string& baseurl, bool important,
	document_container* container)
{
	static const auto perspective = _id("perspective");
	static const auto perspective_origin = _id("perspective-origin");
	if (name == perspective) {
		if (value.size() == 1)
			add_length_property(name, value.front(), "none", f_length | f_positive | f_math, important);
		return;
	}
	if (name == perspective_origin) {
		css_image_position origin;
		int index = 0;
		if (parse_image_position(value, index, origin, true) && index == static_cast<int>(value.size()))
			add_parsed_property(name, property_value(origin, important));
		return;
	}
	static const auto translate = _id("translate");
	static const auto rotate = _id("rotate");
	static const auto scale = _id("scale");
	if (is_one_of(name, translate, rotate, scale)) {
		using Kind = css_transform_function::kind;
		const auto kind = name == translate ? Kind::translate : name == rotate ? Kind::rotate : Kind::scale;
		css_individual_transform transform;
		if (parse_css_individual_transform(kind, value, transform))
			add_parsed_property(name, property_value(std::move(transform), important));
		return;
	}
	static const auto offset = _id("offset");
	static const auto offset_position = _id("offset-position");
	static const auto offset_path = _id("offset-path");
	static const auto offset_distance = _id("offset-distance");
	static const auto offset_rotate = _id("offset-rotate");
	static const auto offset_anchor = _id("offset-anchor");
	if (name == offset) {
		if (const auto motion = parse_motion_shorthand(value, baseurl)) {
			add_parsed_property(offset_position, property_value(motion->position, important));
			add_parsed_property(offset_path, property_value(motion->path, important));
			add_parsed_property(offset_distance, property_value(motion->distance, important));
			add_parsed_property(offset_rotate, property_value(motion->rotation, important));
			add_parsed_property(offset_anchor, property_value(motion->anchor, important));
		}
		return;
	}
	if (name == offset_path) {
		if (const auto path = parse_offset_path(value, baseurl)) add_parsed_property(name, property_value(*path, important));
		return;
	}
	if (name == offset_distance) {
		css_length distance;
		if (value.size() == 1 && distance.from_token(value.front(), f_length_percentage | f_math))
			add_parsed_property(name, property_value(distance, important));
		return;
	}
	if (name == offset_rotate) {
		if (const auto rotation = parse_offset_rotation(value)) add_parsed_property(name, property_value(*rotation, important));
		return;
	}
	if (is_one_of(name, offset_position, offset_anchor)) {
		if (const auto position = parse_offset_position(value, name == offset_position))
			add_parsed_property(name, property_value(*position, important));
		return;
	}
	if (is_one_of(name, _gap_, _row_gap_, _column_gap_)) {
		parse_gap(name, value, important);
		return;
	}
	static const auto counter_set = _id("counter-set");
	static const auto quotes = _id("quotes");
	if (name == quotes) {
		if (auto parsed = parse_quotes(value))
			add_parsed_property(name, property_value(std::move(*parsed), important));
		return;
	}
	if (is_one_of(name, _counter_increment_, _counter_reset_, counter_set)) {
		if (auto counters = parse_counters(value, name == _counter_increment_ ? 1 : 0))
			add_parsed_property(name, property_value(std::move(*counters), important));
		return;
	}
	const css_token val = value.size() == 1 ? value.front() : css_token();
	switch (name)
	{
	case _display_:
		if (const auto display = parse_display(value))
			add_parsed_property(name, property_value(static_cast<int>(*display), important));
		break;

	case _flex_:
		parse_flex(value, important);
		break;

	case _flex_grow_:   // <number [0,∞]>
	case _flex_shrink_:
		if (val.type == NUMBER && val.n.number >= 0 && std::isfinite(val.n.number))
			add_parsed_property(name, property_value(val.n.number, important));
		break;

	case _flex_basis_:
		add_length_property(name, val, flex_basis_strings, f_length_percentage|f_positive|f_math, important);
		break;

	case _flex_flow_:
		parse_flex_flow(value, important);
		break;

	case _justify_items_:
	case _align_items_:
	case _justify_self_:
	case _align_self_:
	case _justify_content_:
	case _align_content_:
	{
		auto alignment = initial_alignment(name);
		if (parse_alignment(name, value, alignment))
			add_parsed_property(name, property_value(alignment, important));
		break;
	}
	case _place_items_:
	case _place_self_:
	case _place_content_:
	{
		const auto& longhands = css_longhands(name);
		auto align = initial_alignment(longhands[0]);
		auto justify = initial_alignment(longhands[1]);
		if (!parse_place_alignment(longhands[0], longhands[1], value, align, justify)) break;
		add_parsed_property(longhands[0], property_value(align, important));
		add_parsed_property(longhands[1], property_value(justify, important));
		break;
	}

	case _transform_:
	{
		css_transform_list transforms;
		if (parse_css_transform(value, transforms))
			add_parsed_property(name, property_value(std::move(transforms), important));
		break;
	}
	case _transform_origin_:
	{
		css_transform_origin origin;
		if (parse_css_transform_origin(value, origin))
			add_parsed_property(name, property_value(origin, important));
		break;
	}
	case _grid_template_:
	case _grid_:
	{
		grid_definition definition;
		if (!parse_grid_definition(value, name == _grid_, definition)) break;
		add_parsed_property(_grid_template_rows_, property_value(std::move(definition.rows), important));
		add_parsed_property(_grid_template_columns_, property_value(std::move(definition.columns), important));
		add_parsed_property(_grid_template_areas_, property_value(std::move(definition.areas), important));
		if (name == _grid_)
		{
			add_parsed_property(_grid_auto_rows_, property_value(std::move(definition.auto_rows), important));
			add_parsed_property(_grid_auto_columns_, property_value(std::move(definition.auto_columns), important));
			add_parsed_property(_grid_auto_flow_, property_value(definition.flow, important));
		}
		break;
	}
	case _grid_row_:
	case _grid_column_:
	{
		grid_axis_placement placement;
		if (!parse_grid_axis_placement(value, placement)) break;
		const auto& longhands = css_longhands(name);
		add_parsed_property(longhands[0], property_value(placement.start, important));
		add_parsed_property(longhands[1], property_value(placement.end, important));
		break;
	}
	case _grid_area_:
	{
		std::array<grid_line, 4> lines;
		if (!parse_grid_area(value, lines)) break;
		const auto& longhands = css_longhands(name);
		for (std::size_t index = 0; index < lines.size(); ++index)
			add_parsed_property(longhands[index], property_value(std::move(lines[index]), important));
		break;
	}
	case _grid_row_start_:
	case _grid_row_end_:
	case _grid_column_start_:
	case _grid_column_end_:
	{
		grid_line line;
		if (parse_grid_line(value, line))
			add_parsed_property(name, property_value(line, important));
		break;
	}
	case _grid_template_columns_:
	case _grid_template_rows_:
	{
		grid_track_template tracks;
		if (parse_grid_tracks(value, tracks))
			add_parsed_property(name, property_value(std::move(tracks), important));
		break;
	}
	case _grid_template_areas_:
	{
		grid_area_template areas;
		if (parse_grid_template_areas(value, areas))
			add_parsed_property(name, property_value(std::move(areas), important));
		break;
	}
	case _grid_auto_rows_:
	case _grid_auto_columns_:
	{
		grid_track_list tracks;
		if (parse_grid_auto_tracks(value, tracks))
			add_parsed_property(name, property_value(std::move(tracks), important));
		break;
	}
	case _grid_auto_flow_:
	{
		grid_auto_flow flow;
		if (parse_grid_auto_flow(value, flow))
			add_parsed_property(name, property_value(flow, important));
		break;
	}
	case _order_: // <integer>
		if (val.type == NUMBER && val.n.number_type == css_number_integer &&
			static_cast<double>(val.n.number) >= std::numeric_limits<int>::min() &&
			static_cast<double>(val.n.number) <= std::numeric_limits<int>::max())
			add_parsed_property(name, property_value((int)val.n.number, important));
		break;

	//  =============================  COUNTER, CONTENT  =============================

	case _content_:
		if (auto content = parse_generated_content(value, baseurl, container))
			add_parsed_property(name, property_value(std::move(*content), important));
		break;

	//  ==================================  OTHER  ==================================

	case _cursor_:
		add_parsed_property(name, property_value(get_repr(value, 0, -1, true), important));
		break;
	default:
		break;
	}
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/flex
// https://drafts.csswg.org/css-flexbox/#flex-property
// flex = none | [ <'flex-grow'> <'flex-shrink'>? || <'flex-basis'> ]
void css_value_parser::parse_flex(const css_token_vector& tokens, bool important)
{
	auto n = tokens.size();
	if (n > 3) return;

	const auto& a = at(tokens, 0);
	const auto& b = at(tokens, 1);
	const auto& c = at(tokens, 2);

	struct flex
	{
		float m_grow = 1; // flex-grow is set to 1 when omitted
		float m_shrink = 1;
		css_length m_basis = 0;

		bool grow(const css_token& tok)
		{
			if (tok.type != NUMBER || tok.n.number < 0 || !std::isfinite(tok.n.number)) return false;
			m_grow = tok.n.number;
			return true;
		}
		bool shrink(const css_token& tok)
		{
			if (tok.type != NUMBER || tok.n.number < 0 || !std::isfinite(tok.n.number)) return false;
			m_shrink = tok.n.number;
			return true;
		}
		// unitless_zero_allowed enforces the rule "A unitless zero that is not already preceded by two flex factors must be interpreted as a flex factor."
		bool basis(const css_token& tok, bool unitless_zero_allowed = false)
		{
			if (!unitless_zero_allowed and tok.type == NUMBER and tok.n.number == 0)
				return false;
			return m_basis.from_token(tok, f_length_percentage | f_positive | f_math, flex_basis_strings);
		}
	};
	flex flex;

	if (n == 1)
	{
		string_id ident = _id(a.ident());
		if (is_one_of(ident, _auto_, _none_))
		{
			css_length _auto = css_length::predef_value(flex_basis_auto);

			switch (ident)
			{
			case _auto_:    flex = {1, 1, _auto}; break; // can be handled by else
			case _none_:    flex = {0, 0, _auto}; break;
			default:;
			}
		}
		else
		{
			bool ok = flex.grow(a) || flex.basis(a);
			if (!ok) return;
		}
	}
	else if (n == 2)
	{
		// <number> <number>
		// <number> <basis>
		// <basis> <number>
		bool ok =
			(flex.grow(a) && (flex.shrink(b) || flex.basis(b))) ||
			(flex.basis(a) && flex.grow(b));

		if (!ok) return;
	}
	else // n == 3
	{
		// <number> <number> <basis>
		// <basis> <number> <number>
		bool ok =
			(flex.grow(a) && flex.shrink(b) && flex.basis(c, true)) ||
			(flex.basis(a) && flex.grow(b) && flex.shrink(c));

		if (!ok) return;
	}

	add_parsed_property(_flex_grow_,	property_value(flex.m_grow, important));
	add_parsed_property(_flex_shrink_,	property_value(flex.m_shrink, important));
	add_parsed_property(_flex_basis_,	property_value(flex.m_basis, important));
}

// flex-flow = <'flex-direction'> || <'flex-wrap'>
void css_value_parser::parse_flex_flow(const css_token_vector& tokens, bool important)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/flex-flow#formal_definition
	int flex_direction = flex_direction_row;
	int flex_wrap      = flex_wrap_nowrap;

	bool direction_found = false;
	bool wrap_found = false;

	for (const auto& token : tokens)
	{
		if (!direction_found && parse_keyword(token, flex_direction, flex_direction_strings))
			direction_found = true;
		else if (!wrap_found && parse_keyword(token, flex_wrap, flex_wrap_strings))
			wrap_found = true;
		else
			return;
	}

	add_parsed_property(_flex_direction_, property_value(flex_direction, important));
	add_parsed_property(_flex_wrap_,      property_value(flex_wrap,      important));
}

} // namespace litehtml

void litehtml::css_value_parser::parse_gap(string_id name, const css_token_vector& value, bool important) {
	if (value.empty() || value.size() > (name == _gap_ ? 2u : 1u)) {
		return;
	}
	const auto parse = [](const css_token& token, css_length& gap) {
		return gap.from_token(token, f_length_percentage | f_positive | f_math, "normal") &&
			(gap.is_predefined() || std::isfinite(gap.val()));
	};
	css_length row;
	css_length column;
	if (!parse(value.front(), row) || !parse(value.back(), column)) {
		return;
	}
	if (name != _column_gap_) add_parsed_property(_row_gap_, property_value(row, important));
	if (name != _row_gap_) add_parsed_property(_column_gap_, property_value(column, important));
}
