#pragma once

#include <litehtml/types.h>

#include <godot_cpp/variant/string.hpp>

namespace litehtml {
	class element;
	class render_item;

	std::shared_ptr<element> inline_boundary_owner(const element& p_before, const element& p_after);
	bool separates_autospace(const render_item& p_inline, bool p_start, pixel_t p_basis);
	bool is_autospace_fragment_boundary(const element& p_owner, const godot::String& p_text, int64_t p_offset);
	pixel_t text_autospace_gap(const element& p_before, const element& p_after);
	bool has_non_breaking_autospace(const element& p_before, const element& p_after);
	void prepare_autospace_replacements(const std::shared_ptr<render_item>& p_root, pixel_t p_basis);
}
