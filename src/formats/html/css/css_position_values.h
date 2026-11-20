#pragma once

#include "css_position.h"
#include "css_token_compat.h"

namespace litehtml {
	// Consume the longest valid <position> prefix, with the background-only three-component form if requested.
	bool parse_image_position(const css_token_vector& p_tokens, int& r_index, css_image_position& r_position,
		bool p_accept_three = false);

	// Parse the position list component for a physical or flow-relative background axis.
	std::optional<css_axis_position> parse_axis_position(const css_token_vector& p_tokens, bool p_horizontal, bool p_logical);
}
