#pragma once

#include "formats/html/events/html_event_target.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <memory>

namespace litehtml { class element; }
namespace dockit { class HTMLDOM; }
class HTMLLayout;
class HTMLElement;
class HTMLDocumentFragment;
class HTMLLabel;
class HTMLLabelCanvas;
class HTMLTextNode;
class HTMLRange;
class HTMLMutationObserver;
class HTMLEvent;
class HTMLWindow;

class HTMLNode : public HTMLEventTarget {
	GDCLASS(HTMLNode, HTMLEventTarget)
	friend class dockit::HTMLDOM;
	friend class HTMLLayout;
	friend class HTMLDocumentFragment;
	friend class ::HTMLLabel;
	friend class ::HTMLLabelCanvas;
	friend class HTMLComponent;
	friend class HTMLRange;
	friend class HTMLMutationObserver;

protected:
	struct DetachedTree;
	godot::ObjectID layout_id;
	uint64_t parse_version = 0;
	uint64_t adoption_version = 0;
	uint64_t document_revision = 1;
	std::weak_ptr<litehtml::element> element;
	std::shared_ptr<DetachedTree> detached_tree;

	static void _bind_methods();
	HTMLLayout* get_owner() const;
	dockit::HTMLDOM& get_dom() const;
	std::shared_ptr<litehtml::element> lock_element() const;
	std::shared_ptr<litehtml::element> lock_related_node(const godot::Ref<HTMLNode>& p_node) const;

public:
	enum NodeType { ELEMENT_NODE = 1, TEXT_NODE = 3, COMMENT_NODE = 8, DOCUMENT_NODE = 9, DOCUMENT_FRAGMENT_NODE = 11 };
	enum DocumentPosition {
		POSITION_DISCONNECTED = 1, POSITION_PRECEDING = 2, POSITION_FOLLOWING = 4,
		POSITION_CONTAINS = 8, POSITION_CONTAINED_BY = 16, POSITION_IMPLEMENTATION_SPECIFIC = 32,
	};

	virtual NodeType get_node_type() const = 0; // 返回节点类型
	bool is_valid() const override; // 检查句柄是否仍属于当前布局
	bool is_connected() const; // 检查节点是否连接到文档
	int64_t get_mutation_revision() const; // 返回逻辑节点的修改版本，失效句柄返回 0
	godot::Ref<HTMLNode> get_parent_node() const; // 返回父节点，包括文档节点和片段
	godot::TypedArray<HTMLNode> get_child_nodes() const; // 返回按树顺序排列的直接子节点快照
	godot::Ref<HTMLNode> get_first_child() const; // 返回第一个直接子节点
	godot::Ref<HTMLNode> get_next_sibling() const; // 返回下一个同父节点
	godot::Ref<HTMLElement> get_assigned_slot() const;
	godot::Ref<HTMLNode> get_root_node() const; // 返回当前树的根节点
	bool contains(const godot::Ref<HTMLNode>& p_node) const; // 检查是否包含该节点或其自身
	int32_t compare_document_position(const godot::Ref<HTMLNode>& p_node) const; // 返回节点间的位置标志
	bool is_equal_node(const godot::Ref<HTMLNode>& p_node) const; // 按类型、内容和子树比较节点
	godot::Error normalize(); // 移除空文本并合并相邻文本节点
	godot::Error add_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
		const godot::Dictionary& p_options = {}) override; // 为此节点注册 DOM 事件监听器
	godot::Error remove_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
		const godot::Dictionary& p_options = {}) override; // 移除之前注册的监听器
	bool dispatch_event(const godot::Ref<HTMLEvent>& p_event) override; // 从此节点派发事件，返回默认动作是否未被取消
	bool has_event_listener(const godot::StringName& p_type) const override; // 检查此节点是否注册了该类型的监听器
};

class HTMLDocumentNode : public HTMLNode {
	GDCLASS(HTMLDocumentNode, HTMLNode)
protected:
	static void _bind_methods();
public:
	NodeType get_node_type() const override { return DOCUMENT_NODE; } // 返回文档节点类型
	godot::String get_base_uri() const; // 返回当前文档的生效基准
	godot::String get_visibility_state() const;
	godot::String get_design_mode() const;
	void set_design_mode(const godot::String& p_mode);
	godot::Ref<HTMLElement> get_active_element() const;
	godot::Ref<HTMLElement> get_document_element() const; // 返回文档的根元素
	godot::Ref<HTMLElement> get_body() const; // 返回文档的正文元素
	godot::Ref<HTMLElement> query_selector(const godot::String& p_selector) const; // 返回文档中第一个匹配选择器的元素
	godot::TypedArray<HTMLElement> query_selector_all(const godot::String& p_selector) const; // 按文档顺序返回所有匹配元素的快照
	godot::Ref<HTMLElement> get_element_by_id(const godot::String& p_id) const; // 按区分大小写的原始 ID 查找第一个元素
	godot::Ref<HTMLElement> create_element(const godot::String& p_tag_name); // 创建属于此文档的分离元素
	godot::Ref<HTMLTextNode> create_text_node(const godot::String& p_data); // 创建属于此文档的分离文本节点
	godot::Ref<HTMLDocumentFragment> create_document_fragment(); // 创建属于此文档的空片段
	godot::Ref<HTMLWindow> get_default_view() const; // 返回文档的 window 事件目标
};

class HTMLTextNode : public HTMLNode {
	GDCLASS(HTMLTextNode, HTMLNode)
protected:
	static void _bind_methods();
public:
	NodeType get_node_type() const override { return TEXT_NODE; } // 返回文本节点类型
	godot::String get_data() const; // 返回完整的源文本
	godot::Error set_data(const godot::String& p_data); // 设置源文本并保留当前句柄
};

class HTMLCommentNode : public HTMLNode {
	GDCLASS(HTMLCommentNode, HTMLNode)
protected:
	static void _bind_methods();
public:
	NodeType get_node_type() const override { return COMMENT_NODE; } // 返回注释节点类型
	godot::String get_data() const; // 返回注释内容
	godot::Error set_data(const godot::String& p_data); // 设置注释内容
};

VARIANT_ENUM_CAST(HTMLNode::NodeType);
VARIANT_ENUM_CAST(HTMLNode::DocumentPosition);
