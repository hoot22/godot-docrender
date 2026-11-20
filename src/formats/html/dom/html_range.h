#pragma once

#include "html_node.h"

#include <godot_cpp/variant/rect2.hpp>

namespace dockit {
	struct HTMLBoundary {
		godot::Ref<HTMLNode> node;
		int64_t offset = 0;
	};

	int compare_html_boundaries(const HTMLBoundary& p_left, const HTMLBoundary& p_right);
	int64_t html_node_length(const HTMLNode& p_node);
}

class HTMLSelection;

class HTMLRange : public godot::RefCounted {
	GDCLASS(HTMLRange, godot::RefCounted)
	friend class dockit::HTMLDOM;
	friend class HTMLLayout;
	friend class HTMLSelection;

	std::weak_ptr<dockit::HTMLDOM> owner;
	uint64_t version = 0;
	dockit::HTMLBoundary start;
	dockit::HTMLBoundary end;

	static void _bind_methods();
	bool accepts(const dockit::HTMLBoundary& p_boundary) const;
	void set_boundaries(dockit::HTMLBoundary p_start, dockit::HTMLBoundary p_end);
	enum class Contents { CLONE, EXTRACT, DELETE };
	godot::Ref<HTMLDocumentFragment> process_contents(Contents p_action);

public:
	bool is_valid() const; // 检查范围是否仍属于当前布局
	bool is_collapsed() const; // 检查范围的端点是否重合
	godot::Ref<HTMLNode> get_start_container() const; // 返回起点所在的节点
	godot::Ref<HTMLNode> get_end_container() const; // 返回终点所在的节点
	int64_t get_start_offset() const; // 返回起点的字符或子节点偏移
	int64_t get_end_offset() const; // 返回终点的字符或子节点偏移
	godot::Error set_start(const godot::Ref<HTMLNode>& p_node, int64_t p_offset); // 设置起点
	godot::Error set_end(const godot::Ref<HTMLNode>& p_node, int64_t p_offset); // 设置终点
	godot::Error collapse(bool p_to_start = false); // 将范围折叠到指定端点
	godot::Error select_node(const godot::Ref<HTMLNode>& p_node); // 选择节点本身
	godot::Error select_node_contents(const godot::Ref<HTMLNode>& p_node); // 选择节点的内容
	godot::String get_text() const; // 返回范围中的源文本
	godot::TypedArray<godot::Rect2> get_client_rects() const; // 返回可见文本片段的文档坐标矩形
	godot::Ref<HTMLDocumentFragment> clone_contents(); // 深复制范围中的内容
	godot::Ref<HTMLDocumentFragment> extract_contents(); // 提取内容并保留完整移出节点的句柄
	godot::Error delete_contents(); // 删除范围中的内容
	godot::Error insert_node(const godot::Ref<HTMLNode>& p_node); // 在起点插入节点或片段
	godot::Error surround_contents(const godot::Ref<HTMLElement>& p_element); // 用元素包裹范围内的内容
};
