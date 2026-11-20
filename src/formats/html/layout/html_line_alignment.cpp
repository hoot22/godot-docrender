#include "html_line_break.h"
#include "html_punctuation_spacing.h"
#include "html_text_autospace.h"
#include "formats/html/text/html_shaped_text.h"

#include <litehtml/document_container.h>
#include <litehtml/el_text.h>
#include <litehtml/line_box.h>
#include <litehtml/render_inline_context.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace litehtml {
	namespace {
		const css_properties& boundary_style(const element& before, const element& after) {
			const auto owner = inline_boundary_owner(before, after);
			return owner ? owner->css() : before.css();
		}
	}

	line_box::wrap_opportunity line_box::wrap_between(const element& before, const element& after, const element* prefix) {
		using opportunity = wrap_opportunity;
		if (before.is_break() || after.is_break() || after.is_white_space() || is_explicit_wrap_point(after) ||
			is_soft_hyphen(after)) return opportunity::none;
		if (is_soft_hyphen(before)) {
			if (!prefix) return opportunity::none;
			const auto ordinary = wrap_between(*prefix, after);
			const auto& style = before.css();
			if (ordinary == opportunity::normal || style.get_hyphens() == hyphens_none ||
				!static_cast<const el_text&>(before).allows_hyphenation() ||
				style.get_text_wrap_mode() != text_wrap_wrap || style.get_word_break() == word_break_break_all ||
				style.get_line_break() == line_break_anywhere || !after.is_text() || after.is_space() ||
				after.is_other_space_separator() || prefix->is_space() || prefix->is_other_space_separator() ||
				prefix->is_glue_space() || after.is_glue_space()) return ordinary;
			return opportunity::hyphen;
		}
		if (is_explicit_wrap_point(before)) {
			return before.css().get_text_wrap_mode() == text_wrap_wrap ? opportunity::normal : opportunity::none;
		}
		const auto& style = before.is_white_space() ? before.css() : boundary_style(before, after);
		if (style.get_text_wrap_mode() != text_wrap_wrap) return opportunity::none;
		const bool before_preserved_space = after.is_space() || after.is_other_space_separator();
		const bool can_wrap_before_space = !before_preserved_space || style.get_white_space_collapse() == white_space_break_spaces;
		if (style.get_line_break() == line_break_anywhere) {
			return can_wrap_before_space && is_grapheme_boundary(before, after) ? opportunity::normal : opportunity::none;
		}
		if (has_non_breaking_autospace(before, after)) return opportunity::none;
		const bool break_after_space = (before.is_space() || before.is_other_space_separator()) &&
			before.css().get_white_space_collapse() == white_space_break_spaces;
		if ((before.is_white_space() || break_after_space ||
			(!before_preserved_space && !before.is_glue_space() && !after.is_glue_space())) &&
			allows_text_boundary_wrap(before, after, style)) return opportunity::normal;

		const auto overflow = style.get_effective_overflow_wrap();
		if (overflow == overflow_wrap_normal || !before.is_text() || !after.is_text() ||
			!can_wrap_before_space || !is_grapheme_boundary(before, after)) return opportunity::none;
		return overflow == overflow_wrap_anywhere ? opportunity::overflow_intrinsic : opportunity::overflow;
	}

	const element* line_box::hyphen_prefix(item_list::const_reverse_iterator prior, item_list::const_reverse_iterator end) {
		for (; prior != end; ++prior) {
			const auto& item = **prior;
			const auto& render = item.get_el();
			if (item.get_type() == line_box_item::type_text_part && !render->skip() && !is_soft_hyphen(*render->src_el())) {
				return render->src_el().get();
			}
		}
		return nullptr;
	}

	pixel_t line_box::ordinary_wrap_end(item_list::iterator boundary) const {
		pixel_t end = (*std::prev(boundary))->right();
		for (auto item = std::make_reverse_iterator(boundary); item != m_items.rend(); ++item) {
			const auto& part = **item;
			if (part.get_type() != line_box_item::type_text_part || part.get_el()->skip()) continue;
			const auto& source = *part.get_el()->src_el();
			if (is_soft_hyphen(source) || is_explicit_wrap_point(source)) continue;
			if (!source.is_white_space() && !is_hanging_space(source)) break;
			end -= part.occupied_width();
		}
		return end;
	}

	bool line_box::fits_hyphen(const line_box_item& item, item_list::iterator boundary) const {
		if (m_suppress_hyphenation || !m_hyphenation_limits.allows_line(m_allow_overflow_wrap ? m_preceding_hyphenated_lines : 0)) return false;
		if (m_allow_overflow_wrap && m_ordinary_wrap_end &&
			!css_hyphenation_limits::allows_hyphen_in_zone(m_hyphenation_zone, m_right - *m_ordinary_wrap_end)) return false;
		const auto last = std::prev(boundary);
		return (*last)->right() + measure_hyphen(*item.get_el()->src_el()) <= m_right;
	}

	void line_box::apply_line_end_hyphen(line_box_item& item) {
		m_hyphenated_at = item.get_el().get();
		m_line_end_hyphen = measure_hyphen(*item.get_el()->src_el());
		item.pos().width = m_line_end_hyphen;
		m_width += m_line_end_hyphen;
		bool after_hyphen = false;
		for (const auto& tail : m_items) {
			if (after_hyphen) tail->pos().x += m_line_end_hyphen;
			after_hyphen |= tail.get() == &item;
		}
	}

	pixel_t line_box::fragment_spacing(const render_item& previous, pixel_t previous_width,
		const render_item& next, pixel_t next_width) {
		if (previous.src_el()->is_inline_box() && next.src_el()->is_inline_box()) return 0;
		const auto gap = previous.css().get_font_metrics().letter_spacing / 2 + next.css().get_font_metrics().letter_spacing / 2;
		return std::max(-std::min(previous_width, next_width), gap);
	}

	void line_box::remember_wrap_point(item_list::iterator current) {
		if ((*current)->get_type() != line_box_item::type_text_part) return;
		const auto& next = *(*current)->get_el()->src_el();
		if (next.is_break()) {
			m_last_wrap_point = m_items.end();
			m_last_wrap_hyphen = nullptr;
			m_ordinary_wrap_end.reset();
			m_last_overflow_wrap_point = m_items.end();
			return;
		}
		for (auto item = std::make_reverse_iterator(current); item != m_items.rend(); ++item) {
			if ((*item)->get_type() != line_box_item::type_text_part || (*item)->get_el()->skip()) continue;
			const auto& source = *(*item)->get_el()->src_el();
			if (is_explicit_wrap_point(source) && (*item)->get_el()->width() == 0 &&
				std::none_of(std::next(item), m_items.rend(), [](const auto& earlier) {
					const auto& render = earlier->get_el();
					return earlier->get_type() == line_box_item::type_text_part && !render->skip() &&
						(render->width() > 0 || !is_explicit_wrap_point(*render->src_el()));
				})) break;
			const auto* prefix = is_soft_hyphen(source) ? hyphen_prefix(std::next(item), m_items.crend()) : nullptr;
			const auto opportunity = wrap_between(source, next, prefix);
			if (opportunity != wrap_opportunity::none) {
				auto boundary = item.base();
				// Closing markers belong to the old line, opening markers move with the new one.
				while (boundary != current && (*boundary)->get_type() == line_box_item::type_inline_end) ++boundary;
				const bool hyphen = opportunity == wrap_opportunity::hyphen;
				const bool use_fallback = !hyphen || !is_hyphenation_fallback(source) || m_last_wrap_point == m_items.end() ||
					(m_last_wrap_hyphen && is_hyphenation_fallback(*m_last_wrap_hyphen->get_el()->src_el()));
				if (opportunity == wrap_opportunity::normal || (hyphen && use_fallback && fits_hyphen(**item, boundary))) {
					m_last_wrap_point = boundary;
					m_last_wrap_hyphen = hyphen ? item->get() : nullptr;
					if (!hyphen && m_hyphenation_zone > 0) {
						m_ordinary_wrap_end = ordinary_wrap_end(boundary);
					}
				}
				else if (m_allow_overflow_wrap && (!hyphen || wrap_between(*prefix, next) != wrap_opportunity::none)) {
					m_last_overflow_wrap_point = boundary;
				}
			}
			break;
		}
	}

	std::optional<line_box::item_list> line_box::wrap_before(const line_box_item& item, bool force_wrap) {
		const auto& render = item.get_el();
		const auto& source = *render->src_el();
		if (m_finished || !source.is_inline()) return item_list{};
		if (item.get_type() != line_box_item::type_text_part) return std::nullopt;
		if (source.is_break() && render->css().get_clear() != clear_none) return item_list{};

		const auto previous = get_last_text_part();
		if (!previous) return std::nullopt;
		if (is_explicit_wrap_point(*previous->src_el()) && is_empty()) return std::nullopt;
		if (previous->src_el()->is_break() && previous->css().get_clear() == clear_none) return item_list{};
		if (source.is_break() || source.is_white_space() || is_hanging_space(source) || is_soft_hyphen(source)) return std::nullopt;

		const auto punctuation = punctuation_before(item);
		const auto previous_change = punctuation.previous ?
			punctuation.previous->punctuation_reduction() - punctuation.previous_reduction : 0;
		const auto width = source.is_preserved_tab() ? text_tab_width(item, m_left + m_width) :
			item.untrimmed_width() - std::max(punctuation.next_reduction, punctuation.possible_end_reduction);
		const bool fits = m_left + m_width + previous_change + width + spacing_before(item) <= m_right;
		if (!force_wrap && fits) return std::nullopt;
		const auto& previous_source = *previous->src_el();
		auto marker = m_items.rbegin();
		const element* prefix = nullptr;
		if (is_soft_hyphen(previous_source)) {
			while ((*marker)->get_type() != line_box_item::type_text_part) ++marker;
			prefix = hyphen_prefix(std::next(marker), m_items.crend());
		}
		auto opportunity = wrap_between(previous_source, source, prefix);
		if (opportunity == wrap_opportunity::hyphen) {
			auto boundary = marker.base();
			while (boundary != m_items.end() && (*boundary)->get_type() == line_box_item::type_inline_end) ++boundary;
			const bool use_fallback = !is_hyphenation_fallback(previous_source) || m_last_wrap_point == m_items.end() ||
				(m_last_wrap_hyphen && is_hyphenation_fallback(*m_last_wrap_hyphen->get_el()->src_el()));
			if (use_fallback && fits_hyphen(**marker, boundary)) {
				apply_line_end_hyphen(**marker);
				return item_list{};
			}
			if (fits) return std::nullopt;
			force_wrap = false;
			opportunity = wrap_between(*prefix, source);
		}
		if (opportunity == wrap_opportunity::normal) return item_list{};
		if (force_wrap) return item_list{};
		auto boundary = m_last_wrap_point;
		if (boundary == m_items.end() && m_allow_overflow_wrap) {
			if (opportunity == wrap_opportunity::overflow || opportunity == wrap_opportunity::overflow_intrinsic) return item_list{};
			boundary = m_last_overflow_wrap_point;
		}
		if (boundary == m_items.end()) return std::nullopt;

		item_list tail;
		for (auto moved = boundary; moved != m_items.end(); ++moved) m_width -= (*moved)->occupied_width();
		tail.splice(tail.end(), m_items, boundary, m_items.end());
		if (m_last_wrap_hyphen) apply_line_end_hyphen(*m_last_wrap_hyphen);
		m_last_wrap_point = m_items.end();
		m_last_wrap_hyphen = nullptr;
		m_ordinary_wrap_end.reset();
		m_last_overflow_wrap_point = m_items.end();
		return tail;
	}

	void line_box::measure_intrinsic_width(pixel_t hanging_width) {
		m_rendered_width.reset();
		m_rendered_width.natural_width = m_intrinsic_indent - hanging_width;
		pixel_t group = m_intrinsic_indent;
		pixel_t opening_edges = 0;
		pixel_t collapsible_end = 0;
		pixel_t terminal_reduction = 0;
		bool first_group = true;
		bool has_content = false;
		bool has_inline_edge = false;
		bool has_opening_edge = false;
		punctuation_spacing preceding_punctuation;
		const element* previous = nullptr;
		const element* previous_word_part = nullptr;
		const auto commit = [&]() {
			m_rendered_width.min_width = std::max(m_rendered_width.min_width, group - collapsible_end - terminal_reduction);
			group = 0;
			collapsible_end = 0;
			terminal_reduction = 0;
			has_content = false;
			first_group = false;
			has_inline_edge = has_opening_edge;
			preceding_punctuation = {};
		};
		for (const auto& item : m_items) {
			m_rendered_width.natural_width += item->get_rendered_natural_width() + item->spacing_before();
			const pixel_t advance = item->get_rendered_min_width();
			if (item->get_type() != line_box_item::type_text_part) {
				const auto type = item->get_type();
				if (type != line_box_item::type_inline_continue) {
					const bool opens = type == line_box_item::type_inline_start;
					const bool separates = separates_autospace(*item->get_el(), opens, m_right - m_left);
					has_inline_edge |= separates;
					has_opening_edge = opens && (has_opening_edge || separates);
				}
				if (item->get_type() == line_box_item::type_inline_end) {
					group += opening_edges + advance;
					opening_edges = 0;
				}
				else opening_edges += advance;
				continue;
			}
			const auto& source = *item->get_el()->src_el();
			if (source.is_break()) {
				commit();
				previous = previous_word_part = nullptr;
				continue;
			}
			auto opportunity = previous ? wrap_between(*previous, source, previous_word_part) : wrap_opportunity::none;
			if (opportunity == wrap_opportunity::hyphen && !m_hyphenation_limits.allows_line(0)) {
				opportunity = wrap_between(*previous_word_part, source);
			}
			const bool breakable = opportunity == wrap_opportunity::normal || opportunity == wrap_opportunity::overflow_intrinsic ||
				opportunity == wrap_opportunity::hyphen;
			if (opportunity == wrap_opportunity::hyphen) {
				group += measure_hyphen(*previous);
				terminal_reduction = 0;
			}
			if (breakable) commit();
			group += opening_edges;
			opening_edges = 0;
			const auto raw_advance = advance + item->punctuation_reduction();
			const auto punctuation = get_punctuation_spacing(source, false, raw_advance);
			const bool trim = (punctuation.flags & text_spacing_all) ||
				(!has_inline_edge && ((!has_content && punctuation.at_start(first_group && m_paragraph_start)) ||
					punctuation.after(preceding_punctuation)));
			const auto prior_reduction = !has_inline_edge && preceding_punctuation.before(punctuation) ?
				preceding_punctuation.reduction : 0;
			group -= prior_reduction;
			terminal_reduction = std::max(pixel_t(0), terminal_reduction - prior_reduction);
			pixel_t width = raw_advance - (trim ? punctuation.reduction : 0) + (breakable ? 0 : item->spacing_before());
			if (is_hanging_space(source)) width = 0;
			else if (source.is_preserved_tab()) width = text_tab_width(*item, group);
			group += width;
			collapsible_end = source.is_white_space() ? width : 0;
			if (raw_advance > 0) {
				preceding_punctuation = get_punctuation_spacing(source, true, raw_advance);
				if (!source.is_white_space() && !is_hanging_space(source)) {
					terminal_reduction = preceding_punctuation.kind == spacing_punctuation::closing &&
						(preceding_punctuation.flags & (text_spacing_end | text_spacing_allow_end)) ?
						preceding_punctuation.reduction : 0;
				}
				has_content = true;
				has_inline_edge = has_opening_edge = false;
			}
			previous = &source;
			if (!is_soft_hyphen(source)) previous_word_part = &source;
		}
		group += opening_edges;
		group += m_line_end_hyphen;
		if (m_line_end_hyphen > 0) terminal_reduction = 0;
		commit();
	}

	bool render_item_inline_context::first_formatted_line() const {
		if (src_el()->tag() != empty_id) return true;
		const auto enclosing = parent();
		if (!enclosing) return true;
		// An anonymous block's first line inherits this distinction from its enclosing block.
		for (const auto& sibling : enclosing->children()) {
			if (sibling.get() == this) break;
			if (!sibling->src_el()->is_float() && sibling->src_el()->tag() != marker_tag() &&
				!is_one_of(sibling->css().get_position(), element_position_absolute, element_position_fixed)) return false;
		}
		return true;
	}

	pixel_t line_box::text_tab_width(const line_box_item& item, pixel_t cursor) const {
		const auto& source = item.get_el()->src_el();
		const auto parent = source->parent();
		if (!parent) return 0;
		const auto& tab = parent->css().get_tab_size();
		double interval = tab.val();
		if (tab.units() == css_units_none) {
			if (!m_font) return 0;
			const double space = source->get_document()->container()->text_width(" ", m_font);
			interval *= std::max(0.0, space + m_font_metrics.letter_spacing);
		}
		if (interval <= 0 || !std::isfinite(interval)) return 0;

		const double remainder = std::fmod(cursor, interval);
		double advance = remainder < 0 ? -remainder : interval - remainder;
		if (advance < parent->css().get_font_metrics().ch_width * 0.5) advance += interval;
		return advance <= std::numeric_limits<pixel_t>::max() ? static_cast<pixel_t>(advance) : 0;
	}

	void line_box::measure_tab(line_box_item& item) const {
		if (item.get_type() != line_box_item::type_text_part || !item.get_el()->src_el()->is_preserved_tab()) return;
		const pixel_t width = text_tab_width(item, m_left + m_width);
		item.pos().width = width;
		item.set_rendered_widths({ width, width });
	}

	bool line_box::is_hanging_space(const element& source) {
		const auto collapse = source.css().get_white_space_collapse();
		const bool wrap = source.css().get_text_wrap_mode() == text_wrap_wrap;
		if (source.is_other_space_separator()) {
			return collapse != white_space_break_spaces && (wrap || source.css().collapses_white_space());
		}
		return source.is_space() && wrap && is_one_of(collapse, white_space_preserve, white_space_preserve_spaces);
	}

	pixel_t line_box::trailing_hanging_width() const {
		pixel_t width = 0;
		for (auto item = m_items.rbegin(); item != m_items.rend(); ++item) {
			if ((*item)->get_type() != line_box_item::type_text_part) continue;
			const auto& render = (*item)->get_el();
			const auto& source = render->src_el();
			if (render->skip() || source->is_break()) continue;
			if (!is_hanging_space(*source)) break;
			width += (*item)->occupied_width();
		}
		return width;
	}

	void line_box::align_horizontal(bool terminal_line, pixel_t hanging_width) {
		const pixel_t free_space = m_right - m_left - m_width;
		// At a forced break, only the overflowing part hangs.
		const pixel_t ignored = terminal_line ? std::clamp(-free_space, pixel_t(0), hanging_width) : hanging_width;
		const pixel_t available = free_space + ignored;
		m_wrap_slack = available;
		if (available <= 0) return;
		auto last_alignment = m_text_align_last;
		if (last_alignment == text_align_auto) {
			last_alignment = m_text_align == text_align_justify ? text_align_left : m_text_align;
		}
		text_align alignment = terminal_line ? last_alignment : m_text_align;
		if (alignment == text_align_justify) {
			if (justify(available)) return;
			alignment = last_alignment == text_align_justify ? text_align_center : last_alignment;
		}
		const pixel_t shift = alignment == text_align_right ? available : alignment == text_align_center ? available / 2 : 0;
		if (shift != 0) {
			for (const auto& item : m_items) item->pos().x += shift;
		}
	}

	bool line_box::justify(pixel_t available) {
		struct TextPart {
			decltype(m_items)::iterator item;
			text_justify method;
			int opportunities;
			bool gap_before;
		};
		std::vector<TextPart> parts;
		char32_t previous_character = 0;
		text_justify previous_method = text_justify_none;
		size_t opportunities = 0;
		for (auto item = m_items.begin(); item != m_items.end(); ++item) {
			if ((*item)->get_type() != line_box_item::type_text_part) continue;
			const auto& render = (*item)->get_el();
			if (render->skip()) continue;
			// Moving text before a preserved tab would move the tab off its stop.
			if (render->src_el()->is_preserved_tab()) return false;
			const auto* text = dynamic_cast<const el_text*>(render->src_el().get());
			const auto parent = text ? text->parent() : nullptr;
			const auto font = parent ? parent->css().get_font() : 0;
			if (!font || text->is_break() || text->css().preserves_white_space()) {
				previous_character = 0;
				continue;
			}
			const auto method = parent->css().get_text_justify();
			if (method == text_justify_none) {
				previous_character = 0;
				continue;
			}
			const auto info = text->get_document()->container()->get_text_justify_info(text->get_rendered_text().c_str(), font, method);
			const bool gap = dockit::has_text_justification_gap(method, previous_character, info.first) &&
				dockit::has_text_justification_gap(previous_method, previous_character, info.first);
			parts.push_back({ item, method, info.count, gap });
			opportunities += info.count + int(gap);
			previous_character = info.last;
			previous_method = method;
		}
		if (opportunities == 0) return false;

		const pixel_t extra = available / static_cast<pixel_t>(opportunities);
		pixel_t shift = 0;
		auto next = m_items.begin();
		for (const auto& part : parts) {
			// Split a boundary gap at inline box edges, as with letter-spacing.
			const pixel_t gap = part.gap_before ? extra : 0;
			for (; next != part.item; ++next) (*next)->pos().x += shift + gap / 2;
			shift += gap;
			(*next)->pos().x += shift;
			if (part.opportunities > 0) {
				const pixel_t added = extra * part.opportunities;
				(*next)->get_el()->set_text_justification({ part.method, extra });
				(*next)->pos().width += added;
				shift += added;
			}
			++next;
		}
		for (; next != m_items.end(); ++next) (*next)->pos().x += shift;
		m_width += shift;
		return true;
	}
}
