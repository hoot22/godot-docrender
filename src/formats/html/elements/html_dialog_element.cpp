#include "html_dialog_element.h"
#include "html_dialog_node.h"

#include "formats/html/events/html_event.h"
#include "formats/html/html_layout.h"
#include "nodes/html_label.h"

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <litehtml/html.h>

using namespace godot;
using dockit::HTMLDialogNode;

void HTMLDialogCloseRequest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_reason"), &HTMLDialogCloseRequest::get_reason);
	ClassDB::bind_method(D_METHOD("get_return_value"), &HTMLDialogCloseRequest::get_return_value);
	ClassDB::bind_method(D_METHOD("prevent_default"), &HTMLDialogCloseRequest::prevent_default);
	ClassDB::bind_method(D_METHOD("is_default_prevented"), &HTMLDialogCloseRequest::is_default_prevented);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reason"), "", "get_reason");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "return_value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT),
		"", "get_return_value");
	BIND_ENUM_CONSTANT(SCRIPT);
	BIND_ENUM_CONSTANT(ESCAPE);
	BIND_ENUM_CONSTANT(BACKDROP);
}

void HTMLDialogElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_open"), &HTMLDialogElement::is_open);
	ClassDB::bind_method(D_METHOD("set_open", "open"), &HTMLDialogElement::set_open);
	ClassDB::bind_method(D_METHOD("is_modal"), &HTMLDialogElement::is_modal);
	ClassDB::bind_method(D_METHOD("get_closed_by"), &HTMLDialogElement::get_closed_by);
	ClassDB::bind_method(D_METHOD("set_closed_by", "value"), &HTMLDialogElement::set_closed_by);
	ClassDB::bind_method(D_METHOD("get_return_value"), &HTMLDialogElement::get_return_value);
	ClassDB::bind_method(D_METHOD("set_return_value", "value"), &HTMLDialogElement::set_return_value);
	ClassDB::bind_method(D_METHOD("show"), &HTMLDialogElement::show);
	ClassDB::bind_method(D_METHOD("show_modal"), &HTMLDialogElement::show_modal);
	ClassDB::bind_method(D_METHOD("close", "return_value"), &HTMLDialogElement::close, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("request_close", "return_value"), &HTMLDialogElement::request_close, DEFVAL(Variant()));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "open"), "set_open", "is_open");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "closed_by"), "set_closed_by", "get_closed_by");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "return_value"), "set_return_value", "get_return_value");
	ADD_SIGNAL(MethodInfo("close_requested",
		PropertyInfo(Variant::OBJECT, "request", PROPERTY_HINT_RESOURCE_TYPE, "HTMLDialogCloseRequest")));
	ADD_SIGNAL(MethodInfo("closed", PropertyInfo(Variant::STRING, "return_value")));
}

bool HTMLDialogElement::is_open() const { return has_attribute("open"); }

void HTMLDialogElement::set_open(bool p_open) {
	if (p_open) set_attribute("open", "");
	else remove_attribute("open");
}

bool HTMLDialogElement::is_modal() const {
	if (const auto node = std::dynamic_pointer_cast<HTMLDialogNode>(lock_element()))
		return node->has_pseudo_class(litehtml::_id("modal"));
	return false;
}

String HTMLDialogElement::get_closed_by() const {
	const auto node = lock_element();
	if (!node) return {};
	const auto keyword = litehtml::lowcase(node->get_attr("closedby", ""));
	if (keyword == "none" || keyword == "closerequest" || keyword == "any") return String::utf8(keyword.c_str());
	return is_modal() ? "closerequest" : "none";
}

void HTMLDialogElement::set_closed_by(const String& p_value) {
	set_attribute("closedby", p_value);
}

String HTMLDialogElement::get_return_value() const {
	if (const auto node = std::dynamic_pointer_cast<HTMLDialogNode>(lock_element())) return node->return_value;
	return {};
}

void HTMLDialogElement::set_return_value(const String& p_value) {
	if (const auto node = std::dynamic_pointer_cast<HTMLDialogNode>(lock_element())) node->return_value = p_value;
}

Error HTMLDialogElement::show() {
	return open_dialog(false);
}

Error HTMLDialogElement::show_modal() {
	return open_dialog(true);
}

Error HTMLDialogElement::open_dialog(bool p_modal) {
	auto* owner = get_owner();
	const auto node = lock_element();
	if (!owner || !node) return ERR_INVALID_PARAMETER;
	if (is_open()) return is_modal() == p_modal ? OK : ERR_INVALID_PARAMETER;
	const bool connected = is_connected();
	if (p_modal && !connected) return ERR_INVALID_PARAMETER;

	const ObjectID host = owner->dialog_host;
	if (connected) {
		if (auto* label = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(host)))
			label->remember_dialog_focus(Ref<HTMLDialogElement>(this), p_modal);
	}
	if (p_modal) owner->push_modal(node);
	const Error error = set_attribute("open", "");
	if (error == OK && p_modal && get_owner() == owner && is_modal()) {
		if (auto* label = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(host))) label->on_dialog_opened();
	}
	return error;
}

Error HTMLDialogElement::close(const Variant& p_return_value) {
	if (!is_valid()) return ERR_INVALID_PARAMETER;
	if (!is_open()) return OK;
	if (p_return_value.get_type() != Variant::NIL && p_return_value.get_type() != Variant::STRING) return ERR_INVALID_PARAMETER;
	const Error error = remove_attribute("open");
	if (error != OK) return error;
	if (p_return_value.get_type() == Variant::STRING) set_return_value(p_return_value);
	const String value = get_return_value();
	const auto* owner = get_owner();
	if (auto* label = owner ? Object::cast_to<HTMLLabel>(ObjectDB::get_instance(owner->dialog_host)) : nullptr)
		label->queue_dialog_closed(Ref<HTMLDialogElement>(this), value);
	else callable_mp(this, &HTMLDialogElement::emit_closed).call_deferred(value);
	return OK;
}

uint64_t HTMLDialogElement::get_open_sequence() const {
	const auto node = std::dynamic_pointer_cast<HTMLDialogNode>(lock_element());
	return node ? node->open_sequence : 0;
}

Error HTMLDialogElement::request_close(const Variant& p_return_value) {
	return dispatch_close_request(HTMLDialogCloseRequest::SCRIPT, p_return_value);
}

Error HTMLDialogElement::dispatch_close_request(HTMLDialogCloseRequest::Reason p_reason, const Variant& p_return_value) {
	HTMLLayout* owner = get_owner();
	if (!owner) return ERR_INVALID_PARAMETER;
	const auto node = std::dynamic_pointer_cast<HTMLDialogNode>(lock_element());
	if (!node) return ERR_INVALID_PARAMETER;
	if (!node->open_sequence || !is_open() || !is_connected()) return OK;
	if (p_return_value.get_type() != Variant::NIL && p_return_value.get_type() != Variant::STRING) return ERR_INVALID_PARAMETER;
	if (node->dispatching_close_request) return ERR_SKIP;
	if (p_reason != HTMLDialogCloseRequest::SCRIPT) {
		const String policy = get_closed_by();
		if (policy == "none" || (p_reason == HTMLDialogCloseRequest::BACKDROP && policy != "any")) return ERR_SKIP;
	}
	const Ref<HTMLDialogElement> keep_alive(this);
	const uint64_t sequence = node->open_sequence;
	const ObjectID owner_id(owner->get_instance_id());
	Ref<HTMLEvent> cancel_event = dockit::make_dom_event(StringName("cancel"), false, true);
	dispatch_event(cancel_event);
	if (cancel_event->is_default_prevented()) return ERR_SKIP;
	HTMLLayout* current_owner = get_owner();
	if (!current_owner || ObjectID(current_owner->get_instance_id()) != owner_id || !is_valid() || node != lock_element() ||
		node->open_sequence != sequence || !is_open() || !is_connected()) return OK;
	Ref<HTMLDialogCloseRequest> request;
	request.instantiate();
	request->reason = p_reason;
	request->return_value = p_return_value;
	request->dispatching = true;
	node->dispatching_close_request = true;
	emit_signal("close_requested", request);
	node->dispatching_close_request = false;
	request->dispatching = false;
	if (request->is_default_prevented()) return ERR_SKIP;
	current_owner = get_owner();
	if (!current_owner || ObjectID(current_owner->get_instance_id()) != owner_id || node != lock_element() ||
		node->open_sequence != sequence || !is_open() || !is_connected()) return OK;
	if (p_reason != HTMLDialogCloseRequest::SCRIPT && current_owner->active_dialog().ptr() != this) return OK;
	return close(p_return_value);
}

void HTMLDialogElement::emit_closed(const String& p_value) {
	if (!is_valid()) return;
	const Ref<HTMLDialogElement> keep_alive(this);
	dispatch_event(dockit::make_dom_event(StringName("close"), false, false));
	if (is_valid()) emit_signal("closed", p_value);
}
