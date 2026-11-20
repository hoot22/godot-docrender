#include "html_cssom.h"

#include "css_animated_property.h"
#include "css_computed_style.h"
#include "css_declarations.h"
#include "css_value_parser.h"
#include "css_syntax_util.h"
#include <litehtml/css_parser.h>

#include <iomanip>
#include <locale>
#include <sstream>

namespace dockit::css {
namespace {
	using namespace litehtml;

	std::string number(double value) {
		std::ostringstream stream;
		stream.imbue(std::locale::classic());
		stream << std::setprecision(7) << value;
		return stream.str();
	}

	std::string length(string_id name, const css_length& value) {
		if (value.expression()) return "calc(" + value.to_string() + ")";
		if (value.intrinsic_size() != css_intrinsic_size::none) return value.to_string();
		if (value.is_predefined()) {
			if (name == _font_weight_) return detail::index_value(value.predef(), font_weight_strings);
			if (name == _font_size_) return detail::index_value(value.predef(), font_size_strings);
			if (name == _border_top_width_ || name == _border_right_width_ || name == _border_bottom_width_ ||
				name == _border_left_width_ || name == _id("outline-width"))
				return detail::index_value(value.predef(), border_width_strings);
			if (name == _text_decoration_thickness_) return detail::index_value(value.predef(), style_text_decoration_thickness_strings);
			if (name == _flex_basis_) return detail::index_value(value.predef(), flex_basis_strings);
			if (name == _max_width_ || name == _max_height_) return "none";
			if (name == _id("line-height") || name == _id("letter-spacing") || name == _id("word-spacing") ||
				name == _id("row-gap") || name == _id("column-gap")) return "normal";
			return "auto";
		}
		return number(value.val()) + (value.units() == css_units_none ? "" :
			detail::index_value(value.units(), css_units_strings));
	}

	std::string color(const web_color& value, const web_color& current) {
		const auto srgb = value.to_srgb(current);
		const std::string components = std::to_string(srgb.red) + ", " + std::to_string(srgb.green) + ", " +
			std::to_string(srgb.blue);
		return srgb.alpha == 255 ? "rgb(" + components + ")" :
			"rgba(" + components + ", " + number(srgb.alpha / 255.0) + ")";
	}

	std::string keyword(string_id name, int value) {
		if (const auto* words = css_keywords(name)) return detail::index_value(value, words);
		static const std::map<string_id, const char*> keywords{
			{_display_, style_display_strings}, {_position_, element_position_strings}, {_visibility_, visibility_strings},
			{_id("overflow-x"), overflow_strings}, {_id("overflow-y"), overflow_strings},
			{_id("border-top-style"), border_style_strings}, {_id("border-right-style"), border_style_strings},
			{_id("border-bottom-style"), border_style_strings}, {_id("border-left-style"), border_style_strings},
			{_id("box-sizing"), "content-box;border-box"}, {_id("direction"), "ltr;rtl"},
			{_id("pointer-events"), "auto;none"}, {_id("table-layout"), "auto;fixed"}, {_id("empty-cells"), "show;hide"}
		};
		const auto entry = keywords.find(name);
		return entry != keywords.end() ? detail::index_value(value, entry->second) : std::string();
	}

	std::string quoted(const std::string& value) {
		std::string result = "\"";
		for (const char c : value) {
			if (c == '"' || c == '\\') result += '\\';
			if (c == '\n') result += "\\a ";
			else if (c == '\r') result += "\\d ";
			else result += c;
		}
		return result + '"';
	}

	std::string font_families(const css_font_families& families) {
		std::string result;
		for (const auto& family : families) {
			if (!result.empty()) result += ", ";
			result += family.generic ? family.name : quoted(family.name);
		}
		return result;
	}

	std::string alignment(const css_alignment& value) {
		const auto prefix = value.overflow == alignment_overflow::safe ? "safe " :
			value.overflow == alignment_overflow::unsafe ? "unsafe " : "";
		return prefix + detail::index_value(static_cast<int>(value.keyword),
			"normal;auto;stretch;start;end;center;self-start;self-end;flex-start;flex-end;left;right;"
			"space-between;space-around;space-evenly;first baseline;last baseline");
	}

	std::string grid_breadth_text(const grid_breadth& value) {
		switch (value.kind) {
			case grid_breadth_kind::automatic: return "auto";
			case grid_breadth_kind::length: return length(empty_id, value.length);
			case grid_breadth_kind::fraction: return number(value.fraction) + "fr";
			case grid_breadth_kind::min_content: return "min-content";
			case grid_breadth_kind::max_content: return "max-content";
			case grid_breadth_kind::fit_content: return "fit-content(" + length(empty_id, value.length) + ")";
		}
		return {};
	}

	void append_word(std::string& result, const std::string& word) {
		if (word.empty()) return;
		if (!result.empty()) result += " ";
		result += word;
	}

	std::string grid_track_text(const grid_track& track) {
		const auto min = grid_breadth_text(track.minimum);
		const auto max = grid_breadth_text(track.maximum);
		if (min == max || track.maximum.kind == grid_breadth_kind::fit_content ||
			(min == "auto" && track.maximum.kind == grid_breadth_kind::fraction)) return max;
		return "minmax(" + min + ", " + max + ")";
	}

	std::string grid_track_text(const grid_track_segment& segment) {
		std::vector<std::vector<std::string>> names(segment.tracks.size() + 1);
		for (const auto& [name, indices] : segment.names) {
			for (const auto index : indices) {
				if (index >= 0 && size_t(index) < names.size()) names[index].push_back(name);
			}
		}
		std::string result;
		for (size_t i = 0; i < names.size(); ++i) {
			if (!names[i].empty()) {
				std::sort(names[i].begin(), names[i].end());
				std::string line;
				for (const auto& name : names[i]) append_word(line, name);
				append_word(result, "[" + line + "]");
			}
			if (i < segment.tracks.size()) append_word(result, grid_track_text(segment.tracks[i]));
		}
		return result;
	}

	std::string grid_tracks(const grid_track_template& value) {
		if (value.subgrid) {
			const auto append_names = [](std::string& result, const grid_subgrid_lines::name_list& names) {
				for (const auto& group : names) {
					std::string line;
					for (const auto& name : group) append_word(line, name);
					append_word(result, "[" + line + "]");
				}
			};
			std::string result = "subgrid";
			append_names(result, value.subgrid->prefix);
			if (!value.subgrid->automatic.empty()) {
				std::string automatic;
				append_names(automatic, value.subgrid->automatic);
				append_word(result, "repeat(auto-fill, " + automatic + ")");
			}
			append_names(result, value.subgrid->suffix);
			return result;
		}
		auto result = grid_track_text(value.prefix);
		if (value.repeat) append_word(result, std::string(value.repeat->fit ? "repeat(auto-fit, " : "repeat(auto-fill, ") +
			grid_track_text(value.repeat->pattern) + ")");
		append_word(result, grid_track_text(value.suffix));
		return result.empty() ? "none" : result;
	}

	std::string transform(const css_transform_list& transforms) {
		std::string result;
		for (const auto& item : transforms) {
			using Kind = css_transform_function::kind;
			if (!result.empty()) result += " ";
			const auto x = [&] { return length(empty_id, item.x); };
			const auto y = [&] { return length(empty_id, item.y); };
			const auto z = [&] { return length(empty_id, item.z); };
			const auto matrix = [&](std::string label, size_t size) {
				for (size_t index = 0; index < size; ++index) {
					if (index != 0) label += ", ";
					label += number(item.matrix[index]);
				}
				return label + ")";
			};
			switch (item.type) {
				case Kind::translate: result += "translate3d(" + x() + ", " + y() + ", " + z() + ")"; break;
				case Kind::scale: result += "scale3d(" + number(item.first) + ", " + number(item.second) + ", " + number(item.third) + ")"; break;
				case Kind::rotate: result += "rotate3d(" + number(item.axis[0]) + ", " + number(item.axis[1]) + ", " +
					number(item.axis[2]) + ", " + number(item.first) + "rad)"; break;
				case Kind::skew: result += "skew(" + number(item.first) + "rad, " + number(item.second) + "rad)"; break;
				case Kind::matrix: result += matrix("matrix(", 6); break;
				case Kind::matrix3d: result += matrix("matrix3d(", 16); break;
				case Kind::perspective: result += "perspective(" + (item.z.is_predefined() ? std::string("none") : z()) + ")"; break;
				case Kind::blend: return {};
			}
		}
		return result.empty() ? "none" : result;
	}
}

std::string property_name(const godot::String& name) {
	const auto utf8 = name.utf8();
	const std::string text(utf8.get_data(), utf8.length());
	return text.compare(0, 2, "--") == 0 ? text : detail::lowcase(text);
}

raw_declaration::vector inline_declarations(const godot::String& text) {
	raw_declaration::vector declarations;
	raw_rule::vector rules;
	css_parser(litehtml::normalize(text.utf8().get_data(), f_componentize)).consume_style_block_contents(declarations, rules);
	for (auto& declaration : declarations) {
		if (declaration.name.compare(0, 2, "--") != 0) declaration.name = detail::lowcase(declaration.name);
	}
	return declarations;
}

std::string serialize_declarations(const raw_declaration::vector& declarations) {
	std::string text;
	for (const auto& declaration : declarations) {
		if (!text.empty()) text += " ";
		text += declaration.name + ": " + get_repr(declaration.value);
		if (declaration.important) text += " !important";
		text += ";";
	}
	return text;
}

std::string serialize_rule(const raw_rule& rule) {
	std::string result;
	if (rule.type == raw_rule::at) result += "@" + rule.name;
	result += get_repr(rule.prelude);
	if (rule.block.type == CURLY_BLOCK) return result + rule.block.get_repr();
	return result + ";";
}

raw_rule::ptr parse_cssom_rule(const godot::String& text) {
	const auto tokens = litehtml::normalize(text.utf8().get_data());
	std::vector<css_token_type> open;
	bool ended = false;
	for (const auto& token : tokens) {
		if (token.type == WHITESPACE) continue;
		if (ended || token.type == BAD_STRING || token.type == BAD_URL) return {};
		if (token.type == FUNCTION || token.type == LEFT_PAREN) open.push_back(RIGHT_PAREN);
		else if (token.type == LEFT_BRACKET) open.push_back(RIGHT_BRACKET);
		else if (token.type == LEFT_BRACE) open.push_back(RIGHT_BRACE);
		else if (token.type == RIGHT_BRACE || token.type == RIGHT_BRACKET || token.type == RIGHT_PAREN) {
			if (open.empty() || open.back() != token.type) return {};
			open.pop_back();
			ended = open.empty() && token.type == RIGHT_BRACE;
		} else if (token.type == SEMICOLON && open.empty()) {
			ended = true;
		}
	}
	if (!ended) return {};
	const auto rules = css_parser::parse_stylesheet(tokens, false);
	return rules.size() == 1 ? rules.front() : nullptr;
}

std::string serialize_value(litehtml::string_id name, const litehtml::property_value& value) {
	using namespace litehtml;
	if (value.is<inherit>()) return "inherit";
	if (value.is<initial_value>()) return "initial";
	if (value.is<revert_value>()) return "revert";
	if (value.is<revert_layer_value>()) return "revert-layer";
	if (value.is<std::string>()) return value.get<std::string>();
	if (value.is<css_token_vector>()) return get_repr(value.get<css_token_vector>());
	if (value.is<css_length>()) return length(name, value.get<css_length>());
	if (value.is<float>()) return number(value.get<float>());
	if (value.is<int>()) return keyword(name, value.get<int>());
	if (value.is<web_color>()) {
		const auto& value_color = value.get<web_color>();
		if (value_color.is_current_color) return "currentcolor";
		if (value_color.css_data || value_color.system != system_color::none) return {};
		return color(value_color, web_color::black);
	}
	if (value.is<css_transform_list>()) return transform(value.get<css_transform_list>());
	if (value.is<css_font_families>()) return font_families(value.get<css_font_families>());
	if (value.is<css_font_style>()) {
		const auto& style = value.get<css_font_style>();
		return style.kind == font_style_oblique ? "oblique " + number(style.angle) + "deg" :
			detail::index_value(style.kind, font_style_strings);
	}
	if (value.is<css_text_indent>()) {
		const auto& indent = value.get<css_text_indent>();
		return length(name, indent.length) + (indent.hanging ? " hanging" : "") + (indent.each_line ? " each-line" : "");
	}
	if (value.is<css_font_size_adjust>()) {
		const auto& adjust = value.get<css_font_size_adjust>();
		using Mode = css_font_size_adjust::Mode;
		if (adjust.mode == Mode::none) return "none";
		const std::string metric = detail::index_value(static_cast<int>(adjust.metric), "ex-height;cap-height;ch-width;ic-width;ic-height");
		return metric + " " + (adjust.mode == Mode::from_font ? "from-font" : number(adjust.value));
	}
	if (value.is<css_alignment>()) return alignment(value.get<css_alignment>());
	if (value.is<grid_track_template>()) return grid_tracks(value.get<grid_track_template>());
	if (value.is<grid_track_list>()) {
		std::string result;
		for (const auto& track : value.get<grid_track_list>()) append_word(result, grid_track_text(track));
		return result.empty() ? "auto" : result;
	}
	if (value.is<grid_auto_flow>()) {
		const auto& flow = value.get<grid_auto_flow>();
		return std::string(flow.by_column ? "column" : "row") + (flow.dense ? " dense" : "");
	}
	if (value.is<grid_line>()) {
		const auto& line = value.get<grid_line>();
		if (line.type == grid_line::kind::automatic) return "auto";
		std::string result = line.type == grid_line::kind::span ? "span" : "";
		if (line.value) append_word(result, std::to_string(line.value));
		append_word(result, line.name);
		return result;
	}
	return {};
}

std::string serialize_computed(litehtml::string_id name, const ComputedStyle& style) {
	using namespace litehtml;
	if (is_box_shorthand(name)) {
		std::vector<std::string> sides;
		for (const auto longhand : css_longhands(name)) sides.push_back(serialize_computed(longhand, style));
		return serialize_box_shorthand(std::move(sides));
	}
	if (name == _id("font-width")) return number(style.get_font_width()) + "%";
	for (const auto& property : animated_properties()) {
		if (name != property.name) continue;
		const auto value = property.read(style);
		return std::visit([&](const auto& typed) {
			using T = std::decay_t<decltype(typed)>;
			if constexpr (std::is_same_v<T, web_color>) return color(typed, style.get_color());
			else return serialize_value(name, property_value(typed, false));
		}, value);
	}
	if (name == _display_) return keyword(name, style.get_display());
	if (name == _position_) return keyword(name, style.get_position());
	if (name == _visibility_) return keyword(name, style.get_visibility());
	if (name == _id("overflow-x")) return keyword(name, style.get_overflow_x());
	if (name == _id("overflow-y")) return keyword(name, style.get_overflow_y());
	if (name == _id("box-sizing")) return keyword(name, style.get_box_sizing());
	if (name == _id("direction")) return style.is_right_to_left() ? "rtl" : "ltr";
	if (name == _font_family_) return font_families(style.get_font_families());
	if (name == _font_style_) return serialize_value(name, property_value(style.get_font_style(), false));
	if (name == _text_align_) return detail::index_value(style.get_text_align(), text_align_strings);
	if (name == _id("text-align-last")) return detail::index_value(style.get_text_align_last(), text_align_strings);
	if (name == _id("white-space-collapse")) return detail::index_value(style.get_white_space_collapse(), white_space_collapse_strings);
	if (name == _id("text-wrap-mode")) return detail::index_value(style.get_text_wrap_mode(), text_wrap_mode_strings);
	if (name == _id("text-wrap-style")) return detail::index_value(style.get_text_wrap_style(), text_wrap_style_strings);
	if (name == _id("cursor")) return style.get_cursor().empty() ? "auto" : style.get_cursor();
	if (name == _id("pointer-events")) return style.accepts_pointer_events() ? "auto" : "none";
	if (name == _id("scroll-behavior")) return style.uses_smooth_scroll() ? "smooth" : "auto";
	if (name == _z_index_) return style.has_z_index() ? std::to_string(style.get_z_index()) : "auto";
	if (name == _order_) return std::to_string(style.get_order());
	if (name == _border_top_style_) return keyword(name, style.get_borders().top.style);
	if (name == _border_right_style_) return keyword(name, style.get_borders().right.style);
	if (name == _border_bottom_style_) return keyword(name, style.get_borders().bottom.style);
	if (name == _border_left_style_) return keyword(name, style.get_borders().left.style);
	if (name == _id("grid-template-columns")) return grid_tracks(style.get_grid_template_columns());
	if (name == _id("grid-template-rows")) return grid_tracks(style.get_grid_template_rows());
	if (name == _id("grid-auto-columns")) return serialize_value(name, property_value(style.get_grid_auto_columns(), false));
	if (name == _id("grid-auto-rows")) return serialize_value(name, property_value(style.get_grid_auto_rows(), false));
	if (name == _id("grid-auto-flow")) return serialize_value(name, property_value(style.get_grid_auto_flow(), false));
	if (name == _id("justify-items")) return alignment(style.get_justify_items());
	if (name == _id("align-items")) return alignment(style.get_align_items());
	if (name == _id("justify-self")) return alignment(style.get_justify_self());
	if (name == _id("align-self")) return alignment(style.get_align_self());
	if (name == _id("justify-content")) return alignment(style.get_justify_content());
	if (name == _id("align-content")) return alignment(style.get_align_content());
	return {};
}

bool is_box_shorthand(litehtml::string_id name) {
	using namespace litehtml;
	static const std::array names{_margin_, _padding_, _id("inset"), _id("scroll-padding"), _id("scroll-margin"),
		_border_color_, _border_width_, _border_style_, _id("gap"), _overflow_};
	return std::find(names.begin(), names.end(), name) != names.end();
}

std::string serialize_box_shorthand(std::vector<std::string> values) {
	if (values.empty() || std::any_of(values.begin(), values.end(), [](const auto& value) { return value.empty(); })) return {};
	if (values.size() == 4 && values[3] == values[1]) values.pop_back();
	if (values.size() == 3 && values[2] == values[0]) values.pop_back();
	if (values.size() == 2 && values[1] == values[0]) values.pop_back();
	std::string result;
	for (const auto& value : values) append_word(result, value);
	return result;
}
}
