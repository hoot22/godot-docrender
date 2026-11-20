#pragma once

#include "html_element.h"

class HTMLProgressElement : public HTMLElement {
	GDCLASS(HTMLProgressElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	double get_value() const; // 返回已限幅的进度值
	void set_value(double p_value); // 设置进度值
	double get_max() const; // 返回有效的进度上限
	void set_max(double p_max); // 设置进度上限
	double get_position() const; // 返回进度比例，不确定时返回 -1
	godot::TypedArray<HTMLElement> get_labels() const; // 返回关联的标签
};

class HTMLMeterElement : public HTMLElement {
	GDCLASS(HTMLMeterElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	double get_min() const; // 返回规范化后的计量值
	void set_min(double p_value); // 设置计量属性
	double get_max() const; // 返回规范化后的计量值
	void set_max(double p_value); // 设置计量属性
	double get_value() const; // 返回规范化后的计量值
	void set_value(double p_value); // 设置计量属性
	double get_low() const; // 返回规范化后的计量值
	void set_low(double p_value); // 设置计量属性
	double get_high() const; // 返回规范化后的计量值
	void set_high(double p_value); // 设置计量属性
	double get_optimum() const; // 返回规范化后的计量值
	void set_optimum(double p_value); // 设置计量属性
	godot::TypedArray<HTMLElement> get_labels() const; // 返回关联的标签
};
