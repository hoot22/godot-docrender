#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace litehtml {
	inline constexpr int grid_track_limit = 10000;

	struct grid_line {
		enum class kind { automatic, number, name, span };
		kind type = kind::automatic;
		std::int64_t value = 0;
		std::string name;
	};

	struct grid_axis_placement {
		grid_line start;
		grid_line end;
	};

	struct grid_item_lines {
		grid_axis_placement rows;
		grid_axis_placement columns;
	};

	struct grid_auto_flow {
		bool by_column = false;
		bool dense = false;
	};

	struct grid_track_range {
		// Zero-based, with the end excluded.
		std::size_t start;
		std::size_t end;
	};

	struct grid_item_placement {
		grid_track_range rows;
		grid_track_range columns;
	};

	// Each entry lists distinct explicit line indices from start to end.
	using grid_line_indices = std::unordered_map<std::string, std::vector<int>>;

}
