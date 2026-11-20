#ifndef LITEHTML_RENDER_TABLE_H
#define LITEHTML_RENDER_TABLE_H

#include "render_item.h"

namespace litehtml
{
	class render_item_table : public render_item
	{
		struct column_range {
			std::shared_ptr<element> source;
			int begin;
			int end;
		};
		struct row_group_range {
			std::shared_ptr<render_item> item;
			int begin;
			int end;
		};
		std::vector<column_range> m_column_groups;
		std::vector<column_range> m_declared_columns;
		std::vector<row_group_range> m_row_groups;
		struct shared_edge {
			int begin;
			int end;
			border stroke;
		};
		using edge_lines = std::map<int, std::vector<shared_edge>>;
		edge_lines m_horizontal_edges;
		edge_lines m_vertical_edges;
		std::vector<bool> m_collapsed_columns;
		std::vector<bool> m_collapsed_rows;
		std::vector<int> m_column_lines;
		std::vector<int> m_row_lines;

		int collect_columns();
		void collect_collapsed_tracks();
		void resolve_shared_borders();
		void paint_shared_borders(uint_ptr hdc, const position& offset) const;
		pixel_t resolve_fixed_columns(pixel_t width, int first_row);
		void apply_column_widths(pixel_t available_width, bool intrinsic_probe);
		void paint_table_backgrounds(uint_ptr hdc, const position& offset) const;

	protected:
		std::optional<pixel_t> find_layout_baseline(bool last) const override;
		size layout_overflow() const override;
		// data for table rendering
		std::unique_ptr<table_grid>	m_grid;
		pixel_t						m_border_spacing_x;
		pixel_t						m_border_spacing_y;

		rendered_width _render(pixel_t x, pixel_t y, const containing_block_context& containing_block_size,
							   formatting_context* fmt_ctx, bool second_pass) override;

	public:
		explicit render_item_table(std::shared_ptr<element>  src_el);

		std::shared_ptr<render_item> clone() override
		{
			return std::make_shared<render_item_table>(src_el());
		}
		void draw_children(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, draw_flag flag, int zindex) override;
		pixel_t get_draw_vertical_offset() override;
		std::shared_ptr<render_item> init() override;
	};

	class render_item_table_part : public render_item
	{
	public:
		explicit render_item_table_part(std::shared_ptr<element>  src_el) : render_item(std::move(src_el))
		{}

		std::shared_ptr<render_item> clone() override
		{
			return std::make_shared<render_item_table_part>(src_el());
		}
		void get_inline_boxes(position::vector& boxes) const override;
	};

	class render_item_table_row : public render_item
	{
	public:
		explicit render_item_table_row(std::shared_ptr<element>  src_el) : render_item(std::move(src_el))
		{}

		std::shared_ptr<render_item> clone() override
		{
			return std::make_shared<render_item_table_row>(src_el());
		}
		void get_inline_boxes( position::vector& boxes ) const override;
	};
}

#endif //LITEHTML_RENDER_TABLE_H
