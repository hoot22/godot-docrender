#include "html_paint_recorder.h"
#include "formats/html/elements/html_details_node.h"
#include "html_depth_sort.h"
#include "html_svg_mask.h"

#include "html_text_mask.h"
#include "formats/html/layout/html_line_break.h"
#include "formats/html/layout/html_inline_text_fragment.h"
#include "formats/html/layout/html_polygon_clip.h"

#include <godot_cpp/classes/atlas_texture.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include "gen/html/html_control_icons.gen.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <litehtml/document.h>
#include <litehtml/document_container.h>
#include <litehtml/el_space.h>
#include <litehtml/el_text.h>
#include <litehtml/html_tag.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		Ref<Shader> paint_shader;
		Ref<Shader> composite_clip_shader;

		const char* PAINT_SHADER_CODE = R"SHADER(
shader_type canvas_item;
#ifdef HTML_COMPOSITE_CLIP
render_mode blend_premul_alpha, unshaded;
uniform vec4 shape_basis = vec4(1.0, 0.0, 0.0, 1.0);
uniform vec2 shape_offset;
uniform vec3 shape_divisor = vec3(0.0, 0.0, 1.0);
uniform vec4 shape_polygon;
#endif

uniform int clip_count;
uniform vec4 clip_boxes[16];
uniform int clip_axes[16];
uniform vec4 clip_radii_x[16];
uniform vec4 clip_radii_y[16];
uniform vec4 clip_bases[16];
uniform vec2 clip_offsets[16];
uniform vec4 clip_divisors[16];
uniform bool transformed_clips;
uniform sampler2D clip_edges : filter_nearest, repeat_disable;
uniform bool has_clip_polygons;
uniform vec4 clip_polygons[16];

uniform bool has_shape;
uniform vec4 shape_box;
uniform vec4 shape_radii_x;
uniform vec4 shape_radii_y;

uniform bool draw_border;
uniform vec4 inner_box;
uniform vec4 inner_radii_x;
uniform vec4 inner_radii_y;
uniform vec4 border_widths;
uniform vec4 border_colors[4];
uniform vec4 border_inner_colors[4];
uniform ivec4 border_styles;
const int BORDER_DOTTED = 2;
const int BORDER_DASHED = 3;
const int BORDER_DOUBLE = 4;
const int BORDER_BEVEL_BANDS = 5;
uniform vec4 border_middle_box;
uniform vec4 border_middle_rx;
uniform vec4 border_middle_ry;
uniform vec4 double_outer_box;
uniform vec4 double_outer_rx;
uniform vec4 double_outer_ry;
uniform vec4 double_inner_box;
uniform vec4 double_inner_rx;
uniform vec4 double_inner_ry;
uniform vec4 border_center_box;
uniform vec4 border_center_rx;
uniform vec4 border_center_ry;
uniform vec4 border_edge_starts;
uniform vec4 border_edge_lengths;
uniform vec4 border_arc_lengths;
uniform vec4 border_periods;
uniform vec4 border_pattern_starts;
uniform bool border_pattern_continuous;

uniform int shadow_type;
uniform vec4 shadow_box;
uniform vec4 shadow_rx;
uniform vec4 shadow_ry;
uniform float shadow_blur;
uniform sampler2D text_shadow_mask : filter_linear_mipmap, repeat_disable;
uniform vec4 text_shadow_box;
uniform int decoration_pattern;
uniform float decoration_thickness;
uniform float decoration_center;

uniform int selection_span_count;
uniform vec2 selection_first_span;
uniform sampler2D selection_spans : filter_nearest, repeat_disable;
uniform vec4 selection_ink;
uniform vec4 ordinary_ink;

uniform int gradient_type;
uniform int stop_count;
uniform sampler2D gradient_stops : filter_nearest, repeat_disable;
uniform vec2 gradient_start;
uniform vec2 gradient_parameters;
uniform vec4 tile;
uniform vec2 tile_period;
uniform ivec2 tile_repeat_axes;
instance uniform vec2 scroll_origin;
instance uniform bool hdr_output;
uniform bool tiled_fill;
uniform bool tiled_image;
uniform bool tile_flip_image_x;

uniform bool pixelated;
uniform vec4 pixelated_source;
uniform ivec2 pixelated_repeat;

varying vec2 document_point;
varying vec4 paint_modulate;

void vertex() {
	document_point = VERTEX;
	paint_modulate = COLOR;
}

int rounded_corner(vec2 from_start, vec2 from_end, vec4 rx, vec4 ry) {
	if (from_start.x < rx.x && from_start.y < ry.x) {
		return 0;
	}
	if (from_end.x < rx.y && from_start.y < ry.y) {
		return 1;
	}
	if (from_end.x < rx.z && from_end.y < ry.z) {
		return 2;
	}
	if (from_start.x < rx.w && from_end.y < ry.w) {
		return 3;
	}
	return -1;
}

float box_coverage(vec2 p, vec4 box, vec4 rx, vec4 ry, vec2 pixel_x, vec2 pixel_y) {
	vec2 from_start = p - box.xy;
	vec2 from_end = box.zw - p;
	vec2 near_edge = min(from_start, from_end);
	float distance_to_edge = min(near_edge.x, near_edge.y);
	vec2 normal = near_edge.x < near_edge.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
	int corner = rounded_corner(from_start, from_end, rx, ry);
	if (corner >= 0 && rx[corner] > 0.0 && ry[corner] > 0.0) {
		vec2 radius = vec2(rx[corner], ry[corner]);
		vec2 edge = vec2(
			corner == 0 || corner == 3 ? from_start.x : from_end.x,
			corner < 2 ? from_start.y : from_end.y);
		vec2 unit = (radius - edge) / radius;
		vec2 gradient = unit / radius;
		float normal_length = length(gradient);
		distance_to_edge = (1.0 - length(unit)) / max(normal_length, 0.000001);
		normal = gradient * vec2(corner == 0 || corner == 3 ? 1.0 : -1.0, corner < 2 ? 1.0 : -1.0) /
			max(normal_length, 0.000001);
	}
	float aa = max(abs(dot(normal, pixel_x)) + abs(dot(normal, pixel_y)), 0.001);
	return clamp(distance_to_edge / aa + 0.5, 0.0, 1.0);
}

// Approximation of the standard normal cumulative distribution.
float gaussian_cdf(float x) {
	float t = 1.0 / (1.0 + 0.2316419 * abs(x));
	float tail = 0.39894228 * exp(-0.5 * x * x) * t *
		(0.31938153 + t * (-0.35656378 + t * (1.78147794 + t * (-1.82125598 + t * 1.33027443))));
	return x < 0.0 ? tail : 1.0 - tail;
}

float ellipse_inset(float y, float rx, float ry) {
	if (y >= ry || ry <= 0.0) {
		return 0.0;
	}
	float relative = 1.0 - y / ry;
	return rx * (1.0 - sqrt(max(0.0, 1.0 - relative * relative)));
}

float shadow_mask(vec2 p, vec2 pixel_x, vec2 pixel_y) {
	vec2 size = shadow_box.zw - shadow_box.xy;
	if (size.x <= 0.0 || size.y <= 0.0) {
		return 0.0;
	}
	if (shadow_blur <= 0.0) {
		return box_coverage(p, shadow_box, shadow_rx, shadow_ry, pixel_x, pixel_y);
	}
	float sigma = shadow_blur * 0.5;
	if (sigma < max(length(pixel_x), length(pixel_y)) * 0.2) {
		return box_coverage(p, shadow_box, shadow_rx, shadow_ry, pixel_x, pixel_y);
	}
	vec2 point = p - shadow_box.xy;
	float low = max(0.0, point.y - 3.5 * sigma);
	float high = min(size.y, point.y + 3.5 * sigma);
	if (low >= high || point.x < -3.5 * sigma || point.x > size.x + 3.5 * sigma) {
		return 0.0;
	}
	float top_radius = max(shadow_ry.x, shadow_ry.y);
	float bottom_radius = max(shadow_ry.z, shadow_ry.w);
	float vertical_low = gaussian_cdf((low - point.y) / sigma);
	float left_radius = max(shadow_rx.x, shadow_rx.w);
	float right_radius = max(shadow_rx.y, shadow_rx.z);
	if ((low >= top_radius && high <= size.y - bottom_radius) ||
			(point.x - 3.5 * sigma >= left_radius && point.x + 3.5 * sigma <= size.x - right_radius)) {
		float vertical = gaussian_cdf((high - point.y) / sigma) - vertical_low;
		return vertical * (gaussian_cdf((size.x - point.x) / sigma) - gaussian_cdf(-point.x / sigma));
	}
	// Integrate each horizontal span analytically, then sample the rounded edges vertically.
	float step_size = (high - low) / 16.0;
	float total = 0.0;
	for (int i = 0; i < 16; i++) {
		float y = low + (float(i) + 0.5) * step_size;
		float left = max(ellipse_inset(y, shadow_rx.x, shadow_ry.x), ellipse_inset(size.y - y, shadow_rx.w, shadow_ry.w));
		float right = size.x - max(ellipse_inset(y, shadow_rx.y, shadow_ry.y), ellipse_inset(size.y - y, shadow_rx.z, shadow_ry.z));
		float horizontal = max(0.0, gaussian_cdf((right - point.x) / sigma) - gaussian_cdf((left - point.x) / sigma));
		float vertical_high = gaussian_cdf((y + step_size * 0.5 - point.y) / sigma);
		total += horizontal * (vertical_high - vertical_low);
		vertical_low = vertical_high;
	}
	return total;
}

float text_mask_sample(vec2 uv, float lod) {
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThanEqual(uv, vec2(1.0)))) {
		return 0.0;
	}
	return textureLod(text_shadow_mask, uv, lod).a;
}

float blurred_text_mask(vec2 point) {
	vec2 uv = (point - text_shadow_box.xy) / text_shadow_box.zw;
	if (shadow_blur <= 0.0) {
		return text_mask_sample(uv, 0.0);
	}
	// Mip levels integrate fine glyph details when the taps are far apart.
	float step_size = shadow_blur / 3.0;
	vec2 step_uv = step_size / text_shadow_box.zw;
	float lod = log2(max(step_size, 1.0));
	const float weights[5] = { 1.0, 0.8007374, 0.4111123, 0.1353353, 0.0285655 };
	float total = 0.0;
	for (int y = -4; y <= 4; ++y) {
		for (int x = -4; x <= 4; ++x) {
			total += weights[abs(x)] * weights[abs(y)] * text_mask_sample(uv + vec2(float(x), float(y)) * step_uv, lod);
		}
	}
	return total * (1.0 / 14.07376);
}

// Approximate partial arc length with Simpson integration of the ellipse speed.
float ellipse_arc_fraction(vec2 axes, float angle, vec2 direction) {
	vec2 half_angle = vec2(sin(angle * 0.5), cos(angle * 0.5));
	float arc = angle * (axes.y + 4.0 * length(axes * half_angle) + length(axes * direction));
	float quarter_arc = (PI * 0.5) * (axes.y + 4.0 * length(axes * vec2(0.70710678)) + axes.x);
	return arc / quarter_arc;
}

float border_path_distance(vec2 p, int side, float perimeter) {
	vec2 start = p - border_center_box.xy;
	vec2 end = border_center_box.zw - p;
	int corner = rounded_corner(start, end, border_center_rx, border_center_ry);
	if (corner >= 0 && border_center_rx[corner] > 0.0 && border_center_ry[corner] > 0.0) {
		vec2 radius = vec2(border_center_rx[corner], border_center_ry[corner]);
		vec2 edge = vec2(corner == 0 || corner == 3 ? start.x : end.x, corner < 2 ? start.y : end.y);
		vec2 unit = (radius - edge) / radius;
		vec2 along = corner % 2 == 0 ? unit : unit.yx;
		float angle = atan(along.y, along.x);
		vec2 axes = corner % 2 == 0 ? radius : radius.yx;
		float fraction = ellipse_arc_fraction(axes, angle, normalize(along));
		int previous_side = (corner + 3) % 4;
		if (corner == 0) {
			return perimeter + (fraction - 1.0) * border_arc_lengths[previous_side];
		}
		return border_edge_starts[previous_side] + border_edge_lengths[previous_side] +
			fraction * border_arc_lengths[previous_side];
	}
	vec4 along = vec4(start.x, start.y, end.x, end.y);
	vec4 tangent = vec4(border_center_rx.x, border_center_ry.y, border_center_rx.z, border_center_ry.w);
	return border_edge_starts[side] + along[side] - tangent[side];
}

// Locate a dot's center on the rounded path, then measure the circle in document pixels.
vec2 border_path_point(float distance_along, float perimeter) {
	distance_along = mod(distance_along, max(perimeter, 0.001));
	int side = 0;
	for (int i = 1; i < 4; i++) {
		if (distance_along >= border_edge_starts[i]) {
			side = i;
		}
	}
	vec2 start = border_center_box.xy;
	vec2 end = border_center_box.zw;
	vec4 tangent = vec4(border_center_rx.x, border_center_ry.y, border_center_rx.z, border_center_ry.w);
	float edge = distance_along - border_edge_starts[side];
	if (edge <= border_edge_lengths[side] || border_arc_lengths[side] == 0.0) {
		float along = edge + tangent[side];
		if (side == 0) { return vec2(start.x + along, start.y); }
		if (side == 1) { return vec2(end.x, start.y + along); }
		if (side == 2) { return vec2(end.x - along, end.y); }
		return vec2(start.x, end.y - along);
	}
	int corner = (side + 1) % 4;
	vec2 radius = vec2(border_center_rx[corner], border_center_ry[corner]);
	vec2 axes = corner % 2 == 0 ? radius : radius.yx;
	float fraction = (edge - border_edge_lengths[side]) / border_arc_lengths[side];
	float low = 0.0;
	float high = PI * 0.5;
	for (int i = 0; i < 7; i++) {
		float angle = (low + high) * 0.5;
		vec2 angle_vector = vec2(sin(angle), cos(angle));
		if (ellipse_arc_fraction(axes, angle, angle_vector) < fraction) {
			low = angle;
		} else {
			high = angle;
		}
	}
	float angle = (low + high) * 0.5;
	vec2 unit = vec2(cos(angle), sin(angle));
	vec2 offset = radius * (vec2(1.0) - (corner % 2 == 0 ? unit : unit.yx));
	return vec2(corner == 0 || corner == 3 ? start.x + offset.x : end.x - offset.x,
		corner < 2 ? start.y + offset.y : end.y - offset.y);
}

vec4 sample_gradient(float t) {
	int first = 0;
	int last = stop_count;
	// Find the first stop strictly after t, preserving coincident stops.
	for (int step_index = 0; step_index < 13 && first < last; step_index++) {
		int middle = (first + last) / 2;
		float position = texelFetch(gradient_stops, ivec2(middle, 1), 0).r;
		if (position <= t) {
			first = middle + 1;
		} else {
			last = middle;
		}
	}
	int left = max(first - 1, 0);
	int right = min(first, stop_count - 1);
	vec4 left_color = texelFetch(gradient_stops, ivec2(left, 0), 0);
	vec4 right_color = texelFetch(gradient_stops, ivec2(right, 0), 0);
	vec2 left_stop = texelFetch(gradient_stops, ivec2(left, 1), 0).rg;
	float span = texelFetch(gradient_stops, ivec2(right, 1), 0).r - left_stop.x;
	float weight = span > 0.0 ? clamp((t - left_stop.x) / span, 0.0, 1.0) : 0.0;
	float hint = left_stop.y;
	if (hint > 0.0 && hint < 1.0) {
		weight = pow(weight, log(0.5) / log(hint));
	}
	left_color.rgb *= left_color.a;
	right_color.rgb *= right_color.a;
	vec4 color = mix(left_color, right_color, weight);
	color.rgb /= max(color.a, 0.000001);
	return color;
}

vec4 canvas_color(vec4 c) {
	if (hdr_output) {
		c.rgb = mix(c.rgb / 12.92, pow(max((c.rgb + 0.055) / 1.055, vec3(0.0)), vec3(2.4)),
			greaterThan(c.rgb, vec3(0.04045)));
	}
	return c;
}

bool selected_text_pixel(float x) {
	if (selection_span_count == 1) return x >= selection_first_span.x && x < selection_first_span.y;
	int columns = textureSize(selection_spans, 0).x;
	int start = 0;
	int end = selection_span_count;
	while (start < end) {
		int midpoint = (start + end) / 2;
		vec2 span = texelFetch(selection_spans, ivec2(midpoint % columns, midpoint / columns), 0).rg;
		if (x < span.x) end = midpoint;
		else if (x >= span.y) start = midpoint + 1;
		else return true;
	}
	return false;
}

vec2 pixelated_uv(vec2 texel, vec2 dx, vec2 dy, vec4 source, ivec2 repeats) {
	vec2 texels_per_pixel = vec2(length(vec2(dx.x, dy.x)), length(vec2(dx.y, dy.y)));
	vec2 copies = max(floor(1.0 / max(texels_per_pixel, vec2(0.000001)) + 0.500001), vec2(1.0));
	vec2 local = texel - source.xy;
	if (repeats.x != 0) {
		local.x = mod(local.x, source.z);
	}
	if (repeats.y != 0) {
		local.y = mod(local.y, source.w);
	}
	// Two adjacent virtual pixels map to the same or adjacent source pixels.
	// Remap the interpolation weights so a single bilinear tap filters the virtual image.
	vec2 intermediate = local * copies - 0.5;
	vec2 first = floor(intermediate);
	vec2 a = floor(first / copies);
	vec2 b = floor((first + 1.0) / copies);
	vec2 sample_point = mix(a, b, fract(intermediate)) + 0.5;
	return source.xy + clamp(sample_point, vec2(0.5), max(vec2(0.5), source.zw - 0.5));
}

vec2 tile_coordinates(vec2 point) {
	vec2 local = point - tile.xy - scroll_origin;
	if (tile_repeat_axes.x != 0) local.x = mod(local.x, tile_period.x);
	if (tile_repeat_axes.y != 0) local.y = mod(local.y, tile_period.y);
	return local;
}

float polygon_coverage(vec2 point, vec2 dx, vec2 dy, vec4 polygon) {
	int index = int(polygon.x);
	int end = index + int(polygon.y);
	int columns = textureSize(clip_edges, 0).x;
	vec2 diagonal = (dx + dy) * 0.25;
	vec2 anti_diagonal = (dx - dy) * 0.25;
	vec4 xs = vec4(point.x) + vec4(diagonal.x, anti_diagonal.x, -anti_diagonal.x, -diagonal.x);
	vec4 ys = vec4(point.y) + vec4(diagonal.y, anti_diagonal.y, -anti_diagonal.y, -diagonal.y);
	// Postfix program. All four subsamples participate in each boolean operation.
	vec4 fills[64];
	int depth = 0;
	while (index < end) {
		vec4 header = texelFetch(clip_edges, ivec2(index % columns, index / columns), 0);
		index++;
		if (header.z != 0.0) {
			depth--;
			fills[depth - 1] = header.z == 1.0 ? max(fills[depth - 1], fills[depth]) : min(fills[depth - 1], fills[depth]);
			continue;
		}
		vec4 winding = vec4(0.0);
		for (int i = 0; i < int(header.x); i++) {
			vec4 edge = texelFetch(clip_edges, ivec2(index % columns, index / columns), 0);
			index++;
			vec2 a = edge.xy;
			vec2 b = edge.zw;
			if (a.y != b.y) {
				vec4 cross_x = vec4(a.x) + (ys - a.y) * ((b.x - a.x) / (b.y - a.y));
				vec4 within_y = step(min(a.y, b.y), ys) - step(max(a.y, b.y), ys);
				winding += sign(b.y - a.y) * within_y * (1.0 - step(cross_x, xs));
			}
		}
		vec4 inside = abs(winding);
		if (header.y != 0.0) inside = mod(inside, 2.0);
		fills[depth] = step(vec4(0.5), inside);
		depth++;
	}
	return dot(fills[0], vec4(0.25));
}

bool map_clip_point(mat2 basis, vec2 offset, vec3 divisor, inout vec2 point, inout vec2 dx, inout vec2 dy) {
	float w = dot(divisor, vec3(point, 1.0));
	if (w <= 0.0) return false;
	point = (basis * point + offset) / w;
	dx = (basis * dx - point * dot(divisor.xy, dx)) / w;
	dy = (basis * dy - point * dot(divisor.xy, dy)) / w;
	return true;
}

void fragment() {
	// Take derivatives before the corner and paint branches diverge.
	vec2 pixel_x = dFdx(document_point);
	vec2 pixel_y = dFdy(document_point);
	float coverage = 1.0;
	for (int i = 0; i < clip_count; i++) {
		int axis = clip_axes[i];
		if (axis == 3) {
			if (transformed_clips && dot(clip_divisors[i].xyz, vec3(document_point, 1.0)) <= 0.0) discard;
			continue;
		}
		vec2 point = document_point;
		vec2 dx = pixel_x;
		vec2 dy = pixel_y;
		if (transformed_clips) {
			vec4 basis = clip_bases[i];
			if (!map_clip_point(mat2(basis.xy, basis.zw), clip_offsets[i], clip_divisors[i].xyz, point, dx, dy)) {
				discard;
			}
		}
		vec4 box = clip_boxes[i];
		if (has_clip_polygons && clip_polygons[i].y > 0.0) {
			coverage = min(coverage, polygon_coverage(point, dx, dy, clip_polygons[i]));
		} else if (axis == 0) {
			coverage = min(coverage, box_coverage(point, box, clip_radii_x[i], clip_radii_y[i], dx, dy));
		} else {
			int coordinate = axis - 1;
			float distance_to_edge = min(point[coordinate] - box[coordinate],
				box[coordinate + 2] - point[coordinate]);
			float aa = max(abs(dx[coordinate]) + abs(dy[coordinate]), 0.001);
			coverage = min(coverage, clamp(distance_to_edge / aa + 0.5, 0.0, 1.0));
		}
	}
#ifdef HTML_COMPOSITE_CLIP
	if (has_shape) {
		vec2 point = document_point;
		vec2 dx = pixel_x;
		vec2 dy = pixel_y;
		if (!map_clip_point(mat2(shape_basis.xy, shape_basis.zw), shape_offset, shape_divisor, point, dx, dy)) discard;
		float mask = shape_polygon.y > 0.0 ? polygon_coverage(point, dx, dy, shape_polygon) :
			box_coverage(point, shape_box, shape_radii_x, shape_radii_y, dx, dy);
		coverage = min(coverage, mask);
	}
	COLOR = texture(TEXTURE, UV) * vec4(paint_modulate.rgb, 1.0) * (paint_modulate.a * coverage);
#else
	if (has_shape) {
		coverage = min(coverage, box_coverage(document_point, shape_box, shape_radii_x, shape_radii_y, pixel_x, pixel_y));
	}
	if (decoration_pattern != 0) {
		float width = decoration_thickness;
		float y = document_point.y - decoration_center;
		float aa = max(length(abs(pixel_x) + abs(pixel_y)), 0.001);
		float distance_to_edge;
		if (decoration_pattern == 1) {
			float x = mod(document_point.x, 2.0 * width) - width;
			distance_to_edge = width * 0.5 - length(vec2(x, y));
		} else if (decoration_pattern == 2) {
			float x = mod(document_point.x, 5.0 * width) - 2.5 * width;
			distance_to_edge = min(width * 1.5 - abs(x), width * 0.5 - abs(y));
		} else {
			float phase = mod(document_point.x, 6.0 * width) / (6.0 * width) * TAU;
			float slope = cos(phase) * TAU / 6.0;
			distance_to_edge = width * 0.5 - abs(y - sin(phase) * width) / sqrt(1.0 + slope * slope);
		}
		coverage = min(coverage, clamp(distance_to_edge / aa + 0.5, 0.0, 1.0));
	} else if (shadow_type == 3) {
		coverage *= blurred_text_mask(document_point);
	} else if (shadow_type != 0) {
		float mask = shadow_mask(document_point, pixel_x, pixel_y);
		if (shadow_type == 1) {
			coverage *= mask * (1.0 - box_coverage(document_point, inner_box, inner_radii_x, inner_radii_y, pixel_x, pixel_y));
		} else {
			coverage *= 1.0 - mask;
		}
	} else if (draw_border) {
		if (inner_box.z > inner_box.x && inner_box.w > inner_box.y) {
			coverage = min(coverage, 1.0 - box_coverage(document_point, inner_box, inner_radii_x, inner_radii_y, pixel_x, pixel_y));
		}
		vec2 a = document_point - shape_box.xy;
		vec2 b = shape_box.zw - document_point;
		vec4 distance = vec4(a.y, b.x, b.y, a.x) / max(border_widths, vec4(0.000001));
		int side = -1;
		for (int i = 0; i < 4; i++) {
			if (border_widths[i] > 0.0 && (side < 0 || distance[i] < distance[side])) {
				side = i;
			}
		}
		int style = border_styles[side];
		if (style == BORDER_DOTTED || style == BORDER_DASHED) {
			int path_side = side;
			if (border_pattern_continuous) {
				vec2 center_start = document_point - border_center_box.xy;
				vec2 center_end = border_center_box.zw - document_point;
				vec4 center_distance = vec4(center_start.y, center_end.x, center_end.y, center_start.x);
				for (int i = 0; i < 4; i++) {
					if (center_distance[i] < center_distance[path_side]) {
						path_side = i;
					}
				}
			}
			float perimeter = border_edge_starts.w + border_edge_lengths.w + border_arc_lengths.w;
			float path = border_path_distance(document_point, path_side, perimeter);
			if (!border_pattern_continuous && side == 0 && path > border_edge_lengths.x + border_arc_lengths.x) {
				path -= perimeter;
			}
			float period = border_periods[side];
			float along = mod(path - border_pattern_starts[side], period) - period * 0.5;
			float aa = max(length(abs(pixel_x) + abs(pixel_y)), 0.001);
			float edge = period * 0.3 - abs(along);
			if (style == BORDER_DOTTED) {
				float radius = min(border_widths[side], period) * 0.5;
				float center = path - along;
				float distance_to_dot = length(document_point - border_path_point(center, perimeter));
				// An adjacent center can be closer around a sharp or elliptical corner.
				if (border_pattern_continuous && distance_to_dot > radius - aa) {
					float adjacent = center + (along < 0.0 ? -period : period);
					distance_to_dot = min(distance_to_dot, length(document_point - border_path_point(adjacent, perimeter)));
				}
				edge = radius - distance_to_dot;
			}
			coverage = min(coverage, clamp(edge / aa + 0.5, 0.0, 1.0));
		} else if (style == BORDER_DOUBLE) {
			float outside_gap = 1.0 - box_coverage(document_point, double_outer_box, double_outer_rx, double_outer_ry, pixel_x, pixel_y);
			float inside_gap = box_coverage(document_point, double_inner_box, double_inner_rx, double_inner_ry, pixel_x, pixel_y);
			coverage = min(coverage, max(outside_gap, inside_gap));
		}
		vec4 border_color = border_colors[side];
		if (style == BORDER_BEVEL_BANDS && border_middle_box.z > border_middle_box.x &&
				border_middle_box.w > border_middle_box.y) {
			float inner_band = box_coverage(document_point, border_middle_box, border_middle_rx, border_middle_ry, pixel_x, pixel_y);
			border_color = mix(border_color, border_inner_colors[side], inner_band);
		}
		COLOR *= canvas_color(border_color);
	} else if (gradient_type > 0 || tiled_fill) {
		vec2 local = tile_coordinates(document_point);
		if (any(lessThan(local, vec2(0.0))) || any(greaterThanEqual(local, tile.zw))) {
			discard;
		}
		if (gradient_type > 0) {
			vec2 delta = tile.xy + local - gradient_start;
			float t;
			if (gradient_type == 1) {
				t = dot(delta, gradient_parameters) / max(dot(gradient_parameters, gradient_parameters), 0.000001);
			} else if (gradient_type == 2) {
				t = length(delta / max(gradient_parameters, vec2(0.000001)));
			} else {
				t = fract(atan(delta.x, -delta.y) / TAU - gradient_parameters.x);
			}
			COLOR *= canvas_color(sample_gradient(t));
		}
	} else if (tiled_image) {
		vec2 local = tile_coordinates(document_point);
		if (any(lessThan(local, vec2(0.0))) || any(greaterThanEqual(local, tile.zw))) {
			discard;
		}
		vec2 ratio = local / tile.zw;
		vec2 axis = vec2(tile_flip_image_x ? -1.0 : 1.0, 1.0);
		if (tile_flip_image_x) {
			ratio.x = 1.0 - ratio.x;
		}
		vec4 region = REGION_RECT / TEXTURE_PIXEL_SIZE.xyxy;
		vec2 texel = region.xy + ratio * region.zw;
		vec2 step_scale = axis * region.zw / tile.zw;
		if (pixelated) {
			vec2 point = pixelated_uv(texel, pixel_x * step_scale, pixel_y * step_scale, region, ivec2(0));
			COLOR = paint_modulate * textureLod(TEXTURE, point * TEXTURE_PIXEL_SIZE, 0.0);
		} else {
			// Clamp within each slice to keep bilinear sampling out of its neighbors.
			vec2 half_texel = min(vec2(0.5), region.zw * 0.5);
			texel = clamp(texel, region.xy + half_texel, region.xy + region.zw - half_texel);
			COLOR = paint_modulate * textureGrad(TEXTURE, texel * TEXTURE_PIXEL_SIZE,
				pixel_x * step_scale * TEXTURE_PIXEL_SIZE, pixel_y * step_scale * TEXTURE_PIXEL_SIZE);
		}
	} else if (pixelated) {
		vec2 texel = UV / TEXTURE_PIXEL_SIZE;
		vec2 point = pixelated_uv(texel, dFdx(texel), dFdy(texel), pixelated_source, pixelated_repeat);
		COLOR = paint_modulate * textureLod(TEXTURE, point * TEXTURE_PIXEL_SIZE, 0.0);
	}
	if (selection_span_count > 0) {
		COLOR *= canvas_color(selected_text_pixel(document_point.x) ? selection_ink : ordinary_ink);
	}
	COLOR.a *= coverage;
#endif
}
)SHADER";

		Color to_color(const web_color& p_color, const web_color& p_current = web_color::black) {
			const auto srgb = p_color.to_srgb(p_current);
			return Color(srgb.red / 255.0f, srgb.green / 255.0f, srgb.blue / 255.0f, srgb.alpha / 255.0f);
		}

		Rect2 image_sample_bounds(const Ref<Texture2D>& p_texture) {
			Rect2 bounds(Vector2(), p_texture->get_size());
			Ref<Texture2D> texture = p_texture;
			for (Ref<AtlasTexture> atlas = texture; atlas.is_valid(); atlas = texture) {
				texture = atlas->get_atlas();
				if (texture.is_null()) return {};
				Rect2 region = atlas->get_region();
				region.size = region.size.floor();
				if (region.size.x == 0) region.size.x = texture->get_width();
				if (region.size.y == 0) region.size.y = texture->get_height();
				// Match AtlasTexture's draw_rect_region mapping to the underlying texture.
				bounds.position += region.position - atlas->get_margin().position;
				bounds = bounds.intersection(region);
			}
			return bounds.intersection(Rect2(Vector2(), texture->get_size()));
		}

		Rect2 get_background_clip(const background_layer& p_layer) {
			return to_rect(p_layer.border_box).intersection(to_rect(p_layer.clip_box));
		}

		struct BackgroundTiles {
			Rect2 tile;
			Vector2i repeat_axes;
			Vector2 spacing;
			bool fixed;

			explicit BackgroundTiles(const background_layer& p_layer) :
				tile(to_rect(p_layer.origin_box)), repeat_axes(p_layer.repeat.x != background_repeat::no_repeat,
					p_layer.repeat.y != background_repeat::no_repeat),
				spacing(p_layer.tile_spacing.width, p_layer.tile_spacing.height),
				fixed(p_layer.attachment == background_attachment_fixed) {}

			Rect2 bounds(const Rect2& p_clip) const {
				if (!tile.is_finite() || !tile.has_area()) return {};
				if (fixed) return p_clip;
				return p_clip.intersection(Rect2(repeat_axes.x ? p_clip.position.x : tile.position.x,
					repeat_axes.y ? p_clip.position.y : tile.position.y, repeat_axes.x ? p_clip.size.x : tile.size.x,
					repeat_axes.y ? p_clip.size.y : tile.size.y));
			}
		};

		void set_tile_uniforms(const Ref<ShaderMaterial>& p_material, const Rect2& p_tile, const Vector2& p_period,
			const Vector2i& p_repeat_axes = Vector2i(1, 1)) {
			p_material->set_shader_parameter("tile", Vector4(p_tile.position.x, p_tile.position.y, p_tile.size.x, p_tile.size.y));
			p_material->set_shader_parameter("tile_period", p_period);
			p_material->set_shader_parameter("tile_repeat_axes", p_repeat_axes);
		}

		Vector4 box_uniform(const Rect2& p_rect) {
			return Vector4(p_rect.position.x, p_rect.position.y, p_rect.get_end().x, p_rect.get_end().y);
		}

		enum BorderPaint { BORDER_NONE, BORDER_SOLID, BORDER_DOTTED, BORDER_DASHED, BORDER_DOUBLE, BORDER_BEVEL_BANDS, BORDER_BEVEL_SOLID };

		BorderPaint get_border_paint(border_style p_style) {
			switch (p_style) {
				case border_style_solid: return BORDER_SOLID;
				case border_style_dotted: return BORDER_DOTTED;
				case border_style_dashed: return BORDER_DASHED;
				case border_style_double: return BORDER_DOUBLE;
				case border_style_groove:
				case border_style_ridge: return BORDER_BEVEL_BANDS;
				case border_style_inset:
				case border_style_outset: return BORDER_BEVEL_SOLID;
				default: return BORDER_NONE;
			}
		}

		float quarter_ellipse_length(float p_x, float p_y) {
			const float sum = p_x + p_y;
			if (sum == 0.0f) {
				return 0.0f;
			}
			// Ramanujan's approximation, divided by four for each corner.
			const float ratio = (p_x - p_y) / sum;
			const float h = ratio * ratio;
			return Math_PI * 0.25f * sum * (1.0f + 3.0f * h / (10.0f + std::sqrt(4.0f - 3.0f * h)));
		}

		float border_pattern_period(float p_length, float p_width, BorderPaint p_style) {
			const float preferred = p_width * (p_style == BORDER_DOTTED ? 2.0f : 5.0f);
			const float count = std::max(1.0f, std::round(p_length / preferred));
			return std::max(0.001f, p_length / count);
		}

	} // namespace

	void HTMLPaintRecorder::clear() {
		recording_geometry = nullptr;
		active_geometry = nullptr;
		clips.clear();
		clip_material.unref();
		pointer_targets.clear();
		overflow_control_bounds = Rect2();
		painted_text.clear();
		painted_text_by_renderer.clear();
		painted_text_by_source.clear();
		paint_runs.clear();
		viewport_background_runs.clear();
		viewport_background_groups.clear();
		open_groups.clear();
		paint_groups.clear();
		filter_depth = 0;
		separate_run = false;
		has_depth_scene = false;
		diagnostic_runs.clear();
		plain_text = String();
		accessible_text = String();
		excluded_selection.clear();
		selection_scopes.clear();
		text_index_spans.clear();
		selection_scope_starts.clear();
		separator_selectable.reset();
		auto_content_text.clear();
		text_runs.clear();
		ellipses.clear();
		ellipses_by_context.clear();
		overflow_paint.clear();
		elided_text_offsets.clear();
		text_overflow_clip_active = false;
	}

	void HTMLPaintRecorder::begin(const HTMLGeometryIndex& p_geometry, const render_item* p_root, const OverflowTextShaper& p_shaper) {
		clear();
		recording_geometry = &p_geometry;
		if (p_root) prepare_text_overflow(*p_root, p_shaper);
	}

	void HTMLPaintRecorder::record_viewport_background(const render_item& p_root, const std::function<void()>& p_draw) {
		// Record only the canvas background with the root's regular transform and paint effects.
		paint_runs.swap(viewport_background_runs);
		paint_groups.swap(viewport_background_groups);
		const bool previous_boundary = separate_run;
		active_geometry = recording_geometry->find_renderer(&p_root);
		begin_paint_group(p_root);
		p_draw();
		end_paint_group();
		active_geometry = nullptr;
		separate_run = previous_boundary;
		paint_runs.swap(viewport_background_runs);
		paint_groups.swap(viewport_background_groups);
	}

	void HTMLPaintRecorder::finish(const element::ptr& p_root, const TextShaper& p_shape_text) {
		clips.clear();
		clip_material.unref();
		active_geometry = nullptr;
		for (HTMLPaintRun& run : paint_runs) {
			for (Command& command : run.commands) {
				if (Text* text = std::get_if<Text>(&command)) {
					const Rect2 rect(text->position, text->layout.size());
					const Rect2 visible = run.clips.empty() && run.clip ? run.matrix.xform(rect).intersection(*run.clip) :
						clipped_visual_bounds(rect, run.matrix, run.clips);
					if (visible.has_area() && text->length > 0) {
						if (text->renderer) {
							const auto* fragment = dynamic_cast<const HTMLInlineTextFragment*>(text->renderer->src_el().get());
							if (fragment) {
								painted_text_by_source[fragment->original_renderer()->src_el().get()].push_back({ text, fragment->source_start() });
							}
							else painted_text_by_renderer[text->renderer].push_back(text);
						}
						else painted_text[text->position].push_back(text);
					}
				}
			}
		}
		String separator;
		AccessibleTextIndex accessibility{accessible_text, {}};
		index_text(p_root, p_shape_text, separator, &accessibility);
		// Paint order can differ from document order for positioned or floating text.
		// The DOM pass assigns offsets while the paint runs retain the exact clip geometry.
		painted_text.clear();
		painted_text_by_renderer.clear();
		painted_text_by_source.clear();
		for (HTMLPaintRun& run : paint_runs) {
			for (Command& command : run.commands) {
				auto* text = std::get_if<Text>(&command);
				if (!text) continue;
				int8_t ellipsis_direction = 0;
				if (text->ellipsis >= 0) {
					const auto& mark = ellipses[text->ellipsis];
					int64_t first = std::numeric_limits<int64_t>::max();
					int64_t last = -1;
					for (const auto& source : mark.sources) {
						const auto& [offset, length] = elided_text_offsets.at(source.element);
						if (offset >= 0 && source.from < length) {
							first = std::min(first, offset + source.from);
							last = std::max(last, offset + std::min(length, source.to));
						}
					}
					if (first < last) {
						text->offset = first;
						text->length = last - first;
						text->selectable = has_selectable_text(first, last);
						ellipsis_direction = mark.rtl ? -1 : 1;
					}
				}
				if (text->offset >= 0) {
					const Rect2 rect(text->position, text->layout.size());
					const Rect2 visible = run.clips.empty() && run.clip ? run.matrix.xform(rect).intersection(*run.clip) :
						clipped_visual_bounds(rect, run.matrix, run.clips);
					text_runs.push_back({ text->layout, rect, visible, text->offset, text->length,
						run.matrix, run.matrix.inverse(), run.clips, ellipsis_direction, text->selection_owner, text->selectable });
				}
			}
		}
		recording_geometry = nullptr;
	}

	void HTMLPaintRecorder::visit_pointer_targets(const Vector2& p_point, const std::function<bool(const PointerTarget&)>& p_visit) const {
		if (!has_depth_scene) {
			for (auto it = pointer_targets.rbegin(); it != pointer_targets.rend(); ++it) {
				if (p_visit(*it)) break;
			}
			return;
		}
		struct Candidate {
			std::size_t index;
			bool group;
			real_t depth;
		};
		const auto depth_at_pointer = [&](const std::optional<Vector3>& p_plane) {
			return p_plane ? html_depth_at(*p_plane, p_point) : real_t(0);
		};
		const auto visit = [&](auto&& self, std::size_t p_first, std::size_t p_last, std::size_t p_child,
			std::size_t p_after, bool p_sort) -> bool {
			std::vector<Candidate> candidates;
			const auto include_targets = [&](std::size_t p_end) {
				for (; p_first < p_end; ++p_first) {
					const auto& target = pointer_targets[p_first];
					if (!target.geometry->contains_visible_point(p_point)) continue;
					candidates.push_back({p_first, false, p_sort ? depth_at_pointer(target.geometry->depth_plane) : real_t(0)});
				}
			};
			for (auto child = p_child; child < p_after; child = paint_groups[child].after_group) {
				const auto& group = paint_groups[child];
				include_targets(group.first_pointer);
				if (group.first_pointer < group.last_pointer)
					candidates.push_back({child, true, p_sort ? depth_at_pointer(group.depth_plane) : real_t(0)});
				p_first = group.last_pointer;
			}
			include_targets(p_last);
			if (p_sort) {
				std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
					return a.depth < b.depth;
				});
			}
			for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
				if (!it->group) {
					if (p_visit(pointer_targets[it->index])) return true;
					continue;
				}
				const auto& group = paint_groups[it->index];
				if (self(self, group.first_pointer, group.last_pointer, it->index + 1, group.after_group, group.sorts_depth))
					return true;
			}
			return false;
		};
		visit(visit, 0, pointer_targets.size(), 0, paint_groups.size(), false);
	}

	std::shared_ptr<litehtml::element> HTMLPaintRecorder::hit_test_element(const Vector2& p_point, bool p_text_selection) const {
		std::shared_ptr<litehtml::element> element;
		visit_hit_elements(p_point, p_text_selection, [&](const auto& p_element) {
			element = p_element;
			return true;
		});
		return element;
	}

	void HTMLPaintRecorder::visit_hit_elements(const Vector2& p_point, bool p_text_selection,
		const std::function<bool(const std::shared_ptr<litehtml::element>&)>& p_visit) const {
		visit_pointer_targets(p_point, [&](const PointerTarget& p_target) {
			if (p_target.axis != PointerTarget::element) return false;
			const auto* geometry = p_target.geometry;
			if (geometry->item->is_visible() && (p_text_selection || geometry->item->css().accepts_pointer_events()) &&
				geometry->contains_visible_point(p_point)) {
				return p_visit(geometry->item->src_el());
			}
			return false;
		});
	}

	const HTMLPaintRecorder::PointerTarget* HTMLPaintRecorder::overflow_control_at(const Vector2& p_point) const {
		if (!overflow_control_bounds.has_point(p_point)) return nullptr;
		const PointerTarget* result = nullptr;
		visit_pointer_targets(p_point, [&](const PointerTarget& p_target) {
			const auto& geometry = *p_target.geometry;
			if (!geometry.item->is_visible() || !geometry.item->css().accepts_pointer_events() ||
				!geometry.contains_visible_point(p_point)) return false;
			if (p_target.axis == PointerTarget::element) return true;
			const Vector2 local = geometry.inverse->xform(p_point);
			if (!p_target.bounds.has_point(local) || !geometry.child_overflow->contains_point(p_point, true)) return false;
			result = &p_target;
			return true;
		});
		return result;
	}

	std::optional<HTMLScrollbarDrag> HTMLPaintRecorder::scrollbar_at(const Vector2& p_point) const {
		const auto* target = overflow_control_at(p_point);
		if (!target || target->axis < 0) return {};
		const auto& geometry = *target->geometry;
		const int axis = target->axis;
		const Vector2 local = geometry.inverse->xform(p_point);
		const auto bar = geometry.scrollbars()[axis];
		const real_t fraction = bar.thumb.has_point(local) ?
			(local[axis] - bar.thumb.position[axis]) / bar.thumb.size[axis] : 0.5;
		return HTMLScrollbarDrag{geometry.item->src_el(), axis, fraction};
	}

	const RenderGeometry* HTMLPaintRecorder::resize_grip_at(const Vector2& p_point) const {
		const auto* target = overflow_control_at(p_point);
		return target && target->axis == PointerTarget::resize ? target->geometry : nullptr;
	}

	void HTMLPaintRecorder::record_css_diagnostic(const RenderGeometry& p_geometry, const Rect2& p_box) {
		if (!p_geometry.inverse) return;
		active_geometry = &p_geometry;
		std::vector<const OverflowClip*> overflow;
		for (auto* clip = p_geometry.overflow; clip; clip = clip->parent) {
			overflow.push_back(clip);
		}
		for (auto it = overflow.rbegin(); it != overflow.rend(); ++it) {
			push_clip(**it);
		}

		const Color color(1.0f, 0.56f, 0.10f, 0.95f);
		const Vector2 end = p_box.get_end();
		const real_t stroke = std::min<real_t>(2.0, std::min(p_box.size.x, p_box.size.y) * 0.5);
		for (const Rect2& edge : {
			Rect2(p_box.position.x, p_box.position.y, p_box.size.x, stroke),
			Rect2(p_box.position.x, end.y - stroke, p_box.size.x, stroke),
			Rect2(p_box.position.x, p_box.position.y + stroke, stroke, p_box.size.y - 2 * stroke),
			Rect2(end.x - stroke, p_box.position.y + stroke, stroke, p_box.size.y - 2 * stroke),
		}) {
			if (edge.has_area()) {
				add_command_to(diagnostic_runs, Fill{ edge, color });
			}
		}
		clips.clear();
		clip_material.unref();
		active_geometry = nullptr;
	}

	void HTMLPaintRecorder::exclude_from_selection(int64_t p_from, int64_t p_to) {
		if (p_from >= p_to) return;
		if (!excluded_selection.empty() && excluded_selection.back().y == p_from) {
			excluded_selection.back().y = static_cast<int32_t>(p_to);
		} else {
			excluded_selection.emplace_back(static_cast<int32_t>(p_from), static_cast<int32_t>(p_to));
		}
	}

	void HTMLPaintRecorder::AccessibleTextIndex::append(const String& p_value) {
		if (p_value.is_empty()) return;
		if (!text.is_empty()) text += separator;
		text += p_value;
		separator = String();
	}

	void HTMLPaintRecorder::AccessibleTextIndex::space() {
		if (separator.is_empty() && !text.is_empty()) separator = " ";
	}

	int64_t HTMLPaintRecorder::index_source_text(const String& p_text, String& r_separator, bool p_selectable,
		AccessibleTextIndex* p_accessible) {
		if (p_accessible) p_accessible->append(p_text);
		const int64_t separator_start = plain_text.length();
		plain_text += r_separator;
		r_separator = String();
		const int64_t start = plain_text.length();
		plain_text += p_text;
		if (!separator_selectable.value_or(p_selectable)) exclude_from_selection(separator_start, start);
		separator_selectable.reset();
		if (p_text.is_empty()) return start;
		for (auto scope = selection_scope_starts.rbegin(); scope != selection_scope_starts.rend() && *scope < 0; ++scope) {
			*scope = start;
		}
		if (!p_selectable) exclude_from_selection(start, plain_text.length());
		return start;
	}

	void HTMLPaintRecorder::index_text(const element::ptr& p_element, const TextShaper& p_shape_text,
		String& r_separator, AccessibleTextIndex* p_accessible, bool p_unlaid, bool p_selectable) {
		if (!p_element || litehtml::is_one_of(p_element->tag(), litehtml::_textarea_, litehtml::_progress_, litehtml::_meter_) ||
			p_element->css().get_display() == display_none ||
			p_element->css().hides_contents()) {
			return;
		}
		const auto display = p_element->css().get_display();
		const auto selection = p_element->css().get_user_select();
		const bool selectable = selection == css_user_select::automatic ? p_selectable : selection != css_user_select::none;
		if (display == display_table_cell) {
			const auto item = p_element->get_render_item();
			if (item && item->collapsed_table_track()) return;
		}
		const bool visible = p_element->css().get_visibility() == visibility_visible;
		if (p_element->is_break()) {
			// A trimmed source line feed stays in the DOM but has no render item.
			if (visible && (p_unlaid || !p_element->is_text() || p_element->get_render_item())) {
				if (!plain_text.is_empty()) {
					if (r_separator == " " || r_separator == "\t") r_separator = String();
					r_separator += "\n";
					separator_selectable = selectable;
				}
				if (p_accessible && !p_accessible->text.is_empty()) {
					auto& separator = p_accessible->separator;
					if (separator == " " || separator == "\t") separator = String();
					separator += "\n";
				}
			}
			return;
		}
		if (display == display_inline_text) {
			const auto elided = elided_text_offsets.find(p_element.get());
			const auto append = [&](const String& p_source) {
				const int64_t start = index_source_text(p_source, r_separator, p_selectable, p_accessible);
				if (elided != elided_text_offsets.end()) elided->second = { start, p_source.length() };
				if (!p_source.is_empty()) text_index_spans.push_back({p_element, start, start + p_source.length()});
				return start;
			};
			const auto append_unpainted = [&](const el_text& p_text) {
				const auto whitespace = p_text.css().get_white_space_collapse();
				if (p_text.is_white_space() && whitespace == white_space_discard) return;
				if (p_text.is_white_space() && litehtml::is_one_of(whitespace, white_space_collapse_spaces, white_space_preserve_breaks)) {
					if (p_accessible) p_accessible->space();
					if (r_separator.is_empty() && !plain_text.is_empty()) r_separator = " ";
					if (r_separator == " " && !separator_selectable.has_value()) separator_selectable = p_selectable;
				}
				else {
					const auto& source = p_text.get_source_text();
					append(source == "\r" || (whitespace == white_space_preserve_spaces && (source == "\t" || source == "\n")) ?
						String(" ") : String::utf8(source.c_str()));
				}
			};
			if (visible) {
				if (const auto* fragments = painted_text_by_source.getptr(p_element.get())) {
					const auto& text = static_cast<const el_text&>(*p_element);
					const int64_t start = append(String::utf8(text.get_source_text().c_str()));
					for (const auto& [draw, offset] : *fragments) {
						draw->offset = start + offset;
						draw->selectable = p_selectable;
					}
					return;
				}
			}
			std::shared_ptr<render_item> item;
			p_element->run_on_renderers([&](const std::shared_ptr<render_item>& p_item) {
				item = p_item;
				return false;
			});
			if (!visible) return;
			const auto* node = dynamic_cast<const el_text*>(p_element.get());
			if (!node) return;
			if (!item) {
				if (elided != elided_text_offsets.end()) append(String::utf8(node->get_source_text().c_str()));
				else if (p_unlaid) append_unpainted(*node);
				return;
			}
			if (p_element->is_white_space()) {
				const auto* space = dynamic_cast<const litehtml::el_space*>(p_element.get());
				const bool replaced = space && space->has_autospace_width();
				if (!replaced || item->skip() || item->width() <= 0) {
					if (p_accessible) p_accessible->space();
					if (r_separator.is_empty() && !plain_text.is_empty() && (!replaced || !plain_text.ends_with(" "))) {
						r_separator = " ";
						separator_selectable = p_selectable;
					}
					return;
				}
			}
			if (!p_element->parent()) return;
			if (item->skip()) {
				if (elided != elided_text_offsets.end()) append(String::utf8(node->get_source_text().c_str()));
				return;
			}
			if (litehtml::is_explicit_wrap_point(*node) || litehtml::is_soft_hyphen(*node)) {
				append(String::utf8(node->get_source_text().c_str()));
				return;
			}
			if (node->is_preserved_tab() && item->pos().width <= 0) {
				append("\t");
				return;
			}
			auto* candidates = painted_text_by_renderer.getptr(item.get());
			RID expected_layout;
			if (!candidates) {
				const auto shaped = p_shape_text(*p_element, String::utf8(node->get_rendered_text().c_str()));
				if (!shaped.is_valid()) {
					if (p_unlaid) append_unpainted(*node);
					return;
				}
				expected_layout = shaped.get_rid();
				const position placement = item->get_placement();
				const Vector2 point(placement.x, std::round(placement.y) + shaped.ascent - shaped.line_ascent());
				candidates = painted_text.getptr(point);
			}
			if (candidates) {
				for (Text* text : *candidates) {
					if (text->offset < 0 && (!expected_layout.is_valid() || text->layout.get_rid() == expected_layout)) {
						// Text transforms preserve code point counts, so the shaped offsets also address the source.
						const auto& source = node->get_source_text();
						const bool replaced = source == "\r" || (node->css().get_white_space_collapse() == white_space_preserve_spaces &&
							(source == "\t" || source == "\n"));
						text->offset = append(replaced ? String(" ") : String::utf8(source.c_str()));
						text->selectable = p_selectable;
						text->selection_owner = p_element->parent().get();
						return;
					}
				}
			}
			if (elided != elided_text_offsets.end()) append(String::utf8(node->get_source_text().c_str()));
			else if (p_unlaid) append_unpainted(*node);
			return;
		}
		const auto annotation_parent = p_element->tag() == litehtml::_rt_ ? p_element->parent() : element::ptr();
		const bool ruby_annotation = annotation_parent && annotation_parent->tag() == litehtml::_ruby_;
		const bool block = !ruby_annotation && !litehtml::is_one_of(display, display_inline, display_inline_block, display_inline_flex,
			display_inline_grid, display_inline_table, display_table_cell);
		const int64_t start = plain_text.length();
		const String preceding_separator = r_separator;
		const auto preceding_separator_selection = separator_selectable;
		if (block && start > 0 && !r_separator.begins_with("\n")) {
			r_separator = "\n";
			separator_selectable.reset();
		}
		const int64_t accessible_start = p_accessible ? p_accessible->text.length() : 0;
		const String accessible_separator = p_accessible ? p_accessible->separator : String();
		AccessibleTextIndex* child_accessibility = p_accessible;
		if (p_accessible) {
			if (block && accessible_start > 0 && !p_accessible->separator.begins_with("\n")) p_accessible->separator = "\n";
			if (visible) {
				const auto* tag = dynamic_cast<const html_tag*>(p_element.get());
				if (tag && tag->get_content_alternative()) {
					p_accessible->append(String::utf8(tag->get_content_alternative()->c_str()));
					child_accessibility = nullptr;
				} else if (p_element->tag() == _img_) {
					p_accessible->append(String::utf8(p_element->get_attr("alt", "")));
				}
			}
		}
		const bool has_scope = selection == css_user_select::none || selection == css_user_select::all;
		if (has_scope) selection_scope_starts.push_back(-1);
		const bool folded = p_element->tag() == litehtml::_details_ && !p_element->get_attr("open");
		const auto summary = folded ? details_summary(*p_element) : element::ptr();
		if (!p_element->css().get_content_image()) {
			for (const auto& child : p_element->children()) {
				if (folded && child != summary && !litehtml::is_generated_tag(child->tag())) continue;
				index_text(child, p_shape_text, r_separator, child_accessibility,
					p_unlaid || p_element->css().has_auto_content_visibility(), selectable);
			}
		}
		if (has_scope) {
			const int64_t scope_start = selection_scope_starts.back();
			selection_scope_starts.pop_back();
			if (scope_start >= 0) selection_scopes.emplace(p_element.get(), Vector2i(scope_start, plain_text.length()));
		}
		if (plain_text.length() > start) {
			if (p_element->css().has_auto_content_visibility()) {
				auto_content_text.emplace_back(p_element.get(), Vector2i(start, plain_text.length()));
			}
			if (block && !r_separator.begins_with("\n")) {
				r_separator = "\n";
				separator_selectable.reset();
			}
			else if (display == display_table_cell && !r_separator.begins_with("\n")) {
				r_separator = "\t";
				separator_selectable.reset();
			}
		} else if (block) {
			r_separator = preceding_separator;
			separator_selectable = preceding_separator_selection;
		}
		if (p_accessible) {
			if (p_accessible->text.length() == accessible_start && block) p_accessible->separator = accessible_separator;
			else if (!p_accessible->separator.begins_with("\n") && p_accessible->text.length() > accessible_start) {
				if (block) p_accessible->separator = "\n";
				else if (display == display_table_cell) p_accessible->separator = "\t";
			}
		}
	}

	const std::vector<HTMLPaintRun>& HTMLPaintRecorder::get_paint_runs() const {
		return paint_runs;
	}

	const String& HTMLPaintRecorder::get_plain_text() const {
		return plain_text;
	}

	String HTMLPaintRecorder::get_copyable_text(int64_t p_from, int64_t p_to) const {
		const int64_t end = CLAMP(p_to, int64_t(0), plain_text.length());
		int64_t offset = CLAMP(p_from, int64_t(0), end);
		String text;
		for (const Vector2i& range : excluded_selection) {
			if (range.y <= offset) continue;
			if (range.x >= end) break;
			if (range.x > offset) text += plain_text.substr(offset, range.x - offset);
			offset = std::min<int64_t>(end, range.y);
		}
		if (offset < end) text += plain_text.substr(offset, end - offset);
		return text;
	}

	bool HTMLPaintRecorder::has_selectable_text(int64_t p_from, int64_t p_to) const {
		if (p_from >= p_to) return false;
		const auto excluded = std::partition_point(excluded_selection.begin(), excluded_selection.end(),
			[p_from](const Vector2i& p_range) { return p_range.y <= p_from; });
		return excluded == excluded_selection.end() || excluded->x > p_from || excluded->y < p_to;
	}

	HTMLSelectionPoint HTMLPaintRecorder::selection_point_at(const Vector2& p_point, element::ptr p_target,
		const element* p_scope) const {
		HTMLSelectionPoint result{ hit_test_text(p_point, p_scope) };
		const auto scope_at = [&](const element& p_element) {
			const auto found = selection_scopes.find(&p_element);
			return found == selection_scopes.end() ? Vector2i() : found->second;
		};
		css_user_select used = css_user_select::automatic;
		bool crossed_override = false;
		for (auto node = std::move(p_target); node; node = node->parent()) {
			if (node->is_text()) continue;
			const auto specified = node->css().get_user_select();
			if (specified == css_user_select::automatic) continue;
			if (used == css_user_select::automatic) used = specified;
			if (specified != used) crossed_override = true;
			if (specified == css_user_select::all && crossed_override) {
				result.enclosing_all = scope_at(*node);
			} else if (!crossed_override && specified != css_user_select::text) {
				result.scope = scope_at(*node);
			}
		}
		result.excluded = used == css_user_select::none;
		if (result.scope.x < result.scope.y) {
			result.offset = CLAMP(result.offset, int64_t(result.scope.x), int64_t(result.scope.y));
		}
		return result;
	}

	bool HTMLPaintRecorder::hit_test_selectable_text(const Vector2& p_point, const element& p_target, bool* r_text_hit) const {
		if (r_text_hit) *r_text_hit = false;
		for (auto run = text_runs.rbegin(); run != text_runs.rend(); ++run) {
			if (run->length <= 0 || !run->selection_owner || !run->visible_rect.has_point(p_point)) continue;
			const Vector2 local = run->inverse.xform(p_point);
			if (!local.is_finite() || !run->rect.has_point(local)) continue;

			int remaining = MAX_ROUNDED_CLIPS;
			const bool clipped = std::any_of(run->clips.rbegin(), run->clips.rend(), [&](const auto& clip) {
				return !clip.contains_point(p_point, use_precise_clip(clip, remaining));
			});
			if (clipped) continue;
			bool matches = run->selection_owner == &p_target;
			for (auto owner = run->selection_owner->parent(); !matches && owner; owner = owner->parent()) {
				matches = owner.get() == &p_target;
			}
			if (!matches) continue;
			if (r_text_hit) *r_text_hit = true;
			return run->selectable;
		}
		return false;
	}

	namespace {
		bool text_belongs_to(const element* p_owner, const element& p_scope) {
			for (auto node = p_owner ? p_owner->shared_from_this() : nullptr; node; node = node->parent()) {
				if (node.get() == &p_scope) return true;
				if (const auto* tag = dynamic_cast<const html_tag*>(node.get()); tag && tag->has_pseudo_class(litehtml::_id("modal")))
					return false;
			}
			return false;
		}
	}

	Vector2i HTMLPaintRecorder::text_range_in(const element& p_scope) const {
		Vector2i offsets(plain_text.length(), 0);
		for (const TextRun& run : text_runs) {
			if (run.selectable && text_belongs_to(run.selection_owner, p_scope)) {
				offsets.x = std::min<int64_t>(offsets.x, run.offset);
				offsets.y = std::max<int64_t>(offsets.y, run.offset + run.length);
			}
		}
		return offsets.x < offsets.y ? offsets : Vector2i();
	}

	int64_t HTMLPaintRecorder::hit_test_text(const Vector2& p_point, const element* p_scope) const {
		const TextRun* closest = nullptr;
		real_t best_y = std::numeric_limits<real_t>::infinity();
		real_t best_x = best_y;
		real_t top = best_y;
		real_t bottom = -best_y;
		for (const TextRun& run : text_runs) {
			if (!run.visible_rect.has_area()) continue;
			if (p_scope && !text_belongs_to(run.selection_owner, *p_scope)) continue;
			const Rect2& rect = run.visible_rect;
			const Vector2 end = rect.get_end();
			top = std::min(top, rect.position.y);
			bottom = std::max(bottom, end.y);
			real_t dy = std::max({ rect.position.y - p_point.y, real_t(0), p_point.y - end.y });
			real_t dx = std::max({ rect.position.x - p_point.x, real_t(0), p_point.x - end.x });
			if ((run.matrix != Transform2D() || !run.clips.empty()) && run.visible_rect.has_point(p_point)) {
				const Vector2 local = run.inverse.xform(p_point);
				if (!local.is_finite()) continue;
				if (!run.rect.has_point(local)) {
					const Vector2 edge = local.clamp(run.rect.position, run.rect.get_end());
					const Vector2 delta = (run.matrix.xform(edge) - p_point).abs();
					if (delta.is_finite()) {
						dx = delta.x;
						dy = delta.y;
					}
				}
				int remaining = MAX_ROUNDED_CLIPS;
				for (auto clip = run.clips.rbegin(); clip != run.clips.rend(); ++clip) {
					if (!clip->contains_point(p_point, use_precise_clip(*clip, remaining))) {
						dy = std::max(dy, real_t(0.01));
					}
				}
			}
			if (dy < best_y || (dy == best_y && dx <= best_x)) {
				closest = &run;
				best_y = dy;
				best_x = dx;
			}
		}
		if (p_scope && (p_point.y < top || p_point.y > bottom)) {
			const Vector2i range = text_range_in(*p_scope);
			return p_point.y < top ? range.x : range.y;
		}
		if (!closest || p_point.y < top) {
			return 0;
		}
		if (p_point.y > bottom) {
			return plain_text.length();
		}
		const real_t x = CLAMP(p_point.x, closest->visible_rect.position.x, closest->visible_rect.get_end().x);
		const Vector2 point(x, CLAMP(p_point.y, closest->visible_rect.position.y, closest->visible_rect.get_end().y));
		const Vector2 local = to_local_point(closest->inverse.xform(point), closest->rect.position);
		if (!local.is_finite()) {
			return closest->offset + (x >= closest->visible_rect.get_center().x ? closest->length : 0);
		}
		if (closest->ellipsis_direction != 0) {
			const bool trailing = (local.x >= closest->rect.size.x * 0.5) == (closest->ellipsis_direction > 0);
			return closest->offset + (trailing ? closest->length : 0);
		}
		return closest->offset + CLAMP(closest->layout.hit_test(local.x), int64_t(0), closest->length);
	}

	TypedArray<Rect2> HTMLPaintRecorder::get_selection_rects(int64_t p_from, int64_t p_to) const {
		TypedArray<Rect2> result;
		const Ref<TextServer> text_server = TextServerManager::get_singleton()->get_primary_interface();
		if (text_server.is_null()) {
			return result;
		}
		for (const TextRun& run : text_runs) {
			const int64_t from = std::max<int64_t>(0, p_from - run.offset);
			const int64_t to = std::min(run.length, p_to - run.offset);
			if (from >= to || (run.ellipsis_direction != 0 && (from != 0 || to != run.length))) continue;
			for (const Vector2& range : run.layout.selection(*text_server.ptr(), from, run.ellipsis_direction ? 1 : to)) {
				const Rect2 rect = translate_rect(Rect2(range.x, 0, range.y - range.x, run.rect.size.y), run.rect.position);
				const Rect2 visible = run.clips.empty() ? run.matrix.xform(rect).intersection(run.visible_rect) :
					clipped_visual_bounds(rect, run.matrix, run.clips);
				if (visible.has_area()) {
					result.push_back(visible);
				}
			}
		}
		return result;
	}

	Ref<ShaderMaterial> HTMLPaintRecorder::create_material(const RoundedRect* p_shape, bool p_composite,
		const TransformedClip* p_path, bool p_include_clips) {
		auto material = make_html_paint_material(p_composite);
		PackedVector4Array boxes;
		PackedVector4Array radii_x;
		PackedVector4Array radii_y;
		PackedVector4Array bases;
		PackedVector2Array offsets;
		PackedVector4Array divisors;
		PackedInt32Array axes;
		PackedVector4Array polygons;
		std::vector<Vector4> edges;
		const auto append_polygon = [&](const std::shared_ptr<const HTMLClipPolygon>& p_polygon) {
			if (!p_polygon) return Vector4();
			const auto first = edges.size();
			const auto append_area = [&](const auto& self, const HTMLClipPolygon& area) -> void {
				// An empty fill also pushes a value, so intersections with empty areas evaluate to zero.
				if (area.outlines.empty()) edges.emplace_back();
				for (const auto& outline : area.outlines) {
					const auto header = edges.size();
					edges.emplace_back();
					outline.for_each_edge([&](const Vector2& a, const Vector2& b) {
						edges.emplace_back(a.x, a.y, b.x, b.y);
					});
					edges[header] = Vector4(edges.size() - header - 1, outline.even_odd ? 1.0 : 0.0, 0, 0);
					for (const auto& clip : outline.clips) {
						self(self, clip);
						edges.emplace_back(0.0f, 0.0f, 2.0f, 0.0f); // Intersection
					}
					if (&outline != &area.outlines.front()) edges.emplace_back(0.0f, 0.0f, 1.0f, 0.0f); // Union
				}
				for (const auto& clip : area.clips) {
					self(self, clip);
					edges.emplace_back(0.0f, 0.0f, 2.0f, 0.0f);
				}
			};
			append_area(append_area, *p_polygon);
			return Vector4(first, edges.size() - first, 0, 0);
		};
		const auto matrix = !p_composite && active_geometry ? active_geometry->matrix : HTMLPlaneTransform();
		bool transformed = false;
		const auto append = [&](const TransformedClip& p_clip) {
			boxes.push_back(box_uniform(p_clip.shape.rect));
			radii_x.push_back(p_clip.shape.radii_x);
			radii_y.push_back(p_clip.shape.radii_y);
			axes.push_back(static_cast<int>(p_clip.axes));
			polygons.push_back(append_polygon(p_clip.polygon));
			const auto relative = p_clip.matrix == matrix ? HTMLPlaneTransform() : p_clip.matrix.inverse() * matrix;
			transformed |= relative != HTMLPlaneTransform();
			bases.push_back(Vector4(relative.row(0).x, relative.row(1).x, relative.row(0).y, relative.row(1).y));
			offsets.push_back(Vector2(relative.row(0).z, relative.row(1).z));
			const auto& w = relative.row(2);
			divisors.push_back(Vector4(w.x, w.y, w.z, 0));
		};
		int remaining = MAX_ROUNDED_CLIPS;
		for (auto clip = clips.rbegin(); p_include_clips && clip != clips.rend() && boxes.size() < MAX_ROUNDED_CLIPS; ++clip) {
			if (use_precise_clip(*clip, remaining)) {
				append(*clip);
			}
		}
		if (p_shape) {
			material->set_shader_parameter("has_shape", true);
			material->set_shader_parameter("shape_box", box_uniform(p_shape->rect));
			material->set_shader_parameter("shape_radii_x", p_shape->radii_x);
			material->set_shader_parameter("shape_radii_y", p_shape->radii_y);
			if (p_path) {
				const auto relative = p_path->matrix.inverse();
				material->set_shader_parameter("shape_basis",
					Vector4(relative.row(0).x, relative.row(1).x, relative.row(0).y, relative.row(1).y));
				material->set_shader_parameter("shape_offset", Vector2(relative.row(0).z, relative.row(1).z));
				material->set_shader_parameter("shape_divisor", relative.row(2));
				if (p_path->polygon) material->set_shader_parameter("shape_polygon", append_polygon(p_path->polygon));
			}
		}
		if (!edges.empty()) {
			constexpr int columns = 1024;
			const int width = static_cast<int>(std::min<size_t>(edges.size(), columns));
			const int height = static_cast<int>((edges.size() + width - 1) / width);
			auto data = godot::Image::create_empty(width, height, false, godot::Image::FORMAT_RGBAF);
			for (size_t index = 0; index < edges.size(); ++index) {
				const auto& edge = edges[index];
				data->set_pixel(index % width, index / width, Color(edge.x, edge.y, edge.z, edge.w));
			}
			material->set_shader_parameter("clip_edges", ImageTexture::create_from_image(data));
			if (!polygons.is_empty()) {
				polygons.resize(MAX_ROUNDED_CLIPS);
				material->set_shader_parameter("has_clip_polygons", true);
				material->set_shader_parameter("clip_polygons", polygons);
			}
		}
		material->set_shader_parameter("clip_count", boxes.size());
		if (!boxes.is_empty()) {
			boxes.resize(MAX_ROUNDED_CLIPS);
			axes.resize(MAX_ROUNDED_CLIPS);
			material->set_shader_parameter("clip_axes", axes);
			radii_x.resize(MAX_ROUNDED_CLIPS);
			radii_y.resize(MAX_ROUNDED_CLIPS);
			material->set_shader_parameter("clip_boxes", boxes);
			material->set_shader_parameter("clip_radii_x", radii_x);
			material->set_shader_parameter("clip_radii_y", radii_y);
			if (transformed) {
				bases.resize(MAX_ROUNDED_CLIPS);
				offsets.resize(MAX_ROUNDED_CLIPS);
				divisors.resize(MAX_ROUNDED_CLIPS);
				material->set_shader_parameter("transformed_clips", true);
				material->set_shader_parameter("clip_bases", bases);
				material->set_shader_parameter("clip_offsets", offsets);
				material->set_shader_parameter("clip_divisors", divisors);
			}
		}
		return material;
	}

	void HTMLPaintRecorder::add_command(Command p_command, Ref<ShaderMaterial> p_material, HTMLTextureSampling p_sampling,
		bool p_fixed_background) {
		add_command_to(paint_runs, std::move(p_command), std::move(p_material), p_sampling, p_fixed_background);
	}

	void HTMLPaintRecorder::add_indexed_text(Text p_text, const element* p_source, const web_color& p_color) {
		const auto* fragment = dynamic_cast<const HTMLInlineTextFragment*>(p_source);
		if (fragment) p_source = fragment->original_renderer()->src_el().get();
		const auto parent = p_source && p_source->is_text() ? p_source->parent() : nullptr;
		const auto* tag = dynamic_cast<const html_tag*>(parent ? parent.get() : p_source);
		p_text.selection_owner = tag;
		if (tag) {
			const auto colors = tag->get_selection_colors(p_color);
			if (colors.background) p_text.selection_background = to_color(*colors.background);
			if (colors.foreground) {
				const Color ink = to_color(*colors.foreground);
				if (ink != p_text.color) p_text.selection_foreground = ink;
			}
		}
		// A partial highlight can give its glyphs their own material at submission time.
		const bool isolates_ink = p_text.selection_foreground.has_value();
		if (isolates_ink) separate_run = true;
		add_command(std::move(p_text));
		if (isolates_ink) separate_run = true;
	}

	void HTMLPaintRecorder::add_command_to(std::vector<HTMLPaintRun>& r_runs, Command p_command, Ref<ShaderMaterial> p_material,
		HTMLTextureSampling p_sampling, bool p_fixed_background) {
		if (active_geometry && !active_geometry->inverse) return;
		const auto matrix = active_geometry ? active_geometry->matrix : HTMLPlaneTransform();
		const auto depth = active_geometry ? active_geometry->depth_plane : std::nullopt;
		std::optional<Rect2> clip;
		if (!clips.empty()) {
			clip = clips.back().bounds;
			if (clip && !clip->has_area()) return;
			if (p_material.is_null()) {
				if (clips.back().has_precise_clip) {
					if (clip_material.is_null() || clip_material_matrix != matrix) {
						clip_material = create_material();
						clip_material_matrix = matrix;
					}
					p_material = clip_material;
				}
			}
		}
		const bool boundary = &r_runs == &paint_runs && std::exchange(separate_run, false);
		if (boundary || r_runs.empty() || r_runs.back().clip != clip || r_runs.back().matrix != matrix || r_runs.back().material != p_material ||
			r_runs.back().sampling != p_sampling || r_runs.back().fixed_background != p_fixed_background || r_runs.back().depth_plane != depth) {
			r_runs.push_back({ clip, matrix, {}, p_material, {}, p_sampling, p_fixed_background, depth });
			if (filter_depth > 0) {
				for (const auto& group : open_groups) {
					if (group.enclosing_clips) for (const auto& entry : *group.enclosing_clips) r_runs.back().clips.push_back(entry);
				}
			}
			const bool has_path = std::any_of(open_groups.begin(), open_groups.end(),
				[](const OpenPaintGroup& group) { return group.path_clip != nullptr; });
			if (has_path || filter_depth > 0 || matrix != Transform2D() || (!clips.empty() && clips.back().has_precise_clip)) {
				for (const auto& entry : clips) r_runs.back().clips.push_back(entry);
			}
			for (const auto& group : open_groups) {
				if (group.path_clip) r_runs.back().clips.push_back(*group.path_clip);
			}
		}
		r_runs.back().commands.push_back(std::move(p_command));
	}

	void HTMLPaintRecorder::fill_rect(const Rect2& p_rect, const Color& p_color) {
		if (p_rect.has_area() && p_color.a > 0.0f) {
			add_command(Fill{ p_rect, p_color });
		}
	}

	void HTMLPaintRecorder::draw_decoration(const Rect2& p_rect, text_decoration_style p_style, const Color& p_color) {
		if (!p_rect.has_area() || !p_rect.is_finite() || p_color.a <= 0.0f) {
			return;
		}
		int pattern;
		switch (p_style) {
			case text_decoration_style_dotted: pattern = 1; break;
			case text_decoration_style_dashed: pattern = 2; break;
			case text_decoration_style_wavy: pattern = 3; break;
			default:
				fill_rect(p_rect, p_color);
				if (p_style == text_decoration_style_double) {
					fill_rect(Rect2(p_rect.position + Vector2(0, p_rect.size.y * 2), p_rect.size), p_color);
				}
				return;
		}
		const auto material = create_material();
		material->set_shader_parameter("decoration_pattern", pattern);
		material->set_shader_parameter("decoration_thickness", p_rect.size.y);
		material->set_shader_parameter("decoration_center", p_rect.get_center().y);
		const real_t margin = pattern == 3 ? p_rect.size.y * 1.5 : 1.0;
		add_command(Fill{ p_rect.grow_side(SIDE_TOP, margin).grow_side(SIDE_BOTTOM, margin), p_color }, material);
	}

	bool HTMLPaintRecorder::push_content_clip(const Rect2& p_box) {
		if (!active_geometry || !active_geometry->inverse) return false;
		const auto& boxes = active_geometry->fragments;
		push_clip(boxes.size() == 1 ? boxes.front().inset(p_box) : RoundedRect(p_box, {}), active_geometry->matrix);
		return true;
	}

	void HTMLPaintRecorder::draw_image_alt(const Rect2& p_box, const std::vector<HTMLTextLayout>& p_lines,
		float p_line_height, web_color p_color, text_align p_align) {
		if (!push_content_clip(p_box)) return;
		const Color color = to_color(p_color);
		float top = 0;
		for (const auto& line : p_lines) {
			if (top >= p_box.size.y) break;
			const float free = std::max(0.0f, float(p_box.size.x) - line.width());
			const float x = p_align == text_align_center ? free * 0.5f : p_align == text_align_right ? free : 0.0f;
			const float leading = (p_line_height - line.line_ascent() - line.line_descent()) * 0.5f;
			add_command(Text{line, p_box.position + Vector2(x, top + leading), color});
			top += p_line_height;
		}
		del_clip();
	}

	void HTMLPaintRecorder::fill_rounded_color(const Rect2& p_rect, const Color& p_color, float p_radius) {
		if (!p_rect.has_area() || p_color.a == 0) return;
		if (p_radius <= 0) {
			fill_rect(p_rect, p_color);
			return;
		}
		border_radiuses radii;
		radii.top_left_x = radii.top_left_y = radii.top_right_x = radii.top_right_y = p_radius;
		radii.bottom_left_x = radii.bottom_left_y = radii.bottom_right_x = radii.bottom_right_y = p_radius;
		push_clip(RoundedRect(p_rect, radii), active_geometry ? active_geometry->matrix : Transform2D());
		fill_rect(p_rect, p_color);
		del_clip();
	}

	Ref<Texture2D> HTMLPaintRecorder::get_control_icon(HTMLControlIcon p_icon) {
		static constexpr std::array svg{control_icons::play_arrow, control_icons::pause, control_icons::volume_up,
			control_icons::volume_off, control_icons::resize_window, control_icons::fullscreen, control_icons::fullscreen_exit,
			control_icons::arrow_drop_down, control_icons::arrow_drop_up, control_icons::arrow_left, control_icons::arrow_right,
			control_icons::radio_button_checked, control_icons::calendar_today, control_icons::schedule,
			control_icons::colorize, control_icons::unfold_more, control_icons::radio_button_unchecked,
			control_icons::check_box, control_icons::check_box_outline_blank, control_icons::indeterminate_check_box,
			control_icons::close_small};
		static_assert(svg.size() == static_cast<size_t>(HTMLControlIcon::count));
		const auto index = static_cast<size_t>(p_icon);
		auto& texture = control_icon_textures[index];
		if (texture.is_null()) {
			Ref<godot::Image> image;
			image.instantiate();
			// Keep the SVG's alpha and use the computed control color for RGB.
			if (image->load_svg_from_string(String::utf8(svg[index]), 3.0f) != OK || image->is_empty()) return {};
			image->convert(godot::Image::FORMAT_RGBA8);
			auto pixels = image->get_data();
			auto* bytes = pixels.ptrw();
			for (int64_t offset = 0; offset < pixels.size(); offset += 4) {
				bytes[offset] = bytes[offset + 1] = bytes[offset + 2] = 255;
			}
			image->set_data(image->get_width(), image->get_height(), false, godot::Image::FORMAT_RGBA8, pixels);
			if (p_icon == HTMLControlIcon::resize) image->flip_x();
			texture = ImageTexture::create_from_image(image);
		}
		return texture;
	}

	void HTMLPaintRecorder::draw_input_icon(HTMLControlIcon p_icon, const Rect2& p_clip, const Rect2& p_rect, const Color& p_color) {
		const auto texture = get_control_icon(p_icon);
		if (texture.is_null() || !p_rect.has_area() || !push_content_clip(p_clip)) return;
		add_image(Image{texture, p_rect, Rect2(Vector2(), texture->get_size()), p_color});
		del_clip();
	}

	void HTMLPaintRecorder::draw_resize_grip(const Rect2& p_clip, const Rect2& p_grip) {
		const auto texture = get_control_icon(HTMLControlIcon::resize);
		if (texture.is_null() || !p_grip.has_area() || !push_content_clip(p_clip)) return;
		// Chromium pairs translucent black strokes with a white highlight one pixel below.
		const Rect2 source(Vector2(), texture->get_size());
		add_image(Image{texture, p_grip, source, Color(0, 0, 0, 0.6f)});
		add_image(Image{texture, Rect2(p_grip.position + Vector2(0, 1), p_grip.size), source, Color(1, 1, 1, 0.6f)});
		del_clip();
	}

	void HTMLPaintRecorder::draw_input_chrome(const Rect2& p_clip, const Rect2& p_rect, const Color& p_color, float p_radius) {
		if (!push_content_clip(p_clip)) return;
		fill_rounded_color(p_rect, p_color, p_radius);
		del_clip();
	}

	void HTMLPaintRecorder::draw_input(const Rect2& p_box, const HTMLTextLayout& p_text, const Vector2& p_origin,
		const web_color& p_color, const PackedVector2Array& p_selection, const PackedVector2Array& p_preedit,
		const Rect2& p_caret, float p_ascent, float p_descent, const HTMLInputPaintColors& p_colors) {
		if (!push_content_clip(p_box)) return;
		const Color color = to_color(p_color);
		const Color selection_background = p_colors.selection_background ? to_color(*p_colors.selection_background, p_color) :
			Color(0.25f, 0.55f, 0.95f, 0.5f);
		for (const Vector2 range : p_selection) {
			fill_rect(Rect2(p_origin.x + range.x, p_origin.y - p_ascent, range.y - range.x, p_ascent + p_descent),
				selection_background);
		}
		if (p_text.is_valid()) {
			const Vector2 top_left = p_origin - Vector2(0, p_text.line_ascent());
			if (p_colors.selection_foreground && !p_selection.is_empty()) {
				std::vector<Vector2> ranges;
				for (const Vector2 range : p_selection)
					ranges.emplace_back(std::max(p_box.position.x, p_origin.x + range.x),
						std::min(p_box.get_end().x, p_origin.x + range.y));
				std::sort(ranges.begin(), ranges.end(), [](Vector2 a, Vector2 b) { return a.x < b.x; });
				const auto matrix = active_geometry ? active_geometry->matrix : Transform2D();
				const auto paint_slice = [&](real_t begin, real_t end, const Color& ink) {
					if (end <= begin || ink.a <= 0) return;
					push_clip(RoundedRect(Rect2(begin, p_box.position.y, end - begin, p_box.size.y), {}), matrix);
					add_command(Text{p_text, top_left, ink});
					del_clip();
				};
				const Color selection_ink = to_color(*p_colors.selection_foreground, p_color);
				real_t left = p_box.position.x;
				for (const Vector2 range : ranges) {
					if (range.x >= range.y || range.y <= left) continue;
					paint_slice(left, range.x, color);
					paint_slice(std::max(left, range.x), range.y, selection_ink);
					left = range.y;
				}
				paint_slice(left, p_box.get_end().x, color);
			} else {
				add_command(Text{p_text, top_left, color});
			}
		}
		for (const Vector2 range : p_preedit) {
			fill_rect(Rect2(p_origin.x + range.x, p_origin.y + 1.0f, range.y - range.x, 1.0f), color);
		}
		fill_rect(p_caret, p_colors.caret ? to_color(*p_colors.caret, p_color) : color);
		del_clip();
	}

	void HTMLPaintRecorder::draw_text(const HTMLShapedText& p_shaped, int64_t p_length, web_color p_color,
		const litehtml::position& p_position, const std::vector<box_shadow>& p_shadows, const css_underline& p_underline) {
		const auto& layout = p_shaped.layout;
		const Vector2 top_left(p_position.x, p_position.y + layout.ascent - layout.line_ascent());
		const Color color = to_color(p_color);
		Text text{ layout, top_left, color, -1, p_length,
			active_geometry && active_geometry->item->css().get_display() == display_inline_text ? active_geometry->item.get() : nullptr };
		const element* source = text.renderer ? text.renderer->src_el().get() : nullptr;

		const int decoration = p_shaped.decoration_line;
		if (decoration == text_decoration_line_none && p_shadows.empty() && !p_shaped.emphasis_centers) {
			add_indexed_text(std::move(text), source, p_color);
			return;
		}
		const float thickness = p_shaped.decoration_thickness;
		const float width = layout.width();
		const float baseline = top_left.y + layout.line_ascent();
		const web_color css_color = p_shaped.decoration_color;
		const Color decoration_color = to_color(css_color, p_color);
		std::vector<TextDecorationMask> decorations;
		const auto add_decoration = [&](int p_flag, real_t p_top) {
			const Rect2 rect(top_left.x, p_top, width, thickness);
			if ((decoration & p_flag) && rect.has_area() && rect.is_finite() && decoration_color.a > 0.0f) {
				decorations.push_back({ rect, p_shaped.decoration_style, decoration_color.a });
			}
		};
		if (decoration != text_decoration_line_none) {
			real_t underline = p_underline.offset.value_or(layout.underline_position());
			if (p_underline.position == underline_from_font) {
				underline = p_underline.font_position + p_underline.offset.value_or(0.0f);
			}
			else if (p_underline.position & underline_under) {
				underline = layout.line_descent() + p_underline.offset.value_or(0.0f);
			}
			add_decoration(text_decoration_line_underline, baseline + underline);
			add_decoration(text_decoration_line_overline, top_left.y);
			add_decoration(text_decoration_line_line_through, baseline - layout.line_ascent() / 3.0f);
		}
		std::vector<TextEmphasisMask> marks;
		const Color emphasis_color = to_color(p_shaped.emphasis_color, p_color);
		if (p_shaped.emphasis_centers && emphasis_color.a > 0.0f) {
			const auto& mark = p_shaped.emphasis;
			const auto size = mark->get_size();
			real_t y = p_shaped.emphasis_under ? baseline + layout.line_descent() : top_left.y;
			for (const auto& rule : decorations) {
				if (p_shaped.emphasis_under && rule.rect.position.y >= baseline) y = std::max(y, rule.rect.get_end().y + 1.0f);
				else if (!p_shaped.emphasis_under && rule.rect.position.y <= top_left.y) y = std::min(y, rule.rect.position.y - 1.0f);
			}
			if (!p_shaped.emphasis_under) y -= size.y;
			marks.reserve(p_shaped.emphasis_centers->size());
			for (const float native : *p_shaped.emphasis_centers) {
				const float center = layout.spacing ? layout.spacing->from_native(native) : native;
				marks.push_back({ mark, Vector2(top_left.x + center - size.x / 2.0f, y), emphasis_color.a });
			}
		}
		HTMLTextMask mask;
		const bool mipmaps = std::any_of(p_shadows.begin(), p_shadows.end(), [](const box_shadow& p_shadow) {
			return p_shadow.blur.val() > 3.0f;
		});
		for (auto shadow = p_shadows.rbegin(); shadow != p_shadows.rend(); ++shadow) {
			const Color shadow_color = to_color(shadow->color, p_color);
			const Vector2 offset(shadow->x.val(), shadow->y.val());
			const float blur = shadow->blur.val();
			if (shadow_color.a <= 0.0f || !offset.is_finite() || !std::isfinite(blur)) {
				continue;
			}
			if (mask.texture.is_null()) {
				mask = create_html_text_mask(layout, top_left, decorations, marks, mipmaps);
				if (mask.texture.is_null()) {
					break;
				}
			}
			const Rect2 box(mask.bounds.position + offset, mask.bounds.size);
			Rect2 destination = box.grow(blur * (4.0f / 3.0f));
			if (!destination.is_finite()) {
				continue;
			}
			if (!clips.empty() && clips.back().bounds) {
				const auto inverse = active_geometry && active_geometry->inverse ? *active_geometry->inverse : HTMLPlaneTransform();
				destination = destination.intersection(inverse.xform(*clips.back().bounds));
			}
			if (!destination.has_area()) {
				continue;
			}
			const auto material = create_material();
			material->set_shader_parameter("shadow_type", 3);
			material->set_shader_parameter("shadow_blur", blur);
			material->set_shader_parameter("text_shadow_mask", mask.texture);
			material->set_shader_parameter("text_shadow_box", Vector4(box.position.x, box.position.y, box.size.x, box.size.y));
			add_command(Fill{ destination, shadow_color }, material);
		}
		add_indexed_text(std::move(text), source, p_color);
		for (const auto& line_decoration : decorations) {
			draw_decoration(line_decoration.rect, line_decoration.style, decoration_color);
		}
		for (const auto& mark : marks) {
			add_command(Text{ HTMLTextLayout{ mark.line }, mark.position, emphasis_color });
		}
	}

	void HTMLPaintRecorder::draw_list_marker(const list_marker& p_marker, const Ref<Texture2D>& p_texture) {
		const Rect2 rect = to_rect(p_marker.pos);
		const Color color = to_color(p_marker.color);
		if (p_texture.is_valid()) {
			add_image(Image{ p_texture, rect, Rect2(Vector2(), p_texture->get_size()) });
			return;
		}
		if (p_marker.marker_type == list_style_type_square) {
			fill_rect(rect, color);
		}
		else if (p_marker.marker_type == list_style_type_disc || p_marker.marker_type == list_style_type_circle) {
			const float radius = std::min(rect.size.x, rect.size.y) / 2.0f;
			add_command(Circle{ rect.get_center(), radius, color, p_marker.marker_type == list_style_type_disc });
		}
	}

	image_rendering HTMLPaintRecorder::get_image_sampling() const {
		return active_geometry ? active_geometry->item->css().get_image_rendering() : image_rendering::automatic;
	}

	void HTMLPaintRecorder::add_image(Image p_image, const RoundedRect* p_shape, const Vector2i& p_repeat,
		Ref<ShaderMaterial> p_material, bool p_fixed_background) {
		const auto mode = get_image_sampling();
		HTMLTextureSampling sampling{ HTMLTextureFilter::inherited, p_repeat != Vector2i() };
		switch (mode) {
			case image_rendering::automatic: break;
			case image_rendering::crisp_edges:
			case image_rendering::optimize_speed: sampling.filter = HTMLTextureFilter::nearest; break;
			case image_rendering::high_quality: sampling.filter = HTMLTextureFilter::linear_mipmaps; break;
			case image_rendering::smooth:
			case image_rendering::optimize_quality:
			case image_rendering::pixelated: sampling.filter = HTMLTextureFilter::linear; break;
		}
		if (mode == image_rendering::pixelated && p_material.is_valid()) {
			p_material->set_shader_parameter("pixelated", true);
		}
		else if (mode == image_rendering::pixelated) {
			const Rect2 source = image_sample_bounds(p_image.texture);
			if (!source.has_area()) return;
			p_material = create_material(p_shape);
			p_material->set_shader_parameter("pixelated", true);
			p_material->set_shader_parameter("pixelated_source", Vector4(source.position.x, source.position.y, source.size.x, source.size.y));
			p_material->set_shader_parameter("pixelated_repeat", p_repeat);
		}
		else if (p_shape) {
			p_material = create_material(p_shape);
		}
		add_command(std::move(p_image), std::move(p_material), sampling, p_fixed_background);
	}

	void HTMLPaintRecorder::draw_image(const background_layer& p_layer, const Ref<Texture2D>& p_texture) {
		if (p_texture.is_null()) return;
		const BackgroundTiles tiles(p_layer);
		const Rect2 bounds = get_background_clip(p_layer);
		Rect2 destination = tiles.bounds(bounds);
		if (!destination.has_area()) return;

		const RoundedRect shape = RoundedRect(to_rect(p_layer.border_box), p_layer.border_radius).inset(bounds);
		const auto* rounded = shape.has_radius() ? &shape : nullptr;
		const Vector2 texture_size = p_texture->get_size();
		if (tiles.fixed || tiles.spacing != Vector2()) {
			const auto material = create_material(rounded);
			material->set_shader_parameter("tiled_image", true);
			material->set_shader_parameter("tile_flip_image_x", p_layer.flip_image_x);
			set_tile_uniforms(material, tiles.tile, tiles.tile.size + tiles.spacing, tiles.repeat_axes);
			add_image(Image{ p_texture, destination, Rect2(Vector2(), texture_size) }, nullptr, {}, material, tiles.fixed);
			return;
		}

		const Vector2 scale = texture_size / tiles.tile.size;
		Rect2 source((destination.position - tiles.tile.position) * scale, destination.size * scale);
		if (p_layer.flip_image_x) {
			source.position.x = texture_size.x - source.get_end().x;
			destination.size.x = -destination.size.x;
		}
		add_image(Image{ p_texture, destination, source }, rounded, tiles.repeat_axes);
	}

	void HTMLPaintRecorder::draw_solid_fill(const background_layer& p_layer, const web_color& p_color) {
		const Rect2 bounds = get_background_clip(p_layer);
		const Color color = to_color(p_color);
		if (!bounds.has_area() || color.a <= 0) {
			return;
		}
		const RoundedRect shape = RoundedRect(to_rect(p_layer.border_box), p_layer.border_radius).inset(bounds);
		const auto* rounded = shape.has_radius() ? &shape : nullptr;
		Rect2 destination = bounds;
		Ref<ShaderMaterial> material;
		const bool fixed = p_layer.fill_image_tile && p_layer.attachment == background_attachment_fixed;
		if (p_layer.fill_image_tile) {
			const BackgroundTiles tiles(p_layer);
			destination = tiles.bounds(bounds);
			if (!destination.has_area()) return;
			if (fixed || tiles.spacing != Vector2()) {
				material = create_material(rounded);
				material->set_shader_parameter("tiled_fill", true);
				set_tile_uniforms(material, tiles.tile, tiles.tile.size + tiles.spacing, tiles.repeat_axes);
			}
		}
		if (material.is_null() && rounded) material = create_material(rounded);
		add_command(Fill{ destination, color }, material, {}, fixed);
	}

	void HTMLPaintRecorder::draw_gradient(const background_layer& p_layer, const background_layer::gradient_base& p_gradient,
		int p_type, const Vector2& p_start, const Vector2& p_parameters) {
		const BackgroundTiles tiles(p_layer);
		const Rect2 bounds = get_background_clip(p_layer);
		const Rect2 destination = tiles.bounds(bounds);
		const auto& points = p_gradient.color_points;
		if (!destination.has_area() || !p_start.is_finite() || !p_parameters.is_finite() ||
			points.empty() || points.size() > 4096) {
			return;
		}
		const int count = static_cast<int>(points.size());
		Ref<godot::Image> stops = godot::Image::create_empty(count, 2, false, godot::Image::FORMAT_RGBAF);
		float offset = points.front().offset;
		for (int i = 0; i < count; ++i) {
			if (!std::isfinite(points[i].offset)) {
				return;
			}
			offset = std::max(offset, points[i].offset);
			const float span = i + 1 < count ? points[i + 1].offset - offset : 0.0f;
			const float hint = points[i].hint && span > 0 ? (*points[i].hint - offset) / span : 0.0f;
			stops->set_pixel(i, 0, to_color(points[i].color));
			stops->set_pixel(i, 1, Color(offset, hint, 0, 0));
		}
		const Ref<ImageTexture> texture = ImageTexture::create_from_image(stops);
		const RoundedRect shape = RoundedRect(to_rect(p_layer.border_box), p_layer.border_radius).inset(bounds);
		const Ref<ShaderMaterial> material = create_material(shape.has_radius() ? &shape : nullptr);
		material->set_shader_parameter("gradient_type", p_type);
		material->set_shader_parameter("stop_count", count);
		material->set_shader_parameter("gradient_stops", texture);
		material->set_shader_parameter("gradient_start", p_start);
		material->set_shader_parameter("gradient_parameters", p_parameters);
		set_tile_uniforms(material, tiles.tile, tiles.tile.size + tiles.spacing, tiles.repeat_axes);
		add_command(Fill{ destination, Color(1, 1, 1) }, material, {}, tiles.fixed);
	}

	void HTMLPaintRecorder::draw_linear_gradient(const background_layer& p_layer, const background_layer::linear_gradient& p_gradient) {
		draw_gradient(p_layer, p_gradient, 1, Vector2(p_gradient.start.x, p_gradient.start.y),
			Vector2(p_gradient.end.x - p_gradient.start.x, p_gradient.end.y - p_gradient.start.y));
	}

	void HTMLPaintRecorder::draw_radial_gradient(const background_layer& p_layer, const background_layer::radial_gradient& p_gradient) {
		draw_gradient(p_layer, p_gradient, 2, Vector2(p_gradient.position.x, p_gradient.position.y),
			Vector2(p_gradient.radius.x, p_gradient.radius.y));
	}

	void HTMLPaintRecorder::draw_conic_gradient(const background_layer& p_layer, const background_layer::conic_gradient& p_gradient) {
		draw_gradient(p_layer, p_gradient, 3, Vector2(p_gradient.position.x, p_gradient.position.y), Vector2(p_gradient.angle / 360.0f, 0));
	}

	void HTMLPaintRecorder::draw_box_shadows(const html_tag& p_element, const litehtml::position& p_box,
		const borders& p_borders, bool p_inset, const std::vector<box_shadow>& p_shadows) {
		const RoundedRect outer(to_rect(p_box), p_borders.radius);
		const Vector2 leading(p_borders.left.width, p_borders.top.width);
		const Vector2 trailing(p_borders.right.width, p_borders.bottom.width);
		const RoundedRect base = p_inset ? outer.inset(Rect2(outer.rect.position + leading, outer.rect.size - leading - trailing)) : outer;
		if (p_inset && !base.rect.has_area()) {
			return;
		}
		const auto& css = p_element.css();
		const auto doc = p_element.get_document();
		const auto pixels = [&](const css_length& p_length) { return doc->to_pixels(p_length, css.get_font_metrics(), 0); };
		for (auto shadow = p_shadows.rbegin(); shadow != p_shadows.rend(); ++shadow) {
			if (shadow->inset != p_inset) {
				continue;
			}
			const float blur = pixels(shadow->blur);
			const float spread = pixels(shadow->spread);
			const Vector2 offset(pixels(shadow->x), pixels(shadow->y));
			if (!offset.is_finite() || !std::isfinite(blur) || !std::isfinite(spread) ||
					(blur == 0.0f && spread == 0.0f && offset == Vector2())) {
				continue;
			}
			RoundedRect shape = base.spread(p_inset ? -spread : spread);
			shape.rect.position += offset;
			Rect2 destination = p_inset ? base.rect : shape.rect.grow(blur * 1.75f);
			const Color color = to_color(shadow->color, css.get_color());
			if (color.a <= 0.0f || !shape.rect.is_finite() || !destination.is_finite() || (!p_inset && !shape.rect.has_area())) {
				continue;
			}
			if (!clips.empty() && clips.back().bounds) {
				const auto inverse = active_geometry && active_geometry->inverse ? *active_geometry->inverse : HTMLPlaneTransform();
				destination = destination.intersection(inverse.xform(*clips.back().bounds));
			}
			if (!destination.has_area()) {
				continue;
			}
			const Ref<ShaderMaterial> material = create_material(p_inset ? &base : nullptr);
			material->set_shader_parameter("shadow_type", p_inset ? 2 : 1);
			material->set_shader_parameter("shadow_box", box_uniform(shape.rect));
			material->set_shader_parameter("shadow_rx", shape.radii_x);
			material->set_shader_parameter("shadow_ry", shape.radii_y);
			material->set_shader_parameter("shadow_blur", blur);
			if (!p_inset) {
				material->set_shader_parameter("inner_box", box_uniform(base.rect));
				material->set_shader_parameter("inner_radii_x", base.radii_x);
				material->set_shader_parameter("inner_radii_y", base.radii_y);
			}
			add_command(Fill{ destination, color }, material);
		}
	}

	void HTMLPaintRecorder::draw_border_image(const Ref<Texture2D>& p_texture,
		const std::vector<HTMLBorderImagePatch>& p_patches, const Rect2& p_visible, bool p_flip_x) {
		for (const auto& patch : p_patches) {
			const Rect2 destination = patch.destination.intersection(p_visible);
			if (!destination.has_area() || !patch.destination.is_finite() || !patch.tile.is_finite() || !patch.period.is_finite() ||
				(!clips.empty() && clips.back().bounds && !clips.back().bounds->intersects(
					active_geometry ? active_geometry->matrix.xform(destination) : destination))) continue;
			const auto material = create_material();
			material->set_shader_parameter("tiled_image", true);
			material->set_shader_parameter("tile_flip_image_x", p_flip_x);
			set_tile_uniforms(material, Rect2(patch.origin, patch.tile), patch.period);
			add_image(Image{ p_texture, destination, patch.source }, nullptr, {}, material);
		}
	}

	void HTMLPaintRecorder::draw_borders(const borders& p_borders, const litehtml::position& p_position) {
		add_borders(paint_runs, p_borders, to_rect(p_position));
	}

	void HTMLPaintRecorder::add_borders(std::vector<HTMLPaintRun>& r_runs, const borders& p_borders, const Rect2& rect) {
		if (!rect.has_area()) {
			return;
		}
		const std::array<const border*, 4> sides{ &p_borders.top, &p_borders.right, &p_borders.bottom, &p_borders.left };
		std::array<BorderPaint, 4> styles;
		for (int side = 0; side < 4; ++side) {
			styles[side] = sides[side]->width > 0.0f ? get_border_paint(sides[side]->style) : BORDER_NONE;
		}
		const auto width = [&](int p_side, float p_maximum) {
			return styles[p_side] == BORDER_NONE ? 0.0f : std::clamp(sides[p_side]->width, 0.0f, p_maximum);
		};
		const float top = width(0, rect.size.y);
		const float bottom = width(2, rect.size.y - top);
		const float left = width(3, rect.size.x);
		const float right = width(1, rect.size.x - left);
		if (top + right + bottom + left == 0.0f) {
			return;
		}
		const Vector4 widths(top, right, bottom, left);
		for (int side = 0; side < 4; ++side) {
			if (widths[side] == 0.0f) {
				styles[side] = BORDER_NONE;
			}
		}
		const float inner_height = rect.size.y - top - bottom;
		const Vector2 end = rect.get_end();
		const RoundedRect outer(rect, p_borders.radius);
		const bool patterned = std::any_of(styles.begin(), styles.end(),
			[](BorderPaint p_style) { return p_style == BORDER_DOTTED || p_style == BORDER_DASHED; });
		const bool double_line = std::find(styles.begin(), styles.end(), BORDER_DOUBLE) != styles.end();
		const bool bevel_bands = std::find(styles.begin(), styles.end(), BORDER_BEVEL_BANDS) != styles.end();
		const bool bevel = bevel_bands || std::find(styles.begin(), styles.end(), BORDER_BEVEL_SOLID) != styles.end();
		const bool solid = std::find(styles.begin(), styles.end(), BORDER_SOLID) != styles.end();
		if (outer.has_radius() || patterned || double_line || bevel || solid) {
			const auto inset = [&](float p_fraction) {
				const Vector2 leading(left * p_fraction, top * p_fraction);
				const Vector2 trailing(right * p_fraction, bottom * p_fraction);
				return outer.inset(Rect2(rect.position + leading, rect.size - leading - trailing));
			};
			const RoundedRect inner = inset(1.0f);
			const Ref<ShaderMaterial> material = create_material(&outer);
			material->set_shader_parameter("draw_border", true);
			material->set_shader_parameter("inner_box", box_uniform(inner.rect));
			material->set_shader_parameter("inner_radii_x", inner.radii_x);
			material->set_shader_parameter("inner_radii_y", inner.radii_y);
			material->set_shader_parameter("border_widths", widths);
			if (patterned || double_line || bevel) {
				material->set_shader_parameter("border_styles", Vector4i(styles[0], styles[1], styles[2], styles[3]));
			}
			if (bevel_bands) {
				const RoundedRect middle = inset(0.5f);
				material->set_shader_parameter("border_middle_box", box_uniform(middle.rect));
				material->set_shader_parameter("border_middle_rx", middle.radii_x);
				material->set_shader_parameter("border_middle_ry", middle.radii_y);
			}
			if (double_line) {
				const RoundedRect gap_outer = inset(1.0f / 3.0f);
				const RoundedRect gap_inner = inset(2.0f / 3.0f);
				material->set_shader_parameter("double_outer_box", box_uniform(gap_outer.rect));
				material->set_shader_parameter("double_outer_rx", gap_outer.radii_x);
				material->set_shader_parameter("double_outer_ry", gap_outer.radii_y);
				material->set_shader_parameter("double_inner_box", box_uniform(gap_inner.rect));
				material->set_shader_parameter("double_inner_rx", gap_inner.radii_x);
				material->set_shader_parameter("double_inner_ry", gap_inner.radii_y);
			}
			if (patterned) {
				const RoundedRect center = inset(0.5f);
				const auto& rx = center.radii_x;
				const auto& ry = center.radii_y;
				const Vector4 edges = Vector4(center.rect.size.x - rx.x - rx.y, center.rect.size.y - ry.y - ry.z,
					center.rect.size.x - rx.z - rx.w, center.rect.size.y - ry.w - ry.x).max(Vector4());
				const Vector4 arcs(quarter_ellipse_length(rx.y, ry.y), quarter_ellipse_length(rx.z, ry.z),
					quarter_ellipse_length(rx.w, ry.w), quarter_ellipse_length(rx.x, ry.x));
				Vector4 starts;
				float perimeter = 0.0f;
				for (int side = 0; side < 4; ++side) {
					starts[side] = perimeter;
					perimeter += edges[side] + arcs[side];
				}
				const bool closed = outer.has_radius() && top > 0.0f && widths == Vector4(top, top, top, top) &&
					std::all_of(styles.begin() + 1, styles.end(), [&](BorderPaint p_style) { return p_style == styles[0]; }) &&
					std::all_of(sides.begin() + 1, sides.end(), [&](const border* p_side) { return p_side->color == sides[0]->color; });
				Vector4 periods;
				Vector4 origins;
				for (int side = 0; side < 4; ++side) {
					if (styles[side] == BORDER_DOTTED || styles[side] == BORDER_DASHED) {
						const int previous = (side + 3) % 4;
						const float preceding = arcs[previous] * 0.5f;
						const float length = preceding + edges[side] + arcs[side] * 0.5f;
						periods[side] = border_pattern_period(closed ? perimeter : length, widths[side], styles[side]);
						origins[side] = closed ? 0.0f : starts[side] - preceding;
					}
				}
				material->set_shader_parameter("border_center_box", box_uniform(center.rect));
				material->set_shader_parameter("border_center_rx", rx);
				material->set_shader_parameter("border_center_ry", ry);
				material->set_shader_parameter("border_edge_starts", starts);
				material->set_shader_parameter("border_edge_lengths", edges);
				material->set_shader_parameter("border_arc_lengths", arcs);
				material->set_shader_parameter("border_periods", periods);
				material->set_shader_parameter("border_pattern_starts", origins);
				material->set_shader_parameter("border_pattern_continuous", closed);
			}
			PackedVector4Array outer_colors;
			PackedVector4Array inner_colors;
			for (int side = 0; side < 4; ++side) {
				const Color color = to_color(sides[side]->color);
				Vector4 outer_color(color.r, color.g, color.b, color.a);
				Vector4 inner_color = outer_color;
				if (styles[side] == BORDER_BEVEL_BANDS || styles[side] == BORDER_BEVEL_SOLID) {
					const Vector4 light = outer_color.lerp(Vector4(1, 1, 1, color.a), 0.35f);
					const Vector4 dark = outer_color.lerp(Vector4(0, 0, 0, color.a), 0.35f);
					const bool top_or_left = side == 0 || side == 3;
					const bool sunken = sides[side]->style == border_style_groove || sides[side]->style == border_style_inset;
					outer_color = top_or_left == sunken ? dark : light;
					inner_color = top_or_left == sunken ? light : dark;
				}
				outer_colors.append(outer_color);
				if (bevel_bands) inner_colors.append(inner_color);
			}
			material->set_shader_parameter("border_colors", outer_colors);
			if (bevel_bands) material->set_shader_parameter("border_inner_colors", inner_colors);
			add_command_to(r_runs, Fill{ rect, Color(1, 1, 1) }, material);
			return;
		}
		const auto fill = [&](const Rect2& p_stripe, const web_color& p_color) {
			const Color color = to_color(p_color);
			if (p_stripe.has_area() && color.a > 0) add_command_to(r_runs, Fill{ p_stripe, color });
		};
		fill(Rect2(rect.position, Vector2(rect.size.x, top)), p_borders.top.color);
		fill(Rect2(rect.position.x, end.y - bottom, rect.size.x, bottom), p_borders.bottom.color);
		fill(Rect2(rect.position.x, rect.position.y + top, left, inner_height), p_borders.left.color);
		fill(Rect2(end.x - right, rect.position.y + top, right, inner_height), p_borders.right.color);
	}

	void HTMLPaintRecorder::set_clip(const litehtml::position& p_position, const border_radiuses& p_radii) {
		push_clip(RoundedRect(to_rect(p_position), p_radii), active_geometry ? active_geometry->matrix : Transform2D());
	}

	void HTMLPaintRecorder::set_overflow_clip(const render_item& p_item) {
		const auto* geometry = recording_geometry ? recording_geometry->find_renderer(&p_item) : nullptr;
		if (geometry && geometry->child_overflow) push_clip(*geometry->child_overflow);
		else push_clip(RoundedRect(Rect2(), {}), Transform2D());
	}

	void HTMLPaintRecorder::set_render_item_clip(const litehtml::position& p_position, const border_radiuses& p_radii,
		const render_item& p_item) {
		const auto* geometry = recording_geometry ? recording_geometry->find_renderer(&p_item) : nullptr;
		push_clip(RoundedRect(to_rect(p_position), p_radii), geometry && geometry->inverse ? geometry->matrix : Transform2D());
	}

	bool HTMLPaintRecorder::begin_render_item(const render_item& p_item, const litehtml::position*& r_clip, bool p_decoration) {
		active_geometry = nullptr;
		active_decoration = p_decoration;
		if (p_item.collapsed_table_track()) return false;
		active_geometry = recording_geometry ? recording_geometry->find_renderer(&p_item) : nullptr;
		if (!active_geometry) return true;
		if (!active_geometry->inverse) return false;
		if (!active_decoration && (recording_geometry->uses_visual_hit_order() || recording_geometry->has_overflow_controls()) &&
			!active_geometry->fragments.empty()) {
			pointer_targets.push_back({ active_geometry });
		}
		if (has_elided_ancestor(p_item)) {
			active_geometry = nullptr;
			return false;
		}
		for (auto* clip = active_geometry->overflow; clip; clip = clip->parent) {
			if (clip->axes != ClipAxes::horizon || clip->matrix == active_geometry->matrix) continue;
			push_clip(*clip);
			++horizon_clip_count;
		}
		const auto elision = overflow_paint.find(&p_item);
		if (elision != overflow_paint.end() && elision->second.clip) {
			const auto& clip = *elision->second.clip;
			push_clip(clip);
			text_overflow_clip_active = true;
		}
		if (filter_depth > 0 || (p_item.parent() && active_geometry->matrix != Transform2D())) r_clip = nullptr;
		return true;
	}

	void HTMLPaintRecorder::end_render_item() {
		if (!active_decoration) record_outline();
		if (std::exchange(text_overflow_clip_active, false)) del_clip();
		const auto horizons = std::exchange(horizon_clip_count, std::size_t(0));
		for (std::size_t i = 0; i < horizons; ++i) del_clip();
		active_geometry = nullptr;
	}

	namespace {
		bool update_scrollbar_target(std::optional<HTMLScrollbarDrag>& r_target, const std::optional<HTMLScrollbarDrag>& p_target) {
			if (r_target.has_value() == p_target.has_value() &&
				(!p_target || (r_target->axis == p_target->axis && r_target->element.lock() == p_target->element.lock()))) return false;
			r_target = p_target;
			return true;
		}
	}

	bool HTMLPaintRecorder::set_scrollbar_hover(const std::optional<HTMLScrollbarDrag>& p_target) {
		return update_scrollbar_target(scrollbar_hovered, p_target);
	}

	bool HTMLPaintRecorder::set_scrollbar_pressed(const std::optional<HTMLScrollbarDrag>& p_target) {
		return update_scrollbar_target(scrollbar_pressed, p_target);
	}

	void HTMLPaintRecorder::paint_scrollbar(const Rect2& p_track, const Rect2& p_thumb, int p_axis,
		const HTMLScrollbarAppearance& p_appearance) {
		const auto matches = [&](const auto& p_target) {
			return p_target && p_target->axis == p_axis && active_geometry &&
				p_target->element.lock() == active_geometry->item->src_el();
		};
		const bool pressed = matches(scrollbar_pressed);
		const bool hot = pressed || matches(scrollbar_hovered);
		fill_rect(p_track, p_appearance.track_color());
		const Rect2 thumb = p_appearance.visible_thumb(p_thumb, p_axis, hot);
		fill_rounded_color(thumb, p_appearance.thumb_color(pressed), std::min(thumb.size.x, thumb.size.y) * 0.5);
	}

	void HTMLPaintRecorder::draw_input_scrollbar(const Rect2& p_clip, const Rect2& p_track, const Rect2& p_thumb,
		int p_axis, const HTMLScrollbarAppearance& p_appearance) {
		if (!push_content_clip(p_clip)) return;
		paint_scrollbar(p_track, p_thumb, p_axis, p_appearance);
		del_clip();
	}

	void HTMLPaintRecorder::draw_scrollbars(const render_item& p_item) {
		if (!recording_geometry->has_overflow_controls() || has_elided_ancestor(p_item)) return;
		const auto* geometry = recording_geometry->find_renderer(&p_item);
		if (!geometry) return;
		const auto bars = geometry->scrollbars();
		active_geometry = geometry;
		const auto add_target = [&](int p_axis, const Rect2& p_box) {
			pointer_targets.push_back({geometry, p_axis, p_box});
			const Rect2 bounds = geometry->matrix.xform(p_box);
			overflow_control_bounds = overflow_control_bounds.has_area() ? overflow_control_bounds.merge(bounds) : bounds;
		};
		const auto appearance = html_scrollbar_appearance(p_item.css());
		for (int axis = 0; axis < 2; ++axis) {
			const auto& bar = bars[axis];
			if (!bar.track.has_area()) continue;
			paint_scrollbar(bar.track, bar.thumb, axis, appearance);
			add_target(axis, bar.track);
		}
		const Rect2 grip = geometry->resize_grip();
		if (grip.has_area()) {
			draw_resize_grip(grip, grip);
			add_target(PointerTarget::resize, grip);
		}
		active_geometry = nullptr;
	}

	void HTMLPaintRecorder::draw_text_overflow(const render_item& p_context) {
		const auto found = ellipses_by_context.find(&p_context);
		if (found == ellipses_by_context.end() || has_elided_ancestor(p_context)) return;
		active_geometry = recording_geometry->find_renderer(&p_context);
		for (int64_t index : found->second) {
			const auto& mark = ellipses[index];
			push_clip(mark.clip);
			const auto& color = p_context.css().get_color();
			add_indexed_text(Text{ mark.layout, mark.origin, to_color(color), -1, 0, nullptr, index }, p_context.src_el().get(), color);
			del_clip();
		}
		active_geometry = nullptr;
	}

	HTMLPaintGroup& HTMLPaintRecorder::start_isolated_group(float p_opacity) {
		open_groups.back().isolated_group = paint_groups.size();
		paint_groups.push_back({paint_runs.size(), 0, 0, p_opacity});
		auto& group = paint_groups.back();
		group.first_pointer = pointer_targets.size();
		separate_run = true;
		return group;
	}

	void HTMLPaintRecorder::begin_background_blend(std::optional<css_blend_mode> p_layer) {
		open_groups.emplace_back();
		auto& group = start_isolated_group(1.0f);
		group.blends_background = !p_layer;
		group.blend_mode = p_layer.value_or(css_blend_mode::normal);
		if (active_geometry) group.depth_plane = active_geometry->depth_plane;
	}

	void HTMLPaintRecorder::begin_paint_group(const render_item& p_item) {
		auto& recording = open_groups.emplace_back();
		recording.is_stacking_context = p_item.establishes_stacking_context();
		const auto& style = p_item.css();
		const auto* geometry = recording_geometry->find_renderer(&p_item);
		recording.mask_geometry = style.get_mask() ? geometry : nullptr;
		const bool sorts_depth = geometry && geometry->starts_3d_context;
		const bool flattens_depth = geometry && geometry->flattens_3d_context;
		const bool extends_depth_scene = geometry && geometry->depth_plane && style.preserves_3d() && !sorts_depth;
		const bool blend_boundary = recording_geometry->has_element_blending() && recording.is_stacking_context &&
			!extends_depth_scene;
		const bool backdrop_root = p_item.is_root() || style.establishes_backdrop_root();
		if (style.has_group_effects() || sorts_depth || flattens_depth || blend_boundary ||
			(backdrop_root && recording_geometry->has_backdrop_filtering())) {
			auto& group = start_isolated_group(static_cast<float>(style.get_opacity()));
			recording.is_blend_boundary = blend_boundary;
			group.backdrop_root = backdrop_root;
			group.can_draw_directly = !style.has_backdrop_effects() && !sorts_depth && !flattens_depth;
			if (style.mixes_with_backdrop()) {
				for (auto ancestor = open_groups.rbegin() + 1; ancestor != open_groups.rend(); ++ancestor) {
					if (!ancestor->is_blend_boundary) continue;
					paint_groups[*ancestor->isolated_group].captures_backdrop = true;
					group.element_blend_mode = style.get_mix_blend_mode();
					break;
				}
			}
			group.sorts_depth = sorts_depth;
			has_depth_scene |= sorts_depth;
			if (geometry) {
				group.depth_plane = sorts_depth ? geometry->enclosing_depth_plane : geometry->depth_plane;
				const auto box = geometry->item->get_placement();
				group.matrix = geometry->matrix.affine_at(to_rect(box).get_center());
				recording.path_clip = geometry->path_clip;
			}
			const auto record_filters = [&](const css_filter_list& filters, std::vector<HTMLPaintFilter>& output) {
				for (const auto& filter : filters) {
					const bool spatial = is_one_of(filter.type, css_filter_type::blur, css_filter_type::drop_shadow);
					const float amount = spatial ? filter.blur.calc_percent(0) : static_cast<float>(filter.amount);
					output.push_back({ filter.type, amount, Vector2(filter.x.calc_percent(0), filter.y.calc_percent(0)),
						to_color(filter.color, style.get_color()) });
				}
			};
			if (style.has_backdrop_filters() && geometry && geometry->inverse && !geometry->fragments.empty()) {
				record_filters(style.get_backdrop_filters(), group.backdrop_filters);
				for (const auto& fragment : geometry->fragments) {
					if (!fragment.rect.has_area()) continue;
					const TransformedClip shape{ fragment, geometry->matrix };
					const TransformedClip rectangle{ RoundedRect(fragment.rect, {}), geometry->matrix };
					const auto output_clip = style.has_filters() || clips.empty() ? std::nullopt : clips.back().bounds;
					group.backdrop_areas.push_back({ shape, create_material(&shape.shape, true, &shape, !style.has_filters()),
						create_material(&rectangle.shape, true, &rectangle, false), output_clip });
				}
				for (auto ancestor = open_groups.rbegin() + 1; ancestor != open_groups.rend(); ++ancestor) {
					if (!ancestor->isolated_group) continue;
					auto& outer = paint_groups[*ancestor->isolated_group];
					if (outer.backdrop_root) {
						outer.captures_filter_backdrop = true;
						break;
					}
					outer.needs_parent_backdrop = true;
				}
			}
			if (style.has_filters() && !clips.empty()) group.output_clip = clips.back().bounds;
			if (const auto* path = recording.path_clip) {
				const Rect2 bounds = path->matrix.xform(path->shape.rect);
				group.output_clip = group.output_clip ? group.output_clip->intersection(bounds) : bounds;
				group.output_material = create_material(&path->shape, true, path, style.has_filters());
			} else if (style.has_filters() && !clips.empty() && clips.back().has_precise_clip) {
				group.output_material = create_material(nullptr, true);
			}
			if (style.has_filters()) {
				record_filters(style.get_filters(), group.filters);
				recording.enclosing_clips = std::exchange(clips, {});
				clip_material.unref();
				++filter_depth;
			}
		}
	}

	void HTMLPaintRecorder::end_paint_group() {
		auto& recording = open_groups.back();
		auto& outlines = recording.outlines;
		paint_runs.insert(paint_runs.end(), std::make_move_iterator(outlines.begin()), std::make_move_iterator(outlines.end()));
		if (recording.isolated_group) {
			auto& group = paint_groups[*recording.isolated_group];
			group.last_run = paint_runs.size();
			group.after_group = paint_groups.size();
			group.last_pointer = pointer_targets.size();
			if (recording.mask_geometry) record_mask(*recording.isolated_group, *recording.mask_geometry);
			separate_run = true;
		}
		if (recording.enclosing_clips) {
			clips = std::move(*recording.enclosing_clips);
			clip_material.unref();
			--filter_depth;
		}
		open_groups.pop_back();
	}

	void HTMLPaintRecorder::record_mask(std::size_t p_group, const RenderGeometry& p_geometry) {
		const auto& item = p_geometry.item;
		const auto& mask = *item->css().get_mask();
		const auto& source = mask.layers;
		auto& output = paint_groups[p_group].mask;
		output.resize(source.m_image.size());
		if (!p_geometry.inverse) return;

		const bool unbounded = std::find(source.m_clip.begin(), source.m_clip.end(), background_box_no_clip) != source.m_clip.end() ||
			std::any_of(source.m_image.begin(), source.m_image.end(), [](const litehtml::image& p_image) {
				return p_image.type == litehtml::image::type_url && p_image.url.find('#') != std::string::npos;
			});
		const Rect2 ink = unbounded ? p_geometry.inverse->xform(html_group_ink_bounds(paint_runs, paint_groups, p_group)) : Rect2();
		auto element = item->src_el();
		auto* container = element->get_document()->container();
		litehtml::position viewport;
		container->get_viewport(viewport);
		auto commands = std::exchange(paint_runs, {});
		auto enclosing_clips = std::exchange(clips, {});
		const auto* previous_geometry = std::exchange(active_geometry, &p_geometry);
		const auto box_of = [](const Rect2& p_box) -> litehtml::position {
			return { p_box.position.x, p_box.position.y, p_box.size.x, p_box.size.y };
		};

		const bool inline_box = item->css().get_display() == display_inline && !p_geometry.fragments.empty();
		const bool sliced = inline_box && p_geometry.fragments.size() > 1;
		Rect2 target_box;
		for (const auto& fragment : p_geometry.fragments)
			target_box = target_box.has_area() ? target_box.merge(fragment.rect) : fragment.rect;
		float slice_width = 0.0f;
		if (inline_box) {
			for (const auto& fragment : p_geometry.fragments) slice_width += fragment.rect.size.x;
		}
		for (std::size_t index = 0; index < output.size(); ++index) {
			auto& layer_output = output[index];
			const auto mode = mask.modes.empty() ? css_mask_mode::match_source :
				static_cast<css_mask_mode>(mask.modes[index % mask.modes.size()]);
			layer_output.luminance = mode == css_mask_mode::luminance;
			if (!mask.composites.empty()) layer_output.composite = mask.composites[index % mask.composites.size()];
			const auto& image = source.m_image[index];
			if (image.type == litehtml::image::type_url) {
				const auto svg = rasterize_svg_mask(*element, image.url, source.m_baseurl, target_box, ink,
					Vector2(viewport.width, viewport.height), p_geometry.matrix, mode);
				if (svg) {
					layer_output.luminance = svg->luminance;
					layer_output.linear_rgb = svg->linear_rgb;
					if (svg->texture.is_valid()) {
						add_image(Image{ svg->texture, svg->box, Rect2(Vector2(), svg->texture->get_size()) });
						layer_output.runs.swap(paint_runs);
					}
					continue;
				}
			}
			if (source.get_layer_type(static_cast<int>(index)) == background::type_none) continue;
			const int clip = source.m_clip.empty() ? background_box_border : source.m_clip[index % source.m_clip.size()];
			const auto draw = [&](litehtml::position p_content, const RoundedRect* p_slice) {
				background_layer layer;
				if (!source.get_layer(static_cast<int>(index), p_content, element.get(), item, layer)) return;
				layer.is_root = false;
				if (clip == background_box_no_clip) {
					layer.border_box = layer.clip_box = box_of(ink);
					layer.border_radius = {};
				} else if (clip == background_box_margin) {
					layer.border_box = layer.clip_box;
					layer.border_radius = {};
				}
				if (p_slice) push_clip(*p_slice, p_geometry.matrix);
				source.draw_layer(0, static_cast<int>(index), layer, container);
				if (p_slice) del_clip();
			};
			if (inline_box) {
				float offset = 0.0f;
				for (const auto& fragment : p_geometry.fragments) {
					auto strip = fragment.rect;
					strip.position.x -= item->css().is_right_to_left() ? slice_width - offset - fragment.rect.size.x : offset;
					strip.size.x = slice_width;
					auto content = box_of(strip);
					content -= item->get_borders();
					content -= item->get_paddings();
					draw(content, sliced ? &fragment : nullptr);
					offset += fragment.rect.size.x;
				}
			} else {
				draw(item->get_placement(), nullptr);
			}
			layer_output.runs.swap(paint_runs);
		}
		active_geometry = previous_geometry;
		clips = std::move(enclosing_clips);
		clip_material.unref();
		paint_runs = std::move(commands);
	}

	void HTMLPaintRecorder::record_outline() {
		if (!active_geometry || !active_geometry->inverse || active_geometry->fragments.empty() ||
			!active_geometry->item->is_visible()) return;
		const auto& css = active_geometry->item->css();
		const auto& outline = css.get_outline();
		if (outline.style == border_style_none) return;
		const bool automatic = outline.style == outline_style_auto;
		const real_t width = automatic ? 2.0f : outline.width.val();
		const real_t gap = outline.offset.val();
		if (width <= 0 || !std::isfinite(width) || !std::isfinite(gap)) return;

		auto fragment = active_geometry->fragments.begin();
		Rect2 box = fragment->rect;
		for (++fragment; fragment != active_geometry->fragments.end(); ++fragment) box = box.merge(fragment->rect);
		// Constrain the two axes independently so a negative offset can never erase the ring.
		const Vector2 outsets = Vector2(width + gap, width + gap).max(Vector2(width, width) - box.size * 0.5f);
		const Rect2 outer(box.position - outsets, box.size + outsets * 2.0f);
		if (!outer.is_finite() || !outer.has_area() ||
			(!clips.empty() && clips.back().bounds && !clips.back().bounds->intersects(active_geometry->matrix.xform(outer)))) return;

		border side;
		side.width = width;
		side.style = automatic ? border_style_solid : static_cast<border_style>(outline.style);
		if (outline.color) {
			side.color = outline.color->to_srgb(css.get_color());
		}
		else {
			const auto* display = DisplayServer::get_singleton();
			const Color accent = display ? display->get_accent_color() : Color(0, 0, 0, 0);
			if (accent.a > 0) {
				const uint32_t rgba = accent.to_rgba32();
				side.color = web_color(rgba >> 24, (rgba >> 16) & 255, (rgba >> 8) & 255, rgba & 255);
			}
			else side.color = web_color(37, 99, 235);
		}
		if (side.color.alpha == 0) return;

		borders ring;
		ring.top = ring.right = ring.bottom = ring.left = side;
		const RoundedRect shape = active_geometry->fragments.size() == 1 ? active_geometry->fragments.front() :
			RoundedRect(box, css.get_borders().radius.calc_percents(box.size.x, box.size.y));
		const auto outset_radius = [&](int p_corner) {
			if (shape.radii_x[p_corner] <= 0 || shape.radii_y[p_corner] <= 0) return Vector2();
			return (Vector2(shape.radii_x[p_corner], shape.radii_y[p_corner]) + outsets).max(Vector2());
		};
		const Vector2 top_left = outset_radius(0);
		const Vector2 top_right = outset_radius(1);
		const Vector2 bottom_right = outset_radius(2);
		const Vector2 bottom_left = outset_radius(3);
		ring.radius.top_left_x = top_left.x;
		ring.radius.top_left_y = top_left.y;
		ring.radius.top_right_x = top_right.x;
		ring.radius.top_right_y = top_right.y;
		ring.radius.bottom_right_x = bottom_right.x;
		ring.radius.bottom_right_y = bottom_right.y;
		ring.radius.bottom_left_x = bottom_left.x;
		ring.radius.bottom_left_y = bottom_left.y;
		// Positioned descendants can paint after their ancestor's local paint group.
		const auto context = std::find_if(open_groups.rbegin(), open_groups.rend(),
			[](const OpenPaintGroup& group) { return group.is_stacking_context; });
		add_borders(context == open_groups.rend() ? paint_runs : context->outlines, ring, outer);
	}

	void HTMLPaintRecorder::push_clip(const TransformedClip& p_clip) {
		auto bounds = clips.empty() ? std::optional<Rect2>{} : clips.back().bounds;
		if (p_clip.axes == ClipAxes::both) {
			const Rect2 rect = p_clip.matrix.xform(p_clip.shape.rect);
			bounds = bounds ? bounds->intersection(rect) : rect;
		}
		else if (p_clip.axes != ClipAxes::horizon) {
			const int axis = p_clip.axes == ClipAxes::x ? 0 : 1;
			if (p_clip.shape.rect.size[axis] <= 0) bounds = Rect2();
		}
		const bool precise = p_clip.needs_precise_geometry() || (!clips.empty() && clips.back().has_precise_clip);
		clips.push_back({ p_clip, bounds, precise });
		clip_material.unref();
	}

	void HTMLPaintRecorder::del_clip() {
		if (!clips.empty()) {
			clips.pop_back();
			clip_material.unref();
		}
	}
} // namespace dockit

Ref<ShaderMaterial> dockit::make_html_paint_material(bool p_composite) {
	auto& shader = p_composite ? composite_clip_shader : paint_shader;
	if (shader.is_null()) {
		shader.instantiate();
		shader->set_code(p_composite ? String("#define HTML_COMPOSITE_CLIP\n") + PAINT_SHADER_CODE : String(PAINT_SHADER_CODE));
	}
	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	return material;
}

void dockit::release_html_paint_shader() {
	paint_shader.unref();
	composite_clip_shader.unref();
}
