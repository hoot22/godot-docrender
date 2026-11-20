#pragma once

#include "formats/html/elements/html_element.h"

namespace dockit { class HTMLTemplateNode; }

class HTMLDocumentFragment : public HTMLNode {
	GDCLASS(HTMLDocumentFragment, HTMLNode)

	friend class dockit::HTMLDOM;
	friend class dockit::HTMLTemplateNode;
protected:
	static void _bind_methods();

public:
	NodeType get_node_type() const override { return DOCUMENT_FRAGMENT_NODE; } // 返回文档片段节点类型
	godot::Error append_child(const godot::Ref<HTMLNode>& p_child); // 将同一布局中的节点移到片段末尾
	godot::Error insert_before(const godot::Ref<HTMLNode>& p_child, const godot::Ref<HTMLNode>& p_reference = {}); // 在指定子节点前插入
	godot::Error remove_child(const godot::Ref<HTMLNode>& p_child); // 分离子节点并保留句柄
	godot::Error replace_child(const godot::Ref<HTMLNode>& p_new_child, const godot::Ref<HTMLNode>& p_old_child); // 替换子节点并保留移出的句柄
	godot::Error replace_children(const godot::Array& p_items); // 整批替换片段子节点
	godot::TypedArray<HTMLElement> get_children() const;				// 按顺序返回片段的直接子元素
	godot::Ref<HTMLElement> get_first_element_child() const; // 返回第一个直接子元素
	godot::Ref<HTMLElement> get_last_element_child() const; // 返回最后一个直接子元素
	int64_t get_child_element_count() const; // 返回直接子元素数量
	godot::Ref<HTMLDocumentFragment> clone_node(bool p_deep = false) const;	// 创建独立片段，可选复制其中的全部内容
	godot::Ref<HTMLElement> query_selector(const godot::String& p_selector) const;	// 返回片段中第一个匹配选择器的元素
	godot::TypedArray<HTMLElement> query_selector_all(const godot::String& p_selector) const;	// 按树顺序返回片段中所有匹配元素的快照
	godot::Ref<HTMLElement> get_element_by_id(const godot::String& p_id) const;	// 按区分大小写的原始 ID 查找片段中的第一个元素
};
