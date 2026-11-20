#include "css_font_feature_values.h"

#include "formats/html/css/css_values.h"

#include <litehtml/css_parser.h>
#include <litehtml/html.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace litehtml {
	namespace {
		constexpr std::array<std::string_view, css_alternate_count> feature_names = {
			"stylistic", "styleset", "character-variant", "swash", "ornaments", "annotation",
		};

		size_t feature_index(const string& name) {
			return std::find(feature_names.begin(), feature_names.end(), lowcase(name)) - feature_names.begin();
		}

		constexpr uint32_t tag(char a, char b, char c, char d) {
			return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(c) << 8) | uint32_t(d);
		}

		uint32_t numbered_tag(char a, char b, uint32_t index) {
			return tag(a, b, char('0' + index / 10), char('0' + index % 10));
		}
	}

	std::string css_font_alternates::cache_key() const {
		std::string key = historical_forms ? ":hist" : "";
		for (const auto& [type, name] : names) {
			key += ":alt=" + std::to_string(size_t(type)) + ':' + std::to_string(name.size()) + ':' + name;
		}
		return key;
	}

	std::optional<css_font_alternates> parse_css_font_alternates(const css_token_vector& tokens) {
		css_font_alternates result;
		if (tokens.size() == 1 && tokens.front().ident() == "normal") return result;
		if (tokens.empty()) return std::nullopt;
		std::array<bool, css_alternate_count> seen{};
		for (const auto& token : tokens) {
			if (token.ident() == "historical-forms" && !result.historical_forms) {
				result.historical_forms = true;
				continue;
			}
			if (token.type != CV_FUNCTION) return std::nullopt;
			const auto type = feature_index(token.name);
			if (type == css_alternate_count || seen[type]) return std::nullopt;
			seen[type] = true;
			const auto args = parse_comma_separated_list(token.value);
			if (args.size() != 1 && type != size_t(css_alternate_type::styleset) &&
					type != size_t(css_alternate_type::character_variant)) return std::nullopt;
			for (const auto& arg : args) {
				if (arg.size() != 1 || arg.front().type != IDENT) return std::nullopt;
				result.names.push_back({ static_cast<css_alternate_type>(type), arg.front().name });
			}
		}
		return result;
	}

	std::optional<css_font_feature_values_rule> parse_font_feature_values_rule(const css_token_vector& prelude,
		const css_token_vector& contents) {
		css_font_families families;
		auto family_tokens = prelude;
		remove_whitespace(family_tokens);
		if (!parse_font_families(family_tokens, families)) return std::nullopt;
		css_font_feature_values_rule result;
		for (auto& family : families) {
			if (family.generic) return std::nullopt;
			result.families.push_back(std::move(family.name));
		}
		raw_declaration::vector descriptors;
		raw_rule::vector blocks;
		css_parser(contents).consume_style_block_contents(descriptors, blocks);
		for (const auto& block : blocks) {
			if (block->type != raw_rule::at || block->block.type != CURLY_BLOCK ||
					std::any_of(block->prelude.begin(), block->prelude.end(), [](const auto& token) { return token.type != WHITESPACE; })) continue;
			const auto type = feature_index(block->name);
			if (type == css_alternate_count) continue;
			const bool styleset = type == size_t(css_alternate_type::styleset);
			const bool character_variant = type == size_t(css_alternate_type::character_variant);
			raw_declaration::vector declarations;
			raw_rule::vector nested;
			css_parser(block->block.value).consume_style_block_contents(declarations, nested);
			for (auto& declaration : declarations) {
				if (declaration.important) continue;
				remove_whitespace(declaration.value);
				if (declaration.value.empty() || (!styleset && declaration.value.size() > (character_variant ? 2 : 1))) continue;
				std::vector<uint32_t> indexes;
				for (const auto& value : declaration.value) {
					if (value.type != NUMBER || value.n.number_type != css_number_integer ||
							!std::isfinite(value.n.number) || value.n.number < 0) break;
					indexes.push_back(static_cast<uint32_t>(std::min<double>(value.n.number, std::numeric_limits<int32_t>::max())));
				}
				if (indexes.size() == declaration.value.size()) {
					result.names[type].insert_or_assign(std::move(declaration.name), std::move(indexes));
				}
			}
		}
		return result;
	}

	css_font_features resolve_font_feature_names(const css_font_alternates& alternates, const css_font_feature_names& definitions) {
		css_font_features result;
		for (const auto& [type, name] : alternates.names) {
			const auto& feature_names = definitions[size_t(type)];
			const auto definition = feature_names.find(name);
			if (definition == feature_names.end()) continue;
			const auto& values = definition->second;
			switch (type) {
			case css_alternate_type::stylistic: result[tag('s', 'a', 'l', 't')] = values.front(); break;
			case css_alternate_type::styleset:
				for (const auto index : values) {
					if (index >= 1 && index <= 99) result[numbered_tag('s', 's', index)] = 1;
				}
				break;
			case css_alternate_type::character_variant:
				if (values.front() >= 1 && values.front() <= 99)
					result[numbered_tag('c', 'v', values.front())] = values.size() == 2 ? values[1] : 1;
				break;
			case css_alternate_type::swash:
				result[tag('s', 'w', 's', 'h')] = values.front();
				result[tag('c', 's', 'w', 'h')] = values.front();
				break;
			case css_alternate_type::ornaments: result[tag('o', 'r', 'n', 'm')] = values.front(); break;
			case css_alternate_type::annotation: result[tag('n', 'a', 'l', 't')] = values.front(); break;
			case css_alternate_type::count: break;
			}
		}
		return result;
	}
}
