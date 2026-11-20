#ifndef LITEHTML_RENDER_INLINE_CONTEXT_H
#define LITEHTML_RENDER_INLINE_CONTEXT_H

#include "render_block.h"

namespace litehtml
{
	/**
	 * An inline formatting context is established by a block container box that contains no block-level boxes.
	 * https://www.w3.org/TR/CSS22/visuren.html#inline-formatting
	 */
	class render_item_inline_context : public render_item_block
	{
		/**
		 *	Structure contains elements with display: inline
		 *	members:
		 *	- element: 		render_item with display: inline
		 *	- boxes: 		rectangles represented inline element content. There are can be many boxes if content
		 *			 is split into some lines
		 *	- start_box:	the start position of currently calculated box
		 */
		struct inlines_item
		{
			std::shared_ptr<render_item> element;
			position::vector boxes;
			position	start_box;

			explicit inlines_item(const std::shared_ptr<render_item>& el) : element(el) {}
		};
	protected:
		std::vector<std::unique_ptr<litehtml::line_box>> m_line_boxes;
		std::vector<size_t> m_suppressed_hyphen_lines;
		rendered_width									 m_rendered_width;

		rendered_width _render_content(pixel_t x, pixel_t y, bool second_pass,
									   const containing_block_context& self_size, formatting_context* fmt_ctx) override;
		void fix_line_width(element_float flt,
							const containing_block_context &self_size, formatting_context* fmt_ctx) override;

		std::list<std::unique_ptr<line_box_item> > finish_last_box(bool end_of_render, const containing_block_context &self_size);
		void place_inline(std::unique_ptr<line_box_item> item, const containing_block_context &self_size,
			formatting_context* fmt_ctx, bool force_wrap = false);
		void layout_inlines(const containing_block_context& self_size, formatting_context* fmt_ctx,
			const std::unordered_set<const render_item*>* forced_cuts = nullptr);
		void prepare_text_fragments(const containing_block_context& self_size);
		void merge_text_fragments();
		void apply_text_wrap(const containing_block_context& self_size, formatting_context* fmt_ctx, bool protect_final_line);
		bool protects_last_hyphenation(const containing_block_context& self_size) const;
		const render_item* last_full_line_hyphen() const;
		void avoid_last_hyphenation(const containing_block_context& self_size, formatting_context* fmt_ctx,
			const formatting_context& initial_context);
		std::vector<std::vector<pixel_t>> wrap_slack_by_group() const;
		std::pair<pixel_t, pixel_t> line_indentation(const containing_block_context& self_size, bool first, bool after_break) const;
		pixel_t new_box(const std::unique_ptr<line_box_item>& el, const containing_block_context& self_size,
						formatting_context* fmt_ctx);
		void apply_vertical_align() override;
		bool first_formatted_line() const;

	  public:
		explicit render_item_inline_context(std::shared_ptr<element> src_el) :
			render_item_block(std::move(src_el))
		{
		}

		std::shared_ptr<render_item> clone() override
		{
			return std::make_shared<render_item_inline_context>(src_el());
		}

		const auto& line_boxes() const { return m_line_boxes; }
		std::optional<pixel_t> find_layout_baseline(bool last) const override;
		pixel_t baseline_content_height() const override;
		void align_content_to_end() override;
		pixel_t get_first_baseline() override;
		pixel_t get_last_baseline() override;
	};
}

#endif //LITEHTML_RENDER_INLINE_CONTEXT_H
