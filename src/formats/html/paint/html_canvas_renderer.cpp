#include "html_canvas_renderer.h"
#include "html_blend_shader.h"
#include "html_depth_sort.h"
#include "formats/html/css/css_values.h"
#include "formats/html/text/html_shaped_text.h"

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

using namespace godot;

namespace dockit {
	namespace {
		Vector2i text_highlight_range(const HTMLText& p_text, const HTMLHighlight& p_highlight) {
			if (p_text.offset < 0) return {};
			const int64_t from = std::max<int64_t>(0, p_highlight.from - p_text.offset);
			const int64_t to = std::min(p_text.length, p_highlight.to - p_text.offset);
			if (from >= to) return {};
			if (p_text.ellipsis >= 0) return from == 0 && to == p_text.length ? Vector2i(0, 1) : Vector2i();
			return Vector2i(from, to);
		}

		Ref<ShaderMaterial> partial_selection_material(const HTMLPaintRun& p_run, const HTMLHighlight& p_selection,
			const TextServer* p_text_server, const Vector2& p_offset, bool p_selection_only = false) {
			if (!p_text_server || p_selection.from >= p_selection.to || p_run.commands.size() != 1) return {};
			const auto* text = std::get_if<HTMLText>(&p_run.commands.front());
			if (!text || !text->selectable || text->ellipsis >= 0 || (!p_selection_only && !text->selection_foreground)) return {};
			const Vector2i range = text_highlight_range(*text, p_selection);
			if (range.x >= range.y || (range.x == 0 && range.y == text->length)) return {};

			std::vector<Vector2> spans;
			const real_t x = text->position.x + p_offset.x;
			for (const Vector2 span : text->layout.selection(*p_text_server, range.x, range.y)) {
				if (span.x < span.y) spans.emplace_back(x + span.x, x + span.y);
			}
			if (spans.empty()) return {};
			std::sort(spans.begin(), spans.end(), [](const Vector2& p_left, const Vector2& p_right) { return p_left.x < p_right.x; });

			Ref<ShaderMaterial> material;
			if (p_run.material.is_valid()) material = p_run.material->duplicate();
			else material = make_html_paint_material();
			material->set_shader_parameter("selection_span_count", static_cast<int64_t>(spans.size()));
			material->set_shader_parameter("selection_ink", text->selection_foreground.value_or(text->color));
			material->set_shader_parameter("ordinary_ink", p_selection_only ? Color(0, 0, 0, 0) : text->color);
			if (spans.size() == 1) {
				material->set_shader_parameter("selection_first_span", spans.front());
			} else {
				const size_t columns = std::min<size_t>(spans.size(), 2048);
				const size_t rows = (spans.size() + columns - 1) / columns;
				const auto image = Image::create_empty(columns, rows, false, Image::FORMAT_RGF);
				if (image.is_null()) return {};
				for (size_t index = 0; index < spans.size(); ++index) {
					image->set_pixel(index % columns, index / columns, Color(spans[index].x, spans[index].y, 0, 1));
				}
				material->set_shader_parameter("selection_spans", ImageTexture::create_from_image(image));
			}
			return material;
		}

		RenderingServer::CanvasItemTextureFilter to_canvas_filter(HTMLTextureFilter p_filter) {
			switch (p_filter) {
				case HTMLTextureFilter::nearest: return RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST;
				case HTMLTextureFilter::linear: return RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR;
				case HTMLTextureFilter::linear_mipmaps: return RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS;
				case HTMLTextureFilter::inherited: return RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
			}
			return RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
		}

		constexpr int GROUP_TILE_SIZE = 2048;
		constexpr int MAX_TILE_HALO = 1024;
		constexpr int FILTER_TAPS = 32;
		using Filter = litehtml::css_filter_type;

		Vector2 blur_extent(const Transform2D& p_matrix, const HTMLPaintFilter& p_filter) {
			return (p_matrix[0].abs() + p_matrix[1].abs()) * (3.0f * p_filter.amount);
		}

		Rect2 include_filter_ink(Rect2 p_bounds, const HTMLPaintGroup& p_group) {
			for (const auto& filter : p_group.filters) {
				if (filter.type != Filter::blur && filter.type != Filter::drop_shadow) continue;
				const auto blur = blur_extent(p_group.matrix, filter);
				const Rect2 expanded(p_bounds.position - blur, p_bounds.size + blur * 2.0f);
				p_bounds = filter.type == Filter::blur ? expanded :
					p_bounds.merge(translate_rect(expanded, p_group.matrix.basis_xform(filter.offset)));
			}
			return p_group.output_clip ? p_bounds.intersection(*p_group.output_clip) : p_bounds;
		}

		Vector2 filter_halo(const std::vector<HTMLPaintFilter>& p_filters, const Transform2D& p_matrix, const Vector2& p_scale) {
			Vector2 halo;
			for (const auto& filter : p_filters) {
				if (filter.type != Filter::blur && filter.type != Filter::drop_shadow) continue;
				halo += (blur_extent(p_matrix, filter) + p_matrix.basis_xform(filter.offset).abs()) * p_scale + Vector2(1.0, 1.0);
			}
			return halo.ceil().min(Vector2(MAX_TILE_HALO, MAX_TILE_HALO));
		}

		Rect2 command_bounds(const HTMLPaintCommand& p_command) {
			return std::visit([](const auto& p_draw) -> Rect2 {
				using Draw = std::decay_t<decltype(p_draw)>;
				if constexpr (std::is_same_v<Draw, HTMLFill>) {
					return p_draw.rect;
				} else if constexpr (std::is_same_v<Draw, HTMLText>) {
					// Include glyph overhang and decorations beyond the measured line box.
					return Rect2(p_draw.position, p_draw.layout.size()).grow(std::max(1.0f, p_draw.layout.height()));
				} else if constexpr (std::is_same_v<Draw, HTMLImage>) {
					return p_draw.destination;
				} else {
					const Vector2 radius(p_draw.radius + 1.0f, p_draw.radius + 1.0f);
					return { p_draw.center - radius, radius * 2.0f };
				}
			}, p_command);
		}

		void include_bounds(Rect2& r_bounds, const Rect2& p_rect) {
			if (p_rect.has_area() && p_rect.is_finite()) r_bounds = r_bounds.has_area() ? r_bounds.merge(p_rect) : p_rect;
		}

		Rect2 run_bounds(const HTMLPaintRun& p_run) {
			Rect2 bounds;
			for (const auto& command : p_run.commands) {
				const Rect2 rect = p_run.matrix.xform(command_bounds(command));
				include_bounds(bounds, p_run.clip ? rect.intersection(*p_run.clip) : rect);
			}
			return bounds;
		}

		std::vector<Rect2> group_bounds(const std::vector<HTMLPaintRun>& p_runs, const std::vector<HTMLPaintGroup>& p_groups,
			std::size_t p_first = 0) {
			std::vector<Rect2> bounds(p_groups.size() - p_first);
			for (std::size_t index = p_groups.size(); index-- > p_first;) {
				const auto& group = p_groups[index];
				if (group.opacity == 0.0f) continue;
				auto& box = bounds[index - p_first];
				for (const auto& area : group.backdrop_areas) {
					const auto visual = area.clip.matrix.xform(area.clip.shape.rect);
					include_bounds(box, area.output_clip ? visual.intersection(*area.output_clip) : visual);
				}
				std::size_t run_index = group.first_run;
				const auto include_runs = [&](std::size_t p_end) {
					for (; run_index < p_end; ++run_index) {
						include_bounds(box, run_bounds(p_runs[run_index]));
					}
				};
				for (std::size_t child = index + 1; child < group.after_group; child = p_groups[child].after_group) {
					include_runs(p_groups[child].first_run);
					include_bounds(box, bounds[child - p_first]);
					run_index = p_groups[child].last_run;
				}
				include_runs(group.last_run);
				if (box.has_area()) box = include_filter_ink(box, group);
			}
			return bounds;
		}

		constexpr const char* PROJECTION_SHADER = R"(
shader_type canvas_item;
render_mode blend_premul_alpha, unshaded;
uniform vec3 inverse_x;
uniform vec3 inverse_y;
uniform vec3 inverse_w;
uniform vec4 tile;
uniform vec2 source_origin;
uniform vec2 source_scale;
varying vec2 document_point;

void vertex() {
	document_point = VERTEX;
}

void fragment() {
	vec3 point = vec3(document_point, 1.0);
	float w = dot(inverse_w, point);
	if (w <= 0.0) discard;
	vec2 local = vec2(dot(inverse_x, point), dot(inverse_y, point)) / w;
	if (any(lessThan(local, tile.xy)) || any(greaterThanEqual(local, tile.zw))) discard;
	vec2 uv = (local * source_scale - source_origin) * TEXTURE_PIXEL_SIZE;
	COLOR = texture(TEXTURE, uv);
}
)";

		constexpr const char* OPACITY_SHADER = R"(
shader_type canvas_item;
render_mode blend_premul_alpha, unshaded;
varying vec4 group_modulate;
void vertex() {
	group_modulate = COLOR;
}
void fragment() {
	COLOR = texture(TEXTURE, UV) * vec4(group_modulate.rgb, 1.0) * group_modulate.a;
}
)";

		constexpr int MASK_APPLY = 4;
		constexpr const char* MASK_SHADER = R"(
shader_type canvas_item;
render_mode blend_disabled, unshaded;
uniform sampler2D lower_layers : repeat_disable, filter_nearest;
uniform int operation;
uniform bool has_lower;
uniform bool luminance;
uniform bool hdr;
uniform bool linear_rgb;

vec3 to_srgb(vec3 color) {
	return mix(color * 12.92, 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
		greaterThan(color, vec3(0.0031308)));
}

vec3 to_linear(vec3 color) {
	return mix(color / 12.92, pow(max((color + 0.055) / 1.055, vec3(0.0)), vec3(2.4)),
		greaterThan(color, vec3(0.04045)));
}

void fragment() {
	vec4 sample_color = texture(TEXTURE, UV);
	float lower_alpha = has_lower ? texture(lower_layers, UV).a : 0.0;
	if (operation == 4) {
		COLOR = sample_color * lower_alpha;
	} else {
		float alpha = sample_color.a;
		if (luminance && alpha > 0.0) {
			vec3 rgb = sample_color.rgb / alpha;
			if (hdr && !linear_rgb) rgb = to_srgb(rgb);
			else if (!hdr && linear_rgb) rgb = to_linear(rgb);
			alpha *= clamp(dot(rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
		}
		if (has_lower) {
			if (operation == 1) alpha *= (1.0 - lower_alpha);
			else if (operation == 2) alpha *= lower_alpha;
			else if (operation == 3) alpha = alpha * (1.0 - lower_alpha) + lower_alpha * (1.0 - alpha);
			else alpha += lower_alpha * (1.0 - alpha);
		}
		COLOR = vec4(alpha);
	}
}
)";

		// CSS filter functions operate on sRGB. Viewport textures carry premultiplied pixels.
		constexpr const char* FILTER_SHADER = R"(
shader_type canvas_item;
render_mode blend_disabled, unshaded;
uniform int operation;
uniform float amount;
uniform bool hdr;
uniform vec2 step_uv;
uniform int taps;
uniform float weights[33];
uniform sampler2D shadow_mask : repeat_disable, filter_linear;
uniform vec2 shadow_offset;
uniform vec4 shadow_color;
uniform bool reflect_edges;
uniform vec4 filter_box;
uniform vec3 uv_to_box_x;
uniform vec3 uv_to_box_y;
uniform vec3 uv_to_box_w;
uniform vec3 box_to_uv_x;
uniform vec3 box_to_uv_y;
uniform vec3 box_to_uv_w;

vec3 to_srgb(vec3 color) {
	return mix(color * 12.92, 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
		greaterThan(color, vec3(0.0031308)));
}

vec3 to_linear(vec3 color) {
	return mix(color / 12.92, pow(max((color + 0.055) / 1.055, vec3(0.0)), vec3(2.4)),
		greaterThan(color, vec3(0.04045)));
}

vec4 to_working_color(vec4 sample_color) {
	if (hdr && sample_color.a > 0.0) sample_color.rgb = to_srgb(sample_color.rgb / sample_color.a) * sample_color.a;
	return sample_color;
}

bool outside_texture(vec2 uv) {
	return any(lessThan(uv, vec2(0.0))) || any(greaterThanEqual(uv, vec2(1.0)));
}

vec4 blur_sample(sampler2D source, vec2 uv) {
	if (reflect_edges) {
		vec3 sample_point = vec3(uv, 1.0);
		float w = dot(uv_to_box_w, sample_point);
		if (w <= 0.0) return vec4(0.0);
		vec2 local = vec2(dot(uv_to_box_x, sample_point), dot(uv_to_box_y, sample_point)) / w - filter_box.xy;
		vec2 size = filter_box.zw;
		vec2 reflected = size - abs(mod(local, 2.0 * size) - size) + filter_box.xy;
		vec3 position = vec3(reflected, 1.0);
		w = dot(box_to_uv_w, position);
		if (w <= 0.0) return vec4(0.0);
		uv = vec2(dot(box_to_uv_x, position), dot(box_to_uv_y, position)) / w;
	}
	if (outside_texture(uv)) return vec4(0.0);
	return to_working_color(texture(source, uv));
}

vec3 change_color(vec3 rgb) {
	if (operation == 1) return mix(rgb, vec3(dot(rgb, vec3(0.2126, 0.7152, 0.0722))), amount);
	if (operation == 2) return mix(rgb, vec3(dot(rgb, vec3(0.393, 0.769, 0.189)),
		dot(rgb, vec3(0.349, 0.686, 0.168)), dot(rgb, vec3(0.272, 0.534, 0.131))), amount);
	if (operation == 3) return mix(vec3(dot(rgb, vec3(0.213, 0.715, 0.072))), rgb, amount);
	if (operation == 4) {
		float c = cos(amount);
		float s = sin(amount);
		return vec3(dot(rgb, vec3(0.213 + c * 0.787 - s * 0.213, 0.715 - c * 0.715 - s * 0.715, 0.072 - c * 0.072 + s * 0.928)),
			dot(rgb, vec3(0.213 - c * 0.213 + s * 0.143, 0.715 + c * 0.285 + s * 0.140, 0.072 - c * 0.072 - s * 0.283)),
			dot(rgb, vec3(0.213 - c * 0.213 - s * 0.787, 0.715 - c * 0.715 + s * 0.715, 0.072 + c * 0.928 + s * 0.072)));
	}
	if (operation == 5) return mix(rgb, vec3(1.0) - rgb, amount);
	if (operation == 7) return rgb * amount;
	return (rgb - 0.5) * amount + 0.5;
}

void fragment() {
	vec4 value = to_working_color(texture(TEXTURE, UV));
	if (operation == 9) {
		value = blur_sample(TEXTURE, UV) * weights[0];
		for (int i = 1; i <= taps; i++) {
			vec2 distance = step_uv * float(i);
			value += (blur_sample(TEXTURE, UV - distance) + blur_sample(TEXTURE, UV + distance)) * weights[i];
		}
	} else if (operation == 10) {
		vec2 shadow_uv = UV - shadow_offset;
		float alpha = outside_texture(shadow_uv) ? 0.0 : texture(shadow_mask, shadow_uv).a * shadow_color.a;
		value += vec4(shadow_color.rgb * alpha, alpha) * (1.0 - value.a);
	} else if (operation == 6) {
		value *= amount;
	} else if (operation > 0 && value.a > 0.0) {
		value.rgb = clamp(change_color(value.rgb / value.a), vec3(0.0), vec3(1.0)) * value.a;
	}
	if (hdr && value.a > 0.0) value.rgb = to_linear(value.rgb / value.a) * value.a;
	COLOR = value;
}
)";
	}

	Rect2 html_group_ink_bounds(const std::vector<HTMLPaintRun>& p_runs,
		const std::vector<HTMLPaintGroup>& p_groups, std::size_t p_group) {
		return group_bounds(p_runs, p_groups, p_group).front();
	}

	void HTMLPaintRun::draw(RenderingServer& p_server, const RID& p_canvas, const Vector2& p_offset,
		const TextServer* p_text_server, const HTMLHighlight& p_search, const HTMLHighlight& p_selection, const RID& p_ink_canvas,
		bool p_selection_only) const {
		for (const HTMLPaintCommand& command : commands) {
			std::visit([&](const auto& p_draw) {
				using Draw = std::decay_t<decltype(p_draw)>;
				if constexpr (std::is_same_v<Draw, HTMLFill>) {
					if (p_selection_only) return;
					const Rect2 rect = translate_rect(p_draw.rect, p_offset);
					p_server.canvas_item_add_rect(p_canvas, rect, p_draw.color);
				}
				else if constexpr (std::is_same_v<Draw, HTMLText>) {
					Color ink = p_draw.color;
					if (p_text_server && p_draw.offset >= 0) {
						for (const HTMLHighlight* highlight : { &p_search, &p_selection }) {
							const bool selection = highlight == &p_selection;
							if (selection && !p_draw.selectable) continue;
							const Vector2i interval = text_highlight_range(p_draw, *highlight);
							if (interval.x >= interval.y) continue;
							if (selection && p_draw.selection_foreground && interval.x == 0 &&
								(interval.y == p_draw.length || p_draw.ellipsis >= 0)) ink = *p_draw.selection_foreground;
							if (p_selection_only && selection) continue;
							const auto& background = selection ? p_draw.selection_background : std::nullopt;
							if (background && background->a == 0) continue;
							for (const Vector2& range : p_draw.layout.selection(*p_text_server, interval.x, interval.y)) {
								const Rect2 rect = translate_rect(Rect2(range.x, 0, range.y - range.x, p_draw.layout.height()),
									p_draw.position + p_offset);
								if (highlight->style && !background) highlight->style->draw(p_canvas, rect);
								else p_server.canvas_item_add_rect(p_canvas, rect, background.value_or(highlight->color));
							}
						}
					}
					if (p_ink_canvas.is_valid()) p_draw.layout.draw(p_text_server, p_ink_canvas, p_draw.position + p_offset, Color(1, 1, 1));
					else if (ink.a > 0) p_draw.layout.draw(p_text_server, p_canvas, p_draw.position + p_offset, ink);
				}
				else if constexpr (std::is_same_v<Draw, HTMLImage>) {
					if (p_selection_only) return;
					const Rect2 rect = translate_rect(p_draw.destination, p_offset);
					p_draw.texture->draw_rect_region(p_canvas, rect, p_draw.source, p_draw.modulate, false, !sampling.repeat);
				}
				else if constexpr (std::is_same_v<Draw, HTMLCircle>) {
					if (p_selection_only) return;
					const Vector2 center = p_draw.center + p_offset;
					if (p_draw.filled) {
						p_server.canvas_item_add_circle(p_canvas, center, p_draw.radius, p_draw.color, true);
					}
					else {
						PackedVector2Array points;
						constexpr int SEGMENTS = 20;
						for (int index = 0; index <= SEGMENTS; ++index) {
							const float angle = static_cast<float>(Math_TAU) * index / SEGMENTS;
							points.push_back(center + Vector2(std::cos(angle), std::sin(angle)) * p_draw.radius);
						}
						p_server.canvas_item_add_polyline(p_canvas, points, PackedColorArray({ p_draw.color }), 1.0f, true);
					}
				}
			}, command);
		}
	}

	void HTMLCanvasRenderer::CanvasDrawing::clear() {
		if (RenderingServer* server = RenderingServer::get_singleton()) {
			for (auto item = items.rbegin(); item != items.rend(); ++item) {
				server->free_rid(*item);
			}
			for (auto surface = surfaces.rbegin(); surface != surfaces.rend(); ++surface) {
				server->free_rid(surface->viewport);
				server->free_rid(surface->canvas);
			}
		}
		items.clear();
		surfaces.clear();
		materials.clear();
		run_count = 0;
	}

	RID HTMLCanvasRenderer::CanvasDrawing::add_item(RenderingServer& p_server, const RID& p_parent, int32_t p_draw_index) {
		const RID item = p_server.canvas_item_create();
		p_server.canvas_item_set_parent(item, p_parent);
		p_server.canvas_item_set_draw_index(item, p_draw_index);
		items.push_back(item);
		return item;
	}

	HTMLCanvasRenderer::Surface HTMLCanvasRenderer::CanvasDrawing::add_surface(RenderingServer& p_server, const RID& p_parent,
		const Vector2i& p_size, const Transform2D& p_mapping, bool p_hdr) {
		const Surface surface{ p_server.viewport_create(), p_server.canvas_create() };
		if (order_passes) {
			pass_indices.insert(surface.viewport, pass_parents.size());
			pass_parents.push_back(p_parent);
		}
		surfaces.push_back(surface);
		p_server.viewport_set_parent_viewport(surface.viewport, p_parent);
		p_server.viewport_set_disable_3d(surface.viewport, true);
		p_server.viewport_set_use_hdr_2d(surface.viewport, p_hdr);
		p_server.viewport_set_transparent_background(surface.viewport, true);
		p_server.viewport_set_size(surface.viewport, p_size.x, p_size.y);
		p_server.viewport_attach_canvas(surface.viewport, surface.canvas);
		p_server.viewport_set_canvas_transform(surface.viewport, surface.canvas, p_mapping);
		p_server.viewport_set_update_mode(surface.viewport, RenderingServer::VIEWPORT_UPDATE_ONCE);
		p_server.viewport_set_active(surface.viewport, true);
		return surface;
	}

	void HTMLCanvasRenderer::CanvasDrawing::set_surface_parent(RenderingServer& p_server, const RID& p_surface,
		const RID& p_parent) {
		if (const auto* index = pass_indices.getptr(p_surface)) pass_parents[*index] = p_parent;
		p_server.viewport_set_parent_viewport(p_surface, p_parent);
	}

	void HTMLCanvasRenderer::CanvasDrawing::order_surface_passes(RenderingServer& p_server, const RID& p_viewport,
		std::size_t p_start) {
		// Backdrop snapshots can feed more than one consumer. Godot's viewport parents encode the
		// resulting pass order, including dependencies between otherwise separate paint groups.
		const std::size_t count = pass_parents.size();
		std::vector<std::vector<std::size_t>> consumers(count);
		std::vector<std::size_t> inputs(count);
		const auto depend = [&](std::size_t p_input, const RID& p_consumer) {
			if (const auto* index = pass_indices.getptr(p_consumer)) {
				consumers[p_input].push_back(*index);
				++inputs[*index];
			}
		};
		for (std::size_t i = 0; i < count; ++i) depend(i, pass_parents[i]);
		for (const auto& [source, consumer] : texture_dependencies) depend(*pass_indices.getptr(source), consumer);
		std::vector<std::size_t> ordered;
		ordered.reserve(count);
		for (std::size_t i = 0; i < count; ++i) {
			if (inputs[i] == 0) ordered.push_back(i);
		}
		for (std::size_t i = 0; i < ordered.size(); ++i) {
			for (auto consumer : consumers[ordered[i]]) {
				if (--inputs[consumer] == 0) ordered.push_back(consumer);
			}
		}
		if (ordered.size() == count) {
			RID parent = p_viewport;
			for (auto i = ordered.rbegin(); i != ordered.rend(); ++i) {
				const RID surface = surfaces[p_start + *i].viewport;
				p_server.viewport_set_parent_viewport(surface, parent);
				parent = surface;
			}
		} else {
			ERR_PRINT("Dockit: cyclic dependencies between HTML paint passes.");
		}
		pass_indices.clear();
		pass_parents.clear();
		texture_dependencies.clear();
		order_passes = false;
	}

	HTMLCanvasRenderer::Surface HTMLCanvasRenderer::filter_surface(RenderingServer& p_server, CanvasDrawing& p_drawing,
		Surface p_source, const RID& p_parent, const Vector2i& p_size, const Vector2& p_scale,
		const std::vector<HTMLPaintFilter>& p_filters, const Transform2D& p_matrix, bool p_hdr, const FilterRegion* p_region) {
		if (p_filters.empty()) return p_source;
		if (filter_shader.is_null()) {
			filter_shader.instantiate();
			filter_shader->set_code(FILTER_SHADER);
		}
		const auto make_material = [&](int p_operation) {
			Ref<ShaderMaterial> material;
			material.instantiate();
			material->set_shader(filter_shader);
			material->set_shader_parameter("operation", p_operation);
			material->set_shader_parameter("hdr", p_hdr);
			p_drawing.materials.push_back(material);
			return material;
		};
		const auto render_pass = [&](const Surface& p_input, const Surface& p_dependency, const Ref<ShaderMaterial>& p_material,
			const Vector2i& p_target_size) {
			const auto surface = p_drawing.add_surface(p_server, p_parent, p_target_size, Transform2D(), p_hdr);
			p_drawing.set_surface_parent(p_server, p_dependency.viewport, surface.viewport);
			const RID item = p_drawing.add_item(p_server, surface.canvas, 0);
			p_server.canvas_item_set_material(item, p_material->get_rid());
			p_server.canvas_item_set_default_texture_filter(item, RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR);
			p_server.canvas_item_add_texture_rect(item, Rect2(Vector2(), Vector2(p_target_size)), p_server.viewport_get_texture(p_input.viewport));
			return surface;
		};
		const auto blur = [&](Surface p_input, float p_sigma, bool p_full_size, bool p_reflect) {
			if (p_sigma <= 0.0f) return p_input;
			Vector2i working_size = p_size;
			Vector2 scale = p_scale;
			const auto target_radius = [&]() {
				return 3.0f * p_sigma * std::max((p_matrix[0] * scale).length(), (p_matrix[1] * scale).length());
			};
			Ref<ShaderMaterial> copy;
			// Downsample wide kernels before filtering to preserve thin detail between taps.
			while (!p_reflect && target_radius() > FILTER_TAPS) {
				const Vector2i smaller = ((working_size + Vector2i(1, 1)) / 2).max(Vector2i(2, 2));
				if (smaller == working_size) break;
				if (copy.is_null()) copy = make_material(0);
				p_input = render_pass(p_input, p_input, copy, smaller);
				working_size = smaller;
				scale = p_scale * Vector2(working_size) / Vector2(p_size);
			}
			for (int axis = 0; axis < 2; ++axis) {
				const auto direction = p_matrix[axis] * scale;
				const float sigma = p_sigma * direction.length();
				if (sigma <= 0.0f || !std::isfinite(sigma)) continue;
				const float radius = std::ceil(std::min(3.0f * sigma, static_cast<float>(MAX_TILE_HALO)));
				const int taps = std::min(static_cast<int>(radius), FILTER_TAPS);
				const float step = radius / taps;
				PackedFloat32Array weights;
				weights.resize(FILTER_TAPS + 1);
				weights.set(0, 1.0f);
				float total = 1.0f;
				for (int index = 1; index <= taps; ++index) {
					const float distance = step * index / sigma;
					const float weight = std::exp(-0.5f * distance * distance);
					weights.set(index, weight);
					total += 2.0f * weight;
				}
				for (int index = 0; index <= taps; ++index) weights.set(index, weights[index] / total);
				const auto material = make_material(static_cast<int>(Filter::blur));
				material->set_shader_parameter("step_uv", direction.normalized() * step / Vector2(working_size));
				material->set_shader_parameter("taps", taps);
				material->set_shader_parameter("weights", weights);
				if (p_reflect) {
					const auto& box = p_region->box;
					material->set_shader_parameter("reflect_edges", true);
					material->set_shader_parameter("filter_box", Vector4(box.position.x, box.position.y, box.size.x, box.size.y));
					for (int row = 0; row < 3; ++row) {
						const char* to_box[] = { "uv_to_box_x", "uv_to_box_y", "uv_to_box_w" };
						const char* to_uv[] = { "box_to_uv_x", "box_to_uv_y", "box_to_uv_w" };
						material->set_shader_parameter(to_box[row], p_region->uv_to_box.row(row));
						material->set_shader_parameter(to_uv[row], p_region->box_to_uv.row(row));
					}
				}
				p_input = render_pass(p_input, p_input, material, working_size);
			}
			if (p_full_size && copy.is_valid()) p_input = render_pass(p_input, p_input, copy, p_size);
			return p_input;
		};
		for (const auto& filter : p_filters) {
			if (filter.type == Filter::blur) {
				p_source = blur(p_source, filter.amount, true, p_region != nullptr);
			} else if (filter.type == Filter::drop_shadow) {
				if (filter.color.a <= 0.0f) continue;
				const auto mask = blur(p_source, filter.amount, false, false);
				const auto material = make_material(static_cast<int>(filter.type));
				material->set_shader_parameter("shadow_mask", p_server.viewport_get_texture(mask.viewport));
				material->set_shader_parameter("shadow_offset", p_matrix.basis_xform(filter.offset) * p_scale / Vector2(p_size));
				const auto& color = filter.color;
				material->set_shader_parameter("shadow_color", Vector4(color.r, color.g, color.b, color.a));
				p_source = render_pass(p_source, mask, material, p_size);
			} else {
				const auto material = make_material(static_cast<int>(filter.type));
				material->set_shader_parameter("amount", std::min(filter.amount, 1000000.0f));
				p_source = render_pass(p_source, p_source, material, p_size);
			}
		}
		return p_source;
	}

	HTMLCanvasRenderer::~HTMLCanvasRenderer() {
		while (!drawings.is_empty()) {
			detach_canvas(drawings.begin()->key);
		}
	}

	void HTMLCanvasRenderer::submit(CanvasItem& p_canvas, const Vector2& p_position, const Rect2& p_clip,
		const std::vector<HTMLPaintRun>& p_runs, const std::vector<HTMLPaintGroup>& p_groups, const std::vector<HTMLPaintRun>& p_diagnostics,
		const HTMLHighlight& p_selection, const HTMLHighlight& p_search, const Vector2& p_viewport_origin, bool p_selection_only) {
		const uint64_t instance_id = p_canvas.get_instance_id();
		CanvasDrawing* drawing = drawings.getptr(instance_id);
		if (!drawing) {
			CanvasDrawing target;
			target.redraw = callable_mp(this, &HTMLCanvasRenderer::on_canvas_redraw).bind(instance_id);
			target.exiting = callable_mp(this, &HTMLCanvasRenderer::detach_canvas).bind(instance_id);
			p_canvas.connect("draw", target.redraw);
			p_canvas.connect("tree_exiting", target.exiting);
			drawing = &drawings.insert(instance_id, std::move(target))->value;
		}
		if (!drawing->submitted) {
			// Replace the previous submission once, then append further submissions in this draw.
			drawing->clear();
			drawing->submitted = true;
			on_canvas_redraw(instance_id);
		}
		// A currently empty scrollport still needs to be redrawn when content scrolls into it.
		if (p_runs.empty() && p_diagnostics.empty()) return;

		RenderingServer& server = *RenderingServer::get_singleton();
		const Ref<TextServer> text_server = TextServerManager::get_singleton()->get_primary_interface();
		if (drawing->items.empty()) drawing->add_item(server, p_canvas.get_canvas_item(), -1);
		const RID root = drawing->items.front();
		RID target;
		std::optional<Rect2> previous_clip;
		Ref<ShaderMaterial> previous_material;
		HTMLPlaneTransform previous_matrix;
		HTMLTextureSampling previous_sampling;
		bool previous_fixed_background = false;
		const auto transform = p_canvas.get_global_transform_with_canvas();
		const Vector2 scale = Vector2(transform[0].length(), transform[1].length()).max(Vector2(1.0, 1.0));
		const Rect2 visible = transform.is_finite() && transform.determinant() != 0 ?
			transform.affine_inverse().xform(p_canvas.get_viewport_rect()) : Rect2();
		const auto* viewport = p_canvas.get_viewport();
		const bool hdr = viewport->is_using_hdr_2d() && server.get_current_rendering_method() != "gl_compatibility";
		const auto configure_run_item = [&](const RID& p_item, const HTMLPaintRun& p_run) {
			if (hdr && p_run.material.is_valid()) {
				server.canvas_item_set_instance_shader_parameter(p_item, "hdr_output", true);
			}
			if (p_run.sampling.filter != HTMLTextureFilter::inherited) {
				server.canvas_item_set_default_texture_filter(p_item, to_canvas_filter(p_run.sampling.filter));
			}
			if (p_run.sampling.repeat) {
				server.canvas_item_set_default_texture_repeat(p_item, RenderingServer::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
			}
		};
		const auto add_ink_canvas = [&](const HTMLPaintRun& p_run, const RID& p_parent, const Ref<ShaderMaterial>& p_ink) {
			if (p_ink.is_null()) return RID();
			const RID item = drawing->add_item(server, p_parent, 0);
			server.canvas_item_set_material(item, p_ink->get_rid());
			configure_run_item(item, p_run);
			if (hdr && p_run.material.is_null()) server.canvas_item_set_instance_shader_parameter(item, "hdr_output", true);
			return item;
		};
		const auto submit_projected = [&](const HTMLPaintRun& p_run, const RID& p_parent, const RID& p_viewport,
			const std::optional<Rect2>& p_clip) {
			Rect2 source_bounds;
			for (const auto& command : p_run.commands) include_bounds(source_bounds, command_bounds(command));
			Rect2 area = p_run.matrix.xform(source_bounds);
			if (p_clip) area = area.intersection(*p_clip);
			const auto inverse = p_run.matrix.try_inverse();
			if (!area.has_area() || !area.is_finite() || !inverse) return;
			const Rect2 sampled = p_clip ? source_bounds.intersection(inverse->xform(area)) : source_bounds;
			if (!sampled.has_area() || !sampled.is_finite()) return;

			// Use the viewport to bound raster resolution. Canvas motion can later reveal the same run.
			const Vector2 extent = visible.has_area() ? area.size.min(visible.size) : area.size;
			const Vector2 resolution = (extent * scale * 4.0f).clamp(Vector2(2, 2), Vector2(8192, 8192));
			const auto tangent = p_run.matrix.affine_at(sampled.get_center());
			const Vector2 magnification((tangent[0] * scale).length(), (tangent[1] * scale).length());
			const Vector2 raster_scale = scale.max(magnification).min(resolution / sampled.size);
			const Vector2 origin = (sampled.position * raster_scale).floor();
			const Vector2i pixels((sampled.get_end() * raster_scale).ceil() - origin);
			if (projection_shader.is_null()) {
				projection_shader.instantiate();
				projection_shader->set_code(PROJECTION_SHADER);
			}
			const auto ink = partial_selection_material(p_run, p_selection, text_server.ptr(), Vector2(), p_selection_only);
			if (ink.is_valid()) drawing->materials.push_back(ink);
			for (int y = 0; y < pixels.y; y += GROUP_TILE_SIZE) {
				for (int x = 0; x < pixels.x; x += GROUP_TILE_SIZE) {
					const Vector2 start = origin + Vector2(x, y);
					const Vector2 size(std::min(GROUP_TILE_SIZE, pixels.x - x), std::min(GROUP_TILE_SIZE, pixels.y - y));
					const Rect2 tile(start / raster_scale, size / raster_scale);
					const Rect2 destination = p_run.matrix.xform(tile).intersection(area);
					if (!destination.has_area()) continue;
					const Vector2 buffer_start = start - Vector2(1, 1);
					const Transform2D mapping(Vector2(raster_scale.x, 0), Vector2(0, raster_scale.y), -buffer_start);
					const auto surface = drawing->add_surface(server, p_viewport, Vector2i(size) + Vector2i(2, 2), mapping, hdr);
					const RID source = drawing->add_item(server, surface.canvas, 0);
					if (p_run.material.is_valid()) server.canvas_item_set_material(source, p_run.material->get_rid());
					configure_run_item(source, p_run);
					p_run.draw(server, source, Vector2(), text_server.ptr(), p_search, p_selection,
						add_ink_canvas(p_run, source, ink), p_selection_only);

					Ref<ShaderMaterial> material;
					material.instantiate();
					material->set_shader(projection_shader);
					material->set_shader_parameter("inverse_x", inverse->row(0));
					material->set_shader_parameter("inverse_y", inverse->row(1));
					material->set_shader_parameter("inverse_w", inverse->row(2));
					material->set_shader_parameter("tile", Vector4(tile.position.x, tile.position.y, tile.get_end().x, tile.get_end().y));
					material->set_shader_parameter("source_origin", buffer_start);
					material->set_shader_parameter("source_scale", raster_scale);
					const RID item = drawing->add_item(server, p_parent, drawing->run_count++);
					server.canvas_item_set_material(item, material->get_rid());
					server.canvas_item_set_transform(item, Transform2D(0.0, p_position));
					server.canvas_item_set_default_texture_filter(item, RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR);
					server.canvas_item_add_texture_rect(item, destination, server.viewport_get_texture(surface.viewport));
					drawing->materials.push_back(material);
				}
			}
			if (p_run.material.is_valid()) drawing->materials.push_back(p_run.material);
		};
		const auto submit_run = [&](const HTMLPaintRun& p_run, const RID& p_parent, const RID& p_viewport, const std::optional<Rect2>& p_limit) {
			std::optional<Rect2> clip;
			if (p_run.clip) {
				clip = translate_rect(*p_run.clip, p_position);
			}
			if (p_limit) {
				clip = clip ? clip->intersection(*p_limit) : p_limit;
			}
			if (clip && !clip->has_area()) {
				return;
			}
			if (!p_run.matrix.is_affine()) {
				// Offscreen runs still interleave with affine neighbors in document paint order.
				submit_projected(p_run, p_parent, p_viewport, clip ? std::optional<Rect2>(translate_rect(*clip, -p_position)) : std::nullopt);
				target = RID();
				return;
			}

			const bool transformed = p_run.material.is_valid() || p_run.matrix != Transform2D();
			const Vector2 offset = transformed ? Vector2() : p_position;
			const auto ink = partial_selection_material(p_run, p_selection, text_server.ptr(), offset, p_selection_only);
			if (ink.is_valid()) target = RID();
			if (!target.is_valid() || clip != previous_clip || p_run.material != previous_material || p_run.matrix != previous_matrix ||
				p_run.sampling != previous_sampling || p_run.fixed_background != previous_fixed_background) {
				const int32_t index = drawing->run_count++;
				if (clip) {
					const RID clip_item = drawing->add_item(server, p_parent, index);
					server.canvas_item_set_custom_rect(clip_item, true, *clip);
					server.canvas_item_set_clip(clip_item, true);
					target = drawing->add_item(server, clip_item, 0);
				}
				else {
					target = drawing->add_item(server, p_parent, index);
				}
				if (p_run.material.is_valid()) {
					server.canvas_item_set_material(target, p_run.material->get_rid());
					drawing->materials.push_back(p_run.material);
				}
				if (transformed) {
					server.canvas_item_set_transform(target, Transform2D(0.0, p_position) * p_run.matrix.affine_at(Vector2()));
				}
				if (p_run.fixed_background && p_viewport_origin != Vector2()) {
					const Vector2 offset = p_run.matrix.inverse().affine_at(p_viewport_origin).basis_xform(p_viewport_origin);
					server.canvas_item_set_instance_shader_parameter(target, "scroll_origin", offset);
				}
				configure_run_item(target, p_run);
				previous_clip = clip;
				previous_material = p_run.material;
				previous_matrix = p_run.matrix;
				previous_sampling = p_run.sampling;
				previous_fixed_background = p_run.fixed_background;
			}
			p_run.draw(server, target, offset, text_server.ptr(), p_search, p_selection,
				add_ink_canvas(p_run, target, ink), p_selection_only);
			if (ink.is_valid()) {
				drawing->materials.push_back(ink);
				target = RID();
			}
		};
		const std::optional<Rect2> clip = p_clip == Rect2() ? std::nullopt : std::optional<Rect2>(p_clip);
		if (p_selection_only) {
			for (const HTMLPaintRun& run : p_runs) {
				for (const HTMLPaintCommand& command : run.commands) {
					const auto* text = std::get_if<HTMLText>(&command);
					if (!text || !text->selectable || text->offset < 0 || text->length <= 0) continue;
					const Vector2i range = text_highlight_range(*text, p_selection);
					if (range.x >= range.y) continue;
					HTMLPaintRun selected_run = run;
					selected_run.commands.clear();
					selected_run.commands.push_back(*text);
					submit_run(selected_run, root, viewport->get_viewport_rid(), clip);
				}
			}
			return;
		}
		if (p_groups.empty()) {
			for (const auto& run : p_runs) submit_run(run, root, viewport->get_viewport_rid(), clip);
		} else {
			if (opacity_material.is_null()) {
				Ref<Shader> shader;
				shader.instantiate();
				shader->set_code(OPACITY_SHADER);
				opacity_material.instantiate();
				opacity_material->set_shader(shader);
			}
			const auto bounds = group_bounds(p_runs, p_groups);
			const auto first_surface = drawing->surfaces.size();
			drawing->order_passes = std::any_of(p_groups.begin(), p_groups.end(), [](const HTMLPaintGroup& p_group) {
				return p_group.captures_filter_backdrop;
			});
			std::vector<Vector2> backdrop_margins(p_groups.size());
			for (std::size_t i = p_groups.size(); i-- > 0;) {
				const auto& group = p_groups[i];
				for (std::size_t child = i + 1; child < group.after_group; child = p_groups[child].after_group) {
					const auto& inner = p_groups[child];
					const auto margin = filter_halo(inner.backdrop_filters, inner.matrix, scale);
					backdrop_margins[i] = backdrop_margins[i].max(margin);
					if (!inner.backdrop_root) backdrop_margins[i] = backdrop_margins[i].max(backdrop_margins[child]);
				}
			}
			struct BackdropSample {
				Surface source;
				Rect2 buffer;
			};
			struct BlendBackdrop {
				Surface current;
				RID parent_viewport;
				Vector2i pixels;
				Transform2D mapping;
				Rect2 buffer;
				std::vector<BackdropSample> behind;
			};
			const auto copy_sample = [&](const BackdropSample& p_sample, const Surface& p_target, int32_t p_order,
				const Ref<ShaderMaterial>& p_material = Ref<ShaderMaterial>(), const std::optional<Rect2>& p_clip = std::nullopt) {
				if (drawing->order_passes)
					drawing->texture_dependencies.emplace_back(p_sample.source.viewport, p_target.viewport);
				RID parent = p_target.canvas;
				if (p_clip) {
					parent = drawing->add_item(server, parent, p_order);
					server.canvas_item_set_custom_rect(parent, true, *p_clip);
					server.canvas_item_set_clip(parent, true);
				}
				const RID item = drawing->add_item(server, parent, p_clip ? 0 : p_order);
				server.canvas_item_set_material(item, (p_material.is_valid() ? p_material : opacity_material)->get_rid());
				server.canvas_item_set_default_texture_filter(item, RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
				Rect2 destination = p_sample.buffer;
				if (p_material.is_valid()) {
					drawing->materials.push_back(p_material);
					server.canvas_item_set_transform(item, Transform2D(0.0, p_position));
					destination.position -= p_position;
				}
				server.canvas_item_add_texture_rect(item, destination, server.viewport_get_texture(p_sample.source.viewport));
			};
			const auto advance_capture = [&](BlendBackdrop& r_backdrop) {
				const auto old = r_backdrop.current;
				const auto next = drawing->add_surface(server, r_backdrop.parent_viewport, r_backdrop.pixels, r_backdrop.mapping, hdr);
				drawing->set_surface_parent(server, old.viewport, next.viewport);
				copy_sample({ old, r_backdrop.buffer }, next, -1);
				r_backdrop.current = next;
				return old;
			};
			const auto advance_backdrop = [&](BlendBackdrop& r_backdrop, const Surface& p_source, litehtml::css_blend_mode p_mode,
				bool p_reparent_source = true) {
				if (element_blend_shader.is_null()) {
					element_blend_shader.instantiate();
					element_blend_shader->set_code(String("#define HTML_ELEMENT_BLEND\n") + html_blend_shader);
				}
				const auto old = advance_capture(r_backdrop);
				const auto next = r_backdrop.current;
				if (p_reparent_source) drawing->set_surface_parent(server, p_source.viewport, next.viewport);
				else if (drawing->order_passes) drawing->texture_dependencies.emplace_back(p_source.viewport, next.viewport);
				const RID old_texture = server.viewport_get_texture(old.viewport);

				Ref<ShaderMaterial> material;
				material.instantiate();
				material->set_shader(element_blend_shader);
				material->set_shader_parameter("mode", static_cast<int>(p_mode));
				material->set_shader_parameter("hdr", hdr);
				material->set_shader_parameter("backdrop", old_texture);
				const auto& box = r_backdrop.buffer;
				material->set_shader_parameter("backdrop_rect", Vector4(box.position.x, box.position.y, box.size.x, box.size.y));
				const RID item = drawing->add_item(server, next.canvas, drawing->run_count++);
				server.canvas_item_set_material(item, material->get_rid());
				drawing->materials.push_back(material);
				return item;
			};
			const auto apply_backdrop = [&](const HTMLPaintGroup& p_group, const std::vector<BackdropSample>& p_samples,
				const Surface& p_foreground, const RID& p_parent, const Rect2& p_buffer, const Vector2i& p_size,
				const Transform2D& p_mapping) {
				const auto result = drawing->add_surface(server, p_parent, p_size, p_mapping, hdr);
				int32_t order = 0;
				for (const auto& area : p_group.backdrop_areas) {
					const auto& clip = area.clip;
					const auto projected = translate_rect(clip.matrix.xform(clip.shape.rect), p_position).intersection(p_buffer);
					if (!projected.has_area() || !projected.is_finite()) continue;
					const auto matrix = clip.matrix.affine_at(clip.shape.rect.get_center());
					const auto margin = filter_halo(p_group.backdrop_filters, matrix, scale);
					const Vector2 origin = (projected.position * scale).floor() - margin;
					const auto pixels = Vector2i((projected.get_end() * scale).ceil() + margin - origin).max(Vector2i(2, 2));
					const Transform2D mapping(Vector2(scale.x, 0), Vector2(0, scale.y), -origin);
					const Rect2 buffer(origin / scale, Vector2(pixels) / scale);
					const auto input = drawing->add_surface(server, result.viewport, pixels, mapping, hdr);
					int32_t layer = 0;
					for (const auto& sample : p_samples) copy_sample(sample, input, layer++, area.input_material);
					const HTMLPlaneTransform uv_to_document(Transform2D(Vector2(buffer.size.x, 0), Vector2(0, buffer.size.y),
						buffer.position - p_position));
					const auto uv_to_box = clip.matrix.inverse() * uv_to_document;
					const FilterRegion region{ clip.shape.rect, uv_to_box, uv_to_box.inverse() };
					const auto filtered = filter_surface(server, *drawing, input, result.viewport, pixels, scale,
						p_group.backdrop_filters, matrix, hdr, &region);
					const auto clip_box = area.output_clip ? std::optional<Rect2>(translate_rect(*area.output_clip, p_position)) : std::nullopt;
					copy_sample({ filtered, buffer }, result, order++, area.material, clip_box);
				}
				drawing->set_surface_parent(server, p_foreground.viewport, result.viewport);
				copy_sample({ p_foreground, p_buffer }, result, order);
				return result;
			};
			const auto submit_background = [&](const auto& p_submit_range, std::size_t p_stack, const RID& p_viewport,
				const Vector2i& p_size, const Transform2D& p_mapping, const Rect2& p_buffer, const Rect2& p_clip) {
				Surface backdrop;
				for (std::size_t layer = p_stack + 1; layer < p_groups[p_stack].after_group; layer = p_groups[layer].after_group) {
					const auto& group = p_groups[layer];
					if (!translate_rect(bounds[layer], p_position).intersects(p_clip)) continue;
					const bool blend = backdrop.viewport.is_valid() && (hdr || group.blend_mode != litehtml::css_blend_mode::normal);
					const auto source = blend || !backdrop.viewport.is_valid() ?
						drawing->add_surface(server, p_viewport, p_size, p_mapping, hdr) : backdrop;
					p_submit_range(p_submit_range, group.first_run, group.last_run, layer + 1, group.after_group,
						source.canvas, source.viewport, p_clip);
					if (!blend) {
						backdrop = source;
						continue;
					}

					if (background_blend_shader.is_null()) {
						background_blend_shader.instantiate();
						background_blend_shader->set_code(html_blend_shader);
					}
					Ref<ShaderMaterial> material;
					material.instantiate();
					material->set_shader(background_blend_shader);
					material->set_shader_parameter("mode", static_cast<int>(group.blend_mode));
					material->set_shader_parameter("hdr", hdr);
					material->set_shader_parameter("backdrop", server.viewport_get_texture(backdrop.viewport));
					const auto result = drawing->add_surface(server, p_viewport, p_size, p_mapping, hdr);
					drawing->set_surface_parent(server, backdrop.viewport, result.viewport);
					drawing->set_surface_parent(server, source.viewport, result.viewport);
					const RID item = drawing->add_item(server, result.canvas, -1);
					server.canvas_item_set_material(item, material->get_rid());
					server.canvas_item_set_default_texture_filter(item, RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
					server.canvas_item_add_texture_rect(item, p_buffer, server.viewport_get_texture(source.viewport));
					drawing->materials.push_back(material);
					backdrop = result;
				}
				return backdrop;
			};

			const auto apply_mask = [&](const HTMLPaintGroup& p_group, const Surface& p_contents, const RID& p_viewport,
				const Vector2i& p_size, const Transform2D& p_mapping, const Rect2& p_buffer) {
				if (mask_shader.is_null()) {
					mask_shader.instantiate();
					mask_shader->set_code(MASK_SHADER);
				}
				const auto compose = [&](const Surface& p_source, const Surface& p_lower, int p_op,
					bool p_luminance, bool p_linear_rgb) {
					const auto result = drawing->add_surface(server, p_viewport, p_size, p_mapping, hdr);
					drawing->set_surface_parent(server, p_source.viewport, result.viewport);
					Ref<ShaderMaterial> material;
					material.instantiate();
					material->set_shader(mask_shader);
					material->set_shader_parameter("operation", p_op);
					material->set_shader_parameter("luminance", p_luminance);
					material->set_shader_parameter("linear_rgb", p_linear_rgb);
					material->set_shader_parameter("hdr", hdr);
					if (p_lower.viewport.is_valid()) {
						drawing->set_surface_parent(server, p_lower.viewport, result.viewport);
						material->set_shader_parameter("has_lower", true);
						material->set_shader_parameter("lower_layers", server.viewport_get_texture(p_lower.viewport));
					}
					const RID item = drawing->add_item(server, result.canvas, 0);
					server.canvas_item_set_material(item, material->get_rid());
					server.canvas_item_set_default_texture_filter(item, RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
					server.canvas_item_add_texture_rect(item, p_buffer, server.viewport_get_texture(p_source.viewport));
					drawing->materials.push_back(material);
					return result;
				};
				Surface lower;
				for (auto layer = p_group.mask.rbegin(); layer != p_group.mask.rend(); ++layer) {
					const auto source = drawing->add_surface(server, p_viewport, p_size, p_mapping, hdr);
					target = RID();
					for (const auto& run : layer->runs) submit_run(run, source.canvas, source.viewport, p_buffer);
					lower = compose(source, lower, layer->composite, layer->luminance, layer->linear_rgb);
				}
				target = RID();
				return compose(p_contents, lower, MASK_APPLY, false, false);
			};

			const auto submit_depth_scene = [&](const auto& p_submit_range, std::size_t p_scene, const Surface& p_surface,
				const Rect2& p_clip, BlendBackdrop* p_backdrop) {
				struct Layer {
					std::size_t index;
					bool group;
					Surface surface{};
					Vector2 origin;
					Vector2i size;
					bool blended = false;
				};
				std::vector<Layer> layers;
				std::vector<HTMLDepthLayer> depths;
				const auto& scene = p_groups[p_scene];
				const auto add_layer = [&](std::size_t p_index, bool p_is_group, const Rect2& p_bounds,
					const std::optional<Vector3>& p_plane) {
					const Rect2 area = translate_rect(p_bounds, p_position).intersection(p_clip);
					if (!area.has_area() || !area.is_finite()) return;
					auto depth = p_plane.value_or(Vector3());
					depth.z -= Vector2(depth.x, depth.y).dot(p_position);
					layers.push_back({p_index, p_is_group});
					depths.push_back({area, depth});
				};
				std::size_t run = scene.first_run;
				const auto add_runs = [&](std::size_t p_end) {
					for (; run < p_end; ++run) add_layer(run, false, run_bounds(p_runs[run]), p_runs[run].depth_plane);
				};
				for (std::size_t child = p_scene + 1; child < scene.after_group; child = p_groups[child].after_group) {
					const auto& group = p_groups[child];
					add_runs(group.first_run);
					add_layer(child, true, bounds[child], group.depth_plane);
					run = group.last_run;
				}
				add_runs(scene.last_run);
				const auto slices = sort_html_depth_layers(depths);
				std::vector<std::size_t> slice_counts(layers.size());
				for (const auto& slice : slices) ++slice_counts[slice.layer];
				const auto draw_layer = [&](const Layer& p_layer, const Surface& p_target, const Rect2& p_limit,
					BlendBackdrop* p_scope, bool p_blend_output) {
					target = RID();
					if (p_layer.group) {
						const auto& group = p_groups[p_layer.index];
						p_submit_range(p_submit_range, group.first_run, group.last_run, p_layer.index, group.after_group,
							p_target.canvas, p_target.viewport, p_limit, p_scope, p_blend_output);
					} else {
						submit_run(p_runs[p_layer.index], p_target.canvas, p_target.viewport, p_limit);
					}
				};
				for (const auto& slice : slices) {
					auto& layer = layers[slice.layer];
					const auto& layer_bounds = depths[slice.layer].bounds;
					if (slice_counts[slice.layer] == 1) {
						const Surface target_surface = p_backdrop ? p_backdrop->current : p_surface;
						draw_layer(layer, target_surface, p_clip, p_backdrop, true);
						continue;
					}
					const bool samples_backdrop = p_backdrop && layer.group &&
						(!p_groups[layer.index].backdrop_areas.empty() || p_groups[layer.index].needs_parent_backdrop);
					// Reuse rasterization unless this slice has to sample the paint order at its depth.
					if (!layer.surface.viewport.is_valid() || samples_backdrop) {
						std::vector<BackdropSample> behind;
						if (samples_backdrop) {
							behind = p_backdrop->behind;
							behind.push_back({ advance_capture(*p_backdrop), p_backdrop->buffer });
						}
						const RID viewport = p_backdrop ? p_backdrop->current.viewport : p_surface.viewport;
						layer.origin = (layer_bounds.position * scale).floor();
						layer.size = Vector2i((layer_bounds.get_end() * scale).ceil() - layer.origin).max(Vector2i(2, 2));
						const Transform2D mapping(Vector2(scale.x, 0), Vector2(0, scale.y), -layer.origin);
						layer.surface = drawing->add_surface(server, viewport, layer.size, mapping, hdr);
						BlendBackdrop isolated{layer.surface, viewport, layer.size, mapping,
							Rect2(layer.origin / scale, Vector2(layer.size) / scale), std::move(behind)};
						draw_layer(layer, isolated.current, layer_bounds, samples_backdrop ? &isolated : nullptr, false);
						layer.surface = isolated.current;
						layer.blended = false;
					}
					PackedVector2Array points, uvs;
					for (const auto& point : slice.polygon) {
						points.push_back(point);
						uvs.push_back((point * scale - layer.origin) / Vector2(layer.size));
					}
					PackedColorArray colors;
					colors.push_back(Color(1, 1, 1, 1));
					RID item;
					if (p_backdrop && layer.group && p_groups[layer.index].element_blend_mode) {
						item = advance_backdrop(*p_backdrop, layer.surface, *p_groups[layer.index].element_blend_mode, !layer.blended);
						layer.blended = true;
					} else {
						const Surface target_surface = p_backdrop ? p_backdrop->current : p_surface;
						if (drawing->order_passes)
							drawing->texture_dependencies.emplace_back(layer.surface.viewport, target_surface.viewport);
						item = drawing->add_item(server, target_surface.canvas, drawing->run_count++);
						server.canvas_item_set_material(item, opacity_material->get_rid());
					}
					server.canvas_item_add_polygon(item, points, colors, uvs, server.viewport_get_texture(layer.surface.viewport));
				}
			};
			const auto submit_range = [&](auto&& self, std::size_t p_first, std::size_t p_last, std::size_t p_child, std::size_t p_after,
				const RID& p_parent, const RID& p_viewport, const std::optional<Rect2>& p_limit,
				BlendBackdrop* p_backdrop = nullptr, bool p_blend_output = true) -> void {
				target = RID();
				const auto active_surface = [&]() {
					return p_backdrop ? p_backdrop->current : Surface{p_viewport, p_parent};
				};
				const auto submit_runs_until = [&](std::size_t p_end) {
					const auto destination = active_surface();
					for (; p_first < p_end; ++p_first)
						submit_run(p_runs[p_first], destination.canvas, destination.viewport, p_limit);
				};
				for (std::size_t child = p_child; child < p_after; child = p_groups[child].after_group) {
					const auto& group = p_groups[child];
					submit_runs_until(group.first_run);
					p_first = group.last_run;
					if (group.can_draw_directly && !group.captures_backdrop && !group.captures_filter_backdrop) {
						const auto destination = active_surface();
						self(self, group.first_run, group.last_run, child + 1, group.after_group, destination.canvas,
							destination.viewport, p_limit, p_backdrop);
						target = RID();
						continue;
					}
					if (!bounds[child].has_area()) continue;
					const bool mix = p_backdrop && p_blend_output && group.element_blend_mode.has_value();
					Rect2 area = translate_rect(bounds[child], p_position);
					if (p_limit) area = area.intersection(*p_limit);
					// Large layers only allocate the region sampled by this draw.
					if ((area.size * scale).length_squared() > 2.0f * GROUP_TILE_SIZE * GROUP_TILE_SIZE)
						area = area.intersection(visible);
					if (!area.has_area() || !area.is_finite() || !scale.is_finite()) continue;
					const Vector2 origin = (area.position * scale).floor();
					const Vector2 extent = (area.get_end() * scale).ceil() - origin;
					constexpr double max_dimension = std::numeric_limits<int32_t>::max();
					if (!extent.is_finite() || extent.x <= 0 || extent.y <= 0 || extent.x > max_dimension || extent.y > max_dimension) continue;
					const Vector2i pixels(extent);
					std::vector<BackdropSample> samples;
					if (p_backdrop && (!group.backdrop_areas.empty() || group.needs_parent_backdrop)) {
						samples = p_backdrop->behind;
						samples.push_back({ advance_capture(*p_backdrop), p_backdrop->buffer });
					}
					const bool captures_filters = group.captures_filter_backdrop || group.needs_parent_backdrop;
					const Vector2 foreground_margin = filter_halo(group.filters, group.matrix, scale);
					const Vector2 halo = (foreground_margin + (captures_filters ? backdrop_margins[child] : Vector2()))
						.min(Vector2(MAX_TILE_HALO, MAX_TILE_HALO));
					const Vector2i buffer_margin(halo * 2.0f);
					for (int32_t y = 0; y < pixels.y;) {
						const int32_t height = std::min(GROUP_TILE_SIZE, pixels.y - y);
						for (int32_t x = 0; x < pixels.x;) {
							const Vector2i size(std::min(GROUP_TILE_SIZE, pixels.x - x), height);
							const Vector2 start = origin + Vector2(x, y);
							x += size.x;
							const Rect2 tile(start / scale, Vector2(size) / scale);
							const Rect2 tile_clip = area.intersection(tile);
							if (!tile_clip.has_area()) continue;
							const auto destination_surface = active_surface();
							if (group.sorts_depth && !group.captures_backdrop && !captures_filters) {
								submit_depth_scene(self, child, destination_surface, tile_clip, nullptr);
								continue;
							}
							const Vector2 buffer_start = start - halo;
							const auto buffer_size = (size + buffer_margin).max(Vector2i(2, 2));
							const Transform2D mapping(Vector2(scale.x, 0.0), Vector2(0.0, scale.y), -buffer_start);
							const Rect2 buffer(buffer_start / scale, Vector2(buffer_size) / scale);
							Surface source;
							if (group.blends_background) {
								source = submit_background(self, child, destination_surface.viewport, buffer_size, mapping, buffer, tile_clip);
								if (!source.viewport.is_valid()) continue;
							} else {
								source = drawing->add_surface(server, destination_surface.viewport, buffer_size, mapping, hdr);
								BlendBackdrop nested{source, destination_surface.viewport, buffer_size, mapping, buffer,
									group.backdrop_root ? std::vector<BackdropSample>() : samples};
								auto* nested_backdrop = group.captures_backdrop || captures_filters ? &nested : nullptr;
								const Rect2 input_clip = halo == Vector2() ? tile_clip : buffer;
								if (group.sorts_depth) {
									submit_depth_scene(self, child, source, input_clip, nested_backdrop);
								} else {
									self(self, group.first_run, group.last_run, child + 1, group.after_group, source.canvas,
										source.viewport, input_clip, nested_backdrop);
								}
								source = nested.current;
							}
							if (!samples.empty() && !group.backdrop_areas.empty())
								source = apply_backdrop(group, samples, source, destination_surface.viewport, buffer, buffer_size, mapping);
							auto surface = filter_surface(server, *drawing, source, destination_surface.viewport, buffer_size, scale,
								group.filters, group.matrix, hdr);
							if (!group.mask.empty())
								surface = apply_mask(group, surface, destination_surface.viewport, buffer_size, mapping, buffer);
							if (mix && group.output_material.is_valid()) {
								const auto masked = drawing->add_surface(server, destination_surface.viewport, buffer_size, mapping, hdr);
								drawing->set_surface_parent(server, surface.viewport, masked.viewport);
								const RID mask_item = drawing->add_item(server, masked.canvas, 0);
								server.canvas_item_set_material(mask_item, group.output_material->get_rid());
								drawing->materials.push_back(group.output_material);
								server.canvas_item_set_transform(mask_item, Transform2D(0.0, p_position));
								server.canvas_item_add_texture_rect(mask_item, translate_rect(buffer, -p_position),
									server.viewport_get_texture(surface.viewport));
								surface = masked;
							}
							RID parent = destination_surface.canvas;
							if (!mix && halo != Vector2() && tile != tile_clip) {
								parent = drawing->add_item(server, parent, drawing->run_count++);
								server.canvas_item_set_custom_rect(parent, true, tile_clip);
								server.canvas_item_set_clip(parent, true);
							}
							const RID item = mix ? advance_backdrop(*p_backdrop, surface, *group.element_blend_mode) :
								drawing->add_item(server, parent, drawing->run_count++);
							Rect2 destination = tile;
							Rect2 texture_region(halo, Vector2(size));
							if (mix) {
								if (halo != Vector2() && tile != tile_clip) {
									destination = tile_clip;
									texture_region.position += (tile_clip.position - tile.position) * scale;
									texture_region.size = tile_clip.size * scale;
								}
							} else if (group.output_material.is_valid()) {
								server.canvas_item_set_material(item, group.output_material->get_rid());
								drawing->materials.push_back(group.output_material);
								server.canvas_item_set_transform(item, Transform2D(0.0, p_position));
								destination.position -= p_position;
							} else {
								server.canvas_item_set_material(item, opacity_material->get_rid());
							}
							server.canvas_item_add_texture_rect_region(item, destination, server.viewport_get_texture(surface.viewport),
								texture_region, Color(1.0, 1.0, 1.0, group.opacity));
						}
						y += height;
					}
					target = RID();
				}
				submit_runs_until(p_last);
			};
			submit_range(submit_range, 0, p_runs.size(), 0, p_groups.size(), root, viewport->get_viewport_rid(), clip);
			if (drawing->order_passes) drawing->order_surface_passes(server, viewport->get_viewport_rid(), first_surface);
		}
		target = RID();
		for (const auto& run : p_diagnostics) {
			submit_run(run, root, viewport->get_viewport_rid(), clip);
		}
	}

	void HTMLCanvasRenderer::queue_redraws() const {
		for (const auto& drawing : drawings) {
			if (auto* canvas = Object::cast_to<CanvasItem>(ObjectDB::get_instance(ObjectID(drawing.key)))) {
				canvas->queue_redraw();
			}
		}
	}

	void HTMLCanvasRenderer::on_canvas_redraw(uint64_t p_canvas_id) {
		CanvasDrawing* drawing = drawings.getptr(p_canvas_id);
		if (!drawing || drawing->redraw_id != 0) {
			return;
		}
		// Another draw listener may have already submitted HTML before this signal arrives.
		drawing->redraw_id = ++next_redraw_id;
		callable_mp_static(&HTMLCanvasRenderer::finish_canvas_redraw).call_deferred(get_instance_id(), p_canvas_id, drawing->redraw_id);
	}

	void HTMLCanvasRenderer::finish_canvas_redraw(uint64_t p_renderer_id, uint64_t p_canvas_id, uint64_t p_redraw_id) {
		auto* renderer = Object::cast_to<HTMLCanvasRenderer>(ObjectDB::get_instance(ObjectID(p_renderer_id)));
		if (!renderer) {
			return;
		}
		CanvasDrawing* drawing = renderer->drawings.getptr(p_canvas_id);
		if (!drawing || drawing->redraw_id != p_redraw_id) {
			return;
		}
		if (!drawing->submitted) {
			drawing->clear();
		}
		drawing->submitted = false;
		drawing->redraw_id = 0;
	}

	void HTMLCanvasRenderer::detach_canvas(uint64_t p_canvas_id) {
		CanvasDrawing* drawing = drawings.getptr(p_canvas_id);
		if (!drawing) {
			return;
		}
		if (auto* canvas = Object::cast_to<CanvasItem>(ObjectDB::get_instance(ObjectID(p_canvas_id)))) {
			canvas->disconnect("draw", drawing->redraw);
			canvas->disconnect("tree_exiting", drawing->exiting);
		}
		drawing->clear();
		drawings.erase(p_canvas_id);
	}
}
