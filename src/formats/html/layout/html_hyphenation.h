#pragma once

#include <litehtml/types.h>
#include <unordered_map>
#include <vector>

namespace litehtml {
	class render_item;
}

namespace dockit {
	struct HTMLAutomaticHyphen {
		int64_t offset;
		bool fallback;
	};
	using HTMLAutomaticHyphens = std::unordered_map<const litehtml::render_item*, std::vector<HTMLAutomaticHyphen>>;

	void prepare_hyphenation(const std::vector<litehtml::render_item*>& p_text, litehtml::pixel_t p_line_width,
		HTMLAutomaticHyphens& r_automatic);
}
