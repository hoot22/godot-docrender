// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <variant>
#include <optional>
#include <algorithm>

namespace litehtml {

	using uint_ptr = std::uintptr_t;
	using std::string;
	using std::vector;
	using std::shared_ptr;
	using std::make_shared;
	using std::optional;
	using std::min;
	using std::max;
	using std::swap;
	using std::abs;


	using pixel_t = float;

	using string_map = std::map<string, string>;
	using int_vector = std::vector<int>;
	using string_vector = std::vector<string>;
	using pixel_vector = std::vector<pixel_t>;

	template <class... Types>
	struct variant : std::variant<Types...>
	{
		using base = variant<Types...>; // for derived class ctors
		using std::variant<Types...>::variant; // inherit ctors
		template<class T> bool is() const { return std::holds_alternative<T>(*this); }
		template<class T> const T& get() const { return std::get<T>(*this); }
		template<class T> T& get() { return std::get<T>(*this); }
	};

	enum document_mode
	{
		no_quirks_mode,
		quirks_mode,
		limited_quirks_mode
	};

	#define  style_text_decoration_line_strings		"none;underline;overline;line-through"

	enum text_decoration_line
	{
		text_decoration_line_none			= 0x00,
		text_decoration_line_underline		= 0x01,
		text_decoration_line_overline		= 0x02,
		text_decoration_line_line_through	= 0x04,
	};

	#define  style_text_decoration_style_strings	"solid;double;dotted;dashed;wavy"

	enum text_decoration_style
	{
		text_decoration_style_solid,
		text_decoration_style_double,
		text_decoration_style_dotted,
		text_decoration_style_dashed,
		text_decoration_style_wavy,
		text_decoration_style_max,
	};

	#define  style_text_decoration_thickness_strings	"auto;from-font"

	enum text_decoration_thickness
	{
		text_decoration_thickness_auto,
		text_decoration_thickness_from_font,
	};

	#define  style_text_emphasis_position_strings	"over;under;left;right"

	enum text_emphasis_position
	{
		text_emphasis_position_over		= 0x00,
		text_emphasis_position_under	= 0x01,
		text_emphasis_position_left		= 0x02,
		text_emphasis_position_right	= 0x04,
	};

	// Transform values 1–8 match the TIFF/EXIF orientation tag.
	enum class css_image_orientation : uint8_t
	{
		from_image,
		identity,
		flip_x,
		rotate_180,
		flip_y,
		transpose,
		rotate_90,
		transverse,
		rotate_270,
		none,
	};

	inline css_image_orientation decorative_image_orientation(css_image_orientation orientation)
	{
		return orientation == css_image_orientation::none ? orientation : css_image_orientation::from_image;
	}

	struct css_image_resolution
	{
		optional<double> density;
		bool from_image = false;
		bool snap = false;
	};

	inline constexpr char image_rendering_keywords[] = "auto;smooth;high-quality;crisp-edges;pixelated;optimizespeed;optimizequality";
	enum class image_rendering { automatic, smooth, high_quality, crisp_edges, pixelated, optimize_speed, optimize_quality };

	struct css_aspect_ratio
	{
		double value = 0;
		bool prefer_natural = true;
	};

	using byte = unsigned char;
	using ucode_t = unsigned int;

	struct margins
	{
		pixel_t	left;
		pixel_t	right;
		pixel_t top;
		pixel_t bottom;

		margins()
		{
			left = right = top = bottom = 0;
		}

		pixel_t width()		const	{ return left + right; }
		pixel_t height()	const	{ return top + bottom; }
	};

	struct pointF
	{
		float x;
		float y;

		pointF() : x(0), y(0) {}
		pointF(float _x, float _y) : x(_x), y(_y) {}

		void set(float _x, float _y) { x = _x; y = _y; }
	};

	struct size
	{
		pixel_t		width;
		pixel_t		height;

		size(pixel_t w, pixel_t h) : width(w), height(h)
		{
		}

		size() : width(0), height(0)
		{
		}
	};

	struct position
	{
		using vector = std::vector<position>;

		pixel_t	x = 0;
		pixel_t	y = 0;
		pixel_t	width = 0;
		pixel_t	height = 0;

		position() = default;

		position(pixel_t _x, pixel_t _y, pixel_t _width, pixel_t _height) :
			x(_x),
			y(_y),
			width(_width),
			height(_height)
		{}

		pixel_t right()		const		{ return x + width;		}
		pixel_t bottom()	const		{ return y + height;	}
		pixel_t left()		const		{ return x;				}
		pixel_t top()		const		{ return y;				}

		void operator+=(const margins& mg)
		{
			x		-= mg.left;
			y		-= mg.top;
			width	+= mg.left + mg.right;
			height	+= mg.top + mg.bottom;
		}
		void operator-=(const margins& mg)
		{
			x		+= mg.left;
			y		+= mg.top;
			width	-= mg.left + mg.right;
			height	-= mg.top + mg.bottom;
		}

		void clear()
		{
			x = y = width = height = 0;
		}

		void round()
		{
			x = std::round(x);
			y = std::round(y);
			width = std::round(width);
			height = std::round(height);
		}

		void operator=(const size& sz)
		{
			width	= sz.width;
			height	= sz.height;
		}

		bool operator==(const position& val)
		{
			return x == val.x && y == val.y && width == val.width && height == val.height;
		}

		void move_to(pixel_t _x, pixel_t _y)
		{
			x = _x;
			y = _y;
		}

		[[nodiscard]]
		bool does_intersect(const position* val, bool can_touch = false) const
		{
			if(!val)
				return true;

			if(!can_touch)
				return (left() <= val->right() && right() >= val->left() && bottom() >= val->top() &&
						top() <= val->bottom()) ||
					   (val->left() <= right() && val->right() >= left() && val->bottom() >= top() &&
						val->top() <= bottom());
			else
				return (left() < val->right() && right() > val->left() && bottom() > val->top() &&
						top() < val->bottom()) ||
					   (val->left() < right() && val->right() > left() && val->bottom() > top() &&
						val->top() < bottom());
		}

		[[nodiscard]]
		bool on_same_line(const position& val, bool can_touch = false) const
		{
			if(can_touch)
				return !(bottom() <= val.top() || top() >= val.bottom());
			else
				return !(bottom() < val.top() || top() > val.bottom());
		}

		[[nodiscard]]
		position intersect(const position& src) const
		{
			position dest;
			pixel_t dest_x = std::max(src.x, x);
			pixel_t dest_y = std::max(src.y, y);
			pixel_t dest_x2 = std::min(src.right(), right());
			pixel_t dest_y2 = std::min(src.bottom(), bottom());

			if (dest_x2 > dest_x && dest_y2 > dest_y)
			{
				dest.x = dest_x;
				dest.y = dest_y;
				dest.width = dest_x2 - dest_x;
				dest.height = dest_y2 - dest_y;
			}
			else
			{
				dest.width = 0;
				dest.height = 0;
			}

			return dest;
		}

		[[nodiscard]]
		bool empty() const
		{
			return width == 0 && height == 0;
		}

		[[nodiscard]]
		bool is_point_inside(pixel_t _x, pixel_t _y) const
		{
			return (_x >= left() && _x < right() && _y >= top() && _y < bottom());
		}
	};

	struct font_metrics
	{
		pixel_t 	font_size = 0;		// Font size in pixels. The same as size argument of the create_font function
		pixel_t		height = 0;			// Font height in pixels.
		pixel_t		ascent = 0;			// The distance from the baseline to the top of a line of text.
		pixel_t		descent = 0;		// The distance from the baseline to the bottom of a line of text.
		pixel_t		x_height = 0;		// Height of the symbol x
		pixel_t 	ch_width = 0;		// Width of the symbol 0
		bool		draw_spaces = true;	// True to call draw text function for spaces. If False, just use space width without draw.
		pixel_t		sub_shift = 0;		// The baseline shift for subscripts.
		pixel_t		underline_position = 0;
		pixel_t		emphasis_extent = 0; // Additional line-box space for text emphasis.
		pixel_t		super_shift = 0;	// The baseline shift for superscripts.

		pixel_t		cap_height = 0;
		pixel_t		ic_width = 0;
		pixel_t		line_height = 0;		// Computed CSS line height, excluding emphasis.
		pixel_t		word_spacing = 0;
		pixel_t		letter_spacing = 0;	// Tracking at boundaries between inline fragments.
		pixel_t		half_line_gap = 0;	// Leading contributed by the font when line-height is normal.
		bool		has_metric_overrides = false;
		std::optional<double> size_adjust_reference;

		pixel_t base_line() const	{ return descent; }
	};

#define  style_display_strings		"none;block;inline;inline-block;inline-table;list-item;table;table-caption;table-cell;table-column;table-column-group;table-footer-group;table-header-group;table-row;table-row-group;inline-text;flex;inline-flex;grid;inline-grid;flow-root;contents"

	enum style_display
	{
		display_none,
		display_block,
		display_inline,
		display_inline_block,
		display_inline_table,
		display_list_item,
		display_table,
		display_table_caption,
		display_table_cell,
		display_table_column,
		display_table_column_group,
		display_table_footer_group,
		display_table_header_group,
		display_table_row,
		display_table_row_group,
		display_inline_text,
		display_flex,
		display_inline_flex,
		display_grid,
		display_inline_grid,
		display_flow_root,
		display_contents,
	};

	inline bool is_grid_display(style_display value)
	{
		return value == display_grid || value == display_inline_grid;
	}

#define  font_size_strings		"xx-small;x-small;small;medium;large;x-large;xx-large;smaller;larger"

	enum font_size
	{
		font_size_xx_small,
		font_size_x_small,
		font_size_small,
		font_size_medium,
		font_size_large,
		font_size_x_large,
		font_size_xx_large,
		font_size_smaller,
		font_size_larger,
	};

#define line_height_strings "normal"

	enum line_height
	{
		line_height_normal
	};

#define  font_style_strings		"normal;italic;oblique"

	enum font_style
	{
		font_style_normal,
		font_style_italic,
		font_style_oblique
	};

#define  font_system_family_name_strings		"caption;icon;menu;message-box;small-caption;status-bar"

#define  font_variant_strings		"normal;small-caps"

	enum font_variant
	{
		font_variant_normal,
		font_variant_small_caps
	};

#define  font_weight_strings	"normal;bold;bolder;lighter"

	enum font_weight
	{
		font_weight_normal,
		font_weight_bold,
		font_weight_bolder,
		font_weight_lighter,
	};

#define  list_style_type_strings	"none;circle;disc;square;armenian;cjk-ideographic;decimal;decimal-leading-zero;georgian;hebrew;hiragana;hiragana-iroha;katakana;katakana-iroha;lower-alpha;lower-greek;lower-latin;lower-roman;upper-alpha;upper-latin;upper-roman"

	enum list_style_type
	{
		list_style_type_none,
		list_style_type_circle,
		list_style_type_disc,
		list_style_type_square,
		list_style_type_armenian,
		list_style_type_cjk_ideographic,
		list_style_type_decimal,
		list_style_type_decimal_leading_zero,
		list_style_type_georgian,
		list_style_type_hebrew,
		list_style_type_hiragana,
		list_style_type_hiragana_iroha,
		list_style_type_katakana,
		list_style_type_katakana_iroha,
		list_style_type_lower_alpha,
		list_style_type_lower_greek,
		list_style_type_lower_latin,
		list_style_type_lower_roman,
		list_style_type_upper_alpha,
		list_style_type_upper_latin,
		list_style_type_upper_roman,
	};

#define  list_style_position_strings	"inside;outside"

	enum list_style_position
	{
		list_style_position_inside,
		list_style_position_outside
	};

#define  vertical_align_strings	"baseline;sub;super;top;text-top;middle;bottom;text-bottom"

	enum vertical_align
	{
		va_baseline,
		va_sub,
		va_super,
		va_top,
		va_text_top,
		va_middle,
		va_bottom,
		va_text_bottom
	};

#define  border_width_strings	"thin;medium;thick"

	enum border_width
	{
		border_width_thin,
		border_width_medium,
		border_width_thick
	};

	const float border_width_thin_value = 1;
	const float border_width_medium_value = 3;
	const float border_width_thick_value = 5;
	const float border_width_values[] = { border_width_thin_value, border_width_medium_value, border_width_thick_value };

#define  border_style_strings	"none;hidden;dotted;dashed;solid;double;groove;ridge;inset;outset"

	enum border_style
	{
		border_style_none,
		border_style_hidden,
		border_style_dotted,
		border_style_dashed,
		border_style_solid,
		border_style_double,
		border_style_groove,
		border_style_ridge,
		border_style_inset,
		border_style_outset
	};

#define  element_float_strings	"none;left;right;inline-start;inline-end"

	enum element_float
	{
		float_none,
		float_left,
		float_right,
		float_inline_start,
		float_inline_end,
	};

#define  element_clear_strings	"none;left;right;both;inline-start;inline-end"

	enum element_clear
	{
		clear_none,
		clear_left,
		clear_right,
		clear_both,
		clear_inline_start,
		clear_inline_end,
	};

#define  css_units_strings	"none;%;in;cm;mm;em;ex;pt;pc;px;vw;vh;vmin;vmax;rem;ch;q;cap;ic;lh;rex;rcap;rch;ric;rlh;vi;vb;svw;svh;svi;svb;svmin;svmax;lvw;lvh;lvi;lvb;lvmin;lvmax;dvw;dvh;dvi;dvb;dvmin;dvmax;cqw;cqh;cqi;cqb;cqmin;cqmax"

	enum css_units : byte // see css_length
	{
		css_units_none,
		css_units_percentage,
		css_units_in,
		css_units_cm,
		css_units_mm,
		css_units_em,
		css_units_ex,
		css_units_pt,
		css_units_pc,
		css_units_px,
		css_units_vw,
		css_units_vh,
		css_units_vmin,
		css_units_vmax,
		css_units_rem,
		css_units_ch,
		css_units_q,
		css_units_cap,
		css_units_ic,
		css_units_lh,
		css_units_rex,
		css_units_rcap,
		css_units_rch,
		css_units_ric,
		css_units_rlh,
		css_units_vi,
		css_units_vb,
		css_units_svw,
		css_units_svh,
		css_units_svi,
		css_units_svb,
		css_units_svmin,
		css_units_svmax,
		css_units_lvw,
		css_units_lvh,
		css_units_lvi,
		css_units_lvb,
		css_units_lvmin,
		css_units_lvmax,
		css_units_dvw,
		css_units_dvh,
		css_units_dvi,
		css_units_dvb,
		css_units_dvmin,
		css_units_dvmax,
		css_units_cqw,
		css_units_cqh,
		css_units_cqi,
		css_units_cqb,
		css_units_cqmin,
		css_units_cqmax,
	};

#define  background_attachment_strings	"scroll;fixed;local"

	enum background_attachment
	{
		background_attachment_scroll,
		background_attachment_fixed,
		background_attachment_local
	};

	enum class background_repeat
	{
		repeat,
		no_repeat,
		round,
		space,
	};

	struct background_repeat_pair
	{
		background_repeat x = background_repeat::repeat;
		background_repeat y = background_repeat::repeat;
	};

// https://drafts.csswg.org/css-box-4/#typedef-visual-box
#define  background_box_strings	"border-box;padding-box;content-box"

	enum background_box
	{
		background_box_border,
		background_box_padding,
		background_box_content,
		background_box_margin,
		background_box_no_clip
	};

#define  background_position_strings				"left;right;top;bottom;center"
	const float background_position_percentages[] =	 {0,   100,  0,  100,    50};

	enum background_position
	{
		background_position_left,
		background_position_right,
		background_position_top,
		background_position_bottom,
		background_position_center,
	};

#define element_position_strings	"static;relative;absolute;fixed;sticky"

	enum element_position
	{
		element_position_static,
		element_position_relative,
		element_position_absolute,
		element_position_fixed,
		element_position_sticky,
	};

#define text_align_strings		"left;right;center;justify;auto;match-parent"

	enum text_align
	{
		text_align_left,
		text_align_right,
		text_align_center,
		text_align_justify,
		text_align_auto,
		text_align_match_parent
	};

	enum text_autospace
	{
		text_autospace_alpha = 1 << 0,
		text_autospace_numeric = 1 << 1,
		text_autospace_punctuation = 1 << 2,
		text_autospace_replace = 1 << 3,
		text_autospace_normal = text_autospace_alpha | text_autospace_numeric,
	};

	enum text_spacing_trim
	{
		text_spacing_start = 1 << 0,
		text_spacing_first = 1 << 1,
		text_spacing_adjacent = 1 << 2,
		text_spacing_end = 1 << 3,
		text_spacing_allow_end = 1 << 4,
		text_spacing_all = 1 << 5,
		text_spacing_normal = text_spacing_adjacent | text_spacing_allow_end,
	};

	enum class punctuation_trim { none, left, right, center };

#define text_justify_strings "auto;none;inter-word;inter-character;ruby"

	enum text_justify
	{
		text_justify_auto,
		text_justify_none,
		text_justify_inter_word,
		text_justify_inter_character,
		text_justify_ruby
	};

	struct text_justify_info
	{
		int count = 0;
		char32_t first = 0;
		char32_t last = 0;
	};

	struct text_justify_adjustment
	{
		text_justify method = text_justify_none;
		pixel_t spacing = 0;
	};

	enum text_transform
	{
		text_transform_none = 0,
		text_transform_capitalize = 1 << 0,
		text_transform_uppercase = 1 << 1,
		text_transform_lowercase = 1 << 2,
		text_transform_case_mask = text_transform_capitalize | text_transform_uppercase | text_transform_lowercase,
		text_transform_full_width = 1 << 3,
		text_transform_full_size_kana = 1 << 4,
		text_transform_math_auto = 1 << 5
	};

#define white_space_collapse_strings "collapse;discard;preserve;preserve-breaks;preserve-spaces;break-spaces"

	enum white_space_collapse
	{
		white_space_collapse_spaces,
		white_space_discard,
		white_space_preserve,
		white_space_preserve_breaks,
		white_space_preserve_spaces,
		white_space_break_spaces
	};

#define text_wrap_mode_strings "wrap;nowrap"

	enum text_wrap_mode { text_wrap_wrap, text_wrap_nowrap };

	inline bool is_css_space_separator(char32_t c)
	{
		return c == 0x1680 || (c >= 0x2000 && c <= 0x200a) ||
			c == 0x202f || c == 0x205f || c == 0x3000;
	}

	inline bool is_unicode_forced_break(char32_t c)
	{
		return c == 0x000b || c == 0x000c || c == 0x0085 || c == 0x2028 || c == 0x2029;
	}

#define resize_strings "none;both;horizontal;vertical;inline;block"
	enum resize_mode { resize_none, resize_both, resize_horizontal, resize_vertical, resize_inline, resize_block };

#define overflow_strings		"visible;hidden;scroll;auto;no-display;no-content;clip"

	enum overflow
	{
		overflow_visible,
		overflow_hidden,
		overflow_scroll,
		overflow_auto,
		overflow_no_display,
		overflow_no_content,
		overflow_clip
	};

	inline bool is_scrollable_overflow(overflow value)
	{
		return value == overflow_hidden || value == overflow_scroll || value == overflow_auto;
	}

	inline bool is_interactive_overflow(overflow value)
	{
		return value == overflow_scroll || value == overflow_auto;
	}

	enum class overscroll_behavior { automatic, contain, none, chain };
	inline constexpr const char* overscroll_behavior_keywords = "auto;contain;none;chain";
	enum class scrollbar_gutter { automatic, stable, both_edges };
	enum containment_flag {
		contain_layout = 1, contain_paint = 2, contain_style = 4, contain_size = 8, contain_inline_size = 16,
	};
	enum class content_visibility { visible, hidden, automatic };
	enum class container_type { normal, inline_size, size };

	enum class scroll_snap_align { none, start, end, center };
	struct css_scroll_snap_align {
		scroll_snap_align block = scroll_snap_align::none;
		scroll_snap_align inline_axis = scroll_snap_align::none;
		bool empty() const { return block == scroll_snap_align::none && inline_axis == scroll_snap_align::none; }
	};

	struct css_scroll_snap_type {
		bool x = false;
		bool y = false;
		bool mandatory = false;
		bool enabled() const { return x || y; }
	};

	inline bool blocks_scroll_chain(overscroll_behavior value)
	{
		return value == overscroll_behavior::contain || value == overscroll_behavior::none;
	}

#define background_size_strings		"auto;cover;contain"

	enum background_size
	{
		background_size_auto,	// must be first, see parse_bg_size
		background_size_cover,
		background_size_contain,
	};

#define visibility_strings			"visible;hidden;collapse"

	enum visibility
	{
		visibility_visible,
		visibility_hidden,
		visibility_collapse,
	};

#define border_collapse_strings		"collapse;separate"

	enum border_collapse
	{
		border_collapse_collapse,
		border_collapse_separate,
	};

#define content_property_string		"none;normal;open-quote;close-quote;no-open-quote;no-close-quote"

	enum content_property
	{
		content_property_none,
		content_property_normal,
		content_property_open_quote,
		content_property_close_quote,
		content_property_no_open_quote,
		content_property_no_close_quote,
	};

#define appearance_strings      "none;auto;menulist-button;textfield;button;checkbox;listbox;menulist;meter;progress-bar;push-button;radio;searchfield;slider-horizontal;square-button;textarea"

	enum appearance
	{
		appearance_none,
		appearance_auto,
		appearance_menulist_button,
		appearance_textfield,
		appearance_button,
		appearance_checkbox,
		appearance_listbox,
		appearance_menulist,
		appearance_meter,
		appearance_progress_bar,
		appearance_push_button,
		appearance_radio,
		appearance_searchfield,
		appearance_slider_horizontal,
		appearance_square_button,
		appearance_textarea,
	};

#define box_sizing_strings		"content-box;border-box"

	enum box_sizing
	{
		box_sizing_content_box,
		box_sizing_border_box,
	};

// https://drafts.csswg.org/mediaqueries/#media-types
// User agents must recognize the following media types as valid, but must make them match nothing.
#define deprecated_media_type_strings	"tty;tv;projection;handheld;braille;embossed;aural;speech"
#define media_type_strings				"all;print;screen;" deprecated_media_type_strings

	enum media_type
	{
		media_type_unknown,
		media_type_all,
		media_type_print,
		media_type_screen,
		media_type_first_deprecated
	};

	enum class color_scheme : uint8_t { light, dark };
	enum color_scheme_support { scheme_light = 1, scheme_dark = 2, scheme_dark_first = 4 };

	struct media_features
	{
		media_type	type;
		pixel_t		width;			// (pixels) For continuous media, this is the width of the viewport including the size of a rendered scroll bar (if any). For paged media, this is the width of the page box.
		pixel_t		height;			// (pixels) The height of the targeted display area of the output device. For continuous media, this is the height of the viewport including the size of a rendered scroll bar (if any). For paged media, this is the height of the page box.
		pixel_t		device_width;	// (pixels) The width of the rendering surface of the output device. For continuous media, this is the width of the screen. For paged media, this is the width of the page sheet size.
		pixel_t		device_height;	// (pixels) The height of the rendering surface of the output device. For continuous media, this is the height of the screen. For paged media, this is the height of the page sheet size.
		int			color;			// The number of bits per color component of the output device. If the device is not a color device, the value is zero.
		int			color_index;	// The number of entries in the color lookup table of the output device. If the device does not use a color lookup table, the value is zero.
		int			monochrome;		// The number of bits per pixel in a monochrome frame buffer. If the device is not a monochrome device, the output device value will be 0.
		pixel_t		resolution;		// The resolution of the output device (in DPI)
		color_scheme preferred_color_scheme = color_scheme::light;
		bool prefers_reduced_motion = false;
		bool prefers_contrast_more = false;
		bool hover = false;
		bool any_hover = false;
		int primary_pointer = 0;
		bool any_pointer_fine = false;
		bool any_pointer_coarse = false;

		media_features()
		{
			type = media_type_unknown;
			width = 0;
			height = 0;
			device_width = 0;
			device_height = 0;
			color = 0;
			color_index = 0;
			monochrome = 0;
			resolution = 0;
		}
	};

#define flex_direction_strings		"row;row-reverse;column;column-reverse"

	enum flex_direction
	{
		flex_direction_row,
		flex_direction_row_reverse,
		flex_direction_column,
		flex_direction_column_reverse
	};

#define flex_wrap_strings		"nowrap;wrap;wrap-reverse"

	enum flex_wrap
	{
		flex_wrap_nowrap,
		flex_wrap_wrap,
		flex_wrap_wrap_reverse
	};

#define flex_justify_content_strings		"normal;flex-start;flex-end;center;space-between;space-around;start;end;left;right;space-evenly;stretch"

	enum flex_justify_content
	{
		flex_justify_content_normal,
		flex_justify_content_flex_start,
		flex_justify_content_flex_end,
		flex_justify_content_center,
		flex_justify_content_space_between,
		flex_justify_content_space_around,
		flex_justify_content_start,
		flex_justify_content_end,
		flex_justify_content_left,
		flex_justify_content_right,
		flex_justify_content_space_evenly,
		flex_justify_content_stretch,
	};

#define self_position_strings		"center;start;end;self-start;self-end;flex-start;flex-end"
#define flex_align_items_strings	"auto;normal;stretch;baseline;" self_position_strings

	enum flex_align_items
	{
		flex_align_items_auto, // used for align-self property only
		flex_align_items_normal,
		flex_align_items_stretch,
		flex_align_items_baseline,

		flex_align_items_center,
		flex_align_items_start,
		flex_align_items_end,
		flex_align_items_self_start,
		flex_align_items_self_end,
		flex_align_items_flex_start,
		flex_align_items_flex_end,

		flex_align_items_first = 0x100,
		flex_align_items_last = 0x200,
		flex_align_items_unsafe = 0x400,
		flex_align_items_safe  = 0x800,
	};

#define flex_align_content_strings		"flex-start;start;flex-end;end;center;space-between;space-around;stretch"

	enum flex_align_content
	{
		flex_align_content_flex_start,
		flex_align_content_start,
		flex_align_content_flex_end,
		flex_align_content_end,
		flex_align_content_center,
		flex_align_content_space_between,
		flex_align_content_space_around,
		flex_align_content_stretch
	};

#define flex_basis_strings		"auto;content;fit-content;min-content;max-content"

	enum flex_basis
	{
		flex_basis_auto,
		flex_basis_content,
		flex_basis_fit_content,
		flex_basis_min_content,
		flex_basis_max_content,
	};

#define caption_side_strings		"top;bottom"

	enum caption_side
	{
		caption_side_top,
		caption_side_bottom
	};
} // namespace litehtml
