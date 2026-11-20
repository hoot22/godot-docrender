#ifndef LH_EL_SPACE_H
#define LH_EL_SPACE_H

#include "el_text.h"

namespace litehtml
{
	class el_space : public el_text
	{
		char32_t m_codepoint;
		std::optional<pixel_t> m_autospace_width;
		bool m_autospace_no_break = false;
	public:
		el_space(const char* text, const std::shared_ptr<document>& doc);

		bool is_white_space() const override;
		bool is_break() const override;
		bool is_space() const override;
		bool is_other_space_separator() const override { return is_css_space_separator(m_codepoint); }
		bool is_glue_space() const override { return m_codepoint == 0x2007 || m_codepoint == 0x202f; }
		void set_autospace_width(std::optional<pixel_t> width, bool non_breaking)
		{
			m_autospace_width = width;
			m_autospace_no_break = non_breaking;
		}
		bool has_autospace_width() const { return m_autospace_width.has_value(); }
		bool has_non_breaking_autospace() const { return m_autospace_no_break; }
		string dump_get_name() override;
	protected:
		void get_content_size(size& sz, pixel_t max_width) override;
	};
}

#endif  // LH_EL_SPACE_H
