// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#pragma once

#include "css_string_id.h"
#include "css_types.h"
#include "css_margins.h"
#include "css_borders.h"
#include "css_clip_path.h"
#include "css_offsets.h"
#include "css_transform.h"
#include "css_transition.h"
#include "css_animation.h"
#include "css_motion_path.h"
#include "css_grid_values.h"
#include "css_shadows.h"
#include "css_values.h"
#include "css_background.h"
#include "css_border_image.h"

#include <tuple>

namespace dockit::css {
	using namespace litehtml;

	struct ComputeContext;

	template<class CssT, class CompT>
	class css_property
	{
	public:
		CssT	css_value;
		CompT	computed_value;

		css_property(const CssT& css_val, const CompT& computed_val) : css_value(css_val), computed_value(computed_val) {}
	};

	// CSS Properties types
	using css_line_height_t = css_property<css_length, pixel_t>;

	class ComputedStyle
	{
	private:
		std::vector<box_shadow> m_text_shadows;
		std::vector<box_shadow> m_box_shadows;
		element_position		m_el_position;
		text_align				m_text_align;
		text_align				m_text_align_last = text_align_auto;
		text_justify				m_text_justify = text_justify_auto;
		int m_text_autospace = text_autospace_normal;
		int m_text_spacing_trim = text_spacing_normal;
		overflow m_overflow_x = overflow_visible;
		overflow m_overflow_y = overflow_visible;
		bool m_smooth_scroll = false;
		css_scroll_snap_type m_scroll_snap_type;
		css_scroll_snap_align m_scroll_snap_align;
		bool m_scroll_snap_stop = false;
		overscroll_behavior m_overscroll_x = overscroll_behavior::automatic;
		overscroll_behavior m_overscroll_y = overscroll_behavior::automatic;
		css_clip_margin m_overflow_clip_margin;
		css_clip_path_ref m_clip_path;
		int m_clip_rule = 0;
		int m_text_anchor = 0;
		std::array<css_length, 4> m_scroll_padding;
		std::array<css_length, 4> m_scroll_margin;
		scrollbar_width m_scrollbar_width = scrollbar_width::automatic;
		scrollbar_gutter m_scrollbar_gutter = scrollbar_gutter::automatic;
		int m_containment = 0;
		int m_used_containment = 0;
		content_visibility m_content_visibility = content_visibility::visible;
		container_type m_container_type = container_type::normal;
		string_vector m_container_names;
		bool m_auto_content_skipped = false;
		bool m_replaced = false;
		bool m_content_replacement = false;
		css_contain_intrinsic_size m_contain_intrinsic_width;
		css_contain_intrinsic_size m_contain_intrinsic_height;
		std::optional<pixel_t> m_remembered_content_width;
		std::optional<pixel_t> m_remembered_content_height;
		std::optional<css_scrollbar_colors> m_scrollbar_colors;
		resize_mode m_resize = resize_none;
		white_space_collapse m_white_space_collapse = white_space_collapse_spaces;
		text_wrap_mode m_text_wrap_mode = text_wrap_wrap;
		text_wrap_style m_text_wrap_style = text_wrap_auto;
		word_break m_word_break = word_break_normal;
		line_break m_line_break = line_break_auto;
		hyphens m_hyphens = hyphens_manual;
		string m_hyphenate_character;
		css_hyphenation_limits m_hyphenation_limits;
		css_hyphenation_chars m_hyphenation_chars;
		overflow_wrap m_overflow_wrap = overflow_wrap_normal;
		int m_white_space_trim = white_space_trim_none;
		style_display			m_display;
		visibility				m_visibility;
		bool m_pointer_events_none = false;
		css_user_select m_user_select = css_user_select::automatic;
		bool m_text_overflow_ellipsis = false;
		int m_ruby_position = 0;
		appearance				m_appearance;
		bool m_fixed_table_layout = false;
		bool m_hide_empty_cells = false;
		web_color m_accent_color{0, 117, 255};
		web_color m_caret_color;
		double m_opacity = 1.0;
		std::shared_ptr<const css_transition_styles> m_transitions;
		std::shared_ptr<const css_animation_styles> m_animations;
		int m_mix_blend_mode = 0;
		int m_isolation = 0;
		int m_unicode_bidi = 0;
		css_filter_list m_filters;
		css_filter_list m_backdrop_filters;
		int m_will_change = 0;
		box_sizing				m_box_sizing;
		css_length				m_z_index;
		vertical_align			m_vertical_align;
		element_float			m_float;
		element_clear			m_clear;
		css_margins				m_css_margins;
		css_margins				m_css_padding;
		css_borders				m_css_borders;
		css_outline m_outline;
		css_aspect_ratio m_aspect_ratio;
		object_fit m_object_fit = object_fit::fill;
		css_image_position m_object_position;
		css_image_resolution m_image_resolution;
		css_image_orientation m_image_orientation = css_image_orientation::from_image;
		int m_image_rendering = static_cast<int>(image_rendering::automatic);
		css_length				m_css_width;
		css_length				m_css_height;
		css_length				m_css_min_width;
		css_length				m_css_min_height;
		css_length				m_css_max_width;
		css_length				m_css_max_height;
		css_offsets				m_css_offsets;
		css_text_indent			m_css_text_indent;
		css_length				m_tab_size{ 8, css_units_none };
		css_length				m_css_line_height;
		css_line_height_t		m_line_height {{}, 0};
		css_list_marker_type	m_list_marker_type;
		list_style_position		m_list_style_position;
		css_image_ptr			m_list_style_image;
		string					m_list_style_image_baseurl;
		background				m_bg;
		css_border_image m_border_image;
		std::shared_ptr<const css_mask> m_mask;
		bool m_svg_mask_uses_alpha = false;
		bool m_svg_interpolates_linear_rgb = false;
		uint_ptr				m_font;
		css_length				m_font_size;
		int m_font_size_keyword = font_size_medium;
		bool m_relative_font_size = true;
		css_font_families		m_font_families;
		css_length				m_font_weight;
		float					m_font_width = 100.0f;
		css_font_style			m_font_style;
		css_font_features m_font_features;
		css_font_features m_font_variant_ligatures;
		css_font_features m_font_variant_numeric;
		css_font_features m_font_variant_east_asian;
		css_font_features m_font_variant_caps;
		css_font_alternates m_font_alternates;
		css_length m_letter_spacing;
		css_length m_word_spacing;
		int m_font_kerning = font_kerning_auto;
		int m_font_optical_sizing = font_optical_sizing_auto;
		css_font_variations m_font_variations;
		css_font_size_adjust m_font_size_adjust;
		int						m_text_decoration_line = text_decoration_line_none;
		text_decoration_style	m_text_decoration_style = text_decoration_style_solid;
		css_length				m_text_decoration_thickness;
		web_color				m_text_decoration_color;
		int						m_text_underline_position = underline_auto;
		css_length				m_text_underline_offset;
		css_underline			m_underline;
		string					m_text_emphasis_style;
		web_color				m_text_emphasis_color;
		int						m_text_emphasis_position = text_emphasis_position_right;
		font_metrics			m_font_metrics;
		text_transform			m_text_transform;
		web_color				m_color;
		int m_color_scheme_support = 0;
		string					m_cursor;
		css_generated_content	m_content;
		css_quotes				m_quotes;
		border_collapse			m_border_collapse;
		css_length				m_css_border_spacing_x;
		css_length				m_css_border_spacing_y;

		float					m_flex_grow;
		float					m_flex_shrink;
		css_length				m_flex_basis;
		flex_direction			m_flex_direction;
		css_alignment m_justify_items;
		css_alignment m_align_items;
		css_alignment m_justify_self;
		css_alignment m_align_self;
		css_alignment m_justify_content;
		css_alignment m_align_content;
		grid_line m_grid_row_start, m_grid_row_end, m_grid_column_start, m_grid_column_end;
		css_transform_list m_transforms;
		css_transform_origin m_transform_origin;
		css_length m_perspective = css_length::predef_value();
		css_image_position m_perspective_origin;
		css_individual_transforms m_individual_transforms;
		int m_transform_box = static_cast<int>(css_transform_box::view);
		int m_backface_visibility = 0;
		int m_transform_style = 0;
		css_motion_path m_motion_path;
		grid_track_template m_grid_template_columns, m_grid_template_rows;
		grid_area_template m_grid_template_areas;
		grid_track_list m_grid_auto_rows, m_grid_auto_columns;
		grid_auto_flow m_grid_auto_flow;
		css_length m_row_gap, m_column_gap;
		flex_wrap				m_flex_wrap;
		flex_justify_content	m_flex_justify_content;
		flex_align_items		m_flex_align_items;
		flex_align_items		m_flex_align_self;
		flex_align_content		m_flex_align_content;

		caption_side			m_caption_side = caption_side_top;

		int 					m_order;

	private:
		void compute_font(const ComputeContext& context);
		void compute_background(const ComputeContext& context);
		void compute_mask(const ComputeContext& context);
		void compute_flex(const ComputeContext& context);
		void snap_border_width(css_length& width, const ComputeContext& context);

	public:
		ComputedStyle() :
				m_el_position(element_position_static),
				m_text_align(text_align_left),
				m_display(display_inline),
				m_visibility(visibility_visible),
				m_appearance(appearance_none),
				m_box_sizing(box_sizing_content_box),
				m_z_index(0),
				m_vertical_align(va_baseline),
				m_float(float_none),
				m_clear(clear_none),
				m_css_margins(),
				m_css_padding(),
				m_css_borders(),
				m_css_width(),
				m_css_height(),
				m_css_min_width(),
				m_css_min_height(),
				m_css_max_width(),
				m_css_max_height(),
				m_css_offsets(),
				m_css_text_indent(),
				m_css_line_height(0),
				m_list_style_position(list_style_position_outside),
				m_bg(),
				m_font(0),
				m_font_size(0),
				m_font_metrics(),
				m_text_transform(text_transform_none),
				m_border_collapse(border_collapse_separate),
				m_css_border_spacing_x(),
				m_css_border_spacing_y(),
				m_flex_grow(0),
				m_flex_shrink(1),
				m_flex_direction(flex_direction_row),
				m_flex_wrap(flex_wrap_nowrap),
				m_flex_justify_content(flex_justify_content_flex_start),
				m_flex_align_items(flex_align_items_stretch),
				m_flex_align_self(flex_align_items_auto),
				m_flex_align_content(flex_align_content_stretch),
				m_order(0)
		{}

		void compute(const ComputeContext& context);
		std::vector<std::tuple<string, string>> dump_get_attrs();

		element_position get_position() const;
		void set_position(element_position mElPosition);

		text_align get_text_align() const;
		text_align get_text_align_last() const { return m_text_align_last; }
		text_justify get_text_justify() const { return m_text_justify; }
		int get_text_autospace() const { return m_text_autospace; }
		int get_text_spacing_trim() const { return m_text_spacing_trim; }
		void set_text_align(text_align mTextAlign);

		overflow get_overflow_x() const { return m_overflow_x; }
		overflow get_overflow_y() const { return m_overflow_y; }
		bool uses_smooth_scroll() const { return m_smooth_scroll; }
		const css_scroll_snap_type& get_scroll_snap_type() const { return m_scroll_snap_type; }
		const css_scroll_snap_align& get_scroll_snap_align() const { return m_scroll_snap_align; }
		bool stops_scroll_snap() const { return m_scroll_snap_stop; }
		overscroll_behavior get_overscroll_x() const { return m_overscroll_x; }
		overscroll_behavior get_overscroll_y() const { return m_overscroll_y; }
		const css_clip_margin& get_overflow_clip_margin() const { return m_overflow_clip_margin; }
		const std::array<css_length, 4>& get_scroll_padding() const { return m_scroll_padding; }
		const std::array<css_length, 4>& get_scroll_margin() const { return m_scroll_margin; }
		scrollbar_width get_scrollbar_width() const { return m_scrollbar_width; }
		scrollbar_gutter get_scrollbar_gutter() const { return m_scrollbar_gutter; }
		bool has_box_containment() const;
		bool allows_box_containment() const;
		bool has_style_containment() const
		{
			return (m_used_containment & contain_style) && m_display != display_none && m_display != display_inline_text;
		}
		bool allows_size_containment() const;
		container_type get_container_type() const { return m_container_type; }
		const string_vector& get_container_names() const { return m_container_names; }
		bool establishes_query_container_context() const
		{
			return m_container_type != container_type::normal && allows_size_containment();
		}
		bool hides_contents() const { return m_content_visibility == content_visibility::hidden && allows_size_containment(); }
		bool has_auto_content_visibility() const { return m_content_visibility == content_visibility::automatic && allows_size_containment(); }
		bool skips_contents() const { return hides_contents() || (m_auto_content_skipped && has_auto_content_visibility()); }
		bool set_auto_content_skipped(bool skip);
		bool has_inline_size_containment() const
		{
			return (m_used_containment & (contain_size | contain_inline_size)) && allows_size_containment();
		}
		bool has_block_size_containment() const
		{
			return (m_used_containment & contain_size) && allows_size_containment();
		}
		bool has_containment() const { return has_box_containment() || has_style_containment() || has_inline_size_containment(); }
		std::optional<pixel_t> get_contain_intrinsic_width() const;
		std::optional<pixel_t> get_contain_intrinsic_height() const;
		void remember_content_size(pixel_t width, pixel_t height);
		void copy_remembered_content_size(const ComputedStyle& source);
		bool has_layout_containment() const { return (m_used_containment & contain_layout) && has_box_containment(); }
		bool has_paint_containment() const { return (m_used_containment & contain_paint) && has_box_containment(); }
		const std::optional<css_scrollbar_colors>& get_scrollbar_colors() const { return m_scrollbar_colors; }
		bool clips_overflow() const { return m_overflow_x != overflow_visible || m_overflow_y != overflow_visible; }
		bool is_scroll_container() const { return is_scrollable_overflow(m_overflow_x) || is_scrollable_overflow(m_overflow_y); }
		bool has_text_overflow_ellipsis() const { return m_text_overflow_ellipsis; }
		resize_mode get_resize() const { return m_resize; }

		white_space_collapse get_white_space_collapse() const { return m_white_space_collapse; }
		text_wrap_mode get_text_wrap_mode() const { return m_text_wrap_mode; }
		text_wrap_style get_text_wrap_style() const { return m_text_wrap_style; }
		word_break get_word_break() const { return m_word_break; }
		line_break get_line_break() const { return m_line_break; }
		hyphens get_hyphens() const { return m_hyphens; }
		const string& get_hyphenate_character() const { return m_hyphenate_character; }
		const css_hyphenation_limits& get_hyphenation_limits() const { return m_hyphenation_limits; }
		const css_hyphenation_chars& get_hyphenation_chars() const { return m_hyphenation_chars; }
		overflow_wrap get_effective_overflow_wrap() const
		{
			return m_word_break == word_break_break_word ? overflow_wrap_anywhere : m_overflow_wrap;
		}
		int get_white_space_trim() const { return m_white_space_trim; }
		void copy_white_space(const ComputedStyle& source)
		{
			m_white_space_collapse = source.get_white_space_collapse();
			m_text_wrap_mode = source.get_text_wrap_mode();
			m_word_break = source.get_word_break();
			m_line_break = source.get_line_break();
			m_hyphens = source.get_hyphens();
			m_overflow_wrap = source.m_overflow_wrap;
		}
		bool collapses_white_space() const
		{
			return m_white_space_collapse == white_space_collapse_spaces || m_white_space_collapse == white_space_preserve_breaks;
		}
		bool preserves_white_space() const
		{
			return m_white_space_collapse == white_space_preserve || m_white_space_collapse == white_space_preserve_spaces ||
				m_white_space_collapse == white_space_break_spaces;
		}

		style_display get_display() const;
		void set_display(style_display mDisplay);
		bool is_flow_root() const { return m_display == display_flow_root; }

		visibility get_visibility() const;
		bool accepts_pointer_events() const { return !m_pointer_events_none; }
		css_user_select get_user_select() const { return m_user_select; }
		void set_visibility(visibility mVisibility);

		appearance get_appearance() const;
		bool uses_fixed_table_layout() const { return m_fixed_table_layout; }
		bool hides_empty_cells() const { return m_hide_empty_cells; }
		bool is_ruby_under() const { return m_ruby_position == 1; }
		bool is_right_to_left() const { return m_bg.m_direction == static_cast<int>(css_direction::rtl); }
		int get_unicode_bidi_mode() const { return m_unicode_bidi; }
		const web_color& get_accent_color() const { return m_accent_color; }
		const web_color& get_caret_color() const { return m_caret_color; }
		double get_opacity() const { return m_opacity; }
		const css_transition_styles* get_transitions() const { return m_transitions.get(); }
		const css_animation_styles* get_animations() const { return m_animations.get(); }
		css_blend_mode get_mix_blend_mode() const { return static_cast<css_blend_mode>(m_mix_blend_mode); }
		bool mixes_with_backdrop() const { return get_mix_blend_mode() != css_blend_mode::normal; }
		bool isolates_blending() const { return m_isolation == 1; }
		bool has_group_opacity() const { return m_opacity < 1.0; }
		const css_filter_list& get_filters() const { return m_filters; }
		bool has_filters() const { return !m_filters.empty(); }
		const css_filter_list& get_backdrop_filters() const { return m_backdrop_filters; }
		bool has_backdrop_filters() const { return !m_backdrop_filters.empty(); }
		const css_clip_path_ref& get_clip_path() const { return m_clip_path; }
		bool uses_even_odd_clip_rule() const { return m_clip_rule == 1; }
		int get_text_anchor() const { return m_text_anchor; }
		const css_mask* get_mask() const { return m_mask && m_mask->active ? m_mask.get() : nullptr; }
		bool svg_mask_uses_alpha() const { return m_svg_mask_uses_alpha; }
		bool svg_interpolates_linear_rgb() const { return m_svg_interpolates_linear_rgb; }
		bool has_will_change(css_will_change_hint hint) const { return (m_will_change & hint) != 0; }
		bool has_backdrop_effects() const {
			return has_group_opacity() || has_filters() || has_backdrop_filters() || m_clip_path || get_mask() || mixes_with_backdrop();
		}
		bool establishes_backdrop_root() const { return has_backdrop_effects() || has_will_change(will_change_backdrop); }
		bool has_group_effects() const { return has_backdrop_effects() || isolates_blending(); }
		void set_appearance(appearance mAppearance);

		box_sizing get_box_sizing() const;
		void set_box_sizing(box_sizing mBoxSizing);

		int get_z_index() const;
		void set_z_index(int mZIndex);

		vertical_align get_vertical_align() const;
		void set_vertical_align(vertical_align mVerticalAlign);

		element_float get_float() const;
		void set_float(element_float mFloat);

		element_clear get_clear() const;
		void set_clear(element_clear mClear);

		const css_margins &get_margins() const;
		void set_margins(const css_margins &mCssMargins);

		const css_margins &get_padding() const;
		void set_padding(const css_margins &mCssPadding);

		const css_borders &get_borders() const;
		void set_borders(const css_borders &mCssBorders);

		const css_aspect_ratio& get_aspect_ratio() const { return m_aspect_ratio; }
		object_fit get_object_fit() const { return m_object_fit; }
		const css_image_position& get_object_position() const { return m_object_position; }
		const css_image_resolution& get_image_resolution() const { return m_image_resolution; }
		const css_outline& get_outline() const { return m_outline; }
		const css_border_image& get_border_image() const { return m_border_image; }
		css_image_orientation get_image_orientation() const { return m_image_orientation; }
		image_rendering get_image_rendering() const { return static_cast<image_rendering>(m_image_rendering); }
		const css_length &get_width() const;
		void set_width(const css_length &mCssWidth);

		const css_length &get_height() const;
		void set_height(const css_length &mCssHeight);

		const css_length &get_min_width() const;
		void set_min_width(const css_length &mCssMinWidth);

		const css_length &get_min_height() const;
		void set_min_height(const css_length &mCssMinHeight);

		const css_length &get_max_width() const;
		void set_max_width(const css_length &mCssMaxWidth);

		const css_length &get_max_height() const;
		void set_max_height(const css_length &mCssMaxHeight);

		const css_offsets &get_offsets() const;
		void set_offsets(const css_offsets &mCssOffsets);

		const css_text_indent& get_text_indent() const { return m_css_text_indent; }
		const css_length& get_tab_size() const { return m_tab_size; }
		const css_length& get_letter_spacing() const { return m_letter_spacing; }
		const css_length& get_word_spacing() const { return m_word_spacing; }

		const css_line_height_t& line_height() const;
		css_line_height_t& line_height_w();

		const css_list_marker_type& get_list_marker_type() const { return m_list_marker_type; }

		list_style_position get_list_style_position() const;
		void set_list_style_position(list_style_position mListStylePosition);

		const css_image_ptr& get_list_style_image() const;

		const string& get_list_style_image_baseurl() const;

		const background &get_bg() const;
		void set_bg(const background &mBg);

		pixel_t get_font_size() const;
		const css_font_size_adjust& get_font_size_adjust() const { return m_font_size_adjust; }
		float get_font_weight() const { return m_font_weight.val(); }
		const css_font_families& get_font_families() const { return m_font_families; }
		const css_font_style& get_font_style() const { return m_font_style; }
		float get_font_width() const { return m_font_width; }
		void set_font_size(pixel_t mFontSize);

		uint_ptr get_font() const;
		void set_font(uint_ptr mFont);

		const font_metrics& get_font_metrics() const;
		void set_font_metrics(const font_metrics& mFontMetrics);

		text_transform get_text_transform() const;
		void set_text_transform(text_transform mTextTransform);

		web_color get_color() const;
		color_scheme get_color_scheme() const { return m_color.scheme; }
		void set_color(web_color color);

		const std::vector<box_shadow>& get_text_shadows() const { return m_text_shadows; }
		const std::vector<box_shadow>& get_box_shadows() const { return m_box_shadows; }

		const string& get_cursor() const;
		void set_cursor(const string& cursor);

		const css_generated_content& get_content() const { return m_content; }
		bool is_replaced() const { return m_replaced; }
		const image* get_content_image() const {
			return m_content_replacement ? m_content.items.front().image_value.get() : nullptr;
		}
		void prepare_marker(bool outside);
		const css_quotes& get_quotes() const { return m_quotes; }

		border_collapse get_border_collapse() const;
		void set_border_collapse(border_collapse mBorderCollapse);

		const css_length& get_border_spacing_x() const ;
		void set_border_spacing_x(const css_length& mBorderSpacingX);

		const css_length& get_border_spacing_y() const;
		void set_border_spacing_y(const css_length& mBorderSpacingY);

		caption_side get_caption_side() const;
		void set_caption_side(caption_side side);

		float get_flex_grow() const;
		float get_flex_shrink() const;
		const css_length& get_flex_basis() const;
		flex_direction get_flex_direction() const;
		css_alignment get_justify_items() const { return m_justify_items; }
		css_alignment get_align_items() const { return m_align_items; }
		css_alignment get_justify_self() const { return m_justify_self; }
		css_alignment get_align_self() const { return m_align_self; }
		css_alignment get_justify_content() const { return m_justify_content; }
		css_alignment get_align_content() const { return m_align_content; }
		grid_item_lines get_grid_lines() const {
			return { {m_grid_row_start, m_grid_row_end}, {m_grid_column_start, m_grid_column_end} };
		}
		bool has_z_index() const { return !m_z_index.is_predefined(); }
		const css_transform_list& get_transforms() const { return m_transforms; }
		const css_transform_origin& get_transform_origin() const { return m_transform_origin; }
		const css_length& get_perspective() const { return m_perspective; }
		const css_image_position& get_perspective_origin() const { return m_perspective_origin; }
		const css_individual_transforms& get_individual_transforms() const { return m_individual_transforms; }
		css_transform_box get_transform_box() const { return static_cast<css_transform_box>(m_transform_box); }
		bool hides_backface() const { return m_backface_visibility == 1; }
		const css_motion_path& get_motion_path() const { return m_motion_path; }
		bool is_transformable() const {
			return m_display != display_inline && m_display != display_inline_text &&
				m_display != display_table_column && m_display != display_table_column_group;
		}
		bool has_perspective() const { return !m_perspective.is_predefined() && is_transformable(); }
		bool anticipates_transform() const { return is_transformable() && has_will_change(will_change_transform); }
		bool specifies_preserve_3d() const { return m_transform_style == 1 && is_transformable(); }
		bool preserves_3d() const {
			return specifies_preserve_3d() && (m_overflow_x == overflow_visible || m_overflow_x == overflow_clip) &&
				(m_overflow_y == overflow_visible || m_overflow_y == overflow_clip) && !has_group_effects() && !has_paint_containment();
		}
		bool has_transform() const {
			return (!m_transforms.empty() || m_individual_transforms.enabled() || m_motion_path.enabled()) &&
				is_transformable();
		}
		const grid_track_template& get_grid_template_columns() const { return m_grid_template_columns; }
		const grid_track_template& get_grid_template_rows() const { return m_grid_template_rows; }
		const grid_area_template& get_grid_template_areas() const { return m_grid_template_areas; }
		const grid_track_list& get_grid_auto_rows() const { return m_grid_auto_rows; }
		const grid_track_list& get_grid_auto_columns() const { return m_grid_auto_columns; }
		grid_auto_flow get_grid_auto_flow() const { return m_grid_auto_flow; }

		const css_length& get_row_gap() const { return m_row_gap; }
		const css_length& get_column_gap() const { return m_column_gap; }
		flex_wrap get_flex_wrap() const;
		flex_justify_content get_flex_justify_content() const;
		flex_align_items get_flex_align_items() const;
		flex_align_items get_flex_align_self() const;
		flex_align_content get_flex_align_content() const;

		int get_order() const;
		void set_order(int order);

		const css_underline& get_underline() const { return m_underline; }
		int get_text_decoration_line() const;
		text_decoration_style get_text_decoration_style() const;
		const css_length& get_text_decoration_thickness() const;
		const css_length& get_text_underline_offset() const { return m_text_underline_offset; }
		const web_color& get_text_decoration_color() const;

		string get_text_emphasis_style() const;
		web_color get_text_emphasis_color() const;
		int get_text_emphasis_position() const;
	};

	inline element_position ComputedStyle::get_position() const
	{
		return m_el_position;
	}

	inline void ComputedStyle::set_position(element_position mElPosition)
	{
		m_el_position = mElPosition;
	}

	inline text_align ComputedStyle::get_text_align() const
	{
		return m_text_align;
	}

	inline void ComputedStyle::set_text_align(text_align mTextAlign)
	{
		m_text_align = mTextAlign;
	}

	inline style_display ComputedStyle::get_display() const
	{
		return is_flow_root() ? display_block : m_display;
	}

	inline void ComputedStyle::set_display(style_display mDisplay)
	{
		m_display = mDisplay;
	}

	inline visibility ComputedStyle::get_visibility() const
	{
		return m_visibility;
	}

	inline void ComputedStyle::set_visibility(visibility mVisibility)
	{
		m_visibility = mVisibility;
	}

	inline appearance ComputedStyle::get_appearance() const
	{
		return m_appearance;
	}

	inline void ComputedStyle::set_appearance(appearance mAppearance)
	{
		m_appearance = mAppearance;
	}

	inline box_sizing ComputedStyle::get_box_sizing() const
	{
		return m_box_sizing;
	}

	inline void ComputedStyle::set_box_sizing(box_sizing mBoxSizing)
	{
		m_box_sizing = mBoxSizing;
	}

	inline int ComputedStyle::get_z_index() const
	{
		return (int)m_z_index.val();
	}

	inline void ComputedStyle::set_z_index(int mZIndex)
	{
		m_z_index.set_value((float)mZIndex, css_units_none);
	}

	inline vertical_align ComputedStyle::get_vertical_align() const
	{
		return m_vertical_align;
	}

	inline void ComputedStyle::set_vertical_align(vertical_align mVerticalAlign)
	{
		m_vertical_align = mVerticalAlign;
	}

	inline element_float ComputedStyle::get_float() const
	{
		return m_float;
	}

	inline void ComputedStyle::set_float(element_float mFloat)
	{
		m_float = mFloat;
	}

	inline element_clear ComputedStyle::get_clear() const
	{
		return m_clear;
	}

	inline void ComputedStyle::set_clear(element_clear mClear)
	{
		m_clear = mClear;
	}

	inline const css_margins &ComputedStyle::get_margins() const
	{
		return m_css_margins;
	}

	inline void ComputedStyle::set_margins(const css_margins &mCssMargins)
	{
		m_css_margins = mCssMargins;
	}

	inline const css_margins &ComputedStyle::get_padding() const
	{
		return m_css_padding;
	}

	inline void ComputedStyle::set_padding(const css_margins &mCssPadding)
	{
		m_css_padding = mCssPadding;
	}

	inline const css_borders &ComputedStyle::get_borders() const
	{
		return m_css_borders;
	}

	inline void ComputedStyle::set_borders(const css_borders &mCssBorders)
	{
		m_css_borders = mCssBorders;
	}

	inline const css_length &ComputedStyle::get_width() const
	{
		return m_css_width;
	}

	inline void ComputedStyle::set_width(const css_length &mCssWidth)
	{
		m_css_width = mCssWidth;
	}

	inline const css_length &ComputedStyle::get_height() const
	{
		return m_css_height;
	}

	inline void ComputedStyle::set_height(const css_length &mCssHeight)
	{
		m_css_height = mCssHeight;
	}

	inline const css_length &ComputedStyle::get_min_width() const
	{
		return m_css_min_width;
	}

	inline void ComputedStyle::set_min_width(const css_length &mCssMinWidth)
	{
		m_css_min_width = mCssMinWidth;
	}

	inline const css_length &ComputedStyle::get_min_height() const
	{
		return m_css_min_height;
	}

	inline void ComputedStyle::set_min_height(const css_length &mCssMinHeight)
	{
		m_css_min_height = mCssMinHeight;
	}

	inline const css_length &ComputedStyle::get_max_width() const
	{
		return m_css_max_width;
	}

	inline void ComputedStyle::set_max_width(const css_length &mCssMaxWidth)
	{
		m_css_max_width = mCssMaxWidth;
	}

	inline const css_length &ComputedStyle::get_max_height() const
	{
		return m_css_max_height;
	}

	inline void ComputedStyle::set_max_height(const css_length &mCssMaxHeight)
	{
		m_css_max_height = mCssMaxHeight;
	}

	inline const css_offsets &ComputedStyle::get_offsets() const
	{
		return m_css_offsets;
	}

	inline void ComputedStyle::set_offsets(const css_offsets &mCssOffsets)
	{
		m_css_offsets = mCssOffsets;
	}

	inline const css_line_height_t& ComputedStyle::line_height() const
	{
		return m_line_height;
	}

	inline css_line_height_t& ComputedStyle::line_height_w()
	{
		return m_line_height;
	}

	inline list_style_position ComputedStyle::get_list_style_position() const
	{
		return m_list_style_position;
	}

	inline void ComputedStyle::set_list_style_position(list_style_position mListStylePosition)
	{
		m_list_style_position = mListStylePosition;
	}

	inline const css_image_ptr& ComputedStyle::get_list_style_image() const { return m_list_style_image; }

	inline const string& ComputedStyle::get_list_style_image_baseurl() const { return m_list_style_image_baseurl; }

	inline const background &ComputedStyle::get_bg() const
	{
		return m_bg;
	}

	inline void ComputedStyle::set_bg(const background &mBg)
	{
		m_bg = mBg;
	}

	inline pixel_t ComputedStyle::get_font_size() const
	{
		return (pixel_t)m_font_size.val();
	}

	inline void ComputedStyle::set_font_size(pixel_t mFontSize)
	{
		m_font_size = (float)mFontSize;
	}

	inline uint_ptr ComputedStyle::get_font() const
	{
		return m_font;
	}

	inline void ComputedStyle::set_font(uint_ptr mFont)
	{
		m_font = mFont;
	}

	inline const font_metrics& ComputedStyle::get_font_metrics() const
	{
		return m_font_metrics;
	}

	inline void ComputedStyle::set_font_metrics(const font_metrics& mFontMetrics)
	{
		m_font_metrics = mFontMetrics;
	}

	inline text_transform ComputedStyle::get_text_transform() const
	{
		return m_text_transform;
	}

	inline void ComputedStyle::set_text_transform(text_transform mTextTransform)
	{
		m_text_transform = mTextTransform;
	}

	inline web_color ComputedStyle::get_color() const { return m_color; }
	inline void ComputedStyle::set_color(web_color color) { m_color = color; }

	inline const string& ComputedStyle::get_cursor() const { return m_cursor; }
	inline void ComputedStyle::set_cursor(const string& cursor) { m_cursor = cursor; }


	inline border_collapse ComputedStyle::get_border_collapse() const
	{
		return m_border_collapse;
	}

	inline void ComputedStyle::set_border_collapse(border_collapse mBorderCollapse)
	{
		m_border_collapse = mBorderCollapse;
	}

	inline const css_length& ComputedStyle::get_border_spacing_x() const
	{
		return m_css_border_spacing_x;
	}

	inline void ComputedStyle::set_border_spacing_x(const css_length& mBorderSpacingX)
	{
		m_css_border_spacing_x = mBorderSpacingX;
	}

	inline const css_length& ComputedStyle::get_border_spacing_y() const
	{
		return m_css_border_spacing_y;
	}

	inline void ComputedStyle::set_border_spacing_y(const css_length& mBorderSpacingY)
	{
		m_css_border_spacing_y = mBorderSpacingY;
	}

	inline float ComputedStyle::get_flex_grow() const
	{
		return m_flex_grow;
	}

	inline float ComputedStyle::get_flex_shrink() const
	{
		return m_flex_shrink;
	}

	inline const css_length& ComputedStyle::get_flex_basis() const
	{
		return m_flex_basis;
	}

	inline flex_direction ComputedStyle::get_flex_direction() const
	{
		return m_flex_direction;
	}

	inline flex_wrap ComputedStyle::get_flex_wrap() const
	{
		return m_flex_wrap;
	}

	inline flex_justify_content ComputedStyle::get_flex_justify_content() const
	{
		return m_flex_justify_content;
	}

	inline flex_align_items ComputedStyle::get_flex_align_items() const
	{
		return m_flex_align_items;
	}

	inline flex_align_items ComputedStyle::get_flex_align_self() const
	{
		return m_flex_align_self;
	}

	inline flex_align_content ComputedStyle::get_flex_align_content() const
	{
		return m_flex_align_content;
	}

	inline caption_side ComputedStyle::get_caption_side() const
	{
		return m_caption_side;
	}
	inline void ComputedStyle::set_caption_side(caption_side side)
	{
		m_caption_side = side;
	}

	inline int ComputedStyle::get_order() const
	{
		return m_order;
	}

	inline void ComputedStyle::set_order(int order)
	{
		m_order = order;
	}

	inline int ComputedStyle::get_text_decoration_line() const
	{
		return m_text_decoration_line;
	}

	inline text_decoration_style ComputedStyle::get_text_decoration_style() const
	{
		return m_text_decoration_style;
	}

	inline const css_length& ComputedStyle::get_text_decoration_thickness() const
	{
		return m_text_decoration_thickness;
	}

	inline const web_color& ComputedStyle::get_text_decoration_color() const
	{
		return m_text_decoration_color;
	}

	inline string ComputedStyle::get_text_emphasis_style() const
	{
		return m_text_emphasis_style;
	}

	inline web_color ComputedStyle::get_text_emphasis_color() const
	{
		return m_text_emphasis_color;
	}

	inline int ComputedStyle::get_text_emphasis_position() const
	{
		return m_text_emphasis_position;
	}
}

