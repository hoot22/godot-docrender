#include "html_element.h"
#include "formats/html/dom/html_shadow_root.h"

#include "formats/html/dom/html_dom.h"
#include "formats/html/dom/html_document_fragment.h"
#include "formats/html/html_layout.h"
#include "html_form_elements.h"
#include "html_form_state.h"

#include <godot_cpp/templates/hash_set.hpp>
#include <litehtml/element.h>
#include <litehtml/el_text.h>
#include <litehtml/html.h>

#include <string_view>

using namespace godot;

Ref<HTMLFormElement> HTMLElement::get_form_owner() const {
	if (const auto node = lock_element()) return get_dom().make_element(dockit::forms::owner(*node));
	return {};
}

Error HTMLElement::set_pointer_capture(int64_t p_pointer_id) {
	if (p_pointer_id < 0 || p_pointer_id > INT32_MAX || !is_valid() || !is_connected()) return ERR_INVALID_PARAMETER;
	return get_dom().set_pointer_capture(Ref<HTMLElement>(this), int32_t(p_pointer_id));
}

Error HTMLElement::release_pointer_capture(int64_t p_pointer_id) {
	if (p_pointer_id < 0 || p_pointer_id > INT32_MAX || !is_valid()) return ERR_INVALID_PARAMETER;
	return get_dom().release_pointer_capture(int32_t(p_pointer_id));
}

bool HTMLElement::has_pointer_capture(int64_t p_pointer_id) const {
	if (p_pointer_id < 0 || p_pointer_id > INT32_MAX || !is_valid()) return false;
	return get_dom().has_pointer_capture(Ref<HTMLElement>(const_cast<HTMLElement*>(this)), int32_t(p_pointer_id));
}

namespace {
	void append_text(const litehtml::element& p_element, litehtml::string& r_text) {
		const auto tag = p_element.tag();
		if (litehtml::is_generated_tag(tag)) {
			return;
		}
		if (p_element.is_text()) {
			p_element.get_text(r_text);
			return;
		}
		for (const auto& child : p_element.children()) {
			append_text(*child, r_text);
		}
	}

	CharString normalize_attribute_name(const String& p_name) {
		if (p_name.is_empty()) {
			return {};
		}
		CharString name = p_name.utf8();
		char* bytes = name.ptrw();
		for (int64_t i = 0; i < name.length(); ++i) {
			if (bytes[i] == '\0') {
				return {};
			}
			if (bytes[i] >= 'A' && bytes[i] <= 'Z') {
				bytes[i] += 'a' - 'A';
			}
		}
		return name;
	}

	String dataset_key(const String& p_name) {
		String key;
		for (int64_t i = 5; i < p_name.length(); ++i) {
			const char32_t ch = p_name[i];
			if (ch == '-' && i + 1 < p_name.length() && p_name[i + 1] >= 'a' && p_name[i + 1] <= 'z') {
				key += String::chr(p_name[++i] - ('a' - 'A'));
			}
			else {
				key += String::chr(ch);
			}
		}
		return key;
	}

	bool is_class_separator(char32_t p_character) {
		return p_character == ' ' || p_character == '\t' || p_character == '\n' || p_character == '\r' || p_character == '\f';
	}

	bool is_valid_class_name(const String& p_name) {
		if (p_name.is_empty()) {
			return false;
		}
		for (int64_t i = 0; i < p_name.length(); ++i) {
			if (p_name[i] == '\0' || is_class_separator(p_name[i])) {
				return false;
			}
		}
		return true;
	}

	bool is_valid_attribute_name(const CharString& p_name) {
		if (p_name.length() == 0) {
			return false;
		}
		for (const unsigned char byte : std::string_view(p_name.get_data(), p_name.length())) {
			if (byte <= ' ' || byte == '/' || byte == '>' || byte == '<' || byte == '=' || byte == '"' || byte == '\'') {
				return false;
			}
		}
		return true;
	}

	String dataset_attribute(const String& p_key) {
		String attribute = "data-";
		for (int64_t i = 0; i < p_key.length(); ++i) {
			const auto ch = p_key[i];
			if (ch == '-' && i + 1 < p_key.length() && p_key[i + 1] >= 'a' && p_key[i + 1] <= 'z') return {};
			if (ch >= 'A' && ch <= 'Z') attribute += String("-") + String::chr(ch + ('a' - 'A'));
			else attribute += String::chr(ch);
		}
		return attribute;
	}

	std::optional<dockit::HTMLDOM::TreeEdit> adjacent_position(const String& p_position) {
		using Edit = dockit::HTMLDOM::TreeEdit;
		const String value = p_position.to_lower();
		if (value == "beforebegin") return Edit::BEFORE;
		if (value == "afterbegin") return Edit::PREPEND;
		if (value == "beforeend") return Edit::APPEND;
		if (value == "afterend") return Edit::AFTER;
		return {};
	}
}

void HTMLElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("attach_shadow", "mode", "slot_assignment", "delegates_focus"), &HTMLElement::attach_shadow, DEFVAL("open"), DEFVAL("named"), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_shadow_root"), &HTMLElement::get_shadow_root);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shadow_root", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE, "HTMLShadowRoot"), "", "get_shadow_root");
	ClassDB::bind_method(D_METHOD("is_content_editable"), &HTMLElement::is_content_editable);
	ClassDB::bind_method(D_METHOD("set_pointer_capture", "pointer_id"), &HTMLElement::set_pointer_capture);
	ClassDB::bind_method(D_METHOD("release_pointer_capture", "pointer_id"), &HTMLElement::release_pointer_capture);
	ClassDB::bind_method(D_METHOD("has_pointer_capture", "pointer_id"), &HTMLElement::has_pointer_capture);
	ClassDB::bind_method(D_METHOD("get_animations"), &HTMLElement::get_animations);
	ClassDB::bind_method(D_METHOD("animate", "keyframes", "options"), &HTMLElement::animate, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_style_property", "name"), &HTMLElement::get_style_property);
	ClassDB::bind_method(D_METHOD("get_style_property_priority", "name"), &HTMLElement::get_style_property_priority);
	ClassDB::bind_method(D_METHOD("get_style_properties"), &HTMLElement::get_style_properties);
	ClassDB::bind_method(D_METHOD("set_style_property", "name", "value", "priority"), &HTMLElement::set_style_property, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("remove_style_property", "name"), &HTMLElement::remove_style_property);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &HTMLElement::get_tag_name);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "content_editable", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT),
		"", "is_content_editable");
	ClassDB::bind_method(D_METHOD("has_attribute", "name"), &HTMLElement::has_attribute);
	ClassDB::bind_method(D_METHOD("get_attribute", "name", "default_value"), &HTMLElement::get_attribute, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_text_content"), &HTMLElement::get_text_content);
	ClassDB::bind_method(D_METHOD("get_parent"), &HTMLElement::get_parent);
	ClassDB::bind_method(D_METHOD("get_children"), &HTMLElement::get_children);
	ClassDB::bind_method(D_METHOD("get_client_rects"), &HTMLElement::get_client_rects);
	ClassDB::bind_method(D_METHOD("get_bounding_rect"), &HTMLElement::get_bounding_rect);
	ClassDB::bind_method(D_METHOD("get_client_size"), &HTMLElement::get_client_size);
	ClassDB::bind_method(D_METHOD("get_client_offset"), &HTMLElement::get_client_offset);
	ClassDB::bind_method(D_METHOD("get_offset_parent"), &HTMLElement::get_offset_parent);
	ClassDB::bind_method(D_METHOD("get_offset_rect"), &HTMLElement::get_offset_rect);
	ClassDB::bind_method(D_METHOD("get_scroll_size"), &HTMLElement::get_scroll_size);
	ClassDB::bind_method(D_METHOD("get_scroll_offset"), &HTMLElement::get_scroll_offset);
	ClassDB::bind_method(D_METHOD("set_scroll_offset", "offset"), &HTMLElement::set_scroll_offset);
	ClassDB::bind_method(D_METHOD("scroll_to", "offset"), &HTMLElement::scroll_to);
	ClassDB::bind_method(D_METHOD("scroll_by", "delta"), &HTMLElement::scroll_by);
	ClassDB::bind_method(D_METHOD("get_css_diagnostics"), &HTMLElement::get_css_diagnostics);
	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &HTMLElement::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &HTMLElement::query_selector_all);
	ClassDB::bind_method(D_METHOD("matches", "selector"), &HTMLElement::matches);
	ClassDB::bind_method(D_METHOD("closest", "selector"), &HTMLElement::closest);
	ClassDB::bind_method(D_METHOD("get_attributes"), &HTMLElement::get_attributes);
	ClassDB::bind_method(D_METHOD("get_dataset"), &HTMLElement::get_dataset);
	ClassDB::bind_method(D_METHOD("get_class_list"), &HTMLElement::get_class_list);
	ClassDB::bind_method(D_METHOD("has_class", "class_name"), &HTMLElement::has_class);
	ClassDB::bind_method(D_METHOD("set_text_content", "text"), &HTMLElement::set_text_content);
	ClassDB::bind_method(D_METHOD("set_attribute", "name", "value"), &HTMLElement::set_attribute);
	ClassDB::bind_method(D_METHOD("remove_attribute", "name"), &HTMLElement::remove_attribute);
	ClassDB::bind_method(D_METHOD("add_class", "class_name"), &HTMLElement::add_class);
	ClassDB::bind_method(D_METHOD("remove_class", "class_name"), &HTMLElement::remove_class);
	ClassDB::bind_method(D_METHOD("toggle_class", "class_name", "force"), &HTMLElement::toggle_class, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_inner_html"), &HTMLElement::get_inner_html);
	ClassDB::bind_method(D_METHOD("set_inner_html", "html"), &HTMLElement::set_inner_html);
	ClassDB::bind_method(D_METHOD("append_child", "child"), &HTMLElement::append_child);
	ClassDB::bind_method(D_METHOD("insert_before", "child", "reference"), &HTMLElement::insert_before, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("append_fragment", "fragment"), &HTMLElement::append_fragment);
	ClassDB::bind_method(D_METHOD("insert_fragment_before", "fragment", "reference"), &HTMLElement::insert_fragment_before, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("replace_child", "new_child", "old_child"), &HTMLElement::replace_child);
	ClassDB::bind_method(D_METHOD("remove_child", "child"), &HTMLElement::remove_child);
	ClassDB::bind_method(D_METHOD("clone_node", "deep"), &HTMLElement::clone_node, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("detach"), &HTMLElement::detach);
	ClassDB::bind_method(D_METHOD("remove"), &HTMLElement::remove);
	ClassDB::bind_method(D_METHOD("get_first_element_child"), &HTMLElement::get_first_element_child);
	ClassDB::bind_method(D_METHOD("get_last_element_child"), &HTMLElement::get_last_element_child);
	ClassDB::bind_method(D_METHOD("get_previous_element_sibling"), &HTMLElement::get_previous_element_sibling);
	ClassDB::bind_method(D_METHOD("get_next_element_sibling"), &HTMLElement::get_next_element_sibling);
	ClassDB::bind_method(D_METHOD("get_child_element_count"), &HTMLElement::get_child_element_count);
	ClassDB::bind_method(D_METHOD("get_elements_by_tag_name", "name"), &HTMLElement::get_elements_by_tag_name);
	ClassDB::bind_method(D_METHOD("get_elements_by_class_name", "names"), &HTMLElement::get_elements_by_class_name);
	ClassDB::bind_method(D_METHOD("get_attribute_names"), &HTMLElement::get_attribute_names);
	ClassDB::bind_method(D_METHOD("has_attributes"), &HTMLElement::has_attributes);
	ClassDB::bind_method(D_METHOD("toggle_attribute", "name", "force"), &HTMLElement::toggle_attribute, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("replace_class", "old_name", "new_name"), &HTMLElement::replace_class);
	ClassDB::bind_method(D_METHOD("set_dataset_value", "key", "value"), &HTMLElement::set_dataset_value);
	ClassDB::bind_method(D_METHOD("remove_dataset_value", "key"), &HTMLElement::remove_dataset_value);
	ClassDB::bind_method(D_METHOD("append", "items"), &HTMLElement::append);
	ClassDB::bind_method(D_METHOD("prepend", "items"), &HTMLElement::prepend);
	ClassDB::bind_method(D_METHOD("before", "items"), &HTMLElement::before);
	ClassDB::bind_method(D_METHOD("after", "items"), &HTMLElement::after);
	ClassDB::bind_method(D_METHOD("replace_with", "items"), &HTMLElement::replace_with);
	ClassDB::bind_method(D_METHOD("replace_children", "items"), &HTMLElement::replace_children);
	ClassDB::bind_method(D_METHOD("insert_adjacent_element", "position", "element"), &HTMLElement::insert_adjacent_element);
	ClassDB::bind_method(D_METHOD("insert_adjacent_text", "position", "text"), &HTMLElement::insert_adjacent_text);
	ClassDB::bind_method(D_METHOD("insert_adjacent_html", "position", "html"), &HTMLElement::insert_adjacent_html);
	ClassDB::bind_method(D_METHOD("get_outer_html"), &HTMLElement::get_outer_html);
	ClassDB::bind_method(D_METHOD("set_outer_html", "html"), &HTMLElement::set_outer_html);
	ClassDB::bind_method(D_METHOD("get_inner_text"), &HTMLElement::get_inner_text);
	ClassDB::bind_method(D_METHOD("set_inner_text", "text"), &HTMLElement::set_inner_text);
	ClassDB::bind_method(D_METHOD("get_id"), &HTMLElement::get_id);
	ClassDB::bind_method(D_METHOD("set_id", "value"), &HTMLElement::set_id);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "id"), "set_id", "get_id");
	ClassDB::bind_method(D_METHOD("get_css_class_name"), &HTMLElement::get_css_class_name);
	ClassDB::bind_method(D_METHOD("set_css_class_name", "value"), &HTMLElement::set_css_class_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "css_class_name"), "set_css_class_name", "get_css_class_name");
	ClassDB::bind_method(D_METHOD("get_lang"), &HTMLElement::get_lang);
	ClassDB::bind_method(D_METHOD("set_lang", "value"), &HTMLElement::set_lang);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "lang"), "set_lang", "get_lang");
	ClassDB::bind_method(D_METHOD("get_dir"), &HTMLElement::get_dir);
	ClassDB::bind_method(D_METHOD("set_dir", "value"), &HTMLElement::set_dir);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "dir"), "set_dir", "get_dir");
	ClassDB::bind_method(D_METHOD("get_title"), &HTMLElement::get_title);
	ClassDB::bind_method(D_METHOD("set_title", "value"), &HTMLElement::set_title);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ClassDB::bind_method(D_METHOD("get_hidden"), &HTMLElement::get_hidden);
	ClassDB::bind_method(D_METHOD("set_hidden", "value"), &HTMLElement::set_hidden);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hidden"), "set_hidden", "get_hidden");
	ClassDB::bind_method(D_METHOD("get_inert"), &HTMLElement::get_inert);
	ClassDB::bind_method(D_METHOD("set_inert", "value"), &HTMLElement::set_inert);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "inert"), "set_inert", "get_inert");
	ClassDB::bind_method(D_METHOD("get_tab_index"), &HTMLElement::get_tab_index);
	ClassDB::bind_method(D_METHOD("set_tab_index", "value"), &HTMLElement::set_tab_index);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_index"), "set_tab_index", "get_tab_index");
}

std::shared_ptr<litehtml::element> HTMLElement::lock_related_element(const Ref<HTMLElement>& p_element) const {
	return lock_related_node(p_element);
}

String HTMLElement::get_tag_name() const {
	const auto node = lock_element();
	return node ? String::utf8(node->get_tagName()) : String();
}

bool HTMLElement::is_content_editable() const {
	const auto node = lock_element();
	if (!node) return false;
	for (auto current = node; current; current = current->parent()) {
		const char* raw = current->get_attr("contenteditable");
		if (!raw) continue;
		const String value = String::utf8(raw).strip_edges().to_lower();
		if (value.is_empty() || value == "true" || value == "plaintext-only") return true;
		if (value == "false") return false;
	}
	return get_dom().is_design_mode_enabled();
}

bool HTMLElement::has_attribute(const String& p_name) const {
	const auto node = lock_element();
	const CharString name = normalize_attribute_name(p_name);
	return node && name.length() > 0 && get_dom().get_element_attribute(*node, name.get_data());
}

String HTMLElement::get_attribute(const String& p_name, const String& p_default_value) const {
	const auto node = lock_element();
	const CharString name = normalize_attribute_name(p_name);
	const char* value = node && name.length() > 0 ? get_dom().get_element_attribute(*node, name.get_data()) : nullptr;
	return value ? String::utf8(value) : p_default_value;
}

String HTMLElement::get_text_content() const {
	const auto node = lock_element();
	if (!node) {
		return {};
	}
	litehtml::string text;
	append_text(*node, text);
	return String::utf8(text.c_str());
}

Ref<HTMLElement> HTMLElement::get_parent() const {
	if (const auto node = lock_element()) {
		return get_dom().make_element(node->parent());
	}
	return {};
}

TypedArray<HTMLElement> HTMLElement::get_children() const {
	const auto node = lock_element();
	return node ? get_dom().get_children(*node) : TypedArray<HTMLElement>();
}

TypedArray<Rect2> HTMLElement::get_client_rects() const {
	const HTMLLayout* layout = get_owner();
	return layout ? layout->get_element_rects(element.lock()) : TypedArray<Rect2>();
}

Rect2 HTMLElement::get_bounding_rect() const {
	const TypedArray<Rect2> boxes = get_client_rects();
	if (boxes.is_empty()) {
		return {};
	}
	Rect2 bounds = boxes[0];
	for (int64_t i = 1; i < boxes.size(); ++i) {
		bounds = bounds.merge(boxes[i]);
	}
	return bounds;
}

TypedArray<Dictionary> HTMLElement::get_css_diagnostics() const {
	if (const auto* layout = get_owner()) {
		if (const auto node = element.lock()) {
			return layout->get_css_diagnostics_for(*node);
		}
	}
	return {};
}

Ref<HTMLElement> HTMLElement::query_selector(const String& p_selector) const {
	if (const auto node = lock_element()) {
		return get_dom().query_selector(p_selector, node);
	}
	return {};
}

TypedArray<HTMLElement> HTMLElement::query_selector_all(const String& p_selector) const {
	if (const auto node = lock_element()) {
		return get_dom().query_selector_all(p_selector, node);
	}
	return {};
}

bool HTMLElement::matches(const String& p_selector) const {
	const auto node = lock_element();
	return node && get_dom().matches(p_selector, node);
}

Ref<HTMLElement> HTMLElement::closest(const String& p_selector) const {
	if (const auto node = lock_element()) {
		return get_dom().closest(p_selector, node);
	}
	return {};
}

Dictionary HTMLElement::get_attributes() const {
	if (const auto node = lock_element()) {
		return get_dom().get_element_attributes(*node);
	}
	return {};
}

Dictionary HTMLElement::get_dataset() const {
	const Dictionary attributes = get_attributes();
	const Array keys = attributes.keys();
	Dictionary dataset;
	for (int64_t i = 0; i < keys.size(); ++i) {
		const String name = keys[i];
		if (name.begins_with("data-")) {
			dataset[dataset_key(name)] = attributes[name];
		}
	}
	return dataset;
}

PackedStringArray HTMLElement::get_class_list() const {
	const String classes = get_attribute("class");
	PackedStringArray result;
	HashSet<String> seen;
	int64_t start = 0;
	for (int64_t i = 0; i <= classes.length(); ++i) {
		if (i < classes.length() && !is_class_separator(classes[i])) {
			continue;
		}
		if (i > start) {
			const String name = classes.substr(start, i - start);
			if (!seen.has(name)) {
				seen.insert(name);
				result.append(name);
			}
		}
		start = i + 1;
	}
	return result;
}

bool HTMLElement::has_class(const String& p_class_name) const {
	return get_class_list().has(p_class_name);
}

Error HTMLElement::set_text_content(const String& p_text) {
	const auto node = lock_element();
	return node ? get_dom().replace_children(node, p_text, false) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::set_attribute(const String& p_name, const String& p_value) {
	const auto node = lock_element();
	const CharString name = normalize_attribute_name(p_name);
	const CharString value = p_value.utf8();
	if (!node || !is_valid_attribute_name(name) ||
		std::string_view(value.get_data(), value.length()).find('\0') != std::string_view::npos) {
		return ERR_INVALID_PARAMETER;
	}
	return get_dom().edit_attribute(node, name.get_data(), value.get_data());
}

Error HTMLElement::remove_attribute(const String& p_name) {
	const auto node = lock_element();
	const CharString name = normalize_attribute_name(p_name);
	return node && is_valid_attribute_name(name) ?
		get_dom().edit_attribute(node, name.get_data(), nullptr) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::add_class(const String& p_class_name) {
	return toggle_class(p_class_name, true);
}

Error HTMLElement::remove_class(const String& p_class_name) {
	return toggle_class(p_class_name, false);
}

Error HTMLElement::toggle_class(const String& p_class_name, const Variant& p_force) {
	if (!is_valid() || !is_valid_class_name(p_class_name) ||
		(p_force.get_type() != Variant::NIL && p_force.get_type() != Variant::BOOL)) {
		return ERR_INVALID_PARAMETER;
	}
	PackedStringArray classes = get_class_list();
	const int64_t index = classes.find(p_class_name);
	const bool present = index >= 0;
	const bool include = p_force.get_type() == Variant::NIL ? !present : bool(p_force);
	if (include == present) {
		return OK;
	}
	if (include) {
		classes.append(p_class_name);
	}
	else {
		classes.remove_at(index);
	}
	return set_attribute("class", String(" ").join(classes));
}

String HTMLElement::get_inner_html() const {
	const auto node = lock_element();
	return node ? get_dom().serialize_children(*node) : String();
}

Error HTMLElement::set_inner_html(const String& p_html) {
	const auto node = lock_element();
	return node ? get_dom().replace_children(node, p_html, true) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::append_child(const Ref<HTMLElement>& p_child) {
	return insert_before(p_child);
}

Error HTMLElement::insert_before(const Ref<HTMLElement>& p_child, const Ref<HTMLElement>& p_reference) {
	const auto parent = lock_element();
	const auto child = lock_related_element(p_child);
	const auto reference = lock_related_element(p_reference);
	if (!parent || !child || (p_reference.is_valid() && !reference)) {
		return ERR_INVALID_PARAMETER;
	}
	return get_dom().insert_child(parent, child, reference);
}

Error HTMLElement::append_fragment(const Ref<HTMLDocumentFragment>& p_fragment) {
	return insert_fragment_before(p_fragment);
}

Error HTMLElement::insert_fragment_before(const Ref<HTMLDocumentFragment>& p_fragment, const Ref<HTMLElement>& p_reference) {
	const auto parent = lock_element();
	const auto reference = lock_related_element(p_reference);
	if (!parent || p_fragment.is_null() || (p_reference.is_valid() && !reference)) {
		return ERR_INVALID_PARAMETER;
	}
	return get_dom().insert_fragment(parent, *p_fragment.ptr(), reference);
}

Error HTMLElement::replace_child(const Ref<HTMLElement>& p_new_child, const Ref<HTMLElement>& p_old_child) {
	const auto parent = lock_element();
	const auto new_child = lock_related_element(p_new_child);
	const auto old_child = lock_related_element(p_old_child);
	return parent && new_child && old_child ? get_dom().replace_child(parent, new_child, old_child) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::remove_child(const Ref<HTMLElement>& p_child) {
	const auto parent = lock_element();
	const auto child = lock_related_element(p_child);
	return parent && child ? get_dom().remove_child(parent, child) : ERR_INVALID_PARAMETER;
}

Ref<HTMLElement> HTMLElement::clone_node(bool p_deep) const {
	const auto node = lock_element();
	if (!node) {
		return {};
	}
	auto& dom = get_dom();
	return dom.copy_node(dom, *node, p_deep);
}

Error HTMLElement::detach() {
	const auto node = lock_element();
	return node ? get_dom().detach_element(node) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::remove() {
	const auto node = lock_element();
	return node ? get_dom().remove_element(node) : ERR_INVALID_PARAMETER;
}

String HTMLElement::get_id() const {
	return get_attribute("id");
}

void HTMLElement::set_id(const String& p_value) {
	set_attribute("id", p_value);
}

String HTMLElement::get_css_class_name() const {
	return get_attribute("class");
}

void HTMLElement::set_css_class_name(const String& p_value) {
	set_attribute("class", p_value);
}

String HTMLElement::get_lang() const {
	return get_attribute("lang");
}

void HTMLElement::set_lang(const String& p_value) {
	set_attribute("lang", p_value);
}

String HTMLElement::get_dir() const {
	return get_attribute("dir");
}

void HTMLElement::set_dir(const String& p_value) {
	set_attribute("dir", p_value);
}

String HTMLElement::get_title() const {
	return get_attribute("title");
}

void HTMLElement::set_title(const String& p_value) {
	set_attribute("title", p_value);
}

bool HTMLElement::get_hidden() const {
	return has_attribute("hidden");
}

void HTMLElement::set_hidden(bool p_value) {
	if (p_value) set_attribute("hidden", "");
	else remove_attribute("hidden");
}

bool HTMLElement::get_inert() const {
	return has_attribute("inert");
}

void HTMLElement::set_inert(bool p_value) {
	if (p_value) set_attribute("inert", "");
	else remove_attribute("inert");
}

int64_t HTMLElement::get_tab_index() const {
	const String value = get_attribute("tabindex").strip_edges();
	if (value.is_valid_int()) return value.to_int();
	const String tag = get_tag_name();
	return tag == "textarea" || tag == "select" || tag == "button" ||
		(tag == "input" && get_attribute("type").nocasecmp_to("hidden") != 0) ||
		(tag == "a" && has_attribute("href")) ? 0 : -1;
}

void HTMLElement::set_tab_index(int64_t p_value) {
	set_attribute("tabindex", String::num_int64(p_value));
}

Error HTMLElement::append(const Array& p_items) {
	const auto node = lock_element();
	return node ? get_dom().edit_nodes(node, dockit::HTMLDOM::TreeEdit::APPEND, p_items) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::prepend(const Array& p_items) {
	const auto node = lock_element();
	return node ? get_dom().edit_nodes(node, dockit::HTMLDOM::TreeEdit::PREPEND, p_items) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::before(const Array& p_items) {
	const auto node = lock_element();
	return node ? get_dom().edit_nodes(node, dockit::HTMLDOM::TreeEdit::BEFORE, p_items) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::after(const Array& p_items) {
	const auto node = lock_element();
	return node ? get_dom().edit_nodes(node, dockit::HTMLDOM::TreeEdit::AFTER, p_items) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::replace_with(const Array& p_items) {
	const auto node = lock_element();
	return node ? get_dom().edit_nodes(node, dockit::HTMLDOM::TreeEdit::REPLACE, p_items) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::replace_children(const Array& p_items) {
	const auto node = lock_element();
	return node ? get_dom().edit_nodes(node, dockit::HTMLDOM::TreeEdit::REPLACE_CHILDREN, p_items) : ERR_INVALID_PARAMETER;
}

Ref<HTMLElement> HTMLElement::get_first_element_child() const {
	const auto children = get_children();
	return children.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(children[0]);
}

Ref<HTMLElement> HTMLElement::get_last_element_child() const {
	const auto children = get_children();
	return children.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(children[children.size() - 1]);
}

Ref<HTMLElement> HTMLElement::get_previous_element_sibling() const {
	Ref<HTMLElement> previous;
	if (const auto parent = get_parent_node(); parent.is_valid()) {
		for (const Ref<HTMLNode>& child : parent->get_child_nodes()) {
			if (child.ptr() == this) return previous;
			if (child->get_node_type() == ELEMENT_NODE) previous = child;
		}
	}
	return {};
}

Ref<HTMLElement> HTMLElement::get_next_element_sibling() const {
	for (auto sibling = get_next_sibling(); sibling.is_valid(); sibling = sibling->get_next_sibling()) {
		if (sibling->get_node_type() == ELEMENT_NODE) return sibling;
	}
	return {};
}

int64_t HTMLElement::get_child_element_count() const {
	return get_children().size();
}

TypedArray<HTMLElement> HTMLElement::get_elements_by_tag_name(const String& p_name) const {
	const auto node = lock_element();
	return node ? get_dom().get_elements(p_name, dockit::HTMLDOM::Lookup::TAG, node) : TypedArray<HTMLElement>();
}

TypedArray<HTMLElement> HTMLElement::get_elements_by_class_name(const String& p_names) const {
	const auto node = lock_element();
	return node ? get_dom().get_elements(p_names, dockit::HTMLDOM::Lookup::CLASS, node) : TypedArray<HTMLElement>();
}

PackedStringArray HTMLElement::get_attribute_names() const {
	PackedStringArray names;
	for (const Variant& name : get_attributes().keys()) names.append(name);
	return names;
}

bool HTMLElement::has_attributes() const {
	return !get_attributes().is_empty();
}

bool HTMLElement::toggle_attribute(const String& p_name, const Variant& p_force) {
	if (!is_valid() || !is_valid_attribute_name(normalize_attribute_name(p_name)) ||
		(p_force.get_type() != Variant::NIL && p_force.get_type() != Variant::BOOL)) return false;
	const bool present = has_attribute(p_name);
	const bool include = p_force.get_type() == Variant::NIL ? !present : bool(p_force);
	if (present != include) {
		const Error error = include ? set_attribute(p_name, "") : remove_attribute(p_name);
		if (error != OK) return present;
	}
	return include;
}

bool HTMLElement::replace_class(const String& p_old_name, const String& p_new_name) {
	if (!is_valid_class_name(p_old_name) || !is_valid_class_name(p_new_name) || !has_class(p_old_name)) return false;
	PackedStringArray classes;
	bool replaced = false;
	for (const String& name : get_class_list()) {
		if (name == p_old_name || name == p_new_name) {
			if (!replaced) classes.append(p_new_name);
			replaced = true;
		} else {
			classes.append(name);
		}
	}
	return set_attribute("class", String(" ").join(classes)) == OK;
}

Error HTMLElement::set_dataset_value(const String& p_key, const String& p_value) {
	return set_attribute(dataset_attribute(p_key), p_value);
}

Error HTMLElement::remove_dataset_value(const String& p_key) {
	return remove_attribute(dataset_attribute(p_key));
}

Error HTMLElement::insert_adjacent_element(const String& p_position, const Ref<HTMLElement>& p_element) {
	const auto node = lock_element();
	const auto edit = adjacent_position(p_position);
	return node && edit ? get_dom().edit_nodes(node, *edit, Array::make(p_element)) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::insert_adjacent_text(const String& p_position, const String& p_text) {
	const auto node = lock_element();
	const auto edit = adjacent_position(p_position);
	return node && edit ? get_dom().edit_nodes(node, *edit, Array::make(p_text)) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::insert_adjacent_html(const String& p_position, const String& p_html) {
	const auto node = lock_element();
	const auto edit = adjacent_position(p_position);
	if (!node || !edit) return ERR_INVALID_PARAMETER;
	using Edit = dockit::HTMLDOM::TreeEdit;
	const bool outside = *edit == Edit::BEFORE || *edit == Edit::AFTER;
	const auto context = outside ? node->parent() : node;
	if (!context) return ERR_INVALID_PARAMETER;
	try {
		const auto fragment = get_dom().parse_fragment(p_html, context);
		return fragment.is_valid() ? get_dom().edit_nodes(node, *edit, Array::make(fragment)) : ERR_PARSE_ERROR;
	} catch (const std::exception&) {
		return ERR_PARSE_ERROR;
	}
}

String HTMLElement::get_outer_html() const {
	const auto node = lock_element();
	return node ? get_dom().serialize_element(*node) : String();
}

Error HTMLElement::set_outer_html(const String& p_html) {
	const auto node = lock_element();
	if (!node) return ERR_INVALID_PARAMETER;
	const auto parent = node->parent();
	if (!parent) return is_connected() ? ERR_INVALID_PARAMETER : OK;
	try {
		const auto fragment = get_dom().parse_fragment(p_html, parent);
		return fragment.is_valid() ? get_dom().edit_nodes(node, dockit::HTMLDOM::TreeEdit::REPLACE, Array::make(fragment)) : ERR_PARSE_ERROR;
	} catch (const std::exception&) {
		return ERR_PARSE_ERROR;
	}
}

String HTMLElement::get_inner_text() const {
	const auto root = lock_element();
	if (!root || !is_connected() || !get_owner()->has_current_layout() ||
		(root->css().get_display() != litehtml::display_contents && !root->get_render_item())) return get_text_content();
	String result;
	String separator;
	const auto append = [&](const String& p_text) {
		if (p_text.is_empty()) return;
		result += separator + p_text;
		separator = String();
	};
	const auto visit = [&](const auto& p_visit, const litehtml::element& p_node) -> void {
		using namespace litehtml;
		const auto& css = p_node.css();
		if (is_generated_tag(p_node.tag()) || css.get_display() == display_none || css.hides_contents()) return;
		if (p_node.is_break() && css.get_visibility() == visibility_visible) {
			if (separator == " ") separator = String();
			append("\n");
			return;
		}
		if (const auto* text = dynamic_cast<const el_text*>(&p_node)) {
			if (css.get_visibility() != visibility_visible) return;
			if (p_node.is_white_space()) {
				if (!result.is_empty() && separator.is_empty() && !result.ends_with("\n")) separator = " ";
			} else {
				append(String::utf8(text->get_rendered_text().c_str()));
			}
			return;
		}
		const auto display = css.get_display();
		const bool block = &p_node != root.get() && is_one_of(display, display_block, display_list_item,
			display_flex, display_grid, display_table, display_table_row, display_table_caption);
		const String boundary = p_node.tag() == _p_ ? "\n\n" : "\n";
		const String previous_separator = separator;
		const int64_t before_children = result.length();
		if (block && !result.is_empty() && (separator == " " || separator.length() < boundary.length())) separator = boundary;
		for (const auto& child : p_node.children()) p_visit(p_visit, *child);
		if (before_children == result.length()) {
			if (block) separator = previous_separator;
			return;
		}
		if (block && !result.is_empty() && (separator == " " || separator.length() < boundary.length())) separator = boundary;
		if (display == display_table_cell && separator.is_empty() && !result.is_empty()) separator = "\t";
	};
	visit(visit, *root);
	return result;
}

Error HTMLElement::set_inner_text(const String& p_text) {
	const auto node = lock_element();
	if (!node) return ERR_INVALID_PARAMETER;
	const String tag = get_tag_name();
	if (tag == "textarea" || tag == "script" || tag == "style" || tag == "title") return set_text_content(p_text);
	if (p_text.is_empty()) return replace_children({});
	Array items;
	for (const String& line : p_text.replace("\r\n", "\n").replace("\r", "\n").split("\n")) {
		if (!items.is_empty()) items.append(get_owner()->create_element("br"));
		items.append(line);
	}
	return replace_children(items);
}

Vector2 HTMLElement::get_client_size() const {
	const auto* owner = get_owner();
	return owner ? owner->element_client_size(*this) : Vector2();
}

Vector2 HTMLElement::get_client_offset() const {
	const auto* owner = get_owner();
	return owner ? owner->element_client_offset(*this) : Vector2();
}

Ref<HTMLElement> HTMLElement::get_offset_parent() const {
	const auto* owner = get_owner();
	return owner ? owner->element_offset_parent(*this) : Ref<HTMLElement>();
}

Rect2 HTMLElement::get_offset_rect() const {
	const auto* owner = get_owner();
	return owner ? owner->element_offset_rect(*this) : Rect2();
}

Vector2 HTMLElement::get_scroll_size() const {
	const auto* owner = get_owner();
	return owner ? owner->element_scroll_size(*this) : Vector2();
}

Vector2 HTMLElement::get_scroll_offset() const {
	const auto* owner = get_owner();
	return owner ? owner->element_scroll_offset(*this) : Vector2();
}

Error HTMLElement::set_scroll_offset(const Vector2& p_offset) {
	auto* owner = get_owner();
	return owner ? owner->set_element_scroll_offset(*this, p_offset) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::scroll_to(const Vector2& p_offset) {
	return set_scroll_offset(p_offset);
}

Error HTMLElement::scroll_by(const Vector2& p_delta) {
	return set_scroll_offset(get_scroll_offset() + p_delta);
}

Ref<HTMLShadowRoot> HTMLElement::attach_shadow(const String& p_mode, const String& p_assignment, bool p_focus) {
	const auto node = lock_element();
	return node ? get_dom().attach_shadow(node, p_mode, p_assignment, p_focus) : Ref<HTMLShadowRoot>();
}
Ref<HTMLShadowRoot> HTMLElement::get_shadow_root() const {
	const auto node = lock_element();
	return node ? get_dom().get_shadow_root(node) : Ref<HTMLShadowRoot>();
}
