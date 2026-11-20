#pragma once

#include "formats/html/elements/html_element.h"
#include "formats/html/events/html_event_target.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <cstdint>
#include <vector>

// DOM 事件对象。脚本可以构造合成事件并交给 HTMLEventTarget.dispatch_event()；
// 引擎内部合成的真实输入事件由 dispatcher 设置 target/current_target/phase，
// 这些字段只保留 getter，脚本无法篡改派发路径。
// 事件路径为 window -> document -> 元素链，因此 target/current_target 是
// HTMLEventTarget（元素、文档节点或 window），而不是固定的 HTMLNode。
class HTMLEvent : public godot::RefCounted {
	GDCLASS(HTMLEvent, godot::RefCounted)

public:
	enum EventPhase {
		PHASE_NONE = 0,
		PHASE_CAPTURING = 1,
		PHASE_AT_TARGET = 2,
		PHASE_BUBBLING = 3,
	};

private:
	godot::StringName type;
	godot::Ref<HTMLEventTarget> target;
	godot::Ref<HTMLEventTarget> original_target;
	godot::Ref<HTMLEventTarget> current_target;
	std::vector<godot::Ref<HTMLEventTarget>> path;
	int phase = PHASE_NONE;
	bool bubbles = false;
	bool cancelable = false;
	bool composed = false;
	bool default_prevented = false;
	bool trusted = false;
	bool propagation_stopped = false;
	bool immediate_stopped = false;
	bool dispatching = false;
	bool passive_context = false;
	bool passive_warned = false;
	int64_t time_stamp = 0;

protected:
	static void _bind_methods();

public:
	// 由派发器使用：在派发期间接管 target / current_target / phase 与事件路径。
	// 同一事件在派发过程中再次派发会被拒绝，返回 false。
	bool begin_dispatch(const godot::Ref<HTMLEventTarget>& p_target,
		const std::vector<godot::Ref<HTMLEventTarget>>& p_path);
	void set_dispatch_phase(const godot::Ref<HTMLEventTarget>& p_node, int p_phase);
	void end_dispatch();
	void set_passive_context(bool p_passive) { passive_context = p_passive; }	// 由派发器在调用 passive 监听器前后切换
	void mark_trusted() { trusted = true; }										// 标记为引擎合成的真实输入事件
	bool is_dispatching() const { return dispatching; }							// 返回是否正在派发
	bool is_propagation_stopped() const { return propagation_stopped; }			// 返回本次派发是否已停止传播
	bool is_immediate_stopped() const { return immediate_stopped; }				// 返回是否已停止当前节点的其余监听器

	godot::StringName get_type() const { return type; }							// 返回事件类型
	void set_type(const godot::StringName& p_type);								// 设置事件类型，派发期间无效
	godot::Ref<HTMLEventTarget> get_target() const { return target; }			// 返回事件的目标
	godot::Ref<HTMLEventTarget> get_current_target() const { return current_target; } // 返回当前处理到的目标
	godot::TypedArray<HTMLEventTarget> composed_path() const;					// 返回派发时的事件路径（target 在前，window 在后）
	int get_event_phase() const { return phase; }								// 返回派发阶段
	bool get_bubbles() const { return bubbles; }								// 返回是否冒泡
	void set_bubbles(bool p_bubbles);											// 设置是否冒泡，派发期间无效
	bool get_cancelable() const { return cancelable; }							// 返回是否可取消
	void set_cancelable(bool p_cancelable);										// 设置是否可取消，派发期间无效
	bool get_composed() const { return composed; }
	void set_composed(bool p_composed);
	bool is_default_prevented() const { return default_prevented; }				// 返回默认动作是否已被取消
	bool is_trusted() const { return trusted; }									// 返回是否由引擎合成
	int64_t get_time_stamp() const { return time_stamp; }						// 返回事件创建时间的毫秒时间戳
	void prevent_default();														// 取消此事件的默认动作（passive 监听器中无效）
	void stop_propagation();													// 阻止事件继续传播到后续节点
	void stop_immediate_propagation();											// 阻止当前节点上其余监听器并停止传播
};

VARIANT_ENUM_CAST(HTMLEvent::EventPhase);

class HTMLCustomEvent : public HTMLEvent {
	GDCLASS(HTMLCustomEvent, HTMLEvent)

	godot::Dictionary detail;

protected:
	static void _bind_methods();

public:
	godot::Dictionary get_detail() const { return detail; } // 返回自定义载荷
	void set_detail(const godot::Dictionary& p_detail); // 设置自定义载荷，派发期间无效
};

class HTMLMouseEvent : public HTMLEvent {
	GDCLASS(HTMLMouseEvent, HTMLEvent)

	godot::Ref<HTMLEventTarget> related_target;
	int button = 0;
	int buttons = 0;
	int detail = 0;
	godot::Vector2 position;
	bool shift = false;
	bool ctrl = false;
	bool alt = false;
	bool meta = false;

protected:
	static void _bind_methods();

public:
	godot::Ref<HTMLEventTarget> get_related_target() const { return related_target; }
	void set_related_target(const godot::Ref<HTMLEventTarget>& p_target) { related_target = p_target; }
	int get_button() const { return button; }								// 返回触发的鼠标键
	void set_button(int p_button) { button = p_button; }
	int get_buttons() const { return buttons; }								// 返回按键位掩码
	void set_buttons(int p_buttons) { buttons = p_buttons; }
	int get_detail() const { return detail; }								// 返回连续点击计数
	void set_detail(int p_detail) { detail = p_detail; }
	godot::Vector2 get_position() const { return position; }				// 返回标签局部坐标中的指针位置
	void set_position(const godot::Vector2& p_position) { position = p_position; }
	bool is_shift_pressed() const { return shift; }
	void set_shift_pressed(bool p_pressed) { shift = p_pressed; }
	bool is_ctrl_pressed() const { return ctrl; }
	void set_ctrl_pressed(bool p_pressed) { ctrl = p_pressed; }
	bool is_alt_pressed() const { return alt; }
	void set_alt_pressed(bool p_pressed) { alt = p_pressed; }
	bool is_meta_pressed() const { return meta; }
	void set_meta_pressed(bool p_pressed) { meta = p_pressed; }
};

class HTMLWheelEvent : public HTMLMouseEvent {
	GDCLASS(HTMLWheelEvent, HTMLMouseEvent)

public:
	enum DeltaMode {
		DELTA_MODE_PIXEL = 0,
	};

private:
	godot::Vector2 delta;
	int delta_mode = DELTA_MODE_PIXEL;

protected:
	static void _bind_methods();

public:
	godot::Vector2 get_delta() const { return delta; }
	void set_delta(const godot::Vector2& p_delta) { delta = p_delta; }
	int get_delta_mode() const { return delta_mode; }
	void set_delta_mode(int p_delta_mode) { delta_mode = p_delta_mode; }
};

VARIANT_ENUM_CAST(HTMLWheelEvent::DeltaMode);

class HTMLPointerEvent : public HTMLMouseEvent {
	GDCLASS(HTMLPointerEvent, HTMLMouseEvent)

	int pointer_id = 0;
	godot::StringName pointer_type;
	bool primary = true;

protected:
	static void _bind_methods();

public:
	int get_pointer_id() const { return pointer_id; }
	void set_pointer_id(int p_pointer_id) { pointer_id = p_pointer_id; }
	godot::StringName get_pointer_type() const { return pointer_type; }
	void set_pointer_type(const godot::StringName& p_pointer_type) { pointer_type = p_pointer_type; }
	bool is_primary() const { return primary; }
	void set_is_primary(bool p_primary) { primary = p_primary; }
};

class HTMLKeyboardEvent : public HTMLEvent {
	GDCLASS(HTMLKeyboardEvent, HTMLEvent)

	int keycode = 0;
	int physical_keycode = 0;
	int key_label = 0;
	int unicode = 0;
	bool shift = false;
	bool ctrl = false;
	bool alt = false;
	bool meta = false;
	bool echo = false;

protected:
	static void _bind_methods();

public:
	int get_keycode() const { return keycode; }
	void set_keycode(int p_keycode) { keycode = p_keycode; }
	int get_physical_keycode() const { return physical_keycode; }
	void set_physical_keycode(int p_physical_keycode) { physical_keycode = p_physical_keycode; }
	int get_key_label() const { return key_label; } // 返回按键在布局中的标签码
	void set_key_label(int p_key_label) { key_label = p_key_label; }
	int get_unicode() const { return unicode; }
	void set_unicode(int p_unicode) { unicode = p_unicode; }
	bool is_shift_pressed() const { return shift; }
	void set_shift_pressed(bool p_pressed) { shift = p_pressed; }
	bool is_ctrl_pressed() const { return ctrl; }
	void set_ctrl_pressed(bool p_pressed) { ctrl = p_pressed; }
	bool is_alt_pressed() const { return alt; }
	void set_alt_pressed(bool p_pressed) { alt = p_pressed; }
	bool is_meta_pressed() const { return meta; }
	void set_meta_pressed(bool p_pressed) { meta = p_pressed; }
	bool is_echo() const { return echo; } // 返回是否为按住产生的重复按键
	void set_echo(bool p_echo) { echo = p_echo; }
};

class HTMLFocusEvent : public HTMLEvent {
	GDCLASS(HTMLFocusEvent, HTMLEvent)

	godot::Ref<HTMLEventTarget> related_target;

protected:
	static void _bind_methods();

public:
	godot::Ref<HTMLEventTarget> get_related_target() const { return related_target; } // 返回焦点转移的对端目标
	void set_related_target(const godot::Ref<HTMLEventTarget>& p_target) { related_target = p_target; }
};

class HTMLSubmitEvent : public HTMLEvent {
	GDCLASS(HTMLSubmitEvent, HTMLEvent)

	godot::Ref<HTMLElement> submitter;

protected:
	static void _bind_methods();

public:
	godot::Ref<HTMLElement> get_submitter() const { return submitter; } // 返回触发提交的按钮，可能为空
	void set_submitter(const godot::Ref<HTMLElement>& p_submitter) { submitter = p_submitter; }
};

class HTMLInputEvent : public HTMLEvent {
	GDCLASS(HTMLInputEvent, HTMLEvent)
	godot::String data;
	godot::StringName input_type;
	bool composing = false;
protected:
	static void _bind_methods();
public:
	godot::String get_data() const { return data; }
	void set_data(const godot::String& p_data);
	godot::StringName get_input_type() const { return input_type; }
	void set_input_type(const godot::StringName& p_input_type);
	bool is_composing() const { return composing; }
	void set_is_composing(bool p_composing);
};

class HTMLCompositionEvent : public HTMLEvent {
	GDCLASS(HTMLCompositionEvent, HTMLEvent)
	godot::String data;
protected:
	static void _bind_methods();
public:
	godot::String get_data() const { return data; }
	void set_data(const godot::String& p_data);
};

class HTMLDataTransfer : public godot::RefCounted {
	GDCLASS(HTMLDataTransfer, godot::RefCounted)

public:
	enum Mode {
		MODE_READ_WRITE,
		MODE_READ_ONLY,
		MODE_PROTECTED,
	};

private:
	godot::HashMap<godot::StringName, godot::String> data;
	Mode mode = MODE_READ_WRITE;
	godot::String effect_allowed = "uninitialized";
	godot::String drop_effect = "none";
protected:
	static void _bind_methods();
public:
	godot::String get_data(const godot::String& p_type) const;
	godot::Error set_data(const godot::String& p_type, const godot::String& p_value);
	void clear_data(const godot::String& p_type = {});
	godot::PackedStringArray get_types() const;
	void set_text(const godot::String& p_text) { data.insert(godot::StringName("text/plain"), p_text); }
	void set_effect_allowed(const godot::String& p_value) { if (mode == MODE_READ_WRITE) effect_allowed = p_value; }
	const godot::String& get_effect_allowed() const { return effect_allowed; }
	void set_drop_effect(const godot::String& p_value) { drop_effect = p_value; }
	const godot::String& get_drop_effect() const { return drop_effect; }
	void set_mode(Mode p_mode) { mode = p_mode; }
	void set_read_only(bool p_read_only) { mode = p_read_only ? MODE_READ_ONLY : MODE_READ_WRITE; }
};

namespace dockit {
	void set_current_drag_transfer(const godot::Ref<HTMLDataTransfer>& p_transfer);
	godot::Ref<HTMLDataTransfer> get_current_drag_transfer();
}

VARIANT_ENUM_CAST(HTMLDataTransfer::Mode);

class HTMLDragEvent : public HTMLMouseEvent {
	GDCLASS(HTMLDragEvent, HTMLMouseEvent)
	godot::Ref<HTMLDataTransfer> data_transfer;
protected:
	static void _bind_methods();
public:
	godot::Ref<HTMLDataTransfer> get_data_transfer() const { return data_transfer; }
	void set_data_transfer(const godot::Ref<HTMLDataTransfer>& p_data_transfer) { data_transfer = p_data_transfer; }
};

class HTMLClipboardEvent : public HTMLEvent {
	GDCLASS(HTMLClipboardEvent, HTMLEvent)
	godot::Ref<HTMLDataTransfer> clipboard_data;
protected:
	static void _bind_methods();
public:
	godot::Ref<HTMLDataTransfer> get_clipboard_data() const { return clipboard_data; }
	void set_clipboard_data(const godot::Ref<HTMLDataTransfer>& p_data) { clipboard_data = p_data; }
};

class HTMLToggleEvent : public HTMLEvent {
	GDCLASS(HTMLToggleEvent, HTMLEvent)
	godot::String old_state;
	godot::String new_state;
protected:
	static void _bind_methods();
public:
	const godot::String& get_old_state() const { return old_state; }
	void set_old_state(const godot::String& p_state) { old_state = p_state; }
	const godot::String& get_new_state() const { return new_state; }
	void set_new_state(const godot::String& p_state) { new_state = p_state; }
};

class HTMLMediaEvent : public HTMLEvent {
	GDCLASS(HTMLMediaEvent, HTMLEvent)
protected:
	static void _bind_methods() {}
};

class HTMLErrorEvent : public HTMLEvent {
	GDCLASS(HTMLErrorEvent, HTMLEvent)
	godot::String message;
	godot::String filename;
	int32_t line_number = 0;
	int32_t column_number = 0;
	godot::Variant error;
protected:
	static void _bind_methods();
public:
	const godot::String& get_message() const { return message; }
	void set_message(const godot::String& p_value) { message = p_value; }
	const godot::String& get_filename() const { return filename; }
	void set_filename(const godot::String& p_value) { filename = p_value; }
	int32_t get_line_number() const { return line_number; }
	void set_line_number(int32_t p_value) { line_number = p_value; }
	int32_t get_column_number() const { return column_number; }
	void set_column_number(int32_t p_value) { column_number = p_value; }
	godot::Variant get_error() const { return error; }
	void set_error(const godot::Variant& p_value) { error = p_value; }
};

class HTMLAnimationEvent : public HTMLEvent {
	GDCLASS(HTMLAnimationEvent, HTMLEvent)
	godot::String animation_name;
	double elapsed_time = 0.0;
	godot::String pseudo_element;
protected:
	static void _bind_methods();
public:
	const godot::String& get_animation_name() const { return animation_name; }
	void set_animation_name(const godot::String& p_value) { animation_name = p_value; }
	double get_elapsed_time() const { return elapsed_time; }
	void set_elapsed_time(double p_value) { elapsed_time = p_value; }
	const godot::String& get_pseudo_element() const { return pseudo_element; }
	void set_pseudo_element(const godot::String& p_value) { pseudo_element = p_value; }
};

class HTMLTransitionEvent : public HTMLEvent {
	GDCLASS(HTMLTransitionEvent, HTMLEvent)
	godot::String property_name;
	double elapsed_time = 0.0;
	godot::String pseudo_element;
protected:
	static void _bind_methods();
public:
	const godot::String& get_property_name() const { return property_name; }
	void set_property_name(const godot::String& p_value) { property_name = p_value; }
	double get_elapsed_time() const { return elapsed_time; }
	void set_elapsed_time(double p_value) { elapsed_time = p_value; }
	const godot::String& get_pseudo_element() const { return pseudo_element; }
	void set_pseudo_element(const godot::String& p_value) { pseudo_element = p_value; }
};

class HTMLTouchPoint : public godot::RefCounted {
	GDCLASS(HTMLTouchPoint, godot::RefCounted)
	int32_t identifier = 0;
	godot::Ref<HTMLElement> target;
	godot::Vector2 position;
protected:
	static void _bind_methods();
public:
	int32_t get_identifier() const { return identifier; }
	void set_identifier(int32_t p_value) { identifier = p_value; }
	godot::Ref<HTMLElement> get_target() const { return target; }
	void set_target(const godot::Ref<HTMLElement>& p_value) { target = p_value; }
	godot::Vector2 get_position() const { return position; }
	void set_position(const godot::Vector2& p_value) { position = p_value; }
};

class HTMLTouchEvent : public HTMLEvent {
	GDCLASS(HTMLTouchEvent, HTMLEvent)
	godot::TypedArray<HTMLTouchPoint> touches;
	godot::TypedArray<HTMLTouchPoint> target_touches;
	godot::TypedArray<HTMLTouchPoint> changed_touches;
	bool shift = false;
	bool ctrl = false;
	bool alt = false;
	bool meta = false;
protected:
	static void _bind_methods();
public:
	godot::TypedArray<HTMLTouchPoint> get_touches() const { return touches; }
	void set_touches(const godot::TypedArray<HTMLTouchPoint>& p_value) { touches = p_value; }
	godot::TypedArray<HTMLTouchPoint> get_target_touches() const { return target_touches; }
	void set_target_touches(const godot::TypedArray<HTMLTouchPoint>& p_value) { target_touches = p_value; }
	godot::TypedArray<HTMLTouchPoint> get_changed_touches() const { return changed_touches; }
	void set_changed_touches(const godot::TypedArray<HTMLTouchPoint>& p_value) { changed_touches = p_value; }
	bool is_shift_pressed() const { return shift; }
	void set_shift_pressed(bool p_value) { shift = p_value; }
	bool is_ctrl_pressed() const { return ctrl; }
	void set_ctrl_pressed(bool p_value) { ctrl = p_value; }
	bool is_alt_pressed() const { return alt; }
	void set_alt_pressed(bool p_value) { alt = p_value; }
	bool is_meta_pressed() const { return meta; }
	void set_meta_pressed(bool p_value) { meta = p_value; }
};

namespace dockit {
	// 构造一个已标记为 trusted 的空事件，按类型选择具体子类。
	godot::Ref<HTMLEvent> make_dom_event(const godot::StringName& p_type, bool p_bubbles, bool p_cancelable);
}
