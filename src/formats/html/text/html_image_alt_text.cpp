#include "html_image_alt_text.h"

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>

#include <algorithm>
#include <utility>

using namespace godot;

namespace dockit {
	HTMLImageAltText::HTMLImageAltText(HTMLTextLayout p_text, float p_line_height, bool p_wrap) :
		text(std::move(p_text)), line_height(std::max(1.0f, p_line_height)), wrap(p_wrap) {}

	const std::vector<HTMLTextLayout>& HTMLImageAltText::layout(float p_width) {
		if (p_width == layout_width) return lines;
		layout_width = p_width;
		lines.clear();
		if (p_width <= 0) return lines;
		if (wrap && text.width() > p_width) {
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			const auto ranges = server->shaped_text_get_line_breaks(text.get_rid(), std::max(1.0f, p_width), 0,
				TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE |
				TextServer::BREAK_TRIM_EDGE_SPACES);
			for (int64_t index = 0; index + 1 < ranges.size(); index += 2) {
				auto line = text.subrange(ranges[index], ranges[index + 1]);
				if (line.is_valid()) lines.push_back(std::move(line));
			}
		}
		if (lines.empty()) lines.push_back(text);
		return lines;
	}
}
