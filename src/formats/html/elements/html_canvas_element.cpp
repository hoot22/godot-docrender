#include "html_canvas_element.h"
#include "html_canvas_node.h"

#include "formats/html/backend/litehtml_container.h"
#include "formats/html/css/css_parser.h"
#include "formats/html/css/css_web_color.h"
#include "formats/html/html_layout.h"

#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <litehtml/render_image.h>
#include <litehtml/document.h>
#include <litehtml/el_image.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>

using namespace godot;

namespace {
	constexpr int64_t MAX_CANVAS_DIMENSION = 8192;
	constexpr int64_t MAX_CANVAS_PIXELS = 16 * 1024 * 1024;

	int64_t canvas_dimension(const String& p_value, int64_t p_fallback) {
		const String value = p_value.strip_edges();
		if (!value.is_valid_int()) return p_fallback;
		return std::clamp(value.to_int(), int64_t(1), MAX_CANVAS_DIMENSION);
	}

	Color source_over(const Color& p_source, const Color& p_destination) {
		const real_t alpha = p_source.a + p_destination.a * (1.0 - p_source.a);
		if (alpha <= 0.0) return Color(0, 0, 0, 0);
		return Color((p_source.r * p_source.a + p_destination.r * p_destination.a * (1.0 - p_source.a)) / alpha,
			(p_source.g * p_source.a + p_destination.g * p_destination.a * (1.0 - p_source.a)) / alpha,
			(p_source.b * p_source.a + p_destination.b * p_destination.a * (1.0 - p_source.a)) / alpha, alpha);
	}

	Ref<Image> image_from_source(const Variant& p_source) {
		if (p_source.get_type() != Variant::OBJECT) return {};
		Object* source = p_source;
		if (Image* image = Object::cast_to<Image>(source)) return Ref<Image>(image);
		if (Texture2D* texture = Object::cast_to<Texture2D>(source)) return texture->get_image();
		if (HTMLCanvasElement* canvas = Object::cast_to<HTMLCanvasElement>(source)) return canvas->get_image();
		return {};
	}

	bool is_canvas_paint(const Variant& p_style) {
		if (p_style.get_type() != Variant::OBJECT) return false;
		Object* object = p_style;
		return Object::cast_to<HTMLCanvasGradient>(object) || Object::cast_to<HTMLCanvasPattern>(object);
	}

	std::optional<Color> parse_canvas_color(const String& p_value) {
		const auto tokens = dockit::css::normalize(p_value.utf8().get_data(),
			dockit::css::f_componentize | dockit::css::f_remove_whitespace);
		litehtml::web_color parsed;
		if (tokens.size() != 1 || !litehtml::parse_color(tokens.front(), parsed, nullptr) || parsed.needs_current_color()) return {};
		const auto color = parsed.to_srgb();
		return Color(color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f, color.alpha / 255.0f);
	}
}

HTMLCanvasRenderingContext2D::HTMLCanvasRenderingContext2D() {
	reset(300, 150);
}

void HTMLCanvasRenderingContext2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_fill_style"), &HTMLCanvasRenderingContext2D::get_fill_style);
	ClassDB::bind_method(D_METHOD("set_fill_style", "style"), &HTMLCanvasRenderingContext2D::set_fill_style);
	ClassDB::bind_method(D_METHOD("get_stroke_style"), &HTMLCanvasRenderingContext2D::get_stroke_style);
	ClassDB::bind_method(D_METHOD("set_stroke_style", "style"), &HTMLCanvasRenderingContext2D::set_stroke_style);
	ClassDB::bind_method(D_METHOD("get_line_width"), &HTMLCanvasRenderingContext2D::get_line_width);
	ClassDB::bind_method(D_METHOD("set_line_width", "width"), &HTMLCanvasRenderingContext2D::set_line_width);
	ClassDB::bind_method(D_METHOD("get_font"), &HTMLCanvasRenderingContext2D::get_font);
	ClassDB::bind_method(D_METHOD("set_font", "font"), &HTMLCanvasRenderingContext2D::set_font);
	ClassDB::bind_method(D_METHOD("get_font_size"), &HTMLCanvasRenderingContext2D::get_font_size);
	ClassDB::bind_method(D_METHOD("set_font_size", "size"), &HTMLCanvasRenderingContext2D::set_font_size);
	ClassDB::bind_method(D_METHOD("save"), &HTMLCanvasRenderingContext2D::save);
	ClassDB::bind_method(D_METHOD("restore"), &HTMLCanvasRenderingContext2D::restore);
	ClassDB::bind_method(D_METHOD("reset_transform"), &HTMLCanvasRenderingContext2D::reset_transform);
	ClassDB::bind_method(D_METHOD("translate", "x", "y"), &HTMLCanvasRenderingContext2D::translate);
	ClassDB::bind_method(D_METHOD("scale", "x", "y"), &HTMLCanvasRenderingContext2D::scale);
	ClassDB::bind_method(D_METHOD("rotate", "angle"), &HTMLCanvasRenderingContext2D::rotate);
	ClassDB::bind_method(D_METHOD("set_transform", "a", "b", "c", "d", "e", "f"), &HTMLCanvasRenderingContext2D::set_transform);
	ClassDB::bind_method(D_METHOD("clear_rect", "x", "y", "width", "height"), &HTMLCanvasRenderingContext2D::clear_rect);
	ClassDB::bind_method(D_METHOD("fill_rect", "x", "y", "width", "height"), &HTMLCanvasRenderingContext2D::fill_rect);
	ClassDB::bind_method(D_METHOD("stroke_rect", "x", "y", "width", "height"), &HTMLCanvasRenderingContext2D::stroke_rect);
	ClassDB::bind_method(D_METHOD("begin_path"), &HTMLCanvasRenderingContext2D::begin_path);
	ClassDB::bind_method(D_METHOD("close_path"), &HTMLCanvasRenderingContext2D::close_path);
	ClassDB::bind_method(D_METHOD("move_to", "x", "y"), &HTMLCanvasRenderingContext2D::move_to);
	ClassDB::bind_method(D_METHOD("line_to", "x", "y"), &HTMLCanvasRenderingContext2D::line_to);
	ClassDB::bind_method(D_METHOD("rect", "x", "y", "width", "height"), &HTMLCanvasRenderingContext2D::rect);
	ClassDB::bind_method(D_METHOD("arc", "x", "y", "radius", "start_angle", "end_angle", "counterclockwise"),
		&HTMLCanvasRenderingContext2D::arc, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("fill"), &HTMLCanvasRenderingContext2D::fill);
	ClassDB::bind_method(D_METHOD("stroke"), &HTMLCanvasRenderingContext2D::stroke);
	ClassDB::bind_method(D_METHOD("fill_text", "text", "x", "y"), &HTMLCanvasRenderingContext2D::fill_text);
	ClassDB::bind_method(D_METHOD("create_linear_gradient", "x0", "y0", "x1", "y1"), &HTMLCanvasRenderingContext2D::create_linear_gradient);
	ClassDB::bind_method(D_METHOD("create_pattern", "source", "repeat"), &HTMLCanvasRenderingContext2D::create_pattern, DEFVAL("repeat"));
	ClassDB::bind_method(D_METHOD("draw_image", "source", "dx", "dy"), &HTMLCanvasRenderingContext2D::draw_image);
	ClassDB::bind_method(D_METHOD("draw_image_sized", "source", "dx", "dy", "dw", "dh"), &HTMLCanvasRenderingContext2D::draw_image_sized);
	ClassDB::bind_method(D_METHOD("draw_image_region", "source", "sx", "sy", "sw", "sh", "dx", "dy", "dw", "dh"),
		&HTMLCanvasRenderingContext2D::draw_image_region);
	ClassDB::bind_method(D_METHOD("put_image_data", "data", "dx", "dy"), &HTMLCanvasRenderingContext2D::put_image_data);
	ClassDB::bind_method(D_METHOD("get_image_data", "sx", "sy", "sw", "sh"), &HTMLCanvasRenderingContext2D::get_image_data);
	ClassDB::bind_method(D_METHOD("get_image"), &HTMLCanvasRenderingContext2D::get_image);
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "fill_style", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT),
		"set_fill_style", "get_fill_style");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "stroke_style", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT),
		"set_stroke_style", "get_stroke_style");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "line_width"), "set_line_width", "get_line_width");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_font", "get_font");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "font_size"), "set_font_size", "get_font_size");
}

void HTMLCanvasGradient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_color_stop", "offset", "color"), &HTMLCanvasGradient::add_color_stop);
}

Error HTMLCanvasGradient::add_color_stop(double p_offset, const Variant& p_color) {
	if (!std::isfinite(p_offset) || p_offset < 0.0 || p_offset > 1.0) return ERR_INVALID_PARAMETER;
	Color color;
	if (p_color.get_type() == Variant::COLOR) color = p_color;
	else if (p_color.get_type() == Variant::STRING) {
		const auto parsed = parse_canvas_color(String(p_color));
		if (!parsed) return ERR_INVALID_PARAMETER;
		color = *parsed;
	} else return ERR_INVALID_PARAMETER;
	stops.push_back({ p_offset, color });
	std::stable_sort(stops.begin(), stops.end(), [](const Stop& p_left, const Stop& p_right) { return p_left.offset < p_right.offset; });
	return OK;
}

void HTMLCanvasGradient::configure(const Vector2& p_start, const Vector2& p_end) {
	start = p_start;
	end = p_end;
	stops.clear();
}

Color HTMLCanvasGradient::sample(const Vector2& p_point) const {
	if (stops.empty()) return Color(0, 0, 0, 0);
	const Vector2 axis = end - start;
	const double denominator = axis.length_squared();
	const double offset = denominator > 0.0 ? (p_point - start).dot(axis) / denominator : 0.0;
	if (offset <= stops.front().offset) return stops.front().color;
	if (offset >= stops.back().offset) return stops.back().color;
	for (size_t index = 1; index < stops.size(); ++index) {
		if (offset <= stops[index].offset) {
			const Stop& left = stops[index - 1];
			const Stop& right = stops[index];
			const double fraction = (offset - left.offset) / (right.offset - left.offset);
			return left.color.lerp(right.color, fraction);
		}
	}
	return stops.back().color;
}

void HTMLCanvasPattern::_bind_methods() {}

void HTMLCanvasPattern::configure(const Ref<Image>& p_image, const String& p_repeat) {
	image = p_image;
	repeat_x = p_repeat == "repeat" || p_repeat == "repeat-x";
	repeat_y = p_repeat == "repeat" || p_repeat == "repeat-y";
}

Color HTMLCanvasPattern::sample(const Vector2& p_point) const {
	if (image.is_null() || image->is_empty()) return Color(0, 0, 0, 0);
	double x = p_point.x;
	double y = p_point.y;
	if (repeat_x) { x = std::fmod(x, image->get_width()); if (x < 0.0) x += image->get_width(); }
	else if (x < 0.0 || x >= image->get_width()) return Color(0, 0, 0, 0);
	if (repeat_y) { y = std::fmod(y, image->get_height()); if (y < 0.0) y += image->get_height(); }
	else if (y < 0.0 || y >= image->get_height()) return Color(0, 0, 0, 0);
	return image->get_pixel(static_cast<int32_t>(x), static_cast<int32_t>(y));
}

void HTMLCanvasRenderingContext2D::mark_dirty() {
	texture_dirty = true;
	if (auto* canvas = Object::cast_to<HTMLCanvasElement>(ObjectDB::get_instance(owner_id))) canvas->canvas_changed();
}

void HTMLCanvasRenderingContext2D::reset(int64_t p_width, int64_t p_height) {
	p_width = std::clamp(p_width, int64_t(1), MAX_CANVAS_DIMENSION);
	p_height = std::clamp(p_height, int64_t(1), MAX_CANVAS_DIMENSION);
	if (p_width * p_height > MAX_CANVAS_PIXELS) p_height = std::max<int64_t>(1, MAX_CANVAS_PIXELS / p_width);
	image = Image::create(static_cast<int32_t>(p_width), static_cast<int32_t>(p_height), false, Image::FORMAT_RGBA8);
	image->fill(Color(0, 0, 0, 0));
	texture.unref();
	texture_dirty = true;
	states.clear();
	state = {};
	path.clear();
	path_closed = false;
}

void HTMLCanvasRenderingContext2D::set_fill_style(const Variant& p_style) {
	if (p_style.get_type() == Variant::COLOR || is_canvas_paint(p_style)) state.fill_style = p_style;
	else if (p_style.get_type() == Variant::STRING) {
		if (const auto color = parse_canvas_color(String(p_style))) state.fill_style = *color;
	}
}
void HTMLCanvasRenderingContext2D::set_stroke_style(const Variant& p_style) {
	if (p_style.get_type() == Variant::COLOR || is_canvas_paint(p_style)) state.stroke_style = p_style;
	else if (p_style.get_type() == Variant::STRING) {
		if (const auto color = parse_canvas_color(String(p_style))) state.stroke_style = *color;
	}
}
void HTMLCanvasRenderingContext2D::set_line_width(double p_width) {
	if (std::isfinite(p_width) && p_width > 0.0) state.line_width = static_cast<float>(std::min(p_width, 256.0));
}
void HTMLCanvasRenderingContext2D::set_font(const Ref<Font>& p_font) { state.font = p_font; }
void HTMLCanvasRenderingContext2D::set_font_size(int32_t p_size) { if (p_size > 0 && p_size <= 512) state.font_size = p_size; }
void HTMLCanvasRenderingContext2D::save() { states.push_back(state); }
void HTMLCanvasRenderingContext2D::restore() { if (!states.empty()) { state = states.back(); states.pop_back(); } }
void HTMLCanvasRenderingContext2D::reset_transform() { state.transform = Transform2D(); }
void HTMLCanvasRenderingContext2D::translate(double p_x, double p_y) { state.transform.translate_local(Vector2(p_x, p_y)); }
void HTMLCanvasRenderingContext2D::scale(double p_x, double p_y) { state.transform = state.transform.scaled_local(Vector2(p_x, p_y)); }
void HTMLCanvasRenderingContext2D::rotate(double p_angle) { state.transform = state.transform.rotated_local(p_angle); }
void HTMLCanvasRenderingContext2D::set_transform(double p_a, double p_b, double p_c, double p_d, double p_e, double p_f) {
	state.transform = Transform2D(Vector2(p_a, p_b), Vector2(p_c, p_d), Vector2(p_e, p_f));
}

void HTMLCanvasRenderingContext2D::put_pixel(const Vector2& p_point, const Color& p_color) {
	const int x = static_cast<int>(std::floor(p_point.x));
	const int y = static_cast<int>(std::floor(p_point.y));
	if (x < 0 || y < 0 || x >= image->get_width() || y >= image->get_height()) return;
	image->set_pixel(x, y, source_over(p_color, image->get_pixel(x, y)));
}

void HTMLCanvasRenderingContext2D::clear_rect(double p_x, double p_y, double p_width, double p_height) {
	if (std::abs(state.transform.determinant()) < CMP_EPSILON) return;
	const Vector2 points[] = {state.transform.xform(Vector2(p_x, p_y)), state.transform.xform(Vector2(p_x + p_width, p_y)),
		state.transform.xform(Vector2(p_x + p_width, p_y + p_height)), state.transform.xform(Vector2(p_x, p_y + p_height))};
	for (const Vector2& point : points) if (!point.is_finite()) return;
	Rect2 bounds(points[0], Vector2());
	for (int i = 1; i < 4; ++i) bounds = bounds.expand(points[i]);
	const int left = static_cast<int>(std::floor(std::clamp<double>(bounds.position.x, 0.0, image->get_width())));
	const int top = static_cast<int>(std::floor(std::clamp<double>(bounds.position.y, 0.0, image->get_height())));
	const int right = static_cast<int>(std::ceil(std::clamp<double>(bounds.get_end().x, 0.0, image->get_width())));
	const int bottom = static_cast<int>(std::ceil(std::clamp<double>(bounds.get_end().y, 0.0, image->get_height())));
	for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) {
		const Vector2 point(x + 0.5f, y + 0.5f);
		bool inside = false;
		for (int i = 0, j = 3; i < 4; j = i++) {
			const Vector2& a = points[i];
			const Vector2& b = points[j];
			if (((a.y > point.y) != (b.y > point.y)) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
		}
		if (inside) image->set_pixel(x, y, Color(0, 0, 0, 0));
	}
	mark_dirty();
}

Color HTMLCanvasRenderingContext2D::style_color(const Variant& p_style, const Vector2& p_position) const {
	if (p_style.get_type() == Variant::COLOR) return p_style;
	if (p_style.get_type() == Variant::STRING) {
		if (const auto color = parse_canvas_color(String(p_style))) return *color;
	}
	if (p_style.get_type() == Variant::OBJECT) {
		Object* object = p_style;
		const Vector2 local = std::abs(state.transform.determinant()) < CMP_EPSILON
			? p_position : state.transform.affine_inverse().xform(p_position);
		if (const auto* gradient = Object::cast_to<HTMLCanvasGradient>(object)) return gradient->sample(local);
		if (const auto* pattern = Object::cast_to<HTMLCanvasPattern>(object)) return pattern->sample(local);
	}
	return Color(0, 0, 0, 1);
}

void HTMLCanvasRenderingContext2D::fill_polygon(const std::vector<Vector2>& p_points, const Variant& p_style) {
	if (p_points.size() < 3) return;
	Vector2 minimum = p_points.front();
	Vector2 maximum = minimum;
	for (const Vector2& point : p_points) { minimum.x = std::min(minimum.x, point.x); minimum.y = std::min(minimum.y, point.y); maximum.x = std::max(maximum.x, point.x); maximum.y = std::max(maximum.y, point.y); }
	const int left = std::max(0, int(std::floor(minimum.x)));
	const int top = std::max(0, int(std::floor(minimum.y)));
	const int right = std::min(image->get_width(), int(std::ceil(maximum.x)));
	const int bottom = std::min(image->get_height(), int(std::ceil(maximum.y)));
	for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) {
		const Vector2 point(x + 0.5f, y + 0.5f);
		bool inside = false;
		for (size_t i = 0, j = p_points.size() - 1; i < p_points.size(); j = i++) {
			const Vector2 a = p_points[i], b = p_points[j];
			if (((a.y > point.y) != (b.y > point.y)) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
		}
		if (inside) put_pixel(point, style_color(p_style, point));
	}
}

void HTMLCanvasRenderingContext2D::draw_line_segment(const Vector2& p_from, const Vector2& p_to, const Variant& p_style) {
	const real_t distance = p_from.distance_to(p_to);
	const int steps = std::max(1, int(std::ceil(distance * 1.5)));
	const int radius = std::max(0, int(std::ceil(state.line_width * 0.5f)) - 1);
	for (int i = 0; i <= steps; ++i) {
		const Vector2 point = p_from.lerp(p_to, real_t(i) / steps);
		for (int y = -radius; y <= radius; ++y) for (int x = -radius; x <= radius; ++x) {
			const Vector2 target = point + Vector2(x, y);
			put_pixel(target, style_color(p_style, target));
		}
	}
}

void HTMLCanvasRenderingContext2D::fill_rect(double p_x, double p_y, double p_width, double p_height) {
	fill_polygon({state.transform.xform(Vector2(p_x, p_y)), state.transform.xform(Vector2(p_x + p_width, p_y)),
		state.transform.xform(Vector2(p_x + p_width, p_y + p_height)), state.transform.xform(Vector2(p_x, p_y + p_height))}, state.fill_style);
	mark_dirty();
}

void HTMLCanvasRenderingContext2D::stroke_rect(double p_x, double p_y, double p_width, double p_height) {
	const Vector2 points[] = {state.transform.xform(Vector2(p_x, p_y)), state.transform.xform(Vector2(p_x + p_width, p_y)),
		state.transform.xform(Vector2(p_x + p_width, p_y + p_height)), state.transform.xform(Vector2(p_x, p_y + p_height))};
	for (int i = 0; i < 4; ++i) draw_line_segment(points[i], points[(i + 1) % 4], state.stroke_style);
	mark_dirty();
}

void HTMLCanvasRenderingContext2D::begin_path() { path.clear(); path_closed = false; }
void HTMLCanvasRenderingContext2D::close_path() { path_closed = true; }
void HTMLCanvasRenderingContext2D::move_to(double p_x, double p_y) { path.push_back(state.transform.xform(Vector2(p_x, p_y))); }
void HTMLCanvasRenderingContext2D::line_to(double p_x, double p_y) { path.push_back(state.transform.xform(Vector2(p_x, p_y))); }
void HTMLCanvasRenderingContext2D::rect(double p_x, double p_y, double p_width, double p_height) {
	path.push_back(state.transform.xform(Vector2(p_x, p_y)));
	path.push_back(state.transform.xform(Vector2(p_x + p_width, p_y)));
	path.push_back(state.transform.xform(Vector2(p_x + p_width, p_y + p_height)));
	path.push_back(state.transform.xform(Vector2(p_x, p_y + p_height)));
	path_closed = true;
}
void HTMLCanvasRenderingContext2D::arc(double p_x, double p_y, double p_radius, double p_start_angle, double p_end_angle, bool p_counterclockwise) {
	if (p_radius < 0.0 || !std::isfinite(p_radius)) return;
	constexpr double tau = 6.2831853071795864769;
	if (p_counterclockwise && p_end_angle > p_start_angle) p_end_angle -= tau;
	if (!p_counterclockwise && p_end_angle < p_start_angle) p_end_angle += tau;
	const int steps = std::max(8, int(std::ceil(std::abs(p_end_angle - p_start_angle) * std::max(1.0, p_radius / 2.0))));
	for (int i = 0; i <= steps; ++i) {
		const real_t angle = p_start_angle + (p_end_angle - p_start_angle) * real_t(i) / steps;
		path.push_back(state.transform.xform(Vector2(p_x + std::cos(angle) * p_radius, p_y + std::sin(angle) * p_radius)));
	}
}
void HTMLCanvasRenderingContext2D::fill() { fill_polygon(path, state.fill_style); mark_dirty(); }
void HTMLCanvasRenderingContext2D::stroke() {
	for (size_t i = 1; i < path.size(); ++i) draw_line_segment(path[i - 1], path[i], state.stroke_style);
	if (path_closed && path.size() > 2) draw_line_segment(path.back(), path.front(), state.stroke_style);
	mark_dirty();
}

void HTMLCanvasRenderingContext2D::draw_text(const String& p_text, const Vector2& p_position, const Variant& p_style) {
	if (p_text.is_empty() || !state.transform.is_finite() || std::abs(state.transform.determinant()) < CMP_EPSILON) return;
	Ref<Font> font = state.font;
	if (font.is_null()) font = ThemeDB::get_singleton()->get_fallback_font();
	if (font.is_null()) return;
	Ref<TextLine> line;
	line.instantiate();
	if (!line->add_string(p_text, font, state.font_size)) return;
	const Ref<TextServer> server = TextServerManager::get_singleton()->get_primary_interface();
	if (server.is_null()) return;
	const Array glyphs = server->shaped_text_get_glyphs(line->get_rid());
	const double ascent = line->get_line_ascent();
	double pen = 0.0;
	for (int64_t glyph_index = 0; glyph_index < glyphs.size(); ++glyph_index) {
		const Dictionary glyph = glyphs[glyph_index];
		const RID face = glyph["font_rid"];
		const int64_t id = glyph["index"];
		const int32_t size = glyph["font_size"];
		const int32_t repeat = std::clamp<int32_t>(glyph["repeat"], 1, 1024);
		const double advance = glyph["advance"];
		const Vector2 shaping_offset = glyph["offset"];
		pen += shaping_offset.x;
		if (face.is_valid() && id != 0 && size > 0) {
			const Vector2i cache_size(size, 0);
			server->font_render_glyph(face, cache_size, id);
			const int64_t atlas_index = server->font_get_glyph_texture_idx(face, cache_size, id);
			const Ref<Image> atlas = atlas_index >= 0 ? server->font_get_texture_image(face, cache_size, atlas_index) : Ref<Image>();
			const Rect2i uv(server->font_get_glyph_uv_rect(face, cache_size, id));
			if (atlas.is_valid() && uv.has_area() && Rect2i(Vector2i(), atlas->get_size()).encloses(uv)) {
				const bool msdf = server->font_is_multichannel_signed_distance_field(face);
				const bool lcd = server->font_get_antialiasing(face) == TextServer::FONT_ANTIALIASING_LCD &&
					atlas.is_valid() && atlas->get_format() == Image::FORMAT_RGBA8;
				const double oversampling = server->font_get_oversampling(face);
				const bool fixed_size = server->font_get_fixed_size(face) > 0;
				const double oversampling_factor = msdf || fixed_size || oversampling <= 0.0 ? 1.0 : oversampling;
				const double glyph_scale = msdf ? 1.0 : server->font_get_scale(face, size) / oversampling_factor;
				const Vector2 glyph_offset = server->font_get_glyph_offset(face, cache_size, id) * glyph_scale;
				const Vector2 glyph_size = server->font_get_glyph_size(face, cache_size, id) * glyph_scale;
				const double msdf_scale = msdf ? server->font_get_msdf_pixel_range(face) * glyph_size.x / uv.size.x : 0.0;
				if (glyph_size.x > 0.0 && glyph_size.y > 0.0 && glyph_size.is_finite()) {
					for (int32_t copy = 0; copy < repeat; ++copy) {
						Vector2 origin = p_position + Vector2(pen + advance * copy, ascent + shaping_offset.y) + glyph_offset;
						if (!msdf && glyph_scale == 1.0) origin = origin.floor();
						const Rect2 local_rect(origin, glyph_size);
						const Vector2 corners[] = {
							state.transform.xform(local_rect.position), state.transform.xform(Vector2(local_rect.get_end().x, local_rect.position.y)),
							state.transform.xform(local_rect.get_end()), state.transform.xform(Vector2(local_rect.position.x, local_rect.get_end().y)),
						};
						Rect2 bounds(corners[0], Vector2());
						for (int corner = 1; corner < 4; ++corner) bounds = bounds.expand(corners[corner]);
						const int left = std::max(0, int(std::floor(bounds.position.x)));
						const int top = std::max(0, int(std::floor(bounds.position.y)));
						const int right = std::min(image->get_width(), int(std::ceil(bounds.get_end().x)));
						const int bottom = std::min(image->get_height(), int(std::ceil(bounds.get_end().y)));
						for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) {
							const Vector2 target(x + 0.5f, y + 0.5f);
							const Vector2 local = state.transform.affine_inverse().xform(target);
							if (!local_rect.has_point(local)) continue;
							const Vector2 sample = Vector2(uv.position) + (local - local_rect.position) / glyph_size * Vector2(uv.size) - Vector2(0.5, 0.5);
							const Vector2i sample_pos(sample.floor());
							const Vector2 fraction = sample - Vector2(sample_pos);
							const auto pixel = [&](const Vector2i& p_point) {
								return atlas->get_pixelv(p_point.clamp(uv.position, uv.get_end() - Vector2i(1, 1)));
							};
							const Color sample_color = pixel(sample_pos).lerp(pixel(sample_pos + Vector2i(1, 0)), fraction.x).lerp(
								pixel(sample_pos + Vector2i(0, 1)).lerp(pixel(sample_pos + Vector2i(1, 1)), fraction.x), fraction.y);
							const float coverage = msdf
								? static_cast<float>(std::clamp<double>((std::max(std::min(sample_color.r, sample_color.g), std::min(std::max(sample_color.r, sample_color.g), sample_color.b)) - 0.5) * std::max(msdf_scale, 1.0) + 0.5, 0.0, 1.0))
								: (lcd ? std::max({ sample_color.r, sample_color.g, sample_color.b }) : sample_color.a);
							if (coverage <= 0.0f) continue;
							Color ink = style_color(p_style, target);
							ink.a *= coverage;
							put_pixel(target, ink);
						}
					}
				}
		}
		}
		pen += advance * repeat;
	}
	mark_dirty();
}

void HTMLCanvasRenderingContext2D::fill_text(const String& p_text, double p_x, double p_y) {
	draw_text(p_text, Vector2(p_x, p_y), state.fill_style);
}

Ref<HTMLCanvasGradient> HTMLCanvasRenderingContext2D::create_linear_gradient(double p_x0, double p_y0, double p_x1, double p_y1) {
	Ref<HTMLCanvasGradient> gradient;
	gradient.instantiate();
	gradient->configure(Vector2(p_x0, p_y0), Vector2(p_x1, p_y1));
	return gradient;
}

Ref<HTMLCanvasPattern> HTMLCanvasRenderingContext2D::create_pattern(const Variant& p_source, const String& p_repeat) {
	const Ref<Image> source = image_from_source(p_source);
	if (source.is_null() || !(p_repeat == "repeat" || p_repeat == "repeat-x" || p_repeat == "repeat-y" || p_repeat == "no-repeat")) return {};
	Ref<HTMLCanvasPattern> pattern;
	pattern.instantiate();
	pattern->configure(source, p_repeat);
	return pattern;
}

void HTMLCanvasRenderingContext2D::draw_image(const Variant& p_source, double p_dx, double p_dy) {
	const Ref<Image> source = image_from_source(p_source);
	if (source.is_valid()) draw_image_region(p_source, 0, 0, source->get_width(), source->get_height(), p_dx, p_dy,
		source->get_width(), source->get_height());
}
void HTMLCanvasRenderingContext2D::draw_image_sized(const Variant& p_source, double p_dx, double p_dy, double p_dw, double p_dh) {
	const Ref<Image> source = image_from_source(p_source);
	if (source.is_valid()) draw_image_region(p_source, 0, 0, source->get_width(), source->get_height(), p_dx, p_dy, p_dw, p_dh);
}
void HTMLCanvasRenderingContext2D::draw_image_region(const Variant& p_source, double p_sx, double p_sy, double p_sw, double p_sh,
	double p_dx, double p_dy, double p_dw, double p_dh) {
	Ref<Image> source = image_from_source(p_source);
	if (source.is_null() || p_sw <= 0 || p_sh <= 0 || p_dw == 0 || p_dh == 0 ||
		std::abs(state.transform.determinant()) < CMP_EPSILON) return;
	const Vector2 corners[] = {state.transform.xform(Vector2(p_dx, p_dy)), state.transform.xform(Vector2(p_dx + p_dw, p_dy)),
		state.transform.xform(Vector2(p_dx + p_dw, p_dy + p_dh)), state.transform.xform(Vector2(p_dx, p_dy + p_dh))};
	for (const Vector2& point : corners) if (!point.is_finite()) return;
	Rect2 bounds(corners[0], Vector2());
	for (int index = 1; index < 4; ++index) bounds = bounds.expand(corners[index]);
	const int left = static_cast<int>(std::floor(std::clamp<double>(bounds.position.x, 0.0, image->get_width())));
	const int top = static_cast<int>(std::floor(std::clamp<double>(bounds.position.y, 0.0, image->get_height())));
	const int right = static_cast<int>(std::ceil(std::clamp<double>(bounds.get_end().x, 0.0, image->get_width())));
	const int bottom = static_cast<int>(std::ceil(std::clamp<double>(bounds.get_end().y, 0.0, image->get_height())));
	const Transform2D inverse = state.transform.affine_inverse();
	for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) {
		const Vector2 local = inverse.xform(Vector2(x + 0.5f, y + 0.5f));
		const real_t u = (local.x - p_dx) / p_dw, v = (local.y - p_dy) / p_dh;
		if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
		const int sx = std::clamp(int(p_sx + u * p_sw), 0, source->get_width() - 1);
		const int sy = std::clamp(int(p_sy + v * p_sh), 0, source->get_height() - 1);
		const Vector2 target = state.transform.xform(local);
		put_pixel(target, source->get_pixel(sx, sy));
	}
	mark_dirty();
}

void HTMLCanvasRenderingContext2D::put_image_data(const Ref<Image>& p_data, int64_t p_dx, int64_t p_dy) {
	if (p_data.is_null()) return;
	for (int y = 0; y < p_data->get_height(); ++y) for (int x = 0; x < p_data->get_width(); ++x) {
		const int dx = static_cast<int>(p_dx + x), dy = static_cast<int>(p_dy + y);
		if (dx >= 0 && dy >= 0 && dx < image->get_width() && dy < image->get_height()) image->set_pixel(dx, dy, p_data->get_pixel(x, y));
	}
	mark_dirty();
}

Ref<Image> HTMLCanvasRenderingContext2D::get_image_data(int64_t p_sx, int64_t p_sy, int64_t p_sw, int64_t p_sh) const {
	if (p_sw <= 0 || p_sh <= 0 || p_sw > MAX_CANVAS_DIMENSION || p_sh > MAX_CANVAS_DIMENSION || p_sw * p_sh > MAX_CANVAS_PIXELS) return {};
	Ref<Image> result = Image::create(static_cast<int32_t>(p_sw), static_cast<int32_t>(p_sh), false, Image::FORMAT_RGBA8);
	result->fill(Color(0, 0, 0, 0));
	const Rect2i source_rect(p_sx, p_sy, p_sw, p_sh);
	const Rect2i clipped = source_rect.intersection(Rect2i(0, 0, image->get_width(), image->get_height()));
	if (clipped.has_area()) for (int y = clipped.position.y; y < clipped.position.y + clipped.size.y; ++y)
		for (int x = clipped.position.x; x < clipped.position.x + clipped.size.x; ++x)
			result->set_pixel(x - source_rect.position.x, y - source_rect.position.y, image->get_pixel(x, y));
	return result;
}
Ref<Image> HTMLCanvasRenderingContext2D::get_image() const {
	Ref<Image> result = Image::create(image->get_width(), image->get_height(), false, Image::FORMAT_RGBA8);
	result->blit_rect(image, Rect2i(0, 0, image->get_width(), image->get_height()), Vector2i());
	return result;
}
Ref<Texture2D> HTMLCanvasRenderingContext2D::get_texture() {
	if (texture.is_null()) texture = ImageTexture::create_from_image(image);
	else if (texture_dirty) texture->update(image);
	texture_dirty = false;
	return texture;
}

void HTMLCanvasElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_width"), &HTMLCanvasElement::get_width);
	ClassDB::bind_method(D_METHOD("set_width", "width"), &HTMLCanvasElement::set_width);
	ClassDB::bind_method(D_METHOD("get_height"), &HTMLCanvasElement::get_height);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &HTMLCanvasElement::set_height);
	ClassDB::bind_method(D_METHOD("get_context", "context_id"), &HTMLCanvasElement::get_context);
	ClassDB::bind_method(D_METHOD("get_image"), &HTMLCanvasElement::get_image);
	ClassDB::bind_method(D_METHOD("to_data_url", "type", "quality"), &HTMLCanvasElement::to_data_url, DEFVAL("image/png"), DEFVAL(0.92));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "width"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "height"), "set_height", "get_height");
}

int64_t HTMLCanvasElement::get_width() const { return canvas_dimension(get_attribute("width"), 300); }
void HTMLCanvasElement::set_width(int64_t p_width) {
	if (p_width < 1 || p_width > MAX_CANVAS_DIMENSION) return;
	set_attribute("width", String::num_int64(p_width));
	if (auto* node = dynamic_cast<dockit::HTMLCanvasNode*>(lock_element().get()); node && node->context.is_valid()) node->context->reset(get_width(), get_height());
	if (auto* layout = get_owner()) layout->repaint_input();
}
int64_t HTMLCanvasElement::get_height() const { return canvas_dimension(get_attribute("height"), 150); }
void HTMLCanvasElement::set_height(int64_t p_height) {
	if (p_height < 1 || p_height > MAX_CANVAS_DIMENSION) return;
	set_attribute("height", String::num_int64(p_height));
	if (auto* node = dynamic_cast<dockit::HTMLCanvasNode*>(lock_element().get()); node && node->context.is_valid()) node->context->reset(get_width(), get_height());
	if (auto* layout = get_owner()) layout->repaint_input();
}

Ref<HTMLCanvasRenderingContext2D> HTMLCanvasElement::get_context(const String& p_context_id) {
	if (p_context_id.to_lower() != "2d") return {};
	auto* node = dynamic_cast<dockit::HTMLCanvasNode*>(lock_element().get());
	if (!node) return {};
	if (node->context.is_null()) {
		node->context.instantiate();
		node->context->reset(get_width(), get_height());
		node->context->set_owner_id(ObjectID(get_instance_id()));
	}
	return node->context;
}

void HTMLCanvasElement::canvas_changed() {
	if (auto* layout = get_owner()) layout->repaint_input();
}

Ref<Image> HTMLCanvasElement::get_image() const {
	const auto* node = dynamic_cast<const dockit::HTMLCanvasNode*>(lock_element().get());
	if (node && node->context.is_valid()) return node->context->get_image();
	Ref<Image> blank = Image::create(static_cast<int32_t>(get_width()), static_cast<int32_t>(get_height()), false, Image::FORMAT_RGBA8);
	blank->fill(Color(0, 0, 0, 0));
	return blank;
}

String HTMLCanvasElement::to_data_url(const String& p_type, double p_quality) const {
	Ref<Image> bitmap = get_image();
	String mime = p_type.to_lower();
	PackedByteArray bytes;
	if (mime == "image/jpeg" || mime == "image/jpg") {
		mime = "image/jpeg";
		bytes = bitmap->save_jpg_to_buffer(std::clamp(p_quality, 0.0, 1.0));
	} else if (mime == "image/webp") {
		bytes = bitmap->save_webp_to_buffer(true, std::clamp(p_quality, 0.0, 1.0));
	} else {
		mime = "image/png";
		bytes = bitmap->save_png_to_buffer();
	}
	return "data:" + mime + ";base64," + Marshalls::get_singleton()->raw_to_base64(bytes);
}

namespace dockit {
void HTMLCanvasNode::parse_attributes() {
	if (const char* width = get_attr("width")) map_to_dimension_property(litehtml::_width_, width);
	if (const char* height = get_attr("height")) map_to_dimension_property(litehtml::_height_, height);
}
void HTMLCanvasNode::get_content_size(litehtml::size& r_size, litehtml::pixel_t) {
	const char* width = get_attr("width");
	const char* height = get_attr("height");
	r_size = {static_cast<litehtml::pixel_t>(width ? std::max(1, std::atoi(width)) : 300),
		static_cast<litehtml::pixel_t>(height ? std::max(1, std::atoi(height)) : 150)};
}
std::shared_ptr<litehtml::render_item> HTMLCanvasNode::create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) {
	auto result = std::make_shared<litehtml::render_item_image>(shared_from_this());
	result->parent(p_parent);
	return result;
}
void HTMLCanvasNode::draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
	const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) {
	litehtml::html_tag::draw(p_hdc, p_x, p_y, p_clip, p_item);
	if (context.is_null()) return;
	auto pos = p_item->pos();
	pos.x += p_x;
	pos.y += p_y;
	if (pos.width <= 0 || pos.height <= 0 || css().skips_contents()) return;
	const Ref<Texture2D> bitmap = context->get_texture();
	if (bitmap.is_null()) return;
	auto* container = static_cast<LiteHTMLContainer*>(get_document()->container());
	container->get_paint_recorder().draw_image(litehtml::make_replaced_object_layer(*this, pos, *p_item), bitmap);
}
}
