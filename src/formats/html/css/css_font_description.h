// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LITEHTML_FONT_DESCRIPTION
#define LITEHTML_FONT_DESCRIPTION

#include <cstring>
#include <string>
#include "formats/html/css/css_values.h"
#include "css_types.h"
#include "css_length.h"
#include "css_web_color.h"

namespace litehtml
{
	enum class css_unicode_bidi : int
	{
		normal,
		embed,
		bidi_override,
		isolate,
		isolate_override,
		plaintext,
	};

	struct font_description
	{
		css_font_families		families;	// Font Family
		std::string				language;	// Effective BCP 47 language for shaping and fallback
		bool					language_is_declared = false;
		bool					use_font_line_gap = true;
		bool					right_to_left = false;
		css_unicode_bidi		unicode_bidi = css_unicode_bidi::normal;
		pixel_t					size = 0;	// Font size
		css_font_style			style;	// Font style and oblique angle
		float					width = 100.0f;	// Font width in percent
		css_font_features features;
		css_font_features variant_features;
		css_font_alternates alternates;
		pixel_t letter_spacing = 0;
		pixel_t word_spacing = 0;
		css_font_kerning kerning = font_kerning_auto;
		css_font_optical_sizing optical_sizing = font_optical_sizing_auto;
		css_font_variations variations;
		css_font_size_adjust size_adjust;
		float					weight = 400;	// Font weight.
		int						decoration_line = text_decoration_line_none;	// Decoration line. A bitset of flags of the enum text_decoration_line
		css_length				decoration_thickness;	// Decoration line thickness in pixels. See predefined values in enumtext_decoration_thickness
		text_decoration_style	decoration_style = text_decoration_style_solid;	// Decoration line style. See enum text_decoration_style
		web_color				decoration_color = web_color::current_color;	// Decoration line color
		std::string				emphasis_style;	// Text emphasis style
		web_color				emphasis_color = web_color::current_color;	// Text emphasis color
		int						emphasis_position = text_emphasis_position_over;	// Text emphasis position

		std::string opentype_key() const
		{
			std::string key;
			if (letter_spacing != 0) key += ":tracking";
			if (kerning != font_kerning_auto) key += ":kern=" + std::to_string(kerning);
			if (optical_sizing == font_optical_sizing_none) key += ":opsz=none";
			if (size_adjust.mode != css_font_size_adjust::Mode::none) {
				uint64_t bits;
				static_assert(sizeof(bits) == sizeof(size_adjust.value));
				std::memcpy(&bits, &size_adjust.value, sizeof(bits));
				key += ":fsa=" + std::to_string(static_cast<int>(size_adjust.metric)) + ':' +
					std::to_string(static_cast<int>(size_adjust.mode)) + ':' + std::to_string(bits);
			}
			for (const auto& [tag, value] : variant_features)
				key += ":vf=" + std::to_string(tag) + ':' + std::to_string(value);
			for (const auto& [tag, value] : features)
				key += ":ft=" + std::to_string(tag) + ':' + std::to_string(value);
			for (const auto& [tag, value] : variations)
			{
				uint32_t bits;
				static_assert(sizeof(bits) == sizeof(value));
				std::memcpy(&bits, &value, sizeof(bits));
				key += ":va=" + std::to_string(tag) + ':' + std::to_string(bits);
			}
			return key + alternates.cache_key();
		}

		std::string	hash() const
		{
			std::string out;
			for (const auto& family : families)
			{
				out += family.generic ? 'G' : 'N';
				out += std::to_string(family.name.size()) + ':' + family.name;
			}
			out += ":sz=" + std::to_string(size);
			out += ":st=" + std::to_string(style.kind) + ':' + std::to_string(style.angle);
			out += ":w=" + std::to_string(weight);
			out += ":wd=" + std::to_string(width);
			out += ":lang=" + std::to_string(language.size()) + ':' + language;
			if (language_is_declared) out += ":declared-lang";
			if (!use_font_line_gap) out += ":fixed-line-height";
			out += ":rtl=" + std::to_string(right_to_left);
			out += ":unicode-bidi=" + std::to_string(static_cast<int>(unicode_bidi));
			out += opentype_key();
			out += ":ls=" + std::to_string(letter_spacing);
			out += ":ws=" + std::to_string(word_spacing);
			out += ":dl=" + std::to_string(decoration_line);
			out += ":dt=" + decoration_thickness.to_string();
			out += ":ds=" + std::to_string(decoration_style);
			out += ":dc=" + decoration_color.to_string();
			out += ":ephs=" + emphasis_style;
			out += ":ephc=" + emphasis_color.to_string();
			out += ":ephp=" + std::to_string(emphasis_position);

			return out;
		}
	};
}

#endif
