#include "render_block_context.h"
#include "document.h"
#include "types.h"

#include <algorithm>

litehtml::rendered_width litehtml::render_item_block_context::_render_content(pixel_t /*x*/, pixel_t /*y*/,
																			  bool second_pass,
																			  const containing_block_context& self_size,
																			  formatting_context*			  fmt_ctx)
{
	std::shared_ptr<render_item> last_margin_el;
	std::shared_ptr<render_item> fieldset_legend;
	if (src_el()->tag() == _fieldset_) {
		for (const auto& child : m_children) {
			const auto position = child->src_el()->css().get_position();
			if (child->src_el()->tag() == _legend_ && child->src_el()->css().get_float() == float_none &&
				position != element_position_absolute && position != element_position_fixed) {
				fieldset_legend = child;
				break;
			}
		}
	}

	rendered_width ret_width;
	pixel_t		   child_top   = 0;
	pixel_t		   last_margin = 0;
	bool		   is_first	   = true;
	for(const auto& el : m_children)
	{
		if (el->src_el()->tag() == marker_tag()) continue;
		// we don't need to process absolute and fixed positioned element on the second pass
		if(second_pass)
		{
			auto el_position = el->src_el()->css().get_position();
			if((el_position == element_position_absolute || el_position == element_position_fixed))
				continue;
		}

		if(el->src_el()->css().get_float() != float_none)
		{
			auto rw = place_float(el, child_top, self_size, fmt_ctx);
			ret_width.merge(rw);
		} else if(el->src_el()->css().get_display() != display_none)
		{
			const bool is_fieldset_legend = el == fieldset_legend;
			if(el->src_el()->css().get_position() == element_position_absolute ||
			   el->src_el()->css().get_position() == element_position_fixed)
			{
				pixel_t min_rendered_width = el->render(0, child_top, self_size, fmt_ctx).natural_width;
				if(min_rendered_width < el->width() && el->src_el()->css().get_width().is_predefined())
				{
					el->render(0, child_top, self_size.new_width(min_rendered_width), fmt_ctx);
				}
			} else
			{
				child_top			= fmt_ctx->get_cleared_top(el, child_top);
				pixel_t child_x		= 0;
				pixel_t child_width = self_size.render_width;
				pixel_t top_margin	= m_margins.top;

				el->calc_outlines(self_size.width);

				// Collapse top margin
				if(is_first && collapse_top_margin())
				{
					if(el->get_margins().top > 0)
					{
						child_top -= el->get_margins().top;
						if(el->get_margins().top > get_margins().top)
						{
							top_margin = el->get_margins().top;
						}
					}
				} else
				{
					if(el->get_margins().top > 0)
					{
						if(last_margin > el->get_margins().top)
						{
							child_top -= el->get_margins().top;
						} else
						{
							child_top -= last_margin;
						}
					}
				}

				rendered_width rw;
				const auto& legend_width = el->src_el()->css().get_width();
				const bool auto_legend_width = is_fieldset_legend && legend_width.is_predefined() &&
					legend_width.intrinsic_size() == css_intrinsic_size::none;
				if (auto_legend_width) {
					const pixel_t available_legend_width = child_width + m_borders.left + m_borders.right;
					rw = el->render(child_x, child_top,
						self_size.new_width(available_legend_width,
							self_size.size_mode | containing_block_context::size_mode_content), nullptr);
					const pixel_t fit_width = std::clamp(rw.natural_width, pixel_t(0), available_legend_width);
					rw = el->render(child_x, child_top, self_size.new_width(fit_width), fmt_ctx);
				} else {
					rw = el->render(child_x, child_top, self_size.new_width(child_width), fmt_ctx);
				}
				// Render table with "width: auto" into returned width
				if(el->css().get_display() == display_table && rw.natural_width < child_width &&
				   el->css().get_width().is_predefined())
				{
					el->render(child_x, child_top, self_size.new_width(rw.natural_width), fmt_ctx);
				}

				// Check if we need to move block to the next line
				if(el->src_el()->is_replaced() || el->src_el()->is_block_formatting_context() ||
				   el->src_el()->css().get_display() == display_table)
				{
					formatting_context::el_position el_pos;
					el_pos.el_pos			= el->pos();
					el_pos.el_pos		   += el->get_margins();
					el_pos.el_pos		   += el->get_paddings();
					el_pos.el_pos		   += el->get_borders();
					el_pos.el_pos.width		= rw.min_width;
					el_pos.container_width	= self_size.render_width;
					el_pos.el_margins		= el->get_margins();
					auto new_pos			= fmt_ctx->place_to_left(el_pos);
					if(new_pos.found)
					{
						child_top	= new_pos.top;
						el->pos().x = new_pos.left + el->content_offset_left();
						el->pos().y = child_top + el->content_offset_top();
						//  Rollback top margin collapse if the block was moved to the new line
						if(is_first && new_pos.new_line && collapse_top_margin())
						{
							top_margin = m_margins.top;
						}
					}
					if(new_pos.width != self_size.render_width && el->css().get_width().is_predefined())
					{
						rw = el->render(new_pos.left, new_pos.top, self_size.new_width(new_pos.width), fmt_ctx);
					}
					rw.natural_width = self_size.render_width - new_pos.width + rw.natural_width;
				}

				if (is_fieldset_legend) {
					// Center the legend border box over the fieldset's block-start border.
					const pixel_t legend_border_height = el->height() - el->margin_top() - el->margin_bottom();
					const pixel_t target_border_top = -m_padding.top - m_borders.top * 0.5f - legend_border_height * 0.5f;
					const pixel_t current_border_top = child_top + el->margin_top();
					el->pos().y += target_border_top - current_border_top;
				}

				const pixel_t auto_margin_width = child_width +
					(is_fieldset_legend ? m_borders.left + m_borders.right : 0);
				pixel_t auto_margin = el->calc_auto_margins(auto_margin_width);
				if(auto_margin != 0)
				{
					el->pos().x += auto_margin;
				}
				// The legend is positioned from the border edge; normal children use the padded content edge.
				if (is_fieldset_legend) el->pos().x -= m_borders.left;
				ret_width.merge(rw);
				m_margins.top	= top_margin;
				child_top	   += el->height();
				// The top border allocation already accounts for its overlap with the legend margin box.
				if (is_fieldset_legend) child_top -= std::min(m_borders.top, std::max<pixel_t>(0, el->height()));
				last_margin		= el->get_margins().bottom;
				last_margin_el	= el;
				is_first		= false;

				if(el->src_el()->css().get_position() == element_position_relative)
				{
					el->apply_relative_shift(self_size);
				}
			}
		}
	}

	layout_content_height = child_top;

	if(self_size.height.type != containing_block_context::cbc_value_type_auto && self_size.height > 0)
	{
		m_pos.height = self_size.height;
	} else
	{
		m_pos.height = child_top;
		if(collapse_bottom_margin())
		{
			m_pos.height -= last_margin;
			if(m_margins.bottom < last_margin)
			{
				m_margins.bottom = last_margin;
			}
			if(last_margin_el)
			{
				last_margin_el->get_margins().bottom = 0;
			}
		}
	}

	return ret_width;
}

litehtml::pixel_t litehtml::render_item_block_context::baseline_content_height() const
{
	return layout_content_height;
}

void litehtml::render_item_block_context::align_content_to_end()
{
	const auto extra = m_pos.height - layout_content_height;
	if (extra <= 0) return;
	for (const auto& child : m_children)
		if (child->src_el()->tag() != marker_tag() && child->src_el()->in_normal_flow()) child->y_shift(extra);
}

std::optional<litehtml::pixel_t> litehtml::render_item_block_context::find_layout_baseline(bool last) const
{
	const auto baseline_of = [&](auto begin, auto end) -> std::optional<pixel_t> {
		for (auto it = begin; it != end; ++it)
		{
			const auto& child = *it;
			if (child->src_el()->tag() == marker_tag() || !child->src_el()->in_normal_flow() || child->css().get_float() != float_none) continue;
			if (const auto baseline = child->layout_baseline(last))
				return content_offset_top() + child->top() + *baseline;
		}
		return std::nullopt;
	};
	if (last) return baseline_of(m_children.rbegin(), m_children.rend());
	return baseline_of(m_children.begin(), m_children.end());
}

litehtml::pixel_t litehtml::render_item_block_context::get_first_baseline()
{
	if(m_children.empty() || css().has_layout_containment())
	{
		return height() - margin_bottom();
	}
	for (const auto& item : m_children)
		if (item->src_el()->tag() != marker_tag())
			return content_offset_top() + item->top() + item->get_first_baseline();
	return height() - margin_bottom();
}

litehtml::pixel_t litehtml::render_item_block_context::get_last_baseline()
{
	if(m_children.empty() || css().has_layout_containment())
	{
		return height() - margin_bottom();
	}
	for (auto item = m_children.rbegin(); item != m_children.rend(); ++item)
		if ((*item)->src_el()->tag() != marker_tag())
			return content_offset_top() + (*item)->top() + (*item)->get_last_baseline();
	return height() - margin_bottom();
}
