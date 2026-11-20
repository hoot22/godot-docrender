#pragma once

#include "html_text_control_node.h"
#include "html_node_copy_state.h"

namespace dockit {
	class HTMLTextAreaNode final : public HTMLTextControlNode, public HTMLNodeCopyState {
		godot::Rect2 resize_handle;
		godot::Vector2 border_extras;
		godot::Vector2 border_size;

	public:
		explicit HTMLTextAreaNode(const std::shared_ptr<litehtml::document>& p_document);
		bool is_text_control() const override { return true; }
		bool is_replaced() const override { return true; }
		bool update_pseudos() override;
		bool copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const override;
		bool appendChild(const litehtml::element::ptr& p_child) override;
		void parse_attributes() override;
		godot::String get_default_value() const;
		void sync_default_value();
		std::shared_ptr<litehtml::render_item> create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) override;
		void draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
			const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) override;
		godot::Vector2i resize_axes() const;
		godot::Rect2 get_resize_handle() const { return resize_handle; }
		godot::Vector2 get_border_size() const { return border_size; }
		godot::Vector2 get_border_extras() const { return border_extras; }
	};
}
