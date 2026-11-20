#pragma once

#include "html_element.h"

class HTMLDialogElement;

class HTMLDialogCloseRequest : public godot::RefCounted {
	GDCLASS(HTMLDialogCloseRequest, godot::RefCounted)

	friend class ::HTMLDialogElement;

public:
	enum Reason { SCRIPT, ESCAPE, BACKDROP };

private:
	Reason reason = SCRIPT;
	godot::Variant return_value;
	bool dispatching = false;
	bool default_prevented = false;

protected:
	static void _bind_methods();

public:
	Reason get_reason() const { return reason; } // 返回发起关闭请求的来源
	godot::Variant get_return_value() const { return return_value; } // 返回请求携带的结果，空值代表保留原结果
	void prevent_default() { if (dispatching) default_prevented = true; } // 在信号回调中取消此次关闭
	bool is_default_prevented() const { return default_prevented; } // 返回此次请求是否已取消
};

VARIANT_ENUM_CAST(HTMLDialogCloseRequest::Reason);

class HTMLDialogElement : public HTMLElement {
	GDCLASS(HTMLDialogElement, HTMLElement)

	friend class ::HTMLLabel;
	friend class ::HTMLLabelCanvas;

	godot::Error open_dialog(bool p_modal);
	uint64_t get_open_sequence() const;
	godot::Error dispatch_close_request(HTMLDialogCloseRequest::Reason p_reason, const godot::Variant& p_return_value = {});
	void emit_closed(const godot::String& p_value);

protected:
	static void _bind_methods();

public:
	bool is_open() const; // 返回对话框的打开属性
	void set_open(bool p_open); // 设置非模态打开属性或移除该属性
	bool is_modal() const; // 判断对话框是否位于当前文档的模态层
	godot::String get_closed_by() const; // 返回当前生效的关闭策略
	void set_closed_by(const godot::String& p_value); // 设置 closedby 属性
	godot::String get_return_value() const; // 返回最近设置的结果
	void set_return_value(const godot::String& p_value); // 设置对话框结果
	godot::Error show(); // 以非模态模式打开对话框
	godot::Error show_modal(); // 将已连接的对话框放入文档顶层
	godot::Error close(const godot::Variant& p_return_value = {}); // 关闭对话框并可选地更新结果
	godot::Error request_close(const godot::Variant& p_return_value = {}); // 发起可取消的脚本关闭请求
};
