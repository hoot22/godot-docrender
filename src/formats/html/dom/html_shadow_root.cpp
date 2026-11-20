#include "html_shadow_root.h"
#include "html_dom.h"
#include "formats/html/elements/html_template_element.h"
#include "formats/html/html_layout.h"
#include <litehtml/document.h>
#include <litehtml/html_tag.h>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <unordered_set>
#include <algorithm>
#include <utility>

using namespace godot;
using dockit::HTMLDOM;

void HTMLShadowRoot::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_host"), &HTMLShadowRoot::get_host);
	ClassDB::bind_method(D_METHOD("get_mode"), &HTMLShadowRoot::get_mode);
	ClassDB::bind_method(D_METHOD("get_slot_assignment"), &HTMLShadowRoot::get_slot_assignment);
	ClassDB::bind_method(D_METHOD("get_delegates_focus"), &HTMLShadowRoot::get_delegates_focus);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "host", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE, "HTMLElement"), "", "get_host");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mode", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "slot_assignment", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_slot_assignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "delegates_focus", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_delegates_focus");
}
Ref<HTMLElement> HTMLShadowRoot::get_host() const {
	return is_valid() ? get_dom().make_element(host.lock()) : Ref<HTMLElement>();
}
void HTMLSlotElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &HTMLSlotElement::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &HTMLSlotElement::set_name);
	ClassDB::bind_method(D_METHOD("assigned_nodes", "flatten"), &HTMLSlotElement::assigned_nodes, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("assigned_elements", "flatten"), &HTMLSlotElement::assigned_elements, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("assign", "nodes"), &HTMLSlotElement::assign);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
}
TypedArray<HTMLNode> HTMLSlotElement::assigned_nodes(bool p_flatten) const {
	return is_valid() ? get_dom().slot_nodes(lock_element(), p_flatten) : TypedArray<HTMLNode>();
}
TypedArray<HTMLElement> HTMLSlotElement::assigned_elements(bool p_flatten) const {
	TypedArray<HTMLElement> result;
	for (const Ref<HTMLNode>& node : assigned_nodes(p_flatten)) {
		const Ref<HTMLElement> element = node;
		if (element.is_valid()) result.append(element);
	}
	return result;
}
Error HTMLSlotElement::assign(const Array& p_nodes) {
	return is_valid() ? get_dom().assign_slot(lock_element(), p_nodes) : ERR_INVALID_PARAMETER;
}

Ref<HTMLShadowRoot> HTMLDOM::get_shadow_root(const litehtml::element::ptr& p_host, bool p_open_only) const {
	for (const auto& root : shadow_roots)
		if (root->host.lock() == p_host && (!p_open_only || root->mode == "open")) return root;
	return {};
}
Ref<HTMLShadowRoot> HTMLDOM::attach_shadow(const litehtml::element::ptr& p_host, const String& p_mode,
	const String& p_assignment, bool p_focus) {
	if (!p_host || p_host->shadow_tree || (p_mode != "open" && p_mode != "closed") ||
		(p_assignment != "named" && p_assignment != "manual")) return {};
	const String tag = String::utf8(p_host->get_tagName());
	static const std::unordered_set<std::string> allowed{
		"article", "aside", "blockquote", "body", "div", "footer", "h1", "h2", "h3", "h4", "h5", "h6", "header", "main", "nav", "p", "section", "span"
	};
	if (!allowed.count(tag.utf8().get_data()) && !tag.contains("-")) return {};
	Ref<HTMLShadowRoot> root;
	root.instantiate();
	root->host = p_host;
	root->mode = p_mode;
	root->slot_assignment = p_assignment;
	root->delegates_focus = p_focus;
	root->detached_tree = register_detached_tree(std::make_shared<litehtml::html_tag>(p_host->get_document()));
	root->detached_tree->public_fragment = true;
	root->detached_tree->host = p_host;
	root->detached_tree->root->shadow_host = p_host;
	bind_handle(*root.ptr(), root->detached_tree->root);
	p_host->shadow_tree = root->detached_tree->root;
	shadow_roots.push_back(root);
	p_host->note_dom_change(litehtml::element::DOM_TREE);
	notify_changed();
	return root;
}
void HTMLDOM::initialize_shadow_roots() {
	if (!document) return;
	const auto visit = [&](const auto& self, const litehtml::element::ptr& parent) -> void {
		const auto children = parent->children();
		for (const auto& child : children) {
			if (std::string(child->get_tagName()) == "template" && child->get_attr("shadowrootmode")) {
				const Ref<HTMLTemplateElement> source = make_element(child);
				const auto root = attach_shadow(parent, String::utf8(child->get_attr("shadowrootmode")),
					String::utf8(child->get_attr("shadowrootslotassignment", "named")), child->get_attr("shadowrootdelegatesfocus") != nullptr);
				if (root.is_valid() && source.is_valid()) {
					Array content; content.push_back(source->get_content());
					root->replace_children(content);
					source->detach();
					self(self, parent->shadow_tree);
					continue;
				}
			}
			self(self, child);
		}
	};
	ChangeScope changes(*this);
	visit(visit, document->root());
	synchronize_slots();
}
void HTMLDOM::synchronize_slots() {
	if (assigning_slots) return;
	assigning_slots = true;
	shadow_roots.erase(std::remove_if(shadow_roots.begin(), shadow_roots.end(), [](const auto& root) { return root->host.expired(); }), shadow_roots.end());
	for (const auto& root : shadow_roots) {
		const auto host = root->host.lock();
		if (!host) continue;
		std::vector<litehtml::element::ptr> slots;
		visit_elements(root->lock_element(), [&](const auto& node) {
			if (std::string(node->get_tagName()) == "slot") slots.push_back(node);
			return false;
		});
		for (const auto& previous : root->previous_slots) {
			if (const auto slot = previous.lock(); slot && std::find(slots.begin(), slots.end(), slot) == slots.end()) {
				for (const auto& assigned : slot->assigned_nodes) if (auto child = assigned.lock()) child->assigned_slot.reset();
				slot->assigned_nodes.clear();
				slot->assigned_render_nodes.clear();
				slot->is_shadow_slot = false;
			}
		}
		root->previous_slots.clear();
		for (const auto& slot : slots) root->previous_slots.push_back(slot);
		for (const auto& child : host->children()) child->assigned_slot.reset();
		for (const auto& slot : slots) {
			slot->is_shadow_slot = true;
			auto previous = slot->assigned_nodes;
			slot->assigned_nodes.clear();
			if (root->slot_assignment == "manual") {
				for (const auto& candidate : slot->manual_nodes) {
					const auto child = candidate.lock();
					if (child && child->parent() == host) { slot->assigned_nodes.push_back(child); child->assigned_slot = slot; }
				}
			} else {
				for (const auto& child : host->children()) {
					if (child->is_comment() || litehtml::is_generated_tag(child->tag()) || !child->assigned_slot.expired()) continue;
					if (std::string(child->get_attr("slot", "")) == slot->get_attr("name", "")) {
						slot->assigned_nodes.push_back(child); child->assigned_slot = slot;
					}
				}
			}
			slot->assigned_render_nodes.clear();
			for (const auto& candidate : slot->assigned_nodes) if (auto child = candidate.lock()) slot->assigned_render_nodes.push_back(child);
			bool changed = previous.size() != slot->assigned_nodes.size();
			if (!changed) for (size_t i = 0; i < previous.size(); ++i) changed |= previous[i].lock() != slot->assigned_nodes[i].lock();
			if (changed) {
				const auto handle = make_node(slot);
				if (std::find(pending_slot_changes.begin(), pending_slot_changes.end(), handle) == pending_slot_changes.end()) pending_slot_changes.push_back(handle);
			}
		}
	}
	assigning_slots = false;
	if (!pending_slot_changes.empty() && !slot_delivery_queued) {
		slot_delivery_queued = true;
		callable_mp_static(&HTMLDOM::deliver_slot_changes).bind(uint64_t(owner_id), generation).call_deferred();
	}
}
void HTMLDOM::deliver_slot_changes(uint64_t p_owner, uint64_t p_version) {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_owner)));
	if (!layout) return;
	auto& dom = layout->get_dom();
	if (dom.generation != p_version) return;
	dom.slot_delivery_queued = false;
	auto pending = std::exchange(dom.pending_slot_changes, {});
	for (const auto& slot : pending) if (slot->is_valid()) {
		auto event = dockit::make_dom_event(StringName("slotchange"), true, false);
		event->set_composed(false);
		dom.dispatch_event(slot, event);
	}
}
TypedArray<HTMLNode> HTMLDOM::slot_nodes(const litehtml::element::ptr& p_slot, bool p_flatten) const {
	TypedArray<HTMLNode> result;
	if (!p_slot || !p_slot->is_shadow_slot) return result;
	std::unordered_set<const litehtml::element*> visited;
	const auto collect = [&](const auto& self, const litehtml::element::ptr& slot) -> void {
		if (!visited.insert(slot.get()).second) return;
		const auto add = [&](const litehtml::element::ptr& child) {
			if (p_flatten && child->is_shadow_slot) self(self, child);
			else if (auto node = make_node(child); node.is_valid() && node->get_node_type() != HTMLNode::COMMENT_NODE && !result.has(node)) result.append(node);
		};
		if (p_flatten && slot->assigned_nodes.empty()) for (const auto& child : slot->children()) add(child);
		else for (const auto& candidate : slot->assigned_nodes) if (auto child = candidate.lock()) add(child);
	};
	collect(collect, p_slot);
	return result;
}
Error HTMLDOM::assign_slot(const litehtml::element::ptr& p_slot, const Array& p_nodes) {
	if (!p_slot) return ERR_INVALID_PARAMETER;
	auto native = p_slot;
	while (native->parent()) native = native->parent();
	const auto root = get_shadow_root(native->shadow_host.lock(), false);
	if (root.is_null() || root->slot_assignment != "manual") return ERR_INVALID_PARAMETER;
	std::vector<litehtml::element::ptr> nodes;
	for (const Variant& value : p_nodes) {
		const Ref<HTMLNode> node = value;
		if (node.is_null() || node->get_owner() != get_layout() || (node->get_node_type() != HTMLNode::ELEMENT_NODE && node->get_node_type() != HTMLNode::TEXT_NODE)) return ERR_INVALID_PARAMETER;
		const auto child = node->lock_element();
		if (std::find(nodes.begin(), nodes.end(), child) == nodes.end()) nodes.push_back(child);
	}
	for (const Ref<HTMLElement>& other : root->query_selector_all("slot")) {
		const auto slot = other->lock_element();
		auto& list = slot->manual_nodes;
		list.erase(std::remove_if(list.begin(), list.end(), [&](const auto& item) { return std::find(nodes.begin(), nodes.end(), item.lock()) != nodes.end(); }), list.end());
	}
	p_slot->manual_nodes.clear();
	for (const auto& node : nodes) p_slot->manual_nodes.push_back(node);
	synchronize_slots();
	notify_changed();
	return OK;
}

TypedArray<HTMLElement> HTMLDOM::component_candidates(const String& p_tag) const {
	auto result = query_selector_all(p_tag);
	for (const auto& root : shadow_roots) if (root->is_connected()) {
		for (const Ref<HTMLElement>& element : root->query_selector_all(p_tag)) result.append(element);
	}
	return result;
}
