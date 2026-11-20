#pragma once

#include "html_element.h"

class HTMLImageElement : public HTMLElement {
	GDCLASS(HTMLImageElement, HTMLElement)

	int64_t get_dimension_hint(const godot::String& p_attribute) const;
	void set_dimension_hint(const godot::String& p_attribute, int64_t p_pixels);

protected:
	static void _bind_methods();

public:
	godot::String get_src() const; // 返回图片的源 src 属性
	void set_src(const godot::String& p_src); // 写入图片的 src 属性
	godot::String get_srcset() const; // 返回源 srcset 属性
	void set_srcset(const godot::String& p_srcset); // 写入图片的 srcset 属性
	godot::String get_sizes() const; // 返回源 sizes 属性
	void set_sizes(const godot::String& p_sizes); // 写入图片的 sizes 属性
	godot::String get_current_src() const; // 返回最近一次成功排版选中的资源路径
	godot::String get_alt() const; // 返回图片的替代文字属性
	void set_alt(const godot::String& p_alt); // 写入图片的替代文字属性
	int64_t get_width_hint() const; // 返回源 width 属性的整数像素提示
	void set_width_hint(int64_t p_width); // 写入 HTML 图片宽度提示
	int64_t get_height_hint() const; // 返回源 height 属性的整数像素提示
	void set_height_hint(int64_t p_height); // 写入 HTML 图片高度提示
	godot::Vector2 get_intrinsic_size() const; // 返回当前排版所用的固有尺寸，不可用时返回零
};
