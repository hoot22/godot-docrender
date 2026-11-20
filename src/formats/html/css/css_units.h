#pragma once

#include "css_length.h"

namespace dockit::css {

	using namespace litehtml;

	struct UnitContext {
		const media_features& viewport;
		const font_metrics& root_font;
		const void* host;
		pixel_t (*points_to_pixels)(const void*, float);
		css_container_units container_units = {};
	};

	pixel_t length_to_pixels(const css_length& length, const font_metrics& metrics, pixel_t basis, const UnitContext& context);
	void compute_length(css_length& length, const font_metrics& metrics, pixel_t basis, const UnitContext& context);

}
