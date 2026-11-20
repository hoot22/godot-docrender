#pragma once

#include "html_element.h"

namespace dockit { class HTMLTextAreaNode; }

class HTMLTextAreaElement : public HTMLElement {
	GDCLASS(HTMLTextAreaElement, HTMLElement)

	std::shared_ptr<dockit::HTMLTextAreaNode> get_textarea() const;

protected:
	static void _bind_methods();

public:
	godot::Ref<HTMLFormElement> get_form() const; // 返回所属表单
	godot::String get_value() const; // 返回多行字段的当前值
	void set_value(const godot::String& p_value); // 设置多行字段的当前值且不触发用户编辑信号
	godot::String get_default_value() const; // 返回来自子文本的默认值
	void set_default_value(const godot::String& p_value); // 设置字段的子文本和默认值
	int64_t get_selection_start() const; // 返回选区起点
	int64_t get_selection_end() const; // 返回选区终点
	void set_selection_range(int64_t p_start, int64_t p_end); // 按 Godot 字符索引设置选区
};
