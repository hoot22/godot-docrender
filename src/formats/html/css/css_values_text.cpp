// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_value_parser.h"
#include <litehtml/html.h>
#include "css_font_face.h"
#include "formats/html/css/css_math.h"

#include <litehtml/css_parser.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>

namespace litehtml {

static bool parse_font_weight(const css_token& token, css_length& weight);

namespace {
	std::optional<int> parse_autospace(const css_token_vector& value) {
		if (value.empty()) return {};
		if (value.size() == 1 && is_one_of(value.front().ident(), "normal", "auto", "no-autospace")) {
			return value.front().ident() == "no-autospace" ? 0 : int(text_autospace_normal);
		}
		int flags = 0;
		bool explicit_mode = false;
		for (const auto& token : value) {
			const auto keyword = token.ident();
			if (is_one_of(keyword, "insert", "replace")) {
				if (explicit_mode) return {};
				explicit_mode = true;
				if (keyword == "replace") flags |= text_autospace_replace;
				continue;
			}
			const int flag = keyword == "ideograph-alpha" ? text_autospace_alpha :
				keyword == "ideograph-numeric" ? text_autospace_numeric :
				keyword == "punctuation" ? text_autospace_punctuation : 0;
			if (!flag || (flags & flag)) return {};
			flags |= flag;
		}
		if (!(flags & (text_autospace_normal | text_autospace_punctuation))) flags |= text_autospace_normal;
		return flags;
	}

	bool is_spacing_trim_keyword(const string& keyword) {
		return is_one_of(keyword, "trim-start", "space-start", "space-first", "trim-adjacent", "space-adjacent",
			"trim-end", "space-end", "allow-end", "trim-both", "space-all", "trim-all");
	}

	std::optional<int> parse_spacing_trim(const css_token_vector& value) {
		if (value.empty()) return {};
		if (value.size() == 1) {
			const auto keyword = value.front().ident();
			if (keyword == "normal" || keyword == "auto") return text_spacing_normal;
			if (keyword == "space-all") return 0;
			if (keyword == "trim-both") return text_spacing_start | text_spacing_adjacent | text_spacing_end;
			if (keyword == "trim-all") return text_spacing_all;
		}
		int flags = text_spacing_start | text_spacing_normal;
		int seen = 0;
		for (const auto& token : value) {
			const auto keyword = token.ident();
			int group = 0;
			int behavior = 0;
			if (is_one_of(keyword, "trim-start", "space-start", "space-first")) {
				group = text_spacing_start | text_spacing_first;
				behavior = keyword == "trim-start" ? text_spacing_start : keyword == "space-first" ? text_spacing_first : 0;
			}
			else if (is_one_of(keyword, "trim-adjacent", "space-adjacent")) {
				group = text_spacing_adjacent;
				behavior = keyword == "trim-adjacent" ? text_spacing_adjacent : 0;
			}
			else if (is_one_of(keyword, "trim-end", "space-end", "allow-end")) {
				group = text_spacing_end | text_spacing_allow_end;
				behavior = keyword == "trim-end" ? text_spacing_end : keyword == "allow-end" ? text_spacing_allow_end : 0;
			}
			if (!group || (seen & group)) return {};
			seen |= group;
			flags = (flags & ~group) | behavior;
		}
		return flags;
	}

	std::optional<int> parse_hyphenation_minimum(const css_token& token) {
		if (const auto integer = parse_counter_integer(token); integer && *integer >= 0) return integer;
		if (const auto number = evaluate_css_scalar_math(token, css_math_scalar::number)) {
			const auto value = std::clamp(std::floor(*number + 0.5), 0.0, double(std::numeric_limits<int>::max()));
			return static_cast<int>(value);
		}
		return std::nullopt;
	}

	std::optional<css_hyphenation_chars> parse_hyphenation_characters(const css_token_vector& tokens) {
		if (tokens.empty() || tokens.size() > 3) return std::nullopt;
		css_hyphenation_chars limits;
		const auto parse = [](const css_token& token, int& field) {
			if (token.ident() == "auto") return true;
			const auto minimum = parse_hyphenation_minimum(token);
			if (!minimum) return false;
			field = *minimum;
			return true;
		};
		if (!parse(tokens.front(), limits.word)) return std::nullopt;
		if (tokens.size() >= 2 && !parse(tokens[1], limits.before)) return std::nullopt;
		if (tokens.size() == 2) limits.after = limits.before;
		if (tokens.size() == 3 && !parse(tokens[2], limits.after)) return std::nullopt;
		return limits;
	}

	std::optional<std::pair<int, int>> parse_text_wrap(const css_token_vector& tokens) {
		if (tokens.empty() || tokens.size() > 2) return std::nullopt;
		int mode = -1;
		int style = -1;
		for (const auto& token : tokens) {
			const auto value = token.ident();
			const int parsed_mode = value_index(value, text_wrap_mode_strings);
			const int parsed_style = value_index(value, text_wrap_style_strings);
			if (parsed_mode >= 0 && mode < 0) mode = parsed_mode;
			else if (parsed_style >= 0 && style < 0) style = parsed_style;
			else return std::nullopt;
		}
		return std::pair{ mode < 0 ? text_wrap_wrap : mode, style < 0 ? text_wrap_auto : style };
	}

	std::optional<int> trim_flag(const string& keyword) {
		if (keyword == "none") return white_space_trim_none;
		if (keyword == "discard-before") return white_space_discard_before;
		if (keyword == "discard-after") return white_space_discard_after;
		if (keyword == "discard-inner") return white_space_discard_inner;
		return std::nullopt;
	}

	std::optional<int> parse_white_space_trim(css_token_vector::const_iterator& token, css_token_vector::const_iterator end) {
		if (token == end) return std::nullopt;
		const auto first = trim_flag(token->ident());
		if (!first) return std::nullopt;
		++token;
		int flags = *first;
		while (token != end) {
			const auto flag = trim_flag(token->ident());
			if (!flag) break;
			if (!flags || !*flag || (flags & *flag)) return std::nullopt;
			flags |= *flag;
			++token;
		}
		return flags;
	}

	struct white_space_value {
		int collapse = white_space_collapse_spaces;
		int wrap = text_wrap_wrap;
		int trim = white_space_trim_none;
	};

	std::optional<white_space_value> parse_white_space(const css_token_vector& tokens) {
		if (tokens.size() == 1) {
			const auto keyword = tokens.front().ident();
			if (keyword == "normal") return white_space_value{};
			if (keyword == "pre") return white_space_value{ white_space_preserve, text_wrap_nowrap };
			if (keyword == "pre-wrap") return white_space_value{ white_space_preserve, text_wrap_wrap };
			if (keyword == "pre-line") return white_space_value{ white_space_preserve_breaks, text_wrap_wrap };
		}
		if (tokens.empty() || tokens.size() > 5) return std::nullopt;
		white_space_value result;
		bool has_collapse = false;
		bool has_wrap = false;
		bool has_trim = false;
		for (auto token = tokens.begin(); token != tokens.end();) {
			const auto keyword = token->ident();
			const int collapse = value_index(keyword, white_space_collapse_strings);
			const int wrap = value_index(keyword, text_wrap_mode_strings);
			if (collapse >= 0 && !has_collapse) {
				result.collapse = collapse;
				has_collapse = true;
				++token;
			}
			else if (wrap >= 0 && !has_wrap) {
				result.wrap = wrap;
				has_wrap = true;
				++token;
			}
			else {
				if (has_trim) return std::nullopt;
				const auto trim = parse_white_space_trim(token, tokens.end());
				if (!trim) return std::nullopt;
				result.trim = *trim;
				has_trim = true;
			}
		}
		return result;
	}

	std::optional<css_text_indent> parse_text_indent(const css_token_vector& tokens) {
		css_text_indent result;
		bool has_length = false;
		for (const auto& token : tokens) {
			const auto keyword = token.ident();
			if (keyword == "hanging" && !result.hanging) result.hanging = true;
			else if (keyword == "each-line" && !result.each_line) result.each_line = true;
			else if (!has_length && result.length.from_token(token, f_length_percentage | f_math)) has_length = true;
			else return std::nullopt;
		}
		return has_length ? std::optional<css_text_indent>(result) : std::nullopt;
	}

	std::optional<int> parse_text_alignment(const string& keyword) {
		if (keyword == "start") return text_align_left;
		if (keyword == "end") return text_align_right;
		const int index = value_index(keyword, text_align_strings);
		return index >= 0 ? std::optional<int>(index) : std::nullopt;
	}

	std::optional<int> parse_text_justification(const css_token_vector& tokens) {
		int method = text_justify_auto;
		bool has_method = false;
		bool no_compress = false;
		for (const auto& token : tokens) {
			const auto keyword = token.ident();
			if (keyword == "no-compress" && !no_compress) {
				no_compress = true;
				continue;
			}
			if (has_method) return std::nullopt;
			method = keyword == "distribute" ? text_justify_inter_character : value_index(keyword, text_justify_strings);
			if (method < 0) return std::nullopt;
			has_method = true;
		}
		if (!has_method && !no_compress) return std::nullopt;
		// This layout expands justification opportunities and leaves authored spacing uncompressed.
		return method;
	}

	std::optional<int> parse_text_transform(const css_token_vector& tokens) {
		if (tokens.size() == 1) {
			if (tokens.front().ident() == "none") return text_transform_none;
			if (tokens.front().ident() == "math-auto") return text_transform_math_auto;
		}
		int result = 0;
		for (const auto& token : tokens) {
			const auto keyword = token.ident();
			int flag;
			if (keyword == "capitalize") flag = text_transform_capitalize;
			else if (keyword == "uppercase") flag = text_transform_uppercase;
			else if (keyword == "lowercase") flag = text_transform_lowercase;
			else if (keyword == "full-width") flag = text_transform_full_width;
			else if (keyword == "full-size-kana") flag = text_transform_full_size_kana;
			else return std::nullopt;
			const int conflicts = (flag & text_transform_case_mask) ? text_transform_case_mask : flag;
			if (result & conflicts) return std::nullopt;
			result |= flag;
		}
		return result ? std::optional<int>(result) : std::nullopt;
	}

	enum variant_group { ligatures, numeric, east_asian, caps, variant_group_count };
	const std::array<string_id, variant_group_count>& variant_properties() {
		static const std::array<string_id, variant_group_count> properties = {
			_id("font-variant-ligatures"), _id("font-variant-numeric"), _id("font-variant-east-asian"), _id("font-variant-caps"),
		};
		return properties;
	}

	constexpr uint32_t font_tag(std::string_view tag) {
		uint32_t result = 0;
		for (const unsigned char character : tag) result = (result << 8) | character;
		return result;
	}

	struct variant_option {
		std::string_view keyword;
		variant_group group;
		unsigned category;
		uint32_t tag;
		uint32_t second_tag = 0;
		uint32_t enabled = 1;
	};

	using variant_settings = std::array<css_font_features, variant_group_count>;
	std::optional<variant_settings> parse_variant_settings(const css_token_vector& tokens, size_t group,
		css_token_vector& alternate_tokens) {
		// Each category accepts at most one keyword. Different categories can occur in any order.
		static constexpr variant_option options[] = {
			{ "common-ligatures", ligatures, 1, font_tag("liga"), font_tag("clig") },
			{ "no-common-ligatures", ligatures, 1, font_tag("liga"), font_tag("clig"), 0 },
			{ "discretionary-ligatures", ligatures, 2, font_tag("dlig") },
			{ "no-discretionary-ligatures", ligatures, 2, font_tag("dlig"), 0, 0 },
			{ "historical-ligatures", ligatures, 4, font_tag("hlig") },
			{ "no-historical-ligatures", ligatures, 4, font_tag("hlig"), 0, 0 },
			{ "contextual", ligatures, 8, font_tag("calt") },
			{ "no-contextual", ligatures, 8, font_tag("calt"), 0, 0 },
			{ "lining-nums", numeric, 1, font_tag("lnum") },
			{ "oldstyle-nums", numeric, 1, font_tag("onum") },
			{ "proportional-nums", numeric, 2, font_tag("pnum") },
			{ "tabular-nums", numeric, 2, font_tag("tnum") },
			{ "diagonal-fractions", numeric, 4, font_tag("frac") },
			{ "stacked-fractions", numeric, 4, font_tag("afrc") },
			{ "ordinal", numeric, 8, font_tag("ordn") },
			{ "slashed-zero", numeric, 16, font_tag("zero") },
			{ "jis78", east_asian, 1, font_tag("jp78") },
			{ "jis83", east_asian, 1, font_tag("jp83") },
			{ "jis90", east_asian, 1, font_tag("jp90") },
			{ "jis04", east_asian, 1, font_tag("jp04") },
			{ "simplified", east_asian, 1, font_tag("smpl") },
			{ "traditional", east_asian, 1, font_tag("trad") },
			{ "full-width", east_asian, 2, font_tag("fwid") },
			{ "proportional-width", east_asian, 2, font_tag("pwid") },
			{ "ruby", east_asian, 4, font_tag("ruby") },
			{ "small-caps", caps, 1, font_tag("smcp") },
			{ "all-small-caps", caps, 1, font_tag("smcp"), font_tag("c2sc") },
			{ "petite-caps", caps, 1, font_tag("pcap") },
			{ "all-petite-caps", caps, 1, font_tag("pcap"), font_tag("c2pc") },
			{ "unicase", caps, 1, font_tag("unic") },
			{ "titling-caps", caps, 1, font_tag("titl") },
		};
		variant_settings result;
		if (tokens.size() == 1 && tokens.front().ident() == "normal") return result;
		if (tokens.size() == 1 && tokens.front().ident() == "none" &&
				(group == variant_group_count || group == ligatures)) {
			for (const auto& option : options) {
				if (option.group != ligatures) continue;
				result[ligatures].emplace(option.tag, 0);
				if (option.second_tag) result[ligatures].emplace(option.second_tag, 0);
			}
			return result;
		}
		std::array<unsigned, variant_group_count> categories{};
		for (const auto& token : tokens) {
			const auto keyword = token.ident();
			if (group == variant_group_count && (token.type == CV_FUNCTION || keyword == "historical-forms")) {
				alternate_tokens.push_back(token);
				continue;
			}
			const auto* option = std::find_if(std::begin(options), std::end(options),
				[&](const variant_option& candidate) { return candidate.keyword == keyword; });
			if (option == std::end(options) || (group != variant_group_count && option->group != group) ||
					(categories[option->group] & option->category)) return std::nullopt;
			categories[option->group] |= option->category;
			auto& features = result[option->group];
			features.emplace(option->tag, option->enabled);
			if (option->second_tag) features.emplace(option->second_tag, option->enabled);
		}
		return result;
	}

	std::optional<int> parse_underline_position(const css_token_vector& tokens) {
		if (tokens.size() == 1) {
			const auto word = tokens.front().ident();
			if (word == "auto") return underline_auto;
			if (word == "from-font") return underline_from_font;
		}
		int position = underline_auto;
		for (const auto& token : tokens) {
			const auto word = token.ident();
			if (word == "under" && !(position & underline_under)) position |= underline_under;
			else if (word == "left" && !(position & (underline_left | underline_right))) position |= underline_left;
			else if (word == "right" && !(position & (underline_left | underline_right))) position |= underline_right;
			else return std::nullopt;
		}
		return position != underline_auto ? std::optional<int>(position) : std::nullopt;
	}

	std::optional<string> parse_emphasis_style(const css_token_vector& tokens) {
		if (tokens.size() == 1 && tokens[0].type == STRING) {
			return tokens[0].str;
		}
		if (tokens.size() == 1 && tokens[0].ident() == "none") return string();
		string fill;
		string shape;
		for (const auto& token : tokens) {
			const auto word = token.ident();
			if (fill.empty() && is_one_of(word, "filled", "open")) {
				fill = word;
			}
			else if (shape.empty() && is_one_of(word, "dot", "circle", "double-circle", "triangle", "sesame")) {
				shape = word;
			}
			else {
				return std::nullopt;
			}
		}
		if (fill.empty() && shape.empty()) return std::nullopt;
		const bool open = fill == "open";
		if (shape == "dot") return open ? "◦" : "•";
		if (shape == "double-circle") return open ? "◎" : "◉";
		if (shape == "triangle") return open ? "△" : "▲";
		if (shape == "sesame") return open ? "﹆" : "﹅";
		return open ? "○" : "●";
	}
}

void css_value_parser::parse_text(string_id name, const css_token_vector& value, bool important, document_container* container)
{
	if (name == _id("hyphenate-character")) {
		if (value.size() != 1) return;
		const auto& token = value.front();
		if (token.type == STRING) add_parsed_property(name, property_value(token.str, important));
		else if (token.ident() == "auto") add_parsed_property(name, property_value(string(default_hyphenate_character), important));
		return;
	}
	if (name == _id("hyphenate-limit-zone")) {
		if (value.size() == 1) add_length_property(name, value.front(), "", f_length_percentage | f_math, important);
		return;
	}
	if (name == _id("hyphenate-limit-lines")) {
		if (value.size() != 1) return;
		const auto& token = value.front();
		if (token.ident() == "no-limit") add_parsed_property(name, property_value(-1, important));
		else if (const auto minimum = parse_hyphenation_minimum(token)) add_parsed_property(name, property_value(*minimum, important));
		return;
	}
	if (name == _id("hyphenate-limit-chars")) {
		if (const auto limits = parse_hyphenation_characters(value)) add_parsed_property(name, property_value(*limits, important));
		return;
	}
	if (name == _id("text-wrap")) {
		if (const auto wrap = parse_text_wrap(value)) {
			add_parsed_property(_id("text-wrap-mode"), property_value(wrap->first, important));
			add_parsed_property(_id("text-wrap-style"), property_value(wrap->second, important));
		}
		return;
	}
	if (name == _white_space_) {
		if (const auto white_space = parse_white_space(value)) {
			add_parsed_property(_id("white-space-collapse"), property_value(white_space->collapse, important));
			add_parsed_property(_id("text-wrap-mode"), property_value(white_space->wrap, important));
			add_parsed_property(_id("white-space-trim"), property_value(white_space->trim, important));
		}
		return;
	}
	if (name == _id("white-space-trim")) {
		auto token = value.begin();
		const auto trim = parse_white_space_trim(token, value.end());
		if (trim && token == value.end()) add_parsed_property(name, property_value(*trim, important));
		return;
	}
	if (name == _text_indent_) {
		if (const auto indent = parse_text_indent(value))
			add_parsed_property(name, property_value(*indent, important));
		return;
	}
	if (name == _id("tab-size")) {
		if (value.size() == 1) add_length_property(name, value.front(), "", f_number | f_length | f_positive | f_math, important);
		return;
	}
	static const auto align_all = _id("text-align-all");
	static const auto align_last = _id("text-align-last");
	if (is_one_of(name, _text_align_, align_all, align_last)) {
		if (value.size() != 1) return;
		const auto keyword = value.front().ident();
		if (name == _text_align_ && keyword == "justify-all") {
			add_parsed_property(align_all, property_value(int(text_align_justify), important));
			add_parsed_property(align_last, property_value(int(text_align_justify), important));
		}
		else if (const auto align = parse_text_alignment(keyword); align && (*align != text_align_auto || name == align_last)) {
			if (name == _text_align_) {
				add_parsed_property(align_all, property_value(*align, important));
				const int last = *align == text_align_match_parent ? *align : text_align_auto;
				add_parsed_property(align_last, property_value(last, important));
			}
			else add_parsed_property(name, property_value(*align, important));
		}
		return;
	}
	if (name == _id("text-autospace")) {
		if (const auto flags = parse_autospace(value)) add_parsed_property(name, property_value(*flags, important));
		return;
	}
	if (name == _id("text-spacing-trim")) {
		if (const auto flags = parse_spacing_trim(value)) add_parsed_property(name, property_value(*flags, important));
		return;
	}
	if (name == _id("text-spacing")) {
		std::optional<int> trim = text_spacing_normal;
		std::optional<int> autospace = text_autospace_normal;
		const auto keyword = value.size() == 1 ? value.front().ident() : string();
		if (keyword == "none") trim = autospace = 0;
		else if (!is_one_of(keyword, "auto", "normal")) {
			css_token_vector trim_tokens;
			css_token_vector autospace_tokens;
			for (const auto& token : value) {
				if (is_one_of(token.ident(), "auto", "normal")) return;
				(is_spacing_trim_keyword(token.ident()) ? trim_tokens : autospace_tokens).push_back(token);
			}
			if (!trim_tokens.empty()) trim = parse_spacing_trim(trim_tokens);
			if (!autospace_tokens.empty()) autospace = parse_autospace(autospace_tokens);
		}
		if (trim && autospace) {
			add_parsed_property(_id("text-spacing-trim"), property_value(*trim, important));
			add_parsed_property(_id("text-autospace"), property_value(*autospace, important));
		}
		return;
	}
	if (name == _id("text-justify")) {
		if (const auto method = parse_text_justification(value))
			add_parsed_property(name, property_value(*method, important));
		return;
	}
	if (name == _text_transform_) {
		if (const auto transform = parse_text_transform(value))
			add_parsed_property(name, property_value(*transform, important));
		return;
	}
	static const auto alternate_property = _id("font-variant-alternates");
	if (name == alternate_property) {
		if (const auto settings = parse_css_font_alternates(value))
			add_parsed_property(name, property_value(*settings, important));
		return;
	}
	const auto& variants = variant_properties();
	const size_t variant = std::find(variants.begin(), variants.end(), name) - variants.begin();
	if (name == _font_variant_ || variant != variants.size()) {
		css_token_vector alternate_tokens;
		if (const auto settings = parse_variant_settings(value, variant, alternate_tokens)) {
			if (name == _font_variant_) {
				if (!alternate_tokens.empty()) {
					const auto alternates = parse_css_font_alternates(alternate_tokens);
					if (!alternates) return;
					add_parsed_property(alternate_property, property_value(*alternates, important));
				}
				for (size_t index = 0; index < variants.size(); ++index)
					add_parsed_property(variants[index], property_value((*settings)[index], important));
			}
			else add_parsed_property(name, property_value((*settings)[variant], important));
		}
		return;
	}
	const css_token val = value.size() == 1 ? value.front() : css_token();
	if (name == _id("letter-spacing") || name == _id("word-spacing")) {
		add_length_property(name, val, "normal", f_length_percentage | f_math, important);
		return;
	}
	if (name == _id("font-width")) {
		if (const auto width = parse_css_font_width(val))
			add_parsed_property(name, property_value(*width, important));
		return;
	}
	if (name == _id("font-feature-settings")) {
		if (const auto settings = parse_css_font_features(value))
			add_parsed_property(name, property_value(*settings, important));
		return;
	}
	if (name == _id("font-variation-settings")) {
		if (const auto settings = parse_css_font_variations(value))
			add_parsed_property(name, property_value(*settings, important));
		return;
	}
	if (name == _id("font-size-adjust")) {
		if (value.empty() || value.size() > 2) return;
		css_font_size_adjust adjust;
		if (value.size() == 1 && val.ident() == "none") {
			add_parsed_property(name, property_value(adjust, important));
			return;
		}
		if (value.size() == 2) {
			const int metric = value_index(value.front().ident(), "ex-height;cap-height;ch-width;ic-width;ic-height", -1);
			if (metric < 0) return;
			adjust.metric = static_cast<css_font_size_adjust::Metric>(metric);
		}
		const auto& factor = value.back();
		if (factor.ident() == "from-font") {
			adjust.mode = css_font_size_adjust::Mode::from_font;
		} else {
			auto number = evaluate_css_scalar_math(factor, css_math_scalar::number);
			if (factor.type == NUMBER && factor.n.number >= 0) number = factor.n.number;
			if (!number || !std::isfinite(*number)) return;
			adjust.mode = css_font_size_adjust::Mode::number;
			adjust.value = std::max(0.0, *number);
		}
		add_parsed_property(name, property_value(adjust, important));
		return;
	}
	static const auto underline_position = _id("text-underline-position");
	static const auto underline_offset = _id("text-underline-offset");
	if (name == underline_position)
	{
		if (const auto position = parse_underline_position(value))
			add_parsed_property(name, property_value(*position, important));
		return;
	}
	if (name == underline_offset)
	{
		add_length_property(name, val, "auto", f_length_percentage | f_math, important);
		return;
	}
	css_length length;
	switch (name)
	{
	case _font_:
		parse_font(value, important);
		break;

	case _font_family_:
	{
		css_font_families families;
		if (parse_font_families(value, families))
			add_parsed_property(name, property_value(families, important));
		break;
	}

	case _font_weight_:
		if (parse_font_weight(val, length))
			add_parsed_property(name, property_value(length, important));
		break;

	case _font_style_:
	{
		css_font_style style;
		int index = 0;
		if (parse_css_font_style(value, index, style) && index == static_cast<int>(value.size())) {
			if (style.kind == font_style_oblique && style.angle == 0.0f) style.kind = font_style_normal;
			add_parsed_property(name, property_value(style, important));
		}
		break;
	}

	case _text_decoration_:
		parse_text_decoration(value, important, container);
		break;

	case _text_decoration_thickness_:
		add_length_property(name, val, style_text_decoration_thickness_strings, f_length_percentage|f_positive, important);
		break;

	case _text_decoration_color_:
		parse_text_decoration_color(val, important, container);
		break;

	case _text_decoration_line_:
		parse_text_decoration_line(value, important);
		break;

	case _text_emphasis_:
		parse_text_emphasis(value, important, container);
		break;

	case _text_emphasis_style_:
		if (const auto emphasis = parse_emphasis_style(value))
			add_parsed_property(name, property_value(*emphasis, important));
		break;

	case _text_emphasis_color_:
		parse_text_emphasis_color(val, important, container);
		break;

	case _text_emphasis_position_:
		parse_text_emphasis_position(value, important);
		break;
	default:
		break;
	}
}

static bool parse_font_weight(const css_token& tok, css_length& weight)
{
	if (int idx = value_index(tok.ident(), font_weight_strings); idx >= 0)
	{
		weight.predef(idx);
		return true;
	}

	// https://drafts.csswg.org/css-fonts/#font-weight-absolute-values
	// Note: fractional values are allowed.
	if (tok.type == NUMBER && tok.n.number >= 1 && tok.n.number <= 1000)
	{
		weight.set_value(tok.n.number, css_units_none);
		return true;
	}

	return false;
}

// <font-style> || <font-variant-css2> || <font-weight> || <font-width-css3>
// None of the allowed values intersect with <font-size>, it cannot consume <font-size>.
// Note: <font-weight> can be a number >=1, but <font-size> inside <font> is <length-percentage>,
// so it can be only 0 number. <font-size> as a standalone property does allow <number>s in quirks mode.
static void parse_font_prefix(const css_token_vector& tokens, int& index,
	css_font_style& style, int& variant, css_length& weight, float& width)
{
	bool style_found = false;
	bool variant_found = false;
	bool weight_found = false;
	bool width_found = false;

	int count = 0;
	while (index < (int)tokens.size() && count++ < 4)
	{
		const auto& tok = tokens[index];
		const auto width_keyword = width_found ? std::nullopt : parse_css_font_width(tok, true);
		// Each omitted component already has its normal value.
		if (tok.ident() == "normal")
		{
			index++;
		} else if (!style_found && parse_css_font_style(tokens, index, style))
		{
			style_found = true;
		}
		else if (!variant_found && parse_keyword(tok, variant, font_variant_strings))
		{
			variant_found = true;
			index++;
		}
		else if (!weight_found && parse_font_weight(tok, weight))
		{
			weight_found = true;
			index++;
		}
		else if (width_keyword)
		{
			width_found = true;
			width = *width_keyword;
			index++;
		}
		else break;
	}
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/font
// https://drafts.csswg.org/css-fonts/#font-prop
// font = <font-style-weight>? <font-size> [ / <line-height> ]? <font-family>
// font = <system-family-name*>
// <font-style-weight> = <font-style> || <font-variant-css2> || <font-weight> || <font-width-css3>
void css_value_parser::parse_font(css_token_vector tokens, bool important)
{
	// initial values
	css_font_style style;
	int variant = font_variant_normal;
	float width = 100.0f;
	css_length weight      = css_length::predef_value(font_weight_normal);
	css_length size        = css_length::predef_value(font_size_medium);
	css_length line_height = css_length::predef_value(line_height_normal);
	css_font_families font_family;

	if (tokens.size() == 1 && value_in_list(tokens[0].ident(), font_system_family_name_strings))
	{
		font_family.push_back({ "system-ui", true });
	} else
	{
		int index = 0;
		parse_font_prefix(tokens, index, style, variant, weight, width);

		// font-size = <absolute-size> | <relative-size> | <length-percentage [0,∞]> | math
		if (!size.from_token(at(tokens, index), f_length_percentage | f_positive | f_math, font_size_strings))
			return;
		index++;

		if (at(tokens, index).ch == '/')
		{
			index++;
			// https://drafts.csswg.org/css2/#propdef-line-height
			// line-height = normal | <number> | <length> | <percentage>
			if (!line_height.from_token(at(tokens, index), f_number | f_length_percentage | f_positive | f_math, line_height_strings))
				return;
			index++;
		}

		remove(tokens, 0, index);
		if (!parse_font_families(tokens, font_family))
			return;
	}
	if (style.kind == font_style_oblique && style.angle == 0.0f) style.kind = font_style_normal;
	add_parsed_property(_font_style_,   property_value(style,       important));
	if (variant == font_variant_small_caps)
		add_parsed_property(variant_properties()[caps], property_value(css_font_features{ { font_tag("smcp"), 1 } }, important));
	add_parsed_property(_font_weight_,  property_value(weight,      important));
	add_parsed_property(_id("font-width"), property_value(width, important));
	add_parsed_property(_font_size_,    property_value(size,        important));
	add_parsed_property(_line_height_,  property_value(line_height, important));
	add_parsed_property(_font_family_,  property_value(font_family, important));
}

void css_value_parser::parse_text_decoration(const css_token_vector& tokens, bool important, document_container* container)
{
	text_decoration decoration;
	if (!parse_decoration(tokens, container, decoration)) return;
	add_parsed_property(_text_decoration_line_, property_value(decoration.line, important));
	add_parsed_property(_text_decoration_style_, property_value(static_cast<int>(decoration.style), important));
	add_parsed_property(_text_decoration_thickness_, property_value(decoration.thickness, important));
	add_parsed_property(_text_decoration_color_, property_value(decoration.color, important));
}

void css_value_parser::parse_text_decoration_color(const css_token& token, bool important, document_container* container)
{
	web_color _color;
	if(parse_color(token, _color, container))
	{
		add_parsed_property(_text_decoration_color_, property_value(_color, important));
		return;
	}
	if(token.type == IDENT && value_in_list(token.ident(), "auto;currentcolor"))
	{
		add_parsed_property(_text_decoration_color_, property_value(web_color::current_color, important));
		return;
	}
}

void css_value_parser::parse_text_decoration_line(const css_token_vector& tokens, bool important)
{
	int lines;
	if (parse_decoration_line(tokens, lines))
		add_parsed_property(_text_decoration_line_, property_value(lines, important));
}

void css_value_parser::parse_text_emphasis(const css_token_vector& tokens, bool important, document_container* container)
{
	css_token_vector style_tokens;
	web_color color = web_color::current_color;
	bool has_color = false;
	for (const auto& token : tokens)
	{
		if (!has_color && parse_color(token, color, container)) has_color = true;
		else style_tokens.push_back(token);
	}
	const auto style = style_tokens.empty() && has_color ? std::optional<string>(string()) : parse_emphasis_style(style_tokens);
	if (!style) return;
	add_parsed_property(_text_emphasis_style_, property_value(*style, important));
	add_parsed_property(_text_emphasis_color_, property_value(color, important));
}

void css_value_parser::parse_text_emphasis_color(const css_token& token, bool important, document_container* container)
{
	web_color color = web_color::current_color;
	if (parse_color(token, color, container))
		add_parsed_property(_text_emphasis_color_, property_value(color, important));
}

void css_value_parser::parse_text_emphasis_position(const css_token_vector& tokens, bool important)
{
	int position = 0;
	bool vertical = false;
	bool horizontal = false;
	for (const auto& token : tokens)
	{
		const auto word = token.ident();
		const bool is_vertical = is_one_of(word, "over", "under");
		const bool is_horizontal = is_one_of(word, "left", "right");
		if ((!is_vertical && !is_horizontal) || (is_vertical && vertical) || (is_horizontal && horizontal)) return;
		vertical |= is_vertical;
		horizontal |= is_horizontal;
		if (word == "under") position |= text_emphasis_position_under;
		else if (word == "left") position |= text_emphasis_position_left;
		else if (word == "right") position |= text_emphasis_position_right;
	}
	if (vertical) add_parsed_property(_text_emphasis_position_,
		property_value(position | (horizontal ? 0 : text_emphasis_position_right), important));
}

} // namespace litehtml
