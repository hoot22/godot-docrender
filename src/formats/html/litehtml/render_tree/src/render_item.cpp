#include "render_item.h"
#include "document.h"
#include <algorithm>
#include <typeinfo>
#include "document_container.h"
#include "el_image.h"
#include "types.h"

litehtml::render_item::render_item(std::shared_ptr<element>  _src_el) :
        m_element(std::move(_src_el)),
        m_skip(false)
{
    document::ptr doc = src_el()->get_document();
	auto fm = css().get_font_metrics();

    m_margins.left		= doc->to_pixels(src_el()->css().get_margins().left,         fm, 0);
    m_margins.right		= doc->to_pixels(src_el()->css().get_margins().right,        fm, 0);
    m_margins.top		= doc->to_pixels(src_el()->css().get_margins().top,          fm, 0);
    m_margins.bottom	= doc->to_pixels(src_el()->css().get_margins().bottom,       fm, 0);

    m_padding.left		= doc->to_pixels(src_el()->css().get_padding().left,         fm, 0);
    m_padding.right		= doc->to_pixels(src_el()->css().get_padding().right,        fm, 0);
    m_padding.top		= doc->to_pixels(src_el()->css().get_padding().top,          fm, 0);
    m_padding.bottom	= doc->to_pixels(src_el()->css().get_padding().bottom,       fm, 0);

    m_borders.left		= doc->to_pixels(src_el()->css().get_borders().left.width,   fm, 0);
    m_borders.right		= doc->to_pixels(src_el()->css().get_borders().right.width,  fm, 0);
    m_borders.top		= doc->to_pixels(src_el()->css().get_borders().top.width,    fm, 0);
    m_borders.bottom	= doc->to_pixels(src_el()->css().get_borders().bottom.width, fm, 0);
}

bool litehtml::render_item::has_preferred_aspect_ratio() const
{
	const auto display = css().get_display();
	return css().get_aspect_ratio().value > 0 && display != display_inline && display != display_inline_text &&
		(display < display_table_cell || display > display_table_row_group);
}

litehtml::pixel_t litehtml::render_item::size_from_aspect_ratio(pixel_t source, css_aspect_ratio ratio, bool to_width) const
{
	const auto width_edge = ratio.prefer_natural ? 0 : box_sizing_width();
	const auto height_edge = ratio.prefer_natural ? 0 : box_sizing_height();
	const auto extent = std::max(0.0, static_cast<double>(source));
	const auto transferred = to_width ? (extent + height_edge) * ratio.value - width_edge :
		(extent + width_edge) / ratio.value - height_edge;
	return static_cast<pixel_t>(std::clamp(transferred, 0.0, static_cast<double>(INT32_MAX)));
}

litehtml::intrinsic_inline_sizes litehtml::render_item::resolve_intrinsic_inline_sizes(
	const containing_block_context& enclosing, std::optional<pixel_t> available_width)
{
	using context = containing_block_context;
	using kind = css_intrinsic_size;
	intrinsic_inline_sizes result;
	if (enclosing.size_mode & context::size_mode_intrinsic_inline_probe) return result;

	const bool assigned = enclosing.size_mode & (context::size_mode_exact_width | context::size_mode_grid_assigned_width);
	const auto& width = css().get_width();
	const auto& min_width = css().get_min_width();
	const auto& max_width = css().get_max_width();
	const kind preferred = assigned || css().get_display() == display_table_cell ? kind::none : width.intrinsic_size();
	const kind minimum = min_width.intrinsic_size();
	const kind maximum = max_width.intrinsic_size();
	if (preferred == kind::none && minimum == kind::none && maximum == kind::none) return result;

	// Measure the contents with an indefinite percentage basis. Only the sized
	// box ignores its authored inline size. Descendants still use their own styles.
	pixel_t min_content = 0;
	pixel_t max_content = 0;
	const auto needs_content = [](kind value) { return !is_one_of(value, kind::none, kind::stretch, kind::stretch_limit); };
	if (needs_content(preferred) || needs_content(minimum) || needs_content(maximum))
	{
		const auto generation = src_el()->get_document()->m_layout_generation;
		if (!m_inline_measurement || m_inline_measurement->generation != generation ||
			m_inline_measurement->height_basis.type != enclosing.height.type ||
			m_inline_measurement->height_basis.value != enclosing.height.value)
		{
			context probe;
			probe.context_idx = enclosing.context_idx;
			probe.width = 1 << 20;
			probe.render_width = probe.width;
			probe.height = enclosing.height;
			probe.size_mode = context::size_mode_content | context::size_mode_grid_measure_width | context::size_mode_intrinsic_inline_probe;
			const auto measured = render(0, 0, probe, nullptr);
			const auto max_width = std::max<pixel_t>(0, measured.natural_width - content_offset_width());
			const auto min_width = std::clamp(measured.min_width - content_offset_width(), pixel_t(0), max_width);
			const inline_measurement entry{ generation, enclosing.height, { max_width, min_width } };
			if (m_inline_measurement) *m_inline_measurement = entry;
			else m_inline_measurement = std::make_unique<inline_measurement>(entry);
		}
		// Reuse content measurements within this reflow. The final layout still runs at the used width.
		const auto& content = m_inline_measurement->content;
		min_content = content.min_width;
		max_content = content.natural_width;
		result.content = content;
	}

	const bool definite_basis = enclosing.width.type == context::cbc_value_type_absolute ||
		(!enclosing.grid_intrinsic_width && !enclosing.intrinsic_inline_basis &&
			!(enclosing.size_mode & (context::size_mode_content | context::size_mode_grid_measure_width)));
	calc_outlines(definite_basis ? enclosing.width.value : 0);
	const auto outlines = content_offset_width();
	const auto available = available_width.value_or(enclosing.width.value);
	const auto resolve = [&](kind value, const css_length& length, bool lower) -> std::optional<pixel_t> {
		switch (value)
		{
		case kind::min_content: return min_content;
		case kind::max_content: return max_content;
		case kind::fit_content:
			return definite_basis ? std::clamp(available - outlines, min_content, max_content) : max_content;
		case kind::fit_content_limit:
		{
			const auto limit = length.intrinsic_size_limit();
			if (limit.units() == css_units_percentage && !definite_basis)
				return lower ? min_content : max_content;
			return std::clamp(limit.calc_percent(enclosing.width.value) - outlines, min_content, max_content);
		}
		case kind::stretch:
			if (definite_basis) return std::max<pixel_t>(0, available - outlines);
			return std::nullopt;
		case kind::stretch_limit:
		{
			const auto limit = length.intrinsic_size_limit();
			if (limit.units() == css_units_percentage && !definite_basis && !lower) return std::nullopt;
			return std::max<pixel_t>(0, limit.calc_percent(definite_basis ? enclosing.width.value : 0) - outlines);
		}
		default:
			return std::nullopt;
		}
	};

	result.preferred = resolve(preferred, width, false);
	result.minimum = resolve(minimum, min_width, true);
	result.maximum = resolve(maximum, max_width, false);
	return result;
}

litehtml::rendered_width litehtml::render_item::render(pixel_t x, pixel_t y,
													   const containing_block_context& containing_block_size,
													   formatting_context* fmt_ctx, bool second_pass)
{
	const auto intrinsic_sizes = containing_block_size.intrinsic_inline.preferred ? containing_block_size.intrinsic_inline :
		resolve_intrinsic_inline_sizes(containing_block_size);
	auto resolved = containing_block_size;
	resolved.intrinsic_inline = intrinsic_sizes;
	const bool intrinsic_outlines = containing_block_size.intrinsic_inline_basis || (containing_block_size.size_mode & containing_block_context::size_mode_grid_measure_width) ||
		(containing_block_size.grid_intrinsic_width && css().get_display() == display_inline_grid);
	calc_outlines(intrinsic_outlines ? 0 : containing_block_size.width.value);
	if (containing_block_size.grid_content_edge == containing_block_context::content_baseline_edge::start)
		m_padding.top += containing_block_size.grid_baseline_padding;
	else if (containing_block_size.grid_content_edge == containing_block_context::content_baseline_edge::end)
		m_padding.bottom += containing_block_size.grid_baseline_padding;

	m_pos.clear();
	m_pos.move_to(x, y);
	m_relative_offset_x = 0;
	m_relative_offset_y = 0;

	pixel_t content_left = content_offset_left();
	pixel_t content_top = content_offset_top();

	m_pos.x += content_left;
	m_pos.y += content_top;


	if(src_el()->is_block_formatting_context() || is_grid_item() || ! fmt_ctx)
	{
		formatting_context fmt;
		auto			   ret = _render(x, y, resolved, &fmt, second_pass);
		fmt.apply_relative_shift(containing_block_size);
		return ret;
	}

	fmt_ctx->push_position(x + content_left, y + content_top);
	auto ret = _render(x, y, resolved, fmt_ctx, second_pass);
	fmt_ctx->pop_position(x + content_left, y + content_top);
	return ret;
}

void litehtml::render_item::calc_outlines( pixel_t parent_width )
{
    m_padding.left	= m_element->css().get_padding().left.calc_percent(parent_width);
    m_padding.right	= m_element->css().get_padding().right.calc_percent(parent_width);

    m_borders.left	= m_element->css().get_borders().left.width.calc_percent(parent_width);
    m_borders.right	= m_element->css().get_borders().right.width.calc_percent(parent_width);
    if (m_shared_table_borders) m_borders = *m_shared_table_borders;

    m_margins.left	= m_element->css().get_margins().left.calc_percent(parent_width);
    m_margins.right	= m_element->css().get_margins().right.calc_percent(parent_width);

    m_margins.top		= m_element->css().get_margins().top.calc_percent(parent_width);
    m_margins.bottom	= m_element->css().get_margins().bottom.calc_percent(parent_width);

    m_padding.top		= m_element->css().get_padding().top.calc_percent(parent_width);
    m_padding.bottom	= m_element->css().get_padding().bottom.calc_percent(parent_width);
}

litehtml::pixel_t litehtml::render_item::calc_auto_margins(pixel_t parent_width)
{
    if ((src_el()->css().get_display() == display_block || src_el()->css().get_display() == display_table ||
            src_el()->css().get_display() == display_grid || is_grid_item()) &&
            src_el()->css().get_position() != element_position_absolute &&
            src_el()->css().get_float() == float_none)
    {
        if (src_el()->css().get_margins().left.is_predefined() && src_el()->css().get_margins().right.is_predefined())
        {
            pixel_t el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right;
            if (el_width <= parent_width)
            {
                m_margins.left = (parent_width - el_width) / 2;
                m_margins.right = (parent_width - el_width) - m_margins.left;
            }
            else
            {
                m_margins.left = 0;
                m_margins.right = 0;
            }
			return m_margins.left;
        }
        else if (src_el()->css().get_margins().left.is_predefined() && !src_el()->css().get_margins().right.is_predefined())
        {
            pixel_t el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right + m_margins.right;
            m_margins.left = parent_width - el_width;
            if (m_margins.left < 0) m_margins.left = 0;
			return m_margins.left;
        }
        else if (!src_el()->css().get_margins().left.is_predefined() && src_el()->css().get_margins().right.is_predefined())
        {
            pixel_t el_width = m_pos.width + m_borders.left + m_borders.right + m_padding.left + m_padding.right + m_margins.left;
            m_margins.right = parent_width - el_width;
            if (m_margins.right < 0) m_margins.right = 0;
        }
    }
	return 0;
}

void litehtml::render_item::apply_relative_shift(const containing_block_context &containing_block_size)
{
    if (src_el()->css().get_position() == element_position_relative)
    {
        css_offsets offsets = src_el()->css().get_offsets();
        pixel_t relative_x = 0;
        pixel_t relative_y = 0;
        if (!offsets.left.is_predefined())
        {
            relative_x = offsets.left.calc_percent(containing_block_size.width);
        }
        else if (!offsets.right.is_predefined())
        {
            relative_x = -offsets.right.calc_percent(containing_block_size.width);
        }
        if (!offsets.top.is_predefined())
        {
            relative_y = offsets.top.calc_percent(containing_block_size.height);
        }
        else if (!offsets.bottom.is_predefined())
        {
            relative_y = -offsets.bottom.calc_percent(containing_block_size.height);
        }
        m_pos.x += relative_x - m_relative_offset_x;
        m_pos.y += relative_y - m_relative_offset_y;
        m_relative_offset_x = relative_x;
        m_relative_offset_y = relative_y;
    }
}

std::tuple<
        std::shared_ptr<litehtml::render_item>,
        std::shared_ptr<litehtml::render_item>,
        std::shared_ptr<litehtml::render_item>
        > litehtml::render_item::split_inlines()
{
    std::tuple<
            std::shared_ptr<litehtml::render_item>,
            std::shared_ptr<litehtml::render_item>,
            std::shared_ptr<litehtml::render_item>
    > ret;
    for(const auto& child: m_children)
    {
        if(child->src_el()->is_block_box() && child->src_el()->css().get_float() == float_none)
        {
            std::get<0>(ret) = clone();
            std::get<1>(ret) = child;
            std::get<2>(ret) = clone();

            std::get<1>(ret)->parent(std::get<0>(ret));
            std::get<2>(ret)->parent(std::get<0>(ret));

            bool found = false;
            for(const auto& ch: m_children)
            {
                if(ch == child)
                {
                    found = true;
                    continue;
                }
                if(!found)
                {
                    std::get<0>(ret)->add_child(ch);
                } else
                {
                    std::get<2>(ret)->add_child(ch);
                }
            }
            break;
        }
        // Atomic inline boxes keep their internal block layout.
        if(child->css().get_display() == display_inline && !child->children().empty())
        {
            auto child_split = child->split_inlines();
            if(std::get<0>(child_split))
            {
                std::get<0>(ret) = clone();
                std::get<1>(ret) = std::get<1>(child_split);
                std::get<2>(ret) = clone();

                std::get<2>(ret)->parent(std::get<0>(ret));

                bool found = false;
                for(const auto& ch: m_children)
                {
                    if(ch == child)
                    {
                        found = true;
                        continue;
                    }
                    if(!found)
                    {
                        std::get<0>(ret)->add_child(ch);
                    } else
                    {
                        std::get<2>(ret)->add_child(ch);
                    }
                }
                std::get<0>(ret)->add_child(std::get<0>(child_split));
                std::get<2>(ret)->add_child(std::get<2>(child_split));
                break;
            }
        }
    }
    return ret;
}

void litehtml::render_item::paint(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip)
{
	auto* container = src_el()->get_document()->container();
	if (container->begin_render_item(*this, clip))
	{
		const auto item = shared_from_this();
		if (css().skips_contents())
			src_el()->draw_background(hdc, x, y, clip, item);
		else if (css().get_content_image()) {
			auto& tag = static_cast<html_tag&>(*src_el());
			tag.draw_background(hdc, x, y, clip, item);
			auto box = pos();
			box.x += x;
			box.y += y;
			box.round();
			if (box.width > 0 && box.height > 0 && box.does_intersect(clip))
				tag.content_image()->draw(tag, hdc, box, *this);
		}
		else
			src_el()->draw(hdc, x, y, clip, item);
	}
	container->end_render_item();
}

std::shared_ptr<litehtml::render_item> litehtml::render_item::fixed_containing_block() const
{
	if (css().get_position() != element_position_fixed) return {};
	for (auto ancestor = parent(); ancestor; ancestor = ancestor->parent())
		if (ancestor->establishes_effect_containing_block()) return ancestor;
	return {};
}

namespace {
	bool float_containing_block_is_rtl(const litehtml::render_item& item)
	{
		using namespace litehtml;
		for (auto ancestor = item.parent(); ancestor; ancestor = ancestor->parent()) {
			switch (ancestor->css().get_display()) {
				case display_inline:
				case display_inline_text:
				case display_table_column:
				case display_table_column_group:
				case display_table_row:
				case display_table_row_group:
				case display_table_header_group:
				case display_table_footer_group:
					continue;
				default:
					return ancestor->css().is_right_to_left();
			}
		}
		return false;
	}
}

litehtml::element_float litehtml::render_item::get_used_float() const
{
	const auto side = css().get_float();
	if (side != float_inline_start && side != float_inline_end) return side;
	const bool left = (side == float_inline_start) != float_containing_block_is_rtl(*this);
	return left ? float_left : float_right;
}

litehtml::element_clear litehtml::render_item::get_used_clear() const
{
	const auto side = css().get_clear();
	if (side != clear_inline_start && side != clear_inline_end) return side;
	const bool left = (side == clear_inline_start) != float_containing_block_is_rtl(*this);
	return left ? clear_left : clear_right;
}

void litehtml::render_item::paint_group(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, bool with_positioned)
{
	auto* container = src_el()->get_document()->container();
	container->begin_paint_group(*this);
	paint(hdc, x, y, clip);
	draw_stacking_context(hdc, x, y, clip, with_positioned);
	container->end_paint_group();
}

bool litehtml::render_item::is_viewport_fixed() const
{
	return css().get_position() == element_position_fixed && !fixed_containing_block();
}

void litehtml::render_item::extend_scroll_size(const size& extent)
{
	if (m_scroll_view)
		m_scroll_view->set(m_pos, {std::max(extent.width, m_scroll_view->get_max_h_scroll() + m_pos.width),
			std::max(extent.height, m_scroll_view->get_max_v_scroll() + m_pos.height)});
}

bool litehtml::render_item::fetch_positioned()
{
    return fetch_positioned(*this);
}

bool litehtml::render_item::fetch_positioned(render_item& parent_context)
{
    bool ret = false;

    m_positioned.clear();
    m_stacking_levels.clear();
    // Layout still visits positioned descendants through their existing containing blocks.
    auto& stacking_context = establishes_stacking_context() ? *this : parent_context;

    litehtml::element_position el_pos;

    for(auto& el : m_children)
    {
        el_pos = el->src_el()->css().get_position();
        if (el->establishes_paint_context())
        {
            add_positioned(el);
            stacking_context.m_stacking_levels.push_back(el->paint_z_index());
        }
        if (!ret && (el_pos == element_position_absolute || el_pos == element_position_fixed))
        {
            ret = true;
        }
        if(el->fetch_positioned(stacking_context))
        {
            ret = true;
        }
    }
    if (&stacking_context == this)
    {
        auto& levels = m_stacking_levels;
        std::sort(levels.begin(), levels.end());
        levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
    }
    return ret;
}

void litehtml::render_item::render_positioned(render_type rt)
{
    position view_port;
    src_el()->get_document()->container()->get_viewport(view_port);

    element_position el_position;
    bool process;
    for (auto& el : m_positioned)
    {
        el_position = el->src_el()->css().get_position();

        process = false;
        if(el->src_el()->css().get_display() != display_none)
        {
            if(el_position == element_position_absolute)
            {
                if(rt != render_fixed_only)
                {
                    process = true;
                }
            } else if(el_position == element_position_fixed)
            {
                if(rt != render_no_fixed)
                {
                    process = true;
                }
            }
        }

        if(process)
        {
			containing_block_context containing_block_size;
            const auto fixed_block = el->fixed_containing_block();
			auto* block_ptr = fixed_block ? fixed_block.get() : this;
			if (el_position == element_position_absolute)
			{
				while (!block_ptr->is_root() && !block_ptr->establishes_positioned_containing_block())
				{
					auto ancestor = block_ptr->parent();
					if (!ancestor) break;
					block_ptr = ancestor.get();
				}
			}
			auto& block = *block_ptr;
            if(el->is_viewport_fixed() || (block.is_root() && !block.establishes_positioned_containing_block()))
            {
				containing_block_size.height	= view_port.height;
				containing_block_size.width		= view_port.width;
            } else
            {
				containing_block_size.height = block.m_pos.height + block.m_padding.height();
				containing_block_size.width = block.m_pos.width + block.m_padding.width();
            }

            css_length	css_left	= el->src_el()->css().get_offsets().left;
            css_length	css_right	= el->src_el()->css().get_offsets().right;
            css_length	css_top		= el->src_el()->css().get_offsets().top;
            css_length	css_bottom	= el->src_el()->css().get_offsets().bottom;

            bool need_render = false;
			const bool positioned_ratio = el->has_preferred_aspect_ratio();

            css_length el_width = el->src_el()->css().get_width();
            css_length el_height = el->src_el()->css().get_height();

			auto fix_height_min_max = [&] (pixel_t height)
			{
				if (positioned_ratio) return el->height();
				auto max_height = el->css().get_max_height();
				auto min_height = el->css().get_min_height();
				if(!max_height.is_predefined())
				{
					pixel_t max_height_value = max_height.calc_percent(containing_block_size.height);
					if(height > max_height_value)
					{
						height = max_height_value;
					}
				}
				if(!min_height.is_predefined())
				{
					pixel_t min_height_value = min_height.calc_percent(containing_block_size.height);
					if(height < min_height_value)
					{
						height = min_height_value;
					}
				}
				return std::max<pixel_t>(0, height - el->box_sizing_height()) + el->content_offset_height();
			};

			auto fix_width_min_max = [&] (pixel_t width)
			{
				if (positioned_ratio || containing_block_size.intrinsic_inline.preferred) return el->width();
				auto max_width = el->css().get_max_width();
				auto min_width = el->css().get_min_width();
				if(!max_width.is_predefined())
				{
					pixel_t max_width_value = max_width.calc_percent(containing_block_size.width);
					if(width > max_width_value)
					{
						width = max_width_value;
					}
				}
				if(!min_width.is_predefined())
				{
					pixel_t min_width_value = min_width.calc_percent(containing_block_size.width);
					if(width < min_width_value)
					{
						width = min_width_value;
					}
				}
				return std::max<pixel_t>(0, width - el->box_sizing_width()) + el->content_offset_width();
			};

			pixel_t bottom = 0;
			pixel_t top = 0;
			pixel_t height = 0;
			auto [el_static_offset_x, el_static_offset_y] = block.element_static_offset(el);
			pixel_t el_static_x = el->m_pos.x + el_static_offset_x;
			pixel_t el_static_y = el->m_pos.y + el_static_offset_y;
			if (positioned_ratio || el_width.intrinsic_size() != css_intrinsic_size::none ||
				el->css().get_min_width().intrinsic_size() != css_intrinsic_size::none ||
				el->css().get_max_width().intrinsic_size() != css_intrinsic_size::none)
			{
				// Insets reduce available space. Percentage sizes still use the original containing block.
				using context = containing_block_context;
				containing_block_size.width.type = containing_block_size.height.type = context::cbc_value_type_absolute;
				containing_block_size.render_width = containing_block_size.width;
				const bool has_insets = !css_left.is_predefined() && !css_right.is_predefined();
				const bool replaced = el->css().is_replaced();
				const bool auto_width = el_width.is_predefined() && el_width.intrinsic_size() == css_intrinsic_size::none;
				const bool auto_height = el_height.is_predefined();
				// If only the vertical axis has two insets, it determines the ratio for two auto sizes.
				const bool use_inset_height = positioned_ratio && !replaced && auto_width && auto_height && !has_insets &&
					!css_top.is_predefined() && !css_bottom.is_predefined();
				if (use_inset_height)
				{
					el->calc_outlines(containing_block_size.width);
					containing_block_size.positioned_height = std::max<pixel_t>(0, containing_block_size.height -
						css_top.calc_percent(containing_block_size.height) - css_bottom.calc_percent(containing_block_size.height) -
						el->content_offset_height());
				}
				const pixel_t start = css_left.is_predefined() && css_right.is_predefined() ?
					el_static_x - el->content_offset_left() : css_left.calc_percent(containing_block_size.width);
				const pixel_t available = std::max<pixel_t>(0, containing_block_size.width - start -
					css_right.calc_percent(containing_block_size.width));
				const auto old_position = el->m_pos;
				auto sizes = el->resolve_intrinsic_inline_sizes(containing_block_size, available);
				const pixel_t stretch = std::max<pixel_t>(0, available - el->content_offset_width());
				pixel_t preferred = old_position.width;
				if (!el_width.is_predefined())
					preferred = el_width.calc_percent(containing_block_size.width) - el->box_sizing_width();
				else if (has_insets && !replaced && !is_one_of(el->css().get_display(), display_table, display_inline_table))
					preferred = stretch;
				else if (sizes.content)
					preferred = std::clamp(stretch, sizes.content->min_width, sizes.content->natural_width);
				const bool width_from_height = positioned_ratio && auto_width && (!auto_height || use_inset_height);
				if (!width_from_height && !(positioned_ratio && replaced && auto_width))
					sizes.preferred = sizes.preferred.value_or(preferred);
				containing_block_size.intrinsic_inline = sizes;
				// Resolve the automatic height at the chosen inline size before resolving bottom alignment.
				el->render(0, 0, containing_block_size, nullptr);
				el_width.set_value(el->m_pos.width, css_units_px);
				if (positioned_ratio) el_height.set_value(el->m_pos.height, css_units_px);
			}
			// Calculate vertical position
			// https://www.w3.org/TR/CSS22/visudet.html#abs-non-replaced-height
			// 10.6.4 Absolutely positioned, non-replaced elements
			if(css_top.is_predefined() && !css_bottom.is_predefined() && el_height.is_predefined())
			{
				// 1. 'top' and 'height' are 'auto' and 'bottom' is not 'auto', then the height is based on the
				// content per 10.6.7, set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
				if(el->css().get_margins().top.is_predefined()) el->m_margins.top = 0;
				if(el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = 0;
				height = el->height();
				bottom = css_bottom.calc_percent(containing_block_size.height);
				top = containing_block_size.height - height - bottom;
			} else if(css_top.is_predefined() && css_bottom.is_predefined() && !el_height.is_predefined())
			{
				// 2. 'top' and 'bottom' are 'auto' and 'height' is not 'auto', then set 'top' to the static position,
				// set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
				if(el->css().get_margins().top.is_predefined()) el->m_margins.top = 0;
				if(el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = 0;
				top = el_static_y - el->content_offset_top();
				height = fix_height_min_max(el_height.calc_percent(containing_block_size.height));
			} else if(!css_top.is_predefined() && css_bottom.is_predefined() && el_height.is_predefined())
			{
				// 3. 'height' and 'bottom' are 'auto' and 'top' is not 'auto', then the height is based on the
				// content per 10.6.7, set 'auto' values for 'margin-top' and 'margin-bottom' to 0,
				// and solve for 'bottom'
				if(el->css().get_margins().top.is_predefined()) el->m_margins.top = 0;
				if(el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = 0;
				height = el->height();
				top = css_top.calc_percent(containing_block_size.height);
			} else if(css_top.is_predefined() && !css_bottom.is_predefined() && !el_height.is_predefined())
			{
				// 4. 'top' is 'auto', 'height' and 'bottom' are not 'auto', then set 'auto' values for 'margin-top'
				// and 'margin-bottom' to 0, and solve for 'top'
				if(el->css().get_margins().top.is_predefined()) el->m_margins.top = 0;
				if(el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = 0;
				height = fix_height_min_max(el_height.calc_percent(containing_block_size.height));
				bottom = css_bottom.calc_percent(containing_block_size.height);
				top = containing_block_size.height - height - bottom;
			} else if(!css_top.is_predefined() && !css_bottom.is_predefined() && el_height.is_predefined())
			{
				// 5. 'height' is 'auto', 'top' and 'bottom' are not 'auto', then 'auto' values for 'margin-top' and
				// 'margin-bottom' are set to 0 and solve for 'height'
				if(el->css().get_margins().top.is_predefined()) el->m_margins.top = 0;
				if(el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = 0;
				bottom = css_bottom.calc_percent(containing_block_size.height);
				top = css_top.calc_percent(containing_block_size.height);
				if(el->src_el()->is_replaced())
				{
					height = el->height() - el->content_offset_height();
					pixel_t reminded = (containing_block_size.height - top - bottom) - height - el->content_offset_height();
					if(reminded > 0)
					{
						int divider = 0;
						if (el->css().get_margins().top.is_predefined()) divider++;
						if (el->css().get_margins().bottom.is_predefined()) divider++;
						if (divider != 0)
						{
							if (el->css().get_margins().top.is_predefined()) el->m_margins.top = reminded / divider;
							if (el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = reminded / divider;
						}
					}
					height += el->content_offset_height();
				} else
				{
					height = containing_block_size.height - top - bottom;
				}
				if(!el->css().get_max_height().is_predefined())
				{
					pixel_t max_height = el->css().get_max_height().calc_percent(containing_block_size.height);
					if(height - el->content_offset_height() > max_height)
					{
						pixel_t reminded = height - el->content_offset_height() - max_height;
						height = max_height;
						int divider = 0;
						if(el->css().get_margins().top.is_predefined()) divider++;
						if(el->css().get_margins().bottom.is_predefined()) divider++;
						if(divider != 0)
						{
							if(el->css().get_margins().top.is_predefined()) el->m_margins.top = reminded / divider;
							if(el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = reminded / divider;
						}
						height += el->content_offset_height();
					}
				}
			} else if(!css_top.is_predefined() && css_bottom.is_predefined() && !el_height.is_predefined())
			{
				// 6. 'bottom' is 'auto', 'top' and 'height' are not 'auto', then set 'auto' values for 'margin-top'
				// and 'margin-bottom' to 0 and solve for 'bottom'
				if(el->css().get_margins().top.is_predefined()) el->m_margins.top = 0;
				if(el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = 0;
				height = fix_height_min_max(el_height.calc_percent(containing_block_size.height));
				top = css_top.calc_percent(containing_block_size.height);
			} else if(css_top.is_predefined() && css_bottom.is_predefined() && el_height.is_predefined())
			{
				// If all three of 'top', 'height', and 'bottom' are auto, set 'top' to the static position and
				// apply rule number three.
				if(el->css().get_margins().top.is_predefined()) el->m_margins.top = 0;
				if(el->css().get_margins().bottom.is_predefined()) el->m_margins.bottom = 0;
				height = el->height();
				top = el_static_y - el->content_offset_top();
			} else
			{
				// If none of the three are 'auto':
				height = fix_height_min_max(el_height.calc_percent(containing_block_size.height));
				top = css_top.calc_percent(containing_block_size.height);
				bottom = css_bottom.calc_percent(containing_block_size.height);
				pixel_t remained = containing_block_size.height - height - top - bottom;

				if(el->css().get_margins().top.is_predefined() && el->css().get_margins().bottom.is_predefined())
				{
					// If both 'margin-top' and 'margin-bottom' are 'auto', solve the equation under the extra
					// constraint that the two margins get equal values.
					el->m_margins.top = el->m_margins.bottom = remained / 2;
					height += el->m_margins.top + el->m_margins.bottom;
				} else
				{
					// If one of 'margin-top' or 'margin-bottom' is 'auto', solve the equation for that value.
					if(el->css().get_margins().top.is_predefined())
					{
						el->m_margins.top = remained;
						height += el->m_margins.top;
					}
					if(el->css().get_margins().bottom.is_predefined())
					{
						el->m_margins.bottom = remained;
						height += el->m_margins.bottom;
					}
				}
			}
			el->m_pos.y = top + el->content_offset_top();
			if(el->m_pos.height != height - el->content_offset_height())
			{
				el->m_pos.height = height - el->content_offset_height();
				need_render = true;
			}

			// Calculate horizontal position
			pixel_t right = 0;
			pixel_t left = 0;
			pixel_t width = 0;
			// https://www.w3.org/TR/CSS22/visudet.html#abs-non-replaced-width
			// 10.3.7 Absolutely positioned, non-replaced elements
			if(css_left.is_predefined() && !css_right.is_predefined() && el_width.is_predefined())
			{
				// 1. 'left' and 'width' are 'auto' and 'right' is not 'auto', then the width is shrink-to-fit.
				// Then solve for 'left'
				if(el->css().get_margins().left.is_predefined()) el->m_margins.left = 0;
				if(el->css().get_margins().right.is_predefined()) el->m_margins.right = 0;
				width = el->width();
				right = css_right.calc_percent(containing_block_size.width);
				left = containing_block_size.width - width - right;
			} else if(css_left.is_predefined() && css_right.is_predefined() && !el_width.is_predefined())
			{
				// 2. 'left' and 'right' are 'auto' and 'width' is not 'auto', then if the 'direction' property of
				// the element establishing the static-position containing block is 'ltr' set 'left' to the
				// static position, otherwise set 'right' to the static position. Then solve for 'left'
				// (if 'direction is 'rtl') or 'right' (if 'direction' is 'ltr').
				if(el->css().get_margins().left.is_predefined()) el->m_margins.left = 0;
				if(el->css().get_margins().right.is_predefined()) el->m_margins.right = 0;
				left = el_static_x - el->content_offset_left();
				width = fix_width_min_max(el_width.calc_percent(containing_block_size.width));
			} else if(!css_left.is_predefined() && css_right.is_predefined() && el_width.is_predefined())
			{
				// 3. 'width' and 'right' are 'auto' and 'left' is not 'auto', then the width is shrink-to-fit .
				// Then solve for 'right'
				if(el->css().get_margins().left.is_predefined()) el->m_margins.left = 0;
				if(el->css().get_margins().right.is_predefined()) el->m_margins.right = 0;
				width = el->width();
				left = css_left.calc_percent(containing_block_size.width);
			} else if(css_left.is_predefined() && !css_right.is_predefined() && !el_width.is_predefined())
			{
				// 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve for 'left'
				if(el->css().get_margins().left.is_predefined()) el->m_margins.left = 0;
				if(el->css().get_margins().right.is_predefined()) el->m_margins.right = 0;
				right = css_right.calc_percent(containing_block_size.width);
				width = fix_width_min_max(el_width.calc_percent(containing_block_size.width));
				left = containing_block_size.width - right - width;
			} else if(!css_left.is_predefined() && !css_right.is_predefined() && el_width.is_predefined())
			{
				// 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve for 'width'
				if(el->css().get_margins().left.is_predefined()) el->m_margins.left = 0;
				if(el->css().get_margins().right.is_predefined()) el->m_margins.right = 0;
				left = css_left.calc_percent(containing_block_size.width);
				right = css_right.calc_percent(containing_block_size.width);
				if(el->src_el()->is_replaced())
				{
					width = el->width() - el->content_offset_width();
					pixel_t reminded = (containing_block_size.width - left - right) - width - el->content_offset_width();
					if(reminded != 0)
					{
						int divider = 0;
						if (el->css().get_margins().left.is_predefined()) divider++;
						if (el->css().get_margins().right.is_predefined()) divider++;
						if (divider != 0)
						{
							if (el->css().get_margins().left.is_predefined()) el->m_margins.left = reminded / divider;
							if (el->css().get_margins().right.is_predefined()) el->m_margins.right = reminded / divider;
						}
					}
					width += el->content_offset_width();
				} else
				{
					width = containing_block_size.width - left - right;
				}
				if(!el->css().get_max_width().is_predefined())
				{
					pixel_t max_width = el->css().get_max_width().calc_percent(containing_block_size.height);
					if(width - el->content_offset_width() > max_width)
					{
						pixel_t reminded = width - el->content_offset_width() - max_width;
						width = max_width;
						int divider = 0;
						if(el->css().get_margins().left.is_predefined()) divider++;
						if(el->css().get_margins().right.is_predefined()) divider++;
						if(divider != 0)
						{
							if(el->css().get_margins().left.is_predefined()) el->m_margins.left = reminded / divider;
							if(el->css().get_margins().right.is_predefined()) el->m_margins.right = reminded / divider;
						}
						width += el->content_offset_width();
					}
				}
			} else if(!css_left.is_predefined() && css_right.is_predefined() && !el_width.is_predefined())
			{
				// 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve for 'right'
				if(el->css().get_margins().left.is_predefined()) el->m_margins.left = 0;
				if(el->css().get_margins().right.is_predefined()) el->m_margins.right = 0;
				left = css_left.calc_percent(containing_block_size.width);
				width = fix_width_min_max(el_width.calc_percent(containing_block_size.width));
			} else if(css_left.is_predefined() && css_right.is_predefined() && el_width.is_predefined())
			{
				// If all three of 'left', 'width', and 'right' are 'auto': First set any 'auto' values for
				// 'margin-left' and 'margin-right' to 0. Then, if the 'direction' property of the element
				// establishing the static-position containing block is 'ltr' set 'left' to the static position
				// and apply rule number three below; otherwise, set 'right' to the static position and apply
				// rule number one below.
				if(el->css().get_margins().left.is_predefined()) el->m_margins.left = 0;
				if(el->css().get_margins().right.is_predefined()) el->m_margins.right = 0;
				width = el->width();
				left = el_static_x - el->content_offset_left();
			} else
			{
				// If none of the three is 'auto':
				width = fix_width_min_max(el_width.calc_percent(containing_block_size.width));
				left = css_left.calc_percent(containing_block_size.width);
				right = css_right.calc_percent(containing_block_size.width);
				pixel_t remained = containing_block_size.width - width - left - right;

				if(el->css().get_margins().left.is_predefined() && el->css().get_margins().right.is_predefined())
				{
					// If both 'margin-left' and 'margin-right' are 'auto', solve the equation under the extra
					// constraint that the two margins get equal values, unless this would make them negative,
					// in which case when direction of the containing block is 'ltr' ('rtl'), set 'margin-left'
					// ('margin-right') to zero and solve for 'margin-right' ('margin-left').
					el->m_margins.left = el->m_margins.right = remained / 2;
					if(el->m_margins.left < 0)
					{
						el->m_margins.left = 0;
						el->m_margins.right = remained;
					}
					width += el->m_margins.left + el->m_margins.right;
				} else
				{
					// If one of 'margin-left' or 'margin-right' is 'auto', solve the equation
					// for that value. If the values are over-constrained, ignore the value for 'left'
					// (in case the 'direction' property of the containing block is 'rtl') or 'right' (in case
					// 'direction' is 'ltr') and solve for that value.
					if(el->css().get_margins().left.is_predefined())
					{
						el->m_margins.left = remained;
						width += el->m_margins.left;
					}
					if(el->css().get_margins().right.is_predefined())
					{
						el->m_margins.right = remained;
						width += el->m_margins.right;
					}
				}
			}
			el->m_pos.x = left + el->content_offset_left();
			if(el->m_pos.width != width - el->content_offset_width())
			{
				el->m_pos.width = width - el->content_offset_width();
				need_render = true;
			}

            if(!el->is_viewport_fixed())
            {
				el->m_pos.x -= el_static_offset_x;
				el->m_pos.y -= el_static_offset_y;
            }

            if(need_render)
            {
                position pos = el->m_pos;
				if (positioned_ratio || containing_block_size.intrinsic_inline.preferred)
					el->render(el->left(), el->top(), containing_block_size, nullptr, true);
				else
					el->render(el->left(), el->top(), containing_block_size.new_width(el->width()), nullptr, true);
                el->m_pos = pos;
            }

            if(el->is_viewport_fixed())
            {
				position fixed_pos = el->pos();
				el->get_redraw_box(fixed_pos);
                src_el()->get_document()->add_fixed_box(fixed_pos);
            }
        }

		el->render_positioned();
    }

    if(!m_positioned.empty())
    {
        std::stable_sort(m_positioned.begin(), m_positioned.end(), [](const std::shared_ptr<render_item>& Left, const std::shared_ptr<render_item>& Right)
            {
                return (Left->paint_z_index() < Right->paint_z_index());
            });
    }
}

void litehtml::render_item::add_positioned(const std::shared_ptr<litehtml::render_item> &el)
{
    if (establishes_paint_context() || is_root())
    {
        m_positioned.push_back(el);
    } else
    {
        auto el_parent = parent();
        if (el_parent)
        {
            el_parent->add_positioned(el);
        }
    }
}

litehtml::position litehtml::render_item::overflow_clip_box(const position& content_box) const
{
	position box = content_box;
	box += m_padding;
	const bool clip_x = css().get_overflow_x() == overflow_clip;
	const bool clip_y = css().get_overflow_y() == overflow_clip;
	if (!clip_x && !clip_y) return box;

	const auto& margin = css().get_overflow_clip_margin();
	position origin = content_box;
	if (margin.box != background_box_content) origin += m_padding;
	if (margin.box == background_box_border) origin += m_borders;
	const pixel_t offset = std::max(0.0f, margin.offset.val());
	if (clip_x) {
		box.x = origin.x - offset;
		box.width = origin.width + 2 * offset;
	}
	if (clip_y) {
		box.y = origin.y - offset;
		box.height = origin.height + 2 * offset;
	}
	return box;
}

void litehtml::render_item::get_redraw_box(litehtml::position& pos, pixel_t x /*= 0*/, pixel_t y /*= 0*/)
{
    if(is_visible())
    {
        pixel_t p_left		= std::min(pos.left(),	x + m_pos.left() - m_padding.left - m_borders.left);
        pixel_t p_right		= std::max(pos.right(), x + m_pos.right() + m_padding.left + m_borders.left);
        pixel_t p_top		= std::min(pos.top(), y + m_pos.top() - m_padding.top - m_borders.top);
        pixel_t p_bottom	= std::max(pos.bottom(), y + m_pos.bottom() + m_padding.bottom + m_borders.bottom);

        pos.x = p_left;
        pos.y = p_top;
        pos.width	= p_right - p_left;
        pos.height	= p_bottom - p_top;

        if(!clips_overflow())
        {
            for(auto& el : m_children)
            {
                if(!el->is_viewport_fixed())
                {
                    el->get_redraw_box(pos, x + m_pos.x, y + m_pos.y);
                }
            }
        }
    }
}

void litehtml::render_item::calc_document_size(litehtml::size& sz, pixel_t x /*= 0*/, pixel_t y /*= 0*/)
{
	if (collapsed_table_track()) return;
	if(css().get_display() != display_inline && css().get_display() != display_table_row)
	{
		if(is_visible())
		{
			if(!is_viewport_fixed())
			{
				sz.width  = std::max(sz.width, x + right());
				sz.height = std::max(sz.height, y + bottom());
			}

			if (!clips_overflow() && !css().has_layout_containment())
			{
				m_scroll_view.reset();
				if (has_table_cell_clip()) return;
				const auto display = css().get_display();
				if (!is_viewport_fixed() && (display == display_table || is_grid_display(display)))
				{
					const auto extent = layout_overflow();
					sz.width = std::max(sz.width, x + m_pos.x + extent.width);
					sz.height = std::max(sz.height, y + m_pos.y + extent.height);
				}
				if (display != display_table)
					for (const auto& child : m_children) child->calc_document_size(sz, x + m_pos.x, y + m_pos.y);
				return;
			}

			size extent = layout_overflow();
			for (const auto& child : m_children) child->calc_document_size(extent, 0, 0);
			if (css().is_scroll_container())
			{
				if (src_el()->is_replaced())
				{
					if (css().get_overflow_x() == overflow_hidden) extent.width = m_pos.width;
					if (css().get_overflow_y() == overflow_hidden) extent.height = m_pos.height;
				}
				if (m_scroll_view) m_scroll_view->set(m_pos, extent);
				else m_scroll_view = std::make_shared<scroll_view>(m_pos, extent);
			}
			else
			{
				m_scroll_view.reset();
				if (has_table_cell_clip() || is_viewport_fixed() || css().has_layout_containment()) return;
				if (css().get_overflow_x() == overflow_visible)
					sz.width = std::max(sz.width, x + m_pos.x + extent.width);
				if (css().get_overflow_y() == overflow_visible)
					sz.height = std::max(sz.height, y + m_pos.y + extent.height);
			}
		}
	} else
	{
		position::vector boxes;
		get_inline_boxes(boxes);
		for(const auto& box : boxes)
		{
			sz.width  = std::max(sz.width, x + box.x + box.width);
			sz.height = std::max(sz.height, y + box.y + box.height);
		}
	}
}

void litehtml::render_item::draw_stacking_context( uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, bool with_positioned )
{
    if(!is_visible()) return;

    if (with_positioned)
    {
        for (const int level : m_stacking_levels)
        {
            if (level >= 0) break;
            draw_children(hdc, x, y, clip, draw_positioned, level);
        }
    }
    draw_children(hdc, x, y, clip, draw_block, 0);
    draw_children(hdc, x, y, clip, draw_floats, 0);
    draw_children(hdc, x, y, clip, draw_inlines, 0);
    if (with_positioned)
    {
        for (const int level : m_stacking_levels)
        {
            if (level >= 0) draw_children(hdc, x, y, clip, draw_positioned, level);
        }
    }
}

void litehtml::render_item::paint_positioned_child(render_item& child, uint_ptr hdc, pixel_t x, pixel_t y,
    const position* clip, int level)
{
    if (!child.is_visible()) return;
    if (child.establishes_stacking_context())
    {
        if (child.paint_z_index() == level)
        {
            const bool fixed = child.is_viewport_fixed();
            child.paint_group(hdc, fixed ? 0 : x, fixed ? 0 : y, clip, true);
        }
        return;
    }
    // An auto-level positioned box paints its own contents at zero. Its positioned descendants
    // still participate in this context, through the same coordinate and overflow clip chain.
    if (level == 0 && child.src_el()->is_positioned())
    {
        child.paint_group(hdc, x, y, clip, false);
    }
    child.draw_children(hdc, x, y, clip, draw_positioned, level);
}

void litehtml::render_item::draw_children(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, draw_flag flag, int zindex)
{
    position pos = m_pos;
    pos.x += x - get_scroll_left();
    pos.y += y - get_scroll_top();

    document::ptr doc = src_el()->get_document();

    const bool clipped = clips_overflow();
    if (clipped) doc->container()->set_overflow_clip(*this);

    for (const auto& el : m_children)
    {
        if (el->is_visible())
        {
            if (flag == draw_positioned)
            {
                paint_positioned_child(*el, hdc, pos.x, pos.y, clip, zindex);
                continue;
            }
            bool process = true;
            switch (flag)
            {
                case draw_block:
                    if (!el->src_el()->is_inline() && !el->is_grid_item() && el->src_el()->css().get_float() == float_none && !el->establishes_paint_context())
                    {
                        el->paint(hdc, pos.x, pos.y, clip);
                    }
                    break;
                case draw_floats:
                    if (!el->is_grid_item() && el->src_el()->css().get_float() != float_none && !el->establishes_paint_context())
                    {
                        el->paint_group(hdc, pos.x, pos.y, clip, false);
                        process = false;
                    }
                    break;
                case draw_inlines:
                    if ((el->is_grid_item() || (el->src_el()->is_inline() && el->src_el()->css().get_float() == float_none)) && !el->establishes_paint_context())
                    {
                        if (el->is_grid_item() || el->src_el()->css().get_display() == display_inline_grid ||
                                el->src_el()->css().get_display() == display_inline_block ||
                                el->src_el()->css().get_display() == display_inline_table || el->src_el()->css().get_display() == display_inline_flex)
                        {
                            el->paint_group(hdc, pos.x, pos.y, clip, false);
                            process = false;
                        }
                        else
                        {
                            el->paint(hdc, pos.x, pos.y, clip);
                        }
                    }
                    break;
                default:
                    break;
            }

            if (process && !el->is_grid_item() && el->src_el()->css().get_float() == float_none &&
                !is_one_of(el->css().get_display(), display_inline_grid, display_inline_block, display_inline_table, display_inline_flex) &&
                !el->establishes_paint_context())
            {
                el->draw_children(hdc, pos.x, pos.y, clip, flag, zindex);
            }
        }
    }

    if (flag == draw_inlines)
    {
        doc->container()->draw_text_overflow(*this);
        if (clipped) doc->container()->draw_scrollbars(*this);
    }

    if (clipped)
    {
        doc->container()->del_clip();
    }
}

std::shared_ptr<litehtml::element>  litehtml::render_item::get_child_by_point(pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y, draw_flag flag, int zindex, const std::function<bool(const std::shared_ptr<render_item>&)>& check)
{
    if (!check && clips_overflow())
    {
        const position box = overflow_clip_box(m_pos);
        if ((css().get_overflow_x() != overflow_visible && (x < box.left() || x >= box.right())) ||
            (css().get_overflow_y() != overflow_visible && (y < box.top() || y >= box.bottom()))) return nullptr;
    }

    element::ptr ret = nullptr;

    position el_pos = m_pos;
    el_pos.x	= x - el_pos.x + get_scroll_left();
    el_pos.y	= y - el_pos.y + get_scroll_top();

    for(auto i = m_children.crbegin(); i != m_children.crend() && !ret; std::advance(i, 1))
    {
        const auto& el = *i;

        const auto display = el->css().get_display();
        const bool row_wrapper = is_one_of(display, display_table_row, display_table_row_group,
            display_table_header_group, display_table_footer_group);
        if((el->is_visible() || (row_wrapper && el->css().get_visibility() == visibility_collapse)) &&
            display != display_inline_text)
        {
            if (flag == draw_positioned)
            {
                const bool context = el->establishes_stacking_context();
                if (!context)
                {
                    // At the same level, descendants were painted after the ancestor's own group.
                    ret = el->get_child_by_point(el_pos.x, el_pos.y, client_x, client_y, flag, zindex, check);
                    if (ret) return ret;
                }
                if (context ? el->paint_z_index() == zindex : zindex == 0 && el->src_el()->is_positioned())
                {
                    const bool fixed = el->is_viewport_fixed();
                    ret = el->get_element_by_point(fixed ? client_x : el_pos.x, fixed ? client_y : el_pos.y,
                        client_x, client_y, check, context);
                }
                continue;
            }
        	bool process = true;
            switch(flag)
            {
                case draw_block:
                    if(!el->src_el()->is_inline() && !el->is_grid_item() && el->src_el()->css().get_float() == float_none && !el->establishes_paint_context())
                    {
                        ret = el->get_child_by_point(el_pos.x, el_pos.y, client_x, client_y, flag, zindex, check);

                        if(!ret && el->is_visible() && el->is_point_inside(el_pos.x, el_pos.y))
                        {
							if (!check || check(el))
							{
								ret = el->src_el();
							}
                        }
                        process = false;
                    }
                    break;
                case draw_floats:
                    if(!el->is_grid_item() && el->src_el()->css().get_float() != float_none && !el->establishes_paint_context())
                    {
                        ret = el->get_element_by_point(el_pos.x, el_pos.y, client_x, client_y, check, false);

                        if(!ret && el->is_visible() && el->is_point_inside(el_pos.x, el_pos.y) && (!check || check(el)))
                        {
                            ret = el->src_el();
                        }
                        process = false;
                    }
                    break;
                case draw_inlines:
                    if((el->is_grid_item() || (el->src_el()->is_inline() && el->src_el()->css().get_float() == float_none)) && !el->establishes_paint_context())
                    {
                        if(el->is_grid_item() || el->src_el()->css().get_display() == display_inline_grid ||
                                el->src_el()->css().get_display() == display_inline_block ||
                                el->src_el()->css().get_display() == display_inline_table ||
                                el->src_el()->css().get_display() == display_inline_flex)
                        {
                            ret = el->get_element_by_point(el_pos.x, el_pos.y, client_x, client_y, check, false);
                            process = false;
                        }
                        if(!ret && el->is_visible() && el->is_point_inside(el_pos.x, el_pos.y))
                        {
                        	if (!check || check(el))
                        	{
                        		ret = el->src_el();
                        	}
                        }
                    }
                    break;
                default:
                    break;
            }

            if (process && !el->establishes_paint_context() && !el->is_grid_item() &&
                el->src_el()->css().get_float() == float_none &&
                !is_one_of(display, display_inline_grid, display_inline_block, display_inline_table, display_inline_flex))
            {
                if (auto child = el->get_child_by_point(el_pos.x, el_pos.y, client_x, client_y, flag, zindex, check))
                {
                    if (!check)
                    {
                        ret = child;
                    }
                    else
                    {
                        child->run_on_renderers([&](const std::shared_ptr<render_item>& ri) {
                            if (!check(ri)) return true;
                            ret = child;
                            return false;
                        });
                    }
                }
            }
        }
    }

    return ret;
}

std::shared_ptr<litehtml::element> litehtml::render_item::get_element_by_point(pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y,
    const std::function<bool(const std::shared_ptr<render_item>&)>& check, bool with_positioned)
{
    if(!is_visible()) return nullptr;

    element::ptr ret;

    if (with_positioned)
    {
        for (auto level = m_stacking_levels.rbegin(); level != m_stacking_levels.rend() && *level >= 0; ++level)
        {
            if (auto hit = get_child_by_point(x, y, client_x, client_y, draw_positioned, *level, check)) return hit;
        }
    }

    ret = get_child_by_point(x, y, client_x, client_y, draw_inlines, 0, check);
    if(ret) return ret;

    ret = get_child_by_point(x, y, client_x, client_y, draw_floats, 0, check);
    if(ret) return ret;

    ret = get_child_by_point(x, y, client_x, client_y, draw_block, 0,check);
    if(ret) return ret;


    if (with_positioned)
    {
        for (auto level = m_stacking_levels.rbegin(); level != m_stacking_levels.rend(); ++level)
        {
            if (*level >= 0) continue;
            if (auto hit = get_child_by_point(x, y, client_x, client_y, draw_positioned, *level, check)) return hit;
        }
    }

    if(is_viewport_fixed())
    {
        if(is_point_inside(client_x, client_y))
        {
			if (!check || check(this->shared_from_this()))
			{
				ret = src_el();
			}

        }
    } else
    {
        if(is_point_inside(x, y))
        {
			if (!check || check(this->shared_from_this()))
			{
				ret = src_el();
			}
        }
    }

    return ret;
}

bool litehtml::render_item::is_point_inside( pixel_t x, pixel_t y ) const
{
	if(src_el()->css().get_display() != display_inline && src_el()->css().get_display() != display_table_row)
	{
		position pos = m_pos;
		pos += m_padding;
		pos += m_borders;
		return pos.is_point_inside(x, y);
	} else
	{
		position::vector boxes;
		get_inline_boxes(boxes);
		for(auto & box : boxes)
		{
			if(box.is_point_inside(x, y))
			{
				return true;
			}
		}
	}
    return false;
}

void litehtml::render_item::get_rendering_boxes( position::vector& redraw_boxes) const
{
    const auto display = css().get_display();
    const bool painted_row_group = (css().get_position() == element_position_sticky || css().has_group_effects()) &&
        is_one_of(display, display_table_row_group, display_table_header_group, display_table_footer_group);
    if(display == display_inline || display == display_table_row || painted_row_group)
    {
        get_inline_boxes(redraw_boxes);
		for(auto& box : redraw_boxes)
		{
			scroll_box(box);
		}
    } else
    {
        position pos = m_pos;
        pos 	+= m_padding;
        pos 	+= m_borders;
        redraw_boxes.push_back(pos);
    }

    if(!is_viewport_fixed())
    {
		auto cur_el = parent();
		pixel_t	 add_x	= 0;
		pixel_t	 add_y	= 0;

        while(cur_el)
		{
			if(cur_el->is_viewport_fixed())
			{
				position view_port;
				src_el()->get_document()->container()->get_viewport(view_port);
				add_x += cur_el->m_pos.x + view_port.left() - cur_el->get_scroll_left();
				add_y += cur_el->m_pos.y + view_port.top() - cur_el->get_scroll_top();
				break;
			}
            add_x += cur_el->m_pos.x - cur_el->get_scroll_left();
            add_y += cur_el->m_pos.y - cur_el->get_scroll_top();
            cur_el = cur_el->parent();
        }
		for(auto& box : redraw_boxes)
		{
			box.x += add_x;
			box.y += add_y;
		}
	} else
	{
		position view_port;
		src_el()->get_document()->container()->get_viewport(view_port);
		for(auto& box : redraw_boxes)
		{
			box.x += view_port.left();
			box.y += view_port.top();
		}
	}
}

void litehtml::render_item::dump(litehtml::dumper& cout)
{
    cout.begin_node(src_el()->dump_get_name() + "{" + string(typeid(*this).name()) + "}");

    auto attrs = src_el()->dump_get_attrs();
    if(!attrs.empty())
    {
        cout.begin_attrs_group("attributes");
        for (const auto &attr: attrs)
        {
            cout.add_attr(std::get<0>(attr), std::get<1>(attr));
        }
        cout.end_attrs_group();
    }

    if(!m_children.empty())
    {
        cout.begin_attrs_group("children");
        for (const auto &el: m_children)
        {
            el->dump(cout);
        }
        cout.end_attrs_group();
    }

    cout.end_node();
}

litehtml::position litehtml::render_item::get_placement() const
{
	litehtml::position pos = m_pos;
	if(is_viewport_fixed())
	{
		position view_port;
		src_el()->get_document()->container()->get_viewport(view_port);
		pos.x += view_port.left();
		pos.y += view_port.top();
		return pos;
	}
	auto cur_el = parent();
	while(cur_el)
	{
		auto p = cur_el->calc_placement();
		pos.x += p.x;
		pos.y += p.y;

		if(cur_el->is_viewport_fixed())
		{
			position view_port;
			src_el()->get_document()->container()->get_viewport(view_port);
			pos.x += view_port.left();
			pos.y += view_port.top();
			return pos;
		}

		cur_el = cur_el->parent();
	}
	return pos;
}

std::shared_ptr<litehtml::render_item> litehtml::render_item::init()
{
    src_el()->add_render(shared_from_this());

    for(auto& el : children())
    {
        el = el->init();
    }

    return shared_from_this();
}

void litehtml::render_item::calc_cb_length(const css_length& len, containing_block_context::typed_pixel percent_base, containing_block_context::typed_pixel& out_value) const
{
	if (!len.is_predefined())
	{
		const bool grid_sizing = is_grid_display(css().get_display()) || is_grid_item();
		if ((len.expression() || grid_sizing) && len.units() == css_units_percentage &&
			is_one_of(percent_base.type, containing_block_context::cbc_value_type_auto, containing_block_context::cbc_value_type_none))
			return;
		if(len.units() == litehtml::css_units_percentage)
		{
			out_value.value = len.calc_percent(percent_base);
			out_value.type = (len.expression() || grid_sizing) ? containing_block_context::cbc_value_type_absolute : containing_block_context::cbc_value_type_percentage;
		} else
		{
			out_value.value = src_el()->get_document()->to_pixels(len, css().get_font_metrics(), 0);
			out_value.type = containing_block_context::cbc_value_type_absolute;
		}
	}
}

litehtml::containing_block_context litehtml::render_item::calculate_containing_block_context(const containing_block_context& cb_context)
{
	containing_block_context ret;
	ret.context_idx = cb_context.context_idx + 1;
	ret.width.value = ret.max_width.value = cb_context.width.value - content_offset_width();
	if(src_el()->css().get_position() != element_position_absolute && css().get_position() != element_position_fixed)
	{
		ret.height.value = cb_context.height.value - content_offset_height();
	}

	const bool intrinsic_probe = cb_context.size_mode & containing_block_context::size_mode_intrinsic_inline_probe;
	const bool uses_ratio = has_preferred_aspect_ratio();
	const auto calc_block_length = [&](const css_length& length, containing_block_context::typed_pixel& value) {
		if (cb_context.contained_auto_block_size && cb_context.height.type == containing_block_context::cbc_value_type_auto &&
			length.units() == css_units_percentage && src_el()->in_normal_flow()) return;
		if (!uses_ratio || length.units() != css_units_percentage ||
			!is_one_of(cb_context.height.type, containing_block_context::cbc_value_type_auto, containing_block_context::cbc_value_type_none))
			calc_cb_length(length, cb_context.height, value);
	};
	auto width_basis = cb_context.width;
	if (!(cb_context.size_mode & containing_block_context::size_mode_content))
		width_basis.type = containing_block_context::cbc_value_type_absolute;

	const auto calc_inline_length = [&](const css_length& length, containing_block_context::typed_pixel& value, bool minimum = false) {
		if (!cb_context.intrinsic_inline_basis || length.units() != css_units_percentage)
			calc_cb_length(length, width_basis, value);
		else if (minimum)
			value = { length.calc_percent(0), containing_block_context::cbc_value_type_absolute };
	};

	const bool grid_width = cb_context.size_mode & containing_block_context::size_mode_grid_assigned_width;
	const bool stretch_height = cb_context.size_mode & containing_block_context::size_mode_grid_stretch_height;
	// Calculate width if css property is not auto
	// We have to use aut value for display_table_cell also.
	if (src_el()->css().get_display() != display_table_cell)
	{
		auto par = parent();
		if((cb_context.size_mode & containing_block_context::size_mode_exact_width) || grid_width)
		{
			ret.width.value = grid_width ? std::max<pixel_t>(0, cb_context.render_width - render_offset_width()) : cb_context.width;
			ret.width.type = containing_block_context::cbc_value_type_absolute;
		} else
		{
			auto *width = &css().get_width();
			if(par && (par->css().get_display() == display_flex || par->css().get_display() == display_inline_flex))
			{
				if(!css().get_flex_basis().is_predefined() && css().get_flex_basis().val() >= 0)
				{
					if(par->css().get_flex_direction() == flex_direction_row || par->css().get_flex_direction() == flex_direction_row_reverse)
					{
						ret.width.type = containing_block_context::cbc_value_type_auto;
						ret.width.value = 0;
						width = nullptr;
					}
				}
			}
			if(width && !intrinsic_probe)
			{
				calc_inline_length(*width, ret.width);
			}
		}
		if (cb_context.positioned_height)
		{
			ret.height = { *cb_context.positioned_height + box_sizing_height(), containing_block_context::cbc_value_type_absolute };
		}
		else if((cb_context.size_mode & containing_block_context::size_mode_exact_height) || stretch_height)
		{
			ret.height.value = stretch_height ? std::max<pixel_t>(0, cb_context.height - render_offset_height()) : cb_context.height;
			ret.height.type = containing_block_context::cbc_value_type_absolute;
		} else
		{
			auto *height = &css().get_height();
			if(par && (par->css().get_display() == display_flex || par->css().get_display() == display_inline_flex))
			{
				if(!css().get_flex_basis().is_predefined() && css().get_flex_basis().val() >= 0)
				{
					if(par->css().get_flex_direction() == flex_direction_column || par->css().get_flex_direction() == flex_direction_column_reverse)
					{
						ret.height.type = containing_block_context::cbc_value_type_auto;
						ret.height.value = 0;
						height = nullptr;
					}
				}
			}
			if(height)
			{
				calc_block_length(*height, ret.height);
			}
		}
		if (!grid_width && ret.width.type != containing_block_context::cbc_value_type_auto && (src_el()->css().get_display() == display_table || src_el()->is_root()))
		{
			ret.width.value -= content_offset_width();
		}
		if (!cb_context.positioned_height && !stretch_height && ret.height.type != containing_block_context::cbc_value_type_auto && (src_el()->css().get_display() == display_table || src_el()->is_root()))
		{
			ret.height.value -= content_offset_height();
		}
	}
	ret.render_width = ret.width;

	if (!intrinsic_probe)
	{
		calc_inline_length(css().get_min_width(), ret.min_width, true);
		calc_inline_length(css().get_max_width(), ret.max_width);
	}

	calc_block_length(css().get_min_height(), ret.min_height);
	calc_block_length(css().get_max_height(), ret.max_height);

	// An intrinsic probe gives descendants room for unwrapped content.
	// Its width becomes a percentage basis only inside a definitively sized box.
	ret.grid_intrinsic_width = ret.width.type != containing_block_context::cbc_value_type_absolute &&
		(cb_context.grid_intrinsic_width || (cb_context.size_mode & containing_block_context::size_mode_grid_measure_width));
	ret.grid_content_edge = cb_context.grid_content_edge;
	if (cb_context.grid_baseline_padding > 0 && css().get_box_sizing() == box_sizing_content_box)
	{
		if (!stretch_height && ret.height.type != containing_block_context::cbc_value_type_auto)
			ret.height.value = std::max<pixel_t>(0, ret.height.value - cb_context.grid_baseline_padding);
		if (ret.min_height.type != containing_block_context::cbc_value_type_none)
			ret.min_height.value = std::max<pixel_t>(0, ret.min_height.value - cb_context.grid_baseline_padding);
		if (ret.max_height.type != containing_block_context::cbc_value_type_none)
			ret.max_height.value = std::max<pixel_t>(0, ret.max_height.value - cb_context.grid_baseline_padding);
	}
	// Fix box sizing
	if(ret.width.type != containing_block_context::cbc_value_type_auto)
	{
		ret.render_width = ret.width - box_sizing_width();
	}
	if(ret.min_width.type != containing_block_context::cbc_value_type_none)
	{
		ret.min_width.value -= box_sizing_width();
	}
	if(ret.max_width.type != containing_block_context::cbc_value_type_none)
	{
		ret.max_width.value -= box_sizing_width();
	}
	if(ret.min_height.type != containing_block_context::cbc_value_type_none)
	{
		ret.min_height.value -= box_sizing_height();
	}
	if(ret.max_height.type != containing_block_context::cbc_value_type_none)
	{
		ret.max_height.value -= box_sizing_height();
	}

	// Intrinsic widths have already been expressed as content-box lengths.
	const auto& intrinsic = cb_context.intrinsic_inline;
	using mode = containing_block_context;
	if (intrinsic.preferred)
	{
		ret.width = { *intrinsic.preferred + box_sizing_width(), mode::cbc_value_type_absolute };
		ret.render_width = { *intrinsic.preferred, mode::cbc_value_type_absolute };
		ret.grid_intrinsic_width = false;
	}
	if (intrinsic.minimum) ret.min_width = { *intrinsic.minimum, mode::cbc_value_type_absolute };
	if (intrinsic.maximum) ret.max_width = { *intrinsic.maximum, mode::cbc_value_type_absolute };
	ret.intrinsic_inline_basis = (intrinsic_probe || cb_context.intrinsic_inline_basis) &&
		ret.width.type != mode::cbc_value_type_absolute;

	// Issue #459: a flex item is rendered to its resolved main size via
	// size_mode_exact_width. For a row-direction flex container the flex algorithm
	// has already clamped the item's main size by its min/max-width, resolved
	// against the flex container's width (see
	// flex_item_row_direction::direction_specific_init). Re-applying a *percentage*
	// min/max-width here would resolve it against the item's own exact size
	// (cb_context.width == the assigned main size) instead of the container, and
	// because the item is rendered more than once (flex_line::init, then
	// align_stretch) the error compounds and the item collapses towards its content
	// width. So skip the width clamp on that exact-width pass.
	if (cb_context.size_mode & containing_block_context::size_mode_exact_width)
	{
		auto par = parent();
		if (par && (par->css().get_display() == display_flex || par->css().get_display() == display_inline_flex)
			&& (par->css().get_flex_direction() == flex_direction_row
				|| par->css().get_flex_direction() == flex_direction_row_reverse))
		{
			ret.min_width.type = containing_block_context::cbc_value_type_none;
			ret.max_width.type = containing_block_context::cbc_value_type_none;
		}
	}

	if (uses_ratio && !src_el()->is_replaced() && !intrinsic_probe)
	{
		const auto ratio = css().get_aspect_ratio();
		const bool auto_width = ret.width.type == mode::cbc_value_type_auto;
		const bool auto_height = ret.height.type == mode::cbc_value_type_auto;
		const auto transfer_bounds = [&](mode::typed_pixel lower, mode::typed_pixel upper,
			mode::typed_pixel& destination_min, mode::typed_pixel& destination_max, bool to_width) {
			if (lower.type != mode::cbc_value_type_none)
			{
				auto value = size_from_aspect_ratio(lower.value, ratio, to_width);
				if (destination_max.type != mode::cbc_value_type_none) value = std::min(value, destination_max.value);
				destination_min = { std::max(destination_min.value, value), mode::cbc_value_type_absolute };
			}
			if (upper.type != mode::cbc_value_type_none)
			{
				auto value = size_from_aspect_ratio(upper.value, ratio, to_width);
				if (destination_min.type != mode::cbc_value_type_none) value = std::max(value, destination_min.value);
				if (destination_max.type != mode::cbc_value_type_none) value = std::min(value, destination_max.value);
				destination_max = { value, mode::cbc_value_type_absolute };
			}
		};
		const auto clamp_size = [](pixel_t value, mode::typed_pixel lower, mode::typed_pixel upper) {
			if (upper.type != mode::cbc_value_type_none) value = std::min(value, upper.value);
			if (lower.type != mode::cbc_value_type_none) value = std::max(value, lower.value);
			return std::max<pixel_t>(0, value);
		};
		// Transfer constraints from the original values, so a bound does not feed itself back through the ratio.
		const auto min_width = ret.min_width;
		const auto max_width = ret.max_width;
		if (auto_width) transfer_bounds(ret.min_height, ret.max_height, ret.min_width, ret.max_width, true);
		if (auto_height) transfer_bounds(min_width, max_width, ret.min_height, ret.max_height, false);
		if (auto_width && !auto_height)
		{
			const auto height = clamp_size(ret.height.value - box_sizing_height(), ret.min_height, ret.max_height);
			const auto width = size_from_aspect_ratio(height, ratio, true);
			ret.width = { width + box_sizing_width(), mode::cbc_value_type_absolute };
			ret.render_width = { width, mode::cbc_value_type_absolute };
			ret.ratio_width = true;
			ret.grid_intrinsic_width = ret.intrinsic_inline_basis = false;
		}
		if (auto_height)
		{
			ret.ratio_height = true;
			const bool fills_width = src_el()->in_normal_flow() && css().get_float() == float_none &&
				!(cb_context.size_mode & mode::size_mode_content) &&
				is_one_of(css().get_display(), display_block, display_list_item, display_flex, display_grid);
			if (!auto_width || fills_width)
			{
				const auto width = clamp_size(ret.render_width.value, ret.min_width, ret.max_width);
				const auto height = clamp_size(size_from_aspect_ratio(width, ratio, false), ret.min_height, ret.max_height);
				ret.height = { height + box_sizing_height(), mode::cbc_value_type_absolute };
			}
		}
	}

	return ret;
}

std::tuple<litehtml::pixel_t, litehtml::pixel_t> litehtml::render_item::element_static_offset(const std::shared_ptr<litehtml::render_item>& el)
{
	pixel_t offset_x = 0;
	pixel_t offset_y = 0;
	auto cur_el = el->parent();
	auto this_el = !el->is_viewport_fixed() ? shared_from_this() : src_el()->get_document()->root_render();
	while(cur_el && cur_el != this_el)
	{
		offset_x += cur_el->m_pos.x;
		offset_y += cur_el->m_pos.y;
		cur_el = cur_el->parent();
	}

	if(el->is_viewport_fixed() || (is_root() && !establishes_positioned_containing_block()))
	{
		offset_x += this_el->m_pos.x;
		offset_y += this_el->m_pos.y;
	} else
	{
		offset_x += m_padding.left;
		offset_y += m_padding.top;
	}

	return {offset_x, offset_y};
}

void litehtml::render_item::y_shift(pixel_t delta)
{
	m_pos.y += delta;
}
