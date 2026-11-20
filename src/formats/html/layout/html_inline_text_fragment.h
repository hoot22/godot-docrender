#pragma once

#include "formats/html/text/html_text_layout.h"

#include <litehtml/el_text.h>

namespace dockit {
	class HTMLInlineTextFragment final : public litehtml::el_text {
		std::shared_ptr<litehtml::render_item> original;
		std::shared_ptr<const godot::String> rendered;
		int64_t run_start;
		int64_t start;
		int64_t end;
		bool lexical_break_before;
		bool hyphenation_fallback;

	public:
		HTMLInlineTextFragment(const std::shared_ptr<litehtml::render_item>& p_original,
			const std::shared_ptr<const godot::String>& p_rendered, int64_t p_run_start,
			const HTMLTextCluster& p_cluster, bool p_lexical_break_before,
			bool p_hyphenation_fallback = false);
		const std::shared_ptr<litehtml::render_item>& original_renderer() const { return original; }
		const godot::String& shaping_text() const { return *rendered; }
		int64_t shaping_start() const { return start - run_start; }
		int64_t shaping_end() const { return end - run_start; }
		int64_t source_start() const { return start; }
		int64_t source_end() const { return end; }
		bool has_lexical_break_before() const { return lexical_break_before; }
		bool is_automatic_hyphen() const { return start == end; }
		bool is_hyphenation_fallback() const { return hyphenation_fallback; }
		bool shares_shaping_run(const HTMLInlineTextFragment& p_other) const { return rendered == p_other.rendered; }
		void extend_to(int64_t p_end) { end = p_end; }
		void draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y, const litehtml::position* p_clip,
			const std::shared_ptr<litehtml::render_item>& p_item) override;
	};
}
