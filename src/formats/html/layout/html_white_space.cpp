#include "formats/html/backend/litehtml_document_adapter.h"

#include <litehtml/el_text.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <iterator>
#include <unordered_set>
#include <vector>

namespace litehtml {
	namespace {
		using TrimmedItems = std::unordered_set<const render_item*>;

		bool is_block_container(const element& p_element) {
			return !p_element.is_replaced() && is_one_of(p_element.css().get_display(), display_block, display_inline_block,
				display_list_item, display_table_cell, display_table_caption);
		}

		bool is_out_of_flow(const render_item& p_item) {
			return p_item.src_el()->is_float() ||
				is_one_of(p_item.css().get_position(), element_position_absolute, element_position_fixed);
		}

		struct Range {
			size_t begin;
			size_t end;
		};

		struct TrimRequest {
			Range content;
			int flags;
			bool trim_through_segment_break;
		};

		struct Flow {
			std::vector<const render_item*> parts;
			std::vector<TrimRequest> requests;

			void trim(TrimmedItems& r_removed) const {
				if (requests.empty() || parts.empty()) return;

				std::vector<size_t> content;
				std::vector<size_t> noncollapsible;
				std::vector<size_t> segment_breaks;
				for (size_t index = 0; index < parts.size(); ++index) {
					const auto& source = *parts[index]->src_el();
					const auto* text = dynamic_cast<const el_text*>(&source);
					const bool segment_break = text && text->get_source_text() == "\n";
					const bool document_space = source.is_space() && (!source.is_break() || segment_break);
					if (segment_break) segment_breaks.push_back(index);
					if (!document_space) content.push_back(index);
					if (!document_space || (!source.is_white_space() && source.css().get_white_space_collapse() != white_space_discard)) {
						noncollapsible.push_back(index);
					}
				}

				std::vector<Range> cuts;
				const auto cut = [&](size_t p_begin, size_t p_end) {
					if (p_begin < p_end) cuts.push_back({ p_begin, p_end });
				};
				for (const auto& request : requests) {
					const auto [begin, end] = request.content;
					if (request.flags & white_space_discard_before) {
						const auto previous = std::lower_bound(noncollapsible.begin(), noncollapsible.end(), begin);
						cut(previous == noncollapsible.begin() ? 0 : *std::prev(previous) + 1, begin);
					}
					if (request.flags & white_space_discard_after) {
						const auto next = std::lower_bound(noncollapsible.begin(), noncollapsible.end(), end);
						cut(end, next == noncollapsible.end() ? parts.size() : *next);
					}
					if (!(request.flags & white_space_discard_inner)) continue;

					const auto first = std::lower_bound(content.begin(), content.end(), begin);
					const auto after_last = std::lower_bound(first, content.end(), end);
					const size_t leading_end = first == after_last ? end : *first;
					const size_t trailing_begin = first == after_last ? begin : *std::prev(after_last) + 1;
					if (request.trim_through_segment_break) {
						const auto leading_break = std::lower_bound(segment_breaks.begin(), segment_breaks.end(), leading_end);
						if (leading_break != segment_breaks.begin()) cut(begin, *std::prev(leading_break) + 1);
						const auto trailing_break = std::lower_bound(segment_breaks.begin(), segment_breaks.end(), trailing_begin);
						if (trailing_break != segment_breaks.end()) cut(*trailing_break, end);
					}
					else {
						cut(begin, leading_end);
						cut(trailing_begin, end);
					}
				}

				// Combine the cuts before pruning. Nested boxes may request the same whitespace.
				std::sort(cuts.begin(), cuts.end(), [](const Range& p_left, const Range& p_right) { return p_left.begin < p_right.begin; });
				size_t removed_through = 0;
				for (const auto& range : cuts) {
					for (size_t index = std::max(removed_through, range.begin); index < range.end; ++index) {
						r_removed.insert(parts[index]);
					}
					removed_through = std::max(removed_through, range.end);
				}
			}
		};

		void trim_context(render_item& p_context, TrimmedItems& r_removed);

		void collect_flow(render_item& p_parent, Flow& r_flow, TrimmedItems& r_removed) {
			for (const auto& child : p_parent.children()) {
				const auto& source = *child->src_el();
				const auto display = child->css().get_display();
				const bool outside_marker = source.tag() == marker_tag() && p_parent.css().get_list_style_position() == list_style_position_outside;
				if (is_out_of_flow(*child) || outside_marker) {
					trim_context(*child, r_removed);
					continue;
				}

				const size_t start = r_flow.parts.size();
				const int flags = child->css().get_white_space_trim();
				if (display == display_inline && !source.is_break() && !source.is_replaced()) {
					collect_flow(*child, r_flow, r_removed);
					if (flags) r_flow.requests.push_back({ { start, r_flow.parts.size() }, flags, false });
				}
				else {
					// Non-inline contents belong to their own context. The box itself interrupts this flow.
					r_flow.parts.push_back(child.get());
					const int outer = flags & (white_space_discard_before | white_space_discard_after);
					if (outer && is_block_container(source)) r_flow.requests.push_back({ { start, start + 1 }, outer, false });
					if (!child->children().empty()) trim_context(*child, r_removed);
				}
			}
		}

		void trim_context(render_item& p_context, TrimmedItems& r_removed) {
			Flow flow;
			collect_flow(p_context, flow, r_removed);
			if (is_block_container(*p_context.src_el()) && (p_context.css().get_white_space_trim() & white_space_discard_inner)) {
				flow.requests.push_back({ { 0, flow.parts.size() }, white_space_discard_inner, true });
			}
			flow.trim(r_removed);
		}

		bool uses_trim(render_item& p_item) {
			return p_item.css().get_white_space_trim() || std::any_of(p_item.children().begin(), p_item.children().end(), [](const auto& p_child) {
				return uses_trim(*p_child);
			});
		}

		void prune_items(render_item& p_item, const TrimmedItems& p_removed) {
			p_item.children().remove_if([&](const auto& p_child) { return p_removed.count(p_child.get()) != 0; });
			for (const auto& child : p_item.children()) prune_items(*child, p_removed);
		}
	}

	void dockit_editor::trim_white_space(render_item& p_root) {
		if (!uses_trim(p_root)) return;
		TrimmedItems removed;
		trim_context(p_root, removed);
		if (!removed.empty()) prune_items(p_root, removed);
	}
}
