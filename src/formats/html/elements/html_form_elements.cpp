#include "html_form_elements.h"
#include "html_output_node.h"
#include "formats/html/backend/litehtml_document_adapter.h"
#include <litehtml/document.h>

#include "html_form_state.h"
#include "html_input_node.h"
#include "html_select_node.h"
#include "html_textarea_node.h"
#include "formats/html/dom/html_dom.h"

#include <algorithm>
#include <iterator>

using namespace godot;
namespace forms = dockit::forms;
using dockit::HTMLInputNode;
using dockit::HTMLOptionNode;
using dockit::HTMLSelectNode;
using dockit::HTMLTextAreaNode;

void HTMLFormElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_controls"), &HTMLFormElement::get_controls);
	ClassDB::bind_method(D_METHOD("get_form_data", "submitter"), &HTMLFormElement::get_form_data, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("reset"), &HTMLFormElement::reset);
}

void HTMLLabelElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_control"), &HTMLLabelElement::get_control);
	ClassDB::bind_method(D_METHOD("get_html_for"), &HTMLLabelElement::get_html_for);
	ClassDB::bind_method(D_METHOD("set_html_for", "value"), &HTMLLabelElement::set_html_for);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "html_for"), "set_html_for", "get_html_for");
}

String HTMLLabelElement::get_html_for() const { return get_attribute("for"); }
void HTMLLabelElement::set_html_for(const String& p_value) {
	set_attribute("for", p_value);
}

void HTMLFieldSetElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_form"), &HTMLFieldSetElement::get_form);
	ClassDB::bind_method(D_METHOD("is_disabled"), &HTMLFieldSetElement::is_disabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "value"), &HTMLFieldSetElement::set_disabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
}

bool HTMLFieldSetElement::is_disabled() const { return has_attribute("disabled"); }
void HTMLFieldSetElement::set_disabled(bool p_value) {
	if (p_value) set_attribute("disabled", "");
	else remove_attribute("disabled");
}

void HTMLLegendElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_form"), &HTMLLegendElement::get_form);
}

void HTMLSelectElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_options"), &HTMLSelectElement::get_options);
	ClassDB::bind_method(D_METHOD("get_form"), &HTMLSelectElement::get_form);
	ClassDB::bind_method(D_METHOD("get_name"), &HTMLSelectElement::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "value"), &HTMLSelectElement::set_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ClassDB::bind_method(D_METHOD("is_disabled"), &HTMLSelectElement::is_disabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "value"), &HTMLSelectElement::set_disabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ClassDB::bind_method(D_METHOD("is_required"), &HTMLSelectElement::is_required);
	ClassDB::bind_method(D_METHOD("set_required", "value"), &HTMLSelectElement::set_required);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "required"), "set_required", "is_required");
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLSelectElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLSelectElement::set_value);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
	ClassDB::bind_method(D_METHOD("is_multiple"), &HTMLSelectElement::is_multiple);
	ClassDB::bind_method(D_METHOD("set_multiple", "value"), &HTMLSelectElement::set_multiple);
	ClassDB::bind_method(D_METHOD("get_size"), &HTMLSelectElement::get_size);
	ClassDB::bind_method(D_METHOD("set_size", "value"), &HTMLSelectElement::set_size);
	ClassDB::bind_method(D_METHOD("get_selected_options"), &HTMLSelectElement::get_selected_options);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "multiple"), "set_multiple", "is_multiple");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "size"), "set_size", "get_size");
	ClassDB::bind_method(D_METHOD("get_selected_index"), &HTMLSelectElement::get_selected_index);
	ClassDB::bind_method(D_METHOD("set_selected_index", "value"), &HTMLSelectElement::set_selected_index);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "selected_index"), "set_selected_index", "get_selected_index");
}

String HTMLSelectElement::get_name() const { return get_attribute("name"); }
void HTMLSelectElement::set_name(const String& p_value) {
	set_attribute("name", p_value);
}

bool HTMLSelectElement::is_disabled() const { return has_attribute("disabled"); }
void HTMLSelectElement::set_disabled(bool p_value) {
	if (p_value) set_attribute("disabled", "");
	else remove_attribute("disabled");
}

bool HTMLSelectElement::is_required() const { return has_attribute("required"); }
void HTMLSelectElement::set_required(bool p_value) {
	if (p_value) set_attribute("required", "");
	else remove_attribute("required");
}

void HTMLOptionElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLOptionElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLOptionElement::set_value);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
	ClassDB::bind_method(D_METHOD("get_label"), &HTMLOptionElement::get_label);
	ClassDB::bind_method(D_METHOD("set_label", "value"), &HTMLOptionElement::set_label);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");
	ClassDB::bind_method(D_METHOD("is_disabled"), &HTMLOptionElement::is_disabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "value"), &HTMLOptionElement::set_disabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ClassDB::bind_method(D_METHOD("is_selected"), &HTMLOptionElement::is_selected);
	ClassDB::bind_method(D_METHOD("set_selected", "value"), &HTMLOptionElement::set_selected);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selected"), "set_selected", "is_selected");
	ClassDB::bind_method(D_METHOD("is_default_selected"), &HTMLOptionElement::is_default_selected);
	ClassDB::bind_method(D_METHOD("set_default_selected", "value"), &HTMLOptionElement::set_default_selected);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_selected"), "set_default_selected", "is_default_selected");
}

bool HTMLOptionElement::is_disabled() const { return has_attribute("disabled"); }
void HTMLOptionElement::set_disabled(bool p_value) {
	if (p_value) set_attribute("disabled", "");
	else remove_attribute("disabled");
}

bool HTMLOptionElement::is_default_selected() const { return has_attribute("selected"); }
void HTMLOptionElement::set_default_selected(bool p_value) {
	if (p_value) set_attribute("selected", "");
	else remove_attribute("selected");
}

void HTMLOptGroupElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_label"), &HTMLOptGroupElement::get_label);
	ClassDB::bind_method(D_METHOD("set_label", "value"), &HTMLOptGroupElement::set_label);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");
	ClassDB::bind_method(D_METHOD("is_disabled"), &HTMLOptGroupElement::is_disabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "value"), &HTMLOptGroupElement::set_disabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
}

String HTMLOptGroupElement::get_label() const { return get_attribute("label"); }
void HTMLOptGroupElement::set_label(const String& p_value) {
	set_attribute("label", p_value);
}

bool HTMLOptGroupElement::is_disabled() const { return has_attribute("disabled"); }
void HTMLOptGroupElement::set_disabled(bool p_value) {
	if (p_value) set_attribute("disabled", "");
	else remove_attribute("disabled");
}

TypedArray<HTMLElement> HTMLFormElement::get_controls() const {
	TypedArray<HTMLElement> result;
	if (const auto form = lock_element()) {
		for (const auto& control : forms::controls(*form)) result.append(get_dom().make_element(control));
	}
	return result;
}

TypedArray<Dictionary> HTMLFormElement::get_form_data(const Ref<HTMLElement>& p_submitter) const {
	return collect_form_data(p_submitter, {});
}

TypedArray<Dictionary> HTMLFormElement::collect_form_data(const Ref<HTMLElement>& p_submitter,
	const Vector2i& p_image_coordinates) const {
	TypedArray<Dictionary> result;
	const auto add_entry = [&](const String& p_name, const Variant& p_value) {
		Dictionary entry;
		entry["name"] = p_name;
		entry["value"] = p_value;
		result.append(entry);
	};
	const auto form = lock_element();
	if (!form) return result;
	const auto submitter = lock_related_element(p_submitter);
	if (p_submitter.is_valid() && (!submitter || forms::button_action(*submitter) != forms::ButtonAction::SUBMIT ||
		forms::owner(*submitter) != form || forms::is_disabled(*submitter))) return result;
	for (const auto& control : forms::controls(*form)) {
		const auto* name = control->get_attr("name");
		if (forms::is_disabled(*control)) continue;
		const auto* input = dynamic_cast<const HTMLInputNode*>(control.get());
		if (input && input->type() == HTMLInputNode::Type::IMAGE) {
			if (control != submitter) continue;
			const String prefix = name && *name ? String::utf8(name) + "." : String();
			add_entry(prefix + String("x"), String::num_int64(p_image_coordinates.x));
			add_entry(prefix + String("y"), String::num_int64(p_image_coordinates.y));
			continue;
		}
		if (!name || !*name) continue;
		String value;
		if (input) {
			if ((input->is_toggle() && !input->is_checked()) ||
				(input->is_button() && control != submitter)) continue;
			if (input->type() == HTMLInputNode::Type::FILE) {
				const auto& files = input->get_files();
				if (files.is_empty()) {
					Ref<HTMLInputFile> empty;
					empty.instantiate();
					add_entry(String::utf8(name), empty);
				} else {
					for (const Ref<HTMLInputFile>& file : files) {
						add_entry(String::utf8(name), file);
					}
				}
				continue;
			}
			value = input->get_value();
		} else if (const auto* textarea = dynamic_cast<const HTMLTextAreaNode*>(control.get())) {
			value = textarea->editor.get_value();
		} else if (const auto* select = dynamic_cast<const HTMLSelectNode*>(control.get())) {
			for (const auto& option : select->get_options()) {
				if (!option->is_selected() || forms::is_disabled(*option)) continue;
				add_entry(String::utf8(name), option->get_value());
			}
			continue;
		} else if (control == submitter) {
			value = String::utf8(control->get_attr("value", ""));
		} else {
			continue;
		}
		add_entry(String::utf8(name), value);
	}
	return result;
}

Error HTMLFormElement::reset() {
	const auto form = lock_element();
	if (!form) return ERR_INVALID_PARAMETER;
	for (const auto& node : forms::controls(*form)) {
		if (auto* input = dynamic_cast<HTMLInputNode*>(node.get())) input->reset();
		else if (auto* area = dynamic_cast<HTMLTextAreaNode*>(node.get())) area->editor.reset_value(area->get_default_value());
		else if (auto* select = dynamic_cast<HTMLSelectNode*>(node.get())) select->reset();
		else if (node->tag() == litehtml::_output_) {
			const Ref<HTMLOutputElement> output = get_dom().make_element(node);
			if (output.is_valid()) output->reset();
		}
	}
	get_dom().form_state_changed(form);
	return OK;
}

Ref<HTMLElement> HTMLLabelElement::get_control() const {
	const auto label = lock_element();
	return label ? get_dom().make_element(forms::labeled_control(*label)) : Ref<HTMLElement>();
}

Ref<HTMLFormElement> HTMLFieldSetElement::get_form() const { return get_form_owner(); }
Ref<HTMLFormElement> HTMLLegendElement::get_form() const { return get_form_owner(); }
Ref<HTMLFormElement> HTMLSelectElement::get_form() const { return get_form_owner(); }

TypedArray<HTMLElement> HTMLSelectElement::get_options() const {
	TypedArray<HTMLElement> result;
	if (const auto select = std::dynamic_pointer_cast<HTMLSelectNode>(lock_element())) {
		for (const auto& option : select->get_options()) result.append(get_dom().make_element(option));
	}
	return result;
}

bool HTMLSelectElement::is_multiple() const { return has_attribute("multiple"); }
void HTMLSelectElement::set_multiple(bool p_value) {
	if (p_value) set_attribute("multiple", "");
	else remove_attribute("multiple");
}
int64_t HTMLSelectElement::get_size() const {
	const auto node = std::dynamic_pointer_cast<HTMLSelectNode>(lock_element());
	return node ? node->get_size() : 0;
}
void HTMLSelectElement::set_size(int64_t p_value) {
	if (p_value >= 0 && p_value <= UINT32_MAX) set_attribute("size", String::num_int64(p_value));
}
TypedArray<HTMLElement> HTMLSelectElement::get_selected_options() const {
	TypedArray<HTMLElement> result;
	if (const auto node = std::dynamic_pointer_cast<HTMLSelectNode>(lock_element())) {
		for (const auto& option : node->get_options()) if (option->is_selected()) result.append(get_dom().make_element(option));
	}
	return result;
}
int64_t HTMLSelectElement::get_selected_index() const {
	const auto select = std::dynamic_pointer_cast<HTMLSelectNode>(lock_element());
	return select ? select->selected_index(select->get_options()) : -1;
}

void HTMLSelectElement::set_selected_index(int64_t p_value) {
	if (const auto select = std::dynamic_pointer_cast<HTMLSelectNode>(lock_element())) {
		select->set_selected_index(select->get_options(), p_value);
		get_dom().form_state_changed(select);
	}
}

String HTMLSelectElement::get_value() const {
	const auto select = std::dynamic_pointer_cast<HTMLSelectNode>(lock_element());
	if (!select) return {};
	const auto options = select->get_options();
	const auto index = select->selected_index(options);
	return index >= 0 ? options[index]->get_value() : String();
}

void HTMLSelectElement::set_value(const String& p_value) {
	if (const auto select = std::dynamic_pointer_cast<HTMLSelectNode>(lock_element())) {
		const auto options = select->get_options();
		const auto match = std::find_if(options.begin(), options.end(), [&](const auto& p_option) {
			return p_option->get_value() == p_value;
		});
		set_selected_index(std::distance(options.begin(), match));
	}
}

String HTMLOptionElement::get_value() const {
	const auto option = std::dynamic_pointer_cast<HTMLOptionNode>(lock_element());
	return option ? option->get_value() : String();
}

void HTMLOptionElement::set_value(const String& p_value) { set_attribute("value", p_value); }

String HTMLOptionElement::get_label() const {
	const auto option = std::dynamic_pointer_cast<HTMLOptionNode>(lock_element());
	if (!option) return {};
	if (const auto* label = option->get_attr("label")) return String::utf8(label);
	return option->get_label();
}

void HTMLOptionElement::set_label(const String& p_value) { set_attribute("label", p_value); }

bool HTMLOptionElement::is_selected() const {
	const auto option = std::dynamic_pointer_cast<HTMLOptionNode>(lock_element());
	return option && option->is_selected();
}

void HTMLOptionElement::set_selected(bool p_value) {
	if (const auto option = std::dynamic_pointer_cast<HTMLOptionNode>(lock_element())) {
		option->set_selected(p_value);
		get_dom().form_state_changed(option);
	}
}

bool dockit::HTMLOutputNode::copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode) const {
	auto* target = dynamic_cast<HTMLOutputNode*>(&p_target);
	if (!target) return false;
	target->default_override = default_override;
	if (default_override && target->children().empty()) {
		litehtml::string text;
		get_text(text);
		auto children = litehtml::dockit_editor::create_text(target->get_document(), text.c_str(), true);
		litehtml::dockit_editor::replace_children(*target, std::move(children));
	}
	return true;
}

String HTMLOutputElement::get_value() const { return get_text_content(); }
void HTMLOutputElement::set_value(const String& p_value) {
	const auto node = std::dynamic_pointer_cast<dockit::HTMLOutputNode>(lock_element());
	if (!node) return;
	const auto previous_default = node->default_override;
	if (!previous_default) node->default_override = get_value();
	if (set_text_content(p_value) != OK) node->default_override = previous_default;
}

String HTMLOutputElement::get_default_value() const {
	const auto node = std::dynamic_pointer_cast<dockit::HTMLOutputNode>(lock_element());
	return node && node->default_override ? *node->default_override : get_value();
}

void HTMLOutputElement::set_default_value(const String& p_value) {
	const auto node = std::dynamic_pointer_cast<dockit::HTMLOutputNode>(lock_element());
	if (!node) return;
	if (node->default_override) node->default_override = p_value;
	else set_text_content(p_value);
}

void HTMLOutputElement::reset() {
	const auto node = std::dynamic_pointer_cast<dockit::HTMLOutputNode>(lock_element());
	if (!node || !node->default_override) return;
	if (set_text_content(*node->default_override) == OK) node->default_override.reset();
}

String HTMLOutputElement::get_name() const { return get_attribute("name"); }
void HTMLOutputElement::set_name(const String& p_value) { set_attribute("name", p_value); }
String HTMLOutputElement::get_html_for() const { return get_attribute("for"); }
void HTMLOutputElement::set_html_for(const String& p_value) { set_attribute("for", p_value); }
Ref<HTMLFormElement> HTMLOutputElement::get_form() const { return get_form_owner(); }
TypedArray<HTMLElement> HTMLOutputElement::get_labels() const {
	TypedArray<HTMLElement> result;
	const auto node = lock_element();
	if (node) for (const auto& label : forms::labels(*node)) result.append(get_dom().make_element(label));
	return result;
}

TypedArray<HTMLElement> HTMLDataListElement::get_options() const { return query_selector_all("option"); }

void HTMLDataListElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_options"), &HTMLDataListElement::get_options);
}

void HTMLOutputElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_form"), &HTMLOutputElement::get_form);
	ClassDB::bind_method(D_METHOD("get_labels"), &HTMLOutputElement::get_labels);
	ClassDB::bind_method(D_METHOD("reset"), &HTMLOutputElement::reset);
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLOutputElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLOutputElement::set_value);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
	ClassDB::bind_method(D_METHOD("get_default_value"), &HTMLOutputElement::get_default_value);
	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &HTMLOutputElement::set_default_value);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "default_value"), "set_default_value", "get_default_value");
	ClassDB::bind_method(D_METHOD("get_name"), &HTMLOutputElement::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "value"), &HTMLOutputElement::set_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ClassDB::bind_method(D_METHOD("get_html_for"), &HTMLOutputElement::get_html_for);
	ClassDB::bind_method(D_METHOD("set_html_for", "value"), &HTMLOutputElement::set_html_for);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "html_for"), "set_html_for", "get_html_for");
}
