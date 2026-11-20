// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include <litehtml/html.h>
#include <litehtml/style.h>
#include <litehtml/css_parser.h>
#include <litehtml/document_container.h>
#include <litehtml/html_tag.h>
#include "formats/html/css/css_declarations.h"
#include <litehtml/document.h>

namespace litehtml
{

void style::add(const string& txt, const string& baseurl, document_container* container, const html_tag* element)
{
	add(normalize(txt, f_componentize), baseurl, container, element);
}

void style::add(const css_token_vector& tokens, const string& baseurl, document_container* container, const html_tag* element)
{
	raw_declaration::vector declarations;
	raw_rule::vector rules;
	css_parser(tokens).consume_style_block_contents(declarations, rules);
	if (!rules.empty())
		css_parse_error("rule inside a style block");
	add_declarations(declarations, baseurl, container, element);
}

void style::add_declarations(const raw_declaration::vector& declarations, const string& baseurl,
	document_container* container, const html_tag* element)
{
	const auto source = container ? container->get_css_source(baseurl, element != nullptr) : css_source{};
	for (const auto& declaration : declarations)
	{
		auto value = declaration.value;
		remove_whitespace(value);
		const auto& name = declaration.name;
		const auto id = _id(name.compare(0, 2, "--") == 0 ? name : lowcase(name));
		add_declaration(id, value, baseurl, declaration.important, container, source, element);
	}
}

void style::add_property(string_id name, const css_token_vector& tokens, const string& baseurl, bool important,
	document_container* container)
{
	add_declaration(name, tokens, baseurl, important, container, {}, nullptr);
}

void style::add_property(string_id name, const string& value, const string& baseurl, bool important, document_container* container)
{
	add_property(name, normalize(value, f_componentize | f_remove_whitespace), baseurl, important, container);
}

void style::add_declaration(string_id name, const css_token_vector& tokens, const string& baseurl, bool important,
	document_container* container, const css_source& source, const html_tag* element)
{
	auto result = parse_css_declaration(name, tokens, baseurl, important, container, source);
	if (auto diagnostic = check_css_declaration(result.status, name, tokens, source))
	{
		if (container) container->report_css_diagnostic(*diagnostic, element);
		if (!element) remember_diagnostic(*diagnostic);
	}
	for (auto& entry : result.properties)
	{
		cascade.add(entry.first, std::move(entry.second),
			{element ? css_inline_layer : css_default_layer, {}, 0});
	}
}

void style::remember_diagnostic(const css_diagnostic& diagnostic)
{
	if (!diagnostics) diagnostics = std::make_shared<std::vector<css_diagnostic>>();
	const auto same_declaration = [&](const css_diagnostic& other) {
		return diagnostic.kind == other.kind && diagnostic.property == other.property && diagnostic.value == other.value &&
			diagnostic.source.kind == other.source.kind && diagnostic.source.path == other.source.path;
	};
	if (std::none_of(diagnostics->begin(), diagnostics->end(), same_declaration))
		diagnostics->push_back(diagnostic);
}

void style::combine(const style& src, dockit::css::CascadePriority priority)
{
	if (src.diagnostics)
	{
		for (const auto& diagnostic : *src.diagnostics)
			remember_diagnostic(diagnostic);
	}
	cascade.merge(src.cascade, priority);
}

void style::snapshot_defaults()
{
	cascade.snapshot_defaults();
}

const property_value& style::get_property(string_id name) const
{
	return cascade.get(name);
}

const css_token_vector* style::get_custom_property(string_id name, bool& local) const
{
	return cascade.get_custom_property(name, local);
}

void style::resolve_cascade(const html_tag& element, bool use_element_variables)
{
	const auto doc = element.get_document();
	const auto parent = element.inheritance_parent();
	const auto* parent_tag = use_element_variables ? &element : dynamic_cast<const html_tag*>(parent.get());
	const auto registered = doc->registered_properties();
	cascade.resolve(doc->has_author_layers(), parent_tag && parent_tag->css().is_right_to_left(),
		[&](string_id name, css_token_vector& tokens) {
		return parent_tag && parent_tag->get_custom_property(name, tokens);
	}, registered, doc->container(), [&](const auto& declaration, const auto& tokens, bool important) {
		return reparse_css_declaration(declaration, tokens, important, doc->container());
	});
}

} // namespace litehtml
