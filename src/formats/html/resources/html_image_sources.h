#pragma once

#include <godot_cpp/variant/string.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace litehtml {
	class element;
	struct media_features;
}

namespace dockit {
	using HTMLImageSourceResolver = std::function<godot::String(std::string_view)>;

	struct HTMLImageCandidate {
		std::string url;
		double value = 1.0;
		bool width_descriptor = false;
	};

	struct HTMLImageDimensions {
		std::string width;
		std::string height;
		bool operator==(const HTMLImageDimensions& p_other) const {
			return width == p_other.width && height == p_other.height;
		}
	};

	struct HTMLImageSelection {
		std::string source;
		std::string resolved_source;
		std::optional<double> density;
		std::optional<HTMLImageDimensions> dimensions;
	};

	std::vector<HTMLImageCandidate> parse_html_srcset(std::string_view p_srcset); // 按 HTML 候选语法解析 URL 和描述符
	HTMLImageSelection select_html_image(const litehtml::element& p_image,
		const HTMLImageSourceResolver& p_resolve, const litehtml::media_features& p_media); // 根据当前视口选择单个图片来源
}
