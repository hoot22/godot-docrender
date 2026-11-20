#include <litehtml/iterators.h>
#include <litehtml/render_inline_context.h>

#include "html_line_break.h"
#include "html_punctuation_spacing.h"
#include "html_text_autospace.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace litehtml {
	namespace {
		constexpr size_t MAX_WRAP_POINTS = 256;
		constexpr size_t MAX_BALANCED_LINES = 10;
		constexpr size_t MAX_PRETTY_LINES = 24;
		constexpr size_t MAX_TERMINAL_HYPHEN_REPLAYS = 8;
		constexpr double WIDTH_EPSILON = 0.01;

		struct WrapPoint {
			const render_item* next = nullptr;
			double start = 0;
			double fit_end = 0;
			double full_end = 0;
			std::optional<double> hyphen_width;
			size_t preceding_ordinary_cut = 0;
			double conditional_end_reduction = 0;
		};

		struct Paragraph {
			std::vector<WrapPoint> points{ {} };
			bool can_plan = true;
		};

		using GroupSlacks = std::vector<std::vector<pixel_t>>;

		double line_cost(text_wrap_style p_style, double p_slack, bool p_last) {
			const double slack = std::max(0.0, p_slack);
			const double shortfall = std::max(0.0, slack - 0.65);
			if (p_style == text_wrap_pretty && p_last) return 0.35 * slack * slack + 5 * shortfall * shortfall;
			if (p_style == text_wrap_avoid_short_last_line) {
				const double shortage = p_last ? shortfall : std::max(0.0, slack - 0.5);
				return shortage * shortage;
			}
			return slack * slack;
		}

		double group_cost(text_wrap_style p_style, const std::vector<pixel_t>& p_slack, pixel_t p_width) {
			if (p_style == text_wrap_balance) {
				double mean = 0;
				for (const auto slack : p_slack) mean += slack;
				mean /= p_slack.size();
				double variance = 0;
				for (const auto slack : p_slack) {
					const double difference = (slack - mean) / p_width;
					variance += difference * difference;
				}
				return variance / p_slack.size();
			}
			double cost = 0;
			const size_t start = p_style == text_wrap_avoid_short_last_line && p_slack.size() > 2 ? p_slack.size() - 2 : 0;
			for (size_t line = start; line < p_slack.size(); ++line) {
				cost += line_cost(p_style, p_slack[line] / double(p_width), line + 1 == p_slack.size());
			}
			return cost;
		}

		bool improves(text_wrap_style p_style, const GroupSlacks& p_baseline, const GroupSlacks& p_candidate, pixel_t p_width) {
			if (p_baseline.size() != p_candidate.size()) return false;
			bool better = false;
			for (size_t index = 0; index < p_baseline.size(); ++index) {
				const auto& original = p_baseline[index];
				const auto& selected = p_candidate[index];
				if (original.size() != selected.size()) return false;
				for (size_t row = 0; row < selected.size(); ++row) {
					if (selected[row] < -WIDTH_EPSILON && selected[row] < original[row] - WIDTH_EPSILON) return false;
				}
				const double improvement = group_cost(p_style, original, p_width) - group_cost(p_style, selected, p_width);
				if (improvement < -1e-6) return false;
				better |= improvement > 1e-6;
			}
			return better;
		}

		void choose_cuts(const Paragraph& p_paragraph, std::pair<double, double> p_capacity, size_t p_line_count,
			text_wrap_style p_style, pixel_t p_line_width, const css_hyphenation_limits& p_hyphenation_limits,
			size_t p_line_offset, const std::vector<size_t>& p_suppressed_hyphen_lines, bool p_protect_final_line,
			std::unordered_set<const render_item*>& r_cuts) {
			const auto& points = p_paragraph.points;
			const size_t last = points.size() - 1;
			if (!p_paragraph.can_plan || p_line_count <= 1 || p_line_count > last || p_capacity.first <= 0 || p_capacity.second <= 0) return;
			if (p_style == text_wrap_balance && p_line_count > MAX_BALANCED_LINES) return;
			if (p_style == text_wrap_pretty && p_line_count > MAX_PRETTY_LINES) return;
			const auto zone = p_hyphenation_limits.zone.calc_percent(p_line_width);
			const auto capacity = [&](size_t p_line) { return p_line == 0 ? p_capacity.first : p_capacity.second; };
			const auto used_width = [&](size_t p_line, size_t p_start, size_t p_end) {
				const auto& end = points[p_end];
				const auto width = end.fit_end - points[p_start].start;
				return width > capacity(p_line) ? width - end.conditional_end_reduction : width;
			};
			const auto allows_cut = [&](size_t p_line, size_t p_start, size_t p_end, size_t p_streak) {
				const auto& point = points[p_end];
				if (!point.hyphen_width) return true;
				if ((p_protect_final_line && p_line == p_line_count - 2) || !p_hyphenation_limits.allows_line(p_streak) ||
					std::find(p_suppressed_hyphen_lines.begin(), p_suppressed_hyphen_lines.end(), p_line_offset + p_line) !=
						p_suppressed_hyphen_lines.end()) return false;
				if (point.preceding_ordinary_cut <= p_start) return true;
				const auto slack = capacity(p_line) - used_width(p_line, p_start, point.preceding_ordinary_cut);
				return css_hyphenation_limits::allows_hyphen_in_zone(zone, static_cast<pixel_t>(slack));
			};
			const auto cost = [&](size_t p_line, size_t p_start, size_t p_end) {
				const double limit = capacity(p_line);
				const double width = used_width(p_line, p_start, p_end);
				const double occupied = p_end == last ? std::max(width, std::min(limit, points[p_end].full_end - points[p_start].start)) : width;
				return line_cost(p_style, (limit - occupied) / limit, p_end == last);
			};

			std::vector<size_t> greedy{ 0 };
			std::vector<size_t> greedy_streaks{ 0 };
			while (greedy.back() < last) {
				const size_t start = greedy.back();
				const size_t line = greedy.size() - 1;
				const double limit = capacity(line) + WIDTH_EPSILON;
				size_t end = start;
				for (size_t next = start + 1; next <= last; ++next) {
					const auto width = used_width(line, start, next);
					if (width - points[next].hyphen_width.value_or(0) > limit) break;
					if (width <= limit && allows_cut(line, start, next, greedy_streaks.back())) end = next;
				}
				if (end == start) return;
				greedy.push_back(end);
				greedy_streaks.push_back(points[end].hyphen_width ? greedy_streaks.back() + 1 : 0);
			}
			// The placement code remains authoritative for line count and flow geometry.
			if (greedy.size() != p_line_count + 1) return;
			const bool tail_only = p_style == text_wrap_avoid_short_last_line;
			const size_t first_row = tail_only ? p_line_count - 2 : 0;
			if (tail_only && points[last].full_end - points[greedy[p_line_count - 1]].start >= 0.35 * capacity(p_line_count - 1)) return;

			struct Choice {
				double cost = std::numeric_limits<double>::infinity();
				size_t previous = 0;
				size_t previous_streak = 0;
			};
			const size_t rows = p_line_count - first_row;
			const int line_limit = p_hyphenation_limits.lines;
			const bool track_streaks = line_limit > 0 && static_cast<size_t>(line_limit) < greedy_streaks[first_row] + rows - 1 &&
				std::any_of(points.begin(), points.end(), [](const WrapPoint& point) { return point.hyphen_width.has_value(); });
			const size_t states = track_streaks ? static_cast<size_t>(line_limit) + 1 : 1;
			std::vector<Choice> best((rows + 1) * points.size() * states);
			const auto choice = [&](size_t p_row, size_t p_point, size_t p_streak) -> Choice& {
				return best[(p_row * points.size() + p_point) * states + p_streak];
			};
			choice(0, greedy[first_row], states == 1 ? 0 : greedy_streaks[first_row]).cost = 0;
			for (size_t row = 0; row < rows; ++row) {
				const size_t line = first_row + row;
				const auto limit = capacity(line) + WIDTH_EPSILON;
				for (size_t start = greedy[first_row]; start < last; ++start) {
					for (size_t streak = 0; streak < states; ++streak) {
						const auto current = choice(row, start, streak).cost;
						if (!std::isfinite(current)) continue;
						for (size_t end = start + 1; end <= last - (rows - row - 1); ++end) {
							const auto width = used_width(line, start, end);
							if (width - points[end].hyphen_width.value_or(0) > limit) break;
							if (width > limit || !allows_cut(line, start, end, streak)) continue;
							const auto ending_streak = states > 1 && points[end].hyphen_width ? streak + 1 : 0;
							auto& target = choice(row + 1, end, ending_streak);
							const double candidate = current + cost(line, start, end);
							if (candidate < target.cost) target = { candidate, start, streak };
						}
					}
				}
			}
			double baseline = 0;
			for (size_t row = first_row; row < p_line_count; ++row) baseline += cost(row, greedy[row], greedy[row + 1]);
			if (!(choice(rows, last, 0).cost + 1e-6 < baseline)) return;

			for (size_t row = 1; row <= first_row; ++row) r_cuts.insert(points[greedy[row]].next);
			size_t end = last;
			size_t streak = 0;
			for (size_t row = rows; row > 1; --row) {
				const auto& selected = choice(row, end, streak);
				end = selected.previous;
				streak = selected.previous_streak;
				r_cuts.insert(points[end].next);
			}
		}
	}

	bool render_item_inline_context::protects_last_hyphenation(const containing_block_context& self_size) const {
		if (css().get_hyphenation_limits().last != hyphenate_last_always || self_size.measuring_inline_size || self_size.grid_intrinsic_width) return false;
		if (src_el()->tag() != empty_id) return true;
		const auto enclosing = parent();
		if (!enclosing) return true;
		bool after_this = false;
		for (const auto& sibling : enclosing->children()) {
			const auto& source = *sibling->src_el();
			if (after_this && source.in_normal_flow() && !source.is_float() && source.tag() != marker_tag()) return false;
			after_this |= sibling.get() == this;
		}
		return true;
	}

	const render_item* render_item_inline_context::last_full_line_hyphen() const {
		return m_line_boxes.size() < 2 ? nullptr : m_line_boxes[m_line_boxes.size() - 2]->m_hyphenated_at;
	}

	void render_item_inline_context::avoid_last_hyphenation(const containing_block_context& self_size, formatting_context* fmt_ctx,
			const formatting_context& initial_context) {
		const auto* marker = last_full_line_hyphen();
		if (!marker) return;
		const size_t first_changed_line = m_line_boxes.size() - 2;
		const auto tail_overflow = [&] {
			double overflow = 0;
			for (size_t index = first_changed_line; index < m_line_boxes.size(); ++index) {
				overflow -= std::min<pixel_t>(0, m_line_boxes[index]->m_wrap_slack);
			}
			return overflow;
		};
		const auto original_overflow = tail_overflow();
		const auto intrinsic = m_rendered_width;
		std::unordered_set<const render_item*> attempted;
		// Moving a word can change which line is penultimate. Stop if the same break simply moves down.
		while (attempted.size() < MAX_TERMINAL_HYPHEN_REPLAYS && attempted.insert(marker).second) {
			m_suppressed_hyphen_lines.push_back(m_line_boxes.size() - 2);
			*fmt_ctx = initial_context;
			layout_inlines(self_size, fmt_ctx);
			if (tail_overflow() > original_overflow + WIDTH_EPSILON) break;
			marker = last_full_line_hyphen();
			if (!marker) {
				m_rendered_width = intrinsic;
				return;
			}
		}
		m_suppressed_hyphen_lines.clear();
		*fmt_ctx = initial_context;
		layout_inlines(self_size, fmt_ctx);
	}

	std::pair<pixel_t, pixel_t> render_item_inline_context::line_indentation(const containing_block_context& self_size,
		bool first, bool after_break) const {
		const auto& indent = css().get_text_indent();
		const bool at_start = first && (indent.each_line || first_formatted_line());
		const bool indented = (at_start || (indent.each_line && after_break)) != indent.hanging;
		const pixel_t intrinsic = indented ? indent.length.calc_percent(0) : 0;
		if (!indented || self_size.measuring_inline_size || self_size.grid_intrinsic_width) return { intrinsic, intrinsic };
		return { indent.length.calc_percent(self_size.render_width), intrinsic };
	}

	GroupSlacks render_item_inline_context::wrap_slack_by_group() const {
		GroupSlacks result;
		bool new_group = true;
		for (const auto& line : m_line_boxes) {
			if (new_group) result.emplace_back();
			result.back().push_back(line->m_wrap_slack);
			new_group = line->ends_with_forced_break();
		}
		return result;
	}

	void render_item_inline_context::apply_text_wrap(const containing_block_context& self_size, formatting_context* fmt_ctx,
			bool protect_final_line) {
		const auto style = css().get_text_wrap_style();
		const pixel_t width = self_size.render_width;
		if (style == text_wrap_auto || style == text_wrap_stable || m_line_boxes.size() < 2 || width <= 0 ||
			self_size.measuring_inline_size || self_size.grid_intrinsic_width || fmt_ctx->get_floats_height() > 0) return;

		std::vector<Paragraph> paragraphs;
		Paragraph current;
		double advance = 0;
		double start_edges = 0;
		double collapsible_end = 0;
		double hanging_end = 0;
		const render_item* previous = nullptr;
		const render_item* previous_with_width = nullptr;
		pixel_t previous_width = 0;
		bool has_inline_edge = false;
		bool has_opening_edge = false;
		punctuation_spacing preceding_punctuation;
		punctuation_spacing terminal_punctuation;
		size_t ordinary_cut = 0;
		bool unsupported_flow = false;
		const auto terminal_reduction = [&](bool p_conditional) {
			const int flag = p_conditional ? text_spacing_allow_end : text_spacing_end;
			return terminal_punctuation.kind == spacing_punctuation::closing && (terminal_punctuation.flags & flag) ?
				terminal_punctuation.reduction : 0;
		};
		const auto finish = [&]() {
			const auto end = advance + start_edges - collapsible_end - terminal_reduction(false);
			current.points.push_back({ nullptr, advance, end - hanging_end, end, {}, 0, terminal_reduction(true) });
			paragraphs.push_back(std::move(current));
			current = {};
			advance = start_edges = collapsible_end = hanging_end = 0;
			previous = previous_with_width = nullptr;
			previous_width = 0;
			has_inline_edge = false;
			has_opening_edge = false;
			preceding_punctuation = terminal_punctuation = {};
			ordinary_cut = 0;
		};
		go_inside_inline go_inside;
		inline_selector inlines;
		elements_iterator iterator(true, &go_inside, &inlines);
		iterator.process(shared_from_this(), [&](const std::shared_ptr<render_item>& item, iterator_item_type type) {
			const auto& source = *item->src_el();
			if (source.tag() == marker_tag() && css().get_list_style_position() == list_style_position_outside) return;
			if (source.is_float() || !source.is_inline()) {
				unsupported_flow = true;
				return;
			}
			if (type == iterator_item_type_start_parent) {
				const auto edge_separates = separates_autospace(*item, true, width);
				has_inline_edge |= edge_separates;
				has_opening_edge |= edge_separates;
				const auto edge = item->content_offset_left();
				current.can_plan &= edge >= 0;
				start_edges += edge;
				return;
			}
			if (type == iterator_item_type_end_parent) {
				has_inline_edge |= separates_autospace(*item, false, width);
				const auto edge = item->content_offset_right();
				current.can_plan &= edge >= 0;
				advance += start_edges + edge;
				start_edges = 0;
				has_opening_edge = false;
				return;
			}
			if (source.is_break()) {
				unsupported_flow |= item->css().get_clear() != clear_none;
				finish();
				return;
			}
			if (source.is_space() && item->css().get_white_space_collapse() == white_space_discard) return;
			const bool collapsible = source.is_white_space();
			if (collapsible && (!previous || previous->src_el()->is_white_space())) return;
			if (source.is_preserved_tab() || current.points.size() >= MAX_WRAP_POINTS) current.can_plan = false;
			pixel_t measured = item->width();
			if (item->css().get_display() == display_inline_text) {
				litehtml::size size;
				item->src_el()->get_content_size(size, width);
				measured = size.width;
			}
			const auto punctuation = get_punctuation_spacing(source, false, measured);
			const auto prior_reduction = !has_inline_edge && preceding_punctuation.before(punctuation) ?
				preceding_punctuation.reduction : 0;
			const auto start_reduction = punctuation.at_start(false) && !has_opening_edge ? punctuation.reduction : 0;
			const bool first = !previous_with_width;
			const bool paragraph_start = !paragraphs.empty() || first_formatted_line();
			const bool trim_here = (punctuation.flags & text_spacing_all) ||
				(first && !has_inline_edge && punctuation.at_start(paragraph_start)) ||
				(!has_inline_edge && punctuation.after(preceding_punctuation));
			const auto reduction = trim_here ? punctuation.reduction : 0;
			const double spacing = previous_with_width && measured > 0 ?
				line_box::fragment_spacing(*previous_with_width, previous_width - prior_reduction, *item, measured - reduction) +
					(has_inline_edge ? 0 : text_autospace_gap(*previous_with_width->src_el(), source)) : 0;
			if (current.can_plan && previous && (previous_with_width || !is_explicit_wrap_point(*previous->src_el()))) {
				const auto* prefix = previous_with_width ? previous_with_width->src_el().get() : nullptr;
				const auto opportunity = line_box::wrap_between(*previous->src_el(), source, prefix);
				if (opportunity == line_box::wrap_opportunity::hyphen && is_hyphenation_fallback(*previous->src_el())) {
					current.can_plan = false;
				}
				if (opportunity == line_box::wrap_opportunity::normal || opportunity == line_box::wrap_opportunity::hyphen) {
					std::optional<double> hyphen;
					if (opportunity == line_box::wrap_opportunity::hyphen) hyphen = measure_hyphen(*previous->src_el());
					else ordinary_cut = current.points.size();
					const auto end = advance - collapsible_end + hyphen.value_or(0) - (hyphen ? 0 : terminal_reduction(false));
					const auto start = advance - prior_reduction + spacing + (punctuation.flags & text_spacing_all ? 0 : start_reduction - reduction);
					current.points.push_back({ item.get(), start, end - hanging_end, end, hyphen, ordinary_cut,
						hyphen ? 0 : terminal_reduction(true) });
				}
			}
			advance += start_edges + measured + spacing - prior_reduction - reduction;
			start_edges = 0;
			has_opening_edge = false;
			collapsible_end = collapsible ? measured + spacing : 0;
			hanging_end = line_box::is_hanging_space(source) ? hanging_end + measured + spacing : 0;
			previous = item.get();
			if (measured > 0) {
				previous_with_width = item.get();
				previous_width = measured - reduction;
				preceding_punctuation = get_punctuation_spacing(source, true, measured);
				if (!collapsible && !line_box::is_hanging_space(source)) terminal_punctuation = preceding_punctuation;
				has_inline_edge = false;
			}
		});
		if (unsupported_flow) return;
		if (previous) finish();
		const auto original = wrap_slack_by_group();
		if (paragraphs.size() != original.size()) return;
		std::unordered_set<const render_item*> cuts;
		const double continuation = double(width) - line_indentation(self_size, false, false).first;
		size_t line_offset = 0;
		for (size_t index = 0; index < paragraphs.size(); ++index) {
			const double first = double(width) - line_indentation(self_size, index == 0, index != 0).first;
			const auto count = original[index].size();
			choose_cuts(paragraphs[index], { first, continuation }, count, style, width, css().get_hyphenation_limits(),
				line_offset, m_suppressed_hyphen_lines, protect_final_line && index + 1 == paragraphs.size(), cuts);
			line_offset += count;
		}
		if (cuts.empty()) return;

		const auto intrinsic = m_rendered_width;
		layout_inlines(self_size, fmt_ctx, &cuts);
		if ((protect_final_line && last_full_line_hyphen()) || !improves(style, original, wrap_slack_by_group(), width)) layout_inlines(self_size, fmt_ctx);
		m_rendered_width = intrinsic;
	}
}
