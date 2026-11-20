#pragma once

#include "html_text_control_node.h"
#include "html_node_copy_state.h"
#include "html_input_element.h"
#include "html_input_time.h"

#include <optional>
#include <string_view>

namespace dockit {
	class HTMLInputNode final : public HTMLTextControlNode, public HTMLNodeCopyState {
		bool checked = false;
		bool dirty_checked = false;
		godot::Rect2 range_box;
		godot::Rect2 decoration_box;
		godot::Rect2 image_box;
		std::optional<godot::Color> preview_color;
		std::optional<godot::String> preview_time;
		bool image_available = false;
		bool indeterminate = false;
		godot::TypedArray<HTMLInputFile> files;

		godot::String button_text() const;
		void paint_contents(LiteHTMLContainer& p_container, const godot::Rect2& p_box);

	public:
		enum class Type { TEXT, SEARCH, PASSWORD, EMAIL, URL, TEL, NUMBER, RANGE, CHECKBOX, RADIO,
			HIDDEN, BUTTON, SUBMIT, RESET, DATE, MONTH, WEEK, TIME, LOCAL, COLOR, FILE, IMAGE };

	private:
		Type previous_type = Type::TEXT;

	public:

		explicit HTMLInputNode(const std::shared_ptr<litehtml::document>& p_document);
		void set_attr(const char* p_name, const char* p_value) override;
		void parse_attributes() override;
		bool is_replaced() const override;
		void get_content_size(litehtml::size& r_size, litehtml::pixel_t p_width) override;
		std::shared_ptr<litehtml::render_item> create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) override;
		void draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
			const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) override;
		void attribute_changed(std::string_view p_name);
		Type type() const;
		godot::String type_name() const;
		bool is_text_control() const override;
		bool is_button() const;
		bool supports_selection() const;
		const char* maxlength() const;
		bool supports_readonly() const;
		bool supports_required() const;
		bool has_picker() const;
		double step_scale() const;
		double step_base() const;
		input_time::Kind time_kind() const;
		bool is_password() const { return type() == Type::PASSWORD; }
		bool accepts_suggestions() const;
		bool is_suggestion_valid(const godot::String& p_value) const;
		godot::Dictionary validity() const;
		double value_as_number() const;
		godot::String format_number(double p_value) const;
		godot::String sanitize_value(const godot::String& p_value) const;
		godot::Error step_number(int64_t p_step, bool& r_changed, std::optional<bool> p_endpoint = {});
		bool set_range_point(const godot::Vector2& p_point);
		bool set_numeric_from_user(double p_value);
		bool update_pseudos() override;
		godot::String get_value() const;
		bool is_checked() const { return checked; }
		bool set_checked(bool p_checked);
		void clear_group_checked();
		void reset();
		bool is_indeterminate() const { return indeterminate; }
		bool set_indeterminate(bool p_value);
		const godot::TypedArray<HTMLInputFile>& get_files() const { return files; }
		bool set_files(const godot::TypedArray<HTMLInputFile>& p_files);
		static godot::Color color_preview(const godot::String& p_value);
		void set_color_preview(std::optional<godot::Color> p_color) { preview_color = p_color; }
		void set_time_preview(std::optional<godot::String> p_value) { preview_time = std::move(p_value); }
		int decoration_at(const godot::Vector2& p_point) const;
		godot::Vector2i image_coordinates(const godot::Vector2& p_point) const;
		bool edit_time_segment(int p_direction, bool p_step);
		bool is_toggle() const { const auto kind = type(); return kind == Type::CHECKBOX || kind == Type::RADIO; }
		bool copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const override;
	};
}
