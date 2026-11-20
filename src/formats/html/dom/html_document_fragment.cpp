#include "html_document_fragment.h"

#include "html_dom.h"

using namespace godot;

void HTMLDocumentFragment::_bind_methods() {
	ClassDB::bind_method(D_METHOD("append_child", "child"), &HTMLDocumentFragment::append_child);
	ClassDB::bind_method(D_METHOD("insert_before", "child", "reference"), &HTMLDocumentFragment::insert_before, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("remove_child", "child"), &HTMLDocumentFragment::remove_child);
	ClassDB::bind_method(D_METHOD("replace_child", "new_child", "old_child"), &HTMLDocumentFragment::replace_child);
	ClassDB::bind_method(D_METHOD("replace_children", "items"), &HTMLDocumentFragment::replace_children);
	ClassDB::bind_method(D_METHOD("get_children"), &HTMLDocumentFragment::get_children);
	ClassDB::bind_method(D_METHOD("get_first_element_child"), &HTMLDocumentFragment::get_first_element_child);
	ClassDB::bind_method(D_METHOD("get_last_element_child"), &HTMLDocumentFragment::get_last_element_child);
	ClassDB::bind_method(D_METHOD("get_child_element_count"), &HTMLDocumentFragment::get_child_element_count);
	ClassDB::bind_method(D_METHOD("clone_node", "deep"), &HTMLDocumentFragment::clone_node, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &HTMLDocumentFragment::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &HTMLDocumentFragment::query_selector_all);
	ClassDB::bind_method(D_METHOD("get_element_by_id", "id"), &HTMLDocumentFragment::get_element_by_id);
}

Error HTMLDocumentFragment::append_child(const Ref<HTMLNode>& p_child) {
	return insert_before(p_child);
}

Error HTMLDocumentFragment::insert_before(const Ref<HTMLNode>& p_child, const Ref<HTMLNode>& p_reference) {
	using Edit = dockit::HTMLDOM::TreeEdit;
	const auto parent = lock_element();
	const auto reference = lock_related_node(p_reference);
	if (!parent || !p_child.is_valid() || (p_reference.is_valid() && (!reference || reference->parent() != parent))) return ERR_INVALID_PARAMETER;
	return get_dom().edit_nodes(reference ? reference : parent, reference ? Edit::BEFORE : Edit::APPEND, Array::make(p_child));
}

Error HTMLDocumentFragment::remove_child(const Ref<HTMLNode>& p_child) {
	const auto parent = lock_element();
	const auto child = lock_related_node(p_child);
	return parent && child && child->parent() == parent ?
		get_dom().edit_nodes(child, dockit::HTMLDOM::TreeEdit::REPLACE, {}) : ERR_INVALID_PARAMETER;
}

Error HTMLDocumentFragment::replace_child(const Ref<HTMLNode>& p_new_child, const Ref<HTMLNode>& p_old_child) {
	const auto parent = lock_element();
	const auto child = lock_related_node(p_old_child);
	return parent && child && child->parent() == parent ?
		get_dom().edit_nodes(child, dockit::HTMLDOM::TreeEdit::REPLACE, Array::make(p_new_child)) : ERR_INVALID_PARAMETER;
}

Error HTMLDocumentFragment::replace_children(const Array& p_items) {
	const auto parent = lock_element();
	return parent ? get_dom().edit_nodes(parent, dockit::HTMLDOM::TreeEdit::REPLACE_CHILDREN, p_items) : ERR_INVALID_PARAMETER;
}

Ref<HTMLElement> HTMLDocumentFragment::get_first_element_child() const {
	const auto children = get_children();
	return children.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(children[0]);
}

Ref<HTMLElement> HTMLDocumentFragment::get_last_element_child() const {
	const auto children = get_children();
	return children.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(children[children.size() - 1]);
}

int64_t HTMLDocumentFragment::get_child_element_count() const {
	return get_children().size();
}

TypedArray<HTMLElement> HTMLDocumentFragment::get_children() const {
	if (const auto dom = detached_tree && detached_tree->root ? detached_tree->owner.lock() : nullptr) {
		return dom->get_children(*detached_tree->root);
	}
	return {};
}

Ref<HTMLDocumentFragment> HTMLDocumentFragment::clone_node(bool p_deep) const {
	if (const auto dom = detached_tree && detached_tree->root ? detached_tree->owner.lock() : nullptr) {
		return dom->copy_fragment(*detached_tree->root, p_deep);
	}
	return {};
}

Ref<HTMLElement> HTMLDocumentFragment::query_selector(const String& p_selector) const {
	if (const auto dom = detached_tree && detached_tree->root ? detached_tree->owner.lock() : nullptr) {
		return dom->query_selector(p_selector, detached_tree->root);
	}
	return {};
}

TypedArray<HTMLElement> HTMLDocumentFragment::query_selector_all(const String& p_selector) const {
	if (const auto dom = detached_tree && detached_tree->root ? detached_tree->owner.lock() : nullptr) {
		return dom->query_selector_all(p_selector, detached_tree->root);
	}
	return {};
}

Ref<HTMLElement> HTMLDocumentFragment::get_element_by_id(const String& p_id) const {
	if (const auto dom = detached_tree && detached_tree->root ? detached_tree->owner.lock() : nullptr) {
		return dom->get_element_by_id(p_id, detached_tree->root);
	}
	return {};
}
