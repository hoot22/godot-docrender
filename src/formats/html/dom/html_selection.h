#pragma once

#include "html_range.h"

class HTMLSelection : public godot::RefCounted {
	GDCLASS(HTMLSelection, godot::RefCounted)
	friend class dockit::HTMLDOM;
	friend class HTMLLayout;

	std::weak_ptr<dockit::HTMLDOM> owner;
	uint64_t version = 0;
	godot::Ref<HTMLRange> range;
	bool backward = false;

	static void _bind_methods();
	void use_range(const godot::Ref<HTMLRange>& p_range, bool p_backward);

public:
	bool is_valid() const; // 检查选区是否仍属于当前布局
	bool is_collapsed() const; // 检查选区是否为空或已折叠
	int64_t get_range_count() const; // 返回当前范围数量
	godot::Ref<HTMLRange> get_range_at(int64_t p_index) const; // 返回当前实时范围
	godot::Ref<HTMLNode> get_anchor_node() const; // 返回选区锚点节点
	int64_t get_anchor_offset() const; // 返回选区锚点偏移
	godot::Ref<HTMLNode> get_focus_node() const; // 返回选区焦点节点
	int64_t get_focus_offset() const; // 返回选区焦点偏移
	godot::Error add_range(const godot::Ref<HTMLRange>& p_range); // 关联当前文档内的实时范围
	void remove_all_ranges(); // 清除选区
	godot::Error collapse(const godot::Ref<HTMLNode>& p_node, int64_t p_offset = 0); // 将选区折叠到节点偏移
	godot::Error extend(const godot::Ref<HTMLNode>& p_node, int64_t p_offset = 0); // 保持锚点并移动焦点
	godot::Error set_base_and_extent(const godot::Ref<HTMLNode>& p_anchor, int64_t p_anchor_offset,
		const godot::Ref<HTMLNode>& p_focus, int64_t p_focus_offset); // 设置有方向的实时选区
	godot::Error select_all_children(const godot::Ref<HTMLNode>& p_node); // 选择指定节点的内容
	godot::String get_text() const; // 返回范围中的源文本
};
