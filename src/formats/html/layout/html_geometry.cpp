#include "html_geometry.h"
#include "html_polygon_clip.h"
#include "html_svg_clip.h"
#include "html_svg_geometry.h"
#include "html_transform_interpolation.h"

#include <litehtml/document.h>
#include <litehtml/render_item.h>

#include <godot_cpp/variant/projection.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace godot;
using namespace litehtml;

namespace dockit {
	Rect2 html_scroll_margin_area(const Rect2& p_box, const css::ComputedStyle& p_style) {
		const auto& margin = p_style.get_scroll_margin();
		const Vector2 start = p_box.position - Vector2(margin[3].calc_percent(0), margin[0].calc_percent(0));
		const Vector2 end = p_box.get_end() + Vector2(margin[1].calc_percent(0), margin[2].calc_percent(0));
		return {start, (end - start).max(Vector2())};
	}

	Rect2 html_scroll_padding_area(const Rect2& p_port, const css::ComputedStyle& p_style) {
		const auto& padding = p_style.get_scroll_padding();
		const auto inset = [&](size_t side, real_t basis) {
			return std::clamp<real_t>(padding[side].calc_percent(basis), 0, basis);
		};
		const Vector2 start(inset(3, p_port.size.x), inset(0, p_port.size.y));
		const Vector2 end(inset(1, p_port.size.x), inset(2, p_port.size.y));
		return {p_port.position + start, (p_port.size - start - end).max(Vector2())};
	}

	Vector2 html_scroll_delta(const Rect2& p_target, const Rect2& p_port, std::optional<float> p_align,
		const HTMLScrollOptions* p_options) {
		const auto nearest = [](real_t start, real_t size, real_t port, real_t extent) {
			const real_t leading = start - port;
			const real_t trailing = leading + size - extent;
			if (leading < 0 && trailing > 0) return real_t(0);
			if ((leading < 0 && size <= extent) || (trailing > 0 && size > extent)) return leading;
			if ((trailing > 0 && size <= extent) || (leading < 0 && size > extent)) return trailing;
			return real_t(0);
		};
		const Vector2 start = p_target.position - p_port.position;
		if (p_options) {
			using Alignment = HTMLScrollOptions::Alignment;
			const auto aligned = [&](int p_axis, Alignment p_value) -> real_t {
				const auto difference = p_target.size[p_axis] - p_port.size[p_axis];
				switch (p_value) {
					case Alignment::START: return start[p_axis];
					case Alignment::CENTER: return start[p_axis] + difference * 0.5;
					case Alignment::END: return start[p_axis] + difference;
					case Alignment::NEAREST: return nearest(p_target.position[p_axis], p_target.size[p_axis],
						p_port.position[p_axis], p_port.size[p_axis]);
				}
				return 0;
			};
			return {aligned(0, p_options->inline_axis), aligned(1, p_options->block)};
		}
		if (p_align) {
			const real_t align = std::isfinite(*p_align) ? std::clamp(*p_align, 0.0f, 1.0f) : 0.0f;
			// HTMLLabel's align parameter positions the target's leading edge.
			return {start.x < 0 || start.x >= p_port.size.x ? start.x : real_t(0),
				start.y - p_port.size.y * align};
		}
		return {nearest(p_target.position.x, p_target.size.x, p_port.position.x, p_port.size.x),
			nearest(p_target.position.y, p_target.size.y, p_port.position.y, p_port.size.y)};
	}

	HTMLSnapResult html_select_scroll_snap(const css::ComputedStyle& p_style, const Rect2& p_snapport,
		const std::vector<HTMLSnapArea>& p_areas, const HTMLSnapRequest& p_request) {
		const auto& type = p_style.get_scroll_snap_type();
		HTMLSnapResult result{p_request.end.clamp(Vector2(), p_request.limit), {}};
		if (!type.enabled() || p_areas.empty()) return result;
		const Vector2 natural = result.offset;
		const Vector2 direction = natural - p_request.start;
		const bool directional = p_request.method == HTMLSnapMethod::direction;
		const bool enabled[] = {type.x && p_request.axes.x, type.y && p_request.axes.y};
		struct Candidate {
			const HTMLSnapArea* area;
			real_t position;
		};
		struct Choice {
			const HTMLSnapArea* area = nullptr;
			real_t position = 0;
			bool stopped = false;
		};
		const auto overlaps = [&](const HTMLSnapArea& area, int axis, real_t offset) {
			const real_t port_start = p_snapport.position[axis] + offset;
			const real_t port_end = port_start + p_snapport.size[axis];
			return area.area.position[axis] < port_end && area.area.get_end()[axis] > port_start;
		};
		const auto alignment = [&](const HTMLSnapArea& area, int axis) -> std::optional<real_t> {
			const auto& align = area.style->get_scroll_snap_align();
			const auto value = axis == 0 ? align.inline_axis : align.block;
			if (value == scroll_snap_align::none) return {};
			const real_t extra = area.area.size[axis] - p_snapport.size[axis];
			const bool rtl = axis == 0 && (extra > 0 ? area.style->is_right_to_left() : p_style.is_right_to_left());
			real_t position = area.area.position[axis] - p_snapport.position[axis];
			if (value == scroll_snap_align::center) position += extra / 2;
			else if ((value == scroll_snap_align::end) != rtl) position += extra;
			position = std::clamp(position, real_t(0), p_request.limit[axis]);
			return overlaps(area, axis, position) ? std::optional(position) : std::nullopt;
		};
		const auto choose = [&](int axis, const Choice& other) {
			const int cross = 1 - axis;
			const real_t origin = p_request.start[axis];
			const real_t end = natural[axis];
			const real_t length = p_snapport.size[axis];
			std::vector<Candidate> candidates;
			for (const auto& area : p_areas) {
				const auto position = alignment(area, axis);
				if (!position) continue;
				if (other.area && (!overlaps(area, cross, other.position) || !overlaps(*other.area, axis, *position))) continue;
				if (!other.area && !overlaps(area, cross, natural[cross])) {
					const real_t corridor_start = p_snapport.position[cross] + std::min(natural[cross], p_request.start[cross]);
					const real_t corridor_end = p_snapport.position[cross] + p_snapport.size[cross] +
						std::max(natural[cross], p_request.start[cross]);
					const bool crosses_area = directional && area.area.position[cross] < corridor_end && area.area.get_end()[cross] > corridor_start;
					if (!crosses_area && !(enabled[cross] && alignment(area, cross))) continue;
				}
				candidates.push_back({&area, *position});
			}
			std::vector<real_t> positions;
			positions.reserve(candidates.size());
			for (const auto& candidate : candidates) positions.push_back(candidate.position);
			std::sort(positions.begin(), positions.end());

			Choice best;
			Choice stop;
			real_t best_distance = std::numeric_limits<real_t>::infinity();
			for (const auto& candidate : candidates) {
				const auto& area = *candidate.area;
				const bool preferred = p_request.preferred[axis] && p_request.preferred[axis] == area.element;
				const real_t travel = candidate.position - origin;
				const real_t intended = direction[axis];
				const bool forward = (intended > 0 && travel > 0.01) || (intended < 0 && travel < -0.01);
				if (directional && forward && area.style->stops_scroll_snap() && std::abs(travel) <= std::abs(intended) &&
					(!stop.area || std::abs(travel) < std::abs(stop.position - origin))) {
					stop = {&area, candidate.position, true};
				}

				real_t position = candidate.position;
				if (area.area.size[axis] > length && (!preferred || p_request.method == HTMLSnapMethod::relayout)) {
					const real_t first = area.area.position[axis] - p_snapport.position[axis];
					const real_t last = area.area.get_end()[axis] - p_snapport.get_end()[axis];
					if (end >= first && end <= last) {
						const auto next = std::upper_bound(positions.begin(), positions.end(), end);
						if (next == positions.begin() || next == positions.end() || *next - *std::prev(next) > length) position = end;
					}
				}
				if (directional && !forward && position == candidate.position) continue;
				const real_t distance = std::abs(position - end);
				if (!preferred && !type.mandatory && distance > length * real_t(0.3)) continue;
				if (preferred || distance < best_distance) {
					best = {&area, position, false};
					best_distance = distance;
					if (preferred) break;
				}
			}
			return stop.area ? stop : best;
		};

		const int first = enabled[0] && (!enabled[1] || std::abs(direction.x) > std::abs(direction.y)) ? 0 : 1;
		const int second = 1 - first;
		Choice choices[2];
		if (enabled[first]) choices[first] = choose(first, {});
		if (enabled[second]) choices[second] = choose(second, choices[first]);
		// A candidate encountered earlier in a diagonal corridor can be out of view at the natural endpoint.
		for (int axis = 0; axis < 2; ++axis) {
			const auto& choice = choices[axis];
			const auto& cross = choices[1 - axis];
			if (!choice.area || !overlaps(*choice.area, 1 - axis, cross.area ? cross.position : natural[1 - axis])) continue;
			result.offset[axis] = choice.position;
			result.stopped[axis] = choice.stopped;
			result.areas[axis] = choice.area->element;
		}
		return result;
	}

	namespace {
		// Insets constrain the border box. Its position box limits how far it can move.
		real_t sticky_axis(real_t p_start, real_t p_size, real_t p_port_start, real_t p_port_size,
			real_t p_containing_start, real_t p_containing_end, real_t p_margin_start, real_t p_margin_end,
			const css_length& p_start_inset, const css_length& p_end_inset, bool p_reversed = false) {
			const bool has_start = !p_start_inset.is_predefined();
			const bool has_end = !p_end_inset.is_predefined();
			if (!has_start && !has_end) return 0;
			real_t start = p_port_start + (has_start ? p_start_inset.calc_percent(p_port_size) : 0);
			real_t end = p_port_start + p_port_size - (has_end ? p_end_inset.calc_percent(p_port_size) : 0);
			if (p_reversed) start = std::min(start, end - p_size);
			else end = std::max(end, start + p_size);

			const real_t box_end = p_start + p_size;
			const real_t available_start = p_start - p_containing_start;
			const real_t available_end = p_containing_end - box_end;
			if (has_start && p_start < start) {
				return std::clamp(start - p_start, real_t(0), std::max(real_t(0), available_end - p_margin_end));
			}
			if (has_end && box_end > end) {
				return -std::clamp(box_end - end, real_t(0), std::max(real_t(0), available_start - p_margin_start));
			}
			return 0;
		}

		Vector2 sticky_translation(const render_item& p_item, const Rect2& p_box, const Rect2& p_scrollport,
			const Rect2& p_containing_box, bool p_rtl) {
			const auto& offsets = p_item.css().get_offsets();
			const Vector2 limit = p_containing_box.get_end();
			return {
				sticky_axis(p_box.position.x, p_box.size.x, p_scrollport.position.x, p_scrollport.size.x,
					p_containing_box.position.x, limit.x, p_item.margin_left(), p_item.margin_right(), offsets.left, offsets.right, p_rtl),
				sticky_axis(p_box.position.y, p_box.size.y, p_scrollport.position.y, p_scrollport.size.y,
					p_containing_box.position.y, limit.y, p_item.margin_top(), p_item.margin_bottom(), offsets.top, offsets.bottom)
			};
		}

		Vector2 transform_pivot(const css_properties& p_css, const Rect2& p_box) {
			const auto& origin = p_css.get_transform_origin();
			return p_box.position + Vector2(origin.x.calc_percent(p_box.size.x), origin.y.calc_percent(p_box.size.y));
		}

		real_t planar_normalization(const Projection& p_matrix) {
			if (p_matrix[0].w != 0 || p_matrix[1].w != 0 || p_matrix[3].w < 0) return 1;
			// Map a plane coincident with the viewer to a finite, very large scale.
			return p_matrix[3].w == 0 ? real_t(1.0 / 65536.0) : p_matrix[3].w;
		}

		HTMLPlaneTransform project_to_plane(const Projection& p_matrix) {
			Vector3 x(p_matrix[0].x, p_matrix[1].x, p_matrix[3].x);
			Vector3 y(p_matrix[0].y, p_matrix[1].y, p_matrix[3].y);
			Vector3 w(p_matrix[0].w, p_matrix[1].w, p_matrix[3].w);
			if (w.x == 0 && w.y == 0 && w.z >= 0) {
				const real_t divisor = planar_normalization(p_matrix);
				x /= divisor;
				y /= divisor;
				w.z = 1;
			}
			return HTMLPlaneTransform(Basis(x.x, x.y, x.z, y.x, y.y, y.z, w.x, w.y, w.z));
		}

		Vector3 projected_depth(const Projection& p_matrix, const HTMLPlaneTransform& p_from_document) {
			// A projected plane has a depth of the form a*x + b*y + c after division by W.
			const Vector3 z(p_matrix[0].z, p_matrix[1].z, p_matrix[3].z);
			return (p_from_document.row(0) * z.x + p_from_document.row(1) * z.y +
				p_from_document.row(2) * z.z) / planar_normalization(p_matrix);
		}

		Projection spatial_translate(Vector3 p_offset) {
			Projection result;
			result[3] = Vector4(p_offset.x, p_offset.y, p_offset.z, 1);
			return result;
		}

		Projection perspective_matrix(const css_length& p_distance) {
			Projection result;
			if (!p_distance.is_predefined())
				result[2].w = -1 / std::max<real_t>(1, p_distance.calc_percent(0));
			return result;
		}

		Basis rotation_basis(const css_transform_function& p_function) {
			const auto& [x, y, z] = p_function.axis;
			// Compute in double precision so an edge-on quarter turn projects to a line.
			const double cosine = std::cos(p_function.first);
			const double sine = std::sin(p_function.first);
			const double c = std::abs(cosine) < 1e-15 ? 0.0 : cosine;
			const double s = std::abs(sine) < 1e-15 ? 0.0 : sine;
			const double t = 1 - c;
			return Basis(
				c + x * x * t, x * y * t - z * s, x * z * t + y * s,
				y * x * t + z * s, c + y * y * t, y * z * t - x * s,
				z * x * t - y * s, z * y * t + x * s, c + z * z * t);
		}

		Projection transform_list_matrix(const css_transform_list& p_list, const Vector2& p_size);

		Projection transform_function_matrix(const css_transform_function& p_function, const Vector2& p_size) {
			using Kind = css_transform_function::kind;
			switch (p_function.type) {
				case Kind::translate:
					return spatial_translate(Vector3(p_function.x.calc_percent(p_size.x), p_function.y.calc_percent(p_size.y),
						p_function.z.calc_percent(0)));
				case Kind::rotate:
					return Projection(Transform3D(rotation_basis(p_function), Vector3()));
				case Kind::scale:
					return Projection(Transform3D(Basis(p_function.first, 0, 0, 0, p_function.second, 0, 0, 0, p_function.third), Vector3()));
				case Kind::skew:
					return Projection(Transform3D(Basis(1, std::tan(p_function.first), 0, std::tan(p_function.second), 1, 0, 0, 0, 1), Vector3()));
				case Kind::matrix: {
					const auto& m = p_function.matrix;
					return Projection(Transform3D(Basis(m[0], m[2], 0, m[1], m[3], 0, 0, 0, 1), Vector3(m[4], m[5], 0)));
				}
				case Kind::matrix3d: {
					const auto& m = p_function.matrix;
					return { Vector4(m[0], m[1], m[2], m[3]), Vector4(m[4], m[5], m[6], m[7]),
						Vector4(m[8], m[9], m[10], m[11]), Vector4(m[12], m[13], m[14], m[15]) };
				}
				case Kind::perspective:
					return perspective_matrix(p_function.z);
				case Kind::blend: {
					const auto& blend = *p_function.blend;
					const auto from = transform_list_matrix(blend.from, p_size);
					const auto to = transform_list_matrix(blend.to, p_size);
					if (const auto sample = interpolate_transform_matrices(from, to, blend.progress)) return *sample;
					return blend.allow_discrete && blend.progress < 0.5 ? from : to;
				}
			}
			return {};
		}

		Projection transform_list_matrix(const css_transform_list& p_list, const Vector2& p_size) {
			Projection result;
			for (const auto& function : p_list) result = result * transform_function_matrix(function, p_size);
			return result;
		}

		using Polygon = std::vector<Vector2>;

		RoundedRect resolve_basic_clip(const css_clip_path& p_path, const Rect2& box, const Transform2D& p_matrix,
			std::shared_ptr<const HTMLClipPolygon>& r_polygon);

		std::optional<RoundedRect> resolve_clip_path(const css_clip_path& p_path, render_item& p_item,
			const std::vector<RoundedRect>& p_fragments, const HTMLPlaneTransform& p_matrix,
			const Vector2& p_viewport, std::shared_ptr<const HTMLClipPolygon>& r_polygon) {
			position content_box = p_item.get_placement();
			position padding_box = content_box;
			padding_box += p_item.get_paddings();
			position border_box = padding_box;
			border_box += p_item.get_borders();
			if (p_item.css().get_display() == display_inline && !p_fragments.empty()) {
				Rect2 fragment_box = p_fragments.front().rect;
				for (size_t i = 1; i < p_fragments.size(); ++i) fragment_box = fragment_box.merge(p_fragments[i].rect);
				border_box = position(fragment_box.position.x, fragment_box.position.y, fragment_box.size.x, fragment_box.size.y);
				padding_box = border_box;
				padding_box -= p_item.get_borders();
				content_box = padding_box;
				content_box -= p_item.get_paddings();
			}
			position margin_box = border_box;
			margin_box += p_item.get_margins();
			Rect2 box;
			switch (p_path.box) {
				case css_shape_box::margin: box = to_rect(margin_box); break;
				case css_shape_box::border: box = to_rect(border_box); break;
				case css_shape_box::padding: box = to_rect(padding_box); break;
				case css_shape_box::content: box = to_rect(content_box); break;
			}
			box.size = box.size.max(Vector2());
			if (!box.is_finite()) return RoundedRect(Rect2(), {});
			const auto sample = p_matrix.affine_at(box.get_center());
			if (p_path.shape == css_clip_shape::reference) {
				auto polygon = resolve_svg_clip(*p_item.src_el(), p_path.url, p_path.base_url, box, p_viewport, sample);
				if (!polygon) return std::nullopt;
				const Rect2 bounds = polygon->bounds();
				if (!bounds.is_finite() || !bounds.has_area()) return RoundedRect(Rect2(), {});
				r_polygon = std::make_shared<const HTMLClipPolygon>(std::move(*polygon));
				return RoundedRect(bounds, {});
			}
			if (p_path.shape == css_clip_shape::box) {
				const RoundedRect rounded(to_rect(border_box), p_item.css().get_borders().radius.calc_percents(border_box.width, border_box.height));
				return p_path.box == css_shape_box::margin ? rounded.outset(box) : rounded.inset(box);
			}
			return resolve_basic_clip(p_path, box, sample, r_polygon);
		}

		RoundedRect resolve_basic_clip(const css_clip_path& p_path, const Rect2& box, const Transform2D& p_matrix,
			std::shared_ptr<const HTMLClipPolygon>& r_polygon) {
			if (p_path.shape == css_clip_shape::box) return RoundedRect(box, {});
			if (is_one_of(p_path.shape, css_clip_shape::polygon, css_clip_shape::path, css_clip_shape::shape)) {
				auto polygon = std::make_shared<HTMLClipPolygon>();
				auto& region = polygon->outlines.emplace_back();
				region.even_odd = p_path.even_odd;
				const auto resolved_shape = p_path.shape_path ?
					css::resolve_shape_path(*p_path.shape_path, box.size.x, box.size.y) : std::nullopt;
				if (p_path.shape_path && !resolved_shape) return RoundedRect(Rect2(), {});
				const auto* outline = resolved_shape ? &*resolved_shape : p_path.svg_path.get();
				const double scale = std::max(1.0, std::hypot(double(p_matrix[0].length()), double(p_matrix[1].length())));
				const double tolerance = 0.2 / scale;
				if (outline) {
					for (const auto& source : css::flatten_svg_path(*outline, tolerance)) {
						if (source.size() < 3) continue;
						auto& contour = region.contours.emplace_back();
						contour.reserve(source.size());
						for (const auto& vertex : source) {
							const Vector2 point = box.position + Vector2(vertex.x, vertex.y);
							if (!point.is_finite()) return RoundedRect(Rect2(), {});
							contour.push_back(point);
						}
					}
				} else {
					auto& contour = region.contours.emplace_back();
					contour.reserve(p_path.vertices.size());
					for (const auto& vertex : p_path.vertices) {
						const Vector2 point = box.position +
							Vector2(vertex[0].calc_percent(box.size.x), vertex[1].calc_percent(box.size.y));
						if (!point.is_finite()) return RoundedRect(Rect2(), {});
						contour.push_back(point);
					}
					round_clip_polygon_vertices(contour, p_path.polygon_radius.calc_percent(0), tolerance);
				}
				const Rect2 bounds = polygon->bounds();
				if (!bounds.is_finite() || !bounds.has_area()) return RoundedRect(Rect2(), {});
				r_polygon = std::move(polygon);
				return RoundedRect(bounds, {});
			}

			const auto& dimensions = p_path.dimensions;
			if (is_one_of(p_path.shape, css_clip_shape::circle, css_clip_shape::ellipse)) {
				const Vector2 center(p_path.center.x.resolve(box.size.x), p_path.center.y.resolve(box.size.y));
				const Vector2 near = center.abs().min((box.size - center).abs());
				const Vector2 far = center.abs().max((box.size - center).abs());
				const auto radius = [&](int axis) -> real_t {
					const auto& value = dimensions[axis];
					if (p_path.shape == css_clip_shape::circle) {
						if (value.is_predefined())
							return value.predef() == 0 ? std::min(near.x, near.y) : std::max(far.x, far.y);
						return value.calc_percent(std::hypot(box.size.x, box.size.y) / std::sqrt(2.0));
					}
					if (value.is_predefined()) return value.predef() == 0 ? near[axis] : far[axis];
					return value.calc_percent(box.size[axis]);
				};
				const Vector2 axes = Vector2(radius(0), radius(p_path.shape == css_clip_shape::circle ? 0 : 1)).max(Vector2());
				const Rect2 ellipse(box.position + center - axes, axes * 2);
				if (!ellipse.is_finite()) return RoundedRect(Rect2(), {});
				border_radiuses rounded;
				rounded.top_left_x = rounded.top_right_x = rounded.bottom_right_x = rounded.bottom_left_x = axes.x;
				rounded.top_left_y = rounded.top_right_y = rounded.bottom_right_y = rounded.bottom_left_y = axes.y;
				return RoundedRect(ellipse, rounded);
			}

			Rect2 rectangle;
			if (p_path.shape == css_clip_shape::xywh) {
				rectangle = Rect2(box.position + Vector2(dimensions[0].calc_percent(box.size.x), dimensions[1].calc_percent(box.size.y)),
					Vector2(dimensions[2].calc_percent(box.size.x), dimensions[3].calc_percent(box.size.y)).max(Vector2()));
			} else {
				Vector2 start(dimensions[3].calc_percent(box.size.x), dimensions[0].calc_percent(box.size.y));
				Vector2 end(dimensions[1].calc_percent(box.size.x), dimensions[2].calc_percent(box.size.y));
				if (p_path.shape == css_clip_shape::inset) {
					for (int axis = 0; axis < 2; ++axis) {
						const auto sum = start[axis] + end[axis];
						if (sum > box.size[axis]) {
							start[axis] *= box.size[axis] / sum;
							end[axis] *= box.size[axis] / sum;
						}
					}
					end = box.size - end;
				} else {
					if (dimensions[1].is_predefined()) end.x = box.size.x;
					if (dimensions[2].is_predefined()) end.y = box.size.y;
				}
				rectangle = Rect2(box.position + start, (end - start).max(Vector2()));
			}
			if (!rectangle.is_finite()) return RoundedRect(Rect2(), {});
			return RoundedRect(rectangle, p_path.radii.calc_percents(rectangle.size.x, rectangle.size.y));
		}

		Polygon rect_polygon(const Rect2& p_box, const HTMLPlaneTransform& p_matrix) {
			const Vector2 end = p_box.get_end();
			return p_matrix.project_polygon(std::array{ p_box.position, Vector2(end.x, p_box.position.y),
				end, Vector2(p_box.position.x, end.y) });
		}

		Polygon clip_polygon(const TransformedClip& p_clip, bool p_rounded) {
			if (!p_rounded || !p_clip.shape.has_radius()) return rect_polygon(p_clip.shape.rect, p_clip.matrix);
			const auto& box = p_clip.shape.rect;
			const Vector2 end = box.get_end();
			const Vector2 corners[] = { box.position, Vector2(end.x, box.position.y), end, Vector2(box.position.x, end.y) };
			const Vector2 directions[] = { Vector2(1, 1), Vector2(-1, 1), Vector2(-1, -1), Vector2(1, -1) };
			constexpr real_t quarter_turn = 1.5707963267948966;
			constexpr int segments = 16;
			Polygon result;
			result.reserve(4 * (segments + 1));
			for (int corner = 0; corner < 4; ++corner) {
				const Vector2 radii(p_clip.shape.radii_x[corner], p_clip.shape.radii_y[corner]);
				if (radii.x <= 0 || radii.y <= 0) {
					result.push_back(corners[corner]);
					continue;
				}
				const Vector2 center = corners[corner] + radii * directions[corner];
				for (int step = 0; step <= segments; ++step) {
					const real_t angle = quarter_turn * (corner + 2 + static_cast<real_t>(step) / segments);
					result.push_back(center + radii * Vector2(std::cos(angle), std::sin(angle)));
				}
			}
			return p_clip.matrix.project_polygon(result);
		}

		RoundedRect motion_reference_box(const render_item& p_item, css_shape_box p_kind, const Rect2& p_viewport) {
			const auto positioning = p_item.css().get_position();
			auto parent = p_item.parent();
			for (; parent; parent = parent->parent()) {
				if (positioning == element_position_absolute) {
					if (parent->establishes_positioned_containing_block()) break;
				} else if (positioning == element_position_fixed) {
					if (parent->establishes_effect_containing_block()) break;
				} else if (parent->css().get_display() != display_inline) {
					break;
				}
			}
			if (!parent) return RoundedRect(Rect2(Vector2(), p_viewport.size), {});
			position content = parent->get_placement();
			content.x -= parent->get_scroll_left();
			content.y -= parent->get_scroll_top();
			position padding = content;
			padding += parent->get_paddings();
			position border = padding;
			border += parent->get_borders();
			const RoundedRect outer(to_rect(border), parent->css().get_borders().radius.calc_percents(border.width, border.height));
			if (p_kind == css_shape_box::content) return outer.inset(to_rect(content));
			if (p_kind == css_shape_box::padding) return outer.inset(to_rect(padding));
			return outer;
		}

		Vector2 box_position(const css_image_position& p_position, const Rect2& p_box) {
			return p_box.position + Vector2(p_position.x.resolve(p_box.size.x), p_position.y.resolve(p_box.size.y));
		}

		double ray_length(const css_offset_ray& p_ray, const Vector2& p_point, const Rect2& p_box, const Vector2& p_direction) {
			using Size = css_offset_ray::size;
			const Vector2 to_start = p_point - p_box.position;
			const Vector2 to_end = p_box.get_end() - p_point;
			if (p_ray.extent == Size::sides) {
				if (to_start.x <= 0 || to_start.y <= 0 || to_end.x <= 0 || to_end.y <= 0) return 0;
				const auto crossing = [](double start, double end, double direction) {
					if (std::abs(direction) < 1e-12) return std::numeric_limits<double>::infinity();
					return direction > 0 ? end / direction : -start / direction;
				};
				return std::min(crossing(to_start.x, to_end.x, p_direction.x), crossing(to_start.y, to_end.y, p_direction.y));
			}
			const auto near = to_start.abs().min(to_end.abs());
			const auto far = to_start.abs().max(to_end.abs());
			if (p_ray.extent == Size::closest_corner) return near.length();
			if (p_ray.extent == Size::farthest_corner) return far.length();
			if (p_ray.extent == Size::farthest_side) return std::max(far.x, far.y);
			return std::min(near.x, near.y);
		}

		css::SVGPath closed_motion_outline(const css_clip_path& p_shape, const RoundedRect& p_reference,
			const std::optional<Vector2>& p_start, const Transform2D& p_matrix, std::optional<Rect2>& r_ellipse) {
			auto shape = p_shape;
			const bool ellipse = is_one_of(shape.shape, css_clip_shape::circle, css_clip_shape::ellipse);
			if (p_start && !shape.center_explicit && ellipse) {
				const Vector2 center = *p_start - p_reference.rect.position;
				shape.center.x.offset = css_length(center.x, css_units_px);
				shape.center.y.offset = css_length(center.y, css_units_px);
			}
			std::shared_ptr<const HTMLClipPolygon> polygon;
			const RoundedRect bounds = shape.shape == css_clip_shape::box ? p_reference :
				resolve_basic_clip(shape, p_reference.rect, p_matrix, polygon);
			if (ellipse && bounds.rect.has_area()) {
				r_ellipse = bounds.rect;
				const Vector2 center = bounds.rect.get_center();
				const Vector2 axes = bounds.rect.size * 0.5;
				css::SVGPathBuilder path;
				path.move_to({center.x + axes.x, center.y});
				for (const auto point : { Vector2(center.x, center.y + axes.y), Vector2(center.x - axes.x, center.y),
					Vector2(center.x, center.y - axes.y), Vector2(center.x + axes.x, center.y) }) {
					css::SVGPathSegment arc{ css::SVGPathSegment::Kind::arc, { point.x, point.y } };
					arc.radii = { axes.x, axes.y };
					arc.sweep = true;
					path.append(arc);
				}
				path.close();
				return path.take_path();
			}
			Polygon points;
			if (polygon && !polygon->outlines.empty() && !polygon->outlines.front().contours.empty()) {
				points = polygon->outlines.front().contours.front();
			} else {
				points = clip_polygon({ bounds, Transform2D() }, true);
				if (bounds.has_radius()) {
					const Vector2 start = bounds.rect.position + Vector2(bounds.radii_x.x, 0);
					const auto first = std::min_element(points.begin(), points.end(), [&](const Vector2& a, const Vector2& b) {
						return a.distance_squared_to(start) < b.distance_squared_to(start);
					});
					std::rotate(points.begin(), first, points.end());
				}
			}
			css::SVGPathBuilder path;
			for (const auto& point : points) {
				if (path.empty()) path.move_to({ point.x, point.y });
				else path.append({ css::SVGPathSegment::Kind::line, { point.x, point.y } });
			}
			path.close();
			return path.take_path();
		}

		svg::PathMeasure::Sample motion_sample(const render_item& p_item, const Rect2& p_box, const Rect2& p_viewport,
			const Transform2D& p_matrix) {
			const auto& motion = p_item.css().get_motion_path();
			const auto reference = motion_reference_box(p_item, motion.path.box, p_viewport);
			std::optional<Vector2> start;
			using Position = css_offset_position::mode;
			if (motion.position.kind == Position::automatic) start = p_box.position;
			if (motion.position.kind == Position::explicit_position) start = box_position(motion.position.position, reference.rect);
			if (const auto& ray = motion.path.ray) {
				const Vector2 origin = ray->origin ? box_position(*ray->origin, reference.rect) : start.value_or(reference.rect.get_center());
				const Vector2 direction(std::sin(ray->angle), -std::cos(ray->angle));
				double length = ray_length(*ray, origin, reference.rect, direction);
				if (ray->contain) length = std::max(0.0, length - std::max(p_box.size.x, p_box.size.y) * 0.5);
				double distance = motion.distance.calc_percent(length);
				if (ray->contain) distance = std::clamp(distance, 0.0, length);
				return { origin + direction * distance, direction };
			}
			const Vector2 default_origin = p_box.position;
			const auto& shape = motion.path.shape;
			if (!shape) return { default_origin, Vector2(1, 0) };

			css::SVGPath path;
			Transform2D placement;
			std::optional<Rect2> ellipse;
			if (shape->svg_path) {
				path = *shape->svg_path;
				placement = Transform2D(0, default_origin);
			} else if (shape->shape_path) {
				path = css::resolve_shape_path(*shape->shape_path, reference.rect.size.x, reference.rect.size.y).value_or(css::SVGPath());
				placement = Transform2D(0, reference.rect.position);
			} else if (shape->shape == css_clip_shape::reference) {
				const auto fragment = shape->url.find('#');
				const auto document = svg::reference_document(*p_item.src_el(), shape->url, shape->base_url);
				const auto root = document ? document->root() : nullptr;
				const auto* target = root && fragment != std::string::npos ?
					svg::resolve_fragment(*root, std::string_view(shape->url).substr(fragment)) : nullptr;
				if (target && svg::is_svg(*target)) {
					path = svg::shape_path(*target, reference.rect.size, target->get_tagName());
					placement = Transform2D(0, reference.rect.position) * svg::transform_attribute(*target);
				}
			} else {
				path = closed_motion_outline(*shape, reference, start, p_matrix, ellipse);
			}
			if (path.empty()) return { default_origin, Vector2(1, 0) };
			if (ellipse && ellipse->size.x == ellipse->size.y) {
				const double radius = ellipse->size.x * 0.5;
				const double turn = 2 * std::acos(-1.0);
				const double perimeter = turn * radius;
				const double angle = std::fmod(motion.distance.calc_percent(perimeter), perimeter) / radius;
				const Vector2 radial(std::cos(angle), std::sin(angle));
				return { ellipse->get_center() + radial * radius, Vector2(-radial.y, radial.x) };
			}
			const double scale = std::max(1.0, std::hypot(double(p_matrix[0].length()), double(p_matrix[1].length())));
			const svg::PathMeasure measure(path, placement, 0.05 / scale, false);
			if (measure.length() <= 0) {
				const auto& start_point = path.front().start;
				return { placement.xform(Vector2(start_point.x, start_point.y)), Vector2(1, 0) };
			}
			double distance = motion.distance.calc_percent(measure.length());
			if (path.back().closed && !measure.is_closed()) {
				distance = std::fmod(distance, measure.length());
				if (distance < 0) distance += measure.length();
			}
			auto sample = measure.sample(distance);
			if (ellipse) {
				const Vector2 delta = sample.point - ellipse->get_center();
				const Vector2 axes = ellipse->size * 0.5;
				sample.tangent = Vector2(-delta.y / (axes.y * axes.y), delta.x / (axes.x * axes.x)).normalized();
			}
			return sample;
		}

		Transform2D motion_matrix(const render_item& p_item, const Rect2& p_box, const Rect2& p_transform_box,
			const Rect2& p_viewport, const Transform2D& p_matrix) {
			const auto& style = p_item.css();
			const auto& motion = style.get_motion_path();
			const Vector2 anchor = motion.anchor.kind == css_offset_position::mode::explicit_position ?
				box_position(motion.anchor.position, p_transform_box) : transform_pivot(style, p_transform_box);
			const auto sample = motion_sample(p_item, p_box, p_viewport, p_matrix);
			const double angle = motion.rotation.angle + (motion.rotation.follows_path ? sample.tangent.angle() : 0);
			return Transform2D(angle, sample.point) * Transform2D(0, -anchor);
		}

		Rect2 transform_reference_box(const render_item& p_item, const Rect2& p_border_box) {
			const auto box = p_item.css().get_transform_box();
			return is_one_of(box, css_transform_box::content, css_transform_box::fill) ?
				to_rect(p_item.get_placement()) : p_border_box;
		}

		Projection element_matrix(const render_item& p_item, const Rect2& p_box, const Rect2& p_viewport,
			const HTMLPlaneTransform& p_parent_matrix) {
			const auto& css = p_item.css();
			const Rect2 reference = transform_reference_box(p_item, p_box);
			const Vector2 center = transform_pivot(css, reference);
			const Vector3 pivot(center.x, center.y, css.get_transform_origin().z.calc_percent(0));
			Projection matrix = spatial_translate(pivot);
			const auto undo_pivot = spatial_translate(-pivot);
			const auto& individual = css.get_individual_transforms();
			for (const auto* function : { &individual.translate, &individual.rotate, &individual.scale }) {
				if (*function) matrix = matrix * transform_function_matrix(**function, reference.size);
			}
			if (css.get_motion_path().enabled()) {
				// Motion maps document-space points, while the transform list operates about the pivot.
				matrix = matrix * undo_pivot;
				const auto visual = p_parent_matrix * project_to_plane(matrix);
				const auto motion = motion_matrix(p_item, p_box, reference, p_viewport, visual.affine_at(center));
				const Basis basis(motion[0].x, motion[1].x, 0, motion[0].y, motion[1].y, 0, 0, 0, 1);
				matrix = matrix * Projection(Transform3D(basis, Vector3(motion[2].x, motion[2].y, 0))) * spatial_translate(pivot);
			}
			matrix = matrix * transform_list_matrix(css.get_transforms(), reference.size);
			return matrix * undo_pivot;
		}

		template<class Side>
		void intersect_half_plane(Polygon& r_polygon, Side p_side, Polygon& r_input) {
			if (r_polygon.empty()) return;
			r_polygon.swap(r_input);
			r_polygon.clear();
			Vector2 previous = r_input.back();
			real_t previous_side = p_side(previous);
			for (const Vector2& point : r_input) {
				const real_t side = p_side(point);
				if ((previous_side >= 0) != (side >= 0)) {
					r_polygon.push_back(previous.lerp(point, previous_side / (previous_side - side)));
				}
				if (side >= 0) r_polygon.push_back(point);
				previous = point;
				previous_side = side;
			}
		}

		void intersect_polygon(Polygon& r_polygon, const Polygon& p_clip, real_t p_winding) {
			if (p_clip.size() < 3) {
				r_polygon.clear();
				return;
			}
			Polygon input;
			for (std::size_t edge = 0; edge < p_clip.size() && !r_polygon.empty(); ++edge) {
				const Vector2 a = p_clip[edge];
				const Vector2 direction = p_winding * (p_clip[(edge + 1) % p_clip.size()] - a);
				intersect_half_plane(r_polygon, [&](const Vector2& p_point) { return direction.cross(p_point - a); }, input);
			}
		}

		void intersect_strip(Polygon& r_polygon, const TransformedClip& p_clip) {
			const auto& rect = p_clip.shape.rect;
			const auto inverse = p_clip.matrix.inverse();
			const int axis = p_clip.axes == ClipAxes::x ? 0 : 1;
			const auto& w = inverse.row(2);
			const auto& component = inverse.row(axis);
			Polygon input;
			for (const auto line : { w, component - w * rect.position[axis], w * rect.get_end()[axis] - component }) {
				intersect_half_plane(r_polygon, [&](const Vector2& p_point) { return line.dot(Vector3(p_point.x, p_point.y, 1)); }, input);
			}
		}

		void intersect_horizon(Polygon& r_polygon, const HTMLPlaneTransform& p_matrix) {
			const auto inverse = p_matrix.inverse();
			const auto& line = inverse.row(2);
			Polygon input;
			intersect_half_plane(r_polygon, [&](const Vector2& p_point) { return line.dot(Vector3(p_point.x, p_point.y, 1)); }, input);
		}

		real_t polygon_area(const Polygon& p_polygon) {
			const Vector2 origin = p_polygon.front();
			real_t twice_area = 0;
			for (std::size_t index = 1; index + 1 < p_polygon.size(); ++index)
				twice_area += (p_polygon[index] - origin).cross(p_polygon[index + 1] - origin);
			return twice_area;
		}
	}

	HTMLClipPolygon svg_basic_clip_polygon(const css_clip_path& p_path, const Rect2& p_box,
		const Transform2D& p_matrix, const Transform2D& p_visual_matrix) {
		std::shared_ptr<const HTMLClipPolygon> polygon;
		const auto shape = resolve_basic_clip(p_path, p_box, p_visual_matrix * p_matrix, polygon);
		if (polygon) return polygon->transformed(p_matrix);
		HTMLClipPolygon result;
		if (shape.rect.has_area()) {
			result.outlines.emplace_back().contours.push_back(clip_polygon({ shape, p_matrix }, true));
		}
		return result;
	}

	bool TransformedClip::contains_point(const Vector2& p_point, bool p_precise) const {
		if (axes == ClipAxes::horizon) {
			const auto inverse = matrix.inverse();
			return inverse.row(2).dot(Vector3(p_point.x, p_point.y, 1)) > 0;
		}
		if (polygon && p_precise) {
			const auto local = matrix.inverse().xform(p_point);
			return local.is_finite() && shape.rect.has_point(local) && polygon->contains_point(local);
		}
		if (axes != ClipAxes::both) {
			if (!p_precise) return true;
			const Vector2 local = matrix.inverse().xform(p_point);
			const int axis = axes == ClipAxes::x ? 0 : 1;
			return local[axis] >= shape.rect.position[axis] && local[axis] < shape.rect.get_end()[axis];
		}
		if (!p_precise) return matrix.xform(shape.rect).has_point(p_point);
		const Vector2 local = matrix.inverse().xform(p_point);
		return local.is_finite() && shape.has_point(local);
	}

	Rect2 clipped_visual_bounds(const Rect2& p_rect, const HTMLPlaneTransform& p_matrix,
		const std::vector<TransformedClip>& p_clips) {
		if (!p_rect.has_area()) return {};
		if (p_clips.empty()) return p_matrix.xform(p_rect);
		Polygon polygon = rect_polygon(p_rect, p_matrix);
		if (polygon.size() < 3) return {};
		std::vector<HTMLClipPolygon> polygon_clips;
		int remaining = MAX_ROUNDED_CLIPS;
		for (auto clip = p_clips.rbegin(); clip != p_clips.rend(); ++clip) {
			const bool use_precise = use_precise_clip(*clip, remaining);
			if (clip->polygon && use_precise) {
				polygon_clips.push_back(clip->polygon->transformed(clip->matrix));
			} else if (clip->axes == ClipAxes::horizon) {
				intersect_horizon(polygon, clip->matrix);
			} else if (clip->axes != ClipAxes::both) {
				const int axis = clip->axes == ClipAxes::x ? 0 : 1;
				if (clip->shape.rect.size[axis] <= 0) return {};
				if (use_precise) intersect_strip(polygon, *clip);
			}
			else {
				if (!clip->shape.rect.has_area()) return {};
				const auto shape = use_precise ? clip_polygon(*clip, true) :
					rect_polygon(clip->matrix.xform(clip->shape.rect), Transform2D());
				intersect_polygon(polygon, shape, use_precise && clip->matrix.determinant() < 0 ? -1 : 1);
			}
			if (polygon.size() < 3) return {};
		}
		if (polygon_area(polygon) == 0) return {};
		if (!polygon_clips.empty()) {
			polygon_clips.emplace_back().outlines.emplace_back().contours.push_back(std::move(polygon));
			return intersect_clip_polygons(polygon_clips);
		}
		Rect2 bounds(polygon.front(), Vector2());
		for (const Vector2& point : polygon) bounds = bounds.expand(point);
		return bounds;
	}

	Rect2 to_rect(const litehtml::position& p_position) {
		return { p_position.x, p_position.y, p_position.width, p_position.height };
	}

	RoundedRect::RoundedRect(const Rect2& p_rect, const border_radiuses& p_radii) :
		rect(p_rect), radii_x(p_radii.top_left_x, p_radii.top_right_x, p_radii.bottom_right_x, p_radii.bottom_left_x),
		radii_y(p_radii.top_left_y, p_radii.top_right_y, p_radii.bottom_right_y, p_radii.bottom_left_y) {
		for (int i = 0; i < 4; ++i) {
			if (radii_x[i] <= 0 || radii_y[i] <= 0 || !std::isfinite(radii_x[i]) || !std::isfinite(radii_y[i])) {
				radii_x[i] = radii_y[i] = 0;
			}
		}
		const auto ratio = [](real_t p_size, real_t p_sum) -> real_t { return p_sum > 0 ? p_size / p_sum : 1; };
		const real_t scale = std::clamp<real_t>(std::min({
			ratio(rect.size.x, radii_x.x + radii_x.y), ratio(rect.size.x, radii_x.z + radii_x.w),
			ratio(rect.size.y, radii_y.x + radii_y.w), ratio(rect.size.y, radii_y.y + radii_y.z) }), 0, 1);
		radii_x *= scale;
		radii_y *= scale;
	}

	RoundedRect RoundedRect::inset(const Rect2& p_rect) const {
		const Vector2 leading = p_rect.position - rect.position;
		const Vector2 trailing = rect.get_end() - p_rect.get_end();
		border_radiuses radii;
		radii.top_left_x = radii_x.x - leading.x;
		radii.top_right_x = radii_x.y - trailing.x;
		radii.bottom_right_x = radii_x.z - trailing.x;
		radii.bottom_left_x = radii_x.w - leading.x;
		radii.top_left_y = radii_y.x - leading.y;
		radii.top_right_y = radii_y.y - leading.y;
		radii.bottom_right_y = radii_y.z - trailing.y;
		radii.bottom_left_y = radii_y.w - trailing.y;
		return RoundedRect(p_rect, radii);
	}

	RoundedRect RoundedRect::outset(const Rect2& p_rect) const {
		const auto expand_radius = [](real_t p_radius, real_t p_distance) {
			if (p_distance > p_radius) {
				const real_t ratio = p_radius / p_distance - 1;
				return p_radius + p_distance * (1 + ratio * ratio * ratio);
			}
			return std::max<real_t>(0, p_radius + p_distance);
		};
		const Vector2 leading = rect.position - p_rect.position;
		const Vector2 trailing = p_rect.get_end() - rect.get_end();
		border_radiuses radii;
		radii.top_left_x = expand_radius(radii_x.x, leading.x);
		radii.top_right_x = expand_radius(radii_x.y, trailing.x);
		radii.bottom_right_x = expand_radius(radii_x.z, trailing.x);
		radii.bottom_left_x = expand_radius(radii_x.w, leading.x);
		radii.top_left_y = expand_radius(radii_y.x, leading.y);
		radii.top_right_y = expand_radius(radii_y.y, leading.y);
		radii.bottom_right_y = expand_radius(radii_y.z, trailing.y);
		radii.bottom_left_y = expand_radius(radii_y.w, trailing.y);
		return RoundedRect(p_rect, radii);
	}

	RoundedRect RoundedRect::spread(real_t p_distance) const {
		return outset(rect.grow(p_distance));
	}

	bool RoundedRect::has_radius() const {
		return radii_x != Vector4();
	}

	bool RoundedRect::has_point(const Vector2& p_position) const {
		if (!p_position.is_finite() || !rect.has_point(p_position)) {
			return false;
		}
		const Vector2 from_start = to_local_point(p_position, rect.position);
		const Vector2 from_end = rect.get_end() - p_position;
		for (int i = 0; i < 4; ++i) {
			const Vector2 radius(radii_x[i], radii_y[i]);
			const Vector2 edge(i == 0 || i == 3 ? from_start.x : from_end.x, i < 2 ? from_start.y : from_end.y);
			if (edge.x < radius.x && edge.y < radius.y) {
				return ((radius - edge) / radius).length_squared() <= 1.0;
			}
		}
		return true;
	}

	bool HTMLScrollbarColors::operator==(const HTMLScrollbarColors& p_other) const {
		return thumb == p_other.thumb && track == p_other.track;
	}

	bool HTMLScrollbarAppearance::operator==(const HTMLScrollbarAppearance& p_other) const {
		return width == p_other.width && colors == p_other.colors && scheme == p_other.scheme;
	}

	float HTMLScrollbarAppearance::thickness() const {
		if (width == scrollbar_width::none) return 0;
		return width == scrollbar_width::thin ? 9.0f : 15.0f;
	}

	float HTMLScrollbarAppearance::thumb_thickness(bool p_hot) const {
		if (width == scrollbar_width::none) return 0;
		if (width == scrollbar_width::thin) return p_hot ? 5.0f : 2.0f;
		return p_hot ? 9.0f : 3.0f;
	}

	Color HTMLScrollbarAppearance::thumb_color(bool p_pressed) const {
		if (colors) return colors->thumb;
		return html_system_color(p_pressed ? system_color::scrollbar_pressed : system_color::scrollbar_thumb, scheme);
	}

	Color HTMLScrollbarAppearance::track_color() const {
		return colors ? colors->track : html_system_color(system_color::scrollbar_track, scheme);
	}

	Color html_system_color(system_color p_color, color_scheme p_scheme) {
		const auto value = system_color_value(p_color, p_scheme);
		return Color::from_rgba8(value.red, value.green, value.blue, value.alpha);
	}

	Rect2 HTMLScrollbarAppearance::visible_thumb(const Rect2& p_thumb, int p_axis, bool p_hot) const {
		Rect2 visible = p_thumb;
		const int cross = 1 - p_axis;
		const real_t gutter = std::min<real_t>(2, visible.size[cross] / 4);
		visible.size[cross] = std::min<real_t>(thumb_thickness(p_hot), visible.size[cross] - gutter);
		visible.position[cross] = p_thumb.get_end()[cross] - gutter - visible.size[cross];
		return visible;
	}

	HTMLScrollbarAppearance html_scrollbar_appearance(const css::ComputedStyle& p_style) {
		HTMLScrollbarAppearance result{ p_style.get_scrollbar_width(), std::nullopt, p_style.get_color_scheme() };
		if (const auto& colors = p_style.get_scrollbar_colors()) {
			const auto to_color = [&](const web_color& p_color) {
				const auto srgb = p_color.to_srgb(p_style.get_color());
				return Color(srgb.red / 255.0f, srgb.green / 255.0f, srgb.blue / 255.0f, srgb.alpha / 255.0f);
			};
			result.colors = HTMLScrollbarColors{ to_color(colors->thumb), to_color(colors->track) };
		}
		return result;
	}

	HTMLScrollbar make_html_scrollbar(const Rect2& p_track, int p_axis, real_t p_page, real_t p_range, real_t p_offset) {
		HTMLScrollbar bar{p_track, p_track, p_range};
		if (p_track.has_area() && p_range > 0) {
			const real_t length = p_track.size[p_axis];
			bar.thumb.size[p_axis] = std::clamp(length * p_page / (p_page + p_range), std::min<real_t>(17, length * 0.5), length);
			bar.thumb.position[p_axis] += (length - bar.thumb.size[p_axis]) * std::clamp(p_offset / p_range, real_t(0), real_t(1));
		}
		return bar;
	}

	Vector2i html_resize_axes(const css::ComputedStyle& p_style) {
		if (!p_style.is_scroll_container()) return {};
		switch (p_style.get_resize()) {
			case resize_both: return {1, 1};
			case resize_horizontal:
			case resize_inline: return {1, 0};
			case resize_vertical:
			case resize_block: return {0, 1};
			default: return {};
		}
	}

	Rect2 html_resize_grip(const Rect2& p_box) {
		const real_t edge = std::min<real_t>({14, p_box.size.x, p_box.size.y});
		const Vector2 size(edge, edge);
		return p_box.has_area() ? Rect2(p_box.get_end() - size, size) : Rect2();
	}

	Rect2 RenderGeometry::resize_grip() const {
		if (!inverse || !child_overflow || !item->is_visible() || item->src_el()->is_replaced() ||
			item->src_el()->is_root() || is_generated_tag(item->src_el()->tag()) ||
			html_resize_axes(item->css()) == Vector2i()) return {};
		return html_resize_grip(child_overflow->shape.rect);
	}

	std::array<HTMLScrollbar, 2> RenderGeometry::scrollbars() const {
		std::array<HTMLScrollbar, 2> result{};
		if (!inverse || !child_overflow || !item->is_visible() || item->src_el()->is_replaced()) return result;
		const auto content = item->scrollable_content_size();
		if (!content) return result;

		const auto& css = item->css();
		const real_t thickness = HTMLScrollbarAppearance{ css.get_scrollbar_width(), std::nullopt }.thickness();
		if (thickness <= 0) return result;
		const Rect2 viewport = child_overflow->shape.rect;
		if (!viewport.has_area()) return result;
		const auto placement = item->get_placement();
		const Vector2 range = Vector2(content->width - placement.width, content->height - placement.height).max(Vector2());
		const auto shows_scrollbar = [](litehtml::overflow p_mode, real_t p_range) {
			return p_mode == overflow_scroll || (p_mode == overflow_auto && p_range > 0);
		};
		const bool horizontal = shows_scrollbar(css.get_overflow_x(), range.x);
		const bool vertical = shows_scrollbar(css.get_overflow_y(), range.y);
		const Vector2 size = viewport.size.min(Vector2(thickness, thickness));
		const Vector2 grip = resize_grip().size;
		const Vector2 end = viewport.get_end();
		if (horizontal) {
			result[0].track = Rect2(viewport.position.x, end.y - size.y,
				viewport.size.x - std::max(grip.x, vertical ? size.x : 0), size.y);
		}
		if (vertical) {
			result[1].track = Rect2(end.x - size.x, viewport.position.y, size.x,
				viewport.size.y - std::max(grip.y, horizontal ? size.y : 0));
		}
		const Vector2 offset(item->get_scroll_left(), item->get_scroll_top());
		for (int axis = 0; axis < 2; ++axis) {
			result[axis] = make_html_scrollbar(result[axis].track, axis, viewport.size[axis], range[axis], offset[axis]);
		}
		return result;
	}

	bool RenderGeometry::contains_visible_point(const Vector2& p_point) const {
		if (!inverse) return false;
		const Vector2 local = inverse->xform(p_point);
		if (!local.is_finite()) return false;
		const bool inside = std::any_of(fragments.begin(), fragments.end(), [&](const RoundedRect& p_fragment) {
			return p_fragment.has_point(local);
		});
		if (!inside) {
			return false;
		}
		int remaining = MAX_ROUNDED_CLIPS;
		for (auto* clip = overflow; clip; clip = clip->parent) {
			const bool precise = use_precise_clip(*clip, remaining);
			if (!clip->contains_point(p_point, precise)) {
				return false;
			}
		}
		return true;
	}

	bool RenderGeometry::has_visible_area(const Rect2& p_box) const {
		if (!inverse) return false;
		const auto transformed_clip = [&] {
			for (auto* clip = overflow; clip; clip = clip->parent)
				if (clip->polygon || clip->matrix != Transform2D() || clip->axes != ClipAxes::both) return true;
			return false;
		};
		if (matrix != Transform2D() || transformed_clip()) {
			std::vector<TransformedClip> shapes;
			for (auto* entry = overflow; entry; entry = entry->parent) shapes.push_back(*entry);
			std::reverse(shapes.begin(), shapes.end());
			return clipped_visual_bounds(p_box, matrix, shapes).has_area();
		}
		Rect2 visible = p_box;
		int remaining = MAX_ROUNDED_CLIPS;
		for (auto* entry = overflow; entry; entry = entry->parent) {
			const RoundedRect& clip = entry->shape;
			visible = visible.intersection(entry->matrix.xform(clip.rect));
			if (!visible.has_area()) {
				return false;
			}
			if (use_precise_clip(*entry, remaining)) {
				// A rectangle entirely inside a cut-off corner cannot be revealed by scrolling the label.
				for (int i = 0; i < 4; ++i) {
					const Vector2 inward(i == 0 || i == 3 ? visible.get_end().x : visible.position.x,
						i < 2 ? visible.get_end().y : visible.position.y);
					const Vector2 edge(i == 0 || i == 3 ? inward.x - clip.rect.position.x : clip.rect.get_end().x - inward.x,
						i < 2 ? inward.y - clip.rect.position.y : clip.rect.get_end().y - inward.y);
					const Vector2 radius(clip.radii_x[i], clip.radii_y[i]);
					if (edge.x < radius.x && edge.y < radius.y && ((radius - edge) / radius).length_squared() >= 1.0) {
						return false;
					}
				}
			}
		}
		return visible.has_area();
	}

	void HTMLGeometryIndex::rebuild(const std::shared_ptr<render_item>& p_root, const Rect2& p_viewport,
		const std::vector<std::shared_ptr<render_item>>& p_top_layer) {
		clear();
		viewport = p_viewport;
		if (p_root) {
			index(p_root, nullptr, Transform2D(), false, viewport, viewport, {}, nullptr);
		}
		top_layer = !p_top_layer.empty();
		indexing_top_layer = true;
		for (const auto& holder : p_top_layer) {
			for (const auto& child : holder->children()) {
				const Rect2 box(Vector2(), viewport.size);
				index(child, nullptr, Transform2D(0, viewport.position), true, box, box, {}, nullptr);
			}
		}
		indexing_top_layer = false;
	}

	namespace {
		Vector2 css_transform_reference_size(const RenderGeometry& entry) {
			Rect2 box = to_rect(entry.item->get_placement());
			if (!entry.fragments.empty()) {
				box = entry.fragments.front().rect;
				for (size_t i = 1; i < entry.fragments.size(); ++i) box = box.merge(entry.fragments[i].rect);
			}
			return transform_reference_box(*entry.item, box).size;
		}
	}

	std::optional<Projection> HTMLGeometryIndex::css_transform_matrix(const element& p_element,
		const css_transform_list& p_transforms) const {
		const auto* entries = find_element(&p_element);
		if (!entries || entries->empty()) return std::nullopt;
		return transform_list_matrix(p_transforms, css_transform_reference_size(*entries->front()));
	}

	bool HTMLGeometryIndex::can_interpolate_transform(const element& p_element, const css_transform_list& p_sample) const {
		const auto* entries = find_element(&p_element);
		if (!entries || entries->empty()) return true;
		const auto size = css_transform_reference_size(*entries->front());
		for (const auto& operation : p_sample) {
			if (!operation.blend) continue;
			const auto& blend = *operation.blend;
			if (!interpolate_transform_matrices(transform_list_matrix(blend.from, size),
				transform_list_matrix(blend.to, size), blend.progress)) return false;
		}
		return true;
	}

	void HTMLGeometryIndex::clear() {
		snap_areas.clear();
		auto_content_boxes.clear();
		snap_containers.clear();
		elements.clear();
		renderers.clear();
		clips.clear();
		transforms = false;
		sticky = false;
		group_effects = false;
		element_blending = false;
		backdrop_filtering = false;
		overflow_controls = false;
		top_layer = false;
	}

	const HTMLGeometryIndex::ElementGeometry* HTMLGeometryIndex::find_element(const element* p_element) const {
		return elements.getptr(p_element);
	}

	const RenderGeometry* HTMLGeometryIndex::find_renderer(const render_item* p_item) const {
		const auto found = renderers.find(p_item);
		return found != renderers.end() ? &found->second : nullptr;
	}

	struct HTMLGeometryIndex::SpatialParent {
		Projection accumulated;
		HTMLPlaneTransform outer;
	};

	void HTMLGeometryIndex::index(const std::shared_ptr<render_item>& p_item, const OverflowClip* p_overflow,
		const HTMLPlaneTransform& p_parent_matrix, bool p_viewport_fixed_subtree,
		const Rect2& p_scrollport, const Rect2& p_containing_box, const Vector2& p_sticky_offset,
		const render_item* p_layout_overflow_boundary, bool p_transform_culled, const Projection* p_parent_perspective,
		const SpatialParent* p_spatial_parent, std::optional<Vector3> p_flattened_depth) {
		const auto& css = p_item->css();
		const auto display = css.get_display();
		if (display == display_table_column || display == display_table_column_group || p_item->collapsed_table_track()) return;
		const bool fixed = p_item->is_viewport_fixed();
		const bool participates = p_spatial_parent && !fixed;
		const bool preserves = css.preserves_3d();
		if (fixed) p_flattened_depth.reset();
		const bool fixed_subtree = p_viewport_fixed_subtree || fixed;
		HTMLPlaneTransform matrix = fixed ? HTMLPlaneTransform(Transform2D(0, indexing_top_layer ? viewport.position : Vector2())) : p_parent_matrix;
		Rect2 scrollport = fixed ? Rect2(Vector2(), viewport.size) : p_scrollport;
		Vector2 sticky_offset = fixed ? Vector2() : p_sticky_offset;
		position::vector boxes;
		if (display != display_inline_text) {
			p_item->get_rendering_boxes(boxes);
		}
		const bool positioned_sticky = display != display_inline_text && css.get_position() == element_position_sticky;
		sticky |= positioned_sticky;
		group_effects |= css.has_group_effects() || preserves;
		element_blending |= css.mixes_with_backdrop();
		backdrop_filtering |= css.has_backdrop_filters();
		std::optional<Projection> child_perspective;
		std::optional<SpatialParent> spatial;
		bool local_horizon = false;
		bool plane_culled = false;
		if (positioned_sticky || css.has_transform() || css.has_perspective() || participates || preserves) {
			const Rect2 content_box = to_rect(p_item->get_placement());
			Rect2 reference = boxes.empty() ? content_box : to_rect(boxes.front());
			for (std::size_t index = 1; index < boxes.size(); ++index) reference = reference.merge(to_rect(boxes[index]));
			Vector2 shift;
			if (positioned_sticky && viewport.has_area() && p_item->parent()) {
				const auto parent = p_item->parent();
				Rect2 containing_box = p_containing_box;
				if (const auto& area = p_item->sticky_grid_area()) {
					const Vector2 origin = to_rect(parent->get_placement()).position + sticky_offset -
						Vector2(parent->get_scroll_left(), parent->get_scroll_top());
					containing_box = translate_rect(to_rect(*area), origin);
				}
				shift = sticky_translation(*p_item, translate_rect(reference, sticky_offset), scrollport,
					containing_box, parent->css().is_right_to_left());
				sticky_offset += shift;
				if (!participates) matrix = matrix * Transform2D(0, shift);
			}
			if (css.has_transform() || participates || preserves) {
				transforms |= css.has_transform();
				Projection local;
				if (css.has_transform()) local = element_matrix(*p_item, reference, viewport, matrix);
				if (p_parent_perspective && !fixed) local = *p_parent_perspective * local;
				if (participates) {
					if (shift != Vector2()) local = spatial_translate(Vector3(shift.x, shift.y, 0)) * local;
					spatial = SpatialParent{p_spatial_parent->accumulated * local, p_spatial_parent->outer};
				} else {
					spatial = SpatialParent{local, matrix};
				}
				const HTMLPlaneTransform projected = project_to_plane(spatial->accumulated);
				const real_t determinant = spatial->accumulated.determinant();
				// The Z component of the transformed front normal is cofactor(3,3) / determinant.
				const real_t cofactor = projected.determinant();
				const bool back_facing = (cofactor < 0 && determinant > 0) || (cofactor > 0 && determinant < 0);
				const auto& divisor = projected.row(2);
				local_horizon = divisor.x != 0 || divisor.y != 0;
				const bool behind_viewer = !local_horizon && divisor.z < 0;
				const bool hidden_backface = css.hides_backface() && back_facing;
				plane_culled = behind_viewer || hidden_backface;
				p_transform_culled |= !std::isfinite(determinant) || determinant == 0 || hidden_backface ||
					(!preserves && behind_viewer);
				matrix = spatial->outer * projected;
			}
			if (css.has_perspective()) {
				const Vector2 eye = box_position(css.get_perspective_origin(), transform_reference_box(*p_item, reference));
				const Vector3 origin(eye.x, eye.y, 0);
				child_perspective = spatial_translate(origin) * perspective_matrix(css.get_perspective()) * spatial_translate(-origin);
			}
		}
		const auto inverse = matrix.try_inverse();
		auto& entry = renderers.emplace(p_item.get(),
			RenderGeometry{ p_item, {}, {}, std::nullopt, matrix,
				p_transform_culled || plane_culled ? std::nullopt : inverse, p_overflow, fixed_subtree }).first->second;
		entry.layout_overflow_boundary = p_layout_overflow_boundary;
		entry.starts_3d_context = preserves && !participates;
		entry.flattens_3d_context = participates && !preserves && !p_item->children().empty() && p_item->establishes_stacking_context();
		if (entry.starts_3d_context) entry.enclosing_depth_plane = p_flattened_depth;
		entry.depth_plane = p_flattened_depth;
		if (inverse && (participates || preserves)) {
			const auto depth = projected_depth(spatial->accumulated, *inverse);
			if (depth.is_finite()) entry.depth_plane = depth;
		}
		// Preserve each flattened ancestor's visible side when multiple projective maps compose.
		if (inverse && local_horizon && !preserves && !matrix.is_affine()) {
			clips.push_back({ { RoundedRect(Rect2(), {}), matrix, ClipAxes::horizon }, *inverse, p_item.get(), p_overflow });
			p_overflow = &clips.back();
			entry.overflow = p_overflow;
		}
		if (display != display_inline_text) {
			elements[p_item->src_el().get()].push_back(&entry);
			if (css.has_auto_content_visibility()) auto_content_boxes.push_back(&entry);
			if (!css.get_scroll_snap_align().empty()) snap_areas.push_back(&entry);
			if (css.get_scroll_snap_type().enabled() && p_item->scrollable_content_size() && !p_item->src_el()->is_replaced())
				snap_containers.push_back(p_item);
			entry.fragments.reserve(boxes.size());
			for (std::size_t i = 0; i < boxes.size(); ++i) {
				const Rect2 box = to_rect(boxes[i]);
				if (!box.is_finite() || box.size.x < 0 || box.size.y < 0) {
					continue;
				}
				border_radiuses radii;
				if (display != display_table_row) {
					radii = css.get_borders().radius.calc_percents(box.size.x, box.size.y);
					if (display == display_inline) {
						if (i > 0) {
							radii.top_left_x = radii.top_left_y = radii.bottom_left_x = radii.bottom_left_y = 0;
						}
						if (i + 1 < boxes.size()) {
							radii.top_right_x = radii.top_right_y = radii.bottom_right_x = radii.bottom_right_y = 0;
						}
					}
				}
				entry.fragments.emplace_back(box, radii);
				const Rect2 visual = matrix.xform(box);
				if (visual.is_finite()) entry.visual_boxes.push_back(visual);
			}
			if (boxes.empty()) {
				const Rect2 fallback = to_rect(p_item->get_placement());
				if (fallback.is_finite() && fallback.size.x >= 0 && fallback.size.y >= 0) {
					entry.anchor_fallback = fallback;
				}
			}
		}

		if (inverse && p_item->has_table_cell_clip()) {
			position box = p_item->get_placement();
			box += p_item->get_paddings();
			box += p_item->get_borders();
			clips.push_back({ { RoundedRect(to_rect(box), {}), matrix }, *inverse, p_item.get(), p_overflow });
			p_overflow = &clips.back();
		}

		if (inverse && css.get_clip_path()) {
			std::shared_ptr<const HTMLClipPolygon> polygon;
			if (const auto shape = resolve_clip_path(*css.get_clip_path(), *p_item, entry.fragments, matrix, viewport.size, polygon)) {
				clips.push_back({ { *shape, matrix, ClipAxes::both, true, std::move(polygon) }, *inverse, p_item.get(), p_overflow });
				p_overflow = &clips.back();
				entry.path_clip = p_overflow;
				entry.overflow = p_overflow;
			}
		}

		// An overflow clip applies to descendants, while a fragment's own
		// rounded border is tested independently for element hits.
		if (inverse && p_item->clips_overflow()) {
			const position content_box = p_item->get_placement();
			const Rect2 clip_box = to_rect(p_item->overflow_clip_box(content_box));
			const bool clip_x = css.get_overflow_x() != overflow_visible;
			const bool clip_y = css.get_overflow_y() != overflow_visible;
			const auto axes = clip_x && clip_y ? ClipAxes::both : clip_x ? ClipAxes::x : ClipAxes::y;
			RoundedRect shape(clip_box, {});
			if (axes == ClipAxes::both) {
				position border_box = content_box;
				border_box += p_item->get_paddings();
				border_box += p_item->get_borders();
				const RoundedRect border(to_rect(border_box),
					css.get_borders().radius.calc_percents(border_box.width, border_box.height));
				const bool uses_clip_edge = css.get_overflow_x() == overflow_clip || css.get_overflow_y() == overflow_clip;
				shape = uses_clip_edge ? border.outset(clip_box) : border.inset(clip_box);
			}
			clips.push_back({ { shape, matrix, axes },
				*inverse, p_item.get(), p_overflow });
			p_overflow = &clips.back();
			entry.child_overflow = p_overflow;
			overflow_controls |= entry.resize_grip().has_area() ||
				(css.get_scrollbar_width() != scrollbar_width::none && p_item->scrollable_content_size().has_value() &&
					!p_item->src_el()->is_replaced() && (is_interactive_overflow(css.get_overflow_x()) || is_interactive_overflow(css.get_overflow_y())));
		}
		Rect2 containing_box = p_containing_box;
		if (viewport.has_area() && !p_item->children().empty()) {
			const bool table_track = is_one_of(display, display_table_row, display_table_row_group,
				display_table_header_group, display_table_footer_group);
			const position content = p_item->get_placement();
			if (display != display_inline && !table_track) {
				containing_box = translate_rect(to_rect(content), sticky_offset);
			}
			if (const auto extent = p_item->scrollable_content_size()) {
				position padding_box = content;
				padding_box += p_item->get_paddings();
				scrollport = translate_rect(to_rect(padding_box), sticky_offset);
				// A scroll container's content coordinate space moves with its children.
				containing_box = translate_rect(containing_box, -Vector2(p_item->get_scroll_left(), p_item->get_scroll_top()));
				containing_box.size = containing_box.size.max(Vector2(extent->width, extent->height));
			}
		}
		const auto* overflow_boundary = css.has_layout_containment() ? p_item.get() : p_layout_overflow_boundary;
		for (const auto& child : p_item->children()) {
			index(child, p_overflow, matrix, fixed_subtree, scrollport, containing_box, sticky_offset, overflow_boundary,
				p_transform_culled, child_perspective ? &*child_perspective : nullptr,
				preserves ? &*spatial : nullptr, entry.depth_plane);
		}
	}

	Vector2 HTMLGeometryIndex::extend_overflow(const Vector2& p_size) const {
		Vector2 extent = p_size;
		for (const auto& pair : renderers) {
			const auto& geometry = pair.second;
			if (!geometry.item->is_visible() || geometry.viewport_fixed_subtree) continue;
			// Back-face culling leaves the projected scrollable area intact.
			if (!geometry.inverse && !geometry.matrix.try_inverse()) continue;
			const auto* nearest_clip = geometry.overflow;
			while (nearest_clip && (nearest_clip->is_path || nearest_clip->axes == ClipAxes::horizon))
				nearest_clip = nearest_clip->parent;
			const auto* boundary = geometry.layout_overflow_boundary;
			const auto* scroll_geometry = boundary && nearest_clip ? find_renderer(nearest_clip->item) : nullptr;
			const bool can_scroll_inside = nearest_clip &&
				nearest_clip->item->css().is_scroll_container() &&
				nearest_clip->matrix != geometry.matrix &&
				(!boundary || nearest_clip->item == boundary ||
					(scroll_geometry && scroll_geometry->layout_overflow_boundary == boundary));
			if (geometry.matrix == Transform2D() && !can_scroll_inside) continue;
			std::vector<TransformedClip> ancestors;
			for (auto* clip = geometry.overflow; clip; clip = clip->parent)
				if (!clip->is_path) ancestors.push_back(*clip);
			std::reverse(ancestors.begin(), ancestors.end());
			const auto include = [&](const Rect2& p_box) {
				if (!p_box.is_finite() || !p_box.has_area()) return;
				const Rect2 box = clipped_visual_bounds(p_box, geometry.matrix, ancestors);
				if (!boundary && box.has_area()) extent = extent.max(box.get_end());
				if (can_scroll_inside) {
					const Rect2 local = (nearest_clip->inverse * geometry.matrix).xform(p_box);
					if (local.is_finite() && local.has_area() && local.get_end().x > nearest_clip->shape.rect.position.x &&
						local.get_end().y > nearest_clip->shape.rect.position.y) {
						auto* item = nearest_clip->item;
						const Vector2 offset(item->get_scroll_left(), item->get_scroll_top());
						const Vector2 end = local.get_end() - to_rect(item->get_placement()).position + offset;
						item->extend_scroll_size(size(end.x, end.y));
					}
				}
			};
			for (const auto& fragment : geometry.fragments) include(fragment.rect);
			const auto& css = geometry.item->css();
			if (!css.has_layout_containment() && (css.get_overflow_x() == overflow_visible || css.get_overflow_y() == overflow_visible)) {
				const auto extra = geometry.item->used_layout_overflow();
				const Rect2 box = to_rect(geometry.item->get_placement());
				auto extent = box;
				if (css.get_overflow_x() == overflow_visible) extent.size.x = std::max(extent.size.x, real_t(extra.width));
				if (css.get_overflow_y() == overflow_visible) extent.size.y = std::max(extent.size.y, real_t(extra.height));
				if (extent.size != box.size) include(extent);
			}
		}
		return extent;
	}
}
