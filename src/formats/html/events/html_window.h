#pragma once

#include "formats/html/events/html_event_target.h"
#include "formats/html/dom/html_node.h"

#include <godot_cpp/core/object_id.hpp>

// DOM Window：文档的全局事件目标。事件路径为 window -> document -> 元素链，
// 因此脚本可以把监听器挂在 window 上观察整篇文档的冒泡事件。
class HTMLWindow : public HTMLEventTarget {
	GDCLASS(HTMLWindow, HTMLEventTarget)
	friend class dockit::HTMLDOM;

	godot::ObjectID layout_id;
	uint64_t parse_version = 0;

protected:
	static void _bind_methods();

	class HTMLLayout* get_owner() const;

public:
	bool is_valid() const override; // 检查窗口是否仍属于当前文档
	godot::Ref<HTMLDocumentNode> get_document() const; // 返回窗口关联的文档节点
	godot::Error add_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
		const godot::Dictionary& p_options = {}) override;
	godot::Error remove_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
		const godot::Dictionary& p_options = {}) override;
	bool dispatch_event(const godot::Ref<HTMLEvent>& p_event) override; // 从 window 派发事件，路径只包含 window
	bool has_event_listener(const godot::StringName& p_type) const override;
};
