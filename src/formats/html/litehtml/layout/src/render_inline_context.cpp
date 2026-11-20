#include "render_inline_context.h"
#include "document.h"
#include "iterators.h"
#include "types.h"

litehtml::rendered_width litehtml::render_item_inline_context::_render_content(
	pixel_t /*x*/, pixel_t /*y*/, bool /*second_pass*/, const containing_block_context& self_size,
	formatting_context* fmt_ctx)
{
	prepare_text_fragments(self_size);
	const bool protect_last = protects_last_hyphenation(self_size);
	std::optional<formatting_context> initial_context;
	if (protect_last) initial_context.emplace(*fmt_ctx);
	layout_inlines(self_size, fmt_ctx);
	if (initial_context) avoid_last_hyphenation(self_size, fmt_ctx, *initial_context);
	apply_text_wrap(self_size, fmt_ctx, protect_last);
	m_suppressed_hyphen_lines.clear();
	merge_text_fragments();

    if (!m_line_boxes.empty())
    {
        if (collapse_top_margin())
        {
            pixel_t old_top = m_margins.top;
            m_margins.top = std::max(m_line_boxes.front()->top_margin(), m_margins.top);
            if (m_margins.top != old_top)
            {
                fmt_ctx->update_floats(m_margins.top - old_top, shared_from_this());
            }
        }
        if (collapse_bottom_margin())
        {
            m_margins.bottom = std::max(m_line_boxes.back()->bottom_margin(), m_margins.bottom);
            m_pos.height = m_line_boxes.back()->bottom() - m_line_boxes.back()->bottom_margin();
        }
        else
        {
            m_pos.height = m_line_boxes.back()->bottom();
        }
    }

	return m_rendered_width;
}

void litehtml::render_item_inline_context::layout_inlines(const containing_block_context& self_size, formatting_context* fmt_ctx,
	const std::unordered_set<const render_item*>* forced_cuts)
{
    m_line_boxes.clear();
	m_rendered_width.reset();

    go_inside_inline go_inside_inlines_selector;
    inline_selector select_inlines;
    elements_iterator inlines_iter(true, &go_inside_inlines_selector, &select_inlines);

    inlines_iter.process(shared_from_this(), [&](const std::shared_ptr<render_item>& el, iterator_item_type item_type)
        {
			if (src_el()->css().get_list_style_position() == list_style_position_outside &&
				el->src_el()->tag() == marker_tag()) return;
			switch (item_type)
			{
				case iterator_item_type_child:
					if (el->src_el()->is_space() && !el->src_el()->is_break() &&
						el->css().get_white_space_collapse() == white_space_discard)
					{
						el->skip(true);
						break;
					}
					place_inline(std::make_unique<line_box_item>(el), self_size, fmt_ctx, forced_cuts && forced_cuts->count(el.get()));
					break;

				case iterator_item_type_start_parent:
					{
						el->clear_inline_boxes();
						place_inline(std::make_unique<lbi_start>(el), self_size, fmt_ctx);
					}
					break;

				case iterator_item_type_end_parent:
				{
					place_inline(std::make_unique<lbi_end>(el), self_size, fmt_ctx);
				}
					break;
			}
        });

    finish_last_box(true, self_size);
}

void litehtml::render_item_inline_context::fix_line_width(element_float flt,
														  const containing_block_context &self_size,
														  formatting_context* fmt_ctx)
{
    if(!m_line_boxes.empty())
    {
		auto el_front = m_line_boxes.back()->get_first_text_part();

        const auto clear = el_front ? el_front->get_used_clear() : clear_none;
        const bool was_cleared = clear == clear_both ||
            (flt == float_left && clear == clear_left) || (flt == float_right && clear == clear_right);

        if(!was_cleared)
        {
			std::list<std::unique_ptr<line_box_item> > items = std::move(m_line_boxes.back()->items());
            m_line_boxes.pop_back();

            for(auto& item : items)
            {
                place_inline(std::move(item), self_size, fmt_ctx);
            }
        } else
        {
            pixel_t line_top = 0;
            line_top = m_line_boxes.back()->top();

            pixel_t line_left	= 0;
            pixel_t line_right	= self_size.render_width;
            fmt_ctx->get_line_left_right(line_top, self_size.render_width, line_left, line_right);

            auto items = m_line_boxes.back()->new_width(line_left, line_right);
            for(auto& item : items)
            {
                place_inline(std::move(item), self_size, fmt_ctx);
            }
        }
    }
}

std::list<std::unique_ptr<litehtml::line_box_item> > litehtml::render_item_inline_context::finish_last_box(bool end_of_render, const containing_block_context &self_size)
{
	std::list<std::unique_ptr<line_box_item> > ret;

    if(!m_line_boxes.empty())
    {
		ret = m_line_boxes.back()->finish(end_of_render, self_size);

        if(m_line_boxes.back()->is_empty() && end_of_render)
        {
			// remove the last empty line
            m_line_boxes.pop_back();
        } else
		{
			m_rendered_width.merge(m_line_boxes.back()->get_rendered_width());
		}
	}
	return ret;
}

litehtml::pixel_t litehtml::render_item_inline_context::new_box(const std::unique_ptr<line_box_item>& el,
																const containing_block_context&		  self_size,
																formatting_context*					  fmt_ctx)
{
	const bool forced = !m_line_boxes.empty() && m_line_boxes.back()->ends_with_forced_break();
	const bool paragraph_start = forced || (m_line_boxes.empty() && first_formatted_line());
	const size_t preceding_hyphens = m_line_boxes.empty() ? 0 : m_line_boxes.back()->consecutive_hyphenated_lines();
	const auto [text_indent, intrinsic_indent] = line_indentation(self_size, m_line_boxes.empty(), forced);

	auto items = finish_last_box(false, self_size);
	pixel_t line_top = 0;
	if(!m_line_boxes.empty())
	{
		line_top = m_line_boxes.back()->bottom();
	}
	line_top = fmt_ctx->get_cleared_top(el->get_el(), line_top);

	formatting_context::el_position el_pos;
	el_pos.container_width	= self_size.render_width;
	el_pos.el_pos			= el->pos();
	if (el->get_el()->css().get_display() == display_inline_grid)
	{
		el_pos.el_pos.width = el->width();
		el_pos.el_pos.height = el->height();
	}
	el_pos.el_pos.y			= line_top;
	el_pos.el_pos.x			= 0;
	el_pos.el_pos.width	   += text_indent;
	auto new_pos			= fmt_ctx->place_to_left(el_pos);

	const bool suppress_hyphenation = std::find(m_suppressed_hyphen_lines.begin(), m_suppressed_hyphen_lines.end(),
		m_line_boxes.size()) != m_suppressed_hyphen_lines.end();
	m_line_boxes.emplace_back(std::make_unique<line_box>(new_pos.top, new_pos.left,
		new_pos.left + new_pos.width, css(), text_indent, intrinsic_indent,
		!self_size.measuring_inline_size && !self_size.grid_intrinsic_width, paragraph_start, preceding_hyphens, suppress_hyphenation));

	// Add items returned by finish_last_box function into the new line
	for(auto& it : items)
	{
		m_line_boxes.back()->add_item(std::move(it));
	}

	return new_pos.top;
}

void litehtml::render_item_inline_context::place_inline(std::unique_ptr<line_box_item>	item,
														const containing_block_context& self_size,
														formatting_context*				fmt_ctx, bool force_wrap)
{
    if(item->get_el()->src_el()->css().get_display() == display_none) return;

    if(item->get_el()->src_el()->is_float())
    {
        pixel_t line_top = 0;
        if(!m_line_boxes.empty())
        {
            line_top = m_line_boxes.back()->top();
        }
		auto rw = place_float(item->get_el(), line_top, self_size, fmt_ctx);
		m_rendered_width.merge(rw);
		return;
	}

	if(item->get_type() == line_box_item::type_text_part)
	{
		if (item->get_el()->css().get_display() == display_inline_grid)
		{
			auto columns = self_size.new_width(self_size.render_width);
			if (self_size.grid_intrinsic_width)
			{
				columns.width.type = containing_block_context::cbc_value_type_auto;
				columns.size_mode = containing_block_context::size_mode_content;
			}
			item->set_rendered_widths(item->get_el()->render(0, 0, columns, fmt_ctx));
		} else if(item->get_el()->src_el()->is_inline_box())
		{
			pixel_t min_rendered_width = item->get_el()->render(0, 0, self_size, fmt_ctx).natural_width;
			if(min_rendered_width < item->get_el()->width() && item->get_el()->src_el()->css().get_width().is_predefined())
			{
				item->get_el()->render(0, 0, self_size.new_width(min_rendered_width), fmt_ctx);
			}
			item->set_rendered_min_width(min_rendered_width);
		} else if(item->get_el()->src_el()->css().get_display() == display_inline_text)
		{
			litehtml::size sz;
			item->get_el()->src_el()->get_content_size(sz, self_size.render_width);
			item->get_el()->pos() = sz;
			item->get_el()->set_text_justification({});
			item->set_rendered_min_width(sz.width);
		}
	}

	if (m_line_boxes.empty())
	{
		new_box(item, self_size, fmt_ctx);
	}
	else if (auto tail = m_line_boxes.back()->wrap_before(*item, force_wrap))
	{
		new_box(tail->empty() ? item : tail->front(), self_size, fmt_ctx);
		for (auto& moved : *tail) m_line_boxes.back()->add_item(std::move(moved));
	}

	if(!item->get_el()->src_el()->is_inline())
	{
		if(m_line_boxes.size() == 1)
		{
			if(collapse_top_margin())
			{
				pixel_t shift = item->get_el()->margin_top();
				if(shift >= 0)
				{
					m_line_boxes.back()->y_shift(-shift);
				}
			}
		} else
		{
			pixel_t shift		= 0;
			pixel_t prev_margin = m_line_boxes[m_line_boxes.size() - 2]->bottom_margin();

			if(prev_margin > item->get_el()->margin_top())
			{
				shift = item->get_el()->margin_top();
			} else
			{
				shift = prev_margin;
			}
			if(shift >= 0)
			{
				m_line_boxes.back()->y_shift(-shift);
			}
		}
	}

	m_line_boxes.back()->add_item(std::move(item));
}

void litehtml::render_item_inline_context::apply_vertical_align()
{
	if(!m_line_boxes.empty())
	{
		pixel_t add			   = 0;
		pixel_t content_height = m_line_boxes.back()->bottom();

		if(m_pos.height > content_height)
		{
			switch(src_el()->css().get_vertical_align())
			{
                case va_middle:
                    add = (m_pos.height - content_height) / 2;
                    break;
                case va_bottom:
                    add = m_pos.height - content_height;
                    break;
                default:
                    add = 0;
                    break;
            }
        }

        if(add != 0)
        {
            for(auto & box : m_line_boxes)
            {
                box->y_shift(add);
            }
        }
    }
}

litehtml::pixel_t litehtml::render_item_inline_context::baseline_content_height() const
{
	return m_line_boxes.empty() ? 0 : m_line_boxes.back()->bottom() - m_line_boxes.front()->top();
}

void litehtml::render_item_inline_context::align_content_to_end()
{
	if (m_line_boxes.empty()) return;
	const auto extra = m_pos.height - m_line_boxes.back()->bottom();
	if (extra <= 0) return;
	// A wrapped inline can appear in several line boxes. Move its
	// accumulated fragments once, while moving every line's metrics.
	std::unordered_set<render_item*> shifted;
	for (auto& line : m_line_boxes)
		line->shift_for_content_alignment(extra, shifted);
}

std::optional<litehtml::pixel_t> litehtml::render_item_inline_context::find_layout_baseline(bool last) const
{
	if (m_line_boxes.empty()) return std::nullopt;
	const auto& line = last ? m_line_boxes.back() : m_line_boxes.front();
	return content_offset_top() + line->bottom() - line->baseline();
}

litehtml::pixel_t litehtml::render_item_inline_context::get_first_baseline()
{
	if (css().has_layout_containment()) return height() - margin_bottom();
	pixel_t bl;
	if(!m_line_boxes.empty())
	{
		const auto &line = m_line_boxes.front();
		bl = line->bottom() - line->baseline() + content_offset_top();
	} else
	{
		bl = height() - margin_bottom();
	}
	return bl;
}

litehtml::pixel_t litehtml::render_item_inline_context::get_last_baseline()
{
	if (css().has_layout_containment()) return height() - margin_bottom();
	pixel_t bl;
	if(!m_line_boxes.empty())
	{
		const auto &line = m_line_boxes.back();
		bl = line->bottom() - line->baseline() + content_offset_top();
	} else
	{
		bl = height();
	}
	return bl;
}
