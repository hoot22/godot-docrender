#include "html_label.h"

#include "formats/html/elements/html_input_node.h"
#include "formats/html/elements/html_textarea_node.h"
#include "formats/html/dom/html_dom.h"
#include "formats/html/dom/html_range.h"
#include "formats/html/dom/html_selection.h"
#include "formats/html/html_ui_translations.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/system_font.hpp>
#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/window.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

using namespace godot;
using dockit::HTMLInputNode;
using dockit::HTMLTextAreaNode;

namespace {
	constexpr real_t INPUT_MENU_OUTER_PADDING = 8.0f;
	constexpr real_t INPUT_MENU_ITEM_PADDING_X = 12.0f;
	constexpr real_t INPUT_MENU_GROUP_GAP = 17.0f;
	constexpr real_t INPUT_MENU_MIN_ROW_HEIGHT = 16.0f;
	constexpr real_t INPUT_MENU_MIN_WIDTH = 220.0f;
	constexpr int INPUT_MENU_SEPARATOR_COUNT = 2;

	const char* edit_maxlength(const dockit::HTMLTextControlNode& p_input) {
		const auto* field = dynamic_cast<const HTMLInputNode*>(&p_input);
		return field ? field->maxlength() : p_input.get_attr("maxlength", "");
	}

	dockit::HTMLTextLayout shape_input_menu_text(const String& p_text, const Ref<Font>& p_font, int p_font_size) {
		if (p_text.is_empty() || p_font.is_null()) return {};
		Ref<TextLine> line;
		line.instantiate();
		if (!line->add_string(p_text, p_font, p_font_size)) return {};
		return dockit::HTMLTextLayout(line, line->get_line_ascent());
	}

	enum InputCommand {
		INPUT_UNDO,
		INPUT_REDO,
		INPUT_CUT,
		INPUT_COPY,
		INPUT_PASTE,
		INPUT_SELECT_ALL,
	};

	struct InputMenuItem {
		const char* label;
		const char* action;
	};

	constexpr InputMenuItem INPUT_MENU_ITEMS[] = {
		{"Undo", "ui_undo"},
		{"Redo", "ui_redo"},
		{"Cut", "ui_cut"},
		{"Copy", "ui_copy"},
		{"Paste", "ui_paste"},
		{"Select All", "ui_text_select_all"},
	};

	Key input_menu_accelerator(const StringName& p_action) {
		for (const Ref<InputEvent>& event : InputMap::get_singleton()->action_get_events(p_action)) {
			const Ref<InputEventKey> key = event;
			if (key.is_valid()) {
				return key->get_physical_keycode() != KEY_NONE ?
					key->get_physical_keycode_with_modifiers() : key->get_keycode_with_modifiers();
			}
		}
		return KEY_NONE;
	}

	Window* find_native_ime_window(Viewport* p_viewport) {
		while (p_viewport) {
			if (auto* window = Object::cast_to<Window>(p_viewport)) {
				if (!window->is_embedded()) return window;

				// Find the actual embedder, which can be above the parent viewport.
				do {
					Node* parent = p_viewport->get_parent();
					p_viewport = parent ? parent->get_viewport() : nullptr;
				} while (p_viewport && !p_viewport->is_embedding_subwindows());
			} else {
				auto* container = Object::cast_to<SubViewportContainer>(p_viewport->get_parent());
				// An offscreen texture has no unique screen position.
				p_viewport = container ? container->get_viewport() : nullptr;
			}
		}
		return nullptr;
	}
}

void HTMLLabelCanvas::set_input_caret(const Ref<HTMLElement>& p_input, const Vector2& p_point, bool p_extend) {
	label->layout->flush_input_paint();
	const auto input = label->layout->text_control(p_input);
	const auto local = label->layout->input_local_point(*p_input.ptr(), p_point);
	if (input && local) {
		const int64_t old_start = input->editor.selection_start();
		const int64_t old_end = input->editor.selection_end();
		const auto mode = input->user_selection_mode();
		if (mode == litehtml::css_user_select::none) return;
		if (mode == litehtml::css_user_select::all) input->editor.set_selection(0, input->editor.get_value().length());
		else input->editor.hit_caret(*local, p_extend);
		if (old_start != input->editor.selection_start() || old_end != input->editor.selection_end()) {
			label->dispatch_control_event(StringName("select"), p_input);
			label->layout->get_dom().queue_selectionchange();
			label->layout->get_dom().dispatch_event(p_input, dockit::make_dom_event(StringName("selectionchange"), true, false));
		}
		restart_caret_blink();
		label->layout->repaint_input();
		queue_redraw();
	}
}

void HTMLLabelCanvas::change_input(const Ref<HTMLElement>& p_input, bool p_update_suggestions,
	const StringName& p_input_type, const String& p_data, bool p_is_composing) {
	if (const auto input = label->layout->text_control(p_input)) {
		label->layout->update_input(*input);
		queue_redraw();
		if (p_update_suggestions && focused_element == p_input && input->get_attr("list")) label->open_suggestion_menu(p_input);
		label->dispatch_input_event(StringName("input"), p_input, p_input_type, p_data, p_is_composing);
		label->emit_signal(input->editor.is_multiline() ? "textarea_changed" : "input_changed", p_input);
	}
}

void HTMLLabelCanvas::commit_input(const Ref<HTMLElement>& p_input, bool p_deferred) {
	const auto input = label->layout->text_control(p_input);
	if (!input || !input->editor.take_user_change()) return;
	const StringName signal = input->editor.is_multiline() ? "textarea_committed" : "input_committed";
	label->dispatch_control_event(StringName("change"), p_input);
	if (p_deferred) {
		const ObjectID id(label->get_instance_id());
		callable_mp_static(+[](uint64_t p_id, const Ref<HTMLElement>& element, uint64_t p_adoption_version,
			uint64_t p_reset_version, const StringName& p_signal) {
			auto* owner = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(ObjectID(p_id)));
			if (owner && element->adoption_version == p_adoption_version) {
				const auto input = owner->layout->text_control(element);
				if (input && input->editor.get_reset_version() == p_reset_version) owner->emit_signal(p_signal, element);
			}
		}).call_deferred(uint64_t(id), p_input, p_input->adoption_version, input->editor.get_reset_version(), signal);
	} else {
		label->emit_signal(signal, p_input);
	}
}

void HTMLLabelCanvas::toggle_checkbox(const Ref<HTMLInputElement>& p_input) {
	const auto input = p_input.is_valid() ? p_input->get_input() : nullptr;
	if (!input || !input->is_toggle() || dockit::forms::is_disabled(*input)) return;
	if (input->type() == HTMLInputNode::Type::CHECKBOX) input->set_indeterminate(false);
	if (!input->set_checked(input->type() == HTMLInputNode::Type::RADIO || !input->is_checked())) return;
	const ObjectID id(label->get_instance_id());
	const uint64_t reset_version = input->editor.get_reset_version();
	change_input(p_input);
	if (auto* owner = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(id)); owner && owner->layout->text_control(p_input) == input &&
		input->editor.get_reset_version() == reset_version) {
		owner->dispatch_control_event(StringName("change"), p_input);
		owner->emit_signal("input_committed", p_input);
	}
}

bool HTMLLabelCanvas::move_radio(const Ref<HTMLInputElement>& p_input, int p_direction) {
	const auto input = p_input->get_input();
	if (!input) return false;
	std::vector<Ref<HTMLInputElement>> available;
	for (const auto& node : dockit::forms::radio_group(*input)) {
		const Ref<HTMLInputElement> peer = label->layout->get_dom().make_element(node);
		if (label->layout->is_focusable(peer)) available.push_back(peer);
	}
	const auto current = std::find(available.begin(), available.end(), p_input);
	if (current == available.end()) return false;
	const auto count = static_cast<ptrdiff_t>(available.size());
	const Ref<HTMLInputElement> next = available[(std::distance(available.begin(), current) + count + p_direction) % count];
	set_focused_element(next, true);
	toggle_checkbox(next);
	return true;
}

void HTMLLabelCanvas::move_range(const Ref<HTMLInputElement>& p_input, const Vector2& p_point, bool p_commit) {
	if (p_input.is_null() || !label->layout->is_focusable(p_input)) return;
	const auto node = p_input->get_input();
	const auto local = label->layout->input_local_point(*p_input.ptr(), p_point);
	if (!node || node->type() != HTMLInputNode::Type::RANGE || !local) return;
	const ObjectID id(label->get_instance_id());
	if (node->set_range_point(*local)) change_input(p_input);
	if (p_commit) {
		if (auto* owner = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(id))) owner->canvas->commit_input(p_input);
	}
}

void HTMLLabelCanvas::remember_interaction_targets() {
	std::map<uint64_t, InteractionTarget> current;
	const auto remember = [&](const Ref<HTMLElement>& element) {
		if (element.is_null()) return;
		const uint64_t id = element->get_instance_id();
		const auto previous = interaction_targets.find(id);
		const Ref<HTMLInputElement> input = element;
		if (previous != interaction_targets.end()) current.emplace(id, previous->second);
		else current.emplace(id, InteractionTarget{element, element->layout_id, element->parse_version,
			element->adoption_version, element->get_mutation_revision(), input.is_valid() ? input->get_type() : String()});
	};
	for (const Ref<HTMLElement>& element : {dragging_input, Ref<HTMLElement>(dragging_range),
		Ref<HTMLElement>(pressed_checkbox), Ref<HTMLElement>(pressed_input_decoration), pressed_button,
		pressed_element, pressed_form_target, focused_element, input_menu_target, drag_source,
		Ref<HTMLElement>(pressed_audio), Ref<HTMLElement>(pressed_video)}) remember(element);
	if (box_drag) remember(box_drag->element);
	if (scrollbar_drag) remember(label->layout->get_dom().make_element(scrollbar_drag->element.lock()));
	for (const auto& [key, touch] : active_touch_pointers) remember(touch.target);
	remember(label->open_select);
	remember(Ref<HTMLElement>(label->picker_input));
	interaction_targets.swap(current);
}

void HTMLLabelCanvas::capture_selection_origin() {
	selection_origin_ranges = {};
	const auto save = [&](int slot, int64_t from, int64_t to) {
		const auto start = label->layout->text_boundary_at(from);
		const auto end = label->layout->text_boundary_at(to);
		if (start.node.is_null() || end.node.is_null()) return;
		auto range = label->layout->create_range();
		if (range.is_null() || range->set_start(start.node, start.offset) != OK ||
			range->set_end(end.node, end.offset) != OK) return;
		selection_origin_ranges[slot] = range;
	};
	save(0, selection_origin.offset, selection_origin.offset);
	if (selection_origin.scope.x < selection_origin.scope.y) save(1, selection_origin.scope.x, selection_origin.scope.y);
	if (selection_origin.enclosing_all.x < selection_origin.enclosing_all.y)
		save(2, selection_origin.enclosing_all.x, selection_origin.enclosing_all.y);
}

void HTMLLabelCanvas::restore_selection_origin() {
	const auto offsets = [&](const Ref<HTMLRange>& range) {
		return Vector2i(label->layout->document_text_offset({range->get_start_container(), range->get_start_offset()}),
			label->layout->document_text_offset({range->get_end_container(), range->get_end_offset()}, false));
	};
	if (selection_origin_ranges[0].is_valid() && selection_origin_ranges[0]->is_valid())
		selection_origin.offset = offsets(selection_origin_ranges[0]).x;
	if (selection_origin_ranges[1].is_valid() && selection_origin_ranges[1]->is_valid())
		selection_origin.scope = offsets(selection_origin_ranges[1]);
	if (selection_origin_ranges[2].is_valid() && selection_origin_ranges[2]->is_valid())
		selection_origin.enclosing_all = offsets(selection_origin_ranges[2]);
}

void HTMLLabelCanvas::reconcile_interactions_after_edit(bool p_after_layout) {
	if (!label || label->layout.is_null()) return;
	if (reconciling_interactions) { reconcile_again = true; return; }
	reconciling_interactions = true;
	do {
		reconcile_again = false;
		const auto connected = [&](const Ref<HTMLElement>& element) {
			if (element.is_null()) return false;
			const auto found = interaction_targets.find(uint64_t(element->get_instance_id()));
			if (found != interaction_targets.end()) {
				const auto& saved = found->second;
				if (saved.owner != element->layout_id || saved.generation != element->parse_version ||
					saved.adoption != element->adoption_version) return false;
			}
			return label->layout->is_element_connected(element);
		};
		const auto usable = [&](const Ref<HTMLElement>& element) {
			return connected(element) && label->layout->is_interaction_target_available(element, p_after_layout);
		};
		const auto same_control_kind = [&](const Ref<HTMLElement>& element) {
			const auto saved = interaction_targets.find(uint64_t(element->get_instance_id()));
			if (saved == interaction_targets.end() || saved->second.revision == element->get_mutation_revision()) return true;
			const Ref<HTMLInputElement> input = element;
			return input.is_null() || saved->second.input_type == input->get_type();
		};
		if (dragging_range.is_valid()) {
			const auto input = dragging_range->get_input();
			if (!usable(dragging_range) || !input || input->type() != HTMLInputNode::Type::RANGE) dragging_range.unref();
		}
		if (dragging_input.is_valid()) {
			const auto input = label->layout->text_control(dragging_input);
			if (!usable(dragging_input) || !same_control_kind(dragging_input) || !input || !input->is_text_control()) dragging_input.unref();
		}
		if (pressed_checkbox.is_valid()) {
			const auto input = pressed_checkbox->get_input();
			if (!usable(pressed_checkbox) || !same_control_kind(pressed_checkbox) || !input || !input->is_toggle()) {
				const auto checkbox = pressed_checkbox;
				pressed_checkbox.unref();
				set_element_pseudo(checkbox, "active", false);
			}
		}
		if (pressed_input_decoration.is_valid() && (!usable(pressed_input_decoration) || !same_control_kind(pressed_input_decoration)))
			pressed_input_decoration.unref();
		if (pressed_button.is_valid() && (!usable(pressed_button) ||
			(p_after_layout && !label->layout->is_enabled_button(pressed_button)))) cancel_button_press();
		if (pressed_audio.is_valid() && !usable(pressed_audio)) {
			pressed_audio.unref();
			pressed_audio_part = dockit::HTMLAudioPart::none;
		}
		if (pressed_video.is_valid() && !usable(pressed_video)) cancel_video_press();
		if (scrollbar_drag) {
			const auto target = label->layout->get_dom().make_element(scrollbar_drag->element.lock());
			if (!usable(target) || (p_after_layout && !label->layout->get_element_scroll_bounds(target))) {
				scrollbar_drag.reset();
				label->layout->set_scrollbar_pressed(std::nullopt);
			}
		}
		if (box_drag && (!usable(box_drag->element) ||
			(p_after_layout && !label->layout->get_element_scroll_bounds(box_drag->element)))) {
			box_drag.reset();
			resize_pending.reset();
			label->layout->set_scrollbar_pressed(std::nullopt);
		}
		if (pressed_element.is_valid() && !usable(pressed_element)) {
			pressed_element.unref();
			pressed_link = 0;
			pressed_blank = false;
		}
		if (pressed_form_target.is_valid() && !usable(pressed_form_target)) pressed_form_target.unref();
		if (drag_source.is_valid() && !connected(drag_source) && !native_drag_started) {
			selection_drag_attempt = element_drag_attempt = false;
			drag_source.unref();
		}
		if (focused_element.is_valid()) {
			if (!usable(focused_element) || (p_after_layout && !label->layout->is_focusable(focused_element))) {
				set_focused_element({});
			} else if (const auto input = label->layout->text_control(focused_element);
				input && input->editor.get_reset_version() != focused_reset_version) {
				focused_reset_version = input->editor.get_reset_version();
				close_ime();
				update_ime();
			}
		}
		if (input_menu_target.is_valid() && (!usable(input_menu_target) || !same_control_kind(input_menu_target)))
			close_input_menu();
		if (label->open_select.is_valid() && !usable(label->open_select)) label->close_select_menu();
		if (label->picker_input.is_valid() && (!usable(label->picker_input) || !label->input_picker_valid()))
			label->close_input_picker();
		std::vector<TouchKey> removed;
		for (const auto& [key, touch] : active_touch_pointers)
			if (touch.target.is_valid() && !connected(touch.target)) removed.push_back(key);
		for (const auto& key : removed) cancel_touch_pointer(key.first, key.second);
		if (p_after_layout && selecting) restore_selection_origin();
		remember_interaction_targets();
		for (auto& [id, target] : interaction_targets) {
			target.revision = target.element->get_mutation_revision();
			const Ref<HTMLInputElement> input = target.element;
			target.input_type = input.is_valid() ? input->get_type() : String();
		}
		if (!selecting && dragging_input.is_null()) set_process_internal(false);
	} while (reconcile_again);
	reconciling_interactions = false;
}

bool HTMLLabelCanvas::handle_input_decoration(const Ref<HTMLInputElement>& p_input, const InputEventMouseButton& p_event,
	const Vector2& p_point, bool p_accepts_event) {
	const auto node = p_input.is_valid() ? p_input->get_input() : nullptr;
	const auto local = node ? label->layout->input_local_point(*p_input.ptr(), p_point) : std::nullopt;
	const int part = local && node->is_editable() && label->layout->is_focusable(p_input) ? node->decoration_at(*local) : 0;
	if (p_event.is_pressed()) {
		pressed_input_decoration.unref();
		if (!part) return false;
		cancel_press();
		pressed_input_decoration = p_input;
		pressed_input_part = part;
		if (p_accepts_event) accept_event();
		return true;
	}
	const auto pressed = std::exchange(pressed_input_decoration, {});
	if (pressed.is_null()) return false;
	if (p_accepts_event) accept_event();
	if (p_event.is_canceled() || !part || pressed != p_input || part != pressed_input_part) return true;
	const ObjectID id(label->get_instance_id());
	if (!label->focus_element(p_input) || !ObjectDB::get_instance(id)) return true;
	if (!label->layout->is_element_connected(p_input) || p_input->get_input() != node) return true;
	if (node->time_kind() != dockit::input_time::Kind::none) {
		label->open_input_picker(p_input);
		return true;
	}
	bool changed = false;
	if (node->type() == HTMLInputNode::Type::NUMBER) node->step_number(part, changed);
	else if (node->type() == HTMLInputNode::Type::SEARCH) {
		node->editor.set_selection(0, node->editor.get_value().length());
		changed = node->editor.insert(String());
	}
	if (changed) {
		change_input(p_input, false);
		if (auto* owner = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(id))) owner->canvas->commit_input(p_input);
	}
	return true;
}

bool HTMLLabelCanvas::handle_input_key(const InputEventKey& p_key) {
	const Ref<HTMLElement> handle = focused_element;
	const auto input = label->layout->text_control(handle);
	if (!input || !label->layout->is_element_connected(handle)) return false;
	const Key key = p_key.get_keycode() != KEY_NONE ? p_key.get_keycode() : p_key.get_physical_keycode();
	if (const Ref<HTMLInputElement> field = handle; field.is_valid() && p_key.is_pressed() && !p_key.is_echo() &&
		!p_key.is_ctrl_pressed() && !p_key.is_meta_pressed() && !dockit::forms::is_disabled(*input)) {
		const auto native = field->get_input();
		const bool specialized = native->type() == HTMLInputNode::Type::FILE || native->type() == HTMLInputNode::Type::COLOR;
		if ((specialized && (key == KEY_SPACE || key == KEY_ENTER || key == KEY_KP_ENTER)) ||
			(p_key.is_alt_pressed() && key == KEY_DOWN && native->has_picker())) {
			label->accept_event();
			label->open_input_picker(field);
			return true;
		}
	}
	if (auto* numeric = dynamic_cast<HTMLInputNode*>(input.get()); numeric && p_key.is_pressed() &&
		!p_key.is_alt_pressed() && !p_key.is_ctrl_pressed() && !p_key.is_meta_pressed() && !input->editor.has_preedit() &&
		!dockit::forms::is_disabled(*input)) {
		const bool range = numeric->type() == HTMLInputNode::Type::RANGE;
		const bool number = numeric->type() == HTMLInputNode::Type::NUMBER;
		const bool vertical = key == KEY_UP || key == KEY_DOWN;
		const bool horizontal = key == KEY_LEFT || key == KEY_RIGHT;
		const bool endpoint = key == KEY_HOME || key == KEY_END;
		if (numeric->time_kind() != dockit::input_time::Kind::none && input->is_editable() && !p_key.is_shift_pressed() &&
			(vertical || horizontal) && numeric->edit_time_segment(key == KEY_LEFT || key == KEY_DOWN ? -1 : 1, vertical)) {
			label->accept_event();
			if (vertical) change_input(handle, false);
			else {
				label->layout->repaint_input();
				queue_redraw();
			}
			return true;
		}
		if ((range && (vertical || horizontal || endpoint)) || (number && vertical && input->is_editable())) {
			int step = key == KEY_DOWN || key == KEY_LEFT ? -1 : 1;
			if (horizontal && input->css().is_right_to_left()) step = -step;
			const ObjectID id(label->get_instance_id());
			label->accept_event();
			bool changed = false;
			numeric->step_number(step, changed, endpoint ? std::optional<bool>(key == KEY_END) : std::nullopt);
			if (changed) {
				change_input(handle);
				if (range) if (auto* owner = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(id))) owner->canvas->commit_input(handle);
			}
			return true;
		}
	}
	if (const auto* toggle = dynamic_cast<const HTMLInputNode*>(input.get()); toggle && toggle->is_toggle()) {
		if (!label->layout->is_focusable(handle)) return false;
		if (p_key.is_pressed() && toggle->type() == HTMLInputNode::Type::RADIO && !p_key.is_alt_pressed() &&
			!p_key.is_meta_pressed() && !p_key.is_ctrl_pressed() &&
			(key == KEY_LEFT || key == KEY_UP || key == KEY_RIGHT || key == KEY_DOWN)) {
			label->accept_event();
			return move_radio(handle, key == KEY_LEFT || key == KEY_UP ? -1 : 1);
		}
		if (key != KEY_SPACE || p_key.is_alt_pressed() || p_key.is_meta_pressed() || p_key.is_ctrl_pressed()) return false;
		label->accept_event();
		if (p_key.is_echo()) return true;
		if (p_key.is_pressed()) {
			checkbox_key_pressed = true;
			set_element_pseudo(handle, "active", true);
		} else if (checkbox_key_pressed) {
			cancel_key_press();
			if (!p_key.is_canceled()) toggle_checkbox(handle);
		}
		return true;
	}
	// One IME result can arrive as several key events. The first character can
	// invalidate the layout through :placeholder-shown while the field still has focus.
	if (!input->is_text_control() || !p_key.is_pressed() || dockit::forms::is_disabled(*input) ||
		input->get_attr("hidden")) return false;
	restart_caret_blink();
	label->layout->flush_input_paint();
	auto& editor = input->editor;

	if (label->context_menu_enabled && !p_key.is_echo() && p_key.is_action("ui_menu", true)) {
		const auto caret = label->layout->input_caret_point(*handle.ptr());
		if (open_input_menu(handle, caret.value_or(label->get_visible_rect().position), false)) {
			label->accept_event();
			return true;
		}
	}

	const bool enter = key == KEY_ENTER || key == KEY_KP_ENTER;
	const bool editable = input->is_editable();
	const char32_t unicode = p_key.get_unicode();
	const bool alt_gr_text = p_key.is_alt_pressed() && p_key.is_ctrl_pressed();
	const bool inserts_text = unicode >= 0x20 && unicode != 0x7f && !p_key.is_meta_pressed() &&
		(!p_key.is_ctrl_pressed() || alt_gr_text);
	const bool was_composing = editor.has_preedit();
	const String composition_data = editor.get_preedit();
	const int64_t old_selection_start = editor.selection_start();
	const int64_t old_selection_end = editor.selection_end();
	if (input->editor.has_preedit() && !inserts_text) {
		if (key == KEY_ESCAPE) {
			label->accept_event();
			close_ime();
			update_ime();
			return true;
		}
		if (enter || key == KEY_BACKSPACE || key == KEY_DELETE || key == KEY_LEFT || key == KEY_RIGHT ||
			key == KEY_UP || key == KEY_DOWN || key == KEY_HOME || key == KEY_END ||
			p_key.is_ctrl_pressed() || p_key.is_meta_pressed()) {
			label->accept_event();
			return true;
		}
	}

	if (!inserts_text) editor.finish_ime_edit();

	bool changed = false;
	StringName edit_type;
	String edit_data;
	const auto selection_mode = input->user_selection_mode();
	const bool extend = p_key.is_shift_pressed() && selection_mode != litehtml::css_user_select::none;
	const char* maxlength = edit_maxlength(*input);
	if (enter && !inserts_text && !editor.is_multiline()) {
		label->accept_event();
		if (!p_key.is_echo()) {
			const ObjectID id(label->get_instance_id());
			commit_input(handle);
			if (auto* owner = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(id))) owner->submit_from_text(handle);
		}
		return true;
	}
	for (int command = 0; command < static_cast<int>(std::size(INPUT_MENU_ITEMS)); ++command) {
		if (p_key.is_action(INPUT_MENU_ITEMS[command].action, true)) {
			label->accept_event();
			run_input_command(handle, command);
			return true;
		}
	}
	if (inserts_text) {
		// IME character events can retain the key code of the confirmation key.
		if (editable) {
			edit_type = StringName("insertText");
			edit_data = String::chr(unicode);
			if (label->dispatch_input_event(StringName("beforeinput"), handle, edit_type, edit_data, was_composing)) {
				changed = editor.insert_character(unicode, maxlength, Engine::get_singleton()->get_process_frames(),
					key == KEY_NONE || enter);
			}
		}
	} else if (key == KEY_LEFT || key == KEY_RIGHT) {
		input->editor.move_caret(key == KEY_LEFT ? -1 : 1, extend);
	} else if (key == KEY_HOME || key == KEY_END) {
		editor.move_to_edge(key == KEY_END, p_key.is_ctrl_pressed() || p_key.is_meta_pressed(), extend);
	} else if (editor.is_multiline() && (key == KEY_UP || key == KEY_DOWN)) {
		editor.move_vertical(key == KEY_UP ? -1 : 1, extend);
	} else if (enter) {
		if (editable) {
			edit_type = StringName("insertLineBreak");
			if (label->dispatch_input_event(StringName("beforeinput"), handle, edit_type, {}, false))
				changed = editor.insert("\n", maxlength);
		}
	} else if (key == KEY_BACKSPACE || key == KEY_DELETE) {
		if (editable) {
			edit_type = StringName(key == KEY_BACKSPACE ? "deleteContentBackward" : "deleteContentForward");
			if (label->dispatch_input_event(StringName("beforeinput"), handle, edit_type, {}, false))
				changed = input->editor.erase(key == KEY_BACKSPACE ? -1 : 1);
		}
	} else {
		return false;
	}
	if (extend && selection_mode == litehtml::css_user_select::all && editor.selection_start() != editor.selection_end()) {
		editor.set_selection(0, editor.get_value().length());
	}
	if (old_selection_start != editor.selection_start() || old_selection_end != editor.selection_end()) {
		label->dispatch_control_event(StringName("select"), handle);
		label->layout->get_dom().queue_selectionchange();
		label->layout->get_dom().dispatch_event(handle, dockit::make_dom_event(StringName("selectionchange"), true, false));
	}
	label->accept_event();
	if (changed && was_composing && !editor.has_preedit())
		label->dispatch_composition_event(StringName("compositionend"), handle, composition_data);
	if (changed) change_input(handle, true, edit_type, edit_data, editor.has_preedit());
	else {
		label->layout->repaint_input();
		queue_redraw();
	}
	return true;
}

bool HTMLLabelCanvas::handle_editable_key(const InputEventKey& p_key) {
	if (!p_key.is_pressed() || !label->layout->is_element_connected(focused_element) ||
		!focused_element->is_content_editable()) return false;
	const Key key = p_key.get_keycode() != KEY_NONE ? p_key.get_keycode() : p_key.get_physical_keycode();
	const bool command = p_key.is_ctrl_pressed() || p_key.is_meta_pressed();
	if (command && !p_key.is_alt_pressed()) {
		if (key == KEY_B || key == KEY_I || key == KEY_U) {
			label->accept_event();
			label->execute_edit_command(key == KEY_B ? "bold" : key == KEY_I ? "italic" : "underline");
			return true;
		}
		if (p_key.is_action("ui_text_select_all", true)) {
			label->select_all();
			label->accept_event();
			return true;
		}
	}
	if (p_key.is_echo() && (key == KEY_BACKSPACE || key == KEY_DELETE || key == KEY_ENTER || key == KEY_KP_ENTER)) return true;
	String input;
	StringName input_type;
	if ((key == KEY_ENTER || key == KEY_KP_ENTER) && !command && !p_key.is_alt_pressed()) {
		input_type = "insertLineBreak";
	} else if (key == KEY_BACKSPACE || key == KEY_DELETE) {
		input_type = key == KEY_BACKSPACE ? "deleteContentBackward" : "deleteContentForward";
	} else if (command && key == KEY_V) {
		input = DisplayServer::get_singleton()->clipboard_get();
		input_type = "insertFromPaste";
		if (input.is_empty()) { label->accept_event(); return true; }
	} else if (!command && !p_key.is_alt_pressed()) {
		const char32_t codepoint = p_key.get_unicode();
		if (codepoint >= 0x20 && codepoint != 0x7f) {
			input = String::chr(codepoint);
			input_type = "insertText";
		}
	}
	if (input_type.is_empty()) return false;
	label->accept_event();
	if (!label->dispatch_input_event(StringName("beforeinput"), focused_element, input_type, input, false)) return true;
	Ref<HTMLSelection> selection = label->layout->get_selection();
	Ref<HTMLRange> range;
	if (selection.is_valid() && selection->get_range_count() > 0) range = selection->get_range_at(0);
	if (range.is_null()) {
		const Vector2i offsets = label->get_selection_range();
		const auto anchor = label->layout->text_boundary_at(offsets.x);
		const auto focus = label->layout->text_boundary_at(offsets.y);
		range = label->layout->create_range();
		if (range.is_null() || range->set_start(anchor.node, anchor.offset) != OK || range->set_end(focus.node, focus.offset) != OK) return true;
		selection->remove_all_ranges();
		selection->add_range(range);
	}
	const bool deleting = input_type == StringName("deleteContentBackward") || input_type == StringName("deleteContentForward");
	if (deleting && range->is_collapsed()) {
		const Vector2i offsets = label->get_selection_range();
		const int64_t from = std::min(offsets.x, offsets.y);
		const int64_t to = std::max(offsets.x, offsets.y);
		const int64_t total = label->layout->get_plain_text().length();
		if (from == to && ((input_type == StringName("deleteContentBackward") && from == 0) ||
			(input_type == StringName("deleteContentForward") && to >= total))) return true;
		const int64_t first = from == to && input_type == StringName("deleteContentBackward") ? from - 1 : from;
		const int64_t last = from == to && input_type == StringName("deleteContentForward") ? to + 1 : to;
		const auto start = label->layout->text_boundary_at(first);
		const auto end = label->layout->text_boundary_at(last);
		if (range->set_start(start.node, start.offset) != OK || range->set_end(end.node, end.offset) != OK) return true;
	}
	if (input_type == StringName("insertLineBreak")) {
		if (range->delete_contents() != OK) return true;
		Ref<HTMLElement> line_break = label->layout->create_element("br");
		if (line_break.is_null() || range->insert_node(line_break) != OK) return true;
		range->collapse(false);
		selection->collapse(range->get_end_container(), range->get_end_offset());
	} else if (deleting) {
		if (range->delete_contents() != OK) return true;
		range->collapse(true);
		selection->collapse(range->get_start_container(), range->get_start_offset());
	} else {
		if (range->delete_contents() != OK) return true;
		Ref<HTMLTextNode> text = label->layout->create_text_node(input);
		if (text.is_null() || range->insert_node(text) != OK) return true;
		selection->collapse(text, input.length());
	}
	label->dispatch_input_event(StringName("input"), focused_element, input_type, input, false);
	label->queue_interaction();
	return true;
}

bool HTMLLabelCanvas::open_input_menu(const Ref<HTMLElement>& p_element, const Vector2& p_position, bool p_pointer) {
	const auto input = label->layout->text_control(p_element);
	if (!input || !input->is_text_control() || !label->layout->is_focusable(p_element)) return false;
	const Rect2 visible = label->get_visible_rect();
	if (!visible.has_area()) return false;

	cancel_press();
	const Ref<HTMLElement> active_element = focused_element;
	if (const auto active = label->layout->text_control(active_element);
		active && active->is_editable() && active->editor.has_preedit() && label->layout->is_element_connected(active_element)) {
		// Keep the marked text before closing the native composition, as LineEdit does.
		const String marked = active->editor.get_preedit();
		close_ime();
		if (active->editor.insert(marked, edit_maxlength(*active))) change_input(active_element);
	}

	label->grab_focus();
	if (!label->context_menu_enabled || !label->layout->is_element_connected(p_element) ||
		label->layout->text_control(p_element) != input || !input->is_text_control() ||
		dockit::forms::is_disabled(*input) || input->get_attr("hidden")) return false;
	if (focused_element != p_element) set_focused_element(p_element);
	auto& editor = input->editor;
	const bool has_selection = editor.selection_start() != editor.selection_end();
	if (p_pointer && !has_selection && input->is_editable()) set_input_caret(p_element, p_position, false);

	if (!input_menu) {
		input_menu = memnew(Control);
		input_menu->set_name("_HTMLInputMenu");
		input_menu->set_z_index(2);
		input_menu->set_clip_contents(false);
		input_menu->connect("draw", callable_mp(this, &HTMLLabelCanvas::draw_input_menu));
		input_menu->connect("gui_input", callable_mp(this, &HTMLLabelCanvas::input_menu_event));
		label->add_child(input_menu, false, INTERNAL_MODE_BACK);
	}
	if (input_menu_font.is_null()) {
		const real_t base_scale = label->get_theme_default_base_scale();
		input_menu_font_size = std::max(1, static_cast<int>(std::round(13.0f * (base_scale > 0.0f ? base_scale : 1.0f))));
		if (OS::get_singleton()->get_name() == "Windows") {
			Ref<SystemFont> system_font;
			system_font.instantiate();
			PackedStringArray names;
			names.append("Segoe UI");
			system_font->set_font_names(names);
			system_font->set_allow_system_fallback(true);
			if (system_font->get_face_count() > 0) input_menu_font = system_font;
		}
		if (input_menu_font.is_null()) input_menu_font = ThemeDB::get_singleton()->get_fallback_font();
	}
	const bool editable = input->is_editable();
	const auto* field = dynamic_cast<const HTMLInputNode*>(input.get());
	const bool password = field && field->is_password();
	const bool disabled[] = {
		!editable || !editor.can_undo(),
		!editable || !editor.can_redo(),
		password || !editable || !has_selection,
		password || !has_selection,
		!editable || DisplayServer::get_singleton()->clipboard_get().is_empty(),
		editor.get_value().is_empty() || input->user_selection_mode() == litehtml::css_user_select::none,
	};
	real_t width = INPUT_MENU_MIN_WIDTH;
	for (size_t command = 0; command < input_menu_rows.size(); ++command) {
		const auto& item = INPUT_MENU_ITEMS[command];
		auto& row = input_menu_rows[command];
		row.caption = shape_input_menu_text(dockit::html_ui_translate(String::utf8(item.label), "input_context_menu"),
			input_menu_font, input_menu_font_size);
		const Key accelerator = input_menu_accelerator(item.action);
		row.shortcut = shape_input_menu_text(accelerator == KEY_NONE ? String() :
			OS::get_singleton()->get_keycode_string(accelerator), input_menu_font, input_menu_font_size);
		row.enabled = !disabled[command];
		const real_t shortcut_width = row.shortcut.is_valid() ? row.shortcut.width() : 0.0f;
		width = std::max(width, real_t(row.caption.width() + shortcut_width + 64));
	}

	input_menu_hover = input_menu_press = -1;
	input_menu_target = p_element;
	close_ime();
	const auto space = label->get_global_transform_with_canvas().affine_inverse().xform(label->get_viewport_rect()).size;
	const bool dark = DisplayServer::get_singleton()->is_dark_mode_supported() && DisplayServer::get_singleton()->is_dark_mode();
	const Color panel_color = dark ? Color::html("#292a2d") : Color::html("#ffffff");
	const Color border_color = dark ? Color::html("#5f6368") : Color::html("#dadce0");
	input_menu_panel_style.instantiate();
	input_menu_panel_style->set_bg_color(panel_color);
	input_menu_panel_style->set_border_width_all(1);
	input_menu_panel_style->set_border_color(border_color);
	input_menu_panel_style->set_corner_radius_all(8);
	input_menu_panel_style->set_shadow_color(Color(0.0f, 0.0f, 0.0f, dark ? 0.34f : 0.16f));
	input_menu_panel_style->set_shadow_size(8);
	input_menu_panel_style->set_shadow_offset(Vector2(0.0f, 2.0f));
	input_menu_panel_style->set_anti_aliased(true);
	input_menu_panel_style->set_aa_size(1.0f);
	input_menu_hover_style.instantiate();
	input_menu_hover_style->set_bg_color(dark ? Color::html("#3c4043") : Color::html("#f1f3f4"));
	input_menu_hover_style->set_corner_radius_all(4);
	input_menu_hover_style->set_anti_aliased(true);
	input_menu_hover_style->set_aa_size(1.0f);
	const real_t line_height = input_menu_font.is_valid() ? input_menu_font->get_height(input_menu_font_size) : 14.0f;
	const real_t preferred_row_height = std::max(real_t(26), line_height + 12.0f);
	const real_t available_row_height = std::max(INPUT_MENU_MIN_ROW_HEIGHT,
		(space.y - INPUT_MENU_OUTER_PADDING * 2 - INPUT_MENU_GROUP_GAP * INPUT_MENU_SEPARATOR_COUNT) / input_menu_rows.size());
	input_menu_row_height = std::min(preferred_row_height, available_row_height);
	const real_t menu_height = INPUT_MENU_OUTER_PADDING * 2 + input_menu_row_height * input_menu_rows.size() +
		INPUT_MENU_GROUP_GAP * INPUT_MENU_SEPARATOR_COUNT;
	input_menu->set_size(Vector2(std::min(width, space.x), menu_height));
	const Vector2 anchor = label->get_global_transform_with_canvas().affine_inverse().xform(
		get_global_transform_with_canvas().xform(p_position.clamp(visible.position, visible.get_end())));
	label->place_overlay(*input_menu, anchor);
	input_menu->show();
	input_menu->queue_redraw();
	return true;
}

void HTMLLabelCanvas::close_input_menu() {
	if (input_menu_target.is_null()) return;
	input_menu_target.unref();
	input_menu->hide();
	callable_mp(this, &HTMLLabelCanvas::update_ime).call_deferred();
}

Rect2 HTMLLabelCanvas::input_menu_row_rect(int p_command) const {
	const int separator_count = int(p_command >= INPUT_CUT) + int(p_command >= INPUT_SELECT_ALL);
	const real_t separator_space = INPUT_MENU_GROUP_GAP * separator_count;
	return Rect2(INPUT_MENU_OUTER_PADDING,
		INPUT_MENU_OUTER_PADDING + separator_space + p_command * input_menu_row_height,
		input_menu->get_size().x - INPUT_MENU_OUTER_PADDING * 2, input_menu_row_height);
}

void HTMLLabelCanvas::draw_input_menu() {
	const Rect2 bounds(Vector2(), input_menu->get_size());
	const bool dark = DisplayServer::get_singleton()->is_dark_mode_supported() && DisplayServer::get_singleton()->is_dark_mode();
	const Color text_color = dark ? Color::html("#e8eaed") : Color::html("#202124");
	const Color secondary_color = dark ? Color::html("#9aa0a6") : Color::html("#5f6368");
	const Color disabled_color = dark ? Color::html("#80868b") : Color::html("#9aa0a6");
	const Color separator_color = dark ? Color::html("#5f6368") : Color::html("#dadce0");
	input_menu->draw_style_box(input_menu_panel_style, bounds);
	const auto server = TextServerManager::get_singleton()->get_primary_interface();
	for (size_t command = 0; command < input_menu_rows.size(); ++command) {
		const auto& row = input_menu_rows[command];
		const Rect2 area = input_menu_row_rect(static_cast<int>(command));
		if (command == INPUT_CUT || command == INPUT_SELECT_ALL) {
			const real_t separator_y = area.position.y - INPUT_MENU_GROUP_GAP * 0.5f;
			input_menu->draw_line(Vector2(INPUT_MENU_ITEM_PADDING_X, separator_y),
				Vector2(bounds.size.x - INPUT_MENU_ITEM_PADDING_X, separator_y), separator_color);
		}
		if (row.enabled && int(command) == input_menu_hover) input_menu->draw_style_box(input_menu_hover_style, area.grow(-2));
		const auto draw = [&](const dockit::HTMLTextLayout& p_line, real_t p_x, real_t p_width, const Color& p_color) {
			if (server.is_null() || !p_line.is_valid() || p_width <= 0) return;
			const real_t baseline = area.get_center().y + (p_line.line_ascent() - p_line.line_descent()) / 2;
			server->shaped_text_draw(p_line.get_rid(), input_menu->get_canvas_item(), Vector2(p_x, baseline), 0, p_width, p_color);
		};
		const real_t shortcut_width = row.shortcut.is_valid() ? row.shortcut.width() : 0.0f;
		const real_t shortcut_x = area.get_end().x - INPUT_MENU_ITEM_PADDING_X - shortcut_width;
		draw(row.caption, area.position.x + INPUT_MENU_ITEM_PADDING_X,
			shortcut_x - area.position.x - INPUT_MENU_ITEM_PADDING_X, row.enabled ? text_color : disabled_color);
		draw(row.shortcut, shortcut_x, shortcut_width + 1, row.enabled ? secondary_color : disabled_color);
	}
}

void HTMLLabelCanvas::input_menu_event(const Ref<InputEvent>& p_event) {
	const Ref<InputEventMouseButton> button = p_event;
	const Ref<InputEventMouseMotion> motion = p_event;
	if (button.is_null() && motion.is_null()) return;
	const Vector2 point = button.is_valid() ? button->get_position() : motion->get_position();
	int hit = -1;
	for (size_t command = 0; command < input_menu_rows.size(); ++command) {
		if (input_menu_rows[command].enabled && input_menu_row_rect(int(command)).has_point(point)) hit = int(command);
	}
	input_menu_hover = hit;
	input_menu->queue_redraw();
	if (button.is_valid() && button->get_button_index() == MOUSE_BUTTON_LEFT) {
		if (button->is_pressed()) input_menu_press = hit;
		else if (hit >= 0 && std::exchange(input_menu_press, -1) == hit && !button->is_canceled()) on_input_menu_selected(hit);
	}
	input_menu->accept_event();
}

bool HTMLLabelCanvas::handle_input_menu_key(const InputEventKey& p_key) {
	if (input_menu_target.is_null() || !p_key.is_pressed()) return false;
	if (p_key.is_action("ui_cancel") || p_key.is_action("ui_menu")) {
		close_input_menu();
		return true;
	}
	if (p_key.is_action("ui_accept")) {
		if (input_menu_hover >= 0) on_input_menu_selected(input_menu_hover);
		return true;
	}
	const bool up = p_key.is_action("ui_up");
	const int count = static_cast<int>(input_menu_rows.size());
	if (up || p_key.is_action("ui_down")) {
		int next = input_menu_hover < 0 ? (up ? 0 : count - 1) : input_menu_hover;
		for (int i = 0; i < count; ++i) {
			next = (next + (up ? count - 1 : 1)) % count;
			if (input_menu_rows[next].enabled) {
				input_menu_hover = next;
				input_menu->queue_redraw();
				break;
			}
		}
		return true;
	}
	if (p_key.is_action("ui_focus_next", true) || p_key.is_action("ui_focus_prev", true)) {
		close_input_menu();
		return false;
	}
	for (size_t i = 0; i < input_menu_rows.size(); ++i) {
		if (p_key.is_action(INPUT_MENU_ITEMS[i].action, true)) {
			if (input_menu_rows[i].enabled) on_input_menu_selected(int(i));
			return true;
		}
	}
	return true;
}

void HTMLLabelCanvas::on_input_menu_selected(int p_command) {
	const Ref<HTMLElement> target = input_menu_target;
	close_input_menu();
	if (target.is_valid() && focused_element == target) run_input_command(target, p_command);
}

void HTMLLabelCanvas::run_input_command(const Ref<HTMLElement>& p_element, int p_command) {
	const auto input = label->layout->text_control(p_element);
	if (!input || !input->is_text_control() || !label->layout->is_element_connected(p_element) ||
		dockit::forms::is_disabled(*input) || input->get_attr("hidden")) return;

	auto& editor = input->editor;
	const bool editable = input->is_editable() && !editor.has_preedit();
	const char* maxlength = edit_maxlength(*input);
	bool changed = false;
	StringName edit_type;
	String edit_data;
	switch (p_command) {
		case INPUT_UNDO:
			if (editable && label->dispatch_input_event(StringName("beforeinput"), p_element,
				StringName("historyUndo"), {}, false)) {
				edit_type = StringName("historyUndo");
				changed = editor.undo();
			}
			break;
		case INPUT_REDO:
			if (editable && label->dispatch_input_event(StringName("beforeinput"), p_element,
				StringName("historyRedo"), {}, false)) {
				edit_type = StringName("historyRedo");
				changed = editor.redo();
			}
			break;
		case INPUT_CUT:
		case INPUT_COPY: {
			const auto* field = dynamic_cast<const HTMLInputNode*>(input.get());
			if (field && field->is_password()) break;
			const int64_t start = editor.selection_start();
			const int64_t length = editor.selection_end() - start;
			if (length > 0 && (p_command == INPUT_COPY || editable)) {
				const String selected = editor.get_value().substr(start, length);
				Ref<HTMLDataTransfer> transfer;
				transfer.instantiate();
				transfer->set_text(selected);
				Ref<HTMLClipboardEvent> event;
				event.instantiate();
				event->set_type(p_command == INPUT_COPY ? StringName("copy") : StringName("cut"));
				event->set_bubbles(true);
				event->set_cancelable(true);
				event->set_composed(true);
				event->set_clipboard_data(transfer);
				event->mark_trusted();
				label->layout->get_dom().dispatch_event(p_element, event);
				const String clipboard = event->is_default_prevented() ? transfer->get_data("text/plain") : selected;
				DisplayServer::get_singleton()->clipboard_set(clipboard);
				if (p_command == INPUT_CUT && !event->is_default_prevented() &&
					label->dispatch_input_event(StringName("beforeinput"), p_element, StringName("deleteByCut"), {}, false)) {
					edit_type = StringName("deleteByCut");
					changed = editor.insert({});
				}
			}
			break;
		}
		case INPUT_PASTE:
			if (editable) {
				const String clipboard = DisplayServer::get_singleton()->clipboard_get();
				Ref<HTMLDataTransfer> transfer;
				transfer.instantiate();
				transfer->set_text(clipboard);
				transfer->set_read_only(true);
				Ref<HTMLClipboardEvent> event;
				event.instantiate();
				event->set_type(StringName("paste"));
				event->set_bubbles(true);
				event->set_cancelable(true);
				event->set_composed(true);
				event->set_clipboard_data(transfer);
				event->mark_trusted();
				label->layout->get_dom().dispatch_event(p_element, event);
				if (!event->is_default_prevented() && !clipboard.is_empty() &&
					label->dispatch_input_event(StringName("beforeinput"), p_element, StringName("insertFromPaste"), clipboard, false)) {
					edit_type = StringName("insertFromPaste");
					edit_data = clipboard;
					changed = editor.insert(clipboard, maxlength);
				}
			}
			break;
		case INPUT_SELECT_ALL:
			if (input->user_selection_mode() != litehtml::css_user_select::none) {
				const int64_t old_start = editor.selection_start();
				const int64_t old_end = editor.selection_end();
				editor.set_selection(0, editor.get_value().length());
				if (old_start != editor.selection_start() || old_end != editor.selection_end()) {
					label->dispatch_control_event(StringName("select"), p_element);
					label->layout->get_dom().queue_selectionchange();
				}
			}
			break;
		default:
			return;
	}
	restart_caret_blink();
	if (changed) change_input(p_element, true, edit_type, edit_data, false);
	else {
		label->layout->repaint_input();
		queue_redraw();
	}
}

void HTMLLabelCanvas::set_input_caret_visible(dockit::HTMLTextEditor& p_editor, bool p_visible) {
	if (p_editor.set_caret_visible(p_visible)) {
		label->layout->repaint_input();
		queue_redraw();
	}
}

void HTMLLabelCanvas::restart_caret_blink() {
	const auto input = label->layout->text_control(focused_element);
	if (!input || !input->is_text_control() || !is_inside_tree() || !caret_window_active || !label->has_focus()) return;

	set_input_caret_visible(input->editor, true);
	if (!label->caret_blink) {
		if (caret_blink_timer) caret_blink_timer->stop();
		return;
	}

	if (!caret_blink_timer) {
		caret_blink_timer = memnew(Timer);
		caret_blink_timer->set_ignore_time_scale(true);
		caret_blink_timer->connect("timeout", callable_mp(this, &HTMLLabelCanvas::blink_caret));
		add_child(caret_blink_timer, false, INTERNAL_MODE_BACK);
	}
	caret_blink_timer->start(label->caret_blink_interval);
}

void HTMLLabelCanvas::stop_caret_blink() {
	if (caret_blink_timer) caret_blink_timer->stop();
	if (const auto input = label->layout->text_control(focused_element)) {
		set_input_caret_visible(input->editor, false);
	}
}

void HTMLLabelCanvas::blink_caret() {
	const auto input = label->layout->text_control(focused_element);
	if (!input || !input->is_text_control() || !label->layout->is_element_connected(focused_element)) {
		stop_caret_blink();
		return;
	}
	set_input_caret_visible(input->editor, !input->editor.is_caret_visible());
}

void HTMLLabelCanvas::close_ime() {
	if (ime_window) {
		const auto window = *ime_window;
		ime_window.reset();
		ime_position.reset();
		label->update_internal_processing();
		auto* display = DisplayServer::get_singleton();
		display->window_set_ime_position(Vector2i(), window);
		display->window_set_ime_active(false, window);
	}
	const auto handle = focused_element;
	if (const auto input = label->layout->text_control(handle); input && input->editor.has_preedit()) {
		const String data = input->editor.get_preedit();
		if (input->editor.cancel_preedit()) {
			label->dispatch_composition_event(StringName("compositionend"), handle, data);
			label->layout->repaint_input();
			queue_redraw();
		}
	}
}

void HTMLLabelCanvas::update_ime() {
	if (input_menu_target.is_valid()) return;

	const auto handle = focused_element;
	if (!label->layout->is_element_connected(handle)) {
		close_ime();
		return;
	}
	const auto input = label->layout->text_control(handle);
	auto* display = DisplayServer::get_singleton();
	if (!input || !input->is_editable() || !label->is_inside_tree() || !label->has_focus() ||
		!label->is_visible_in_tree() || !display->has_feature(DisplayServer::FEATURE_IME)) {
		if (ime_window) close_ime();
		return;
	}

	Viewport* viewport = get_viewport();
	Window* field_window = get_window();
	Window* native_window = find_native_ime_window(viewport);
	const int32_t id = native_window ? native_window->get_window_id() : -1;
	if (id < 0 || !field_window || !field_window->has_focus() || !native_window->has_focus()) {
		if (ime_window) close_ime();
		return;
	}
	if (ime_window != id) {
		if (ime_window) close_ime();
		ime_window = id;
		// A window stretch or embedded-window move can change the anchor without redrawing the field.
		label->update_internal_processing();
		display->window_set_ime_active(true, id);
	}

	const auto point = label->layout->input_caret_point(*handle.ptr());
	const Rect2 visible = label->get_visible_rect();
	if (!point || !visible.has_area()) return;
	const Vector2 local = point->clamp(visible.position, visible.get_end());
	// Viewport screen transforms map into the native window's client area.
	const Vector2 client = (viewport->get_screen_transform() * get_global_transform_with_canvas()).xform(local);
	const Vector2i client_size = display->window_get_size(id);
	if (!client.is_finite() || client_size.x <= 0 || client_size.y <= 0) return;
	const Vector2i position(client.round().clamp(Vector2(), Vector2(client_size - Vector2i(1, 1))));
	if (ime_position != position) {
		ime_position = position;
		display->window_set_ime_position(position, id);
	}
}

void HTMLLabelCanvas::on_ime_update() {
	if (!ime_window || !label->has_focus()) return;
	const auto handle = focused_element;
	if (!label->layout->is_element_connected(handle)) {
		close_ime();
		return;
	}
	const auto input = label->layout->text_control(handle);
	if (!input || !input->is_editable()) return;
	auto* display = DisplayServer::get_singleton();
	const String previous = input->editor.get_preedit();
	const String next = display->ime_get_text();
	const bool was_composing = input->editor.has_preedit();
	if (!was_composing && !next.is_empty()) label->dispatch_composition_event(StringName("compositionstart"), handle, {});
	input->editor.update_preedit(next, display->ime_get_selection(),
		Engine::get_singleton()->get_process_frames());
	if (!next.is_empty() && next != previous)
		label->dispatch_composition_event(StringName("compositionupdate"), handle, next);
	else if (was_composing && next.is_empty())
		label->dispatch_composition_event(StringName("compositionend"), handle, previous);
	restart_caret_blink();
	label->layout->repaint_input();
	queue_redraw();
}


bool HTMLLabelCanvas::begin_resize_drag(const Vector2& p_point) {
	label->layout->flush_input_paint();
	const auto target = label->layout->resize_grip_at(p_point);
	if (!target || target->element.is_null()) return false;
	const Vector2 local = target->inverse.xform(p_point);
	if (!local.is_finite()) return false;
	cancel_press();
	const auto inverse = target->inverse * Transform2D(0, label->get_visible_rect().position);
	box_drag.emplace(BoxDrag{target->element, target->axes, inverse, local, target->size});
	remember_interaction_targets();
	update_cursor(p_point);
	return true;
}

bool HTMLLabelCanvas::begin_textarea_scrollbar_drag(const Ref<HTMLElement>& p_element, const Vector2& p_point) {
	const auto node = label->layout->text_control(p_element);
	const auto* textarea = dynamic_cast<const HTMLTextAreaNode*>(node.get());
	if (!textarea) return false;
	label->layout->flush_input_paint();
	const auto inverse = label->layout->input_inverse(*p_element.ptr());
	if (!inverse) return false;
	const Vector2 local = inverse->xform(p_point);
	if (!local.is_finite()) return false;
	const auto scrollbar = textarea->editor.scrollbar_at(local);
	if (!scrollbar) return false;

	cancel_press();
	auto& drag = box_drag.emplace();
	drag.element = p_element;
	drag.vertical_scrollbar = *scrollbar;
	drag.scrollbar_grab = textarea->editor.scrollbar_grab_offset(*scrollbar, local);
	remember_interaction_targets();
	label->layout->set_scrollbar_pressed(dockit::HTMLScrollbarDrag{node, *scrollbar ? 1 : 0, 0});
	move_box_drag(p_point);
	update_cursor(p_point);
	return true;
}

void HTMLLabelCanvas::move_box_drag(const Vector2& p_point) {
	if (!box_drag) return;
	const auto& drag = *box_drag;
	if (drag.axes == Vector2i()) {
		const auto node = label->layout->text_control(drag.element);
		const auto* textarea = dynamic_cast<const HTMLTextAreaNode*>(node.get());
		if (!textarea || dockit::forms::is_disabled(*textarea)) {
			box_drag.reset();
			label->layout->set_scrollbar_pressed(std::nullopt);
			return;
		}
		if (const auto local = label->layout->input_local_point(*drag.element.ptr(), p_point)) {
			node->editor.drag_scrollbar(drag.vertical_scrollbar, *local, drag.scrollbar_grab);
			label->layout->repaint_input();
			queue_redraw();
		}
		return;
	}
	const auto current = drag.inverse.xform(p_point - label->get_visible_rect().position);
	const auto delta = (current - drag.start_point) * Vector2(drag.axes);
	const auto size = (drag.start_size + delta).max(Vector2());
	if (!size.is_finite() || (delta == Vector2() && !dragged_resize)) return;
	dragged_resize = true;
	resize_pending = size;
	if (!resize_queued) {
		resize_queued = true;
		callable_mp(this, &HTMLLabelCanvas::flush_box_resize).call_deferred();
	}
}

void HTMLLabelCanvas::flush_box_resize() {
	resize_queued = false;
	const auto size = std::exchange(resize_pending, std::nullopt);
	if (!label || !box_drag || !size) return;
	const auto& drag = *box_drag;
	applying_box_resize = true;
	label->layout->resize_element(drag.element, *size, drag.axes);
	applying_box_resize = false;
}

void HTMLLabelCanvas::update_input_autoscroll() {
	const auto input = label->layout->text_control(dragging_input);
	if (!input) {
		dragging_input.unref();
		set_process_internal(false);
		return;
	}
	const auto point = get_local_mouse_position();
	const auto local = label->layout->input_local_point(*dragging_input.ptr(), point);
	if (!local || !input->editor.is_multiline()) return;
	const Rect2 viewport = input->editor.get_viewport();
	const Vector2 outside = *local - local->clamp(viewport.position, viewport.get_end());
	if (outside != Vector2() && input->editor.scroll_by(outside * std::min(1.0, get_process_delta_time() * 12.0)) != Vector2()) {
		set_input_caret(dragging_input, point, true);
	}
}

std::optional<Control::CursorShape> HTMLLabelCanvas::box_resize_cursor(const Vector2& p_point) const {
	Vector2i axes;
	if (box_drag) {
		axes = box_drag->axes;
	} else if (const auto target = label->layout->resize_grip_at(p_point)) {
		axes = target->axes;
	}
	if (axes.x && axes.y) return CURSOR_FDIAGSIZE;
	if (axes.x) return CURSOR_HSIZE;
	if (axes.y) return CURSOR_VSIZE;
	return std::nullopt;
}
