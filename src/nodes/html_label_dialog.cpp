#include "html_label.h"

#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

using namespace godot;

void HTMLLabel::remember_dialog_focus(const Ref<HTMLDialogElement>& p_dialog, bool p_modal) {
	// Keep an earlier restore point when a dialog is closed and reopened before the next layout.
	if (auto it = std::find_if(dialog_focus.begin(), dialog_focus.end(),
		[&](const DialogFocus& entry) { return entry.dialog == p_dialog; }); it != dialog_focus.end()) {
		it->modal = p_modal;
		it->pending = true;
		return;
	}
	Control* native = is_inside_tree() ? get_viewport()->gui_get_focus_owner() : nullptr;
	const Ref<HTMLElement> focused = get_focused_element();
	dialog_focus.push_back({ p_dialog, focused, layout->top_modal(),
		native && native != this ? ObjectID(native->get_instance_id()) : ObjectID(),
		p_modal, focused.is_valid() && focused->matches(":focus-visible") });
}

void HTMLLabel::on_dialog_opened() {
	if (!canvas) return;
	close_select_menu();
	canvas->close_input_menu();
	canvas->cancel_press();
	canvas->tab_elements.clear();
	canvas->update_focus_mode();
	cancel_viewport_scroll();
	cancel_viewport_pan_snap();
	layout->cancel_scroll_activity();
	viewport_touch_index = -1;
	viewport_touch_dragged = false;
	dragging_viewport_scrollbar = {};
	set_viewport_bar_hover(-1);
	layout->update_hover(std::nullopt);
	clear_selection();
}

void HTMLLabel::sync_dialog_focus(bool p_has_viewport) {
	const auto top = layout->top_modal();
	const bool top_changed = focused_modal != top;
	std::optional<DialogFocus> restore;
	for (auto it = dialog_focus.begin(); it != dialog_focus.end();) {
		if (it->dialog->is_open() && it->dialog->is_connected() && (!it->modal || it->dialog->is_modal())) {
			++it;
			continue;
		}
		// A covered dialog may close while another dialog is using it as its restore point.
		for (auto successor = std::next(it); successor != dialog_focus.end(); ++successor) {
			if (successor->previous_modal == it->dialog ||
				(successor->previous.is_valid() && it->dialog->contains(successor->previous))) {
				successor->previous = it->previous;
				successor->previous_modal = it->previous_modal;
				successor->previous_control = it->previous_control;
				successor->keyboard = it->keyboard;
			}
		}
		if (it->dialog == focused_modal ||
			(!it->modal && top.is_null() && it->dialog->contains(canvas->focused_element))) restore = *it;
		it = dialog_focus.erase(it);
	}
	focused_modal = top;
	if (!is_visible_in_tree()) return;
	const auto focus_dialog = [&](const Ref<HTMLDialogElement>& p_dialog) {
		grab_focus();
		if (has_focus()) canvas->set_focused_element(p_has_viewport ? layout->dialog_focus_target(p_dialog) : Ref<HTMLElement>(), true);
		};
	if (top.is_valid()) {
		bool pending = false;
		for (auto& entry : dialog_focus) {
			if (entry.dialog == top) pending = std::exchange(entry.pending, false);
		}
		if (restore && p_has_viewport && layout->is_focusable(restore->previous) && has_focus()) {
			canvas->set_focused_element(restore->previous, restore->keyboard, false);
		}
		else if (pending || top_changed || restore || (has_focus() && !layout->is_focusable(canvas->focused_element))) {
			focus_dialog(top);
		}
		return;
	}
	Ref<HTMLDialogElement> opened;
	if (p_has_viewport) {
		for (auto& entry : dialog_focus) {
			if (std::exchange(entry.pending, false)) opened = entry.dialog;
		}
	}
	if (opened.is_valid()) {
		focus_dialog(opened);
	}
	else if (restore && has_focus()) {
		if (p_has_viewport && layout->is_focusable(restore->previous)) {
			canvas->set_focused_element(restore->previous, restore->keyboard, false);
		}
		else {
			canvas->set_focused_element({});
			if (auto* previous = Object::cast_to<Control>(ObjectDB::get_instance(restore->previous_control));
				previous && previous->is_visible_in_tree() && previous->get_viewport() == get_viewport() &&
				previous->get_focus_mode() != FOCUS_NONE) previous->grab_focus();
		}
	}
}

bool HTMLLabel::handle_dialog_escape(const InputEventKey& p_key) {
	const Key key = p_key.get_keycode() != KEY_NONE ? p_key.get_keycode() : p_key.get_physical_keycode();
	if (key != KEY_ESCAPE) return false;
	if (dialog_escape_held) {
		if (!p_key.is_pressed()) dialog_escape_held = false;
		accept_event();
		return true;
	}
	const auto dialog = layout->active_dialog();
	if (dialog.is_null() || !p_key.is_pressed()) return false;
	const bool allowed = dialog->get_closed_by() != "none";
	if (!allowed && !dialog->is_modal()) return false;
	accept_event();
	if (!p_key.is_echo()) {
		dialog_escape_held = true;
		if (canvas) canvas->update_focus_mode();
		if (allowed) dialog->dispatch_close_request(HTMLDialogCloseRequest::ESCAPE);
	}
	return true;
}

void HTMLLabel::request_dialog_from_pointer(const Ref<HTMLDialogElement>& p_dialog, uint64_t p_sequence) {
	if (is_visible_in_tree() && layout->active_dialog() == p_dialog && p_dialog->get_open_sequence() == p_sequence)
		p_dialog->dispatch_close_request(HTMLDialogCloseRequest::BACKDROP);
}

void HTMLLabel::queue_dialog_closed(const Ref<HTMLDialogElement>& p_dialog, const String& p_value) {
	if (!is_inside_tree()) {
		callable_mp(p_dialog.ptr(), &HTMLDialogElement::emit_closed).call_deferred(p_value);
		return;
	}
	closed_dialogs.emplace_back(p_dialog, p_value);
	queue_interaction();
}

void HTMLLabel::emit_dialog_closures() {
	for (const auto& [dialog, value] : std::exchange(closed_dialogs, {})) dialog->emit_closed(value);
}
