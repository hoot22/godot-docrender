#include "render_table.h"
#include "document_container.h"
#include "document.h"
#include "formats/html/elements/html_table_attributes.h"

#include <limits>
#include <set>
#include <stdexcept>


litehtml::render_item_table::render_item_table(std::shared_ptr<element> _src_el) :
        render_item(std::move(_src_el)),
        m_border_spacing_x(0),
        m_border_spacing_y(0)
{
}

litehtml::rendered_width litehtml::render_item_table::_render(pixel_t x, pixel_t y,
															  const containing_block_context& containing_block_size,
															  formatting_context* fmt_ctx, bool /*second_pass*/)
{
	if(!m_grid)
		return {0, 0};

    collect_collapsed_tracks();
    const bool separated = css().get_border_collapse() == border_collapse_separate;
    m_border_spacing_x = separated ? css().get_border_spacing_x().calc_percent(0) : 0;
    m_border_spacing_y = separated ? css().get_border_spacing_y().calc_percent(0) : 0;
    if (!separated) resolve_shared_borders();
    containing_block_context self_size = calculate_containing_block_context(containing_block_size);

    const pixel_t table_width_spacing = separated ? m_border_spacing_x * (m_grid->cols_count() + 1) : 0;
    const pixel_t available_width = std::max<pixel_t>(0, self_size.render_width - table_width_spacing);
    const bool fixed_layout = css().uses_fixed_table_layout() &&
        !css().get_width().is_predefined() && self_size.width.type != containing_block_context::cbc_value_type_auto;
    pixel_t table_width = 0;
    pixel_t min_table_width = 0;
    pixel_t max_table_width = 0;
    int first_row = 0;
    if (fixed_layout)
    {
        first_row = static_cast<int>(std::find(m_collapsed_rows.begin(), m_collapsed_rows.end(), false) - m_collapsed_rows.begin());
        table_width = min_table_width = max_table_width = resolve_fixed_columns(available_width, first_row);
    }
    else
    {
        // Calculate the minimum content width (MCW) of each cell: the formatted content may span any number of lines but may not overflow the cell box.
        // If the specified 'width' (W) of the cell is greater than MCW, W is the minimum cell width. A value of 'auto' means that MCW is the minimum
        // cell width.
        //
        // Also, calculate the "maximum" cell width of each cell: formatting the content without breaking lines other than where explicit line breaks occur.

        for (int col = 0; col < m_grid->cols_count(); ++col)
        {
            m_grid->column(col).min_width = 0;
            m_grid->column(col).max_width = 0;
        }
        const bool intrinsic_probe = (containing_block_size.size_mode & containing_block_context::size_mode_intrinsic_inline_probe) != 0;
        const bool single_sized_column = m_grid->cols_count() == 1 && self_size.width.type != containing_block_context::cbc_value_type_auto;
        for (int row = 0; row < m_grid->rows_count(); ++row)
        {
            for (auto& cell : m_grid->cells(row))
            {
                auto& column = m_grid->column(cell.column);
                if (single_sized_column)
                {
                    const auto measured = cell.el->render(0, 0, self_size.new_width(available_width), fmt_ctx);
                    cell.min_width = measured.min_width;
                    cell.max_width = measured.natural_width;
                }
                else if (cell.colspan == 1 && !column.css_width.is_predefined() &&
                    column.css_width.units() != css_units_percentage)
                {
                    const pixel_t width = column.css_width.calc_percent(self_size.width);
                    const auto measured = cell.el->render(0, 0, self_size.new_width(width), fmt_ctx);
                    cell.min_width = cell.max_width = std::max(width, measured.natural_width);
                }
                else
                {
                    cell.min_width = cell.el->render(0, 0, self_size.new_width(cell.el->content_offset_width()), fmt_ctx).natural_width;
                    cell.max_width = cell.el->render(0, 0, self_size.new_width(available_width), fmt_ctx).natural_width;
                }
                if (cell.colspan == 1)
                {
                    column.min_width = std::max(column.min_width, cell.min_width);
                    column.max_width = std::max(column.max_width, cell.max_width);
                }
            }
        }

        apply_column_widths(available_width, intrinsic_probe);
        for (int row = 0; row < m_grid->rows_count(); ++row)
        {
            for (const auto& cell : m_grid->cells(row))
            {
                if (cell.colspan == 1) continue;
                const int end = cell.column + cell.colspan;
                pixel_t minimum = 0;
                pixel_t maximum = 0;
                for (int col = cell.column; col < end; ++col)
                {
                    if (col > cell.column)
                    {
                        const pixel_t gap = m_border_spacing_x;
                        minimum += gap;
                        maximum += gap;
                    }
                    minimum += m_grid->column(col).min_width;
                    maximum += m_grid->column(col).max_width;
                }
                if (minimum < cell.min_width)
                    m_grid->distribute_min_width(cell.min_width - minimum, cell.column, end - 1);
                if (maximum < cell.max_width)
                    m_grid->distribute_max_width(cell.max_width - maximum, cell.column, end - 1);
                for (int col = cell.column; col < end; ++col)
                    m_grid->column(col).max_width = std::max(m_grid->column(col).min_width, m_grid->column(col).max_width);
            }
        }

        // If the 'table' or 'inline-table' element's 'width' property has a computed value (W) other than 'auto', the used width is the
        // greater of W, CAPMIN, and the minimum width required by all the columns plus cell spacing or borders (MIN).
        // If the used width is greater than MIN, the extra width should be distributed over the columns.
        //
        // If the 'table' or 'inline-table' element has 'width: auto', the used width is the greater of the table's containing block width,
        // CAPMIN, and MIN. However, if either CAPMIN or the maximum width required by the columns plus cell spacing or borders (MAX) is
        // less than that of the containing block, use max(MAX, CAPMIN).


        if (self_size.width.type == containing_block_context::cbc_value_type_absolute)
        {
            table_width = m_grid->calc_table_width(self_size.render_width - table_width_spacing, false, min_table_width, max_table_width);
        }
        else
        {
            table_width = m_grid->calc_table_width(self_size.render_width - table_width_spacing, self_size.width.type == containing_block_context::cbc_value_type_auto, min_table_width, max_table_width);
        }

    }
    table_width += table_width_spacing;
    const auto& intrinsic = containing_block_size.intrinsic_inline;
    if (intrinsic.minimum || intrinsic.maximum)
    {
        pixel_t target = table_width;
        if (self_size.max_width.type != containing_block_context::cbc_value_type_none)
            target = std::min(target, self_size.max_width.value);
        if (self_size.min_width.type != containing_block_context::cbc_value_type_none)
            target = std::max(target, self_size.min_width.value);
        if (target != table_width)
        {
            if (fixed_layout)
                table_width = resolve_fixed_columns(std::max<pixel_t>(0, target - table_width_spacing), first_row) + table_width_spacing;
            else table_width = m_grid->calc_table_width(target - table_width_spacing, false,
                min_table_width, max_table_width) + table_width_spacing;
        }
    }
    min_table_width += table_width_spacing;
    max_table_width += table_width_spacing;
    m_grid->calc_horizontal_positions(m_border_spacing_x);

    std::vector<int> covered_rows(m_grid->rows_count() + 1);
    std::vector<int> shown_rows(m_grid->rows_count() + 1);

    // Measure heights at the final column widths before distributing spanning contributions.
    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        auto& track = m_grid->row(row);
        track.height = !track.css_height.is_predefined() && track.css_height.units() != css_units_percentage ?
            std::max<pixel_t>(0, track.css_height.calc_percent(0)) : 0;
        for (auto& cell : m_grid->cells(row))
        {
            const auto& first = m_grid->column(cell.column);
            const auto& last = m_grid->column(cell.column + cell.colspan - 1);
            const pixel_t width = last.right - first.left;
            cell.el->set_table_track_geometry(false);
            cell.el->render(first.left, 0, self_size.new_width(width), fmt_ctx, true);
            cell.el->pos().width = std::max<pixel_t>(0, width - cell.el->content_offset_width());
            const bool hide = separated && cell.el->css().hides_empty_cells() &&
                (cell.el->css().get_visibility() != visibility_visible ||
                    std::none_of(cell.el->children().begin(), cell.el->children().end(), [](const auto& child) {
                        const auto& child_style = child->css();
                        if (!child->is_visible() || is_one_of(child_style.get_position(),
                            element_position_absolute, element_position_fixed)) return false;
                        return !child->src_el()->is_white_space() || !child_style.collapses_white_space();
                    }));
            cell.el->set_empty_table_cell(hide);
            ++covered_rows[row];
            --covered_rows[row + cell.rowspan];
            if (!hide)
            {
                ++shown_rows[row];
                --shown_rows[row + cell.rowspan];
            }
            cell.min_height = cell.el->height();
            const auto& height = cell.el->css().get_height();
            if (!height.is_predefined() && height.units() != css_units_percentage)
            {
                const pixel_t extras = cell.el->height() - cell.el->pos().height - cell.el->box_sizing_height();
                cell.min_height = std::max(cell.min_height, height.calc_percent(0) + extras);
            }
            if (cell.rowspan == 1)
                track.height = std::max(track.height, cell.min_height);
        }
    }
    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        for (const auto& cell : m_grid->cells(row))
        {
            if (cell.rowspan == 1) continue;
            const int end = row + cell.rowspan;
            pixel_t height = m_grid->row(row).height;
            for (int next = row + 1; next < end; ++next)
            {
                height += m_border_spacing_y + m_grid->row(next).height;
            }
            m_grid->row(end - 1).height += std::max<pixel_t>(0, cell.min_height - height);
        }
    }

    const pixel_t table_height_spacing = separated ? m_border_spacing_y * (m_grid->rows_count() + 1) : 0;

    // calculate block height
    pixel_t block_height = 0;
	if (self_size.ratio_height)
	{
		block_height = size_from_aspect_ratio(table_width, css().get_aspect_ratio(), false);
		if (self_size.max_height.type != containing_block_context::cbc_value_type_none)
			block_height = std::min(block_height, self_size.max_height.value);
		if (self_size.min_height.type != containing_block_context::cbc_value_type_none)
			block_height = std::max(block_height, self_size.min_height.value);
	}
	else if(self_size.height.type != containing_block_context::cbc_value_type_auto && self_size.height > 0)
	{
		block_height = self_size.height -
			((containing_block_size.size_mode & containing_block_context::size_mode_grid_stretch_height ||
			  containing_block_size.grid_content_edge != containing_block_context::content_baseline_edge::none) ?
			 box_sizing_height() : m_padding.height() + m_borders.height());
	}

    // calculate minimum height from m_css.get_min_height()
    pixel_t min_height = 0;
    if (!src_el()->css().get_min_height().is_predefined() && src_el()->css().get_min_height().units() == css_units_percentage)
    {
		min_height = src_el()->css().get_min_height().calc_percent(containing_block_size.height);
    }
    else
    {
        min_height = (pixel_t)src_el()->css().get_min_height().val();
    }

    if (containing_block_size.grid_content_edge != containing_block_context::content_baseline_edge::none)
        min_height = std::max<pixel_t>(0, self_size.min_height);
    pixel_t minimum_table_height = std::max(block_height, min_height);

    m_grid->calc_rows_height(minimum_table_height - table_height_spacing, m_border_spacing_y);
    m_grid->calc_vertical_positions(m_border_spacing_y);

    pixel_t table_height = m_grid->rows_count() ? m_grid->row(m_grid->rows_count() - 1).bottom : 0;

    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        for (const auto& cell : m_grid->cells(row))
        {
            const auto& first = m_grid->row(row);
            const auto& last = m_grid->row(row + cell.rowspan - 1);
            cell.el->pos().y = first.top + cell.el->content_offset_top();
            cell.el->pos().height = std::max<pixel_t>(0, last.bottom - first.top -
                cell.el->content_offset_top() - cell.el->content_offset_bottom());
            cell.el->apply_vertical_align();
        }
    }

    table_height += m_border_spacing_y;

    int covered = 0, shown = 0;
    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        covered += covered_rows[row];
        shown += shown_rows[row];
        if (covered > 0 && shown == 0) m_collapsed_rows[row] = true;
    }

    // Measure at uncollapsed track sizes, then compress positions without reflowing cell contents.
    const auto compress = [](int count, const std::vector<bool>& collapsed, pixel_t gap, auto bounds) {
        pixel_t removed = 0;
        for (int i = 0; i < count; ++i)
        {
            auto [begin, end] = bounds(i);
            begin -= removed;
            end -= removed;
            if (collapsed[i])
            {
                removed += end - begin + gap;
                end = begin;
            }
        }
        return removed;
    };
    const pixel_t removed_width = compress(m_grid->cols_count(), m_collapsed_columns, m_border_spacing_x,
        [&](int i) { auto& col = m_grid->column(i); return std::tie(col.left, col.right); });
    table_width = std::max<pixel_t>(0, table_width - removed_width);
    min_table_width = std::max<pixel_t>(0, min_table_width - removed_width);
    max_table_width = std::max<pixel_t>(0, max_table_width - removed_width);
    table_height = std::max<pixel_t>(0, table_height - compress(m_grid->rows_count(), m_collapsed_rows,
        m_border_spacing_y, [&](int i) { auto& row = m_grid->row(i); return std::tie(row.top, row.bottom); }));

    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        for (auto& cell : m_grid->cells(row))
        {
            const auto column_end = cell.column + cell.colspan;
            const auto row_end = row + cell.rowspan;
            const int visible_columns = m_column_lines[column_end] - m_column_lines[cell.column];
            const int visible_rows = m_row_lines[row_end] - m_row_lines[row];
            const bool clipped = visible_columns != cell.colspan || visible_rows != cell.rowspan;
            const bool collapsed = visible_columns == 0 || visible_rows == 0;
            cell.el->set_table_track_geometry(collapsed, clipped);
            auto& pos = cell.el->pos();
            const auto& first_col = m_grid->column(cell.column);
            const auto& first_row = m_grid->row(row);
            pos.x = first_col.left + cell.el->content_offset_left();
            pos.y = first_row.top + cell.el->content_offset_top();
            // End coordinates of trailing collapsed tracks can still include the gap of the preceding track.
            int last_col = column_end - 1;
            while (last_col > cell.column && m_collapsed_columns[last_col]) --last_col;
            int last_row = row_end - 1;
            while (last_row > row && m_collapsed_rows[last_row]) --last_row;
            pos.width = std::max<pixel_t>(0, m_grid->column(last_col).right - first_col.left - cell.el->content_offset_width());
            pos.height = std::max<pixel_t>(0, m_grid->row(last_row).bottom - first_row.top -
                cell.el->content_offset_top() - cell.el->content_offset_bottom());
        }
    }

    // Render table captions
    // Table border doesn't round the caption, so we have to start caption in the border position
    pixel_t top_captions = -border_top();

    for (auto& caption : m_grid->captions())
    {
		if(caption->css().get_caption_side() == caption_side_top)
		{
			caption->render(-border_left(), top_captions, self_size.new_width(table_width + border_left() + border_right()), fmt_ctx);
			top_captions += caption->height();
		}
    }

    top_captions += border_top();
    m_grid->top_captions_height(top_captions);
    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        for (const auto& cell : m_grid->cells(row)) cell.el->pos().y += top_captions;
    }

	pixel_t bottom_captions = 0;

	for (auto& caption : m_grid->captions())
	{
		if(caption->css().get_caption_side() == caption_side_bottom)
		{
			caption->render(-border_left(), table_height + top_captions + bottom_captions, self_size.new_width(table_width + border_left() + border_right()), fmt_ctx);
			bottom_captions += caption->height();
		}
	}

	m_pos.move_to(x + content_offset_left(), y + content_offset_top());
	m_pos.width = table_width;
	m_pos.height = table_height + top_captions + bottom_captions;

	if (containing_block_size.size_mode & containing_block_context::size_mode_intrinsic_inline_probe)
		return { max_table_width + content_offset_width(), min_table_width + content_offset_width() };
	if(self_size.width.type != containing_block_context::cbc_value_type_absolute)
	{
		rendered_width ret;
		ret.natural_width = ret.min_width = std::min(table_width, max_table_width) + content_offset_width();
		return ret;
	}
	return {table_width + content_offset_width(), table_width + content_offset_width()};
}

int litehtml::render_item_table::collect_columns()
{
    m_column_groups.clear();
    m_declared_columns.clear();
    int next = 0;
    const auto append = [&](const std::shared_ptr<element>& source) {
        const int count = dockit::read_table_span(source->get_attr("span", ""), 1, 1000);
        if (next > std::numeric_limits<int>::max() - count)
            throw std::length_error("HTML table column range is too large.");
        const int start = next;
        next += count;
        return column_range{source, start, next};
    };
    for (const auto& item : m_children)
    {
        if (item->css().get_display() == display_table_column)
            m_declared_columns.push_back(append(item->src_el()));
        else if (item->css().get_display() == display_table_column_group)
        {
            const int start = next;
            for (const auto& col : item->children())
            {
                if (col->css().get_display() == display_table_column)
                    m_declared_columns.push_back(append(col->src_el()));
            }
            if (next != start) m_column_groups.push_back({item->src_el(), start, next});
            else if (std::none_of(item->src_el()->children().begin(), item->src_el()->children().end(),
                [](const auto& child) { return child->tag() == _col_; }))
                m_column_groups.push_back(append(item->src_el()));
        }
    }
    return next;
}

void litehtml::render_item_table::collect_collapsed_tracks()
{
    m_collapsed_columns.assign(m_grid->cols_count(), false);
    m_collapsed_rows.assign(m_grid->rows_count(), false);
    const auto mark_columns = [&](const column_range& range) {
        if (range.source->css().get_visibility() == visibility_collapse)
            std::fill(m_collapsed_columns.begin() + range.begin, m_collapsed_columns.begin() + range.end, true);
    };
    for (const auto& group : m_column_groups) mark_columns(group);
    for (const auto& column : m_declared_columns) mark_columns(column);
    for (const auto& group : m_row_groups)
    {
        if (group.item->css().get_visibility() == visibility_collapse)
            std::fill(m_collapsed_rows.begin() + group.begin, m_collapsed_rows.begin() + group.end, true);
    }
    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        if (m_grid->row(row).el_row->css().get_visibility() == visibility_collapse)
            m_collapsed_rows[row] = true;
    }
    const auto collect_lines = [](const std::vector<bool>& collapsed, std::vector<int>& lines) {
        lines.resize(collapsed.size() + 1);
        lines[0] = 0;
        for (size_t i = 0; i < collapsed.size(); ++i) lines[i + 1] = lines[i] + !collapsed[i];
    };
    collect_lines(m_collapsed_columns, m_column_lines);
    collect_lines(m_collapsed_rows, m_row_lines);
}

void litehtml::render_item_table::resolve_shared_borders()
{
    struct candidate {
        shared_edge edge;
        int origin;
        int column;
        int row;
    };
    using candidates = std::map<int, std::vector<candidate>>;
    candidates horizontal, vertical;
    const int columns = m_column_lines.back(), rows = m_row_lines.back();
    const auto add = [](candidates& lines, int line, int begin, int end,
        const border& stroke, int origin, int column, int row) {
        if (begin < end && stroke.style != border_style_none)
            lines[line].push_back({{begin, end, stroke}, origin, column, row});
    };
    const auto add_box = [&](const element& source, int x0, int x1, int y0, int y1, int origin) {
        if (x0 == x1 || y0 == y1) return;
        const auto& style = source.css();
        const auto stroke = [&](const css_border& value) {
            border result(value);
            result.color = result.color.to_srgb(style.get_color());
            return result;
        };
        add(vertical, x0, y0, y1, stroke(style.get_borders().left), origin, x0, y0);
        add(vertical, x1, y0, y1, stroke(style.get_borders().right), origin, x0, y0);
        add(horizontal, y0, x0, x1, stroke(style.get_borders().top), origin, x0, y0);
        add(horizontal, y1, x0, x1, stroke(style.get_borders().bottom), origin, x0, y0);
    };

    add_box(*src_el(), 0, columns, 0, rows, 0);
    for (const auto& group : m_column_groups)
        add_box(*group.source, m_column_lines[group.begin], m_column_lines[group.end], 0, rows, 1);
    for (const auto& column : m_declared_columns)
    {
        for (int i = column.begin; i < column.end; ++i)
            add_box(*column.source, m_column_lines[i], m_column_lines[i + 1], 0, rows, 2);
    }
    for (const auto& group : m_row_groups)
        add_box(*group.item->src_el(), 0, columns, m_row_lines[group.begin], m_row_lines[group.end], 3);
    border no_divider;
    no_divider.style = border_style_hidden;
    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        add_box(*m_grid->row(row).el_row->src_el(), 0, columns, m_row_lines[row], m_row_lines[row + 1], 4);
        for (const auto& cell : m_grid->cells(row))
        {
            const int left = m_column_lines[cell.column], right = m_column_lines[cell.column + cell.colspan];
            const int top = m_row_lines[row], bottom = m_row_lines[row + cell.rowspan];
            add_box(*cell.el->src_el(), left, right, top, bottom, 5);
            if (left == right || top == bottom) continue;
            for (int line = left + 1; line < right; ++line)
                add(vertical, line, top, bottom, no_divider, 6, left, top);
            for (int line = top + 1; line < bottom; ++line)
                add(horizontal, line, left, right, no_divider, 6, left, top);
        }
    }
    const auto rank_style = [](border_style style) {
        switch (style)
        {
            case border_style_double: return 8;
            case border_style_solid: return 7;
            case border_style_dashed: return 6;
            case border_style_dotted: return 5;
            case border_style_ridge: return 4;
            case border_style_outset: return 3;
            case border_style_groove: return 2;
            case border_style_inset: return 1;
            default: return 0;
        }
    };
    const bool rtl = css().is_right_to_left();
    const auto priority = [&](const candidate& value) {
        return std::tuple(value.edge.stroke.style == border_style_hidden, value.edge.stroke.width,
            rank_style(value.edge.stroke.style), value.origin, rtl ? value.column : -value.column, -value.row);
    };

    const auto resolve = [&](const candidates& input, edge_lines& output) {
        output.clear();
        for (const auto& [line, choices] : input)
        {
            struct event {
                int offset;
                size_t choice;
                bool enter;
            };
            std::vector<event> events;
            events.reserve(choices.size() * 2);
            for (size_t i = 0; i < choices.size(); ++i)
            {
                events.push_back({choices[i].edge.begin, i, true});
                events.push_back({choices[i].edge.end, i, false});
            }
            std::sort(events.begin(), events.end(), [](const event& a, const event& b) { return a.offset < b.offset; });
            const auto order = [&](size_t a, size_t b) {
                return std::tuple(priority(choices[a]), a) < std::tuple(priority(choices[b]), b);
            };
            std::set<size_t, decltype(order)> active(order);
            auto& edges = output[line];
            int start = events.front().offset;
            for (const auto& event : events)
            {
                if (event.offset > start && !active.empty())
                {
                    border stroke = choices[*active.rbegin()].edge.stroke;
                    if (stroke.width > 0 && stroke.style != border_style_hidden)
                    {
                        if (stroke.style == border_style_inset) stroke.style = border_style_ridge;
                        else if (stroke.style == border_style_outset) stroke.style = border_style_groove;
                        if (!edges.empty() && edges.back().end == start && edges.back().stroke.width == stroke.width &&
                            edges.back().stroke.style == stroke.style && edges.back().stroke.color == stroke.color)
                            edges.back().end = event.offset;
                        else edges.push_back({start, event.offset, stroke});
                    }
                }
                if (event.enter) active.insert(event.choice);
                else active.erase(event.choice);
                start = event.offset;
            }
        }
    };
    resolve(horizontal, m_horizontal_edges);
    resolve(vertical, m_vertical_edges);

    const auto half_width = [](const edge_lines& lines, int line, int begin, int end) {
        pixel_t width = 0;
        const auto found = lines.find(line);
        if (found == lines.end() || begin >= end) return width;
        const auto& edges = found->second;
        auto segment = std::lower_bound(edges.begin(), edges.end(), begin,
            [](const shared_edge& edge, int start) { return edge.end <= start; });
        for (; segment != edges.end() && segment->begin < end; ++segment)
            width = std::max(width, segment->stroke.width / 2);
        return width;
    };
    const auto box_borders = [&](int left, int right, int top, int bottom) {
        margins result;
        result.left = half_width(m_vertical_edges, left, top, bottom);
        result.right = half_width(m_vertical_edges, right, top, bottom);
        result.top = half_width(m_horizontal_edges, top, left, right);
        result.bottom = half_width(m_horizontal_edges, bottom, left, right);
        return result;
    };
    if (columns && rows) set_shared_table_borders(box_borders(0, columns, 0, rows));
    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        m_grid->row(row).el_row->set_shared_table_borders({});
        for (const auto& cell : m_grid->cells(row))
            cell.el->set_shared_table_borders(box_borders(m_column_lines[cell.column],
                m_column_lines[cell.column + cell.colspan], m_row_lines[row], m_row_lines[row + cell.rowspan]));
    }
}

void litehtml::render_item_table::paint_shared_borders(uint_ptr hdc, const position& offset) const
{
    if (m_horizontal_edges.empty() && m_vertical_edges.empty()) return;
    auto* container = src_el()->get_document()->container();
    const position* clip = nullptr;
    if (container->begin_render_item(*this, clip, true))
    {
        const auto coordinate = [](const std::vector<int>& lines, int line, auto begin, auto end) {
            if (line == lines.back())
            {
                const auto last = std::lower_bound(lines.begin(), lines.end(), line);
                return end(static_cast<int>(last - lines.begin()) - 1);
            }
            const auto track = std::upper_bound(lines.begin(), lines.end(), line);
            return begin(static_cast<int>(track - lines.begin()) - 1);
        };
        const auto x = [&](int line) {
            return offset.x + coordinate(m_column_lines, line, [&](int col) { return m_grid->column(col).left; },
                [&](int col) { return m_grid->column(col).right; });
        };
        const auto y = [&](int line) {
            return offset.y + m_grid->top_captions_height() + coordinate(m_row_lines, line,
                [&](int row) { return m_grid->row(row).top; }, [&](int row) { return m_grid->row(row).bottom; });
        };
        for (const auto& [line, edges] : m_horizontal_edges)
        {
            for (const auto& edge : edges)
            {
                borders decoration;
                decoration.top = edge.stroke;
                const pixel_t cap = edge.stroke.width / 2;
                const pixel_t begin = x(edge.begin);
                const pixel_t end = x(edge.end);
                // Collapsed strokes meet on their centerlines. Extend the caps to their junctions to seal corner pixels.
                container->draw_borders(hdc, decoration, {begin - cap, y(line) - cap,
                    end - begin + cap * 2, edge.stroke.width}, false);
            }
        }
        for (const auto& [line, edges] : m_vertical_edges)
        {
            for (const auto& edge : edges)
            {
                borders decoration;
                decoration.left = edge.stroke;
                const pixel_t cap = edge.stroke.width / 2;
                const pixel_t begin = y(edge.begin);
                const pixel_t end = y(edge.end);
                container->draw_borders(hdc, decoration, {x(line) - cap, begin - cap,
                    edge.stroke.width, end - begin + cap * 2}, false);
            }
        }
    }
    container->end_render_item();
}

litehtml::pixel_t litehtml::render_item_table::resolve_fixed_columns(pixel_t width, int first_row)
{
    const int count = m_grid->cols_count();
    std::vector<bool> sized(count);
    for (int column = 0; column < count; ++column) m_grid->column(column).width = 0;

    const auto assign_width = [&](int begin, int end, pixel_t target) {
        int available = 0;
        for (int column = begin; column < end; ++column)
        {
            if (sized[column]) target -= m_grid->column(column).width;
            else ++available;
        }
        if (available == 0) return;
        const pixel_t share = std::max<pixel_t>(0, target) / available;
        for (int column = begin; column < end; ++column)
        {
            if (sized[column]) continue;
            m_grid->column(column).width = share;
            sized[column] = true;
        }
    };

    for (const auto& range : m_declared_columns)
    {
        const auto& value = range.source->css().get_width();
        if (value.is_predefined()) continue;
        for (int column = range.begin; column < range.end; ++column)
            assign_width(column, column + 1, value.calc_percent(width));
    }
    for (const auto& range : m_column_groups)
    {
        const auto& value = range.source->css().get_width();
        if (!value.is_predefined())
            assign_width(range.begin, range.end, value.calc_percent(width) -
                m_border_spacing_x * (range.end - range.begin - 1));
    }
    if (first_row < m_grid->rows_count())
    {
        for (const auto& cell : m_grid->cells(first_row))
        {
            const auto& style = cell.el->css();
            const auto& value = style.get_width();
            if (value.is_predefined()) continue;
            cell.el->calc_outlines(width);
            const pixel_t extras = style.get_box_sizing() == box_sizing_border_box ? 0 : cell.el->content_offset_width();
            assign_width(cell.column, cell.column + cell.colspan, value.calc_percent(width) + extras -
                m_border_spacing_x * (cell.colspan - 1));
        }
    }
    const bool has_unassigned = std::find(sized.begin(), sized.end(), false) != sized.end();
    if (has_unassigned) assign_width(0, count, width);
    pixel_t total = 0;
    for (int column = 0; column < count; ++column) total += m_grid->column(column).width;
    if (!has_unassigned && total < width && count > 0)
    {
        const pixel_t extra = (width - total) / count;
        for (int column = 0; column < count; ++column) m_grid->column(column).width += extra;
        total = width;
    }
    return count == 0 ? width : total;
}

void litehtml::render_item_table::apply_column_widths(pixel_t available_width, bool intrinsic_probe)
{
    const auto width = [&](const column_range& range) -> pixel_t {
        const auto& value = range.source->css().get_width();
        if (value.is_predefined() || (intrinsic_probe && value.units() == css_units_percentage)) return 0;
        return std::max<pixel_t>(0, value.calc_percent(available_width));
    };
    for (const auto& range : m_declared_columns)
    {
        const pixel_t minimum = width(range);
        for (int col = range.begin; col < range.end; ++col)
        {
            auto& column = m_grid->column(col);
            column.min_width = std::max(minimum, column.min_width);
            column.max_width = std::max(column.min_width, column.max_width);
        }
    }
    for (const auto& range : m_column_groups)
    {
        pixel_t extra = width(range);
        for (int col = range.begin; col < range.end; ++col) extra -= m_grid->column(col).min_width;
        if (extra <= 0) continue;
        m_grid->distribute_min_width(extra, range.begin, range.end - 1);
        for (int col = range.begin; col < range.end; ++col)
            m_grid->column(col).max_width = std::max(m_grid->column(col).min_width, m_grid->column(col).max_width);
    }
}

std::shared_ptr<litehtml::render_item> litehtml::render_item_table::init()
{
    m_grid = std::make_unique<table_grid>();
    m_row_groups.clear();

    using item_ptr = std::shared_ptr<render_item>;
    item_ptr header;
    item_ptr footer;
    for (const auto& item : m_children)
    {
        const auto display = item->css().get_display();
        if (!header && display == display_table_header_group) header = item;
        if (!footer && display == display_table_footer_group) footer = item;
    }
    const auto add_row = [&](const item_ptr& row, int end) {
        m_grid->begin_row(row, end);
        for (auto& cell : row->children())
        {
            if (cell->css().get_display() != display_table_cell) continue;
            cell = cell->init();
            m_grid->add_cell(cell);
        }
        row->src_el()->add_render(row);
    };
    int next_row = 0;
    const auto group_end = [&](std::size_t count) {
        if (count > static_cast<std::size_t>(std::numeric_limits<int>::max() - next_row))
            throw std::length_error("HTML table row range is too large.");
        return next_row + static_cast<int>(count);
    };
    const auto add_group = [&](const item_ptr& group) {
        if (!group) return;
        const auto& children = group->children();
        const auto is_row = [](const item_ptr& item) { return item->css().get_display() == display_table_row; };
        const int end = group_end(std::count_if(children.begin(), children.end(), is_row));
        m_row_groups.push_back({group, next_row, end});
        for (const auto& row : children)
        {
            if (is_row(row)) add_row(row, end);
        }
        group->src_el()->add_render(group);
        next_row = end;
    };

    add_group(header);
    std::vector<item_ptr> direct_rows;
    const auto flush_rows = [&]() {
        const int end = group_end(direct_rows.size());
        for (const auto& row : direct_rows) add_row(row, end);
        direct_rows.clear();
        next_row = end;
    };
    for (auto& item : m_children)
    {
        if (item->src_el()->is_table_skip()) continue;
        const auto display = item->css().get_display();
        if (display == display_table_row)
        {
            direct_rows.push_back(item);
            continue;
        }
        flush_rows();
        if (item == header || item == footer) continue;
        if (display == display_table_caption)
        {
            item = item->init();
            m_grid->captions().push_back(item);
        }
        else if (display == display_table_row_group || display == display_table_header_group || display == display_table_footer_group)
            add_group(item);
    }
    flush_rows();
    add_group(footer);
    m_grid->finish(collect_columns());

    src_el()->add_render(shared_from_this());
    return shared_from_this();
}

litehtml::size litehtml::render_item_table::layout_overflow() const
{
    size extent(m_pos.width, m_pos.height);
    if (!m_grid || !css().uses_fixed_table_layout() || css().get_width().is_predefined()) return extent;
    for (int row = 0; row < m_grid->rows_count(); ++row)
    {
        for (const auto& cell : m_grid->cells(row))
            cell.el->calc_document_size(extent);
    }
    return extent;
}

std::optional<litehtml::pixel_t> litehtml::render_item_table::find_layout_baseline(bool last) const
{
	if (!m_grid || m_grid->rows_count() == 0) return std::nullopt;
	const int edge = last ? m_grid->rows_count() - 1 : 0;
	std::optional<pixel_t> baseline;
	std::optional<pixel_t> content_edge;
	for (int row = 0; row <= edge; ++row)
	{
		for (const auto& cell : m_grid->cells(row))
		{
			if (last && row + cell.rowspan - 1 < edge) continue;
			const auto& item = *cell.el;
			if (item.collapsed_table_track()) continue;
			const auto end = item.bottom() - item.content_offset_bottom();
			content_edge = std::max(content_edge.value_or(end), end);
			if (item.css().get_vertical_align() == va_baseline)
			{
				if (const auto offset = item.layout_baseline(false))
				{
					const auto value = item.top() + *offset;
					baseline = std::max(baseline.value_or(value), value);
				}
			}
		}
	}
	return content_offset_top() + baseline.value_or(content_edge.value_or(m_grid->row(edge).el_row->pos().y));
}

void litehtml::render_item_table::paint_table_backgrounds(uint_ptr hdc, const position& offset) const
{
    auto* container = src_el()->get_document()->container();
    const auto paint = [&](const table_cell& cell, const element& source) {
        if (cell.el->collapsed_table_track() || cell.el->empty_table_cell() ||
            source.css().get_visibility() != visibility_visible) return;
        const auto& bg = source.css().get_bg();
        const auto color = bg.m_color.to_srgb(bg.m_current_color);
        if (!color.alpha) return;
        background_layer layer;
        layer.border_box = cell.el->pos();
        layer.border_box += cell.el->get_paddings();
        layer.border_box += cell.el->get_borders();
        layer.border_box.x += offset.x;
        layer.border_box.y += offset.y;
        layer.clip_box = layer.border_box;
        container->draw_solid_fill(hdc, layer, color);
    };
    const auto paint_columns = [&](const std::vector<column_range>& ranges) {
        if (ranges.empty()) return;
        for (int row = 0; row < m_grid->rows_count(); ++row)
        {
            for (const auto& cell : m_grid->cells(row))
            {
                const auto match = std::lower_bound(ranges.begin(), ranges.end(), cell.column,
                    [](const column_range& range, int col) { return range.end <= col; });
                if (match != ranges.end() && match->begin <= cell.column) paint(cell, *match->source);
            }
        }
    };
    paint_columns(m_column_groups);
    paint_columns(m_declared_columns);
    for (const auto& group : m_row_groups)
    {
        const auto& style = group.item->css();
        if (group.item->establishes_paint_context() || style.get_visibility() != visibility_visible ||
            !style.get_bg().m_color.to_srgb(style.get_bg().m_current_color).alpha) continue;
        for (int row = group.begin; row < group.end; ++row)
        {
            for (const auto& cell : m_grid->cells(row)) paint(cell, *group.item->src_el());
        }
    }
}

void litehtml::render_item_table::draw_children(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, draw_flag flag, int zindex)
{
    if (!m_grid) return;

    auto* container = src_el()->get_document()->container();
    position viewport = m_pos;
    viewport.x += x;
    viewport.y += y;
    const bool clips_content = clips_overflow();
    if (clips_content) container->set_overflow_clip(*this);

    position pos = viewport;
    pos.x -= get_scroll_left();
    pos.y -= get_scroll_top();
    if (flag == draw_block) paint_table_backgrounds(hdc, pos);
    const auto paint_table_part = [&](const std::shared_ptr<render_item>& item) {
        if (flag == draw_positioned)
        {
            paint_positioned_child(*item, hdc, pos.x, pos.y, clip, zindex);
            return;
        }
        if (item->establishes_paint_context()) return;
        if (flag == draw_block) item->paint(hdc, pos.x, pos.y, clip);
        item->draw_children(hdc, pos.x, pos.y, clip, flag, zindex);
    };
    for (auto& caption : m_grid->captions())
    {
        paint_table_part(caption);
    }
    auto group = m_row_groups.begin();
    for (int row = 0; row < m_grid->rows_count(); row++)
    {
        while (group != m_row_groups.end() && group->end <= row) ++group;
        if (group != m_row_groups.end() && group->begin == row && group->item->establishes_paint_context())
        {
            if (group->item->is_visible()) paint_table_part(group->item);
            row = group->end - 1;
            continue;
        }
        const auto& item = m_grid->row(row).el_row;
        if (item->establishes_paint_context())
        {
            if (item->is_visible()) paint_table_part(item);
            continue;
        }
        if (flag == draw_block && !m_collapsed_rows[row])
        {
            const position* row_clip = clip;
            if (item->is_visible() && container->begin_render_item(*item, row_clip))
                item->src_el()->draw_background(hdc, pos.x, pos.y, row_clip, item);
            if (item->is_visible()) container->end_render_item();
        }
        for (const auto& cell : m_grid->cells(row))
        {
            if (cell.el->collapsed_table_track()) continue;
            if (cell.el->has_table_cell_clip())
            {
                position cell_box = cell.el->pos();
                cell_box += cell.el->get_paddings();
                cell_box += cell.el->get_borders();
                cell_box.x += pos.x;
                cell_box.y += pos.y;
                container->set_render_item_clip(cell_box, {}, *cell.el);
            }
            paint_table_part(cell.el);
            if (cell.el->has_table_cell_clip()) container->del_clip();
        }
    }
    if (flag == draw_block) paint_shared_borders(hdc, pos);
    if (clips_content)
    {
        if (flag == draw_inlines) container->draw_scrollbars(*this);
        container->del_clip();
    }
}

litehtml::pixel_t litehtml::render_item_table::get_draw_vertical_offset()
{
    if(m_grid)
    {
        return m_grid->top_captions_height();
    }
    return 0;
}

void litehtml::render_item_table_part::get_inline_boxes(position::vector& boxes) const
{
    for (const auto& child : m_children)
    {
        if (child->css().get_display() == display_table_row && !child->collapsed_table_track())
            child->get_inline_boxes(boxes);
    }
}

void litehtml::render_item_table_row::get_inline_boxes( position::vector& boxes ) const
{
	position pos;
	for(auto& el : m_children)
	{
		if(el->src_el()->css().get_display() == display_table_cell &&
            !el->collapsed_table_track() && !el->empty_table_cell())
		{
			pos.x		= el->left() + el->margin_left();
			pos.y		= el->top() - m_padding.top - m_borders.top;

			pos.width	= el->right() - pos.x - el->margin_right() - el->margin_left();
			pos.height	= el->height() + m_padding.top + m_padding.bottom + m_borders.top + m_borders.bottom;

			boxes.push_back(pos);
		}
	}
}
