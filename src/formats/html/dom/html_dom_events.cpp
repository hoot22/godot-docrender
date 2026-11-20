#include "html_shadow_root.h"
#include "html_dom.h"

#include "formats/html/events/html_event.h"
#include "formats/html/html_layout.h"
#include "nodes/html_label.h"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <litehtml/document.h>

#include <algorithm>
#include <cstddef>
#include <utility>

using namespace godot;
using dockit::HTMLDOM;

namespace {
	// 防御监听器内部再次派发同一事件造成的无限递归
	constexpr int MAX_EVENT_DISPATCH_DEPTH = 32;
	constexpr std::size_t MAX_EVENT_PATH_LENGTH = 1024;

	class EventDispatchScope {
		int& depth;
		Ref<HTMLEvent> event;
		std::function<void()> on_outermost_end;
		bool finished = false;

	public:
		EventDispatchScope(int& p_depth, const Ref<HTMLEvent>& p_event, std::function<void()> p_on_outermost_end) :
			depth(p_depth), event(p_event), on_outermost_end(std::move(p_on_outermost_end)) {
			++depth;
		}
		EventDispatchScope(const EventDispatchScope&) = delete;
		EventDispatchScope& operator=(const EventDispatchScope&) = delete;

		void finish() {
			if (finished) return;
			finished = true;
			--depth;
			event->end_dispatch();
			if (depth == 0 && on_outermost_end) on_outermost_end();
		}
		~EventDispatchScope() { finish(); }
	};
}

Error HTMLDOM::register_event_listener(ListenerStore& r_store, const StringName& p_type,
	const Callable& p_callable, bool p_capture, bool p_once, bool p_passive) {
	if (p_callable.is_null()) return ERR_INVALID_PARAMETER;
	if (p_type == StringName()) return ERR_INVALID_PARAMETER;
	auto& list = r_store.by_type[p_type];
	for (const auto& entry : list) {
		// 与 DOM 一致：同一 (类型, 回调, capture) 只注册一次
		if (entry.capture == p_capture && entry.callable == p_callable) return OK;
	}
	EventListener listener;
	listener.callable = p_callable;
	const Object* object = p_callable.get_object();
	listener.owner = object ? ObjectID(object->get_instance_id()) : ObjectID();
	listener.id = allocate_event_listener_id();
	listener.capture = p_capture;
	listener.once = p_once;
	listener.passive = p_passive;
	list.push_back(listener);
	++r_store.count;
	++event_listener_count;
	++event_counts_by_type[p_type];
	return OK;
}

uint64_t HTMLDOM::allocate_event_listener_id() {
	const uint64_t id = next_event_listener_id++;
	if (next_event_listener_id == 0) next_event_listener_id = 1;
	return id;
}

Error HTMLDOM::unregister_event_listener(ListenerStore& r_store, const StringName& p_type,
	const Callable& p_callable, bool p_capture) {
	auto* list_ptr = r_store.by_type.getptr(p_type);
	if (!list_ptr) return OK;
	auto& list = *list_ptr;
	const std::size_t before = list.size();
	list.erase(std::remove_if(list.begin(), list.end(), [&](const EventListener& p_entry) {
		return p_entry.capture == p_capture && p_entry.callable == p_callable;
		}), list.end());
	const std::size_t removed = before - list.size();
	if (removed == 0) return OK;
	r_store.count -= removed;
	event_listener_count -= removed;
	if (auto* count = event_counts_by_type.getptr(p_type)) {
		if (*count > removed) *count -= removed;
		else event_counts_by_type.erase(p_type);
	}
	if (list.empty()) r_store.by_type.erase(p_type);
	return OK;
}

bool HTMLDOM::store_has_event_listener(const ListenerStore& p_store, const StringName& p_type) {
	const auto* list_ptr = p_store.by_type.getptr(p_type);
	return list_ptr && !list_ptr->empty();
}

Error HTMLDOM::add_event_listener(const litehtml::element::ptr& p_element, const StringName& p_type,
	const Callable& p_callable, bool p_capture, bool p_once, bool p_passive) {
	if (!p_element) return ERR_INVALID_PARAMETER;
	if (p_callable.is_null() || p_type == StringName()) return ERR_INVALID_PARAMETER;
	return register_event_listener(event_listeners[p_element.get()], p_type, p_callable, p_capture, p_once, p_passive);
}

Error HTMLDOM::add_document_event_listener(const StringName& p_type, const Callable& p_callable,
	bool p_capture, bool p_once, bool p_passive) {
	return register_event_listener(document_event_listeners, p_type, p_callable, p_capture, p_once, p_passive);
}

Error HTMLDOM::add_window_event_listener(const StringName& p_type, const Callable& p_callable,
	bool p_capture, bool p_once, bool p_passive) {
	return register_event_listener(window_event_listeners, p_type, p_callable, p_capture, p_once, p_passive);
}

Error HTMLDOM::remove_event_listener(const litehtml::element::ptr& p_element, const StringName& p_type,
	const Callable& p_callable, bool p_capture) {
	if (!p_element) return ERR_INVALID_PARAMETER;
	auto* store = event_listeners.getptr(p_element.get());
	if (!store) return OK;
	const Error result = unregister_event_listener(*store, p_type, p_callable, p_capture);
	if (store->count == 0) event_listeners.erase(p_element.get());
	return result;
}

Error HTMLDOM::remove_document_event_listener(const StringName& p_type, const Callable& p_callable, bool p_capture) {
	return unregister_event_listener(document_event_listeners, p_type, p_callable, p_capture);
}

Error HTMLDOM::remove_window_event_listener(const StringName& p_type, const Callable& p_callable, bool p_capture) {
	return unregister_event_listener(window_event_listeners, p_type, p_callable, p_capture);
}

bool HTMLDOM::has_event_listener(const litehtml::element& p_element, const StringName& p_type) {
	const auto* store = event_listeners.getptr(&p_element);
	return store && store_has_event_listener(*store, p_type);
}

bool HTMLDOM::has_document_event_listener(const StringName& p_type) const {
	return store_has_event_listener(document_event_listeners, p_type);
}

bool HTMLDOM::has_window_event_listener(const StringName& p_type) const {
	return store_has_event_listener(window_event_listeners, p_type);
}

bool HTMLDOM::has_event_listener_for_type(const StringName& p_type) const {
	const auto* count = event_counts_by_type.getptr(p_type);
	return count && *count > 0;
}

std::size_t HTMLDOM::get_event_listener_count(const StringName& p_type) const {
	const auto* count = event_counts_by_type.getptr(p_type);
	return count ? *count : 0;
}

Ref<HTMLWindow> HTMLDOM::get_window() const {
	if (!document || !document->root()) return Ref<HTMLWindow>();
	if (auto* handle = Object::cast_to<HTMLWindow>(ObjectDB::get_instance(window_handle))) {
		return Ref<HTMLWindow>(handle);
	}
	Ref<HTMLWindow> handle;
	handle.instantiate();
	handle->layout_id = owner_id;
	handle->parse_version = generation;
	window_handle = ObjectID(handle->get_instance_id());
	return handle;
}

void HTMLDOM::clear_event_listeners() {
	release_all_pointer_captures();
	event_listeners.clear();
	document_event_listeners.by_type.clear();
	document_event_listeners.count = 0;
	window_event_listeners.by_type.clear();
	window_event_listeners.count = 0;
	event_counts_by_type.clear();
	event_listener_count = 0;
	window_handle = ObjectID();
	pending_scroll_targets.clear();
	scroll_delivery_queued = false;
	++scroll_delivery_epoch;
	pointer_capture_targets.clear();
	pending_pointer_capture_events.clear();
}

void HTMLDOM::forget_subtree_listeners(const litehtml::element& p_element) {
	if (event_listener_count == 0) return;
	const auto visit = [&](const auto& p_self, const litehtml::element& p_node) -> void {
		if (auto* store = event_listeners.getptr(&p_node)) {
			for (const auto& entry : store->by_type) {
				event_listener_count -= entry.value.size();
				if (auto* count = event_counts_by_type.getptr(entry.key)) {
					if (*count > entry.value.size()) *count -= entry.value.size();
					else event_counts_by_type.erase(entry.key);
				}
			}
			event_listeners.erase(&p_node);
		}
		for (const auto& child : p_node.children()) p_self(p_self, *child);
		};
	visit(visit, p_element);
}

void HTMLDOM::begin_pointer(int32_t p_pointer_id) {
	active_pointer_ids.insert(p_pointer_id);
}

void HTMLDOM::end_pointer(int32_t p_pointer_id) {
	if (active_pointer_ids.has(p_pointer_id)) release_pointer_capture(p_pointer_id);
	active_pointer_ids.erase(p_pointer_id);
	if (event_dispatch_depth == 0) flush_pointer_capture_events();
}

void HTMLDOM::release_pointer_captures_in_subtree(const litehtml::element& p_element) {
	std::vector<int32_t> pointer_ids;
	for (const auto& entry : pointer_capture_targets) {
		if (entry.value.is_null() || !entry.value->is_valid()) continue;
		const auto captured_element = entry.value->lock_element();
		if (captured_element && is_host_including_ancestor(p_element, captured_element)) {
			pointer_ids.push_back(entry.key);
		}
	}
	for (const int32_t pointer_id : pointer_ids) {
		if (active_pointer_ids.has(pointer_id)) release_pointer_capture(pointer_id);
		else pointer_capture_targets.erase(pointer_id);
	}
}

void HTMLDOM::release_all_pointer_captures() {
	struct Capture {
		Ref<HTMLElement> target;
		int32_t pointer_id = 0;
	};
	std::vector<Capture> captures;
	captures.reserve(pointer_capture_targets.size());
	for (const auto& entry : pointer_capture_targets) captures.push_back({ entry.value, entry.key });
	pointer_capture_targets.clear();
	active_pointer_ids.clear();
	pending_pointer_capture_events.clear();
	for (const Capture& capture : captures) {
		if (capture.target.is_null() || !capture.target->is_valid()) continue;
		Ref<HTMLEvent> event = make_dom_event(StringName("lostpointercapture"), true, false);
		const Ref<HTMLPointerEvent> pointer_event = event;
		if (pointer_event.is_valid()) pointer_event->set_pointer_id(capture.pointer_id);
		dispatch_event(capture.target, event);
	}
}

Error HTMLDOM::set_pointer_capture(const Ref<HTMLElement>& p_target, int32_t p_pointer_id) {
	if (!active_pointer_ids.has(p_pointer_id) || p_target.is_null() || !p_target->is_valid() || !p_target->is_connected()) {
		return ERR_INVALID_PARAMETER;
	}
	if (const auto* previous = pointer_capture_targets.getptr(p_pointer_id); previous && previous->ptr() == p_target.ptr()) {
		return OK;
	}
	if (const auto* previous = pointer_capture_targets.getptr(p_pointer_id); previous && previous->is_valid()) {
		pending_pointer_capture_events.push_back({ StringName("lostpointercapture"), *previous, p_pointer_id });
	}
	pointer_capture_targets[p_pointer_id] = p_target;
	pending_pointer_capture_events.push_back({ StringName("gotpointercapture"), p_target, p_pointer_id });
	if (event_dispatch_depth == 0) flush_pointer_capture_events();
	return OK;
}

Error HTMLDOM::release_pointer_capture(int32_t p_pointer_id) {
	if (!active_pointer_ids.has(p_pointer_id)) return ERR_INVALID_PARAMETER;
	const auto* captured = pointer_capture_targets.getptr(p_pointer_id);
	if (!captured) return OK;
	const Ref<HTMLElement> target = *captured;
	pointer_capture_targets.erase(p_pointer_id);
	if (target.is_valid()) pending_pointer_capture_events.push_back({ StringName("lostpointercapture"), target, p_pointer_id });
	if (event_dispatch_depth == 0) flush_pointer_capture_events();
	return OK;
}

bool HTMLDOM::has_pointer_capture(const Ref<HTMLElement>& p_target, int32_t p_pointer_id) const {
	const auto* captured = pointer_capture_targets.getptr(p_pointer_id);
	return captured && captured->ptr() == p_target.ptr() && p_target.is_valid() && p_target->is_connected();
}

Ref<HTMLElement> HTMLDOM::get_pointer_capture(int32_t p_pointer_id) const {
	const auto* captured = pointer_capture_targets.getptr(p_pointer_id);
	return captured && captured->is_valid() && (*captured)->is_connected() ? *captured : Ref<HTMLElement>();
}

void HTMLDOM::flush_pointer_capture_events() {
	if (flushing_pointer_capture_events || event_dispatch_depth > 0) return;
	flushing_pointer_capture_events = true;
	std::size_t delivered = 0;
	while (!pending_pointer_capture_events.empty() && delivered++ < 256) {
		const PendingPointerCaptureEvent pending = pending_pointer_capture_events.front();
		pending_pointer_capture_events.erase(pending_pointer_capture_events.begin());
		if (pending.target.is_null() || !pending.target->is_valid()) continue;
		Ref<HTMLEvent> event = make_dom_event(pending.type, true, false);
		const Ref<HTMLPointerEvent> pointer_event = event;
		if (pointer_event.is_valid()) pointer_event->set_pointer_id(pending.pointer_id);
		dispatch_event(pending.target, event);
	}
	if (delivered >= 256) pending_pointer_capture_events.clear();
	flushing_pointer_capture_events = false;
}

void HTMLDOM::transfer_listeners(HTMLDOM& p_source, const litehtml::element& p_from,
	const litehtml::element::ptr& p_to) {
	if (&p_source == this || !p_to) return;
	auto* source_ptr = p_source.event_listeners.getptr(&p_from);
	if (!source_ptr) return;
	ListenerStore moved = std::move(*source_ptr);
	p_source.event_listeners.erase(&p_from);
	p_source.event_listener_count -= moved.count;
	auto& target = event_listeners[p_to.get()];
	for (auto& entry : moved.by_type) {
		auto& list = target.by_type[entry.key];
		for (auto& listener : entry.value) {
			listener.id = allocate_event_listener_id();
			list.push_back(listener);
		}
		if (auto* count = p_source.event_counts_by_type.getptr(entry.key)) {
			if (*count > entry.value.size()) *count -= entry.value.size();
			else p_source.event_counts_by_type.erase(entry.key);
		}
		event_counts_by_type[entry.key] += entry.value.size();
	}
	target.count += moved.count;
	event_listener_count += moved.count;
}

void HTMLDOM::invoke_event_listeners(const EventPathEntry& p_entry, const Ref<HTMLEvent>& p_event, int p_phase) {
	if (p_event->is_immediate_stopped()) return;
	Ref<HTMLEventTarget> current;
	const auto get_store = [this, &p_entry, &current]() -> ListenerStore* {
		switch (p_entry.slot) {
		case EventTargetSlot::ELEMENT:
			if (p_entry.node.is_null() || !p_entry.node->is_valid() || !p_entry.element) return nullptr;
			current = p_entry.node;
			return event_listeners.getptr(p_entry.element);
		case EventTargetSlot::DOCUMENT:
			if (p_entry.node.is_null() || !p_entry.node->is_valid()) return nullptr;
			current = p_entry.node;
			return &document_event_listeners;
		case EventTargetSlot::WINDOW:
			if (p_entry.window.is_null() || !p_entry.window->is_valid()) return nullptr;
			current = p_entry.window;
			return &window_event_listeners;
		}
		return nullptr;
		};
	ListenerStore* store = get_store();
	const StringName type = p_event->get_type();
	auto* list_ptr = store ? store->by_type.getptr(type) : nullptr;
	// 派发期间允许增删监听器，因此按快照迭代
	const std::vector<EventListener> snapshot = list_ptr ? *list_ptr : std::vector<EventListener>();
	if (snapshot.empty() && p_phase == HTMLEvent::PHASE_CAPTURING) return;
	p_event->set_dispatch_phase(current, p_phase);
	for (const EventListener& entry : snapshot) {
		if (p_event->is_immediate_stopped()) break;
		if (p_phase == HTMLEvent::PHASE_CAPTURING && !entry.capture) continue;
		if (p_phase == HTMLEvent::PHASE_BUBBLING && entry.capture) continue;
		// 回调可能移除、清空或迁移监听器，因此每次调用前都重新获取存储并检查注册 ID
		ListenerStore* active_store = get_store();
		if (!active_store) continue;
		const auto* active_list = active_store->by_type.getptr(type);
		if (!active_list || std::none_of(active_list->begin(), active_list->end(), [&](const EventListener& p_active) {
			return p_active.id == entry.id;
			})) continue;
		if (entry.once) unregister_event_listener(*active_store, type, entry.callable, entry.capture);
		if (!entry.callable.is_valid()) {
			if (ListenerStore* invalid_store = get_store()) {
				unregister_event_listener(*invalid_store, type, entry.callable, entry.capture);
			}
			continue;
		}
		p_event->set_passive_context(entry.passive);
		entry.callable.call(p_event);
		p_event->set_passive_context(false);
	}
	if (p_phase == HTMLEvent::PHASE_CAPTURING || p_event->is_immediate_stopped()) return;
	store = get_store();
	if (current.is_null()) return;
	if (current->has_event_handler_override(type)) {
		const Callable handler = current->get_event_handler(type);
		if (handler.is_valid()) {
			const Variant result = handler.call(p_event);
			if (result.get_type() == Variant::BOOL && !bool(result)) p_event->prevent_default();
		}
		return;
	}
	const Ref<HTMLNode> current_node = current;
	if (current_node.is_null() || current_node->get_node_type() != HTMLNode::ELEMENT_NODE) return;
	const Ref<HTMLElement> element = current_node;
	HTMLLayout* layout = get_layout();
	if (!layout) return;
	auto* label = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(ObjectID(layout->get_event_host_id())));
	if (label) label->invoke_inline_event_handler(element, p_event);
}

bool HTMLDOM::dispatch_event(const Ref<HTMLEventTarget>& p_target, const Ref<HTMLEvent>& p_event) {
	if (p_event.is_null()) return true;
	if (event_dispatch_depth >= MAX_EVENT_DISPATCH_DEPTH) {
		ERR_PRINT("HTML event dispatch exceeded the maximum depth and was ignored.");
		return !p_event->is_default_prevented();
	}
	if (p_target.is_null() || !p_target->is_valid()) return !p_event->is_default_prevented();

	std::vector<EventPathEntry> entries;
	std::vector<Ref<HTMLEventTarget>> targets;
	const Ref<HTMLNode> node = p_target;
	if (node.is_valid()) {
		for (Ref<HTMLNode> current = node; current.is_valid(); current = [&]() -> Ref<HTMLNode> {
			const auto native = current->lock_element();
			if (native) if (const auto assigned = native->assigned_slot.lock()) return make_node(assigned);
			if (const Ref<HTMLShadowRoot> shadow = current; shadow.is_valid()) {
				if (!p_event->get_composed() && node->get_root_node() == current) return {};
				return shadow->get_host();
			}
			return current->get_parent_node();
		}()) {
			EventPathEntry entry;
			entry.slot = current->get_node_type() == HTMLNode::DOCUMENT_NODE ?
				EventTargetSlot::DOCUMENT : EventTargetSlot::ELEMENT;
			entry.node = current;
			const auto element = current->lock_element();
			entry.element = element.get();
			if (entry.slot == EventTargetSlot::ELEMENT && !entry.element) break;
			entries.push_back(entry);
			targets.push_back(Ref<HTMLEventTarget>(current));
			if (entries.size() >= MAX_EVENT_PATH_LENGTH) break;
		}
	}
	else if (const Ref<HTMLWindow> window = p_target; window.is_valid()) {
		EventPathEntry entry;
		entry.slot = EventTargetSlot::WINDOW;
		entry.window = window;
		entries.push_back(entry);
		targets.push_back(Ref<HTMLEventTarget>(window));
	}
	if (entries.empty()) return !p_event->is_default_prevented();

	// 元素路径末尾是文档节点时，把 window 追加为最后一项：捕获最先进入，冒泡最后离开
	if (entries.back().slot == EventTargetSlot::DOCUMENT) {
		if (const Ref<HTMLWindow> window = get_window(); window.is_valid()) {
			EventPathEntry entry;
			entry.slot = EventTargetSlot::WINDOW;
			entry.window = window;
			entries.push_back(entry);
			targets.push_back(Ref<HTMLEventTarget>(window));
		}
	}

	if (!p_event->begin_dispatch(p_target, targets)) {
		ERR_PRINT("The event is already being dispatched.");
		return !p_event->is_default_prevented();
	}
	const std::shared_ptr<HTMLDOM> keep_alive = shared_from_this();
	EventDispatchScope dispatch_scope(event_dispatch_depth, p_event, [keep_alive]() {
		keep_alive->flush_pointer_capture_events();
	});

	for (std::size_t index = entries.size(); index > 1; --index) {
		if (p_event->is_propagation_stopped()) break;
		invoke_event_listeners(entries[index - 1], p_event, HTMLEvent::PHASE_CAPTURING);
	}
	if (!p_event->is_propagation_stopped()) {
		// 目标阶段：捕获与非捕获监听器按注册顺序合并执行一次
		invoke_event_listeners(entries[0], p_event, HTMLEvent::PHASE_AT_TARGET);
	}
	if (!p_event->is_propagation_stopped() && p_event->get_bubbles()) {
		for (std::size_t index = 1; index < entries.size(); ++index) {
			invoke_event_listeners(entries[index], p_event, HTMLEvent::PHASE_BUBBLING);
			if (p_event->is_propagation_stopped()) break;
		}
	}

	const bool not_canceled = !p_event->is_default_prevented();
	if (event_debug) {
		UtilityFunctions::print(String("[HTML event] ") + String(p_event->get_type()) +
			" target=" + p_target->get_class() + " path=" + String::num_int64(int64_t(entries.size())) +
			" listeners=" + String::num_int64(int64_t(get_event_listener_count(p_event->get_type()))) +
			" default_prevented=" + String(p_event->is_default_prevented() ? "true" : "false"));
	}
	return not_canceled;
}

void HTMLDOM::queue_selectionchange() {
	if (selectionchange_queued) return;
	selectionchange_queued = true;
	callable_mp_static(&HTMLDOM::deliver_selectionchange).bind(
		static_cast<uint64_t>(owner_id), ++selectionchange_epoch, lifetime).call_deferred();
}

void HTMLDOM::deliver_selectionchange(uint64_t p_owner_id, uint64_t p_epoch, uint64_t p_lifetime) {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_owner_id)));
	if (!layout) return;
	const auto dom = layout->get_dom().shared_from_this();
	if (dom->selectionchange_epoch != p_epoch || dom->lifetime != p_lifetime) return;
	dom->selectionchange_queued = false;
	const Ref<HTMLDocumentNode> document = dom->get_document_node();
	if (document.is_valid()) dom->dispatch_event(document, dockit::make_dom_event(StringName("selectionchange"), false, false));
}

void HTMLDOM::queue_scroll_event(const Ref<HTMLNode>& p_node) {
	if (p_node.is_null()) return;
	++scrollend_delivery_epoch;
	if (std::none_of(pending_scrollend_targets.begin(), pending_scrollend_targets.end(),
		[&](const Ref<HTMLNode>& p_target) { return p_target.ptr() == p_node.ptr(); })) {
		pending_scrollend_targets.push_back(p_node);
	}
	for (const auto& entry : pending_scroll_targets) {
		if (entry.ptr() == p_node.ptr()) return;
	}
	pending_scroll_targets.push_back(p_node);
	if (scroll_delivery_queued) return;
	scroll_delivery_queued = true;
	callable_mp_static(&HTMLDOM::deliver_scroll_events).bind(
		static_cast<uint64_t>(owner_id), ++scroll_delivery_epoch, lifetime).call_deferred();
}

void HTMLDOM::deliver_scroll_events(uint64_t p_owner_id, uint64_t p_epoch, uint64_t p_lifetime) {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_owner_id)));
	if (!layout) return;
	const auto dom = layout->get_dom().shared_from_this();
	if (dom->scroll_delivery_epoch != p_epoch || dom->lifetime != p_lifetime) return;
	dom->scroll_delivery_queued = false;
	std::vector<Ref<HTMLNode>> pending = std::move(dom->pending_scroll_targets);
	dom->pending_scroll_targets.clear();
	for (const Ref<HTMLNode>& node : pending) {
		if (dom->lifetime != p_lifetime) break;
		if (node.is_null() || !node.is_valid()) continue;
		dom->dispatch_event(Ref<HTMLEventTarget>(node), dockit::make_dom_event(StringName("scroll"), false, false));
	}
	if (!dom->scrollend_delivery_queued) {
		dom->scrollend_delivery_queued = true;
		callable_mp_static(&HTMLDOM::deliver_scrollend).bind(p_owner_id, dom->scrollend_delivery_epoch, p_lifetime).call_deferred();
	}
}

void HTMLDOM::deliver_scrollend(uint64_t p_owner_id, uint64_t p_epoch, uint64_t p_lifetime) {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_owner_id)));
	if (!layout) return;
	const auto dom = layout->get_dom().shared_from_this();
	if (dom->lifetime != p_lifetime) return;
	if (dom->scrollend_delivery_epoch != p_epoch) {
		callable_mp_static(&HTMLDOM::deliver_scrollend).bind(p_owner_id, dom->scrollend_delivery_epoch, p_lifetime).call_deferred();
		return;
	}
	dom->scrollend_delivery_queued = false;
	std::vector<Ref<HTMLNode>> pending = std::move(dom->pending_scrollend_targets);
	dom->pending_scrollend_targets.clear();
	for (const Ref<HTMLNode>& node : pending) {
		if (dom->lifetime != p_lifetime) break;
		if (node.is_null() || !node.is_valid()) continue;
		dom->dispatch_event(Ref<HTMLEventTarget>(node), dockit::make_dom_event(StringName("scrollend"), false, false));
	}
}
