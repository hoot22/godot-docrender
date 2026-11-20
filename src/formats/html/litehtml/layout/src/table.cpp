// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "html.h"
#include "table.h"
#include "element.h"
#include "render_item.h"
#include "types.h"

#include "formats/html/elements/html_table_attributes.h"

#include <iterator>
#include <limits>
#include <stdexcept>

void litehtml::table_grid::begin_row(const std::shared_ptr<render_item>& row, int group_end)
{
	const int current_row = static_cast<int>(m_rows.size());
	for (auto it = m_occupied.begin(); it != m_occupied.end();)
	{
		if (it->second.row_end <= current_row) it = m_occupied.erase(it);
		else ++it;
	}
	m_cells.emplace_back();
	m_rows.emplace_back(pixel_t(0), row);
	m_group_end = group_end;
	m_next_column = 0;
}

void litehtml::table_grid::add_cell(const std::shared_ptr<render_item>& el)
{
	table_cell cell;
	cell.el = el;
	const int row = static_cast<int>(m_rows.size()) - 1;
	cell.colspan = dockit::read_table_span(el->src_el()->get_attr("colspan", ""), 1, 1000);
	const int rowspan = dockit::read_table_span(el->src_el()->get_attr("rowspan", ""), 0, 65534);
	cell.rowspan = rowspan == 0 ? m_group_end - row : std::min(rowspan, m_group_end - row);

	int column = m_next_column;
	for (;;)
	{
		if (column > std::numeric_limits<int>::max() - cell.colspan)
			throw std::length_error("HTML table column range is too large.");
		const auto next = m_occupied.upper_bound(column);
		if (next != m_occupied.begin() && std::prev(next)->second.column_end > column)
		{
			column = std::prev(next)->second.column_end;
			continue;
		}
		if (next == m_occupied.end() || next->first >= column + cell.colspan) break;
		column = next->second.column_end;
	}

	cell.column = column;
	m_next_column = column + cell.colspan;
	m_cols_count = std::max(m_cols_count, m_next_column);
	if (cell.rowspan > 1) m_occupied.emplace(column, occupied_span{m_next_column, row + cell.rowspan});
	m_cells.back().push_back(std::move(cell));
}

void litehtml::table_grid::finish(int declared_columns)
{
	m_rows_count = static_cast<int>(m_rows.size());
	m_cols_count = std::max(m_cols_count, declared_columns);
	m_columns.resize(m_cols_count);
	m_occupied.clear();
	for (int row_index = 0; row_index < m_rows_count; ++row_index)
	{
		for (const auto& entry : cells(row_index))
		{
			auto& first = column(entry.column);
			const auto& width = entry.el->css().get_width();
			if (entry.colspan == 1 && !width.is_predefined() && first.css_width.is_predefined())
				first.css_width = width;
		}
	}
}

void litehtml::table_grid::distribute_max_width( pixel_t width, int start, int end )
{
	table_column_accessor_max_width selector;
	distribute_width(width, start, end, &selector);
}

void litehtml::table_grid::distribute_min_width( pixel_t width, int start, int end )
{
	table_column_accessor_min_width selector;
	distribute_width(width, start, end, &selector);
}

void litehtml::table_grid::distribute_width( pixel_t width, int start, int end, table_column_accessor* acc )
{
	if(!(start >= 0 && start < m_cols_count && end >= 0 && end < m_cols_count))
	{
		return;
	}

	pixel_t cols_width = 0;
	for(int col = start; col <= end; col++)
	{
		cols_width += m_columns[col].max_width;
	}

	pixel_t add = width / (end - start + 1);
	pixel_t added_width = 0;
	for(int col = start; col <= end; col++)
	{
		if(cols_width != 0)
		{
			add = width * (m_columns[col].max_width / cols_width);
		}
		added_width += add;
		acc->get(m_columns[col]) += add;
	}
	if(added_width < width)
	{
		acc->get(m_columns[start]) += width - added_width;
	}
}

void litehtml::table_grid::distribute_width( pixel_t width, int start, int end )
{
	if(!(start >= 0 && start < m_cols_count && end >= 0 && end < m_cols_count))
	{
		return;
	}

	std::vector<table_column*> distribute_columns;

	for(int step = 0; step < 3; step++)
	{
		distribute_columns.clear();

		switch(step)
		{
		case 0:
			{
				// distribute between the columns with width == auto
				for(int col = start; col <= end; col++)
				{
					if(m_columns[col].css_width.is_predefined())
					{
						distribute_columns.push_back(&m_columns[col]);
					}
				}
			}
			break;
		case 1:
			{
				// distribute between the columns with percents
				for(int col = start; col <= end; col++)
				{
					if(!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
					{
						distribute_columns.push_back(&m_columns[col]);
					}
				}
			}
			break;
		case 2:
			{
				//well distribute between all columns
				for(int col = start; col <= end; col++)
				{
					distribute_columns.push_back(&m_columns[col]);
				}
			}
			break;
		}

		pixel_t added_width = 0;

		if(!distribute_columns.empty() || step == 2)
		{
			pixel_t cols_width = 0;
			for(const auto& column : distribute_columns)
			{
				cols_width += column->max_width - column->min_width;
			}

			if(cols_width != 0)
			{
				pixel_t add = width / (pixel_t) distribute_columns.size();
				for(const auto& column : distribute_columns)
				{
					add = width * ((column->max_width - column->min_width) / cols_width);
					if(column->width + add >= column->min_width)
					{
						column->width	+= add;
						added_width		+= add;
					} else
					{
						added_width	+= (column->width - column->min_width) * (add / abs(add));
						column->width = column->min_width;
					}
				}
				if(added_width < width && step)
				{
					distribute_columns.front()->width += width - added_width;
					added_width = width;
				}
			} else
			{
				distribute_columns.back()->width += width;
				added_width = width;
			}
		}

		if(added_width == width)
		{
			break;
		} else
		{
			width -= added_width;
		}
	}
}

litehtml::pixel_t litehtml::table_grid::calc_table_width(pixel_t block_width, bool is_auto, pixel_t& min_table_width, pixel_t& max_table_width)
{
	//pixel_t table_width = 0;

	min_table_width = 0; // MIN
	max_table_width = 0; // MAX

	pixel_t cur_width = 0;
	pixel_t max_w = 0;
	pixel_t min_w = 0;

	for(int col = 0; col < m_cols_count; col++)
	{
		min_table_width += m_columns[col].min_width;
		max_table_width += m_columns[col].max_width;

		if(!m_columns[col].css_width.is_predefined())
		{
			m_columns[col].width = m_columns[col].css_width.calc_percent(block_width);
			m_columns[col].width = std::max(m_columns[col].width, m_columns[col].min_width);
		} else
		{
			m_columns[col].width = m_columns[col].min_width;
			max_w += m_columns[col].max_width;
			min_w += m_columns[col].min_width;
		}

		cur_width += m_columns[col].width;
	}

	if(cur_width == block_width)
	{
		return cur_width;
	}

	if(cur_width < block_width)
	{
		if(cur_width - min_w + max_w <= block_width)
		{
			cur_width = 0;
			for(int col = 0; col < m_cols_count; col++)
			{
				if(m_columns[col].css_width.is_predefined())
				{
					m_columns[col].width = m_columns[col].max_width;
				}
				cur_width += m_columns[col].width;
			}
			if(cur_width == block_width || is_auto)
			{
				return cur_width;
			}
		}
		distribute_width(block_width - cur_width, 0, m_cols_count - 1);
		cur_width = 0;
		for(int col = 0; col < m_cols_count; col++)
		{
			cur_width += m_columns[col].width;
		}
	} else
	{
		pixel_t fixed_width = 0;
		float percent = 0;
		for(int col = 0; col < m_cols_count; col++)
		{
			if(!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
			{
				percent += m_columns[col].css_width.val();
			} else
			{
				fixed_width += m_columns[col].width;
			}
		}
		auto scale = (float) (100.0 / percent);
		cur_width = 0;
		for(int col = 0; col < m_cols_count; col++)
		{
			if(!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
			{
				css_length w;
				w.set_value(m_columns[col].css_width.val() * scale, css_units_percentage);
				m_columns[col].width = w.calc_percent(block_width - fixed_width);
				if(m_columns[col].width < m_columns[col].min_width)
				{
					m_columns[col].width = m_columns[col].min_width;
				}
			}
			cur_width += m_columns[col].width;
		}
		// If the table is still too wide shrink columns with % widths
		if(cur_width > block_width)
		{
			while(true)
			{
				bool shrunk = false;
				for(int col = 0; col < m_cols_count; col++)
				{
					if(!m_columns[col].css_width.is_predefined() && m_columns[col].css_width.units() == css_units_percentage)
					{
						if(m_columns[col].width > m_columns[col].min_width)
						{
							m_columns[col].width--;
							cur_width--;
							shrunk = true;
							if(cur_width == block_width)
							{
								break;
							}
						}
					}
				}
				if(cur_width == block_width || !shrunk)
				{
					break;
				}
			}
		}
	}
	return cur_width;
}

void litehtml::table_grid::clear()
{
	m_rows_count	= 0;
	m_cols_count	= 0;
	m_cells.clear();
	m_occupied.clear();
	m_next_column = 0;
	m_group_end = 0;
	m_columns.clear();
	m_rows.clear();
}

void litehtml::table_grid::calc_horizontal_positions(pixel_t spacing)
{
	pixel_t left = spacing;
	for (auto& column : m_columns)
	{
		column.left = left;
		column.right = left + column.width;
		left = column.right + spacing;
	}
}

void litehtml::table_grid::calc_vertical_positions(pixel_t spacing)
{
	pixel_t top = spacing;
	for (auto& row : m_rows)
	{
		row.top = top;
		row.bottom = top + row.height;
		top = row.bottom + spacing;
	}
}

void litehtml::table_grid::calc_rows_height(pixel_t blockHeight, pixel_t /*borderSpacingY*/)
{
	pixel_t min_table_height = 0;

	// compute vertical size inferred by cells
	for (auto& row : m_rows)
	{
		if (!row.css_height.is_predefined())
		{
			if (row.css_height.units() != css_units_percentage)
			{
				if (row.height < (pixel_t)row.css_height.val())
				{
					row.height = (pixel_t)row.css_height.val();
				}
			}
		}
		row.min_height = row.height;
		min_table_height += row.height;
	}

	//min_table_height += borderSpacingY * ((int) m_rows.size() + 1);

	if (blockHeight > min_table_height)
	{
		pixel_t extra_height = blockHeight - min_table_height;
		int auto_count = 0; // number of rows with height=auto
		for (auto& row : m_rows)
		{
			if (!row.css_height.is_predefined() && row.css_height.units() == css_units_percentage)
			{
				row.height = row.css_height.calc_percent(blockHeight);
				if (row.height < row.min_height)
				{
					row.height = row.min_height;
				}

				extra_height -= row.height - row.min_height;

				if (extra_height <= 0) break;
			}
			else if (row.css_height.is_predefined())
			{
				auto_count++;
			}
		}
		if (extra_height > 0)
		{
			if (auto_count)
			{
				// distribute height to the rows with height=auto
				pixel_t extra_row_height = extra_height / (pixel_t) auto_count;
				for (auto& row : m_rows)
				{
					if (row.css_height.is_predefined())
					{
						row.height += extra_row_height;
					}
				}
			}
			else
			{
				// We don't have rows with height=auto, so distribute height to all rows
				if (!m_rows.empty())
				{
					pixel_t extra_row_height = extra_height / (pixel_t) m_rows.size();
					for (auto& row : m_rows)
					{
						row.height += extra_row_height;
					}
				}
			}
		}
		else if (extra_height < 0)
		{
			extra_height = -extra_height;
			for (auto row = m_rows.rbegin(); row < m_rows.rend() && extra_height > 0; row++)
			{
				if (row->height > row->min_height)
				{
					if (row->height - extra_height >= row->min_height)
					{
						row->height -= extra_height;
						extra_height = 0;
					}
					else
					{
						extra_height -= row->height - row->min_height;
						row->height = row->min_height;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

litehtml::pixel_t& litehtml::table_column_accessor_max_width::get( table_column& col )
{
	return col.max_width;
}

litehtml::pixel_t& litehtml::table_column_accessor_min_width::get( table_column& col )
{
	return col.min_width;
}

litehtml::pixel_t& litehtml::table_column_accessor_width::get( table_column& col )
{
	return col.width;
}

litehtml::table_row::table_row(pixel_t h, const std::shared_ptr<render_item>& row)
{
	min_height		= 0;
	height			= h;
	el_row			= row;
	top				= 0;
	bottom			= 0;
	if (row)
	{
		css_height = row->src_el()->css().get_height();
	}
}
