#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/variant/variant.hpp>

class HTMLEvent;
class HTMLEventTarget;

class HTMLListenerHandle : public godot::RefCounted {
	GDCLASS(HTMLListenerHandle, godot::RefCounted)
	friend class HTMLEventTarget;
	godot::Ref<HTMLEventTarget> target;
	godot::StringName type;
	godot::Callable callable;
	bool capture = false;
	bool canceled = false;
protected:
	static void _bind_methods();
public:
	void cancel();
	bool is_canceled() const { return canceled; }
};

// DOM EventTarget。HTMLNode 与 HTMLWindow 的共同基类：监听器由所属文档的 HTMLDOM
// 统一持有，子类只负责把调用转发到自己的槽位（元素 / 文档 / 窗口）。
class HTMLEventTarget : public godot::RefCounted {
	GDCLASS(HTMLEventTarget, godot::RefCounted)
	godot::HashMap<godot::StringName, godot::Callable> event_handlers;
	godot::HashSet<godot::StringName> event_handler_overrides;

protected:
	static void _bind_methods();
	bool _set(const godot::StringName& p_name, const godot::Variant& p_value);
	bool _get(const godot::StringName& p_name, godot::Variant& r_value) const;
	void _get_property_list(godot::List<godot::PropertyInfo>* p_list) const;

public:
	bool has_event_handler_override(const godot::StringName& p_type) const;
	godot::Callable get_event_handler(const godot::StringName& p_type) const;
	virtual bool is_valid() const = 0; // 检查目标是否仍属于当前文档
	virtual godot::Error add_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
		const godot::Dictionary& p_options = {}) = 0; // 注册监听器
	godot::Ref<HTMLListenerHandle> listen(const godot::StringName& p_type, const godot::Callable& p_callable,
		const godot::Dictionary& p_options = {});
	virtual godot::Error remove_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
		const godot::Dictionary& p_options = {}) = 0; // 移除之前注册的监听器
	virtual bool dispatch_event(const godot::Ref<HTMLEvent>& p_event) = 0; // 从此目标派发事件，返回默认动作是否未被取消
	virtual bool has_event_listener(const godot::StringName& p_type) const = 0; // 检查是否注册了该类型的监听器
};
