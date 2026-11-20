// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_stylesheet_rules.h"

#include "css_syntax_util.h"

#include <utility>

namespace dockit::css {

namespace {

using namespace detail;
using Kind = StylesheetRuleKind;

bool parse_layer_name(const css_token_vector& tokens, vector<string>& names) {
	size_t first = 0;
	size_t last = tokens.size();
	while (first < last && tokens[first].type == WHITESPACE) ++first;
	while (last > first && tokens[last - 1].type == WHITESPACE) --last;
	for (size_t index = first; index < last; ++index) {
		const auto& token = tokens[index];
		if ((index - first) % 2 == 1) {
			if (token.type != '.') return false;
		} else {
			if (token.type != IDENT || is_one_of(token.ident(), "initial", "inherit", "unset", "revert", "revert-layer")) {
				return false;
			}
			names.push_back(token.name);
		}
	}
	return (last - first) % 2 == 1;
}

std::optional<StylesheetRule> parse_layer(const raw_rule& rule) {
	if (rule.block.type == CURLY_BLOCK) {
		vector<string> names;
		const bool anonymous = std::all_of(rule.prelude.begin(), rule.prelude.end(),
			[](const css_token& token) { return token.type == WHITESPACE; });
		if (!anonymous && !parse_layer_name(rule.prelude, names)) return {};
		return StylesheetRule{Kind::layer_block, rule, {std::move(names)}};
	}

	const auto list = parse_comma_separated_list(rule.prelude);
	vector<vector<string>> paths(list.size());
	for (size_t index = 0; index < list.size(); ++index) {
		if (!parse_layer_name(list[index], paths[index])) return {};
	}
	return StylesheetRule{Kind::layer_order, rule, std::move(paths)};
}

std::optional<StylesheetImport> parse_import(const raw_rule& rule) {
	const auto& tokens = rule.prelude;
	int index = 0;
	skip_whitespace(tokens, index);
	StylesheetImport result;
	if (!parse_import_url(at(tokens, index), result.url) || rule.block.type == CURLY_BLOCK) {
		css_parse_error("invalid @import rule");
		return {};
	}
	skip_whitespace(tokens, ++index);
	const auto& layer = at(tokens, index);
	const bool named_layer = layer.type == CV_FUNCTION && lowcase(layer.name) == "layer";
	result.layered = named_layer || layer.ident() == "layer";
	if (named_layer && !parse_layer_name(layer.value, result.layer)) return {};
	if (result.layered) skip_whitespace(tokens, ++index);

	const auto& condition = at(tokens, index);
	if (condition.type == CV_FUNCTION && lowcase(condition.name) == "supports") {
		result.supports = &condition.value;
		++index;
	}
	result.media = slice(tokens, index);
	return result;
}

std::optional<Kind> block_rule_kind(const string& name) {
	if (name == "media") return Kind::media;
	if (name == "container") return Kind::container;
	if (name == "supports") return Kind::supports;
	if (name == "scope") return Kind::scope;
	if (name == "font-face") return Kind::font_face;
	if (name == "font-feature-values") return Kind::font_feature_values;
	if (name == "counter-style") return Kind::counter_style;
	if (name == "keyframes") return Kind::keyframes;
	if (name == "property") return Kind::property;
	return {};
}

} // namespace

bool parse_url_token(const css_token& token, string& url) {
	const string* text = nullptr;
	if (token.type == URL) {
		text = &token.str;
	} else if (token.type == CV_FUNCTION && detail::is_one_of(detail::lowcase(token.name), "url", "src") &&
		token.value.size() == 1 && token.value.front().type == STRING) {
		text = &token.value.front().str;
	}
	if (!text) return false;
	const auto start = text->find_first_not_of(" \n\r\t\f");
	url = start == string::npos ? string{} : text->substr(start, text->find_last_not_of(" \n\r\t\f") - start + 1);
	return true;
}

bool parse_import_url(const css_token& token, string& url) {
	if (token.type == STRING) {
		url = token.str;
		return true;
	}
	return parse_url_token(token, url);
}

void visit_stylesheet_rules(const raw_rule::vector& rules, bool top_level,
	const std::function<bool(const StylesheetRule&)>& visit) {
	bool import_allowed = top_level;
	bool saw_import = false;
	for (const auto& rule : rules) {
		if (rule->type == raw_rule::declaration_list) {
			if (visit({Kind::declarations, *rule})) import_allowed = false;
			continue;
		}
		if (rule->type == raw_rule::qualified) {
			if (visit({Kind::style, *rule})) import_allowed = false;
			continue;
		}

		const auto name = detail::lowcase(rule->name);
		if (name == "charset") continue;
		if (name == "layer") {
			if (const auto layer = parse_layer(*rule)) {
				if (layer->kind == Kind::layer_block || saw_import) import_allowed = false;
				visit(*layer);
			}
			continue;
		}
		if (name == "import") {
			if (import_allowed) {
				if (auto parsed = parse_import(*rule)) {
					saw_import = visit({Kind::import, *rule, {}, std::move(parsed)}) || saw_import;
				}
			} else {
				css_parse_error("incorrect placement of @import rule");
			}
			continue;
		}
		if (const auto kind = block_rule_kind(name)) {
			if (rule->block.type == CURLY_BLOCK && visit({*kind, *rule})) import_allowed = false;
		} else {
			css_parse_error("unrecognized rule @" + rule->name);
		}
	}
}

} // namespace dockit::css
