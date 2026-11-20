#include "html_metadata_elements.h"
#include "html_stylesheet_node.h"

#include "formats/html/backend/litehtml_document_adapter.h"
#include "formats/html/css/html_style_sheet.h"
#include "formats/html/html_layout.h"

#include <litehtml/html.h>
#include <litehtml/document_container.h>

using namespace godot;

namespace dockit {
	bool HTMLStylesheetNode::accepts_style_text() const {
		const auto type = litehtml::lowcase(get_attr("type", ""));
		return type.empty() || type == "text/css";
	}

	void HTMLStylesheetNode::parse_attributes() {
		const auto doc = get_document();
		if (tag() == litehtml::_link_) {
			doc->container()->link(doc, shared_from_this());
		} else if (accepts_style_text()) {
			litehtml::string text;
			get_text(text);
			doc->add_stylesheet(text.c_str(), nullptr, get_attr("media"), parent().get(), shared_from_this(), style_disabled);
		}
	}

	void HTMLStylesheetNode::compute_styles(bool p_recursive) {
		if (tag() == litehtml::_style_) css_w().set_display(litehtml::display_none);
		else html_tag::compute_styles(p_recursive);
	}

	bool HTMLStylesheetNode::removeChild(const litehtml::element::ptr& p_child) {
		if (!html_tag::removeChild(p_child)) return false;
		if (tag() == litehtml::_style_) litehtml::dockit_editor::reset_stylesheet_source(*this);
		return true;
	}
}

void HTMLTitleElement::_bind_methods() {
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text_content", "get_text_content");
}

void HTMLMetaElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &HTMLMetaElement::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "value"), &HTMLMetaElement::set_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ClassDB::bind_method(D_METHOD("get_content"), &HTMLMetaElement::get_content);
	ClassDB::bind_method(D_METHOD("set_content", "value"), &HTMLMetaElement::set_content);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "content"), "set_content", "get_content");
	ClassDB::bind_method(D_METHOD("get_http_equiv"), &HTMLMetaElement::get_http_equiv);
	ClassDB::bind_method(D_METHOD("set_http_equiv", "value"), &HTMLMetaElement::set_http_equiv);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "http_equiv"), "set_http_equiv", "get_http_equiv");
	ClassDB::bind_method(D_METHOD("get_media"), &HTMLMetaElement::get_media);
	ClassDB::bind_method(D_METHOD("set_media", "value"), &HTMLMetaElement::set_media);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "media"), "set_media", "get_media");
	ClassDB::bind_method(D_METHOD("get_charset"), &HTMLMetaElement::get_charset);
	ClassDB::bind_method(D_METHOD("set_charset", "value"), &HTMLMetaElement::set_charset);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "charset"), "set_charset", "get_charset");
}

String HTMLMetaElement::get_name() const { return get_attribute("name"); }
void HTMLMetaElement::set_name(const String& p_value) { set_attribute("name", p_value); }

String HTMLMetaElement::get_content() const { return get_attribute("content"); }
void HTMLMetaElement::set_content(const String& p_value) { set_attribute("content", p_value); }

String HTMLMetaElement::get_http_equiv() const { return get_attribute("http-equiv"); }
void HTMLMetaElement::set_http_equiv(const String& p_value) { set_attribute("http-equiv", p_value); }

String HTMLMetaElement::get_media() const { return get_attribute("media"); }
void HTMLMetaElement::set_media(const String& p_value) { set_attribute("media", p_value); }

String HTMLMetaElement::get_charset() const { return get_attribute("charset"); }
void HTMLMetaElement::set_charset(const String& p_value) { set_attribute("charset", p_value); }

void HTMLBaseElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_href"), &HTMLBaseElement::get_href);
	ClassDB::bind_method(D_METHOD("set_href", "value"), &HTMLBaseElement::set_href);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "href"), "set_href", "get_href");
	ClassDB::bind_method(D_METHOD("get_target"), &HTMLBaseElement::get_target);
	ClassDB::bind_method(D_METHOD("set_target", "value"), &HTMLBaseElement::set_target);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "target"), "set_target", "get_target");
	ClassDB::bind_method(D_METHOD("get_resolved_href"), &HTMLBaseElement::get_resolved_href);
}

String HTMLBaseElement::get_href() const { return get_attribute("href"); }
void HTMLBaseElement::set_href(const String& p_value) { set_attribute("href", p_value); }

String HTMLBaseElement::get_target() const { return get_attribute("target"); }
void HTMLBaseElement::set_target(const String& p_value) { set_attribute("target", p_value); }

String HTMLBaseElement::get_resolved_href() const {
	const auto* owner = get_owner();
	return owner ? owner->resolve_link_href(get_href(), true) : String();
}

void HTMLScriptElement::_bind_methods() {
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text_content", "get_text_content");
	ClassDB::bind_method(D_METHOD("get_src"), &HTMLScriptElement::get_src);
	ClassDB::bind_method(D_METHOD("set_src", "value"), &HTMLScriptElement::set_src);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "src"), "set_src", "get_src");
	ClassDB::bind_method(D_METHOD("get_type"), &HTMLScriptElement::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "value"), &HTMLScriptElement::set_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");
	ClassDB::bind_method(D_METHOD("is_async"), &HTMLScriptElement::is_async);
	ClassDB::bind_method(D_METHOD("set_async", "value"), &HTMLScriptElement::set_async);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "async"), "set_async", "is_async");
	ClassDB::bind_method(D_METHOD("is_defer"), &HTMLScriptElement::is_defer);
	ClassDB::bind_method(D_METHOD("set_defer", "value"), &HTMLScriptElement::set_defer);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "defer"), "set_defer", "is_defer");
	ClassDB::bind_method(D_METHOD("is_no_module"), &HTMLScriptElement::is_no_module);
	ClassDB::bind_method(D_METHOD("set_no_module", "value"), &HTMLScriptElement::set_no_module);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_module"), "set_no_module", "is_no_module");
}

String HTMLScriptElement::get_src() const { return get_attribute("src"); }
void HTMLScriptElement::set_src(const String& p_value) { set_attribute("src", p_value); }

String HTMLScriptElement::get_type() const { return get_attribute("type"); }
void HTMLScriptElement::set_type(const String& p_value) { set_attribute("type", p_value); }

bool HTMLScriptElement::is_async() const { return has_attribute("async"); }
void HTMLScriptElement::set_async(bool p_value) {
	if (p_value) set_attribute("async", "");
	else remove_attribute("async");
}

bool HTMLScriptElement::is_defer() const { return has_attribute("defer"); }
void HTMLScriptElement::set_defer(bool p_value) {
	if (p_value) set_attribute("defer", "");
	else remove_attribute("defer");
}

bool HTMLScriptElement::is_no_module() const { return has_attribute("nomodule"); }
void HTMLScriptElement::set_no_module(bool p_value) {
	if (p_value) set_attribute("nomodule", "");
	else remove_attribute("nomodule");
}

void HTMLLinkElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_href"), &HTMLLinkElement::get_href);
	ClassDB::bind_method(D_METHOD("set_href", "value"), &HTMLLinkElement::set_href);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "href"), "set_href", "get_href");
	ClassDB::bind_method(D_METHOD("get_rel"), &HTMLLinkElement::get_rel);
	ClassDB::bind_method(D_METHOD("set_rel", "value"), &HTMLLinkElement::set_rel);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "rel"), "set_rel", "get_rel");
	ClassDB::bind_method(D_METHOD("get_type"), &HTMLLinkElement::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "value"), &HTMLLinkElement::set_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");
	ClassDB::bind_method(D_METHOD("get_media"), &HTMLLinkElement::get_media);
	ClassDB::bind_method(D_METHOD("set_media", "value"), &HTMLLinkElement::set_media);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "media"), "set_media", "get_media");
	ClassDB::bind_method(D_METHOD("is_disabled"), &HTMLLinkElement::is_disabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "value"), &HTMLLinkElement::set_disabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ClassDB::bind_method(D_METHOD("get_stylesheet_id"), &HTMLLinkElement::get_stylesheet_id);
	ClassDB::bind_method(D_METHOD("get_sheet"), &HTMLLinkElement::get_sheet);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "sheet", PROPERTY_HINT_NONE, "",
		PROPERTY_USAGE_DEFAULT, "HTMLStyleSheet"), "", "get_sheet");
	ClassDB::bind_method(D_METHOD("get_resolved_href"), &HTMLLinkElement::get_resolved_href);
}

String HTMLLinkElement::get_href() const { return get_attribute("href"); }
void HTMLLinkElement::set_href(const String& p_value) { set_attribute("href", p_value); }

String HTMLLinkElement::get_rel() const { return get_attribute("rel"); }
void HTMLLinkElement::set_rel(const String& p_value) { set_attribute("rel", p_value); }

String HTMLLinkElement::get_type() const { return get_attribute("type"); }
void HTMLLinkElement::set_type(const String& p_value) { set_attribute("type", p_value); }

String HTMLLinkElement::get_media() const { return get_attribute("media"); }
void HTMLLinkElement::set_media(const String& p_value) { set_attribute("media", p_value); }

int64_t HTMLLinkElement::get_stylesheet_id() const {
	auto* owner = get_owner();
	return owner ? owner->get_stylesheet_id(*this) : 0;
}

Ref<HTMLStyleSheet> HTMLLinkElement::get_sheet() const {
	auto* owner = get_owner();
	return owner ? owner->get_stylesheet(*this) : Ref<HTMLStyleSheet>();
}

String HTMLLinkElement::get_resolved_href() const {
	const auto* owner = get_owner();
	return owner && has_attribute("href") ? owner->resolve_link_href(get_href()) : String();
}

bool HTMLLinkElement::is_disabled() const { return has_attribute("disabled"); }

void HTMLLinkElement::set_disabled(bool p_value) {
	if (p_value) set_attribute("disabled", "");
	else remove_attribute("disabled");
}

void HTMLStyleElement::_bind_methods() {
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text_content", "get_text_content");
	ClassDB::bind_method(D_METHOD("get_type"), &HTMLStyleElement::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "value"), &HTMLStyleElement::set_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");
	ClassDB::bind_method(D_METHOD("get_media"), &HTMLStyleElement::get_media);
	ClassDB::bind_method(D_METHOD("set_media", "value"), &HTMLStyleElement::set_media);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "media"), "set_media", "get_media");
	ClassDB::bind_method(D_METHOD("is_disabled"), &HTMLStyleElement::is_disabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "value"), &HTMLStyleElement::set_disabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ClassDB::bind_method(D_METHOD("get_stylesheet_id"), &HTMLStyleElement::get_stylesheet_id);
	ClassDB::bind_method(D_METHOD("get_sheet"), &HTMLStyleElement::get_sheet);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "sheet", PROPERTY_HINT_NONE, "",
		PROPERTY_USAGE_DEFAULT, "HTMLStyleSheet"), "", "get_sheet");
}

String HTMLStyleElement::get_type() const { return get_attribute("type"); }
void HTMLStyleElement::set_type(const String& p_value) { set_attribute("type", p_value); }

String HTMLStyleElement::get_media() const { return get_attribute("media"); }
void HTMLStyleElement::set_media(const String& p_value) { set_attribute("media", p_value); }

int64_t HTMLStyleElement::get_stylesheet_id() const {
	auto* owner = get_owner();
	return owner ? owner->get_stylesheet_id(*this) : 0;
}

Ref<HTMLStyleSheet> HTMLStyleElement::get_sheet() const {
	auto* owner = get_owner();
	return owner ? owner->get_stylesheet(*this) : Ref<HTMLStyleSheet>();
}

bool HTMLStyleElement::is_disabled() const {
	const auto node = std::dynamic_pointer_cast<dockit::HTMLStylesheetNode>(lock_element());
	return node && node->accepts_style_text() && node->style_disabled && is_connected();
}

void HTMLStyleElement::set_disabled(bool p_value) {
	const auto node = std::dynamic_pointer_cast<dockit::HTMLStylesheetNode>(lock_element());
	if (!node || !node->accepts_style_text() || node->style_disabled == p_value || !is_connected()) return;
	node->style_disabled = p_value;
	get_owner()->invalidate_styles();
}
