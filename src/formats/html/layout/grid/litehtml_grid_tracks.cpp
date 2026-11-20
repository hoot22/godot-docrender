#include "litehtml_grid_tracks.h"

#include <litehtml/html.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace litehtml {
	namespace {
		bool uses_percentage(const grid_breadth& breadth) {
			return breadth.has_length() && breadth.length.units() == css_units_percentage;
		}

		grid_breadth_kind used_kind(const grid_breadth& breadth, bool definite) {
			if (definite || !uses_percentage(breadth)) return breadth.kind;
			return breadth.kind == grid_breadth_kind::fit_content ? grid_breadth_kind::max_content : grid_breadth_kind::automatic;
		}

		bool is_intrinsic(grid_breadth_kind kind) {
			return !is_one_of(kind, grid_breadth_kind::length, grid_breadth_kind::fraction);
		}

		std::size_t auto_repeat_count(const grid_track_template& axis, const css_length& gap,
			std::optional<pixel_t> definite, std::optional<pixel_t> minimum, std::optional<pixel_t> maximum) {
			const auto upper = definite ? definite : maximum;
			const auto bound = upper ? upper : minimum;
			if (!bound) return 1;

			const auto basis = std::max(*bound, minimum.value_or(0));
			const auto track_breadth = [basis](const grid_track& track) {
				const auto resolve = [basis](const grid_breadth& breadth) {
					return breadth.kind == grid_breadth_kind::length ? static_cast<double>(breadth.length.calc_percent(basis)) : 0.0;
				};
				return std::max({ 0.0, resolve(track.minimum), resolve(track.maximum) });
			};
			const double gap_size = std::max<pixel_t>(0, gap.calc_percent(basis));
			// Charge one gutter per track and then remove the trailing gutter.
			double fixed_extent = -gap_size;
			for (const auto* segment : { &axis.prefix, &axis.suffix })
				for (const auto& track : segment->tracks) fixed_extent += track_breadth(track) + gap_size;
			const auto& pattern = axis.repeat->pattern.tracks;
			double pattern_extent = 0;
			for (const auto& track : pattern) pattern_extent += std::max(1.0, track_breadth(track)) + gap_size;
			if (!std::isfinite(basis) || !std::isfinite(fixed_extent) || !std::isfinite(pattern_extent)) return 1;
			const double remaining = std::max(0.0, basis - fixed_extent);
			const double repetitions = remaining / pattern_extent;
			// Track and gap lengths can round before this division (e.g. two 45% tracks and a 10% gap).
			const double tolerance = std::numeric_limits<pixel_t>::epsilon() * std::max(1.0, repetitions);
			const double count = upper ? std::floor(repetitions + tolerance) : std::ceil(repetitions - tolerance);
			const auto limit = (grid_track_limit - axis.prefix.tracks.size() - axis.suffix.tracks.size()) / pattern.size();
			return static_cast<std::size_t>(std::clamp(count, 1.0, static_cast<double>(limit)));
		}

		std::size_t span_of(grid_track_range range) {
			return range.end - range.start;
		}

		struct sized_track {
			double base = 0;
			double limit = 0;
			double factor = 0;
			double fit_limit = std::numeric_limits<double>::infinity();
			grid_breadth_kind minimum = grid_breadth_kind::automatic;
			grid_breadth_kind maximum = grid_breadth_kind::automatic;
			bool unbounded = false;
		};

		void grow_to_limits(std::vector<sized_track>& tracks, double& free_space) {
			std::vector<sized_track*> growable;
			for (auto& track : tracks)
				if (track.limit > track.base) growable.push_back(&track);
			std::sort(growable.begin(), growable.end(), [](const auto* a, const auto* b) {
				return a->limit - a->base < b->limit - b->base;
				});
			for (std::size_t index = 0; index < growable.size() && free_space > 0; ++index) {
				auto& track = *growable[index];
				const double growth = std::min(track.limit - track.base,
					free_space / static_cast<double>(growable.size() - index));
				track.base += growth;
				free_space -= growth;
			}
		}

		double find_fraction(std::vector<const sized_track*>& flexible, double space) {
			double factors = 0;
			for (const auto* track : flexible) factors += track->factor;
			std::sort(flexible.begin(), flexible.end(), [](const auto* a, const auto* b) {
				return a->base / a->factor > b->base / b->factor;
				});
			// Freeze the tracks whose minimum exceeds their share, largest ratio first.
			for (const auto* candidate : flexible) {
				const auto& track = *candidate;
				if (space / std::max(1.0, factors) * track.factor >= track.base) break;
				space -= track.base;
				factors -= track.factor;
			}
			return std::max(0.0, space) / std::max(1.0, factors);
		}

		void grow_flexible_tracks(std::vector<sized_track>& tracks, double& free_space) {
			std::vector<const sized_track*> flexible;
			double space = free_space;
			for (const auto& track : tracks) {
				if (track.factor <= 0) continue;
				flexible.push_back(&track);
				space += track.base;
			}
			const double fraction = find_fraction(flexible, space);
			for (auto& track : tracks) {
				if (track.factor <= 0) continue;
				const double size = std::max(track.base, track.factor * fraction);
				free_space -= size - track.base;
				track.base = size;
			}
		}

		struct track_increase {
			std::size_t index;
			double capacity;
			double factor;
			double increase = 0;
		};

		enum class span_sizing { intrinsic_base, min_content_base, max_content_base, minimum_limit, preferred_limit };

		bool affects_track(grid_breadth_kind kind, span_sizing step) {
			switch (step) {
				case span_sizing::intrinsic_base:
				case span_sizing::minimum_limit:
					return is_intrinsic(kind);
				case span_sizing::min_content_base:
					return is_one_of(kind, grid_breadth_kind::min_content, grid_breadth_kind::max_content);
				case span_sizing::max_content_base:
					return kind == grid_breadth_kind::max_content;
				case span_sizing::preferred_limit:
					return is_intrinsic(kind) && kind != grid_breadth_kind::min_content;
			}
			return false;
		}

		// Equal distribution up to each limit, with saturated tracks frozen.
		void distribute_within_limits(std::vector<track_increase>& entries, double& space) {
			std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.capacity < b.capacity; });
			for (std::size_t i = 0; i < entries.size() && space > 0; ++i) {
				auto& entry = entries[i];
				const double growth = std::min(entry.capacity, space / (entries.size() - i));
				entry.increase += growth;
				space -= growth;
			}
		}

		void span_increase(const std::vector<sized_track>& sizes, const grid_track_contribution& item, double span_gaps,
			bool flexible, span_sizing step, std::vector<double>& planned, std::vector<track_increase>& eligible,
			std::vector<track_increase>& others) {
			const bool grow_limits = step == span_sizing::minimum_limit || step == span_sizing::preferred_limit;
			const bool use_preferred = step == span_sizing::max_content_base || step == span_sizing::preferred_limit;
			double space = (use_preferred ? item.preferred :
				step == span_sizing::intrinsic_base ? item.minimum : item.min_content) - span_gaps;
			eligible.clear();
			others.clear();
			for (auto i = item.range.start; i < item.range.end; ++i) {
				const auto& track = sizes[i];
				const double current = grow_limits ? track.limit : track.base;
				space -= current;
				const auto kind = grow_limits ? track.maximum : track.minimum;
				const bool applies = affects_track(kind, step) && (!flexible || track.maximum == grid_breadth_kind::fraction);
				const auto limit = track.unbounded || track.maximum == grid_breadth_kind::fraction ?
					track.fit_limit : std::min(track.fit_limit, track.limit);
				const double capacity = std::max(0.0, limit - current);
				if (applies) {
					eligible.push_back({ i, capacity, track.factor });
				} else if (!flexible && capacity > 0 &&
					!(step == span_sizing::preferred_limit && kind == grid_breadth_kind::min_content)) {
					others.push_back({ i, capacity, 0 });
				}
			}
			if (space <= 0 || eligible.empty()) return;

			if (flexible) {
				const double factors = std::accumulate(eligible.begin(), eligible.end(), 0.0,
					[](double sum, const auto& entry) { return sum + entry.factor; });
				const double equal = space * std::max(0.0, 1.0 - factors) / eligible.size();
				for (auto& entry : eligible) entry.increase = space * entry.factor / std::max(1.0, factors) + equal;
			} else {
				distribute_within_limits(eligible, space);
				distribute_within_limits(others, space);
				for (const auto& entry : others)
					planned[entry.index] = std::max(planned[entry.index], entry.increase);

				// Distribute beyond the current growth limits to eligible intrinsic maxima.
				// A fit-content maximum stops accepting growth at its argument.
				others.clear();
				for (const auto& entry : eligible) {
					const auto& track = sizes[entry.index];
					const bool preferred = use_preferred ?
						is_one_of(track.maximum, grid_breadth_kind::max_content, grid_breadth_kind::automatic, grid_breadth_kind::fit_content) :
						is_intrinsic(track.maximum);
					if (!preferred) continue;
					const double current = grow_limits ? track.limit : track.base;
					const double capacity = track.fit_limit - current - entry.increase;
					if (capacity > 0) others.push_back({ entry.index, capacity, 0, entry.increase });
				}
				distribute_within_limits(others, space);
				// Minimum contributions can grow beyond a fit-content cap if needed.
				const double extra = !grow_limits ? space / eligible.size() : 0;
				for (auto& entry : eligible) entry.increase += extra;
				for (const auto& entry : others)
					planned[entry.index] = std::max(planned[entry.index], entry.increase + extra);
			}
			for (const auto& entry : eligible)
				planned[entry.index] = std::max(planned[entry.index], entry.increase);
		}

		void resolve_spans(std::vector<sized_track>& sizes, const std::vector<const grid_track_contribution*>& spans,
			const grid_track_gutters& gutters, double gap, bool flexible) {
			if (spans.empty()) return;
			std::vector<double> planned(sizes.size());
			std::vector<track_increase> eligible;
			std::vector<track_increase> others;
			for (auto group = spans.begin(); group != spans.end();) {
				const auto group_end = flexible ? spans.end() : std::find_if(group + 1, spans.end(), [&](const auto* item) {
					return span_of(item->range) != span_of((*group)->range);
				});
				const auto plan_increases = [&](span_sizing step) {
					std::fill(planned.begin(), planned.end(), 0);
					for (auto it = group; it != group_end; ++it) {
						const auto& item = **it;
						span_increase(sizes, item, gap * gutters.count_inside(item.range), flexible, step, planned, eligible, others);
					}
				};
				for (const auto step : { span_sizing::intrinsic_base, span_sizing::min_content_base, span_sizing::max_content_base }) {
					plan_increases(step);
					for (std::size_t i = 0; i < sizes.size(); ++i) {
						sizes[i].base += planned[i];
						sizes[i].limit = std::max(sizes[i].limit, sizes[i].base);
					}
				}
				if (!flexible) {
					plan_increases(span_sizing::minimum_limit);
					for (std::size_t i = 0; i < sizes.size(); ++i) sizes[i].limit += planned[i];
					plan_increases(span_sizing::preferred_limit);
					for (std::size_t i = 0; i < sizes.size(); ++i) {
						sizes[i].limit += planned[i];
					}
					// The unbounded tracks can absorb both limit increases for this span length.
					for (auto it = group; it != group_end; ++it)
						for (auto i = (*it)->range.start; i < (*it)->range.end; ++i) sizes[i].unbounded = false;
				}
				group = group_end;
			}
		}
	}

	expanded_grid_tracks expand_grid_tracks(const grid_track_template& axis, const css_length& gap,
		std::optional<pixel_t> definite, std::optional<pixel_t> minimum, std::optional<pixel_t> maximum) {
		expanded_grid_tracks expanded;
		const auto count = axis.repeat ? auto_repeat_count(axis, gap, definite, minimum, maximum) : 0;
		const auto repeated = count ? count * axis.repeat->pattern.tracks.size() : 0;
		const auto insertion = axis.prefix.tracks.size();
		expanded.tracks.reserve(insertion + repeated + axis.suffix.tracks.size());
		append_grid_segment(expanded, axis.prefix);
		for (std::size_t i = 0; i < count; ++i)
			append_grid_segment(expanded, axis.repeat->pattern);
		append_grid_segment(expanded, axis.suffix);
		if (axis.repeat && axis.repeat->fit) expanded.fit_range = grid_track_range{ insertion, insertion + repeated };
		return expanded;
	}

	void apply_grid_template_areas(const grid_area_template& areas, expanded_grid_tracks& rows, expanded_grid_tracks& columns,
		bool shared_rows, bool shared_columns) {
		// An area can extend the explicit grid without assigning track sizes.
		if (!shared_rows) rows.explicit_count = std::max(rows.tracks.size(), areas.rows);
		if (!shared_columns) columns.explicit_count = std::max(columns.tracks.size(), areas.columns);
		for (const auto& [name, bounds] : areas.areas) {
			const auto row_end = std::min(bounds.rows.end, rows.explicit_count);
			const auto column_end = std::min(bounds.columns.end, columns.explicit_count);
			if (bounds.rows.start >= row_end || bounds.columns.start >= column_end) continue;
			rows.areas.insert_or_assign(name, grid_track_range{bounds.rows.start, row_end});
			columns.areas.insert_or_assign(name, grid_track_range{bounds.columns.start, column_end});
		}
	}

	grid_line_indices grid_placement_names(const expanded_grid_tracks& axis) {
		auto names = axis.names;
		const auto add_name = [](grid_line_indices& names, const std::string& name, std::size_t index) {
			auto& lines = names[name];
			const auto line = static_cast<int>(index);
			const auto at = std::lower_bound(lines.begin(), lines.end(), line);
			if (at == lines.end() || *at != line) lines.insert(at, line);
		};
		for (const auto& [name, bounds] : axis.areas) {
			add_name(names, name + "-start", bounds.start);
			add_name(names, name + "-end", bounds.end);
		}
		return names;
	}

	grid_track_gutters finish_grid_tracks(expanded_grid_tracks& axis, const grid_track_list& pattern, std::size_t offset, std::size_t count,
		const std::vector<grid_item_placement>& items, grid_track_range grid_item_placement::* dimension) {
		const auto sized_end = offset + axis.tracks.size();
		axis.tracks.insert(axis.tracks.begin(), offset, grid_track{});
		axis.tracks.resize(count);
		if (!pattern.empty()) {
			for (std::size_t i = 0; i < offset; ++i)
				axis.tracks[offset - 1 - i] = pattern[pattern.size() - 1 - i % pattern.size()];
			for (auto i = sized_end; i < count; ++i)
				axis.tracks[i] = pattern[(i - sized_end) % pattern.size()];
		}
		grid_track_gutters gutters{ count, {} };
		if (!axis.fit_range) return gutters;

		const auto start = axis.fit_range->start + offset;
		const auto end = axis.fit_range->end + offset;
		std::vector<std::ptrdiff_t> coverage(end - start + 1);
		for (const auto& item : items) {
			const auto& range = item.*dimension;
			const auto first = std::max(start, range.start);
			const auto last = std::min(end, range.end);
			if (first < last) {
				++coverage[first - start];
				--coverage[last - start];
			}
		}
		const css_length zero(0);
		const grid_breadth zero_breadth{ grid_breadth_kind::length, zero };
		gutters.active_before.resize(count + 1);
		std::ptrdiff_t occupied = 0;
		for (std::size_t i = 0; i < count; ++i) {
			bool active = true;
			if (i >= start && i < end) {
				occupied += coverage[i - start];
				active = occupied > 0;
				if (!active) axis.tracks[i] = { zero_breadth, zero_breadth };
			}
			gutters.active_before[i + 1] = gutters.active_before[i] + active;
		}
		if (gutters.active_before.back() == count) gutters.active_before.clear();
		return gutters;
	}

	std::size_t grid_track_gutters::count_inside(grid_track_range range) const {
		const auto active = active_before.empty() ? span_of(range) : active_before[range.end] - active_before[range.start];
		return active > 0 ? active - 1 : 0;
	}

	bool grid_track_gutters::after(std::size_t track) const {
		if (active_before.empty()) return track + 1 < track_count;
		return active_before[track] < active_before[track + 1] && active_before[track + 1] < active_before.back();
	}

	bool grid_tracks_use_percentage(const grid_track_list& tracks) {
		return std::any_of(tracks.begin(), tracks.end(), [](const auto& track) {
			return uses_percentage(track.minimum) || uses_percentage(track.maximum);
			});
	}

	bool grid_range_needs_content(const grid_track_list& tracks, grid_track_range range, bool definite) {
		for (auto i = range.start; i < range.end; ++i) {
			const auto& track = tracks[i];
			if (is_intrinsic(used_kind(track.minimum, definite)) ||
				is_intrinsic(used_kind(track.maximum, definite)) ||
				(!definite && track.maximum.kind == grid_breadth_kind::fraction && track.maximum.fraction > 0)) return true;
		}
		return false;
	}

	std::optional<pixel_t> fixed_grid_span(const grid_track_list& tracks, grid_track_range range,
		const grid_track_gutters& gutters, const css_length& gap, std::optional<pixel_t> available) {
		const auto gutter_count = gutters.count_inside(range);
		if (!available && gutter_count && gap.units() == css_units_percentage) return std::nullopt;
		const auto basis = available.value_or(0);
		auto extent = static_cast<pixel_t>(gutter_count) * gap.calc_percent(basis);
		for (auto i = range.start; i < range.end; ++i) {
			const auto& track = tracks[i];
			if (used_kind(track.minimum, available.has_value()) != grid_breadth_kind::length ||
				used_kind(track.maximum, available.has_value()) != grid_breadth_kind::length) return std::nullopt;
			const auto minimum = track.minimum.length.calc_percent(basis);
			if (minimum < track.maximum.length.calc_percent(basis)) return std::nullopt;
			extent += minimum;
		}
		return extent;
	}

	bool grid_range_has_auto_minimum(const grid_track_list& tracks, grid_track_range range, bool definite) {
		bool automatic = false;
		for (auto i = range.start; i < range.end; ++i) {
			if (span_of(range) > 1 && tracks[i].maximum.kind == grid_breadth_kind::fraction) return false;
			automatic |= used_kind(tracks[i].minimum, definite) == grid_breadth_kind::automatic;
		}
		return automatic;
	}

	grid_track_sizes size_grid_tracks(
		const grid_track_list& tracks,
		const std::vector<grid_track_contribution>& contributions,
		const grid_track_gutters& gutters,
		pixel_t gap,
		std::optional<pixel_t> available, bool stretch
	) {
		grid_track_sizes result;
		if (tracks.empty()) return result;
		const double gaps = static_cast<double>(gap) * gutters.count_inside({ 0, tracks.size() });
		const auto basis = available.value_or(0);
		std::vector<sized_track> sizes;
		sizes.reserve(tracks.size());
		std::vector<grid_track_contribution> singles(tracks.size());
		std::vector<const grid_track_contribution*> spans;
		std::vector<const grid_track_contribution*> flexible_spans;
		for (const auto& item : contributions) {
			if (span_of(item.range) == 1) {
				auto& single = singles[item.range.start];
				single.minimum = std::max(single.minimum, item.minimum);
				single.min_content = std::max(single.min_content, item.min_content);
				single.preferred = std::max(single.preferred, item.preferred);
			} else {
				bool crosses_flexible = false;
				for (auto i = item.range.start; i < item.range.end; ++i)
					if (tracks[i].maximum.kind == grid_breadth_kind::fraction) {
						crosses_flexible = true;
						break;
					}
				(crosses_flexible ? flexible_spans : spans).push_back(&item);
			}
		}
		double intrinsic_fraction = 0;
		for (std::size_t i = 0; i < tracks.size(); ++i) {
			const auto& track = tracks[i];
			const auto& content = singles[i];
			const auto min_kind = used_kind(track.minimum, available.has_value());
			const auto max_kind = used_kind(track.maximum, available.has_value());
			const double fixed_max = max_kind == grid_breadth_kind::length ? track.maximum.length.calc_percent(basis) : 0;
			sized_track size;
			size.minimum = min_kind;
			size.maximum = max_kind;
			switch (min_kind) {
				case grid_breadth_kind::length: size.base = track.minimum.length.calc_percent(basis); break;
				case grid_breadth_kind::min_content: size.base = content.min_content; break;
				case grid_breadth_kind::max_content: size.base = content.preferred; break;
				default: size.base = content.minimum; break;
			}
			if (min_kind == grid_breadth_kind::automatic && max_kind == grid_breadth_kind::length)
				size.base = std::min(size.base, fixed_max);
			size.base = std::max(0.0, size.base);
			if (max_kind == grid_breadth_kind::fit_content)
				size.fit_limit = std::max<pixel_t>(0, track.maximum.length.calc_percent(basis));
			const double max_contribution = max_kind == grid_breadth_kind::min_content ? content.min_content : content.preferred;
			const double growth_limit = is_intrinsic(max_kind) ? std::min(size.fit_limit, max_contribution) : fixed_max;
			size.limit = std::max(size.base, growth_limit);
			size.factor = track.maximum.fraction;
			size.unbounded = is_intrinsic(max_kind) && content.preferred == 0;
			sizes.push_back(size);
		}
		std::sort(spans.begin(), spans.end(), [](const auto* a, const auto* b) { return span_of(a->range) < span_of(b->range); });
		resolve_spans(sizes, spans, gutters, gap, false);
		resolve_spans(sizes, flexible_spans, gutters, gap, true);
		const double minimum = std::accumulate(sizes.begin(), sizes.end(), gaps, [](double sum, const auto& track) {
			return sum + track.base;
		});
		result.minimum = static_cast<pixel_t>(minimum);
		if (available) {
			double free_space = std::max(0.0, *available - minimum);
			grow_to_limits(sizes, free_space);
			grow_flexible_tracks(sizes, free_space);
			if (stretch && free_space > 0) {
				const auto automatic = std::count_if(sizes.begin(), sizes.end(), [](const auto& track) {
					return track.maximum == grid_breadth_kind::automatic;
				});
				if (automatic)
					for (auto& track : sizes)
						if (track.maximum == grid_breadth_kind::automatic) track.base += free_space / static_cast<double>(automatic);
			}
		}
		else {
			for (std::size_t i = 0; i < sizes.size(); ++i)
				if (sizes[i].factor > 0)
					intrinsic_fraction = std::max(intrinsic_fraction, std::max(sizes[i].base, static_cast<double>(singles[i].preferred)) /
						std::max(1.0, sizes[i].factor));
			std::vector<const sized_track*> flexible;
			for (const auto* item : flexible_spans) {
				flexible.clear();
				double space = item->preferred - static_cast<double>(gap) * gutters.count_inside(item->range);
				for (auto i = item->range.start; i < item->range.end; ++i) {
					const auto& track = sizes[i];
					if (track.factor > 0) flexible.push_back(&track);
					else space -= track.limit;
				}
				intrinsic_fraction = std::max(intrinsic_fraction, find_fraction(flexible, space));
			}
			for (auto& track : sizes) track.base = std::max(track.limit, track.factor * intrinsic_fraction);
		}
		double extent = gaps;
		result.tracks.reserve(sizes.size());
		for (const auto& size : sizes) {
			result.tracks.push_back(static_cast<pixel_t>(size.base));
			extent += size.base;
		}
		result.extent = static_cast<pixel_t>(extent);
		return result;
	}
}
