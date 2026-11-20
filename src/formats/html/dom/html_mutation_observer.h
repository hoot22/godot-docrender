#pragma once

#include "html_node.h"

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <vector>

namespace dockit {
	struct HTMLMutation {
		enum Kind { CHILD_LIST, ATTRIBUTES, CHARACTER_DATA };
		Kind kind;
		godot::Ref<HTMLNode> target;
		godot::TypedArray<HTMLNode> added;
		godot::TypedArray<HTMLNode> removed;
		godot::Ref<HTMLNode> previous;
		godot::Ref<HTMLNode> next;
		godot::String attribute;
		godot::Variant old_value;
	};
}

class HTMLMutationObserver : public godot::RefCounted {
	GDCLASS(HTMLMutationObserver, godot::RefCounted)
	friend class dockit::HTMLDOM;

	struct Registration {
		std::weak_ptr<dockit::HTMLDOM> owner;
		godot::Ref<HTMLNode> target;
		bool child_list = false;
		bool attributes = false;
		bool character_data = false;
		bool subtree = false;
		bool attribute_old_value = false;
		bool character_data_old_value = false;
		bool has_attribute_filter = false;
		godot::PackedStringArray attribute_filter;
	};
	struct Transient {
		Registration registration;
		godot::Ref<HTMLNode> root;
	};
	struct Pending {
		std::weak_ptr<dockit::HTMLDOM> owner;
		godot::Dictionary record;
	};
	std::vector<Registration> registrations;
	std::vector<Transient> transient;
	std::vector<Pending> pending;
	uint64_t delivery_version = 0;

	static void _bind_methods();
	void enqueue(dockit::HTMLDOM& p_owner, const dockit::HTMLMutation& p_change,
		const std::vector<godot::Ref<HTMLNode>>& p_ancestry);
	void clear_owner(const dockit::HTMLDOM& p_owner);
	godot::TypedArray<godot::Dictionary> take_owner(dockit::HTMLDOM& p_owner);

public:
	godot::Error observe(const godot::Ref<HTMLNode>& p_target, const godot::Dictionary& p_options); // 观察节点及可选的子树
	void disconnect(); // 解除观察并清空待交付记录
	godot::TypedArray<godot::Dictionary> take_records(); // 取出并清空待交付记录
};
