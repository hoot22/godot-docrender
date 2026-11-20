#include "formats/html/text/html_image_alt_text.h"
#include "formats/html/layout/grid/litehtml_render_grid.h"
#include "render_image.h"
#include "document.h"
#include "el_image.h"
#include "types.h"

litehtml::rendered_width litehtml::render_item_image::_render(pixel_t x, pixel_t y,
															  const containing_block_context& containing_block_size,
															  formatting_context* /*fmt_ctx*/, bool /*second_pass*/)
{
    pixel_t parent_width = containing_block_size.width;
	containing_block_context self_size = calculate_containing_block_context(containing_block_size);
	const bool contain_width = css().has_inline_size_containment();
	const bool contain_height = css().has_block_size_containment();
	const auto use_contained_size = [&](litehtml::size natural) {
		if (contain_width) natural.width = css().get_contain_intrinsic_width().value_or(0);
		if (contain_height) natural.height = css().get_contain_intrinsic_height().value_or(0);
		return natural;
	};

    m_pos.move_to(x, y);

	const auto* html_image = dynamic_cast<const el_image*>(src_el().get());
	const auto* tag = dynamic_cast<const html_tag*>(src_el().get());
	const auto* replacement = tag ? tag->content_image() : nullptr;
	if (auto* alt = html_image && !replacement ? html_image->get_alt_text() : nullptr)
	{
		pixel_t measure_width = alt->get_width();
		if (!(containing_block_size.size_mode & (containing_block_context::size_mode_intrinsic_inline_probe |
			containing_block_context::size_mode_grid_measure_width)))
		{
			const auto available = std::max<pixel_t>(0, parent_width - content_offset_width());
			measure_width = std::min(measure_width, available);
		}
		const auto width = size_grid_image(*this, self_size, use_contained_size({measure_width, 0}), false).width;
		const auto height = alt->layout(width).size() * alt->get_line_height();
		m_pos = size_grid_image(*this, self_size, use_contained_size({measure_width, height}), false);
		m_pos.move_to(x + content_offset_left(), y + content_offset_top());
		src_el()->css_w().line_height_w().computed_value = this->height();
		return {this->width(), this->width()};
	}

    document::ptr doc = src_el()->get_document();

    litehtml::size sz;
	if (!css().skips_contents()) {
		if (replacement) sz = replacement->natural_size(*doc->container());
		else src_el()->get_content_size(sz, containing_block_size.width);
	}
	sz = use_contained_size(sz);

	if (replacement || contain_width || has_preferred_aspect_ratio() || is_grid_item() ||
		(containing_block_size.size_mode & (containing_block_context::size_mode_exact_width |
			containing_block_context::size_mode_exact_height)) ||
		(containing_block_size.size_mode & containing_block_context::size_mode_intrinsic_inline_probe) ||
		containing_block_size.intrinsic_inline.preferred || containing_block_size.intrinsic_inline.minimum ||
		containing_block_size.intrinsic_inline.maximum || css().get_width().intrinsic_size() != css_intrinsic_size::none)
	{
		const auto used = size_grid_image(*this, self_size, sz, !contain_width);
		m_pos.width = used.width;
		m_pos.height = used.height;
		m_pos.move_to(x + content_offset_left(), y + content_offset_top());
		src_el()->css_w().line_height_w().computed_value = height();
		return {width(), width()};
	}


    m_pos.width		= sz.width;
    m_pos.height	= sz.height;

    src_el()->css_w().line_height_w().computed_value = height();

    if(src_el()->css().get_height().is_predefined() && src_el()->css().get_width().is_predefined())
    {
        m_pos.height	= sz.height;
        m_pos.width		= sz.width;

        // check for max-width
        if(!src_el()->css().get_max_width().is_predefined())
        {
            pixel_t max_width = doc->to_pixels(css().get_max_width(), css().get_font_metrics(), parent_width);
            if(m_pos.width > max_width)
            {
                m_pos.width = max_width;
            }
            if(sz.width != 0)
            {
                m_pos.height = m_pos.width * sz.height / sz.width;
            } else
            {
                m_pos.height = sz.height;
            }
        }

        // check for max-height
        if(!src_el()->css().get_max_height().is_predefined())
        {
            pixel_t max_height = calc_max_height(sz.height, containing_block_size.height);
            if(m_pos.height > max_height)
            {
                m_pos.height = max_height;
            }
            if(sz.height != 0)
            {
                m_pos.width = m_pos.height * sz.width / sz.height;
            } else
            {
                m_pos.width = sz.width;
            }
        }
    } else if(!src_el()->css().get_height().is_predefined() && src_el()->css().get_width().is_predefined())
    {
		if(self_size.height.type != containing_block_context::cbc_value_type_auto && self_size.height > 0)
		{
			m_pos.height = self_size.height;
		}

        // check for max-height
        if(!src_el()->css().get_max_height().is_predefined())
        {
            pixel_t max_height = calc_max_height(sz.height, containing_block_size.height);
            if(m_pos.height > max_height)
            {
                m_pos.height = max_height;
            }
        }

        if(sz.height != 0)
        {
            m_pos.width = m_pos.height * sz.width / sz.height;
        } else
        {
            m_pos.width = sz.width;
        }
    } else if(src_el()->css().get_height().is_predefined() && !src_el()->css().get_width().is_predefined())
    {
        m_pos.width = src_el()->css().get_width().calc_percent(parent_width);

        // check for max-width
        if(!src_el()->css().get_max_width().is_predefined())
        {
            pixel_t max_width = doc->to_pixels(css().get_max_width(), css().get_font_metrics(), parent_width);
            if(m_pos.width > max_width)
            {
                m_pos.width = max_width;
            }
        }

        if(sz.width != 0)
        {
            m_pos.height = m_pos.width * sz.height / sz.width;
        } else
        {
            m_pos.height = sz.height;
        }
    } else
    {
        m_pos.width		= src_el()->css().get_width().calc_percent(parent_width);
        m_pos.height	= 0;
		if(self_size.height.type != containing_block_context::cbc_value_type_auto && self_size.height > 0)
		{
			m_pos.height = self_size.height;
		}

        // check for max-height
        if(!src_el()->css().get_max_height().is_predefined())
        {
            pixel_t max_height = calc_max_height(sz.height, containing_block_size.height);
            if(m_pos.height > max_height)
            {
                m_pos.height = max_height;
            }
        }

        // check for max-height
        if(!src_el()->css().get_max_width().is_predefined())
        {
            pixel_t max_width = doc->to_pixels(css().get_max_width(), css().get_font_metrics(), parent_width);
            if(m_pos.width > max_width)
            {
                m_pos.width = max_width;
            }
        }
    }

    m_pos.x	+= content_offset_left();
    m_pos.y += content_offset_top();

	rendered_width ret;
	ret.natural_width = ret.min_width = m_pos.width + content_offset_left() + content_offset_right();

	return ret;
}

litehtml::pixel_t litehtml::render_item_image::calc_max_height(pixel_t image_height, pixel_t containing_block_height)
{
    document::ptr doc = src_el()->get_document();
    return doc->to_pixels(css().get_max_height(), css().get_font_metrics(),
						  containing_block_height == 0 ? image_height : containing_block_height);
}
