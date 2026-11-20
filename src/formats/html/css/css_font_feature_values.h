#pragma once

#include "css_font_face.h"

#include <array>

namespace litehtml {
	enum class css_alternate_type { stylistic, styleset, character_variant, swash, ornaments, annotation, count };
	constexpr size_t css_alternate_count = static_cast<size_t>(css_alternate_type::count);
	struct css_named_alternate {
		css_alternate_type type;
		string name;
	};

	struct css_font_alternates {
		bool historical_forms = false;
		std::vector<css_named_alternate> names;

		std::string cache_key() const;
	};

	using css_font_feature_names = std::array<std::map<string, std::vector<uint32_t>>, css_alternate_count>;

	struct css_font_feature_values_rule {
		string_vector families;
		css_font_feature_names names;

		bool operator==(const css_font_feature_values_rule& other) const {
			return families == other.families && names == other.names;
		}
	};

	std::optional<css_font_alternates> parse_css_font_alternates(const css_token_vector& tokens);
	std::optional<css_font_feature_values_rule> parse_font_feature_values_rule(const css_token_vector& prelude,
		const css_token_vector& contents);
	css_font_features resolve_font_feature_names(const css_font_alternates& alternates, const css_font_feature_names& definitions);
}
