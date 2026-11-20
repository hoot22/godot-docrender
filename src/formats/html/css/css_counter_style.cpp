#include "css_counter_style.h"

#include <litehtml/css_parser.h>
#include <litehtml/html.h>
#include <litehtml/num_cvt.h>
#include <litehtml/utf8_strings.h>

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>

#include <algorithm>
#include <charconv>
#include <limits>

namespace litehtml {
namespace {
	constexpr auto INT_MINIMUM = std::numeric_limits<int>::min();
	constexpr auto INT_MAXIMUM = std::numeric_limits<int>::max();
	constexpr size_t MAX_COUNTER_BYTES = 16384;
	constexpr size_t MAX_STYLE_CHAIN = 64;
	constexpr const char* SYSTEM_NAMES = "cyclic;fixed;symbolic;alphabetic;numeric;additive;extends";
	const css_counter_style* builtin(const string& name);

	std::optional<string> style_name(const css_token& token, bool allow_none) {
		if (token.type != IDENT) return std::nullopt;
		const string keyword = token.ident();
		if ((!allow_none && keyword == "none") ||
			is_one_of(keyword, "inherit", "initial", "unset", "revert", "revert-layer", "default")) return std::nullopt;
		return builtin(keyword) || value_in_list(keyword, list_style_type_strings) ? keyword : token.name;
	}

	std::optional<string> symbol(const css_token& token, bool allow_ident = true) {
		if (token.type == STRING) return token.str;
		if (allow_ident && token.type == IDENT) return token.name;
		return std::nullopt;
	}

	std::optional<string_vector> parse_symbols(const css_token_vector& tokens, size_t start, bool allow_ident) {
		if (start == tokens.size()) return std::nullopt;
		string_vector result;
		for (size_t i = start; i < tokens.size(); ++i) {
			auto value = symbol(tokens[i], allow_ident);
			if (!value) return std::nullopt;
			result.push_back(std::move(*value));
		}
		return result;
	}

	std::optional<std::vector<std::pair<int, int>>> parse_ranges(const css_token_vector& tokens) {
		std::vector<std::pair<int, int>> ranges;
		if (tokens.size() == 1 && tokens.front().ident() == "auto") return ranges;
		for (const auto& pair : parse_comma_separated_list(tokens)) {
			if (pair.size() != 2) return std::nullopt;
			const auto lower = pair[0].ident() == "infinite" ? std::optional<int>(INT_MINIMUM) : parse_counter_integer(pair[0]);
			const auto upper = pair[1].ident() == "infinite" ? std::optional<int>(INT_MAXIMUM) : parse_counter_integer(pair[1]);
			if (!lower || !upper || *lower > *upper) return std::nullopt;
			ranges.emplace_back(*lower, *upper);
		}
		return ranges;
	}

	std::optional<std::vector<std::pair<int, string>>> parse_additive(const css_token_vector& tokens) {
		std::vector<std::pair<int, string>> additive;
		for (const auto& pair : parse_comma_separated_list(tokens)) {
			if (pair.size() != 2) return std::nullopt;
			const auto number_first = pair[0].type == NUMBER;
			const auto weight = parse_counter_integer(pair[number_first ? 0 : 1]);
			auto text = symbol(pair[number_first ? 1 : 0]);
			if (!weight || !text || *weight < 0 || (!additive.empty() && *weight >= additive.back().first)) return std::nullopt;
			additive.emplace_back(*weight, std::move(*text));
		}
		return additive;
	}

	bool has_required_symbols(const css_counter_style& style) {
		switch (style.system) {
			case counter_system::numeric:
			case counter_system::alphabetic: return style.symbols.size() >= 2;
			case counter_system::additive: return !style.additive.empty();
			case counter_system::extends: return style.symbols.empty() && style.additive.empty();
			default: return !style.symbols.empty();
		}
	}

	void parse_system(const css_token_vector& tokens, css_counter_style& style) {
		const int index = value_index(tokens.front().ident(), SYSTEM_NAMES);
		if (index < 0) return;
		const auto system = static_cast<counter_system>(index);
		int first = 1;
		string base;
		if (system == counter_system::extends) {
			if (tokens.size() != 2) return;
			auto name = style_name(tokens[1], false);
			if (!name) return;
			base = std::move(*name);
		}
		else if (system == counter_system::fixed && tokens.size() == 2) {
			const auto parsed = parse_counter_integer(tokens[1]);
			if (!parsed) return;
			first = *parsed;
		}
		else if (tokens.size() != 1) return;
		style.system = system;
		style.first = first;
		style.base = std::move(base);
	}

	bool append(string& result, const string& piece, int64_t repeats = 1) {
		if (piece.empty()) return true;
		if (static_cast<uint64_t>(repeats) > (MAX_COUNTER_BYTES - result.size()) / piece.size()) return false;
		for (int64_t n = 0; n < repeats; ++n) result += piece;
		return true;
	}

	std::optional<string> represent(const css_counter_style& style, int64_t value) {
		const auto count = static_cast<int64_t>(style.symbols.size());
		string result;
		switch (style.system) {
			case counter_system::cyclic: return style.symbols[((value - 1) % count + count) % count];
			case counter_system::fixed:
				if (value < style.first || value - style.first >= count) return std::nullopt;
				return style.symbols[value - style.first];
			case counter_system::symbolic:
				if (value <= 0 || !append(result, style.symbols[(value - 1) % count], (value - 1) / count + 1)) return std::nullopt;
				return result;
			case counter_system::alphabetic:
			case counter_system::numeric: {
				const bool alphabetic = style.system == counter_system::alphabetic;
				if (value < (alphabetic ? 1 : 0)) return std::nullopt;
				std::vector<size_t> digits;
				do {
					if (alphabetic) --value;
					digits.push_back(value % count);
					value /= count;
				} while (value > 0);
				for (auto digit = digits.rbegin(); digit != digits.rend(); ++digit)
					if (!append(result, style.symbols[*digit])) return std::nullopt;
				return result;
			}
			case counter_system::additive:
				if (value == 0) {
					if (style.additive.back().first == 0) return style.additive.back().second;
					return std::nullopt;
				}
				for (const auto& [weight, text] : style.additive) {
					if (weight == 0 || weight > value) continue;
					if (!append(result, text, value / weight)) return std::nullopt;
					value %= weight;
					if (value == 0) return result;
				}
				return std::nullopt;
			case counter_system::extends: return std::nullopt;
		}
		return std::nullopt;
	}

	const css_counter_style* builtin(const string& name) {
		static const auto styles = [] {
			std::map<string, css_counter_style> out;
			auto& decimal = out["decimal"];
			decimal.system = counter_system::numeric;
			for (char digit = '0'; digit <= '9'; ++digit) decimal.symbols.emplace_back(1, digit);
			out["decimal-leading-zero"] = decimal;
			out["decimal-leading-zero"].pad = { 2, "0" };
			const auto add_numeric = [&](const char* key, char32_t zero) {
				auto& style = out[key];
				style.system = counter_system::numeric;
				for (int digit = 0; digit < 10; ++digit) style.symbols.emplace_back(litehtml_from_utf32(std::u32string(1, zero + digit)));
			};
			static constexpr std::pair<const char*, char32_t> digit_blocks[] = {
				{ "arabic-indic", 0x0660 }, { "persian", 0x06f0 }, { "devanagari", 0x0966 }, { "bengali", 0x09e6 },
				{ "gurmukhi", 0x0a66 }, { "gujarati", 0x0ae6 }, { "oriya", 0x0b66 }, { "tamil", 0x0be6 },
				{ "telugu", 0x0c66 }, { "kannada", 0x0ce6 }, { "malayalam", 0x0d66 }, { "thai", 0x0e50 },
				{ "lao", 0x0ed0 }, { "tibetan", 0x0f20 }, { "myanmar", 0x1040 }, { "cambodian", 0x17e0 },
				{ "mongolian", 0x1810 },
			};
			for (const auto& [style, zero] : digit_blocks) add_numeric(style, zero);
			out["khmer"] = out["cambodian"];
			const auto add_symbols = [&](const char* key, counter_system system, std::u32string_view symbols) -> css_counter_style& {
				auto& style = out[key];
				style.system = system;
				for (const auto codepoint : symbols) style.symbols.emplace_back(litehtml_from_utf32(std::u32string(1, codepoint)));
				style.suffix = "\xe3\x80\x81";
				return style;
			};
			add_symbols("cjk-decimal", counter_system::numeric, U"\u3007\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d")
				.ranges = std::vector<std::pair<int, int>>{ { 0, INT_MAXIMUM } };
			add_symbols("cjk-earthly-branch", counter_system::fixed, U"\u5b50\u4e11\u5bc5\u536f\u8fb0\u5df3\u5348\u672a\u7533\u9149\u620c\u4ea5")
				.fallback = "cjk-decimal";
			add_symbols("cjk-heavenly-stem", counter_system::fixed, U"\u7532\u4e59\u4e19\u4e01\u620a\u5df1\u5e9a\u8f9b\u58ec\u7678")
				.fallback = "cjk-decimal";
			for (const auto& [style, first] : { std::pair{ "lower-alpha", 'a' }, std::pair{ "upper-alpha", 'A' } }) {
				auto& alphabet = out[style];
				alphabet.system = counter_system::alphabetic;
				for (int i = 0; i < 26; ++i) alphabet.symbols.emplace_back(1, first + i);
			}
			out["lower-latin"] = out["lower-alpha"];
			out["upper-latin"] = out["upper-alpha"];
			auto& greek = out["lower-greek"];
			greek.system = counter_system::alphabetic;
			for (int codepoint = 0x03B1; codepoint <= 0x03C9; ++codepoint) {
				if (codepoint == 0x03C2) continue;
				greek.symbols.push_back({ static_cast<char>(0xC0 | (codepoint >> 6)), static_cast<char>(0x80 | (codepoint & 0x3F)) });
			}
			for (const string style : { "upper-roman", "lower-roman" }) {
				auto& roman = out[style];
				roman.system = counter_system::additive;
				roman.ranges = std::vector<std::pair<int, int>>{ { 1, 3999 } };
				for (const int weight : { 1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1 })
					roman.additive.emplace_back(weight, style == "upper-roman" ? num_cvt::to_roman_upper(weight) : num_cvt::to_roman_lower(weight));
			}
			for (const auto& [style, text] : { std::pair{ "disc", "\xe2\x80\xa2" }, std::pair{ "circle", "\xe2\x97\xa6" },
				std::pair{ "square", "\xe2\x96\xaa" }, std::pair{ "disclosure-open", "\xe2\x96\xbe" },
				std::pair{ "disclosure-closed", "\xe2\x96\xb8" }, std::pair{ "none", "" } }) {
				auto& mark = out[style];
				mark.system = counter_system::cyclic;
				mark.symbols.emplace_back(text);
				mark.suffix = *text == '\0' ? "" : " ";
			}
			return out;
		}();
		const auto found = styles.find(name);
		return found == styles.end() ? nullptr : &found->second;
	}

	void inherit_descriptors(css_counter_style& parent, const css_counter_style& child) {
		if (child.ranges) parent.ranges = child.ranges;
		if (child.negative) parent.negative = child.negative;
		if (child.pad) parent.pad = child.pad;
		if (child.prefix) parent.prefix = child.prefix;
		if (child.suffix) parent.suffix = child.suffix;
		if (child.fallback) parent.fallback = child.fallback;
	}

	css_counter_style resolve(const css_counter_style& style, const css_counter_style_lookup& lookup) {
		std::vector<const css_counter_style*> extensions;
		const css_counter_style* base = &style;
		while (base->system == counter_system::extends) {
			if (const auto cycle = std::find(extensions.begin(), extensions.end(), base); cycle != extensions.end()) {
				// Each member of the cycle extends decimal on its own.
				extensions.erase(cycle + 1, extensions.end());
				base = builtin("decimal");
				break;
			}
			if (extensions.size() == MAX_STYLE_CHAIN) {
				base = builtin("decimal");
				break;
			}
			extensions.push_back(base);
			const string& name = base->base;
			base = lookup(name);
			if (!base) base = builtin(name);
			if (!base) base = builtin("decimal");
		}
		css_counter_style result = *base;
		for (auto extension = extensions.rbegin(); extension != extensions.rend(); ++extension)
			inherit_descriptors(result, **extension);
		return result;
	}

	int64_t grapheme_count(const string& text) {
		const auto value = godot::String::utf8(text.data(), text.size());
		const auto server = godot::TextServerManager::get_singleton()->get_primary_interface();
		return server.is_valid() ? server->string_get_character_breaks(value).size() : value.length();
	}

	std::optional<string> representation(int value, const css_counter_style& style) {
		if (style.ranges && !style.ranges->empty()) {
			if (std::none_of(style.ranges->begin(), style.ranges->end(), [value](const auto& pair) {
				return value >= pair.first && value <= pair.second;
			})) return std::nullopt;
		}
		else if ((style.system == counter_system::alphabetic || style.system == counter_system::symbolic) && value < 1) {
			return std::nullopt;
		}
		else if (style.system == counter_system::additive && value < 0) return std::nullopt;

		const bool negative = value < 0 && style.system != counter_system::cyclic && style.system != counter_system::fixed;
		const auto result = represent(style, negative ? -static_cast<int64_t>(value) : value);
		if (!result || result->size() > MAX_COUNTER_BYTES) return std::nullopt;
		static const std::pair<string, string> minus{ "-", "" };
		const auto& sign = style.negative ? *style.negative : minus;
		string padded;
		if (negative && !append(padded, sign.first)) return std::nullopt;
		if (style.pad && style.pad->first > 0) {
			int64_t missing = style.pad->first - grapheme_count(*result);
			if (negative) missing -= grapheme_count(sign.first) + grapheme_count(sign.second);
			if (missing > 0 && !append(padded, style.pad->second, missing)) return std::nullopt;
		}
		if (!append(padded, *result) || (negative && !append(padded, sign.second))) return std::nullopt;
		return padded;
	}
}

	std::optional<int> parse_counter_integer(const css_token& token) {
		if (token.type != NUMBER || token.n.number_type != css_number_integer) return std::nullopt;
		if (!token.repr.empty()) {
			const char* start = token.repr.data();
			const char* end = start + token.repr.size();
			if (*start == '+') ++start;
			int parsed;
			const auto result = std::from_chars(start, end, parsed);
			if (result.ec == std::errc() && result.ptr == end) return parsed;
			if (result.ec == std::errc::result_out_of_range) return *start == '-' ? INT_MINIMUM : INT_MAXIMUM;
			return std::nullopt;
		}
		const double value = token.n.number;
		if (!(value >= INT_MINIMUM && value <= INT_MAXIMUM)) return std::nullopt;
		return static_cast<int>(value);
	}

	std::optional<css_counter_style_reference> parse_counter_style_reference(const css_token& token, bool allow_none) {
		if (auto name = style_name(token, allow_none)) return css_counter_style_reference{ std::move(*name), {} };
		if (token.type != CV_FUNCTION || lowcase(token.name) != "symbols" || token.value.empty()) return std::nullopt;
		css_counter_style style;
		style.suffix = " ";
		const int system = value_index(token.value.front().ident(), SYSTEM_NAMES);
		const bool has_system = system >= 0 && system <= static_cast<int>(counter_system::numeric);
		if (has_system) style.system = static_cast<counter_system>(system);
		auto symbols = parse_symbols(token.value, has_system ? 1 : 0, false);
		if (!symbols) return std::nullopt;
		style.symbols = std::move(*symbols);
		if (!has_required_symbols(style)) return std::nullopt;
		return css_counter_style_reference{ {}, std::make_shared<css_counter_style>(std::move(style)) };
	}

	std::optional<std::pair<string, css_counter_style>> parse_counter_style_rule(const css_token_vector& prelude,
		const css_token_vector& block) {
		auto name_tokens = prelude;
		remove_whitespace(name_tokens);
		if (name_tokens.size() != 1) return std::nullopt;
		auto name = style_name(name_tokens.front(), false);
		if (!name || is_one_of(*name, "decimal", "disc", "square", "circle", "disclosure-open", "disclosure-closed")) return std::nullopt;
		css_counter_style style;
		raw_declaration::vector declarations;
		raw_rule::vector rules;
		css_parser(block).consume_style_block_contents(declarations, rules);
		for (auto& declaration : declarations) {
			auto& tokens = declaration.value;
			remove_whitespace(tokens);
			if (declaration.important || tokens.empty()) continue;
			const auto descriptor = lowcase(declaration.name);
			if (descriptor == "system") parse_system(tokens, style);
			else if (descriptor == "symbols") {
				if (auto parsed = parse_symbols(tokens, 0, true)) style.symbols = std::move(*parsed);
			}
			else if (descriptor == "additive-symbols") {
				if (auto parsed = parse_additive(tokens)) style.additive = std::move(*parsed);
			}
			else if (descriptor == "range") {
				if (auto parsed = parse_ranges(tokens)) style.ranges = std::move(*parsed);
			}
			else if (descriptor == "fallback" && tokens.size() == 1) {
				if (auto parsed = style_name(tokens.front(), false)) style.fallback = std::move(*parsed);
			}
			else if ((descriptor == "prefix" || descriptor == "suffix") && tokens.size() == 1) {
				if (auto parsed = symbol(tokens.front())) (descriptor == "prefix" ? style.prefix : style.suffix) = std::move(*parsed);
			}
			else if (descriptor == "negative" && tokens.size() <= 2) {
				auto first = symbol(tokens[0]);
				auto second = tokens.size() == 2 ? symbol(tokens[1]) : std::optional<string>(string());
				if (first && second) style.negative = { std::move(*first), std::move(*second) };
			}
			else if (descriptor == "pad" && tokens.size() == 2) {
				const bool number_first = tokens[0].type == NUMBER;
				const auto length = parse_counter_integer(tokens[number_first ? 0 : 1]);
				auto text = symbol(tokens[number_first ? 1 : 0]);
				if (length && *length >= 0 && text) style.pad = { *length, std::move(*text) };
			}
		}
		if (!has_required_symbols(style)) return std::nullopt;
		return std::make_pair(std::move(*name), std::move(style));
	}

	string format_css_counter(int value, const css_counter_style_reference& reference,
		const css_counter_style_lookup& lookup, bool marker) {
		const auto find = [&](const string& name) {
			const auto* style = lookup(name);
			return style ? style : builtin(name);
		};
		const auto* selected = reference.anonymous ? reference.anonymous.get() : find(reference.name);
		if (!selected) selected = builtin("decimal");
		auto style = resolve(*selected, lookup);
		const string prefix = marker ? style.prefix.value_or("") : "";
		const string suffix = marker ? style.suffix.value_or(". ") : "";
		std::vector<const css_counter_style*> visited;
		while (visited.size() < MAX_STYLE_CHAIN && std::find(visited.begin(), visited.end(), selected) == visited.end()) {
			if (auto result = representation(value, style)) return prefix + *result + suffix;
			visited.push_back(selected);
			selected = find(style.fallback.value_or("decimal"));
			if (!selected) break;
			style = resolve(*selected, lookup);
		}
		return prefix + std::to_string(value) + suffix;
	}
}
