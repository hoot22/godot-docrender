#include "html_list_elements.h"

#include <charconv>
#include <string_view>

using namespace godot;

namespace {
	int32_t read_list_integer(const HTMLElement& p_element, const char* p_name, int32_t p_default) {
		const auto bytes = p_element.get_attribute(p_name).utf8();
		std::string_view text(bytes.get_data(), bytes.length());
		const auto first = text.find_first_not_of(" \t\n\r\f");
		if (first == std::string_view::npos) return p_default;
		text.remove_prefix(first);
		if (text.front() == '+') {
			text.remove_prefix(1);
			if (text.empty() || text.front() < '0' || text.front() > '9') return p_default;
		}

		int32_t value = 0;
		const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
		return parsed.ec == std::errc() ? value : p_default;
	}
}

void HTMLOListElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_start"), &HTMLOListElement::get_start);
	ClassDB::bind_method(D_METHOD("set_start", "start"), &HTMLOListElement::set_start);
	ClassDB::bind_method(D_METHOD("is_reversed"), &HTMLOListElement::is_reversed);
	ClassDB::bind_method(D_METHOD("set_reversed", "reversed"), &HTMLOListElement::set_reversed);
	ClassDB::bind_method(D_METHOD("get_type"), &HTMLOListElement::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &HTMLOListElement::set_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "start"), "set_start", "get_start");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reversed"), "set_reversed", "is_reversed");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");
}

int32_t HTMLOListElement::get_start() const { return read_list_integer(*this, "start", 1); }
void HTMLOListElement::set_start(int32_t p_start) { set_attribute("start", String::num_int64(p_start)); }

bool HTMLOListElement::is_reversed() const { return has_attribute("reversed"); }

void HTMLOListElement::set_reversed(bool p_reversed) {
	if (p_reversed) set_attribute("reversed", "");
	else remove_attribute("reversed");
}

String HTMLOListElement::get_type() const { return get_attribute("type"); }
void HTMLOListElement::set_type(const String& p_type) { set_attribute("type", p_type); }

void HTMLLIElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLLIElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLLIElement::set_value);
	ClassDB::bind_method(D_METHOD("get_type"), &HTMLLIElement::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &HTMLLIElement::set_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");
}

int32_t HTMLLIElement::get_value() const { return read_list_integer(*this, "value", 0); }
void HTMLLIElement::set_value(int32_t p_value) { set_attribute("value", String::num_int64(p_value)); }

String HTMLLIElement::get_type() const { return get_attribute("type"); }
void HTMLLIElement::set_type(const String& p_type) { set_attribute("type", p_type); }
