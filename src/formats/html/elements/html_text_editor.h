#pragma once

#include "formats/html/text/html_text_layout.h"

#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <optional>
#include <functional>
#include <vector>

namespace litehtml { class html_tag; }

namespace dockit {
	class LiteHTMLContainer;
	namespace css { class ComputedStyle; }

	class HTMLTextEditor {
		struct Line {
			int64_t start;
			int64_t end;
			HTMLTextLayout shaped;
			godot::Vector2 baseline;
			float bottom;
		};

		struct Selection {
			int64_t anchor;
			int64_t caret;
			bool trailing_affinity;
		};

		struct Replacement {
			int64_t offset;
			godot::String removed;
			godot::String inserted;
		};

		struct Edit {
			Selection before;
			Selection after;
			std::vector<Replacement> replacements;
		};

		struct ImeEdit {
			uint64_t frame;
			Selection before;
			bool recorded = false;
		};

		const bool multiline;
		godot::String value;
		godot::String preedit;
		godot::Vector2i preedit_selection;
		std::vector<Edit> history;
		size_t history_position = 0;
		std::optional<ImeEdit> ime_edit;
		bool dirty_value = false;
		bool user_changed = false;
		bool last_edit_was_user = false;
		uint64_t reset_version = 0;
		int64_t anchor = 0;
		int64_t caret = 0;
		bool trailing_affinity = false;
		float horizontal_scroll = 0;
		HTMLTextLayout shaped;
		godot::Vector2 text_origin;
		godot::Rect2 caret_rect;
		bool showing_placeholder = false;
		bool caret_visible = true;

		std::vector<Line> lines;
		godot::String laid_out_text;
		godot::Rect2 viewport;
		godot::Vector2 scroll;
		godot::Vector2 full_size;
		godot::Vector2 scroll_limit;
		godot::Rect2 vertical_track;
		godot::Rect2 horizontal_track;
		bool vertical_scrollbar = false;
		bool horizontal_scrollbar = false;
		std::optional<float> preferred_x;
		std::optional<size_t> caret_line_hint;
		bool reveal_caret = true;
		godot::Vector2 layout_size = {-1, -1};
		float layout_line_height = -1;
		int layout_wrap = 0;
		int layout_overflow_x = 0;
		int layout_overflow_y = 0;
		float layout_scrollbar_width = 0;
		int layout_align = 0;
		bool layout_resize = false;
		uintptr_t layout_font = 0;

		Selection selection() const { return {anchor, caret, trailing_affinity}; }
		void clear_history();
		void restore_selection(const Selection& p_selection);
		bool replace_selection(const godot::String& p_text, const char* p_maxlength, const Selection& p_before, bool p_merge);
		void set_preedit(const godot::String& p_text, const godot::Vector2i& p_selection);
		godot::String normalize(const godot::String& p_value) const;
		float caret_x(int64_t p_offset, float p_near = 0.0f, bool p_nearest = false) const;
		float line_caret_x(const Line& p_line, int64_t p_offset, std::optional<float> p_near = {}) const;
		size_t line_at_offset(int64_t p_offset) const;
		size_t line_at_y(float p_y) const;
		void hit_single(float p_x, bool p_extend);
		void move_single(int p_direction, bool p_extend);
		void layout_lines(LiteHTMLContainer& p_container, const css::ComputedStyle& p_style,
			const css::ComputedStyle& p_text_style, const godot::String& p_text, float p_width, float p_line_height);
		godot::Rect2 scrollbar_thumb(const godot::Rect2& p_track, bool p_vertical) const;

	public:
		std::function<void()> value_changed;
		explicit HTMLTextEditor(bool p_multiline) : multiline(p_multiline) {}
		bool is_multiline() const { return multiline; }
		const godot::String& get_value() const { return value; }
		void use_default(const godot::String& p_value);
		bool set_value(const godot::String& p_value);
		void reset_value(const godot::String& p_value);
		uint64_t get_reset_version() const { return reset_version; }
		void copy_value_state(const HTMLTextEditor& p_source, bool p_preserve_selection);
		int64_t selection_start() const;
		int64_t selection_end() const;
		int64_t caret_offset() const { return caret; }
		bool set_selection(int64_t p_anchor, int64_t p_caret);
		void hit_caret(const godot::Vector2& p_point, bool p_extend);
		void move_caret(int p_direction, bool p_extend);
		void move_vertical(int p_direction, bool p_extend);
		void move_to_edge(bool p_end, bool p_document, bool p_extend);
		bool erase(int p_direction);
		bool insert(const godot::String& p_text, const char* p_maxlength = "");
		bool insert_character(char32_t p_character, const char* p_maxlength, uint64_t p_frame, bool p_ime_event);
		bool can_undo() const { return history_position > 0; }
		bool can_redo() const { return history_position < history.size(); }
		bool undo();
		bool redo();
		void finish_ime_edit();
		bool cancel_preedit();
		void update_preedit(const godot::String& p_text, const godot::Vector2i& p_selection, uint64_t p_frame);
		bool has_preedit() const { return !preedit.is_empty(); }
		const godot::String& get_preedit() const { return preedit; }
		bool take_user_change();
		bool has_length_violation(const char* p_minlength, const char* p_maxlength) const;
		bool has_dirty_value() const { return dirty_value; }
		godot::Rect2 get_caret_rect() const { return caret_rect; }
		bool is_caret_visible() const { return caret_visible; }
		bool set_caret_visible(bool p_visible) {
			if (caret_visible == p_visible) return false;
			caret_visible = p_visible;
			return true;
		}
		godot::Rect2 get_viewport() const { return viewport; }
		godot::Vector2 scroll_by(const godot::Vector2& p_delta);
		std::optional<bool> scrollbar_at(const godot::Vector2& p_point) const;
		void drag_scrollbar(bool p_vertical, const godot::Vector2& p_point, float p_grab_offset);
		float scrollbar_grab_offset(bool p_vertical, const godot::Vector2& p_point) const;
		void paint_single(LiteHTMLContainer& p_container, const godot::Rect2& p_box,
			const litehtml::html_tag& p_host, const godot::String& p_placeholder, bool p_focused, bool p_password = false,
			const godot::String* p_preview = nullptr);
		void paint_multiline(LiteHTMLContainer& p_container, const godot::Rect2& p_box,
			const litehtml::html_tag& p_host, const godot::String& p_placeholder, bool p_focused, bool p_resize_handle);
	};
}
