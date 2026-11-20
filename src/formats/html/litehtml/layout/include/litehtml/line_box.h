#ifndef LH_LINE_BOX_H
#define LH_LINE_BOX_H

#include <optional>
#include <unordered_set>
#include <memory>
#include "css_properties.h"
#include "types.h"

namespace litehtml
{
    class render_item;

    struct line_context
    {
        pixel_t calculatedTop;
        pixel_t top;
        pixel_t left;
        pixel_t right;

        pixel_t width() const
        {
            return right - left;
        }
        void fix_top()
        {
            calculatedTop = top;
        }
		line_context() : calculatedTop(0), top(0), left(0), right(0) {}
    };

	class line_box_item
	{
	public:
		enum element_type
		{
			type_text_part,
			type_inline_start,
			type_inline_continue,
			type_inline_end
		};
	protected:
		std::shared_ptr<render_item> m_element;
		std::optional<pixel_t> m_rendered_natural_width;
		pixel_t m_rendered_min_width = 0;
		pixel_t m_items_top = 0;
		pixel_t m_items_bottom = 0;
		pixel_t m_spacing_before = 0;
		pixel_t m_autospace_before = 0;
		pixel_t m_punctuation_reduction = 0;
	public:
		explicit line_box_item(const std::shared_ptr<render_item>& element) : m_element(element) {}
		line_box_item(const line_box_item& el) = default;
		line_box_item(line_box_item&&) = default;
		virtual ~line_box_item();

		virtual pixel_t height() const;
		const std::shared_ptr<render_item>& get_el() const { return m_element; }
		virtual position& pos();
		virtual void place_to(pixel_t x, pixel_t y);
		virtual pixel_t width() const;
		void set_spacing_before(pixel_t spacing, pixel_t autospace)
		{
			m_spacing_before = spacing;
			m_autospace_before = autospace;
		}
		pixel_t autospace_before() const { return m_autospace_before; }
		pixel_t untrimmed_width() const { return width() + m_punctuation_reduction; }
		pixel_t punctuation_reduction() const { return m_punctuation_reduction; }
		void set_punctuation_reduction(punctuation_trim trim, pixel_t reduction);
		pixel_t spacing_before() const { return m_spacing_before; }
		pixel_t occupied_width() const { return width() + m_spacing_before; }
		virtual pixel_t top() const;
		virtual pixel_t bottom() const;
		virtual pixel_t right() const;
		virtual pixel_t left() const;
		virtual element_type get_type() const	{ return type_text_part; }
		virtual pixel_t get_rendered_min_width() const	{ return m_rendered_min_width - m_punctuation_reduction; }
		virtual void set_rendered_min_width(pixel_t min_width) { m_rendered_min_width = min_width; }
		pixel_t get_rendered_natural_width() const
		{
			return m_rendered_natural_width ? *m_rendered_natural_width - m_punctuation_reduction : get_rendered_min_width();
		}
		void set_rendered_widths(rendered_width widths)
		{
			m_rendered_natural_width = widths.natural_width;
			set_rendered_min_width(widths.min_width);
		}
		virtual void y_shift(pixel_t shift);

		void reset_items_height() { m_items_top = m_items_bottom = 0; }
		void add_item_height(pixel_t item_top, pixel_t item_bottom)
		{
			m_items_top = std::min(m_items_top, item_top);
			m_items_bottom = std::max(m_items_bottom, item_bottom);
		}
		pixel_t get_items_top() const { return m_items_top; }
		pixel_t get_items_bottom() const { return m_items_bottom; }
	};

	class lbi_start : public line_box_item
	{
	protected:
		position m_pos;
	public:
		explicit lbi_start(const std::shared_ptr<render_item>& element);
		~lbi_start() override;

		void place_to(pixel_t x, pixel_t y) override;
		pixel_t height() const override;
		pixel_t width() const override;
		position& pos() override { return m_pos; }
		pixel_t top() const override;
		pixel_t bottom() const override;
		pixel_t right() const override;
		pixel_t left() const override;
		element_type get_type() const override	{ return type_inline_start; }
		pixel_t get_rendered_min_width() const override { return width(); }
	};

	class lbi_end : public lbi_start
	{
	public:
		explicit lbi_end(const std::shared_ptr<render_item>& element);
		virtual ~lbi_end() override;

		void place_to(pixel_t x, pixel_t y) override;
		pixel_t right() const override;
		pixel_t left() const override;
		element_type get_type() const override	{ return type_inline_end; }
		void y_shift(pixel_t) override {}
	};

	class lbi_continue : public lbi_start
	{
	public:
		explicit lbi_continue(const std::shared_ptr<render_item>& element);
		virtual ~lbi_continue() override;

		void place_to(pixel_t x, pixel_t y) override;
		pixel_t right() const override;
		pixel_t left() const override;
		pixel_t width() const override;
		element_type get_type() const override	{ return type_inline_continue; }
	};

	class line_box
    {
		friend class render_item_inline_context;
		struct va_context
		{
			pixel_t			line_height = 0;
			pixel_t			baseline = 0;
			font_metrics 	fm;
			line_box_item*	start_lbi = nullptr;
		};

        pixel_t					m_top;
        pixel_t					m_left;
        pixel_t					m_right;
        pixel_t					m_height;
        pixel_t					m_width;
		css_line_height_t		m_default_line_height;
        font_metrics			m_font_metrics;
        pixel_t					m_baseline;
        text_align				m_text_align;
        text_align				m_text_align_last;
		pixel_t m_indent;
		pixel_t m_intrinsic_indent;
		pixel_t m_wrap_slack;
		uint_ptr m_font;
		bool m_right_to_left;
		bool m_allow_overflow_wrap;
		bool m_paragraph_start;
		bool m_finished = false;
		bool m_ended_with_break = false;
		css_hyphenation_limits m_hyphenation_limits;
		pixel_t m_hyphenation_zone;
		size_t m_preceding_hyphenated_lines;
		bool m_suppress_hyphenation;
		const render_item* m_hyphenated_at = nullptr;
		rendered_width			m_rendered_width;

		using item_list = std::list<std::unique_ptr<line_box_item>>;
		item_list m_items;
		item_list::iterator m_last_wrap_point = m_items.end();
		line_box_item* m_last_wrap_hyphen = nullptr;
		pixel_t m_line_end_hyphen = 0;
		std::optional<pixel_t> m_ordinary_wrap_end;
		item_list::iterator m_last_overflow_wrap_point = m_items.end();
		enum class wrap_opportunity { none, normal, hyphen, overflow, overflow_intrinsic };
    public:
		line_box(pixel_t top, pixel_t left, pixel_t right, const css_properties& style,
				 pixel_t indent, pixel_t intrinsic_indent, bool allow_overflow_wrap, bool paragraph_start,
				 size_t preceding_hyphenated_lines, bool suppress_hyphenation) :
			m_top(top),
			m_left(left + indent),
			m_right(right),
			m_height(0),
			m_width(0),
			m_default_line_height(style.line_height()),
			m_font_metrics(style.get_font_metrics()),
			m_baseline(0),
			m_text_align(style.get_text_align()),
			m_text_align_last(style.get_text_align_last()),
			m_indent(indent),
			m_intrinsic_indent(intrinsic_indent),
			m_wrap_slack(right - left - indent),
			m_font(style.get_font()),
			m_right_to_left(style.is_right_to_left()),
			m_allow_overflow_wrap(allow_overflow_wrap),
			m_paragraph_start(paragraph_start),
			m_hyphenation_limits(style.get_hyphenation_limits()),
			m_hyphenation_zone(m_hyphenation_limits.zone.calc_percent(right - left)),
			m_preceding_hyphenated_lines(preceding_hyphenated_lines),
			m_suppress_hyphenation(suppress_hyphenation)
		{
		}

		pixel_t		   bottom() const { return m_top + height(); }
		pixel_t		   top() const { return m_top; }
		pixel_t		   right() const { return m_left + width(); }
		pixel_t		   left() const { return m_left; }
		pixel_t		   height() const { return m_height; }
		pixel_t		   width() const { return m_width; }
		pixel_t		   line_right() const { return m_right; }
		rendered_width get_rendered_width() const { return m_rendered_width; }

		void	add_item(std::unique_ptr<line_box_item> item);
		std::optional<item_list> wrap_before(const line_box_item& item, bool force_wrap = false);
		bool	is_empty() const;
		bool ends_with_forced_break() const;
		size_t consecutive_hyphenated_lines() const { return m_hyphenated_at ? m_preceding_hyphenated_lines + 1 : 0; }
		pixel_t baseline() const;
		pixel_t top_margin() const;
		pixel_t bottom_margin() const;
		void shift_for_content_alignment(pixel_t shift, std::unordered_set<render_item*>& shifted);
		void	y_shift(pixel_t shift);
		
		std::list< std::unique_ptr<line_box_item> >	finish(bool last_box, const containing_block_context &containing_block_size);
		std::list< std::unique_ptr<line_box_item> > new_width(pixel_t left, pixel_t right);
		std::shared_ptr<render_item> 		get_last_text_part() const;
		std::shared_ptr<render_item> 		get_first_text_part() const;
		std::list< std::unique_ptr<line_box_item> >& 	items() { return m_items; }
		const item_list& items() const { return m_items; }
	private:
		struct punctuation_update
		{
			line_box_item* previous = nullptr;
			pixel_t previous_reduction = 0;
			pixel_t next_reduction = 0;
			pixel_t possible_end_reduction = 0;
			punctuation_trim next_trim = punctuation_trim::none;
		};
		punctuation_update punctuation_before(const line_box_item& item) const;
		void apply_punctuation_trim(line_box_item& item, punctuation_trim trim, pixel_t reduction);
		void trim_line_end(pixel_t hanging_width);
		pixel_t spacing_before(const line_box_item& item, pixel_t* autospace = nullptr) const;
		pixel_t text_tab_width(const line_box_item& item, pixel_t cursor) const;
		void measure_intrinsic_width(pixel_t hanging_width);
		void measure_tab(line_box_item& item) const;
		void remember_wrap_point(item_list::iterator current);
		static bool is_hanging_space(const element& source);
		static wrap_opportunity wrap_between(const element& before, const element& after, const element* prefix = nullptr);
		static const element* hyphen_prefix(item_list::const_reverse_iterator prior, item_list::const_reverse_iterator end);
		pixel_t ordinary_wrap_end(item_list::iterator boundary) const;
		bool fits_hyphen(const line_box_item& item, item_list::iterator boundary) const;
		void apply_line_end_hyphen(line_box_item& item);
		static pixel_t fragment_spacing(const render_item& previous, pixel_t previous_width,
			const render_item& next, pixel_t next_width);
		pixel_t trailing_hanging_width() const;
		void align_horizontal(bool terminal_line, pixel_t hanging_width);
		void reorder_bidi_text();
		bool justify(pixel_t available);
        bool				have_last_space() const;
        bool				is_break_only() const;
		static pixel_t		calc_va_baseline(const va_context& current, vertical_align va, const font_metrics& new_font, pixel_t top, pixel_t bottom);
    };
}

#endif //LH_LINE_BOX_H
