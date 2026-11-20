// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#pragma once

#include "css_parser.h"

#include <optional>

namespace dockit::css {

enum class StylesheetRuleKind {
	style,
	declarations,
	layer_block,
	layer_order,
	import,
	media,
	container,
	supports,
	scope,
	font_face,
	font_feature_values,
	counter_style,
	keyframes,
	property,
};

struct StylesheetImport {
	string url;
	bool layered = false;
	vector<string> layer;
	const css_token_vector* supports = nullptr;
	css_token_vector media;
};

struct StylesheetRule {
	StylesheetRuleKind kind;
	const raw_rule& syntax;
	vector<vector<string>> layers{};
	std::optional<StylesheetImport> import{};
};

// The visitor consumes records synchronously and reports whether their semantic grammar is valid.
// Token references remain valid for the duration of the call.
void visit_stylesheet_rules(const raw_rule::vector& rules, bool top_level,
	const std::function<bool(const StylesheetRule&)>& visit);

bool parse_url_token(const css_token& token, string& url);
bool parse_import_url(const css_token& token, string& url);

} // namespace dockit::css
