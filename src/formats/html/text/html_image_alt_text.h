#pragma once

#include "html_text_layout.h"

#include <vector>

namespace dockit {
	class HTMLImageAltText {
		HTMLTextLayout text;
		float line_height;
		bool wrap;
		float layout_width = -1;
		std::vector<HTMLTextLayout> lines;

	public:
		HTMLImageAltText(HTMLTextLayout p_text, float p_line_height, bool p_wrap);
		float get_width() const { return text.width(); }
		float get_line_height() const { return line_height; }
		const std::vector<HTMLTextLayout>& layout(float p_width); // 测量和绘制共用按宽度切分的文字行
	};
}
