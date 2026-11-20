// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_value_parser.h"
#include <litehtml/html.h>
#include "css_image_value.h"
#include "css_position_values.h"
#include "css_stylesheet_rules.h"
#include "formats/html/css/css_math.h"

#include <litehtml/css_parser.h>

namespace litehtml {

static bool parse_bg_image(const css_token& token, image& bg_image, document_container* container);
static bool parse_bg_position_size(const css_token_vector& tokens, int& index, css_image_position& position, css_size& size,
	bool accept_three = true);
static bool parse_bg_size(const css_token_vector& tokens, int& index, css_size& size);
static bool parse_border_width(const css_token& token, css_length& width);

namespace {
	std::optional<css_filter_list> parse_filter(const css_token_vector& p_value, document_container* p_container) {
		if (p_value.size() == 1 && p_value.front().ident() == "none") return css_filter_list{};
		if (p_value.empty()) return std::nullopt;
		constexpr std::string_view names[] = {
			"grayscale", "sepia", "saturate", "hue-rotate", "invert", "opacity", "brightness", "contrast", "blur", "drop-shadow",
		};
		using Type = css_filter_type;
		css_filter_list result;
		for (const auto& token : p_value) {
			if (token.type != CV_FUNCTION) return std::nullopt;
			const auto name = lowcase(token.name);
			const auto found = std::find(std::begin(names), std::end(names), name);
			if (found == std::end(names)) return std::nullopt;
			css_filter operation{ static_cast<Type>(found - std::begin(names) + 1) };
			auto args = token.value;
			args.erase(std::remove_if(args.begin(), args.end(), [](const css_token& arg) { return arg.type == WHITESPACE; }), args.end());
			if (operation.type == Type::drop_shadow) {
				std::vector<box_shadow> shadows;
				if (!parse_text_shadows(args, p_container, shadows) || shadows.size() != 1) return std::nullopt;
				operation.x = shadows.front().x;
				operation.y = shadows.front().y;
				operation.blur = shadows.front().blur;
				operation.color = shadows.front().color;
			} else {
				if (args.size() > 1) return std::nullopt;
				if (operation.type == Type::blur) {
					if (!args.empty() && !operation.blur.from_token(args.front(), f_length | f_positive | f_math)) return std::nullopt;
				} else if (operation.type == Type::hue_rotate) {
					const auto angle = args.empty() ? std::optional<double>(0.0) : parse_css_angle(args.front(), true);
					if (!angle) return std::nullopt;
					operation.amount = *angle;
				} else if (!args.empty()) {
					const auto& arg = args.front();
					const bool computed = arg.type == CV_FUNCTION;
					std::optional<double> amount = evaluate_css_scalar_math(arg, css_math_scalar::number_or_percentage);
					if (arg.type == NUMBER || arg.type == PERCENTAGE) amount = arg.n.number / (arg.type == PERCENTAGE ? 100.0 : 1.0);
					if (!amount || !std::isfinite(*amount) || (*amount < 0 && !computed)) return std::nullopt;
					operation.amount = std::max(0.0, *amount);
					if (is_one_of(operation.type, Type::grayscale, Type::sepia, Type::invert, Type::opacity)) {
						operation.amount = std::min(1.0, operation.amount);
					}
				}
			}
			result.push_back(std::move(operation));
		}
		return result;
	}

	std::optional<css_image_orientation> parse_image_orientation(const css_token_vector& p_tokens)
	{
		using orientation = css_image_orientation;
		if (p_tokens.size() == 1) {
			const auto keyword = p_tokens.front().ident();
			if (keyword == "none") return orientation::none;
			if (keyword == "from-image") return orientation::from_image;
		}
		bool flip = false;
		std::optional<double> angle;
		for (const auto& token : p_tokens) {
			if (token.ident() == "flip" && !flip) flip = true;
			else if (!angle) {
				angle = parse_css_angle(token);
				if (!angle) return std::nullopt;
			}
			else return std::nullopt;
		}
		if (!angle && !flip) return std::nullopt;
		constexpr double quarter_turn = 1.57079632679489661923;
		const int turns = static_cast<int>(std::floor(angle.value_or(0.0) / quarter_turn + 0.5));
		constexpr orientation rotations[] = {
			orientation::identity, orientation::rotate_90, orientation::rotate_180, orientation::rotate_270,
		};
		constexpr orientation flipped_rotations[] = {
			orientation::flip_x, orientation::transpose, orientation::flip_y, orientation::transverse,
		};
		const int index = (turns + 4) % 4;
		return flip ? flipped_rotations[index] : rotations[index];
	}

	std::optional<css_image_resolution> parse_image_resolution(const css_token_vector& p_tokens)
	{
		css_image_resolution result;
		for (const auto& token : p_tokens) {
			const auto keyword = token.ident();
			if (keyword == "from-image" && !result.from_image) result.from_image = true;
			else if (keyword == "snap" && !result.snap) result.snap = true;
			else {
				if (result.density) return std::nullopt;
				result.density = parse_css_resolution(token);
				if (!result.density) return std::nullopt;
			}
		}
		return result.density || result.from_image ? std::optional(result) : std::nullopt;
	}

	std::optional<object_fit> parse_object_fit(const css_token_vector& tokens)
	{
		if (tokens.size() == 1) {
			const auto keyword = tokens.front().ident();
			if (keyword == "fill") return object_fit::fill;
			if (keyword == "contain") return object_fit::contain;
			if (keyword == "cover") return object_fit::cover;
			if (keyword == "none") return object_fit::none;
			if (keyword == "scale-down") return object_fit::scale_down;
		}
		if (tokens.size() == 2) {
			auto fit = tokens[0].ident();
			auto modifier = tokens[1].ident();
			if (fit == "scale-down") std::swap(fit, modifier);
			if (modifier == "scale-down") {
				if (fit == "contain") return object_fit::scale_down;
				if (fit == "cover") return object_fit::cover_scale_down;
			}
		}
		return std::nullopt;
	}

}

void css_value_parser::parse_background_and_border(string_id name, const css_token_vector& value,
	const string& baseurl, bool important, document_container* container)
{
	if (name == _id("background-blend-mode")) {
		int_vector modes;
		for (std::size_t i = 0; i < value.size(); ++i) {
			if (i % 2 == 1) {
				if (value[i].type != COMMA) return;
			} else {
				const int mode = value_index(value[i].ident(), css_blend_mode_keywords);
				if (mode < 0) return;
				modes.push_back(mode);
			}
		}
		if (value.size() % 2 == 1) add_parsed_property(name, property_value(modes, important));
		return;
	}
	if (name == _id("filter") || name == _id("backdrop-filter")) {
		if (auto filter = parse_filter(value, container)) add_parsed_property(name, property_value(std::move(*filter), important));
		return;
	}
	if (is_one_of(name, _id("background-position-inline"), _id("background-position-block"))) {
		parse_background_position(name, value, important);
		return;
	}
	if (is_one_of(name, _background_repeat_, _id("background-repeat-x"), _id("background-repeat-y"))) {
		parse_background_repeat(name, value, important);
		return;
	}
	if (is_one_of(name, _id("outline"), _id("outline-width"), _id("outline-style"), _id("outline-color"),
		_id("outline-offset"))) {
		parse_outline(name, value, important, container);
		return;
	}
	if (is_one_of(name, _id("border-image"), _id("border-image-source"), _id("border-image-slice"),
		_id("border-image-width"), _id("border-image-outset"), _id("border-image-repeat"))) {
		parse_border_image(name, value, baseurl, important, container);
		return;
	}
	if (name == _id("object-fit")) {
		if (const auto fit = parse_object_fit(value)) add_parsed_property(name, property_value(*fit, important));
		return;
	}
	if (name == _id("image-orientation")) {
		if (const auto orientation = parse_image_orientation(value)) add_parsed_property(name, property_value(*orientation, important));
		return;
	}
	if (name == _id("image-resolution")) {
		if (const auto resolution = parse_image_resolution(value)) add_parsed_property(name, property_value(*resolution, important));
		return;
	}
	if (name == _id("object-position")) {
		int index = 0;
		css_image_position position;
		if (parse_image_position(value, index, position) && index == static_cast<int>(value.size()))
			add_parsed_property(name, property_value(position, important));
		return;
	}
	if (name == _id("box-shadow") || name == _id("text-shadow")) {
		std::vector<box_shadow> shadows;
		const bool parsed = name == _id("box-shadow") ? parse_box_shadows(value, container, shadows)
			: parse_text_shadows(value, container, shadows);
		if (parsed) {
			add_parsed_property(name, property_value(shadows, important));
		}
		return;
	}
	const css_token val = value.size() == 1 ? value[0] : css_token();
	const auto ident = val.ident();
	if (name == _id("scrollbar-color")) {
		if (ident == "auto") add_parsed_property(name, property_value(initial_value{}, important));
		else if (value.size() == 2) {
			css_scrollbar_colors colors;
			if (parse_color(value[0], colors.thumb, container) && parse_color(value[1], colors.track, container))
				add_parsed_property(name, property_value(std::optional<css_scrollbar_colors>{ colors }, important));
		}
		return;
	}
	if (name == _id("accent-color") || name == _id("caret-color")) {
		if (ident == "auto") add_parsed_property(name, property_value(initial_value{}, important));
		else {
			web_color color;
			if (parse_color(val, color, container)) add_parsed_property(name, property_value(color, important));
		}
		return;
	}
	if (name == _id("opacity")) {
		std::optional<double> opacity;
		if (val.type == NUMBER || val.type == PERCENTAGE) {
			opacity = val.type == PERCENTAGE ? val.n.number * 0.01 : val.n.number;
		} else {
			opacity = evaluate_css_scalar_math(val, css_math_scalar::number_or_percentage);
		}
		if (opacity && !std::isnan(*opacity))
			add_parsed_property(name, property_value(static_cast<float>(std::clamp(*opacity, 0.0, 1.0)), important));
		return;
	}
	web_color clr[4];
	css_length len[4];
	int idx[4];
	switch (name)
	{
	//  =============================  COLOR  =============================

	case _color_:
	case _background_color_:
	case _border_top_color_:
	case _border_bottom_color_:
	case _border_left_color_:
	case _border_right_color_:
		if (parse_color(val, *clr, container))
			add_parsed_property(name, property_value(*clr, important));
		break;

	//  =============================  BACKGROUND  =============================

	case _background_:
		parse_background(value, baseurl, important, container);
		break;

	case _background_image_:
		parse_background_image(value, baseurl, important, container);
		break;

	case _background_position_:
	case _background_position_x_:
	case _background_position_y_:
		parse_background_position(name, value, important);
		break;

	case _background_size_:
		parse_background_size(value, important);
		break;

	case _background_attachment_:
	case _background_origin_:
	case _background_clip_:
		parse_keyword_comma_list(name, value, important);
		break;

	//  =============================  BORDER  =============================

	case _border_:
		parse_border(value, important, container);
		break;

	case _border_left_:
	case _border_right_:
	case _border_top_:
	case _border_bottom_:
		parse_border_side(name, value, important, container);
		break;

	case _border_width_:
		if (int n = parse_1234_values(value, len, parse_border_width))
			add_four_properties(_border_top_width_, len, n, important);
		break;
	case _border_style_:
		if (int n = parse_1234_values(value, idx, parse_keyword, (string)border_style_strings, 0))
			add_four_properties(_border_top_style_, idx, n, important);
		break;
	case _border_color_:
		if (int n = parse_1234_values(value, clr, parse_color, container))
			add_four_properties(_border_top_color_, clr, n, important);
		break;

	case _border_top_width_:
	case _border_bottom_width_:
	case _border_left_width_:
	case _border_right_width_:
		if (parse_border_width(val, *len))
			add_parsed_property(name, property_value(*len, important));
		break;

	// border-bottom-left-radius = <length-percentage [0,∞]>{1,2}  https://developer.mozilla.org/en-US/docs/Web/CSS/border-bottom-left-radius
	case _border_bottom_left_radius_:
	case _border_bottom_right_radius_:
	case _border_top_right_radius_:
	case _border_top_left_radius_:
		if (parse_two_lengths(value, len, f_length_percentage | f_positive | f_math))
		{
			const auto& corners = css_longhands(name);
			add_parsed_property(corners[0], property_value(len[0], important));
			add_parsed_property(corners[1], property_value(len[1], important));
		}
		break;

	case _border_radius_x_:
	case _border_radius_y_:
	{
		string_id top_left = name == _border_radius_x_ ?
			_border_top_left_radius_x_ :
			_border_top_left_radius_y_;

		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_positive))
			add_four_properties(top_left, len, n, important);
		break;
	}

	case _border_radius_:
		parse_border_radius(value, important);
		break;

	// border-spacing = <length>{1,2}  https://developer.mozilla.org/en-US/docs/Web/CSS/border-spacing
	// Lengths may not be negative.  https://drafts.csswg.org/css2/#separated-borders
	case _border_spacing_:
		if (parse_two_lengths(value, len, f_length | f_positive))
		{
			add_parsed_property(__litehtml_border_spacing_x_, property_value(len[0], important));
			add_parsed_property(__litehtml_border_spacing_y_, property_value(len[1], important));
		}
		break;

	//  =============================  LIST  =============================

	case _list_style_type_:
		if (val.type == STRING)
			add_parsed_property(name, property_value(css_list_marker_type{ {}, val.str }, important));
		else if (auto reference = parse_counter_style_reference(val, true))
			add_parsed_property(name, property_value(css_list_marker_type{ std::move(*reference), {} }, important));
		break;

	case _list_style_image_:
		if (val.ident() == "none") {
			add_parsed_property(_list_style_image_, property_value(css_image_ptr{}, important));
			add_parsed_property(_list_style_image_baseurl_, property_value(baseurl, important));
		} else if (auto value = parse_css_image(val, container)) {
			add_parsed_property(_list_style_image_, property_value(css_image_ptr(std::make_shared<image>(std::move(*value))), important));
			add_parsed_property(_list_style_image_baseurl_, property_value(baseurl, important));
		}
		break;

	case _list_style_:
		parse_list_style(value, baseurl, important, container);
		break;
	default:
		break;
	}
}

// https://drafts.csswg.org/css-lists/#list-style-property
// <'list-style-position'> || <'list-style-image'> || <'list-style-type'>
void css_value_parser::parse_list_style(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/list-style#formal_definition
	css_list_marker_type type;
	int position = list_style_position_outside;
	css_image_ptr marker_image;

	bool type_found = false;
	bool position_found = false;
	bool image_found = false;
	int none_count = 0;

	for (const auto& token : tokens)
	{
		// "...none is a valid value for both list-style-image and list-style-type. To resolve this ambiguity,
		// a value of none ... must be applied to whichever of the two properties aren’t otherwise set by the shorthand."
		if (token.ident() == "none") {
			none_count++;
			continue;
		}
		if (!position_found && parse_keyword(token, position, list_style_position_strings)) {
			position_found = true;
			continue;
		}
		if (!image_found) {
			if (auto value = parse_css_image(token, container)) {
				marker_image = std::make_shared<image>(std::move(*value));
				image_found = true;
				continue;
			}
		}
		if (!type_found && token.type == STRING)
		{
			type_found = true;
			type.text = token.str;
		}
		else if (!type_found && !value_in_list(token.ident(), list_style_position_strings))
		{
			auto reference = parse_counter_style_reference(token, false);
			if (!reference) return;
			type_found = true;
			type.counter = std::move(*reference);
		}
		else
			return; // syntax error
	}

	switch (none_count)
	{
	case 0:
		break;
	case 1:
		if (type_found && image_found) return;
		if (!type_found) type.counter.name = "none";
		break;
	case 2:
		if (type_found || image_found) return;
		type.counter.name = "none";
		break;
	default:
		return; // syntax error
	}

	add_parsed_property(_list_style_type_,			property_value(type,	 important));
	add_parsed_property(_list_style_position_,		property_value(position, important));
	add_parsed_property(_list_style_image_,			property_value(std::move(marker_image), important));
	add_parsed_property(_list_style_image_baseurl_,	property_value(baseurl,	 important));
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/border-radius
// border-radius = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?
void css_value_parser::parse_border_radius(const css_token_vector& tokens, bool important)
{
	int i;
	for (i = 0; i < (int)tokens.size() && tokens[i].ch != '/'; i++) {}

	if (i == (int)tokens.size()) // no '/'
	{
		css_length len[4];
		if (int n = parse_1234_lengths(tokens, len, f_length_percentage | f_positive))
		{
			add_four_properties(_border_top_left_radius_x_, len, n, important);
			add_four_properties(_border_top_left_radius_y_, len, n, important);
		}
	}
	else
	{
		auto raduis_x = slice(tokens, 0, i);
		auto raduis_y = slice(tokens, i + 1);

		css_length rx[4], ry[4];
		int n = parse_1234_lengths(raduis_x, rx, f_length_percentage | f_positive);
		int m = parse_1234_lengths(raduis_y, ry, f_length_percentage | f_positive);

		if (n && m)
		{
			add_four_properties(_border_top_left_radius_x_, rx, n, important);
			add_four_properties(_border_top_left_radius_y_, ry, m, important);
		}
	}
}

bool parse_border_width(const css_token& token, css_length& w)
{
	css_length width;
	if (!width.from_token(token, f_length | f_positive | f_math, border_width_strings))
		return false;

	if (width.is_predefined())
		width.set_value(border_width_values[width.predef()], css_units_px);

	w = width;
	return true;
}

namespace {
	bool parse_outline_style(const css_token& token, int& r_style) {
		if (token.ident() == "auto") {
			r_style = outline_style_auto;
			return true;
		}
		return parse_keyword(token, r_style, border_style_strings) && r_style != border_style_hidden;
	}
}

void css_value_parser::parse_outline(string_id name, const css_token_vector& tokens, bool important, document_container* container)
{
	const auto width_id = _id("outline-width");
	const auto style_id = _id("outline-style");
	const auto color_id = _id("outline-color");
	if (name != _id("outline")) {
		if (tokens.size() != 1) return;
		const auto& token = tokens.front();
		css_length length;
		int style;
		web_color color;
		if (name == width_id && parse_border_width(token, length))
			add_parsed_property(name, property_value(length, important));
		else if (name == style_id && parse_outline_style(token, style))
			add_parsed_property(name, property_value(style, important));
		else if (name == color_id) {
			if (token.ident() == "auto")
				add_parsed_property(name, property_value(std::optional<web_color>(), important));
			else if (parse_color(token, color, container))
				add_parsed_property(name, property_value(std::optional<web_color>(color), important));
		}
		else if (name == _id("outline-offset") && length.from_token(token, f_length | f_math))
			add_parsed_property(name, property_value(length, important));
		return;
	}

	css_outline result;
	bool width_found = false;
	bool style_found = false;
	int autos = 0;
	for (const auto& token : tokens) {
		web_color color;
		if (token.ident() == "auto")
			++autos;
		else if (!width_found && parse_border_width(token, result.width))
			width_found = true;
		else if (!style_found && parse_outline_style(token, result.style))
			style_found = true;
		else if (!result.color && parse_color(token, color, container))
			result.color = color;
		else
			return;
	}
	if (autos + static_cast<int>(style_found) + static_cast<int>(result.color.has_value()) > 2) return;
	if (autos > 0 && !style_found) result.style = outline_style_auto;
	add_parsed_property(width_id, property_value(result.width, important));
	add_parsed_property(style_id, property_value(result.style, important));
	add_parsed_property(color_id, property_value(result.color, important));
}

// https://drafts.csswg.org/css-backgrounds/#propdef-border
// <line-width> || <line-style> || <color>
// <line-width> = <length [0,∞]> | thin | medium | thick
// <line-style> = none | hidden | dotted | dashed | solid | double | groove | ridge | inset | outset
bool parse_border_helper(const css_token_vector& tokens, document_container* container,
	css_length& width, border_style& style, web_color& color)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/border#formal_definition
	css_length   _width = border_width_medium_value;
	border_style _style = border_style_none;
	web_color    _color = web_color::current_color;

	bool width_found = false;
	bool style_found = false;
	bool color_found = false;

	for (const auto& token : tokens)
	{
		if (!width_found && parse_border_width(token, _width))
			width_found = true;
		else if (!style_found && parse_keyword(token, _style, border_style_strings))
			style_found = true;
		else if (!color_found && parse_color(token, _color, container))
			color_found = true;
		else
			return false;
	}

	width = _width;
	style = _style;
	color = _color;
	return true;
}

void css_value_parser::parse_border(const css_token_vector& tokens, bool important, document_container* container)
{
	css_length width;
	border_style style;
	web_color color;

	if (!parse_border_helper(tokens, container, width, style, color))
		return;

	for (auto name : {_border_left_width_, _border_right_width_, _border_top_width_, _border_bottom_width_})
		add_parsed_property(name, property_value(width, important));

	for (auto name : {_border_left_style_, _border_right_style_, _border_top_style_, _border_bottom_style_})
		add_parsed_property(name, property_value(style, important));

	for (auto name : {_border_left_color_, _border_right_color_, _border_top_color_, _border_bottom_color_})
		add_parsed_property(name, property_value(color, important));
}

// https://drafts.csswg.org/css-backgrounds/#border-shorthands
// border-top, border-right, border-bottom, border-left
void css_value_parser::parse_border_side(string_id name, const css_token_vector& tokens, bool important, document_container* container)
{
	css_length width;
	border_style style;
	web_color color;

	if (!parse_border_helper(tokens, container, width, style, color))
		return;

	const auto& side = css_longhands(name);
	add_parsed_property(side[0], property_value(width, important));
	add_parsed_property(side[1], property_value(static_cast<int>(style), important));
	add_parsed_property(side[2], property_value(color, important));
}

namespace {
	std::optional<background_repeat> parse_repeat_axis(const css_token& token)
	{
		const auto keyword = token.ident();
		if (keyword == "repeat") return background_repeat::repeat;
		if (keyword == "no-repeat") return background_repeat::no_repeat;
		if (keyword == "round") return background_repeat::round;
		if (keyword == "space") return background_repeat::space;
		return std::nullopt;
	}

	bool parse_repeat_pair(const css_token_vector& tokens, int& index, background_repeat_pair& r_pair)
	{
		using mode = background_repeat;
		const auto first = at(tokens, index).ident();
		// Dockit currently lays out boxes in the horizontal writing mode.
		if (first == "repeat-x" || first == "repeat-inline") {
			r_pair = { mode::repeat, mode::no_repeat };
			++index;
			return true;
		}
		if (first == "repeat-y" || first == "repeat-block") {
			r_pair = { mode::no_repeat, mode::repeat };
			++index;
			return true;
		}
		const auto horizontal = parse_repeat_axis(at(tokens, index));
		if (!horizontal) return false;
		const auto vertical = parse_repeat_axis(at(tokens, ++index));
		if (vertical) ++index;
		r_pair = { *horizontal, vertical.value_or(*horizontal) };
		return true;
	}
}

void css_value_parser::parse_background_repeat(string_id name, const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	vector<background_repeat> horizontal, vertical;
	const bool shorthand = name == _background_repeat_;
	for (const auto& layer : layers) {
		if (shorthand) {
			int index = 0;
			background_repeat_pair repeat;
			if (!parse_repeat_pair(layer, index, repeat) || index != static_cast<int>(layer.size())) return;
			horizontal.push_back(repeat.x);
			vertical.push_back(repeat.y);
		}
		else {
			if (layer.size() != 1) return;
			const auto repeat = parse_repeat_axis(layer.front());
			if (!repeat) return;
			horizontal.push_back(*repeat);
		}
	}
	if (shorthand) {
		add_parsed_property(_id("background-repeat-x"), property_value(horizontal, important));
		add_parsed_property(_id("background-repeat-y"), property_value(vertical, important));
	}
	else add_parsed_property(name, property_value(horizontal, important));
}

namespace {
	int parse_mask_box(const css_token& token, bool clip)
	{
		const auto keyword = token.ident();
		if (keyword == "margin-box") return background_box_margin;
		if (keyword == "no-clip") return clip ? background_box_no_clip : -1;
		if (keyword == "fill-box") return clip ? background_box_content : background_box_border;
		if (keyword == "stroke-box" || keyword == "view-box") return background_box_border;
		return value_index(keyword, background_box_strings);
	}

	struct MaskLayerValues {
		image source;
		css_image_position position{ {}, {} };
		css_size size{ css_length::predef_value(background_size_auto), css_length::predef_value(background_size_auto) };
		background_repeat_pair repeat;
		int origin = background_box_border;
		int clip = background_box_border;
		int mode = static_cast<int>(css_mask_mode::match_source);
		int composite = static_cast<int>(css_mask_composite::add);
	};

	bool parse_mask_layer(const css_token_vector& tokens, document_container* container, MaskLayerValues& layer)
	{
		bool image_found = false, position_found = false, repeat_found = false;
		bool origin_found = false, clip_found = false, mode_found = false, composite_found = false;
		for (int index = 0; index < static_cast<int>(tokens.size());) {
			const auto& token = tokens[index];
			if (!image_found && parse_bg_image(token, layer.source, container)) {
				image_found = true;
			} else if (!position_found && parse_bg_position_size(tokens, index, layer.position, layer.size, false)) {
				position_found = true;
				continue;
			} else if (!repeat_found && parse_repeat_pair(tokens, index, layer.repeat)) {
				repeat_found = true;
				continue;
			} else if (!mode_found && parse_keyword(token, layer.mode, "match-source;alpha;luminance")) {
				mode_found = true;
			} else if (!composite_found && parse_keyword(token, layer.composite, "add;subtract;intersect;exclude")) {
				composite_found = true;
			} else if (const int origin = parse_mask_box(token, false); !origin_found && origin >= 0) {
				origin_found = true;
				layer.origin = origin;
				if (!clip_found) layer.clip = parse_mask_box(token, true);
			} else if (const int clip = parse_mask_box(token, true); !clip_found && clip >= 0) {
				clip_found = true;
				layer.clip = clip;
			} else {
				return false;
			}
			++index;
		}
		return !tokens.empty();
	}

	bool parse_mask_longhand(string_id name, const css_token_vector& tokens, document_container* container, MaskLayerValues& layer)
	{
		int index = 0;
		const int count = static_cast<int>(tokens.size());
		if (name == _id("mask-position")) return parse_image_position(tokens, index, layer.position) && index == count;
		if (name == _id("mask-size")) return parse_bg_size(tokens, index, layer.size) && index == count;
		if (name == _id("mask-repeat")) return parse_repeat_pair(tokens, index, layer.repeat) && index == count;
		if (count != 1) return false;
		const auto& token = tokens.front();
		if (name == _id("mask-image")) return parse_bg_image(token, layer.source, container);
		if (name == _id("mask-mode")) return parse_keyword(token, layer.mode, "match-source;alpha;luminance");
		if (name == _id("mask-composite")) return parse_keyword(token, layer.composite, "add;subtract;intersect;exclude");
		const bool clip = name == _id("mask-clip");
		int& box = clip ? layer.clip : layer.origin;
		box = parse_mask_box(token, clip);
		return box >= 0;
	}
}

void css_value_parser::parse_mask(string_id name, const css_token_vector& tokens, const string& baseurl,
	bool important, document_container* container)
{
	const auto parts = parse_comma_separated_list(tokens);
	if (parts.empty()) return;

	const bool shorthand = name == _id("mask");
	css_mask mask;
	auto& layers = mask.layers;
	for (const auto& part : parts) {
		MaskLayerValues layer;
		if (shorthand ? !parse_mask_layer(part, container, layer) : !parse_mask_longhand(name, part, container, layer)) return;
		layers.m_image.push_back(std::move(layer.source));
		layers.m_position_x.push_back(layer.position.x);
		layers.m_position_y.push_back(layer.position.y);
		layers.m_size.push_back(layer.size);
		layers.m_repeat_x.push_back(layer.repeat.x);
		layers.m_repeat_y.push_back(layer.repeat.y);
		layers.m_origin.push_back(layer.origin);
		layers.m_clip.push_back(layer.clip);
		mask.modes.push_back(layer.mode);
		mask.composites.push_back(layer.composite);
	}

	const auto add = [&](const char* property, auto&& value) {
		const auto id = _id(property);
		if (shorthand || id == name) add_parsed_property(id, property_value(std::move(value), important));
	};
	add("mask-image", layers.m_image);
	if (shorthand || name == _id("mask-image"))
		add_parsed_property(_id("__dockit-mask-image-baseurl"), property_value(baseurl, important));
	if (shorthand || name == _id("mask-position")) {
		add_parsed_property(_id("__dockit-mask-position-x"), property_value(std::move(layers.m_position_x), important));
		add_parsed_property(_id("__dockit-mask-position-y"), property_value(std::move(layers.m_position_y), important));
	}
	if (shorthand || name == _id("mask-repeat")) {
		add_parsed_property(_id("__dockit-mask-repeat-x"), property_value(std::move(layers.m_repeat_x), important));
		add_parsed_property(_id("__dockit-mask-repeat-y"), property_value(std::move(layers.m_repeat_y), important));
	}
	add("mask-size", layers.m_size);
	add("mask-origin", layers.m_origin);
	add("mask-clip", layers.m_clip);
	add("mask-mode", mask.modes);
	add("mask-composite", mask.composites);
}

void css_value_parser::parse_background(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	web_color color;
	std::vector<image> images;
	vector<css_axis_position> x_positions, y_positions;
	size_vector sizes;
	vector<background_repeat> repeats_x, repeats_y;
	int_vector attachments, origins, clips;

	for (size_t i = 0; i < layers.size(); i++)
	{
		background bg;
		if (!parse_bg_layer(layers[i], container, bg, i == layers.size() - 1))
			return;

		color = bg.m_color;
		images.push_back(bg.m_image[0]);
		x_positions.push_back(bg.m_position_x[0]);
		y_positions.push_back(bg.m_position_y[0]);
		sizes.push_back(bg.m_size[0]);
		repeats_x.push_back(bg.m_repeat_x[0]);
		repeats_y.push_back(bg.m_repeat_y[0]);
		attachments.push_back(bg.m_attachment[0]);
		origins.push_back(bg.m_origin[0]);
		clips.push_back(bg.m_clip[0]);
	}

	add_parsed_property(_background_color_,			property_value(color,		important));
	add_parsed_property(_background_image_,			property_value(images,		important));
	add_parsed_property(_background_image_baseurl_, property_value(baseurl,		important));
	add_parsed_property(_background_position_x_,	property_value(x_positions, important));
	add_parsed_property(_background_position_y_,	property_value(y_positions, important));
	add_parsed_property(_background_size_,			property_value(sizes,		important));
	add_parsed_property(_id("background-repeat-x"), property_value(repeats_x, important));
	add_parsed_property(_id("background-repeat-y"), property_value(repeats_y, important));
	add_parsed_property(_background_attachment_,	property_value(attachments, important));
	add_parsed_property(_background_origin_,		property_value(origins,		important));
	add_parsed_property(_background_clip_,			property_value(clips,		important));
	add_parsed_property(_id("background-blend-mode"), property_value(int_vector{}, important));
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-layer
// <bg-layer> = <bg-image> || <bg-position> [ / <bg-size> ]? || <repeat-style> || <attachment> || <visual-box> || <visual-box>
// <final-bg-layer> =  <bg-layer> || <'background-color'>
bool css_value_parser::parse_bg_layer(const css_token_vector& tokens, document_container* container, background& bg, bool final_layer)
{
	bg.m_color = web_color::transparent;
	bg.m_image = {{}};
	css_image_position position{ {}, {} };
	bg.m_size = { css_size(css_length::predef_value(background_size_auto), css_length::predef_value(background_size_auto)) };
	background_repeat_pair repeat;
	bg.m_attachment = { background_attachment_scroll };
	bg.m_origin = { background_box_padding };
	bg.m_clip = { background_box_border };

	bool color_found = false;
	bool image_found = false;
	bool position_found = false;
	bool repeat_found = false;
	bool attachment_found = false;
	bool origin_found = false;
	bool clip_found = false;

	if (tokens.empty()) return false;
	for (int i = 0; i < (int)tokens.size(); i++)
	{
		if (!color_found && final_layer && parse_color(tokens[i], bg.m_color, container))
			color_found = true;
		else if (!image_found && parse_bg_image(tokens[i], bg.m_image[0], container))
			image_found = true;
		else if (!position_found && parse_bg_position_size(tokens, i, position, bg.m_size[0]))
			position_found = true, i--;
		else if (!repeat_found && parse_repeat_pair(tokens, i, repeat))
			repeat_found = true, i--;
		else if (!attachment_found && parse_keyword(tokens[i], bg.m_attachment[0], background_attachment_strings))
			attachment_found = true;
		// If one <visual-box> value is present then it sets both background-origin and background-clip to that value.
		// If two values are present, then the first sets background-origin and the second background-clip.
		else if (!origin_found && parse_keyword(tokens[i], bg.m_origin[0], background_box_strings))
			origin_found = true, bg.m_clip[0] = bg.m_origin[0];
		else if (!clip_found && parse_keyword(tokens[i], bg.m_clip[0], background_box_strings))
			clip_found = true;
		else
			return false;
	}
	bg.m_position_x = { position.x };
	bg.m_position_y = { position.y };
	bg.m_repeat_x = { repeat.x };
	bg.m_repeat_y = { repeat.y };
	return true;
}

// <bg-position> [ / <bg-size> ]?
bool parse_bg_position_size(const css_token_vector& tokens, int& index, css_image_position& position, css_size& size,
	bool accept_three)
{
	int end = index;
	if (!parse_image_position(tokens, end, position, accept_three))
		return false;

	if (at(tokens, end).ch == '/' && !parse_bg_size(tokens, ++end, size))
		return false;

	index = end;
	return true;
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-size
// <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
bool parse_bg_size(const css_token_vector& tokens, int& index, css_size& size)
{
	css_length a, b;

	if (!a.from_token(at(tokens, index), f_length_percentage | f_positive | f_math, background_size_strings))
		return false;

	// cover | contain
	if (a.is_predefined() && a.predef() != background_size_auto)
	{
		size.width = size.height = a;
		index++;
		return true;
	}

	if (b.from_token(at(tokens, index + 1), f_length_percentage | f_positive | f_math, "auto"))
		index += 2;
	else
	{
		b.predef(background_size_auto); // If only one value is given the second is assumed to be auto.
		index++;
	}

	size.width = a;
	size.height = b;
	return true;
}

bool is_one_of_predef(const css_length& x, int idx1, int idx2)
{
	return x.is_predefined() && is_one_of(x.predef(), idx1, idx2);
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-position
// https://www.w3.org/TR/css-values-4/#position
// <bg-position> = [ left | center | right | top | bottom | <length-percentage> ] |
//   [ left | center | right | <length-percentage> ]  [ top | center | bottom | <length-percentage> ]
// Side-relative values (like bottom 10%) are not supported, so only 1 or 2 values are parsed (not 3 or 4).
bool parse_bg_position(const css_token_vector& tokens, int& index, css_length& x, css_length& y,
	bool convert_keywords_to_percents, int length_flags)
{
	enum {
		left   = background_position_left,
		right  = background_position_right,
		top    = background_position_top,
		bottom = background_position_bottom,
		center = background_position_center
	};

	css_length a, b;

	if (!a.from_token(at(tokens, index), length_flags, background_position_strings))
		return false;

	if (!b.from_token(at(tokens, index + 1), length_flags, background_position_strings))
	{
		// If only one value is specified, the second value is assumed to be center.
		b.predef(center);

		// fix wrong order
		if (is_one_of_predef(a, top, bottom))
			swap(a, b);

		index++;
	}
	else // two values
	{
		// try to fix wrong order
		// A pair of keywords can be reordered, while a combination of keyword and length or percentage cannot.
		if ((is_one_of_predef(a, top, bottom) && b.is_predefined()) ||
			(a.is_predefined() && is_one_of_predef(b, left, right)))
			swap(a, b);

		// check for wrong order
		if (is_one_of_predef(a, top, bottom) || is_one_of_predef(b, left, right))
			return false;

		index += 2;
	}

	if (convert_keywords_to_percents)
	{
		if (a.is_predefined())
			a.set_value(background_position_percentages[a.predef()], css_units_percentage);
		if (b.is_predefined())
			b.set_value(background_position_percentages[b.predef()], css_units_percentage);
	}

	x = a;
	y = b;
	return true;
}

void css_value_parser::parse_background_image(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	std::vector<image> images;

	for (const auto& layer : layers)
	{
		image image;
		if (layer.size() != 1)
			return;
		if (!parse_bg_image(layer[0], image, container))
			return;
		images.push_back(image);
	}

	add_parsed_property(_background_image_,			property_value(images, important));
	add_parsed_property(_background_image_baseurl_, property_value(baseurl, important));
}

// <bg-image> = <image> | none
bool parse_bg_image(const css_token& tok, image& bg_image, document_container* container)
{
	if (tok.ident() == "none")
	{
		bg_image.type = image::type_none;
		return true;
	}

	if (auto value = parse_css_image(tok, container))
	{
		bg_image = std::move(*value);
		return true;
	}

	return false;
}

// https://drafts.csswg.org/css-values-4/#urls
bool parse_url(const css_token& tok, string& url)
{
	return dockit::css::parse_url_token(tok, url);
}

void css_value_parser::parse_keyword_comma_list(string_id name, const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	int_vector vec;

	for (const auto& layer : layers)
	{
		int idx;
		if (layer.size() != 1) return;
		if (!parse_keyword(layer[0], idx, css_keywords(name))) return;
		vec.push_back(idx);
	}

	add_parsed_property(name, property_value(vec, important));
}

void css_value_parser::parse_background_position(string_id name, const css_token_vector& tokens, bool important)
{
	const auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	vector<css_axis_position> first_axis, second_axis;
	const bool shorthand = name == _background_position_;
	const bool horizontal = name == _background_position_x_ || name == _id("background-position-inline");
	const bool logical = name == _id("background-position-inline") || name == _id("background-position-block");
	for (const auto& layer : layers) {
		if (shorthand) {
			int index = 0;
			css_image_position position;
			if (!parse_image_position(layer, index, position, true) || index != static_cast<int>(layer.size())) return;
			first_axis.push_back(position.x);
			second_axis.push_back(position.y);
		}
		else {
			const auto position = parse_axis_position(layer, horizontal, logical);
			if (!position) return;
			first_axis.push_back(*position);
		}
	}

	add_parsed_property(shorthand || horizontal ? _background_position_x_ : _background_position_y_,
		property_value(first_axis, important));
	if (shorthand) add_parsed_property(_background_position_y_, property_value(second_axis, important));
}

void css_value_parser::parse_background_size(const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	size_vector sizes;

	for (const auto& layer : layers)
	{
		css_size size;
		int index = 0;
		if (!parse_bg_size(layer, index, size) || index != (int)layer.size())
			return;

		sizes.push_back(size);
	}

	add_parsed_property(_background_size_, property_value(sizes, important));
}

} // namespace litehtml
