#include "html_polygon_clip.h"
#include "html_plane_transform.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace godot;

namespace dockit {
	namespace {
		bool filled(int p_winding, bool p_even_odd) {
			return p_even_odd ? p_winding % 2 != 0 : p_winding != 0;
		}

		struct Edge {
			double low;
			double high;
			double x;
			double slope;
			double min_x;
			double max_x;
			int winding;

			double at(double p_y) const { return x + (p_y - low) * slope; }
		};

		struct Contour {
			std::vector<Edge> edges;
			bool even_odd;
		};

		struct ClipArea {
			struct Outline {
				Contour contour;
				std::vector<ClipArea> clips;
			};
			std::vector<Outline> outlines;
			std::vector<ClipArea> clips;
		};

		struct Span {
			const Edge* left;
			const Edge* right;
		};

		std::vector<Span> filled_spans(const Contour& p_contour, double p_y) {
			std::vector<std::pair<double, const Edge*>> crossings;
			for (const auto& edge : p_contour.edges) {
				if (p_y >= edge.low && p_y < edge.high) crossings.emplace_back(edge.at(p_y), &edge);
			}
			std::sort(crossings.begin(), crossings.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
			std::vector<Span> spans;
			const Edge* opening = nullptr;
			int winding = 0;
			for (size_t index = 0; index < crossings.size();) {
				const auto [x, edge] = crossings[index];
				const bool was_filled = filled(winding, p_contour.even_odd);
				do {
					winding += crossings[index++].second->winding;
				} while (index < crossings.size() && crossings[index].first == x);
				const bool is_filled = filled(winding, p_contour.even_odd);
				if (!was_filled && is_filled) opening = edge;
				else if (was_filled && !is_filled) spans.push_back({ opening, edge });
			}
			return spans;
		}

		std::vector<Span> intersect_spans(const std::vector<Span>& p_a, const std::vector<Span>& p_b, double p_y) {
			std::vector<Span> intersection;
			size_t a = 0;
			size_t b = 0;
			while (a < p_a.size() && b < p_b.size()) {
				const auto* left = p_a[a].left->at(p_y) > p_b[b].left->at(p_y) ? p_a[a].left : p_b[b].left;
				const double right_a = p_a[a].right->at(p_y);
				const double right_b = p_b[b].right->at(p_y);
				if (left->at(p_y) < std::min(right_a, right_b)) {
					intersection.push_back({ left, right_a <= right_b ? p_a[a].right : p_b[b].right });
				}
				if (right_a <= right_b) ++a;
				if (right_b <= right_a) ++b;
			}
			return intersection;
		}

		std::vector<Span> merge_spans(std::vector<Span> p_spans, double p_y) {
			std::sort(p_spans.begin(), p_spans.end(), [p_y](const Span& a, const Span& b) {
				return a.left->at(p_y) < b.left->at(p_y);
			});
			std::vector<Span> merged;
			for (const auto& span : p_spans) {
				if (merged.empty() || merged.back().right->at(p_y) < span.left->at(p_y)) {
					merged.push_back(span);
				} else if (merged.back().right->at(p_y) < span.right->at(p_y)) {
					merged.back().right = span.right;
				}
			}
			return merged;
		}

		std::vector<Span> area_spans(const ClipArea& p_area, double p_y) {
			std::vector<Span> spans;
			for (const auto& outline : p_area.outlines) {
				auto filled = filled_spans(outline.contour, p_y);
				for (const auto& clip : outline.clips) {
					if (filled.empty()) break;
					filled = intersect_spans(filled, area_spans(clip, p_y), p_y);
				}
				spans.insert(spans.end(), filled.begin(), filled.end());
			}
			if (p_area.outlines.size() > 1) spans = merge_spans(std::move(spans), p_y);
			for (const auto& clip : p_area.clips) {
				if (spans.empty()) break;
				spans = intersect_spans(spans, area_spans(clip, p_y), p_y);
			}
			return spans;
		}

		ClipArea build_clip_area(const HTMLClipPolygon& p_polygon, const Rect2& p_common,
			std::vector<double>& r_heights, std::vector<const Edge*>& r_edges) {
			ClipArea result;
			for (const auto& outline : p_polygon.outlines) {
				Contour contour{{}, outline.even_odd};
				outline.for_each_edge([&](const Vector2& a, const Vector2& b) {
					const auto& start = a.y <= b.y ? a : b;
					const auto& end = a.y <= b.y ? b : a;
					if (start.y >= end.y || start.y >= p_common.get_end().y || end.y <= p_common.position.y) return;
					contour.edges.push_back({ start.y, end.y, start.x,
						(static_cast<double>(end.x) - start.x) / (static_cast<double>(end.y) - start.y),
						std::min(start.x, end.x), std::max(start.x, end.x), a.y < b.y ? 1 : -1 });
					if (start.y > p_common.position.y) r_heights.push_back(start.y);
					if (end.y < p_common.get_end().y) r_heights.push_back(end.y);
				});
				if (contour.edges.empty()) continue;
				for (const auto& edge : contour.edges) r_edges.push_back(&edge);
				auto& entry = result.outlines.emplace_back(ClipArea::Outline{ std::move(contour), {} });
				for (const auto& clip : outline.clips) {
					entry.clips.push_back(build_clip_area(clip, p_common, r_heights, r_edges));
				}
			}
			for (const auto& clip : p_polygon.clips) {
				result.clips.push_back(build_clip_area(clip, p_common, r_heights, r_edges));
			}
			return result;
		}

		Rect2 clip_bounds(Rect2 p_bounds, const std::vector<HTMLClipPolygon>& p_clips) {
			for (const auto& clip : p_clips) {
				if (!p_bounds.has_area()) break;
				p_bounds = p_bounds.intersection(clip.bounds());
			}
			return p_bounds;
		}
	}

	void round_clip_polygon_vertices(std::vector<Vector2>& r_vertices, double p_radius, double p_tolerance) {
		if (!(p_radius > 0) || r_vertices.size() < 3) return;

		r_vertices.erase(std::unique(r_vertices.begin(), r_vertices.end()), r_vertices.end());
		if (r_vertices.size() > 1 && r_vertices.front() == r_vertices.back()) r_vertices.pop_back();
		const size_t count = r_vertices.size();
		if (count < 3) return;

		std::vector<Vector2> rounded;
		rounded.reserve(count);
		Vector2 previous = r_vertices.back();
		for (size_t i = 0; i < count; ++i) {
			const Vector2 vertex = r_vertices[i];
			const Vector2 to_previous = previous - vertex;
			const Vector2 to_next = r_vertices[(i + 1) % count] - vertex;
			previous = vertex;
			const double incoming_length = std::hypot(to_previous.x, to_previous.y);
			const double outgoing_length = std::hypot(to_next.x, to_next.y);
			const Vector2 incoming = to_previous / incoming_length;
			const Vector2 outgoing = to_next / outgoing_length;
			const double cross = incoming.cross(outgoing);
			const double dot = incoming.dot(outgoing);
			if (std::abs(cross) < 1e-12) {
				rounded.push_back(vertex);
				continue;
			}

			const double half_angle = std::atan2(std::abs(cross), dot) * 0.5;
			const double tangent = std::tan(half_angle);
			// Each vertex may use at most half of either adjoining edge.
			const double distance = std::min(p_radius / tangent, std::min(incoming_length, outgoing_length) * 0.5);
			const double radius = distance * tangent;
			const double sweep = std::atan2(-cross, -dot);
			const double step = 4 * std::asin(std::sqrt(std::min(0.5, p_tolerance / radius / 2)));
			const int pieces = static_cast<int>(std::clamp(std::ceil(std::abs(sweep) / std::max(step, 1e-9)), 1.0, 4096.0));
			const Vector2 start = vertex + incoming * distance;
			const Vector2 radius_direction = Vector2(-incoming.y, incoming.x) * std::copysign(1.0, sweep);
			rounded.push_back(start);
			for (int segment = 1; segment < pieces; ++segment) {
				const double angle = sweep * segment / pieces;
				const double half_sine = std::sin(angle * 0.5);
				// Sample relative to the tangent point to avoid subtracting a distant circle center.
				rounded.push_back(start - radius_direction * (2 * radius * half_sine * half_sine) -
					incoming * (radius * std::sin(std::abs(angle))));
			}
			rounded.push_back(vertex + outgoing * distance);
		}
		r_vertices = std::move(rounded);
	}

	Rect2 HTMLClipOutline::bounds() const {
		Rect2 bounds;
		bool started = false;
		for (const auto& vertices : contours) {
			if (vertices.size() < 3) continue;
			for (const auto& point : vertices) {
				bounds = started ? bounds.expand(point) : Rect2(point, {});
				started = true;
			}
		}
		return clip_bounds(bounds, clips);
	}

	Rect2 HTMLClipPolygon::bounds() const {
		Rect2 total;
		for (const auto& outline : outlines) {
			const Rect2 box = outline.bounds();
			if (box.has_area()) total = total.has_area() ? total.merge(box) : box;
		}
		return clip_bounds(total, clips);
	}

	bool HTMLClipOutline::contains_point(const Vector2& p_point) const {
		int winding = 0;
		for_each_edge([&](const Vector2& a, const Vector2& b) {
			const bool downward = a.y <= p_point.y && b.y > p_point.y;
			const bool upward = b.y <= p_point.y && a.y > p_point.y;
			if (downward || upward) {
				const double cross_x = a.x + (static_cast<double>(b.x) - a.x) *
					(static_cast<double>(p_point.y) - a.y) / (static_cast<double>(b.y) - a.y);
				if (cross_x > p_point.x) winding += downward ? 1 : -1;
			}
		});
		return filled(winding, even_odd) && std::all_of(clips.begin(), clips.end(), [&](const HTMLClipPolygon& clip) {
			return clip.contains_point(p_point);
		});
	}

	bool HTMLClipPolygon::contains_point(const Vector2& p_point) const {
		return std::all_of(clips.begin(), clips.end(), [&](const HTMLClipPolygon& clip) {
			return clip.contains_point(p_point);
		}) && std::any_of(outlines.begin(), outlines.end(), [&](const HTMLClipOutline& outline) {
			return outline.contains_point(p_point);
		});
	}

	HTMLClipPolygon HTMLClipPolygon::transformed(const HTMLPlaneTransform& p_matrix) const {
		HTMLClipPolygon result;
		for (const auto& outline : outlines) {
			auto& output = result.outlines.emplace_back();
			output.even_odd = outline.even_odd;
			for (const auto& contour : outline.contours) {
				auto points = p_matrix.project_polygon(contour);
				if (points.size() >= 3) output.contours.push_back(std::move(points));
			}
			for (const auto& clip : outline.clips) output.clips.push_back(clip.transformed(p_matrix));
		}
		for (const auto& clip : clips) result.clips.push_back(clip.transformed(p_matrix));
		return result;
	}

	Rect2 intersect_clip_polygons(const std::vector<HTMLClipPolygon>& p_polygons) {
		if (p_polygons.empty()) return {};
		Rect2 common;
		for (const auto& polygon : p_polygons) {
			const Rect2 bounds = polygon.bounds();
			common = &polygon == &p_polygons.front() ? bounds : common.intersection(bounds);
			if (!common.has_area()) return {};
		}

		std::vector<ClipArea> areas;
		areas.reserve(p_polygons.size());
		std::vector<double> heights{ common.position.y, common.get_end().y };
		std::vector<const Edge*> edges;
		for (const auto& polygon : p_polygons) {
			if (areas.emplace_back(build_clip_area(polygon, common, heights, edges)).outlines.empty()) return {};
		}

		// The scanline edge order is fixed between vertices and intersections.
		for (size_t i = 0; i < edges.size(); ++i) {
			const auto& a = *edges[i];
			for (size_t j = i + 1; j < edges.size(); ++j) {
				const auto& b = *edges[j];
				if (a.slope == b.slope || a.max_x < b.min_x || b.max_x < a.min_x) continue;
				const double low = std::max({ a.low, b.low, static_cast<double>(common.position.y) });
				const double high = std::min({ a.high, b.high, static_cast<double>(common.get_end().y) });
				if (low >= high) continue;
				const double crossing = low + (b.at(low) - a.at(low)) / (a.slope - b.slope);
				if (crossing > low && crossing < high) heights.push_back(crossing);
			}
		}
		std::sort(heights.begin(), heights.end());
		heights.erase(std::unique(heights.begin(), heights.end()), heights.end());

		double left = std::numeric_limits<double>::infinity();
		double right = -left;
		double top = left;
		double bottom = right;
		for (size_t band = 1; band < heights.size(); ++band) {
			const double start = heights[band - 1];
			const double end = heights[band];
			const double y = start + (end - start) * 0.5;
			auto spans = area_spans(areas.front(), y);
			for (size_t area = 1; area < areas.size() && !spans.empty(); ++area) {
				spans = intersect_spans(spans, area_spans(areas[area], y), y);
			}
			for (const auto& span : spans) {
				left = std::min({ left, span.left->at(start), span.left->at(end) });
				right = std::max({ right, span.right->at(start), span.right->at(end) });
				top = std::min(top, start);
				bottom = std::max(bottom, end);
			}
		}
		if (!(left < right && top < bottom)) return {};
		return Rect2(left, top, right - left, bottom - top).intersection(common);
	}
}
