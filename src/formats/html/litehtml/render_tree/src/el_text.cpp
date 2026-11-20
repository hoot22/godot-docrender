#include "html.h"
#include "el_text.h"
#include "el_space.h"
#include "render_item.h"
#include "document_container.h"
#include "formats/html/layout/html_line_break.h"

litehtml::el_text::el_text(const char* text, const document::ptr& doc, bool whole_text_node)
	: element(doc), m_whole_text_node(whole_text_node)
{
	if(text)
	{
		m_text = text;
	}
	m_use_transformed	= false;
	m_draw_spaces		= true;
	css_w().set_display(display_inline_text);
}

void litehtml::el_text::get_content_size( size& sz, pixel_t /*max_width*/ )
{
	sz = m_size;
}

void litehtml::el_text::get_text( string& text ) const
{
	text += m_text;
}

void litehtml::el_text::compute_styles(bool /*recursive*/)
{
	element::ptr el_parent = parent();
	if (el_parent)
	{
		css_w().line_height_w() = el_parent->css().line_height();
		css_w().set_font(el_parent->css().get_font());
		css_w().set_font_metrics(el_parent->css().get_font_metrics());
		css_w().set_visibility(el_parent->css().get_visibility());
		css_w().copy_white_space(el_parent->css());
		css_w().set_text_transform(el_parent->css().get_text_transform());
	}
	css_w().set_display(display_inline_text);
	css_w().set_float(float_none);

	const auto transform = m_css.get_text_transform();
	m_use_transformed = false;
	if (is_break() || is_explicit_wrap_point(*this) || is_soft_hyphen(*this))
	{
		m_transformed_text.clear();
		m_use_transformed = true;
	}
	else if (is_white_space())
	{
		m_transformed_text = " ";
		m_use_transformed = true;
	}
	else if (m_text == "\r" || (m_css.get_white_space_collapse() == white_space_preserve_spaces &&
		(m_text == "\n" || m_text == "\t")))
	{
		m_transformed_text = " ";
		if (transform != text_transform_none)
			get_document()->container()->transform_text(m_transformed_text, transform);
		m_use_transformed = true;
	}
	else if (!is_preserved_tab() && transform != text_transform_none &&
		(transform != text_transform_math_auto || m_whole_text_node))
	{
		m_transformed_text = m_text;
		get_document()->container()->transform_text(m_transformed_text, transform);
		m_use_transformed = true;
	}

	element::ptr p = parent();
	while(p && p->css().get_display() == display_inline)
	{
		if(p->css().get_position() == element_position_relative)
		{
			css_w().set_offsets(p->css().get_offsets());
			css_w().set_position(element_position_relative);
			break;
		}
		p = p->parent();
	}
	if(p)
	{
		css_w().set_position(element_position_static);
	}

	font_metrics fm;
	uint_ptr font = 0;
	if (el_parent)
	{
		font = el_parent->css().get_font();
		fm = el_parent->css().get_font_metrics();
	}
	if(is_break() || !font)
	{
		m_size.height	= 0;
		m_size.width	= 0;
	} else
	{
		m_size.width	= is_preserved_tab() ? 0 : get_document()->container()->measure_text(get_rendered_text().c_str(), font, fm);
		m_size.height	= fm.height;
		css_w().set_font_metrics(fm);
	}
	m_draw_spaces = fm.draw_spaces;
}

void litehtml::el_text::draw(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip, const std::shared_ptr<render_item> &ri)
{
	if (is_white_space() && !m_draw_spaces)
	{
		const auto* space = dynamic_cast<const el_space*>(this);
		if (!space || !space->has_autospace_width()) return;
	}
	if (is_preserved_tab() && ri->pos().width <= 0) return;

	position pos = ri->pos();
	pos.x	+= x;
	pos.y	+= y;
	pos.y = std::round(pos.y);

	if(pos.does_intersect(clip))
	{
		element::ptr el_parent = parent();
		if (el_parent)
		{
			document::ptr doc = get_document();

			uint_ptr font = el_parent->css().get_font();
			if(font)
			{
				web_color color = el_parent->css().get_color();
				const bool discretionary = is_soft_hyphen(*this);
				if (discretionary && pos.width <= 0) return;
				const auto& text = discretionary ? el_parent->css().get_hyphenate_character() : get_rendered_text();
				doc->container()->draw_styled_text(hdc, text.c_str(), font,
											color, pos, *el_parent, ri->get_text_justification(), ri->get_punctuation_trim());
			}
		}
	}
}

litehtml::string litehtml::el_text::dump_get_name()
{
	return "text: \"" + get_escaped_string(m_text) + "\"";
}

std::vector<std::tuple<litehtml::string, litehtml::string>> litehtml::el_text::dump_get_attrs()
{
	return {};
}
