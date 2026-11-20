#include "html_depth_sort.h"

#include <algorithm>
#include <limits>
#include <utility>

using namespace godot;

namespace dockit {
	namespace {
		using Slice = HTMLDepthSlice;
		using Polygon = std::vector<Vector2>;
		constexpr real_t DEPTH_EPSILON = 0.00001;

		std::pair<Polygon, Polygon> split_by_depth(const Polygon& p_polygon, const Vector3& p_difference) {
			Polygon behind, in_front;
			Vector2 previous = p_polygon.back();
			real_t prior_depth = html_depth_at(p_difference, previous);
			for (const auto& point : p_polygon) {
				const auto depth = html_depth_at(p_difference, point);
				if ((depth < 0 && prior_depth > 0) || (depth > 0 && prior_depth < 0)) {
					const auto intersection = previous.lerp(point, prior_depth / (prior_depth - depth));
					behind.push_back(intersection);
					in_front.push_back(intersection);
				}
				if (depth <= 0) behind.push_back(point);
				if (depth >= 0) in_front.push_back(point);
				previous = point;
				prior_depth = depth;
			}
			return {std::move(behind), std::move(in_front)};
		}
	}

	std::vector<HTMLDepthSlice> sort_html_depth_layers(const std::vector<HTMLDepthLayer>& p_layers) {
		std::vector<Slice> input;
		input.reserve(p_layers.size());
		for (std::size_t i = 0; i < p_layers.size(); ++i) {
			const Rect2& area = p_layers[i].bounds;
			if (!area.has_area()) continue;
			const Vector2 end = area.get_end();
			input.push_back({i, {area.position, Vector2(end.x, area.position.y), end, Vector2(area.position.x, end.y)}});
		}
		if (input.empty()) return input;
		const auto& first = p_layers[input.front().layer].depth;
		if (std::all_of(input.begin(), input.end(), [&](const Slice& p_slice) {
				const auto& depth = p_layers[p_slice.layer].depth;
				return depth.x == first.x && depth.y == first.y;
			})) {
			std::stable_sort(input.begin(), input.end(), [&](const Slice& a, const Slice& b) {
				return p_layers[a.layer].depth.z < p_layers[b.layer].depth.z;
			});
			return input;
		}

		// Partition by a plane instead of a representative Z. A crossing face contributes one polygon to each side.
		struct Branch {
			std::vector<Slice> slices;
			bool emit = false;
		};
		std::vector<Branch> stack;
		stack.push_back({std::move(input)});
		std::vector<Slice> result;
		while (!stack.empty()) {
			auto branch = std::move(stack.back());
			stack.pop_back();
			if (branch.emit || branch.slices.size() < 2) {
				for (auto& slice : branch.slices) result.push_back(std::move(slice));
				continue;
			}
			const auto& pivot = p_layers[branch.slices[branch.slices.size() / 2].layer].depth;
			Branch behind, plane, in_front;
			plane.emit = true;
			for (auto& slice : branch.slices) {
				const Vector3 difference = p_layers[slice.layer].depth - pivot;
				real_t low = std::numeric_limits<real_t>::infinity();
				real_t high = -low;
				for (const auto& point : slice.polygon) {
					const auto depth = html_depth_at(difference, point);
					low = std::min(low, depth);
					high = std::max(high, depth);
				}
				if (low >= -DEPTH_EPSILON && high <= DEPTH_EPSILON) {
					plane.slices.push_back(std::move(slice));
				} else if (high <= 0) {
					behind.slices.push_back(std::move(slice));
				} else if (low >= 0) {
					in_front.slices.push_back(std::move(slice));
				} else {
					auto [far_half, near_half] = split_by_depth(slice.polygon, difference);
					behind.slices.push_back({slice.layer, std::move(far_half)});
					in_front.slices.push_back({slice.layer, std::move(near_half)});
				}
			}
			stack.push_back(std::move(in_front));
			stack.push_back(std::move(plane));
			stack.push_back(std::move(behind));
		}
		return result;
	}
}
