#include "html_style_sheet.h"

#include "formats/html/dom/html_dom.h"
#include "formats/html/elements/html_stylesheet_node.h"
#include "formats/html/html_layout.h"

using namespace godot;

void HTMLStyleSheet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &HTMLStyleSheet::is_valid);
	ClassDB::bind_method(D_METHOD("get_id"), &HTMLStyleSheet::get_id);
	ClassDB::bind_method(D_METHOD("get_owner_node"), &HTMLStyleSheet::get_owner_node);
	ClassDB::bind_method(D_METHOD("get_href"), &HTMLStyleSheet::get_href);
	ClassDB::bind_method(D_METHOD("get_media"), &HTMLStyleSheet::get_media);
	ClassDB::bind_method(D_METHOD("is_disabled"), &HTMLStyleSheet::is_disabled);
	ClassDB::bind_method(D_METHOD("get_css_rules"), &HTMLStyleSheet::get_css_rules);
	ClassDB::bind_method(D_METHOD("insert_rule", "rule", "index"), &HTMLStyleSheet::insert_rule, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("delete_rule", "index"), &HTMLStyleSheet::delete_rule);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "id"), "", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "owner_node", PROPERTY_HINT_NONE, "",
		PROPERTY_USAGE_DEFAULT, "HTMLElement"), "", "get_owner_node");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "href"), "", "get_href");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "media"), "", "get_media");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "", "is_disabled");
}

HTMLLayout* HTMLStyleSheet::get_layout() const {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(layout_id));
	const auto node = owner.lock();
	const auto is_current = [&] {
		return layout->parse_version == parse_version && node->source_version == source_version &&
			layout->get_dom().is_element_connected(*node);
	};
	// Refresh can discover a different external source through the document base.
	return layout && node && is_current() && layout->refresh_cssom() && is_current() ? layout : nullptr;
}

bool HTMLStyleSheet::is_valid() const { return get_layout() != nullptr; }

int64_t HTMLStyleSheet::get_id() const { return is_valid() ? id : 0; }

Ref<HTMLElement> HTMLStyleSheet::get_owner_node() const {
	const auto* layout = get_layout();
	return layout ? layout->wrap_hit_element(owner.lock()) : Ref<HTMLElement>();
}

String HTMLStyleSheet::get_href() const {
	return is_valid() ? String::utf8(owner.lock()->linked_path.c_str()) : String();
}

String HTMLStyleSheet::get_media() const {
	return is_valid() ? String::utf8(owner.lock()->get_attr("media", "")) : String();
}

bool HTMLStyleSheet::is_disabled() const {
	if (!is_valid()) return false;
	const auto node = owner.lock();
	return node->tag() == litehtml::_link_ ? node->get_attr("disabled") != nullptr : node->style_disabled;
}

PackedStringArray HTMLStyleSheet::get_css_rules() const {
	auto* layout = get_layout();
	return layout ? layout->get_css_rules(id) : PackedStringArray();
}

Error HTMLStyleSheet::insert_rule(const String& p_rule, int64_t p_index) {
	auto* layout = get_layout();
	return layout ? layout->insert_rule(id, p_rule, p_index) : ERR_INVALID_PARAMETER;
}

Error HTMLStyleSheet::delete_rule(int64_t p_index) {
	auto* layout = get_layout();
	return layout ? layout->delete_rule(id, p_index) : ERR_INVALID_PARAMETER;
}
