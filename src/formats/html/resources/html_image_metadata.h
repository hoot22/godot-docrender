#pragma once

#include <godot_cpp/variant/string.hpp>

namespace dockit {
	struct HTMLImageMetadata {
		double dpi = 0;
		int orientation = 1;
	};

	HTMLImageMetadata read_html_image_metadata(const godot::String& p_path); // 读取本地位图的等轴 DPI 和 EXIF 方向
}
