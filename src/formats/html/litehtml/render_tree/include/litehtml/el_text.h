#ifndef LH_EL_TEXT_H
#define LH_EL_TEXT_H

#include "element.h"
#include "document.h"

namespace litehtml
{
	class el_text : public element
	{
	protected:
		string			m_text;
		string			m_transformed_text;
		size			m_size;
		bool			m_use_transformed;
		bool			m_draw_spaces;
		bool			m_whole_text_node;
		bool m_hyphenation_allowed = false;
		std::shared_ptr<const string> m_dom_text;
		std::weak_ptr<element> m_dom_source;
		int64_t m_dom_offset = 0;
	public:
		el_text(const char* text, const document::ptr& doc, bool whole_text_node = true);

		bool is_preserved_tab() const override
		{
			return m_text == "\t" && (css().get_white_space_collapse() == white_space_preserve ||
				css().get_white_space_collapse() == white_space_break_spaces);
		}
		const string& get_source_text() const { return m_text; }
		const std::shared_ptr<const string>& dom_text() const { return m_dom_text; }
		std::shared_ptr<element> dom_source() const { return m_dom_source.lock(); }
		int64_t dom_offset() const { return m_dom_offset; }
		void set_dom_text(const std::shared_ptr<const string>& p_text, const element::ptr& p_source, int64_t p_offset) {
			m_dom_text = p_text;
			m_dom_source = p_source;
			m_dom_offset = p_offset;
		}
		bool is_whole_text_node() const { return m_whole_text_node; }
		const string& get_rendered_text() const { return m_use_transformed ? m_transformed_text : m_text; }
		bool allows_hyphenation() const { return m_hyphenation_allowed; }
		void set_hyphenation_allowed(bool allowed) { m_hyphenation_allowed = allowed; }

		void				get_text(string& text) const override;
		void				compute_styles(bool recursive) override;
		bool				is_text() const override { return true; }

		void draw(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip, const std::shared_ptr<render_item> &ri) override;
		string				dump_get_name() override;
		std::vector<std::tuple<string, string>> dump_get_attrs() override;
	protected:
		void				get_content_size(size& sz, pixel_t max_width) override;
	};
}

#endif  // LH_EL_TEXT_H
