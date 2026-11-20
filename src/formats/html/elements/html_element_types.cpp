#include "html_element_types.h"

using namespace godot;

void HTMLDListElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_compact"), &HTMLDListElement::is_compact);
	ClassDB::bind_method(D_METHOD("set_compact", "value"), &HTMLDListElement::set_compact);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "compact"), "set_compact", "is_compact");
}

bool HTMLDListElement::is_compact() const { return has_attribute("compact"); }
void HTMLDListElement::set_compact(bool p_value) {
	if (p_value) set_attribute("compact", "");
	else remove_attribute("compact");
}

void HTMLMenuElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_compact"), &HTMLMenuElement::is_compact);
	ClassDB::bind_method(D_METHOD("set_compact", "value"), &HTMLMenuElement::set_compact);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "compact"), "set_compact", "is_compact");
}

bool HTMLMenuElement::is_compact() const { return has_attribute("compact"); }
void HTMLMenuElement::set_compact(bool p_value) {
	if (p_value) set_attribute("compact", "");
	else remove_attribute("compact");
}

void HTMLQuoteElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_cite"), &HTMLQuoteElement::get_cite);
	ClassDB::bind_method(D_METHOD("set_cite", "value"), &HTMLQuoteElement::set_cite);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "cite"), "set_cite", "get_cite");
}

String HTMLQuoteElement::get_cite() const { return get_attribute("cite"); }
void HTMLQuoteElement::set_cite(const godot::String& p_value) {
	set_attribute("cite", p_value);
}

void HTMLBRElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_clear"), &HTMLBRElement::get_clear);
	ClassDB::bind_method(D_METHOD("set_clear", "value"), &HTMLBRElement::set_clear);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "clear"), "set_clear", "get_clear");
}

String HTMLBRElement::get_clear() const { return get_attribute("clear"); }
void HTMLBRElement::set_clear(const godot::String& p_value) {
	set_attribute("clear", p_value);
}

void HTMLHRElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_align"), &HTMLHRElement::get_align);
	ClassDB::bind_method(D_METHOD("set_align", "value"), &HTMLHRElement::set_align);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "align"), "set_align", "get_align");
	ClassDB::bind_method(D_METHOD("get_color"), &HTMLHRElement::get_color);
	ClassDB::bind_method(D_METHOD("set_color", "value"), &HTMLHRElement::set_color);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "color"), "set_color", "get_color");
	ClassDB::bind_method(D_METHOD("is_no_shade"), &HTMLHRElement::is_no_shade);
	ClassDB::bind_method(D_METHOD("set_no_shade", "value"), &HTMLHRElement::set_no_shade);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_shade"), "set_no_shade", "is_no_shade");
	ClassDB::bind_method(D_METHOD("get_size"), &HTMLHRElement::get_size);
	ClassDB::bind_method(D_METHOD("set_size", "value"), &HTMLHRElement::set_size);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "size"), "set_size", "get_size");
	ClassDB::bind_method(D_METHOD("get_width"), &HTMLHRElement::get_width);
	ClassDB::bind_method(D_METHOD("set_width", "value"), &HTMLHRElement::set_width);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "width"), "set_width", "get_width");
}

String HTMLHRElement::get_align() const { return get_attribute("align"); }
void HTMLHRElement::set_align(const godot::String& p_value) {
	set_attribute("align", p_value);
}

String HTMLHRElement::get_color() const { return get_attribute("color"); }
void HTMLHRElement::set_color(const godot::String& p_value) {
	set_attribute("color", p_value);
}

bool HTMLHRElement::is_no_shade() const { return has_attribute("noshade"); }
void HTMLHRElement::set_no_shade(bool p_value) {
	if (p_value) set_attribute("noshade", "");
	else remove_attribute("noshade");
}

String HTMLHRElement::get_size() const { return get_attribute("size"); }
void HTMLHRElement::set_size(const godot::String& p_value) {
	set_attribute("size", p_value);
}

String HTMLHRElement::get_width() const { return get_attribute("width"); }
void HTMLHRElement::set_width(const godot::String& p_value) {
	set_attribute("width", p_value);
}
