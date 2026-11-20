#include "html_selection.h"

#include "html_dom.h"

using namespace godot;
using dockit::HTMLBoundary;

void HTMLSelection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &HTMLSelection::is_valid);
	ClassDB::bind_method(D_METHOD("is_collapsed"), &HTMLSelection::is_collapsed);
	ClassDB::bind_method(D_METHOD("get_range_count"), &HTMLSelection::get_range_count);
	ClassDB::bind_method(D_METHOD("get_range_at", "index"), &HTMLSelection::get_range_at);
	ClassDB::bind_method(D_METHOD("get_anchor_node"), &HTMLSelection::get_anchor_node);
	ClassDB::bind_method(D_METHOD("get_anchor_offset"), &HTMLSelection::get_anchor_offset);
	ClassDB::bind_method(D_METHOD("get_focus_node"), &HTMLSelection::get_focus_node);
	ClassDB::bind_method(D_METHOD("get_focus_offset"), &HTMLSelection::get_focus_offset);
	ClassDB::bind_method(D_METHOD("add_range", "range"), &HTMLSelection::add_range);
	ClassDB::bind_method(D_METHOD("remove_all_ranges"), &HTMLSelection::remove_all_ranges);
	ClassDB::bind_method(D_METHOD("collapse", "node", "offset"), &HTMLSelection::collapse, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("extend", "node", "offset"), &HTMLSelection::extend, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_base_and_extent", "anchor", "anchor_offset", "focus", "focus_offset"),
		&HTMLSelection::set_base_and_extent);
	ClassDB::bind_method(D_METHOD("select_all_children", "node"), &HTMLSelection::select_all_children);
	ClassDB::bind_method(D_METHOD("get_text"), &HTMLSelection::get_text);
}

bool HTMLSelection::is_valid() const {
	const auto dom = owner.lock();
	return dom && dom->get_lifetime() == version && dom->get_document();
}

bool HTMLSelection::is_collapsed() const { return get_range_count() == 0 || range->is_collapsed(); }

int64_t HTMLSelection::get_range_count() const {
	return is_valid() && range.is_valid() && range->is_valid() &&
		range->start.node->is_connected() && range->end.node->is_connected() ? 1 : 0;
}

Ref<HTMLRange> HTMLSelection::get_range_at(int64_t p_index) const {
	return p_index == 0 && get_range_count() == 1 ? range : Ref<HTMLRange>();
}

Ref<HTMLNode> HTMLSelection::get_anchor_node() const {
	return get_range_count() ? (backward ? range->end.node : range->start.node) : Ref<HTMLNode>();
}

int64_t HTMLSelection::get_anchor_offset() const {
	return get_range_count() ? (backward ? range->end.offset : range->start.offset) : 0;
}

Ref<HTMLNode> HTMLSelection::get_focus_node() const {
	return get_range_count() ? (backward ? range->start.node : range->end.node) : Ref<HTMLNode>();
}

int64_t HTMLSelection::get_focus_offset() const {
	return get_range_count() ? (backward ? range->start.offset : range->end.offset) : 0;
}

void HTMLSelection::use_range(const Ref<HTMLRange>& p_range, bool p_backward) {
	if (range == p_range && backward == p_backward) return;
	range = p_range;
	backward = p_backward;
	if (const auto dom = owner.lock()) dom->selection_changed();
}

Error HTMLSelection::add_range(const Ref<HTMLRange>& p_range) {
	if (!is_valid() || p_range.is_null() || !p_range->is_valid() || p_range->owner.lock() != owner.lock() ||
		!p_range->start.node->is_connected() || !p_range->end.node->is_connected()) return ERR_INVALID_PARAMETER;
	if (!get_range_count()) use_range(p_range, false);
	return OK;
}

void HTMLSelection::remove_all_ranges() {
	if (is_valid()) use_range({}, false);
}

Error HTMLSelection::collapse(const Ref<HTMLNode>& p_node, int64_t p_offset) {
	if (p_node.is_null()) {
		remove_all_ranges();
		return is_valid() ? OK : ERR_INVALID_PARAMETER;
	}
	return set_base_and_extent(p_node, p_offset, p_node, p_offset);
}

Error HTMLSelection::extend(const Ref<HTMLNode>& p_node, int64_t p_offset) {
	if (!get_range_count()) return ERR_INVALID_PARAMETER;
	return set_base_and_extent(get_anchor_node(), get_anchor_offset(), p_node, p_offset);
}

Error HTMLSelection::set_base_and_extent(const Ref<HTMLNode>& p_anchor, int64_t p_anchor_offset,
	const Ref<HTMLNode>& p_focus, int64_t p_focus_offset) {
	if (!is_valid() || p_anchor.is_null() || p_focus.is_null() || !p_anchor->is_connected() || !p_focus->is_connected()) return ERR_INVALID_PARAMETER;
	const auto selection_range = owner.lock()->create_range();
	const HTMLBoundary anchor{p_anchor, p_anchor_offset};
	const HTMLBoundary focus{p_focus, p_focus_offset};
	if (!selection_range->accepts(anchor) || !selection_range->accepts(focus)) return ERR_INVALID_PARAMETER;
	const bool reversed = dockit::compare_html_boundaries(anchor, focus) > 0;
	selection_range->set_boundaries(reversed ? focus : anchor, reversed ? anchor : focus);
	use_range(selection_range, reversed);
	return OK;
}

Error HTMLSelection::select_all_children(const Ref<HTMLNode>& p_node) {
	if (p_node.is_null() || !p_node->is_valid()) return ERR_INVALID_PARAMETER;
	return set_base_and_extent(p_node, 0, p_node, dockit::html_node_length(*p_node.ptr()));
}

String HTMLSelection::get_text() const {
	return get_range_count() ? range->get_text() : String();
}
