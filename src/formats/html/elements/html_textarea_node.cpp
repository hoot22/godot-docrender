#include "html_textarea_node.h"

#include "formats/html/backend/litehtml_container.h"

#include <litehtml/document.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <utility>

using namespace godot;

namespace dockit {
	namespace {
		int64_t dimension_hint(const char* p_text, int64_t p_default) {
			const String text = String::utf8(p_text).strip_edges();
			const int64_t number = text.is_valid_int() ? text.to_int() : 0;
			return number > 0 ? std::min<int64_t>(number, 1000000) : p_default;
		}
	}

	HTMLTextAreaNode::HTMLTextAreaNode(const std::shared_ptr<litehtml::document>& p_document) :
		HTMLTextControlNode(p_document, true) {
		update_pseudos();
	}

	String HTMLTextAreaNode::get_default_value() const {
		litehtml::string text;
		for (const auto& child : m_children) {
			if (child->is_text()) child->get_text(text);
		}
		return String::utf8(text.c_str());
	}

	void HTMLTextAreaNode::sync_default_value() {
		editor.use_default(get_default_value());
		update_pseudos();
	}

	bool HTMLTextAreaNode::appendChild(const litehtml::element::ptr& p_child) {
		if (!html_tag::appendChild(p_child)) {
			return false;
		}
		if (p_child->is_text()) {
			sync_default_value();
		}
		return true;
	}

	bool HTMLTextAreaNode::update_pseudos() {
		const bool disabled = forms::is_disabled(*this);
		bool changed = set_pseudo_class(litehtml::_id("disabled"), disabled);
		changed |= set_pseudo_class(litehtml::_id("enabled"), !disabled);
		changed |= set_pseudo_class(litehtml::_id("placeholder-shown"), editor.get_value().is_empty() && get_attr("placeholder"));
		return changed;
	}

	bool HTMLTextAreaNode::copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const {
		auto* target = dynamic_cast<HTMLTextAreaNode*>(&p_target);
		if (!target) {
			return false;
		}
		target->editor.copy_value_state(editor, p_mode == HTMLNodeCopyMode::ADOPT);
		target->update_pseudos();
		return true;
	}

	void HTMLTextAreaNode::parse_attributes() {
		html_tag::parse_attributes();
		update_pseudos();
		m_style.add_property(litehtml::_width_, std::to_string(dimension_hint(get_attr("cols", ""), 20)) + "ch");
		m_style.add_property(litehtml::_height_, std::to_string(dimension_hint(get_attr("rows", ""), 2)) + "lh");
		if (String::utf8(get_attr("wrap", "")).to_lower() == "off") {
			m_style.add_property(litehtml::_white_space_, "pre");
		}
	}

	std::shared_ptr<litehtml::render_item> HTMLTextAreaNode::create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) {
		// The DOM text supplies the default value. The editor paints the live value.
		struct RestoreChildren {
			litehtml::elements_list& target;
			litehtml::elements_list children;
			~RestoreChildren() { target.swap(children); }
		} restore{m_children, std::exchange(m_children, {})};
		return html_tag::create_render_item(p_parent);
	}

	Vector2i HTMLTextAreaNode::resize_axes() const {
		return forms::is_disabled(*this) ? Vector2i() : html_resize_axes(css());
	}

	void HTMLTextAreaNode::draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
		const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) {
		html_tag::draw(p_hdc, p_x, p_y, p_clip, p_item);
		const auto pos = p_item->pos();
		const Rect2 box(pos.x + p_x, pos.y + p_y, pos.width, pos.height);
		const auto& padding = p_item->get_paddings();
		const auto& border = p_item->get_borders();
		const Rect2 border_box = box.grow_individual(padding.left + border.left, padding.top + border.top,
			padding.right + border.right, padding.bottom + border.bottom);
		border_size = border_box.size;
		border_extras = border_size - box.size;
		const bool resizable = resize_axes() != Vector2i() && border_box.has_area();
		auto* container = static_cast<LiteHTMLContainer*>(get_document()->container());
		editor.paint_multiline(*container, box, *this, String::utf8(get_attr("placeholder", "")), focused, resizable);

		resize_handle = {};
		if (resizable) {
			resize_handle = html_resize_grip(border_box);
			container->draw_resize_grip(border_box, resize_handle);
		}
	}
}
