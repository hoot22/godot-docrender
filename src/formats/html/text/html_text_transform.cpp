#include "formats/html/backend/litehtml_container.h"

#include <algorithm>
#include <iterator>
#include <utility>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		using CodepointMapping = std::pair<char32_t, char32_t>;

		template <size_t Size>
		char32_t map_codepoint(char32_t p_character, const CodepointMapping (&p_mappings)[Size]) {
			const auto* mapping = std::lower_bound(std::begin(p_mappings), std::end(p_mappings), p_character,
				[](const CodepointMapping& p_entry, char32_t p_value) { return p_entry.first < p_value; });
			return mapping != std::end(p_mappings) && mapping->first == p_character ? mapping->second : p_character;
		}

		char32_t full_width(char32_t p_character) {
			if (p_character >= 0x0021 && p_character <= 0x007e) return p_character + 0xfee0;
			// Unicode Decomposition_Mapping, inverting <wide> and applying <narrow> directly.
			static constexpr CodepointMapping other_widths[] = {
				{ 0x0020, 0x3000 }, { 0x00a2, 0xffe0 }, { 0x00a3, 0xffe1 }, { 0x00a5, 0xffe5 },
				{ 0x00a6, 0xffe4 }, { 0x00ac, 0xffe2 }, { 0x00af, 0xffe3 }, { 0x20a9, 0xffe6 },
				{ 0x2985, 0xff5f }, { 0x2986, 0xff60 }, { 0xffe8, 0x2502 }, { 0xffe9, 0x2190 },
				{ 0xffea, 0x2191 }, { 0xffeb, 0x2192 }, { 0xffec, 0x2193 }, { 0xffed, 0x25a0 }, { 0xffee, 0x25cb },
			};
			static constexpr char32_t katakana[] = {
				0x3002, 0x300c, 0x300d, 0x3001, 0x30fb, 0x30f2, 0x30a1, 0x30a3,
				0x30a5, 0x30a7, 0x30a9, 0x30e3, 0x30e5, 0x30e7, 0x30c3, 0x30fc,
				0x30a2, 0x30a4, 0x30a6, 0x30a8, 0x30aa, 0x30ab, 0x30ad, 0x30af,
				0x30b1, 0x30b3, 0x30b5, 0x30b7, 0x30b9, 0x30bb, 0x30bd, 0x30bf,
				0x30c1, 0x30c4, 0x30c6, 0x30c8, 0x30ca, 0x30cb, 0x30cc, 0x30cd,
				0x30ce, 0x30cf, 0x30d2, 0x30d5, 0x30d8, 0x30db, 0x30de, 0x30df,
				0x30e0, 0x30e1, 0x30e2, 0x30e4, 0x30e6, 0x30e8, 0x30e9, 0x30ea,
				0x30eb, 0x30ec, 0x30ed, 0x30ef, 0x30f3, 0x3099, 0x309a,
			};
			if (p_character >= 0xff61 && p_character < 0xff61 + std::size(katakana)) return katakana[p_character - 0xff61];
			if (p_character == 0xffa0) return 0x3164;
			if (p_character >= 0xffa1 && p_character <= 0xffbe) return 0x3131 + p_character - 0xffa1;
			if (p_character >= 0xffc2 && p_character <= 0xffdc) {
				const char32_t vowel = p_character - 0xffc2;
				if (vowel % 8 < 6) return 0x314f + vowel - 2 * (vowel / 8);
			}
			return map_codepoint(p_character, other_widths);
		}

		char32_t full_size_kana(char32_t p_character) {
			// CSS Text 4 Appendix G, including the supplementary small-kana block.
			static constexpr CodepointMapping kana[] = {
				{ 0x3041, 0x3042 }, { 0x3043, 0x3044 }, { 0x3045, 0x3046 }, { 0x3047, 0x3048 },
				{ 0x3049, 0x304a }, { 0x3063, 0x3064 }, { 0x3083, 0x3084 }, { 0x3085, 0x3086 },
				{ 0x3087, 0x3088 }, { 0x308e, 0x308f }, { 0x3095, 0x304b }, { 0x3096, 0x3051 },
				{ 0x30a1, 0x30a2 }, { 0x30a3, 0x30a4 }, { 0x30a5, 0x30a6 }, { 0x30a7, 0x30a8 },
				{ 0x30a9, 0x30aa }, { 0x30c3, 0x30c4 }, { 0x30e3, 0x30e4 }, { 0x30e5, 0x30e6 },
				{ 0x30e7, 0x30e8 }, { 0x30ee, 0x30ef }, { 0x30f5, 0x30ab }, { 0x30f6, 0x30b1 },
				{ 0x31f0, 0x30af }, { 0x31f1, 0x30b7 }, { 0x31f2, 0x30b9 }, { 0x31f3, 0x30c8 },
				{ 0x31f4, 0x30cc }, { 0x31f5, 0x30cf }, { 0x31f6, 0x30d2 }, { 0x31f7, 0x30d5 },
				{ 0x31f8, 0x30d8 }, { 0x31f9, 0x30db }, { 0x31fa, 0x30e0 }, { 0x31fb, 0x30e9 },
				{ 0x31fc, 0x30ea }, { 0x31fd, 0x30eb }, { 0x31fe, 0x30ec }, { 0x31ff, 0x30ed },
				{ 0xff67, 0xff71 }, { 0xff68, 0xff72 }, { 0xff69, 0xff73 }, { 0xff6a, 0xff74 },
				{ 0xff6b, 0xff75 }, { 0xff6c, 0xff94 }, { 0xff6d, 0xff95 }, { 0xff6e, 0xff96 }, { 0xff6f, 0xff82 },
				{ 0x1b132, 0x3053 }, { 0x1b150, 0x3090 }, { 0x1b151, 0x3091 }, { 0x1b152, 0x3092 },
				{ 0x1b155, 0x30b3 }, { 0x1b164, 0x30f0 }, { 0x1b165, 0x30f1 }, { 0x1b166, 0x30f2 }, { 0x1b167, 0x30f3 },
			};
			return map_codepoint(p_character, kana);
		}

		char32_t math_italic(char32_t p_character) {
			if (p_character >= U'A' && p_character <= U'Z') return 0x1d434 + p_character - U'A';
			if (p_character == U'h') return 0x210e;
			if (p_character >= U'a' && p_character <= U'z') return 0x1d44e + p_character - U'a';
			if (p_character >= 0x0391 && p_character <= 0x03a9 && p_character != 0x03a2) return 0x1d6e2 + p_character - 0x0391;
			if (p_character >= 0x03b1 && p_character <= 0x03c9) return 0x1d6fc + p_character - 0x03b1;
			static constexpr CodepointMapping variants[] = {
				{ 0x0131, 0x1d6a4 }, { 0x0237, 0x1d6a5 }, { 0x03d1, 0x1d717 }, { 0x03d5, 0x1d719 },
				{ 0x03d6, 0x1d71b }, { 0x03f0, 0x1d718 }, { 0x03f1, 0x1d71a }, { 0x03f4, 0x1d6f3 },
				{ 0x03f5, 0x1d716 }, { 0x2202, 0x1d715 }, { 0x2207, 0x1d6fb },
			};
			return map_codepoint(p_character, variants);
		}
	}

	void LiteHTMLContainer::transform_text(litehtml::string& r_text, text_transform p_transform) {
		if (p_transform == text_transform_none || r_text.empty()) return;
		String text = String::utf8(r_text.c_str());
		if (p_transform == text_transform_math_auto) {
			if (text.length() != 1) return;
			text[0] = math_italic(text[0]);
		}
		else {
			switch (p_transform & text_transform_case_mask) {
			case text_transform_uppercase: text = text.to_upper(); break;
			case text_transform_lowercase: text = text.to_lower(); break;
			case text_transform_capitalize: text = text.substr(0, 1).to_upper() + text.substr(1); break;
			default: break;
			}
			if (p_transform & (text_transform_full_width | text_transform_full_size_kana)) {
				char32_t* codepoints = text.ptrw();
				for (int64_t index = 0; index < text.length(); ++index) {
					if (p_transform & text_transform_full_width) codepoints[index] = full_width(codepoints[index]);
					if (p_transform & text_transform_full_size_kana) codepoints[index] = full_size_kana(codepoints[index]);
				}
			}
		}
		r_text = text.utf8().get_data();
	}
}
