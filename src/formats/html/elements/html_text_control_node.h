#pragma once

#include "html_text_editor.h"
#include "html_form_state.h"

#include <litehtml/html_tag.h>

namespace dockit {
	class HTMLTextControlNode : public litehtml::html_tag {
	protected:
		bool focused = false;

	public:
		HTMLTextEditor editor;
		HTMLTextControlNode(const std::shared_ptr<litehtml::document>& p_document, bool p_multiline) :
			html_tag(p_document), editor(p_multiline) {
			editor.value_changed = [this] { note_dom_change(DOM_STATE); };
		}

		virtual bool is_text_control() const = 0;
		virtual bool update_pseudos() = 0;
		bool is_editable() const { return is_text_control() && !forms::is_disabled(*this) && !get_attr("readonly"); }
		litehtml::css_user_select user_selection_mode() const {
			using litehtml::css_user_select;
			if (is_editable()) return css_user_select::text;
			if (css().get_user_select() != css_user_select::automatic) return css().get_user_select();
			for (auto node = parent(); node; node = node->parent()) {
				if (node->css().get_user_select() != css_user_select::automatic) return node->css().get_user_select();
			}
			return css_user_select::text;
		}
		void set_focus_style(bool p_focused) { focused = p_focused; }
	};
}
