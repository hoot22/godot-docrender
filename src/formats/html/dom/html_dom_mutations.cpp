#include "html_dom.h"

#include "formats/html/html_layout.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <unordered_set>
#include <utility>

using namespace godot;
using dockit::HTMLBoundary;
using dockit::HTMLDOM;
using dockit::HTMLMutation;

void HTMLDOM::track_mutations(const litehtml::element::ptr& p_node) const {
	if (!p_node || litehtml::is_generated_tag(p_node->tag())) return;
	track_node_mutations(p_node);
	for (const auto& child : p_node->children()) track_mutations(child);
}

void HTMLDOM::track_node_mutations(const litehtml::element::ptr& p_node) const {
	if (!p_node || litehtml::is_generated_tag(p_node->tag())) return;
	if (!p_node->dom_mutation) p_node->dom_mutation = std::make_shared<litehtml::element::DOMMutationState>();
	p_node->dom_mutation->notify = [owner = const_cast<HTMLDOM*>(this)->weak_from_this()](litehtml::element& node, unsigned flags) {
		if (const auto dom = owner.lock()) dom->queue_change(node, flags);
	};
}

void HTMLDOM::queue_change(litehtml::element& p_node, unsigned p_flags) {
	if (const auto found = pending_change_indices.find(&p_node); found != pending_change_indices.end()) {
		auto& change = pending_changes[found->second];
		change.flags = change.target.lock().get() == &p_node ? change.flags | p_flags : p_flags;
		change.target = p_node.shared_from_this();
		change.revision = p_node.mutation_revision();
		return;
	}
	pending_change_indices.emplace(&p_node, pending_changes.size());
	pending_changes.push_back({p_node.shared_from_this(), p_node.mutation_revision(), p_flags});
}

void HTMLDOM::notify_changed() {
	changed_styles = true;
	if (change_depth == 0) flush_changes();
}

void HTMLDOM::flush_changes() {
	if (change_depth || flushing_changes) return;
	const auto keep_alive = shared_from_this();
	flushing_changes = true;
	while (!pending_changes.empty() || changed_styles) {
		Changes batch;
		batch.swap(pending_changes);
		pending_change_indices.clear();
		synchronize_slots();
		const bool styles = std::exchange(changed_styles, false);
		if (on_mutation && !batch.empty()) on_mutation(batch);
		if (styles && on_change) on_change();
	}
	flushing_changes = false;
}

namespace {
	std::vector<Ref<HTMLNode>> ancestry_of(Ref<HTMLNode> p_node) {
		std::vector<Ref<HTMLNode>> ancestry;
		while (p_node.is_valid()) {
			ancestry.push_back(p_node);
			p_node = p_node->get_parent_node();
		}
		return ancestry;
	}
}

HTMLLayout* HTMLDOM::get_layout() const {
	return Object::cast_to<HTMLLayout>(ObjectDB::get_instance(owner_id));
}

Ref<HTMLRange> HTMLDOM::create_range() {
	const auto root = get_document_node();
	if (root.is_null()) return {};
	Ref<HTMLRange> range;
	range.instantiate();
	range->owner = weak_from_this();
	range->version = lifetime;
	range->start = range->end = {root, 0};
	ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
		[](ObjectID p_id) { return !ObjectDB::get_instance(p_id); }), ranges.end());
	ranges.push_back(ObjectID(range->get_instance_id()));
	return range;
}

Ref<HTMLSelection> HTMLDOM::get_selection() {
	if (!document) return {};
	if (selection.is_null()) {
		selection.instantiate();
		selection->version = lifetime;
		selection->owner = weak_from_this();
	}
	return selection;
}

void HTMLDOM::range_changed(const HTMLRange& p_range) {
	if (selection.is_valid() && selection->range.ptr() == &p_range) selection_changed();
}

HTMLDOM::ChildChange HTMLDOM::capture_children(const litehtml::element::ptr& p_parent) {
	ChildChange change;
	if (!p_parent) return change;
	change.target = make_node(p_parent);
	if (change.target.is_null()) return change;
	change.ancestry = ancestry_of(change.target);
	change.before = change.target->get_child_nodes();
	const auto capture = [&](const Ref<HTMLRange>& p_range, const HTMLBoundary& p_point, bool p_start) {
		if (p_point.node == change.target) {
			change.endpoints.push_back({p_range, p_start, {}, p_point.offset});
			return;
		}
		auto node = p_point.node;
		while (node.is_valid()) {
			auto parent = node->get_parent_node();
			if (parent == change.target) {
				const int64_t index = change.before.find(node);
				if (index >= 0) change.endpoints.push_back({p_range, p_start, node, index});
				return;
			}
			node = std::move(parent);
		}
	};
	for (ObjectID id : ranges) {
		auto* range = Object::cast_to<HTMLRange>(ObjectDB::get_instance(id));
		if (!range || !range->is_valid()) continue;
		const Ref<HTMLRange> held(range);
		capture(held, range->start, true);
		capture(held, range->end, false);
	}
	return change;
}

void HTMLDOM::finish_children(const ChildChange& p_change, const std::vector<litehtml::element::ptr>& p_moved) {
	if (p_change.target.is_null()) return;
	const auto& before = p_change.before;
	const auto after = p_change.target->get_child_nodes();
	if (before == after) return;

	std::unordered_set<Object*> moved;
	for (const auto& node : p_moved) {
		if (const auto handle = make_node(node); handle.is_valid()) moved.insert(handle.ptr());
	}
	std::unordered_set<Object*> old_children;
	std::unordered_set<Object*> new_children;
	for (const Ref<HTMLNode>& node : before) old_children.insert(node.ptr());
	for (const Ref<HTMLNode>& node : after) new_children.insert(node.ptr());

	HTMLMutation record{HTMLMutation::CHILD_LIST, p_change.target};
	std::vector<int64_t> removed_indices;
	std::vector<int64_t> added_indices;
	for (int64_t i = 0; i < before.size(); ++i) {
		const Ref<HTMLNode> node = before[i];
		if (!new_children.count(node.ptr()) || moved.count(node.ptr())) {
			record.removed.append(node);
			removed_indices.push_back(i);
		}
	}
	for (int64_t i = 0; i < after.size(); ++i) {
		const Ref<HTMLNode> node = after[i];
		if (!old_children.count(node.ptr()) || moved.count(node.ptr())) {
			record.added.append(node);
			added_indices.push_back(i);
		}
	}
	if (removed_indices.empty() && added_indices.empty()) return;
	if (const auto target = p_change.target->lock_element()) target->note_dom_change(litehtml::element::DOM_CHILDREN);
	std::unordered_set<HTMLNode*> relocated;
	for (const Ref<HTMLNode>& node : record.removed) {
		if (const auto native = node->lock_element()) native->note_dom_change(litehtml::element::DOM_TREE);
		relocated.insert(node.ptr());
	}
	for (const Ref<HTMLNode>& node : record.added) {
		if (!relocated.count(node.ptr())) {
			if (const auto native = node->lock_element()) native->note_dom_change(litehtml::element::DOM_TREE);
		}
	}
	const int64_t first = std::min(removed_indices.empty() ? before.size() : removed_indices.front(),
		added_indices.empty() ? after.size() : added_indices.front());
	if (first > 0) record.previous = before[first - 1];
	int64_t tail = after.size();
	for (int64_t old_tail = before.size(); tail > first && old_tail > first; --old_tail, --tail) {
		const Ref<HTMLNode> candidate = after[tail - 1];
		if (before[old_tail - 1] != after[tail - 1] || moved.count(candidate.ptr())) break;
	}
	if (tail < after.size()) record.next = after[tail];

	bool selection_moved = false;
	for (const auto& item : p_change.endpoints) {
		if (item.branch.is_valid() && !record.removed.has(item.branch)) continue;
		auto& point = item.start ? item.range->start : item.range->end;
		if (item.branch.is_null() && point.node != p_change.target) continue;
		int64_t offset = item.branch.is_null() ? point.offset : item.index;
		offset -= std::lower_bound(removed_indices.begin(), removed_indices.end(), offset) - removed_indices.begin();
		for (const int64_t index : added_indices) {
			if (index >= offset) break;
			++offset;
		}
		if (point.node != p_change.target || point.offset != offset) {
			point = {p_change.target, offset};
			selection_moved |= selection.is_valid() && selection->range == item.range;
		}
	}
	publish_mutation(record, p_change.ancestry);
	if (selection_moved) selection_changed();
}

void HTMLDOM::publish_attribute(const litehtml::element::ptr& p_element, const char* p_name, const Variant& p_old) {
	p_element->note_dom_change(litehtml::element::DOM_ATTRIBUTE);
	if (observers.empty()) return;
	HTMLMutation record{HTMLMutation::ATTRIBUTES, make_node(p_element)};
	record.attribute = String::utf8(p_name);
	record.old_value = p_old;
	publish_mutation(record, ancestry_of(record.target));
}

void HTMLDOM::publish_mutation(const HTMLMutation& p_change, const std::vector<Ref<HTMLNode>>& p_ancestry) {
	if (p_change.target.is_null()) return;
	observers.erase(std::remove_if(observers.begin(), observers.end(), [&](ObjectID p_id) {
		auto* observer = Object::cast_to<HTMLMutationObserver>(ObjectDB::get_instance(p_id));
		if (!observer) return true;
		observer->enqueue(*this, p_change, p_ancestry);
		return false;
	}), observers.end());
}

void HTMLDOM::track_observer(ObjectID p_id) {
	if (std::find(observers.begin(), observers.end(), p_id) == observers.end()) observers.push_back(p_id);
}

void HTMLDOM::queue_observer_delivery() {
	if (observer_delivery_queued) return;
	observer_delivery_queued = true;
	const auto callback = callable_mp_static(&HTMLDOM::deliver_observers).bind(
		static_cast<uint64_t>(owner_id), ++observer_delivery_epoch, lifetime);
	auto* tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	if (delivering_observers && tree) {
		tree->connect("process_frame", callback, Object::CONNECT_ONE_SHOT);
	} else {
		callback.call_deferred();
	}
}

void HTMLDOM::deliver_observers(uint64_t p_owner_id, uint64_t p_epoch, uint64_t p_lifetime) {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_owner_id)));
	if (!layout) return;
	const auto dom = layout->get_dom().shared_from_this();
	if (dom->observer_delivery_epoch != p_epoch || dom->lifetime != p_lifetime) return;
	dom->observer_delivery_queued = false;
	dom->delivering_observers = true;
	struct Delivery {
		Ref<HTMLMutationObserver> observer;
		TypedArray<Dictionary> records;
		uint64_t version;
	};
	std::vector<Delivery> deliveries;
	for (const auto id : dom->observers) {
		auto* observer = Object::cast_to<HTMLMutationObserver>(ObjectDB::get_instance(id));
		if (!observer) continue;
		auto records = observer->take_owner(*dom);
		if (!records.is_empty()) deliveries.push_back({Ref<HTMLMutationObserver>(observer), records, observer->delivery_version});
	}
	for (const auto& delivery : deliveries) {
		if (dom->lifetime != p_lifetime) break;
		if (delivery.observer->delivery_version == delivery.version) {
			delivery.observer->emit_signal("mutations_delivered", delivery.records);
		}
	}
	dom->delivering_observers = false;
}

void HTMLDOM::transfer_observers(HTMLDOM& p_source) {
	if (&p_source == this) return;
	for (const auto id : p_source.observers) {
		auto* observer = Object::cast_to<HTMLMutationObserver>(ObjectDB::get_instance(id));
		if (!observer) continue;
		for (auto& entry : observer->registrations) {
			if (entry.owner.lock().get() == &p_source && entry.target->is_valid() && &entry.target->get_dom() == this) {
				entry.owner = weak_from_this();
				track_observer(id);
			}
		}
	}
}
