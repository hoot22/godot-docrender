#pragma once

#include <godot_cpp/variant/vector2i.hpp>

#include <algorithm>
#include <cstdint>

namespace dockit {
	struct HTMLSelectionPoint {
		int64_t offset = 0;
		godot::Vector2i scope;
		godot::Vector2i enclosing_all;
		bool excluded = false;

		void translate(int64_t p_offset) {
			offset += p_offset;
			const godot::Vector2i shift(p_offset, p_offset);
			if (scope.x < scope.y) scope += shift;
			if (enclosing_all.x < enclosing_all.y) enclosing_all += shift;
		}
	};

	inline godot::Vector2i html_selection_between(const HTMLSelectionPoint& p_origin, const HTMLSelectionPoint& p_target) {
		int64_t anchor = p_origin.offset;
		int64_t caret = p_target.offset;
		if (p_target.excluded && p_target.scope.x < p_target.scope.y) {
			caret = anchor <= p_target.scope.x ? p_target.scope.x : p_target.scope.y;
		}
		const bool backwards = caret < anchor;
		int64_t first = std::min(anchor, caret);
		int64_t last = std::max(anchor, caret);
		const auto include = [&](const godot::Vector2i& p_range) {
			if (p_range.x >= p_range.y) return;
			first = std::min<int64_t>(first, p_range.x);
			last = std::max<int64_t>(last, p_range.y);
		};
		if (!p_origin.excluded) include(p_origin.scope);
		if (!p_target.excluded) include(p_target.scope);
		if (p_origin.enclosing_all != p_target.enclosing_all) {
			include(p_origin.enclosing_all);
			include(p_target.enclosing_all);
		}
		return backwards ? godot::Vector2i(last, first) : godot::Vector2i(first, last);
	}
}
