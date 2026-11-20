#pragma once

#include <litehtml/types.h>

namespace litehtml {
	class css_properties;
	class element;

	bool is_explicit_wrap_point(const element& p_element);
	bool is_soft_hyphen(const element& p_element);
	bool is_hyphenation_fallback(const element& p_element);
	pixel_t measure_hyphen(const element& p_element);
	bool is_lexical_character(char32_t p_character);
	bool extends_lexical_character(char32_t p_character);
	bool is_grapheme_boundary(const element& p_left, const element& p_right);
	bool allows_text_boundary_wrap(const element& p_left, const element& p_right, const css_properties& p_style);
}
