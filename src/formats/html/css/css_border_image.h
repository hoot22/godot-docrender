#pragma once

#include "css_gradient.h"

#include <array>

namespace litehtml {
	struct css_border_image_source {
		image value;
		string baseurl;
	};

	struct css_border_image_slice {
		struct Side {
			double value = 100;
			bool percentage = true;
		};
		std::array<Side, 4> sides;
		bool fill = false;
	};

	struct css_border_image_offsets {
		using Side = std::variant<double, css_length>;
		std::array<Side, 4> sides{};
	};

	enum class border_image_tiling { stretch, repeat, round, space };

	struct css_border_image_repeat {
		border_image_tiling horizontal = border_image_tiling::stretch;
		border_image_tiling vertical = border_image_tiling::stretch;
	};

	struct css_border_image {
		css_border_image_source source;
		css_border_image_slice slice;
		css_border_image_offsets width{ { 1.0, 1.0, 1.0, 1.0 } };
		css_border_image_offsets outset;
		css_border_image_repeat repeat;
	};

}
