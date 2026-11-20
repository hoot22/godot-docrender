#pragma once

#include "html_element.h"

class HTMLButtonElement : public HTMLElement {
	GDCLASS(HTMLButtonElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	godot::Ref<HTMLFormElement> get_form() const; // 返回所属表单
	bool is_disabled() const; // 判断是否存在 disabled 属性
	void set_disabled(bool p_disabled); // 添加或移除 disabled 属性
	godot::String get_type() const; // 返回按钮的源 type 属性
	void set_type(const godot::String& p_type); // 写入按钮的 type 属性
	godot::String get_name() const; // 返回按钮的源 name 属性
	void set_name(const godot::String& p_name); // 写入按钮的 name 属性
	godot::String get_value() const; // 返回按钮的源 value 属性
	void set_value(const godot::String& p_value); // 写入按钮的 value 属性
};
