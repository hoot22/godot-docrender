#include "html_label.h"

#include "formats/html/dom/html_dom.h"
#include "formats/html/elements/html_form_elements.h"
#include "formats/html/elements/html_input_node.h"
#include "formats/html/events/html_event.h"
#include "formats/html/events/html_window.h"
#include "formats/html/html_layout.h"

#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input.hpp>

#include <utility>

using namespace godot;

void HTMLLabel::set_event_debug(bool p_enabled) {
	event_debug = p_enabled;
	if (layout.is_valid()) layout->get_dom().set_event_debug(p_enabled);
}

int64_t HTMLLabel::get_event_listener_count(const StringName& p_type) const {
	return layout.is_valid() ? int64_t(layout->get_dom().get_event_listener_count(p_type)) : 0;
}

String HTMLLabel::get_document_visibility_state() const {
	return document_visibility_state.value_or(is_visible_in_tree()) ? String("visible") : String("hidden");
}

void HTMLLabel::queue_document_ready_events() {
	if (!parsed || source_dirty || document_ready_events_queued || document_ready_events_sent) return;
	document_ready_events_queued = true;
	callable_mp(this, &HTMLLabel::dispatch_document_ready_events).call_deferred();
}

void HTMLLabel::dispatch_document_ready_events() {
	document_ready_events_queued = false;
	if (document_ready_events_sent || source_dirty || !parsed || layout.is_null()) return;
	document_ready_events_sent = true;
	const ObjectID id(get_instance_id());
	const Ref<HTMLLayout> keep_layout = layout;
	auto& dom = keep_layout->get_dom();
	const Ref<HTMLDocumentNode> document_node = dom.get_document_node();
	if (document_node.is_valid()) dom.dispatch_event(document_node, dockit::make_dom_event(StringName("DOMContentLoaded"), true, false));
	if (!ObjectDB::get_instance(id)) return;
	const Ref<HTMLWindow> window = dom.get_window();
	if (window.is_valid()) dom.dispatch_event(window, dockit::make_dom_event(StringName("load"), false, false));
}

void HTMLLabel::queue_window_resize_event(const Vector2i& p_viewport_size) {
	if (p_viewport_size.x <= 0 || p_viewport_size.y <= 0) return;
	if (!last_window_resize_event_size) {
		last_window_resize_event_size = p_viewport_size;
		return;
	}
	if (*last_window_resize_event_size == p_viewport_size) return;
	pending_window_resize_event_size = p_viewport_size;
	if (window_resize_event_queued) return;
	window_resize_event_queued = true;
	callable_mp(this, &HTMLLabel::dispatch_window_resize_event).call_deferred();
}

void HTMLLabel::dispatch_window_resize_event() {
	window_resize_event_queued = false;
	const std::optional<Vector2i> pending = std::exchange(pending_window_resize_event_size, std::nullopt);
	if (!pending || !is_inside_tree() || *pending != viewport_size ||
		(last_window_resize_event_size && *last_window_resize_event_size == *pending) || layout.is_null()) return;
	last_window_resize_event_size = *pending;
	const Ref<HTMLWindow> window = layout->get_dom().get_window();
	if (!window.is_valid()) return;
	Ref<HTMLEvent> event = dockit::make_dom_event(StringName("resize"), false, false);
	event->set_composed(false);
	layout->get_dom().dispatch_event(window, event);
}

void HTMLLabel::update_document_visibility_state(bool p_visible, bool p_dispatch) {
	if (!document_visibility_state) {
		document_visibility_state = p_visible;
		return;
	}
	if (*document_visibility_state == p_visible) return;
	document_visibility_state = p_visible;
	if (!p_dispatch || layout.is_null()) return;
	const Ref<HTMLDocumentNode> document_node = layout->get_dom().get_document_node();
	if (!document_node.is_valid()) return;
	Ref<HTMLEvent> event = dockit::make_dom_event(StringName("visibilitychange"), false, false);
	event->set_composed(false);
	layout->get_dom().dispatch_event(document_node, event);
}

void HTMLLabel::dispatch_window_focus_event(bool p_focused) {
	if (window_focus_state && *window_focus_state == p_focused) return;
	window_focus_state = p_focused;
	if (layout.is_null()) return;
	const Ref<HTMLWindow> window = layout->get_dom().get_window();
	if (!window.is_valid()) return;
	Ref<HTMLEvent> event = dockit::make_dom_event(
		p_focused ? StringName("focus") : StringName("blur"), false, false);
	event->set_composed(false);
	layout->get_dom().dispatch_event(window, event);
}

namespace {
	Ref<HTMLFocusEvent> make_focus_event(const StringName& p_type, bool p_bubbles, const Ref<HTMLElement>& p_related) {
		Ref<HTMLFocusEvent> event;
		event.instantiate();
		event->set_type(p_type);
		event->set_bubbles(p_bubbles);
		event->set_cancelable(false);
		event->set_related_target(p_related);
		event->mark_trusted();
		return event;
	}
}

bool HTMLLabelCanvas::dispatch_pointer_event(const StringName& p_type, const Ref<HTMLElement>& p_target,
	const InputEventMouseButton& p_event, const Vector2& p_point, bool p_cancelable) {
	PointerInfo info;
	info.position = p_point;
	info.button = int(p_event.get_button_index());
	info.buttons = int(p_event.get_button_mask());
	info.pressed = p_event.is_pressed();
	info.shift = p_event.is_shift_pressed();
	info.ctrl = p_event.is_ctrl_pressed();
	info.alt = p_event.is_alt_pressed();
	info.meta = p_event.is_meta_pressed();
	info.device = p_event.get_device();
	return dispatch_pointer_event(p_type, p_target, info, p_cancelable);
}

bool HTMLLabelCanvas::dispatch_pointer_event(const StringName& p_type, const Ref<HTMLElement>& p_target,
	const PointerInfo& p_info, bool p_cancelable, const Ref<HTMLElement>& p_related_target) {
	if (!label || label->layout.is_null()) return true;
	if (p_target.is_null() || !p_target->is_valid()) return true;
	auto& dom = label->layout->get_dom();
	const bool pointer = p_type == StringName("pointerdown") || p_type == StringName("pointerup") ||
		p_type == StringName("pointermove") || p_type == StringName("pointerover") || p_type == StringName("pointerout") ||
		p_type == StringName("pointerenter") || p_type == StringName("pointerleave") || p_type == StringName("pointercancel");
	if (p_type == StringName("pointerdown")) dom.begin_pointer(p_info.pointer_id);
	Ref<HTMLElement> dispatch_target = p_target;
	if (pointer && p_type != StringName("pointerdown")) {
		if (const Ref<HTMLElement> captured = dom.get_pointer_capture(p_info.pointer_id); captured.is_valid()) dispatch_target = captured;
	}
	Ref<HTMLMouseEvent> event;
	Ref<HTMLPointerEvent> pointer_event;
	if (pointer) {
		pointer_event.instantiate();
		event = pointer_event;
	}
	else event.instantiate();
	const bool bubbles = p_type != StringName("mouseenter") && p_type != StringName("mouseleave") &&
		p_type != StringName("pointerenter") && p_type != StringName("pointerleave");
	event->set_type(p_type);
	event->set_bubbles(bubbles);
	event->set_cancelable(p_cancelable);
	event->set_composed(true);
	event->mark_trusted();
	const int button = p_info.button;
	event->set_button(button);
	event->set_buttons(p_info.buttons);
	event->set_detail(p_info.detail);
	event->set_position(p_info.position - label->get_visible_rect().position);
	event->set_related_target(p_related_target);
	event->set_shift_pressed(p_info.shift);
	event->set_ctrl_pressed(p_info.ctrl);
	event->set_alt_pressed(p_info.alt);
	event->set_meta_pressed(p_info.meta);
	if (pointer_event.is_valid()) {
		pointer_event->set_pointer_id(p_info.pointer_id);
		pointer_event->set_pointer_type(p_info.kind == PointerKind::TOUCH ? StringName("touch") :
			p_info.kind == PointerKind::PEN ? StringName("pen") : StringName("mouse"));
		pointer_event->set_is_primary(p_info.primary);
	}
	dom.dispatch_event(dispatch_target, event);
	const bool not_canceled = !event->is_default_prevented();
	if (p_type == StringName("pointerup") || p_type == StringName("pointercancel")) dom.end_pointer(p_info.pointer_id);
	return not_canceled;
}

bool HTMLLabelCanvas::dispatch_hover_events(const TypedArray<HTMLElement>& p_previous, const Vector2& p_position) {
	if (!label || label->layout.is_null()) return false;
	const TypedArray<HTMLElement> next = label->layout->get_hovered_elements();
	std::vector<Ref<HTMLElement>> leaving;
	std::vector<Ref<HTMLElement>> entering;
	Ref<HTMLElement> old_target;
	Ref<HTMLElement> new_target;
	for (int64_t index = 0; index < p_previous.size(); ++index) {
		const Ref<HTMLElement> element = p_previous[index];
		if (element.is_null() || !element->is_valid() || !element->is_connected()) continue;
		if (old_target.is_null()) old_target = element;
		bool present = false;
		for (int64_t other = 0; other < next.size(); ++other) {
			const Ref<HTMLElement> candidate = next[other];
			if (candidate.ptr() == element.ptr()) {
				present = true;
				break;
			}
		}
		if (!present) leaving.push_back(element);
	}
	for (int64_t index = next.size() - 1; index >= 0; --index) {
		const Ref<HTMLElement> element = next[index];
		if (element.is_null() || !element->is_valid() || !element->is_connected()) continue;
		new_target = element;
		bool present = false;
		for (int64_t other = 0; other < p_previous.size(); ++other) {
			const Ref<HTMLElement> candidate = p_previous[other];
			if (candidate.ptr() == element.ptr()) {
				present = true;
				break;
			}
		}
		if (!present) entering.push_back(element);
	}
	PointerInfo info;
	info.position = p_position;
	info.button = -1;
	info.buttons = int(Input::get_singleton()->get_mouse_button_mask());
	if (old_target != new_target && old_target.is_valid()) {
		dispatch_pointer_event(StringName("pointerout"), old_target, info, false, new_target);
		for (const Ref<HTMLElement>& element : leaving)
			dispatch_pointer_event(StringName("pointerleave"), element, info, false, new_target);
		dispatch_pointer_event(StringName("mouseout"), old_target, info, false, new_target);
		for (const Ref<HTMLElement>& element : leaving)
			dispatch_pointer_event(StringName("mouseleave"), element, info, false, new_target);
	}
	if (old_target != new_target && new_target.is_valid()) {
		dispatch_pointer_event(StringName("pointerover"), new_target, info, false, old_target);
		for (const Ref<HTMLElement>& element : entering)
			dispatch_pointer_event(StringName("pointerenter"), element, info, false, old_target);
		dispatch_pointer_event(StringName("mouseover"), new_target, info, false, old_target);
		for (const Ref<HTMLElement>& element : entering)
			dispatch_pointer_event(StringName("mouseenter"), element, info, false, old_target);
	}
	return false;
}

bool HTMLLabelCanvas::dispatch_key_event(const StringName& p_type, const InputEventKey& p_key) {
	if (!label || label->layout.is_null()) return false;
	auto& dom = label->layout->get_dom();
	const bool pressed = p_type == StringName("keydown");
	Ref<HTMLKeyboardEvent> event;
	event.instantiate();
	event->set_type(p_type);
	event->set_bubbles(true);
	event->set_cancelable(pressed);
	event->mark_trusted();
	event->set_keycode(int(p_key.get_keycode()));
	event->set_physical_keycode(int(p_key.get_physical_keycode()));
	event->set_key_label(int(p_key.get_key_label()));
	event->set_unicode(int(p_key.get_unicode()));
	event->set_shift_pressed(p_key.is_shift_pressed());
	event->set_ctrl_pressed(p_key.is_ctrl_pressed());
	event->set_alt_pressed(p_key.is_alt_pressed());
	event->set_meta_pressed(p_key.is_meta_pressed());
	event->set_echo(p_key.is_echo());
	Ref<HTMLElement> target = focused_element;
	if (target.is_null() || !target->is_valid()) {
		target = label->layout->get_body();
		if (target.is_null()) return false;
	}
	dom.dispatch_event(target, event);
	return event->is_default_prevented();
}

void HTMLLabelCanvas::dispatch_blur_events(const Ref<HTMLElement>& p_previous, const Ref<HTMLElement>& p_next) {
	if (!label || label->layout.is_null()) return;
	if (p_previous.ptr() == p_next.ptr()) return;
	auto& dom = label->layout->get_dom();
	if (p_previous.is_valid() && p_previous->is_valid()) {
		dom.dispatch_event(p_previous, make_focus_event(StringName("blur"), false, p_next));
		dom.dispatch_event(p_previous, make_focus_event(StringName("focusout"), true, p_next));
	}
}

void HTMLLabelCanvas::dispatch_focus_events(const Ref<HTMLElement>& p_previous, const Ref<HTMLElement>& p_next) {
	if (!label || label->layout.is_null()) return;
	if (p_previous.ptr() == p_next.ptr()) return;
	auto& dom = label->layout->get_dom();
	if (p_next.is_valid() && p_next->is_valid()) {
		dom.dispatch_event(p_next, make_focus_event(StringName("focus"), false, p_previous));
		dom.dispatch_event(p_next, make_focus_event(StringName("focusin"), true, p_previous));
	}
}

bool HTMLLabel::dispatch_form_event(const StringName& p_type, const Ref<HTMLFormElement>& p_form,
	const Ref<HTMLElement>& p_submitter) {
	if (layout.is_null() || p_form.is_null() || !p_form->is_valid()) return false;
	auto& dom = layout->get_dom();
	Ref<HTMLSubmitEvent> event;
	event.instantiate();
	event->set_type(p_type);
	event->set_bubbles(true);
	event->set_cancelable(true);
	event->set_submitter(p_submitter);
	event->mark_trusted();
	dom.dispatch_event(p_form, event);
	return event->is_default_prevented();
}

bool HTMLLabel::dispatch_control_event(const StringName& p_type, const Ref<HTMLElement>& p_element) {
	if (layout.is_null() || p_element.is_null() || !p_element->is_valid()) return false;
	if (p_type == StringName("input")) return dispatch_input_event(p_type, p_element, {}, {}, false);
	auto& dom = layout->get_dom();
	dom.dispatch_event(p_element, dockit::make_dom_event(p_type, true, false));
	return false;
}

bool HTMLLabel::dispatch_input_event(const StringName& p_type, const Ref<HTMLElement>& p_element,
	const StringName& p_input_type, const String& p_data, bool p_is_composing) {
	if (layout.is_null() || p_element.is_null() || !p_element->is_valid()) return false;
	const ObjectID host_id(get_instance_id());
	const auto original_layout = layout;
	const uint64_t adoption = p_element->adoption_version;
	const auto control = layout->text_control(p_element);
	const auto* input = dynamic_cast<const dockit::HTMLInputNode*>(control.get());
	const String input_type = input ? input->type_name() : String();
	const uint64_t reset = control ? control->editor.get_reset_version() : 0;
	Ref<HTMLInputEvent> event;
	event.instantiate();
	event->set_type(p_type);
	event->set_bubbles(true);
	event->set_cancelable(p_type == StringName("beforeinput"));
	event->set_composed(true);
	event->set_data(p_data);
	event->set_input_type(p_input_type);
	event->set_is_composing(p_is_composing);
	event->mark_trusted();
	layout->get_dom().dispatch_event(p_element, event);
	if (event->is_default_prevented()) return false;
	if (p_type != StringName("beforeinput")) return true;
	auto* host = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(host_id));
	if (!host || host->layout != original_layout || p_element->adoption_version != adoption ||
		!original_layout->is_interaction_target_available(p_element, false)) return false;
	const auto current = original_layout->text_control(p_element);
	if (current != control) return false;
	if (current && (!current->is_editable() || current->editor.get_reset_version() != reset)) return false;
	if (input && input->type_name() != input_type) return false;
	return current || p_element->is_content_editable();
}

void HTMLLabel::dispatch_composition_event(const StringName& p_type, const Ref<HTMLElement>& p_element, const String& p_data) {
	if (layout.is_null() || p_element.is_null() || !p_element->is_valid()) return;
	Ref<HTMLCompositionEvent> event;
	event.instantiate();
	event->set_type(p_type);
	event->set_bubbles(true);
	event->set_cancelable(false);
	event->set_composed(true);
	event->set_data(p_data);
	event->mark_trusted();
	layout->get_dom().dispatch_event(p_element, event);
}

void HTMLLabel::dispatch_media_event(const StringName& p_type, const Ref<HTMLElement>& p_element) {
	if (layout.is_null() || p_element.is_null() || !p_element->is_valid()) return;
	layout->get_dom().dispatch_event(p_element, dockit::make_dom_event(p_type, false, false));
}

void HTMLLabel::dispatch_media_error_event(const Ref<HTMLElement>& p_element, const String& p_message, Error p_error) {
	if (layout.is_null() || p_element.is_null() || !p_element->is_valid()) return;
	Ref<HTMLErrorEvent> event;
	event.instantiate();
	event->set_type(StringName("error"));
	event->set_bubbles(false);
	event->set_composed(false);
	event->set_message(p_message);
	event->set_error(p_error);
	event->mark_trusted();
	layout->get_dom().dispatch_event(p_element, event);
}

void HTMLLabel::dispatch_scroll_event(const Ref<HTMLElement>& p_element) {
	if (layout.is_null() || p_element.is_null() || !p_element->is_valid()) return;
	layout->get_dom().queue_scroll_event(p_element);
}

Ref<HTMLDocumentNode> HTMLLabel::get_document_node() const {
	if (layout.is_null()) return {};
	return layout->get_dom().get_document_node();
}
