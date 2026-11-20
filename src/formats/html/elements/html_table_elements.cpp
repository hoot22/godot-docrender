#include "html_table_elements.h"

#include "html_table_attributes.h"
#include "formats/html/dom/html_dom.h"

#include <iterator>
#include <limits>

using namespace godot;
using dockit::HTMLDOM;
using litehtml::element;
using litehtml::string_id;

namespace {
	bool is_section(string_id p_tag) {
		return p_tag == litehtml::_thead_ || p_tag == litehtml::_tbody_ || p_tag == litehtml::_tfoot_;
	}

	template <typename Handle, typename Predicate>
	void append_children(const HTMLDOM& p_dom, const element& p_node, TypedArray<Handle>& r_result, Predicate p_matches) {
		for (const auto& child : p_node.children()) {
			if (p_matches(child->tag())) r_result.append(p_dom.make_element(child));
		}
	}

	void append_rows(const HTMLDOM& p_dom, const element& p_node, TypedArray<HTMLTableRowElement>& r_result) {
		append_children(p_dom, p_node, r_result, [](string_id p_tag) { return p_tag == litehtml::_tr_; });
	}

	template <typename Handle>
	int64_t index_of(const TypedArray<Handle>& p_elements, const HTMLElement* p_target) {
		for (int64_t index = 0; index < p_elements.size(); ++index) {
			const Ref<Handle> entry = p_elements[index];
			if (entry.ptr() == p_target) return index;
		}
		return -1;
	}

	template <typename Handle>
	Ref<Handle> insert_item(HTMLDOM& p_dom, HTMLElement& p_parent, const TypedArray<Handle>& p_children,
		const char* p_tag, int64_t p_index) {
		if (p_index < -1 || p_index > p_children.size()) return {};
		const Ref<Handle> child = p_dom.create_element(String(p_tag).utf8());
		const Ref<HTMLElement> reference = p_index >= 0 && p_index < p_children.size() ? Ref<HTMLElement>(p_children[p_index]) : Ref<HTMLElement>();
		return child.is_valid() && p_parent.insert_before(child, reference) == OK ? child : Ref<Handle>();
	}

	template <typename Handle>
	Error delete_item(const TypedArray<Handle>& p_children, int64_t p_index) {
		if (p_index < -1 || p_index >= p_children.size()) return ERR_INVALID_PARAMETER;
		if (p_children.is_empty()) return OK;
		const Ref<Handle> child = p_children[p_index < 0 ? p_children.size() - 1 : p_index];
		return child->detach();
	}

	int64_t reflected_span(const HTMLElement& p_element, const char* p_name, int p_minimum, int p_maximum) {
		const CharString value = p_element.get_attribute(p_name).utf8();
		return dockit::read_table_span(value.get_data(), p_minimum, p_maximum);
	}

	void write_span(HTMLElement& p_element, const char* p_name, int64_t p_value) {
		p_element.set_attribute(p_name, String::num_int64(p_value >= 0 && p_value <= std::numeric_limits<int32_t>::max() ? p_value : 1));
	}
}

void HTMLTableColElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_span"), &HTMLTableColElement::get_span);
	ClassDB::bind_method(D_METHOD("set_span", "span"), &HTMLTableColElement::set_span);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "span"), "set_span", "get_span");
}

int64_t HTMLTableColElement::get_span() const { return reflected_span(*this, "span", 1, 1000); }
void HTMLTableColElement::set_span(int64_t p_span) { write_span(*this, "span", p_span); }

void HTMLTableCellElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_cell_index"), &HTMLTableCellElement::get_cell_index);
	ClassDB::bind_method(D_METHOD("get_col_span"), &HTMLTableCellElement::get_col_span);
	ClassDB::bind_method(D_METHOD("set_col_span", "span"), &HTMLTableCellElement::set_col_span);
	ClassDB::bind_method(D_METHOD("get_row_span"), &HTMLTableCellElement::get_row_span);
	ClassDB::bind_method(D_METHOD("set_row_span", "span"), &HTMLTableCellElement::set_row_span);
	ClassDB::bind_method(D_METHOD("get_headers"), &HTMLTableCellElement::get_headers);
	ClassDB::bind_method(D_METHOD("set_headers", "value"), &HTMLTableCellElement::set_headers);
	ClassDB::bind_method(D_METHOD("get_scope"), &HTMLTableCellElement::get_scope);
	ClassDB::bind_method(D_METHOD("set_scope", "value"), &HTMLTableCellElement::set_scope);
	ClassDB::bind_method(D_METHOD("get_abbr"), &HTMLTableCellElement::get_abbr);
	ClassDB::bind_method(D_METHOD("set_abbr", "value"), &HTMLTableCellElement::set_abbr);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cell_index"), "", "get_cell_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "col_span"), "set_col_span", "get_col_span");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "row_span"), "set_row_span", "get_row_span");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "headers"), "set_headers", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scope"), "set_scope", "get_scope");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "abbr"), "set_abbr", "get_abbr");
}

int64_t HTMLTableCellElement::get_cell_index() const {
	const Ref<HTMLTableRowElement> row = get_parent();
	return row.is_valid() ? index_of(row->get_cells(), this) : -1;
}

int64_t HTMLTableCellElement::get_col_span() const { return reflected_span(*this, "colspan", 1, 1000); }
void HTMLTableCellElement::set_col_span(int64_t p_span) { write_span(*this, "colspan", p_span); }
int64_t HTMLTableCellElement::get_row_span() const { return reflected_span(*this, "rowspan", 0, 65534); }
void HTMLTableCellElement::set_row_span(int64_t p_span) { write_span(*this, "rowspan", p_span); }
String HTMLTableCellElement::get_headers() const { return get_attribute("headers"); }
void HTMLTableCellElement::set_headers(const String& p_value) { set_attribute("headers", p_value); }
String HTMLTableCellElement::get_scope() const {
	const String scope = get_attribute("scope").to_lower();
	return scope == "row" || scope == "col" || scope == "rowgroup" || scope == "colgroup" ? scope : String();
}
void HTMLTableCellElement::set_scope(const String& p_value) { set_attribute("scope", p_value); }
String HTMLTableCellElement::get_abbr() const { return get_attribute("abbr"); }
void HTMLTableCellElement::set_abbr(const String& p_value) { set_attribute("abbr", p_value); }

void HTMLTableRowElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_row_index"), &HTMLTableRowElement::get_row_index);
	ClassDB::bind_method(D_METHOD("get_section_row_index"), &HTMLTableRowElement::get_section_row_index);
	ClassDB::bind_method(D_METHOD("get_cells"), &HTMLTableRowElement::get_cells);
	ClassDB::bind_method(D_METHOD("insert_cell", "index"), &HTMLTableRowElement::insert_cell, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("delete_cell", "index"), &HTMLTableRowElement::delete_cell);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "row_index"), "", "get_row_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "section_row_index"), "", "get_section_row_index");
}

int64_t HTMLTableRowElement::get_row_index() const {
	Ref<HTMLElement> parent = get_parent();
	const Ref<HTMLTableSectionElement> section = parent;
	if (section.is_valid()) parent = section->get_parent();
	const Ref<HTMLTableElement> table = parent;
	return table.is_valid() ? index_of(table->get_rows(), this) : -1;
}

int64_t HTMLTableRowElement::get_section_row_index() const {
	const Ref<HTMLElement> parent = get_parent();
	const Ref<HTMLTableSectionElement> section = parent;
	if (section.is_valid()) return index_of(section->get_rows(), this);
	const Ref<HTMLTableElement> table = parent;
	return table.is_valid() ? index_of(table->get_rows(), this) : -1;
}

TypedArray<HTMLTableCellElement> HTMLTableRowElement::get_cells() const {
	TypedArray<HTMLTableCellElement> cells;
	if (const auto node = lock_element()) {
		append_children(get_dom(), *node, cells, [](string_id p_tag) { return p_tag == litehtml::_td_ || p_tag == litehtml::_th_; });
	}
	return cells;
}

Ref<HTMLTableCellElement> HTMLTableRowElement::insert_cell(int64_t p_index) {
	return is_valid() ? insert_item(get_dom(), *this, get_cells(), "td", p_index) : Ref<HTMLTableCellElement>();
}

Error HTMLTableRowElement::delete_cell(int64_t p_index) {
	return is_valid() ? delete_item(get_cells(), p_index) : ERR_INVALID_PARAMETER;
}

void HTMLTableSectionElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rows"), &HTMLTableSectionElement::get_rows);
	ClassDB::bind_method(D_METHOD("insert_row", "index"), &HTMLTableSectionElement::insert_row, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("delete_row", "index"), &HTMLTableSectionElement::delete_row);
}

TypedArray<HTMLTableRowElement> HTMLTableSectionElement::get_rows() const {
	TypedArray<HTMLTableRowElement> rows;
	if (const auto node = lock_element()) append_rows(get_dom(), *node, rows);
	return rows;
}

Ref<HTMLTableRowElement> HTMLTableSectionElement::insert_row(int64_t p_index) {
	return is_valid() ? insert_item(get_dom(), *this, get_rows(), "tr", p_index) : Ref<HTMLTableRowElement>();
}

Error HTMLTableSectionElement::delete_row(int64_t p_index) {
	return is_valid() ? delete_item(get_rows(), p_index) : ERR_INVALID_PARAMETER;
}

void HTMLTableElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_caption"), &HTMLTableElement::get_caption);
	ClassDB::bind_method(D_METHOD("set_caption", "caption"), &HTMLTableElement::set_caption);
	ClassDB::bind_method(D_METHOD("create_caption"), &HTMLTableElement::create_caption);
	ClassDB::bind_method(D_METHOD("delete_caption"), &HTMLTableElement::delete_caption);
	ClassDB::bind_method(D_METHOD("get_t_head"), &HTMLTableElement::get_t_head);
	ClassDB::bind_method(D_METHOD("set_t_head", "head"), &HTMLTableElement::set_t_head);
	ClassDB::bind_method(D_METHOD("create_t_head"), &HTMLTableElement::create_t_head);
	ClassDB::bind_method(D_METHOD("delete_t_head"), &HTMLTableElement::delete_t_head);
	ClassDB::bind_method(D_METHOD("get_t_foot"), &HTMLTableElement::get_t_foot);
	ClassDB::bind_method(D_METHOD("set_t_foot", "foot"), &HTMLTableElement::set_t_foot);
	ClassDB::bind_method(D_METHOD("create_t_foot"), &HTMLTableElement::create_t_foot);
	ClassDB::bind_method(D_METHOD("delete_t_foot"), &HTMLTableElement::delete_t_foot);
	ClassDB::bind_method(D_METHOD("get_t_bodies"), &HTMLTableElement::get_t_bodies);
	ClassDB::bind_method(D_METHOD("create_t_body"), &HTMLTableElement::create_t_body);
	ClassDB::bind_method(D_METHOD("get_rows"), &HTMLTableElement::get_rows);
	ClassDB::bind_method(D_METHOD("insert_row", "index"), &HTMLTableElement::insert_row, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("delete_row", "index"), &HTMLTableElement::delete_row);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "caption", PROPERTY_HINT_RESOURCE_TYPE, "HTMLTableCaptionElement"), "set_caption", "get_caption");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "t_head", PROPERTY_HINT_RESOURCE_TYPE, "HTMLTableSectionElement"), "set_t_head", "get_t_head");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "t_foot", PROPERTY_HINT_RESOURCE_TYPE, "HTMLTableSectionElement"), "set_t_foot", "get_t_foot");
}

Ref<HTMLElement> HTMLTableElement::get_table_part(const char* p_tag) const {
	if (const auto table = lock_element()) {
		const string_id tag = litehtml::_id(p_tag);
		for (const auto& child : table->children()) {
			if (child->tag() == tag) return get_dom().make_element(child);
		}
	}
	return {};
}

Error HTMLTableElement::set_table_part(const char* p_tag, const Ref<HTMLElement>& p_value) {
	const auto table = lock_element();
	if (!table) return ERR_INVALID_PARAMETER;
	const auto value = lock_related_element(p_value);
	const string_id tag = litehtml::_id(p_tag);
	if (p_value.is_valid() && (!value || value->tag() != tag)) return ERR_INVALID_PARAMETER;
	const auto old = lock_related_element(get_table_part(p_tag));
	element::ptr reference;
	for (const auto& child : table->children()) {
		if (child == old || child == value || litehtml::is_generated_tag(child->tag())) continue;
		if (tag == litehtml::_caption_ || (tag == litehtml::_thead_ && HTMLDOM::is_element(*child) &&
			child->tag() != litehtml::_caption_ && child->tag() != litehtml::_colgroup_)) {
			reference = child;
			break;
		}
	}
	return get_dom().set_table_child(table, value, old, reference);
}

Ref<HTMLElement> HTMLTableElement::create_table_part(const char* p_tag) {
	if (!is_valid()) return {};
	if (const auto part = get_table_part(p_tag); part.is_valid()) return part;
	const auto part = get_dom().create_element(String(p_tag).utf8());
	return part.is_valid() && set_table_part(p_tag, part) == OK ? part : Ref<HTMLElement>();
}

Ref<HTMLTableCaptionElement> HTMLTableElement::get_caption() const { return get_table_part("caption"); }
Error HTMLTableElement::set_caption(const Ref<HTMLTableCaptionElement>& p_value) { return set_table_part("caption", p_value); }
Ref<HTMLTableCaptionElement> HTMLTableElement::create_caption() { return create_table_part("caption"); }
Error HTMLTableElement::delete_caption() { return set_table_part("caption", {}); }
Ref<HTMLTableSectionElement> HTMLTableElement::get_t_head() const { return get_table_part("thead"); }
Error HTMLTableElement::set_t_head(const Ref<HTMLTableSectionElement>& p_value) { return set_table_part("thead", p_value); }
Ref<HTMLTableSectionElement> HTMLTableElement::create_t_head() { return create_table_part("thead"); }
Error HTMLTableElement::delete_t_head() { return set_table_part("thead", {}); }
Ref<HTMLTableSectionElement> HTMLTableElement::get_t_foot() const { return get_table_part("tfoot"); }
Error HTMLTableElement::set_t_foot(const Ref<HTMLTableSectionElement>& p_value) { return set_table_part("tfoot", p_value); }
Ref<HTMLTableSectionElement> HTMLTableElement::create_t_foot() { return create_table_part("tfoot"); }
Error HTMLTableElement::delete_t_foot() { return set_table_part("tfoot", {}); }

TypedArray<HTMLTableSectionElement> HTMLTableElement::get_t_bodies() const {
	TypedArray<HTMLTableSectionElement> bodies;
	if (const auto table = lock_element()) {
		append_children(get_dom(), *table, bodies, [](string_id p_tag) { return p_tag == litehtml::_tbody_; });
	}
	return bodies;
}

Ref<HTMLTableSectionElement> HTMLTableElement::create_t_body() {
	const auto table = lock_element();
	if (!table) return {};
	const auto& children = table->children();
	element::ptr reference;
	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if ((*it)->tag() == litehtml::_tbody_) {
			if (it != children.rbegin()) reference = *std::prev(it);
			break;
		}
	}
	const Ref<HTMLTableSectionElement> section = get_dom().create_element(String("tbody").utf8());
	return section.is_valid() && get_dom().insert_child(table, lock_related_element(section), reference) == OK ? section : Ref<HTMLTableSectionElement>();
}

TypedArray<HTMLTableRowElement> HTMLTableElement::get_rows() const {
	TypedArray<HTMLTableRowElement> rows;
	const auto table = lock_element();
	if (!table) return rows;
	for (const auto& child : table->children()) {
		if (child->tag() == litehtml::_thead_) append_rows(get_dom(), *child, rows);
	}
	for (const auto& child : table->children()) {
		if (child->tag() == litehtml::_tr_) rows.append(get_dom().make_element(child));
		else if (child->tag() == litehtml::_tbody_) append_rows(get_dom(), *child, rows);
	}
	for (const auto& child : table->children()) {
		if (child->tag() == litehtml::_tfoot_) append_rows(get_dom(), *child, rows);
	}
	return rows;
}

Ref<HTMLTableRowElement> HTMLTableElement::insert_row(int64_t p_index) {
	if (!is_valid()) return {};
	const auto rows = get_rows();
	if (p_index < -1 || p_index > rows.size()) return {};
	if (rows.is_empty()) {
		const auto bodies = get_t_bodies();
		if (!bodies.is_empty()) {
			const Ref<HTMLTableSectionElement> body = bodies[bodies.size() - 1];
			return body->insert_row();
		}
		const Ref<HTMLTableSectionElement> body = get_dom().create_element(String("tbody").utf8());
		if (body.is_null()) return {};
		const auto row = body->insert_row();
		return row.is_valid() && append_child(body) == OK ? row : Ref<HTMLTableRowElement>();
	}
	const bool append = p_index == -1 || p_index == rows.size();
	const Ref<HTMLTableRowElement> target = rows[append ? rows.size() - 1 : p_index];
	const auto parent = target->get_parent();
	const Ref<HTMLTableRowElement> row = get_dom().create_element(String("tr").utf8());
	const Ref<HTMLElement> reference = append ? Ref<HTMLElement>() : Ref<HTMLElement>(target);
	return row.is_valid() && parent->insert_before(row, reference) == OK ? row : Ref<HTMLTableRowElement>();
}

Error HTMLTableElement::delete_row(int64_t p_index) {
	return is_valid() ? delete_item(get_rows(), p_index) : ERR_INVALID_PARAMETER;
}
