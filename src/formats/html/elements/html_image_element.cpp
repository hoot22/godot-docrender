#include "html_image_element.h"

#include "formats/html/html_layout.h"

using namespace godot;

void HTMLImageElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_src"), &HTMLImageElement::get_src);
	ClassDB::bind_method(D_METHOD("set_src", "src"), &HTMLImageElement::set_src);
	ClassDB::bind_method(D_METHOD("get_srcset"), &HTMLImageElement::get_srcset);
	ClassDB::bind_method(D_METHOD("set_srcset", "srcset"), &HTMLImageElement::set_srcset);
	ClassDB::bind_method(D_METHOD("get_sizes"), &HTMLImageElement::get_sizes);
	ClassDB::bind_method(D_METHOD("set_sizes", "sizes"), &HTMLImageElement::set_sizes);
	ClassDB::bind_method(D_METHOD("get_current_src"), &HTMLImageElement::get_current_src);
	ClassDB::bind_method(D_METHOD("get_alt"), &HTMLImageElement::get_alt);
	ClassDB::bind_method(D_METHOD("set_alt", "alt"), &HTMLImageElement::set_alt);
	ClassDB::bind_method(D_METHOD("get_width_hint"), &HTMLImageElement::get_width_hint);
	ClassDB::bind_method(D_METHOD("set_width_hint", "width"), &HTMLImageElement::set_width_hint);
	ClassDB::bind_method(D_METHOD("get_height_hint"), &HTMLImageElement::get_height_hint);
	ClassDB::bind_method(D_METHOD("set_height_hint", "height"), &HTMLImageElement::set_height_hint);
	ClassDB::bind_method(D_METHOD("get_intrinsic_size"), &HTMLImageElement::get_intrinsic_size);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "src"), "set_src", "get_src");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "srcset"), "set_srcset", "get_srcset");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "sizes"), "set_sizes", "get_sizes");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "current_src"), "", "get_current_src");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "alt"), "set_alt", "get_alt");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "width_hint"), "set_width_hint", "get_width_hint");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "height_hint"), "set_height_hint", "get_height_hint");
}

String HTMLImageElement::get_src() const {
	return get_attribute("src");
}

void HTMLImageElement::set_src(const String& p_src) {
	set_attribute("src", p_src);
}

String HTMLImageElement::get_srcset() const {
	return get_attribute("srcset");
}

void HTMLImageElement::set_srcset(const String& p_srcset) {
	set_attribute("srcset", p_srcset);
}

String HTMLImageElement::get_sizes() const {
	return get_attribute("sizes");
}

void HTMLImageElement::set_sizes(const String& p_sizes) {
	set_attribute("sizes", p_sizes);
}

String HTMLImageElement::get_current_src() const {
	const auto* layout = get_owner();
	return layout ? layout->get_image_current_src(*this) : String();
}

String HTMLImageElement::get_alt() const {
	return get_attribute("alt");
}

void HTMLImageElement::set_alt(const String& p_alt) {
	set_attribute("alt", p_alt);
}

int64_t HTMLImageElement::get_dimension_hint(const String& p_attribute) const {
	const String value = get_attribute(p_attribute).strip_edges();
	const int64_t hint = value.is_valid_int() ? value.to_int() : 0;
	return hint >= 0 ? hint : 0;
}

void HTMLImageElement::set_dimension_hint(const String& p_attribute, int64_t p_pixels) {
	ERR_FAIL_COND(p_pixels < 0);
	set_attribute(p_attribute, String::num_int64(p_pixels));
}

int64_t HTMLImageElement::get_width_hint() const {
	return get_dimension_hint("width");
}

void HTMLImageElement::set_width_hint(int64_t p_width) {
	set_dimension_hint("width", p_width);
}

int64_t HTMLImageElement::get_height_hint() const {
	return get_dimension_hint("height");
}

void HTMLImageElement::set_height_hint(int64_t p_height) {
	set_dimension_hint("height", p_height);
}

Vector2 HTMLImageElement::get_intrinsic_size() const {
	const auto* layout = get_owner();
	return layout ? layout->get_image_intrinsic_size(*this) : Vector2();
}
