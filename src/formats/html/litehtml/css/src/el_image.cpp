#include "el_image.h"
#include "formats/html/css/css_image_value.h"
#include "render_image.h"
#include "document_container.h"
#include "html_microsyntaxes.h"

#include <utility>

litehtml::el_image::el_image(const document::ptr& doc, const string& baseurl) :
	html_tag(doc), m_baseurl(baseurl)
{
	m_css.set_display(display_inline_block);
}

bool litehtml::el_image::select_source(dockit::HTMLImageSelection p_selection)
{
	const bool dimensions_changed = m_selection ?
		!(m_selection->dimensions == p_selection.dimensions) : p_selection.dimensions.has_value();
	m_selection = std::move(p_selection);
	return dimensions_changed;
}

void litehtml::el_image::commit_source()
{
	if (m_selection) m_current_src = m_selection->resolved_source;
}

const litehtml::string& litehtml::el_image::image_source() const
{
	return m_selection ? m_selection->source : m_src;
}

void litehtml::css_replaced_image::set(const image& value, const string& base)
{
	source.m_baseurl = base;
	if (source.m_image.empty()) source.m_image.push_back(value);
	else source.m_image.front() = value;
}

litehtml::size litehtml::css_replaced_image::natural_size(document_container& container) const
{
	return get_css_image_natural_size(source.m_image.front(), source.m_baseurl, container);
}

void litehtml::el_image::set_generated_image(const image& value, const string& base, bool list_marker)
{
	if (!m_generated_image) m_generated_image = std::make_unique<css_replaced_image>();
	m_generated_image->set(value, base);
	m_list_marker = list_marker;
}

void litehtml::el_image::get_content_size( size& sz, pixel_t /*max_width*/ )
{
	auto* container = get_document()->container();
	if (m_generated_image) {
		sz = m_generated_image->natural_size(*container);
		if (m_list_marker && (sz.width <= 0 || sz.height <= 0)) {
			const auto em = css().get_font_size();
			sz = {em, em};
		}
		return;
	}
	const auto& source = image_source();
	const auto source_density = m_selection ? m_selection->density : std::nullopt;
	const double density = resolve_image_dppx(source, m_baseurl, *container, css().get_image_resolution(), false, source_density);
	container->get_css_image_size(source.c_str(), m_baseurl.c_str(), density, sz, css().get_image_orientation());
}

bool litehtml::el_image::is_replaced() const
{
	return true;
}

void litehtml::el_image::parse_attributes()
{
	if (!m_selection) {
		const auto image_parent = parent();
		if (!get_attr("srcset") && (!image_parent || image_parent->tag() != _picture_)) {
			m_src = get_attr("src", "");
		}
	}

	// https://html.spec.whatwg.org/multipage/rendering.html#attributes-for-embedded-content-and-images:the-img-element-5
	const auto* dimensions = m_selection && m_selection->dimensions ? &*m_selection->dimensions : nullptr;
	const char* width = dimensions ? dimensions->width.c_str() : get_attr("width");
	const char* height = dimensions ? dimensions->height.c_str() : get_attr("height");
	if (width) map_to_dimension_property(_width_, width);
	if (height) map_to_dimension_property(_height_, height);

	float w = 0;
	float h = 0;
	html_dimension_type w_type;
	html_dimension_type h_type;
	if (width && height && html_parse_dimension_value(width, w, w_type) &&
		html_parse_dimension_value(height, h, h_type) && w_type == html_length && h_type == html_length)
	{
		m_style.add_property(_aspect_ratio_, css_token_vector{
			{ IDENT, "auto" }, { NUMBER, w, css_number_number }, { static_cast<css_token_type>('/') }, { NUMBER, h, css_number_number },
		});
	}
}

namespace
{
	litehtml::size fit_object(litehtml::size box, litehtml::size natural, litehtml::object_fit fit)
	{
		using litehtml::object_fit;
		if (fit == object_fit::fill || natural.width <= 0 || natural.height <= 0) return box;
		if (fit == object_fit::none) return natural;

		const auto horizontal = box.width / natural.width;
		const auto vertical = box.height / natural.height;
		const bool cover = fit == object_fit::cover || fit == object_fit::cover_scale_down;
		auto scale = cover ? std::max(horizontal, vertical) : std::min(horizontal, vertical);
		if (fit == object_fit::scale_down || fit == object_fit::cover_scale_down)
			scale = std::min(scale, 1.0f);
		return { natural.width * scale, natural.height * scale };
	}

	// Reuse the object sizing and clip for HTML images and CSS replaced content.
	litehtml::background_layer make_image_layer(litehtml::html_tag& owner, const litehtml::position& pos,
		litehtml::render_item& render, const litehtml::css_replaced_image* replacement = nullptr)
	{
		using namespace litehtml;
		background_layer layer;
		layer.clip_box = pos;
		layer.origin_box = pos;
		const auto& style = owner.css();
		const auto fit = style.get_object_fit();
		if (fit != object_fit::fill) {
			size natural;
			if (replacement) natural = replacement->natural_size(*owner.get_document()->container());
			else owner.get_content_size(natural, pos.width);
			const auto extent = fit_object({pos.width, pos.height}, natural, fit);
			layer.origin_box.width = extent.width;
			layer.origin_box.height = extent.height;
		}
		const auto& object_position = style.get_object_position();
		layer.origin_box.x += object_position.x.resolve(pos.width - layer.origin_box.width);
		layer.origin_box.y += object_position.y.resolve(pos.height - layer.origin_box.height);
		layer.border_box = pos;
		layer.border_box += render.get_paddings();
		layer.border_box += render.get_borders();
		layer.repeat = {background_repeat::no_repeat, background_repeat::no_repeat};
		layer.border_radius = style.get_borders().radius.calc_percents(layer.border_box.width, layer.border_box.height);
		return layer;
	}
}

litehtml::background_layer litehtml::make_replaced_object_layer(html_tag& p_owner, const position& p_box, render_item& p_item)
{
	return make_image_layer(p_owner, p_box, p_item);
}

void litehtml::css_replaced_image::draw(html_tag& owner, uint_ptr hdc, const position& box, render_item& render)
{
	if (source.m_image.front().is_empty()) return;
	auto layer = make_image_layer(owner, box, render, this);
	const auto& value = source.m_image.front();
	source.m_current_color = owner.css().get_color();
	source.m_direction = owner.css().get_bg().m_direction;
	layer.image_orientation = value.orientation;
	layer.flip_image_x = value.direction && static_cast<int>(*value.direction) != source.m_direction;
	layer.fill_image_tile = value.type == image::type_solid;
	source.draw_layer(hdc, 0, layer, owner.get_document()->container());
}

void litehtml::el_image::draw(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip, const std::shared_ptr<render_item> &ri)
{
	html_tag::draw(hdc, x, y, clip, ri);
	position pos = ri->pos();
	pos.x += x;
	pos.y += y;
	if (m_alt_text)
	{
		if (pos.width > 0 && pos.height > 0 && pos.does_intersect(clip))
			get_document()->container()->draw_image_alt(*m_alt_text, pos, *this);
		return;
	}
	pos.round();
	if (pos.width <= 0 || pos.height <= 0 || !pos.does_intersect(clip)) return;
	if (m_generated_image) {
		m_generated_image->draw(*this, hdc, pos, *ri);
	} else {
		auto layer = make_image_layer(*this, pos, *ri);
		layer.image_orientation = css().get_image_orientation();
		get_document()->container()->draw_image(hdc, layer, image_source(), m_baseurl);
	}
}

void litehtml::el_image::compute_styles(bool recursive)
{
	html_tag::compute_styles(recursive);
	if (m_generated_image) return;

	const auto& source = image_source();
	auto* container = get_document()->container();
	const auto orientation = css().get_image_orientation();
	m_alt_text.reset();
	const char* alt = get_attr("alt", "");
	if (*alt && !css().skips_contents())
	{
		if (source.empty() || !container->is_image_available(source.c_str(), m_baseurl.c_str(), orientation))
			m_alt_text = container->shape_image_alt(alt, *this);
	}
	else if(!source.empty())
	{
		if(!css().get_height().is_predefined() && !css().get_width().is_predefined())
		{
			container->load_image(source.c_str(), m_baseurl.c_str(), true, orientation);
		} else
		{
			container->load_image(source.c_str(), m_baseurl.c_str(), false, orientation);
		}
	}
}

litehtml::string litehtml::el_image::dump_get_name()
{
	return "img src=\"" + image_source() + "\"";
}

std::shared_ptr<litehtml::render_item> litehtml::el_image::create_render_item(const std::shared_ptr<render_item>& parent_ri)
{
	auto ret = std::make_shared<render_item_image>(shared_from_this());
	ret->parent(parent_ri);
	return ret;
}
