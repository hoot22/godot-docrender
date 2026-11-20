#pragma once

#include "html_hyphenation.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace litehtml {
	class render_item;
}

namespace dockit {
	using HTMLLexicalBreaks = std::unordered_map<const litehtml::render_item*, std::vector<int64_t>>;

	struct HTMLInlineWordBreaks {
		HTMLLexicalBreaks lexical;
		HTMLAutomaticHyphens automatic_hyphens;
	};

	HTMLInlineWordBreaks prepare_inline_word_breaks(const std::shared_ptr<litehtml::render_item>& p_root, litehtml::pixel_t p_line_width);
}
