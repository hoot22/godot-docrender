#include "html_range.h"

#include "html_document_fragment.h"
#include "html_dom.h"
#include "formats/html/html_layout.h"

#include <algorithm>
#include <functional>
#include <vector>

using namespace godot;
using dockit::HTMLBoundary;
using dockit::HTMLDOM;

namespace {
	bool character_node(const HTMLNode& p_node) {
		return p_node.get_node_type() == HTMLNode::TEXT_NODE || p_node.get_node_type() == HTMLNode::COMMENT_NODE;
	}

	String node_data(const Ref<HTMLNode>& p_node) {
		if (const Ref<HTMLTextNode> text = p_node; text.is_valid()) return text->get_data();
		if (const Ref<HTMLCommentNode> comment = p_node; comment.is_valid()) return comment->get_data();
		return {};
	}

	Ref<HTMLNode> branch_under(Ref<HTMLNode> p_node, const Ref<HTMLNode>& p_ancestor) {
		for (auto parent = p_node->get_parent_node(); parent.is_valid(); parent = p_node->get_parent_node()) {
			if (parent == p_ancestor) return p_node;
			p_node = parent;
		}
		return {};
	}

	Ref<HTMLNode> common_container(const HTMLBoundary& p_start, const HTMLBoundary& p_end) {
		for (auto root = p_start.node; root.is_valid(); root = root->get_parent_node()) {
			if (root->contains(p_end.node)) return root;
		}
		return {};
	}

	struct Piece {
		Ref<HTMLNode> node;
		bool whole = false;
		int64_t from = 0;
		int64_t to = 0;
		std::vector<Piece> children;
	};

	std::vector<Piece> range_contents(const Ref<HTMLNode>& p_parent, const HTMLBoundary& p_start, const HTMLBoundary& p_end) {
		if (character_node(*p_parent.ptr())) {
			return {{p_parent, false, p_start.node == p_parent ? p_start.offset : 0,
				p_end.node == p_parent ? p_end.offset : dockit::html_node_length(*p_parent.ptr()), {}}};
		}
		std::vector<Piece> contents;
		const auto children = p_parent->get_child_nodes();
		for (int64_t i = 0; i < children.size(); ++i) {
			const HTMLBoundary before{p_parent, i};
			const HTMLBoundary after{p_parent, i + 1};
			if (dockit::compare_html_boundaries(after, p_start) <= 0) continue;
			if (dockit::compare_html_boundaries(before, p_end) >= 0) break;
			const Ref<HTMLNode> child = children[i];
			const bool whole = dockit::compare_html_boundaries(p_start, before) <= 0 &&
				dockit::compare_html_boundaries(after, p_end) <= 0;
			if (whole) contents.push_back({child, true, 0, 0, {}});
			else if (character_node(*child.ptr())) {
				auto partial = range_contents(child, p_start, p_end);
				contents.push_back(std::move(partial.front()));
			} else {
				contents.push_back({child, false, 0, 0, range_contents(child, p_start, p_end)});
			}
		}
		return contents;
	}

	void append_node_text(const Ref<HTMLNode>& p_node, String& r_text) {
		if (p_node->get_node_type() == HTMLNode::TEXT_NODE) r_text += node_data(p_node);
		else for (const Ref<HTMLNode>& child : p_node->get_child_nodes()) append_node_text(child, r_text);
	}
}

int64_t dockit::html_node_length(const HTMLNode& p_node) {
	if (const auto* text = Object::cast_to<HTMLTextNode>(&p_node)) return text->get_data().length();
	if (const auto* comment = Object::cast_to<HTMLCommentNode>(&p_node)) return comment->get_data().length();
	return p_node.get_child_nodes().size();
}

int dockit::compare_html_boundaries(const HTMLBoundary& p_left, const HTMLBoundary& p_right) {
	if (p_left.node == p_right.node) return (p_left.offset > p_right.offset) - (p_left.offset < p_right.offset);
	const auto position = p_left.node->compare_document_position(p_right.node);
	if (position & HTMLNode::POSITION_CONTAINED_BY) {
		const auto branch = branch_under(p_right.node, p_left.node);
		return p_left.offset <= p_left.node->get_child_nodes().find(branch) ? -1 : 1;
	}
	if (position & HTMLNode::POSITION_CONTAINS) {
		const auto branch = branch_under(p_left.node, p_right.node);
		return p_right.offset <= p_right.node->get_child_nodes().find(branch) ? 1 : -1;
	}
	return position & HTMLNode::POSITION_PRECEDING ? 1 : -1;
}

void HTMLRange::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &HTMLRange::is_valid);
	ClassDB::bind_method(D_METHOD("is_collapsed"), &HTMLRange::is_collapsed);
	ClassDB::bind_method(D_METHOD("get_start_container"), &HTMLRange::get_start_container);
	ClassDB::bind_method(D_METHOD("get_end_container"), &HTMLRange::get_end_container);
	ClassDB::bind_method(D_METHOD("get_start_offset"), &HTMLRange::get_start_offset);
	ClassDB::bind_method(D_METHOD("get_end_offset"), &HTMLRange::get_end_offset);
	ClassDB::bind_method(D_METHOD("set_start", "node", "offset"), &HTMLRange::set_start);
	ClassDB::bind_method(D_METHOD("set_end", "node", "offset"), &HTMLRange::set_end);
	ClassDB::bind_method(D_METHOD("collapse", "to_start"), &HTMLRange::collapse, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("select_node", "node"), &HTMLRange::select_node);
	ClassDB::bind_method(D_METHOD("select_node_contents", "node"), &HTMLRange::select_node_contents);
	ClassDB::bind_method(D_METHOD("get_text"), &HTMLRange::get_text);
	ClassDB::bind_method(D_METHOD("get_client_rects"), &HTMLRange::get_client_rects);
	ClassDB::bind_method(D_METHOD("clone_contents"), &HTMLRange::clone_contents);
	ClassDB::bind_method(D_METHOD("extract_contents"), &HTMLRange::extract_contents);
	ClassDB::bind_method(D_METHOD("delete_contents"), &HTMLRange::delete_contents);
	ClassDB::bind_method(D_METHOD("insert_node", "node"), &HTMLRange::insert_node);
	ClassDB::bind_method(D_METHOD("surround_contents", "element"), &HTMLRange::surround_contents);
}

bool HTMLRange::accepts(const HTMLBoundary& p_boundary) const {
	const auto dom = owner.lock();
	return dom && dom->get_lifetime() == version && p_boundary.node.is_valid() && p_boundary.node->is_valid() &&
		&p_boundary.node->get_dom() == dom.get() && p_boundary.offset >= 0 &&
		p_boundary.offset <= dockit::html_node_length(*p_boundary.node.ptr());
}

bool HTMLRange::is_valid() const {
	const auto dom = owner.lock();
	return dom && dom->get_lifetime() == version && start.node.is_valid() && end.node.is_valid() &&
		start.node->is_valid() && end.node->is_valid() && &start.node->get_dom() == dom.get() && &end.node->get_dom() == dom.get();
}

bool HTMLRange::is_collapsed() const {
	return is_valid() && start.node == end.node && start.offset == end.offset;
}

Ref<HTMLNode> HTMLRange::get_start_container() const { return is_valid() ? start.node : Ref<HTMLNode>(); }
Ref<HTMLNode> HTMLRange::get_end_container() const { return is_valid() ? end.node : Ref<HTMLNode>(); }
int64_t HTMLRange::get_start_offset() const { return is_valid() ? start.offset : 0; }
int64_t HTMLRange::get_end_offset() const { return is_valid() ? end.offset : 0; }

void HTMLRange::set_boundaries(HTMLBoundary p_start, HTMLBoundary p_end) {
	start = std::move(p_start);
	end = std::move(p_end);
	if (const auto dom = owner.lock()) dom->range_changed(*this);
}

Error HTMLRange::set_start(const Ref<HTMLNode>& p_node, int64_t p_offset) {
	const HTMLBoundary proposed{p_node, p_offset};
	if (!accepts(proposed)) return ERR_INVALID_PARAMETER;
	const bool reset_end = !is_valid() || p_node->get_root_node() != end.node->get_root_node() ||
		dockit::compare_html_boundaries(proposed, end) > 0;
	set_boundaries(proposed, reset_end ? proposed : end);
	return OK;
}

Error HTMLRange::set_end(const Ref<HTMLNode>& p_node, int64_t p_offset) {
	const HTMLBoundary proposed{p_node, p_offset};
	if (!accepts(proposed)) return ERR_INVALID_PARAMETER;
	const bool reset_start = !is_valid() || p_node->get_root_node() != start.node->get_root_node() ||
		dockit::compare_html_boundaries(proposed, start) < 0;
	set_boundaries(reset_start ? proposed : start, proposed);
	return OK;
}

Error HTMLRange::collapse(bool p_to_start) {
	if (!is_valid()) return ERR_INVALID_PARAMETER;
	const auto point = p_to_start ? start : end;
	set_boundaries(point, point);
	return OK;
}

Error HTMLRange::select_node(const Ref<HTMLNode>& p_node) {
	if (!accepts({p_node, 0})) return ERR_INVALID_PARAMETER;
	const auto parent = p_node->get_parent_node();
	if (parent.is_null()) return ERR_INVALID_PARAMETER;
	const int64_t index = parent->get_child_nodes().find(p_node);
	if (index < 0) return ERR_INVALID_PARAMETER;
	set_boundaries({parent, index}, {parent, index + 1});
	return OK;
}

Error HTMLRange::select_node_contents(const Ref<HTMLNode>& p_node) {
	if (!accepts({p_node, 0})) return ERR_INVALID_PARAMETER;
	set_boundaries({p_node, 0}, {p_node, dockit::html_node_length(*p_node.ptr())});
	return OK;
}

String HTMLRange::get_text() const {
	if (!is_valid() || is_collapsed()) return {};
	const auto parent = common_container(start, end);
	if (parent.is_null()) return {};
	String text;
	const auto append = [&](const auto& p_visit, const std::vector<Piece>& p_pieces) -> void {
		for (const auto& piece : p_pieces) {
			if (piece.whole) append_node_text(piece.node, text);
			else if (piece.node->get_node_type() == HTMLNode::TEXT_NODE) text += node_data(piece.node).substr(piece.from, piece.to - piece.from);
			else p_visit(p_visit, piece.children);
		}
	};
	append(append, range_contents(parent, start, end));
	return text;
}

TypedArray<Rect2> HTMLRange::get_client_rects() const {
	if (!is_valid()) return {};
	const auto dom = owner.lock();
	const auto* layout = dom->get_layout();
	return layout ? layout->range_client_rects(*this) : TypedArray<Rect2>();
}

Ref<HTMLDocumentFragment> HTMLRange::process_contents(Contents p_action) {
	if (!is_valid()) return {};
	const auto dom = owner.lock();
	HTMLDOM::ChangeScope changes(*dom);
	const auto result = dom->create_document_fragment();
	if (is_collapsed()) return result;
	const auto parent = common_container(start, end);
	if (parent.is_null()) return {};
	const bool edit = p_action != Contents::CLONE;
	const bool output = p_action != Contents::DELETE;
	const auto pieces = range_contents(parent, start, end);
	if (edit && parent->get_node_type() == HTMLNode::DOCUMENT_NODE) {
		for (const auto& piece : pieces) if (piece.whole) return {};
	}

	// Keep the collapse point live while extraction or deletion adjusts surrounding boundaries.
	Ref<HTMLRange> bookmark;
	if (edit) {
		HTMLBoundary point = start;
		if (!start.node->contains(end.node)) {
			const auto branch = branch_under(start.node, parent);
			point = {parent, parent->get_child_nodes().find(branch) + 1};
		}
		bookmark = dom->create_range();
		bookmark->set_boundaries(point, point);
	}

	const auto transfer = [&](const auto& p_visit, const std::vector<Piece>& p_items, const Ref<HTMLNode>& p_target) -> bool {
		for (const auto& item : p_items) {
			const auto source = item.node->lock_element();
			Ref<HTMLNode> clone;
			if (item.whole && edit) {
				const Error changed = output
					? dom->edit_nodes(p_target->lock_element(), HTMLDOM::TreeEdit::APPEND, Array::make(item.node))
					: dom->edit_nodes(source, HTMLDOM::TreeEdit::REPLACE, {});
				if (changed != OK) return false;
			} else if (character_node(*item.node.ptr())) {
				const String data = node_data(item.node);
				if (output) clone = dom->create_character_node(item.whole ? data : data.substr(item.from, item.to - item.from),
					item.node->get_node_type() == HTMLNode::COMMENT_NODE);
				if (edit && dom->replace_character_data(source, "", item.from, item.to - item.from) != OK) return false;
			} else {
				if (output) clone = dom->copy_node(*dom, *source, item.whole);
				if (output && clone.is_null()) return false;
				if (!item.whole && !p_visit(p_visit, item.children, clone)) return false;
			}
			if (clone.is_valid() && dom->edit_nodes(p_target->lock_element(), HTMLDOM::TreeEdit::APPEND, Array::make(clone)) != OK) return false;
		}
		return true;
	};
	if (!transfer(transfer, pieces, result)) return {};
	if (edit) set_boundaries(bookmark->start, bookmark->start);
	return result;
}

Ref<HTMLDocumentFragment> HTMLRange::clone_contents() { return process_contents(Contents::CLONE); }
Ref<HTMLDocumentFragment> HTMLRange::extract_contents() { return process_contents(Contents::EXTRACT); }
Error HTMLRange::delete_contents() { return process_contents(Contents::DELETE).is_valid() ? OK : ERR_INVALID_PARAMETER; }

Error HTMLRange::insert_node(const Ref<HTMLNode>& p_node) {
	if (!is_valid() || !accepts({p_node, 0}) || p_node == start.node || start.node->get_node_type() == HTMLNode::COMMENT_NODE) return ERR_INVALID_PARAMETER;
	const auto dom = owner.lock();
	HTMLDOM::ChangeScope changes(*dom);
	const HTMLBoundary original = start;
	const bool collapsed = is_collapsed();
	const bool split = original.node->get_node_type() == HTMLNode::TEXT_NODE;
	const auto parent = split ? original.node->get_parent_node() : original.node;
	if (parent.is_valid() && parent->get_node_type() == HTMLNode::DOCUMENT_NODE) return ERR_INVALID_PARAMETER;
	const auto native_parent = parent.is_valid() ? parent->lock_element() : nullptr;
	if (!dom->can_insert_range_node(native_parent, p_node)) return ERR_INVALID_PARAMETER;
	Ref<HTMLNode> reference;
	if (split) {
		const auto suffix = dom->split_range_text(original);
		if (suffix.is_null()) return ERR_INVALID_PARAMETER;
		reference = suffix;
	} else {
		const auto children = parent->get_child_nodes();
		if (original.offset < children.size()) reference = children[original.offset];
	}
	if (reference == p_node) reference = p_node->get_next_sibling();
	const auto fragment = p_node->get_node_type() == HTMLNode::DOCUMENT_FRAGMENT_NODE ? p_node->get_child_nodes() : TypedArray<HTMLNode>();
	Ref<HTMLNode> last = p_node;
	if (p_node->get_node_type() == HTMLNode::DOCUMENT_FRAGMENT_NODE) {
		last = fragment.is_empty() ? Ref<HTMLNode>() : Ref<HTMLNode>(fragment[fragment.size() - 1]);
	}
	const Error inserted = dom->edit_nodes(reference.is_valid() ? reference->lock_element() : native_parent,
		reference.is_valid() ? HTMLDOM::TreeEdit::BEFORE : HTMLDOM::TreeEdit::APPEND, Array::make(p_node));
	if (inserted == OK && collapsed && last.is_valid()) {
		const int64_t index = parent->get_child_nodes().find(last);
		if (index >= 0) set_boundaries(start, {parent, index + 1});
	}
	return inserted;
}

Error HTMLRange::surround_contents(const Ref<HTMLElement>& p_element) {
	if (!is_valid() || !accepts({p_element, 0})) return ERR_INVALID_PARAMETER;
	const auto parent = common_container(start, end);
	if (parent.is_null() || p_element->contains(parent)) return ERR_INVALID_PARAMETER;
	for (const auto& item : range_contents(parent, start, end)) {
		if ((!item.whole && item.node->get_node_type() != HTMLNode::TEXT_NODE) || item.node == p_element ||
			(item.whole && item.node->contains(p_element))) return ERR_INVALID_PARAMETER;
	}
	const auto dom = owner.lock();
	HTMLDOM::ChangeScope changes(*dom);
	const auto insertion = character_node(*start.node.ptr()) ? start.node->get_parent_node() : start.node;
	if (start.node->get_node_type() == HTMLNode::COMMENT_NODE || insertion.is_null() ||
		insertion->get_node_type() == HTMLNode::DOCUMENT_NODE ||
		!dom->can_insert_range_node(insertion->lock_element(), p_element) ||
		!dom->range_can_wrap(p_element->lock_element())) return ERR_INVALID_PARAMETER;
	const auto fragment = extract_contents();
	if (fragment.is_null()) return ERR_INVALID_PARAMETER;
	const Error cleared = p_element->replace_children({});
	if (cleared != OK) return cleared;
	const Error inserted = insert_node(p_element);
	if (inserted != OK) return inserted;
	const Error appended = p_element->append_fragment(fragment);
	return appended == OK ? select_node(p_element) : appended;
}
