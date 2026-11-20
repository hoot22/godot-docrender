#pragma once

#include <litehtml/render_block.h>
#include <cmath>

namespace dockit {
	// Controls paint their current label themselves, but its intrinsic width still participates in layout.
	class HTMLControlRenderItem final : public litehtml::render_item_block {
		litehtml::size intrinsic;

	public:
		enum class Baseline { text, swatch };
		static litehtml::pixel_t text_baseline(litehtml::pixel_t p_height, const litehtml::font_metrics& p_font) {
			return (p_height + p_font.ascent - p_font.descent) / 2;
		}
		static litehtml::pixel_t paint_text_baseline(litehtml::pixel_t p_y, litehtml::pixel_t p_height,
			const litehtml::font_metrics& p_font, litehtml::pixel_t p_ascent) {
			// Inline text snaps the top of its laid-out text box before recording the glyphs.
			return std::round(p_y + text_baseline(p_height, p_font) - p_ascent) + p_ascent;
		}

	protected:
		Baseline baseline;

		std::optional<litehtml::pixel_t> find_layout_baseline(bool) const override {
			if (baseline == Baseline::swatch) {
				// The inline color control uses the bottom of its swatch wrapper.
				return content_offset_top() + std::max<litehtml::pixel_t>(0, m_pos.height - 4);
			}
			return content_offset_top() + text_baseline(m_pos.height, css().get_font_metrics());
		}

		litehtml::rendered_width _render_content(litehtml::pixel_t, litehtml::pixel_t, bool,
			const litehtml::containing_block_context&, litehtml::formatting_context*) override {
			m_pos.height = intrinsic.height;
			return {intrinsic.width, intrinsic.width};
		}

	public:
		HTMLControlRenderItem(const litehtml::element::ptr& p_element, litehtml::size p_intrinsic,
			Baseline p_baseline = Baseline::text) :
			render_item_block(p_element), intrinsic(p_intrinsic), baseline(p_baseline) {}

		litehtml::pixel_t get_first_baseline() override { return baseline_or_border_bottom(false); }
		litehtml::pixel_t get_last_baseline() override { return baseline_or_border_bottom(true); }

		std::shared_ptr<litehtml::render_item> init() override {
			src_el()->add_render(shared_from_this());
			return shared_from_this();
		}

		std::shared_ptr<litehtml::render_item> clone() override {
			return std::make_shared<HTMLControlRenderItem>(src_el(), intrinsic, baseline);
		}
	};
}
