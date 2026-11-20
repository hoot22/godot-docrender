#include "html.h"
#include "document.h"
#include "el_space.h"
#include "utf8_strings.h"

litehtml::el_space::el_space(const char* text, const std::shared_ptr<document>& doc)
	: el_text(text, doc), m_codepoint(*static_cast<const char32_t*>(utf8_to_utf32(text)))
{
}

void litehtml::el_space::get_content_size(size& sz, pixel_t max_width)
{
	el_text::get_content_size(sz, max_width);
	if (m_autospace_width) sz.width = *m_autospace_width;
}

bool litehtml::el_space::is_white_space() const
{
	return is_space() && !is_break() && css().collapses_white_space();
}

bool litehtml::el_space::is_break() const
{
	if (is_unicode_forced_break(m_codepoint)) return true;
	return m_codepoint == U'\n' && is_one_of(css().get_white_space_collapse(), white_space_preserve,
		white_space_preserve_breaks, white_space_break_spaces);
}

bool litehtml::el_space::is_space() const
{
	return is_one_of(m_codepoint, U' ', U'\t', U'\n', U'\r', U'\f');
}

litehtml::string litehtml::el_space::dump_get_name()
{
	return "space: \"" + get_escaped_string(m_text) + "\"";
}
