#include "html_textarea_element.h"
#include "html_form_elements.h"

#include "html_textarea_node.h"
#include "formats/html/dom/html_dom.h"
#include "formats/html/events/html_event.h"
#include "formats/html/html_layout.h"

#include <algorithm>

using namespace godot;

Ref<HTMLFormElement> HTMLTextAreaElement::get_form() const { return get_form_owner(); }
using dockit::HTMLTextAreaNode;

void HTMLTextAreaElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_form"), &HTMLTextAreaElement::get_form);
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLTextAreaElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLTextAreaElement::set_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &HTMLTextAreaElement::get_default_value);
	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &HTMLTextAreaElement::set_default_value);
	ClassDB::bind_method(D_METHOD("get_selection_start"), &HTMLTextAreaElement::get_selection_start);
	ClassDB::bind_method(D_METHOD("get_selection_end"), &HTMLTextAreaElement::get_selection_end);
	ClassDB::bind_method(D_METHOD("set_selection_range", "start", "end"), &HTMLTextAreaElement::set_selection_range);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "default_value"), "set_default_value", "get_default_value");
}

std::shared_ptr<HTMLTextAreaNode> HTMLTextAreaElement::get_textarea() const {
	return std::dynamic_pointer_cast<HTMLTextAreaNode>(lock_element());
}

String HTMLTextAreaElement::get_value() const {
	const auto node = get_textarea();
	return node ? node->editor.get_value() : String();
}

void HTMLTextAreaElement::set_value(const String& p_value) {
	if (const auto node = get_textarea(); node && node->editor.set_value(p_value)) get_owner()->update_input(*node);
}

String HTMLTextAreaElement::get_default_value() const {
	const auto node = get_textarea();
	return node ? node->get_default_value() : String();
}

void HTMLTextAreaElement::set_default_value(const String& p_value) {
	ERR_FAIL_COND(set_text_content(p_value) != OK);
}

int64_t HTMLTextAreaElement::get_selection_start() const {
	const auto node = get_textarea();
	return node ? node->editor.selection_start() : -1;
}

int64_t HTMLTextAreaElement::get_selection_end() const {
	const auto node = get_textarea();
	return node ? node->editor.selection_end() : -1;
}

void HTMLTextAreaElement::set_selection_range(int64_t p_start, int64_t p_end) {
	if (const auto node = get_textarea()) {
		const int64_t old_start = node->editor.selection_start();
		const int64_t old_end = node->editor.selection_end();
		if (!node->editor.set_selection(std::min(p_start, p_end), p_end)) return;
		get_owner()->update_input(*node);
		if (old_start != node->editor.selection_start() || old_end != node->editor.selection_end()) {
			dispatch_event(dockit::make_dom_event(StringName("select"), true, false));
			dispatch_event(dockit::make_dom_event(StringName("selectionchange"), true, false));
			get_dom().queue_selectionchange();
		}
	}
}
