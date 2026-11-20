#pragma once

#include "html_node_copy_state.h"

#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <litehtml/html_tag.h>

#include <optional>
#include <vector>

namespace dockit {
	class HTMLSelectNode;
	class HTMLOptionNode : public litehtml::html_tag, public HTMLNodeCopyState {
		friend class HTMLSelectNode;
		std::optional<bool> selected;
		bool dirty = false;
		void assign_selected(bool p_selected, bool p_dirty);

	public:
		using litehtml::html_tag::html_tag;
		HTMLSelectNode* select() const;
		godot::String get_value() const;
		godot::String get_label() const;
		bool is_selected() const;
		void set_selected(bool p_selected);
		void attribute_changed();
		bool copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const override;
	};

	class HTMLSelectNode : public litehtml::html_tag, public HTMLNodeCopyState {
		bool dropdown_open = false;
		bool allow_empty = false;
		godot::Rect2 list_box;
		double row_height = 1.0;
		int scroll_row = 0;
		int active_row = -1;
		int anchor_row = -1;
		struct SelectionSnapshot {
			std::weak_ptr<HTMLOptionNode> option;
			bool selected;
		};
		std::vector<SelectionSnapshot> selection_snapshot;
		godot::String selected_value_snapshot;
		bool has_selection_snapshot = false;

	public:
		using litehtml::html_tag::html_tag;
		using Options = std::vector<std::shared_ptr<HTMLOptionNode>>;
		Options get_options() const;
		Options get_list_options() const;
		void use_single_selection();
		bool is_multiple() const { return get_attr("multiple") != nullptr; }
		int64_t get_size() const;
		bool is_list_box() const { return is_multiple() || get_size() > 1; }
		void set_dropdown_open(bool p_open) { dropdown_open = p_open; }
		int64_t display_size() const;
		bool is_option_selected(const HTMLOptionNode& p_option) const;
		int list_row_at(const godot::Vector2& p_point) const;
		int list_active_row() const { return active_row; }
		bool scroll_list(int p_rows);
		bool select_list_row(int p_row, bool p_extend, bool p_toggle, bool p_move_only = false);
		int64_t selected_index(const Options& p_options) const;
		void set_selected_index(const Options& p_options, int64_t p_index);
		bool is_missing() const;
		void reset();
		void sync_mutation_revision();
		bool copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const override;
		std::shared_ptr<litehtml::render_item> create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) override;
		void draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
			const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) override;
	};
}
