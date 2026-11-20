// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_declarations.h"
#include "css_variables.h"
#include "css_value_parser.h"

#include <litehtml/css_parser.h>
#include <litehtml/html.h>

#include <algorithm>
#include <unordered_map>

namespace litehtml {

namespace {
	constexpr std::array<const char*, 4> physical_sides{"top", "right", "bottom", "left"};
	constexpr std::array<const char*, 4> logical_sides{"block-start", "inline-end", "block-end", "inline-start"};
	constexpr std::array<const char*, 4> physical_corners{"top-left", "top-right", "bottom-right", "bottom-left"};
	constexpr std::array<const char*, 4> logical_corners{"start-start", "start-end", "end-end", "end-start"};

	css_flow_edges make_edges(const string& prefix, const string& suffix = {}, bool corners = false) {
		css_flow_edges result{{}, {}, corners};
		for (size_t side = 0; side < result.physical.size(); ++side) {
			result.physical[side] = _id(prefix + (corners ? physical_corners : physical_sides)[side] + suffix);
			result.logical[side] = _id(prefix + (corners ? logical_corners : logical_sides)[side] + suffix);
		}
		return result;
	}

	const auto& edge_groups() {
		static const std::array<css_flow_edges, static_cast<size_t>(css_edge_group::count)> groups{
			make_edges("margin-"), make_edges("padding-"),
			{{_top_, _right_, _bottom_, _left_}, make_edges("inset-").logical},
			make_edges("scroll-padding-"), make_edges("scroll-margin-"),
			make_edges("border-", "-width"), make_edges("border-", "-style"), make_edges("border-", "-color"),
			make_edges("border-", "-radius-x", true), make_edges("border-", "-radius-y", true),
		};
		return groups;
	}
}

const css_flow_edges& css_edge_properties(css_edge_group group) {
	return edge_groups()[static_cast<size_t>(group)];
}

string_id css_map_logical_property(string_id name, bool rtl) {
	static const auto lookup = [] {
		std::unordered_map<string_id, std::array<string_id, 2>> result;
		for (const auto& group : edge_groups()) {
			for (size_t side = 0; side < group.logical.size(); ++side) {
				result.emplace(group.logical[side], std::array{group.physical[side], group.physical[group.map_direction(side, true)]});
			}
		}
		return result;
	}();
	const auto found = lookup.find(name);
	return found == lookup.end() ? empty_id : found->second[rtl];
}

namespace {
	enum class property_family { wide_only, keyword, box, background, mask, text, layout, transition, animation, scroll_padding, scroll_margin, logical, logical_pair };

	struct property_definition {
		property_family family;
		vector<string_id> longhands{};
		const char* keywords = nullptr;
		string_id syntax = empty_id;
	};

	const property_definition* find_property(string_id name) {
		static const std::map<string_id, property_definition> properties = {
			{ _id("all"), { property_family::wide_only } },
			{ _z_index_, { property_family::box } },
			{ _text_indent_, { property_family::text } },
			{ _id("tab-size"), { property_family::text } },
			{ _padding_left_, { property_family::box } },
			{ _padding_right_, { property_family::box } },
			{ _padding_top_, { property_family::box } },
			{ _padding_bottom_, { property_family::box } },
			{ _left_, { property_family::box } },
			{ _right_, { property_family::box } },
			{ _top_, { property_family::box } },
			{ _bottom_, { property_family::box } },
			{ _id("inset"), { property_family::box, { _top_, _right_, _bottom_, _left_ } } },
			{ _margin_left_, { property_family::box } },
			{ _margin_right_, { property_family::box } },
			{ _margin_top_, { property_family::box } },
			{ _margin_bottom_, { property_family::box } },
			{ _width_, { property_family::box } },
			{ _aspect_ratio_, { property_family::box } },
			{ _height_, { property_family::box } },
			{ _min_width_, { property_family::box } },
			{ _min_height_, { property_family::box } },
			{ _max_width_, { property_family::box } },
			{ _max_height_, { property_family::box } },
			{ _line_height_, { property_family::box } },
			{ _font_size_, { property_family::box } },
			{ _margin_, { property_family::box, { _margin_top_, _margin_right_, _margin_bottom_, _margin_left_ } } },
			{ _padding_, { property_family::box, { _padding_top_, _padding_right_, _padding_bottom_, _padding_left_ } } },
			{ _color_, { property_family::background } },
			{ _id("accent-color"), { property_family::background } },
			{ _id("caret-color"), { property_family::background } },
			{ _id("table-layout"), { property_family::keyword, {}, "auto;fixed" } },
			{ _id("empty-cells"), { property_family::keyword, {}, "show;hide" } },
			{ _id("opacity"), { property_family::background } },
			{ _id("transition"), { property_family::transition, { _id("transition-property"), _id("transition-duration"),
				_id("transition-delay"), _id("transition-timing-function"), _id("transition-behavior") } } },
			{ _id("transition-property"), { property_family::transition } },
			{ _id("transition-duration"), { property_family::transition } },
			{ _id("transition-delay"), { property_family::transition } },
			{ _id("transition-timing-function"), { property_family::transition } },
			{ _id("transition-behavior"), { property_family::transition } },
			{ _id("animation"), { property_family::animation, { _id("animation-name"), _id("animation-duration"),
				_id("animation-delay"), _id("animation-timing-function"), _id("animation-iteration-count"),
				_id("animation-direction"), _id("animation-fill-mode"), _id("animation-play-state") } } },
			{ _id("animation-name"), { property_family::animation } },
			{ _id("animation-duration"), { property_family::animation } },
			{ _id("animation-delay"), { property_family::animation } },
			{ _id("animation-timing-function"), { property_family::animation } },
			{ _id("animation-iteration-count"), { property_family::animation } },
			{ _id("animation-direction"), { property_family::animation } },
			{ _id("animation-fill-mode"), { property_family::animation } },
			{ _id("animation-play-state"), { property_family::animation } },
			{ _id("filter"), { property_family::background } },
			{ _id("backdrop-filter"), { property_family::background } },
			{ _id("will-change"), { property_family::box } },
			{ _id("mask"), { property_family::mask, {
				_id("mask-image"), _id("__dockit-mask-image-baseurl"),
				_id("__dockit-mask-position-x"), _id("__dockit-mask-position-y"),
				_id("mask-size"), _id("__dockit-mask-repeat-x"), _id("__dockit-mask-repeat-y"),
				_id("mask-origin"), _id("mask-clip"), _id("mask-mode"), _id("mask-composite") } } },
			{ _id("mask-image"), { property_family::mask, { _id("mask-image"), _id("__dockit-mask-image-baseurl") } } },
			{ _id("mask-position"), { property_family::mask, {
				_id("__dockit-mask-position-x"), _id("__dockit-mask-position-y") } } },
			{ _id("mask-repeat"), { property_family::mask, {
				_id("__dockit-mask-repeat-x"), _id("__dockit-mask-repeat-y") } } },
			{ _id("mask-size"), { property_family::mask } },
			{ _id("mask-origin"), { property_family::mask } },
			{ _id("mask-clip"), { property_family::mask } },
			{ _id("mask-mode"), { property_family::mask } },
			{ _id("mask-composite"), { property_family::mask } },
			{ _id("mask-type"), { property_family::keyword, {}, "luminance;alpha" } },
			{ _id("color-interpolation"), { property_family::keyword, {}, "srgb;linearrgb;auto" } },
			{ _background_color_, { property_family::background } },
			{ _border_top_color_, { property_family::background } },
			{ _border_bottom_color_, { property_family::background } },
			{ _border_left_color_, { property_family::background } },
			{ _border_right_color_, { property_family::background } },
			{ _background_, { property_family::background, {
				_background_color_, _background_position_x_, _background_position_y_,
				_id("background-repeat-x"), _id("background-repeat-y"),
				_background_attachment_, _background_image_, _background_image_baseurl_, _background_size_,
				_background_origin_, _background_clip_, _id("background-blend-mode") } } },
			{ _id("background-blend-mode"), { property_family::background } },
			{ _id("mix-blend-mode"), { property_family::keyword, {}, css_mix_blend_mode_keywords } },
			{ _id("isolation"), { property_family::keyword, {}, "auto;isolate" } },
			{ _id("object-fit"), { property_family::background } },
			{ _id("object-position"), { property_family::background } },
			{ _id("image-rendering"), { property_family::keyword, {}, image_rendering_keywords } },
			{ _id("image-orientation"), { property_family::background } },
			{ _id("image-resolution"), { property_family::background } },
			{ _id("direction"), { property_family::keyword, {}, direction_keywords } },
			{ _id("unicode-bidi"), { property_family::keyword, {}, "normal;embed;bidi-override;isolate;isolate-override;plaintext" } },
			{ _background_image_, { property_family::background, { _background_image_, _background_image_baseurl_ } } },
			{ _background_position_, { property_family::background, { _background_position_x_, _background_position_y_ } } },
			{ _background_position_x_, { property_family::background } },
			{ _background_position_y_, { property_family::background } },
			{ _id("background-position-inline"), { property_family::background, { _background_position_x_ } } },
			{ _id("background-position-block"), { property_family::background, { _background_position_y_ } } },
			{ _background_size_, { property_family::background } },
			{ _background_repeat_, { property_family::background, {
				_id("background-repeat-x"), _id("background-repeat-y") } } },
			{ _id("background-repeat-x"), { property_family::background } },
			{ _id("background-repeat-y"), { property_family::background } },
			{ _background_attachment_, { property_family::background, {}, background_attachment_strings } },
			{ _background_origin_, { property_family::background, {}, background_box_strings } },
			{ _background_clip_, { property_family::background, {}, background_box_strings } },
			{ _id("outline"), { property_family::background, {
				_id("outline-width"), _id("outline-style"), _id("outline-color") } } },
			{ _id("outline-width"), { property_family::background } },
			{ _id("outline-style"), { property_family::background } },
			{ _id("outline-color"), { property_family::background } },
			{ _id("outline-offset"), { property_family::background } },
			{ _id("border-image-source"), { property_family::background } },
			{ _id("border-image-slice"), { property_family::background } },
			{ _id("border-image-width"), { property_family::background } },
			{ _id("border-image-outset"), { property_family::background } },
			{ _id("border-image-repeat"), { property_family::background } },
			{ _id("border-image"), { property_family::background, {
				_id("border-image-source"), _id("border-image-slice"), _id("border-image-width"), _id("border-image-outset"), _id("border-image-repeat") } } },
			{ _border_, { property_family::background, {
				_border_top_width_, _border_right_width_, _border_bottom_width_, _border_left_width_,
				_border_top_style_, _border_right_style_, _border_bottom_style_, _border_left_style_,
				_border_top_color_, _border_right_color_, _border_bottom_color_, _border_left_color_,
				_id("border-image-source"), _id("border-image-slice"), _id("border-image-width"), _id("border-image-outset"), _id("border-image-repeat") } } },
			{ _border_left_, { property_family::background, { _border_left_width_, _border_left_style_, _border_left_color_ } } },
			{ _border_right_, { property_family::background, { _border_right_width_, _border_right_style_, _border_right_color_ } } },
			{ _border_top_, { property_family::background, { _border_top_width_, _border_top_style_, _border_top_color_ } } },
			{ _border_bottom_, { property_family::background, { _border_bottom_width_, _border_bottom_style_, _border_bottom_color_ } } },
			{ _border_width_, { property_family::background, { _border_top_width_, _border_right_width_, _border_bottom_width_, _border_left_width_ } } },
			{ _border_style_, { property_family::background, { _border_top_style_, _border_right_style_, _border_bottom_style_, _border_left_style_ } } },
			{ _border_color_, { property_family::background, { _border_top_color_, _border_right_color_, _border_bottom_color_, _border_left_color_ } } },
			{ _border_top_width_, { property_family::background } },
			{ _border_bottom_width_, { property_family::background } },
			{ _border_left_width_, { property_family::background } },
			{ _border_right_width_, { property_family::background } },
			{ _border_bottom_left_radius_, { property_family::background, { _border_bottom_left_radius_x_, _border_bottom_left_radius_y_ } } },
			{ _border_bottom_right_radius_, { property_family::background, { _border_bottom_right_radius_x_, _border_bottom_right_radius_y_ } } },
			{ _border_top_right_radius_, { property_family::background, { _border_top_right_radius_x_, _border_top_right_radius_y_ } } },
			{ _border_top_left_radius_, { property_family::background, { _border_top_left_radius_x_, _border_top_left_radius_y_ } } },
			{ _border_radius_x_, { property_family::background, { _border_top_left_radius_x_, _border_top_right_radius_x_, _border_bottom_right_radius_x_, _border_bottom_left_radius_x_ } } },
			{ _border_radius_y_, { property_family::background, { _border_top_left_radius_y_, _border_top_right_radius_y_, _border_bottom_right_radius_y_, _border_bottom_left_radius_y_ } } },
			{ _border_radius_, { property_family::background, {
				_border_top_left_radius_x_, _border_top_left_radius_y_, _border_top_right_radius_x_, _border_top_right_radius_y_,
				_border_bottom_left_radius_x_, _border_bottom_left_radius_y_, _border_bottom_right_radius_x_, _border_bottom_right_radius_y_ } } },
			{ _border_spacing_, { property_family::background, { __litehtml_border_spacing_x_, __litehtml_border_spacing_y_ } } },
			{ _list_style_image_, { property_family::background, { _list_style_image_, _list_style_image_baseurl_ } } },
			{ _list_style_, { property_family::background, { _list_style_image_, _list_style_image_baseurl_, _list_style_position_, _list_style_type_ } } },
			{ _id("box-shadow"), { property_family::background } },
			{ _id("text-shadow"), { property_family::background } },
			{ _font_, { property_family::text, { _font_style_, _font_weight_, _id("font-width"), _font_size_, _line_height_,
				_id("font-variant-ligatures"), _id("font-variant-numeric"), _id("font-variant-east-asian"), _id("font-variant-caps"),
				_id("font-variant-alternates"),
				_font_family_, _id("font-feature-settings"), _id("font-variation-settings"), _id("font-kerning"),
				_id("font-optical-sizing"), _id("font-size-adjust") } } },
			{ _font_variant_, { property_family::text, { _id("font-variant-ligatures"), _id("font-variant-numeric"),
				_id("font-variant-east-asian"), _id("font-variant-caps"), _id("font-variant-alternates") } } },
			{ _id("font-variant-ligatures"), { property_family::text } },
			{ _id("font-variant-numeric"), { property_family::text } },
			{ _id("font-variant-east-asian"), { property_family::text } },
			{ _id("font-variant-caps"), { property_family::text } },
			{ _id("font-variant-alternates"), { property_family::text } },
			{ _font_family_, { property_family::text } },
			{ _font_weight_, { property_family::text } },
			{ _id("font-width"), { property_family::text } },
			{ _id("font-feature-settings"), { property_family::text } },
			{ _id("font-variation-settings"), { property_family::text } },
			{ _id("font-kerning"), { property_family::keyword, {}, "auto;normal;none" } },
			{ _id("font-optical-sizing"), { property_family::keyword, {}, "auto;none" } },
			{ _id("font-size-adjust"), { property_family::text } },
			{ _id("letter-spacing"), { property_family::text } },
			{ _id("word-spacing"), { property_family::text } },
			{ _text_decoration_, { property_family::text, { _text_decoration_color_, _text_decoration_line_, _text_decoration_style_, _text_decoration_thickness_ } } },
			{ _text_decoration_thickness_, { property_family::text } },
			{ _text_decoration_color_, { property_family::text } },
			{ _text_decoration_line_, { property_family::text } },
			{ _id("text-underline-position"), { property_family::text } },
			{ _id("text-underline-offset"), { property_family::text } },
			{ _text_emphasis_, { property_family::text, { _text_emphasis_style_, _text_emphasis_color_ } } },
			{ _text_emphasis_style_, { property_family::text } },
			{ _text_emphasis_color_, { property_family::text } },
			{ _text_emphasis_position_, { property_family::text } },
			{ _gap_, { property_family::layout, { _row_gap_, _column_gap_ } } },
			{ _row_gap_, { property_family::layout } },
			{ _column_gap_, { property_family::layout } },
			{ _flex_, { property_family::layout, { _flex_grow_, _flex_shrink_, _flex_basis_ } } },
			{ _flex_grow_, { property_family::layout } },
			{ _flex_shrink_, { property_family::layout } },
			{ _flex_basis_, { property_family::layout } },
			{ _flex_flow_, { property_family::layout, { _flex_direction_, _flex_wrap_ } } },
			{ _justify_items_, { property_family::layout } },
			{ _align_items_, { property_family::layout } },
			{ _justify_self_, { property_family::layout } },
			{ _align_self_, { property_family::layout } },
			{ _justify_content_, { property_family::layout } },
			{ _align_content_, { property_family::layout } },
			{ _place_items_, { property_family::layout, { _align_items_, _justify_items_ } } },
			{ _place_self_, { property_family::layout, { _align_self_, _justify_self_ } } },
			{ _place_content_, { property_family::layout, { _align_content_, _justify_content_ } } },
			{ _transform_, { property_family::layout } },
			{ _transform_origin_, { property_family::layout } },
			{ _id("perspective"), { property_family::layout } },
			{ _id("perspective-origin"), { property_family::layout } },
			{ _id("transform-box"), { property_family::keyword, {}, "content-box;border-box;fill-box;stroke-box;view-box" } },
			{ _id("transform-style"), { property_family::keyword, {}, "flat;preserve-3d" } },
			{ _id("backface-visibility"), { property_family::keyword, {}, "visible;hidden" } },
			{ _id("translate"), { property_family::layout } },
			{ _id("rotate"), { property_family::layout } },
			{ _id("scale"), { property_family::layout } },
			{ _id("offset-path"), { property_family::layout } },
			{ _id("offset-distance"), { property_family::layout } },
			{ _id("offset-rotate"), { property_family::layout } },
			{ _id("offset-position"), { property_family::layout } },
			{ _id("offset-anchor"), { property_family::layout } },
			{ _id("offset"), { property_family::layout, { _id("offset-position"), _id("offset-path"),
				_id("offset-distance"), _id("offset-rotate"), _id("offset-anchor") } } },
			{ _grid_template_, { property_family::layout, { _grid_template_rows_, _grid_template_columns_, _grid_template_areas_ } } },
			{ _grid_, { property_family::layout, { _grid_template_rows_, _grid_template_columns_, _grid_template_areas_, _grid_auto_rows_, _grid_auto_columns_, _grid_auto_flow_ } } },
			{ _grid_row_, { property_family::layout, { _grid_row_start_, _grid_row_end_ } } },
			{ _grid_column_, { property_family::layout, { _grid_column_start_, _grid_column_end_ } } },
			{ _grid_area_, { property_family::layout, { _grid_row_start_, _grid_column_start_, _grid_row_end_, _grid_column_end_ } } },
			{ _grid_row_start_, { property_family::layout } },
			{ _grid_row_end_, { property_family::layout } },
			{ _grid_column_start_, { property_family::layout } },
			{ _grid_column_end_, { property_family::layout } },
			{ _grid_template_columns_, { property_family::layout } },
			{ _grid_template_rows_, { property_family::layout } },
			{ _grid_template_areas_, { property_family::layout } },
			{ _grid_auto_rows_, { property_family::layout } },
			{ _grid_auto_columns_, { property_family::layout } },
			{ _grid_auto_flow_, { property_family::layout } },
			{ _order_, { property_family::layout } },
			{ _counter_increment_, { property_family::layout } },
			{ _counter_reset_, { property_family::layout } },
			{ _id("counter-set"), { property_family::layout } },
			{ _content_, { property_family::layout } },
			{ _id("quotes"), { property_family::layout } },
			{ _cursor_, { property_family::layout } },
			{ _display_, { property_family::layout } },
			{ _visibility_, { property_family::keyword, {}, visibility_strings } },
			{ _id("pointer-events"), { property_family::keyword, {}, "auto;none" } },
			{ _id("user-select"), { property_family::keyword, {}, "auto;text;none;all" } },
			{ _id("contain"), { property_family::box } },
			{ _id("content-visibility"), { property_family::keyword, {}, "visible;hidden;auto" } },
			{ _id("container-name"), { property_family::box } },
			{ _id("container-type"), { property_family::keyword, {}, "normal;inline-size;size" } },
			{ _id("container"), { property_family::box, { _id("container-name"), _id("container-type") } } },
			{ _id("contain-intrinsic-width"), { property_family::box } },
			{ _id("contain-intrinsic-height"), { property_family::box } },
			{ _id("contain-intrinsic-size"), { property_family::box,
				{ _id("contain-intrinsic-width"), _id("contain-intrinsic-height") } } },
			{ _position_, { property_family::keyword, {}, element_position_strings } },
			{ _float_, { property_family::keyword, {}, element_float_strings } },
			{ _clear_, { property_family::keyword, {}, element_clear_strings } },
			{ _overflow_, { property_family::box, { _id("overflow-x"), _id("overflow-y") } } },
			{ _id("overflow-x"), { property_family::box } },
			{ _id("overflow-y"), { property_family::box } },
			{ _id("overflow-clip-margin"), { property_family::box } },
			{ _id("clip-path"), { property_family::box } },
			{ _id("clip-rule"), { property_family::keyword, {}, "nonzero;evenodd" } },
			{ _id("text-anchor"), { property_family::keyword, {}, "start;middle;end" } },
			{ _id("scroll-behavior"), { property_family::keyword, {}, "auto;smooth" } },
			{ _id("scroll-snap-type"), { property_family::box } },
			{ _id("scroll-snap-align"), { property_family::box } },
			{ _id("scroll-snap-stop"), { property_family::keyword, {}, "normal;always" } },
			{ _id("overscroll-behavior"), { property_family::box, { _id("overscroll-behavior-x"), _id("overscroll-behavior-y") } } },
			{ _id("overscroll-behavior-x"), { property_family::keyword, {}, overscroll_behavior_keywords } },
			{ _id("overscroll-behavior-y"), { property_family::keyword, {}, overscroll_behavior_keywords } },
			{ _id("scrollbar-width"), { property_family::keyword, {}, "auto;thin;none" } },
			{ _id("scrollbar-color"), { property_family::background } },
			{ _id("color-scheme"), { property_family::box } },
			{ _id("scrollbar-gutter"), { property_family::box } },
			{ _id("ruby-position"), { property_family::keyword, {}, "over;under" } },
			{ _id("text-overflow"), { property_family::keyword, {}, "clip;ellipsis" } },
			{ _id("resize"), { property_family::keyword, {}, resize_strings } },
			{ _appearance_, { property_family::keyword, {}, appearance_strings } },
			{ _box_sizing_, { property_family::keyword, {}, box_sizing_strings } },
			{ _text_align_, { property_family::text, { _id("text-align-all"), _id("text-align-last") } } },
			{ _id("text-align-all"), { property_family::text } },
			{ _id("text-align-last"), { property_family::text } },
			{ _id("text-justify"), { property_family::text } },
			{ _id("text-autospace"), { property_family::text } },
			{ _id("text-spacing-trim"), { property_family::text } },
			{ _id("text-spacing"), { property_family::text, { _id("text-spacing-trim"), _id("text-autospace") } } },
			{ _vertical_align_, { property_family::keyword, {}, vertical_align_strings } },
			{ _text_transform_, { property_family::text } },
			{ _white_space_, { property_family::text, { _id("white-space-collapse"), _id("text-wrap-mode"), _id("white-space-trim") } } },
			{ _id("white-space-collapse"), { property_family::keyword, {}, white_space_collapse_strings } },
			{ _id("text-wrap-mode"), { property_family::keyword, {}, text_wrap_mode_strings } },
			{ _id("text-wrap-style"), { property_family::keyword, {}, text_wrap_style_strings } },
			{ _id("word-break"), { property_family::keyword, {}, word_break_strings } },
			{ _id("line-break"), { property_family::keyword, {}, line_break_strings } },
			{ _id("overflow-wrap"), { property_family::keyword, {}, overflow_wrap_strings } },
			{ _id("hyphens"), { property_family::keyword, {}, hyphens_strings } },
			{ _id("hyphenate-character"), { property_family::text } },
			{ _id("hyphenate-limit-zone"), { property_family::text } },
			{ _id("hyphenate-limit-lines"), { property_family::text } },
			{ _id("hyphenate-limit-chars"), { property_family::text } },
			{ _id("hyphenate-limit-last"), { property_family::keyword, {}, hyphenate_limit_last_strings } },
			{ _id("text-wrap"), { property_family::text, { _id("text-wrap-mode"), _id("text-wrap-style") } } },
			{ _id("white-space-trim"), { property_family::text } },
			{ _font_style_, { property_family::text } },
			{ _list_style_type_, { property_family::background } },
			{ _list_style_position_, { property_family::keyword, {}, list_style_position_strings } },
			{ _border_left_style_, { property_family::keyword, {}, border_style_strings } },
			{ _border_right_style_, { property_family::keyword, {}, border_style_strings } },
			{ _border_top_style_, { property_family::keyword, {}, border_style_strings } },
			{ _border_bottom_style_, { property_family::keyword, {}, border_style_strings } },
			{ _border_collapse_, { property_family::keyword, {}, border_collapse_strings } },
			{ _flex_direction_, { property_family::keyword, {}, flex_direction_strings } },
			{ _flex_wrap_, { property_family::keyword, {}, flex_wrap_strings } },
			{ _caption_side_, { property_family::keyword, {}, caption_side_strings } },
			{ _text_decoration_style_, { property_family::keyword, {}, style_text_decoration_style_strings } },
		};
		if (const auto it = properties.find(name); it != properties.end()) return &it->second;

		static const auto flow_properties = [] {
			std::map<string_id, property_definition> result;
			const auto add_edges = [&](const string& prefix, const string& suffix, css_edge_group kind) {
				const auto& group = css_edge_properties(kind);
				for (size_t side = 0; side < group.logical.size(); ++side) {
					result.emplace(group.logical[side], property_definition{property_family::logical, {}, nullptr, group.physical[side]});
				}
				result.emplace(_id(prefix + "block" + suffix), property_definition{property_family::logical_pair,
					{group.logical[0], group.logical[2]}, nullptr, group.physical[0]});
				result.emplace(_id(prefix + "inline" + suffix), property_definition{property_family::logical_pair,
					{group.logical[3], group.logical[1]}, nullptr, group.physical[3]});
			};
			add_edges("margin-", "", css_edge_group::margin);
			add_edges("padding-", "", css_edge_group::padding);
			add_edges("inset-", "", css_edge_group::inset);
			add_edges("border-", "-width", css_edge_group::border_width);
			add_edges("border-", "-style", css_edge_group::border_style);
			add_edges("border-", "-color", css_edge_group::border_color);

			const auto add_scroll = [&](const string& name, css_edge_group kind, property_family family) {
				const auto& group = css_edge_properties(kind);
				result.emplace(_id(name), property_definition{family, {group.physical.begin(), group.physical.end()}});
				for (auto physical : group.physical) result.emplace(physical, property_definition{family});
				add_edges(name + "-", "", kind);
			};
			add_scroll("scroll-padding", css_edge_group::scroll_padding, property_family::scroll_padding);
			add_scroll("scroll-margin", css_edge_group::scroll_margin, property_family::scroll_margin);

			const auto border_side = [](size_t side) {
				return vector<string_id>{css_edge_properties(css_edge_group::border_width).logical[side],
					css_edge_properties(css_edge_group::border_style).logical[side],
					css_edge_properties(css_edge_group::border_color).logical[side]};
			};
			static constexpr std::array physical_borders{_border_top_, _border_right_, _border_bottom_, _border_left_};
			static constexpr std::array physical_radii{
				_border_top_left_radius_, _border_top_right_radius_, _border_bottom_right_radius_, _border_bottom_left_radius_,
			};
			for (size_t index = 0; index < logical_sides.size(); ++index) {
				result.emplace(_id(string("border-") + logical_sides[index]),
					property_definition{property_family::logical, border_side(index), nullptr, physical_borders[index]});
				result.emplace(_id(string("border-") + logical_corners[index] + "-radius"),
					property_definition{property_family::logical,
						{css_edge_properties(css_edge_group::radius_x).logical[index], css_edge_properties(css_edge_group::radius_y).logical[index]},
						nullptr, physical_radii[index]});
			}
			const auto add_border_axis = [&](const char* name, size_t start, size_t end) {
				auto targets = border_side(start);
				const auto other = border_side(end);
				targets.insert(targets.end(), other.begin(), other.end());
				result.emplace(_id(name), property_definition{property_family::logical, std::move(targets), nullptr, physical_borders[start]});
			};
			add_border_axis("border-block", 0, 2);
			add_border_axis("border-inline", 3, 1);
			return result;
		}();
		const auto found = flow_properties.find(name);
		return found == flow_properties.end() ? nullptr : &found->second;
	}
}

const vector<string_id>& css_longhands(string_id name) {
	static const vector<string_id> empty;
	const auto* definition = find_property(name);
	return definition ? definition->longhands : empty;
}

const char* css_keywords(string_id name) {
	const auto* definition = find_property(name);
	return definition ? definition->keywords : nullptr;
}

std::optional<css_diagnostic> check_css_declaration(css_declaration_status status, string_id name,
	const css_token_vector& tokens, const css_source& source)
{
	if (source.kind == css_source_kind::none ||
		(status != css_declaration_status::unknown && status != css_declaration_status::invalid))
	{
		return std::nullopt;
	}
	const auto kind = status == css_declaration_status::unknown ?
		css_diagnostic_kind::unknown_property : css_diagnostic_kind::unsupported_value;
	return css_diagnostic{ kind, _s(name), tokens.empty() ? string() : get_repr(tokens, 0, -1, true), source };
}

static bool has_drawing_gap(string_id, const property_value&)
{
	// Keep this shared check for declarations that parse ahead of their painting support.
	return false;
}

static props_map parse_property_value(string_id name, const property_definition& definition, const css_token_vector& tokens,
	const string& baseurl, bool important, document_container* container)
{
	const auto& longhands = definition.longhands;
	const auto keyword = tokens.size() == 1 ? tokens.front().ident() : string();
	css_value_parser parser;
	switch (definition.family)
	{
	case property_family::wide_only:
		break;
	case property_family::keyword:
		if (const int index = value_index(keyword, definition.keywords); index >= 0)
			parser.add_parsed_property(name, property_value(index, important));
		break;
	case property_family::box:
		parser.parse_box(name, tokens, baseurl, important);
		break;
	case property_family::background:
		parser.parse_background_and_border(name, tokens, baseurl, important, container);
		break;
	case property_family::mask:
		parser.parse_mask(name, tokens, baseurl, important, container);
		break;
	case property_family::text:
		parser.parse_text(name, tokens, important, container);
		break;
	case property_family::layout:
		parser.parse_layout(name, tokens, baseurl, important, container);
		break;
	case property_family::transition:
		parse_css_transition(parser, name, tokens, important);
		break;
	case property_family::animation:
		parse_css_animation(parser, name, tokens, important);
		break;
	case property_family::scroll_padding:
	case property_family::scroll_margin: {
		const bool padding = definition.family == property_family::scroll_padding;
		const int options = (padding ? f_length_percentage | f_positive : f_length) | f_math;
		const char* keywords = padding ? "auto" : "";
		if (tokens.size() > (longhands.empty() ? 1 : longhands.size())) break;
		css_length values[4];
		const int count = parse_1234_lengths(tokens, values, options, keywords);
		if (!count) break;
		if (longhands.empty()) {
			parser.add_parsed_property(name, property_value(values[0], important));
		}
		else {
			const int indices[] = {0, count > 1 ? 1 : 0, count > 2 ? 2 : 0, count > 3 ? 3 : count > 1 ? 1 : 0};
			for (size_t side = 0; side < longhands.size(); ++side)
				parser.add_parsed_property(longhands[side], property_value(values[indices[side]], important));
		}
		break;
	}
	case property_family::logical: {
		const auto& syntax = *find_property(definition.syntax);
		const auto parsed = parse_property_value(definition.syntax, syntax, tokens, baseurl, important, container);
		if (syntax.longhands.empty()) {
			if (const auto value = parsed.find(definition.syntax); value != parsed.end())
				parser.add_parsed_property(name, value->second);
		}
		else {
			for (size_t index = 0; index < longhands.size(); ++index) {
				const auto from = syntax.longhands[index % syntax.longhands.size()];
				if (const auto value = parsed.find(from); value != parsed.end())
					parser.add_parsed_property(longhands[index], value->second);
			}
		}
		break;
	}
	case property_family::logical_pair: {
		if (tokens.empty() || tokens.size() > 2) break;
		const auto& syntax = *find_property(definition.syntax);
		const auto start = parse_property_value(definition.syntax, syntax, {tokens.front()}, baseurl, important, container);
		if (start.empty()) break;
		const auto finish = tokens.size() == 2 ?
			parse_property_value(definition.syntax, syntax, {tokens.back()}, baseurl, important, container) : start;
		if (finish.empty()) break;
		parser.add_parsed_property(longhands[0], start.at(definition.syntax));
		parser.add_parsed_property(longhands[1], finish.at(definition.syntax));
		break;
	}
	}
	return parser.take_properties();
}

string_id css_canonical_property(string_id name) {
	static const std::map<string_id, string_id> aliases = {
		{ _id("font-stretch"), _id("font-width") },
		{ _id("word-wrap"), _id("overflow-wrap") },
		{ _id("-webkit-user-select"), _id("user-select") },
		// Dockit currently lays out boxes in the horizontal writing mode.
		{ _id("inline-size"), _width_ },
		{ _id("min-inline-size"), _min_width_ },
		{ _id("max-inline-size"), _max_width_ },
		{ _id("block-size"), _height_ },
		{ _id("contain-intrinsic-inline-size"), _id("contain-intrinsic-width") },
		{ _id("contain-intrinsic-block-size"), _id("contain-intrinsic-height") },
		{ _id("background-repeat-inline"), _id("background-repeat-x") },
		{ _id("background-repeat-block"), _id("background-repeat-y") },
		{ _id("overflow-inline"), _id("overflow-x") },
		{ _id("overflow-block"), _id("overflow-y") },
		{ _id("overscroll-behavior-inline"), _id("overscroll-behavior-x") },
		{ _id("overscroll-behavior-block"), _id("overscroll-behavior-y") },
		{ _id("min-block-size"), _min_height_ },
		{ _id("max-block-size"), _max_height_ },
	};
	const auto alias = aliases.find(name);
	return alias == aliases.end() ? name : alias->second;
}

static css_parsed_declaration parse_declaration(string_id name, const css_token_vector& tokens,
	const string& baseurl, bool important, document_container* container, const css_source& source,
	const css_token_vector& original_tokens)
{
	using status = css_declaration_status;
	const auto original_name = name;
	name = css_canonical_property(name);
	const auto& spelling = _s(name);
	const bool custom = spelling.size() > 2 && spelling.compare(0, 2, "--") == 0;
	const auto* definition = find_property(name);
	if (!custom && !definition) return { status::unknown, {} };
	if (tokens.empty() ? !custom : !is_declaration_value(tokens)) return { status::invalid, {} };

	const auto variables = inspect_css_variables(tokens);
	if (variables == css_variable_syntax::invalid) return { status::invalid, {} };
	const auto& longhands = css_longhands(name);
	const auto expand = [&](const property_value& value, status result) {
		props_map properties;
		if (longhands.empty()) properties.emplace(name, value);
		else for (const auto longhand : longhands) properties.emplace(longhand, value);
		return css_parsed_declaration{ result, std::move(properties) };
	};

	const auto wide_keyword = read_css_wide_keyword(tokens);
	switch (wide_keyword)
	{
	case css_wide_keyword::inherit: return expand(property_value(inherit{}, important), status::parsed);
	case css_wide_keyword::unset:
	case css_wide_keyword::initial:
		// This non-inherited logical longhand defaults to the flow-start edge, even in RTL.
		if (name == _id("background-position-inline")) {
			const vector<css_axis_position> initial{ { {}, css_position_edge::flow_start } };
			return expand(property_value(initial, important), status::parsed);
		}
		return wide_keyword == css_wide_keyword::initial ?
			expand(property_value(initial_value{}, important), status::parsed) :
			expand(property_value(invalid{}, important), status::parsed);
	case css_wide_keyword::revert: return expand(property_value(revert_value{}, important), status::parsed);
	case css_wide_keyword::revert_layer: return expand(property_value(revert_layer_value{}, important), status::parsed);
	case css_wide_keyword::none: break;
	}

	if (custom)
		return expand(property_value(tokens, important), status::parsed);
	if (variables == css_variable_syntax::valid)
	{
		property_value value(invalid{}, important);
		value.deferred = std::make_shared<property_value::deferred_declaration>(
			property_value::deferred_declaration{ original_name, tokens, baseurl, source });
		return expand(value, status::deferred);
	}

	auto properties = parse_property_value(name, *definition, tokens, baseurl, important, container);
	if (properties.empty()) return { status::invalid, {} };
	for (const auto longhand : longhands)
		properties.try_emplace(longhand, initial_value{}, important);

	if (source.kind != css_source_kind::none)
	{
		std::shared_ptr<const property_value::declaration_origin> origin;
		for (auto& [longhand, value] : properties)
		{
			if (!has_drawing_gap(longhand, value)) continue;
			if (!origin) origin = std::make_shared<property_value::declaration_origin>(
				property_value::declaration_origin{ original_name, get_repr(original_tokens, 0, -1, true), source });
			value.origin = origin;
		}
	}
	return { status::parsed, std::move(properties) };
}

css_parsed_declaration parse_css_declaration(string_id name, const css_token_vector& tokens,
	const string& baseurl, bool important, document_container* container, const css_source& source)
{
	return parse_declaration(name, tokens, baseurl, important, container, source, tokens);
}

css_parsed_declaration reparse_css_declaration(const property_value::deferred_declaration& declaration,
	const css_token_vector& tokens, bool important, document_container* container)
{
	return parse_declaration(declaration.name, tokens, declaration.baseurl, important, container, declaration.source, declaration.tokens);
}

bool supports_css_declaration(const css_token_vector& tokens)
{
	int index = 0;
	skip_whitespace(tokens, index);
	if (index == static_cast<int>(tokens.size()) || tokens[index].type != IDENT)
		return false;

	auto declaration = css_parser(css_token_vector(tokens.begin() + index, tokens.end())).consume_declaration();
	if (!declaration) return false;

	remove_whitespace(declaration.value);
	const auto name = _id(declaration.name.compare(0, 2, "--") == 0 ? declaration.name : lowcase(declaration.name));
	const auto result = parse_css_declaration(name, declaration.value, "", declaration.important, nullptr, {});
	if (result.status == css_declaration_status::deferred)
		return true;
	return result.status == css_declaration_status::parsed &&
		std::none_of(result.properties.begin(), result.properties.end(), [](const auto& entry) {
			return has_drawing_gap(entry.first, entry.second);
		});
}

} // namespace litehtml
