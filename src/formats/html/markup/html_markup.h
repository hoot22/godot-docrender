#pragma once

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace dockit {
	struct HTMLInlineColors {
		std::optional<godot::Color> text;
		std::optional<godot::Color> background;
	};

	HTMLInlineColors parse_html_inline_colors(const godot::String& p_style);// 从行内 CSS 读取前景色和背景色
	godot::String decode_html_entities(
		const godot::String& p_text,
		bool p_attribute = false
	);																		// 按普通文本或属性值的规则解码 HTML 字符引用
	godot::String decode_html_fragment(const godot::String& p_fragment);	// 解码链接片段中的百分号编码
}
