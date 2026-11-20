#pragma once

#include "html_element.h"

class HTMLHtmlElement : public HTMLElement {
	GDCLASS(HTMLHtmlElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLHeadElement : public HTMLElement {
	GDCLASS(HTMLHeadElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLBodyElement : public HTMLElement {
	GDCLASS(HTMLBodyElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLDivElement : public HTMLElement {
	GDCLASS(HTMLDivElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLParagraphElement : public HTMLElement {
	GDCLASS(HTMLParagraphElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLSpanElement : public HTMLElement {
	GDCLASS(HTMLSpanElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLPreElement : public HTMLElement {
	GDCLASS(HTMLPreElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLArticleElement : public HTMLElement {
	GDCLASS(HTMLArticleElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLSectionElement : public HTMLElement {
	GDCLASS(HTMLSectionElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLDListElement : public HTMLElement {
	GDCLASS(HTMLDListElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	bool is_compact() const; // 读取紧凑显示属性
	void set_compact(bool p_value); // 设置紧凑显示属性
};

class HTMLMenuElement : public HTMLElement {
	GDCLASS(HTMLMenuElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	bool is_compact() const; // 读取紧凑显示属性
	void set_compact(bool p_value); // 设置紧凑显示属性
};

class HTMLQuoteElement : public HTMLElement {
	GDCLASS(HTMLQuoteElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	godot::String get_cite() const; // 读取引文来源
	void set_cite(const godot::String& p_value); // 设置引文来源
};

class HTMLBRElement : public HTMLElement {
	GDCLASS(HTMLBRElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	godot::String get_clear() const; // 读取换行清除属性
	void set_clear(const godot::String& p_value); // 设置换行清除属性
};

class HTMLHRElement : public HTMLElement {
	GDCLASS(HTMLHRElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	godot::String get_align() const; // 读取对齐属性
	void set_align(const godot::String& p_value); // 设置对齐属性
	godot::String get_color() const; // 读取颜色属性
	void set_color(const godot::String& p_value); // 设置颜色属性
	bool is_no_shade() const; // 读取无阴影属性
	void set_no_shade(bool p_value); // 设置无阴影属性
	godot::String get_size() const; // 读取粗细属性
	void set_size(const godot::String& p_value); // 设置粗细属性
	godot::String get_width() const; // 读取宽度属性
	void set_width(const godot::String& p_value); // 设置宽度属性
};
