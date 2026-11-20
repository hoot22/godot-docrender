#include "html_label.h"

#include "editor/dockit_source_editor.h"
#include "formats/html/dom/html_document_fragment.h"
#include "formats/html/dom/html_dom.h"
#include "formats/html/events/html_event.h"
#include "formats/html/html_behavior.h"
#include "formats/html/elements/html_anchor_element.h"
#include "formats/html/elements/html_details_element.h"
#include "formats/html/elements/html_image_element.h"
#include "formats/html/elements/html_input_element.h"
#include "formats/html/elements/html_input_node.h"
#include "formats/html/elements/html_textarea_element.h"
#include "formats/html/css/css_values.h"
#include "formats/html/html_ui_translations.h"
#include "formats/http_requests.h"

#include "formats/html/markup/html_markup.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_pan_gesture.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

using namespace godot;

namespace {
	constexpr int DRAG_DEADZONE = 6;
	constexpr int HOVER_PASSES = 2;
	constexpr const char* DOCUMENT_DEFAULTS = ":root::selection { color: HighlightText; background-color: Highlight; }";

	std::optional<bool> tab_direction(const InputEventKey& p_key) {
		if (p_key.is_action_pressed("ui_focus_prev", true, true)) return true;
		if (p_key.is_action_pressed("ui_focus_next", true, true)) return false;
		return std::nullopt;
	}

	std::optional<dockit::HTMLScrollOptions> scroll_options(const Dictionary& p_values) {
		using Options = dockit::HTMLScrollOptions;
		using Alignment = Options::Alignment;
		const auto align = [](const String& p_text) -> std::optional<Alignment> {
			if (p_text == "start") return Alignment::START;
			if (p_text == "center") return Alignment::CENTER;
			if (p_text == "end") return Alignment::END;
			if (p_text == "nearest") return Alignment::NEAREST;
			return {};
			};
		const auto block = align(p_values.get("block", "start"));
		const auto inline_axis = align(p_values.get("inline", "nearest"));
		const String behavior = p_values.get("behavior", "auto");
		if (!block || !inline_axis || (behavior != "auto" && behavior != "instant" && behavior != "smooth")) return {};
		return Options{ *block, *inline_axis, behavior == "auto" ? Options::Behavior::AUTO :
			(behavior == "smooth" ? Options::Behavior::SMOOTH : Options::Behavior::INSTANT) };
	}

	std::optional<Control::CursorShape> get_css_cursor(std::string_view p_cursor) {
		static constexpr std::pair<std::string_view, Control::CursorShape> shapes[] = {
			{ "default", Control::CURSOR_ARROW },
			{ "pointer", Control::CURSOR_POINTING_HAND },
			{ "text", Control::CURSOR_IBEAM },
			{ "crosshair", Control::CURSOR_CROSS },
			{ "move", Control::CURSOR_MOVE },
			{ "wait", Control::CURSOR_WAIT },
			{ "progress", Control::CURSOR_BUSY },
			{ "help", Control::CURSOR_HELP },
			{ "not-allowed", Control::CURSOR_FORBIDDEN },
			{ "ew-resize", Control::CURSOR_HSIZE },
			{ "ns-resize", Control::CURSOR_VSIZE },
		};
		for (const auto& [keyword, shape] : shapes) {
			if (std::equal(p_cursor.begin(), p_cursor.end(), keyword.begin(), keyword.end(), [](char p_char, char p_lowercase) {
				return p_char == p_lowercase || (p_char >= 'A' && p_char <= 'Z' && p_char - 'A' + 'a' == p_lowercase);
				})) {
				return shape;
			}
		}
		return std::nullopt;
	}
}

void HTMLLabelCanvas::_draw() {
	if (!label) {
		return;
	}
	const Rect2 visible_rect = label->get_visible_rect();
	if (visible_rect.has_area()) {
		const Vector2i selection = label->get_selection_range();
		const Color color = dockit::html_system_color(litehtml::system_color::highlight, label->page_color_scheme);
		label->layout->draw_with_highlights(this, Vector2(), visible_rect, selection, nullptr, color, {}, nullptr, visible_rect.position);
		update_ime();
	}
}

bool HTMLLabelCanvas::_has_point(const Vector2& p_point) const {
	return label && label->get_visible_rect().has_point(p_point);
}

void HTMLLabelCanvas::show_diagnostic_tooltip() {
	if (diagnostic_panel_text.is_empty() || !label->is_visible_in_tree()) return;
	const Vector2 viewport_size = get_viewport_rect().size;
	const int width = std::max(1, int(std::min<real_t>(380.0, viewport_size.x * 0.8)));
	const int max_height = std::max(1, int(std::min<real_t>(320.0, viewport_size.y * 0.6)));
	const String css = R"CSS(
		body { margin: 0; padding: 8px 12px; background: #23272e; color: #f5f5f5; font-size: 13px; }
		.element { font-weight: bold; padding-bottom: 6px; }
		.diagnostic { border-top: 1px solid #51565f; padding: 6px 0; }
		.kind { color: #ffba65; }
		.source { color: #b8bdc7; font-size: 12px; }
	)CSS";
	const String html = "<!doctype html>" + diagnostic_panel_text;
	// Use the same document layout to size the diagnostic overlay.
	Ref<HTMLLayout> measure;
	measure.instantiate();
	if (measure->parse(html, "res://", css) != OK || measure->layout(Vector2i(width, max_height)) != OK) {
		return;
	}
	if (!diagnostic_panel) {
		diagnostic_panel = memnew(HTMLLabel);
		diagnostic_panel->set_name("CSSDiagnostics");
		diagnostic_panel->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		diagnostic_panel->set_selection_enabled(false);
		diagnostic_panel->set_mouse_behavior_recursive(MOUSE_BEHAVIOR_DISABLED);
		diagnostic_panel->set_z_index(2);
		label->add_child(diagnostic_panel, false, INTERNAL_MODE_BACK);
	}
	diagnostic_panel->set_stylesheet(css);
	diagnostic_panel->set_text(html);
	diagnostic_panel->set_size(Vector2(width, std::clamp(measure->get_content_size().y, 1, max_height)));
	label->place_overlay(*diagnostic_panel, diagnostic_location + Vector2(18, 24));
	diagnostic_panel->show();
}

void HTMLLabelCanvas::refresh_diagnostic_tooltip(const std::optional<Vector2>& p_point) {
	const bool point_supplied = p_point.has_value();
	const Vector2 point = p_point.value_or(Vector2());
	const bool has_diagnostic_point = point_supplied && !selecting && input_menu_target.is_null() && _has_point(point);
	const String content = has_diagnostic_point ? label->layout->get_css_diagnostic_tooltip(point) : String();
	if (content == diagnostic_panel_text) return;
	diagnostic_panel_text = content;
	if (diagnostic_panel) diagnostic_panel->hide();
	if (diagnostic_timer) diagnostic_timer->stop();
	if (content.is_empty() || !point_supplied) return;
	diagnostic_location = label->get_global_transform_with_canvas().affine_inverse().xform(
		get_global_transform_with_canvas().xform(point));
	if (!diagnostic_timer) {
		diagnostic_timer = memnew(Timer);
		diagnostic_timer->set_one_shot(true);
		diagnostic_timer->set_wait_time(0.5);
		diagnostic_timer->connect("timeout", callable_mp(this, &HTMLLabelCanvas::show_diagnostic_tooltip));
		add_child(diagnostic_timer, false, INTERNAL_MODE_BACK);
	}
	diagnostic_timer->start();
}

void HTMLLabelCanvas::_gui_input(const Ref<InputEvent>& p_event) {
	if (!label || p_event.is_null()) {
		return;
	}
	const Rect2 visible_rect = label->get_visible_rect();
	// Touch-generated mouse events use the same path as a mouse click.
	if (p_event->get_device() != InputEvent::DEVICE_ID_EMULATION && !Input::get_singleton()->is_emulating_mouse_from_touch()) {
		if (const Ref<InputEventScreenTouch> touch = p_event; touch.is_valid()) {
			const bool dialog_handled = handle_dialog_pointer(touch->get_position(), touch->is_pressed(),
				touch->is_canceled(), touch->get_device(), touch->get_index());
			if (!dialog_handled) {
				if (touch->is_pressed()) handle_touch_press(*touch.ptr());
				else handle_touch_release(*touch.ptr());
			}
			return;
		}
		if (const Ref<InputEventScreenDrag> drag = p_event; drag.is_valid()) {
			update_dialog_pointer(drag->get_position(), drag->get_device(), drag->get_index());
			handle_touch_drag(*drag.ptr());
			return;
		}
	}
	if (label->viewport_touch_dragged) return;
	const Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid()) {
		PointerInfo pointer;
		pointer.position = motion->get_position();
		pointer.buttons = int(motion->get_button_mask());
		pointer.shift = motion->is_shift_pressed();
		pointer.ctrl = motion->is_ctrl_pressed();
		pointer.alt = motion->is_alt_pressed();
		pointer.meta = motion->is_meta_pressed();
		pointer.device = motion->get_device();
		pointer.source_event = p_event;
		label->input_mouse_position_known = true;
		label->input_mouse_view_position.reset();
		if (visible_rect.has_point(pointer.position))
			label->input_mouse_view_position = pointer.position - visible_rect.position;
		handle_pointer_move(pointer);
		return;
	}

	const Ref<InputEventPanGesture> pan = p_event;
	const Ref<InputEventMouseButton> button = p_event;
	const auto scroll = [&](const Vector2& position) {
		if (!visible_rect.has_point(position)) return;
		if (button.is_valid() && button->is_pressed() &&
			(button->get_button_index() == MOUSE_BUTTON_WHEEL_UP || button->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN ||
				button->get_button_index() == MOUSE_BUTTON_WHEEL_LEFT || button->get_button_index() == MOUSE_BUTTON_WHEEL_RIGHT)) {
			HTMLLabelCanvas::PointerInfo pointer;
			pointer.position = position;
			pointer.shift = button->is_shift_pressed();
			pointer.ctrl = button->is_ctrl_pressed();
			pointer.alt = button->is_alt_pressed();
			pointer.meta = button->is_meta_pressed();
			pointer.source_event = p_event;
			if (!handle_wheel_input(pointer, 36.0f)) {
				accept_event();
				return;
			}
		}
		const auto result = label->layout->scroll_overflow_from_input(position, p_event, *label,
			callable_mp(label, &HTMLLabel::on_nested_scroll_advanced));
		if (result == dockit::HTMLScrollInput::ignored) {
			if (label->layout->top_modal().is_valid()) accept_event();
			return;
		}
		cancel_click();
		if (selecting && selection_dragged) extend_document_selection(position);
		label->request_hover_check();
		if (result == dockit::HTMLScrollInput::handled || label->layout->top_modal().is_valid()) accept_event();
		};
	if (pan.is_valid()) {
		outside_dialog_press.reset();
		scroll(pan->get_position());
		return;
	}
	if (button.is_null()) return;
	const Vector2 point = button->get_position();
	const int button_index = int(button->get_button_index());
	const bool wheel_button = button_index == MOUSE_BUTTON_WHEEL_UP || button_index == MOUSE_BUTTON_WHEEL_DOWN ||
		button_index == MOUSE_BUTTON_WHEEL_LEFT || button_index == MOUSE_BUTTON_WHEEL_RIGHT;
	PointerInfo pointer;
	pointer.position = point;
	pointer.button = button_index;
	pointer.buttons = int(button->get_button_mask());
	pointer.pressed = button->is_pressed();
	pointer.canceled = button->is_canceled();
	pointer.shift = button->is_shift_pressed();
	pointer.ctrl = button->is_ctrl_pressed();
	pointer.alt = button->is_alt_pressed();
	pointer.meta = button->is_meta_pressed();
	pointer.device = button->get_device();
	pointer.source_event = p_event;
	label->input_mouse_position_known = true;
	label->input_mouse_view_position.reset();
	if (visible_rect.has_point(point)) label->input_mouse_view_position = point - visible_rect.position;
	label->request_hover_check();
	if (button->get_button_index() == MOUSE_BUTTON_LEFT) {
		if (handle_dialog_pointer(point, button->is_pressed(), button->is_canceled(), button->get_device())) return;
	}
	else if (button->is_pressed()) {
		outside_dialog_press.reset();
	}
	if (!wheel_button) {
		const int previous_buttons = mouse_buttons;
		const int button_mask = button_index > 0 && button_index < 32 ? 1 << (button_index - 1) : 0;
		if (button->is_pressed()) mouse_buttons |= button_mask;
		else mouse_buttons &= ~button_mask;
		pointer.buttons = mouse_buttons;
		const Ref<HTMLElement> event_target = label->layout->get_element_at(point);
		const uint64_t event_adoption = event_target.is_valid() ? event_target->adoption_version : 0;
		pointer.target = event_target;
		if (button_index == MOUSE_BUTTON_LEFT)
			pointer.detail = click_detail_for(event_target, point - visible_rect.position);
		if (previous_buttons == 0 && mouse_buttons != 0) {
			mouse_compatibility_suppressed = false;
			if (!dispatch_pointer_event(StringName("pointerdown"), event_target, pointer, true)) {
				mouse_compatibility_suppressed = true;
				accept_event();
				return;
			}
		}
		if (!button->is_pressed() && previous_buttons != 0 && mouse_buttons == 0)
			dispatch_pointer_event(StringName("pointerup"), event_target, pointer, false);
		if (mouse_compatibility_suppressed) {
			if (!button->is_pressed() && mouse_buttons == 0) mouse_compatibility_suppressed = false;
			accept_event();
			return;
		}
		if (button->is_pressed()) pointer.mouse_down_prevented =
			!dispatch_pointer_event(StringName("mousedown"), event_target, pointer, true);
		else dispatch_pointer_event(StringName("mouseup"), event_target, pointer, false);
		if (button->is_pressed() && event_target.is_valid() &&
			(event_target->adoption_version != event_adoption || !label->layout->is_interaction_target_available(event_target, false))) {
			cancel_press();
			accept_event();
			return;
		}
		if (button->get_button_index() == MOUSE_BUTTON_LEFT) {
			if (pointer.pressed) handle_pointer_press(pointer);
			else handle_pointer_release(pointer);
			return;
		}
	}
	if (handle_video_button(*button.ptr(), point) || handle_audio_button(*button.ptr(), point)) {
		accept_event();
		return;
	}
	if (visible_rect.has_point(point) && label->handle_listbox_mouse(label->layout->get_element_at(point), *button.ptr(), point)) {
		cancel_press();
		accept_event();
		return;
	}
	if (button->get_button_index() == MOUSE_BUTTON_RIGHT && button->is_pressed() && visible_rect.has_point(point) &&
		!dispatch_pointer_event(StringName("contextmenu"), label->layout->get_element_at(point), *button.ptr(), point, true)) {
		// 监听器接管了右键菜单。
		accept_event();
		return;
	}
	if (button->get_button_index() == MOUSE_BUTTON_RIGHT && button->is_pressed() && label->context_menu_enabled &&
		visible_rect.has_point(point) && open_input_menu(label->layout->get_element_at(point), point, true)) {
		accept_event();
		return;
	}
	scroll(point);
}

bool HTMLLabelCanvas::handle_dialog_pointer(const Vector2& p_point, bool p_pressed, bool p_cancelled, int p_device, int p_touch) {
	if (outside_dialog_press && (outside_dialog_press->device != p_device || outside_dialog_press->touch != p_touch)) return false;
	const auto press = std::exchange(outside_dialog_press, std::nullopt);
	const auto dialog = label->layout->active_dialog();
	if (p_cancelled || dialog.is_null() || !_has_point(p_point)) return false;
	if (dialog->get_closed_by() != "any") return false;
	const bool modal = dialog->is_modal();
	const bool outside = label->layout->is_outside_dialog(*dialog.ptr(), p_point);
	const Vector2 position = p_point - label->get_visible_rect().position;
	if (p_pressed) {
		if (outside && modal) {
			cancel_press();
			accept_event();
		}
		if (outside)
			outside_dialog_press = OutsideDialogPress{ dialog, dialog->get_open_sequence(), position, p_device, p_touch };
		return outside && modal;
	}
	if (!press || press->dialog != dialog || press->open_sequence != dialog->get_open_sequence() || !outside ||
		press->position.distance_squared_to(position) > DRAG_DEADZONE * DRAG_DEADZONE) return false;
	if (modal) {
		accept_event();
		label->request_dialog_from_pointer(dialog, press->open_sequence);
	}
	else {
		// Let an ordinary click still activate its target outside the non-modal dialog.
		callable_mp(label, &HTMLLabel::request_dialog_from_pointer).call_deferred(dialog, press->open_sequence);
	}
	return modal;
}

void HTMLLabelCanvas::update_dialog_pointer(const Vector2& p_point, int p_device, int p_touch) {
	if (!outside_dialog_press || outside_dialog_press->device != p_device || outside_dialog_press->touch != p_touch) return;
	const Vector2 position = p_point - label->get_visible_rect().position;
	if (!_has_point(p_point) || outside_dialog_press->position.distance_squared_to(position) > DRAG_DEADZONE * DRAG_DEADZONE)
		outside_dialog_press.reset();
}

void HTMLLabelCanvas::extend_document_selection(const Vector2& p_point) {
	const Vector2i selection = dockit::html_selection_between(selection_origin, label->layout->selection_point_at(p_point));
	selection_anchor = selection.x;
	selection_caret = selection.y;
	label->capture_node_selection();
}

void HTMLLabel::_input(const Ref<InputEvent>& p_event) {
	if (is_canvas_picker_open()) {
		const ObjectID host_id(get_instance_id()), viewport_id(get_viewport()->get_instance_id());
		const bool handled = is_time_picker_open() ? handle_clock_picker_input(p_event) : handle_color_picker_input(p_event);
		if (handled) {
			if (auto* viewport = Object::cast_to<Viewport>(ObjectDB::get_instance(viewport_id))) viewport->set_input_as_handled();
			return;
		}
		if (!ObjectDB::get_instance(host_id)) return;
	}
	const Ref<InputEventKey> key = p_event;
	if (dialog_escape_held && key.is_valid() &&
		(key->get_keycode() == KEY_ESCAPE || (key->get_keycode() == KEY_NONE && key->get_physical_keycode() == KEY_ESCAPE))) {
		if (!key->is_pressed()) {
			dialog_escape_held = false;
			if (canvas) canvas->update_focus_mode();
		}
		get_viewport()->set_input_as_handled();
		return;
	}
	const Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid() && viewport_control) {
		const Vector2 point = viewport_control->get_global_transform_with_canvas().affine_inverse().xform(motion->get_position());
		if (hovered_viewport_bar >= 0 || viewport_scrollbar(0).track.has_point(point) || viewport_scrollbar(1).track.has_point(point)) {
			queue_interaction();
		}
	}
	if (canvas && canvas->input_menu_target.is_valid()) {
		const Ref<InputEventMouseButton> button = p_event;
		if (button.is_valid() && button->is_pressed()) {
			const Control& menu = *canvas->input_menu;
			if (!Rect2(Vector2(), menu.get_size()).has_point(menu.get_global_transform_with_canvas().affine_inverse().xform(button->get_position()))) {
				canvas->close_input_menu();
			}
		}
	}
	if (open_select.is_valid()) {
		const Ref<InputEventMouseButton> button = p_event;
		if (button.is_valid() && button->is_pressed() && button->get_button_index() == MOUSE_BUTTON_LEFT &&
			!Rect2(Vector2(), select_menu->get_size()).has_point(select_menu->get_global_transform_with_canvas().affine_inverse().xform(button->get_position()))) {
			const auto point = get_global_transform_with_canvas().affine_inverse().xform(button->get_position());
			if (get_element_at(point) != open_select) close_select_menu();
		}
	}
	if (!canvas || has_focus() || (canvas->tab_elements.empty() && layout->top_modal().is_null()) || !is_visible_in_tree()) {
		return;
	}
	if (key.is_valid()) {
		if (const auto direction = tab_direction(*key.ptr())) {
			// Godot moves focus after _input, and FOCUS_ENTER has no input event.
			canvas->tab_entry_backward = direction;
			callable_mp(canvas, &HTMLLabelCanvas::clear_tab_entry).call_deferred();
		}
	}
}

void HTMLLabel::_gui_input(const Ref<InputEvent>& p_event) {
	const Ref<InputEventKey> key = p_event;
	if (canvas && key.is_valid()) {
		if (canvas->handle_input_menu_key(*key.ptr())) accept_event();
		else if (!canvas->handle_dom_key(*key.ptr())) canvas->handle_selection_key(*key.ptr());
	}
}

void HTMLLabelCanvas::handle_selection_key(const InputEventKey& p_key) {
	if (!label->selection_enabled || !p_key.is_pressed()) {
		return;
	}
	if (p_key.is_action("ui_text_select_all", true)) {
		label->select_all();
		label->accept_event();
	}
	else if (p_key.is_action("ui_copy", true) && label->copy_selection()) {
		label->accept_event();
	}
}

void HTMLLabelCanvas::clear_tab_entry() {
	tab_entry_backward.reset();
}

void HTMLLabelCanvas::focus_tab_endpoint(bool p_backward) {
	if (!label->has_focus() || focused_element.is_valid()) return;
	if (tab_elements.empty()) {
		if (const auto dialog = label->layout->top_modal(); dialog.is_valid())
			set_focused_element(label->layout->dialog_focus_target(dialog), true);
	}
	else {
		set_focused_element(p_backward ? tab_elements.back() : tab_elements.front(), true);
		set_audio_focus_edge(p_backward);
		set_video_focus_edge(p_backward);
	}
}

bool HTMLLabelCanvas::handle_dom_key(const InputEventKey& p_key) {
	// 输入法组合期间由编辑器接管按键，不派发 DOM 键盘事件
	const auto composing_control = label->layout->text_control(focused_element);
	const bool composing = composing_control && composing_control->editor.has_preedit();
	if (!composing) {
		const bool prevented = dispatch_key_event(p_key.is_pressed() ? StringName("keydown") : StringName("keyup"), p_key);
		if (prevented) {
			label->accept_event();
			return true;
		}
	}
	return handle_key_default_action(p_key);
}

void HTMLLabelCanvas::set_focused_element(const Ref<HTMLElement>& p_element, bool p_keyboard, bool p_reveal) {
	if (focused_element != p_element) {
		if (!label->layout->prepare_content_focus(p_element)) return;
		const uint64_t transition = ++focus_transition;
		const uint64_t adoption = p_element.is_valid() ? p_element->adoption_version : 0;
		close_input_menu();
		label->close_select_menu();
		const Ref<HTMLElement> previous = focused_element;
		stop_caret_blink();
		close_ime();
		commit_input(previous, true);
		if (focus_transition != transition) return;
		dragging_input.unref();
		cancel_key_press();
		set_element_pseudo(previous, "focus", false);
		set_element_pseudo(previous, "focus-visible", false);
		// blur/focusout 先于焦点转移派发：派发期间 get_focused_element() 为空。
		focused_element = Ref<HTMLElement>();
		dispatch_blur_events(previous, p_element);
		if (focus_transition != transition) return;
		if (p_element.is_valid() && (p_element->adoption_version != adoption ||
			!label->layout->is_interaction_target_available(p_element, false))) return;
		focused_element = p_element;
		remember_interaction_targets();
		const auto input = label->layout->text_control(focused_element);
		focused_reset_version = input ? input->editor.get_reset_version() : 0;
		set_element_pseudo(focused_element, "focus", true);
		// focus/focusin 在焦点已经落位之后派发。
		dispatch_focus_events(previous, p_element);
		if (focus_transition != transition || focused_element != p_element) return;
		restart_caret_blink();
		label->layout->repaint_input();
		update_ime();
	}
	set_element_pseudo(focused_element, "focus-visible", p_keyboard);
	label->reveal_focused_element = focused_element.is_valid() && p_keyboard && p_reveal;
	if (label->reveal_focused_element) {
		label->queue_interaction();
	}
}

void HTMLLabelCanvas::set_element_pseudo(const Ref<HTMLElement>& p_button, const char* p_name, bool p_enabled) {
	if (label->layout->set_interaction_pseudo(p_button, p_name, p_enabled)) {
		label->queue_interaction_style_refresh();
	}
}

void HTMLLabelCanvas::update_focus_mode() {
	const bool tab_reachable = !tab_elements.empty() || label->layout->top_modal().is_valid();
	label->set_focus_mode(tab_reachable ? FOCUS_ALL :
		(label->selection_enabled || has_focusable_elements ? FOCUS_CLICK : FOCUS_NONE));
	label->set_process_input(tab_reachable || label->dialog_escape_held || label->is_canvas_picker_open());
}

void HTMLLabelCanvas::_notification(int p_what) {
	if (!label) {
		return;
	}
	if (p_what == NOTIFICATION_DRAG_END) {
		if (native_drag_started) {
			Ref<HTMLDataTransfer> transfer = dockit::get_current_drag_transfer();
			if (transfer.is_null()) {
				transfer.instantiate();
				transfer->set_text(drag_text);
			}
			transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
			if (drag_source.is_valid()) {
				dispatch_drag_event(StringName("dragend"), drag_source, transfer, get_local_mouse_position());
			}
			dockit::set_current_drag_transfer({});
			suppress_next_release = true;
		}
		native_drag_started = false;
		selection_drag_attempt = false;
		element_drag_attempt = false;
		drag_source.unref();
		drag_text = String();
		drag_elapsed = 0.0;
		set_process(false);
		clear_drop_target(get_local_mouse_position());
	}
	else if (p_what == NOTIFICATION_MOUSE_ENTER || p_what == NOTIFICATION_MOUSE_EXIT) {
		if (p_what == NOTIFICATION_MOUSE_EXIT) {
			cancel_click();
		}
		if (!label->updating_gui_hover) {
			label->request_hover_check();
		}
	}
	else if (p_what == NOTIFICATION_EXIT_TREE) {
		cancel_press();
		cancel_all_touch_pointers();
		set_focused_element({});
		caret_window_active = true;
		tab_elements.clear();
		has_focusable_elements = false;
		clear_tab_entry();
		set_default_cursor_shape(CURSOR_ARROW);
	}
	else if (p_what == NOTIFICATION_INTERNAL_PROCESS && dragging_input.is_valid()) {
		update_input_autoscroll();
	}
	else if (p_what == NOTIFICATION_INTERNAL_PROCESS && selecting && selection_dragged) {
		const Rect2 visible = label->get_visible_rect();
		const Vector2 point = get_local_mouse_position();
		const Vector2 clamped = point.clamp(visible.position, visible.get_end());
		const Vector2 outside = point - clamped;
		if (outside != Vector2() && !label->layout->top_modal_node()) {
			const Vector2i offset = label->get_scroll_offset();
			const Vector2 step = outside * std::min(1.0, get_process_delta_time() * 12.0);
			label->apply_scroll_offset(offset + Vector2i(step.round()) * label->viewport_input_axes);
			const Vector2i scrolled = label->get_scroll_offset() - offset;
			if (scrolled != Vector2i()) {
				extend_document_selection(clamped + Vector2(scrolled));
				queue_redraw();
			}
		}
	}
}

void HTMLLabelCanvas::cancel_press() {
	cancel_video_press();
	pressed_audio.unref();
	pressed_audio_part = dockit::HTMLAudioPart::none;
	if (label && (box_drag || scrollbar_drag)) label->layout->set_scrollbar_pressed(std::nullopt);
	box_drag.reset();
	scrollbar_drag.reset();
	resize_pending.reset();
	dragged_resize = false;
	dragging_input.unref();
	dragging_range.unref();
	cancel_click();
	cancel_key_press();
	selecting = false;
	selection_dragged = false;
	selection_origin_ranges = {};
	if (!native_drag_started) {
		selection_drag_attempt = false;
		element_drag_attempt = false;
		drag_source.unref();
	}
	set_process_internal(false);
}

void HTMLLabelCanvas::cancel_click() {
	outside_dialog_press.reset();
	pressed_blank = false;
	if (pressed_checkbox.is_valid()) {
		set_element_pseudo(pressed_checkbox, "active", false);
		pressed_checkbox.unref();
	}
	pressed_link = 0;
	pressed_element.unref();
	pressed_form_target.unref();
	cancel_button_press();
}

void HTMLLabelCanvas::cancel_button_press() {
	if (pressed_button.is_valid()) {
		set_element_pseudo(pressed_button, "active", false);
		pressed_button.unref();
	}
}

void HTMLLabelCanvas::cancel_key_press() {
	if (std::exchange(checkbox_key_pressed, false)) set_element_pseudo(focused_element, "active", false);
	if (activation_key != KEY_NONE) {
		activation_key = KEY_NONE;
		set_element_pseudo(focused_element, "active", false);
	}
}

void HTMLLabelCanvas::update_cursor(const std::optional<Vector2>& p_point) {
	if (!p_point || scrollbar_drag) {
		set_default_cursor_shape(CURSOR_ARROW);
		return;
	}
	if (const auto resize = box_resize_cursor(*p_point)) {
		set_default_cursor_shape(*resize);
		return;
	}
	if (label->layout->scrollbar_at(*p_point)) {
		set_default_cursor_shape(CURSOR_ARROW);
		return;
	}
	if (selection_dragged || dragging_input.is_valid()) {
		set_default_cursor_shape(CURSOR_IBEAM);
		return;
	}
	if (is_video_control_at(*p_point)) {
		set_default_cursor_shape(CURSOR_POINTING_HAND);
		return;
	}
	bool over_link = false;
	bool over_selectable_text = false;
	const auto css_cursor = get_css_cursor(label->layout->get_cursor_at(*p_point, over_link,
		label->selection_enabled ? &over_selectable_text : nullptr));
	set_default_cursor_shape(css_cursor.value_or(over_link ? CURSOR_POINTING_HAND :
		(over_selectable_text ? CURSOR_IBEAM : CURSOR_ARROW)));
}

HTMLLabel::HTMLLabel() {
	layout.instantiate();
	session_storage.instantiate();
	session_storage->configure_session();
	layout->set_event_host_id(ObjectID(get_instance_id()));
	layout->get_dom().set_event_debug(event_debug);
	layout->uses_document_viewport = true;
	layout->uses_owner_animation_clock = true;
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}

void HTMLLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bus", "bus"), &HTMLLabel::set_bus);
	ClassDB::bind_method(D_METHOD("get_bus"), &HTMLLabel::get_bus);
	ClassDB::bind_method(D_METHOD("set_use_fmod_bus", "enabled"), &HTMLLabel::set_use_fmod_bus);
	ClassDB::bind_method(D_METHOD("is_using_fmod_bus"), &HTMLLabel::is_using_fmod_bus);
	ClassDB::bind_method(D_METHOD("set_document", "document"), &HTMLLabel::set_document);
	ClassDB::bind_method(D_METHOD("set_behaviors_enabled", "enabled"), &HTMLLabel::set_behaviors_enabled);
	ClassDB::bind_method(D_METHOD("are_behaviors_enabled"), &HTMLLabel::are_behaviors_enabled);
	ClassDB::bind_method(D_METHOD("set_component_scripts", "scripts"), &HTMLLabel::set_component_scripts);
	ClassDB::bind_method(D_METHOD("get_component_scripts"), &HTMLLabel::get_component_scripts);
	ClassDB::bind_method(D_METHOD("get_component_diagnostics"), &HTMLLabel::get_component_diagnostics);
	ClassDB::bind_method(D_METHOD("reload_components"), &HTMLLabel::reload_components);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "component_scripts", PROPERTY_HINT_DICTIONARY_TYPE, "StringName;GDScript"), "set_component_scripts", "get_component_scripts");
	ClassDB::bind_method(D_METHOD("set_behavior_scripts", "scripts"), &HTMLLabel::set_behavior_scripts);
	ClassDB::bind_method(D_METHOD("get_behavior_scripts"), &HTMLLabel::get_behavior_scripts);
	ClassDB::bind_method(D_METHOD("reload_behaviors"), &HTMLLabel::reload_behaviors);
	ClassDB::bind_method(D_METHOD("get_behavior_diagnostics"), &HTMLLabel::get_behavior_diagnostics);
	ClassDB::bind_method(D_METHOD("get_document"), &HTMLLabel::get_document);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &HTMLLabel::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &HTMLLabel::get_text);
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &HTMLLabel::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &HTMLLabel::get_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_uri"), &HTMLLabel::get_base_uri);
	ClassDB::bind_method(D_METHOD("set_storage_namespace", "namespace"), &HTMLLabel::set_storage_namespace);
	ClassDB::bind_method(D_METHOD("get_storage_namespace"), &HTMLLabel::get_storage_namespace);
	ClassDB::bind_method(D_METHOD("navigate", "url", "replace"), &HTMLLabel::navigate, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("back"), &HTMLLabel::back);
	ClassDB::bind_method(D_METHOD("forward"), &HTMLLabel::forward);
	ClassDB::bind_method(D_METHOD("go", "delta"), &HTMLLabel::go);
	ClassDB::bind_method(D_METHOD("get_location"), &HTMLLabel::get_location);
	ClassDB::bind_method(D_METHOD("set_stylesheet", "stylesheet"), &HTMLLabel::set_stylesheet);
	ClassDB::bind_method(D_METHOD("get_stylesheet"), &HTMLLabel::get_stylesheet);
	ClassDB::bind_method(D_METHOD("get_content_size"), &HTMLLabel::get_content_size);
	ClassDB::bind_method(D_METHOD("get_render_error"), &HTMLLabel::get_render_error);
	ClassDB::bind_method(D_METHOD("set_css_diagnostics_enabled", "enabled"), &HTMLLabel::set_css_diagnostics_enabled);
	ClassDB::bind_method(D_METHOD("is_css_diagnostics_enabled"), &HTMLLabel::is_css_diagnostics_enabled);
	ClassDB::bind_method(D_METHOD("get_css_diagnostics"), &HTMLLabel::get_css_diagnostics);
	ClassDB::bind_method(D_METHOD("set_event_debug", "enabled"), &HTMLLabel::set_event_debug);
	ClassDB::bind_method(D_METHOD("is_event_debug_enabled"), &HTMLLabel::is_event_debug_enabled);
	ClassDB::bind_method(D_METHOD("get_event_listener_count", "type"), &HTMLLabel::get_event_listener_count);
	ClassDB::bind_method(D_METHOD("get_scroll_offset"), &HTMLLabel::get_scroll_offset);
	ClassDB::bind_method(D_METHOD("set_scroll_offset", "offset"), &HTMLLabel::set_scroll_offset);
	ClassDB::bind_method(D_METHOD("scroll_to_anchor", "id", "align"), &HTMLLabel::scroll_to_anchor, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("create_element", "tag_name"), &HTMLLabel::create_element);
	ClassDB::bind_method(D_METHOD("create_document_fragment"), &HTMLLabel::create_document_fragment);
	ClassDB::bind_method(D_METHOD("get_document_node"), &HTMLLabel::get_document_node);
	ClassDB::bind_method(D_METHOD("create_range"), &HTMLLabel::create_range);
	ClassDB::bind_method(D_METHOD("get_selection"), &HTMLLabel::get_selection);
	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &HTMLLabel::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &HTMLLabel::query_selector_all);
	ClassDB::bind_method(D_METHOD("get_element_by_id", "id"), &HTMLLabel::get_element_by_id);
	ClassDB::bind_method(D_METHOD("get_element_at", "position"), &HTMLLabel::get_element_at);
	ClassDB::bind_method(D_METHOD("get_elements_at", "position"), &HTMLLabel::get_elements_at);
	ClassDB::bind_method(D_METHOD("get_focused_element"), &HTMLLabel::get_focused_element);
	ClassDB::bind_method(D_METHOD("focus_element", "element"), &HTMLLabel::focus_element);
	ClassDB::bind_method(D_METHOD("execute_edit_command", "command", "value"), &HTMLLabel::execute_edit_command, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("activate_element", "element"), &HTMLLabel::activate_element);
	ClassDB::bind_method(D_METHOD("submit_form", "form", "submitter"), &HTMLLabel::submit_form, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("scroll_to_element", "element", "align"), &HTMLLabel::scroll_to_element, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("scroll_element_into_view", "element", "options"), &HTMLLabel::scroll_element_into_view, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("scroll_to_text_range", "from", "to"), &HTMLLabel::scroll_to_text_range);
	ClassDB::bind_method(D_METHOD("set_selection_enabled", "enabled"), &HTMLLabel::set_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_selection_enabled"), &HTMLLabel::is_selection_enabled);
	ClassDB::bind_method(D_METHOD("set_drag_and_drop_selection_enabled", "enabled"), &HTMLLabel::set_drag_and_drop_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_drag_and_drop_selection_enabled"), &HTMLLabel::is_drag_and_drop_selection_enabled);
	ClassDB::bind_method(D_METHOD("set_transparent_canvas", "enabled"), &HTMLLabel::set_transparent_canvas);
	ClassDB::bind_method(D_METHOD("is_transparent_canvas"), &HTMLLabel::is_transparent_canvas);
	ClassDB::bind_method(D_METHOD("set_context_menu_enabled", "enabled"), &HTMLLabel::set_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("is_context_menu_enabled"), &HTMLLabel::is_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("set_animation_process_mode", "mode"), &HTMLLabel::set_animation_process_mode);
	ClassDB::bind_method(D_METHOD("get_animation_process_mode"), &HTMLLabel::get_animation_process_mode);
	ClassDB::bind_method(D_METHOD("get_animations", "element"), &HTMLLabel::get_animations, DEFVAL(Ref<HTMLElement>()));
	ClassDB::bind_method(D_METHOD("animate", "element", "keyframes", "options"), &HTMLLabel::animate, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_computed_style", "element", "pseudo"), &HTMLLabel::get_computed_style, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_computed_style_property", "element", "name", "pseudo"), &HTMLLabel::get_computed_style_property, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_stylesheets"), &HTMLLabel::get_stylesheets);
	ClassDB::bind_method(D_METHOD("get_css_rules", "stylesheet"), &HTMLLabel::get_css_rules);
	ClassDB::bind_method(D_METHOD("insert_rule", "stylesheet", "rule", "index"), &HTMLLabel::insert_rule, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("delete_rule", "stylesheet", "index"), &HTMLLabel::delete_rule);
	ClassDB::bind_method(D_METHOD("supports_css", "property_or_condition", "value"), &HTMLLabel::supports_css, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("supports_selector", "selector"), &HTMLLabel::supports_selector);
	ClassDB::bind_method(D_METHOD("matches_media", "query"), &HTMLLabel::matches_media);
	ClassDB::bind_method(D_METHOD("advance_animation", "delta"), &HTMLLabel::advance_animation);
	ClassDB::bind_method(D_METHOD("set_caret_blink_enabled", "enabled"), &HTMLLabel::set_caret_blink_enabled);
	ClassDB::bind_method(D_METHOD("is_caret_blink_enabled"), &HTMLLabel::is_caret_blink_enabled);
	ClassDB::bind_method(D_METHOD("set_caret_blink_interval", "interval"), &HTMLLabel::set_caret_blink_interval);
	ClassDB::bind_method(D_METHOD("get_caret_blink_interval"), &HTMLLabel::get_caret_blink_interval);
	ClassDB::bind_method(D_METHOD("select_all"), &HTMLLabel::select_all);
	ClassDB::bind_method(D_METHOD("clear_selection"), &HTMLLabel::clear_selection);
	ClassDB::bind_method(D_METHOD("get_selected_text"), &HTMLLabel::get_selected_text);
	ClassDB::bind_method(D_METHOD("get_accessible_text"), &HTMLLabel::get_accessible_text);
	ClassDB::bind_method(D_METHOD("get_plain_text"), &HTMLLabel::get_plain_text);
	ClassDB::bind_method(D_METHOD("get_selection_range"), &HTMLLabel::get_selection_range);
	ClassDB::bind_method(D_METHOD("set_selection_range", "from", "to"), &HTMLLabel::set_selection_range);
	ClassDB::bind_method(D_METHOD("get_selection_rects"), &HTMLLabel::get_selection_rects);
	ClassDB::bind_method(D_METHOD("copy_selection"), &HTMLLabel::copy_selection);
	ClassDB::bind_method(D_METHOD("find_text", "text", "from", "case_sensitive"), &HTMLLabel::find_text, DEFVAL(0), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_video_backend", "backend"), &HTMLLabel::set_video_backend);
	ClassDB::bind_method(D_METHOD("get_video_backend"), &HTMLLabel::get_video_backend);
	ClassDB::bind_method(D_METHOD("set_video_bus", "bus"), &HTMLLabel::set_video_bus);
	ClassDB::bind_method(D_METHOD("get_video_bus"), &HTMLLabel::get_video_bus);
	ClassDB::bind_method(D_METHOD("get_fullscreen_video"), &HTMLLabel::get_fullscreen_video);

	BIND_ENUM_CONSTANT(VIDEO_BACKEND_AUTO);
	BIND_ENUM_CONSTANT(VIDEO_BACKEND_FFMPEG);
	BIND_ENUM_CONSTANT(VIDEO_BACKEND_VLC);

	BIND_ENUM_CONSTANT(ANIMATION_PROCESS_IDLE);
	BIND_ENUM_CONSTANT(ANIMATION_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(ANIMATION_PROCESS_MANUAL);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_dir", PROPERTY_HINT_DIR), "set_base_dir", "get_base_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "storage_namespace"), "set_storage_namespace", "get_storage_namespace");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "location", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_location");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "document", PROPERTY_HINT_RESOURCE_TYPE, "HTMLDocument"), "set_document", "get_document");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stylesheet", PROPERTY_HINT_MULTILINE_TEXT), "set_stylesheet", "get_stylesheet");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transparent_canvas"), "set_transparent_canvas", "is_transparent_canvas");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selection_enabled"), "set_selection_enabled", "is_selection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_and_drop_selection_enabled"),
		"set_drag_and_drop_selection_enabled", "is_drag_and_drop_selection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "context_menu_enabled"), "set_context_menu_enabled", "is_context_menu_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "animation_process_mode", PROPERTY_HINT_ENUM, "Idle,Physics,Manual"),
		"set_animation_process_mode", "get_animation_process_mode");

	ADD_GROUP("Video", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "video_backend", PROPERTY_HINT_ENUM, "Auto,FFmpeg,VLC"),
		"set_video_backend", "get_video_backend");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "video_bus", PROPERTY_HINT_ENUM), "set_video_bus", "get_video_bus");

	ADD_GROUP("Audio", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "bus", PROPERTY_HINT_ENUM), "set_bus", "get_bus");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_fmod_bus"), "set_use_fmod_bus", "is_using_fmod_bus");
	ADD_GROUP("Caret", "caret_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caret_blink"), "set_caret_blink_enabled", "is_caret_blink_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "caret_blink_interval", PROPERTY_HINT_RANGE, "0.1,10,0.01,suffix:s"),
		"set_caret_blink_interval", "get_caret_blink_interval");
	
	ADD_GROUP("Behavior", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "behaviors_enabled"), "set_behaviors_enabled", "are_behaviors_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "behavior_scripts", PROPERTY_HINT_DICTIONARY_TYPE, "StringName;GDScript"),
		"set_behavior_scripts", "get_behavior_scripts");
	
	ADD_GROUP("Debug", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "css_diagnostics_enabled"), "set_css_diagnostics_enabled", "is_css_diagnostics_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "event_debug"), "set_event_debug", "is_event_debug_enabled");
	
	ADD_SIGNAL(MethodInfo("video_failed", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE, "HTMLVideoElement"),
		PropertyInfo(Variant::INT, "error"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("video_fullscreen_changed", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE, "HTMLVideoElement"),
		PropertyInfo(Variant::BOOL, "active")));
	ADD_SIGNAL(MethodInfo("link_pressed", PropertyInfo(Variant::STRING, "uri")));
	ADD_SIGNAL(MethodInfo("location_changed", PropertyInfo(Variant::STRING, "location")));
	ADD_SIGNAL(MethodInfo("navigation_failed", PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("element_clicked", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE,
		String(), PROPERTY_USAGE_DEFAULT, HTMLElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("textarea_changed", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE,
		"", PROPERTY_USAGE_DEFAULT, HTMLTextAreaElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("textarea_committed", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE,
		"", PROPERTY_USAGE_DEFAULT, HTMLTextAreaElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("input_changed", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE,
		"", PROPERTY_USAGE_DEFAULT, HTMLInputElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("input_committed", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE,
		"", PROPERTY_USAGE_DEFAULT, HTMLInputElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("button_pressed", PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE,
		String(), PROPERTY_USAGE_DEFAULT, HTMLElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("layout_changed"));
	const PropertyInfo form_info(Variant::OBJECT, "form", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, HTMLFormElement::get_class_static());
	ADD_SIGNAL(MethodInfo("form_submitted", form_info, PropertyInfo(Variant::ARRAY, "entries"),
		PropertyInfo(Variant::OBJECT, "submitter", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, HTMLElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("form_validation_failed", form_info,
		PropertyInfo(Variant::OBJECT, "control", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, HTMLElement::get_class_static())));
	ADD_SIGNAL(MethodInfo("form_reset", form_info));
	ADD_SIGNAL(MethodInfo("select_changed",
		PropertyInfo(Variant::OBJECT, "element", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, HTMLSelectElement::get_class_static())));
}

void HTMLLabel::_validate_property(PropertyInfo& p_property) const {
	validate_audio_property(p_property);
	if (!caret_blink && p_property.name == StringName("caret_blink_interval")) {
		p_property.usage &= ~PROPERTY_USAGE_EDITOR;
	}
}

void HTMLLabel::_draw() {
	if (!transparent_canvas) {
		draw_rect(Rect2(Vector2(), get_size()), dockit::html_system_color(litehtml::system_color::canvas, page_color_scheme));
	}
}

void HTMLLabel::set_transparent_canvas(bool p_enabled) {
	if (transparent_canvas == p_enabled) return;
	transparent_canvas = p_enabled;
	queue_redraw();
}

bool HTMLLabel::is_transparent_canvas() const {
	return transparent_canvas;
}

void HTMLLabel::_notification(int p_what) {
	switch (p_what) {
	case NOTIFICATION_TRANSLATION_CHANGED:
		queue_refresh();
		queue_accessibility_update();
		if (canvas) canvas->close_input_menu();
		if (time_picker_canvas) time_picker_canvas->queue_redraw();
		break;
	case NOTIFICATION_ACCESSIBILITY_UPDATE:
		update_accessibility_tree();
		break;
	case NOTIFICATION_POST_ENTER_TREE:
		behavior_host_in_tree = true;
		update_document_visibility_state(is_visible_in_tree(), true);
		ensure_controls();
		connect_audio_buses(true);
		queue_audio_sync();
		queue_video_sync();
		if (color_scheme_timer && is_visible_in_tree()) color_scheme_timer->start();
		queue_refresh();
		break;
	case NOTIFICATION_RESIZED:
		queue_redraw();
		if (canvas) {
			canvas->cancel_click();
		}
		queue_refresh();
		break;
	case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		queue_refresh();
		break;
	case NOTIFICATION_OS_IME_UPDATE:
		if (canvas) canvas->on_ime_update();
		break;
	case NOTIFICATION_WM_WINDOW_FOCUS_IN:
		if (canvas) {
			canvas->caret_window_active = true;
			canvas->restart_caret_blink();
			canvas->update_ime();
		}
		dispatch_window_focus_event(true);
		break;
	case NOTIFICATION_INTERNAL_PROCESS:
		poll_audio();
		poll_videos(get_process_delta_time());
		if (animation_process_mode == ANIMATION_PROCESS_IDLE && viewport_size.x > 0) {
			layout->advance_css_animations(get_process_delta_time());
			update_internal_processing();
		}
		if (canvas && canvas->ime_window) canvas->update_ime();
		update_behavior_processing();
		break;
	case NOTIFICATION_INTERNAL_PHYSICS_PROCESS:
		if (animation_process_mode == ANIMATION_PROCESS_PHYSICS && viewport_size.x > 0) {
			layout->advance_css_animations(get_physics_process_delta_time());
			update_internal_processing();
		}
		break;
	case NOTIFICATION_FOCUS_ENTER:
		if (canvas && canvas->tab_entry_backward) {
			canvas->focus_tab_endpoint(*canvas->tab_entry_backward);
			canvas->clear_tab_entry();
		}
		else if (canvas && layout->top_modal().is_valid()) canvas->focus_tab_endpoint(false);
		break;
	case NOTIFICATION_FOCUS_EXIT:
		if (is_canvas_picker_open()) close_input_picker();
		if (canvas) {
			const bool was_dragging = canvas->selection_dragged;
			canvas->cancel_press();
			canvas->set_focused_element({});
			if (was_dragging) {
				request_hover_check();
			}
		}
		break;
	case NOTIFICATION_VISIBILITY_CHANGED:
		update_document_visibility_state(is_visible_in_tree(), true);
		queue_accessibility_update();
		if (!is_visible_in_tree()) close_input_picker();
		if (is_visible_in_tree()) {
			if (color_scheme_timer && color_scheme_timer->is_inside_tree()) color_scheme_timer->start();
			poll_preferred_color_scheme();
			if (layout->top_modal_node()) queue_refresh();
			gui_hover_dirty = true;
			request_hover_check();
		}
		else if (canvas) {
			input_mouse_position_known = false;
			input_mouse_view_position.reset();
			leave_video_fullscreen();
			if (color_scheme_timer) color_scheme_timer->stop();
			viewport_touch_index = -1;
			viewport_touch_dragged = false;
			canvas->cancel_all_touch_pointers();
			dragging_viewport_scrollbar = {};
			cancel_viewport_scroll();
			cancel_viewport_pan_snap();
			layout->cancel_scroll_activity();
			canvas->cancel_press();
			canvas->set_focused_element({});
			hover_passes_remaining = 0;
			if (layout->update_hover(std::nullopt) == dockit::HTMLHoverUpdate::layout) {
				queue_refresh();
			}
			queue_interaction();
		}
		break;
	case NOTIFICATION_EXIT_TREE: {
		update_document_visibility_state(false, true);
		pending_window_resize_event_size.reset();
		window_resize_event_queued = false;
		clear_accessibility_tree();
		if (navigation_request_id != 0) {
			dockit::cancel_http_request(navigation_request_id);
			navigation_request_id = 0;
			++navigation_sequence;
		}
		input_mouse_position_known = false;
		input_mouse_view_position.reset();
		close_input_picker();
		const ObjectID id(get_instance_id());
		behavior_host_in_tree = false;
		request_behavior_reset();
		if (!ObjectDB::get_instance(id)) return;
		dialog_escape_held = false;
		if (!closed_dialogs.empty()) callable_mp(this, &HTMLLabel::emit_dialog_closures).call_deferred();
		clear_videos();
		connect_audio_buses(false);
		clear_audio();
		viewport_touch_index = -1;
		viewport_touch_dragged = false;
		set_process_internal(false);
		set_physics_process_internal(false);
		dragging_viewport_scrollbar = {};
		cancel_viewport_scroll();
		cancel_viewport_pan_snap();
		layout->cancel_scroll_activity();
		close_select_menu();
		layout->update_hover(std::nullopt);
		hover_passes_remaining = 0;
		hover_reflow = false;
		view_update_pending = false;
		layout_signal_pending = false;
		reveal_focused_element = false;
		break;
	}
	case NOTIFICATION_WM_WINDOW_FOCUS_OUT:
		if (is_canvas_picker_open()) close_input_picker();
		dialog_escape_held = false;
		viewport_touch_index = -1;
		viewport_touch_dragged = false;
		dragging_viewport_scrollbar = {};
		if (viewport_bars) viewport_bars->queue_redraw();
		close_select_menu();
		if (canvas) {
			canvas->cancel_all_touch_pointers();
			canvas->close_input_menu();
			canvas->cancel_press();
			canvas->caret_window_active = false;
			canvas->stop_caret_blink();
			canvas->close_ime();
		}
		dispatch_window_focus_event(false);
		break;
	case NOTIFICATION_WM_MOUSE_EXIT:
		stop_color_eyedropper();
		break;
	}
}

void HTMLLabel::update_accessibility_tree() {
	auto* display = DisplayServer::get_singleton();
	const RID accessible = get_accessibility_element();
	if (!display || !accessible.is_valid()) return;
	clear_accessibility_tree();
	display->accessibility_update_set_role(accessible, DisplayServer::ROLE_CONTAINER);
	display->accessibility_update_set_value(accessible, String());
	if (!is_visible_in_tree() || !layout.is_valid() || source_dirty || !canvas || viewport_size.x <= 0 || viewport_size.y <= 0) return;

	const Ref<HTMLElement> document_element = layout->get_document_element();
	if (document_element.is_null()) return;
	std::unordered_map<uint64_t, RID> elements_by_id;
	std::vector<std::tuple<RID, String, int32_t>> pending_relations;
	const Transform2D transform = canvas->get_global_transform_with_canvas();

	const auto role_for = [](const Ref<HTMLElement>& p_element, bool& r_heading, bool& r_landmark) {
		using Role = DisplayServer::AccessibilityRole;
		r_heading = false;
		r_landmark = false;
		const String tag = p_element->get_tag_name();
		String role = p_element->get_attribute("role").strip_edges().to_lower();
		if (role == "none" || role == "presentation") return Role::ROLE_UNKNOWN;
		if (role.is_empty()) {
			if (tag == "button" || tag == "summary") return Role::ROLE_BUTTON;
			if (tag == "a" && p_element->has_attribute("href")) return Role::ROLE_LINK;
			if (tag == "input") {
				const String type = p_element->get_attribute("type", "text").to_lower();
				if (type == "hidden") return Role::ROLE_UNKNOWN;
				if (type == "checkbox") return Role::ROLE_CHECK_BOX;
				if (type == "radio") return Role::ROLE_RADIO_BUTTON;
				if (type == "range") return Role::ROLE_SLIDER;
				if (type == "number") return Role::ROLE_SPIN_BUTTON;
				if (type == "button" || type == "submit" || type == "reset" || type == "image") return Role::ROLE_DEFAULT_BUTTON;
				return Role::ROLE_TEXT_FIELD;
			}
			if (tag == "textarea" || p_element->is_content_editable()) return Role::ROLE_MULTILINE_TEXT_FIELD;
			if (tag == "select") return Role::ROLE_LIST_BOX;
			if (tag == "option") return Role::ROLE_LIST_BOX_OPTION;
			if (tag == "img") return p_element->get_attribute("alt").is_empty() ? Role::ROLE_UNKNOWN : Role::ROLE_IMAGE;
			if (tag == "progress" || tag == "meter") return Role::ROLE_PROGRESS_INDICATOR;
			if (tag == "table") return Role::ROLE_TABLE;
			if (tag == "tr") return Role::ROLE_ROW;
			if (tag == "th") return p_element->get_attribute("scope").to_lower() == "row" ? Role::ROLE_ROW_HEADER : Role::ROLE_COLUMN_HEADER;
			if (tag == "td") return Role::ROLE_CELL;
			if (tag == "ul" || tag == "ol" || tag == "menu") return Role::ROLE_LIST;
			if (tag == "li") return Role::ROLE_LIST_ITEM;
			if (tag == "dialog") return Role::ROLE_DIALOG;
			if (tag == "details" || tag == "fieldset" || tag == "optgroup") return Role::ROLE_CONTAINER;
			if (tag == "audio") return Role::ROLE_AUDIO;
			if (tag == "video") return Role::ROLE_VIDEO;
			if (tag == "label") return Role::ROLE_STATIC_TEXT;
			if (tag == "legend" || tag == "output") return Role::ROLE_STATIC_TEXT;
			if (tag.length() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6') {
				r_heading = true;
				return Role::ROLE_STATIC_TEXT;
			}
			if (tag == "main" || tag == "nav" || tag == "header" || tag == "footer" || tag == "aside" ||
				tag == "section" || tag == "form" || tag == "article" || tag == "search") {
				r_landmark = tag == "main" || tag == "nav" || tag == "header" || tag == "footer" || tag == "aside" ||
					tag == "section" || tag == "form" || tag == "search";
				return Role::ROLE_CONTAINER;
			}
			return Role::ROLE_UNKNOWN;
		}
		if (role == "button") return Role::ROLE_BUTTON;
		if (role == "link") return Role::ROLE_LINK;
		if (role == "checkbox") return Role::ROLE_CHECK_BOX;
		if (role == "radio") return Role::ROLE_RADIO_BUTTON;
		if (role == "switch") return Role::ROLE_CHECK_BUTTON;
		if (role == "slider") return Role::ROLE_SLIDER;
		if (role == "spinbutton") return Role::ROLE_SPIN_BUTTON;
		if (role == "progressbar") return Role::ROLE_PROGRESS_INDICATOR;
		if (role == "textbox" || role == "searchbox") return Role::ROLE_TEXT_FIELD;
		if (role == "list") return Role::ROLE_LIST;
		if (role == "listitem") return Role::ROLE_LIST_ITEM;
		if (role == "option") return Role::ROLE_LIST_BOX_OPTION;
		if (role == "listbox" || role == "combobox") return Role::ROLE_LIST_BOX;
		if (role == "table" || role == "grid" || role == "treegrid") return Role::ROLE_TABLE;
		if (role == "row") return Role::ROLE_ROW;
		if (role == "cell" || role == "gridcell") return Role::ROLE_CELL;
		if (role == "columnheader") return Role::ROLE_COLUMN_HEADER;
		if (role == "rowheader") return Role::ROLE_ROW_HEADER;
		if (role == "img" || role == "image") return Role::ROLE_IMAGE;
		if (role == "dialog" || role == "alertdialog") return Role::ROLE_DIALOG;
		if (role == "heading") { r_heading = true; return Role::ROLE_STATIC_TEXT; }
		if (role == "banner" || role == "navigation" || role == "main" || role == "contentinfo" ||
			role == "complementary" || role == "region" || role == "form" || role == "search") {
			r_landmark = true;
			return Role::ROLE_CONTAINER;
		}
		if (role == "group" || role == "article" || role == "status" || role == "alert" || role == "log") return Role::ROLE_CONTAINER;
		if (role == "separator") return Role::ROLE_SPLITTER;
		if (role == "menuitem") return Role::ROLE_MENU_ITEM;
		if (role == "menu") return Role::ROLE_MENU;
		if (role == "menubar") return Role::ROLE_MENU_BAR;
		if (role == "tab") return Role::ROLE_TAB;
		if (role == "tablist") return Role::ROLE_TAB_BAR;
		if (role == "tabpanel") return Role::ROLE_TAB_PANEL;
		return Role::ROLE_UNKNOWN;
	};

	const auto is_hidden = [&](const Ref<HTMLElement>& p_element) {
		for (Ref<HTMLElement> ancestor = p_element; ancestor.is_valid(); ancestor = ancestor->get_parent()) {
			if (ancestor->get_hidden() || ancestor->get_inert() || ancestor->get_attribute("aria-hidden").to_lower() == "true") return true;
			if (layout->get_computed_style_property(ancestor, "display") == "none") return true;
		}
		const String visibility_value = layout->get_computed_style_property(p_element, "visibility");
		return visibility_value == "hidden" || visibility_value == "collapse";
	};

	const auto transformed_bounds = [&](const Rect2& p_rect) {
		const Vector2 points[] = {
			transform.xform(p_rect.position),
			transform.xform(Vector2(p_rect.get_end().x, p_rect.position.y)),
			transform.xform(p_rect.get_end()),
			transform.xform(Vector2(p_rect.position.x, p_rect.get_end().y)),
		};
		Vector2 min = points[0];
		Vector2 max = points[0];
		for (int i = 1; i < 4; ++i) {
			min = min.min(points[i]);
			max = max.max(points[i]);
		}
		return Rect2(min, max - min);
	};

	const auto add_element = [&](auto&& self, const Ref<HTMLElement>& p_element, const RID& p_parent, bool p_text_covered) -> void {
		if (p_element.is_null() || !p_element->is_valid() || is_hidden(p_element)) return;
		bool is_heading = false;
		bool is_landmark = false;
		const DisplayServer::AccessibilityRole role = role_for(p_element, is_heading, is_landmark);
		RID child_parent = p_parent;
		bool text_covered = p_text_covered;
		if (role != DisplayServer::ROLE_UNKNOWN) {
			const RID child = display->accessibility_create_sub_element(p_parent, role);
			if (child.is_valid()) {
				accessibility_tree_elements.push_back(child);
				elements_by_id[p_element->get_instance_id()] = child;
				child_parent = child;
				String name = p_element->get_attribute("aria-label").strip_edges();
				const String labelledby = p_element->get_attribute("aria-labelledby").strip_edges();
				if (!labelledby.is_empty()) {
					String joined;
					for (const String& id : labelledby.split(" ", false)) {
						const Ref<HTMLElement> label_element = layout->get_element_by_id(id);
						if (label_element.is_null()) continue;
						if (!joined.is_empty()) joined += " ";
						joined += label_element->get_text_content().strip_edges();
						pending_relations.emplace_back(child, id, 0);
					}
					if (!joined.is_empty()) name = joined;
				}
			const String tag = p_element->get_tag_name();
			const String type = p_element->get_attribute("type", "text").to_lower();
			if (name.is_empty() && tag == "img") name = p_element->get_attribute("alt");
			if (name.is_empty() && tag == "input" && (type == "button" || type == "submit" || type == "reset" || type == "image")) {
				if (type == "image") {
					name = p_element->has_attribute("alt") ? p_element->get_attribute("alt") :
						dockit::html_ui_translate("Submit", "input.default_action");
				} else if (p_element->has_attribute("value")) {
					name = p_element->get_attribute("value");
				} else if (type == "submit") {
					name = dockit::html_ui_translate("Submit", "input.default_action");
				} else if (type == "reset") {
					name = dockit::html_ui_translate("Reset", "input.default_action");
				}
			}
			if (name.is_empty() && (tag == "input" || tag == "textarea" || tag == "select" || tag == "progress" || tag == "meter")) {
				for (const Ref<HTMLElement>& label_element : layout->query_selector_all("label")) {
					if (label_element->get_attribute("for") == p_element->get_id() && !p_element->get_id().is_empty()) {
						name = label_element->get_text_content().strip_edges();
						break;
					}
				}
				for (Ref<HTMLElement> ancestor = p_element->get_parent(); name.is_empty() && ancestor.is_valid(); ancestor = ancestor->get_parent()) {
					if (ancestor->get_tag_name() == "label") name = ancestor->get_text_content().strip_edges();
				}
			}
			if (name.is_empty() && tag == "optgroup") name = p_element->get_attribute("label").strip_edges();
			if (name.is_empty() && (is_heading || tag == "button" || tag == "a" || tag == "summary" || tag == "label" ||
				tag == "legend" || tag == "li" || tag == "option" || tag == "td" || tag == "th" ||
				role == DisplayServer::ROLE_CELL || role == DisplayServer::ROLE_ROW_HEADER ||
				role == DisplayServer::ROLE_COLUMN_HEADER || role == DisplayServer::ROLE_IMAGE)) {
				name = p_element->get_text_content().strip_edges();
			}
			if (name.is_empty() && p_element->is_content_editable()) name = p_element->get_text_content().strip_edges();
			if (name.is_empty()) name = p_element->get_attribute("title").strip_edges();
			if (!name.is_empty()) display->accessibility_update_set_name(child, name);
			if (role == DisplayServer::ROLE_STATIC_TEXT && !name.is_empty()) display->accessibility_update_set_value(child, name);
			if (!name.is_empty() && (role == DisplayServer::ROLE_STATIC_TEXT || role == DisplayServer::ROLE_BUTTON ||
				role == DisplayServer::ROLE_DEFAULT_BUTTON || role == DisplayServer::ROLE_LINK || role == DisplayServer::ROLE_TEXT_FIELD ||
				role == DisplayServer::ROLE_MULTILINE_TEXT_FIELD || role == DisplayServer::ROLE_CELL || role == DisplayServer::ROLE_ROW_HEADER ||
				role == DisplayServer::ROLE_COLUMN_HEADER || role == DisplayServer::ROLE_LIST_ITEM ||
				role == DisplayServer::ROLE_LIST_BOX_OPTION || role == DisplayServer::ROLE_IMAGE)) {
				text_covered = true;
			}
			const String description = p_element->get_attribute("aria-description", p_element->get_attribute("title")).strip_edges();
			if (!description.is_empty() && description != name) display->accessibility_update_set_description(child, description);
			if (is_heading) {
				String level = p_element->get_attribute("aria-level");
				if (level.is_empty() && tag.length() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6') {
					level = String::num_int64(tag.substr(1).to_int());
				}
				if (level.is_empty()) {
					display->accessibility_update_set_role_description(child,
						dockit::html_ui_translate("heading", "accessibility.role"));
				} else {
					Dictionary values;
					values["level"] = level;
					display->accessibility_update_set_role_description(child,
						dockit::html_ui_translate("heading level {level}", "accessibility.role").format(values));
				}
			}
			else if (is_landmark) {
				String description_role = p_element->get_attribute("role").to_lower();
				if (description_role.is_empty()) {
					if (tag == "main") description_role = "main";
					else if (tag == "nav") description_role = "navigation";
					else if (tag == "header") description_role = "banner";
					else if (tag == "footer") description_role = "content information";
					else if (tag == "aside") description_role = "complementary";
					else if (tag == "form") description_role = "form";
					else if (tag == "search") description_role = "search";
					else description_role = "region";
				}
				display->accessibility_update_set_role_description(child,
					dockit::html_ui_translate(description_role, "accessibility.role"));
			}
			const TypedArray<Rect2> boxes = p_element->get_client_rects();
			if (!boxes.is_empty()) {
				Rect2 bounds = transformed_bounds(boxes[0]);
				for (int64_t i = 1; i < boxes.size(); ++i) bounds = bounds.merge(transformed_bounds(boxes[i]));
				display->accessibility_update_set_bounds(child, bounds);
			}
			const bool disabled = p_element->has_attribute("disabled") || p_element->get_attribute("aria-disabled").to_lower() == "true";
			display->accessibility_update_set_flag(child, DisplayServer::FLAG_DISABLED, disabled);
			display->accessibility_update_set_flag(child, DisplayServer::FLAG_REQUIRED,
				p_element->has_attribute("required") || p_element->get_attribute("aria-required").to_lower() == "true");
			display->accessibility_update_set_flag(child, DisplayServer::FLAG_READONLY,
				p_element->has_attribute("readonly") || p_element->get_attribute("aria-readonly").to_lower() == "true");
			const String expanded = p_element->get_attribute("aria-expanded").to_lower();
			if (expanded == "true" || expanded == "false") {
				display->accessibility_update_set_state_description(child,
					dockit::html_ui_translate(expanded == "true" ? "expanded" : "collapsed", "accessibility.state"));
			}
			const String pressed = p_element->get_attribute("aria-pressed").to_lower();
			if (pressed == "true" || pressed == "false" || pressed == "mixed") {
				display->accessibility_update_set_state_description(child, dockit::html_ui_translate(
					pressed == "mixed" ? "mixed" : (pressed == "true" ? "pressed" : "not pressed"), "accessibility.state"));
			}
			if (p_element->get_attribute("aria-checked").to_lower() == "mixed") {
				display->accessibility_update_set_state_description(child,
					dockit::html_ui_translate("mixed", "accessibility.state"));
			}
			const String aria_checked = p_element->get_attribute("aria-checked").to_lower();
			if (aria_checked == "true" || aria_checked == "false") {
				if (role == DisplayServer::ROLE_CHECK_BOX || role == DisplayServer::ROLE_CHECK_BUTTON || role == DisplayServer::ROLE_RADIO_BUTTON) {
					display->accessibility_update_set_checked(child, aria_checked == "true");
				}
			}
			if (p_element->get_attribute("aria-invalid").to_lower() == "true") {
				display->accessibility_update_set_state_description(child,
					dockit::html_ui_translate("invalid", "accessibility.state"));
			}
			display->accessibility_update_set_tooltip(child, p_element->get_attribute("title"));
			if (!p_element->get_lang().is_empty()) display->accessibility_update_set_language(child, p_element->get_lang());
			const String live = p_element->get_attribute("aria-live").to_lower();
			if (live == "polite" || p_element->get_attribute("role").to_lower() == "status") display->accessibility_update_set_live(child, DisplayServer::LIVE_POLITE);
			else if (live == "assertive" || p_element->get_attribute("role").to_lower() == "alert") {
				display->accessibility_update_set_live(child, DisplayServer::LIVE_ASSERTIVE);
			}
			if (const auto* anchor = Object::cast_to<HTMLAnchorElement>(p_element.ptr()); anchor && !anchor->get_resolved_href().is_empty()) {
				display->accessibility_update_set_url(child, anchor->get_resolved_href());
			}
			if (const auto* input = Object::cast_to<HTMLInputElement>(p_element.ptr())) {
				if (input->get_type() == "checkbox" || input->get_type() == "radio") {
					display->accessibility_update_set_checked(child,
						p_element->get_attribute("aria-checked").is_empty() ? input->is_checked() : p_element->get_attribute("aria-checked").to_lower() == "true");
				}
				display->accessibility_update_set_value(child, input->get_value());
				const Dictionary validity = input->get_validity();
				if (!bool(validity.get("valid", true))) display->accessibility_update_set_state_description(child,
					dockit::html_ui_translate("invalid", "accessibility.state"));
				if (role == DisplayServer::ROLE_SLIDER || role == DisplayServer::ROLE_SPIN_BUTTON) {
					const double number = input->get_value_as_number();
					if (std::isfinite(number)) display->accessibility_update_set_num_value(child, number);
					display->accessibility_update_set_num_range(child,
						p_element->get_attribute("min").is_empty() ? 0.0 : p_element->get_attribute("min").to_float(),
						p_element->get_attribute("max").is_empty() ? 100.0 : p_element->get_attribute("max").to_float());
					const String step = p_element->get_attribute("step");
					if (!step.is_empty() && step != "any") display->accessibility_update_set_num_step(child, step.to_float());
				}
			}
			if (const auto* textarea = Object::cast_to<HTMLTextAreaElement>(p_element.ptr())) {
				display->accessibility_update_set_value(child, textarea->get_value());
			}
			if (const auto* select = Object::cast_to<HTMLSelectElement>(p_element.ptr())) {
				display->accessibility_update_set_value(child, select->get_value());
			}
			if (const auto* option = Object::cast_to<HTMLOptionElement>(p_element.ptr())) {
				display->accessibility_update_set_list_item_selected(child, option->is_selected());
			}
			if (const auto* details = Object::cast_to<HTMLDetailsElement>(p_element.ptr())) {
				display->accessibility_update_set_state_description(child,
					dockit::html_ui_translate(details->is_open() ? "expanded" : "collapsed", "accessibility.state"));
				const uint64_t id = p_element->get_instance_id();
				const auto action = callable_mp(this, &HTMLLabel::perform_accessibility_action).bind(id,
					int32_t(details->is_open() ? DisplayServer::ACTION_COLLAPSE : DisplayServer::ACTION_EXPAND));
				display->accessibility_update_add_action(child,
					details->is_open() ? DisplayServer::ACTION_COLLAPSE : DisplayServer::ACTION_EXPAND, action);
			}
			else if (expanded == "true" || expanded == "false") {
				const uint64_t id = p_element->get_instance_id();
				const DisplayServer::AccessibilityAction action_type = expanded == "true" ?
					DisplayServer::ACTION_COLLAPSE : DisplayServer::ACTION_EXPAND;
				display->accessibility_update_add_action(child, action_type,
					callable_mp(this, &HTMLLabel::perform_accessibility_action).bind(id, int32_t(action_type)));
			}
			if (role == DisplayServer::ROLE_TABLE) {
				const TypedArray<HTMLElement> rows = p_element->query_selector_all("tr");
				int32_t row_count = 0;
				int32_t columns = 0;
				for (int64_t row_index = 0; row_index < rows.size(); ++row_index) {
					const Ref<HTMLElement> row = rows[row_index];
					if (row->closest("table") != p_element) continue;
					++row_count;
					if (columns != 0) continue;
					for (const Ref<HTMLElement>& cell : row->get_children()) {
						if (cell->get_tag_name() == "td" || cell->get_tag_name() == "th") {
							columns += std::max(1, int(cell->get_attribute("colspan", "1").to_int()));
						}
					}
				}
				display->accessibility_update_set_table_row_count(child, row_count);
				display->accessibility_update_set_table_column_count(child, columns);
			}
			if (role == DisplayServer::ROLE_ROW) {
				const Ref<HTMLElement> table = p_element->closest("table");
				if (table.is_valid()) {
					const TypedArray<HTMLElement> rows = table->query_selector_all("tr");
					int32_t row_index = 0;
					for (int64_t index = 0; index < rows.size(); ++index) {
						const Ref<HTMLElement> row = rows[index];
						if (row->closest("table") != table) continue;
						if (row == p_element) {
							display->accessibility_update_set_table_row_index(child, row_index);
							break;
						}
						++row_index;
					}
				}
			}
			if (role == DisplayServer::ROLE_CELL || role == DisplayServer::ROLE_ROW_HEADER || role == DisplayServer::ROLE_COLUMN_HEADER) {
				const Ref<HTMLElement> row = p_element->closest("tr");
				const Ref<HTMLElement> table = row.is_valid() ? row->closest("table") : Ref<HTMLElement>();
				if (row.is_valid() && table.is_valid()) {
					int32_t row_index = 0;
					const TypedArray<HTMLElement> rows = table->query_selector_all("tr");
					for (int64_t index = 0; index < rows.size(); ++index) {
						const Ref<HTMLElement> candidate = rows[index];
						if (candidate->closest("table") != table) continue;
						if (candidate == row) break;
						++row_index;
					}
					int32_t column_index = 0;
					for (const Ref<HTMLElement>& cell : row->get_children()) {
						if (cell->get_tag_name() != "td" && cell->get_tag_name() != "th") continue;
						if (cell == p_element) break;
						column_index += std::max(1, int(cell->get_attribute("colspan", "1").to_int()));
					}
					display->accessibility_update_set_table_cell_position(child, row_index, column_index);
					display->accessibility_update_set_table_cell_span(child,
						std::max(1, int(p_element->get_attribute("rowspan", "1").to_int())),
						std::max(1, int(p_element->get_attribute("colspan", "1").to_int())));
				}
			}
			if (role == DisplayServer::ROLE_LIST) {
				int32_t count = 0;
				for (const Ref<HTMLElement>& item : p_element->get_children()) if (item->get_tag_name() == "li") ++count;
				display->accessibility_update_set_list_item_count(child, count);
			}
			if (role == DisplayServer::ROLE_LIST_ITEM || role == DisplayServer::ROLE_LIST_BOX_OPTION) {
				Ref<HTMLElement> list = p_element->get_parent();
				while (list.is_valid() && list->get_tag_name() != "ul" && list->get_tag_name() != "ol" && list->get_tag_name() != "menu") {
					list = list->get_parent();
				}
				if (list.is_valid()) {
					int32_t index = 0;
					for (const Ref<HTMLElement>& sibling : list->get_children()) {
						if (sibling->get_tag_name() != "li") continue;
						if (sibling == p_element) break;
						++index;
					}
					display->accessibility_update_set_list_item_index(child, index);
					int32_t level = 1;
					for (Ref<HTMLElement> parent = list->get_parent(); parent.is_valid(); parent = parent->get_parent()) {
						if (parent->get_tag_name() == "ul" || parent->get_tag_name() == "ol" || parent->get_tag_name() == "menu") ++level;
					}
					display->accessibility_update_set_list_item_level(child, level);
				}
				else if (role == DisplayServer::ROLE_LIST_BOX_OPTION) {
					const Ref<HTMLElement> select = p_element->closest("select");
					if (select.is_valid()) {
						int32_t index = 0;
						for (const Ref<HTMLElement>& option : select->query_selector_all("option")) {
							if (option == p_element) break;
							++index;
						}
						display->accessibility_update_set_list_item_index(child, index);
					}
				}
				const String selected = p_element->get_attribute("aria-selected").to_lower();
				if (selected == "true" || selected == "false") display->accessibility_update_set_list_item_selected(child, selected == "true");
				const String expanded_state = p_element->get_attribute("aria-expanded").to_lower();
				if (expanded_state == "true" || expanded_state == "false") display->accessibility_update_set_list_item_expanded(child, expanded_state == "true");
			}
			const String haspopup = p_element->get_attribute("aria-haspopup").to_lower();
			if (haspopup == "menu") display->accessibility_update_set_popup_type(child, DisplayServer::POPUP_MENU);
			else if (haspopup == "listbox") display->accessibility_update_set_popup_type(child, DisplayServer::POPUP_LIST);
			else if (haspopup == "tree") display->accessibility_update_set_popup_type(child, DisplayServer::POPUP_TREE);
			else if (haspopup == "dialog" || haspopup == "true") display->accessibility_update_set_popup_type(child, DisplayServer::POPUP_DIALOG);
			display->accessibility_update_set_flag(child, DisplayServer::FLAG_BUSY,
				p_element->get_attribute("aria-busy").to_lower() == "true");
			const Ref<HTMLDialogElement> active_modal = layout->top_modal();
			display->accessibility_update_set_flag(child, DisplayServer::FLAG_MODAL,
				p_element->get_attribute("aria-modal").to_lower() == "true" ||
				(active_modal.is_valid() && active_modal->get_instance_id() == p_element->get_instance_id()));
			for (const String& id : p_element->get_attribute("aria-describedby").strip_edges().split(" ", false)) {
				pending_relations.emplace_back(child, id, 1);
			}
			for (const String& id : p_element->get_attribute("aria-controls").strip_edges().split(" ", false)) {
				pending_relations.emplace_back(child, id, 2);
			}
			const uint64_t id = p_element->get_instance_id();
			display->accessibility_update_add_action(child, DisplayServer::ACTION_SCROLL_INTO_VIEW,
				callable_mp(this, &HTMLLabel::perform_accessibility_action).bind(id, int32_t(DisplayServer::ACTION_SCROLL_INTO_VIEW)));
			if (layout->is_focusable(p_element)) {
				display->accessibility_update_add_action(child, DisplayServer::ACTION_FOCUS,
					callable_mp(this, &HTMLLabel::perform_accessibility_action).bind(id, int32_t(DisplayServer::ACTION_FOCUS)));
			}
			if (role == DisplayServer::ROLE_BUTTON || role == DisplayServer::ROLE_DEFAULT_BUTTON || role == DisplayServer::ROLE_LINK ||
				tag == "summary" || tag == "option") {
				display->accessibility_update_add_action(child, DisplayServer::ACTION_CLICK,
					callable_mp(this, &HTMLLabel::perform_accessibility_action).bind(id, int32_t(DisplayServer::ACTION_CLICK)));
			}
		}
		}
		for (const Ref<HTMLNode>& child : p_element->get_child_nodes()) {
			if (auto* child_element = Object::cast_to<HTMLElement>(child.ptr())) {
				self(self, Ref<HTMLElement>(child_element), child_parent, text_covered);
				continue;
			}
			if (text_covered) continue;
			if (auto* text_node = Object::cast_to<HTMLTextNode>(child.ptr())) {
				const String text = text_node->get_data().strip_edges();
				if (text.is_empty()) continue;
				const RID text_element = display->accessibility_create_sub_element(child_parent, DisplayServer::ROLE_STATIC_TEXT);
				if (!text_element.is_valid()) continue;
				accessibility_tree_elements.push_back(text_element);
				display->accessibility_update_set_name(text_element, text);
				display->accessibility_update_set_value(text_element, text);
				const Rect2 bounds = p_element->get_bounding_rect();
				if (bounds.has_area()) display->accessibility_update_set_bounds(text_element, transformed_bounds(bounds));
				for (Ref<HTMLElement> ancestor = p_element; ancestor.is_valid(); ancestor = ancestor->get_parent()) {
					const String language = ancestor->get_lang();
					if (!language.is_empty()) {
						display->accessibility_update_set_language(text_element, language);
						break;
					}
				}
			}
		}
	};

	add_element(add_element, document_element, accessible, false);
	if (accessibility_tree_elements.empty()) display->accessibility_update_set_value(accessible, get_accessible_text());
	for (const auto& [source, target_id, relation] : pending_relations) {
		const Ref<HTMLElement> target = layout->get_element_by_id(target_id);
		if (target.is_null()) continue;
		const auto it = elements_by_id.find(target->get_instance_id());
		if (it == elements_by_id.end()) continue;
		if (relation == 0) display->accessibility_update_add_related_labeled_by(source, it->second);
		else if (relation == 1) display->accessibility_update_add_related_described_by(source, it->second);
		else display->accessibility_update_add_related_controls(source, it->second);
	}
}

void HTMLLabel::clear_accessibility_tree() {
	auto* display = DisplayServer::get_singleton();
	if (display) {
		for (auto it = accessibility_tree_elements.rbegin(); it != accessibility_tree_elements.rend(); ++it) {
			if (display->accessibility_has_element(*it)) display->accessibility_free_element(*it);
		}
	}
	accessibility_tree_elements.clear();
}

void HTMLLabel::perform_accessibility_action(uint64_t p_element_id, int32_t p_action) {
	auto* element = Object::cast_to<HTMLElement>(ObjectDB::get_instance(ObjectID(p_element_id)));
	if (!element || !element->is_valid()) return;
	const Ref<HTMLElement> target = Ref<HTMLElement>(element);
	if (p_action == DisplayServer::ACTION_FOCUS) {
		focus_element(target);
	}
	else if (p_action == DisplayServer::ACTION_CLICK) {
		if (target->has_attribute("disabled") || target->get_attribute("aria-disabled").to_lower() == "true" || !canvas) return;
		HTMLLabelCanvas::PointerInfo pointer;
		pointer.target = target;
		pointer.position = target->get_bounding_rect().get_center();
		pointer.detail = 1;
		if (canvas->dispatch_pointer_event(StringName("click"), target, pointer, true)) activate_element(target);
	}
	else if (p_action == DisplayServer::ACTION_SCROLL_INTO_VIEW) {
		scroll_to_element(target);
	}
	else if (p_action == DisplayServer::ACTION_BLUR && canvas && canvas->focused_element == target) {
		canvas->set_focused_element({});
	}
	else if (auto* details = Object::cast_to<HTMLDetailsElement>(element)) {
		if (p_action == DisplayServer::ACTION_EXPAND) details->set_open(true);
		else if (p_action == DisplayServer::ACTION_COLLAPSE) details->set_open(false);
	}
	else if (p_action == DisplayServer::ACTION_EXPAND || p_action == DisplayServer::ACTION_COLLAPSE) {
		if (!canvas) return;
		HTMLLabelCanvas::PointerInfo pointer;
		pointer.target = target;
		pointer.position = target->get_bounding_rect().get_center();
		pointer.detail = 1;
		if (canvas->dispatch_pointer_event(StringName("click"), target, pointer, true)) activate_element(target);
	}
}

void HTMLLabel::set_document(const Ref<HTMLDocument>& p_document) {
	if (document == p_document) {
		return;
	}
	if (navigation_request_id != 0) dockit::cancel_http_request(navigation_request_id);
	navigation_request_id = 0;
	++navigation_sequence;
	if (document.is_valid()) {
		document->disconnect("changed", callable_mp(this, &HTMLLabel::document_changed).bind(document->get_instance_id()));
	}
	document = p_document;
	navigation_history.clear();
	navigation_position = -1;
	current_location = document.is_valid() ?
		(document->get_source_path().is_empty() ? document->get_path() : document->get_source_path()) : source_path;
	if (document.is_valid()) {
		DockitSourceEditorPlugin::observe_source_document(document);
		document->connect("changed", callable_mp(this, &HTMLLabel::document_changed).bind(document->get_instance_id()));
	}
	source_changed();
}

Ref<HTMLDocument> HTMLLabel::get_document() const {
	return document;
}

void HTMLLabel::set_text(const String& p_text) {
	if (text != p_text) {
		text = p_text;
		if (document.is_null()) {
			if (navigation_request_id != 0) dockit::cancel_http_request(navigation_request_id);
			navigation_request_id = 0;
			++navigation_sequence;
			navigation_history.clear();
			navigation_position = -1;
			current_location = source_path;
			source_changed();
		}
	}
}

String HTMLLabel::get_text() const {
	return text;
}

void HTMLLabel::set_base_dir(const String& p_base_dir) {
	if (base_dir != p_base_dir) {
		base_dir = p_base_dir;
		if (document.is_null()) {
			source_changed();
		}
	}
}

String HTMLLabel::get_base_dir() const {
	return base_dir;
}

String HTMLLabel::get_base_uri() const { return layout->get_base_uri(); }

void HTMLLabel::set_source_path(const String& p_path) {
	if (source_path != p_path) {
		source_path = p_path;
		if (document.is_null()) {
			if (navigation_request_id != 0) dockit::cancel_http_request(navigation_request_id);
			navigation_request_id = 0;
			++navigation_sequence;
			navigation_history.clear();
			navigation_position = -1;
			current_location = p_path;
			source_changed();
		}
	}
}

void HTMLLabel::set_stylesheet(const String& p_stylesheet) {
	if (stylesheet != p_stylesheet) {
		stylesheet = p_stylesheet;
		if (document.is_null()) {
			source_changed();
		}
	}
}

String HTMLLabel::get_stylesheet() const {
	return stylesheet;
}

Vector2i HTMLLabel::get_content_size() const {
	return content_size;
}

String HTMLLabel::get_render_error() const {
	return render_error;
}

void HTMLLabel::set_css_diagnostics_enabled(bool p_enabled) {
	if (layout->is_css_diagnostics_enabled() == p_enabled) {
		return;
	}
	layout->set_css_diagnostics_enabled(p_enabled);
	if (p_enabled) {
		queue_refresh();
	}
	else if (canvas) {
		canvas->queue_redraw();
		canvas->refresh_diagnostic_tooltip(std::nullopt);
	}
}

bool HTMLLabel::is_css_diagnostics_enabled() const {
	return layout->is_css_diagnostics_enabled();
}

TypedArray<Dictionary> HTMLLabel::get_css_diagnostics() const {
	return layout->get_css_diagnostics();
}

Vector2i HTMLLabel::get_scroll_offset() const {
	return scroll_offset;
}

void HTMLLabel::set_scroll_offset(const Vector2i& p_offset) {
	cancel_viewport_scroll();
	cancel_viewport_pan_snap();
	if (!is_inside_tree() || !viewport_control || refresh_queued || pending_scroll_offset) {
		pending_scroll_offset = p_offset;
		pending_scroll_requested = true;
		return;
	}
	const Vector2 limit = Vector2((content_size - viewport_size).max(Vector2i()));
	const auto snapped = layout->snap_viewport(viewport_size, { get_scroll_offset(), p_offset, limit,{ 1, 1 },
		dockit::HTMLSnapMethod::destination });
	apply_scroll_offset(Vector2i(snapped.offset.round()));
}

bool HTMLLabel::scroll_to_anchor(const String& p_id, float p_align) {
	if (!is_inside_tree() || refresh_queued || viewport_size.x <= 0 || viewport_size.y <= 0) {
		return false;
	}
	const auto target = layout->scroll_anchor_into_view(p_id, p_align, *this,
		callable_mp(this, &HTMLLabel::on_nested_scroll_advanced));
	if (!target) return false;
	apply_scroll_target(*target, p_align);
	request_hover_check();
	return true;
}

Ref<HTMLElement> HTMLLabel::create_element(const String& p_tag_name) {
	parse_pending_source();
	return layout->create_element(p_tag_name);
}

Ref<HTMLDocumentFragment> HTMLLabel::create_document_fragment() {
	parse_pending_source();
	return layout->create_document_fragment();
}

Ref<HTMLRange> HTMLLabel::create_range() {
	parse_pending_source();
	return layout->create_range();
}

Ref<HTMLSelection> HTMLLabel::get_selection() const {
	return source_dirty ? Ref<HTMLSelection>() : layout->get_selection();
}

Ref<HTMLElement> HTMLLabel::query_selector(const String& p_selector) const {
	return source_dirty ? Ref<HTMLElement>() : layout->query_selector(p_selector);
}

TypedArray<HTMLElement> HTMLLabel::query_selector_all(const String& p_selector) const {
	return source_dirty ? TypedArray<HTMLElement>() : layout->query_selector_all(p_selector);
}

Ref<HTMLElement> HTMLLabel::get_element_by_id(const String& p_id) const {
	return source_dirty ? Ref<HTMLElement>() : layout->get_element_by_id(p_id);
}

Ref<HTMLElement> HTMLLabel::get_element_at(const Vector2& p_position) const {
	const auto local = get_hit_document_position(p_position);
	return local ? layout->get_element_at(*local) : Ref<HTMLElement>();
}

TypedArray<HTMLElement> HTMLLabel::get_elements_at(const Vector2& p_position) const {
	const auto local = get_hit_document_position(p_position);
	return local ? layout->get_elements_at(*local) : TypedArray<HTMLElement>();
}

std::optional<Vector2> HTMLLabel::get_hit_document_position(const Vector2& p_position) const {
	if (!canvas || !is_inside_tree() || refresh_queued || !p_position.is_finite() || !Rect2(Vector2(), get_size()).has_point(p_position)) {
		return {};
	}
	const Vector2 local = (canvas->get_global_transform().affine_inverse() * get_global_transform()).xform(p_position);
	return get_visible_rect().has_point(local) ? std::optional<Vector2>(local) : std::nullopt;
}

Ref<HTMLElement> HTMLLabel::get_focused_element() const {
	return canvas && has_focus() && layout->is_element_connected(canvas->focused_element) &&
		layout->allows_modal_interaction(canvas->focused_element)
		? canvas->focused_element : Ref<HTMLElement>();
}

bool HTMLLabel::focus_element(const Ref<HTMLElement>& p_element) {
	if (!canvas || !is_visible_in_tree() || !layout->is_focusable(p_element)) return false;
	grab_focus();
	if (!has_focus()) return false;
	canvas->set_focused_element(p_element, true);
	return canvas->focused_element == p_element;
}

void HTMLLabel::on_design_mode_changed() {
	if (!canvas || layout.is_null()) return;
	if (refresh_queued && is_inside_tree()) refresh();
	canvas->tab_elements = layout->get_tab_elements(canvas->has_focusable_elements);
	canvas->update_focus_mode();
	if (canvas->focused_element.is_valid() && !layout->is_focusable(canvas->focused_element)) {
		canvas->set_focused_element({});
	}
	layout->repaint_input();
}

bool HTMLLabel::execute_edit_command(const String& p_command, const String& p_value) {
	if (source_dirty || !layout.is_valid()) return false;
	Ref<HTMLElement> host = get_focused_element();
	if (host.is_null() || !host->is_content_editable()) return false;
	const Ref<HTMLSelection> selection = layout->get_selection();
	if (selection.is_null() || selection->get_range_count() == 0) return false;
	const Ref<HTMLRange> range = selection->get_range_at(0);
	if (range.is_null() || range->is_collapsed()) return false;
	const String command = p_command.to_lower();
	String tag;
	String input_type;
	if (command == "bold") { tag = "strong"; input_type = "formatBold"; }
	else if (command == "italic") { tag = "em"; input_type = "formatItalic"; }
	else if (command == "underline") { tag = "u"; input_type = "formatUnderline"; }
	else if (command == "strikethrough" || command == "strike") { tag = "s"; input_type = "formatStrikeThrough"; }
	else if (command == "subscript") { tag = "sub"; input_type = "formatSubscript"; }
	else if (command == "superscript") { tag = "sup"; input_type = "formatSuperscript"; }
	else if (command == "formatblock") {
		tag = p_value.strip_edges().to_lower().trim_prefix("<").trim_suffix(">");
		if (!(tag == "p" || tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" ||
			tag == "h6" || tag == "blockquote" || tag == "pre" || tag == "div")) return false;
		input_type = "formatBlock";
	}
	else if (command == "insertorderedlist" || command == "insertunorderedlist") {
		tag = command == "insertorderedlist" ? "ol" : "ul";
		input_type = command == "insertorderedlist" ? "insertOrderedList" : "insertUnorderedList";
	}
	else if (command == "createlink") {
		if (p_value.is_empty() || p_value.contains("\"") || p_value.contains("<") ||
			p_value.contains(">") || p_value.begins_with("javascript:") || p_value.begins_with("data:")) return false;
		tag = "a";
		input_type = "createLink";
	}
	else if (command == "forecolor" || command == "backcolor") {
		if (!Color::html_is_valid(p_value)) return false;
		tag = "span";
		input_type = command == "forecolor" ? "formatForeColor" : "formatBackColor";
	}
	else return false;
	if (!dispatch_input_event(StringName("beforeinput"), host, input_type, p_value, false)) return false;
	Ref<HTMLElement> wrapper = layout->create_element(tag);
	if (wrapper.is_null()) return false;
	if (command == "createlink") wrapper->set_attribute("href", p_value);
	if (command == "forecolor") wrapper->set_attribute("style", String("color: ") + p_value + ";");
	if (command == "backcolor") wrapper->set_attribute("style", String("background-color: ") + p_value + ";");
	if (range->surround_contents(wrapper) != OK) return false;
	selection->remove_all_ranges();
	Ref<HTMLRange> selected = layout->create_range();
	selected->select_node_contents(wrapper);
	selection->add_range(selected);
	dispatch_input_event(StringName("input"), host, input_type, p_value, false);
	queue_refresh(true);
	return true;
}

bool HTMLLabel::activate_element(const Ref<HTMLElement>& p_element) {
	if (source_dirty || !is_visible_in_tree() || !layout->allows_modal_interaction(p_element)) return false;
	const String uri = layout->get_activatable_link_uri(p_element);
	if (!uri.is_empty()) {
		activate_link(uri);
		return true;
	}
	return activate_form_control(p_element);
}

void HTMLLabel::activate_link(const String& p_uri) {
	if (p_uri.begins_with("#")) {
		scroll_to_anchor(dockit::decode_html_fragment(p_uri.substr(1)));
	}
	emit_signal("link_pressed", p_uri);
}

bool HTMLLabel::scroll_to_element(const Ref<HTMLElement>& p_element, float p_align) {
	if (!is_inside_tree() || refresh_queued || viewport_size.x <= 0 || viewport_size.y <= 0) {
		return false;
	}
	const auto target = layout->scroll_element_into_view(p_element, p_align, *this,
		callable_mp(this, &HTMLLabel::on_nested_scroll_advanced));
	if (!target) return false;
	apply_scroll_target(*target, p_align);
	request_hover_check();
	return true;
}

bool HTMLLabel::scroll_element_into_view(const Ref<HTMLElement>& p_element, const Dictionary& p_options) {
	if (!is_inside_tree() || refresh_queued || viewport_size.x <= 0 || viewport_size.y <= 0) return false;
	const auto options = scroll_options(p_options);
	if (!options) return false;
	const auto target = layout->scroll_element_into_view(p_element, {}, *this,
		callable_mp(this, &HTMLLabel::on_nested_scroll_advanced), &*options);
	if (!target) return false;
	apply_scroll_target(*target, {}, &*options);
	request_hover_check();
	return true;
}

void HTMLLabel::apply_scroll_target(const dockit::HTMLScrollTarget& p_target, std::optional<float> p_align,
	const dockit::HTMLScrollOptions* p_options) {
	if (p_target.viewport_fixed) return;
	cancel_viewport_pan_snap();
	const Vector2i current = get_scroll_offset();
	const Rect2 region = layout->viewport_scroll_region(Rect2(current, viewport_size));
	const Vector2 offset = Vector2(current) + dockit::html_scroll_delta(p_target.area, region, p_align, p_options);
	const Vector2i max_offset = (content_size - viewport_size).max(Vector2i());
	const auto rounded = p_align ? offset : Vector2(
		offset.x < current.x ? std::floor(offset.x) : std::ceil(offset.x),
		offset.y < current.y ? std::floor(offset.y) : std::ceil(offset.y));
	const auto snap = layout->snap_viewport(viewport_size, { current, rounded, max_offset,{ 1, 1 },
		dockit::HTMLSnapMethod::destination,{ p_target.element, p_target.element } });
	const Vector2i destination(layout->viewport_snap_axes() == Vector2i() ? snap.offset : snap.offset.round());
	cancel_viewport_scroll();
	if (destination == current) return;
	const bool css_smooth = layout->viewport_uses_smooth_scroll();
	if (!(p_options ? p_options->smooth(css_smooth) : css_smooth)) {
		apply_scroll_offset(destination);
		return;
	}
	const auto update = callable_mp(this, &HTMLLabel::advance_viewport_scroll).bind(Vector2(current), Vector2(destination));
	viewport_scroll_tween = HTMLLayout::start_scroll_tween(*this, update);
}

void HTMLLabel::advance_viewport_scroll(double p_fraction, Vector2 p_from, Vector2 p_to) {
	applying_scroll_tween = true;
	apply_scroll_offset(Vector2i(p_from.lerp(p_to, p_fraction).round()));
	applying_scroll_tween = false;
	if (p_fraction >= 1.0) viewport_scroll_tween.unref();
}

void HTMLLabel::cancel_viewport_scroll() {
	if (viewport_scroll_tween.is_valid()) {
		viewport_scroll_tween->kill();
		viewport_scroll_tween.unref();
	}
}

void HTMLLabel::on_nested_scroll_advanced() {
	canvas->cancel_click();
	request_hover_check();
}

void HTMLLabel::set_selection_enabled(bool p_enabled) {
	selection_enabled = p_enabled;
	if (canvas) {
		canvas->update_focus_mode();
		if (!p_enabled) {
			canvas->cancel_press();
			clear_selection();
		}
		request_hover_check();
	}
}

bool HTMLLabel::is_selection_enabled() const {
	return selection_enabled;
}

void HTMLLabel::set_drag_and_drop_selection_enabled(bool p_enabled) {
	drag_and_drop_selection_enabled = p_enabled;
	if (!p_enabled && canvas) {
		canvas->selection_drag_attempt = false;
		canvas->drag_source.unref();
		canvas->drag_text = String();
	}
}

void HTMLLabel::set_context_menu_enabled(bool p_enabled) {
	context_menu_enabled = p_enabled;
	if (!p_enabled && canvas) canvas->close_input_menu();
}

bool HTMLLabel::is_context_menu_enabled() const {
	return context_menu_enabled;
}

void HTMLLabel::set_animation_process_mode(AnimationProcessMode p_mode) {
	ERR_FAIL_COND(p_mode < ANIMATION_PROCESS_IDLE || p_mode > ANIMATION_PROCESS_MANUAL);
	if (animation_process_mode == p_mode) return;
	animation_process_mode = p_mode;
	update_internal_processing();
}

HTMLLabel::AnimationProcessMode HTMLLabel::get_animation_process_mode() const {
	return animation_process_mode;
}

TypedArray<HTMLAnimation> HTMLLabel::get_animations(const Ref<HTMLElement>& p_element) {
	flush_queued_refresh();
	return layout->get_animations(p_element);
}

Ref<HTMLAnimation> HTMLLabel::animate(const Ref<HTMLElement>& p_element, const TypedArray<Dictionary>& p_keyframes,
	const Dictionary& p_options) {
	flush_queued_refresh();
	return layout->animate(p_element, p_keyframes, p_options);
}

Dictionary HTMLLabel::get_computed_style(const Ref<HTMLElement>& p_element, const String& p_pseudo) {
	flush_queued_refresh();
	return layout->get_computed_style(p_element, p_pseudo);
}

String HTMLLabel::get_computed_style_property(const Ref<HTMLElement>& p_element, const String& p_name, const String& p_pseudo) {
	flush_queued_refresh();
	return layout->get_computed_style_property(p_element, p_name, p_pseudo);
}

TypedArray<Dictionary> HTMLLabel::get_stylesheets() {
	parse_pending_source();
	return layout->get_stylesheets();
}

PackedStringArray HTMLLabel::get_css_rules(int64_t p_stylesheet) {
	parse_pending_source();
	return layout->get_css_rules(p_stylesheet);
}

Error HTMLLabel::insert_rule(int64_t p_stylesheet, const String& p_rule, int64_t p_index) {
	parse_pending_source();
	return layout->insert_rule(p_stylesheet, p_rule, p_index);
}

Error HTMLLabel::delete_rule(int64_t p_stylesheet, int64_t p_index) {
	parse_pending_source();
	return layout->delete_rule(p_stylesheet, p_index);
}

bool HTMLLabel::supports_css(const String& p_property_or_condition, const String& p_value) const {
	return layout->supports_css(p_property_or_condition, p_value);
}

bool HTMLLabel::supports_selector(const String& p_selector) const {
	return layout->supports_selector(p_selector);
}

bool HTMLLabel::matches_media(const String& p_query) const {
	return !source_dirty && layout->matches_media(p_query);
}

void HTMLLabel::advance_animation(double p_delta) {
	ERR_FAIL_COND_MSG(!std::isfinite(p_delta) || p_delta < 0.0, "Animation delta must be finite and non-negative.");
	if (!is_inside_tree()) return;
	// Commit pending style changes at the current time before moving the clock.
	flush_queued_refresh();
	if (viewport_size.x <= 0) return;
	layout->advance_css_animations(p_delta);
	flush_queued_refresh();
	update_internal_processing();
}

void HTMLLabel::set_caret_blink_enabled(bool p_enabled) {
	if (caret_blink == p_enabled) return;
	caret_blink = p_enabled;
	if (canvas) canvas->restart_caret_blink();
	notify_property_list_changed();
}

bool HTMLLabel::is_caret_blink_enabled() const {
	return caret_blink;
}

void HTMLLabel::set_caret_blink_interval(double p_interval) {
	ERR_FAIL_COND_MSG(!std::isfinite(p_interval) || p_interval <= 0.0, "Caret blink interval must be a finite positive number.");
	if (caret_blink_interval == p_interval) return;
	caret_blink_interval = p_interval;
	if (canvas && caret_blink) canvas->restart_caret_blink();
}

double HTMLLabel::get_caret_blink_interval() const {
	return caret_blink_interval;
}

void HTMLLabel::select_all() {
	if (layout->top_modal_node()) {
		const auto range = layout->modal_text_range();
		set_selection_range(range.x, range.y);
	}
	else set_selection_range(0, get_plain_text().length());
}

void HTMLLabel::clear_selection() {
	if (const auto selection = get_selection(); selection.is_valid()) selection->remove_all_ranges();
	use_node_selection = false;
	if (canvas) {
		const bool changed = canvas->selection_anchor != canvas->selection_caret;
		canvas->selection_anchor = canvas->selection_caret = 0;
		canvas->queue_redraw();
		if (changed) callable_mp(layout.ptr(), &HTMLLayout::request_auto_content_update).call_deferred();
	}
}

String HTMLLabel::get_accessible_text() const {
	return viewport_size.x > 0 && viewport_size.y > 0 ? layout->get_accessible_text() : String();
}

String HTMLLabel::get_plain_text() const {
	return source_dirty ? String() : layout->get_plain_text();
}

Vector2i HTMLLabel::get_selection_range() const {
	return source_dirty ? Vector2i() : get_content_selection_range();
}

void HTMLLabel::set_selection_range(int64_t p_from, int64_t p_to) {
	if (!selection_enabled || !canvas || source_dirty) return;
	const int64_t length = layout->get_plain_text().length();
	const int64_t anchor = std::clamp(p_from, int64_t(0), length);
	const int64_t caret = std::clamp(p_to, int64_t(0), length);
	if (!use_node_selection && anchor == canvas->selection_anchor && caret == canvas->selection_caret) return;
	canvas->selection_anchor = anchor;
	canvas->selection_caret = caret;
	capture_node_selection();
	canvas->queue_redraw();
	callable_mp(layout.ptr(), &HTMLLayout::request_auto_content_update).call_deferred();
}

TypedArray<Rect2> HTMLLabel::get_selection_rects() const {
	const auto range = get_selection_range();
	return layout->get_selection_rects(range.x, range.y);
}

bool HTMLLabel::copy_selection() const {
	const auto text = get_selected_text();
	if (text.is_empty() || !DisplayServer::get_singleton()) return false;
	DisplayServer::get_singleton()->clipboard_set(text);
	return true;
}

Vector2i HTMLLabel::find_text(const String& p_text, int64_t p_from, bool p_case_sensitive) const {
	if (p_text.is_empty()) return { -1, -1 };
	const auto content = get_plain_text();
	const int64_t from = std::clamp(p_from, int64_t(0), content.length());
	const int64_t index = p_case_sensitive ? content.find(p_text, from) : content.findn(p_text, from);
	return index < 0 ? Vector2i(-1, -1) : Vector2i(index, index + p_text.length());
}

bool HTMLLabel::scroll_to_text_range(int64_t p_from, int64_t p_to) {
	if (!is_inside_tree() || refresh_queued || viewport_size.x <= 0 || viewport_size.y <= 0) return false;
	const auto boxes = layout->get_selection_rects(p_from, p_to);
	if (boxes.is_empty()) return false;
	apply_scroll_target({ boxes[0], false, nullptr }, {});
	request_hover_check();
	return true;
}

String HTMLLabel::get_selected_text() const {
	if (!canvas) {
		return {};
	}
	const auto range = get_selection_range();
	return layout->get_copyable_text(range.x, range.y);
}

PackedStringArray HTMLLabel::_get_configuration_warnings() const {
	PackedStringArray warnings;
	if (!render_error.is_empty()) {
		warnings.append(render_error);
	}
	return warnings;
}

void HTMLLabel::ensure_controls() {
	if (viewport_control) {
		return;
	}

	const auto* display = DisplayServer::get_singleton();
	if (display) {
		color_scheme_timer = memnew(Timer);
		color_scheme_timer->set_wait_time(0.25);
		color_scheme_timer->set_ignore_time_scale(true);
		color_scheme_timer->connect("timeout", callable_mp(this, &HTMLLabel::poll_preferred_color_scheme));
		add_child(color_scheme_timer, false, INTERNAL_MODE_BACK);
	}

	viewport_control = memnew(Control);
	viewport_control->set_name("_HTMLScroll");
	viewport_control->set_mouse_filter(MOUSE_FILTER_PASS);
	add_child(viewport_control, false, INTERNAL_MODE_BACK);
	viewport_control->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	viewport_control->connect("draw", callable_mp(this, &HTMLLabel::draw_viewport_gutters));
	viewport_control->connect("gui_input", callable_mp(this, &HTMLLabel::on_viewport_gui_input));
	viewport_control->connect("mouse_entered", callable_mp(this, &HTMLLabel::request_hover_check));
	viewport_control->connect("mouse_exited", callable_mp(this, &HTMLLabel::set_viewport_bar_hover).bind(-1));
	viewport_control->connect("mouse_exited", callable_mp(this, &HTMLLabel::on_viewport_mouse_exited));

	scroll_content = memnew(Control);
	scroll_content->set_name("_HTMLContent");
	scroll_content->set_mouse_filter(MOUSE_FILTER_IGNORE);
	scroll_content->set_clip_contents(true);
	viewport_control->add_child(scroll_content, false, INTERNAL_MODE_BACK);

	viewport_bars = memnew(Control);
	viewport_bars->set_name("_HTMLViewportBars");
	viewport_bars->set_mouse_filter(MOUSE_FILTER_IGNORE);
	viewport_bars->set_z_index(1);
	viewport_control->add_child(viewport_bars, false, INTERNAL_MODE_BACK);
	viewport_bars->connect("draw", callable_mp(this, &HTMLLabel::draw_viewport_scrollbars));

	canvas = memnew(HTMLLabelCanvas);
	canvas->set_name("_HTMLCanvas");
	canvas->label = this;
	canvas->set_mouse_filter(MOUSE_FILTER_PASS);
	canvas->update_focus_mode();
	scroll_content->add_child(canvas, false, INTERNAL_MODE_BACK);
}

void HTMLLabel::poll_preferred_color_scheme() {
	if (layout->update_preferred_color_scheme()) queue_refresh();
}

void HTMLLabel::queue_refresh(bool p_resource_only) {
	hover_reflow = false;
	hover_passes_remaining = HOVER_PASSES;
	if (!canvas || !is_inside_tree()) return;
	resource_refresh_only = p_resource_only && (!refresh_queued || resource_refresh_only);
	if (!refresh_queued) {
		refresh_queued = true;
		callable_mp(this, &HTMLLabel::flush_queued_refresh).call_deferred();
	}
}

void HTMLLabel::flush_queued_refresh() {
	if (refresh_queued) refresh();
}

void HTMLLabel::queue_interaction_style_refresh() {
	interaction_styles_dirty = true;
	queue_refresh();
}

void HTMLLabel::source_changed() {
	request_behavior_reset(true);
}

void HTMLLabel::reset_document_source() {
	input_mouse_position_known = false;
	input_mouse_view_position.reset();
	close_input_picker();
	clear_videos();
	clear_audio();
	if (canvas) canvas->cancel_all_touch_pointers();
	cancel_viewport_scroll();
	cancel_viewport_pan_snap();
	viewport_touch_index = -1;
	viewport_touch_dragged = false;
	dragging_viewport_scrollbar = {};
	close_select_menu();
	dialog_focus.clear();
	closed_dialogs.clear();
	focused_modal.unref();
	source_dirty = true;
	document_ready_events_queued = false;
	document_ready_events_sent = false;
	layout->clear();
	update_internal_processing();
	pending_scroll_offset = Vector2i();
	pending_scroll_requested = false;
	if (canvas) {
		canvas->refresh_diagnostic_tooltip(std::nullopt);
		canvas->cancel_press();
		canvas->set_focused_element({});
		canvas->tab_elements.clear();
		canvas->has_focusable_elements = false;
		canvas->update_focus_mode();
		clear_selection();
	}
	queue_refresh();
}

void HTMLLabel::document_changed(uint64_t p_document_id) {
	if (document.is_valid() && document->get_instance_id() == p_document_id) {
		source_changed();
	}
}

void HTMLLabel::parse_pending_source() {
	if (!source_dirty) {
		return;
	}
	source_dirty = false;
	layout_updated = true;
	layout->audio_host = ObjectID(get_instance_id());
	layout->video_host = ObjectID(get_instance_id());
	layout->dialog_host = ObjectID(get_instance_id());
	if (document.is_valid()) {
		document->parse_into(*layout.ptr(), DOCUMENT_DEFAULTS);
		parsed = layout->get_render_error().is_empty();
	}
	else {
		parsed = layout->parse_source(text, base_dir, stylesheet, Dictionary(), source_path, false, DOCUMENT_DEFAULTS) == OK;
	}
	layout->get_dom().set_event_debug(event_debug);
	sync_videos();
	layout->set_edit_callback(callable_mp(this, &HTMLLabel::on_layout_edited));
	layout->get_dom().set_mutation_callback([id = ObjectID(get_instance_id())](const dockit::HTMLDOM::Changes&) {
		if (auto* host = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(id))) host->on_dom_mutated();
	});
	queue_audio_sync();
	layout->set_dom_selection_change_callback(callable_mp(this, &HTMLLabel::on_node_selection_changed));
	layout->set_element_scroll_callback(callable_mp(this, &HTMLLabel::on_nested_scroll_advanced));
	layout->set_transition_callback(callable_mp(this, &HTMLLabel::on_transition_frame));
	layout->set_content_visibility_callbacks(callable_mp(this, &HTMLLabel::on_auto_content_update),
		callable_mp(this, &HTMLLabel::get_content_selection_range));
	layout->set_resource_layout_callback(callable_mp(this, &HTMLLabel::on_resource_layout_update));
}

void HTMLLabel::on_auto_content_update() {
	layout_updated = true;
	queue_refresh();
}

void HTMLLabel::on_resource_layout_update() {
	layout_updated = true;
	queue_refresh(true);
}

void HTMLLabel::on_transition_frame(bool p_size_changed) {
	if (p_size_changed) queue_refresh();
	request_hover_check();
}

void HTMLLabel::update_internal_processing() {
	const bool in_tree = is_inside_tree();
	const bool animate = in_tree && viewport_size.x > 0 && layout->has_active_css_animations();
	const bool track_ime = in_tree && canvas && canvas->ime_window.has_value();
	const bool behavior_processing = in_tree && std::any_of(attached_behaviors.begin(), attached_behaviors.end(),
		[](const Ref<HTMLBehavior>& p_behavior) { return p_behavior.is_valid() && p_behavior->has_async_work(); });
	set_process_internal(track_ime || (in_tree && (has_playing_audio() || has_active_video())) ||
		(animate && animation_process_mode == ANIMATION_PROCESS_IDLE) || behavior_processing);
	set_physics_process_internal(animate && animation_process_mode == ANIMATION_PROCESS_PHYSICS);
}

void HTMLLabel::update_behavior_processing() {
	const ObjectID id(get_instance_id());
	const std::vector<Ref<HTMLBehavior>> behaviors = attached_behaviors;
	const double delta = get_process_delta_time();
	for (const Ref<HTMLBehavior>& behavior : behaviors) {
		if (behavior.is_null() || behavior->get_label() != this) continue;
		behavior->process_async(delta);
		if (!ObjectDB::get_instance(id)) return;
	}
}

Vector2i HTMLLabel::get_content_selection_range() const {
	if (!canvas) return {};
	if (use_node_selection) return layout->get_node_selection_offsets();
	return Vector2i(std::min(canvas->selection_anchor, canvas->selection_caret), std::max(canvas->selection_anchor, canvas->selection_caret));
}

void HTMLLabel::capture_node_selection() {
	capturing_node_selection = true;
	layout->set_node_selection_offsets(canvas->selection_anchor, canvas->selection_caret);
	capturing_node_selection = false;
	use_node_selection = false;
}

void HTMLLabel::on_node_selection_changed() {
	if (!capturing_node_selection) use_node_selection = true;
	if (canvas) canvas->queue_redraw();
}

void HTMLLabel::on_dom_mutated() {
	queue_component_sync();
	if (!canvas) return;
	canvas->reconcile_interactions_after_edit(false);
	const auto selection = get_selection();
	if (selection.is_valid() && selection->get_range_count() > 0) use_node_selection = true;
	canvas->queue_redraw();
}

void HTMLLabel::on_layout_edited() {
	queue_audio_sync();
	queue_video_sync();
	layout_updated = true;
	on_dom_mutated();
	if (canvas) {
		auto& tabs = canvas->tab_elements;
		tabs.erase(std::remove_if(tabs.begin(), tabs.end(),
			[this](const auto& element) { return !layout->is_element_connected(element); }), tabs.end());
	}
	queue_refresh();
}

dockit::HTMLScrollbar HTMLLabel::viewport_scrollbar(int p_axis) const {
	if (!visible_viewport_bars[p_axis] || viewport_size == Vector2i()) return {};
	const real_t thickness = viewport_scrollbar_style.thickness();
	const Rect2 track = p_axis == 0 ?
		Rect2(viewport_painted_gutters.x, viewport_size.y, viewport_size.x, thickness) :
		Rect2(is_layout_rtl() ? 0 : get_size().x - thickness, 0, thickness, viewport_size.y);
	return dockit::make_html_scrollbar(track, p_axis, viewport_size[p_axis],
		std::max(0, content_size[p_axis] - viewport_size[p_axis]), scroll_offset[p_axis]);
}

void HTMLLabel::draw_viewport_scrollbars() {
	for (int axis = 0; axis < 2; ++axis) {
		const auto bar = viewport_scrollbar(axis);
		if (!bar.track.has_area()) continue;
		const bool pressed = dragging_viewport_scrollbar[axis];
		const Rect2 thumb = viewport_scrollbar_style.visible_thumb(bar.thumb, axis, pressed || hovered_viewport_bar == axis);
		viewport_bars->draw_rect(bar.track, viewport_scrollbar_style.track_color());
		const auto ink = viewport_scrollbar_style.thumb_color(pressed);
		const real_t radius = std::min(thumb.size.x, thumb.size.y) / 2.0f;
		if (radius <= 0 || ink.a == 0) continue;
		PackedVector2Array outline;
		for (int end = 0; end < 2; ++end) {
			Vector2 center = thumb.get_center();
			center[axis] = end == 0 ? thumb.get_end()[axis] - radius : thumb.position[axis] + radius;
			for (int step = 0; step <= 8; ++step) {
				const real_t angle = (end + step / 8.0f) * Math_PI - Math_PI / 2.0f + axis * Math_PI / 2.0f;
				outline.append(center + Vector2(Math::cos(angle), Math::sin(angle)) * radius);
			}
		}
		viewport_bars->draw_colored_polygon(outline, ink);
	}
}

void HTMLLabel::set_viewport_bar_hover(int p_axis) {
	if (std::exchange(hovered_viewport_bar, p_axis) != p_axis) viewport_bars->queue_redraw();
}

void HTMLLabel::draw_viewport_gutters() {
	if (viewport_painted_gutters == Vector2() || viewport_size == Vector2i()) return;
	const Vector2 translation = scroll_content->get_position() + canvas->get_position();
	const Vector2 origin = translation + Vector2(scroll_offset);
	layout->draw_viewport_background(*viewport_control, translation,
		Rect2(origin - Vector2(viewport_painted_gutters.x, 0), Vector2(viewport_painted_gutters.x, viewport_size.y)),
		scroll_offset);
	layout->draw_viewport_background(*viewport_control, translation,
		Rect2(origin + Vector2(viewport_size.x, 0), Vector2(viewport_painted_gutters.y, viewport_size.y)),
		scroll_offset);
}

void HTMLLabel::refresh() {
	refresh_queued = false;
	const bool resource_update = std::exchange(resource_refresh_only, false);
	if (!is_inside_tree()) {
		return;
	}
	if (!resource_update && !hover_reflow && !interaction_styles_dirty) {
		canvas->reconcile_interactions_after_edit(false);
	}
	if (!pending_scroll_offset) {
		pending_scroll_offset = get_scroll_offset();
	}

	parse_pending_source();
	const bool button_style_changed = std::exchange(interaction_styles_dirty, false);
	if (button_style_changed && parsed) {
		layout->mark_styles_pending();
	}
	const bool layout_changed = layout_updated || hover_reflow || button_style_changed;
	layout_updated = false;
	if (!parsed) {
		if (std::exchange(page_color_scheme, litehtml::color_scheme::light) != litehtml::color_scheme::light) queue_redraw();
		viewport_scrollbar_style = {};
		update_view({}, {}, {}, { 1, 1 }, layout_changed, layout->get_render_error());
		return;
	}

	using namespace litehtml;
	const Vector2 available = get_size();
	const Vector2i media_size(available.floor());
	if (media_size.x <= 0 || media_size.y <= 0) {
		update_view({}, {}, {}, { 1, 1 }, layout_changed);
		return;
	}
	bool skip_base_layout = layout->can_skip_hover_base_layout(media_size);
	if (skip_base_layout && layout->update_preferred_color_scheme()) {
		skip_base_layout = false;
	}
	if (!skip_base_layout && layout->layout(media_size) != OK) {
		update_view({}, {}, {}, { 1, 1 }, layout_changed, layout->get_render_error());
		return;
	}

	const auto style = layout->get_viewport_style();
	if (std::exchange(page_color_scheme, style.scheme) != style.scheme) queue_redraw();
	viewport_scrollbar_style = style.scrollbar;
	const bool show_bars = style.scrollbar.width != scrollbar_width::none;
	const real_t bar_width = style.scrollbar.thickness();
	const real_t bar_height = bar_width;
	const bool reserve = style.gutter != scrollbar_gutter::automatic;
	const real_t opposite_gutter = style.gutter == scrollbar_gutter::both_edges ? bar_width : 0;
	const Vector2 unconditional_gutters(
		(reserve || style.root_overflow[1] == overflow_scroll ? bar_width : 0) + opposite_gutter,
		style.root_overflow[0] == overflow_scroll ? bar_height : 0);
	const Vector2i unit_size((available - unconditional_gutters).floor());
	const Vector2i input_axes(is_interactive_overflow(style.overflow[0]), is_interactive_overflow(style.overflow[1]));
	const auto visible_bar = [show_bars](overflow p_mode, bool p_overflowing) {
		return show_bars && (p_mode == overflow_scroll || (p_mode == overflow_auto && p_overflowing));
		};
	const auto client_size = [&](bool p_horizontal, bool p_vertical) {
		const Vector2 gutters((reserve || p_vertical ? bar_width : 0) + opposite_gutter, p_horizontal ? bar_height : 0);
		return Vector2i((available - gutters).floor());
		};
	bool horizontal = visible_bar(style.overflow[0], false);
	bool vertical = visible_bar(style.overflow[1], false);
	Vector2i viewport = client_size(horizontal, vertical);

	// Measure with unconditional gutters before testing for bars that depend on overflowing content.
	for (;;) {
		if (viewport.x <= 0 || viewport.y <= 0) {
			update_view({}, {}, {}, { 1, 1 }, layout_changed);
			return;
		}
		if (layout->layout_viewport(viewport, media_size, unit_size) != OK) {
			update_view({}, {}, {}, { 1, 1 }, layout_changed, layout->get_render_error());
			return;
		}
		const Vector2i size = layout->get_content_size();
		horizontal = visible_bar(style.overflow[0], horizontal || size.x > viewport.x);
		vertical = visible_bar(style.overflow[1], vertical || size.y > viewport.y);
		const Vector2i next_viewport = client_size(horizontal, vertical);
		if (next_viewport == viewport) {
			if (picker_input.is_valid() && !input_picker_valid()) close_input_picker();
			if (open_select.is_valid()) {
				const Ref<HTMLSelectElement> select = open_select;
				select_rows = select.is_valid() ? layout->select_menu_rows(open_select) : layout->suggestion_menu_rows(open_select);
				if (select_rows.empty()) close_select_menu();
			}
			const real_t empty_gutter = reserve && !vertical ? bar_width : 0;
			const Vector2 empty_gutters = is_layout_rtl() ?
				Vector2(empty_gutter, opposite_gutter) : Vector2(opposite_gutter, empty_gutter);
			Vector2 painted_gutters = empty_gutters;
			if (vertical) painted_gutters[is_layout_rtl() ? 0 : 1] = bar_width;
			layout->complete_hover_layout();
			update_view(viewport, size, { horizontal, vertical }, input_axes, layout_changed, {}, painted_gutters);
			return;
		}
		viewport = next_viewport;
	}
}

void HTMLLabel::update_view(const Vector2i& p_viewport, const Vector2i& p_content,
	const Vector2i& p_visible_bars, const Vector2i& p_input_axes, bool p_layout_changed, const String& p_error,
	const Vector2& p_painted_gutters) {
	view_update_pending = true;
	if (p_viewport == Vector2i()) {
		layout->update_hover(std::nullopt);
		hover_passes_remaining = 0;
		canvas->tab_elements.clear();
		canvas->has_focusable_elements = false;
		canvas->cancel_press();
		if (layout->top_modal_node()) canvas->update_focus_mode();
		sync_dialog_focus(false);
		canvas->set_focused_element({});
	}
	else {
		canvas->tab_elements = layout->get_tab_elements(canvas->has_focusable_elements);
		if (layout->top_modal_node() || canvas->has_focusable_elements) canvas->update_focus_mode();
		sync_dialog_focus(true);
		canvas->reconcile_interactions_after_edit(true);
		if (open_select.is_valid()) refresh_select_menu();
		if (canvas->focused_element.is_valid() && !layout->is_focusable(canvas->focused_element)) {
			canvas->set_focused_element({});
		}
		if (canvas->pressed_button.is_valid() && !layout->is_enabled_button(canvas->pressed_button)) {
			canvas->cancel_button_press();
		}
		if (canvas->box_drag && !layout->get_element_scroll_bounds(canvas->box_drag->element)) {
			canvas->cancel_press();
		}
	}
	canvas->update_focus_mode();
	layout_signal_pending |= p_layout_changed || viewport_size != p_viewport ||
		content_size != p_content || render_error != p_error;
	viewport_size = p_viewport;
	queue_window_resize_event(p_viewport);
	queue_accessibility_update();
	update_internal_processing();
	content_size = p_content;
	viewport_input_axes = p_input_axes;
	const int64_t text_length = layout->get_plain_text().length();
	canvas->selection_anchor = std::min(canvas->selection_anchor, text_length);
	canvas->selection_caret = std::min(canvas->selection_caret, text_length);
	canvas->set_size(Vector2(content_size.max(viewport_size)));
	scroll_content->set_position(Vector2(p_painted_gutters.x, 0));
	scroll_content->set_size(Vector2(p_viewport));
	visible_viewport_bars = p_visible_bars;
	viewport_painted_gutters = p_painted_gutters;
	layout->set_viewport_gutters(p_painted_gutters);
	viewport_control->queue_redraw();
	viewport_bars->queue_redraw();
	sync_viewport_scroll();
	if (!pending_navigation_anchor.is_empty() && p_viewport.x > 0 && p_viewport.y > 0) {
		const String anchor = std::exchange(pending_navigation_anchor, String());
		scroll_to_anchor(anchor);
	}
	callable_mp(this, &HTMLLabel::apply_pending_scroll).call_deferred();
	if (render_error != p_error) {
		render_error = p_error;
		update_configuration_warnings();
	}
}

Rect2 HTMLLabel::get_visible_rect() const {
	return Rect2(Vector2(scroll_offset), Vector2(viewport_size));
}

void HTMLLabel::apply_scroll_offset(const Vector2i& p_offset) {
	const Vector2i max_offset = (content_size - viewport_size).max(Vector2i());
	const Vector2i offset = p_offset.clamp(Vector2i(), max_offset);
	if (scroll_offset == offset) return;
	if (!applying_scroll_tween) cancel_viewport_scroll();
	scroll_offset = offset;
	canvas->cancel_click();
	if (!hover_reflow) hover_passes_remaining = HOVER_PASSES;
	sync_viewport_scroll();
	dispatch_scroll_event(layout->get_document_element());
}

void HTMLLabel::apply_pending_scroll() {
	if (!is_inside_tree() || refresh_queued) {
		return;
	}
	if (pending_scroll_offset) {
		Vector2i offset = *pending_scroll_offset;
		pending_scroll_offset.reset();
		const bool requested = std::exchange(pending_scroll_requested, false);
		if (requested || (viewport_scroll_tween.is_null() && viewport_pan_snap_timer.is_null() && dragging_viewport_scrollbar == Vector2i())) {
			const Vector2 limit = Vector2((content_size - viewport_size).max(Vector2i()));
			const auto method = requested ? dockit::HTMLSnapMethod::destination : dockit::HTMLSnapMethod::relayout;
			offset = Vector2i(layout->snap_viewport(viewport_size, { get_scroll_offset(), offset, limit,{ 1, 1 }, method }).offset.round());
		}
		apply_scroll_offset(offset);
	}
	view_update_pending = false;
	queue_interaction();
}

void HTMLLabel::on_viewport_gui_input(const Ref<InputEvent>& p_event) {
	if (layout->top_modal().is_valid()) {
		viewport_control->accept_event();
		return;
	}
	if (handle_viewport_touch(p_event)) {
		const Ref<InputEventScreenDrag> drag = p_event;
		if (drag.is_valid() && viewport_touch_dragged && canvas)
			canvas->cancel_touch_pointer(drag->get_device(), drag->get_index());
		viewport_control->accept_event();
		return;
	}
	const Ref<InputEventMouseMotion> motion = p_event;
	const Ref<InputEventMouseButton> button = p_event;
	const auto move_bar = [&](const Vector2& p_point) {
		const int hit = viewport_scrollbar(0).track.has_point(p_point) ? 0 : viewport_scrollbar(1).track.has_point(p_point) ? 1 : -1;
		set_viewport_bar_hover(hit);
		return hit;
		};
	if (motion.is_valid()) {
		const Vector2 global_point = viewport_control->get_global_transform_with_canvas().xform(motion->get_position());
		const Vector2 label_point = get_global_transform_with_canvas().affine_inverse().xform(global_point);
		const int hovered_bar = move_bar(motion->get_position());
		if (hovered_bar >= 0 || !Rect2(Vector2(), Vector2(viewport_size)).has_point(label_point)) {
			input_mouse_position_known = true;
			input_mouse_view_position.reset();
		}
		else if (!input_mouse_position_known) {
			input_mouse_position_known = true;
			input_mouse_view_position = label_point;
		}
		for (int axis = 0; axis < 2; ++axis) {
			if (dragging_viewport_scrollbar[axis]) {
				drag_viewport_bar(axis, motion->get_position()[axis]);
				viewport_control->accept_event();
			}
		}
		return;
	}
	if (button.is_valid() && button->get_button_index() == MOUSE_BUTTON_LEFT) {
		const auto point = button->get_position();
		const int hit = move_bar(point);
		if (button->is_pressed() && hit >= 0) {
			cancel_viewport_scroll();
			cancel_viewport_pan_snap();
			const auto bar = viewport_scrollbar(hit);
			dragging_viewport_scrollbar[hit] = 1;
			viewport_scrollbar_grab = bar.thumb.has_point(point) ? (point[hit] - bar.thumb.position[hit]) / bar.thumb.size[hit] : 0.5;
			drag_viewport_bar(hit, point[hit]);
			viewport_bars->queue_redraw();
			viewport_control->accept_event();
		}
		else if (!button->is_pressed() && dragging_viewport_scrollbar != Vector2i()) {
			const auto axes = std::exchange(dragging_viewport_scrollbar, Vector2i());
			if (!button->is_canceled()) settle_viewport_snap(axes);
			viewport_bars->queue_redraw();
			viewport_control->accept_event();
		}
		return;
	}
	if (button.is_valid() && button->is_pressed() &&
		(button->get_button_index() == MOUSE_BUTTON_WHEEL_UP || button->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN ||
			button->get_button_index() == MOUSE_BUTTON_WHEEL_LEFT || button->get_button_index() == MOUSE_BUTTON_WHEEL_RIGHT)) {
		if (wheel_dispatched_in_canvas) {
			wheel_dispatched_in_canvas = false;
			++wheel_dispatch_sequence;
		}
		else if (canvas) {
			const Vector2 global_point = viewport_control->get_global_transform_with_canvas().xform(button->get_position());
			HTMLLabelCanvas::PointerInfo pointer;
			pointer.position = canvas->get_global_transform_with_canvas().affine_inverse().xform(global_point);
			pointer.shift = button->is_shift_pressed();
			pointer.ctrl = button->is_ctrl_pressed();
			pointer.alt = button->is_alt_pressed();
			pointer.meta = button->is_meta_pressed();
			pointer.source_event = p_event;
			if (!canvas->handle_wheel_input(pointer, 36.0f)) {
				viewport_control->accept_event();
				return;
			}
		}
	}

	const Vector2 steps = HTMLLayout::scroll_input_steps(p_event);
	if (steps == Vector2()) return;
	cancel_viewport_scroll();
	const Ref<InputEventPanGesture> pan = p_event;
	if (pan.is_null()) cancel_viewport_pan_snap();
	const Vector2 contained = layout->viewport_contained_scroll(steps);
	const Vector2i snap_axes = layout->viewport_snap_axes() * viewport_input_axes;
	const Vector2i active_axes = Vector2i(steps.x != 0, steps.y != 0) * viewport_input_axes;
	const Vector2 current = get_scroll_offset();
	const Vector2 limit = Vector2((content_size - viewport_size).max(Vector2i()));
	const Vector2 natural = current + Vector2(viewport_size) * steps * Vector2(viewport_input_axes) / 8.0f;
	const auto snap = pan.is_valid() ?
		dockit::HTMLSnapResult{ natural.clamp(Vector2(), limit),{} } :
		layout->snap_viewport(viewport_size, { current, natural, limit, active_axes, dockit::HTMLSnapMethod::direction });
	apply_scroll_offset(Vector2i(snap.offset.round()));
	const Vector2 applied = get_scroll_offset();
	Vector2 remaining = steps;
	for (int axis = 0; axis < 2; ++axis) {
		if (contained[axis] != 0 || snap.stopped[axis] || applied[axis] != current[axis]) remaining[axis] = 0;
	}
	const Vector2i pan_axes(snap_axes.x && applied.x != current.x, snap_axes.y && applied.y != current.y);
	if (pan.is_valid() && pan_axes != Vector2i()) {
		viewport_pan_axes = viewport_pan_axes.max(pan_axes);
		if (viewport_pan_snap_timer.is_valid()) viewport_pan_snap_timer->kill();
		viewport_pan_snap_timer = HTMLLayout::start_snap_timer(*this, callable_mp(this, &HTMLLabel::settle_viewport_pan));
	}
	if (!HTMLLayout::forward_scroll_remainder(p_event, remaining)) viewport_control->accept_event();
}

void HTMLLabel::on_viewport_mouse_exited() {
	if (get_viewport() && get_viewport()->gui_get_hovered_control() == canvas) return;
	input_mouse_position_known = true;
	input_mouse_view_position.reset();
	request_hover_check();
}

void HTMLLabel::clear_canvas_wheel_dispatch(uint64_t p_sequence) {
	if (wheel_dispatch_sequence == p_sequence) wheel_dispatched_in_canvas = false;
}

void HTMLLabel::place_overlay(Control& p_overlay, const Vector2& p_point) {
	const Rect2 bounds = get_global_transform_with_canvas().affine_inverse().xform(get_viewport_rect());
	p_overlay.set_position(p_point.clamp(bounds.position, (bounds.get_end() - p_overlay.get_size()).max(bounds.position)));
}

bool HTMLLabel::handle_viewport_touch(const Ref<InputEvent>& p_event) {
	const Ref<InputEventScreenTouch> touch = p_event;
	const Ref<InputEventScreenDrag> drag = p_event;
	if (touch.is_valid()) {
		if (touch->is_pressed() && viewport_touch_index < 0 && viewport_input_axes != Vector2i() &&
			(content_size - viewport_size).max(Vector2i()) * viewport_input_axes != Vector2i()) {
			cancel_viewport_scroll();
			cancel_viewport_pan_snap();
			viewport_touch_index = touch->get_index();
			viewport_touch_motion = {};
			viewport_touch_velocity = {};
			viewport_touch_dragged = false;
		}
		if (!touch->is_pressed() && touch->get_index() == viewport_touch_index) {
			viewport_touch_index = -1;
			const bool dragged = std::exchange(viewport_touch_dragged, false);
			if (dragged && !touch->is_canceled()) {
				const Vector2 current = scroll_offset;
				const Vector2 limit = Vector2((content_size - viewport_size).max(Vector2i()));
				const Vector2 projected = (current + viewport_touch_velocity * 0.16f).clamp(Vector2(), limit);
				const auto snap = layout->snap_viewport(viewport_size, { current, projected, limit, viewport_input_axes,
					dockit::HTMLSnapMethod::destination });
				if (viewport_touch_velocity.length() >= 30 && snap.offset.distance_to(current) >= 1) {
					viewport_scroll_tween = HTMLLayout::start_scroll_tween(*this,
						callable_mp(this, &HTMLLabel::advance_viewport_scroll).bind(current, snap.offset));
				}
				else {
					settle_viewport_snap(viewport_input_axes);
				}
			}
			return dragged;
		}
	}
	if (drag.is_null() || drag->get_index() != viewport_touch_index) return false;
	const Vector2 delta = -drag->get_relative() * Vector2(viewport_input_axes);
	if (!delta.is_finite()) return false;
	const Vector2 velocity = -drag->get_velocity() * Vector2(viewport_input_axes);
	if (velocity.is_finite()) viewport_touch_velocity = velocity;
	viewport_touch_motion += delta;
	if (!viewport_touch_dragged && viewport_touch_motion.length() < DRAG_DEADZONE) return false;
	if (!std::exchange(viewport_touch_dragged, true)) canvas->cancel_press();
	const Vector2 movement = viewport_touch_motion.round();
	viewport_touch_motion -= movement;
	apply_scroll_offset(scroll_offset + Vector2i(movement));
	return true;
}

void HTMLLabel::drag_viewport_bar(int p_axis, real_t p_position) {
	const auto bar = viewport_scrollbar(p_axis);
	const real_t travel = bar.track.size[p_axis] - bar.thumb.size[p_axis];
	if (travel <= 0 || !std::isfinite(p_position)) return;
	const real_t fraction = (p_position - bar.track.position[p_axis] - bar.thumb.size[p_axis] * viewport_scrollbar_grab) / travel;
	Vector2i offset = scroll_offset;
	offset[p_axis] = Math::round(std::clamp(fraction, real_t(0), real_t(1)) * bar.range);
	apply_scroll_offset(offset);
}

void HTMLLabel::settle_viewport_pan() {
	viewport_pan_snap_timer.unref();
	settle_viewport_snap(std::exchange(viewport_pan_axes, Vector2i()));
}

void HTMLLabel::cancel_viewport_pan_snap() {
	if (viewport_pan_snap_timer.is_valid()) viewport_pan_snap_timer->kill();
	viewport_pan_snap_timer.unref();
	viewport_pan_axes = {};
}

void HTMLLabel::settle_viewport_snap(const Vector2i& p_axes) {
	const Vector2i axes = p_axes * viewport_input_axes;
	if (!is_inside_tree() || layout->viewport_snap_axes() * axes == Vector2i()) return;
	const Vector2 current = get_scroll_offset();
	const Vector2 limit = Vector2((content_size - viewport_size).max(Vector2i()));
	const auto snap = layout->snap_viewport(viewport_size, { current, current, limit, axes,
		dockit::HTMLSnapMethod::destination });
	apply_scroll_offset(Vector2i(snap.offset.round()));
}

void HTMLLabel::sync_viewport_scroll() {
	canvas->set_position(-Vector2(scroll_offset));
	layout->set_viewport_scroll_offset(scroll_offset);
	gui_hover_dirty = true;
	if (viewport_painted_gutters != Vector2()) viewport_control->queue_redraw();
	viewport_bars->queue_redraw();
	canvas->queue_redraw();
	queue_interaction();
}

void HTMLLabel::request_hover_check() {
	hover_passes_remaining = HOVER_PASSES;
	queue_interaction();
}

void HTMLLabel::queue_interaction() {
	if (canvas && is_inside_tree() && !interaction_queued) {
		interaction_queued = true;
		callable_mp(this, &HTMLLabel::update_interaction).call_deferred();
	}
}

void HTMLLabel::update_interaction() {
	interaction_queued = false;
	if (!is_inside_tree() || refresh_queued || view_update_pending) {
		return;
	}
	update_select_menu();
	if (picker_type == "color") update_color_picker();
	else if (is_time_picker_open()) update_clock_picker();

	if (std::exchange(reveal_focused_element, false)) {
		if (const auto target = layout->scroll_element_into_view(canvas->focused_element, std::nullopt, *this,
			callable_mp(this, &HTMLLabel::on_nested_scroll_advanced))) {
			apply_scroll_target(*target, std::nullopt);
		}
	}

	std::optional<Vector2> point;
	if (is_visible_in_tree() && viewport_size != Vector2i()) {
		Viewport* viewport = get_viewport();
		if (gui_hover_dirty) {
			gui_hover_dirty = false;
			// Refresh Godot's hovered Control after moving the canvas or its scrollbars.
			updating_gui_hover = true;
			viewport->update_mouse_cursor_state();
			updating_gui_hover = false;
			if (!is_inside_tree() || refresh_queued) {
				return;
			}
		}
		const auto* hovered = viewport->gui_get_hovered_control();
		const Vector2 port_mouse = viewport_control->get_local_mouse_position();
		int hovered_bar = -1;
		if (hovered == viewport_control && !layout->top_modal_node()) {
			for (int axis = 0; axis < 2; ++axis) {
				if (viewport_scrollbar(axis).track.has_point(port_mouse)) hovered_bar = axis;
			}
		}
		set_viewport_bar_hover(hovered_bar);
		const Vector2 live_mouse = canvas->get_local_mouse_position();
		const bool has_recorded_mouse = input_mouse_position_known && input_mouse_view_position &&
			Rect2(Vector2(), Vector2(viewport_size)).has_point(*input_mouse_view_position);
		// Headless has no live cursor position; synthetic input is available through the recorded event point.
		const bool live_mouse_available = DisplayServer::get_singleton()->get_name() != "headless";
		if (hovered == canvas && get_visible_rect().has_point(live_mouse) && live_mouse_available) {
			point = live_mouse;
		}
		else if (has_recorded_mouse) {
			point = *input_mouse_view_position + Vector2(scroll_offset);
		}
	}

	dockit::HTMLHitTestResult hit_test;
	if (point) hit_test.emplace(layout->hit_test_element(*point));
	layout->update_scrollbar_hover(point, hit_test);
	if (!canvas->selection_dragged && !canvas->scrollbar_drag) {
		const TypedArray<HTMLElement> previous_hover = layout->get_hovered_elements();
		const dockit::HTMLHoverUpdate hover_update = layout->update_hover(point, hit_test);
		if (hover_update != dockit::HTMLHoverUpdate::unchanged) {
			// 在回流之前派发 mouseenter / mouseleave，避免链已被改写
			canvas->dispatch_hover_events(previous_hover, canvas->get_local_mouse_position());
			if (hover_update == dockit::HTMLHoverUpdate::layout && hover_passes_remaining > 0) {
				// A hover rule can move or hide its own target. Allow one correction after sorting.
				--hover_passes_remaining;
				hover_reflow = true;
				refresh();
				return;
			}
		}
	}

	hover_passes_remaining = 0;
	hover_reflow = false;
	canvas->update_cursor(point);
	canvas->refresh_diagnostic_tooltip(point);
	if (!refresh_queued && !closed_dialogs.empty()) {
		const ObjectID id(get_instance_id());
		emit_dialog_closures();
		if (!ObjectDB::get_instance(id)) return;
	}
	queue_behavior_mount();
	queue_document_ready_events();
	if (layout_signal_pending && !refresh_queued) {
		layout_signal_pending = false;
		emit_signal("layout_changed");
	}
}
