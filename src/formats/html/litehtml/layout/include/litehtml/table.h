// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LH_TABLE_H
#define LH_TABLE_H

#include <vector>
#include <memory>
#include <map>
#include "css_length.h"
#include "types.h"

namespace litehtml
{
	class render_item;

	struct table_row
	{
		using vector = std::vector<table_row>;

		pixel_t			height;
		std::shared_ptr<render_item>	el_row;
		pixel_t			top;
		pixel_t			bottom;
		css_length		css_height;
		pixel_t			min_height;

		table_row()
		{
			min_height		= 0;
			top				= 0;
			bottom			= 0;
			height			= 0;
			el_row			= nullptr;
			css_height.predef(0);
		}

		table_row(pixel_t h, const std::shared_ptr<render_item>& row);

		table_row(const table_row& val)
		{
			min_height = val.min_height;
			top = val.top;
			bottom = val.bottom;
			height = val.height;
			css_height = val.css_height;
			el_row = val.el_row;
		}

		table_row(table_row&& val) noexcept
		{
			min_height = val.min_height;
			top = val.top;
			bottom = val.bottom;
			height = val.height;
			css_height = val.css_height;
			el_row = std::move(val.el_row);
		}
	};

	struct table_column
	{
		using vector = std::vector<table_column>;

		pixel_t		min_width;
		pixel_t		max_width;
		pixel_t		width;
		css_length	css_width;
		pixel_t		left;
		pixel_t		right;

		table_column()
		{
			left			= 0;
			right			= 0;
			min_width		= 0;
			max_width		= 0;
			width			= 0;
			css_width.predef(0);
		}

		table_column(pixel_t min_w, pixel_t max_w)
		{
			left			= 0;
			right			= 0;
			max_width		= max_w;
			min_width		= min_w;
			width			= 0;
			css_width.predef(0);
		}

		table_column(const table_column& val)
		{
			left			= val.left;
			right			= val.right;
			max_width		= val.max_width;
			min_width		= val.min_width;
			width			= val.width;
			css_width		= val.css_width;
		}
	};

	class table_column_accessor
	{
	public:
		virtual pixel_t& get(table_column& col) = 0;

	protected:
		~table_column_accessor() = default;
	};

	class table_column_accessor_max_width final : public table_column_accessor
	{
	public:
		pixel_t& get(table_column& col) override;
	};

	class table_column_accessor_min_width final : public table_column_accessor
	{
	public:
		pixel_t& get(table_column& col) override;
	};

	class table_column_accessor_width final : public table_column_accessor
	{
	public:
		pixel_t& get(table_column& col) override;
	};

	struct table_cell
	{
		std::shared_ptr<render_item>	el;
		int column = 0;
		int				colspan;
		int				rowspan;
		pixel_t			min_width;
		pixel_t			min_height;
		pixel_t			max_width;
		pixel_t			max_height;
		pixel_t			width;
		pixel_t			height;

		table_cell()
		{
			min_width		= 0;
			min_height		= 0;
			max_width		= 0;
			max_height		= 0;
			width			= 0;
			height			= 0;
			colspan			= 1;
			rowspan			= 1;
			el				= nullptr;
		}

		table_cell(const table_cell& val)
		{
			column = val.column;
			el				= val.el;
			colspan			= val.colspan;
			rowspan			= val.rowspan;
			width			= val.width;
			height			= val.height;
			min_width		= val.min_width;
			min_height		= val.min_height;
			max_width		= val.max_width;
			max_height		= val.max_height;
		}

		table_cell(table_cell&& val) noexcept
		{
			column = val.column;
			el = std::move(val.el);
			colspan = val.colspan;
			rowspan = val.rowspan;
			width = val.width;
			height = val.height;
			min_width = val.min_width;
			min_height = val.min_height;
			max_width = val.max_width;
			max_height = val.max_height;
		}
	};

	class table_grid
	{
	public:
		using rows = std::vector<std::vector<table_cell>>;
	private:
		int						m_rows_count;
		int						m_cols_count;
		rows					m_cells;
		struct occupied_span {
			int column_end;
			int row_end;
		};
		std::map<int, occupied_span> m_occupied;
		int m_next_column = 0;
		int m_group_end = 0;
		table_column::vector	m_columns;
		table_row::vector		m_rows;
		std::vector<std::shared_ptr<render_item>> m_captions;
		pixel_t					m_top_captions_height;
		pixel_t					m_bottom_captions_height;
	public:

		table_grid() :
			m_rows_count(0),
			m_cols_count(0),
			m_top_captions_height(0),
			m_bottom_captions_height(0)
		{
		}

		void			clear();
		void			begin_row(const std::shared_ptr<render_item>& row, int group_end);
		void			add_cell(const std::shared_ptr<render_item>& el);

		void			finish(int declared_columns);
		std::vector<table_cell>& cells(int row) { return m_cells[row]; }
		const std::vector<table_cell>& cells(int row) const { return m_cells[row]; }
		table_column&	column(int c)	{ return m_columns[c];	}
		table_row&		row(int r)		{ return m_rows[r];		}
		std::vector<std::shared_ptr<render_item>>& captions()		{ return m_captions; }

		int				rows_count() const	{ return m_rows_count;	}
		int				cols_count() const	{ return m_cols_count; }

		void			top_captions_height(pixel_t height) { m_top_captions_height = height; }
		pixel_t			top_captions_height() const { return m_top_captions_height; }
		void			bottom_captions_height(pixel_t height) { m_bottom_captions_height = height; }
		pixel_t			bottom_captions_height() const { return m_bottom_captions_height; }

		void			distribute_max_width(pixel_t width, int start, int end);
		void			distribute_min_width(pixel_t width, int start, int end);
		void			distribute_width(pixel_t width, int start, int end);
		void			distribute_width(pixel_t width, int start, int end, table_column_accessor* acc);
		pixel_t			calc_table_width(pixel_t block_width, bool is_auto, pixel_t& min_table_width, pixel_t& max_table_width);
		void			calc_horizontal_positions(pixel_t spacing);
		void			calc_vertical_positions(pixel_t spacing);
		void			calc_rows_height(pixel_t blockHeight, pixel_t borderSpacingY);
	};
}

#endif  // LH_TABLE_H
