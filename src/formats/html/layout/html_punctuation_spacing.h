#pragma once

#include <litehtml/types.h>

namespace litehtml {
	class element;

	enum class spacing_punctuation { other, opening, closing, middle, narrow_opening, narrow_closing, ideographic_space };

	struct punctuation_spacing {
		spacing_punctuation kind = spacing_punctuation::other;
		punctuation_trim trim = punctuation_trim::none;
		pixel_t reduction = 0;
		pixel_t font_size = 0;
		int flags = 0;

		bool at_start(bool p_paragraph_start) const;
		bool before(const punctuation_spacing& p_next) const;
		bool after(const punctuation_spacing& p_previous) const;
	};

	bool is_fullwidth_spacing_punctuation(char32_t p_character);
	bool is_punctuation_variation_selector(char32_t p_character);
	punctuation_spacing get_punctuation_spacing(const element& p_element, bool p_trailing, pixel_t p_untrimmed_width);
}
