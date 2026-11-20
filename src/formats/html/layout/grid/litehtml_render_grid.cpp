#include "litehtml_render_grid.h"

#include "litehtml_grid_placement.h"
#include "litehtml_grid_tracks.h"

#include <litehtml/html_tag.h>

#include <numeric>

namespace litehtml {
	namespace {

		using context = containing_block_context;
		using alignment = alignment_keyword;
		using content_edge = context::content_baseline_edge;

		bool is_definite(context::typed_pixel value) {
			return is_one_of(value.type, context::cbc_value_type_absolute, context::cbc_value_type_percentage);
		}

		std::optional<pixel_t> definite_size(context::typed_pixel value) {
			if (!is_definite(value)) return std::nullopt;
			return std::max<pixel_t>(0, value.value);
		}

		bool has_scrollable_overflow(const render_item& item) {
			return item.css().is_scroll_container();
		}

		pixel_t clamp_size(pixel_t size, context::typed_pixel min, context::typed_pixel max) {
			if (is_definite(max)) size = std::min(size, max.value);
			if (is_definite(min)) size = std::max(size, min.value);
			return std::max<pixel_t>(0, size);
		}

		bool stretches(css_alignment value, bool replaced = false) {
			return value.keyword == alignment::stretch || (value.keyword == alignment::normal && !replaced);
		}

		pixel_t position_offset(css_alignment value, pixel_t free_space) {
			if (free_space < 0 && value.overflow == alignment_overflow::safe) return 0;
			switch (value.keyword) {
				case alignment::end:
				case alignment::self_end:
				case alignment::flex_end:
				case alignment::right:
					return free_space;
				case alignment::last_baseline:
					return std::max<pixel_t>(0, free_space);
				case alignment::center:
					return free_space / 2;
				default:
					return 0;
			}
		}

		struct grid_item_alignment {
			css_alignment horizontal;
			css_alignment vertical;
			bool shared_columns = false;
			bool shared_rows = false;
		};

		css_alignment resolve_self_alignment(css_alignment self, css_alignment items) {
			return self.keyword == alignment::automatic ? items : self;
		}

		bool is_baseline_alignment(css_alignment value) {
			return is_one_of(value.keyword, alignment::first_baseline, alignment::last_baseline);
		}

		bool depends_on_grid_size(const render_item& item, bool horizontal = false) {
			const auto& css = item.css();
			const auto percentage = [](const css_length& length) {
				return !length.is_predefined() && length.units() == css_units_percentage;
			};
			return horizontal ? percentage(css.get_width()) || percentage(css.get_min_width()) || percentage(css.get_max_width()) :
				percentage(css.get_height()) || percentage(css.get_min_height()) || percentage(css.get_max_height());
		}

		struct grid_track_baselines {
			std::optional<pixel_t> first;
			std::optional<pixel_t> last;
		};

		struct grid_baseline_sample {
			std::size_t track;
			bool last;
			pixel_t distance;

			pixel_t shim(const std::vector<grid_track_baselines>& groups) const {
				return (last ? groups[track].last : groups[track].first).value_or(distance) - distance;
			}
		};

		struct grid_sizing_baselines {
			grid_baseline_sample minimum;
			grid_baseline_sample min_content;
			grid_baseline_sample preferred;
		};

		struct grid_sizing_item {
			std::optional<grid_track_contribution> contribution;
			std::optional<grid_sizing_baselines> baselines;
		};

		using grid_baseline_groups = std::vector<grid_track_baselines>;

		struct grid_row_measurement {
			enum class alignment_mode { positional, self_baseline, content_baseline } mode = alignment_mode::positional;
			std::optional<grid_baseline_sample> baseline;
			std::optional<grid_track_contribution> contribution;
		};

		content_edge grid_content_edge(const render_item& item, css_alignment self) {
			if (item.src_el()->is_replaced()) return content_edge::none;
			const auto content = item.css().get_align_content();
			const auto preference = is_baseline_alignment(self) && (content.keyword == alignment::normal || is_baseline_alignment(content)) ?
				self : content;
			if (preference.keyword == alignment::first_baseline) return content_edge::start;
			if (preference.keyword == alignment::last_baseline) return content_edge::end;
			return content_edge::none;
		}

		bool coordinates_content_baseline(const render_item& item, css_alignment self, css_alignment content) {
			if (item.src_el()->is_replaced() || !is_baseline_alignment(content) || is_baseline_alignment(self)) return false;
			const auto& margins = item.css().get_margins();
			const bool auto_top = margins.top.is_predefined();
			const bool auto_bottom = margins.bottom.is_predefined();
			if (auto_top || auto_bottom)
				return content.keyword == alignment::last_baseline ? auto_top && !auto_bottom : auto_bottom && !auto_top;
			if (content.keyword == alignment::last_baseline)
				return is_one_of(self.keyword, alignment::end, alignment::self_end, alignment::flex_end,
					alignment::normal, alignment::stretch);
			return is_one_of(self.keyword, alignment::start, alignment::self_start, alignment::flex_start,
				alignment::normal, alignment::stretch);
		}

		void set_content_baseline_context(context& cell, const render_item& item, css_alignment self, pixel_t padding = 0) {
			cell.grid_content_edge = grid_content_edge(item, self);
			cell.grid_baseline_padding = padding;
		}

		grid_baseline_sample baseline_sample(grid_track_range range, css_alignment align, pixel_t size, pixel_t offset) {
			const bool last = align.keyword == alignment::last_baseline;
			return { last ? range.end - 1 : range.start, last, last ? size - offset : offset };
		}

		grid_baseline_sample measure_row_baseline(const render_item& item, grid_track_range rows, css_alignment align) {
			return baseline_sample(rows, align, item.height(), item.baseline_or_border_bottom(align.keyword == alignment::last_baseline));
		}

		void include_baseline(grid_baseline_groups& groups, const grid_baseline_sample& sample) {
			auto& group = sample.last ? groups[sample.track].last : groups[sample.track].first;
			group = std::max(group.value_or(sample.distance), sample.distance);
		}

		std::vector<grid_track_contribution> resolve_grid_contributions(const std::vector<grid_sizing_item>& items, std::size_t tracks) {
			grid_baseline_groups minimum(tracks), min_content(tracks), preferred(tracks);
			for (const auto& item : items) {
				if (!item.baselines) continue;
				include_baseline(minimum, item.baselines->minimum);
				include_baseline(min_content, item.baselines->min_content);
				include_baseline(preferred, item.baselines->preferred);
			}
			std::vector<grid_track_contribution> result;
			result.reserve(items.size());
			for (const auto& item : items) {
				if (!item.contribution) continue;
				auto contribution = *item.contribution;
				if (item.baselines) {
					contribution.minimum += item.baselines->minimum.shim(minimum);
					contribution.min_content += item.baselines->min_content.shim(min_content);
					contribution.preferred += item.baselines->preferred.shim(preferred);
				}
				result.push_back(contribution);
			}
			return result;
		}

		std::optional<grid_baseline_sample> measure_content_baseline(const render_item& item,
			grid_track_range rows, css_alignment align) {
			if (const auto offset = item.layout_baseline(align.keyword == alignment::last_baseline))
				return baseline_sample(rows, align, item.height(), *offset);
			return std::nullopt;
		}

		grid_baseline_sample measure_column_baseline(grid_track_range columns, css_alignment align, pixel_t width, pixel_t left_margin) {
			// Horizontal writing has no vertical text baseline. The alphabetic
			// baseline of the synthesized vertical-lr box is its left border edge.
			return baseline_sample(columns, align, width, left_margin);
		}

		bool stretches_grid_height(const render_item& item, grid_item_alignment align) {
			const auto& margins = item.css().get_margins();
			return align.shared_rows || (item.css().get_height().is_predefined() &&
				stretches(align.vertical, item.src_el()->is_replaced() || item.has_preferred_aspect_ratio()) &&
				!margins.top.is_predefined() && !margins.bottom.is_predefined());
		}

		bool uses_height_for_intrinsic_width(const render_item& item) {
			const auto& css = item.css();
			if (css.has_inline_size_containment()) return false;
			if (item.has_preferred_aspect_ratio() || item.src_el()->is_replaced()) return true;
			if (is_one_of(css.get_display(), display_flex, display_inline_flex) &&
				is_one_of(css.get_flex_direction(), flex_direction_column, flex_direction_column_reverse) &&
				css.get_flex_wrap() != flex_wrap_nowrap) return true;
			return std::any_of(item.children().begin(), item.children().end(), [](const auto& child) {
				return child->src_el()->in_normal_flow() && uses_height_for_intrinsic_width(*child);
			});
		}

		rendered_width render_intrinsic_width(render_item& item, const context& grid,
			grid_item_alignment align, std::optional<pixel_t> height) {
			// Give the renderer room for unwrapped content and an indefinite percentage basis.
			context probe;
			probe.width = 1 << 20;
			probe.render_width = probe.width;
			probe.size_mode = context::size_mode_content | context::size_mode_grid_measure_width;
			if (height) {
				probe.height = {*height, context::cbc_value_type_absolute};
				if (stretches_grid_height(item, align)) probe.size_mode |= context::size_mode_grid_stretch_height;
			}
			probe.context_idx = grid.context_idx;
			auto widths = item.render(0, 0, probe, nullptr);
			const auto& css = item.css();
			const auto& block_size = css.get_height();
			const bool definite_block_size = !block_size.is_predefined() &&
				(block_size.units() != css_units_percentage || height.has_value());
			if (item.has_preferred_aspect_ratio() && css.get_width().is_predefined() &&
				css.get_width().intrinsic_size() == css_intrinsic_size::none &&
				(definite_block_size || (probe.size_mode & context::size_mode_grid_stretch_height))) {
				// A definite block size transfers through the ratio to the inline contribution.
				widths.min_width = item.width();
			}
			return widths;
		}

		struct grid_intrinsic_width {
			pixel_t minimum;
			pixel_t preferred;
		};

		grid_intrinsic_width intrinsic_content_width(const render_item& item, rendered_width measured) {
			const auto preferred = std::max<pixel_t>(0, item.width() - item.content_offset_width());
			const bool nowrap = item.css().get_text_wrap_mode() == text_wrap_nowrap;
			const auto minimum = nowrap ? preferred :
				std::clamp(measured.min_width - item.content_offset_width(), pixel_t(0), preferred);
			return { minimum, preferred };
		}

		context cell_context(render_item& item, const context& grid, grid_item_alignment align,
			std::optional<grid_intrinsic_width>& intrinsic, pixel_t width, std::optional<pixel_t> height) {
			context cell;
			cell.context_idx = grid.context_idx;
			cell.width = { width, context::cbc_value_type_absolute };
			cell.render_width = cell.width;
			if (height) cell.height = { *height, context::cbc_value_type_absolute };
			const auto& margins = item.css().get_margins();
			const bool replaced = item.css().is_replaced();
			const auto& css_width = item.css().get_width();
			const bool stretch_height = height && stretches_grid_height(item, align);
			const bool width_from_height = item.has_preferred_aspect_ratio() &&
				(!item.css().get_height().is_predefined() || stretch_height) && align.horizontal.keyword != alignment::stretch;
			if (align.shared_columns) {
				cell.size_mode |= context::size_mode_grid_assigned_width;
			} else if (css_width.is_predefined() && css_width.intrinsic_size() == css_intrinsic_size::none && !width_from_height) {
				if (stretches(align.horizontal, replaced) && !margins.left.is_predefined() && !margins.right.is_predefined()) {
					cell.size_mode |= context::size_mode_grid_assigned_width;
				} else if (!replaced) {
					if (!intrinsic) {
						const auto measured = render_intrinsic_width(item, grid, align, height);
						intrinsic = intrinsic_content_width(item, measured);
					}
					item.calc_outlines(width);
					const auto outlines = item.content_offset_width();
					const auto fit = std::clamp(width - outlines, intrinsic->minimum, intrinsic->preferred);
					// Keep the full grid area as the percentage basis.
					cell.render_width.value = fit + outlines;
					cell.size_mode |= context::size_mode_grid_assigned_width;
				}
			}
			if (stretch_height) cell.size_mode |= context::size_mode_grid_stretch_height;
			return cell;
		}

		pixel_t align_item(css_alignment align, pixel_t free_space, pixel_t& leading, pixel_t& trailing,
			const css_length& leading_style, const css_length& trailing_style) {
			const bool auto_leading = leading_style.is_predefined();
			const bool auto_trailing = trailing_style.is_predefined();
			if (free_space > 0 && (auto_leading || auto_trailing)) {
				const auto share = free_space / (auto_leading && auto_trailing ? 2 : 1);
				if (auto_leading) leading = share;
				if (auto_trailing) trailing = share;
				return auto_leading ? share : 0;
			}
			return position_offset(align, free_space);
		}

		pixel_t align_row_item(render_item& item, css_alignment self, pixel_t height) {
			const auto& style = item.css().get_margins();
			auto& margins = item.get_margins();
			if (!item.src_el()->is_replaced() && stretches(self) &&
				item.css().get_align_content().keyword == alignment::last_baseline)
				self = { alignment::self_end, alignment_overflow::safe };
			return align_item(self, height - item.height(), margins.top, margins.bottom, style.top, style.bottom);
		}

		struct grid_axis_spacing {
			pixel_t leading = 0;
			pixel_t extra_gap = 0;
		};

		grid_axis_spacing distribute_tracks(css_alignment align, pixel_t free_space, std::size_t active_tracks) {
			if (active_tracks == 0) return {};
			switch (align.keyword) {
				case alignment::space_between:
					if (free_space > 0 && active_tracks > 1)
						return { 0, free_space / static_cast<pixel_t>(active_tracks - 1) };
					return {};
				case alignment::space_around:
					if (free_space <= 0) return {};
					return { free_space / (2 * static_cast<pixel_t>(active_tracks)),
						free_space / static_cast<pixel_t>(active_tracks) };
				case alignment::space_evenly: {
					if (free_space <= 0) return {};
					const auto share = free_space / static_cast<pixel_t>(active_tracks + 1);
					return { share, share };
				}
				default:
					return { position_offset(align, free_space), 0 };
			}
		}

		struct grid_axis_geometry {
			std::vector<pixel_t> tracks;
			std::vector<pixel_t> offsets;

			grid_axis_geometry() = default;

			grid_axis_geometry(const std::vector<pixel_t>& track_sizes, const grid_track_gutters& gutters,
				pixel_t gap, pixel_t available, css_alignment align) : tracks(track_sizes) {
				const auto gutter_extent = gap * static_cast<pixel_t>(gutters.count_inside({0, tracks.size()}));
				const auto occupied = std::accumulate(tracks.begin(), tracks.end(), static_cast<double>(gutter_extent));
				const auto spacing = distribute_tracks(align, static_cast<pixel_t>(available - occupied), gutters.active_count());
				offsets.reserve(tracks.size());
				auto offset = spacing.leading;
				for (std::size_t i = 0; i < tracks.size(); ++i) {
					offsets.push_back(offset);
					offset += tracks[i] + (gutters.after(i) ? gap + spacing.extra_gap : 0);
				}
			}

			pixel_t start(grid_track_range range) const {
				return offsets[range.start];
			}

			pixel_t extent(grid_track_range range) const {
				const auto last = range.end - 1;
				// Keep single-track sizes exact even when positioned far from the origin.
				if (last == range.start) return tracks[last];
				return offsets[last] - start(range) + tracks[last];
			}

			pixel_t extent() const {
				return tracks.empty() ? 0 : offsets.back() + tracks.back();
			}

			pixel_t gap_after(std::size_t track) const {
				return offsets[track + 1] - (offsets[track] + tracks[track]);
			}
		};

		std::optional<pixel_t> row_maximum(const grid_track& track, std::optional<pixel_t> height) {
			const auto definite = [height](const grid_breadth& breadth) {
				return breadth.kind == grid_breadth_kind::length &&
					(height || breadth.length.units() != css_units_percentage);
			};
			if (!definite(track.maximum)) return std::nullopt;
			const auto basis = height.value_or(0);
			const auto maximum = track.maximum.length.calc_percent(basis);
			return definite(track.minimum) ? std::max(maximum, track.minimum.length.calc_percent(basis)) : maximum;
		}

		bool min_content_changed(const std::vector<grid_track_contribution>& before,
			const std::vector<grid_track_contribution>& after) {
			return before.size() != after.size() ||
				!std::equal(before.begin(), before.end(), after.begin(), [](const auto& a, const auto& b) {
					return a.range.start == b.range.start && a.range.end == b.range.end && a.min_content == b.min_content;
				});
		}

		std::optional<pixel_t> grid_content_baseline(const std::vector<render_item*>& items,
			const std::vector<grid_item_placement>& placements, const grid_axis_geometry& geometry,
			const std::vector<grid_track_baselines>& groups, bool last) {
			if (items.empty()) return std::nullopt;
			const auto row_of = [last](const auto& placement) {
				return last ? placement.rows.end - 1 : placement.rows.start;
			};
			auto edge = row_of(placements.front());
			for (const auto& placement : placements) {
				const auto row = row_of(placement);
				edge = last ? std::max(edge, row) : std::min(edge, row);
			}
			const auto& group = groups[edge];
			const auto start = geometry.offsets[edge];
			const auto end = start + geometry.tracks[edge];
			if (group.first && (!last || !group.last)) return start + *group.first;
			if (group.last) return end - *group.last;

			std::optional<std::size_t> baseline_item;
			std::size_t baseline_column = 0;
			for (std::size_t i = 0; i < items.size(); ++i) {
				const auto& placement = placements[i];
				if (row_of(placement) != edge) continue;
				const auto column = last ? placement.columns.end - 1 : placement.columns.start;
				if (!baseline_item || (last ? column >= baseline_column : column < baseline_column)) {
					baseline_item = i;
					baseline_column = column;
				}
			}
			const auto& item = *items[*baseline_item];
			return item.top() + item.baseline_or_border_bottom(last);
		}

	}

	struct grid_parent_span {
		const expanded_grid_tracks* parent;
		grid_track_range range;
		std::size_t offset;
	};

	struct grid_baseline_track_source {
		const grid_track_list* tracks;
		std::size_t offset;
		bool definite;

		grid_baseline_track_source slice(std::size_t start) const {
			return {tracks, offset + start, definite};
		}

		bool needs_content(grid_track_range range) const {
			return grid_range_needs_content(*tracks, {range.start + offset, range.end + offset}, definite);
		}

		bool has_auto_minimum(grid_track_range range) const {
			return grid_range_has_auto_minimum(*tracks, {range.start + offset, range.end + offset}, definite);
		}
	};

	struct grid_baseline_sink {
		grid_baseline_groups& groups;
		bool has_samples = false;
	};

	struct grid_shared_axis : grid_parent_span {
		grid_parent_span cross;
		grid_baseline_track_source baseline_tracks;
		std::optional<pixel_t> basis;
		pixel_t gap;
		const grid_axis_geometry* geometry = nullptr;
		std::vector<grid_sizing_item>* contributions = nullptr;
		grid_baseline_sink* collect_baselines = nullptr;
		const grid_baseline_groups* used_baselines = nullptr;
	};

	struct grid_subgrid_layout {
		std::optional<grid_shared_axis> columns;
		std::optional<grid_shared_axis> rows;
	};

	namespace {
		expanded_grid_tracks inherit_grid_axis(const grid_shared_axis& shared, const grid_subgrid_lines& lines) {
			expanded_grid_tracks axis;
			axis.explicit_count = shared.range.end - shared.range.start;
			const auto& parent = *shared.parent;
			axis.tracks.assign(parent.tracks.begin() + shared.range.start, parent.tracks.begin() + shared.range.end);
			for (std::size_t i = 0; i < axis.tracks.size(); ++i) {
				auto& track = axis.tracks[i];
				if (shared.geometry) {
					const grid_breadth fixed{grid_breadth_kind::length, css_length(shared.geometry->tracks[shared.range.start + i])};
					track = {fixed, fixed};
				} else if (shared.basis) {
					for (auto* bound : {&track.minimum, &track.maximum}) {
						if (bound->has_length()) bound->length = css_length(bound->length.calc_percent(*shared.basis));
					}
				}
			}
			for (const auto& [name, parent_lines] : parent.names) {
				for (const auto line : parent_lines) {
					const auto index = static_cast<std::size_t>(line) + shared.offset;
					if (index >= shared.range.start && index <= shared.range.end)
						axis.names[name].push_back(static_cast<int>(index - shared.range.start));
				}
			}
			for (const auto& [name, area] : parent.areas) {
				const auto& cross = shared.cross;
				const auto other = cross.parent->areas.find(name);
				if (other != cross.parent->areas.end() && (other->second.end + cross.offset <= cross.range.start ||
					other->second.start + cross.offset >= cross.range.end)) continue;
				const auto start = std::max(area.start + shared.offset, shared.range.start);
				const auto end = std::min(area.end + shared.offset, shared.range.end);
				if (start < end) axis.areas.emplace(name, grid_track_range{start - shared.range.start, end - shared.range.start});
			}
			add_subgrid_line_names(lines, axis.explicit_count, axis.names);
			return axis;
		}

		grid_axis_geometry subgrid_geometry(const grid_shared_axis& shared, const css_length& gap,
			pixel_t size, pixel_t leading) {
			const auto& parent = *shared.geometry;
			const auto span = shared.range;
			const auto origin = parent.start(span) + leading;
			const auto half_difference = [&](std::size_t after) {
				return gap.is_predefined() ? 0.0f : (gap.calc_percent(size) - parent.gap_after(after)) / 2;
			};
			grid_axis_geometry result;
			result.offsets.reserve(span.end - span.start);
			result.tracks.reserve(span.end - span.start);
			for (auto track = span.start; track < span.end; ++track) {
				const auto start = track == span.start ? 0 : parent.offsets[track] - origin + half_difference(track - 1);
				const auto end = track + 1 == span.end ? size :
					parent.offsets[track] - origin + parent.tracks[track] - half_difference(track);
				result.offsets.push_back(start);
				result.tracks.push_back(std::max<pixel_t>(0, end - start));
			}
			return result;
		}

		void contribute_subgrid(const grid_shared_axis& shared, const std::vector<grid_sizing_item>& contributions,
			pixel_t local_gap, pixel_t leading, pixel_t trailing) {
			if (!shared.contributions) return;
			const auto count = shared.range.end - shared.range.start;
			const auto half_difference = (local_gap - shared.gap) / 2;
			const auto append = [&](grid_sizing_item item) {
				if (item.contribution) {
					auto& size = *item.contribution;
					const auto adjustment = (size.range.start == 0 ? leading : half_difference) +
						(size.range.end == count ? trailing : half_difference);
					size.minimum += adjustment;
					size.min_content += adjustment;
					size.preferred += adjustment;
					size.range.start += shared.range.start;
					size.range.end += shared.range.start;
				}
				if (item.baselines) {
					auto& group = *item.baselines;
					for (auto* sample : {&group.minimum, &group.min_content, &group.preferred}) {
						sample->distance += sample->last ? (sample->track + 1 == count ? trailing : half_difference) :
							(sample->track == 0 ? leading : half_difference);
						sample->track += shared.range.start;
					}
				}
				shared.contributions->push_back(item);
			};
			std::size_t first_occupied = count;
			std::size_t last_occupied = 0;
			for (const auto& item : contributions) {
				if (!item.contribution) continue;
				first_occupied = std::min(first_occupied, item.contribution->range.start);
				last_occupied = std::max(last_occupied, item.contribution->range.end);
			}
			if (last_occupied == 0) {
				if (count == 1) append({grid_track_contribution{{0, 1}}});
				else {
					append({grid_track_contribution{{0, 1}, -half_difference, -half_difference, -half_difference}});
					append({grid_track_contribution{{count - 1, count}, -half_difference, -half_difference, -half_difference}});
				}
			}
			for (const auto& item : contributions) {
				append(item);
				if (!item.contribution) continue;
				// When there are empty tracks at an edge, an extra spanning
				// contribution carries the subgrid's outer sizing to that edge.
				if (first_occupied > 0 && leading && item.contribution->range.start == first_occupied) {
					auto edge = item;
					edge.contribution->range.start = 0;
					append(edge);
				}
				if (last_occupied < count && trailing && item.contribution->range.end == last_occupied) {
					auto edge = item;
					edge.contribution->range.end = count;
					append(edge);
				}
			}
		}

		pixel_t shared_baseline_offset(const grid_shared_axis& shared, const grid_axis_geometry& geometry,
			pixel_t leading, std::size_t track, bool last) {
			const auto& parent = *shared.geometry;
			const auto parent_track = shared.range.start + track;
			const auto local_start = parent.start(shared.range) + leading + geometry.offsets[track];
			if (!last) return local_start - parent.offsets[parent_track];
			return parent.offsets[parent_track] + parent.tracks[parent_track] - (local_start + geometry.tracks[track]);
		}

		void export_used_baselines(const grid_shared_axis* shared, const grid_baseline_groups& groups,
			const grid_axis_geometry& geometry, pixel_t leading) {
			if (!shared || !shared->collect_baselines) return;
			for (std::size_t index = 0; index < groups.size(); ++index) {
				const auto& group = groups[index];
				for (const bool last : {false, true}) {
					const auto distance = last ? group.last : group.first;
					if (distance) {
						include_baseline(shared->collect_baselines->groups, {shared->range.start + index, last,
							*distance + shared_baseline_offset(*shared, geometry, leading, index, last)});
						shared->collect_baselines->has_samples = true;
					}
				}
			}
		}

		grid_baseline_groups inherit_used_baselines(const grid_shared_axis* shared, const grid_axis_geometry& geometry,
			pixel_t leading) {
			if (!shared || !shared->used_baselines) return {};
			grid_baseline_groups groups(geometry.tracks.size());
			for (std::size_t index = 0; index < groups.size(); ++index) {
				const auto& group = (*shared->used_baselines)[shared->range.start + index];
				if (group.first)
					groups[index].first = *group.first - shared_baseline_offset(*shared, geometry, leading, index, false);
				if (group.last)
					groups[index].last = *group.last - shared_baseline_offset(*shared, geometry, leading, index, true);
			}
			return groups;
		}

	}

	std::shared_ptr<render_item> render_item_grid::init() {
		std::list<std::shared_ptr<render_item>> items;
		std::shared_ptr<render_item> text_run;
		bool has_text = false;
		const auto flush_text = [&] {
			if (has_text) items.push_back(text_run);
			text_run.reset();
			has_text = false;
			};

		for (const auto& child : m_children) {
			if (child->css().get_display() == display_inline_text) {
				if (!text_run) {
					auto box = std::make_shared<html_tag>(src_el(), "display: block");
					text_run = std::make_shared<render_item_block>(box);
					text_run->parent(shared_from_this());
				}
				text_run->add_child(child);
				has_text |= !child->src_el()->is_white_space();
			}
			else {
				// Out-of-flow nodes do not interrupt a run of text in the grid.
				if (child->src_el()->in_normal_flow()) flush_text();
				items.push_back(child);
			}
		}
		flush_text();
		items.sort([](const auto& a, const auto& b) { return a->css().get_order() < b->css().get_order(); });
		m_children.swap(items);

		src_el()->add_render(shared_from_this());
		for (auto& child : m_children) child = child->init();
		return shared_from_this();
	}

	size size_grid_image(const render_item& item, const context& self, const size& intrinsic, bool use_intrinsic_ratio) {
		const bool width_auto = !is_definite(self.width);
		const bool height_auto = !is_definite(self.height);
		const double natural_ratio = use_intrinsic_ratio && intrinsic.height > 0 ?
			static_cast<double>(intrinsic.width) / intrinsic.height : 0;
		const auto specified = item.css().get_aspect_ratio();
		const auto ratio = specified.value > 0 && (!specified.prefer_natural || natural_ratio == 0) ?
			specified : css_aspect_ratio{ natural_ratio, true };
		pixel_t width = width_auto ? intrinsic.width : self.render_width;
		pixel_t height = height_auto ? intrinsic.height : self.height - item.box_sizing_height();
		if (ratio.value > 0 && width_auto && height_auto) {
			// Use the intersection of the two axes' limits to scale the natural image.
			auto lower = self.min_width;
			auto upper = self.max_width;
			if (is_definite(self.min_height)) {
				const auto transferred = item.size_from_aspect_ratio(self.min_height, ratio, true);
				lower = { std::max(is_definite(lower) ? lower.value : 0, transferred), context::cbc_value_type_absolute };
			}
			if (is_definite(self.max_height)) {
				const auto transferred = item.size_from_aspect_ratio(self.max_height, ratio, true);
				upper = { is_definite(upper) ? std::min(upper.value, transferred) : transferred, context::cbc_value_type_absolute };
			}
			if (width == 0 && height > 0) width = item.size_from_aspect_ratio(height, ratio, true);
			width = clamp_size(width, lower, upper);
			height = item.size_from_aspect_ratio(width, ratio, false);
			return { width, clamp_size(height, self.min_height, self.max_height) };
		}
		if (!width_auto) width = clamp_size(width, self.min_width, self.max_width);
		if (!height_auto) height = clamp_size(height, self.min_height, self.max_height);
		if (ratio.value > 0 && width_auto) width = item.size_from_aspect_ratio(height, ratio, true);
		if (ratio.value > 0 && height_auto) height = item.size_from_aspect_ratio(width, ratio, false);
		return { clamp_size(width, self.min_width, self.max_width), clamp_size(height, self.min_height, self.max_height) };
	}

	std::optional<pixel_t> render_item_grid::find_layout_baseline(bool last) const {
		const auto& value = last ? last_baseline : first_baseline;
		if (!value) return std::nullopt;
		return content_offset_top() + *value;
	}

	rendered_width render_item_grid::_render(
		pixel_t x,
		pixel_t y,
		const context& containing_block_size,
		formatting_context*, bool
	) {
		first_baseline.reset();
		last_baseline.reset();
		occupied_row_height = 0;
		auto self = calculate_containing_block_context(containing_block_size);
		const auto* shared_columns = shared_tracks && shared_tracks->columns ? &*shared_tracks->columns : nullptr;
		const auto* shared_rows = shared_tracks && shared_tracks->rows ? &*shared_tracks->rows : nullptr;
		const bool export_columns = shared_columns && shared_columns->contributions;
		const bool export_rows = shared_rows && shared_rows->contributions;
		if (shared_columns) {
			self.min_width = {0, context::cbc_value_type_none};
			self.max_width = {0, context::cbc_value_type_none};
			self.ratio_width = false;
			if (shared_columns->geometry) {
				const auto size = std::max<pixel_t>(0, shared_columns->geometry->extent(shared_columns->range) - content_offset_width());
				self.width = {size + box_sizing_width(), context::cbc_value_type_absolute};
				self.render_width = size;
			} else {
				self.width = {0, context::cbc_value_type_auto};
				self.render_width = std::max<pixel_t>(0, containing_block_size.width - content_offset_width());
			}
		}
		if (shared_rows) {
			self.min_height = {0, context::cbc_value_type_none};
			self.max_height = {0, context::cbc_value_type_none};
			self.ratio_height = false;
			if (shared_rows->geometry) {
				const auto size = std::max<pixel_t>(0, shared_rows->geometry->extent(shared_rows->range) - content_offset_height());
				self.height = {size + box_sizing_height(), context::cbc_value_type_absolute};
			} else {
				self.height = {0, context::cbc_value_type_auto};
			}
		}
		const bool inline_grid = css().get_display() == display_inline_grid;
		const bool measure_width = containing_block_size.size_mode & context::size_mode_grid_measure_width;
		const bool fit_content = containing_block_size.size_mode & context::size_mode_content;
		const bool definite_width = (is_definite(self.width) && !(fit_content &&
			(containing_block_size.size_mode & context::size_mode_exact_width))) ||
			(!inline_grid && !fit_content && src_el()->in_normal_flow() && css().get_float() == float_none);
		const bool definite_height = is_definite(self.height);
		const auto available_width = clamp_size(self.render_width, self.min_width, self.max_width);
		const auto available_height = clamp_size(self.height - box_sizing_height(), self.min_height, self.max_height);
		const auto& specified_column_gap = css().get_column_gap();
		const auto& specified_row_gap = css().get_row_gap();
		const auto column_gap = shared_columns && specified_column_gap.is_predefined() ? css_length(shared_columns->gap) : specified_column_gap;
		const auto row_gap = shared_rows && specified_row_gap.is_predefined() ? css_length(shared_rows->gap) : specified_row_gap;
		const auto width = definite_width ? std::optional<pixel_t>(available_width) : std::nullopt;
		const auto height = definite_height ? std::optional<pixel_t>(available_height) : std::nullopt;

		auto column_axis = expand_grid_tracks(css().get_grid_template_columns(), column_gap,
			width, definite_size(self.min_width), definite_size(self.max_width));
		auto row_axis = expand_grid_tracks(css().get_grid_template_rows(), row_gap,
			height, definite_size(self.min_height), definite_size(self.max_height));
		if (shared_columns) column_axis = inherit_grid_axis(*shared_columns, *css().get_grid_template_columns().subgrid);
		if (shared_rows) row_axis = inherit_grid_axis(*shared_rows, *css().get_grid_template_rows().subgrid);
		apply_grid_template_areas(css().get_grid_template_areas(), row_axis, column_axis, shared_rows != nullptr, shared_columns != nullptr);
		const auto empty_track_sizes = [](expanded_grid_tracks axis, const grid_track_list& pattern, pixel_t gap,
			grid_track_range grid_item_placement::* dimension, std::optional<pixel_t> fallback) {
			if (fallback) return grid_track_sizes{ {}, *fallback, *fallback };
			const auto gutters = finish_grid_tracks(axis, pattern, 0, axis.explicit_count, {}, dimension);
			return size_grid_tracks(axis.tracks, {}, gutters, gap, std::nullopt, false);
		};
		std::optional<grid_track_sizes> contained_columns;
		if (css().has_inline_size_containment()) {
			contained_columns = empty_track_sizes(column_axis, css().get_grid_auto_columns(), column_gap.calc_percent(0),
				&grid_item_placement::columns, css().get_contain_intrinsic_width());
		}
		std::optional<grid_track_sizes> contained_rows;
		if (css().has_block_size_containment()) {
			contained_rows = empty_track_sizes(row_axis, css().get_grid_auto_rows(), row_gap.calc_percent(0),
				&grid_item_placement::rows, css().get_contain_intrinsic_height());
		}
		const auto& columns = column_axis.tracks;
		const auto& rows = row_axis.tracks;
		std::vector<render_item*> items;
		std::vector<grid_item_lines> positions;
		for (const auto& child : m_children) {
			if (child->src_el()->in_normal_flow()) {
				items.push_back(child.get());
				positions.push_back(child->css().get_grid_lines());
			}
		}
		const auto row_names = row_axis.areas.empty() ? grid_line_indices{} : grid_placement_names(row_axis);
		const auto column_names = column_axis.areas.empty() ? grid_line_indices{} : grid_placement_names(column_axis);
		const auto placement = place_grid_items(positions, row_axis.explicit_count, column_axis.explicit_count,
			row_axis.areas.empty() ? row_axis.names : row_names,
			column_axis.areas.empty() ? column_axis.names : column_names,
			css().get_grid_auto_flow(), shared_rows != nullptr, shared_columns != nullptr);
		const auto column_gutters = finish_grid_tracks(column_axis, css().get_grid_auto_columns(), placement.column_offset, placement.column_count,
			placement.items, &grid_item_placement::columns);
		const auto row_gutters = finish_grid_tracks(row_axis, css().get_grid_auto_rows(), placement.row_offset, placement.row_count,
			placement.items, &grid_item_placement::rows);
		const auto column_baseline_tracks = shared_columns ? shared_columns->baseline_tracks :
			grid_baseline_track_source{&columns, 0, definite_width};
		const auto row_baseline_tracks = shared_rows ? shared_rows->baseline_tracks :
			grid_baseline_track_source{&rows, 0, definite_height};

		const css_alignment start{alignment::start, alignment_overflow::safe};
		const auto justify_content = shared_columns ? start : css().get_justify_content();
		auto align_content = shared_rows ? start : css().get_align_content();
		if (!shared_rows && containing_block_size.grid_content_edge != content_edge::none) {
			align_content = { containing_block_size.grid_content_edge == content_edge::end ? alignment::end : alignment::start,
				alignment_overflow::safe };
		}
		std::vector<grid_item_alignment> alignments;
		std::vector<grid_subgrid_layout> subgrids(items.size());
		std::vector<bool> height_sensitive;
		height_sensitive.reserve(items.size());
		bool has_height_sensitive_items = false;
		bool needs_adopted_columns = false;
		alignments.reserve(items.size());
		for (std::size_t index = 0; index < items.size(); ++index) {
			const auto* item = items[index];
			auto& inherited = subgrids[index];
			const auto& placed = placement.items[index];
			const auto& style = item->css();
			const bool requests_subgrid = style.get_grid_template_columns().subgrid || style.get_grid_template_rows().subgrid;
			if (requests_subgrid && !style.has_layout_containment() && !style.has_paint_containment() &&
				dynamic_cast<render_item_grid*>(items[index])) {
				const grid_parent_span column_span{&column_axis, placed.columns, placement.column_offset};
				const grid_parent_span row_span{&row_axis, placed.rows, placement.row_offset};
				if (style.get_grid_template_columns().subgrid) {
					inherited.columns = grid_shared_axis{column_span, row_span, column_baseline_tracks.slice(placed.columns.start),
						width, column_gap.calc_percent(width.value_or(0))};
				}
				if (style.get_grid_template_rows().subgrid) {
					inherited.rows = grid_shared_axis{row_span, column_span, row_baseline_tracks.slice(placed.rows.start),
						height, row_gap.calc_percent(height.value_or(0))};
				}
			}
			alignments.push_back({
				inherited.columns ? start : resolve_self_alignment(style.get_justify_self(), css().get_justify_items()),
				inherited.rows ? start : resolve_self_alignment(style.get_align_self(), css().get_align_items()),
				inherited.columns.has_value(), inherited.rows.has_value()
			});
			const bool sensitive = uses_height_for_intrinsic_width(*item);
			height_sensitive.push_back(sensitive);
			has_height_sensitive_items |= sensitive;
			needs_adopted_columns |= sensitive && inherited.columns.has_value();
		}
		const auto with_subgrid = [&](std::size_t index, const auto& work) {
			const auto& inherited = subgrids[index];
			if (!inherited.columns && !inherited.rows) return work();
			auto& grid = static_cast<render_item_grid&>(*items[index]);
			struct restore_tracks {
				const grid_subgrid_layout*& slot;
				const grid_subgrid_layout* previous;
				~restore_tracks() { slot = previous; }
			} restore{grid.shared_tracks, grid.shared_tracks};
			grid.shared_tracks = &inherited;
			return work();
		};
		std::vector<std::optional<grid_intrinsic_width>> intrinsic(items.size());
		std::vector<bool> column_baseline_eligible(items.size());
		const auto initial_row_gap = row_gap.calc_percent(height.value_or(0));
		std::vector<std::optional<pixel_t>> row_maxima;
		std::optional<grid_axis_geometry> estimated_rows;
		if (has_height_sensitive_items) {
			if (shared_rows && shared_rows->geometry) {
				estimated_rows = subgrid_geometry(*shared_rows, specified_row_gap, available_height, content_offset_top());
			} else {
				row_maxima.reserve(rows.size());
				for (const auto& row : rows) row_maxima.push_back(row_maximum(row, height));
				if (std::all_of(row_maxima.begin(), row_maxima.end(), [](const auto& size) { return size.has_value(); })) {
					std::vector<pixel_t> sizes;
					sizes.reserve(rows.size());
					for (const auto& value : row_maxima) sizes.push_back(*value);
					estimated_rows.emplace(sizes, row_gutters, initial_row_gap, available_height, height ? align_content : start);
				}
			}
		}
		const auto heights_for_columns = [&](const grid_axis_geometry* geometry) {
			std::vector<std::optional<pixel_t>> heights(items.size());
			for (std::size_t index = 0; index < items.size(); ++index) {
				if (!height_sensitive[index]) continue;
				const auto range = placement.items[index].rows;
				if (geometry) {
					heights[index] = geometry->extent(range);
				} else {
					pixel_t extent = initial_row_gap * row_gutters.count_inside(range);
					std::size_t track = range.start;
					for (; track < range.end && row_maxima[track]; ++track) extent += *row_maxima[track];
					if (track == range.end) heights[index] = extent;
				}
			}
			return heights;
		};
		const auto measure_columns = [&](const std::vector<std::optional<pixel_t>>& block_sizes,
			const grid_axis_geometry* row_geometry, pixel_t measured_row_gap,
			const grid_axis_geometry* previous_columns, pixel_t measured_column_gap) {
			std::vector<grid_sizing_item> column_inputs;
			column_inputs.reserve(items.size());
			for (auto& inherited : subgrids) {
				if (inherited.columns) {
					inherited.columns->geometry = previous_columns;
					inherited.columns->gap = measured_column_gap;
					inherited.columns->contributions = &column_inputs;
				}
				if (inherited.rows) {
					inherited.rows->geometry = row_geometry;
					inherited.rows->gap = measured_row_gap;
					inherited.rows->contributions = nullptr;
				}
			}
			for (std::size_t index = 0; index < items.size(); ++index) {
				const auto range = placement.items[index].columns;
				auto& item = *items[index];
				const auto align = alignments[index].horizontal;
				const auto& margins = item.css().get_margins();
				const bool needs_content = grid_range_needs_content(columns, range, definite_width);
				const bool contributes = needs_content || export_columns;
				if (alignments[index].shared_columns) {
					with_subgrid(index, [&] { render_intrinsic_width(item, self, alignments[index], block_sizes[index]); });
					continue;
				}
				const bool baseline_aligned = is_baseline_alignment(align) &&
					!margins.left.is_predefined() && !margins.right.is_predefined() &&
					!(depends_on_grid_size(item, true) && column_baseline_tracks.needs_content(range));
				column_baseline_eligible[index] = baseline_aligned;
				if (!contributes && !baseline_aligned) continue;
				const auto widths = with_subgrid(index, [&] {
					return render_intrinsic_width(item, self, alignments[index], block_sizes[index]);
				});
				intrinsic[index] = intrinsic_content_width(item, widths);
				const auto& css_width = item.css().get_width();
				const bool explicit_width = !css_width.is_predefined() && css_width.units() != css_units_percentage;
				const auto& css_min_width = item.css().get_min_width();
				const bool explicit_minimum = !css_min_width.is_predefined();
				const auto min_width = explicit_minimum ? css_min_width.calc_percent(0) : 0;
				const auto outer_min_width = std::max<pixel_t>(0, min_width - item.box_sizing_width()) + item.content_offset_width();
				const bool auto_minimum = column_baseline_tracks.has_auto_minimum(range);
				const auto preferred = item.width();
				const auto min_content = explicit_width || item.src_el()->is_replaced() ? preferred :
					std::max(outer_min_width, intrinsic[index]->minimum + item.content_offset_width());
				const auto minimum = explicit_width ? preferred :
					explicit_minimum || has_scrollable_overflow(item) || !auto_minimum ? outer_min_width : min_content;
				grid_sizing_item input;
				if (baseline_aligned) {
					const auto margin = item.margin_left();
					const bool intrinsic_tracks = column_baseline_tracks.needs_content(range);
					if (const auto span = intrinsic_tracks ? std::nullopt : fixed_grid_span(columns, range, column_gutters, column_gap, width)) {
						// Fixed tracks contribute a group reference even though they cannot grow.
						const auto cell = cell_context(item, self, alignments[index], intrinsic[index], *span, block_sizes[index]);
						with_subgrid(index, [&] { item.render(0, 0, cell, nullptr); });
						const auto sample = measure_column_baseline(range, align, item.width(), item.margin_left());
						input.baselines = grid_sizing_baselines{sample, sample, sample};
					} else if (intrinsic_tracks) {
						input.baselines = grid_sizing_baselines{
							measure_column_baseline(range, align, minimum, margin),
							measure_column_baseline(range, align, min_content, margin),
							measure_column_baseline(range, align, preferred, margin)
						};
					}
				}
				if (contributes) input.contribution = grid_track_contribution{range, minimum, min_content, preferred};
				column_inputs.push_back(input);
			}
			for (auto& inherited : subgrids) {
				if (inherited.columns) inherited.columns->contributions = nullptr;
			}
			return column_inputs;
		};
		const auto* preliminary_rows = estimated_rows ? &*estimated_rows : nullptr;
		const auto preliminary_heights = heights_for_columns(preliminary_rows);
		const auto initial_column_gap = column_gap.calc_percent(width.value_or(0));
		std::optional<grid_axis_geometry> adopted_columns;
		if (has_height_sensitive_items && shared_columns && shared_columns->geometry)
			adopted_columns = subgrid_geometry(*shared_columns, specified_column_gap, available_width, content_offset_left());
		auto column_inputs = measure_columns(preliminary_heights, preliminary_rows, initial_row_gap,
			adopted_columns ? &*adopted_columns : nullptr, initial_column_gap);
		const auto column_content = resolve_grid_contributions(column_inputs, columns.size());
		pixel_t preferred_track_width;
		pixel_t minimum_track_width;
		pixel_t horizontal_gap;
		const auto solve_columns = [&](const std::vector<grid_track_contribution>& column_content) {
			const auto initial_columns = size_grid_tracks(columns, column_content, column_gutters,
				column_gap.calc_percent(width.value_or(0)), width, stretches(justify_content));
			preferred_track_width = contained_columns ? contained_columns->extent : initial_columns.extent;
			minimum_track_width = contained_columns ? contained_columns->minimum : initial_columns.minimum;
			m_pos.width = definite_width ? available_width : clamp_size(
				measure_width ? preferred_track_width : std::clamp(available_width, minimum_track_width, preferred_track_width),
				self.min_width, self.max_width);
			if (self.ratio_width && css().get_min_width().is_predefined() &&
				css().get_min_width().intrinsic_size() == css_intrinsic_size::none && !is_scrollable_overflow(css().get_overflow_x())) {
				m_pos.width = clamp_size(std::max(m_pos.width, minimum_track_width), self.min_width, self.max_width);
			}
			horizontal_gap = column_gap.calc_percent(m_pos.width);
			const auto column_sizes = !definite_width || m_pos.width != available_width ?
				size_grid_tracks(columns, column_content, column_gutters, horizontal_gap, m_pos.width, stretches(justify_content)) : initial_columns;
			return shared_columns && shared_columns->geometry ?
				subgrid_geometry(*shared_columns, specified_column_gap, m_pos.width, content_offset_left()) :
				grid_axis_geometry(column_sizes.tracks, column_gutters, horizontal_gap, m_pos.width, justify_content);
		};
		auto column_geometry = solve_columns(column_content);
		const bool refine_shared_contributions = export_columns && shared_columns->geometry && !shared_rows;
		const bool may_refine_columns = (!shared_columns || refine_shared_contributions) && (!shared_rows || shared_rows->geometry) &&
			has_height_sensitive_items;
		const auto export_inline_contributions = [&] {
			if (shared_columns) contribute_subgrid(*shared_columns, column_inputs, initial_column_gap,
				content_offset_left(), content_offset_right());
		};
		const auto intrinsic_widths = [&]() -> rendered_width {
			const auto& intrinsic = containing_block_size.intrinsic_inline;
			const bool fit = is_one_of(css().get_width().intrinsic_size(), css_intrinsic_size::fit_content, css_intrinsic_size::fit_content_limit);
			const auto minimum = intrinsic.preferred && !fit ? m_pos.width :
				std::min(m_pos.width, std::max(minimum_track_width, intrinsic.minimum.value_or(0)));
			return { m_pos.width + content_offset_width(), minimum + content_offset_width() };
		};
		if (measure_width && !may_refine_columns) {
			export_inline_contributions();
			return intrinsic_widths();
		}

		std::vector<grid_row_measurement> measurements(items.size());
		using row_alignment = grid_row_measurement::alignment_mode;
		std::vector<grid_sizing_item> row_inputs;
		row_inputs.reserve(items.size());
		pixel_t vertical_gap;
		const auto measure_and_solve_rows = [&]() {
			row_inputs.clear();
			std::fill(measurements.begin(), measurements.end(), grid_row_measurement{});
			for (auto& inherited : subgrids) {
				if (inherited.columns) {
					inherited.columns->contributions = nullptr;
					inherited.columns->geometry = &column_geometry;
					inherited.columns->gap = horizontal_gap;
				}
				if (inherited.rows) {
					inherited.rows->geometry = nullptr;
					inherited.rows->gap = initial_row_gap;
					inherited.rows->contributions = &row_inputs;
				}
			}
			for (std::size_t index = 0; index < items.size(); ++index) {
				const auto& placed = placement.items[index];
				auto& item = *items[index];
				const auto align = alignments[index];
				auto& measurement = measurements[index];
				const bool needs_content = grid_range_needs_content(rows, placed.rows, definite_height);
				const bool contributes = export_rows || needs_content || (!definite_height && grid_range_needs_content(rows, placed.rows, true));
				const auto& margins = item.css().get_margins();
				const bool dependent = depends_on_grid_size(item);
				const auto content = item.css().get_align_content();
				if (!align.shared_rows && !(dependent && row_baseline_tracks.needs_content(placed.rows))) {
					if (is_baseline_alignment(align.vertical) && !margins.top.is_predefined() && !margins.bottom.is_predefined())
						measurement.mode = row_alignment::self_baseline;
					else if (coordinates_content_baseline(item, align.vertical, content))
						measurement.mode = row_alignment::content_baseline;
				}
				const bool baseline_aligned = measurement.mode != row_alignment::positional;
				if (!contributes && !baseline_aligned && !align.shared_rows) continue;
				const auto span_height = baseline_aligned && (dependent || measurement.mode == row_alignment::content_baseline) ?
					fixed_grid_span(rows, placed.rows, row_gutters, row_gap, height) : std::nullopt;
				with_subgrid(index, [&] {
					auto cell = cell_context(item, self, align, intrinsic[index], column_geometry.extent(placed.columns), span_height);
					cell.size_mode |= context::size_mode_grid_measure_height;
					if (baseline_aligned) cell.size_mode |= context::size_mode_grid_measure_baseline;
					set_content_baseline_context(cell, item, measurement.mode == row_alignment::self_baseline ? align.vertical : css_alignment{});
					item.render(0, 0, cell, nullptr);
				});
				if (align.shared_rows) continue;
				if (!dependent || span_height) {
					if (measurement.mode == row_alignment::self_baseline)
						measurement.baseline = measure_row_baseline(item, placed.rows, align.vertical);
					else if (measurement.mode == row_alignment::content_baseline)
						measurement.baseline = measure_content_baseline(item, placed.rows, content);
				}
				if (!contributes) continue;
				const bool explicit_height = !item.css().get_height().is_predefined() &&
					item.css().get_height().units() != css_units_percentage;
				const auto& min_height = item.css().get_min_height();
				const bool explicit_minimum = !min_height.is_predefined();
				const bool auto_minimum = grid_range_has_auto_minimum(rows, placed.rows, definite_height);
				const auto minimum = !explicit_height && (explicit_minimum || has_scrollable_overflow(item) || !auto_minimum) ?
					item.content_offset_height() + (explicit_minimum ? std::max<pixel_t>(0, min_height.calc_percent(0) - item.box_sizing_height()) : 0) :
					item.height();
				measurement.contribution = grid_track_contribution{ placed.rows, minimum, item.height(), item.height() };
			}
			for (const auto& measurement : measurements) {
				if (!measurement.contribution && !measurement.baseline) continue;
				grid_sizing_item measured{measurement.contribution, std::nullopt};
				if (const auto& sample = measurement.baseline)
					measured.baselines = grid_sizing_baselines{*sample, *sample, *sample};
				row_inputs.push_back(measured);
			}
			for (auto& inherited : subgrids) {
				if (inherited.rows) inherited.rows->contributions = nullptr;
			}

			const auto row_content = resolve_grid_contributions(row_inputs, rows.size());
			const auto initial_rows = size_grid_tracks(rows, row_content, row_gutters,
				row_gap.calc_percent(height.value_or(0)), height, stretches(align_content));
			const auto preferred_track_height = contained_rows ? contained_rows->extent : initial_rows.extent;
			const auto minimum_track_height = contained_rows ? contained_rows->minimum : initial_rows.minimum;
			m_pos.height = definite_height ? available_height : clamp_size(preferred_track_height, self.min_height, self.max_height);
			if (self.ratio_height) {
				auto ratio_height = size_from_aspect_ratio(m_pos.width, css().get_aspect_ratio(), false);
				if (css().get_min_height().is_predefined() && !is_scrollable_overflow(css().get_overflow_y()))
					ratio_height = std::max(ratio_height, minimum_track_height);
				m_pos.height = clamp_size(ratio_height, self.min_height, self.max_height);
			}
			vertical_gap = row_gap.calc_percent(m_pos.height);
			const bool resolve_rows = definite_height ? m_pos.height != available_height :
				(grid_tracks_use_percentage(rows) || m_pos.height != initial_rows.extent);
			const auto row_sizes = resolve_rows ?
				size_grid_tracks(rows, row_content, row_gutters, vertical_gap, m_pos.height, stretches(align_content)) : initial_rows;
			return shared_rows && shared_rows->geometry ?
				subgrid_geometry(*shared_rows, specified_row_gap, m_pos.height, content_offset_top()) :
				grid_axis_geometry(row_sizes.tracks, row_gutters, vertical_gap, m_pos.height, align_content);
		};
		auto row_geometry = measure_and_solve_rows();
		// Revisit the inline contributions once using the resolved block geometry.
		// A subgrid on the inline axis returns those measurements to its owner.
		if (may_refine_columns) {
			const auto solved_heights = heights_for_columns(&row_geometry);
			if (needs_adopted_columns || solved_heights != preliminary_heights) {
				column_inputs = measure_columns(solved_heights, &row_geometry, vertical_gap, &column_geometry, horizontal_gap);
				if (!refine_shared_contributions) {
					const auto revised_columns = resolve_grid_contributions(column_inputs, columns.size());
					if (min_content_changed(column_content, revised_columns)) {
						const auto previous_width = m_pos.width;
						auto revised_geometry = solve_columns(revised_columns);
						const bool columns_changed = m_pos.width != previous_width || revised_geometry.tracks != column_geometry.tracks ||
							revised_geometry.offsets != column_geometry.offsets;
						column_geometry = std::move(revised_geometry);
						if (columns_changed) row_geometry = measure_and_solve_rows();
					}
				}
			}
		}
		export_inline_contributions();
		if (shared_rows) {
			contribute_subgrid(*shared_rows, row_inputs, initial_row_gap, content_offset_top(), content_offset_bottom());
		}
		if (measure_width) return intrinsic_widths();

		const auto natural_width = inline_grid && !definite_width ?
			clamp_size(preferred_track_width, self.min_width, self.max_width) : m_pos.width;
		const auto minimum_width = inline_grid && !definite_width ?
			clamp_size(minimum_track_width, self.min_width, self.max_width) :
			(definite_width && is_definite(self.width) ? m_pos.width : minimum_track_width);
		const rendered_width widths{ natural_width + content_offset_width(), minimum_width + content_offset_width() };
		const bool measuring_height = containing_block_size.size_mode & context::size_mode_grid_measure_height;
		const bool measuring_baseline = containing_block_size.size_mode & context::size_mode_grid_measure_baseline;
		if (measuring_height && !measuring_baseline) return widths;

		for (auto& inherited : subgrids) {
			if (inherited.columns) {
				inherited.columns->geometry = &column_geometry;
				inherited.columns->gap = horizontal_gap;
			}
			if (inherited.rows) {
				inherited.rows->contributions = nullptr;
				inherited.rows->geometry = &row_geometry;
				inherited.rows->gap = vertical_gap;
			}
		}
		occupied_row_height = row_geometry.extent() - (row_geometry.offsets.empty() ? 0 : row_geometry.offsets.front());
		track_extent = { column_geometry.extent(), row_geometry.extent() };
		grid_baseline_groups row_baselines(rows.size());
		grid_baseline_groups column_baselines(columns.size());
		const auto inherited_row_baselines = inherit_used_baselines(shared_rows, row_geometry, content_offset_top());
		const auto inherited_column_baselines = inherit_used_baselines(shared_columns, column_geometry, content_offset_left());
		std::vector<grid_baseline_sink> row_sources(items.size(), {row_baselines});
		std::vector<grid_baseline_sink> column_sources(items.size(), {column_baselines});
		for (std::size_t index = 0; index < items.size(); ++index) {
			auto& inherited = subgrids[index];
			if (inherited.rows) {
				inherited.rows->collect_baselines = &row_sources[index];
				if (!inherited_row_baselines.empty()) inherited.rows->used_baselines = &inherited_row_baselines;
			}
			if (inherited.columns) {
				inherited.columns->collect_baselines = &column_sources[index];
				if (!inherited_column_baselines.empty()) inherited.columns->used_baselines = &inherited_column_baselines;
			}
		}
		std::vector<std::optional<grid_baseline_sample>> final_columns(items.size());

		const auto render_final_cell = [&](std::size_t index) {
			const auto& placed = placement.items[index];
			const position area{ column_geometry.start(placed.columns), row_geometry.start(placed.rows),
				column_geometry.extent(placed.columns), row_geometry.extent(placed.rows) };
			auto& item = *items[index];
			const auto align = alignments[index];
			auto& measurement = measurements[index];
			with_subgrid(index, [&] {
				auto cell = cell_context(item, self, align, intrinsic[index], area.width, area.height);
				if (!align.shared_rows)
					set_content_baseline_context(cell, item, measurement.mode == row_alignment::self_baseline ? align.vertical : css_alignment{});
				item.render(area.x, area.y, cell, nullptr);
			});
		};
		for (std::size_t index = 0; index < items.size(); ++index) {
			render_final_cell(index);
			const auto& placed = placement.items[index];
			auto& item = *items[index];
			const auto align = alignments[index];
			auto& measurement = measurements[index];
			const auto height = row_geometry.extent(placed.rows);
			if (column_baseline_eligible[index]) {
				final_columns[index] = measure_column_baseline(placed.columns, align.horizontal, item.width(), item.margin_left());
				include_baseline(column_baselines, *final_columns[index]);
			}
			measurement.baseline.reset();
			if (align.shared_rows) continue;
			if (measurement.mode == row_alignment::self_baseline) {
				measurement.baseline = measure_row_baseline(item, placed.rows, align.vertical);
			} else {
				const auto space = height - item.height();
				const auto top = align_row_item(item, align.vertical, height);
				item.pos().y += top;
				if (measurement.mode == row_alignment::content_baseline) {
					const auto content = item.css().get_align_content();
					const bool last = content.keyword == alignment::last_baseline;
					if (top == (last ? space : 0)) {
						measurement.baseline = measure_content_baseline(item, placed.rows, content);
					}
				}
			}
			if (measurement.baseline) include_baseline(row_baselines, *measurement.baseline);
		}
		export_used_baselines(shared_rows, row_baselines, row_geometry, content_offset_top());
		export_used_baselines(shared_columns, column_baselines, column_geometry, content_offset_left());
		const auto& used_row_baselines = inherited_row_baselines.empty() ? row_baselines : inherited_row_baselines;
		const auto& used_column_baselines = inherited_column_baselines.empty() ? column_baselines : inherited_column_baselines;

		// An owning axis can now send the resolved groups to each participating
		// subgrid. An axis still being collected by an ancestor waits for that pass.
		for (std::size_t index = 0; index < items.size(); ++index) {
			auto& inherited = subgrids[index];
			bool final_pass = false;
			if (inherited.rows && inherited.rows->collect_baselines) {
				inherited.rows->collect_baselines = nullptr;
				if (row_sources[index].has_samples && !inherited.rows->used_baselines &&
					(!shared_rows || !shared_rows->collect_baselines)) {
					inherited.rows->used_baselines = &used_row_baselines;
					final_pass = true;
				}
			}
			if (inherited.columns && inherited.columns->collect_baselines) {
				inherited.columns->collect_baselines = nullptr;
				if (column_sources[index].has_samples && !inherited.columns->used_baselines &&
					(!shared_columns || !shared_columns->collect_baselines)) {
					inherited.columns->used_baselines = &used_column_baselines;
					final_pass = true;
				}
			}
			if (final_pass) {
				const auto y = items[index]->pos().y;
				render_final_cell(index);
				items[index]->pos().y = y;
			}
		}
		for (std::size_t index = 0; index < items.size(); ++index) {
			auto& measurement = measurements[index];
			const auto& baseline = measurement.baseline;
			if (!baseline) continue;
			auto& item = *items[index];
			const auto& placed = placement.items[index];
			const auto shim = baseline->shim(used_row_baselines);
			const auto height = row_geometry.extent(placed.rows);
			if (measurement.mode == row_alignment::self_baseline) {
				item.pos().y += baseline->last ? height - item.height() - shim : shim;
			} else if (shim > 0) {
				const auto width = column_geometry.extent(placed.columns);
				auto cell = cell_context(item, self, alignments[index], intrinsic[index], width, height);
				const auto& css = item.css();
				const bool auto_height = css.get_height().is_predefined() && !(cell.size_mode & context::size_mode_grid_stretch_height);
				// For a constrained last-baseline item, overflow follows safe end.
				if (!auto_height && baseline->last && shim > item.pos().height - item.baseline_content_height()) continue;
				set_content_baseline_context(cell, item, {}, shim);
				with_subgrid(index, [&] {
					item.render(column_geometry.start(placed.columns), row_geometry.start(placed.rows), cell, nullptr);
				});
				item.pos().y += align_row_item(item, alignments[index].vertical, height);
			}
		}
		for (std::size_t index = 0; index < items.size(); ++index) {
			auto& item = *items[index];
			if (alignments[index].shared_columns) continue;
			const auto range = placement.items[index].columns;
			const auto align = alignments[index].horizontal;
			const auto& css_margins = item.css().get_margins();
			auto& used = item.get_margins();
			if (!final_columns[index]) {
				item.pos().x += align_item(align, column_geometry.extent(range) - item.width(),
					used.left, used.right, css_margins.left, css_margins.right);
			}
		}
		for (std::size_t index = 0; index < items.size(); ++index) {
			if (const auto& baseline = final_columns[index]) {
				auto& item = *items[index];
				const auto shim = baseline->shim(used_column_baselines);
				item.pos().x += baseline->last ? column_geometry.extent(placement.items[index].columns) - item.width() - shim : shim;
			}
		}
		if (!inherited_row_baselines.empty()) {
			for (std::size_t index = 0; index < row_baselines.size(); ++index) {
				auto& group = row_baselines[index];
				const auto& inherited = inherited_row_baselines[index];
				if (group.first && inherited.first) group.first = inherited.first;
				if (group.last && inherited.last) group.last = inherited.last;
			}
		}
		first_baseline = grid_content_baseline(items, placement.items, row_geometry, row_baselines, false);
		last_baseline = grid_content_baseline(items, placement.items, row_geometry, row_baselines, true);

		for (std::size_t index = 0; index < items.size(); ++index) {
			auto& item = *items[index];
			const auto& placed = placement.items[index];
			const auto width = column_geometry.extent(placed.columns);
			const auto height = row_geometry.extent(placed.rows);
			if (item.css().get_position() == element_position_relative) {
				context cell;
				cell.width = width;
				cell.height = height;
				item.apply_relative_shift(cell);
			} else if (item.css().get_position() == element_position_sticky) {
				item.set_sticky_grid_area({ column_geometry.start(placed.columns), row_geometry.start(placed.rows), width, height });
			}
		}

		context positioned;
		positioned.context_idx = self.context_idx;
		positioned.width = { m_pos.width, context::cbc_value_type_absolute };
		positioned.render_width = positioned.width;
		positioned.height = { m_pos.height, context::cbc_value_type_absolute };
		for (const auto& child : m_children) {
			if (child->src_el()->in_normal_flow()) continue;
			const auto natural = child->render(0, 0, positioned, nullptr).natural_width;
			if (child->css().get_width().is_predefined() && natural < child->width())
				child->render(0, 0, positioned.new_width(natural), nullptr);
		}
		m_pos.move_to(x + content_offset_left(), y + content_offset_top());
		return widths;
	}

}
