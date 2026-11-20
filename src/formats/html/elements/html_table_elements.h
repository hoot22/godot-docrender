#pragma once

#include "html_element.h"

class HTMLTableCaptionElement : public HTMLElement {
	GDCLASS(HTMLTableCaptionElement, HTMLElement)

protected:
	static void _bind_methods() {}
};

class HTMLTableColElement : public HTMLElement {
	GDCLASS(HTMLTableColElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	int64_t get_span() const; // 返回列或列组的跨度
	void set_span(int64_t p_span); // 写入列或列组的跨度属性
};

class HTMLTableCellElement : public HTMLElement {
	GDCLASS(HTMLTableCellElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	int64_t get_cell_index() const; // 返回在所属行中的零基索引
	int64_t get_col_span() const; // 返回跨列数
	void set_col_span(int64_t p_span); // 写入跨列属性
	int64_t get_row_span() const; // 返回跨行数，零表示延伸至行组末尾
	void set_row_span(int64_t p_span); // 写入跨行属性
	godot::String get_headers() const; // 返回关联表头的 ID 字符串
	void set_headers(const godot::String& p_value); // 写入关联表头的 ID 字符串
	godot::String get_scope() const; // 返回已知的表头作用范围关键字
	void set_scope(const godot::String& p_value); // 写入表头作用范围属性
	godot::String get_abbr() const; // 返回表头的简写
	void set_abbr(const godot::String& p_value); // 写入表头的简写
};

class HTMLTableRowElement : public HTMLElement {
	GDCLASS(HTMLTableRowElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	int64_t get_row_index() const; // 返回在所属表格中的零基索引
	int64_t get_section_row_index() const; // 返回在父行组或表格中的零基索引
	godot::TypedArray<HTMLTableCellElement> get_cells() const; // 返回直属单元格的快照
	godot::Ref<HTMLTableCellElement> insert_cell(int64_t p_index = -1); // 插入一个数据单元格
	godot::Error delete_cell(int64_t p_index); // 分离指定单元格
};

class HTMLTableSectionElement : public HTMLElement {
	GDCLASS(HTMLTableSectionElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	godot::TypedArray<HTMLTableRowElement> get_rows() const; // 返回直属行的快照
	godot::Ref<HTMLTableRowElement> insert_row(int64_t p_index = -1); // 在行组中插入一行
	godot::Error delete_row(int64_t p_index); // 分离行组中的指定行
};

class HTMLTableElement : public HTMLElement {
	GDCLASS(HTMLTableElement, HTMLElement)

	godot::Ref<HTMLElement> get_table_part(const char* p_tag) const;
	godot::Error set_table_part(const char* p_tag, const godot::Ref<HTMLElement>& p_value);
	godot::Ref<HTMLElement> create_table_part(const char* p_tag);

protected:
	static void _bind_methods();

public:
	godot::Ref<HTMLTableCaptionElement> get_caption() const; // 返回第一个直属标题
	godot::Error set_caption(const godot::Ref<HTMLTableCaptionElement>& p_value); // 移入或清除表格标题
	godot::Ref<HTMLTableCaptionElement> create_caption(); // 返回现有标题或创建标题
	godot::Error delete_caption(); // 分离第一个直属标题
	godot::Ref<HTMLTableSectionElement> get_t_head() const; // 返回第一个直属表头
	godot::Error set_t_head(const godot::Ref<HTMLTableSectionElement>& p_value); // 移入或清除表头
	godot::Ref<HTMLTableSectionElement> create_t_head(); // 返回现有表头或创建表头
	godot::Error delete_t_head(); // 分离第一个直属表头
	godot::Ref<HTMLTableSectionElement> get_t_foot() const; // 返回第一个直属表尾
	godot::Error set_t_foot(const godot::Ref<HTMLTableSectionElement>& p_value); // 移入或清除表尾
	godot::Ref<HTMLTableSectionElement> create_t_foot(); // 返回现有表尾或创建表尾
	godot::Error delete_t_foot(); // 分离第一个直属表尾
	godot::TypedArray<HTMLTableSectionElement> get_t_bodies() const; // 返回直属表体的快照
	godot::Ref<HTMLTableSectionElement> create_t_body(); // 创建并插入一个表体
	godot::TypedArray<HTMLTableRowElement> get_rows() const; // 按表格顺序返回行的快照
	godot::Ref<HTMLTableRowElement> insert_row(int64_t p_index = -1); // 在表格中插入一行
	godot::Error delete_row(int64_t p_index); // 分离表格中的指定行
};
