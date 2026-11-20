#pragma once

#include "css_counter_style.h"
#include "css_font_face.h"
#include "css_font_feature_values.h"
#include "css_gradient.h"

#include "css_length.h"
#include "css_string_id.h"
#include "css_web_color.h"
#include <optional>

namespace litehtml {
	enum class css_user_select { automatic, text, none, all };
	enum css_will_change_hint {
		will_change_transform = 1 << 0,
		will_change_backface = 1 << 1,
		will_change_containment = 1 << 2,
		will_change_filter = 1 << 3,
		will_change_backdrop = 1 << 4,
		will_change_isolation = 1 << 5,
		will_change_position = 1 << 6,
		will_change_z_index = 1 << 7,
		will_change_content_visibility = 1 << 8,
	};

	inline constexpr int outline_style_auto = -1;

	enum class scrollbar_width { automatic, thin, none };

	struct css_scrollbar_colors {
		web_color thumb;
		web_color track;
	};

	enum class css_filter_type {
		grayscale = 1, sepia, saturate, hue_rotate, invert, opacity, brightness, contrast, blur, drop_shadow,
	};

	struct css_filter {
		css_filter_type type;
		double amount = 1.0;
		css_length x;
		css_length y;
		css_length blur;
		web_color color = web_color::current_color;
	};
	using css_filter_list = std::vector<css_filter>;

	struct css_clip_margin {
		background_box box = background_box_padding;
		css_length offset;
	};

	struct css_contain_intrinsic_size {
		std::optional<css_length> length;
		bool remember = false;
	};

	struct css_outline {
		css_length width = border_width_medium_value;
		css_length offset;
		int style = border_style_none;
		std::optional<web_color> color;
	};
	#define text_wrap_style_strings "auto;balance;stable;pretty;avoid-short-last-line"
	enum text_wrap_style { text_wrap_auto, text_wrap_balance, text_wrap_stable, text_wrap_pretty, text_wrap_avoid_short_last_line };
	#define word_break_strings "normal;keep-all;break-word;break-all;manual"
	enum word_break { word_break_normal, word_break_keep_all, word_break_break_word, word_break_break_all, word_break_manual };
	#define line_break_strings "auto;loose;normal;strict;anywhere"
	enum line_break { line_break_auto, line_break_loose, line_break_normal, line_break_strict, line_break_anywhere };
	#define overflow_wrap_strings "normal;break-word;anywhere"
	enum overflow_wrap { overflow_wrap_normal, overflow_wrap_break_word, overflow_wrap_anywhere };
	#define hyphens_strings "manual;none;auto"
	enum hyphens { hyphens_manual, hyphens_none, hyphens_auto };
	#define hyphenate_limit_last_strings "none;always;column;page;spread"
	enum hyphenate_limit_last { hyphenate_last_none, hyphenate_last_always, hyphenate_last_column, hyphenate_last_page, hyphenate_last_spread };
	inline constexpr char default_hyphenate_character[] = "\xe2\x80\x90";
	struct css_hyphenation_chars {
		int word = 5;
		int before = 2;
		int after = 2;

		bool allows(int64_t prefix, int64_t suffix) const {
			return prefix >= before && suffix >= after && prefix + suffix >= word;
		}
	};
	struct css_hyphenation_limits {
		css_length zone;
		int lines = -1;
		hyphenate_limit_last last = hyphenate_last_none;

		bool allows_line(size_t preceding) const {
			return lines < 0 || preceding < static_cast<size_t>(lines);
		}
		static bool allows_hyphen_in_zone(pixel_t threshold, pixel_t ordinary_slack) {
			return threshold <= 0 || ordinary_slack < 0 || ordinary_slack > threshold;
		}
	};

	enum white_space_trim {
		white_space_trim_none = 0,
		white_space_discard_before = 1 << 0,
		white_space_discard_after = 1 << 1,
		white_space_discard_inner = 1 << 2,
	};

	struct css_text_indent {
		css_length length;
		bool hanging = false;
		bool each_line = false;
	};

	struct css_font_style {
		font_style kind = font_style_normal;
		float angle = 0.0f;
	};
	enum css_font_kerning { font_kerning_auto, font_kerning_normal, font_kerning_none };
	enum css_font_optical_sizing { font_optical_sizing_auto, font_optical_sizing_none };

	struct css_font_size_adjust {
		enum class Metric { ex_height, cap_height, ch_width, ic_width, ic_height };
		enum class Mode { none, number, from_font };
		Metric metric = Metric::ex_height;
		Mode mode = Mode::none;
		double value = 0.0;

		bool operator==(const css_font_size_adjust& other) const {
			return metric == other.metric && mode == other.mode && value == other.value;
		}
	};

	struct css_font_family {
		string name;
		bool generic = false;
	};
	using css_font_families = std::vector<css_font_family>;

	bool parse_font_families(const css_token_vector& p_tokens, css_font_families& r_families);

	enum class css_content_kind { text, attribute, image, counter, counters, marker_counter, open_quote, close_quote, no_open_quote, no_close_quote };

	struct css_content_item {
		css_content_kind kind;
		string value;
		string fallback_or_separator;
		css_counter_style_reference counter_style{};
		css_image_ptr image_value;
	};

	struct css_generated_content {
		bool normal = true;
		std::vector<css_content_item> items;
		string base_url;
		std::vector<css_content_item> alternative;

		bool is_single_image() const { return items.size() == 1 && items.front().kind == css_content_kind::image; }
	};

	struct css_list_marker_type {
		css_counter_style_reference counter{ "disc", {} };
		std::optional<string> text;
	};

	enum class css_quotes_kind { automatic, none, match_parent, pairs };

	inline string_id marker_tag() {
		static const auto marker = _id("-tag-marker");
		return marker;
	}

	inline bool is_generated_tag(string_id p_tag) {
		static const auto backdrop = _id("-tag-backdrop");
		return p_tag == __tag_before_ || p_tag == __tag_after_ || p_tag == marker_tag() || p_tag == backdrop;
	}

	struct css_quotes {
		css_quotes_kind kind = css_quotes_kind::automatic;
		string_vector pairs;
	};

	struct css_counter_change {
		string_id name;
		int value;
	};
	using css_counter_changes = std::vector<css_counter_change>;

	enum class alignment_keyword {
		normal, automatic, stretch,
		start, end, center, self_start, self_end, flex_start, flex_end, left, right,
		space_between, space_around, space_evenly, first_baseline, last_baseline
	};

	enum class alignment_overflow { automatic, safe, unsafe };

	struct css_alignment {
		alignment_keyword keyword = alignment_keyword::normal;
		alignment_overflow overflow = alignment_overflow::automatic;
	};

	css_alignment initial_alignment(string_id p_property);
	bool parse_alignment(string_id p_property, const css_token_vector& p_tokens, css_alignment& r_alignment);
	bool parse_place_alignment(string_id p_align_property, string_id p_justify_property,
		const css_token_vector& p_tokens, css_alignment& r_align, css_alignment& r_justify);
	flex_align_items to_flex_item_alignment(css_alignment p_alignment);
	flex_align_content to_flex_align_content(css_alignment p_alignment);
	flex_justify_content to_flex_justify_content(css_alignment p_alignment);

	enum class css_source_kind {
		none,
		inline_style,
		style_element,
		stylesheet,
		local_stylesheet
	};
	enum class css_diagnostic_kind {
		unknown_property,
		unsupported_value,
		not_rendered
	};

	struct css_source {
		css_source_kind kind = css_source_kind::none;
		string path;
	};

	struct css_diagnostic {
		css_diagnostic_kind kind;
		string property;
		string value;
		css_source source;
	};

	struct text_decoration {
		int line = text_decoration_line_none;
		text_decoration_style style = text_decoration_style_solid;
		css_length thickness = css_length::predef_value(text_decoration_thickness_auto);
		web_color color = web_color::current_color;
	};

	enum text_underline_position {
		underline_auto = 0,
		underline_from_font = 1,
		underline_under = 2,
		underline_left = 4,
		underline_right = 8,
	};

	struct css_underline {
		int position = underline_auto;
		std::optional<pixel_t> offset;
		pixel_t font_position = 0;
	};

	bool parse_decoration_line(const css_token_vector& p_tokens, int& r_line);
	bool parse_decoration(const css_token_vector& p_tokens, document_container* p_container, text_decoration& r_decoration);
}
