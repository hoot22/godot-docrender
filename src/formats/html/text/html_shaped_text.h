#pragma once

#include "html_text_layout.h"

#include <litehtml/types.h>
#include <litehtml/web_color.h>
#include <memory>
#include <vector>

namespace godot {
	class TextServer;
}

namespace dockit {
	bool has_text_justification_gap(litehtml::text_justify p_method, char32_t p_left, char32_t p_right);

	class HTMLTextSpacing {
		struct Glyph {
			godot::RID font;
			godot::Vector2 position;
			int64_t index;
			int32_t size;
			bool missing;
		};

		std::vector<Glyph> glyphs;
		std::vector<float> glyph_offsets;
		std::vector<godot::Vector2> boundaries;
		float width = 0;

		float map(float p_x, bool p_inverse) const;

	public:
		static litehtml::text_justify_info get_justification_info(const HTMLTextLayout& p_layout, const godot::String& p_text,
			litehtml::text_justify p_method);
		static std::shared_ptr<const HTMLTextSpacing> create(const HTMLTextLayout& p_layout, const godot::String& p_text,
			float p_letters, float p_words, litehtml::text_justify_adjustment p_justification = {});
		float get_width() const { return width; }
		float glyph_offset(size_t p_index) const { return glyph_offsets[p_index]; }
		float from_native(float p_x) const { return map(p_x, false); }
		float to_native(float p_x) const { return map(p_x, true); }
		void draw(const godot::TextServer& p_server, const godot::RID& p_canvas, const godot::Vector2& p_baseline,
			const godot::Color& p_color) const;
	};

	struct HTMLShapedText {
		HTMLTextLayout layout;
		int decoration_line;
		litehtml::text_decoration_style decoration_style;
		litehtml::web_color decoration_color;
		float decoration_thickness;
		godot::Ref<godot::TextLine> emphasis;
		const std::vector<float>* emphasis_centers;
		litehtml::web_color emphasis_color;
		bool emphasis_under;
	};
}
