#include "html_event_target.h"

#include "html_event.h"

#include <array>

using namespace godot;

namespace {
	constexpr const char* EVENT_HANDLER_TYPES[] = {
		"abort", "animationcancel", "animationend", "animationiteration", "animationstart", "auxclick", "DOMContentLoaded",
		"beforeinput", "beforetoggle", "blur", "canplay", "canplaythrough", "cancel", "change", "click",
		"close", "compositionend", "compositionstart", "compositionupdate", "contextmenu", "copy", "cut",
		"dblclick", "drag", "dragend", "dragenter", "dragleave", "dragover", "dragstart",
		"drop", "durationchange", "emptied", "ended", "error", "focus", "focusin", "focusout",
		"gotpointercapture", "input", "invalid", "keydown", "keyup", "load", "loadstart", "loadeddata", "loadedmetadata",
		"mousedown", "mouseenter", "mouseleave", "mousemove", "mouseout", "mouseover", "mouseup", "paste",
		"pause", "play", "playing", "pointercancel", "pointerdown", "pointerenter", "pointerleave", "pointermove",
		"pointerout", "pointerover", "pointerup", "progress", "ratechange", "reset", "resize", "scroll", "scrollend",
		"seeked", "seeking", "select", "selectionchange", "stalled", "submit", "suspend", "timeupdate",
		"toggle", "touchcancel", "touchend", "touchmove", "touchstart", "transitioncancel", "transitionend",
		"transitionrun", "transitionstart", "visibilitychange", "volumechange", "waiting", "wheel", "lostpointercapture"
	};

	StringName event_type_from_handler_property(const StringName& p_name) {
		const String name = String(p_name);
		if (!name.begins_with("on") || name.length() <= 2) return {};
		const StringName type(name.substr(2).to_lower());
		if (type == StringName("domcontentloaded")) return StringName("DOMContentLoaded");
		for (const char* candidate : EVENT_HANDLER_TYPES) {
			if (type == StringName(candidate)) return type;
		}
		return {};
	}
}

void HTMLEventTarget::_bind_methods() {
	ClassDB::bind_method(D_METHOD("listen", "type", "callable", "options"),
		&HTMLEventTarget::listen, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("is_valid"), &HTMLEventTarget::is_valid);
	ClassDB::bind_method(D_METHOD("add_event_listener", "type", "callable", "options"),
		&HTMLEventTarget::add_event_listener, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("remove_event_listener", "type", "callable", "options"),
		&HTMLEventTarget::remove_event_listener, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("dispatch_event", "event"), &HTMLEventTarget::dispatch_event);
	ClassDB::bind_method(D_METHOD("has_event_listener", "type"), &HTMLEventTarget::has_event_listener);
}

bool HTMLEventTarget::_set(const StringName& p_name, const Variant& p_value) {
	const StringName type = event_type_from_handler_property(p_name);
	if (type == StringName()) return false;
	if (p_value.get_type() != Variant::CALLABLE && p_value.get_type() != Variant::NIL) return false;
	event_handler_overrides.insert(type);
	event_handlers[type] = p_value.get_type() == Variant::CALLABLE ? Callable(p_value) : Callable();
	return true;
}

bool HTMLEventTarget::_get(const StringName& p_name, Variant& r_value) const {
	const StringName type = event_type_from_handler_property(p_name);
	if (type == StringName()) return false;
	const Callable* callable = event_handlers.getptr(type);
	r_value = callable ? Variant(*callable) : Variant(Callable());
	return true;
}

void HTMLEventTarget::_get_property_list(List<PropertyInfo>* p_list) const {
	for (const char* type : EVENT_HANDLER_TYPES) {
		p_list->push_back(PropertyInfo(Variant::CALLABLE, String("on") + type));
	}
}

bool HTMLEventTarget::has_event_handler_override(const StringName& p_type) const {
	return event_handler_overrides.has(p_type);
}

Callable HTMLEventTarget::get_event_handler(const StringName& p_type) const {
	const Callable* callable = event_handlers.getptr(p_type);
	return callable ? *callable : Callable();
}

void HTMLListenerHandle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("cancel"), &HTMLListenerHandle::cancel);
	ClassDB::bind_method(D_METHOD("is_canceled"), &HTMLListenerHandle::is_canceled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "canceled"), "", "is_canceled");
}

void HTMLListenerHandle::cancel() {
	if (canceled) return;
	canceled = true;
	if (target.is_valid()) {
		Dictionary options;
		options["capture"] = capture;
		target->remove_event_listener(type, callable, options);
	}
	target.unref();
	callable = Callable();
}

Ref<HTMLListenerHandle> HTMLEventTarget::listen(const StringName& p_type, const Callable& p_callable,
	const Dictionary& p_options) {
	if (add_event_listener(p_type, p_callable, p_options) != OK) return {};
	Ref<HTMLListenerHandle> handle;
	handle.instantiate();
	handle->target = Ref<HTMLEventTarget>(this);
	handle->type = p_type;
	handle->callable = p_callable;
	handle->capture = p_options.get("capture", false);
	return handle;
}
