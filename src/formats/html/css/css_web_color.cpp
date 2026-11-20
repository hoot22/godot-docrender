// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include <litehtml/html.h>
#include "css_web_color.h"
#include "formats/html/css/css_color.h"
#include <cmath>
#include <litehtml/css_parser.h>
#include <litehtml/os_types.h>
#include <litehtml/document_container.h>

namespace litehtml
{

const web_color web_color::transparent   = web_color(0, 0, 0, 0);
const web_color web_color::black         = web_color(0, 0, 0, 255);
const web_color web_color::white         = web_color(255, 255, 255, 255);
const web_color web_color::current_color = web_color(true);

gradient gradient::transparent;

struct def_color
{
	const char* name;
	const char* rgb;
};

def_color g_def_colors[] =
{
	{"transparent","rgba(0, 0, 0, 0)"},
	{"AliceBlue","#F0F8FF"},
	{"AntiqueWhite","#FAEBD7"},
	{"Aqua","#00FFFF"},
	{"Aquamarine","#7FFFD4"},
	{"Azure","#F0FFFF"},
	{"Beige","#F5F5DC"},
	{"Bisque","#FFE4C4"},
	{"Black","#000000"},
	{"BlanchedAlmond","#FFEBCD"},
	{"Blue","#0000FF"},
	{"BlueViolet","#8A2BE2"},
	{"Brown","#A52A2A"},
	{"BurlyWood","#DEB887"},
	{"CadetBlue","#5F9EA0"},
	{"Chartreuse","#7FFF00"},
	{"Chocolate","#D2691E"},
	{"Coral","#FF7F50"},
	{"CornflowerBlue","#6495ED"},
	{"Cornsilk","#FFF8DC"},
	{"Crimson","#DC143C"},
	{"Cyan","#00FFFF"},
	{"DarkBlue","#00008B"},
	{"DarkCyan","#008B8B"},
	{"DarkGoldenRod","#B8860B"},
	{"DarkGray","#A9A9A9"},
	{"DarkGrey","#A9A9A9"},
	{"DarkGreen","#006400"},
	{"DarkKhaki","#BDB76B"},
	{"DarkMagenta","#8B008B"},
	{"DarkOliveGreen","#556B2F"},
	{"Darkorange","#FF8C00"},
	{"DarkOrchid","#9932CC"},
	{"DarkRed","#8B0000"},
	{"DarkSalmon","#E9967A"},
	{"DarkSeaGreen","#8FBC8F"},
	{"DarkSlateBlue","#483D8B"},
	{"DarkSlateGray","#2F4F4F"},
	{"DarkSlateGrey","#2F4F4F"},
	{"DarkTurquoise","#00CED1"},
	{"DarkViolet","#9400D3"},
	{"DeepPink","#FF1493"},
	{"DeepSkyBlue","#00BFFF"},
	{"DimGray","#696969"},
	{"DimGrey","#696969"},
	{"DodgerBlue","#1E90FF"},
	{"FireBrick","#B22222"},
	{"FloralWhite","#FFFAF0"},
	{"ForestGreen","#228B22"},
	{"Fuchsia","#FF00FF"},
	{"Gainsboro","#DCDCDC"},
	{"GhostWhite","#F8F8FF"},
	{"Gold","#FFD700"},
	{"GoldenRod","#DAA520"},
	{"Gray","#808080"},
	{"Grey","#808080"},
	{"Green","#008000"},
	{"GreenYellow","#ADFF2F"},
	{"HoneyDew","#F0FFF0"},
	{"HotPink","#FF69B4"},
	{"Ivory","#FFFFF0"},
	{"Khaki","#F0E68C"},
	{"Lavender","#E6E6FA"},
	{"LavenderBlush","#FFF0F5"},
	{"LawnGreen","#7CFC00"},
	{"LemonChiffon","#FFFACD"},
	{"LightBlue","#ADD8E6"},
	{"LightCoral","#F08080"},
	{"LightCyan","#E0FFFF"},
	{"LightGoldenRodYellow","#FAFAD2"},
	{"LightGray","#D3D3D3"},
	{"LightGrey","#D3D3D3"},
	{"LightGreen","#90EE90"},
	{"LightPink","#FFB6C1"},
	{"LightSalmon","#FFA07A"},
	{"LightSeaGreen","#20B2AA"},
	{"LightSkyBlue","#87CEFA"},
	{"LightSlateGray","#778899"},
	{"LightSlateGrey","#778899"},
	{"LightSteelBlue","#B0C4DE"},
	{"LightYellow","#FFFFE0"},
	{"Lime","#00FF00"},
	{"LimeGreen","#32CD32"},
	{"Linen","#FAF0E6"},
	{"Magenta","#FF00FF"},
	{"Maroon","#800000"},
	{"MediumAquaMarine","#66CDAA"},
	{"MediumBlue","#0000CD"},
	{"MediumOrchid","#BA55D3"},
	{"MediumPurple","#9370D8"},
	{"MediumSeaGreen","#3CB371"},
	{"MediumSlateBlue","#7B68EE"},
	{"MediumSpringGreen","#00FA9A"},
	{"MediumTurquoise","#48D1CC"},
	{"MediumVioletRed","#C71585"},
	{"MidnightBlue","#191970"},
	{"MintCream","#F5FFFA"},
	{"MistyRose","#FFE4E1"},
	{"Moccasin","#FFE4B5"},
	{"NavajoWhite","#FFDEAD"},
	{"Navy","#000080"},
	{"OldLace","#FDF5E6"},
	{"Olive","#808000"},
	{"OliveDrab","#6B8E23"},
	{"Orange","#FFA500"},
	{"OrangeRed","#FF4500"},
	{"Orchid","#DA70D6"},
	{"PaleGoldenRod","#EEE8AA"},
	{"PaleGreen","#98FB98"},
	{"PaleTurquoise","#AFEEEE"},
	{"PaleVioletRed","#D87093"},
	{"PapayaWhip","#FFEFD5"},
	{"PeachPuff","#FFDAB9"},
	{"Peru","#CD853F"},
	{"Pink","#FFC0CB"},
	{"Plum","#DDA0DD"},
	{"PowderBlue","#B0E0E6"},
	{"Purple","#800080"},
	{"Red","#FF0000"},
	{"RosyBrown","#BC8F8F"},
	{"RoyalBlue","#4169E1"},
	{"SaddleBrown","#8B4513"},
	{"Salmon","#FA8072"},
	{"SandyBrown","#F4A460"},
	{"SeaGreen","#2E8B57"},
	{"SeaShell","#FFF5EE"},
	{"Sienna","#A0522D"},
	{"Silver","#C0C0C0"},
	{"SkyBlue","#87CEEB"},
	{"SlateBlue","#6A5ACD"},
	{"SlateGray","#708090"},
	{"SlateGrey","#708090"},
	{"Snow","#FFFAFA"},
	{"SpringGreen","#00FF7F"},
	{"SteelBlue","#4682B4"},
	{"Tan","#D2B48C"},
	{"Teal","#008080"},
	{"Thistle","#D8BFD8"},
	{"Tomato","#FF6347"},
	{"Turquoise","#40E0D0"},
	{"Violet","#EE82EE"},
	{"Wheat","#F5DEB3"},
	{"White","#FFFFFF"},
	{"WhiteSmoke","#F5F5F5"},
	{"Yellow","#FFFF00"},
	{"YellowGreen","#9ACD32"},
};

// <hex-color>  https://drafts.csswg.org/css-color-4/#typedef-hex-color
bool parse_hash_color(const css_token& tok, web_color& color)
{
	if (tok.type != HASH) return false;

	string s = tok.str;
	int len = (int)s.size();
	if (!is_one_of(len, 3, 4, 6, 8)) return false;
	for (auto ch : s) if (!is_hex_digit(ch)) return false;

	string r, g, b, a = "ff";
	if (len == 3 || len == 4)
	{
		r = {s[0], s[0]};
		g = {s[1], s[1]};
		b = {s[2], s[2]};
		if (len == 4)
			a = {s[3], s[3]};
	}
	else // 6 or 8
	{
		r = {s[0], s[1]};
		g = {s[2], s[3]};
		b = {s[4], s[5]};
		if (len == 8)
			a = {s[6], s[7]};
	}
	auto read_two_hex_digits = [](auto str)
	{
		return byte(16 * digit_value(str[0]) + digit_value(str[1]));
	};

	color = web_color(
		read_two_hex_digits(r),
		read_two_hex_digits(g),
		read_two_hex_digits(b),
		read_two_hex_digits(a));
	return true;
}

float clamp(float x, float min, float max)
{
	if (x < min) return min;
	if (x > max) return max;
	return x;
}

// https://drafts.csswg.org/css-color-5/#typedef-color-function
bool parse_func_color(const css_token& tok, web_color& color, document_container* container, unsigned depth)
{
	return parse_absolute_color(tok, color) ||
		parse_relative_color(tok, color, container, depth) ||
		parse_color_mix(tok, color, container, depth) ||
		parse_adaptive_color(tok, color, container, depth);
}

string resolve_name(const string& name, document_container* container)
{
	for (auto clr : g_def_colors)
	{
		if (equal_i(name, clr.name))
			return clr.rgb;
	}

	if (container)
		return container->resolve_color(name);

	return "";
}

namespace {
	struct PaletteEntry {
		const char* name;
		uint32_t light;
		uint32_t dark;
	};

	constexpr PaletteEntry palette[] = {
		{"", 0x000000ff, 0x000000ff},
		{"canvas", 0xffffffff, 0x121212ff},
		{"canvastext", 0x000000ff, 0xffffffff},
		{"field", 0xffffffff, 0x3b3b3bff},
		{"fieldtext", 0x000000ff, 0xffffffff},
		{"buttonface", 0xf0f0f0ff, 0x6b6b6bff},
		{"buttontext", 0x000000ff, 0xffffffff},
		{"buttonborder", 0x767676ff, 0x939393ff},
		{"graytext", 0x64748bff, 0xb4b4b4ff},
		{"linktext", 0x0000eeff, 0x9e9effff},
		{"activetext", 0xff0000ff, 0xff9e9eff},
		{"visitedtext", 0x551a8bff, 0xd0adf0ff},
		{"highlight", 0x3369c2ff, 0x3564acff},
		{"highlighttext", 0xffffffff, 0xffffffff},
		{"accentcolor", 0x0075ffff, 0x99c8ffff},
		{"accentcolortext", 0xffffffff, 0x101010ff},
		{"-dockit-button-hover", 0xe5e5e5ff, 0x7b7b7bff},
		{"-dockit-button-active", 0xf5f5f5ff, 0x565656ff},
		{"-dockit-border-hover", 0x4f4f4fff, 0xabababff},
		{"-dockit-border-active", 0x8d8d8dff, 0x838383ff},
		{"-dockit-disabled-face", 0xefefef4d, 0x8d8d8d4d},
		{"-dockit-disabled-border", 0x7676764d, 0x9b9b9b4d},
		{"-dockit-disabled-text", 0x1010104d, 0xffffff4d},
		{"-dockit-focus-ring", 0x101010ff, 0xb8d7ffff},
		{"-dockit-gauge-track", 0xefefefff, 0x3b3b3bff},
		{"-dockit-gauge-border", 0xb7b7b7ff, 0x797979ff},
		{"-dockit-scrollbar-track", 0xf1f1f1ff, 0x2c2c2cff},
		{"-dockit-scrollbar-thumb", 0x00000072, 0xffffff80},
		{"-dockit-scrollbar-pressed", 0x00000080, 0xffffff99},
		{"-dockit-menu-face", 0xf5f7fcff, 0x333333ff},
		{"-dockit-menu-border", 0x63738aff, 0x8c8c8cff},
		{"-dockit-menu-hover", 0x4a82e352, 0x85b1ff52},
		{"-dockit-list-selected", 0x3369c2ff, 0x3564acff},
		{"-dockit-list-hover", 0xc2d4f5ff, 0x4d5564ff},
	};
	static_assert(std::size(palette) == static_cast<size_t>(system_color::list_hover) + 1);
}

web_color system_color_value(system_color value, color_scheme scheme) {
	const auto& entry = palette[static_cast<size_t>(value)];
	const uint32_t rgba = scheme == color_scheme::dark ? entry.dark : entry.light;
	return web_color(byte(rgba >> 24), byte(rgba >> 16), byte(rgba >> 8), byte(rgba));
}

bool parse_name_color(const css_token& tok, web_color& color, document_container* container, unsigned depth)
{
	if (tok.type != IDENT) return false;
	const auto ident = tok.ident();
	if (ident == "currentcolor")
	{
		color = web_color::current_color;
		return true;
	}
	for (size_t i = 1; i < std::size(palette); ++i) {
		if (ident == palette[i].name) {
			color = web_color(static_cast<system_color>(i));
			return true;
		}
	}
	string str = resolve_name(tok.name, container);
	auto tokens = normalize(str, f_componentize | f_remove_whitespace);
	if (tokens.size() != 1) return false;
	return parse_color_at_depth(tokens[0], color, container, depth);
}

// https://drafts.csswg.org/css-color-5/#typedef-color
bool parse_color(const css_token& tok, web_color& color, document_container* container)
{
	return parse_color_at_depth(tok, color, container, 0);
}

bool parse_color_at_depth(const css_token& tok, web_color& color, document_container* container, unsigned depth)
{
	return
		parse_hash_color(tok, color) ||
		parse_func_color(tok, color, container, depth) ||
		parse_name_color(tok, color, container, depth);
}

web_color web_color::darken(double fraction) const
{
	int v_red = (int)red;
	int v_blue = (int)blue;
	int v_green = (int)green;
	v_red = (int)max(v_red - (v_red * fraction), 0.0);
	v_blue = (int)max(v_blue - (v_blue * fraction), 0.0);
	v_green = (int)max(v_green - (v_green * fraction), 0.0);
	return {(byte)v_red, (byte)v_green, (byte)v_blue, alpha};
}


string web_color::to_string() const
{
	if (is_current_color) return "currentcolor";
	if (system != system_color::none) return palette[static_cast<size_t>(system)].name;
	if (css_data) return to_srgb().to_string();
	char str[9];
	if(alpha)
	{
		t_snprintf(str, 9, "%02X%02X%02X%02X", red, green, blue, alpha);
	} else
	{
		t_snprintf(str, 9, "%02X%02X%02X", red, green, blue);
	}
	return str;
}

} // namespace litehtml
