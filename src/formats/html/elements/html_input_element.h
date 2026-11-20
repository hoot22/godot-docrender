#pragma once

#include "html_element.h"

#include <godot_cpp/classes/file_access.hpp>
#include <mutex>

namespace dockit { class HTMLInputNode; }

class HTMLInputFile : public godot::RefCounted {
	GDCLASS(HTMLInputFile, godot::RefCounted)
	friend class HTMLLabel;
	friend class HTMLFormElement;
	godot::Ref<godot::FileAccess> file;
	std::mutex read_mutex;
	godot::String name;
	int64_t size = 0;
	static godot::Ref<HTMLInputFile> open(const godot::String& p_path);

protected:
	static void _bind_methods();

public:
	godot::String get_name() const; // 返回文件名
	int64_t get_size() const; // 返回选择文件时的字节数
	godot::PackedByteArray read_bytes(int64_t p_offset, int64_t p_count); // 读取所选文件内的字节
};

class HTMLInputElement : public HTMLElement {
	GDCLASS(HTMLInputElement, HTMLElement)
	friend class ::HTMLLabel;
	friend class ::HTMLLabelCanvas;
	friend class HTMLLayout;

	std::shared_ptr<dockit::HTMLInputNode> get_input() const;

protected:
	static void _bind_methods();

public:
	godot::Ref<HTMLFormElement> get_form() const; // 返回所属表单
	godot::String get_type() const; // 返回有效的输入类型
	void set_type(const godot::String& p_value); // 设置输入类型属性
	double get_value_as_number() const; // 返回数值或 NaN
	void set_value_as_number(double p_value); // 设置数值字段的值
	godot::String get_value() const; // 返回输入框的当前值
	void set_value(const godot::String& p_value); // 设置当前值且不发出用户编辑信号
	bool is_checked() const; // 返回复选框的当前选中状态
	void set_checked(bool p_checked); // 设置当前选中状态且不修改 checked 属性
	bool is_indeterminate() const; // 返回复选框的混合状态
	void set_indeterminate(bool p_value); // 设置复选框的混合状态
	godot::Dictionary get_validity() const; // 返回当前约束校验标志
	godot::Error step_up(int p_count = 1); // 按当前类型的步长增加值
	godot::Error step_down(int p_count = 1); // 按当前类型的步长减少值
	godot::Dictionary get_value_as_date() const; // 返回日期时间分量
	godot::Error set_value_as_date(const godot::Dictionary& p_value); // 设置日期时间分量
	godot::TypedArray<HTMLInputFile> get_files() const; // 返回当前选择的只读文件句柄
	godot::Error show_picker(); // 在所属 HTMLLabel 中打开选择器
	int64_t get_selection_start() const; // 返回文本选区起点
	int64_t get_selection_end() const; // 返回文本选区终点
	void set_selection_range(int64_t p_start, int64_t p_end); // 按 Godot 字符索引设置文本选区
};
