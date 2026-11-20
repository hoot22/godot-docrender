#include "html_label.h"

#include "formats/html/elements/html_input_node.h"
#include "formats/html/paint/html_paint_recorder.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;
using dockit::HTMLControlIcon;
using dockit::HTMLInputNode;

namespace {
	constexpr float panel_width = 234.0f;
	constexpr float eyedropper_diameter = 112.0f;
	constexpr int eyedropper_pixels = 11;

	constexpr const char* eyedropper_shader = R"(
shader_type canvas_item;
render_mode unshaded;

void fragment() {
	vec2 cells = vec2(textureSize(TEXTURE, 0));
	vec2 point = UV * cells;
	vec2 from_center = abs(point - cells * 0.5);
	vec2 grid = min(fract(point), 1.0 - fract(point));
	float radius = cells.x * 0.5;
	float distance_from_center = length(from_center);
	float edge = smoothstep(radius - 0.18, radius - 0.10, distance_from_center);
	float coverage = 1.0 - smoothstep(radius - 0.08, radius, distance_from_center);
	float grid_line = 1.0 - smoothstep(0.025, 0.075, min(grid.x, grid.y));
	float center = max(from_center.x, from_center.y);
	float center_outer = smoothstep(0.43, 0.50, center) * (1.0 - smoothstep(0.61, 0.68, center));
	float center_inner = smoothstep(0.33, 0.41, center) * (1.0 - smoothstep(0.49, 0.56, center));
	vec3 red = vec3(0.88, 0.05, 0.05);

	vec3 color = mix(texture(TEXTURE, UV).rgb, red, grid_line * 0.72);
	color = mix(color, vec3(1.0), center_outer);
	color = mix(color, red, max(edge, center_inner));
	COLOR = vec4(color, coverage);
}
)";

	Vector3 color_hsl(const Color& p_color, float p_hue) {
		const float light = (std::max({ p_color.r, p_color.g, p_color.b }) +
			std::min({ p_color.r, p_color.g, p_color.b })) * 0.5f;
		const float chroma = p_color.get_v() * p_color.get_s();
		const float denom = 1.0f - std::abs(2.0f * light - 1.0f);
		return { p_hue, denom > 0.0f ? std::clamp(chroma / denom, 0.0f, 1.0f) : 0.0f, light };
	}

	String color_value(const Color& p_color) {
		return vformat("rgb(%d %d %d / %s)", int(std::lround(p_color.r * 255.0f)), int(std::lround(p_color.g * 255.0f)),
			int(std::lround(p_color.b * 255.0f)), String::num(p_color.a, 8));
	}

	void draw_gradient(Control& p_canvas, const Rect2& p_rect, const Color& p_start, const Color& p_end, bool p_vertical) {
		const auto from = p_rect.position;
		const auto to = p_rect.get_end();
		const PackedVector2Array vertices(Array::make(from, Vector2(to.x, from.y), to, Vector2(from.x, to.y)));
		const PackedColorArray colors(Array::make(p_start, p_vertical ? p_start : p_end,
			p_end, p_vertical ? p_end : p_start));
		p_canvas.draw_polygon(vertices, colors);
	}

	void draw_checker(Control& p_canvas, const Rect2& p_rect) {
		p_canvas.draw_rect(p_rect, Color(1, 1, 1));
		for (int y = 0; y * 4 < p_rect.size.y; ++y) {
			for (int x = y % 2; x * 4 < p_rect.size.x; x += 2) {
				const Rect2 tile(p_rect.position + Vector2(x * 4, y * 4), Vector2(4, 4));
				p_canvas.draw_rect(p_rect.intersection(tile), Color(0.82f, 0.82f, 0.82f));
			}
		}
	}

	bool can_use_eyedropper() {
		return DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_MOUSE);
	}
}

Color HTMLLabel::ColorPickerState::color() const {
	return Color::from_hsv(h, s, v, a);
}

Vector2 HTMLLabel::ColorPickerState::size() const {
	return { panel_width, (alpha ? 284.0f : 252.0f) + (presets.empty() ? 0.0f : 28.0f) };
}

Rect2 HTMLLabel::ColorPickerState::part_rect(int p_part) const {
	switch (p_part) {
		case well: return { 1, 1, 232, 137 };
		case hue: return { 88, 154, 130, 12 };
		case opacity: return alpha ? Rect2(24, 192, 186, 14) : Rect2();
		case eyedropper: return can_use_eyedropper() ? Rect2(5, 145, 32, 32) : Rect2();
		case format: return { 18, alpha ? 249.0f : 217.0f, 198, 25 };
		default:
			if (p_part >= channel && p_part < preset) {
				const float y = alpha ? 219.0f : 187.0f;
				if (format_index == hex) return p_part == channel ? Rect2(24, y, 186, 25) : Rect2();
				return { 24.0f + (p_part - channel) * 64.0f, y, 58, 25 };
			}
			if (p_part >= preset && size_t(p_part - preset) < presets.size()) {
				return { 12.0f + (p_part - preset) * 18.0f, (alpha ? 284.0f : 252.0f) + 2.0f, 16, 16 };
			}
			return {};
	}
}

String HTMLLabel::ColorPickerState::field_text(int p_channel) const {
	const Color current = color();
	if (format_index == hex) return "#" + current.to_html(alpha).to_upper();
	if (format_index == hsl) {
		const Vector3 hsl_color = color_hsl(current, h);
		return String::num_int64(std::lround(hsl_color[p_channel] * (p_channel == 0 ? 360.0f : 100.0f))) +
			(p_channel == 0 ? "" : "%");
	}
	return String::num_int64(std::lround(current[p_channel] * 255.0f));
}

Error HTMLLabel::show_color_picker() {
	const auto input = picker_input->get_input();
	color_picker.alpha = input->get_attr("alpha");
	const Color current = HTMLInputNode::color_preview(input->get_value());
	color_picker.h = current.get_h();
	color_picker.s = current.get_s();
	color_picker.v = current.get_v();
	color_picker.a = color_picker.alpha ? current.a : 1.0f;
	for (const auto& row : layout->suggestion_menu_rows(picker_input)) {
		color_picker.presets.push_back(row.value);
		if (color_picker.presets.size() == 12) break;
	}
	if (!color_picker_canvas) {
		color_picker_canvas = memnew(Control);
		color_picker_canvas->set_name("_HTMLColorPicker");
		color_picker_canvas->set_mouse_filter(MOUSE_FILTER_STOP);
		color_picker_canvas->set_z_index(2);
		color_picker_canvas->hide();
		add_child(color_picker_canvas, false, INTERNAL_MODE_BACK);
		color_picker_canvas->connect("draw", callable_mp(this, &HTMLLabel::draw_color_picker));
	}
	update_color_picker();
	if (picker_type != "color") return ERR_UNAVAILABLE;
	set_process_input(true);
	return OK;
}

void HTMLLabel::update_color_picker() {
	if (!place_input_picker_canvas(*color_picker_canvas, color_picker.size())) return;
	if (color_eyedropper && color_eyedropper->viewport != get_viewport()->get_visible_rect()) stop_color_eyedropper();
	color_picker_canvas->show();
	color_picker_canvas->queue_redraw();
}

Color HTMLLabel::ColorEyedropper::sample(const Vector2i& p_pixel) const {
	const Vector2i pos = p_pixel.clamp(Vector2i(), frame->get_size() - Vector2i(1, 1));
	const Color color = frame->get_pixelv(pos);
	return linear ? color.linear_to_srgb() : color;
}

void HTMLLabel::start_color_eyedropper(const Vector2& p_position) {
	if (!can_use_eyedropper() || color_eyedropper) return;
	const auto* viewport = get_viewport();
	const Rect2 bounds = viewport->get_visible_rect();
	if (!bounds.has_area()) return;
	// Use one frame for both the lens and the selected color, captured before drawing the lens.
	const auto texture = viewport->get_texture();
	const auto frame = texture.is_valid() ? texture->get_image() : Ref<Image>();
	if (frame.is_null() || frame->is_empty()) return;

	if (!color_eyedropper_canvas) {
		color_eyedropper_canvas = memnew(TextureRect);
		color_eyedropper_canvas->set_name("_HTMLColorEyedropper");
		color_eyedropper_canvas->set_mouse_filter(MOUSE_FILTER_IGNORE);
		color_eyedropper_canvas->set_z_index(color_picker_canvas->get_z_index() - 1);
		color_eyedropper_canvas->set_texture_filter(TEXTURE_FILTER_NEAREST);
		color_eyedropper_canvas->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		color_eyedropper_canvas->set_size(Vector2(eyedropper_diameter, eyedropper_diameter));
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(eyedropper_shader);
		Ref<ShaderMaterial> material;
		material.instantiate();
		material->set_shader(shader);
		color_eyedropper_canvas->set_material(material);
		color_eyedropper_canvas->hide();
		add_child(color_eyedropper_canvas, false, INTERNAL_MODE_BACK);
	}
	auto* display = DisplayServer::get_singleton();
	const auto previous = display->mouse_get_mode();
	const auto hidden = previous == DisplayServer::MOUSE_MODE_CONFINED || previous == DisplayServer::MOUSE_MODE_CONFINED_HIDDEN ?
		DisplayServer::MOUSE_MODE_CONFINED_HIDDEN : DisplayServer::MOUSE_MODE_HIDDEN;
	color_eyedropper.emplace(ColorEyedropper{frame, bounds, previous, hidden, viewport->is_using_hdr_2d()});
	display->mouse_set_mode(hidden);
	move_color_eyedropper(p_position);
}

void HTMLLabel::stop_color_eyedropper() {
	if (!color_eyedropper) return;
	const auto session = std::exchange(color_eyedropper, std::nullopt);
	color_eyedropper_canvas->hide();
	color_eyedropper_canvas->set_texture({});
	auto* display = DisplayServer::get_singleton();
	if (display && display->mouse_get_mode() == session->hidden_mode) display->mouse_set_mode(session->previous_mode);
}

void HTMLLabel::move_color_eyedropper(const Vector2& p_position) {
	auto& session = *color_eyedropper;
	if (get_viewport()->get_visible_rect() != session.viewport) {
		stop_color_eyedropper();
		return;
	}
	const auto transform = get_global_transform_with_canvas();
	if (!session.viewport.has_point(p_position) || transform.determinant() == 0.0f) {
		color_eyedropper_canvas->hide();
		session.pixel = Vector2i(-1, -1);
		return;
	}
	const Vector2 image_position = (p_position - session.viewport.position) * Vector2(session.frame->get_size()) / session.viewport.size;
	const Vector2i pixel = Vector2i(image_position.floor()).clamp(Vector2i(), session.frame->get_size() - Vector2i(1, 1));
	if (session.pixel != pixel) {
		session.pixel = pixel;
		const auto tile = Image::create_empty(eyedropper_pixels, eyedropper_pixels, false, Image::FORMAT_RGBA8);
		const Vector2i start = pixel - Vector2i(eyedropper_pixels / 2, eyedropper_pixels / 2);
		for (int y = 0; y < eyedropper_pixels; ++y) {
			for (int x = 0; x < eyedropper_pixels; ++x) {
				tile->set_pixel(x, y, session.sample(start + Vector2i(x, y)));
			}
		}
		const Ref<ImageTexture> texture = color_eyedropper_canvas->get_texture();
		if (texture.is_valid()) texture->update(tile);
		else color_eyedropper_canvas->set_texture(ImageTexture::create_from_image(tile));
	}
	const Vector2 center = transform.affine_inverse().xform(p_position);
	color_eyedropper_canvas->set_position(center - Vector2(eyedropper_diameter / 2, eyedropper_diameter / 2));
	color_eyedropper_canvas->show();
}

bool HTMLLabel::handle_color_eyedropper_input(const Ref<InputEvent>& p_event) {
	const Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		const auto code = key->get_keycode() == KEY_NONE ? key->get_physical_keycode() : key->get_keycode();
		if (key->is_pressed() && code == KEY_ESCAPE) stop_color_eyedropper();
		return true;
	}
	const Ref<InputEventMouse> mouse = p_event;
	if (mouse.is_null()) return false;
	move_color_eyedropper(mouse->get_position());
	if (!color_eyedropper) return true;
	const Ref<InputEventMouseButton> button = p_event;
	if (button.is_null() || button->get_button_index() != MOUSE_BUTTON_LEFT) return true;
	auto& session = *color_eyedropper;
	if (button->is_pressed()) {
		session.pressed = session.pixel.x >= 0;
	} else if (std::exchange(session.pressed, false) && !button->is_canceled() && session.pixel.x >= 0) {
		const auto sample = session.sample(session.pixel);
		stop_color_eyedropper();
		update_color_picker_value(color_value(Color(sample.r, sample.g, sample.b, color_picker.a)));
		select_color_picker_part(ColorPickerState::well);
	}
	return true;
}

void HTMLLabel::draw_color_picker() {
	if (!input_picker_valid()) return;
	auto& surface = *color_picker_canvas;
	const auto& state = color_picker;
	const bool dark = layout->get_element_color_scheme(picker_input) == litehtml::color_scheme::dark;
	const Color face = dark ? Color::html("#292a2d") : Color(1, 1, 1);
	const Color ink = dark ? Color::html("#e8eaed") : Color::html("#202124");
	const Color edge = dark ? Color::html("#606368") : Color::html("#bfbfbf");
	const Color hover = dark ? Color::html("#383a3f") : Color::html("#f7f7f7");
	const Color focus = dark ? Color::html("#8ab4f8") : Color::html("#1967d2");
	const Color current = state.color();
	const Rect2 panel(Vector2(), state.size());
	surface.draw_rect(panel, face);
	surface.draw_rect(panel.grow(-0.5f), edge, false, 1);
	const Rect2 well = state.part_rect(ColorPickerState::well);
	draw_gradient(surface, well, Color(1, 1, 1), Color::from_hsv(state.h, 1, 1), false);
	draw_gradient(surface, well, Color(0, 0, 0, 0), Color(0, 0, 0), true);
	const Rect2 hue = state.part_rect(ColorPickerState::hue);
	for (int i = 0; i < 6; ++i) {
		const float width = hue.size.x / 6.0f;
		draw_gradient(surface, Rect2(hue.position + Vector2(width * i, 0), Vector2(width, hue.size.y)),
			Color::from_hsv(i / 6.0f, 1, 1), Color::from_hsv((i + 1) % 6 / 6.0f, 1, 1), false);
	}
	const auto ring = [&](Vector2 center, Color fill, bool focused) {
		surface.draw_circle(center, 8.0f, Color(0, 0, 0, 0.36f), true, -1, true);
		surface.draw_circle(center, 6.8f, fill, true, -1, true);
		surface.draw_circle(center, 6.8f, focused ? focus : Color(1, 1, 1), false, 2, true);
	};
	ring(well.position + Vector2(state.s, 1.0f - state.v) * well.size, current, state.active == ColorPickerState::well);
	ring(Vector2(hue.position.x + state.h * hue.size.x, hue.get_center().y),
		Color::from_hsv(state.h, 1, 1), state.active == ColorPickerState::hue);
	if (state.alpha) {
		const auto alpha = state.part_rect(ColorPickerState::opacity);
		draw_checker(surface, alpha);
		draw_gradient(surface, alpha, Color(current.r, current.g, current.b, 0), Color(current.r, current.g, current.b), false);
		surface.draw_rect(alpha, edge, false, 1);
		ring(Vector2(alpha.position.x + state.a * alpha.size.x, alpha.get_center().y), current,
			state.active == ColorPickerState::opacity);
	}
	const Rect2 eye = state.part_rect(ColorPickerState::eyedropper);
	if (eye.has_area()) {
		if (state.hovered == ColorPickerState::eyedropper) surface.draw_rect(eye, hover);
		const auto icon = layout->get_control_icon(HTMLControlIcon::colorize);
		if (icon.is_valid()) surface.draw_texture_rect(icon, eye.grow(-6), false, ink);
		if (state.active == ColorPickerState::eyedropper) surface.draw_rect(eye, focus, false, 1);
	}
	const Vector2 sample(56, 160);
	surface.draw_circle(sample, 16, edge, true, -1, true);
	surface.draw_circle(sample, 15, face.blend(current), true, -1, true);
	const auto text_server = TextServerManager::get_singleton()->get_primary_interface();
	const auto draw_text = [&](const String& p_text, const Rect2& p_rect, Color p_ink) {
		const auto line = layout->shape_control_text(picker_input, p_text);
		if (!line.is_valid() || text_server.is_null()) return;
		const Vector2 origin = p_rect.get_center() + Vector2(-line.width() * 0.5f, (line.line_ascent() - line.line_descent()) * 0.5f);
		text_server->shaped_text_draw(line.get_rid(), surface.get_canvas_item(), origin, -1, -1, p_ink);
	};
	for (int i = 0; i < 3; ++i) {
		const int part = ColorPickerState::channel + i;
		const Rect2 field = state.part_rect(part);
		if (!field.has_area()) continue;
		const bool active = state.active == part;
		surface.draw_rect(field, active && state.replace_text ? focus : face);
		surface.draw_rect(field, active ? focus : edge, false, 1);
		draw_text(active ? state.editing : state.field_text(i), field, active && state.replace_text ? face : ink);
	}
	const Rect2 format = state.part_rect(ColorPickerState::format);
	if (state.hovered == ColorPickerState::format) surface.draw_rect(format, hover);
	if (state.active == ColorPickerState::format) surface.draw_rect(format, focus, false, 1);
	const char* labels[] = { "RGB", "HSL", "HEX" };
	draw_text(labels[state.format_index], format, ink);
	const auto format_icon = layout->get_control_icon(HTMLControlIcon::unfold_more);
	if (format_icon.is_valid()) surface.draw_texture_rect(format_icon,
		Rect2(format.get_end().x - 23, format.get_center().y - 9, 18, 18), false, ink);
	for (int i = 0; i < int(state.presets.size()); ++i) {
		const int part = ColorPickerState::preset + i;
		const Rect2 tile = state.part_rect(part);
		const auto color = HTMLInputNode::color_preview(state.presets[i]);
		surface.draw_rect(tile, face.blend(color));
		surface.draw_rect(tile, state.active == part || state.hovered == part ? focus : edge, false, 1);
	}
}

void HTMLLabel::select_color_picker_part(int p_part) {
	auto& state = color_picker;
	state.active = p_part;
	state.replace_text = p_part >= ColorPickerState::channel && p_part < ColorPickerState::preset;
	state.editing = state.replace_text ? state.field_text(p_part - ColorPickerState::channel) : String();
	color_picker_canvas->queue_redraw();
}

void HTMLLabel::update_color_picker_preview() {
	const auto input = picker_input->get_input();
	if (!input) return;
	input->set_color_preview(color_picker.color());
	layout->repaint_input();
	color_picker_canvas->queue_redraw();
}

void HTMLLabel::update_color_picker_value(const String& p_value) {
	auto& state = color_picker;
	const auto input = picker_input->get_input();
	state.value = input->sanitize_value(p_value);
	const Color current = HTMLInputNode::color_preview(state.value);
	if (current.get_s() > 0.0f) state.h = current.get_h();
	state.s = current.get_s();
	state.v = current.get_v();
	state.a = current.a;
	update_color_picker_preview();
}

void HTMLLabel::update_color_picker_point(int p_part, const Vector2& p_point) {
	auto& state = color_picker;
	const Rect2 shape = state.part_rect(p_part);
	if (!shape.has_area()) return;
	const auto unit = ((p_point - shape.position) / shape.size).clamp(Vector2(), Vector2(1, 1));
	switch (p_part) {
		case ColorPickerState::well: state.s = unit.x; state.v = 1.0f - unit.y; break;
		case ColorPickerState::hue: state.h = unit.x; break;
		case ColorPickerState::opacity: state.a = unit.x; break;
		default: return;
	}
	state.value = color_value(state.color());
	update_color_picker_preview();
}

void HTMLLabel::update_color_picker_field() {
	const auto& state = color_picker;
	const int component = state.active - ColorPickerState::channel;
	if (component < 0 || component >= 3) return;
	if (state.format_index == ColorPickerState::hex) {
		const String value = state.editing.begins_with("#") ? state.editing : "#" + state.editing;
		if ((value.length() == 7 || (state.alpha && value.length() == 9)) && Color::html_is_valid(value)) {
			Color parsed = Color::html(value);
			if (value.length() == 7) parsed.a = state.a;
			update_color_picker_value(color_value(parsed));
		}
		return;
	}
	const String edit = state.editing.trim_suffix("%");
	if (!edit.is_valid_int()) return;
	const int64_t value = edit.to_int();
	if (state.format_index == ColorPickerState::rgb) {
		if (value < 0 || value > 255) return;
		Color current = state.color();
		current[component] = value / 255.0f;
		update_color_picker_value(color_value(current));
	} else {
		const int limit = component == 0 ? 360 : 100;
		if (value < 0 || value > limit) return;
		Vector3 hsl = color_hsl(state.color(), state.h);
		hsl[component] = float(value) / limit;
		if (component == 0) color_picker.h = hsl.x;
		update_color_picker_value(vformat("hsl(%s %s%% %s%% / %s)", String::num(hsl.x * 360, 6),
			String::num(hsl.y * 100, 6), String::num(hsl.z * 100, 6), String::num(state.a, 8)));
	}
}

void HTMLLabel::finish_color_picker(bool p_commit) {
	const String value = color_picker.value;
	if (p_commit && !value.is_empty()) commit_picker_value(value);
	else close_input_picker();
}

bool HTMLLabel::handle_color_picker_input(const Ref<InputEvent>& p_event) {
	if (!input_picker_valid()) {
		close_input_picker();
		return false;
	}
	if (color_eyedropper) return handle_color_eyedropper_input(p_event);
	using Part = ColorPickerState;
	auto& state = color_picker;
	const Ref<InputEventMouse> mouse = p_event;
	const Ref<InputEventMouseButton> button = p_event;
	const Ref<InputEventMouseMotion> motion = p_event;
	const Ref<InputEventKey> key = p_event;
	const auto activate = [&](int part) {
		if (part == Part::format) {
			state.format_index = (state.format_index + 1) % 3;
			color_picker_canvas->queue_redraw();
		} else if (part == Part::eyedropper) {
			start_color_eyedropper(mouse.is_valid() ? mouse->get_position() : get_viewport()->get_mouse_position());
		} else if (part >= Part::preset) {
			update_color_picker_value(state.presets[part - Part::preset]);
			finish_color_picker(true);
		}
	};
	const int end = Part::preset + int(state.presets.size());
	if (mouse.is_valid()) {
		const Vector2 point = color_picker_canvas->get_global_transform_with_canvas().affine_inverse().xform(mouse->get_position());
		const bool inside = Rect2(Vector2(), state.size()).has_point(point);
		int part = -1;
		for (int i = 0; inside && i < end; ++i) {
			if (state.part_rect(i).has_point(point)) { part = i; break; }
		}
		if (motion.is_valid()) {
			if (state.dragging >= 0) update_color_picker_point(state.dragging, point);
			if (std::exchange(state.hovered, part) != part) color_picker_canvas->queue_redraw();
			return inside || state.dragging >= 0;
		}
		if (button.is_null() || button->get_button_index() != MOUSE_BUTTON_LEFT) return inside;
		if (!button->is_pressed()) {
			const int drag = std::exchange(state.dragging, -1);
			if (drag >= 0 && !button->is_canceled()) update_color_picker_point(drag, point);
			return inside || drag >= 0;
		}
		if (!inside) {
			const Vector2 local = get_global_transform_with_canvas().affine_inverse().xform(mouse->get_position());
			const bool anchor = get_element_at(local) == picker_input;
			finish_color_picker(true);
			return anchor;
		}
		if (part < 0) return true;
		select_color_picker_part(part);
		if (part <= Part::opacity) {
			state.dragging = part;
			update_color_picker_point(part, point);
		} else {
			activate(part);
		}
		return true;
	}
	if (key.is_null()) return false;
	if (!key->is_pressed()) return true;
	const Key code = key->get_keycode() != KEY_NONE ? key->get_keycode() : key->get_physical_keycode();
	if (code == KEY_ESCAPE) {
		finish_color_picker(false);
		return true;
	}
	if (code == KEY_ENTER || code == KEY_KP_ENTER) {
		if (state.active >= Part::preset || state.active == Part::eyedropper || state.active == Part::format) activate(state.active);
		else finish_color_picker(true);
		return true;
	}
	if (code == KEY_TAB) {
		const int direction = key->is_shift_pressed() ? -1 : 1;
		int next = state.active < 0 ? (direction > 0 ? end - 1 : 0) : state.active;
		do { next = (next + direction + end) % end; } while (!state.part_rect(next).has_area());
		select_color_picker_part(next);
		return true;
	}
	const bool field = state.active >= Part::channel && state.active < Part::preset;
	if (field) {
		if (key->is_action("ui_text_select_all", true)) state.replace_text = true;
		else if (key->is_action("ui_copy", true)) DisplayServer::get_singleton()->clipboard_set(state.editing);
		else {
			String text;
			if (key->is_action("ui_paste", true)) text = DisplayServer::get_singleton()->clipboard_get().strip_edges();
			else if (key->get_unicode() && !key->is_ctrl_pressed() && !key->is_meta_pressed()) text = String::chr(key->get_unicode());
			const int max_length = state.format_index == Part::hex ? (state.alpha ? 9 : 7) : 4;
			if (code == KEY_BACKSPACE || code == KEY_DELETE) {
				state.editing = state.replace_text ? String() : state.editing.substr(0, std::max<int64_t>(0, state.editing.length() - 1));
				state.replace_text = false;
			} else if (!text.is_empty()) {
				if (state.replace_text) state.editing = String();
				state.editing = (state.editing + text).substr(0, max_length);
				state.replace_text = false;
			}
			update_color_picker_field();
		}
		color_picker_canvas->queue_redraw();
		return true;
	}
	const float dx = code == KEY_LEFT ? -1 : code == KEY_RIGHT ? 1 : 0;
	const float dy = code == KEY_UP ? -1 : code == KEY_DOWN ? 1 : 0;
	if ((dx || dy) && state.active >= Part::well && state.active <= Part::opacity) {
		const Rect2 shape = state.part_rect(state.active);
		const Vector2 unit = state.active == Part::well ? Vector2(state.s, 1.0f - state.v) :
			Vector2(state.active == Part::hue ? state.h : state.a, 0);
		const Vector2 step = state.active == Part::well ? Vector2(dx, dy) : Vector2(dx - dy, 0);
		update_color_picker_point(state.active, shape.position + unit * shape.size + step * (key->is_shift_pressed() ? 10 : 1));
	}
	return true;
}
