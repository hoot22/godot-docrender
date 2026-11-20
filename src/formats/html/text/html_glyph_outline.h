#pragma once

#include "formats/html/css/css_svg_path.h"

#include <cstdint>

namespace godot {
	class RID;
	class TextServer;
}

namespace dockit {
	css::SVGPath read_glyph_outline(const godot::TextServer& p_server, const godot::RID& p_font,
		int p_size, int64_t p_index, bool& r_reverse); // 把字形的直线、二次和三次贝塞尔轮廓转为路径
}
