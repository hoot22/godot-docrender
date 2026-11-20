#ifndef LH_RENDER_ITEM_H
#define LH_RENDER_ITEM_H

#include <optional>
#include <memory>
#include <list>
#include <tuple>
#include "html.h"
#include "types.h"
#include "line_box.h"
#include "table.h"
#include "formatting_context.h"
#include "element.h"
#include "scroll_view.h"

namespace litehtml
{
    class element;

    class render_item : public std::enable_shared_from_this<render_item>
    {
    private:
        struct inline_measurement
        {
            uint64_t generation;
            containing_block_context::typed_pixel height_basis;
            rendered_width content;
        };
        std::unique_ptr<inline_measurement> m_inline_measurement;
        bool fetch_positioned(render_item& stacking_context);
        std::vector<int> m_stacking_levels;

    protected:
        std::shared_ptr<element>                    m_element;
        std::weak_ptr<render_item>                  m_parent;
        std::list<std::shared_ptr<render_item>>     m_children;
        margins						                m_margins;
        margins						                m_padding;
        margins						                m_borders;
        position					                m_pos;
        pixel_t m_relative_offset_x = 0;
        pixel_t m_relative_offset_y = 0;
        bool                                        m_skip;
        bool m_empty_table_cell = false;
        bool m_collapsed_table_track = false;
        bool m_clipped_table_cell = false;
        std::optional<margins> m_shared_table_borders;
        text_justify_adjustment                     m_justification;
        punctuation_trim m_punctuation_trim = punctuation_trim::none;
        std::vector<std::shared_ptr<render_item>>   m_positioned;
    	std::shared_ptr<scroll_view>				m_scroll_view;
		std::optional<position> m_sticky_grid_area;

		containing_block_context calculate_containing_block_context(const containing_block_context& cb_context);
		void calc_cb_length(const css_length& len, containing_block_context::typed_pixel percent_base, containing_block_context::typed_pixel& out_value) const;
		virtual std::optional<pixel_t> find_layout_baseline(bool) const { return std::nullopt; }
		virtual size layout_overflow() const { return {}; }
		void paint_positioned_child(render_item& child, uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, int level);
		virtual rendered_width	 _render(pixel_t /*x*/, pixel_t /*y*/,
										 const containing_block_context& /*containing_block_size*/,
										 formatting_context* /*fmt_ctx*/, bool /*second_pass = false*/)
		{ return {0, 0}; }

	  public:
        explicit render_item(std::shared_ptr<element>  src_el);

        virtual ~render_item() = default;

    	pixel_t get_scroll_left() const
		{
			return m_scroll_view ? m_scroll_view->get_left() : 0;
		}

    	pixel_t get_scroll_top() const
    	{
    		return m_scroll_view ? m_scroll_view->get_top() : 0;
    	}

		std::optional<size> scrollable_content_size() const
		{
			if (!m_scroll_view) return std::nullopt;
			return size(m_pos.width + m_scroll_view->get_max_h_scroll(),
				m_pos.height + m_scroll_view->get_max_v_scroll());
		}

		void scroll_box(position& box) const
        {
        	if (m_scroll_view)
        	{
        		box.x -= m_scroll_view->get_left();
        		box.y -= m_scroll_view->get_top();
        	}
        }

    	pixel_t h_scroll(pixel_t dx)
    	{
    		return m_scroll_view ? m_scroll_view->h_scroll(dx) : 0;
    	}

    	pixel_t v_scroll(pixel_t dy)
    	{
    		return m_scroll_view ? m_scroll_view->v_scroll(dy) : 0;
    	}

		bool is_h_scrollable(const pixel_t dx) const
		{
			return m_scroll_view && css().get_visibility() == visibility_visible &&
				is_interactive_overflow(css().get_overflow_x()) && m_scroll_view->is_h_scrollable(dx);
		}

		bool is_v_scrollable(const pixel_t dy) const
		{
			return m_scroll_view && css().get_visibility() == visibility_visible &&
				is_interactive_overflow(css().get_overflow_y()) && m_scroll_view->is_v_scrollable(dy);
		}

        const std::list<std::shared_ptr<render_item>>& children() const { return m_children; }

        std::list<std::shared_ptr<render_item>>& children()
        {
            return m_children;
        }

    	// Access to the m_pos
        position& pos()
        {
            return m_pos;
        }

    	// Calculates the position of the element in the document
    	// This position is relative to the x and y arguments
    	// Scroll shifts are applied to the position
    	position calc_placement(int x = 0, int y = 0) const
    	{
    		position pos = m_pos;
    		pos.x += x - get_scroll_left();
    		pos.y += y - get_scroll_top();
    		return pos;
    	}

        void set_text_justification(text_justify_adjustment adjustment) { m_justification = adjustment; }
        text_justify_adjustment get_text_justification() const { return m_justification; }
        void set_punctuation_trim(punctuation_trim value) { m_punctuation_trim = value; }
        punctuation_trim get_punctuation_trim() const { return m_punctuation_trim; }

        bool skip() const
        {
            return m_skip;
        }

        void skip(bool val)
        {
            m_skip = val;
        }

        pixel_t right() const
        {
            return left() + width();
        }

        pixel_t left() const
        {
            return m_pos.left() - m_margins.left - m_padding.left - m_borders.left;
        }

        pixel_t top() const
        {
            return m_pos.top() - m_margins.top - m_padding.top - m_borders.top;
        }

        pixel_t bottom() const
        {
            return top() + height();
        }

        pixel_t height() const
        {
            return m_pos.height + m_margins.height() + m_padding.height() + m_borders.height();
        }

        pixel_t width() const
        {
            return m_pos.width + m_margins.width() + m_padding.width() + m_borders.width();
        }

        pixel_t padding_top() const
        {
            return m_padding.top;
        }

        pixel_t padding_bottom() const
        {
            return m_padding.bottom;
        }

        pixel_t padding_left() const
        {
            return m_padding.left;
        }

        pixel_t padding_right() const
        {
            return m_padding.right;
        }

        pixel_t border_top() const
        {
            return m_borders.top;
        }

        pixel_t border_bottom() const
        {
            return m_borders.bottom;
        }

        pixel_t border_left() const
        {
            return m_borders.left;
        }

        pixel_t border_right() const
        {
            return m_borders.right;
        }

        pixel_t margin_top() const
        {
            return m_margins.top;
        }

        pixel_t margin_bottom() const
        {
            return m_margins.bottom;
        }

        pixel_t margin_left() const
        {
            return m_margins.left;
        }

        pixel_t margin_right() const
        {
            return m_margins.right;
        }

        std::shared_ptr<render_item> parent() const
        {
            return m_parent.lock();
        }

        margins& get_margins()
        {
            return m_margins;
        }

        margins& get_paddings()
        {
            return m_padding;
        }

		void set_paddings(const margins& val)
		{
			m_padding = val;
		}

        margins& get_borders()
        {
            return m_borders;
        }

		/**
		 * Top offset to the element content. Includes paddings, margins and borders.
		 */
        pixel_t content_offset_top() const
        {
            return m_margins.top + m_padding.top + m_borders.top;
        }

		/**
		 * Bottom offset to the element content. Includes paddings, margins and borders.
		 */
        pixel_t content_offset_bottom() const
        {
            return m_margins.bottom + m_padding.bottom + m_borders.bottom;
        }

		/**
		 * Left offset to the element content. Includes paddings, margins and borders.
		 */
        pixel_t content_offset_left() const
        {
            return m_margins.left + m_padding.left + m_borders.left;
        }

		/**
		 * Right offset to the element content. Includes paddings, margins and borders.
		 */
        pixel_t content_offset_right() const
        {
            return m_margins.right + m_padding.right + m_borders.right;
        }

		/**
		 * Sum of left and right offsets to the element content. Includes paddings, margins and borders.
		 */
        pixel_t content_offset_width() const
        {
            return content_offset_left() + content_offset_right();
        }

		/**
		 * Sum of top and bottom offsets to the element content. Includes paddings, margins and borders.
		 */
        pixel_t content_offset_height() const
        {
            return content_offset_top() + content_offset_bottom();
        }

		pixel_t render_offset_left() const
		{
			if(css().get_box_sizing() == box_sizing_content_box)
			{
				return m_margins.left + m_borders.left + m_padding.left;
			}
			return m_margins.left;
		}

		pixel_t render_offset_right() const
		{
			if(css().get_box_sizing() == box_sizing_content_box)
			{
				return m_margins.right + m_borders.right + m_padding.right;
			}
			return m_margins.right;
		}

		pixel_t render_offset_width() const
		{
			return render_offset_left() + render_offset_right();
		}

		pixel_t render_offset_top() const
		{
			if(css().get_box_sizing() == box_sizing_content_box)
			{
				return m_margins.top + m_borders.top + m_padding.top;
			}
			return m_margins.top;
		}

		pixel_t render_offset_bottom() const
		{
			if(css().get_box_sizing() == box_sizing_content_box)
			{
				return m_margins.bottom + m_borders.bottom + m_padding.bottom;
			}
			return m_margins.bottom;
		}

		pixel_t render_offset_height() const
		{
			return render_offset_top() + render_offset_bottom();
		}

		pixel_t box_sizing_left() const
		{
			if(css().get_box_sizing() == box_sizing_border_box)
			{
				return m_padding.left + m_borders.left;
			}
			return 0;
		}

		pixel_t box_sizing_right() const
		{
			if(css().get_box_sizing() == box_sizing_border_box)
			{
				return m_padding.right + m_borders.right;
			}
			return 0;
		}

		pixel_t box_sizing_width() const
		{
			return box_sizing_left() + box_sizing_right();
		}

		pixel_t box_sizing_top() const
		{
			if(css().get_box_sizing() == box_sizing_border_box)
			{
				return m_padding.top + m_borders.top;
			}
			return 0;
		}

		pixel_t box_sizing_bottom() const
		{
			if(css().get_box_sizing() == box_sizing_border_box)
			{
				return m_padding.bottom + m_borders.bottom;
			}
			return 0;
		}

		pixel_t box_sizing_height() const
		{
			return box_sizing_top() + box_sizing_bottom();
		}

        void parent(const std::shared_ptr<render_item>& par)
        {
            m_parent = par;
        }

        const std::shared_ptr<element>& src_el() const
        {
            return m_element;
        }

		const css_properties& css() const
		{
			return m_element->css();
		}

        void add_child(const std::shared_ptr<render_item>& ri)
        {
            m_children.push_back(ri);
            ri->parent(shared_from_this());
        }

		bool is_root() const
		{
			return m_parent.expired();
		}

        bool collapse_top_margin() const
        {
            return m_borders.top == 0 &&
                   m_padding.top == 0 &&
                   m_element->in_normal_flow() &&
                   m_element->css().get_float() == float_none &&
                   !is_grid_display(css().get_display()) &&
                   !is_grid_item() &&
                   m_margins.top >= 0 &&
				   !is_flex_item() &&
                   !is_root() &&
                   !css().is_flow_root() && !css().is_scroll_container() && !css().has_box_containment() &&
                   !css().establishes_query_container_context();
        }

        bool collapse_bottom_margin() const
        {
            return m_borders.bottom == 0 &&
                   m_padding.bottom == 0 &&
                   !has_preferred_aspect_ratio() &&
                   !css().has_block_size_containment() &&
                   m_element->in_normal_flow() &&
                   m_element->css().get_float() == float_none &&
                   !is_grid_display(css().get_display()) &&
                   !is_grid_item() &&
                   m_margins.bottom >= 0 &&
                   !is_root() &&
                   !css().is_flow_root() && !css().is_scroll_container() && !css().has_box_containment() &&
                   !css().establishes_query_container_context();
        }

		bool clips_overflow() const
		{
			const auto display = css().get_display();
			return css().clips_overflow() && display != display_inline && display != display_inline_text;
		}

		position overflow_clip_box(const position& content_box) const;

        bool empty_table_cell() const { return m_empty_table_cell; }
        void set_empty_table_cell(bool value) { m_empty_table_cell = value; }
        bool collapsed_table_track() const { return m_collapsed_table_track; }
        void set_table_track_geometry(bool collapsed, bool clipped = false)
        {
            m_collapsed_table_track = collapsed;
            m_clipped_table_cell = clipped;
        }
        bool has_table_cell_clip() const { return m_clipped_table_cell; }
        bool shares_table_borders() const { return m_shared_table_borders.has_value(); }
        void set_shared_table_borders(margins value) { m_shared_table_borders = m_borders = value; }

        bool is_visible() const
        {
            return !(m_skip || m_collapsed_table_track || src_el()->css().get_display() == display_none || src_el()->css().get_visibility() != visibility_visible);
        }

		void paint(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip);
		void paint_group(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, bool with_positioned);
		bool establishes_effect_containing_block() const {
			if (css().has_transform() || css().has_perspective() || css().specifies_preserve_3d() ||
				css().has_box_containment() || css().anticipates_transform() ||
				(css().allows_box_containment() && css().has_will_change(will_change_containment)) ||
				(css().allows_size_containment() && css().has_will_change(will_change_content_visibility)) ||
				(!is_root() && (css().has_filters() || css().has_backdrop_filters() ||
					css().has_will_change(will_change_filter)))) return true;
			if (!css().is_transformable() || (!css().hides_backface() && !css().has_will_change(will_change_backface))) return false;
			const auto containing = parent();
			return containing && containing->css().preserves_3d();
		}
		bool establishes_positioned_containing_block() const {
			return src_el()->is_positioned() || css().has_will_change(will_change_position) ||
				establishes_effect_containing_block();
		}
		bool establishes_item_stacking_context() const {
			return css().has_z_index() && (is_grid_item() || (src_el()->in_normal_flow() && is_flex_item()));
		}
		bool establishes_stacking_context() const {
			const auto position = css().get_position();
			const bool z_hint = css().has_will_change(will_change_z_index) &&
				(src_el()->is_positioned() || is_grid_item() || (src_el()->in_normal_flow() && is_flex_item()));
			return is_root() || establishes_effect_containing_block() || establishes_item_stacking_context() ||
				css().has_group_effects() || position == element_position_fixed || position == element_position_sticky ||
				(src_el()->is_positioned() && css().has_z_index()) || z_hint || css().has_will_change(will_change_position) ||
				css().has_will_change(will_change_isolation) || css().has_will_change(will_change_backdrop);
		}
		bool establishes_paint_context() const { return src_el()->is_positioned() || establishes_stacking_context(); }
		int paint_z_index() const { return src_el()->is_positioned() || establishes_item_stacking_context() ? css().get_z_index() : 0; }
		std::shared_ptr<render_item> fixed_containing_block() const;
		element_float get_used_float() const;
		element_clear get_used_clear() const;
		bool is_viewport_fixed() const;
		void extend_scroll_size(const size& extent);
		void set_sticky_grid_area(const position& area) { m_sticky_grid_area = area; }
		const std::optional<position>& sticky_grid_area() const { return m_sticky_grid_area; }
		size used_layout_overflow() const { return layout_overflow(); }

		bool is_grid_item() const
		{
			auto p = parent();
			return p && is_grid_display(p->css().get_display()) && src_el()->in_normal_flow();
		}

		bool is_flex_item() const
		{
			auto par = parent();
			if(par && (par->css().get_display() == display_inline_flex || par->css().get_display() == display_flex))
			{
				return true;
			}
			return false;
		}

		bool has_preferred_aspect_ratio() const;
		pixel_t size_from_aspect_ratio(pixel_t source, css_aspect_ratio ratio, bool to_width) const;
		intrinsic_inline_sizes resolve_intrinsic_inline_sizes(const containing_block_context& enclosing,
			std::optional<pixel_t> available_width = std::nullopt);
		rendered_width render(pixel_t x, pixel_t y, const containing_block_context& containing_block_size,
							  formatting_context* fmt_ctx, bool second_pass = false);
		void apply_relative_shift(const containing_block_context &containing_block_size);
        void calc_outlines( pixel_t parent_width );
        pixel_t calc_auto_margins(pixel_t parent_width);	// returns left margin

        virtual std::shared_ptr<render_item> init();
        virtual void apply_vertical_align() {}
		// Layout baselines use the top of the margin box as their origin.
		std::optional<pixel_t> layout_baseline(bool last) const
		{
			if (css().has_layout_containment()) return std::nullopt;
			auto baseline = find_layout_baseline(last);
			if (baseline && css().is_scroll_container())
				*baseline = std::clamp(*baseline, margin_top(), std::max(margin_top(), height() - margin_bottom()));
			return baseline;
		}
		pixel_t baseline_or_border_bottom(bool last) const
		{
			return layout_baseline(last).value_or(height() - margin_bottom());
		}
		virtual pixel_t baseline_content_height() const { return m_pos.height; }
		virtual pixel_t get_inline_baseline() { return css().has_layout_containment() ? height() : get_last_baseline(); }

		/**
		 * Get first baseline position. Default position is element bottom without bottom margin.
		 * @returns offset of the first baseline from element top
		 */
		virtual pixel_t get_first_baseline() { return height() - margin_bottom(); }
		/**
		 * Get the last baseline position.  The default position is element bottom without bottom margin.
		 * @returns offset of the last baseline from element top
		 */
		virtual pixel_t get_last_baseline() { return height() - margin_bottom(); }

        virtual std::shared_ptr<render_item> clone()
        {
            return std::make_shared<render_item>(src_el());
        }
        std::tuple<
                std::shared_ptr<litehtml::render_item>,
                std::shared_ptr<litehtml::render_item>,
                std::shared_ptr<litehtml::render_item>
                > split_inlines();
        bool fetch_positioned();
        void render_positioned(render_type rt = render_all);
		// returns element offset related to the containing block
		std::tuple<pixel_t, pixel_t> element_static_offset(const std::shared_ptr<litehtml::render_item> &el);
        void add_positioned(const std::shared_ptr<litehtml::render_item> &el);
        void get_redraw_box(litehtml::position& pos, pixel_t x = 0, pixel_t y = 0);
        void calc_document_size( litehtml::size& sz, pixel_t x = 0, pixel_t y = 0 );
		virtual void get_inline_boxes( position::vector& /*boxes*/ ) const {};
		virtual void set_inline_boxes( position::vector& /*boxes*/ ) {};
		virtual void add_inline_box( const position& /*box*/ ) {};
		virtual void clear_inline_boxes() {};
        pixel_t relative_offset_x() const { return m_relative_offset_x; }
        pixel_t relative_offset_y() const { return m_relative_offset_y; }
        void draw_stacking_context( uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, bool with_positioned );
        virtual void draw_children( uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, draw_flag flag, int zindex );
        virtual pixel_t get_draw_vertical_offset() { return 0; }
        virtual std::shared_ptr<element> get_child_by_point(pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y, draw_flag flag, int zindex,
        	const std::function<bool(const std::shared_ptr<render_item>&)>& check);
        std::shared_ptr<element> get_element_by_point(pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y,
            const std::function<bool(const std::shared_ptr<render_item>&)>& check, bool with_positioned = true);
        bool is_point_inside( pixel_t x, pixel_t y ) const;
        void dump(litehtml::dumper& cout);
		position get_placement() const;
        virtual void y_shift(pixel_t shift);
        /**
         * Returns the boxes of rendering element. All coordinates are absolute
         *
         * @param redraw_boxes [out] resulting rendering boxes
         * @return
         */
        void get_rendering_boxes( position::vector& redraw_boxes) const;
	};
}

#endif //LH_RENDER_ITEM_H
