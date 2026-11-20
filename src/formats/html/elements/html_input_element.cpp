#include "html_input_element.h"
#include "html_form_elements.h"

#include "html_input_node.h"
#include "formats/html/dom/html_dom.h"
#include "formats/html/events/html_event.h"
#include "formats/html/html_layout.h"
#include "nodes/html_label.h"

#include <cmath>
#include <limits>

using namespace godot;

Ref<HTMLFormElement> HTMLInputElement::get_form() const { return get_form_owner(); }
using dockit::HTMLInputNode;

void HTMLInputFile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &HTMLInputFile::get_name);
	ClassDB::bind_method(D_METHOD("get_size"), &HTMLInputFile::get_size);
	ClassDB::bind_method(D_METHOD("read_bytes", "offset", "count"), &HTMLInputFile::read_bytes);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "size"), "", "get_size");
}

Ref<HTMLInputFile> HTMLInputFile::open(const String& p_path) {
	const auto open_file = FileAccess::open(p_path, FileAccess::READ);
	if (open_file.is_null()) return {};
	Ref<HTMLInputFile> result;
	result.instantiate();
	result->file = open_file;
	result->name = p_path.get_file();
	result->size = open_file->get_length();
	return result;
}

String HTMLInputFile::get_name() const { return name; }
int64_t HTMLInputFile::get_size() const { return size; }

PackedByteArray HTMLInputFile::read_bytes(int64_t p_offset, int64_t p_count) {
	if (file.is_null() || p_offset < 0 || p_offset >= size || p_count <= 0) return {};
	const std::lock_guard<std::mutex> lock(read_mutex);
	file->seek(p_offset);
	return file->get_buffer(std::min(size - p_offset, p_count));
}

void HTMLInputElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_form"), &HTMLInputElement::get_form);
	ClassDB::bind_method(D_METHOD("get_type"), &HTMLInputElement::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "value"), &HTMLInputElement::set_type);
	ClassDB::bind_method(D_METHOD("get_value_as_number"), &HTMLInputElement::get_value_as_number);
	ClassDB::bind_method(D_METHOD("set_value_as_number", "value"), &HTMLInputElement::set_value_as_number);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "value_as_number"), "set_value_as_number", "get_value_as_number");
	ClassDB::bind_method(D_METHOD("get_value"), &HTMLInputElement::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &HTMLInputElement::set_value);
	ClassDB::bind_method(D_METHOD("is_checked"), &HTMLInputElement::is_checked);
	ClassDB::bind_method(D_METHOD("set_checked", "checked"), &HTMLInputElement::set_checked);
	ClassDB::bind_method(D_METHOD("get_selection_start"), &HTMLInputElement::get_selection_start);
	ClassDB::bind_method(D_METHOD("get_selection_end"), &HTMLInputElement::get_selection_end);
	ClassDB::bind_method(D_METHOD("set_selection_range", "start", "end"), &HTMLInputElement::set_selection_range);
	ClassDB::bind_method(D_METHOD("is_indeterminate"), &HTMLInputElement::is_indeterminate);
	ClassDB::bind_method(D_METHOD("set_indeterminate", "value"), &HTMLInputElement::set_indeterminate);
	ClassDB::bind_method(D_METHOD("get_validity"), &HTMLInputElement::get_validity);
	ClassDB::bind_method(D_METHOD("step_up", "count"), &HTMLInputElement::step_up, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("step_down", "count"), &HTMLInputElement::step_down, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("get_value_as_date"), &HTMLInputElement::get_value_as_date);
	ClassDB::bind_method(D_METHOD("set_value_as_date", "parts"), &HTMLInputElement::set_value_as_date);
	ClassDB::bind_method(D_METHOD("get_files"), &HTMLInputElement::get_files);
	ClassDB::bind_method(D_METHOD("show_picker"), &HTMLInputElement::show_picker);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "checked"), "set_checked", "is_checked");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "indeterminate"), "set_indeterminate", "is_indeterminate");
}

std::shared_ptr<HTMLInputNode> HTMLInputElement::get_input() const {
	return std::dynamic_pointer_cast<HTMLInputNode>(lock_element());
}

String HTMLInputElement::get_type() const {
	const auto input = get_input();
	return input ? input->type_name() : String();
}

void HTMLInputElement::set_type(const String& p_value) { set_attribute("type", p_value); }

double HTMLInputElement::get_value_as_number() const {
	const auto input = get_input();
	return input ? input->value_as_number() : std::numeric_limits<double>::quiet_NaN();
}

void HTMLInputElement::set_value_as_number(double p_value) {
	const auto input = get_input();
	if (!input || (input->type() != HTMLInputNode::Type::NUMBER && input->type() != HTMLInputNode::Type::RANGE &&
		input->time_kind() == dockit::input_time::Kind::none)) return;
	if (std::isfinite(p_value)) set_value(input->format_number(p_value));
	else if (std::isnan(p_value)) set_value("");
}

String HTMLInputElement::get_value() const {
	const auto input = get_input();
	return input ? input->get_value() : String();
}

void HTMLInputElement::set_value(const String& p_value) {
	if (const auto input = get_input()) {
		if (input->type() == HTMLInputNode::Type::FILE) {
			if (p_value.is_empty() && input->set_files({})) get_owner()->update_input(*input);
		} else if (input->is_toggle() || input->is_button() || input->type() == HTMLInputNode::Type::HIDDEN) {
			set_attribute("value", p_value);
		} else if (input->editor.set_value(input->sanitize_value(p_value))) {
			get_owner()->update_input(*input);
		}
	}
}

bool HTMLInputElement::is_checked() const {
	const auto input = get_input();
	return input && input->is_checked();
}

void HTMLInputElement::set_checked(bool p_checked) {
	if (const auto input = get_input(); input && input->set_checked(p_checked)) {
		get_owner()->update_input(*input);
	}
}

bool HTMLInputElement::is_indeterminate() const {
	const auto input = get_input();
	return input && input->is_indeterminate();
}

void HTMLInputElement::set_indeterminate(bool p_value) {
	if (const auto input = get_input(); input && input->set_indeterminate(p_value)) get_owner()->update_input(*input);
}

Dictionary HTMLInputElement::get_validity() const {
	const auto input = get_input();
	return input ? input->validity() : Dictionary();
}

Error HTMLInputElement::step_up(int p_count) {
	const auto input = get_input();
	if (!input) return ERR_UNCONFIGURED;
	bool changed = false;
	const auto error = input->step_number(p_count, changed);
	if (changed) {
		input->editor.take_user_change();
		get_owner()->update_input(*input);
	}
	return error;
}

Error HTMLInputElement::step_down(int p_count) {
	if (p_count == std::numeric_limits<int>::min()) return ERR_INVALID_PARAMETER;
	return step_up(-p_count);
}

Dictionary HTMLInputElement::get_value_as_date() const {
	const auto input = get_input();
	if (!input || input->time_kind() == dockit::input_time::Kind::local) return {};
	return dockit::input_time::parts(input->time_kind(), input->value_as_number());
}

Error HTMLInputElement::set_value_as_date(const Dictionary& p_value) {
	const auto input = get_input();
	using Kind = dockit::input_time::Kind;
	if (!input || input->time_kind() == Kind::none || input->time_kind() == Kind::local) return ERR_UNAVAILABLE;
	if (p_value.is_empty()) {
		set_value("");
		return OK;
	}
	const auto value = dockit::input_time::from_parts(input->time_kind(), p_value);
	if (!value) return ERR_INVALID_PARAMETER;
	set_value_as_number(*value);
	return OK;
}

TypedArray<HTMLInputFile> HTMLInputElement::get_files() const {
	const auto input = get_input();
	return input && input->type() == HTMLInputNode::Type::FILE ? TypedArray<HTMLInputFile>(input->get_files().duplicate()) :
		TypedArray<HTMLInputFile>();
}

Error HTMLInputElement::show_picker() {
	const auto* owner = get_owner();
	auto* label = owner ? Object::cast_to<HTMLLabel>(ObjectDB::get_instance(owner->dialog_host)) : nullptr;
	return label ? label->open_input_picker(Ref<HTMLInputElement>(this)) : ERR_UNAVAILABLE;
}

int64_t HTMLInputElement::get_selection_start() const {
	const auto input = get_input();
	return input && input->supports_selection() ? input->editor.selection_start() : -1;
}

int64_t HTMLInputElement::get_selection_end() const {
	const auto input = get_input();
	return input && input->supports_selection() ? input->editor.selection_end() : -1;
}

void HTMLInputElement::set_selection_range(int64_t p_start, int64_t p_end) {
	if (const auto input = get_input(); input && input->supports_selection()) {
		const int64_t old_start = input->editor.selection_start();
		const int64_t old_end = input->editor.selection_end();
		if (!input->editor.set_selection(std::min(p_start, p_end), p_end)) return;
		get_owner()->update_input(*input);
		if (old_start != input->editor.selection_start() || old_end != input->editor.selection_end()) {
			dispatch_event(dockit::make_dom_event(StringName("select"), true, false));
			dispatch_event(dockit::make_dom_event(StringName("selectionchange"), true, false));
			get_dom().queue_selectionchange();
		}
	}
}
