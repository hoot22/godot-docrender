#include "html_shadow_root.h"
#include "html_node.h"

#include "html_document_fragment.h"
#include "html_dom.h"
#include "formats/html/elements/html_element.h"
#include "formats/html/events/html_event.h"
#include "formats/html/events/html_window.h"
#include "formats/html/html_layout.h"
#include "nodes/html_label.h"

#include <litehtml/el_text.h>

#include <vector>

using namespace godot;

namespace {
	void read_event_options(const Dictionary& p_options, bool& r_capture, bool& r_once, bool& r_passive) {
		r_capture = p_options.get("capture", false);
		r_once = p_options.get("once", false);
		r_passive = p_options.get("passive", false);
	}
}

void HTMLDocumentNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_base_uri"), &HTMLDocumentNode::get_base_uri);
	ClassDB::bind_method(D_METHOD("get_visibility_state"), &HTMLDocumentNode::get_visibility_state);
	ClassDB::bind_method(D_METHOD("get_design_mode"), &HTMLDocumentNode::get_design_mode);
	ClassDB::bind_method(D_METHOD("set_design_mode", "mode"), &HTMLDocumentNode::set_design_mode);
	ClassDB::bind_method(D_METHOD("get_active_element"), &HTMLDocumentNode::get_active_element);
	ClassDB::bind_method(D_METHOD("get_document_element"), &HTMLDocumentNode::get_document_element);
	ClassDB::bind_method(D_METHOD("get_body"), &HTMLDocumentNode::get_body);
	ClassDB::bind_method(D_METHOD("get_default_view"), &HTMLDocumentNode::get_default_view);
	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &HTMLDocumentNode::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &HTMLDocumentNode::query_selector_all);
	ClassDB::bind_method(D_METHOD("get_element_by_id", "id"), &HTMLDocumentNode::get_element_by_id);
	ClassDB::bind_method(D_METHOD("create_element", "tag_name"), &HTMLDocumentNode::create_element);
	ClassDB::bind_method(D_METHOD("create_text_node", "data"), &HTMLDocumentNode::create_text_node);
	ClassDB::bind_method(D_METHOD("create_document_fragment"), &HTMLDocumentNode::create_document_fragment);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "document_element", PROPERTY_HINT_RESOURCE_TYPE, "HTMLElement", PROPERTY_USAGE_NONE),
		"", "get_document_element");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "design_mode"), "set_design_mode", "get_design_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "visibility_state"), "", "get_visibility_state");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "active_element", PROPERTY_HINT_RESOURCE_TYPE, "HTMLElement", PROPERTY_USAGE_NONE),
		"", "get_active_element");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "HTMLElement", PROPERTY_USAGE_NONE),
		"", "get_body");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_view", PROPERTY_HINT_RESOURCE_TYPE, "HTMLWindow",
		PROPERTY_USAGE_NONE), "", "get_default_view");
}

String HTMLDocumentNode::get_base_uri() const {
	const auto* layout = get_owner();
	return layout ? layout->get_base_uri() : String();
}

String HTMLDocumentNode::get_visibility_state() const {
	const auto* layout = get_owner();
	if (!layout) return String("hidden");
	const auto* host = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(ObjectID(layout->get_event_host_id())));
	return host ? host->get_document_visibility_state() : String("hidden");
}

String HTMLDocumentNode::get_design_mode() const {
	return is_valid() && get_dom().is_design_mode_enabled() ? String("on") : String("off");
}

void HTMLDocumentNode::set_design_mode(const String& p_mode) {
	if (!is_valid()) return;
	const String mode = p_mode.strip_edges().to_lower();
	const bool previous = get_dom().is_design_mode_enabled();
	if (mode == "on") get_dom().set_design_mode_enabled(true);
	else if (mode == "off") get_dom().set_design_mode_enabled(false);
	if (previous == get_dom().is_design_mode_enabled()) return;
	const auto* layout = get_owner();
	if (!layout) return;
	if (auto* label = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(ObjectID(layout->get_event_host_id())))) {
		label->on_design_mode_changed();
	}
}

Ref<HTMLElement> HTMLDocumentNode::get_active_element() const {
	const auto* layout = get_owner();
	if (!layout) return {};
	auto* host = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(ObjectID(layout->get_event_host_id())));
	return host ? host->get_focused_element() : Ref<HTMLElement>();
}

Ref<HTMLElement> HTMLDocumentNode::get_document_element() const {
	const auto* layout = get_owner();
	return layout ? layout->get_document_element() : Ref<HTMLElement>();
}

Ref<HTMLElement> HTMLDocumentNode::get_body() const {
	const auto* layout = get_owner();
	return layout ? layout->get_body() : Ref<HTMLElement>();
}

Ref<HTMLElement> HTMLDocumentNode::query_selector(const String& p_selector) const {
	const auto* layout = get_owner();
	return layout ? layout->query_selector(p_selector) : Ref<HTMLElement>();
}

TypedArray<HTMLElement> HTMLDocumentNode::query_selector_all(const String& p_selector) const {
	const auto* layout = get_owner();
	return layout ? layout->query_selector_all(p_selector) : TypedArray<HTMLElement>();
}

Ref<HTMLElement> HTMLDocumentNode::get_element_by_id(const String& p_id) const {
	const auto* layout = get_owner();
	return layout ? layout->get_element_by_id(p_id) : Ref<HTMLElement>();
}

Ref<HTMLElement> HTMLDocumentNode::create_element(const String& p_tag_name) {
	auto* layout = get_owner();
	return layout ? layout->create_element(p_tag_name) : Ref<HTMLElement>();
}

Ref<HTMLTextNode> HTMLDocumentNode::create_text_node(const String& p_data) {
	auto* layout = get_owner();
	return layout ? layout->create_text_node(p_data) : Ref<HTMLTextNode>();
}

Ref<HTMLDocumentFragment> HTMLDocumentNode::create_document_fragment() {
	auto* layout = get_owner();
	return layout ? layout->create_document_fragment() : Ref<HTMLDocumentFragment>();
}

Ref<HTMLWindow> HTMLDocumentNode::get_default_view() const {
	if (!is_valid()) return Ref<HTMLWindow>();
	return get_dom().get_window();
}

void HTMLNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_mutation_revision"), &HTMLNode::get_mutation_revision);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mutation_revision", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_mutation_revision");
	ClassDB::bind_method(D_METHOD("get_node_type"), &HTMLNode::get_node_type);
	ClassDB::bind_method(D_METHOD("is_valid"), &HTMLNode::is_valid);
	ClassDB::bind_method(D_METHOD("is_connected"), &HTMLNode::is_connected);
	ClassDB::bind_method(D_METHOD("get_parent_node"), &HTMLNode::get_parent_node);
	ClassDB::bind_method(D_METHOD("get_child_nodes"), &HTMLNode::get_child_nodes);
	ClassDB::bind_method(D_METHOD("get_first_child"), &HTMLNode::get_first_child);
	ClassDB::bind_method(D_METHOD("get_next_sibling"), &HTMLNode::get_next_sibling);
	ClassDB::bind_method(D_METHOD("get_assigned_slot"), &HTMLNode::get_assigned_slot);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "assigned_slot", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE, "HTMLElement"), "", "get_assigned_slot");
	ClassDB::bind_method(D_METHOD("get_root_node"), &HTMLNode::get_root_node);
	ClassDB::bind_method(D_METHOD("contains", "node"), &HTMLNode::contains);
	ClassDB::bind_method(D_METHOD("compare_document_position", "node"), &HTMLNode::compare_document_position);
	ClassDB::bind_method(D_METHOD("is_equal_node", "node"), &HTMLNode::is_equal_node);
	ClassDB::bind_method(D_METHOD("normalize"), &HTMLNode::normalize);
	BIND_ENUM_CONSTANT(ELEMENT_NODE);
	BIND_ENUM_CONSTANT(TEXT_NODE);
	BIND_ENUM_CONSTANT(COMMENT_NODE);
	BIND_ENUM_CONSTANT(DOCUMENT_NODE);
	BIND_ENUM_CONSTANT(DOCUMENT_FRAGMENT_NODE);
	BIND_ENUM_CONSTANT(POSITION_DISCONNECTED);
	BIND_ENUM_CONSTANT(POSITION_PRECEDING);
	BIND_ENUM_CONSTANT(POSITION_FOLLOWING);
	BIND_ENUM_CONSTANT(POSITION_CONTAINS);
	BIND_ENUM_CONSTANT(POSITION_CONTAINED_BY);
	BIND_ENUM_CONSTANT(POSITION_IMPLEMENTATION_SPECIFIC);
}

HTMLLayout* HTMLNode::get_owner() const {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(layout_id));
	return layout && layout->parse_version == parse_version ? layout : nullptr;
}

dockit::HTMLDOM& HTMLNode::get_dom() const {
	return get_owner()->get_dom();
}

std::shared_ptr<litehtml::element> HTMLNode::lock_element() const {
	return get_owner() ? element.lock() : nullptr;
}

std::shared_ptr<litehtml::element> HTMLNode::lock_related_node(const Ref<HTMLNode>& p_node) const {
	return p_node.is_valid() && p_node->get_owner() == get_owner() ? p_node->lock_element() : nullptr;
}

bool HTMLNode::is_valid() const {
	return get_owner() && !element.expired();
}

bool HTMLNode::is_connected() const {
	const auto node = lock_element();
	return node && get_dom().is_element_connected(*node);
}

int64_t HTMLNode::get_mutation_revision() const {
	const auto node = lock_element();
	if (!node) return 0;
	return get_node_type() == DOCUMENT_NODE ? document_revision : node->mutation_revision();
}

Ref<HTMLNode> HTMLNode::get_parent_node() const {
	if (get_node_type() == DOCUMENT_NODE) return {};
	if (const auto node = lock_element()) {
		auto& dom = get_dom();
		if (const auto parent = node->parent()) return dom.make_node(parent);
		if (node->shadow_host.expired() && dom.is_element_connected(*node)) return dom.get_document_node();
	}
	return {};
}

TypedArray<HTMLNode> HTMLNode::get_child_nodes() const {
	TypedArray<HTMLNode> children;
	const auto node = lock_element();
	if (!node) return children;
	auto& dom = get_dom();
	if (get_node_type() == DOCUMENT_NODE) {
		children.append(dom.make_node(node));
		return children;
	}
	const litehtml::string* group = nullptr;
	for (const auto& child : node->children()) {
		const auto* text = dynamic_cast<const litehtml::el_text*>(child.get());
		const auto* current = text ? text->dom_text().get() : nullptr;
		if ((!current || current != group)) {
			if (const auto handle = dom.make_node(child); handle.is_valid()) children.append(handle);
		}
		group = current;
	}
	return children;
}

Ref<HTMLNode> HTMLNode::get_first_child() const {
	const auto children = get_child_nodes();
	return children.is_empty() ? Ref<HTMLNode>() : Ref<HTMLNode>(children[0]);
}

Ref<HTMLNode> HTMLNode::get_next_sibling() const {
	if (const auto parent = get_parent_node(); parent.is_valid()) {
		bool found = false;
		for (const Ref<HTMLNode>& child : parent->get_child_nodes()) {
			if (found) return child;
			found = child.ptr() == this;
		}
	}
	return {};
}

Ref<HTMLNode> HTMLNode::get_root_node() const {
	if (!is_valid()) return {};
	Ref<HTMLNode> root(const_cast<HTMLNode*>(this));
	for (auto parent = root->get_parent_node(); parent.is_valid(); parent = parent->get_parent_node()) root = parent;
	return root;
}

bool HTMLNode::contains(const Ref<HTMLNode>& p_node) const {
	if (!is_valid()) return false;
	for (auto node = p_node; node.is_valid(); node = node->get_parent_node()) {
		if (node.ptr() == this) return true;
	}
	return false;
}

int32_t HTMLNode::compare_document_position(const Ref<HTMLNode>& p_node) const {
	if (!is_valid() || p_node.is_null() || !p_node->is_valid()) return POSITION_DISCONNECTED;
	if (p_node.ptr() == this) return 0;
	const auto path = [](Ref<HTMLNode> p_start) {
		std::vector<Ref<HTMLNode>> result;
		while (p_start.is_valid()) {
			result.push_back(p_start);
			p_start = p_start->get_parent_node();
		}
		return result;
	};
	const auto self = path(Ref<HTMLNode>(const_cast<HTMLNode*>(this)));
	const auto other = path(p_node);
	if (self.back() != other.back()) {
		return POSITION_DISCONNECTED | POSITION_IMPLEMENTATION_SPECIFIC |
			(get_instance_id() < p_node->get_instance_id() ? POSITION_FOLLOWING : POSITION_PRECEDING);
	}
	auto a = self.rbegin();
	auto b = other.rbegin();
	while (a != self.rend() && b != other.rend() && *a == *b) {
		++a;
		++b;
	}
	if (a == self.rend()) return POSITION_CONTAINED_BY | POSITION_FOLLOWING;
	if (b == other.rend()) return POSITION_CONTAINS | POSITION_PRECEDING;
	for (const Ref<HTMLNode>& child : (*(a - 1))->get_child_nodes()) {
		if (child == *a) return POSITION_FOLLOWING;
		if (child == *b) return POSITION_PRECEDING;
	}
	return POSITION_DISCONNECTED;
}

bool HTMLNode::is_equal_node(const Ref<HTMLNode>& p_node) const {
	const auto a = lock_element();
	const auto b = p_node.is_valid() ? p_node->lock_element() : nullptr;
	if (!a || !b || get_node_type() != p_node->get_node_type()) return false;
	if (get_node_type() == TEXT_NODE || get_node_type() == COMMENT_NODE) {
		return get_dom().character_data(*a) == p_node->get_dom().character_data(*b);
	}
	if (get_node_type() == ELEMENT_NODE && (a->tag() != b->tag() ||
		get_dom().get_element_attributes(*a) != p_node->get_dom().get_element_attributes(*b))) return false;
	const auto left = get_child_nodes();
	const auto right = p_node->get_child_nodes();
	if (left.size() != right.size()) return false;
	for (int64_t i = 0; i < left.size(); ++i) {
		const Ref<HTMLNode> child = left[i];
		if (!child->is_equal_node(right[i])) return false;
	}
	return true;
}

Error HTMLNode::normalize() {
	const auto node = lock_element();
	return node ? get_dom().normalize(node) : ERR_INVALID_PARAMETER;
}

Error HTMLNode::add_event_listener(const StringName& p_type, const Callable& p_callable, const Dictionary& p_options) {
	if (!is_valid()) return ERR_INVALID_PARAMETER;
	bool capture = false;
	bool once = false;
	bool passive = false;
	read_event_options(p_options, capture, once, passive);
	if (!p_options.has("passive") && get_node_type() == DOCUMENT_NODE) {
		passive = p_type == StringName("wheel") || p_type == StringName("touchstart") || p_type == StringName("touchmove");
	}
	if (get_node_type() == DOCUMENT_NODE) {
		return get_dom().add_document_event_listener(p_type, p_callable, capture, once, passive);
	}
	const auto node = lock_element();
	if (!node) return ERR_INVALID_PARAMETER;
	return get_dom().add_event_listener(node, p_type, p_callable, capture, once, passive);
}

Error HTMLNode::remove_event_listener(const StringName& p_type, const Callable& p_callable,
	const Dictionary& p_options) {
	if (!is_valid()) return ERR_INVALID_PARAMETER;
	const bool capture = p_options.get("capture", false);
	if (get_node_type() == DOCUMENT_NODE) {
		return get_dom().remove_document_event_listener(p_type, p_callable, capture);
	}
	const auto node = lock_element();
	if (!node) return ERR_INVALID_PARAMETER;
	return get_dom().remove_event_listener(node, p_type, p_callable, capture);
}

bool HTMLNode::dispatch_event(const Ref<HTMLEvent>& p_event) {
	if (p_event.is_null()) return true;
	if (!is_valid()) return !p_event->is_default_prevented();
	return get_dom().dispatch_event(Ref<HTMLEventTarget>(this), p_event);
}

bool HTMLNode::has_event_listener(const StringName& p_type) const {
	if (!is_valid()) return false;
	if (get_node_type() == DOCUMENT_NODE) return get_dom().has_document_event_listener(p_type);
	const auto node = lock_element();
	return node ? get_dom().has_event_listener(*node, p_type) : false;
}

void HTMLTextNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_data"), &HTMLTextNode::get_data);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &HTMLTextNode::set_data);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "data"), "set_data", "get_data");
}

String HTMLTextNode::get_data() const {
	const auto node = lock_element();
	return node ? get_dom().character_data(*node) : String();
}

Error HTMLTextNode::set_data(const String& p_data) {
	const auto node = lock_element();
	return node ? get_dom().set_character_data(node, p_data) : ERR_INVALID_PARAMETER;
}

void HTMLCommentNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_data"), &HTMLCommentNode::get_data);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &HTMLCommentNode::set_data);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "data"), "set_data", "get_data");
}

String HTMLCommentNode::get_data() const {
	const auto node = lock_element();
	return node ? get_dom().character_data(*node) : String();
}

Error HTMLCommentNode::set_data(const String& p_data) {
	const auto node = lock_element();
	return node ? get_dom().set_character_data(node, p_data) : ERR_INVALID_PARAMETER;
}

Ref<HTMLElement> HTMLNode::get_assigned_slot() const {
	const auto node = lock_element();
	const auto slot = node ? node->assigned_slot.lock() : nullptr;
	if (!slot) return {};
	const auto root = get_dom().make_node(slot)->get_root_node();
	const Ref<HTMLShadowRoot> shadow = root;
	return shadow.is_valid() && shadow->get_mode() == "closed" ? Ref<HTMLElement>() : get_dom().make_element(slot);
}
