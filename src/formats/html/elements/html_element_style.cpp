#include "html_element.h"

#include "formats/html/css/css_declarations.h"
#include "formats/html/css/html_cssom.h"
#include "formats/html/html_layout.h"
#include <litehtml/css_parser.h>

using namespace godot;
namespace syntax = dockit::css;

namespace {
	using namespace litehtml;

	css_parsed_declaration parse(const syntax::raw_declaration& declaration) {
		auto tokens = declaration.value;
		syntax::remove_whitespace(tokens);
		return parse_css_declaration(_id(declaration.name), tokens, {}, declaration.important, nullptr, {});
	}

	struct InlineValue {
		std::string text;
		bool important = false;
		bool found = false;
	};

	InlineValue read(const syntax::raw_declaration::vector& declarations, const std::string& name) {
		const auto id = css_canonical_property(_id(name));
		InlineValue result;
		for (const auto& declaration : declarations) {
			if (result.important && !declaration.important) continue;
			const auto parsed = parse(declaration);
			if (declaration.name == name && (parsed.status == css_declaration_status::parsed ||
				parsed.status == css_declaration_status::deferred)) {
				result = {syntax::get_repr(declaration.value), declaration.important, true};
			} else if (const auto found = parsed.properties.find(id); found != parsed.properties.end()) {
				result = {found->second.deferred ? std::string() : syntax::serialize_value(id, found->second),
					declaration.important, true};
			}
		}
		if (syntax::is_box_shorthand(id)) {
			std::vector<std::string> values;
			std::optional<bool> priority;
			for (const auto key : css_longhands(id)) {
				const auto part = read(declarations, _s(key));
				if (!part.found || (priority && *priority != part.important)) return {};
				values.push_back(part.text);
				priority = part.important;
			}
			const auto text = syntax::serialize_box_shorthand(std::move(values));
			if (!text.empty()) return {text, priority.value_or(false), true};
		}
		return result;
	}

	Error edit(HTMLElement& element, const std::string& name, const syntax::raw_declaration* replacement) {
		const auto id = css_canonical_property(_id(name));
		const auto& longhands = css_longhands(id);
		const auto affects = [&](string_id property) {
			return property == id || std::find(longhands.begin(), longhands.end(), property) != longhands.end();
		};
		const auto declarations = syntax::inline_declarations(element.get_attribute("style"));
		syntax::raw_declaration::vector result;
		for (const auto& declaration : declarations) {
			if (css_canonical_property(_id(declaration.name)) == id) continue;
			if (!replacement || declaration.important == replacement->important || declaration.important) {
				const auto parsed = parse(declaration);
				if (std::any_of(parsed.properties.begin(), parsed.properties.end(), [&](const auto& entry) { return affects(entry.first); })) {
					syntax::raw_declaration::vector expanded_declarations;
					bool serializable = true;
					for (const auto& [property, value] : parsed.properties) {
						if (affects(property)) continue;
						auto text = syntax::serialize_value(property, value);
						if (text.empty() && value.is<invalid>() && !value.deferred) text = "unset";
						const std::string property_name = _s(property);
						if (text.empty() || css_canonical_property(_id(property_name)) != property) {
							serializable = false;
							break;
						}
						syntax::raw_declaration expanded{property_name, litehtml::normalize(text, syntax::f_componentize), value.m_important};
						if (parse(expanded).status != css_declaration_status::parsed) {
							serializable = false;
							break;
						}
						expanded_declarations.push_back(std::move(expanded));
					}
					if (serializable) {
						for (auto& expanded : expanded_declarations) result.push_back(std::move(expanded));
						continue;
					}
					if (!replacement || (declaration.important && !replacement->important)) return ERR_UNAVAILABLE;
				}
			}
			result.push_back(declaration);
		}
		if (replacement) result.push_back(*replacement);
		const auto text = syntax::serialize_declarations(result);
		return element.set_attribute("style", String::utf8(text.c_str()));
	}
}

String HTMLElement::get_style_property(const String& p_name) const {
	const auto result = read(syntax::inline_declarations(get_attribute("style")), syntax::property_name(p_name));
	return String::utf8(result.text.c_str());
}

String HTMLElement::get_style_property_priority(const String& p_name) const {
	return read(syntax::inline_declarations(get_attribute("style")), syntax::property_name(p_name)).important ? "important" : "";
}

Dictionary HTMLElement::get_style_properties() const {
	Dictionary result;
	const auto declarations = syntax::inline_declarations(get_attribute("style"));
	for (const auto& declaration : declarations) {
		const auto value = read(declarations, declaration.name);
		if (value.found) result[String::utf8(declaration.name.c_str())] = String::utf8(value.text.c_str());
	}
	return result;
}

Error HTMLElement::set_style_property(const String& p_name, const String& p_value, const String& p_priority) {
	if (!is_valid()) return ERR_INVALID_PARAMETER;
	const auto name = syntax::property_name(p_name);
	const auto name_tokens = litehtml::normalize(name, syntax::f_componentize);
	if (name_tokens.size() != 1 || name_tokens.front().type != syntax::IDENT || name_tokens.front().str != name ||
		(!p_priority.is_empty() && p_priority.nocasecmp_to("important") != 0)) return ERR_INVALID_PARAMETER;
	if (p_value.is_empty()) return edit(*this, name, nullptr);
	syntax::raw_declaration declaration{name, litehtml::normalize(p_value.utf8().get_data(), syntax::f_componentize), !p_priority.is_empty()};
	if (!syntax::is_declaration_value(declaration.value)) return ERR_INVALID_DATA;
	const auto status = parse(declaration).status;
	if (status != litehtml::css_declaration_status::parsed && status != litehtml::css_declaration_status::deferred) return ERR_INVALID_DATA;
	return edit(*this, name, &declaration);
}

String HTMLElement::remove_style_property(const String& p_name) {
	if (!is_valid()) return {};
	const auto name = syntax::property_name(p_name);
	const auto value = read(syntax::inline_declarations(get_attribute("style")), name);
	if (!value.found || edit(*this, name, nullptr) != OK) return {};
	return String::utf8(value.text.c_str());
}

TypedArray<HTMLAnimation> HTMLElement::get_animations() {
	auto* owner = get_owner();
	return owner ? owner->get_animations(Ref<HTMLElement>(this)) : TypedArray<HTMLAnimation>();
}

Ref<HTMLAnimation> HTMLElement::animate(const TypedArray<Dictionary>& p_keyframes, const Dictionary& p_options) {
	auto* owner = get_owner();
	return owner ? owner->animate(Ref<HTMLElement>(this), p_keyframes, p_options) : Ref<HTMLAnimation>();
}
