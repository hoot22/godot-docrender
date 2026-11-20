#include "html.h"
#include "line_box.h"
#include "formats/html/layout/html_line_break.h"
#include "formats/html/layout/html_text_autospace.h"
#include "formats/html/css/css_font_description.h"
#include "element.h"
#include "render_item.h"
#include "types.h"
#include <litehtml/document.h>
#include <litehtml/document_container.h>
#include <litehtml/el_text.h>
#include <litehtml/utf8_strings.h>
#include <algorithm>
#include <limits>
#include <string>

//////////////////////////////////////////////////////////////////////////////////////////

litehtml::line_box_item::~line_box_item() = default;

void litehtml::line_box_item::place_to(pixel_t x, pixel_t y)
{
	m_element->pos().x = x + m_element->content_offset_left();
	m_element->pos().y = y + m_element->content_offset_top();
}

void litehtml::line_box_item::set_punctuation_reduction(punctuation_trim trim, pixel_t reduction)
{
	if (m_punctuation_reduction != reduction) pos().width += m_punctuation_reduction - reduction;
	m_punctuation_reduction = reduction;
	m_element->set_punctuation_trim(reduction > 0 ? trim : punctuation_trim::none);
}

litehtml::position& litehtml::line_box_item::pos()
{
	return m_element->pos();
}


litehtml::pixel_t litehtml::line_box_item::width() const
{
	return m_element->width();
}

litehtml::pixel_t litehtml::line_box_item::top() const
{
	return m_element->top();
}

litehtml::pixel_t litehtml::line_box_item::bottom() const
{
	return m_element->bottom();
}

litehtml::pixel_t litehtml::line_box_item::right() const
{
	return m_element->right();
}

litehtml::pixel_t litehtml::line_box_item::left() const
{
	return m_element->left();
}

litehtml::pixel_t litehtml::line_box_item::height() const
{
	return m_element->height();
}

void litehtml::line_box_item::y_shift(pixel_t shift)
{
	m_element->y_shift(shift);
}

//////////////////////////////////////////////////////////////////////////////////////////

litehtml::lbi_start::lbi_start(const std::shared_ptr<render_item>& element) : line_box_item(element)
{
	m_pos.height = m_element->src_el()->css().get_font_metrics().height;
	m_pos.width = m_element->content_offset_left();
}

litehtml::lbi_start::~lbi_start() = default;

void litehtml::lbi_start::place_to(pixel_t x, pixel_t y)
{
	m_pos.x = x + m_element->content_offset_left();
	m_pos.y = y;
}

litehtml::pixel_t litehtml::lbi_start::width() const
{
	return m_pos.width;
}

litehtml::pixel_t litehtml::lbi_start::top() const
{
	return m_pos.y;
}

litehtml::pixel_t litehtml::lbi_start::bottom() const
{
	return m_pos.y + m_pos.height;
}

litehtml::pixel_t litehtml::lbi_start::right() const
{
	return m_pos.x;
}

litehtml::pixel_t litehtml::lbi_start::left() const
{
	return m_pos.x - m_element->content_offset_left();
}

litehtml::pixel_t litehtml::lbi_start::height() const
{
	return m_pos.height;
}

//////////////////////////////////////////////////////////////////////////////////////////

litehtml::lbi_end::lbi_end(const std::shared_ptr<render_item>& element) : lbi_start(element)
{
	m_pos.height = m_element->src_el()->css().get_font_metrics().height;
	m_pos.width = m_element->content_offset_right();
}

litehtml::lbi_end::~lbi_end() = default;

void litehtml::lbi_end::place_to(pixel_t x, pixel_t y)
{
	m_pos.x = x;
	m_pos.y = y;
}

litehtml::pixel_t litehtml::lbi_end::right() const
{
	return m_pos.x + m_pos.width;
}

litehtml::pixel_t litehtml::lbi_end::left() const
{
	return m_pos.x;
}

//////////////////////////////////////////////////////////////////////////////////////////

litehtml::lbi_continue::lbi_continue(const std::shared_ptr<render_item>& element) : lbi_start(element)
{
	m_pos.height = m_element->src_el()->css().get_font_metrics().height;
	m_pos.width = 0;
}

litehtml::lbi_continue::~lbi_continue() = default;

void litehtml::lbi_continue::place_to(pixel_t x, pixel_t y)
{
	m_pos.x = x;
	m_pos.y = y;
}

litehtml::pixel_t litehtml::lbi_continue::right() const
{
	return m_pos.x;
}

litehtml::pixel_t litehtml::lbi_continue::left() const
{
	return m_pos.x;
}

litehtml::pixel_t litehtml::lbi_continue::width() const
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

litehtml::pixel_t litehtml::line_box::spacing_before(const line_box_item& item, pixel_t* autospace) const
{
	if (autospace) *autospace = 0;
	const auto& next = item.get_el();
	if (item.get_type() != line_box_item::type_text_part || next->width() <= 0 || next->src_el()->is_break() ||
		next->src_el()->is_preserved_tab() || !next->src_el()->is_inline()) return 0;
	bool has_edge = false;
	for (auto it = m_items.rbegin(); it != m_items.rend(); ++it)
	{
		const auto& previous = (*it)->get_el();
		const auto type = (*it)->get_type();
		if (type != line_box_item::type_text_part)
		{
			if (type != line_box_item::type_inline_continue)
				has_edge |= separates_autospace(*previous, type == line_box_item::type_inline_start, m_right - m_left);
			continue;
		}
		if (previous->skip()) continue;
		if (previous->src_el()->is_break() || previous->src_el()->is_preserved_tab() || !previous->src_el()->is_inline()) break;
		if (previous->width() <= 0) continue;
		const auto gap = has_edge ? 0 : text_autospace_gap(*previous->src_el(), *next->src_el());
		if (autospace) *autospace = gap;
		return fragment_spacing(*previous, previous->width(), *next, next->width()) + gap;
	}
	return 0;
}

void litehtml::line_box::add_item(std::unique_ptr<line_box_item> item)
{
    item->get_el()->skip(false);
    bool add	= true;
	switch (item->get_type())
	{
		case line_box_item::type_text_part:
			if(item->get_el()->src_el()->is_white_space())
			{
				add = !is_empty() && !have_last_space();
			}
			break;
		case line_box_item::type_inline_start:
		case line_box_item::type_inline_end:
		case line_box_item::type_inline_continue:
			add = true;
			break;
	}
	if(add)
	{
		const auto punctuation = punctuation_before(*item);
		if (item->get_type() == line_box_item::type_text_part)
			item->set_punctuation_reduction(punctuation.next_trim, punctuation.next_reduction);
		if (punctuation.previous)
			apply_punctuation_trim(*punctuation.previous, punctuation_trim::right, punctuation.previous_reduction);
		measure_tab(*item);
		pixel_t autospace = 0;
		const auto spacing = spacing_before(*item, &autospace);
		item->set_spacing_before(spacing, autospace);
		item->place_to(m_left + m_width + item->spacing_before(), m_top);
		m_width += item->occupied_width();
		m_height = std::max(m_height, item->get_el()->height());
		m_items.emplace_back(std::move(item));
		remember_wrap_point(std::prev(m_items.end()));
	} else
	{
		item->get_el()->skip(true);
	}
}

litehtml::pixel_t litehtml::line_box::calc_va_baseline(const va_context& current, vertical_align va, const font_metrics& new_font, pixel_t top, pixel_t bottom)
{
	switch(va)
	{
		case va_super:
			return current.baseline - current.fm.super_shift;
		case va_sub:
			return current.baseline + current.fm.sub_shift;
		case va_middle:
			return current.baseline - current.fm.x_height / 2;
		case va_text_top:
			return current.baseline - (current.fm.ascent - current.fm.half_line_gap) +
				(new_font.ascent - new_font.half_line_gap);
		case va_text_bottom:
			return current.baseline + (current.fm.descent - current.fm.half_line_gap) -
				(new_font.descent - new_font.half_line_gap);
		case va_bottom:
			return bottom - new_font.base_line();
		case va_top:
			return top + new_font.height - new_font.base_line();
		default:
			return current.baseline;
	}
}

std::list< std::unique_ptr<litehtml::line_box_item> > litehtml::line_box::finish(bool last_box, const containing_block_context &containing_block_size)
{
	std::list< std::unique_ptr<line_box_item> > ret_items;
	if (m_finished) return ret_items;
	m_ended_with_break = ends_with_forced_break();
	m_finished = true;

	if(!last_box)
	{
		while(!m_items.empty())
		{
			if (m_items.back()->get_type() == line_box_item::type_text_part)
			{
				// remove trailing spaces
				if (m_items.back()->get_el()->src_el()->is_break() ||
					m_items.back()->get_el()->src_el()->is_white_space())
				{
					m_width -= m_items.back()->occupied_width();
					m_items.back()->get_el()->skip(true);
					m_items.pop_back();
				} else
				{
					break;
				}
			} else if (m_items.back()->get_type() == line_box_item::type_inline_start)
			{
				// remove trailing empty inline_start markers
				// these markers will be added at the beginning of the next line box
				m_width -= m_items.back()->width();
				ret_items.emplace_front(std::move(m_items.back()));
				m_items.pop_back();
			} else
			{
				break;
			}
		}
	}
	{
		// Remove trailing spaces even when an inline end marker follows them.
		auto iter = m_items.rbegin();
		while(iter != m_items.rend())
		{
			if ((*iter)->get_type() == line_box_item::type_text_part)
			{
				if ((*iter)->get_el()->src_el()->is_white_space() ||
					(!last_box && (*iter)->get_el()->src_el()->is_break()))
				{
					(*iter)->get_el()->skip(true);
					m_width -= (*iter)->occupied_width();
					// Space can be between text and inline_end marker
					// We have to shift all items on the right side
					if(iter != m_items.rbegin())
					{
						auto r_iter = iter;
						r_iter--;
						while (true)
						{
							(*r_iter)->pos().x -= (*iter)->occupied_width();
							if (r_iter == m_items.rbegin())
							{
								break;
							}
							r_iter--;
						}
					}
					// erase white space element
					iter = decltype(iter) (m_items.erase( std::next(iter).base() ));
				} else
				{
					break;
				}
			} else
			{
				iter++;
			}
		}
	}

    if( is_empty() || (!is_empty() && last_box && is_break_only()) )
    {
        m_height = m_default_line_height.computed_value;
		m_baseline = m_font_metrics.base_line();
        return ret_items;
    }

	trim_line_end(trailing_hanging_width());

	// Divide tracking between adjacent inline boxes after the line edges are trimmed.
	auto previous = m_items.end();
	for (auto item = m_items.begin(); item != m_items.end(); ++item)
	{
		if ((*item)->get_type() != line_box_item::type_text_part) continue;
		const auto& next = (*item)->get_el();
		if (next->skip() || next->width() <= 0) continue;
		const auto autospace = (*item)->autospace_before();
		const auto tracking = (*item)->spacing_before() - autospace;
		if (previous != m_items.end() && (tracking != 0 || autospace != 0))
		{
			const auto before = (*previous)->get_el()->css().get_font_metrics().letter_spacing / 2;
			const auto after = next->css().get_font_metrics().letter_spacing / 2;
			const auto shift = before + after != 0 ? before * (tracking / (before + after)) : 0;
			// Automatic spacing belongs to the common ancestor, outside the adjacent inline fragments.
			for (auto marker = std::next(previous); marker != item; ++marker)
			{
				const bool opening = (*marker)->get_type() != line_box_item::type_inline_end;
				(*marker)->pos().x += shift + (opening ? autospace : 0);
			}
		}
		previous = item;
	}

	const auto hanging_width = trailing_hanging_width();
	measure_intrinsic_width(hanging_width);
	align_horizontal(last_box || m_ended_with_break, hanging_width);

	std::optional<pixel_t> line_height;
	bool has_emphasis = false;

	if(!m_default_line_height.css_value.is_predefined())
	{
		line_height = m_default_line_height.computed_value;
	}

	va_context current_context;
	std::list<va_context> contexts;

	current_context.baseline = 0;
	current_context.fm = m_font_metrics;
	current_context.start_lbi = nullptr;
	current_context.line_height = m_default_line_height.computed_value;

	struct items_dimensions
	{
		pixel_t top = 0;
		pixel_t bottom = 0;
		int count = 0;
		pixel_t max_height = 0;

		void add_item(const line_box_item* item)
		{
			top = std::min(top, item->top());
			bottom = std::max(bottom, item->bottom());
			max_height = std::max(max_height, item->height());
			count++;
		}
		pixel_t height() const { return bottom - top; }
	};

	items_dimensions line_max_height;
	items_dimensions top_aligned_max_height;
	items_dimensions bottom_aligned_max_height;
	items_dimensions inline_boxes_dims;

	// First pass
	// 1. Align all items by baseline
	// 2. top/button aligned items are aligned by baseline
	// 3. Calculate top and button of the linebox separately for items in baseline
	//    and for top and bottom aligned items
    for (const auto& lbi : m_items)
	{

		// Calculate new baseline for inline start/continue
		// Inline start/continue elements are inline containers like <span>
		if (lbi->get_type() == line_box_item::type_inline_start || lbi->get_type() == line_box_item::type_inline_continue)
		{
			contexts.push_back(current_context);
			if(is_one_of(lbi->get_el()->css().get_vertical_align(), va_top, va_bottom))
			{
				// top/bottom aligned inline boxes are aligned by baseline == 0
				current_context.baseline = 0;
				current_context.start_lbi = lbi.get();
				current_context.start_lbi->reset_items_height();
			} else if(current_context.start_lbi)
			{
				current_context.baseline = calc_va_baseline(current_context,
					lbi->get_el()->css().get_vertical_align(),
					lbi->get_el()->css().get_font_metrics(),
					current_context.start_lbi->top(), current_context.start_lbi->bottom());
			} else
			{
				current_context.start_lbi = nullptr;
				current_context.baseline = calc_va_baseline(current_context,
															lbi->get_el()->css().get_vertical_align(),
															lbi->get_el()->css().get_font_metrics(),
															line_max_height.top, line_max_height.bottom);
			}
			current_context.fm = lbi->get_el()->css().get_font_metrics();
			current_context.line_height = lbi->get_el()->css().line_height().computed_value;
		}

		pixel_t bl = current_context.baseline;
		pixel_t content_offset = 0;
		bool is_top_bottom_box = false;
		bool ignore = false;

		// Align element by baseline
		if(!is_one_of(lbi->get_el()->src_el()->css().get_display(), display_inline_text, display_inline))
		{
			// Apply margins, paddings and border for inline boxes
			content_offset = lbi->get_el()->content_offset_top();
			switch (lbi->get_el()->css().get_vertical_align())
			{
			case va_bottom:
			case va_top:
				// Align by base line 0 all inline boxes with top and bottom vertical align
				bl = 0;
				is_top_bottom_box = true;
				break;

			case va_text_bottom:
				lbi->pos().y = bl + current_context.fm.descent - current_context.fm.half_line_gap - lbi->get_el()->height() + content_offset;
				ignore = true;
				break;

			case va_text_top:
				lbi->pos().y = bl - current_context.fm.ascent + current_context.fm.half_line_gap + content_offset;
				ignore = true;
				break;

			case va_middle:
				lbi->pos().y = bl - current_context.fm.x_height / 2 - lbi->get_el()->height() / 2 + content_offset;
				ignore = true;
				break;

			default:
				bl = calc_va_baseline(current_context,
									  lbi->get_el()->css().get_vertical_align(),
									  lbi->get_el()->css().get_font_metrics(),
									  line_max_height.top, line_max_height.bottom);
				break;
			}
		}
		if(!ignore)
		{
			lbi->pos().y = bl - lbi->get_el()->get_inline_baseline() + content_offset;
		}

		if(is_top_bottom_box)
		{
			switch (lbi->get_el()->css().get_vertical_align())
			{
				case va_top:
					top_aligned_max_height.add_item(lbi.get());
					break;
				case va_bottom:
					bottom_aligned_max_height.add_item(lbi.get());
					break;
				default:
					break;
			}
		} else if(current_context.start_lbi)
		{
			current_context.start_lbi->add_item_height(lbi->top(), lbi->bottom());
			switch (current_context.start_lbi->get_el()->css().get_vertical_align())
			{
				case va_top:
					top_aligned_max_height.add_item(lbi.get());
					break;
				case va_bottom:
					bottom_aligned_max_height.add_item(lbi.get());
					break;
				default:
					break;
			}
		} else
		{
			if(!lbi->get_el()->src_el()->is_inline_box())
			{
				line_max_height.add_item(lbi.get());
			} else
			{
				inline_boxes_dims.add_item(lbi.get());
			}
		}

		if (lbi->get_type() == line_box_item::type_text_part && lbi->get_el()->css().get_display() == display_inline_text &&
			!lbi->get_el()->src_el()->is_white_space())
		{
			has_emphasis |= lbi->get_el()->css().get_font_metrics().emphasis_extent > 0;
		}

		if(!lbi->get_el()->src_el()->is_inline_box() && !lbi->get_el()->css().line_height().css_value.is_predefined())
		{
			if(line_height.has_value())
			{
				line_height = std::max(line_height.value(), lbi->get_el()->css().line_height().computed_value);
			} else
			{
				line_height = lbi->get_el()->css().line_height().computed_value;
			}
		}

		if (lbi->get_type() == line_box_item::type_inline_end)
		{
			if(!contexts.empty())
			{
				current_context = contexts.back();
				contexts.pop_back();
			}
		}
	}

	pixel_t top_shift = 0;
	if (line_height.has_value() && has_emphasis)
	{
		line_height = std::max(*line_height, line_max_height.height());
	}
	if(line_height.has_value())
	{
		m_height = line_height.value();
		if(line_max_height.count != 0)
		{
			// We have inline items
			top_shift = std::abs(line_max_height.top);
			const pixel_t top_shift_correction = (line_height.value() - line_max_height.height()) / 2;
			// We have to calculate the baseline from the top of the line box due to possible round errors.
			// The top_shift_correction is actually text offset from the top of the line box.
			// The (top_shift_correction + line_max_height.height()): is the bottom of the text with shift (text_bottom).
			// The line_max_height.bottom is the text baseline here.
			// So the formula is:
			// baseline = line_height - text_bottom + text_baseline
			// The linebox baseline is the length from the bottom of the line box to the text baseline.
			m_baseline = line_height.value() - (top_shift_correction + line_max_height.height()) + line_max_height.bottom;
			top_shift += top_shift_correction;
			if(inline_boxes_dims.count)
			{
				const pixel_t diff2 = std::abs(inline_boxes_dims.top) - std::abs(top_shift);
				if(diff2 > 0)
				{
					m_height += diff2;
					top_shift += diff2;
					m_baseline += diff2;
				}
				const pixel_t diff1 = inline_boxes_dims.bottom - (line_max_height.bottom + top_shift_correction);
				if(diff1 > 0)
				{
					m_height += diff1;
				}
			}
		} else if(inline_boxes_dims.count != 0)
		{
			// We have inline boxes only
			m_height = inline_boxes_dims.height();
			top_shift = std::abs(inline_boxes_dims.top);
			m_baseline = inline_boxes_dims.bottom;
		} else
		{
			// We don't have inline items and inline boxes
			top_shift = 0;
		}


		const pixel_t top_down_height = std::max(top_aligned_max_height.max_height, bottom_aligned_max_height.max_height);
		if(top_down_height > m_height)
		{
			if(bottom_aligned_max_height.count)
			{
				top_shift += bottom_aligned_max_height.height() - m_height;
			}
			m_height = top_down_height;
		}
	} else
	{
		// Add inline boxes dimensions
		line_max_height.top = std::min(line_max_height.top, inline_boxes_dims.top);
		line_max_height.bottom = std::max(line_max_height.bottom, inline_boxes_dims.bottom);

		// Height is maximum from inline elements height and top/bottom aligned elements height
		m_height = std::max({line_max_height.height(), top_aligned_max_height.height(), bottom_aligned_max_height.height()});

		top_shift = -std::min(line_max_height.top, line_max_height.bottom - bottom_aligned_max_height.height());
		m_baseline = line_max_height.bottom;
	}

	struct inline_item_box
	{
		std::shared_ptr<render_item> element;
		position box;

		inline_item_box() = default;
		explicit inline_item_box(const std::shared_ptr<render_item>& el) : element(el) {}
	};

	std::list<inline_item_box> inlines;

	contexts.clear();

	current_context.baseline = 0;
	current_context.fm = m_font_metrics;
	current_context.start_lbi = nullptr;
	//int va_top_bottom = 0;

	// Second pass:
	// 1. Vertical align top/bottom
	// 2. Apply relative shift
	// 3. Calculate inline boxes
    for (const auto& lbi : m_items)
    {
		if(is_one_of(lbi->get_type(), line_box_item::type_inline_start, line_box_item::type_inline_continue))
		{
			contexts.push_back(current_context);
			current_context.fm = lbi->get_el()->css().get_font_metrics();

			if(lbi->get_el()->css().get_vertical_align() == va_top)
			{
				current_context.baseline = m_top - lbi->get_items_top();
				current_context.start_lbi = lbi.get();
			} else if(lbi->get_el()->css().get_vertical_align() == va_bottom)
			{
				current_context.baseline = m_top + m_height - lbi->get_items_bottom();
				current_context.start_lbi = lbi.get();
			}
		} else if(lbi->get_type() == line_box_item::type_inline_end)
		{
			if(!contexts.empty())
			{
				current_context = contexts.back();
				contexts.pop_back();
			}
		}

		if(current_context.start_lbi)
		{
			lbi->pos().y = current_context.baseline - lbi->get_el()->get_inline_baseline() +
						   lbi->get_el()->content_offset_top();
		} else if(is_one_of(lbi->get_el()->css().get_vertical_align(), va_top, va_bottom) && lbi->get_type() == line_box_item::type_text_part)
		{
			if(lbi->get_el()->css().get_vertical_align() == va_top)
			{
				lbi->pos().y = m_top + lbi->get_el()->content_offset_top();
			} else
			{
				lbi->pos().y = m_top + m_height - (lbi->bottom() - lbi->top()) + lbi->get_el()->content_offset_bottom();
			}
		} else
		{
			// move element to the correct position
			lbi->pos().y += m_top + top_shift;
		}

        lbi->get_el()->apply_relative_shift(containing_block_size);

		// Calculate and push inline box into the render item element
		if(lbi->get_type() == line_box_item::type_inline_start || lbi->get_type() == line_box_item::type_inline_continue)
		{
			if(lbi->get_type() == line_box_item::type_inline_start)
			{
				lbi->get_el()->clear_inline_boxes();
			}
			inlines.emplace_back(lbi->get_el());
			inlines.back().box.x = lbi->left();
			inlines.back().box.y = lbi->top() - lbi->get_el()->content_offset_top();
			inlines.back().box.height = lbi->bottom() - lbi->top() + lbi->get_el()->content_offset_height();
		} else if(lbi->get_type() == line_box_item::type_inline_end)
		{
			if(!inlines.empty())
			{
				inlines.back().box.width = lbi->right() - inlines.back().box.x;
				inlines.back().box.x += inlines.back().element->relative_offset_x();
				inlines.back().box.y += inlines.back().element->relative_offset_y();
				inlines.back().element->add_inline_box(inlines.back().box);
				inlines.pop_back();
			}
		}
    }

	for(auto iter = inlines.rbegin(); iter != inlines.rend(); ++iter)
	{
		iter->box.width =  m_items.back()->right() - iter->box.x;
		iter->box.x += iter->element->relative_offset_x();
		iter->box.y += iter->element->relative_offset_y();
		iter->element->add_inline_box(iter->box);

		ret_items.emplace_front(std::unique_ptr<line_box_item>(new lbi_continue(iter->element)));
	}
	reorder_bidi_text();

	return ret_items;
}

void litehtml::line_box::reorder_bidi_text()
{
	struct TextPart {
		line_box_item* item = nullptr;
		int64_t start = 0;
		int64_t end = 0;
	};
	struct BidiScope {
		element::ptr node;
		css_unicode_bidi mode = css_unicode_bidi::normal;
		bool right_to_left = false;
	};

	std::vector<TextPart> parts;
	std::vector<std::pair<int64_t, int64_t>> ranges;
	string bidi_text;
	int64_t bidi_length = 0;
	bool has_rtl = m_right_to_left;
	bool has_bidi_scope = false;
	const auto append_control = [&](char32_t p_codepoint) {
		append_char(bidi_text, p_codepoint);
		++bidi_length;
	};
	const auto open_scope = [&](const BidiScope& p_scope) {
		const char32_t embedding = p_scope.right_to_left ? 0x202b : 0x202a;
		const char32_t override = p_scope.right_to_left ? 0x202e : 0x202d;
		const char32_t isolate = p_scope.right_to_left ? 0x2067 : 0x2066;
		switch (p_scope.mode) {
			case css_unicode_bidi::embed: append_control(embedding); break;
			case css_unicode_bidi::bidi_override: append_control(override); break;
			case css_unicode_bidi::isolate: append_control(isolate); break;
			case css_unicode_bidi::isolate_override: append_control(isolate); append_control(override); break;
			case css_unicode_bidi::plaintext: append_control(0x2068); break;
			case css_unicode_bidi::normal: break;
		}
	};
	const auto close_scope = [&](const BidiScope& p_scope) {
		if (p_scope.mode == css_unicode_bidi::embed || p_scope.mode == css_unicode_bidi::bidi_override) append_control(0x202c);
		else if (p_scope.mode == css_unicode_bidi::isolate || p_scope.mode == css_unicode_bidi::plaintext) append_control(0x2069);
		else if (p_scope.mode == css_unicode_bidi::isolate_override) { append_control(0x202c); append_control(0x2069); }
	};
	const auto get_scopes = [](const element::ptr& p_source) {
		std::vector<element::ptr> ancestors;
		for (element::ptr node = p_source ? p_source->parent() : nullptr; node; node = node->parent())
			ancestors.push_back(node);
		std::reverse(ancestors.begin(), ancestors.end());
		std::vector<BidiScope> result;
		for (const auto& node : ancestors) {
			const auto mode = static_cast<css_unicode_bidi>(node->css().get_unicode_bidi_mode());
			if (mode == css_unicode_bidi::normal) continue;
			const auto parent = node->parent();
			const auto parent_mode = parent ? static_cast<css_unicode_bidi>(parent->css().get_unicode_bidi_mode()) : css_unicode_bidi::normal;
			const bool explicit_scope = node->tag() == _bdi_ || node->tag() == _bdo_ || node->get_attr("dir") != nullptr;
			if (mode != parent_mode || explicit_scope) result.push_back({ node, mode, node->css().is_right_to_left() });
		}
		return result;
	};
	std::vector<BidiScope> active_scopes;
	for (const auto& item : m_items) {
		if (item->get_type() != line_box_item::type_text_part || item->get_el()->skip()) continue;
		const auto source = item->get_el()->src_el();
		const auto* text_element = dynamic_cast<const el_text*>(source.get());
		if (!text_element || source->is_break()) continue;
		const string& text = text_element->get_rendered_text();
		if (text.empty()) continue;

		auto scopes = get_scopes(source);
		has_bidi_scope |= !scopes.empty();
		size_t common = 0;
		while (common < active_scopes.size() && common < scopes.size() &&
			active_scopes[common].node.get() == scopes[common].node.get()) ++common;
		for (size_t index = active_scopes.size(); index > common; --index) close_scope(active_scopes[index - 1]);
		for (size_t index = common; index < scopes.size(); ++index) open_scope(scopes[index]);
		active_scopes = std::move(scopes);

		const auto utf32 = utf8_to_utf32(text);
		const char32_t* codepoints = utf32;
		const int64_t length = static_cast<int64_t>(std::char_traits<char32_t>::length(codepoints));
		const int64_t start = bidi_length;
		bidi_text += text;
		bidi_length += length;
		parts.push_back({ item.get(), start, start + length });
		ranges.emplace_back(start, start + length);
		for (int64_t index = 0; index < length; ++index) {
			const char32_t codepoint = codepoints[index];
			has_rtl |= (codepoint >= 0x0590 && codepoint <= 0x08ff) ||
				(codepoint >= 0xfb1d && codepoint <= 0xfdff) || (codepoint >= 0xfe70 && codepoint <= 0xfeff) ||
				(codepoint >= 0x10800 && codepoint <= 0x10fff) || (codepoint >= 0x1e800 && codepoint <= 0x1eeff);
			has_bidi_scope |= (codepoint >= 0x202a && codepoint <= 0x202e) ||
				(codepoint >= 0x2066 && codepoint <= 0x2069);
		}
	}
	for (size_t index = active_scopes.size(); index > 0; --index) close_scope(active_scopes[index - 1]);
	if ((!has_rtl && !has_bidi_scope) || parts.size() < 2 || !m_font) return;

	// litehtml splits words into separate text items, so resolve their visual order from the full line.
	const auto order = parts.front().item->get_el()->src_el()->get_document()->container()->get_bidi_visual_order(
		bidi_text.c_str(), m_font, m_right_to_left, ranges);
	if (order.size() != parts.size()) return;
	std::vector<bool> seen(parts.size());
	for (const size_t index : order) {
		if (index >= parts.size() || seen[index]) return;
		seen[index] = true;
	}
	bool changed = false;
	for (size_t index = 0; index < order.size(); ++index) changed |= order[index] != index;
	if (!changed) return;

	pixel_t cursor = std::numeric_limits<pixel_t>::max();
	for (const auto& part : parts) cursor = std::min(cursor, part.item->pos().x - part.item->spacing_before());
	for (const size_t index : order) {
		if (index >= parts.size()) return;
		auto& part = parts[index];
		part.item->pos().x = cursor + part.item->spacing_before() + part.item->get_el()->content_offset_left();
		cursor += part.item->occupied_width();
	}
}

std::shared_ptr<litehtml::render_item> litehtml::line_box::get_first_text_part() const
{
	for(const auto & item : m_items)
	{
		if(item->get_type() == line_box_item::type_text_part)
		{
			return item->get_el();
		}
	}
	return nullptr;
}


std::shared_ptr<litehtml::render_item> litehtml::line_box::get_last_text_part() const
{
	for(auto iter = m_items.rbegin(); iter != m_items.rend(); iter++)
	{
		if((*iter)->get_type() == line_box_item::type_text_part)
		{
			return (*iter)->get_el();
		}
	}
	return nullptr;
}


bool litehtml::line_box::ends_with_forced_break() const
{
	if (m_finished) return m_ended_with_break;
	const auto last = get_last_text_part();
	return last && last->src_el()->is_break();
}

bool litehtml::line_box::have_last_space()  const
{
	auto last_el = get_last_text_part();
	if(last_el)
	{
		return last_el->src_el()->is_white_space() || last_el->src_el()->is_break();
	}
	return false;
}

bool litehtml::line_box::is_empty() const
{
    if(m_items.empty()) return true;
	if(m_items.size() == 1 &&
		m_items.front()->get_el()->src_el()->is_break() &&
		m_items.front()->get_el()->src_el()->css().get_clear() != clear_none)
	{
		return true;
	}
    for (const auto& el : m_items)
    {
		if(el->get_type() == line_box_item::type_text_part)
		{
			const auto& item = el->get_el();
			if (item->width() == 0 && (is_explicit_wrap_point(*item->src_el()) || is_soft_hyphen(*item->src_el()))) continue;
			if (!item->skip() || item->src_el()->is_break())
			{
				return false;
			}
		}
    }
    return true;
}

litehtml::pixel_t litehtml::line_box::baseline() const
{
    return m_baseline;
}

litehtml::pixel_t litehtml::line_box::top_margin() const
{
    return 0;
}

litehtml::pixel_t litehtml::line_box::bottom_margin() const
{
    return 0;
}

void litehtml::line_box::shift_for_content_alignment(pixel_t shift, std::unordered_set<render_item*>& shifted)
{
	m_top += shift;
	for (const auto& item : m_items)
		if (shifted.insert(item->get_el().get()).second) item->get_el()->y_shift(shift);
}

void litehtml::line_box::y_shift( pixel_t shift )
{
	m_top += shift;
	for (auto& el : m_items)
	{
		el->y_shift(shift);
	}
}

bool litehtml::line_box::is_break_only() const
{
    if(m_items.empty()) return false;

	bool break_found = false;

	for (auto iter = m_items.rbegin(); iter != m_items.rend(); iter++)
	{
		if((*iter)->get_type() == line_box_item::type_text_part)
		{
			if((*iter)->get_el()->src_el()->is_break())
			{
				break_found = true;
			} else if(!(*iter)->get_el()->skip())
			{
				return false;
			}
		}
	}
	return break_found;
}

std::list< std::unique_ptr<litehtml::line_box_item> > litehtml::line_box::new_width( pixel_t left, pixel_t right)
{
	std::list< std::unique_ptr<line_box_item> > ret_items;
	left += m_indent;
	if (left == m_left && right == m_right) return ret_items;
	m_left = left;
	m_right = right;
	m_width = 0;
	m_height = 0;
	item_list unplaced;
	unplaced.splice(unplaced.end(), m_items);
	m_last_wrap_point = m_items.end();
	m_last_wrap_hyphen = nullptr;
	m_line_end_hyphen = 0;
	m_hyphenated_at = nullptr;
	m_ordinary_wrap_end.reset();
	m_hyphenation_zone = m_hyphenation_limits.zone.calc_percent(m_right - m_left + m_indent);
	m_last_overflow_wrap_point = m_items.end();
	while (!unplaced.empty())
	{
		if (!m_items.empty())
		{
			if (auto tail = wrap_before(*unplaced.front()))
			{
				ret_items.splice(ret_items.end(), *tail);
				ret_items.splice(ret_items.end(), unplaced);
				break;
			}
		}
		add_item(std::move(unplaced.front()));
		unplaced.pop_front();
	}
	return ret_items;
}
