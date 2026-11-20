#include "types.h"
#include "render_flex.h"
#include "html_tag.h"

litehtml::rendered_width litehtml::render_item_flex::_render_content(pixel_t x, pixel_t y, bool second_pass,
																	 const containing_block_context& self_size,
																	 formatting_context*			 fmt_ctx)
{
	bool is_row_direction = true;
	bool reverse = false;
	pixel_t container_main_size = self_size.render_width;

	switch (css().get_flex_direction())
	{
		case flex_direction_column:
			is_row_direction = false;
			reverse = false;
			break;
		case flex_direction_column_reverse:
			is_row_direction = false;
			reverse = true;
			break;
		case flex_direction_row:
			is_row_direction = true;
			reverse = false;
			break;
		case flex_direction_row_reverse:
			is_row_direction = true;
			reverse = true;
			break;
	}

	const auto clamp_axis = [](pixel_t size, const auto& min_size, const auto& max_size) {
		if (min_size.type != containing_block_context::cbc_value_type_none)
			size = std::max(size, min_size.value);
		if (max_size.type != containing_block_context::cbc_value_type_none)
			size = std::min(size, max_size.value);
		return std::max<pixel_t>(0, size);
	};
	const bool definite_width = self_size.width.type != containing_block_context::cbc_value_type_auto ||
		(!self_size.intrinsic_inline_basis && css().get_display() == display_flex && css().get_float() == float_none && src_el()->in_normal_flow());
	const bool definite_height = self_size.height.type == containing_block_context::cbc_value_type_absolute ||
		(self_size.height.type == containing_block_context::cbc_value_type_percentage && self_size.height > 0);
	const bool has_used_height = definite_height || self_size.contained_auto_block_size.has_value();
	const pixel_t content_width = clamp_axis(self_size.render_width, self_size.min_width, self_size.max_width);
	const pixel_t content_height = self_size.contained_auto_block_size.value_or(
		clamp_axis(self_size.height - box_sizing_height(), self_size.min_height, self_size.max_height));
	// Cyclic percentage gutters resolve to zero in Flexbox.
	const pixel_t row_gap = css().get_row_gap().calc_percent(definite_height ? content_height : 0);
	const pixel_t column_gap = css().get_column_gap().calc_percent(definite_width ? content_width : 0);
	const pixel_t main_gap = is_row_direction ? column_gap : row_gap;
	const pixel_t cross_gap = is_row_direction ? row_gap : column_gap;

	const bool auto_main_height = !is_row_direction && !has_used_height;
	const bool has_max_height = self_size.max_height.type != containing_block_context::cbc_value_type_none;
	const bool single_line = css().get_flex_wrap() == flex_wrap_nowrap || (auto_main_height && !has_max_height);
	if (!is_row_direction)
		container_main_size = auto_main_height && has_max_height ? std::max<pixel_t>(0, self_size.max_height) : content_height;

	/////////////////////////////////////////////////////////////////
	/// Split flex items to lines
	/////////////////////////////////////////////////////////////////
	m_lines = get_lines(self_size, fmt_ctx, is_row_direction, container_main_size, single_line, main_gap, second_pass);
	if (m_lines.empty()) return {0, 0};
	if (auto_main_height)
	{
		const auto longest = std::max_element(m_lines.begin(), m_lines.end(), [](const flex_line& a, const flex_line& b) {
			return a.main_size < b.main_size;
		});
		container_main_size = clamp_axis(longest->main_size, self_size.min_height, self_size.max_height);
	}

	pixel_t sum_cross_size = cross_gap * static_cast<pixel_t>(m_lines.size() - 1);
	// Natural row width measures the items together, including gaps between items on different lines.
	pixel_t ret_width = is_row_direction ? main_gap * static_cast<pixel_t>(m_lines.size() - 1) : 0;

	/////////////////////////////////////////////////////////////////
	/// Resolving Flexible Lengths
	/// REF: https://www.w3.org/TR/css-flexbox-1/#resolve-flexible-lengths
	/////////////////////////////////////////////////////////////////
	for(auto& ln : m_lines)
	{
		if(is_row_direction)
		{
			ret_width += ln.base_size;
		}
		ln.init(container_main_size, is_row_direction, self_size, fmt_ctx);
		sum_cross_size += ln.cross_size;
		if(reverse)
		{
			ln.items.reverse();
		}
	}

	pixel_t free_cross_size = 0;
	bool is_wrap_reverse = css().get_flex_wrap() == flex_wrap_wrap_reverse;

	/////////////////////////////////////////////////////////////////
	/// Calculate free cross size
	/////////////////////////////////////////////////////////////////
	if (is_row_direction)
	{
		if (has_used_height)
			free_cross_size = content_height - sum_cross_size;
	} else
	{
		free_cross_size = self_size.render_width - sum_cross_size;
		ret_width = sum_cross_size;
	}

	/////////////////////////////////////////////////////////////////
	/// Fix align-content property
	/////////////////////////////////////////////////////////////////
	const bool content_baseline = self_size.grid_content_edge != containing_block_context::content_baseline_edge::none;
	layout_content_height = is_row_direction ? sum_cross_size : 0;
	if (!is_row_direction)
		for (const auto& line : m_lines) layout_content_height = std::max(layout_content_height, line.main_size);
	flex_align_content align_content = css().get_flex_align_content();
	if (content_baseline && is_row_direction)
	{
		align_content = self_size.grid_content_edge == containing_block_context::content_baseline_edge::end && free_cross_size >= 0 ?
			flex_align_content_end : flex_align_content_start;
		if (align_content == flex_align_content_start && is_wrap_reverse)
			free_cross_size = 0;
	}

	if(align_content == flex_align_content_space_between)
	{
		// If the leftover free-space is negative or there is only a single flex line in the flex
		// container, this value is identical to flex-start.
		if (m_lines.size() == 1 || free_cross_size < 0) align_content = flex_align_content_flex_start;
	}
	if(align_content == flex_align_content_space_around)
	{
		// If the leftover free-space is negative or there is only a single flex line in the flex
		// container, this value is identical to flex-start.
		if (m_lines.size() == 1 || free_cross_size < 0) align_content = flex_align_content_center;
	}

	/////////////////////////////////////////////////////////////////
	/// Distribute free cross size for align-content: stretch
	/////////////////////////////////////////////////////////////////
	if (align_content == flex_align_content_stretch && free_cross_size > 0)
	{
		const pixel_t share = free_cross_size / static_cast<pixel_t>(m_lines.size());
		for (auto& line : m_lines) line.cross_size += share;
		free_cross_size = 0;
	}

	/// Reverse lines for flex-wrap: wrap-reverse
	if(css().get_flex_wrap() == flex_wrap_wrap_reverse)
	{
		m_lines.reverse();
	}

	/////////////////////////////////////////////////////////////////
	/// Align flex lines
	/////////////////////////////////////////////////////////////////
	pixel_t line_pos = 0;
	pixel_t add_before_line = 0;
	pixel_t add_after_line = 0;
	switch (align_content)
	{
		case flex_align_content_flex_start:
			if(is_wrap_reverse)
			{
				line_pos = free_cross_size;
			}
			break;
		case flex_align_content_flex_end:
			if(!is_wrap_reverse)
			{
				line_pos = free_cross_size;
			}
			break;
		case flex_align_content_end:
			line_pos = free_cross_size;
			break;
		case flex_align_content_center:
			line_pos = free_cross_size / 2;
			break;
		case flex_align_content_space_between:
			add_after_line = free_cross_size / (pixel_t) (m_lines.size() - 1);
			break;
		case flex_align_content_space_around:
			add_before_line = add_after_line = free_cross_size / (pixel_t) (m_lines.size() * 2);
			break;
		default:
			if(is_wrap_reverse)
			{
				line_pos = free_cross_size;
			}
			break;
	}
	bool first_line = true;
	for(auto &ln : m_lines)
	{
		if (!first_line) line_pos += cross_gap;
		first_line = false;
		line_pos += add_before_line;
		ln.cross_start = line_pos;
		line_pos += ln.cross_size + add_after_line;
	}

	/// Fix justify-content property
	flex_justify_content justify_content = css().get_flex_justify_content();
	if((justify_content == flex_justify_content_right || justify_content == flex_justify_content_left) && !is_row_direction)
	{
		justify_content = flex_justify_content_start;
	}

	/////////////////////////////////////////////////////////////////
	/// Align flex items in flex lines
	/////////////////////////////////////////////////////////////////
	for(auto &ln : m_lines)
	{
		pixel_t height = ln.calculate_items_position(container_main_size,
									justify_content,
									is_row_direction,
									self_size,
									fmt_ctx);
		m_pos.height = std::max(m_pos.height, height);
	}

	if (content_baseline && !is_row_direction &&
		self_size.grid_content_edge == containing_block_context::content_baseline_edge::end && has_used_height)
	{
		for (const auto& line : m_lines)
		{
			const auto last = std::max_element(line.items.begin(), line.items.end(), [](const auto& a, const auto& b) {
				return a->el->bottom() < b->el->bottom();
			});
			if (last == line.items.end()) continue;
			const auto extra = content_height - (*last)->el->bottom();
			if (extra <= 0) continue;
			for (const auto& item : line.items) item->el->y_shift(extra);
		}
	}

	// Auto height includes the full line stack, even when items do not stretch to the line edges.
	if (is_row_direction && !definite_height)
		m_pos.height = std::max(m_pos.height, sum_cross_size);
	else if (auto_main_height)
		m_pos.height = std::max(m_pos.height, container_main_size);

	// calculate the final position
	m_pos.move_to(x, y);
	m_pos.x += content_offset_left();
	m_pos.y += content_offset_top();

	pixel_t minimum_width = ret_width;
	if (is_row_direction && self_size.measuring_inline_size)
	{
		minimum_width = 0;
		for (const auto& line : m_lines)
		{
			if (single_line && !line.items.empty())
				minimum_width += main_gap * static_cast<pixel_t>(line.items.size() - 1);
			for (const auto& item : line.items)
			{
				if (single_line) minimum_width += item->min_size;
				else minimum_width = std::max(minimum_width, item->min_size);
			}
		}
	}
	return {ret_width, std::min(ret_width, minimum_width)};
}

std::list<litehtml::flex_line> litehtml::render_item_flex::get_lines(const litehtml::containing_block_context &self_size,
																	 litehtml::formatting_context *fmt_ctx,
																	 bool is_row_direction, pixel_t container_main_size,
																	 bool single_line, pixel_t main_gap, bool second_pass)
{
	bool reverse_main;
	bool reverse_cross = css().get_flex_wrap() == flex_wrap_wrap_reverse;

	if(is_row_direction)
	{
		reverse_main = css().get_flex_direction() == flex_direction_row_reverse;
	} else
	{
		reverse_main = css().get_flex_direction() == flex_direction_column_reverse;
	}

	std::list<flex_line> lines;
	flex_line line(reverse_main, reverse_cross);
	std::list<std::shared_ptr<flex_item>> items;
	int src_order = 0;
	bool sort_required = false;
	def_value<int> prev_order(0);

	for( auto& el : m_children)
	{

		// Positioned children keep their render boxes but never occupy a flex slot.
		if (!el->src_el()->in_normal_flow())
		{
			if (!second_pass && el->css().get_display() != display_none)
			{
				const auto natural_width = el->render(0, 0, self_size, fmt_ctx).natural_width;
				if (natural_width < el->width() && el->css().get_width().is_predefined())
					el->render(0, 0, self_size.new_width(natural_width), fmt_ctx);
			}
			continue;
		}
		std::shared_ptr<flex_item> item = nullptr;
		if(is_row_direction)
		{
			item = std::make_shared<flex_item_row_direction>(el);
		} else
		{
			item = std::make_shared<flex_item_column_direction>(el);
		}
		item->init(self_size, fmt_ctx, css().get_flex_align_items());
		item->src_order = src_order++;

		if(prev_order.is_default())
		{
			prev_order = item->order;
		} else if(!sort_required && item->order != prev_order)
		{
			sort_required = true;
		}

		items.emplace_back(item);
	}

	if(sort_required)
	{
		items.sort([](const std::shared_ptr<flex_item>& item1, const std::shared_ptr<flex_item>& item2)
					   {
					   		if(item1->order < item2->order) return true;
					   		if(item1->order == item2->order)
							{
								return item1->src_order < item2->src_order;
							}
							return false;
					   });
	}

	// Add flex items to lines
	for(auto& item : items)
	{
		if(!line.items.empty() && !single_line && line.main_size + main_gap + item->main_size > container_main_size)
		{
			lines.emplace_back(line);
			line = flex_line(reverse_main, reverse_cross);
		}

		line.main_gap = main_gap;
		if (!line.items.empty())
		{
			line.base_size += main_gap;
			line.main_size += main_gap;
		}
		line.base_size += item->base_size;
		line.main_size += item->main_size;
		if(!item->auto_margin_main_start.is_default()) line.num_auto_margin_main_start++;
		if(!item->auto_margin_main_end.is_default()) line.num_auto_margin_main_end++;
		line.items.push_back(item);
	}
	// Add the last line to the lines list
	if(!line.items.empty())
	{
		lines.emplace_back(line);
	}
	return lines;
}

std::shared_ptr<litehtml::render_item> litehtml::render_item_flex::init()
{
    auto doc = src_el()->get_document();
    decltype(m_children) new_children;
    decltype(m_children) inlines;

    auto convert_inlines = [&]()
        {
        if(!inlines.empty())
        {
            // Find last not space
            auto not_space = std::find_if(inlines.rbegin(), inlines.rend(), [&](const std::shared_ptr<render_item>& el)
                {
                return !el->src_el()->is_space();
                });
            if(not_space != inlines.rend())
            {
                // Erase all spaces at the end
                inlines.erase((not_space.base()), inlines.end());
            }

            auto anon_el = std::make_shared<html_tag>(src_el());
            auto anon_ri = std::make_shared<render_item_block>(anon_el);
            for(const auto& inl : inlines)
            {
                anon_ri->add_child(inl);
            }
            anon_ri->parent(shared_from_this());

            new_children.push_back(anon_ri->init());
            inlines.clear();
        }
        };

    for (const auto& el : m_children)
    {
        if(el->src_el()->css().get_display() == display_inline_text)
        {
            if(!inlines.empty())
            {
                inlines.push_back(el);
            } else
            {
                if (!el->src_el()->is_white_space())
                {
                    inlines.push_back(el);
                }
            }
        } else
        {
            convert_inlines();
            if(el->src_el()->is_block_box())
            {
                // Add block boxes as is
                el->parent(shared_from_this());
                new_children.push_back(el->init());
            } else
            {
                // Wrap inlines with anonymous block box
                auto anon_el = std::make_shared<html_tag>(el->src_el());
                auto anon_ri = std::make_shared<render_item_block>(anon_el);
                anon_ri->add_child(el->init());
                anon_ri->parent(shared_from_this());
                new_children.push_back(anon_ri->init());
            }
        }
    }
    convert_inlines();
    children() = new_children;

    src_el()->add_render(shared_from_this());
    return shared_from_this();
}

std::optional<litehtml::pixel_t> litehtml::render_item_flex::find_layout_baseline(bool last) const
{
	if (m_lines.empty()) return std::nullopt;
	const bool row = is_one_of(css().get_flex_direction(), flex_direction_row, flex_direction_row_reverse);
	const bool reverse_lines = !row && css().get_flex_wrap() == flex_wrap_wrap_reverse;
	const auto& line = last != reverse_lines ? m_lines.back() : m_lines.front();
	if (line.items.empty()) return std::nullopt;
	if (row)
	{
		const auto& preferred = last ? line.last_baseline : line.first_baseline;
		const auto& alternate = last ? line.first_baseline : line.last_baseline;
		const auto& shared = preferred.type() == baseline::baseline_type_none ? alternate : preferred;
		if (shared.type() != baseline::baseline_type_none)
			return content_offset_top() + line.cross_start + shared.get_offset_from_top(line.cross_size);
	}
	const auto& item = *(last ? line.items.back()->el : line.items.front()->el);
	return content_offset_top() + item.top() + item.baseline_or_border_bottom(last);
}

litehtml::pixel_t litehtml::render_item_flex::get_first_baseline()
{
	if (css().has_layout_containment()) return height() - margin_bottom();
	if(css().get_flex_direction() == flex_direction_row || css().get_flex_direction() == flex_direction_row_reverse)
	{
		if(!m_lines.empty())
		{
			const auto &first_line = m_lines.front();
			if(first_line.first_baseline.type() != baseline::baseline_type_none)
			{
				return first_line.cross_start + first_line.first_baseline.get_offset_from_top(first_line.cross_size) + content_offset_top();
			}
			if(first_line.last_baseline.type() != baseline::baseline_type_none)
			{
				return first_line.cross_start + first_line.last_baseline.get_offset_from_top(first_line.cross_size) + content_offset_top();
			}
		}
	}
	if(!m_lines.empty())
	{
		if(!m_lines.front().items.empty())
		{
			return m_lines.front().items.front()->el->get_first_baseline() + content_offset_top();
		}
	}
	return height();
}

litehtml::pixel_t litehtml::render_item_flex::get_last_baseline()
{
	if (css().has_layout_containment()) return height() - margin_bottom();
	if(css().get_flex_direction() == flex_direction_row || css().get_flex_direction() == flex_direction_row_reverse)
	{
		if(!m_lines.empty())
		{
			const auto &first_line = m_lines.front();
			if(first_line.last_baseline.type() != baseline::baseline_type_none)
			{
				return first_line.cross_start + first_line.last_baseline.get_offset_from_top(first_line.cross_size) + content_offset_top();
			}
			if(first_line.first_baseline.type() != baseline::baseline_type_none)
			{
				return first_line.cross_start + first_line.first_baseline.get_offset_from_top(first_line.cross_size) + content_offset_top();
			}
		}
	}
	if(!m_lines.empty())
	{
		if(!m_lines.front().items.empty())
		{
			return m_lines.front().items.front()->el->get_last_baseline() + content_offset_top();
		}
	}
	return height();
}
