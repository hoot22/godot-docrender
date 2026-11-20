#pragma once

#include "css_token_compat.h"

#include <functional>
#include <memory>
#include <optional>

namespace litehtml {
	enum class counter_system { cyclic, fixed, symbolic, alphabetic, numeric, additive, extends };

	struct css_counter_style {
		counter_system system = counter_system::symbolic;
		int first = 1;
		string base;
		string_vector symbols;
		std::vector<std::pair<int, string>> additive;
		std::optional<std::vector<std::pair<int, int>>> ranges;
		std::optional<std::pair<string, string>> negative;
		std::optional<std::pair<int, string>> pad;
		std::optional<string> prefix;
		std::optional<string> suffix;
		std::optional<string> fallback;
	};

	struct css_counter_style_reference {
		string name;
		std::shared_ptr<const css_counter_style> anonymous;
	};

	using css_counter_style_lookup = std::function<const css_counter_style*(const string&)>;

	std::optional<int> parse_counter_integer(const css_token& token);
	std::optional<css_counter_style_reference> parse_counter_style_reference(const css_token& token, bool allow_none);
	std::optional<std::pair<string, css_counter_style>> parse_counter_style_rule(const css_token_vector& prelude,
		const css_token_vector& block);
	string format_css_counter(int value, const css_counter_style_reference& reference,
		const css_counter_style_lookup& lookup, bool marker = false);
}
