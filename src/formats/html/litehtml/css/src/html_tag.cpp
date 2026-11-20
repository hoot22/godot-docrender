#include <algorithm>

#include "html.h"
#include "formats/html/css/css_litehtml_dom.h"
#include "html_tag.h"
#include "document.h"
#include "html_microsyntaxes.h"
#include "iterators.h"
#include "stylesheet.h"
#include "table.h"
#include "line_box.h"
#include "render_item.h"
#include "internal.h"
#include "document_container.h"
#include "el_before_after.h"
#include "el_image.h"

namespace litehtml
{

static bool is_generated_content(const element& el)
{
	return is_generated_tag(el.tag());
}

html_tag::~html_tag() = default;

static bool is_selector_tree_element(const element& el)
{
	return el.tag() != empty_id && !is_generated_content(el);
}

litehtml::html_tag::html_tag(const std::shared_ptr<document>& doc) : element(doc)
{
	m_tag = empty_id;
	m_id = empty_id;
}

litehtml::html_tag::html_tag(const element::ptr& parent, const string& style) : element(parent->get_document()),
	m_tag(empty_id),
	m_id(empty_id)
{
	litehtml::style st;
	st.add(style);
	add_style(st);
	this->parent(parent);
	compute_styles();
}

bool litehtml::html_tag::appendChild(const element::ptr &el)
{
	if(el)
	{
		el->parent(shared_from_this());
		m_children.push_back(el);
		return true;
	}
	return false;
}

bool litehtml::html_tag::removeChild(const element::ptr &el)
{
	if(el && el->parent() == shared_from_this())
	{
		el->parent(nullptr);
		m_children.erase(std::remove(m_children.begin(), m_children.end(), el), m_children.end());
		return true;
	}
	return false;
}

void litehtml::html_tag::clearRecursive()
{
	for(auto& el : m_children)
	{
		el->clearRecursive();
		el->parent(nullptr);
	}
	m_children.clear();
}

string_id html_tag::id() const
{
	return m_id;
}

string_id html_tag::tag() const
{
	return m_tag;
}

const char* html_tag::get_tagName() const
{
	return _s(m_tag).c_str();
}

void html_tag::set_tagName( const char* tag )
{
	m_tag = _id(lowcase(tag));
}

void html_tag::set_attr( const char* _name, const char* _val )
{
	if (_name && _val)
	{
		// attribute names in attribute selector are matched ASCII case-insensitively regardless of document mode
		string name = lowcase(_name);
		// m_attrs has all attribute values, including class and id, in their original case
		// because in attribute selector values are matched case-sensitively even in quirks mode
		m_attrs[name] = _val;

		if (name == "class")
		{
			string val = _val;
			// class names in class selector (.xxx) are matched ASCII case-insensitively in quirks mode
			if (get_document()->mode() == quirks_mode) lcase(val);
			m_str_classes = split_string(val, whitespace, "", "");
			m_classes.clear();
			for (auto cls : m_str_classes) m_classes.push_back(_id(cls));
		}
		else if (name == "id")
		{
			string val = _val;
			// ids in id selector (#xxx) are matched ASCII case-insensitively in quirks mode
			if (get_document()->mode() == quirks_mode) lcase(val);
			m_id = _id(val);
		}
	}
}

const char* html_tag::get_attr( const char* name, const char* def ) const
{
	auto attr = m_attrs.find(name);
	if(attr != m_attrs.end())
	{
		return attr->second.c_str();
	}
	return def;
}

litehtml::elements_list litehtml::html_tag::select_all(const string& selector )
{
	css_selector sel;
	sel.parse(selector, get_document()->mode());

	return select_all(sel);
}

litehtml::elements_list litehtml::html_tag::select_all(const css_selector& selector )
{
	litehtml::elements_list res;
	select_all(selector, res);
	return res;
}

void litehtml::html_tag::select_all(const css_selector& selector, elements_list& res)
{
	if(select(selector))
	{
		res.push_back(shared_from_this());
	}

	for(auto& el : m_children)
	{
		el->select_all(selector, res);
	}
}


litehtml::element::ptr litehtml::html_tag::select_one( const string& selector )
{
	css_selector sel;
	sel.parse(selector, get_document()->mode());

	return select_one(sel);
}

litehtml::element::ptr litehtml::html_tag::select_one( const css_selector& selector )
{
	if(select(selector))
	{
		return shared_from_this();
	}

	for(auto& el : m_children)
	{
		element::ptr res = el->select_one(selector);
		if(res)
		{
			return res;
		}
	}
	return nullptr;
}

bool html_tag::apply_auxiliary_pseudo(int match, const css_selector& selector,
	const dockit::css::CascadePriority& priority)
{
	if (match & select_match_with_backdrop)
	{
		if (!has_pseudo_class(_id("modal"))) return true;
		if (!m_auxiliary_pseudos) m_auxiliary_pseudos = std::make_unique<auxiliary_pseudo_styles>();
		auto& backdrop = m_auxiliary_pseudos->backdrop;
		if (!backdrop)
		{
			backdrop = std::make_shared<html_tag>(get_document());
			backdrop->set_tagName("-tag-backdrop");
			backdrop->parent(shared_from_this());
		}
		backdrop->m_style.combine(*selector.m_style, priority);
		return true;
	}
	if (!(match & (select_match_with_placeholder | select_match_with_selection))) return false;
	if (match & select_match_with_placeholder)
	{
		if (tag() != _input_ && tag() != _textarea_) return true;
	}
	if (!m_auxiliary_pseudos) m_auxiliary_pseudos = std::make_unique<auxiliary_pseudo_styles>();
	auto& declarations = (match & select_match_with_placeholder) ? m_auxiliary_pseudos->placeholder : m_auxiliary_pseudos->selection;
	if (!declarations) declarations = std::make_unique<style>();
	declarations->combine(*selector.m_style, priority);
	return true;
}

void html_tag::apply_matched_style(int match, const css_selector& selector, int proximity, bool create_generated)
{
	const auto priority = selector.cascade_priority(proximity);
	if (!(match & select_match_with_pseudo_element))
	{
		m_style.combine(*selector.m_style, priority);
		return;
	}
	if (apply_auxiliary_pseudo(match, selector, priority)) return;

	element::ptr generated;
	if (match & select_match_with_marker)
		generated = get_element_marker(create_generated);
	else if (match & select_match_with_after)
		generated = get_element_after(*selector.m_style, create_generated);
	else if (match & select_match_with_before)
		generated = get_element_before(*selector.m_style, create_generated);
	if (generated) static_cast<html_tag&>(*generated).m_style.combine(*selector.m_style, priority);
}

void litehtml::html_tag::apply_stylesheet( const litehtml::css& stylesheet )
{
	const string tag_name = _s(m_tag);
	for(const auto& sel : stylesheet.selectors())
	{
		const auto& right = sel->m_syntax->m_right;
		if (right.m_tag != "*" && right.m_tag != tag_name)
			continue;
		if (!right.m_attrs.empty() && right.m_attrs.front().type == dockit::css::select_class &&
			std::find(m_str_classes.begin(), m_str_classes.end(), right.m_attrs.front().name) == m_str_classes.end())
			continue;

		int proximity = 0;
		int apply = match_style_selector(*sel, false, proximity);
		if (apply == select_no_match) continue;
		auto used = std::make_unique<used_selector>(sel, false);
		if (sel->is_media_valid())
		{
			if ((apply & select_match_pseudo_class) || sel->m_container_query) apply = match_style_selector(*sel, true, proximity);
			if (apply != select_no_match)
			{
				apply_matched_style(apply, *sel, proximity, true);
				used->m_used = true;
			}
		}
		m_used_styles.push_back(std::move(used));
	}

	for(auto& el : m_children)
	{
		if(is_selector_tree_element(*el))
		{
			el->apply_stylesheet(stylesheet);
		}
	}
}

void litehtml::html_tag::get_content_size( size& sz, pixel_t max_width )
{
	sz.height	= 0;
	if(m_css.get_display() == display_block)
	{
		sz.width	= max_width;
	} else
	{
		sz.width	= 0;
	}
}

void litehtml::html_tag::draw(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip, const std::shared_ptr<render_item> &ri)
{
	position pos = ri->pos();
	pos.x	+= x;
	pos.y	+= y;

	draw_background(hdc, x, y, clip, ri);
}

bool html_tag::get_custom_property(string_id name, css_token_vector& result) const
{
	for (auto tag = this; tag;)
	{
		bool local = false;
		if (const auto* value = tag->m_style.get_custom_property(name, local))
		{
			result = *value;
			return true;
		}
		if (local) return false;
		tag = dynamic_cast<const html_tag*>(tag->inheritance_parent().get());
	}
	return false;
}

void html_tag::apply_inline_style()
{
	if (const char* style = get_attr("style"))
		m_style.add(style, "", get_document()->container(), this);
}

void litehtml::html_tag::compute_styles(bool recursive)
{
	document::ptr doc = get_document();
	m_style.resolve_cascade(*this);

	if (const auto* diagnostics = m_style.get_diagnostics())
	{
		for (const auto& diagnostic : *diagnostics)
			doc->container()->report_css_diagnostic(diagnostic, this);
	}
	for (const auto& diagnostic : m_style.get_computed_diagnostics())
		doc->container()->report_css_diagnostic(diagnostic, this);

	m_css.compute(this, doc);
	if (has_pseudo_class(_id("modal"))) {
		m_css.set_position(element_position_fixed);
		m_css.set_float(float_none);
		if (m_css.get_display() == display_inline || m_css.get_display() == display_contents) m_css.set_display(display_block);
	}
	m_content_alternative.reset();
	if (const auto* image = m_css.get_content_image()) {
		if (!m_content_image) m_content_image = std::make_unique<css_replaced_image>();
		m_content_image->set(*image, m_css.get_content().base_url);
	} else {
		m_content_image.reset();
	}
	if (m_auxiliary_pseudos)
	{
		if (const auto& backdrop = m_auxiliary_pseudos->backdrop) backdrop->compute_styles(false);
		const auto resolve = [&](style& declarations) {
			declarations.resolve_cascade(*this, true);
			for (const auto& diagnostic : declarations.get_computed_diagnostics())
				doc->container()->report_css_diagnostic(diagnostic, this);
		};
		if (m_auxiliary_pseudos->selection) resolve(*m_auxiliary_pseudos->selection);
		if (m_auxiliary_pseudos->placeholder)
		{
			resolve(*m_auxiliary_pseudos->placeholder);
			m_auxiliary_pseudos->placeholder_css = std::make_unique<css_properties>();
			m_auxiliary_pseudos->placeholder_css->compute_placeholder(*this, *m_auxiliary_pseudos->placeholder, *doc);
		}
	}
	compute_selection_colors();
	if (!is_generated_content(*this)) {
		handle_counter_properties();
		if (m_content_image) compute_content_alternative();
	}

	if (recursive)
	{
		compute_child_styles();
	}
}

bool html_tag::append_content_text(const css_content_item& item, string& text)
{
	using kind = css_content_kind;
	if (item.kind == kind::text) {
		text += item.value;
		return true;
	}
	if (item.kind == kind::attribute) {
		const auto origin = is_generated_content(*this) ? parent() : nullptr;
		const char* value = (origin ? *origin : *this).get_attr(item.value.c_str());
		text += value ? value : item.fallback_or_separator;
		return true;
	}
	if (!is_one_of(item.kind, kind::counter, kind::marker_counter, kind::counters)) return false;
	const auto doc = get_document();
	const auto lookup = [&](const string& name) { return doc->find_counter_style(name); };
	const auto counter = _id(item.value);
	if (item.kind == kind::counters) {
		const auto values = get_counter_values(counter);
		for (size_t i = 0; i < values.size(); ++i) {
			if (i != 0) text += item.fallback_or_separator;
			text += format_css_counter(values[i], item.counter_style, lookup);
		}
	} else {
		text += format_css_counter(get_counter_value(counter), item.counter_style, lookup, item.kind == kind::marker_counter);
	}
	return true;
}

void html_tag::compute_content_alternative()
{
	const auto& alternative = css().get_content().alternative;
	if (alternative.empty()) return;
	auto& text = m_content_alternative.emplace();
	for (const auto& item : alternative) append_content_text(item, text);
}

const css_properties* html_tag::get_placeholder_style() const
{
	return m_auxiliary_pseudos ? m_auxiliary_pseudos->placeholder_css.get() : nullptr;
}

bool html_tag::has_placeholder_color() const
{
	return m_auxiliary_pseudos && m_auxiliary_pseudos->placeholder &&
		!m_auxiliary_pseudos->placeholder->get_property(_color_).is<invalid>();
}

void html_tag::compute_selection_colors()
{
	const auto parent = this->parent();
	const auto* tag = dynamic_cast<const html_tag*>(parent.get());
	m_selection_colors = tag ? tag->m_selection_colors : nullptr;
	if (!m_auxiliary_pseudos || !m_auxiliary_pseudos->selection) return;
	const auto& declarations = *m_auxiliary_pseudos->selection;
	const auto& foreground = declarations.get_property(_color_);
	const auto& background = declarations.get_property(_background_color_);
	const bool author = foreground.priority.layer != css_default_layer || background.priority.layer != css_default_layer;
	if (!author && !foreground.is<web_color>() && !foreground.is<initial_value>() &&
		!background.is<web_color>() && !background.is<initial_value>()) return;

	auto colors = std::make_shared<cascaded_selection_colors>(m_selection_colors ? *m_selection_colors : cascaded_selection_colors{});
	colors->author |= author;
	const auto scheme = css().get_color().scheme;
	if (foreground.is<web_color>()) colors->foreground = foreground.get<web_color>().compute_for_scheme(scheme);
	else if (foreground.is<initial_value>()) colors->foreground = web_color::black;
	if (background.is<web_color>()) colors->background = background.get<web_color>().compute_for_scheme(scheme);
	else if (background.is<initial_value>()) colors->background = web_color::transparent;
	m_selection_colors = std::move(colors);
}

html_tag::selection_colors html_tag::get_selection_colors(const web_color& text_color) const
{
	if (!m_selection_colors) return {};
	const auto& colors = *m_selection_colors;
	selection_colors result;
	if (colors.foreground) result.foreground = colors.foreground->resolve(text_color);
	if (colors.background) result.background = colors.background->resolve(result.foreground.value_or(text_color));
	else if (colors.author) result.background = web_color::transparent;
	return result;
}

bool html_tag::has_author_property(string_id name) const
{
	const auto& value = get_cascaded_property(name);
	return value.priority.layer != css_default_layer;
}

bool litehtml::html_tag::is_white_space() const
{
	return false;
}

int	html_tag::select(const css_selector::vector& selector_list, bool apply_pseudo)
{
	for (auto sel : selector_list)
	{
		if (int result = select(*sel, apply_pseudo))
			return result;
	}
	return select_no_match;
}

int litehtml::html_tag::select(const string& selector)
{
	css_selector sel;
	sel.parse(selector, get_document()->mode());
	return select(sel, true);
}

int litehtml::html_tag::select(const css_selector& selector, bool apply_pseudo)
{
	int proximity = 0;
	return match_style_selector(selector, apply_pseudo, proximity);
}

int html_tag::match_style_selector(const css_selector& selector, bool apply_pseudo, int& proximity)
{
	proximity = std::numeric_limits<int>::max();
	if (!selector.m_syntax) return select_no_match;
	namespace syntax = dockit::css;
	const auto mode = apply_pseudo ? syntax::MatchMode::complete : syntax::MatchMode::candidate;
	int flags;
	if (selector.m_scope)
	{
		const auto match = syntax::match_scoped_selector(selector_dom_view(), this, *selector.m_syntax, *selector.m_scope, mode);
		proximity = match.proximity;
		flags = selector_match_flags(match.flags);
	}
	else
		flags = selector_match_flags(syntax::match_selector(selector_dom_view(), this, *selector.m_syntax, {nullptr, mode}));
	if (flags != select_no_match && apply_pseudo && selector.m_container_query &&
		!get_document()->matches_container_query(*selector.m_container_query, *this, flags & select_match_with_pseudo_element))
		return select_no_match;
	return flags;
}
void litehtml::html_tag::parse_attributes()
{
	for(auto& el : m_children)
	{
		el->parse_attributes();
	}
}

void litehtml::html_tag::get_text( string& text ) const
{
	for (auto& el : m_children)
	{
		el->get_text(text);
	}
}

bool litehtml::html_tag::is_body()  const
{
	return false;
}

void litehtml::html_tag::set_data( const char* /*data*/ )
{

}

bool litehtml::html_tag::on_mouse_over()
{
	bool ret = false;

	element::ptr el = shared_from_this();
	while(el)
	{
		if(el->set_pseudo_class(_hover_, true))
		{
			ret = true;
		}
		el = el->parent();
	}

	return ret;
}

bool litehtml::html_tag::on_mouse_leave()
{
	bool ret = false;

	element::ptr el = shared_from_this();
	while(el)
	{
		if(el->set_pseudo_class(_hover_, false))
		{
			ret = true;
		}
		if(el->set_pseudo_class(_active_, false))
		{
			ret = true;
		}
		el = el->parent();
	}

	return ret;
}

bool litehtml::html_tag::on_lbutton_down()
{
	bool ret = false;

	element::ptr el = shared_from_this();
	while (el)
	{
		if (el->set_pseudo_class(_active_, true))
		{
			ret = true;
		}
		el = el->parent();
	}

	return ret;
}

bool litehtml::html_tag::on_lbutton_up(bool is_click)
{
	bool ret = false;

	element::ptr el = shared_from_this();
	while (el)
	{
		if (el->set_pseudo_class(_active_, false))
		{
			ret = true;
		}
		el = el->parent();
	}

	if (is_click) on_click();

	return ret;
}

void litehtml::html_tag::on_click()
{
	if (!is_root())
	{
		if(!get_document()->container()->on_element_click(shared_from_this()))
		{
			element::ptr el_parent = parent();
			if (el_parent)
			{
				el_parent->on_click();
			}
		}
	}
}

bool litehtml::html_tag::is_break() const
{
	return false;
}

void litehtml::html_tag::draw_background(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip,
										 const std::shared_ptr<render_item> &ri)
{

	if (ri->empty_table_cell()) return;
	const bool has_border_image = !m_css.get_border_image().source.value.is_empty();
	std::shared_ptr<render_item> fieldset_legend;
	if (tag() == _fieldset_) {
		for (const auto& child : ri->children()) {
			const auto position = child->src_el()->css().get_position();
			if (child->src_el()->tag() == _legend_ && child->src_el()->css().get_float() == float_none &&
				position != element_position_absolute && position != element_position_fixed) {
				fieldset_legend = child;
				break;
			}
		}
	}
	const auto paint_borders = [&](borders decoration, const position& box, bool root, const position* image_box, bool first, bool last) {
		if (ri->shares_table_borders()) return;
		const bool collapsed = m_css.get_border_collapse() == border_collapse_collapse &&
			is_one_of(m_css.get_display(), display_table, display_inline_table, display_table_cell, display_table_row,
				display_table_row_group, display_table_header_group, display_table_footer_group);
		if (has_border_image && !collapsed)
		{
			const auto& image_border = image_box ? m_css.get_borders() : decoration;
			if (get_document()->container()->draw_border_image(*this, image_border, image_box ? *image_box : box,
				image_box ? &box : nullptr, first, last)) return;
		}
		if (!decoration.is_visible()) return;
		for (auto* side : { &decoration.left, &decoration.right, &decoration.top, &decoration.bottom })
		{
			side->color = side->color.to_srgb(m_css.get_color());
			if (collapsed && side->style == border_style_inset)
				side->style = border_style_ridge;
			else if (collapsed && side->style == border_style_outset)
				side->style = border_style_groove;
		}
		get_document()->container()->draw_borders(hdc, decoration, box, root);
	};
	const auto paint_fieldset_borders = [&](const borders& decoration, const position& box,
		const position& content_origin, bool root) {
		if (!fieldset_legend || has_border_image || decoration.top.width <= 0) {
			paint_borders(decoration, box, root, nullptr, true, true);
			return;
		}

		position legend_box = fieldset_legend->pos();
		legend_box.x += content_origin.x;
		legend_box.y += content_origin.y;
		legend_box += fieldset_legend->get_paddings();
		legend_box += fieldset_legend->get_borders();
		position legend_margin_box = legend_box;
		legend_margin_box += fieldset_legend->get_margins();
		if (legend_margin_box.bottom() <= box.y || legend_margin_box.top() >= box.y + decoration.top.width) {
			paint_borders(decoration, box, root, nullptr, true, true);
			return;
		}

		const pixel_t gap_start = std::clamp(legend_margin_box.left(), box.x, box.right());
		const pixel_t gap_end = std::clamp(legend_margin_box.right(), box.x, box.right());
		if (gap_end <= gap_start) {
			paint_borders(decoration, box, root, nullptr, true, true);
			return;
		}

		borders sides = decoration;
		sides.top.width = 0;
		sides.top.style = border_style_none;
		paint_borders(sides, box, root, nullptr, true, true);

		const auto paint_top_segment = [&](pixel_t p_left, pixel_t p_right, bool p_left_corner, bool p_right_corner) {
			if (p_right <= p_left) return;
			borders top;
			top.top = decoration.top;
			top.radius = decoration.radius;
			if (!p_left_corner) top.radius.top_left_x = top.radius.top_left_y = 0;
			if (!p_right_corner) top.radius.top_right_x = top.radius.top_right_y = 0;
			top.radius.bottom_right_x = top.radius.bottom_right_y = 0;
			top.radius.bottom_left_x = top.radius.bottom_left_y = 0;
			position segment = box;
			segment.x = p_left;
			segment.width = p_right - p_left;
			paint_borders(top, segment, root, nullptr, true, true);
		};
		paint_top_segment(box.x, gap_start, true, false);
		paint_top_segment(gap_end, box.right(), false, true);
	};
	const auto paint_shadows = [&](position box, borders decoration, bool inset) {
		box.round();
		get_document()->container()->draw_box_shadows(*this, box, decoration, inset);
	};
	const auto display = m_css.get_display();
	const bool painted_row_group = ri->establishes_paint_context() &&
		is_one_of(display, display_table_row_group, display_table_header_group, display_table_footer_group);
	if(display != display_inline && display != display_table_row && !painted_row_group)
	{
		position pos = ri->pos();
		pos.x	+= x;
		pos.y	+= y;

		position border_box = pos;
		border_box += ri->get_paddings();
		border_box += ri->get_borders();


		borders shadow_box = m_css.get_borders();
		shadow_box.radius = m_css.get_borders().radius.calc_percents(border_box.width, border_box.height);
		paint_shadows(border_box, shadow_box, false);
		if(has_border_image || border_box.does_intersect(clip) || is_root())
		{
			auto v_offset = ri->get_draw_vertical_offset();
			pos.y += v_offset;
			pos.height -= v_offset;

			const background* bg = get_background();
			if(bg)
			{
				bg->draw_layers(hdc, pos, this, ri, *get_document()->container(), [&](background_layer& layer)
				{
					if(is_root() && (clip != nullptr))
					{
						layer.clip_box = *clip;
						layer.border_box = *clip;
					}

					layer.border_box.round();
					layer.clip_box.round();
				});
			}

			paint_shadows(border_box, shadow_box, true);
			borders bdr = m_css.get_borders();
			if(has_border_image || bdr.is_visible())
			{
				border_box.round();
				bdr.radius = m_css.get_borders().radius.calc_percents(border_box.width, border_box.height);
				paint_fieldset_borders(bdr, border_box, pos, is_root());
			}
		}
	} else
	{
		const background* bg = get_background();

		position::vector boxes;
		ri->get_inline_boxes(boxes);

		// Slice a single horizontal decoration across an inline's line fragments.
		const bool split_image = has_border_image && m_css.get_display() == display_inline && boxes.size() > 1;
		pixel_t image_width = 0;
		if (split_image)
			for (const auto& fragment : boxes) image_width += fragment.width;
		pixel_t image_offset = 0;
		position content_box;

		for(auto box = boxes.begin(); box != boxes.end(); box++)
		{
			const auto fragment_width = box->width;
			box->x	+= x;
			box->y	+= y;

			if(has_border_image || box->does_intersect(clip))
			{
				content_box = *box;
				content_box -= ri->get_borders();
				content_box -= ri->get_paddings();

				css_borders bdr;

				// set left borders radius for the first box
				if(box == boxes.begin())
				{
					bdr.radius.bottom_left_x	= m_css.get_borders().radius.bottom_left_x;
					bdr.radius.bottom_left_y	= m_css.get_borders().radius.bottom_left_y;
					bdr.radius.top_left_x		= m_css.get_borders().radius.top_left_x;
					bdr.radius.top_left_y		= m_css.get_borders().radius.top_left_y;
				}

				// set right borders radius for the last box
				if(box == boxes.end() - 1)
				{
					bdr.radius.bottom_right_x	= m_css.get_borders().radius.bottom_right_x;
					bdr.radius.bottom_right_y	= m_css.get_borders().radius.bottom_right_y;
					bdr.radius.top_right_x		= m_css.get_borders().radius.top_right_x;
					bdr.radius.top_right_y		= m_css.get_borders().radius.top_right_y;
				}


				bdr.top		= m_css.get_borders().top;
				bdr.bottom	= m_css.get_borders().bottom;
				if(box == boxes.begin())
				{
					bdr.left	= m_css.get_borders().left;
				}
				if(box == boxes.end() - 1)
				{
					bdr.right	= m_css.get_borders().right;
				}


				borders shadow_box = bdr;
				shadow_box.radius = bdr.radius.calc_percents(box->width, box->height);
				paint_shadows(*box, shadow_box, false);
				if(bg)
				{
					bg->draw_layers(hdc, content_box, this, ri, *get_document()->container(), [&](background_layer& layer)
					{
						layer.border_radius = bdr.radius.calc_percents(box->width, box->height);
						layer.border_box.round();
						layer.clip_box.round();
					});
				}
				paint_shadows(*box, shadow_box, true);
				if(has_border_image || bdr.is_visible())
				{
					borders b = bdr;
					b.radius = bdr.radius.calc_percents(box->width, box->height);
					box->round();
					position image_box = *box;
					image_box.x -= image_offset;
					image_box.width = image_width;
					paint_borders(b, *box, false, split_image ? &image_box : nullptr,
						box == boxes.begin(), box == boxes.end() - 1);
				}
			}
			image_offset += fragment_width;
		}
	}
}

bool litehtml::html_tag::set_pseudo_class( string_id cls, bool add )
{
	bool ret = false;
	if(add)
	{
		if(std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), cls) == m_pseudo_classes.end())
		{
			m_pseudo_classes.push_back(cls);
			ret = true;
		}
	} else
	{
		auto pi = std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), cls);
		if(pi != m_pseudo_classes.end())
		{
			m_pseudo_classes.erase(pi);
			ret = true;
		}
	}
	return ret;
}

bool litehtml::html_tag::set_class( const char* pclass, bool add )
{
	string_vector classes;
	bool changed = false;

	split_string( pclass, classes, " " );

	if(add)
	{
		for( auto & _class : classes )
		{
			if(std::find(m_str_classes.begin(), m_str_classes.end(), _class) == m_str_classes.end())
			{
				m_str_classes.push_back( std::move( _class ) );
				changed = true;
			}
		}
	} else
	{
		for( const auto & _class : classes )
		{
			auto end = std::remove(m_str_classes.begin(), m_str_classes.end(), _class);

			if(end != m_str_classes.end())
			{
				m_str_classes.erase(end, m_str_classes.end());
				changed = true;
			}
		}
	}

	if( changed )
	{
		string class_string;
		join_string(class_string, m_str_classes, " ");
		set_attr("class", class_string.c_str());

		return true;
	}
	else
	{
		return false;
	}

}

bool litehtml::html_tag::is_replaced() const
{
	return false;
}

element::ptr html_tag::get_element_marker(bool create)
{
	if (!m_children.empty() && m_children.front()->tag() == marker_tag())
		return m_children.front();
	if (!create) return nullptr;
	auto element = std::make_shared<el_marker>(get_document());
	element->parent(shared_from_this());
	m_children.push_front(element);
	return element;
}

element::ptr html_tag::get_element_before(const style&, bool create)
{
	auto where = m_children.begin();
	if (where != m_children.end() && (*where)->tag() == marker_tag()) ++where;
	if (where != m_children.end() && (*where)->tag() == __tag_before_) return *where;
	if (!create) return nullptr;
	auto element = std::make_shared<el_before>(get_document());
	element->parent(shared_from_this());
	m_children.insert(where, element);
	return element;
}

litehtml::element::ptr litehtml::html_tag::get_element_after(const style& style, bool create)
{
	if(!m_children.empty())
	{
		if( m_children.back()->tag() == __tag_after_ )
		{
			return m_children.back();
		}
	}
	if(create)
	{
		return add_pseudo_after(style);
	}
	return nullptr;
}


void litehtml::html_tag::compute_child_styles()
{
	if (css().hides_contents()) return;
	if (css().get_display() == display_list_item)
		get_element_marker(true);
	const auto quote_scope = css().has_style_containment() ? get_document() : nullptr;
	const size_t outer_quote_depth = quote_scope ? quote_scope->quote_depth() : 0;
	const element* preceding = nullptr;
	if (shadow_tree) { shadow_tree->compute_styles(); return; }
	for (const auto& child : composed_children())
	{
		child->inherit_counters(preceding);
		child->compute_styles();
		if (child->tag() != empty_id && child->css().get_display() != display_none)
			preceding = child.get();
	}
	if (quote_scope)
		quote_scope->set_quote_depth(outer_quote_depth);
}

void litehtml::html_tag::handle_counter_properties()
{
	if (css().get_display() == display_none) return;
	for (auto ancestor = parent(); ancestor; ancestor = ancestor->parent())
	{
		if (ancestor->css().get_display() == display_none || ancestor->css().get_content_image()) return;
	}
	static const auto set = _id("counter-set");
	static const auto list_item = _id("list-item");
	static const css_counter_changes empty;
	const auto declarations = [&](string_id property) -> const css_counter_changes& {
		for (const auto* current = this; current;)
		{
			const auto& value = current->m_style.get_property(property);
			if (value.is<css_counter_changes>()) return value.get<css_counter_changes>();
			if (!value.is<inherit>()) break;
			current = dynamic_cast<const html_tag*>(current->parent().get());
		}
		return empty;
	};
	const auto& resets = declarations(_counter_reset_);
	const auto& increments = declarations(_counter_increment_);
	const auto& sets = declarations(set);
	const auto mentions_list_item = [&](const css_counter_change& change) { return change.name == list_item; };

	if (is_one_of(tag(), _ol_, _ul_, _id("menu"), _dir_) && !std::any_of(resets.begin(), resets.end(), mentions_list_item))
	{
		const bool reversed = tag() == _ol_ && get_attr("reversed");
		int start = 1;
		const char* start_attribute = tag() == _ol_ ? get_attr("start") : nullptr;
		if (reversed && (!start_attribute || !html_parse_integer(start_attribute, start)))
		{
			start = static_cast<int>(std::count_if(m_children.begin(), m_children.end(), [](const element::ptr& child) {
				return child->tag() == _li_;
			}));
		}
		else if (start_attribute)
			html_parse_integer(start_attribute, start);
		reset_list_counter(list_item, start, reversed);
	}
	for (const auto& change : resets) reset_counter(change.name, change.value);
	for (const auto& change : increments) increment_counter(change.name, change.value);

	if (css().get_display() == display_list_item)
	{
		if (!std::any_of(increments.begin(), increments.end(), mentions_list_item))
			increment_list_counter(list_item);
		if (!std::any_of(sets.begin(), sets.end(), mentions_list_item))
		{
			int value;
			const char* attribute = tag() == _li_ ? get_attr("value") : nullptr;
			if (attribute && html_parse_integer(attribute, value)) set_counter(list_item, value);
		}
	}
	for (const auto& change : sets) set_counter(change.name, change.value);
}

void litehtml::html_tag::add_style(const style& style)
{
	m_style.combine(style);
}

void litehtml::html_tag::refresh_styles()
{
	for (auto& el : m_children)
	{
		if(el->css().get_display() != display_inline_text)
		{
			el->refresh_styles();
		}
	}

	m_style.clear();
	m_auxiliary_pseudos.reset();

	for (auto& usel : m_used_styles)
	{
		usel->m_used = false;

		if(usel->m_selector->is_media_valid())
		{
			const auto& selector = *usel->m_selector;
			int proximity = 0;
			const int apply = match_style_selector(selector, true, proximity);
			if (apply == select_no_match) continue;
			apply_matched_style(apply, selector, proximity, false);
			usel->m_used = true;
		}
	}
	apply_inline_style();
}

const litehtml::background* litehtml::html_tag::get_background(bool own_only)
{
	if(own_only)
	{
		// return own background with check for empty one
		if(m_css.get_bg().is_empty())
		{
			return nullptr;
		}
		return &m_css.get_bg();
	}

	if(m_css.get_bg().is_empty())
	{
		// if this is root element (<html>) try to get background from body
		if (is_root() && !css().has_containment())
		{
			for (const auto& el : m_children)
			{
				if( el->is_body() && !el->css().has_containment() )
				{
					// return own body background
					return el->get_background(true);
				}
			}
		}
		return nullptr;
	}

	if(is_body() && !css().has_containment())
	{
		element::ptr el_parent = parent();
		if (el_parent)
		{
			if (!el_parent->css().has_containment() && !el_parent->get_background(true))
			{
				// parent of body will draw background for body
				return nullptr;
			}
		}
	}

	return &m_css.get_bg();
}

string html_tag::dump_get_name()
{
	if(m_tag == empty_id)
	{
		return "anon [html_tag]";
	}
	return _s(m_tag) + " [html_tag]";
}

// https://html.spec.whatwg.org/multipage/rendering.html#maps-to-the-pixel-length-property
void html_tag::map_to_pixel_length_property(string_id prop_name, string attr_value)
{
	int n;
	if (html_parse_non_negative_integer(attr_value, n))
	{
		css_token tok(DIMENSION, (float)n, css_number_integer, "px");
		m_style.add_property(prop_name, {tok});
	}
}

// https://html.spec.whatwg.org/multipage/rendering.html#tables-2:attr-table-border
void html_tag::map_to_pixel_length_property_with_default_value(string_id prop_name, string attr_value, int default_value)
{
	int n = default_value;
	html_parse_non_negative_integer(attr_value, n);
	css_token tok(DIMENSION, (float)n, css_number_integer, "px");
	m_style.add_property(prop_name, {tok});
}

// https://html.spec.whatwg.org/multipage/rendering.html#maps-to-the-dimension-property
void html_tag::map_to_dimension_property(string_id prop_name, string attr_value)
{
	float x = 0;
	html_dimension_type type = html_length;
	if (!html_parse_dimension_value(attr_value, x, type))
		return;

	css_token tok;
	if (type == html_length)
		tok = {DIMENSION,  x, css_number_number, "px"};
	else
		tok = {PERCENTAGE, x, css_number_number};

	m_style.add_property(prop_name, {tok});
}

// https://html.spec.whatwg.org/multipage/rendering.html#maps-to-the-dimension-property-(ignoring-zero)
void html_tag::map_to_dimension_property_ignoring_zero(string_id prop_name, string attr_value)
{
	float x = 0;
	html_dimension_type type = html_length;
	if (!html_parse_nonzero_dimension_value(attr_value, x, type))
		return;

	css_token tok;
	if (type == html_length)
		tok = {DIMENSION,  x, css_number_number, "px"};
	else
		tok = {PERCENTAGE, x, css_number_number};

	m_style.add_property(prop_name, {tok});
}

} // namespace litehtml
