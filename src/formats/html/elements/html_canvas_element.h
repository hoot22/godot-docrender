#pragma once

#include "html_element.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <vector>

class HTMLCanvasRenderingContext2D;

class HTMLCanvasGradient : public godot::RefCounted {
	GDCLASS(HTMLCanvasGradient, godot::RefCounted)
	friend class HTMLCanvasRenderingContext2D;
	struct Stop { double offset = 0.0; godot::Color color; };
	std::vector<Stop> stops;
	godot::Vector2 start;
	godot::Vector2 end;
protected:
	static void _bind_methods();
public:
	godot::Error add_color_stop(double p_offset, const godot::Variant& p_color);
	void configure(const godot::Vector2& p_start, const godot::Vector2& p_end);
	godot::Color sample(const godot::Vector2& p_point) const;
};

class HTMLCanvasPattern : public godot::RefCounted {
	GDCLASS(HTMLCanvasPattern, godot::RefCounted)
	friend class HTMLCanvasRenderingContext2D;
	godot::Ref<godot::Image> image;
	bool repeat_x = true;
	bool repeat_y = true;
protected:
	static void _bind_methods();
public:
	void configure(const godot::Ref<godot::Image>& p_image, const godot::String& p_repeat);
	godot::Color sample(const godot::Vector2& p_point) const;
};

class HTMLCanvasElement : public HTMLElement {
	GDCLASS(HTMLCanvasElement, HTMLElement)
	friend class HTMLCanvasRenderingContext2D;

protected:
	static void _bind_methods();

public:
	void canvas_changed();
	int64_t get_width() const;
	void set_width(int64_t p_width);
	int64_t get_height() const;
	void set_height(int64_t p_height);
	godot::Ref<HTMLCanvasRenderingContext2D> get_context(const godot::String& p_context_id);
	godot::Ref<godot::Image> get_image() const;
	godot::String to_data_url(const godot::String& p_type = "image/png", double p_quality = 0.92) const;
};

class HTMLCanvasRenderingContext2D : public godot::RefCounted {
	GDCLASS(HTMLCanvasRenderingContext2D, godot::RefCounted)
	friend class HTMLCanvasElement;

	struct State {
		godot::Variant fill_style = godot::Color(0, 0, 0, 1);
		godot::Variant stroke_style = godot::Color(0, 0, 0, 1);
		float line_width = 1.0f;
		godot::Ref<godot::Font> font;
		int32_t font_size = 16;
		godot::Transform2D transform;
	};

	godot::Ref<godot::Image> image;
	godot::Ref<godot::ImageTexture> texture;
	godot::ObjectID owner_id;
	std::vector<State> states;
	State state;
	std::vector<godot::Vector2> path;
	bool path_closed = false;
	bool texture_dirty = true;

	void mark_dirty();
	void put_pixel(const godot::Vector2& p_point, const godot::Color& p_color);
	void draw_line_segment(const godot::Vector2& p_from, const godot::Vector2& p_to, const godot::Variant& p_style);
	void fill_polygon(const std::vector<godot::Vector2>& p_points, const godot::Variant& p_style);
	void draw_text(const godot::String& p_text, const godot::Vector2& p_position, const godot::Variant& p_style);
	godot::Color style_color(const godot::Variant& p_style, const godot::Vector2& p_position) const;
	void reset(int64_t p_width, int64_t p_height);
	void set_owner_id(godot::ObjectID p_owner) { owner_id = p_owner; }

protected:
	static void _bind_methods();

public:
	HTMLCanvasRenderingContext2D();
	void set_fill_style(const godot::Variant& p_style);
	godot::Variant get_fill_style() const { return state.fill_style; }
	void set_stroke_style(const godot::Variant& p_style);
	godot::Variant get_stroke_style() const { return state.stroke_style; }
	void set_line_width(double p_width);
	double get_line_width() const { return state.line_width; }
	void set_font(const godot::Ref<godot::Font>& p_font);
	godot::Ref<godot::Font> get_font() const { return state.font; }
	void set_font_size(int32_t p_size);
	int32_t get_font_size() const { return state.font_size; }
	void save();
	void restore();
	void reset_transform();
	void translate(double p_x, double p_y);
	void scale(double p_x, double p_y);
	void rotate(double p_angle);
	void set_transform(double p_a, double p_b, double p_c, double p_d, double p_e, double p_f);
	void clear_rect(double p_x, double p_y, double p_width, double p_height);
	void fill_rect(double p_x, double p_y, double p_width, double p_height);
	void stroke_rect(double p_x, double p_y, double p_width, double p_height);
	void begin_path();
	void close_path();
	void move_to(double p_x, double p_y);
	void line_to(double p_x, double p_y);
	void rect(double p_x, double p_y, double p_width, double p_height);
	void arc(double p_x, double p_y, double p_radius, double p_start_angle, double p_end_angle, bool p_counterclockwise = false);
	void fill();
	void stroke();
	void fill_text(const godot::String& p_text, double p_x, double p_y);
	godot::Ref<HTMLCanvasGradient> create_linear_gradient(double p_x0, double p_y0, double p_x1, double p_y1);
	godot::Ref<HTMLCanvasPattern> create_pattern(const godot::Variant& p_source, const godot::String& p_repeat = "repeat");
	void draw_image(const godot::Variant& p_source, double p_dx, double p_dy);
	void draw_image_sized(const godot::Variant& p_source, double p_dx, double p_dy, double p_dw, double p_dh);
	void draw_image_region(const godot::Variant& p_source, double p_sx, double p_sy, double p_sw, double p_sh,
		double p_dx, double p_dy, double p_dw, double p_dh);
	void put_image_data(const godot::Ref<godot::Image>& p_data, int64_t p_dx, int64_t p_dy);
	godot::Ref<godot::Image> get_image_data(int64_t p_sx, int64_t p_sy, int64_t p_sw, int64_t p_sh) const;
	godot::Ref<godot::Image> get_image() const;
	godot::Ref<godot::Texture2D> get_texture();
};

