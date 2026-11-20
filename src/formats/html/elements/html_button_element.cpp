#include "html_button_element.h"
#include "html_form_elements.h"

using namespace godot;

Ref<HTMLFormElement> HTMLButtonElement::get_form() const { return get_form_owner(); }

void HTMLButtonElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_form"), &HTMLButtonElement::get_form);
	ClassDB::bind_method(D_METHOD("is_disabled"), &HTMLButtonElement::is_disabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "disabled"), &HTMLButtonElement::set_disabled);
	ClassDB::bind_method(D_METHOD("get_type"), &HTMLButtonElement::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &HTMLButtonElement::set_type);
	ClassDB::bind_method(D_METHOD("get_name"), &HTMLButtonElement::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &HTMLButtonElement::set_name);
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLButtonElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLButtonElement::set_value);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
}

bool HTMLButtonElement::is_disabled() const {
	return has_attribute("disabled");
}

void HTMLButtonElement::set_disabled(bool p_disabled) {
	if (p_disabled) set_attribute("disabled", "");
	else remove_attribute("disabled");
}

String HTMLButtonElement::get_type() const {
	return get_attribute("type");
}

void HTMLButtonElement::set_type(const String& p_type) {
	set_attribute("type", p_type);
}

String HTMLButtonElement::get_name() const {
	return get_attribute("name");
}

void HTMLButtonElement::set_name(const String& p_name) {
	set_attribute("name", p_name);
}

String HTMLButtonElement::get_value() const {
	return get_attribute("value");
}

void HTMLButtonElement::set_value(const String& p_value) {
	set_attribute("value", p_value);
}
