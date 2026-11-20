#pragma once

#include "html_element.h"

#include <cstdint>

class HTMLOListElement : public HTMLElement {
	GDCLASS(HTMLOListElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	int32_t get_start() const; // 读取起始编号属性，缺失或无效时返回 1
	void set_start(int32_t p_start); // 设置起始编号属性
	bool is_reversed() const; // 检查是否存在倒序属性
	void set_reversed(bool p_reversed); // 添加或移除倒序属性
	godot::String get_type() const; // 读取列表标记类型属性
	void set_type(const godot::String& p_type); // 设置列表标记类型属性
};

class HTMLUListElement : public HTMLElement {
	GDCLASS(HTMLUListElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLLIElement : public HTMLElement {
	GDCLASS(HTMLLIElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	int32_t get_value() const; // 读取项目编号属性，缺失或无效时返回 0
	void set_value(int32_t p_value); // 设置项目编号属性
	godot::String get_type() const; // 读取列表项的旧版标记类型属性
	void set_type(const godot::String& p_type); // 设置列表项的旧版标记类型属性
};
