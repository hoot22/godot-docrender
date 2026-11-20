#pragma once

#include "formats/html/css/css_grid_lines.h"

namespace litehtml {
	struct grid_placement {
		std::vector<grid_item_placement> items;
		std::size_t row_count;
		std::size_t column_count;
		std::size_t row_offset = 0;
		std::size_t column_offset = 0;
	};

	grid_placement place_grid_items(const std::vector<grid_item_lines>& items, std::size_t explicit_rows, std::size_t explicit_columns,
		const grid_line_indices& row_names, const grid_line_indices& column_names, grid_auto_flow flow,
		bool subgrid_rows = false, bool subgrid_columns = false);
}
