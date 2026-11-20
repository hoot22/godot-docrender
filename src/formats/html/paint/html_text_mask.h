#pragma once

#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <litehtml/types.h>

#include <vector>

namespace dockit {
	class HTMLTextLayout;

	struct TextDecorationMask {
		godot::Rect2 rect;
		litehtml::text_decoration_style style;
		float alpha;
	};

	struct HTMLTextMask {
		godot::Ref<godot::Texture2D> texture;
		godot::Rect2 bounds;
	};

	struct TextEmphasisMask {
		godot::Ref<godot::TextLine> line;
		godot::Vector2 position;
		float alpha;
	};

	HTMLTextMask create_html_text_mask(const HTMLTextLayout& p_layout, const godot::Vector2& p_position,
		const std::vector<TextDecorationMask>& p_decorations, const std::vector<TextEmphasisMask>& p_emphasis,
		bool p_mipmaps);
}
