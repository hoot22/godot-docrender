// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_value_parser.h"
#include <litehtml/html.h>
#include "formats/html/css/css_math.h"
#include "css_media_query.h"

#include <litehtml/css_parser.h>

namespace litehtml {

namespace {
	std::optional<int> parse_will_change(const css_token_vector& tokens) {
		if (tokens.size() == 1 && tokens.front().ident() == "auto") return 0;
		const auto features = parse_comma_separated_list(tokens);
		if (features.empty()) return {};
		int hints = 0;
		for (const auto& feature : features) {
			if (feature.size() != 1 || feature.front().type != IDENT) return {};
			const auto name = feature.front().ident();
			if (is_one_of(name, "will-change", "none", "all", "auto", "default",
					"inherit", "initial", "unset", "revert", "revert-layer")) return {};
			// The mask and offset shorthands also hint at mask-image and offset-path.
			if (is_one_of(name, "transform", "transform-style", "perspective", "translate", "rotate", "scale",
					"offset", "offset-path", "offset-position")) hints |= will_change_transform;
			else if (name == "backface-visibility") hints |= will_change_backface;
			else if (name == "contain") hints |= will_change_containment;
			else if (name == "content-visibility") hints |= will_change_content_visibility;
			else if (is_one_of(name, "filter", "backdrop-filter")) hints |= will_change_filter | will_change_backdrop;
			else if (is_one_of(name, "opacity", "clip-path", "mask", "mask-image", "mix-blend-mode"))
				hints |= will_change_backdrop;
			else if (name == "isolation") hints |= will_change_isolation;
			else if (name == "position") hints |= will_change_position;
			else if (name == "z-index") hints |= will_change_z_index;
		}
		return hints;
	}

	std::optional<css_scroll_snap_type> parse_scroll_snap_type(const css_token_vector& tokens) {
		if (tokens.empty() || tokens.size() > 2) return {};
		const auto axis = tokens.front().ident();
		if (axis == "none") return tokens.size() == 1 ? std::optional(css_scroll_snap_type{}) : std::nullopt;
		css_scroll_snap_type type;
		type.x = is_one_of(axis, "x", "inline", "both");
		type.y = is_one_of(axis, "y", "block", "both");
		if (!type.enabled()) return {};
		if (tokens.size() == 2) {
			const auto strictness = tokens.back().ident();
			if (!is_one_of(strictness, "mandatory", "proximity")) return {};
			type.mandatory = strictness == "mandatory";
		}
		return type;
	}

	int parse_overflow_keyword(const css_token& token)
	{
		const auto keyword = token.ident();
		return keyword == "overlay" ? static_cast<int>(overflow_auto) : value_index(keyword, overflow_strings);
	}

	std::optional<string_vector> parse_container_names(const css_token_vector& tokens, size_t end)
	{
		if (end == 0) return std::nullopt;
		if (end == 1 && tokens.front().ident() == "none") return string_vector{};
		string_vector names;
		for (size_t i = 0; i < end; ++i) {
			if (!is_container_name(tokens[i])) return std::nullopt;
			names.push_back(tokens[i].name);
		}
		return names;
	}

	std::optional<css_contain_intrinsic_size> parse_contain_intrinsic_size(const css_token_vector& tokens, size_t& index)
	{
		css_contain_intrinsic_size result;
		if (index != tokens.size() && tokens[index].ident() == "auto") {
			++index;
			result.remember = true;
		}
		if (index == tokens.size()) return std::nullopt;
		const auto& token = tokens[index++];
		if (token.ident() == "none") return result;
		css_length length;
		if (!length.from_token(token, f_length | f_positive | f_math)) return std::nullopt;
		result.length = length;
		return result;
	}

	std::optional<css_clip_margin> parse_clip_margin(const css_token_vector& tokens)
	{
		if (tokens.empty() || tokens.size() > 2) return {};
		css_clip_margin result;
		bool has_box = false;
		bool has_offset = false;
		for (const auto& token : tokens) {
			const int box = value_index(token.ident(), background_box_strings);
			if (box >= 0 && !has_box) {
				result.box = static_cast<background_box>(box);
				has_box = true;
			}
			else if (!has_offset && result.offset.from_token(token, f_length | f_positive | f_math)) {
				has_offset = true;
			}
			else return {};
		}
		return result;
	}

	std::optional<css_aspect_ratio> parse_aspect_ratio(const css_token_vector& tokens)
	{
		std::size_t first = 0;
		std::size_t end = tokens.size();
		const bool leading_auto = end && tokens.front().ident() == "auto";
		const bool trailing_auto = end > 1 && tokens.back().ident() == "auto";
		if (leading_auto) ++first;
		if (trailing_auto) --end;
		if (leading_auto && trailing_auto) return std::nullopt;
		if (first == end) return leading_auto ? std::optional(css_aspect_ratio{}) : std::nullopt;

		const auto count = end - first;
		if (count != 1 && (count != 3 || tokens[first + 1].type != '/')) return std::nullopt;
		const auto component = [](const css_token& token) -> std::optional<double> {
			if (token.type == NUMBER)
				return std::isfinite(token.n.number) && token.n.number >= 0 ? std::optional<double>(token.n.number) : std::nullopt;
			const auto number = evaluate_css_scalar_math(token, css_math_scalar::number);
			if (!number || !std::isfinite(*number)) return std::nullopt;
			return std::max(0.0, *number);
		};
		const auto width = component(tokens[first]);
		const auto height = count == 3 ? component(tokens[first + 2]) : std::optional(1.0);
		if (!width || !height) return std::nullopt;
		const double ratio = *width > 0 && *height > 0 ? *width / *height : 0;
		return css_aspect_ratio{std::isfinite(ratio) ? ratio : 0.0, leading_auto || trailing_auto};
	}

	bool parse_intrinsic_width(const css_token& value, css_length& width)
	{
		const auto keyword = value.ident();
		auto kind = css_intrinsic_size::none;
		if (keyword == "min-content") kind = css_intrinsic_size::min_content;
		else if (keyword == "max-content") kind = css_intrinsic_size::max_content;
		else if (keyword == "fit-content") kind = css_intrinsic_size::fit_content;
		else if (keyword == "stretch") kind = css_intrinsic_size::stretch;
		else if (value.type == CV_FUNCTION) {
			const auto function = lowcase(value.name);
			if (function == "fit-content") kind = css_intrinsic_size::fit_content_limit;
			else if (function == "stretch") kind = css_intrinsic_size::stretch_limit;
			else return false;
			if (value.value.size() != 1 || !width.from_token(value.value.front(), f_length_percentage | f_positive | f_math))
				return false;
		}
		if (kind == css_intrinsic_size::none) return false;
		width.set_intrinsic_size(kind);
		return true;
	}
}

void css_value_parser::parse_box(string_id name, const css_token_vector& value, const string& baseurl, bool important)
{
	if (name == _id("color-scheme")) {
		if (value.empty()) return;
		if (value.size() == 1 && value.front().ident() == "normal") {
			add_parsed_property(name, property_value(0, important));
			return;
		}
		int schemes = 0;
		bool only = false;
		for (size_t i = 0; i < value.size(); ++i) {
			if (value[i].type != IDENT) return;
			const auto ident = value[i].ident();
			if (ident == "only") {
				if (only || value.size() == 1 || (i != 0 && i + 1 != value.size())) return;
				only = true;
			}
			else if (ident == "light") schemes |= scheme_light;
			else if (ident == "dark") schemes |= schemes ? scheme_dark : scheme_dark | scheme_dark_first;
			else if (is_one_of(ident, "normal", "initial", "inherit", "unset", "revert", "revert-layer", "default")) return;
		}
		add_parsed_property(name, property_value(schemes, important));
		return;
	}
	if (name == _id("will-change")) {
		if (const auto hints = parse_will_change(value)) add_parsed_property(name, property_value(*hints, important));
		return;
	}
	if (name == _id("clip-path")) {
		if (const auto path = parse_css_clip_path(value, baseurl))
			add_parsed_property(name, property_value(*path, important));
		return;
	}
	css_length len[4];
	static const auto inset = _id("inset");
	if (name == inset)
	{
		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_math, "auto"))
			add_four_properties(_top_, len, n, important);
		return;
	}
	const css_token val = value.size() == 1 ? value.front() : css_token();
	const auto container_name_id = _id("container-name");
	if (name == container_name_id) {
		if (const auto names = parse_container_names(value, value.size()))
			add_parsed_property(name, property_value(*names, important));
		return;
	}
	if (name == _id("container")) {
		const auto slash = std::find_if(value.begin(), value.end(), [](const css_token& token) { return token.ch == '/'; });
		const auto names = parse_container_names(value, slash - value.begin());
		if (!names) return;
		int type = static_cast<int>(container_type::normal);
		if (slash != value.end()) {
			if (value.end() - slash != 2) return;
			type = value_index(value.back().ident(), "normal;inline-size;size");
			if (type < 0) return;
		}
		add_parsed_property(container_name_id, property_value(*names, important));
		add_parsed_property(_id("container-type"), property_value(type, important));
		return;
	}
	if (name == _id("contain")) {
		int flags = 0;
		if (val.ident() == "strict") {
			flags = contain_size | contain_layout | contain_paint | contain_style;
		} else if (val.ident() == "content") {
			flags = contain_layout | contain_paint | contain_style;
		} else if (val.ident() != "none") {
			for (const auto& token : value) {
				const auto keyword = token.ident();
				int flag;
				if (keyword == "layout") flag = contain_layout;
				else if (keyword == "paint") flag = contain_paint;
				else if (keyword == "style") flag = contain_style;
				else if (keyword == "size") flag = contain_size;
				else if (keyword == "inline-size") flag = contain_inline_size;
				else return;
				if (flags & flag) return;
				flags |= flag;
			}
			if ((flags & contain_size) && (flags & contain_inline_size)) return;
		}
		add_parsed_property(name, property_value(flags, important));
		return;
	}
	const auto intrinsic_width = _id("contain-intrinsic-width");
	const auto intrinsic_height = _id("contain-intrinsic-height");
	if (is_one_of(name, intrinsic_width, intrinsic_height, _id("contain-intrinsic-size"))) {
		size_t index = 0;
		const auto first = parse_contain_intrinsic_size(value, index);
		if (!first) return;
		if (name == intrinsic_width || name == intrinsic_height) {
			if (index == value.size()) add_parsed_property(name, property_value(*first, important));
		} else {
			const auto second = index == value.size() ? first : parse_contain_intrinsic_size(value, index);
			if (!second || index != value.size()) return;
			add_parsed_property(intrinsic_width, property_value(*first, important));
			add_parsed_property(intrinsic_height, property_value(*second, important));
		}
		return;
	}
	if (name == _id("scrollbar-gutter")) {
		std::optional<scrollbar_gutter> gutter;
		if (val.ident() == "auto") gutter = scrollbar_gutter::automatic;
		else if (val.ident() == "stable") gutter = scrollbar_gutter::stable;
		else if (value.size() == 2 &&
			((value[0].ident() == "stable" && value[1].ident() == "both-edges") ||
				(value[0].ident() == "both-edges" && value[1].ident() == "stable"))) {
			gutter = scrollbar_gutter::both_edges;
		}
		if (gutter) add_parsed_property(name, property_value(static_cast<int>(*gutter), important));
		return;
	}
	if (name == _id("scroll-snap-type")) {
		if (const auto type = parse_scroll_snap_type(value)) add_parsed_property(name, property_value(*type, important));
		return;
	}
	if (name == _id("scroll-snap-align")) {
		if (value.empty() || value.size() > 2) return;
		constexpr const char* alignments = "none;start;end;center";
		const int block = value_index(value.front().ident(), alignments);
		const int inline_axis = value_index(value.back().ident(), alignments);
		if (block >= 0 && inline_axis >= 0) {
			add_parsed_property(name, property_value(css_scroll_snap_align{
				static_cast<scroll_snap_align>(block), static_cast<scroll_snap_align>(inline_axis)}, important));
		}
		return;
	}
	if (name == _id("overscroll-behavior")) {
		if (value.empty() || value.size() > 2) return;
		const int x = value_index(value.front().ident(), overscroll_behavior_keywords);
		const int y = value_index(value.back().ident(), overscroll_behavior_keywords);
		if (x >= 0 && y >= 0) {
			add_parsed_property(_id("overscroll-behavior-x"), property_value(x, important));
			add_parsed_property(_id("overscroll-behavior-y"), property_value(y, important));
		}
		return;
	}
	if (name == _id("overflow-x") || name == _id("overflow-y")) {
		if (const int mode = parse_overflow_keyword(val); mode >= 0)
			add_parsed_property(name, property_value(mode, important));
		return;
	}
	if (name == _id("overflow-clip-margin")) {
		if (const auto margin = parse_clip_margin(value))
			add_parsed_property(name, property_value(*margin, important));
		return;
	}
	if (is_one_of(name, _width_, _min_width_, _max_width_)) {
		css_length width;
		if (parse_intrinsic_width(val, width)) {
			add_parsed_property(name, property_value(width, important));
			return;
		}
	}
	switch (name)
	{
	case _overflow_:
		if (!value.empty() && value.size() <= 2) {
			const int x = parse_overflow_keyword(value.front());
			const int y = parse_overflow_keyword(value.back());
			if (x >= 0 && y >= 0) {
				add_parsed_property(_id("overflow-x"), property_value(x, important));
				add_parsed_property(_id("overflow-y"), property_value(y, important));
			}
		}
		return;

	case _aspect_ratio_:
		if (const auto ratio = parse_aspect_ratio(value)) add_parsed_property(name, property_value(*ratio, important));
		return;

	case _z_index_:
		return add_length_property(name, val, "auto", f_integer, important);

	// <length-percentage [0,∞]>  https://developer.mozilla.org/en-US/docs/Web/CSS/padding-left
	case _padding_left_:
	case _padding_right_:
	case _padding_top_:
	case _padding_bottom_:
		return add_length_property(name, val, "", f_length_percentage|f_positive|f_math, important);

	// auto | <length-percentage>  https://developer.mozilla.org/en-US/docs/Web/CSS/left
	case _left_:
	case _right_:
	case _top_:
	case _bottom_:
	case _margin_left_:
	case _margin_right_:
	case _margin_top_:
	case _margin_bottom_:
		return add_length_property(name, val, "auto", f_length_percentage|f_math, important);

	// auto | <length-percentage [0,∞]>
	case _width_:
	case _height_:
	case _min_width_:
	case _min_height_:
		return add_length_property(name, val, "auto", f_length_percentage|f_positive|f_math, important);

	// none | <length-percentage [0,∞]>
	case _max_width_:
	case _max_height_:
		return add_length_property(name, val, "none", f_length_percentage|f_positive|f_math, important);

	//  normal | <number [0,∞]> | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/line-height#formal_syntax
	case _line_height_:
		return add_length_property(name, val, "normal", f_number|f_length_percentage|f_positive|f_math, important);

	// font-size = <absolute-size> | <relative-size> | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/font-size#formal_syntax
	case _font_size_:
		return add_length_property(name, val, font_size_strings, f_length_percentage|f_positive|f_math, important);

	case _margin_:
		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_math, "auto"))
			add_four_properties(_margin_top_, len, n, important);
		break;

	case _padding_:
		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_positive | f_math))
			add_four_properties(_padding_top_, len, n, important);
		break;
	default:
		break;
	}
}

void css_value_parser::add_length_property(string_id name, css_token val, string keywords, int options, bool important)
{
	css_length length;
	if (length.from_token(val, options, keywords))
		add_parsed_property(name, property_value(length, important));
}

bool parse_length(const css_token& tok, css_length& length, int options, string keywords)
{
	return length.from_token(tok, options, keywords);
}

// parses 1 or 2 lengths, but always returns 2 lengths
bool parse_two_lengths(const css_token_vector& tokens, css_length len[2], int options)
{
	auto n = tokens.size();
	if (n != 1 && n != 2) return false;

	css_length a, b;
	if (!a.from_token(tokens[0], options)) return false;
	if (n == 1) b = a;

	if (n == 2 && !b.from_token(tokens[1], options)) return false;

	len[0] = a;
	len[1] = b;
	return true;
}

int parse_1234_lengths(const css_token_vector& tokens, css_length len[4], int options, string keywords)
{
	return parse_1234_values(tokens, len, parse_length, options, keywords);
}

} // namespace litehtml
