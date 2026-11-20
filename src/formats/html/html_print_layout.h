#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector4.hpp>

#include <vector>

class HTMLDocument;
class HTMLLayout;

struct HTMLPrintPageStyle {
	godot::Vector2 size;
	godot::Vector4 margins;
};

class HTMLPrintLayout : public godot::RefCounted {
	GDCLASS(HTMLPrintLayout, godot::RefCounted)
	friend class HTMLDocument;

	struct Page {
		godot::Vector2 size;
		godot::Rect2 content_rect;
		double source_y = 0.0;
		double source_end = 0.0;
	};

	godot::Ref<HTMLLayout> layout;
	std::vector<Page> pages;
	HTMLPrintPageStyle first_style;
	HTMLPrintPageStyle left_style;
	HTMLPrintPageStyle right_style;
	godot::PackedStringArray diagnostics;
	godot::String source_html;
	godot::String source_stylesheet;
	godot::Dictionary local_stylesheets;
	std::vector<double> forced_page_breaks;
	int64_t layout_width = 0;
	bool ready = false;
	bool ready_signal_queued = false;
	int64_t layout_error = godot::OK;
	int64_t pending_error = godot::OK;

	void prepare(const HTMLDocument& p_document, const godot::Dictionary& p_options);
	void rebuild_pages();
	void on_resources_changed();
	void emit_ready_signal();
	void add_diagnostic_once(const godot::String& p_message);
	void queue_ready_signal();
	static void _bind_methods();

public:
	HTMLPrintLayout() = default;
	~HTMLPrintLayout() override;
	bool is_ready() const;
	int64_t get_page_count() const;
	godot::Vector2 get_page_size(int64_t p_page_index) const;
	godot::Rect2 get_page_content_rect(int64_t p_page_index) const;
	godot::PackedStringArray get_diagnostics() const;
	int64_t get_error() const;
	godot::String get_error_message() const;
	godot::PackedByteArray to_pdf();
	int64_t save_pdf(const godot::String& p_path);
};
