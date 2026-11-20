// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_compute_context.h"
#include "css_declarations.h"
#include "css_syntax_util.h"

#include <cmath>

namespace dockit::css {
namespace {

	using detail::index_value;
	using detail::is_one_of;

	template<class Value, class Field>
	Value read_property(const ComputeContext& context, string_id name, bool inherited,
		const Value& initial, const Field* parent_value)
	{
		const property_value& value = context.read_declaration(context.declarations, name);
		if (value.is<Value>()) return value.get<Value>();
		if (parent_value && ((inherited && !value.is<initial_value>()) || value.is<inherit>()))
			return static_cast<Value>(*parent_value);
		return initial;
	}

	template<class Value, class ReadParent>
	Value read_flow_property(const ComputeContext& context, css_edge_group kind, size_t side,
		const Value& initial, bool rtl, const ReadParent& read_parent) {
		const auto& group = css_edge_properties(kind);
		const auto name = group.physical[side];
		const property_value& value = context.read_declaration(context.declarations, name);
		if (value.is<Value>()) return value.get<Value>();
		if (context.parent && value.is<inherit>()) {
			const bool flip = context.parent->is_right_to_left() != rtl && css_map_logical_property(value.declaration_property, rtl) == name;
			return static_cast<Value>(read_parent(group.map_direction(side, flip)));
		}
		return initial;
	}

}

UnitContext ComputeContext::unit_context(const font_metrics& metrics, css_length_context overrides) const
{
	const auto& media = overrides.viewport ? *overrides.viewport : viewport;
	const auto* root = overrides.root_font ? overrides.root_font : root_font;
	return {media, root ? *root : metrics, &host, [](const void* instance, float points) {
		return static_cast<const StyleHost*>(instance)->points_to_pixels(points);
	}, host.container_units()};
}

pixel_t ComputeContext::to_pixels(const css_length& length, const font_metrics& metrics,
	pixel_t basis, css_length_context overrides) const
{
	return length_to_pixels(length, metrics, basis, unit_context(metrics, overrides));
}

void ComputeContext::cvt_units(css_length& length, const font_metrics& metrics, pixel_t basis) const
{
	compute_length(length, metrics, basis, unit_context(metrics));
}

void ComputedStyle::compute(const ComputeContext& context)
{
	const auto* parent = context.parent;
	const auto& host = context.host;
	const auto* color_parent = parent;
	m_color_scheme_support = read_property(context, _id("color-scheme"), true, 0,
		parent ? &parent->m_color_scheme_support : nullptr);
	const int preferred = context.viewport.preferred_color_scheme == color_scheme::dark ? scheme_dark : scheme_light;
	const bool dark = m_color_scheme_support & preferred ? preferred == scheme_dark : m_color_scheme_support & scheme_dark_first;
	const auto scheme = dark ? color_scheme::dark : color_scheme::light;
	web_color inherited_color = parent ? parent->get_color() : system_color_value(system_color::canvas_text, scheme);
	inherited_color.scheme = scheme;
	m_color = read_property(context, _color_, true, web_color(system_color::canvas_text),
		parent ? &parent->m_color : nullptr).resolve(inherited_color);
	m_color.scheme = scheme;
	const auto& content_declaration = context.read_declaration(context.declarations, _content_);
	static const css_generated_content no_content;
	m_content = read_property(context, _content_, false, no_content, parent ? &parent->m_content : nullptr);
	m_content_replacement = context.allow_content_replacement && m_content.is_single_image();
	m_replaced = context.is_replaced || m_content_replacement;
	const auto ignore_image_hint = [&](string_id name) {
		return context.replacement_ignores_size_hints && m_content_replacement &&
			context.read_declaration(context.declarations, name).priority.layer == css_default_layer;
	};

	m_el_position	 = (element_position)	read_property<int>(context,  _position_,		false,	element_position_static,	 (parent ? &parent->m_el_position : nullptr));
	m_display		 = (style_display)		read_property<int>(context,  _display_,			false,	display_inline,			 (parent ? &parent->m_display : nullptr));
	if (m_display == display_contents && m_content_replacement)
		m_display = display_none;
	else if (m_display == display_contents && context.is_root)
		m_display = display_block;
	else if (m_display == display_contents && context.suppress_contents_display)
		m_display = display_none;
	if (m_content_replacement) {
		if (is_one_of(m_display, display_table, display_grid, display_flex, display_flow_root))
			m_display = display_block;
		else if (!is_one_of(m_display, display_none, display_block, display_list_item))
			m_display = display_inline_block;
	}
	m_visibility	 = (visibility)			read_property<int>(context,  _visibility_,		true,	visibility_visible,		 (parent ? &parent->m_visibility : nullptr));
	m_pointer_events_none = read_property<int>(context, _id("pointer-events"), true, 0,
		parent ? &parent->m_pointer_events_none : nullptr) != 0;
	m_user_select = static_cast<css_user_select>(read_property<int>(context, _id("user-select"), false,
		static_cast<int>(css_user_select::automatic), parent ? &parent->m_user_select : nullptr));
	m_float			 = (element_float)		read_property<int>(context,  _float_,			false,	float_none,				 (parent ? &parent->m_float : nullptr));
	m_clear			 = (element_clear)		read_property<int>(context,  _clear_,			false,	clear_none,				 (parent ? &parent->m_clear : nullptr));
	m_appearance	 = (appearance)			read_property<int>(context,  _appearance_,		false,	appearance_none,			 (parent ? &parent->m_appearance : nullptr));
	m_fixed_table_layout = read_property<int>(context, _id("table-layout"), false, 0,
		parent ? &parent->m_fixed_table_layout : nullptr) == 1;
	m_hide_empty_cells = read_property<int>(context, _id("empty-cells"), true, 0,
		parent ? &parent->m_hide_empty_cells : nullptr) == 1;
	m_accent_color = read_property(context, _id("accent-color"), true, web_color(system_color::accent),
		parent ? &parent->m_accent_color : nullptr).compute_for_scheme(scheme);
	m_caret_color = read_property(context, _id("caret-color"), true, web_color::current_color,
		parent ? &parent->m_caret_color : nullptr).compute_for_scheme(scheme);
	m_opacity = read_property(context, _id("opacity"), false, 1.0f, parent ? &parent->m_opacity : nullptr);
	static const std::array transition_names{
		_id("transition-property"), _id("transition-duration"), _id("transition-delay"),
		_id("transition-timing-function"), _id("transition-behavior")
	};
	if (std::any_of(transition_names.begin(), transition_names.end(), [&](string_id name) {
		return !context.read_declaration(context.declarations, name).is<invalid>();
	})) {
		static const css_transition_styles defaults;
		const auto* previous = parent && parent->m_transitions ? parent->m_transitions.get() : &defaults;
		m_transitions = std::make_shared<css_transition_styles>(css_transition_styles{
			read_property(context, transition_names[0], false, defaults.properties, &previous->properties),
			read_property(context, transition_names[1], false, defaults.durations, &previous->durations),
			read_property(context, transition_names[2], false, defaults.delays, &previous->delays),
			read_property(context, transition_names[3], false, defaults.easings, &previous->easings),
			read_property(context, transition_names[4], false, defaults.behaviors, &previous->behaviors)
		});
	} else {
		m_transitions.reset();
	}
	const auto& animation_names = css_animation_property_names();
	if (std::any_of(animation_names.begin(), animation_names.end(), [&](string_id name) {
		return !context.read_declaration(context.declarations, name).is<invalid>();
	})) {
		static const css_animation_styles defaults;
		const auto& previous = parent && parent->m_animations ? *parent->m_animations : defaults;
		m_animations = std::make_shared<css_animation_styles>(css_animation_styles{
			read_property(context, animation_names[0], false, defaults.names, &previous.names),
			read_property(context, animation_names[1], false, defaults.durations, &previous.durations),
			read_property(context, animation_names[2], false, defaults.delays, &previous.delays),
			read_property(context, animation_names[3], false, defaults.easings, &previous.easings),
			read_property(context, animation_names[4], false, defaults.iterations, &previous.iterations),
			read_property(context, animation_names[5], false, defaults.directions, &previous.directions),
			read_property(context, animation_names[6], false, defaults.fills, &previous.fills),
			read_property(context, animation_names[7], false, defaults.play_states, &previous.play_states)
		});
	} else {
		m_animations.reset();
	}
	m_mix_blend_mode = read_property(context, _id("mix-blend-mode"), false, 0, parent ? &parent->m_mix_blend_mode : nullptr);
	m_isolation = read_property(context, _id("isolation"), false, 0, parent ? &parent->m_isolation : nullptr);
	const auto& unicode_bidi = context.read_declaration(context.declarations, _id("unicode-bidi"));
	if (unicode_bidi.is<int>()) {
		m_unicode_bidi = unicode_bidi.get<int>();
	} else if (unicode_bidi.is<initial_value>()) {
		m_unicode_bidi = 0;
	} else if (unicode_bidi.is<inherit>()) {
		m_unicode_bidi = parent ? parent->m_unicode_bidi : 0;
	} else {
		m_unicode_bidi = parent ? parent->m_unicode_bidi : 0;
	}
	m_box_sizing	 = (box_sizing)			read_property<int>(context,  _box_sizing_,		false,	box_sizing_content_box,	 (parent ? &parent->m_box_sizing : nullptr));
	m_overflow_x = static_cast<overflow>(read_property<int>(context, _id("overflow-x"), false, overflow_visible,
		parent ? &parent->m_overflow_x : nullptr));
	m_overflow_y = static_cast<overflow>(read_property<int>(context, _id("overflow-y"), false, overflow_visible,
		parent ? &parent->m_overflow_y : nullptr));
	if (is_scroll_container()) {
		const auto normalize = [](overflow value) {
			if (value == overflow_visible) return overflow_auto;
			if (value == overflow_clip) return overflow_hidden;
			return value;
		};
		m_overflow_x = normalize(m_overflow_x);
		m_overflow_y = normalize(m_overflow_y);
	}
	m_smooth_scroll = read_property<int>(context, _id("scroll-behavior"), false, 0,
		parent ? &parent->m_smooth_scroll : nullptr) != 0;
	m_scroll_snap_type = read_property(context, _id("scroll-snap-type"), false, css_scroll_snap_type{},
		parent ? &parent->m_scroll_snap_type : nullptr);
	m_scroll_snap_align = read_property(context, _id("scroll-snap-align"), false, css_scroll_snap_align{},
		parent ? &parent->m_scroll_snap_align : nullptr);
	m_scroll_snap_stop = read_property<int>(context, _id("scroll-snap-stop"), false, 0,
		parent ? &parent->m_scroll_snap_stop : nullptr) != 0;
	m_overscroll_x = static_cast<overscroll_behavior>(read_property<int>(context, _id("overscroll-behavior-x"), false,
		static_cast<int>(overscroll_behavior::automatic), parent ? &parent->m_overscroll_x : nullptr));
	m_overscroll_y = static_cast<overscroll_behavior>(read_property<int>(context, _id("overscroll-behavior-y"), false,
		static_cast<int>(overscroll_behavior::automatic), parent ? &parent->m_overscroll_y : nullptr));
	m_scrollbar_width = static_cast<scrollbar_width>(read_property<int>(context, _id("scrollbar-width"), false,
		static_cast<int>(scrollbar_width::automatic), parent ? &parent->m_scrollbar_width : nullptr));
	m_scrollbar_gutter = static_cast<scrollbar_gutter>(read_property<int>(context, _id("scrollbar-gutter"), false,
		static_cast<int>(scrollbar_gutter::automatic), parent ? &parent->m_scrollbar_gutter : nullptr));
	m_containment = read_property(context, _id("contain"), false, 0, parent ? &parent->m_containment : nullptr);
	m_content_visibility = static_cast<content_visibility>(read_property<int>(context, _id("content-visibility"), false,
		static_cast<int>(content_visibility::visible), parent ? &parent->m_content_visibility : nullptr));
	m_container_type = static_cast<container_type>(read_property<int>(context, _id("container-type"), false,
		static_cast<int>(container_type::normal), parent ? &parent->m_container_type : nullptr));
	m_container_names = read_property(context, _id("container-name"), false, string_vector{},
		parent ? &parent->m_container_names : nullptr);
	m_scrollbar_colors = read_property(context, _id("scrollbar-color"), true, std::optional<css_scrollbar_colors>{},
		parent ? &parent->m_scrollbar_colors : nullptr);
	if (m_scrollbar_colors) {
		m_scrollbar_colors->thumb = m_scrollbar_colors->thumb.compute_for_scheme(scheme);
		m_scrollbar_colors->track = m_scrollbar_colors->track.compute_for_scheme(scheme);
	}
	m_ruby_position = read_property<int>(context, _id("ruby-position"), true, 0, parent ? &parent->m_ruby_position : nullptr);
	m_text_overflow_ellipsis = read_property<int>(context, _id("text-overflow"), false, 0,
		parent ? &parent->m_text_overflow_ellipsis : nullptr) != 0;
	m_resize = static_cast<resize_mode>(read_property<int>(context, _id("resize"), false, resize_none,
		parent ? &parent->m_resize : nullptr));
	m_text_align = static_cast<text_align>(read_property<int>(context, _id("text-align-all"), true, text_align_left, (parent ? &parent->m_text_align : nullptr)));
	m_text_align_last = static_cast<text_align>(read_property<int>(context, _id("text-align-last"), true, text_align_auto, (parent ? &parent->m_text_align_last : nullptr)));
	m_text_justify = static_cast<text_justify>(read_property<int>(context, _id("text-justify"), true, text_justify_auto, (parent ? &parent->m_text_justify : nullptr)));
	m_text_autospace = read_property<int>(context, _id("text-autospace"), true, text_autospace_normal, (parent ? &parent->m_text_autospace : nullptr));
	m_text_spacing_trim = read_property<int>(context, _id("text-spacing-trim"), true, text_spacing_normal, (parent ? &parent->m_text_spacing_trim : nullptr));
	if (m_text_align == text_align_match_parent)
		m_text_align = color_parent ? color_parent->get_text_align() : text_align_left;
	if (m_text_align_last == text_align_match_parent)
		m_text_align_last = color_parent ? color_parent->get_text_align_last() : text_align_left;
	m_vertical_align = (vertical_align)		read_property<int>(context,  _vertical_align_,	false,	va_baseline,				 (parent ? &parent->m_vertical_align : nullptr));
	m_text_transform = (text_transform)		read_property<int>(context,  _text_transform_,	true,	text_transform_none,		 (parent ? &parent->m_text_transform : nullptr));
	m_white_space_collapse = static_cast<white_space_collapse>(read_property<int>(context, _id("white-space-collapse"),
		true, white_space_collapse_spaces, (parent ? &parent->m_white_space_collapse : nullptr)));
	m_text_wrap_mode = static_cast<text_wrap_mode>(read_property<int>(context, _id("text-wrap-mode"),
		true, text_wrap_wrap, (parent ? &parent->m_text_wrap_mode : nullptr)));
	m_text_wrap_style = static_cast<text_wrap_style>(read_property<int>(context, _id("text-wrap-style"),
		true, text_wrap_auto, (parent ? &parent->m_text_wrap_style : nullptr)));
	m_word_break = static_cast<word_break>(read_property<int>(context, _id("word-break"),
		true, word_break_normal, (parent ? &parent->m_word_break : nullptr)));
	m_line_break = static_cast<line_break>(read_property<int>(context, _id("line-break"),
		true, line_break_auto, (parent ? &parent->m_line_break : nullptr)));
	m_hyphens = static_cast<hyphens>(read_property<int>(context, _id("hyphens"), true, hyphens_manual, (parent ? &parent->m_hyphens : nullptr)));
	m_hyphenate_character = read_property<string>(context, _id("hyphenate-character"), true, default_hyphenate_character,
		(parent ? &parent->m_hyphenate_character : nullptr));
	m_hyphenation_limits.lines = read_property<int>(context, _id("hyphenate-limit-lines"), true, -1, (parent ? &parent->m_hyphenation_limits.lines : nullptr));
	m_hyphenation_limits.last = static_cast<hyphenate_limit_last>(read_property<int>(context, _id("hyphenate-limit-last"),
		true, hyphenate_last_none, (parent ? &parent->m_hyphenation_limits.last : nullptr)));
	m_hyphenation_chars = read_property(context, _id("hyphenate-limit-chars"), true, css_hyphenation_chars{}, (parent ? &parent->m_hyphenation_chars : nullptr));
	m_overflow_wrap = static_cast<overflow_wrap>(read_property<int>(context, _id("overflow-wrap"),
		true, overflow_wrap_normal, (parent ? &parent->m_overflow_wrap : nullptr)));
	m_white_space_trim = read_property<int>(context, _id("white-space-trim"), false, white_space_trim_none, (parent ? &parent->m_white_space_trim : nullptr));
	m_caption_side	 = (caption_side)		read_property<int>(context,  _caption_side_,	true,	caption_side_top,		 (parent ? &parent->m_caption_side : nullptr));

	// https://www.w3.org/TR/CSS22/visuren.html#dis-pos-flo
	if (m_display == display_none)
	{
		// 1. If 'display' has the value 'none', then 'position' and 'float' do not apply. In this case, the element
		//    generates no box.
		m_float = float_none;
	} else
	{
		// 2. Otherwise, if 'position' has the value 'absolute' or 'fixed', the box is absolutely positioned,
		//    the computed value of 'float' is 'none', and display is set according to the table below.
		//    The position of the box will be determined by the 'top', 'right', 'bottom' and 'left' properties
		//    and the box's containing block.
		if (m_el_position == element_position_absolute || m_el_position == element_position_fixed)
		{
			m_float = float_none;

			if (m_display == display_inline_table)
			{
				m_display = display_table;
			} else if (m_display == display_inline ||
					   m_display == display_table_row_group ||
					   m_display == display_table_column ||
					   m_display == display_table_column_group ||
					   m_display == display_table_header_group ||
					   m_display == display_table_footer_group ||
					   m_display == display_table_row ||
					   m_display == display_table_cell ||
					   m_display == display_table_caption ||
					   m_display == display_inline_block)
			{
				m_display = display_block;
			}
		} else if (m_float != float_none)
		{
			// 3. Otherwise, if 'float' has a value other than 'none', the box is floated and 'display' is set
			//    according to the table below.
			if (m_display == display_inline_table)
			{
				m_display = display_table;
			} else if (m_display == display_inline ||
					   m_display == display_table_row_group ||
					   m_display == display_table_column ||
					   m_display == display_table_column_group ||
					   m_display == display_table_header_group ||
					   m_display == display_table_footer_group ||
					   m_display == display_table_row ||
					   m_display == display_table_cell ||
					   m_display == display_table_caption ||
					   m_display == display_inline_block)
			{
				m_display = display_block;
			}
		} else if(context.is_root)
		{
			// 4. Otherwise, if the element is the root element, 'display' is set according to the table below,
			//    except that it is undefined in CSS 2.2 whether a specified value of 'list-item' becomes a
			//    computed value of 'block' or 'list-item'.
			if (m_display == display_inline_table)
			{
				m_display = display_table;
			} else if (m_display == display_inline ||
				m_display == display_table_row_group ||
				m_display == display_table_column ||
				m_display == display_table_column_group ||
				m_display == display_table_header_group ||
				m_display == display_table_footer_group ||
				m_display == display_table_row ||
				m_display == display_table_cell ||
				m_display == display_table_caption ||
				m_display == display_inline_block ||
				m_display == display_list_item)
			{
				m_display = display_block;
			}
		} else if(context.is_replaced && m_display == display_inline)
		{
			m_display = display_inline_block;
		}
	}
	// 5. Otherwise, the remaining 'display' property values apply as specified.

	const auto* box_parent = context.box_parent;
	if (m_display == display_inline_grid)
	{
		if (context.is_root || m_float != float_none ||
			is_one_of(m_el_position, element_position_absolute, element_position_fixed) ||
			(box_parent && is_one_of(box_parent->get_display(), display_flex, display_inline_flex, display_grid, display_inline_grid)))
			m_display = display_grid;
	}

	compute_font(context);
	m_overflow_clip_margin = read_property(context, _id("overflow-clip-margin"), false, css_clip_margin{},
		parent ? &parent->m_overflow_clip_margin : nullptr);
	context.cvt_units(m_overflow_clip_margin.offset, m_font_metrics, 0);
	m_filters = read_property(context, _id("filter"), false, css_filter_list(), parent ? &parent->m_filters : nullptr);
	m_backdrop_filters = read_property(context, _id("backdrop-filter"), false, css_filter_list(),
		parent ? &parent->m_backdrop_filters : nullptr);
	m_will_change = read_property(context, _id("will-change"), false, 0, parent ? &parent->m_will_change : nullptr);
	for (auto* filters : { &m_filters, &m_backdrop_filters }) {
		for (auto& filter : *filters) {
			filter.color = filter.color.compute_for_scheme(scheme);
			context.cvt_units(filter.x, m_font_metrics, 0);
			context.cvt_units(filter.y, m_font_metrics, 0);
			context.cvt_units(filter.blur, m_font_metrics, 0);
		}
	}

	static const std::vector<box_shadow> no_shadows;
	static const auto text_shadow_property = _id("text-shadow");
	static const auto box_shadow_property = _id("box-shadow");
	m_box_shadows = read_property(context, box_shadow_property, false, no_shadows, parent ? &parent->m_box_shadows : nullptr);
	for (auto& shadow : m_box_shadows) shadow.color = shadow.color.compute_for_scheme(scheme);
	m_text_shadows = read_property(context, text_shadow_property, true, no_shadows, (parent ? &parent->m_text_shadows : nullptr));
	for (auto& shadow : m_text_shadows)
	{
		shadow.color = shadow.color.compute_for_scheme(scheme);
		context.cvt_units(shadow.x, m_font_metrics, 0);
		context.cvt_units(shadow.y, m_font_metrics, 0);
		context.cvt_units(shadow.blur, m_font_metrics, 0);
	}
	const css_length _auto = css_length::predef_value(0);
	const css_length none = _auto;

	m_aspect_ratio = ignore_image_hint(_aspect_ratio_) ? css_aspect_ratio{} :
		read_property(context, _aspect_ratio_, false, css_aspect_ratio{}, (parent ? &parent->m_aspect_ratio : nullptr));
	m_object_fit = read_property(context, _id("object-fit"), false, object_fit::fill, (parent ? &parent->m_object_fit : nullptr));
	m_image_resolution = read_property(context, _id("image-resolution"), true, css_image_resolution{}, (parent ? &parent->m_image_resolution : nullptr));
	m_image_orientation = read_property(context, _id("image-orientation"), true, css_image_orientation::from_image, (parent ? &parent->m_image_orientation : nullptr));
	m_image_rendering = read_property(context, _id("image-rendering"), true,
		static_cast<int>(image_rendering::automatic), (parent ? &parent->m_image_rendering : nullptr));
	m_bg.m_direction = read_property(context, _id("direction"), true, static_cast<int>(css_direction::ltr), (parent ? &parent->m_bg.m_direction : nullptr));
	const bool rtl = is_right_to_left();
	m_clip_path = read_property(context, _id("clip-path"), false, css_clip_path_ref{}, parent ? &parent->m_clip_path : nullptr);
	m_clip_rule = read_property(context, _id("clip-rule"), true, 0, parent ? &parent->m_clip_rule : nullptr);
	m_svg_mask_uses_alpha = read_property<int>(context, _id("mask-type"), false, 0,
		parent ? &parent->m_svg_mask_uses_alpha : nullptr) != 0;
	m_svg_interpolates_linear_rgb = read_property<int>(context, _id("color-interpolation"), true, 0,
		parent ? &parent->m_svg_interpolates_linear_rgb : nullptr) == 1;
	m_text_anchor = read_property(context, _id("text-anchor"), true, 0, parent ? &parent->m_text_anchor : nullptr);
	const auto compute_shape_position = [&](css_image_position& p_position) {
		p_position.x.resolve_flow_direction(rtl);
		p_position.y.resolve_flow_direction(false);
		context.cvt_units(p_position.x.offset, m_font_metrics, 0);
		context.cvt_units(p_position.y.offset, m_font_metrics, 0);
	};
	const auto compute_path_shape = [&](const css_clip_path_ref& p_shape) -> css_clip_path_ref {
		if (!p_shape || p_shape->shape == css_clip_shape::reference) return p_shape;
		auto path = std::make_shared<css_clip_path>(*p_shape);
		for (auto& dimension : path->dimensions) context.cvt_units(dimension, m_font_metrics, 0);
		auto& radii = path->radii;
		for (auto* radius : { &radii.top_left_x, &radii.top_left_y, &radii.top_right_x, &radii.top_right_y,
			&radii.bottom_right_x, &radii.bottom_right_y, &radii.bottom_left_x, &radii.bottom_left_y }) {
			context.cvt_units(*radius, m_font_metrics, 0);
		}
		compute_shape_position(path->center);
		for (auto& vertex : path->vertices) {
			for (auto& coordinate : vertex) context.cvt_units(coordinate, m_font_metrics, 0);
		}
		context.cvt_units(path->polygon_radius, m_font_metrics, 0);
		if (path->shape_path) compute_shape_path(*path->shape_path, context, m_font_metrics, rtl);
		return path;
	};
	m_clip_path = compute_path_shape(m_clip_path);
	m_motion_path.path = read_property(context, _id("offset-path"), false, css_offset_path{},
		parent ? &parent->m_motion_path.path : nullptr);
	m_motion_path.position = read_property(context, _id("offset-position"), false,
		css_offset_position{ css_offset_position::mode::normal, {} }, parent ? &parent->m_motion_path.position : nullptr);
	m_motion_path.anchor = read_property(context, _id("offset-anchor"), false, css_offset_position{},
		parent ? &parent->m_motion_path.anchor : nullptr);
	m_motion_path.distance = read_property(context, _id("offset-distance"), false, css_length{},
		parent ? &parent->m_motion_path.distance : nullptr);
	m_motion_path.rotation = read_property(context, _id("offset-rotate"), false, css_offset_rotation{},
		parent ? &parent->m_motion_path.rotation : nullptr);
	m_motion_path.path.shape = compute_path_shape(m_motion_path.path.shape);
	if (m_motion_path.path.ray && m_motion_path.path.ray->origin) compute_shape_position(*m_motion_path.path.ray->origin);
	compute_shape_position(m_motion_path.position.position);
	compute_shape_position(m_motion_path.anchor.position);
	context.cvt_units(m_motion_path.distance, m_font_metrics, 0);
	const auto compute_scroll_edges = [&](css_edge_group kind, auto& output, const auto* inherited) {
		for (size_t side = 0; side < output.size(); ++side) {
			output[side] = read_flow_property(context, kind, side, css_length(), rtl,
				[&](size_t parent_side) -> const css_length& { return (*inherited)[parent_side]; });
			context.cvt_units(output[side], m_font_metrics, 0);
		}
	};
	compute_scroll_edges(css_edge_group::scroll_padding, m_scroll_padding, parent ? &parent->m_scroll_padding : nullptr);
	compute_scroll_edges(css_edge_group::scroll_margin, m_scroll_margin, parent ? &parent->m_scroll_margin : nullptr);
	m_object_position = read_property(context, _id("object-position"), false, css_image_position{}, (parent ? &parent->m_object_position : nullptr));
	m_object_position.x.resolve_flow_direction(m_bg.m_direction == static_cast<int>(css_direction::rtl));
	m_object_position.y.resolve_flow_direction(false);
	context.cvt_units(m_object_position.x.offset, m_font_metrics, 0);
	context.cvt_units(m_object_position.y.offset, m_font_metrics, 0);
	m_css_width = ignore_image_hint(_width_) ? _auto :
		read_property<css_length>(context, _width_, false, _auto, parent ? &parent->m_css_width : nullptr);
	m_css_height = ignore_image_hint(_height_) ? _auto :
		read_property<css_length>(context, _height_, false, _auto, parent ? &parent->m_css_height : nullptr);

	m_css_min_width  = read_property<css_length>(context, _min_width_,  false, _auto, (parent ? &parent->m_css_min_width : nullptr));
	m_css_min_height = read_property<css_length>(context, _min_height_, false, _auto, (parent ? &parent->m_css_min_height : nullptr));

	m_css_max_width  = read_property<css_length>(context, _max_width_,  false, none, (parent ? &parent->m_css_max_width : nullptr));
	m_css_max_height = read_property<css_length>(context, _max_height_, false, none, (parent ? &parent->m_css_max_height : nullptr));

	context.cvt_units(m_css_width, m_font_metrics, 0);
	context.cvt_units(m_css_height, m_font_metrics, 0);

	context.cvt_units(m_css_min_width, m_font_metrics, 0);
	context.cvt_units(m_css_min_height, m_font_metrics, 0);

	context.cvt_units(m_css_max_width, m_font_metrics, 0);
	context.cvt_units(m_css_max_height, m_font_metrics, 0);

	m_contain_intrinsic_width = read_property(context, _id("contain-intrinsic-width"), false, css_contain_intrinsic_size{},
		parent ? &parent->m_contain_intrinsic_width : nullptr);
	m_contain_intrinsic_height = read_property(context, _id("contain-intrinsic-height"), false, css_contain_intrinsic_size{},
		parent ? &parent->m_contain_intrinsic_height : nullptr);
	if (m_contain_intrinsic_width.length) context.cvt_units(*m_contain_intrinsic_width.length, m_font_metrics, 0);
	if (m_contain_intrinsic_height.length) context.cvt_units(*m_contain_intrinsic_height.length, m_font_metrics, 0);
	if (!m_contain_intrinsic_width.remember) m_remembered_content_width.reset();
	if (!m_contain_intrinsic_height.remember) m_remembered_content_height.reset();

	const auto compute_edges = [&](css_edge_group kind, auto& box, const auto* inherited,
			const auto& members, const css_length& initial) {
		for (size_t index = 0; index < members.size(); ++index) {
			auto& output = box.*members[index];
			output = read_flow_property(context, kind, index, initial, rtl,
				[&](size_t side) -> const css_length& { return inherited->*members[side]; });
			context.cvt_units(output, m_font_metrics, 0);
		}
	};
	static constexpr std::array box_sides{&css_margins::top, &css_margins::right, &css_margins::bottom, &css_margins::left};
	compute_edges(css_edge_group::margin, m_css_margins, parent ? &parent->m_css_margins : nullptr, box_sides, {});
	compute_edges(css_edge_group::padding, m_css_padding, parent ? &parent->m_css_padding : nullptr, box_sides, {});

	static constexpr std::array border_sides{&css_borders::top, &css_borders::right, &css_borders::bottom, &css_borders::left};
	const auto parent_border = [&](size_t side) -> const css_border& { return parent->m_css_borders.*border_sides[side]; };
	for (size_t side = 0; side < border_sides.size(); ++side) {
		auto& border = m_css_borders.*border_sides[side];
		border.color = read_flow_property(context, css_edge_group::border_color, side, web_color::current_color, rtl,
			[&](size_t index) -> const web_color& { return parent_border(index).color; }).compute_for_scheme(scheme);
		border.style = static_cast<border_style>(read_flow_property(context, css_edge_group::border_style, side,
			static_cast<int>(border_style_none), rtl, [&](size_t index) { return parent_border(index).style; }));
		border.width = read_flow_property(context, css_edge_group::border_width, side, css_length(border_width_medium_value), rtl,
			[&](size_t index) -> const css_length& { return parent_border(index).width; });
		if (border.style == border_style_none || border.style == border_style_hidden) border.width = 0;
		snap_border_width(border.width, context);
	}

	m_outline.style = read_property(context, _id("outline-style"), false, static_cast<int>(border_style_none), (parent ? &parent->m_outline.style : nullptr));
	m_outline.width = read_property(context, _id("outline-width"), false, css_length(border_width_medium_value), (parent ? &parent->m_outline.width : nullptr));
	m_outline.offset = read_property(context, _id("outline-offset"), false, css_length(), (parent ? &parent->m_outline.offset : nullptr));
	m_outline.color = read_property(context, _id("outline-color"), false, std::optional<web_color>(), (parent ? &parent->m_outline.color : nullptr));
	if (m_outline.color) m_outline.color = m_outline.color->compute_for_scheme(scheme);
	if (!m_outline.color && m_outline.style != outline_style_auto) m_outline.color = web_color::current_color;
	snap_border_width(m_outline.width, context);
	context.cvt_units(m_outline.offset, m_font_metrics, 0);

	static constexpr std::array corner_x{&css_border_radius::top_left_x, &css_border_radius::top_right_x,
		&css_border_radius::bottom_right_x, &css_border_radius::bottom_left_x};
	static constexpr std::array corner_y{&css_border_radius::top_left_y, &css_border_radius::top_right_y,
		&css_border_radius::bottom_right_y, &css_border_radius::bottom_left_y};
	const auto* parent_radius = parent ? &parent->m_css_borders.radius : nullptr;
	compute_edges(css_edge_group::radius_x, m_css_borders.radius, parent_radius, corner_x, {});
	compute_edges(css_edge_group::radius_y, m_css_borders.radius, parent_radius, corner_y, {});

	m_border_collapse = (border_collapse) read_property<int>(context, _border_collapse_, true, border_collapse_separate, (parent ? &parent->m_border_collapse : nullptr));

	m_css_border_spacing_x = read_property<css_length>(context, __litehtml_border_spacing_x_, true, 0, (parent ? &parent->m_css_border_spacing_x : nullptr));
	m_css_border_spacing_y = read_property<css_length>(context, __litehtml_border_spacing_y_, true, 0, (parent ? &parent->m_css_border_spacing_y : nullptr));

	context.cvt_units(m_css_border_spacing_x, m_font_metrics, 0);
	context.cvt_units(m_css_border_spacing_y, m_font_metrics, 0);

	static constexpr std::array inset_sides{&css_offsets::top, &css_offsets::right, &css_offsets::bottom, &css_offsets::left};
	compute_edges(css_edge_group::inset, m_css_offsets, parent ? &parent->m_css_offsets : nullptr, inset_sides, _auto);

	m_z_index = read_property<css_length>(context, _z_index_, false, _auto, (parent ? &parent->m_z_index : nullptr));
	static const auto quotes = _id("quotes");
	static const css_quotes automatic_quotes;
	if (content_declaration.is<css_generated_content>()) {
		for (auto& item : m_content.items) {
			if (!item.image_value) continue;
			auto image = *item.image_value;
			host.compute_image(image, m_content.base_url, m_font_metrics, m_image_resolution, m_image_orientation, scheme);
			item.image_value = std::make_shared<const litehtml::image>(std::move(image));
		}
	}
	m_quotes = read_property(context, quotes, true, automatic_quotes, (parent ? &parent->m_quotes : nullptr));
	m_cursor = read_property<string>(context, _cursor_, true, "auto", (parent ? &parent->m_cursor : nullptr));

	m_css_text_indent = read_property(context, _text_indent_, true, css_text_indent{}, (parent ? &parent->m_css_text_indent : nullptr));
	context.cvt_units(m_css_text_indent.length, m_font_metrics, 0);
	m_hyphenation_limits.zone = read_property(context, _id("hyphenate-limit-zone"), true, css_length(), (parent ? &parent->m_hyphenation_limits.zone : nullptr));
	context.cvt_units(m_hyphenation_limits.zone, m_font_metrics, 0);

	m_tab_size = read_property(context, _id("tab-size"), true, css_length(8, css_units_none), (parent ? &parent->m_tab_size : nullptr));
	if (m_tab_size.units() == css_units_none)
	{
		if (m_tab_size.expression())
			m_tab_size.set_value(context.to_pixels(m_tab_size, m_font_metrics, 0), css_units_none);
	}
	else context.cvt_units(m_tab_size, m_font_metrics, 0);

	static const css_list_marker_type default_list_marker;
	m_list_marker_type = read_property(context, _list_style_type_, true, default_list_marker, (parent ? &parent->m_list_marker_type : nullptr));
	m_list_style_position = (list_style_position) read_property<int>(context, _list_style_position_, true, list_style_position_outside, (parent ? &parent->m_list_style_position : nullptr));

	m_list_style_image = read_property(context, _list_style_image_, true, css_image_ptr{}, parent ? &parent->m_list_style_image : nullptr);
	m_list_style_image_baseurl = read_property<string>(context, _list_style_image_baseurl_, true, "",
		parent ? &parent->m_list_style_image_baseurl : nullptr);
	if (m_list_style_image && context.read_declaration(context.declarations, _list_style_image_).is<css_image_ptr>()) {
		auto value = *m_list_style_image;
		host.compute_image(value, m_list_style_image_baseurl, m_font_metrics, m_image_resolution,
			decorative_image_orientation(m_image_orientation), scheme);
		m_list_style_image = value.is_empty() ? nullptr : std::make_shared<const image>(std::move(value));
	}

	m_order = read_property<int>(context, _order_, false, 0, (parent ? &parent->m_order : nullptr));

	compute_background(context);
	compute_mask(context);
	m_justify_items = read_property(context, _justify_items_, false, initial_alignment(_justify_items_), (parent ? &parent->m_justify_items : nullptr));
	m_align_items = read_property(context, _align_items_, false, initial_alignment(_align_items_), (parent ? &parent->m_align_items : nullptr));
	m_justify_self = read_property(context, _justify_self_, false, initial_alignment(_justify_self_), (parent ? &parent->m_justify_self : nullptr));
	m_align_self = read_property(context, _align_self_, false, initial_alignment(_align_self_), (parent ? &parent->m_align_self : nullptr));
	m_justify_content = read_property(context, _justify_content_, false, initial_alignment(_justify_content_), (parent ? &parent->m_justify_content : nullptr));
	m_align_content = read_property(context, _align_content_, false, initial_alignment(_align_content_), (parent ? &parent->m_align_content : nullptr));
	m_grid_row_start = read_property(context, _grid_row_start_, false, grid_line(), (parent ? &parent->m_grid_row_start : nullptr));
	m_grid_row_end = read_property(context, _grid_row_end_, false, grid_line(), (parent ? &parent->m_grid_row_end : nullptr));
	m_grid_column_start = read_property(context, _grid_column_start_, false, grid_line(), (parent ? &parent->m_grid_column_start : nullptr));
	m_grid_column_end = read_property(context, _grid_column_end_, false, grid_line(), (parent ? &parent->m_grid_column_end : nullptr));
	m_transforms = read_property(context, _transform_, false, css_transform_list(), (parent ? &parent->m_transforms : nullptr));
	m_transform_origin = read_property(context, _transform_origin_, false, css_transform_origin(), (parent ? &parent->m_transform_origin : nullptr));
	m_perspective = read_property(context, _id("perspective"), false, css_length::predef_value(),
		parent ? &parent->m_perspective : nullptr);
	m_perspective_origin = read_property(context, _id("perspective-origin"), false, css_image_position{},
		parent ? &parent->m_perspective_origin : nullptr);
	context.cvt_units(m_perspective, m_font_metrics, 0);
	context.cvt_units(m_perspective_origin.x.offset, m_font_metrics, 0);
	context.cvt_units(m_perspective_origin.y.offset, m_font_metrics, 0);
	m_transform_box = read_property(context, _id("transform-box"), false,
		static_cast<int>(css_transform_box::view), parent ? &parent->m_transform_box : nullptr);
	m_backface_visibility = read_property(context, _id("backface-visibility"), false, 0,
		parent ? &parent->m_backface_visibility : nullptr);
	m_transform_style = read_property(context, _id("transform-style"), false, 0,
		parent ? &parent->m_transform_style : nullptr);
	m_individual_transforms.translate = read_property(context, _id("translate"), false, css_individual_transform{},
		parent ? &parent->m_individual_transforms.translate : nullptr);
	m_individual_transforms.rotate = read_property(context, _id("rotate"), false, css_individual_transform{},
		parent ? &parent->m_individual_transforms.rotate : nullptr);
	m_individual_transforms.scale = read_property(context, _id("scale"), false, css_individual_transform{},
		parent ? &parent->m_individual_transforms.scale : nullptr);
	for (auto& transform : m_transforms)
	{
		context.cvt_units(transform.x, m_font_metrics, 0);
		context.cvt_units(transform.y, m_font_metrics, 0);
		context.cvt_units(transform.z, m_font_metrics, 0);
	}
	context.cvt_units(m_transform_origin.x, m_font_metrics, 0);
	context.cvt_units(m_transform_origin.y, m_font_metrics, 0);
	context.cvt_units(m_transform_origin.z, m_font_metrics, 0);
	if (m_individual_transforms.translate) {
		context.cvt_units(m_individual_transforms.translate->x, m_font_metrics, 0);
		context.cvt_units(m_individual_transforms.translate->y, m_font_metrics, 0);
		context.cvt_units(m_individual_transforms.translate->z, m_font_metrics, 0);
	}
	m_grid_template_columns = read_property(context, _grid_template_columns_, false, grid_track_template(), (parent ? &parent->m_grid_template_columns : nullptr));
	m_grid_template_rows = read_property(context, _grid_template_rows_, false, grid_track_template(), (parent ? &parent->m_grid_template_rows : nullptr));
	m_grid_template_areas = read_property(context, _grid_template_areas_, false, grid_area_template(), (parent ? &parent->m_grid_template_areas : nullptr));
	m_grid_auto_rows = read_property(context, _grid_auto_rows_, false, grid_track_list(), (parent ? &parent->m_grid_auto_rows : nullptr));
	m_grid_auto_columns = read_property(context, _grid_auto_columns_, false, grid_track_list(), (parent ? &parent->m_grid_auto_columns : nullptr));
	m_grid_auto_flow = read_property(context, _grid_auto_flow_, false, grid_auto_flow(), (parent ? &parent->m_grid_auto_flow : nullptr));
	const auto compute_track_lengths = [&](grid_track_list& tracks) {
		for (auto& track : tracks)
			for (auto* breadth : {&track.minimum, &track.maximum})
				if (breadth->has_length()) context.cvt_units(breadth->length, m_font_metrics, 0);
	};
	for (auto* axis : {&m_grid_template_columns, &m_grid_template_rows})
	{
		compute_track_lengths(axis->prefix.tracks);
		compute_track_lengths(axis->suffix.tracks);
		if (axis->repeat) compute_track_lengths(axis->repeat->pattern.tracks);
	}
	compute_track_lengths(m_grid_auto_rows);
	compute_track_lengths(m_grid_auto_columns);

	if (box_parent && is_grid_display(box_parent->get_display()) && m_display != display_none &&
		!is_one_of(m_el_position, element_position_absolute, element_position_fixed))
	{
		m_float = float_none;
		m_clear = clear_none;
		switch (m_display)
		{
			case display_inline_table: m_display = display_table; break;
			case display_inline_flex: m_display = display_flex; break;
			case display_inline:
			case display_inline_block:
			case display_table_caption:
			case display_table_cell:
			case display_table_row:
			case display_table_column:
			case display_table_column_group:
			case display_table_row_group:
			case display_table_header_group:
			case display_table_footer_group: m_display = display_block; break;
			default: break;
		}
	}

	compute_flex(context);
	set_auto_content_skipped(m_auto_content_skipped && m_content_visibility == content_visibility::automatic);
}

static const litehtml::pixel_t font_size_table[8][7] =
{
		{ 9,    9,     9,     9,    11,    14,    18},
		{ 9,    9,     9,    10,    12,    15,    20},
		{ 9,    9,     9,    11,    13,    17,    22},
		{ 9,    9,    10,    12,    14,    18,    24},
		{ 9,    9,    10,    13,    16,    20,    26},
		{ 9,    9,    11,    14,    17,    21,    28},
		{ 9,   10,    12,    15,    17,    23,    30},
		{ 9,   10,    13,    16,    18,    24,    32}
};

void ComputedStyle::prepare_marker(bool outside)
{
	m_display = outside ? display_inline_block : display_inline;
	if (outside)
	{
		m_white_space_collapse = white_space_collapse_spaces;
		m_text_wrap_mode = text_wrap_nowrap;
	}
	m_el_position = element_position_static;
	m_float = float_none;
	m_clear = clear_none;
	m_vertical_align = va_baseline;
	m_overflow_x = m_overflow_y = overflow_visible;
	m_smooth_scroll = false;
	m_scroll_snap_type = {};
	m_scroll_snap_align = {};
	m_scroll_snap_stop = false;
	m_overscroll_x = m_overscroll_y = overscroll_behavior::automatic;
	m_overflow_clip_margin = {};
	m_clip_path.reset();
	m_scroll_padding = {};
	m_scroll_margin = {};
	m_scrollbar_width = scrollbar_width::automatic;
	m_scrollbar_gutter = scrollbar_gutter::automatic;
	m_containment = 0;
	m_used_containment = 0;
	m_content_visibility = content_visibility::visible;
	m_container_type = container_type::normal;
	m_container_names.clear();
	m_auto_content_skipped = false;
	m_contain_intrinsic_width = m_contain_intrinsic_height = {};
	m_box_sizing = box_sizing_content_box;
	m_text_align = text_align_left;
	m_text_align_last = text_align_auto;
	m_css_text_indent = {};
	m_aspect_ratio = {};
	m_css_width = m_css_height = m_css_min_width = m_css_min_height = m_css_max_width = m_css_max_height = css_length::predef_value();
	m_css_margins = {};
	m_css_padding = {};
	m_css_borders = {};
	m_transforms.clear();
	m_perspective = css_length::predef_value();
	m_perspective_origin = {};
	m_individual_transforms = {};
	m_transform_box = static_cast<int>(css_transform_box::view);
	m_backface_visibility = 0;
	m_transform_style = 0;
	m_motion_path = {};
	m_z_index = css_length::predef_value();
	m_will_change = 0;
}

void ComputedStyle::compute_font(const ComputeContext& context)
{
	const auto* parent = context.parent;
	const auto& host = context.host;
	m_font_families = read_property(context, _font_family_, true, css_font_families(), (parent ? &parent->m_font_families : nullptr));
	const auto uses_fixed_size = [](const css_font_families& families) {
		return families.size() == 1 && families.front().generic && families.front().name == "monospace";
	};
	const bool fixed = uses_fixed_size(m_font_families);
	const bool parent_fixed = parent && uses_fixed_size(parent->m_font_families);
	const auto& declared_size = context.read_declaration(context.declarations, _font_size_);
	css_length sz = read_property<css_length>(context, _font_size_, true, css_length::predef_value(font_size_medium), (parent ? &parent->m_font_size : nullptr));

	const bool inherits_size = parent && !declared_size.is<css_length>() && !declared_size.is<initial_value>();
	if (inherits_size) {
		m_font_size_keyword = parent->m_font_size_keyword;
		m_relative_font_size = parent->m_relative_font_size;
		if (fixed != parent_fixed && m_font_size_keyword >= 0) sz = css_length::predef_value(m_font_size_keyword);
	}
	else {
		const bool keyword = sz.is_predefined() && sz.predef() >= font_size_xx_small && sz.predef() <= font_size_xx_large;
		m_font_size_keyword = keyword ? sz.predef() : -1;
		const bool relative_unit = !sz.is_predefined() && !sz.expression() &&
			is_one_of(sz.units(), css_units_percentage, css_units_em, css_units_ex, css_units_ch, css_units_cap, css_units_ic, css_units_lh);
		const bool relative_keyword = sz.is_predefined() && is_one_of(sz.predef(), int(font_size_smaller), int(font_size_larger));
		m_relative_font_size = keyword || ((!parent || parent->m_relative_font_size) && (relative_unit || relative_keyword));
	}

	pixel_t parent_sz = 0;
	const pixel_t standard_size = host.default_font_size();
	const pixel_t fixed_size = host.default_fixed_font_size();
	const pixel_t doc_font_size = fixed ? fixed_size : standard_size;
	const auto* el_parent = parent;
	if (el_parent)
	{
		parent_sz = el_parent->get_font_size();
	} else
	{
		parent_sz = standard_size;
	}

	pixel_t font_size = parent_sz;

	if(sz.is_predefined())
	{
		const auto size_index = doc_font_size - 9;
		const auto whole_index = static_cast<int>(size_index);
		int idx_in_table = whole_index + (size_index - whole_index >= 0.5f);
		if(idx_in_table >= 0 && idx_in_table <= 7)
		{
			if(sz.predef() >= font_size_xx_small && sz.predef() <= font_size_xx_large)
			{
				font_size = font_size_table[idx_in_table][sz.predef()];
			} else if(sz.predef() == font_size_smaller)
			{
				font_size = parent_sz / (pixel_t) 1.2;
			}  else if(sz.predef() == font_size_larger)
			{
				font_size = parent_sz * (pixel_t) 1.2;
			} else
			{
				font_size = parent_sz;
			}
		} else
		{
			switch(sz.predef())
			{
				case font_size_xx_small:
					font_size = doc_font_size * 3 / 5;
					break;
				case font_size_x_small:
					font_size = doc_font_size * 3 / 4;
					break;
				case font_size_small:
					font_size = doc_font_size * 8 / 9;
					break;
				case font_size_large:
					font_size = doc_font_size * 6 / 5;
					break;
				case font_size_x_large:
					font_size = doc_font_size * 3 / 2;
					break;
				case font_size_xx_large:
					font_size = doc_font_size * 2;
					break;
				case font_size_smaller:
					font_size = parent_sz / (pixel_t) 1.2;
					break;
				case font_size_larger:
					font_size = parent_sz * (pixel_t) 1.2;
					break;
				default:
					font_size = parent_sz;
					break;
			}
		}
	} else
	{
		const auto metrics = el_parent ? el_parent->get_font_metrics() : host.initial_font_metrics();
		font_size = context.to_pixels(sz, metrics, parent_sz, { el_parent ? nullptr : &metrics });
	}

	if (m_relative_font_size && m_font_size_keyword < 0 && fixed != parent_fixed && fixed_size > 0 && standard_size > 0) {
		font_size *= fixed ? fixed_size / standard_size : standard_size / fixed_size;
	}
	m_font_size = (float)font_size;

	// initialize font
	m_font_weight		=              read_property<css_length>(context, _font_weight_,		true, css_length::predef_value(font_weight_normal), (parent ? &parent->m_font_weight : nullptr));
	m_font_width = read_property(context, _id("font-width"), true, 100.0f, (parent ? &parent->m_font_width : nullptr));
	m_font_style = read_property(context, _font_style_, true, css_font_style{}, (parent ? &parent->m_font_style : nullptr));
	m_font_features = read_property(context, _id("font-feature-settings"), true, css_font_features{}, (parent ? &parent->m_font_features : nullptr));
	m_font_variant_ligatures = read_property(context, _id("font-variant-ligatures"), true, css_font_features{},
		(parent ? &parent->m_font_variant_ligatures : nullptr));
	m_font_variant_numeric = read_property(context, _id("font-variant-numeric"), true, css_font_features{},
		(parent ? &parent->m_font_variant_numeric : nullptr));
	m_font_variant_east_asian = read_property(context, _id("font-variant-east-asian"), true, css_font_features{},
		(parent ? &parent->m_font_variant_east_asian : nullptr));
	m_font_variant_caps = read_property(context, _id("font-variant-caps"), true, css_font_features{},
		(parent ? &parent->m_font_variant_caps : nullptr));
	m_font_alternates = read_property(context, _id("font-variant-alternates"), true, css_font_alternates{}, (parent ? &parent->m_font_alternates : nullptr));
	m_font_kerning = read_property(context, _id("font-kerning"), true, int(font_kerning_auto), (parent ? &parent->m_font_kerning : nullptr));
	m_font_optical_sizing = read_property(context, _id("font-optical-sizing"), true, int(font_optical_sizing_auto), (parent ? &parent->m_font_optical_sizing : nullptr));
	m_font_variations = read_property(context, _id("font-variation-settings"), true, css_font_variations{}, (parent ? &parent->m_font_variations : nullptr));
	m_font_size_adjust = read_property(context, _id("font-size-adjust"), true, css_font_size_adjust{}, (parent ? &parent->m_font_size_adjust : nullptr));
	const bool boxless = m_display == display_contents;
	const auto* decorating_parent = context.box_parent;
	const bool propagate_decoration = !boxless &&
								!is_one_of(m_display, display_inline_block, display_inline_table, display_inline_flex, display_inline_grid) &&
								m_float == float_none && !is_one_of(m_el_position, element_position_absolute, element_position_fixed);
	const auto read_decoration = [&](string_id name, auto initial, auto member) {
		// Explicit inheritance uses the DOM, while line decoration follows the box tree.
		const property_value& value = context.read_declaration(context.declarations, name);
		const auto* source = value.is<inherit>() ? parent : decorating_parent;
		return read_property(context, name, propagate_decoration, initial, source ? &(source->*member) : nullptr);
	};

	m_text_decoration_line = read_decoration(_text_decoration_line_, int(text_decoration_line_none), &ComputedStyle::m_text_decoration_line);

	// Merge parent text decoration with child text decoration
	if (propagate_decoration && decorating_parent)
	{
		m_text_decoration_line |= decorating_parent->get_text_decoration_line();
	}

	m_text_decoration_color = read_decoration(_text_decoration_color_, web_color::current_color,
		&ComputedStyle::m_text_decoration_color).compute_for_scheme(m_color.scheme);
	m_text_decoration_thickness = read_decoration(_text_decoration_thickness_,
		css_length::predef_value(text_decoration_thickness_auto), &ComputedStyle::m_text_decoration_thickness);
	m_text_decoration_style = static_cast<text_decoration_style>(read_decoration(
		_text_decoration_style_, int(text_decoration_style_solid), &ComputedStyle::m_text_decoration_style));

	// text-emphasis
	m_text_emphasis_style = read_property<string>(context, _text_emphasis_style_, true, "", (parent ? &parent->m_text_emphasis_style : nullptr));
	m_text_emphasis_position = read_property<int>(context, _text_emphasis_position_, true, text_emphasis_position_right, (parent ? &parent->m_text_emphasis_position : nullptr));
	m_text_emphasis_color = read_property(context, _text_emphasis_color_, true, web_color::current_color,
		parent ? &parent->m_text_emphasis_color : nullptr).compute_for_scheme(m_color.scheme);


	if(m_font_weight.is_predefined())
	{
		switch(m_font_weight.predef())
		{
			case font_weight_bold:
				m_font_weight = 700;
				break;
			case font_weight_bolder:
				{
					const int inherited = (int) (parent ? parent->m_font_weight.val() : 400);
					if(inherited < 400) m_font_weight = 400;
					else if(inherited >= 400 && inherited < 600) m_font_weight = 700;
					else m_font_weight = 900;
				}
				break;
			case font_weight_lighter:
				{
					const int inherited = (int) (parent ? parent->m_font_weight.val() : 400);
					if(inherited < 600) m_font_weight = 100;
					else if(inherited >= 600 && inherited < 800) m_font_weight = 400;
					else m_font_weight = 700;
				}
				break;
			default:
				m_font_weight = 400;
				break;
		}
	}

	m_line_height.css_value = read_property<css_length>(context, _line_height_, true, css_length::predef_value(),
		(parent ? &parent->m_line_height.css_value : nullptr));
	font_description descr;
	descr.families				= m_font_families;
	descr.size					= font_size;
	descr.use_font_line_gap = m_line_height.css_value.is_predefined();
	descr.style					= m_font_style;
	descr.width = m_font_width;
	descr.features = m_font_features;
	descr.alternates = m_font_alternates;
	descr.variant_features.insert(m_font_variant_ligatures.begin(), m_font_variant_ligatures.end());
	descr.variant_features.insert(m_font_variant_numeric.begin(), m_font_variant_numeric.end());
	descr.variant_features.insert(m_font_variant_east_asian.begin(), m_font_variant_east_asian.end());
	descr.variant_features.insert(m_font_variant_caps.begin(), m_font_variant_caps.end());
	descr.kerning = static_cast<css_font_kerning>(m_font_kerning);
	descr.optical_sizing = static_cast<css_font_optical_sizing>(m_font_optical_sizing);
	descr.variations = m_font_variations;
	descr.size_adjust = m_font_size_adjust;
	descr.weight				= m_font_weight.val();
	descr.right_to_left = is_right_to_left();
	descr.unicode_bidi = static_cast<css_unicode_bidi>(m_unicode_bidi);
	const auto* painted_decoration = boxless ? decorating_parent : this;
	if (painted_decoration && painted_decoration->m_text_decoration_line)
	{
		descr.decoration_line = painted_decoration->m_text_decoration_line;
		descr.decoration_thickness = painted_decoration->m_text_decoration_thickness;
		descr.decoration_style = painted_decoration->m_text_decoration_style;
		descr.decoration_color = painted_decoration->m_text_decoration_color.to_srgb(painted_decoration->m_color);
	}
	descr.emphasis_style		= m_text_emphasis_style;
	if (!m_text_emphasis_style.empty()) descr.emphasis_color = m_text_emphasis_color.to_srgb(m_color);
	descr.emphasis_position		= m_text_emphasis_position;


	// Resolve the decorating box's font and line height before converting decoration lengths.
	const auto unit = m_text_decoration_thickness.units();
	const bool resolve_thickness = !m_text_decoration_thickness.is_predefined() &&
		(m_text_decoration_thickness.expression() || !is_one_of(unit, css_units_percentage, css_units_px, css_units_none));
	if (resolve_thickness && !boxless)
		descr.decoration_thickness = css_length::predef_value(text_decoration_thickness_auto);

	m_font_metrics = {};
	m_font = host.get_font(descr, &m_font_metrics);
	if (m_font_size_adjust.mode == css_font_size_adjust::Mode::from_font)
	{
		auto reference = m_font_metrics.size_adjust_reference;
		if (!m_font && !reference) {
			auto probe = descr;
			probe.size = host.default_font_size();
			font_metrics probe_metrics;
			host.get_font(probe, &probe_metrics);
			reference = probe_metrics.size_adjust_reference;
		}
		if (reference) {
			m_font_size_adjust.mode = css_font_size_adjust::Mode::number;
			m_font_size_adjust.value = *reference;
		} else {
			m_font_size_adjust = {};
		}
	}
	m_font_metrics.font_size = font_size;
	if (!m_font)
	{
		m_font_metrics.draw_spaces = false;
	}

	if (m_line_height.css_value.is_predefined())
	{
		m_line_height.computed_value = m_font_metrics.height;
		m_font_metrics.line_height = m_font_metrics.height - m_font_metrics.emphasis_extent;
	}
	else
	{
		auto metrics = m_font_metrics;
		// A line-height declared in lh uses the parent's line height. The root uses the initial one.
		metrics.line_height = el_parent ? el_parent->get_font_metrics().line_height : host.initial_font_metrics().line_height;
		const auto computed = context.to_pixels(m_line_height.css_value, metrics, font_size, { el_parent ? nullptr : &metrics });
		if (m_line_height.css_value.units() == css_units_none)
		{
			m_line_height.computed_value = computed * font_size;
			m_line_height.css_value.set_value(computed, css_units_none);
		}
		else
		{
			m_line_height.computed_value = computed;
			m_line_height.css_value = computed;
		}
		m_font_metrics.line_height = m_line_height.computed_value;
	}

	if (resolve_thickness)
	{
		context.cvt_units(m_text_decoration_thickness, m_font_metrics, 0);
		if (!boxless) descr.decoration_thickness = m_text_decoration_thickness;
	}

	const auto resolve_spacing = [&](string_id name, css_length& spacing, const css_length* inherited_spacing) {
		auto value = read_property(context, name, true, css_length::predef_value(), inherited_spacing);
		if (value.is_predefined()) value = 0;
		context.cvt_units(value, m_font_metrics, font_size);
		spacing = std::move(value);
		const auto pixels = spacing.calc_percent(font_size);
		return std::isfinite(pixels) ? pixels : pixel_t(0);
	};
	descr.letter_spacing = resolve_spacing(_id("letter-spacing"), m_letter_spacing, (parent ? &parent->m_letter_spacing : nullptr));
	descr.word_spacing = resolve_spacing(_id("word-spacing"), m_word_spacing, (parent ? &parent->m_word_spacing : nullptr));
	m_font_metrics.letter_spacing = descr.letter_spacing;
	m_font_metrics.word_spacing = descr.word_spacing;
	if (resolve_thickness || descr.letter_spacing != 0 || descr.word_spacing != 0)
		m_font = host.get_font(descr, nullptr);

	static const auto underline_position = _id("text-underline-position");
	static const auto underline_offset = _id("text-underline-offset");
	m_text_underline_position = read_property<int>(context, underline_position, true, underline_auto, (parent ? &parent->m_text_underline_position : nullptr));
	m_text_underline_offset = read_property<css_length>(context, underline_offset, true, css_length::predef_value(), (parent ? &parent->m_text_underline_offset : nullptr));
	context.cvt_units(m_text_underline_offset, m_font_metrics, font_size);

	// The values used by an existing underline come from its decorating box.
	if (boxless)
	{
		m_underline = decorating_parent ? decorating_parent->m_underline : css_underline{};
	}
	else if (propagate_decoration && decorating_parent && (decorating_parent->m_text_decoration_line & text_decoration_line_underline))
	{
		m_underline = decorating_parent->m_underline;
	}
	else
	{
		m_underline = {};
		if (m_text_decoration_line & text_decoration_line_underline)
		{
			m_underline.position = m_text_underline_position;
			m_underline.font_position = m_font_metrics.underline_position;
			if (!m_text_underline_offset.is_predefined())
				m_underline.offset = m_text_underline_offset.calc_percent(font_size);
		}
	}
}

void ComputedStyle::compute_background(const ComputeContext& context)
{
	const auto* parent = context.parent;
	const auto& host = context.host;
	m_bg.m_color = read_property(context, _background_color_, false, web_color::transparent,
		parent ? &parent->m_bg.m_color : nullptr).compute_for_scheme(m_color.scheme);
	m_bg.m_current_color = m_color;

	const css_size auto_auto(css_length::predef_value(background_size_auto), css_length::predef_value(background_size_auto));
	m_bg.m_position_x = read_property<vector<css_axis_position>>(context, _background_position_x_, false, {{}}, (parent ? &parent->m_bg.m_position_x : nullptr));
	m_bg.m_position_y = read_property<vector<css_axis_position>>(context, _background_position_y_, false, {{}}, (parent ? &parent->m_bg.m_position_y : nullptr));
	m_bg.m_size			= read_property<size_vector>  (context, _background_size_,       false, { auto_auto }, (parent ? &parent->m_bg.m_size : nullptr));

	for (auto& x : m_bg.m_position_x)
	{
		x.resolve_flow_direction(m_bg.m_direction == static_cast<int>(css_direction::rtl));
		context.cvt_units(x.offset, m_font_metrics, 0);
	}
	for (auto& y : m_bg.m_position_y)
	{
		y.resolve_flow_direction(false);
		context.cvt_units(y.offset, m_font_metrics, 0);
	}
	for (auto& size : m_bg.m_size)
	{
		context.cvt_units(size.width,  m_font_metrics, 0);
		context.cvt_units(size.height, m_font_metrics, 0);
	}

	m_bg.m_attachment = read_property<int_vector>(context, _background_attachment_, false, { background_attachment_scroll }, (parent ? &parent->m_bg.m_attachment : nullptr));
	m_bg.m_repeat_x = read_property<vector<background_repeat>>(context, _id("background-repeat-x"), false,
		{ background_repeat::repeat }, (parent ? &parent->m_bg.m_repeat_x : nullptr));
	m_bg.m_repeat_y = read_property<vector<background_repeat>>(context, _id("background-repeat-y"), false,
		{ background_repeat::repeat }, (parent ? &parent->m_bg.m_repeat_y : nullptr));
	m_bg.m_clip       = read_property<int_vector>(context, _background_clip_,       false, { background_box_border },        (parent ? &parent->m_bg.m_clip : nullptr));
	m_bg.m_origin     = read_property<int_vector>(context, _background_origin_,     false, { background_box_padding },       (parent ? &parent->m_bg.m_origin : nullptr));
	m_bg.m_blend_modes = read_property<int_vector>(context, _id("background-blend-mode"), false, {},
		parent ? &parent->m_bg.m_blend_modes : nullptr);

	m_bg.m_image   = read_property<vector<image>>(context, _background_image_,  false, {{}}, (parent ? &parent->m_bg.m_image : nullptr));
	m_bg.m_baseurl = read_property<string>(context, _background_image_baseurl_, false, "",   (parent ? &parent->m_bg.m_baseurl : nullptr));

	for (auto& image : m_bg.m_image)
		host.compute_image(image, m_bg.m_baseurl, m_font_metrics, m_image_resolution,
			decorative_image_orientation(m_image_orientation), m_color.scheme);

	static const css_border_image defaults;
	m_border_image.source = read_property(context, _id("border-image-source"), false, defaults.source, (parent ? &parent->m_border_image.source : nullptr));
	m_border_image.slice = read_property(context, _id("border-image-slice"), false, defaults.slice, (parent ? &parent->m_border_image.slice : nullptr));
	m_border_image.width = read_property(context, _id("border-image-width"), false, defaults.width, (parent ? &parent->m_border_image.width : nullptr));
	m_border_image.outset = read_property(context, _id("border-image-outset"), false, defaults.outset, (parent ? &parent->m_border_image.outset : nullptr));
	m_border_image.repeat = read_property(context, _id("border-image-repeat"), false, defaults.repeat, (parent ? &parent->m_border_image.repeat : nullptr));
	for (auto* offsets : { &m_border_image.width, &m_border_image.outset })
		for (auto& side : offsets->sides)
			if (auto* length = std::get_if<css_length>(&side))
				context.cvt_units(*length, m_font_metrics, 0);
	host.compute_image(m_border_image.source.value, m_border_image.source.baseurl, m_font_metrics,
		m_image_resolution, decorative_image_orientation(m_image_orientation), m_color.scheme);
}

void ComputedStyle::compute_mask(const ComputeContext& context)
{
	const auto& longhands = css_longhands(_id("mask"));
	if (std::none_of(longhands.begin(), longhands.end(), [&](string_id name) {
		const auto& value = context.read_declaration(context.declarations, name);
		return !value.is<invalid>() && !value.is<initial_value>();
	})) {
		m_mask.reset();
		return;
	}
	auto mask = std::make_shared<css_mask>();
	static const css_mask defaults;
	const auto& parent = context.parent && context.parent->m_mask ? *context.parent->m_mask : defaults;
	auto& layers = mask->layers;
	const auto& inherited = parent.layers;
	const auto read = [&](const char* name, auto& field, const auto& parent_field) {
		field = read_property(context, _id(name), false, field, &parent_field);
	};
	layers.m_origin = { background_box_border };
	layers.m_color = web_color::transparent;
	layers.m_current_color = m_color;
	layers.m_direction = m_bg.m_direction;
	read("mask-image", layers.m_image, inherited.m_image);
	read("__dockit-mask-image-baseurl", layers.m_baseurl, inherited.m_baseurl);
	read("__dockit-mask-position-x", layers.m_position_x, inherited.m_position_x);
	read("__dockit-mask-position-y", layers.m_position_y, inherited.m_position_y);
	read("__dockit-mask-repeat-x", layers.m_repeat_x, inherited.m_repeat_x);
	read("__dockit-mask-repeat-y", layers.m_repeat_y, inherited.m_repeat_y);
	read("mask-size", layers.m_size, inherited.m_size);
	read("mask-origin", layers.m_origin, inherited.m_origin);
	read("mask-clip", layers.m_clip, inherited.m_clip);
	read("mask-mode", mask->modes, parent.modes);
	read("mask-composite", mask->composites, parent.composites);

	const auto& image_declaration = context.read_declaration(context.declarations, _id("mask-image"));
	mask->active = image_declaration.is<inherit>() ? parent.active :
		std::any_of(layers.m_image.begin(), layers.m_image.end(), [](const image& source) {
			return source.type != image::type_none;
		});
	for (auto& x : layers.m_position_x) {
		x.resolve_flow_direction(is_right_to_left());
		context.cvt_units(x.offset, m_font_metrics, 0);
	}
	for (auto& y : layers.m_position_y) {
		y.resolve_flow_direction(false);
		context.cvt_units(y.offset, m_font_metrics, 0);
	}
	for (auto& size : layers.m_size) {
		context.cvt_units(size.width, m_font_metrics, 0);
		context.cvt_units(size.height, m_font_metrics, 0);
	}
	if (!image_declaration.is<inherit>()) {
		for (auto& image : layers.m_image)
			context.host.compute_image(image, layers.m_baseurl, m_font_metrics, m_image_resolution,
				decorative_image_orientation(m_image_orientation), m_color.scheme);
	}
	m_mask = std::move(mask);
}

void ComputedStyle::compute_flex(const ComputeContext& context)
{
	const auto* parent = context.parent;

	// Keep computed values on all elements so explicit inheritance works through block ancestors.
	m_row_gap = read_property(context, _row_gap_, false, css_length::predef_value(), (parent ? &parent->m_row_gap : nullptr));
	m_column_gap = read_property(context, _column_gap_, false, css_length::predef_value(), (parent ? &parent->m_column_gap : nullptr));
	context.cvt_units(m_row_gap, m_font_metrics, 0);
	context.cvt_units(m_column_gap, m_font_metrics, 0);

	m_flex_direction = (flex_direction) read_property<int>(context, _flex_direction_, false, flex_direction_row, (parent ? &parent->m_flex_direction : nullptr));
	m_flex_wrap = (flex_wrap) read_property<int>(context, _flex_wrap_, false, flex_wrap_nowrap, (parent ? &parent->m_flex_wrap : nullptr));
	m_flex_justify_content = to_flex_justify_content(m_justify_content);
	m_flex_align_items = to_flex_item_alignment(m_align_items);
	m_flex_align_content = to_flex_align_content(m_align_content);
	m_flex_align_self = to_flex_item_alignment(m_align_self);

	m_flex_grow = read_property<float>(context, _flex_grow_, false, 0, (parent ? &parent->m_flex_grow : nullptr));
	m_flex_shrink = read_property<float>(context, _flex_shrink_, false, 1, (parent ? &parent->m_flex_shrink : nullptr));
	m_flex_basis = read_property<css_length>(context, _flex_basis_, false, css_length::predef_value(flex_basis_auto), (parent ? &parent->m_flex_basis : nullptr));
	if (!m_flex_basis.is_predefined() && m_flex_basis.units() == css_units_none && m_flex_basis.val() != 0)
		m_flex_basis.predef(flex_basis_auto);
	context.cvt_units(m_flex_basis, m_font_metrics, 0);

	const auto* box_parent = context.box_parent;
	if (box_parent && is_one_of(box_parent->m_display, display_flex, display_inline_flex))
	{
		if(m_display == display_inline || m_display == display_inline_block)
		{
			m_display = display_block;
		} else if(m_display == display_inline_table)
		{
			m_display = display_table;
		} else if(m_display == display_inline_flex)
		{
			m_display = display_flex;
		}
	}
}

bool ComputedStyle::has_box_containment() const
{
	return (m_used_containment & (contain_layout | contain_paint)) && allows_box_containment();
}

bool ComputedStyle::allows_box_containment() const
{
	return (m_display != display_inline || m_replaced) &&
		!is_one_of(m_display, display_none, display_contents, display_inline_text,
		display_table_column, display_table_column_group, display_table_row, display_table_row_group,
		display_table_header_group, display_table_footer_group);
}

bool ComputedStyle::set_auto_content_skipped(bool skip)
{
	const bool changed = m_auto_content_skipped != skip;
	m_auto_content_skipped = skip;
	m_used_containment = m_containment;
	if (m_container_type != container_type::normal)
		m_used_containment |= contain_style | (m_container_type == container_type::size ? contain_size : contain_inline_size);
	if (has_auto_content_visibility() || hides_contents())
		m_used_containment |= contain_layout | contain_style | contain_paint;
	if (skips_contents()) m_used_containment |= contain_size;
	return changed;
}

bool ComputedStyle::allows_size_containment() const
{
	return (m_display != display_inline || m_replaced) &&
		!is_one_of(m_display, display_none, display_contents, display_inline_text, display_table, display_inline_table) &&
		(m_display < display_table_cell || m_display > display_table_row_group);
}

void ComputedStyle::remember_content_size(pixel_t width, pixel_t height)
{
	if ((!m_contain_intrinsic_width.remember && !m_contain_intrinsic_height.remember) ||
		skips_contents() || is_one_of(m_display, display_none, display_inline_text) || (m_display == display_inline && !m_replaced)) return;
	if (m_contain_intrinsic_width.remember && !has_inline_size_containment())
		m_remembered_content_width = std::max<pixel_t>(0, width);
	if (m_contain_intrinsic_height.remember && !has_block_size_containment())
		m_remembered_content_height = std::max<pixel_t>(0, height);
}

void ComputedStyle::copy_remembered_content_size(const ComputedStyle& source)
{
	m_remembered_content_width = source.m_remembered_content_width;
	m_remembered_content_height = source.m_remembered_content_height;
}

std::optional<pixel_t> ComputedStyle::get_contain_intrinsic_width() const
{
	if (skips_contents() && m_remembered_content_width) return m_remembered_content_width;
	if (!m_contain_intrinsic_width.length) return std::nullopt;
	return std::max<pixel_t>(0, m_contain_intrinsic_width.length->calc_percent(0));
}

std::optional<pixel_t> ComputedStyle::get_contain_intrinsic_height() const
{
	if (skips_contents() && m_remembered_content_height) return m_remembered_content_height;
	if (!m_contain_intrinsic_height.length) return std::nullopt;
	return std::max<pixel_t>(0, m_contain_intrinsic_height.length->calc_percent(0));
}

// https://www.w3.org/TR/css-values-4/#snap-a-length-as-a-border-width
void ComputedStyle::snap_border_width(css_length& width, const ComputeContext& context)
{
	if (width.is_predefined() || width.units() == css_units_percentage)
	{
		return;
	}

	pixel_t px = context.to_pixels(width, m_font_metrics, 0);
	
	if (px > 0 && px < 1)
	{
		px = 1;
	} else
	{
		px = std::floor(px);
	}

	width.set_value(px, css_units_px);
}

std::vector<std::tuple<litehtml::string, litehtml::string>> ComputedStyle::dump_get_attrs()
{
	std::vector<std::tuple<string, string>> ret;

	ret.emplace_back("display", index_value(m_display, style_display_strings));
	ret.emplace_back("el_position", index_value(m_el_position, element_position_strings));
	ret.emplace_back("text_align", index_value(m_text_align, text_align_strings));
	ret.emplace_back("font_size", m_font_size.to_string());
	ret.emplace_back("overflow_x", index_value(m_overflow_x, overflow_strings));
	ret.emplace_back("overflow_y", index_value(m_overflow_y, overflow_strings));
	ret.emplace_back("white_space_collapse", index_value(m_white_space_collapse, white_space_collapse_strings));
	ret.emplace_back("text_wrap_mode", index_value(m_text_wrap_mode, text_wrap_mode_strings));
	ret.emplace_back("text_wrap_style", index_value(m_text_wrap_style, text_wrap_style_strings));
	ret.emplace_back("word_break", index_value(m_word_break, word_break_strings));
	ret.emplace_back("line_break", index_value(m_line_break, line_break_strings));
	ret.emplace_back("overflow_wrap", index_value(m_overflow_wrap, overflow_wrap_strings));
	if (m_white_space_trim)
	{
		string trim;
		if (m_white_space_trim & white_space_discard_before) trim += "discard-before ";
		if (m_white_space_trim & white_space_discard_after) trim += "discard-after ";
		if (m_white_space_trim & white_space_discard_inner) trim += "discard-inner ";
		trim.pop_back();
		ret.emplace_back("white_space_trim", std::move(trim));
	}
	ret.emplace_back("visibility", index_value(m_visibility, visibility_strings));
	ret.emplace_back("pointer_events", m_pointer_events_none ? "none" : "auto");
	ret.emplace_back("appearance", index_value(m_appearance, appearance_strings));
	ret.emplace_back("box_sizing", index_value(m_box_sizing, box_sizing_strings));
	ret.emplace_back("z_index", m_z_index.to_string());
	ret.emplace_back("vertical_align", index_value(m_vertical_align, vertical_align_strings));
	ret.emplace_back("float", index_value(m_float, element_float_strings));
	ret.emplace_back("clear", index_value(m_clear, element_clear_strings));
	ret.emplace_back("margins", m_css_margins.to_string());
	ret.emplace_back("padding", m_css_padding.to_string());
	ret.emplace_back("borders", m_css_borders.to_string());
	ret.emplace_back("width", m_css_width.to_string());
	ret.emplace_back("height", m_css_height.to_string());
	ret.emplace_back("min_width", m_css_min_width.to_string());
	ret.emplace_back("min_height", m_css_min_width.to_string());
	ret.emplace_back("max_width", m_css_max_width.to_string());
	ret.emplace_back("max_height", m_css_max_width.to_string());
	ret.emplace_back("offsets", m_css_offsets.to_string());
	ret.emplace_back("text_indent", m_css_text_indent.length.to_string());
	ret.emplace_back("line_height", std::to_string(m_line_height.computed_value));
	ret.emplace_back("list_style_type", m_list_marker_type.text.value_or(m_list_marker_type.counter.name));
	ret.emplace_back("list_style_position", index_value(m_list_style_position, list_style_position_strings));
	ret.emplace_back("border_spacing_x", m_css_border_spacing_x.to_string());
	ret.emplace_back("border_spacing_y", m_css_border_spacing_y.to_string());

	return ret;
}

}
