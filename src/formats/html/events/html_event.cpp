#include "formats/html/dom/html_shadow_root.h"
#include "html_event.h"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include <chrono>

using namespace godot;

namespace {
	template <typename T>
	Ref<HTMLEvent> instantiate_event() {
		Ref<T> event;
		event.instantiate();
		return event;
	}

	using EventFactory = Ref<HTMLEvent>(*)();

	int64_t now_milliseconds() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	// 只在派发器接管前允许修改，避免脚本篡改正在传播的事件。
	bool accepts_configuration(const HTMLEvent& p_event) {
		return !p_event.is_dispatching();
	}
}

void HTMLEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_type"), &HTMLEvent::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &HTMLEvent::set_type);
	ClassDB::bind_method(D_METHOD("get_target"), &HTMLEvent::get_target);
	ClassDB::bind_method(D_METHOD("get_current_target"), &HTMLEvent::get_current_target);
	ClassDB::bind_method(D_METHOD("composed_path"), &HTMLEvent::composed_path);
	ClassDB::bind_method(D_METHOD("get_event_phase"), &HTMLEvent::get_event_phase);
	ClassDB::bind_method(D_METHOD("get_bubbles"), &HTMLEvent::get_bubbles);
	ClassDB::bind_method(D_METHOD("set_bubbles", "bubbles"), &HTMLEvent::set_bubbles);
	ClassDB::bind_method(D_METHOD("get_cancelable"), &HTMLEvent::get_cancelable);
	ClassDB::bind_method(D_METHOD("set_cancelable", "cancelable"), &HTMLEvent::set_cancelable);
	ClassDB::bind_method(D_METHOD("get_composed"), &HTMLEvent::get_composed);
	ClassDB::bind_method(D_METHOD("set_composed", "composed"), &HTMLEvent::set_composed);
	ClassDB::bind_method(D_METHOD("is_default_prevented"), &HTMLEvent::is_default_prevented);
	ClassDB::bind_method(D_METHOD("is_trusted"), &HTMLEvent::is_trusted);
	ClassDB::bind_method(D_METHOD("get_time_stamp"), &HTMLEvent::get_time_stamp);
	ClassDB::bind_method(D_METHOD("prevent_default"), &HTMLEvent::prevent_default);
	ClassDB::bind_method(D_METHOD("stop_propagation"), &HTMLEvent::stop_propagation);
	ClassDB::bind_method(D_METHOD("stop_immediate_propagation"), &HTMLEvent::stop_immediate_propagation);
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "type"), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "target", PROPERTY_HINT_RESOURCE_TYPE, "HTMLEventTarget",
		PROPERTY_USAGE_NONE), "", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "current_target", PROPERTY_HINT_RESOURCE_TYPE, "HTMLEventTarget",
		PROPERTY_USAGE_NONE), "", "get_current_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "event_phase", PROPERTY_HINT_ENUM, "NONE,CAPTURING,AT_TARGET,BUBBLING"),
		"", "get_event_phase");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bubbles"), "set_bubbles", "get_bubbles");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cancelable"), "set_cancelable", "get_cancelable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "composed"), "set_composed", "get_composed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_prevented"), "", "is_default_prevented");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trusted"), "", "is_trusted");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "time_stamp"), "", "get_time_stamp");
	BIND_ENUM_CONSTANT(PHASE_NONE);
	BIND_ENUM_CONSTANT(PHASE_CAPTURING);
	BIND_ENUM_CONSTANT(PHASE_AT_TARGET);
	BIND_ENUM_CONSTANT(PHASE_BUBBLING);
}

bool HTMLEvent::begin_dispatch(const Ref<HTMLEventTarget>& p_target,
	const std::vector<Ref<HTMLEventTarget>>& p_path) {
	if (dispatching) return false;
	target = p_target;
	original_target = p_target;
	current_target = p_target;
	path = p_path;
	phase = PHASE_NONE;
	propagation_stopped = false;
	immediate_stopped = false;
	passive_context = false;
	dispatching = true;
	if (!time_stamp) time_stamp = now_milliseconds();
	return true;
}

void HTMLEvent::set_dispatch_phase(const Ref<HTMLEventTarget>& p_node, int p_phase) {
	current_target = p_node;
	target = original_target;
	Ref<HTMLNode> adjusted = original_target;
	const Ref<HTMLNode> current = p_node;
	while (adjusted.is_valid()) {
		const Ref<HTMLShadowRoot> root = adjusted->get_root_node();
		if (root.is_null() || (current.is_valid() && root->contains(current))) break;
		adjusted = root->get_host();
		target = adjusted;
	}
	phase = p_phase;
	immediate_stopped = false;
}

void HTMLEvent::end_dispatch() {
	dispatching = false;
	phase = PHASE_NONE;
	current_target = Ref<HTMLEventTarget>();
	path.clear();
	passive_context = false;
}

TypedArray<HTMLEventTarget> HTMLEvent::composed_path() const {
	TypedArray<HTMLEventTarget> result;
	const Ref<HTMLNode> current = current_target;
	for (const auto& entry : path) {
		Ref<HTMLNode> node = entry;
		bool hidden = false;
		while (node.is_valid()) {
			const Ref<HTMLShadowRoot> root = node->get_root_node();
			if (root.is_null()) break;
			if (root->get_mode() == "closed" && (current.is_null() || !root->contains(current))) { hidden = true; break; }
			node = root->get_host();
		}
		if (!hidden) result.append(entry);
	}
	return result;
}

void HTMLEvent::set_type(const StringName& p_type) {
	if (!accepts_configuration(*this)) return;
	type = p_type;
}

void HTMLEvent::set_bubbles(bool p_bubbles) {
	if (!accepts_configuration(*this)) return;
	bubbles = p_bubbles;
}

void HTMLEvent::set_cancelable(bool p_cancelable) {
	if (!accepts_configuration(*this)) return;
	cancelable = p_cancelable;
}

void HTMLEvent::set_composed(bool p_composed) {
	if (!accepts_configuration(*this)) return;
	composed = p_composed;
}

void HTMLEvent::prevent_default() {
	if (passive_context) {
		// 与浏览器一致：passive 监听器里的 prevent_default 不生效，只提示一次。
		if (!passive_warned) {
			passive_warned = true;
			WARN_PRINT("prevent_default() was ignored because the listener was registered with passive = true.");
		}
		return;
	}
	if (cancelable) default_prevented = true;
}

void HTMLEvent::stop_propagation() {
	if (dispatching) propagation_stopped = true;
}

void HTMLEvent::stop_immediate_propagation() {
	if (!dispatching) return;
	propagation_stopped = true;
	immediate_stopped = true;
}

void HTMLCustomEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_detail"), &HTMLCustomEvent::get_detail);
	ClassDB::bind_method(D_METHOD("set_detail", "detail"), &HTMLCustomEvent::set_detail);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "detail"), "set_detail", "get_detail");
}

void HTMLCustomEvent::set_detail(const Dictionary& p_detail) {
	if (!accepts_configuration(*this)) return;
	detail = p_detail;
}

void HTMLMouseEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_related_target"), &HTMLMouseEvent::get_related_target);
	ClassDB::bind_method(D_METHOD("set_related_target", "target"), &HTMLMouseEvent::set_related_target);
	ClassDB::bind_method(D_METHOD("get_button"), &HTMLMouseEvent::get_button);
	ClassDB::bind_method(D_METHOD("set_button", "button"), &HTMLMouseEvent::set_button);
	ClassDB::bind_method(D_METHOD("get_buttons"), &HTMLMouseEvent::get_buttons);
	ClassDB::bind_method(D_METHOD("set_buttons", "buttons"), &HTMLMouseEvent::set_buttons);
	ClassDB::bind_method(D_METHOD("get_detail"), &HTMLMouseEvent::get_detail);
	ClassDB::bind_method(D_METHOD("set_detail", "detail"), &HTMLMouseEvent::set_detail);
	ClassDB::bind_method(D_METHOD("get_position"), &HTMLMouseEvent::get_position);
	ClassDB::bind_method(D_METHOD("set_position", "position"), &HTMLMouseEvent::set_position);
	ClassDB::bind_method(D_METHOD("is_shift_pressed"), &HTMLMouseEvent::is_shift_pressed);
	ClassDB::bind_method(D_METHOD("set_shift_pressed", "pressed"), &HTMLMouseEvent::set_shift_pressed);
	ClassDB::bind_method(D_METHOD("is_ctrl_pressed"), &HTMLMouseEvent::is_ctrl_pressed);
	ClassDB::bind_method(D_METHOD("set_ctrl_pressed", "pressed"), &HTMLMouseEvent::set_ctrl_pressed);
	ClassDB::bind_method(D_METHOD("is_alt_pressed"), &HTMLMouseEvent::is_alt_pressed);
	ClassDB::bind_method(D_METHOD("set_alt_pressed", "pressed"), &HTMLMouseEvent::set_alt_pressed);
	ClassDB::bind_method(D_METHOD("is_meta_pressed"), &HTMLMouseEvent::is_meta_pressed);
	ClassDB::bind_method(D_METHOD("set_meta_pressed", "pressed"), &HTMLMouseEvent::set_meta_pressed);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "related_target", PROPERTY_HINT_RESOURCE_TYPE, "HTMLEventTarget",
		PROPERTY_USAGE_NONE), "set_related_target", "get_related_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "button"), "set_button", "get_button");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "buttons"), "set_buttons", "get_buttons");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "detail"), "set_detail", "get_detail");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shift_pressed"), "set_shift_pressed", "is_shift_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ctrl_pressed"), "set_ctrl_pressed", "is_ctrl_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "alt_pressed"), "set_alt_pressed", "is_alt_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "meta_pressed"), "set_meta_pressed", "is_meta_pressed");
}

void HTMLWheelEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_delta"), &HTMLWheelEvent::get_delta);
	ClassDB::bind_method(D_METHOD("set_delta", "delta"), &HTMLWheelEvent::set_delta);
	ClassDB::bind_method(D_METHOD("get_delta_mode"), &HTMLWheelEvent::get_delta_mode);
	ClassDB::bind_method(D_METHOD("set_delta_mode", "delta_mode"), &HTMLWheelEvent::set_delta_mode);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "delta"), "set_delta", "get_delta");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "delta_mode", PROPERTY_HINT_ENUM, "Pixel"), "set_delta_mode", "get_delta_mode");
	BIND_ENUM_CONSTANT(DELTA_MODE_PIXEL);
}

void HTMLPointerEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pointer_id"), &HTMLPointerEvent::get_pointer_id);
	ClassDB::bind_method(D_METHOD("set_pointer_id", "pointer_id"), &HTMLPointerEvent::set_pointer_id);
	ClassDB::bind_method(D_METHOD("get_pointer_type"), &HTMLPointerEvent::get_pointer_type);
	ClassDB::bind_method(D_METHOD("set_pointer_type", "pointer_type"), &HTMLPointerEvent::set_pointer_type);
	ClassDB::bind_method(D_METHOD("is_primary"), &HTMLPointerEvent::is_primary);
	ClassDB::bind_method(D_METHOD("set_is_primary", "primary"), &HTMLPointerEvent::set_is_primary);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pointer_id"), "set_pointer_id", "get_pointer_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "pointer_type"), "set_pointer_type", "get_pointer_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_primary"), "set_is_primary", "is_primary");
}

void HTMLKeyboardEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_keycode"), &HTMLKeyboardEvent::get_keycode);
	ClassDB::bind_method(D_METHOD("set_keycode", "keycode"), &HTMLKeyboardEvent::set_keycode);
	ClassDB::bind_method(D_METHOD("get_physical_keycode"), &HTMLKeyboardEvent::get_physical_keycode);
	ClassDB::bind_method(D_METHOD("set_physical_keycode", "physical_keycode"), &HTMLKeyboardEvent::set_physical_keycode);
	ClassDB::bind_method(D_METHOD("get_key_label"), &HTMLKeyboardEvent::get_key_label);
	ClassDB::bind_method(D_METHOD("set_key_label", "key_label"), &HTMLKeyboardEvent::set_key_label);
	ClassDB::bind_method(D_METHOD("get_unicode"), &HTMLKeyboardEvent::get_unicode);
	ClassDB::bind_method(D_METHOD("set_unicode", "unicode"), &HTMLKeyboardEvent::set_unicode);
	ClassDB::bind_method(D_METHOD("is_shift_pressed"), &HTMLKeyboardEvent::is_shift_pressed);
	ClassDB::bind_method(D_METHOD("set_shift_pressed", "pressed"), &HTMLKeyboardEvent::set_shift_pressed);
	ClassDB::bind_method(D_METHOD("is_ctrl_pressed"), &HTMLKeyboardEvent::is_ctrl_pressed);
	ClassDB::bind_method(D_METHOD("set_ctrl_pressed", "pressed"), &HTMLKeyboardEvent::set_ctrl_pressed);
	ClassDB::bind_method(D_METHOD("is_alt_pressed"), &HTMLKeyboardEvent::is_alt_pressed);
	ClassDB::bind_method(D_METHOD("set_alt_pressed", "pressed"), &HTMLKeyboardEvent::set_alt_pressed);
	ClassDB::bind_method(D_METHOD("is_meta_pressed"), &HTMLKeyboardEvent::is_meta_pressed);
	ClassDB::bind_method(D_METHOD("set_meta_pressed", "pressed"), &HTMLKeyboardEvent::set_meta_pressed);
	ClassDB::bind_method(D_METHOD("is_echo"), &HTMLKeyboardEvent::is_echo);
	ClassDB::bind_method(D_METHOD("set_echo", "echo"), &HTMLKeyboardEvent::set_echo);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "keycode"), "set_keycode", "get_keycode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "physical_keycode"), "set_physical_keycode", "get_physical_keycode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "key_label"), "set_key_label", "get_key_label");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "unicode"), "set_unicode", "get_unicode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shift_pressed"), "set_shift_pressed", "is_shift_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ctrl_pressed"), "set_ctrl_pressed", "is_ctrl_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "alt_pressed"), "set_alt_pressed", "is_alt_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "meta_pressed"), "set_meta_pressed", "is_meta_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "echo"), "set_echo", "is_echo");
}

void HTMLFocusEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_related_target"), &HTMLFocusEvent::get_related_target);
	ClassDB::bind_method(D_METHOD("set_related_target", "target"), &HTMLFocusEvent::set_related_target);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "related_target", PROPERTY_HINT_RESOURCE_TYPE, "HTMLEventTarget",
		PROPERTY_USAGE_NONE), "set_related_target", "get_related_target");
}

void HTMLSubmitEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_submitter"), &HTMLSubmitEvent::get_submitter);
	ClassDB::bind_method(D_METHOD("set_submitter", "submitter"), &HTMLSubmitEvent::set_submitter);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "submitter", PROPERTY_HINT_RESOURCE_TYPE, "HTMLElement",
		PROPERTY_USAGE_NONE), "set_submitter", "get_submitter");
}

void HTMLInputEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_data"), &HTMLInputEvent::get_data);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &HTMLInputEvent::set_data);
	ClassDB::bind_method(D_METHOD("get_input_type"), &HTMLInputEvent::get_input_type);
	ClassDB::bind_method(D_METHOD("set_input_type", "input_type"), &HTMLInputEvent::set_input_type);
	ClassDB::bind_method(D_METHOD("is_composing"), &HTMLInputEvent::is_composing);
	ClassDB::bind_method(D_METHOD("set_is_composing", "composing"), &HTMLInputEvent::set_is_composing);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "data"), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "input_type"), "set_input_type", "get_input_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_composing"), "set_is_composing", "is_composing");
}

void HTMLInputEvent::set_data(const String& p_data) { if (accepts_configuration(*this)) data = p_data; }
void HTMLInputEvent::set_input_type(const StringName& p_input_type) { if (accepts_configuration(*this)) input_type = p_input_type; }
void HTMLInputEvent::set_is_composing(bool p_composing) { if (accepts_configuration(*this)) composing = p_composing; }

void HTMLCompositionEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_data"), &HTMLCompositionEvent::get_data);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &HTMLCompositionEvent::set_data);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "data"), "set_data", "get_data");
}

void HTMLCompositionEvent::set_data(const String& p_data) { if (accepts_configuration(*this)) data = p_data; }

void HTMLDataTransfer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_data", "type"), &HTMLDataTransfer::get_data);
	ClassDB::bind_method(D_METHOD("set_data", "type", "value"), &HTMLDataTransfer::set_data);
	ClassDB::bind_method(D_METHOD("clear_data", "type"), &HTMLDataTransfer::clear_data, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_types"), &HTMLDataTransfer::get_types);
	ClassDB::bind_method(D_METHOD("get_effect_allowed"), &HTMLDataTransfer::get_effect_allowed);
	ClassDB::bind_method(D_METHOD("set_effect_allowed", "value"), &HTMLDataTransfer::set_effect_allowed);
	ClassDB::bind_method(D_METHOD("get_drop_effect"), &HTMLDataTransfer::get_drop_effect);
	ClassDB::bind_method(D_METHOD("set_drop_effect", "value"), &HTMLDataTransfer::set_drop_effect);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "types"), "", "get_types");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "effect_allowed"), "set_effect_allowed", "get_effect_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "drop_effect"), "set_drop_effect", "get_drop_effect");
	BIND_ENUM_CONSTANT(MODE_READ_WRITE);
	BIND_ENUM_CONSTANT(MODE_READ_ONLY);
	BIND_ENUM_CONSTANT(MODE_PROTECTED);
}

String HTMLDataTransfer::get_data(const String& p_type) const {
	if (mode == MODE_PROTECTED) return {};
	const StringName key = p_type.strip_edges().to_lower();
	const String* value = data.getptr(key);
	return value ? *value : String();
}

Error HTMLDataTransfer::set_data(const String& p_type, const String& p_value) {
	if (mode != MODE_READ_WRITE || p_type.strip_edges().is_empty()) return ERR_INVALID_PARAMETER;
	data.insert(StringName(p_type.strip_edges().to_lower()), p_value);
	return OK;
}

void HTMLDataTransfer::clear_data(const String& p_type) {
	if (mode != MODE_READ_WRITE) return;
	if (p_type.is_empty()) data.clear();
	else data.erase(StringName(p_type.strip_edges().to_lower()));
}

PackedStringArray HTMLDataTransfer::get_types() const {
	PackedStringArray result;
	for (const KeyValue<StringName, String>& entry : data) result.push_back(String(entry.key));
	result.sort();
	return result;
}

namespace dockit {
	namespace { Ref<HTMLDataTransfer> current_drag_transfer; }
	void set_current_drag_transfer(const Ref<HTMLDataTransfer>& p_transfer) { current_drag_transfer = p_transfer; }
	Ref<HTMLDataTransfer> get_current_drag_transfer() { return current_drag_transfer; }
}

void HTMLDragEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_data_transfer"), &HTMLDragEvent::get_data_transfer);
	ClassDB::bind_method(D_METHOD("set_data_transfer", "data_transfer"), &HTMLDragEvent::set_data_transfer);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "data_transfer", PROPERTY_HINT_RESOURCE_TYPE, "HTMLDataTransfer"),
		"set_data_transfer", "get_data_transfer");
}

void HTMLClipboardEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_clipboard_data"), &HTMLClipboardEvent::get_clipboard_data);
	ClassDB::bind_method(D_METHOD("set_clipboard_data", "clipboard_data"), &HTMLClipboardEvent::set_clipboard_data);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "clipboard_data", PROPERTY_HINT_RESOURCE_TYPE, "HTMLDataTransfer"),
		"set_clipboard_data", "get_clipboard_data");
}

void HTMLToggleEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_old_state"), &HTMLToggleEvent::get_old_state);
	ClassDB::bind_method(D_METHOD("set_old_state", "state"), &HTMLToggleEvent::set_old_state);
	ClassDB::bind_method(D_METHOD("get_new_state"), &HTMLToggleEvent::get_new_state);
	ClassDB::bind_method(D_METHOD("set_new_state", "state"), &HTMLToggleEvent::set_new_state);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "old_state"), "set_old_state", "get_old_state");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "new_state"), "set_new_state", "get_new_state");
}

void HTMLErrorEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_message"), &HTMLErrorEvent::get_message);
	ClassDB::bind_method(D_METHOD("set_message", "value"), &HTMLErrorEvent::set_message);
	ClassDB::bind_method(D_METHOD("get_filename"), &HTMLErrorEvent::get_filename);
	ClassDB::bind_method(D_METHOD("set_filename", "value"), &HTMLErrorEvent::set_filename);
	ClassDB::bind_method(D_METHOD("get_line_number"), &HTMLErrorEvent::get_line_number);
	ClassDB::bind_method(D_METHOD("set_line_number", "value"), &HTMLErrorEvent::set_line_number);
	ClassDB::bind_method(D_METHOD("get_column_number"), &HTMLErrorEvent::get_column_number);
	ClassDB::bind_method(D_METHOD("set_column_number", "value"), &HTMLErrorEvent::set_column_number);
	ClassDB::bind_method(D_METHOD("get_error"), &HTMLErrorEvent::get_error);
	ClassDB::bind_method(D_METHOD("set_error", "value"), &HTMLErrorEvent::set_error);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "message"), "set_message", "get_message");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "filename"), "set_filename", "get_filename");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "line_number"), "set_line_number", "get_line_number");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "column_number"), "set_column_number", "get_column_number");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "error", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT),
		"set_error", "get_error");
}

void HTMLAnimationEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_animation_name"), &HTMLAnimationEvent::get_animation_name);
	ClassDB::bind_method(D_METHOD("set_animation_name", "value"), &HTMLAnimationEvent::set_animation_name);
	ClassDB::bind_method(D_METHOD("get_elapsed_time"), &HTMLAnimationEvent::get_elapsed_time);
	ClassDB::bind_method(D_METHOD("set_elapsed_time", "value"), &HTMLAnimationEvent::set_elapsed_time);
	ClassDB::bind_method(D_METHOD("get_pseudo_element"), &HTMLAnimationEvent::get_pseudo_element);
	ClassDB::bind_method(D_METHOD("set_pseudo_element", "value"), &HTMLAnimationEvent::set_pseudo_element);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "elapsed_time"), "set_elapsed_time", "get_elapsed_time");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pseudo_element"), "set_pseudo_element", "get_pseudo_element");
}

void HTMLTransitionEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_property_name"), &HTMLTransitionEvent::get_property_name);
	ClassDB::bind_method(D_METHOD("set_property_name", "value"), &HTMLTransitionEvent::set_property_name);
	ClassDB::bind_method(D_METHOD("get_elapsed_time"), &HTMLTransitionEvent::get_elapsed_time);
	ClassDB::bind_method(D_METHOD("set_elapsed_time", "value"), &HTMLTransitionEvent::set_elapsed_time);
	ClassDB::bind_method(D_METHOD("get_pseudo_element"), &HTMLTransitionEvent::get_pseudo_element);
	ClassDB::bind_method(D_METHOD("set_pseudo_element", "value"), &HTMLTransitionEvent::set_pseudo_element);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "property_name"), "set_property_name", "get_property_name");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "elapsed_time"), "set_elapsed_time", "get_elapsed_time");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pseudo_element"), "set_pseudo_element", "get_pseudo_element");
}

void HTMLTouchPoint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_identifier"), &HTMLTouchPoint::get_identifier);
	ClassDB::bind_method(D_METHOD("set_identifier", "value"), &HTMLTouchPoint::set_identifier);
	ClassDB::bind_method(D_METHOD("get_target"), &HTMLTouchPoint::get_target);
	ClassDB::bind_method(D_METHOD("set_target", "value"), &HTMLTouchPoint::set_target);
	ClassDB::bind_method(D_METHOD("get_position"), &HTMLTouchPoint::get_position);
	ClassDB::bind_method(D_METHOD("set_position", "value"), &HTMLTouchPoint::set_position);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "identifier"), "set_identifier", "get_identifier");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "target", PROPERTY_HINT_RESOURCE_TYPE, "HTMLElement"), "set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "set_position", "get_position");
}

void HTMLTouchEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_touches"), &HTMLTouchEvent::get_touches);
	ClassDB::bind_method(D_METHOD("set_touches", "touches"), &HTMLTouchEvent::set_touches);
	ClassDB::bind_method(D_METHOD("get_target_touches"), &HTMLTouchEvent::get_target_touches);
	ClassDB::bind_method(D_METHOD("set_target_touches", "touches"), &HTMLTouchEvent::set_target_touches);
	ClassDB::bind_method(D_METHOD("get_changed_touches"), &HTMLTouchEvent::get_changed_touches);
	ClassDB::bind_method(D_METHOD("set_changed_touches", "touches"), &HTMLTouchEvent::set_changed_touches);
	ClassDB::bind_method(D_METHOD("is_shift_pressed"), &HTMLTouchEvent::is_shift_pressed);
	ClassDB::bind_method(D_METHOD("set_shift_pressed", "pressed"), &HTMLTouchEvent::set_shift_pressed);
	ClassDB::bind_method(D_METHOD("is_ctrl_pressed"), &HTMLTouchEvent::is_ctrl_pressed);
	ClassDB::bind_method(D_METHOD("set_ctrl_pressed", "pressed"), &HTMLTouchEvent::set_ctrl_pressed);
	ClassDB::bind_method(D_METHOD("is_alt_pressed"), &HTMLTouchEvent::is_alt_pressed);
	ClassDB::bind_method(D_METHOD("set_alt_pressed", "pressed"), &HTMLTouchEvent::set_alt_pressed);
	ClassDB::bind_method(D_METHOD("is_meta_pressed"), &HTMLTouchEvent::is_meta_pressed);
	ClassDB::bind_method(D_METHOD("set_meta_pressed", "pressed"), &HTMLTouchEvent::set_meta_pressed);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "touches", PROPERTY_HINT_ARRAY_TYPE, "HTMLTouchPoint"), "set_touches", "get_touches");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "target_touches", PROPERTY_HINT_ARRAY_TYPE, "HTMLTouchPoint"), "set_target_touches", "get_target_touches");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "changed_touches", PROPERTY_HINT_ARRAY_TYPE, "HTMLTouchPoint"), "set_changed_touches", "get_changed_touches");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shift_pressed"), "set_shift_pressed", "is_shift_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ctrl_pressed"), "set_ctrl_pressed", "is_ctrl_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "alt_pressed"), "set_alt_pressed", "is_alt_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "meta_pressed"), "set_meta_pressed", "is_meta_pressed");
}

namespace dockit {
	Ref<HTMLEvent> make_dom_event(const StringName& p_type, bool p_bubbles, bool p_cancelable) {
		static const HashMap<StringName, EventFactory> event_factories = [] {
			HashMap<StringName, EventFactory> factories;
			for (const StringName& type : { StringName("mousedown"), StringName("mouseup"), StringName("mousemove"),
				StringName("mouseover"), StringName("mouseout"), StringName("mouseenter"), StringName("mouseleave"),
				StringName("click"), StringName("dblclick"), StringName("contextmenu"), StringName("auxclick") }) {
				factories.insert(type, &instantiate_event<HTMLMouseEvent>);
			}
			for (const StringName& type : { StringName("dragstart"), StringName("drag"), StringName("dragenter"),
				StringName("dragover"), StringName("dragleave"), StringName("drop"), StringName("dragend") }) {
				factories.insert(type, &instantiate_event<HTMLDragEvent>);
			}
			for (const StringName& type : { StringName("pointerdown"), StringName("pointerup"), StringName("pointermove"),
				StringName("pointerover"), StringName("pointerout"), StringName("pointerenter"), StringName("pointerleave"),
				StringName("pointercancel"), StringName("gotpointercapture"), StringName("lostpointercapture") }) {
				factories.insert(type, &instantiate_event<HTMLPointerEvent>);
			}
			for (const StringName& type : { StringName("touchstart"), StringName("touchmove"), StringName("touchend"), StringName("touchcancel") }) {
				factories.insert(type, &instantiate_event<HTMLTouchEvent>);
			}
			factories.insert(StringName("wheel"), &instantiate_event<HTMLWheelEvent>);
			factories.insert(StringName("keydown"), &instantiate_event<HTMLKeyboardEvent>);
			factories.insert(StringName("keyup"), &instantiate_event<HTMLKeyboardEvent>);
			factories.insert(StringName("focus"), &instantiate_event<HTMLFocusEvent>);
			factories.insert(StringName("blur"), &instantiate_event<HTMLFocusEvent>);
			factories.insert(StringName("focusin"), &instantiate_event<HTMLFocusEvent>);
			factories.insert(StringName("focusout"), &instantiate_event<HTMLFocusEvent>);
			factories.insert(StringName("submit"), &instantiate_event<HTMLSubmitEvent>);
			factories.insert(StringName("reset"), &instantiate_event<HTMLSubmitEvent>);
			factories.insert(StringName("beforeinput"), &instantiate_event<HTMLInputEvent>);
			factories.insert(StringName("input"), &instantiate_event<HTMLInputEvent>);
			for (const StringName& type : { StringName("compositionstart"), StringName("compositionupdate"), StringName("compositionend") })
				factories.insert(type, &instantiate_event<HTMLCompositionEvent>);
			for (const StringName& type : { StringName("copy"), StringName("cut"), StringName("paste") })
				factories.insert(type, &instantiate_event<HTMLClipboardEvent>);
			for (const StringName& type : { StringName("beforetoggle"), StringName("toggle") })
				factories.insert(type, &instantiate_event<HTMLToggleEvent>);
			for (const StringName& type : { StringName("loadstart"), StringName("progress"), StringName("suspend"), StringName("abort"),
				StringName("emptied"), StringName("stalled"), StringName("loadedmetadata"), StringName("loadeddata"),
				StringName("canplay"), StringName("canplaythrough"), StringName("play"), StringName("playing"),
				StringName("pause"), StringName("waiting"), StringName("seeking"), StringName("seeked"), StringName("ended"),
				StringName("durationchange"), StringName("timeupdate"), StringName("ratechange"), StringName("volumechange") })
				factories.insert(type, &instantiate_event<HTMLMediaEvent>);
			factories.insert(StringName("error"), &instantiate_event<HTMLErrorEvent>);
			for (const StringName& type : { StringName("animationstart"), StringName("animationiteration"),
				StringName("animationend"), StringName("animationcancel") })
				factories.insert(type, &instantiate_event<HTMLAnimationEvent>);
			for (const StringName& type : { StringName("transitionrun"), StringName("transitionstart"),
				StringName("transitionend"), StringName("transitioncancel") })
				factories.insert(type, &instantiate_event<HTMLTransitionEvent>);
			factories.insert(StringName("scrollend"), &instantiate_event<HTMLEvent>);
			factories.insert(StringName("resize"), &instantiate_event<HTMLEvent>);
			factories.insert(StringName("visibilitychange"), &instantiate_event<HTMLEvent>);
			return factories;
		}();
		Ref<HTMLEvent> event;
		if (const EventFactory* factory = event_factories.getptr(p_type)) event = (*factory)();
		else event.instantiate();
		event->set_type(p_type);
		event->set_bubbles(p_bubbles);
		event->set_cancelable(p_cancelable);
		event->set_composed(true);
		event->mark_trusted();
		if (Ref<HTMLDragEvent> drag_event = event; drag_event.is_valid()) {
			Ref<HTMLDataTransfer> transfer;
			transfer.instantiate();
			if (p_type == StringName("drop")) transfer->set_mode(HTMLDataTransfer::MODE_READ_ONLY);
			else if (p_type != StringName("dragstart")) transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
			drag_event->set_data_transfer(transfer);
		}
		return event;
	}
}
