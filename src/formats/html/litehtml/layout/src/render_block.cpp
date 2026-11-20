#include "render_block.h"
#include "formatting_context.h"
#include "render_inline_context.h"
#include "render_block_context.h"
#include "document.h"
#include "document_container.h"
#include "html_tag.h"
#include "types.h"

litehtml::rendered_width litehtml::render_item_block::place_float(const std::shared_ptr<render_item>& el, pixel_t top,
																  const containing_block_context& self_size,
																  formatting_context*			  fmt_ctx)
{
	pixel_t line_top		   = fmt_ctx->get_cleared_top(el, top);
	pixel_t line_left = 0;

	auto nv = el->render(line_left, line_top, self_size.new_width(self_size.render_width), fmt_ctx);
	if(nv.natural_width < el->width() && el->src_el()->css().get_width().is_predefined())
	{
		el->render(line_left, line_top, self_size.new_width(nv.natural_width), fmt_ctx);
	}

	formatting_context::new_position new_pos;
	formatting_context::el_position	 el_pos;
	el_pos.el_pos			= el->pos();
	el_pos.el_pos		   += el->get_margins();
	el_pos.el_pos		   += el->get_paddings();
	el_pos.el_pos		   += el->get_borders();
	el_pos.container_width	= self_size.render_width;
	const auto side = el->get_used_float();
	if(side == float_left)
	{
		new_pos = fmt_ctx->place_to_left(el_pos);
	} else if(side == float_right)
	{
		el->pos().x		= self_size.render_width - el_pos.el_pos.width + el->content_offset_left();
		el_pos.el_pos.x = self_size.render_width - el_pos.el_pos.width;
		new_pos			= fmt_ctx->place_to_right(el_pos);
	}
	nv.min_width = nv.natural_width;
	if(new_pos.found)
	{
		el->pos().x = new_pos.left + el->content_offset_left();
		el->pos().y = new_pos.top + el->content_offset_top();
	}
	nv.natural_width = self_size.render_width - new_pos.width + nv.natural_width;
	fmt_ctx->add_float(el, nv.min_width, self_size.context_idx);
	fix_line_width(side, self_size, fmt_ctx);

	return nv;
}

std::shared_ptr<litehtml::render_item> litehtml::render_item_block::init()
{
    std::shared_ptr<render_item> ret;

    // Separate an outside marker before grouping the normal flow into anonymous boxes.
    std::shared_ptr<render_item> outside_marker;
    if (css().get_display() == display_list_item && css().get_list_style_position() == list_style_position_outside &&
        !m_children.empty() && m_children.front()->src_el()->tag() == marker_tag())
    {
        outside_marker = m_children.front();
        m_children.erase(m_children.begin());
    }
    // Split inline blocks with box blocks inside
    auto iter = m_children.begin();
    while (iter != m_children.end())
    {
        const auto& el = *iter;
        if(el->src_el()->css().get_display() == display_inline && !el->children().empty())
        {
            auto split_el = el->split_inlines();
            if(std::get<0>(split_el))
            {
                iter = m_children.erase(iter);
                iter = m_children.insert(iter, std::get<2>(split_el));
                iter = m_children.insert(iter, std::get<1>(split_el));
                iter = m_children.insert(iter, std::get<0>(split_el));

                std::get<0>(split_el)->parent(shared_from_this());
                std::get<1>(split_el)->parent(shared_from_this());
                std::get<2>(split_el)->parent(shared_from_this());
                continue;
            }
        }
        ++iter;
    }

    bool has_block_level = false;
	bool has_inlines = false;
    for (const auto& el : m_children)
    {
		if(!el->src_el()->is_float())
		{
			if (el->src_el()->is_block_box())
			{
				has_block_level = true;
			} else if (el->src_el()->is_inline())
			{
				has_inlines = true;
			}
		}
        if(has_block_level && has_inlines)
            break;
    }
    if(has_block_level)
    {
        ret = std::make_shared<render_item_block_context>(src_el());
        ret->parent(parent());

        auto doc = src_el()->get_document();
        decltype(m_children) new_children;
        decltype(m_children) inlines;
        bool not_ws_added = false;
        for (const auto& el : m_children)
        {
            if(el->src_el()->is_inline())
            {
                inlines.push_back(el);
                if(!el->src_el()->is_white_space())
                    not_ws_added = true;
            } else
            {
                if(not_ws_added)
                {
                    auto anon_el = std::make_shared<html_tag>(src_el());
                    auto anon_ri = std::make_shared<render_item_block>(anon_el);
                    for(const auto& inl : inlines)
                    {
                        anon_ri->add_child(inl);
                    }

                    not_ws_added = false;
                    new_children.push_back(anon_ri);
                    anon_ri->parent(ret);
                }
                new_children.push_back(el);
                el->parent(ret);
                inlines.clear();
            }
        }
        if(!inlines.empty() && not_ws_added)
        {
            auto anon_el = std::make_shared<html_tag>(src_el());
            auto anon_ri = std::make_shared<render_item_block>(anon_el);
            for(const auto& inl : inlines)
            {
                anon_ri->add_child(inl);
            }

            new_children.push_back(anon_ri);
            anon_ri->parent(ret);
        }
        ret->children() = new_children;
    }

    if(!ret)
    {
        ret = std::make_shared<render_item_inline_context>(src_el());
        ret->parent(parent());
        ret->children() = children();
        for (const auto &el: ret->children())
        {
            el->parent(ret);
        }
    }

    if (outside_marker)
    {
        outside_marker->parent(ret);
        ret->children().insert(ret->children().begin(), outside_marker);
    }

    ret->src_el()->add_render(ret);

    for(auto& el : ret->children())
    {
        el = el->init();
	}

	return ret;
}

litehtml::rendered_width litehtml::render_item_block::_render(pixel_t x, pixel_t y,
															  const containing_block_context& containing_block_size,
															  formatting_context* fmt_ctx, bool second_pass)
{
	containing_block_context self_size = calculate_containing_block_context(containing_block_size);
	self_size.measuring_inline_size = (containing_block_size.size_mode & containing_block_context::size_mode_content) != 0;
	using context = containing_block_context;
	const auto constrain = [](pixel_t value, context::typed_pixel lower, context::typed_pixel upper) {
		if (upper.type != context::cbc_value_type_none) value = std::min(value, upper.value);
		if (lower.type != context::cbc_value_type_none) value = std::max(value, lower.value);
		return std::max<pixel_t>(0, value);
	};
	std::optional<pixel_t> contained_width;
	if (css().has_inline_size_containment())
	{
		const bool auto_width = self_size.width.type == context::cbc_value_type_auto;
		contained_width = constrain(auto_width ? css().get_contain_intrinsic_width().value_or(0) : self_size.render_width.value,
			self_size.min_width, self_size.max_width);
		if (auto_width && (self_size.measuring_inline_size ||
			(!is_flex_item() && !is_grid_item() && (src_el()->is_inline_box() || src_el()->is_float() || !src_el()->in_normal_flow()))))
		{
			self_size.width = *contained_width + box_sizing_width();
			self_size.render_width = *contained_width;
		}
	}
	const bool contain_height = css().has_block_size_containment();
	if (contain_height && self_size.height.type == context::cbc_value_type_auto && !self_size.ratio_height)
	{
		self_size.contained_auto_block_size = constrain(css().get_contain_intrinsic_height().value_or(0),
			self_size.min_height, self_size.max_height);
		self_size.height = 0;
	}

    //*****************************************
    // Render content
    //*****************************************
	auto [ret_width, ret_min_width] = _render_content(x, y, second_pass, self_size, fmt_ctx);
	if (contained_width) ret_width = ret_min_width = *contained_width;
	//*****************************************

	if (src_el()->css().get_display() == display_list_item)
	{
		if(m_pos.height == 0)
		{
			m_pos.height = css().line_height().computed_value;
		}
	}

	bool requires_rerender = false;		// when true, the second pass for content rendering is required

	// Set block width
	if (!(containing_block_size.size_mode & containing_block_context::size_mode_content) ||
		((containing_block_size.size_mode & containing_block_context::size_mode_grid_measure_width) &&
		 self_size.width.type == containing_block_context::cbc_value_type_absolute))
	{
		if(self_size.width.type == containing_block_context::cbc_value_type_absolute)
		{
			ret_width = m_pos.width = self_size.render_width;
		} else
		{
			m_pos.width = self_size.render_width;
		}
	} else
	{
		m_pos.width = ret_width;
		if(self_size.width.type == containing_block_context::cbc_value_type_absolute && ret_width > self_size.width)
		{
			ret_width = self_size.width;
		}
	}

	// Fix width with max-width attribute
	if(self_size.max_width.type != containing_block_context::cbc_value_type_none)
	{
		if(m_pos.width > self_size.max_width)
		{
			m_pos.width = self_size.max_width;
			requires_rerender = true;
		}
	}

	// Fix width with min-width attribute
	if(self_size.min_width.type != containing_block_context::cbc_value_type_none)
	{
		if(m_pos.width < self_size.min_width)
		{
			m_pos.width = self_size.min_width;
			requires_rerender = true;
		}
	} else if(m_pos.width < 0)
	{
		m_pos.width = 0;
	}

	if (self_size.ratio_width && src_el()->in_normal_flow() && !is_scrollable_overflow(css().get_overflow_x()) && css().get_min_width().is_predefined() &&
		css().get_min_width().intrinsic_size() == css_intrinsic_size::none)
	{
		const auto minimum = self_size.max_width.type == containing_block_context::cbc_value_type_none ?
			ret_min_width : std::min(ret_min_width, self_size.max_width.value);
		if (minimum > m_pos.width)
		{
			m_pos.width = minimum;
			requires_rerender = true;
		}
	}

	// re-render content with new width if required
	if (requires_rerender && !second_pass && !is_root())
	{
		if(src_el()->is_block_formatting_context())
		{
			fmt_ctx->clear_floats(-1);
		} else
		{
			fmt_ctx->clear_floats(self_size.context_idx);
		}

		// new_width() takes the CSS width, while m_pos.width is already the content width.
		_render_content(x, y, true, self_size.new_width(m_pos.width + self_size.width - self_size.render_width), fmt_ctx);
	}

	// Set block height
	if (self_size.ratio_height)
	{
		m_pos.height = size_from_aspect_ratio(m_pos.width, css().get_aspect_ratio(), false);
		if (!contain_height && src_el()->in_normal_flow() && css().get_min_height().is_predefined() && !is_scrollable_overflow(css().get_overflow_y()))
		{
			m_pos.height = std::max(m_pos.height, baseline_content_height());
			if (src_el()->is_block_formatting_context())
				m_pos.height = std::max(m_pos.height, fmt_ctx->get_floats_height());
		}
	}
	else if (self_size.contained_auto_block_size)
	{
		m_pos.height = *self_size.contained_auto_block_size;
	}
	else if (self_size.height.type != containing_block_context::cbc_value_type_auto &&
	    (contain_height || !(containing_block_size.size_mode & containing_block_context::size_mode_content)))
	{
		// TODO: Something wrong here
		// Percentage height from undefined containing block height is usually <= 0
		if(self_size.height.type == containing_block_context::cbc_value_type_percentage)
		{
			if (self_size.height > 0)
			{
				m_pos.height = self_size.height;
			}
		} else
		{
			m_pos.height = self_size.height;
		}
		m_pos.height -= box_sizing_height();
	} else if (src_el()->is_block_formatting_context())
	{
		// add the floats' height to the block height
		pixel_t floats_height = fmt_ctx->get_floats_height();
		if (floats_height > m_pos.height)
		{
			m_pos.height = floats_height;
		}
	}
	if (!self_size.ratio_height && (containing_block_size.size_mode & containing_block_context::size_mode_content))
	{
		if(self_size.height.type == containing_block_context::cbc_value_type_absolute)
		{
			if(m_pos.height > self_size.height)
			{
				m_pos.height = self_size.height;
			}
		}
	}

	if ((is_grid_item() || has_preferred_aspect_ratio()) && self_size.min_height.type != containing_block_context::cbc_value_type_none &&
		self_size.max_height.type != containing_block_context::cbc_value_type_none)
		self_size.max_height.value = std::max(self_size.max_height.value, self_size.min_height.value);

	// Fix height with min-height attribute
	if(self_size.min_height.type != containing_block_context::cbc_value_type_none)
	{
		if(m_pos.height < self_size.min_height)
		{
			m_pos.height = self_size.min_height;
		}
	} else if(m_pos.height < 0)
	{
		m_pos.height = 0;
	}

	// Fix width with max-width attribute
	if(self_size.max_height.type != containing_block_context::cbc_value_type_none)
	{
		if(m_pos.height > self_size.max_height)
		{
			m_pos.height = self_size.max_height;
		}
	}

	if (containing_block_size.grid_content_edge == containing_block_context::content_baseline_edge::end)
		align_content_to_end();

    // calculate the final position
    m_pos.move_to(x, y);
    m_pos.x += content_offset_left();
    m_pos.y += content_offset_top();

	if (css().get_display() == display_list_item && css().get_list_style_position() == list_style_position_outside &&
		!m_children.empty() && m_children.front()->src_el()->tag() == marker_tag())
	{
		const auto& marker = m_children.front();
		auto width = self_size.new_width(self_size.render_width, containing_block_context::size_mode_content);
		width.width.type = containing_block_context::cbc_value_type_auto;
		width.height.type = containing_block_context::cbc_value_type_auto;
		marker->render(0, 0, width, nullptr);
		const auto baseline = css().has_layout_containment() ? find_layout_baseline(false) : layout_baseline(false);
		const pixel_t first_baseline = baseline.value_or(content_offset_top() +
			(css().line_height().computed_value - css().get_font_metrics().height) / 2 + css().get_font_metrics().ascent);
		const pixel_t gap = css().get_font() ? src_el()->get_document()->container()->text_width(" ", css().get_font()) : 0;
		marker->pos().x = -marker->width() - content_offset_left() - gap;
		marker->pos().y = first_baseline - content_offset_top() - marker->get_first_baseline();
	}

	const bool intrinsic_probe = containing_block_size.size_mode & containing_block_context::size_mode_intrinsic_inline_probe;
	const auto& intrinsic = containing_block_size.intrinsic_inline;
	if (intrinsic.preferred || intrinsic.minimum || intrinsic.maximum || self_size.ratio_width) ret_width = m_pos.width;

	rendered_width ret;
	ret.natural_width = ret_width + content_offset_width();
	if(!intrinsic_probe && !intrinsic.preferred && css().is_scroll_container() && css().get_width().is_predefined() &&
	   css().get_display() != display_table_cell)
	{
		ret.min_width = content_offset_left() + content_offset_right();
	} else
	{
		const auto keyword = css().get_width().intrinsic_size();
		const bool fits_content = keyword == css_intrinsic_size::fit_content || keyword == css_intrinsic_size::fit_content_limit;
		if(intrinsic_probe || fits_content || (css().get_width().is_predefined() && !intrinsic.preferred))
		{
			ret.min_width = ret_min_width + content_offset_width();
		} else
		{
			ret.min_width = width();
		}
	}

	if (intrinsic.minimum)
		ret.min_width = std::max(ret.min_width, *intrinsic.minimum + content_offset_width());
	if (intrinsic.maximum || intrinsic.preferred)
		ret.min_width = std::min(ret.min_width, width());

	return ret;
}
