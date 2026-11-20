#pragma once

#include "formats/html/elements/html_element.h"
#include "formats/html/events/html_event.h"
#include "formats/html/events/html_window.h"
#include "html_mutation_observer.h"
#include "html_selection.h"
#include "html_shadow_root.h"

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <litehtml/element.h>

#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

class HTMLShadowRoot;

namespace litehtml {
	class document;
}

struct HTMLNode::DetachedTree {
	std::weak_ptr<dockit::HTMLDOM> owner;
	std::weak_ptr<litehtml::element> host;
	litehtml::element::ptr root;
	bool public_fragment = false;

	DetachedTree(std::weak_ptr<dockit::HTMLDOM> p_owner, const litehtml::element::ptr& p_root);
	~DetachedTree();
};

namespace dockit {
	class HTMLTemplateNode;
	class HTMLDialogNode;

	class HTMLDOM : public std::enable_shared_from_this<HTMLDOM> {
		friend struct HTMLNode::DetachedTree;
		friend class ::HTMLRange;

		std::vector<godot::Ref<HTMLShadowRoot>> shadow_roots;
		bool assigning_slots = false;
		std::vector<godot::Ref<HTMLNode>> pending_slot_changes;
		bool slot_delivery_queued = false;
		static void deliver_slot_changes(uint64_t p_owner, uint64_t p_version);
		std::shared_ptr<litehtml::document> document;
		godot::ObjectID owner_id;
		uint64_t generation = 0;
		mutable godot::HashMap<const litehtml::element*, godot::ObjectID> node_handles;
		mutable godot::ObjectID document_handle;
		godot::HashMap<const litehtml::element*, std::weak_ptr<HTMLNode::DetachedTree>> detached_trees;
		struct SourceAttributes {
			std::weak_ptr<litehtml::element> element;
			litehtml::string_map values;
		};
		godot::HashMap<const litehtml::element*, SourceAttributes> source_attributes;
		std::vector<std::weak_ptr<HTMLDialogNode>> dialog_nodes;
		uint64_t last_dialog_sequence = 0;
		std::function<void()> on_change;
		std::function<void()> on_selection_change;
		uint64_t lifetime = 0;
		std::vector<godot::ObjectID> ranges;
		std::vector<godot::ObjectID> observers;
		godot::Ref<HTMLSelection> selection;
		bool observer_delivery_queued = false;
		bool delivering_observers = false;
		uint64_t observer_delivery_epoch = 0;

		// 事件监听器按目标槽位分开存放：元素按 litehtml 元素指针索引，document 与 window
		// 各有一份独立列表，避免"文档节点"和 <html> 元素共用同一个键而重复触发。
		struct EventListener {
			godot::Callable callable;
			godot::ObjectID owner;
			uint64_t id = 0;
			bool capture = false;
			bool once = false;
			bool passive = false;
		};
		struct ListenerStore {
			godot::HashMap<godot::StringName, std::vector<EventListener>> by_type;
			std::size_t count = 0;
		};
		enum class EventTargetSlot { ELEMENT, DOCUMENT, WINDOW };
		struct EventPathEntry {
			EventTargetSlot slot = EventTargetSlot::ELEMENT;
			const litehtml::element* element = nullptr;
			godot::Ref<HTMLNode> node;
			godot::Ref<HTMLWindow> window;
		};
		godot::HashMap<const litehtml::element*, ListenerStore> event_listeners;
		ListenerStore document_event_listeners;
		ListenerStore window_event_listeners;
		godot::HashMap<godot::StringName, std::size_t> event_counts_by_type;
		std::size_t event_listener_count = 0;
	bool event_debug = false;
	bool design_mode = false;
	godot::HashSet<int32_t> active_pointer_ids;
	godot::HashMap<int32_t, godot::Ref<HTMLElement>> pointer_capture_targets;
	struct PendingPointerCaptureEvent {
		godot::StringName type;
		godot::Ref<HTMLElement> target;
		int32_t pointer_id = 0;
	};
	std::vector<PendingPointerCaptureEvent> pending_pointer_capture_events;
	bool flushing_pointer_capture_events = false;
		uint64_t next_event_listener_id = 1;
		mutable godot::ObjectID window_handle;
		int event_dispatch_depth = 0;
		std::vector<godot::Ref<HTMLNode>> pending_scroll_targets;
		std::vector<godot::Ref<HTMLNode>> pending_scrollend_targets;
		bool scroll_delivery_queued = false;
		uint64_t scroll_delivery_epoch = 0;
		bool selectionchange_queued = false;
		uint64_t selectionchange_epoch = 0;
		bool scrollend_delivery_queued = false;
		uint64_t scrollend_delivery_epoch = 0;

		godot::Error register_event_listener(ListenerStore& r_store, const godot::StringName& p_type,
			const godot::Callable& p_callable, bool p_capture, bool p_once, bool p_passive);
		uint64_t allocate_event_listener_id();
		godot::Error unregister_event_listener(ListenerStore& r_store, const godot::StringName& p_type,
			const godot::Callable& p_callable, bool p_capture);
		static bool store_has_event_listener(const ListenerStore& p_store, const godot::StringName& p_type);
		void invoke_event_listeners(const EventPathEntry& p_entry, const godot::Ref<HTMLEvent>& p_event, int p_phase);

		struct ChildChange {
			struct Endpoint {
				godot::Ref<HTMLRange> range;
				bool start;
				godot::Ref<HTMLNode> branch;
				int64_t index;
			};
			godot::Ref<HTMLNode> target;
			godot::TypedArray<HTMLNode> before;
			std::vector<godot::Ref<HTMLNode>> ancestry;
			std::vector<Endpoint> endpoints;
		};
		ChildChange capture_children(const litehtml::element::ptr& p_parent);
		void finish_children(const ChildChange& p_change, const std::vector<litehtml::element::ptr>& p_moved = {});
		void publish_mutation(const HTMLMutation& p_change, const std::vector<godot::Ref<HTMLNode>>& p_ancestry);
		void publish_attribute(const litehtml::element::ptr& p_element, const char* p_name, const godot::Variant& p_old);
		void transfer_observers(HTMLDOM& p_source);
		static void deliver_observers(uint64_t p_owner_id, uint64_t p_epoch, uint64_t p_lifetime);
		godot::Error replace_character_data(const litehtml::element::ptr& p_node, const godot::String& p_data,
			int64_t p_offset, int64_t p_count, bool p_notify = true);
		bool can_insert_range_node(const litehtml::element::ptr& p_parent, const godot::Ref<HTMLNode>& p_node) const;
		bool range_can_wrap(const litehtml::element::ptr& p_node) const;
		godot::Ref<HTMLNode> split_range_text(const HTMLBoundary& p_boundary);

		struct AdoptedHandle {
			const litehtml::element* source;
			litehtml::element::ptr target;
			godot::Ref<HTMLNode> handle;
		};
		struct Adoption {
			std::vector<std::pair<litehtml::element::const_ptr, litehtml::element::ptr>> nodes;
			struct Fragment {
				godot::Ref<HTMLDocumentFragment> source;
				HTMLTemplateNode* target;
			};
			std::vector<AdoptedHandle> handles;
			std::vector<Fragment> fragments;
		};

		enum class SelectorQuery {
			FIRST,
			ALL,
			SELF,
			CLOSEST
		};

		godot::TypedArray<HTMLElement> select_elements(const godot::String& p_selector, SelectorQuery p_query,
			const litehtml::element::ptr& p_scope = {}) const;

		std::shared_ptr<HTMLNode::DetachedTree> get_detached_tree(const litehtml::element& p_element) const;
		std::shared_ptr<HTMLNode::DetachedTree> register_detached_tree(const litehtml::element::ptr& p_root);
		godot::Ref<HTMLDocumentFragment> create_document_fragment(const std::shared_ptr<litehtml::document>& p_document);
		bool is_host_including_ancestor(const litehtml::element& p_ancestor, litehtml::element::ptr p_node) const;
		void update_detached_handles(const litehtml::element::ptr& p_root,
			const std::shared_ptr<HTMLNode::DetachedTree>& p_tree);
		void retain_detached_subtree(const litehtml::element::ptr& p_root);
		void release_detached_root(const std::shared_ptr<HTMLNode::DetachedTree>& p_tree, const litehtml::element::ptr& p_node);
		godot::Error place_child(const litehtml::element::ptr& p_parent, const litehtml::element::ptr& p_child,
			const litehtml::element::ptr& p_reference, bool p_replace_reference, bool p_notify = true, bool* r_changed = nullptr);
		godot::Error detach_child(const litehtml::element::ptr& p_element, bool p_notify);
		void forget_subtree(const litehtml::element& p_element);
		godot::Ref<HTMLNode> find_node_handle(const litehtml::element& p_element) const;
		void bind_handle(HTMLNode& r_handle, const litehtml::element::ptr& p_node) const;
		void transfer_adoption(HTMLDOM& p_source, const litehtml::element& p_root, const Adoption& p_adoption);
		godot::String serialize(const litehtml::element& p_element, bool p_outer) const;
		godot::Ref<HTMLElement> make_detached_element(const litehtml::element::ptr& p_element);
		litehtml::element::ptr clone_subtree(const HTMLDOM& p_source_dom, const litehtml::element& p_source, bool p_deep,
			Adoption* r_adoption = nullptr);
		bool clone_children(const HTMLDOM& p_source_dom, const litehtml::element& p_source, litehtml::element& r_target,
			Adoption* r_adoption = nullptr);
		void set_source_attribute(const litehtml::element::ptr& p_element, const char* p_name, const char* p_value);
		void forget_source_attributes(const litehtml::element& p_element);

	public:
		godot::Ref<HTMLShadowRoot> attach_shadow(const litehtml::element::ptr& p_host, const godot::String& p_mode,
			const godot::String& p_assignment, bool p_delegates_focus);
		godot::Ref<HTMLShadowRoot> get_shadow_root(const litehtml::element::ptr& p_host, bool p_open_only = true) const;
		void synchronize_slots();
		void initialize_shadow_roots();
		godot::TypedArray<HTMLElement> component_candidates(const godot::String& p_tag) const;
		godot::TypedArray<HTMLNode> slot_nodes(const litehtml::element::ptr& p_slot, bool p_flatten) const;
		godot::Error assign_slot(const litehtml::element::ptr& p_slot, const godot::Array& p_nodes);
	public:
		struct Change {
			std::weak_ptr<litehtml::element> target;
			uint64_t revision;
			unsigned flags;
		};
		using Changes = std::vector<Change>;
		class ChangeScope {
			HTMLDOM& dom;
		public:
			explicit ChangeScope(HTMLDOM& p_dom) : dom(p_dom) { ++dom.change_depth; }
			~ChangeScope() { if (--dom.change_depth == 0) dom.flush_changes(); }
		};
	private:
		Changes pending_changes;
		std::unordered_map<const litehtml::element*, size_t> pending_change_indices;
		std::function<void(const Changes&)> on_mutation;
		int change_depth = 0;
		bool flushing_changes = false;
		bool changed_styles = false;
		void track_mutations(const litehtml::element::ptr& p_node) const;
		void track_node_mutations(const litehtml::element::ptr& p_node) const;
		void queue_change(litehtml::element& p_node, unsigned p_flags);
		void notify_changed();
	public:
		void set_mutation_callback(std::function<void(const Changes&)> p_callback) { on_mutation = std::move(p_callback); }
		void flush_changes();
		enum class TreeEdit { APPEND, PREPEND, BEFORE, AFTER, REPLACE, REPLACE_CHILDREN };
		enum class Lookup { TAG, CLASS, NAME };
		explicit HTMLDOM(std::function<void()> p_on_change);

		void set_document(std::shared_ptr<litehtml::document> p_document, godot::ObjectID p_owner_id, uint64_t p_generation);
		const std::shared_ptr<litehtml::document>& get_document() const { return document; }
		void track_dialog(const std::shared_ptr<HTMLDialogNode>& p_dialog);
		void sync_open_dialogs();
		std::shared_ptr<HTMLDialogNode> last_open_dialog() const;
		void clear();
		uint64_t get_lifetime() const { return lifetime; }
		HTMLLayout* get_layout() const;
		bool is_design_mode_enabled() const { return design_mode; }
		void set_design_mode_enabled(bool p_enabled) { design_mode = p_enabled; }
		godot::Ref<HTMLRange> create_range();
		godot::Ref<HTMLSelection> get_selection();
		void range_changed(const HTMLRange& p_range);
		void selection_changed() { queue_selectionchange(); if (on_selection_change) on_selection_change(); }
		void queue_selectionchange();
		static void deliver_selectionchange(uint64_t p_owner_id, uint64_t p_epoch, uint64_t p_lifetime);
		void set_selection_callback(std::function<void()> p_callback) { on_selection_change = std::move(p_callback); }
		void track_observer(godot::ObjectID p_id);
		void queue_observer_delivery();
		godot::Error add_event_listener(const litehtml::element::ptr& p_element, const godot::StringName& p_type,
			const godot::Callable& p_callable, bool p_capture, bool p_once, bool p_passive);
		godot::Error add_document_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
			bool p_capture, bool p_once, bool p_passive);
		godot::Error add_window_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
			bool p_capture, bool p_once, bool p_passive);
		godot::Error remove_event_listener(const litehtml::element::ptr& p_element, const godot::StringName& p_type,
			const godot::Callable& p_callable, bool p_capture);
		godot::Error remove_document_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
			bool p_capture);
		godot::Error remove_window_event_listener(const godot::StringName& p_type, const godot::Callable& p_callable,
			bool p_capture);
		bool dispatch_event(const godot::Ref<HTMLEventTarget>& p_target, const godot::Ref<HTMLEvent>& p_event);
		bool has_event_listeners() const { return event_listener_count > 0; }
		bool has_event_listener(const litehtml::element& p_element, const godot::StringName& p_type);
		bool has_document_event_listener(const godot::StringName& p_type) const;
		bool has_window_event_listener(const godot::StringName& p_type) const;
		bool has_event_listener_for_type(const godot::StringName& p_type) const;
	std::size_t get_event_listener_count(const godot::StringName& p_type) const;
	void set_event_debug(bool p_enabled) { event_debug = p_enabled; }
		std::size_t get_event_listener_count() const { return event_listener_count; }
		godot::Ref<HTMLWindow> get_window() const; // 返回当前文档的 window 事件目标
		uint64_t get_generation() const { return generation; }
		void clear_event_listeners();
	void begin_pointer(int32_t p_pointer_id);
	void end_pointer(int32_t p_pointer_id);
	void release_pointer_captures_in_subtree(const litehtml::element& p_element);
	void release_all_pointer_captures();
	godot::Error set_pointer_capture(const godot::Ref<HTMLElement>& p_target, int32_t p_pointer_id);
	godot::Error release_pointer_capture(int32_t p_pointer_id);
	bool has_pointer_capture(const godot::Ref<HTMLElement>& p_target, int32_t p_pointer_id) const;
	godot::Ref<HTMLElement> get_pointer_capture(int32_t p_pointer_id) const;
	void flush_pointer_capture_events();
		void forget_subtree_listeners(const litehtml::element& p_element);
		void transfer_listeners(HTMLDOM& p_source, const litehtml::element& p_from, const litehtml::element::ptr& p_to);
		void queue_scroll_event(const godot::Ref<HTMLNode>& p_node);
		static void deliver_scroll_events(uint64_t p_owner_id, uint64_t p_epoch, uint64_t p_lifetime);
		static void deliver_scrollend(uint64_t p_owner_id, uint64_t p_epoch, uint64_t p_lifetime);
		void normalize_details(const litehtml::element::ptr& p_tree, const std::vector<litehtml::element::ptr>& p_moved = {}, bool p_prefer_moved = false, bool p_notify = true);
		void form_state_changed(const litehtml::element::ptr& p_node);

		static godot::CharString normalize_tag_name(const godot::String& p_tag_name);
		static bool is_element(const litehtml::element& p_element);
		template <typename Visitor>
		static bool visit_elements(const litehtml::element::ptr& p_node, const Visitor& p_visit) {
			if (!p_node || litehtml::is_generated_tag(p_node->tag())) {
				return false;
			}
			if (is_element(*p_node) && p_visit(p_node)) {
				return true;
			}
			for (const auto& child : p_node->children()) {
				if (visit_elements(child, p_visit)) {
					return true;
				}
			}
			return false;
		}

		godot::Ref<HTMLElement> make_element(const litehtml::element::ptr& p_element) const;
		godot::Ref<HTMLNode> make_node(const litehtml::element::ptr& p_node) const;
		godot::Ref<HTMLDocumentNode> get_document_node() const;
		godot::Ref<HTMLNode> create_character_node(const godot::String& p_data, bool p_comment);
		godot::String character_data(const litehtml::element& p_node) const;
		HTMLBoundary text_boundary(const litehtml::element::ptr& p_part, int64_t p_offset) const;
		godot::Error set_character_data(const litehtml::element::ptr& p_node, const godot::String& p_data, bool p_notify = true);
		godot::Error normalize(const litehtml::element::ptr& p_node);
		godot::Error edit_nodes(const litehtml::element::ptr& p_target, TreeEdit p_edit, const godot::Array& p_items,
			bool p_notify = true);
		godot::Ref<HTMLDocumentFragment> parse_fragment(const godot::String& p_html, const litehtml::element::ptr& p_context);
		godot::Ref<HTMLDocumentFragment> import_fragment(const HTMLDocumentFragment& p_source);
		bool adopt_fragment(HTMLDocumentFragment& r_source);
		godot::TypedArray<HTMLElement> get_children(const litehtml::element& p_element) const;
		godot::TypedArray<HTMLElement> get_elements(const godot::String& p_value, Lookup p_lookup,
			const litehtml::element::ptr& p_scope = {}) const;
		godot::Ref<HTMLElement> create_element(const godot::CharString& p_tag);
		godot::Ref<HTMLDocumentFragment> create_document_fragment();
		litehtml::element::ptr create_template_node(const std::shared_ptr<litehtml::document>& p_document);
		godot::Ref<HTMLDocumentFragment> copy_fragment(const litehtml::element& p_root, bool p_deep);
		godot::Ref<HTMLElement> copy_node(const HTMLDOM& p_source_dom, const litehtml::element& p_source, bool p_deep);
		bool adopt_node(HTMLDOM& p_source_dom, const litehtml::element::ptr& p_source);
		godot::Ref<HTMLElement> query_selector(const godot::String& p_selector, const litehtml::element::ptr& p_scope = {}) const;
		godot::TypedArray<HTMLElement> query_selector_all(const godot::String& p_selector, const litehtml::element::ptr& p_scope = {}) const;
		bool matches(const godot::String& p_selector, const litehtml::element::ptr& p_scope) const;
		godot::Ref<HTMLElement> closest(const godot::String& p_selector, const litehtml::element::ptr& p_scope) const;
		godot::Ref<HTMLElement> get_element_by_id(const godot::String& p_id, const litehtml::element::ptr& p_root = {}) const;
		bool is_element_connected(const litehtml::element& p_element) const;
		void remember_source_attributes(const litehtml::element::ptr& p_element, const litehtml::string_map& p_values);
		const litehtml::string_map* get_source_attributes(const litehtml::element& p_element) const;
		godot::Dictionary get_element_attributes(const litehtml::element& p_element) const;
		const char* get_element_attribute(const litehtml::element& p_element, const char* p_name) const;
		godot::String serialize_children(const litehtml::element& p_element) const;
		godot::String serialize_element(const litehtml::element& p_element) const;
		godot::Error edit_attribute(const litehtml::element::ptr& p_element, const char* p_name, const char* p_value);
		godot::Error replace_children(const litehtml::element::ptr& p_element, const godot::String& p_content, bool p_parse_html);
		godot::Error insert_child(const litehtml::element::ptr& p_parent, const litehtml::element::ptr& p_child,
			const litehtml::element::ptr& p_reference);
		godot::Error set_table_child(const litehtml::element::ptr& p_table, const litehtml::element::ptr& p_new_child,
			const litehtml::element::ptr& p_old_child, const litehtml::element::ptr& p_reference);
		godot::Error insert_fragment(const litehtml::element::ptr& p_parent, const HTMLDocumentFragment& p_fragment,
			const litehtml::element::ptr& p_reference);
		godot::Error replace_child(const litehtml::element::ptr& p_parent, const litehtml::element::ptr& p_new_child,
			const litehtml::element::ptr& p_old_child);
		godot::Error remove_child(const litehtml::element::ptr& p_parent, const litehtml::element::ptr& p_child);
		godot::Error detach_element(const litehtml::element::ptr& p_element);
		godot::Error remove_element(const litehtml::element::ptr& p_element);
	};
}
