#include "css_font_face.h"

#include "formats/html/css/css_values.h"
#include "formats/html/css/css_math.h"

#include <litehtml/css_parser.h>
#include <litehtml/html.h>
#include <litehtml/style.h>

#include <cmath>
#include <limits>
#include <string_view>

namespace litehtml {
	bool supports_font_format(const string& format) {
		return is_one_of(format, "truetype", "opentype", "woff", "woff2", "collection");
	}

	bool supports_font_tech(const string& tech) {
		return tech == "variations";
	}

	namespace {
		std::optional<float> parse_oblique_angle(const css_token& token) {
			constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
			if (token.type == CV_FUNCTION) {
				const auto radians = evaluate_css_scalar_math(token, css_math_scalar::angle);
				if (!radians) return {};
				return static_cast<float>(std::clamp(*radians * radians_to_degrees, -90.0, 90.0));
			}
			if (token.type != DIMENSION || !std::isfinite(token.n.number)) return {};
			const auto unit = lowcase(token.unit);
			float degrees = token.n.number;
			if (unit == "rad") degrees *= static_cast<float>(radians_to_degrees);
			else if (unit == "grad") degrees *= 0.9f;
			else if (unit == "turn") degrees *= 360.0f;
			else if (unit != "deg") return {};
			if (degrees < -90.0f || degrees > 90.0f) return {};
			return degrees;
		}

		constexpr char32_t MAX_CODEPOINT = 0x10ffff;

		int hex_digit(char ch) {
			if (ch >= '0' && ch <= '9') return ch - '0';
			if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
			if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
			return -1;
		}

		char32_t consume_hex(std::string_view& text, int& count) {
			char32_t value = 0;
			count = 0;
			while (count < 6 && !text.empty()) {
				const int digit = hex_digit(text.front());
				if (digit < 0) break;
				value = value * 16 + digit;
				text.remove_prefix(1);
				++count;
			}
			return value;
		}

		std::optional<std::pair<char32_t, char32_t>> parse_unicode_range(std::string_view text) {
			constexpr std::string_view whitespace = " \t\n\r\f";
			const auto first_char = text.find_first_not_of(whitespace);
			if (first_char == std::string_view::npos) return {};
			text = text.substr(first_char, text.find_last_not_of(whitespace) - first_char + 1);
			if (text.size() < 3 || (text.front() != 'u' && text.front() != 'U') || text[1] != '+') return {};
			text.remove_prefix(2);
			int digits;
			char32_t first = consume_hex(text, digits);
			char32_t last = first;
			if (!text.empty() && text.front() == '?') {
				while (digits < 6 && !text.empty() && text.front() == '?') {
					first *= 16;
					last = last * 16 + 15;
					text.remove_prefix(1);
					++digits;
				}
			}
			else if (digits > 0 && !text.empty() && text.front() == '-') {
				text.remove_prefix(1);
				last = consume_hex(text, digits);
			}
			if (digits == 0 || !text.empty() || first > last || last > MAX_CODEPOINT) return {};
			return std::make_pair(first, last);
		}

		std::optional<css_unicode_ranges> parse_unicode_ranges(const css_token_vector& tokens) {
			css_unicode_ranges ranges;
			for (const auto& entry : parse_comma_separated_list(tokens)) {
				const auto range = parse_unicode_range(get_repr(entry));
				if (!range) return {};
				ranges.push_back(*range);
			}
			std::sort(ranges.begin(), ranges.end());
			css_unicode_ranges merged;
			for (const auto& range : ranges) {
				if (!merged.empty() && range.first <= merged.back().second + 1) {
					merged.back().second = std::max(merged.back().second, range.second);
				}
				else {
					merged.push_back(range);
				}
			}
			if (merged.size() == 1 && merged.front().first == 0 && merged.front().second == MAX_CODEPOINT)
				merged.clear();
			return merged;
		}

		std::optional<bool> parse_format(const css_token& token) {
			if (token.type != CV_FUNCTION || !equal_i(token.name, "format") || token.value.size() != 1) return {};
			const auto& argument = token.value.front();
			const auto name = argument.type == STRING ? lowcase(argument.str) : argument.ident();
			if (supports_font_format(name)) return false;
			if (argument.type == STRING && is_one_of(name, "woff2-variations", "woff-variations",
				"truetype-variations", "opentype-variations")) return true;
			return {};
		}

		std::optional<std::vector<css_font_face_source>> parse_sources(const css_token_vector& tokens) {
			std::vector<css_font_face_source> sources;
			for (const auto& entry : parse_comma_separated_list(tokens)) {
				if (entry.empty()) continue;
				const auto& source = entry.front();
				if (entry.size() == 1 && source.type == CV_FUNCTION && equal_i(source.name, "local")) {
					css_font_families names;
					if (parse_font_families(source.value, names) && names.size() == 1 &&
						!names.front().generic && !names.front().name.empty()) {
						sources.push_back({ std::move(names.front().name), true, false });
					}
					continue;
				}
				string url;
				if (!parse_url(source, url) || url.empty()) continue;
				size_t next = 1;
				bool variations = false;
				if (next < entry.size()) {
					if (const auto format = parse_format(entry[next])) {
						variations = *format;
						++next;
					}
				}
				if (next < entry.size() && entry[next].type == CV_FUNCTION && equal_i(entry[next].name, "tech")) {
					const auto options = parse_comma_separated_list(entry[next].value);
					if (!std::all_of(options.begin(), options.end(), [](const auto& option) {
						return option.size() == 1 && supports_font_tech(option.front().ident());
					})) continue;
					variations = true;
					++next;
				}
				if (next < entry.size()) continue;
				sources.push_back({ std::move(url), false, variations });
			}
			return sources.empty() ? std::nullopt : std::optional(std::move(sources));
		}

		std::optional<float> parse_weight(const css_token& token) {
			if (token.ident() == "normal") return 400.0f;
			if (token.ident() == "bold") return 700.0f;
			if (token.type == NUMBER && std::isfinite(token.n.number) && token.n.number >= 1.0f && token.n.number <= 1000.0f)
				return token.n.number;
			return std::nullopt;
		}

		template<typename Settings, typename ParseValue>
		std::optional<Settings> parse_opentype_settings(const css_token_vector& tokens, ParseValue parse_value) {
			Settings settings;
			if (tokens.size() == 1 && tokens.front().ident() == "normal") return settings;
			for (const auto& entry : parse_comma_separated_list(tokens)) {
				if (entry.empty() || entry.size() > 2 || entry.front().type != STRING || entry.front().str.size() != 4)
					return std::nullopt;
				uint32_t tag = 0;
				for (const unsigned char character : entry.front().str) {
					if (character < 0x20 || character > 0x7e) return std::nullopt;
					tag = (tag << 8) | character;
				}
				const auto value = parse_value(entry.size() == 2 ? &entry.back() : nullptr);
				if (!value) return std::nullopt;
				settings.insert_or_assign(tag, *value);
			}
			return settings;
		}
	}

	std::optional<css_font_features> parse_css_font_features(const css_token_vector& tokens) {
		return parse_opentype_settings<css_font_features>(tokens, [](const css_token* token) -> std::optional<uint32_t> {
			if (!token || token->ident() == "on") return 1;
			if (token->ident() == "off") return 0;
			constexpr double maximum = std::numeric_limits<int32_t>::max();
			if (token->type == NUMBER && token->n.number_type == css_number_integer &&
				std::isfinite(token->n.number) && token->n.number >= 0.0f)
				return static_cast<uint32_t>(std::min<double>(token->n.number, maximum));
			if (const auto value = evaluate_css_scalar_math(*token, css_math_scalar::number))
				return static_cast<uint32_t>(std::clamp(std::floor(*value + 0.5), 0.0, maximum));
			return std::nullopt;
		});
	}

	std::optional<css_font_variations> parse_css_font_variations(const css_token_vector& tokens) {
		return parse_opentype_settings<css_font_variations>(tokens, [](const css_token* token) -> std::optional<float> {
			if (!token) return std::nullopt;
			if (token->type == NUMBER && std::isfinite(token->n.number)) return token->n.number;
			if (const auto value = evaluate_css_scalar_math(*token, css_math_scalar::number)) {
				constexpr double maximum = std::numeric_limits<float>::max();
				return static_cast<float>(std::clamp(*value, -maximum, maximum));
			}
			return std::nullopt;
		});
	}

	bool parse_css_font_style(const css_token_vector& tokens, int& index, css_font_style& result) {
		const int kind = value_index(at(tokens, index).ident(), font_style_strings);
		if (kind < 0) return false;
		++index;
		result = { static_cast<font_style>(kind), 0.0f };
		if (kind == font_style_oblique) {
			result.angle = default_oblique_angle;
			if (const auto angle = parse_oblique_angle(at(tokens, index))) {
				result.angle = *angle;
				++index;
			}
		}
		return true;
	}

	std::optional<float> parse_css_font_width(const css_token& token, bool keywords_only) {
		static constexpr float widths[] = { 50.0f, 62.5f, 75.0f, 87.5f, 100.0f, 112.5f, 125.0f, 150.0f, 200.0f };
		const int keyword = value_index(token.ident(),
			"ultra-condensed;extra-condensed;condensed;semi-condensed;normal;semi-expanded;expanded;extra-expanded;ultra-expanded");
		if (keyword >= 0) return widths[keyword];
		if (keywords_only) return {};
		if (token.type == PERCENTAGE && std::isfinite(token.n.number) && token.n.number >= 0.0f)
			return token.n.number;
		if (const auto width = evaluate_css_scalar_math(token, css_math_scalar::percentage, 100.0))
			return static_cast<float>(std::clamp(*width, 0.0, double(std::numeric_limits<float>::max())));
		return {};
	}

	static std::optional<double> parse_nonnegative_percentage(const css_token& token) {
		if (token.type == PERCENTAGE && std::isfinite(token.n.number) && token.n.number >= 0) {
			return token.n.number / 100.0;
		}
		if (const auto value = evaluate_css_scalar_math(token, css_math_scalar::percentage); value && std::isfinite(*value)) {
			return std::max(0.0, *value);
		}
		return {};
	}

	static std::optional<css_font_metric_override> parse_metric_override(const css_token_vector& tokens) {
		if (tokens.empty() || tokens.size() > 2) return {};
		const auto parse = [](const css_token& token, std::optional<double>& ratio) {
			if (token.ident() == "normal") return true;
			ratio = parse_nonnegative_percentage(token);
			return ratio.has_value();
		};
		css_font_metric_override result;
		if (!parse(tokens.front(), result.horizontal)) return {};
		if (tokens.size() == 2 && !parse(tokens.back(), result.vertical)) return {};
		return result;
	}

	std::optional<css_font_face> parse_font_face_rule(const css_token_vector& prelude, const css_token_vector& block,
		const string& base_url) {
		if (std::any_of(prelude.begin(), prelude.end(), [](const css_token& token) { return token.type != WHITESPACE; }))
			return std::nullopt;
		css_font_face face;
		face.base_url = base_url;
		raw_declaration::vector declarations;
		raw_rule::vector nested;
		css_parser(block).consume_style_block_contents(declarations, nested);
		bool has_sources = false;
		for (auto& declaration : declarations) {
			if (declaration.important) continue;
			auto& value = declaration.value;
			const auto name = lowcase(declaration.name);
			if (name == "unicode-range") {
				if (auto ranges = parse_unicode_ranges(value))
					face.unicode_ranges = std::move(*ranges);
				continue;
			}
			remove_whitespace(value);
			if (value.empty()) continue;
			if (name == "font-family") {
				css_font_families families;
				if (parse_font_families(value, families) && families.size() == 1 && !families.front().generic)
					face.family = std::move(families.front().name);
			}
			else if (name == "src") {
				if (auto sources = parse_sources(value)) {
					face.sources = std::move(*sources);
					has_sources = true;
				}
			}
			else if (name == "font-style") {
				int index = 0;
				css_font_style style;
				if (!parse_css_font_style(value, index, style)) continue;
				float end = style.angle;
				if (style.kind == font_style_oblique && index < static_cast<int>(value.size()) && index == 2) {
					const auto angle = parse_oblique_angle(value[index++]);
					if (!angle) continue;
					end = *angle;
				}
				if (index != static_cast<int>(value.size())) continue;
				face.style = style.kind;
				face.min_angle = std::min(style.angle, end);
				face.max_angle = std::max(style.angle, end);
			}
			else if (name == "font-weight" && value.size() <= 2) {
				const auto first = parse_weight(value.front());
				const auto last = parse_weight(value.back());
				if (first && last) {
					face.min_weight = std::min(*first, *last);
					face.max_weight = std::max(*first, *last);
				}
			}
			else if (name == "font-feature-settings") {
				if (auto settings = parse_css_font_features(value)) face.features = std::move(*settings);
			}
			else if (name == "font-variation-settings") {
				if (auto settings = parse_css_font_variations(value)) face.variations = std::move(*settings);
			}
			else if (name == "ascent-override") {
				if (auto metrics = parse_metric_override(value)) face.metrics.ascent = *metrics;
			}
			else if (name == "descent-override") {
				if (auto metrics = parse_metric_override(value)) face.metrics.descent = *metrics;
			}
			else if (name == "line-gap-override") {
				if (auto metrics = parse_metric_override(value)) face.metrics.line_gap = *metrics;
			}
			else if (name == "size-adjust" && value.size() == 1) {
				if (auto ratio = parse_nonnegative_percentage(value.front())) face.size_adjust = *ratio;
			}
			else if (is_one_of(name, "font-width", "font-stretch") && value.size() <= 2) {
				if (value.size() == 1 && value.front().ident() == "auto") {
					face.width_range.reset();
					continue;
				}
				const auto first = parse_css_font_width(value.front());
				const auto last = parse_css_font_width(value.back());
				if (first && last) face.width_range = std::make_pair(std::min(*first, *last), std::max(*first, *last));
			}
		}
		if (face.family.empty() || !has_sources) return std::nullopt;
		return face;
	}
}
