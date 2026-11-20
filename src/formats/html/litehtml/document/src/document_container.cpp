#include "utf8_strings.h"
#include "document_container.h"
#include <cmath>

bool litehtml::document_container::is_image_available(const char* src, const char* baseurl, css_image_orientation orientation)
{
	litehtml::size extent{};
	get_image_size(src, baseurl, extent, orientation);
	return extent.width > 0 && extent.height > 0;
}

void litehtml::document_container::get_css_image_size(const char* src, const char* baseurl, double density, litehtml::size& sz, css_image_orientation orientation)
{
	sz = {};
	if (density <= 0 || !std::isfinite(density)) return;
	get_image_size(src, baseurl, sz, orientation);
	// Limit extreme author-specified densities without changing the image's aspect ratio.
	const double limit = 2147483520.0; // Largest float below INT32_MAX.
	const double denominator = std::max(density, std::max(sz.width, sz.height) / limit);
	sz.width = static_cast<pixel_t>(sz.width / denominator);
	sz.height = static_cast<pixel_t>(sz.height / denominator);
}

void litehtml::document_container::split_text(const char* text, const std::function<void(const char*)>& on_word, const std::function<void(const char*)>& on_space)
{
	std::u32string str;
	std::u32string str_in = (const char32_t*)utf8_to_utf32(text);
	for (auto c : str_in)
	{
		if (c == U' ' || c == U'\t' || c == U'\n' || c == U'\r' || is_unicode_forced_break(c) || is_css_space_separator(c))
		{
			if (!str.empty())
			{
				on_word(utf32_to_utf8(str));
				str.clear();
			}
			str += c;
			on_space(utf32_to_utf8(str));
			str.clear();
		}
		// CJK character range
		else if (c >= 0x4E00 && c <= 0x9FCC)
		{
			if (!str.empty())
			{
				on_word(utf32_to_utf8(str));
				str.clear();
			}
			str += c;
			on_word(utf32_to_utf8(str));
			str.clear();
		}
		else
		{
			str += c;
		}
	}
	if (!str.empty())
	{
		on_word(utf32_to_utf8(str));
	}
}
