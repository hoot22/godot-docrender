#pragma once

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace godot { class Texture2D; }

namespace dockit {
	struct HTMLResourceBase {
		godot::String uri;
		godot::String directory;
	};
	HTMLResourceBase make_html_resource_base(const godot::String& p_directory, const godot::String& p_source_path = {});
	HTMLResourceBase resolve_html_document_base(const HTMLResourceBase& p_fallback, const char* p_href);
	godot::String resolve_html_link_href(const godot::String& p_href, const HTMLResourceBase& p_base);

	godot::Ref<godot::Texture2D> load_html_image_file(const godot::String& p_path); // 读取已解析的本地纹理路径
	godot::String resolve_html_audio_path(const godot::String& p_src, const godot::String& p_base_dir);
	godot::String resolve_html_video_path(const godot::String& p_src, const godot::String& p_base_dir);
	godot::String packed_html_video_path(const godot::String& p_path);
	std::optional<godot::String> normalize_html_resource_path(
		const godot::String& p_path
	);																				// 规范化 res:// 或 user:// 路径并拒绝越界路径
	godot::String resolve_html_image_path(
		const godot::String& p_src,
		const godot::String& p_base_dir
	);																				// 按基目录解析并校验支持的本地图片路径
	godot::String resolve_html_stylesheet_path(
		const godot::String& p_src,
		const godot::String& p_base_dir
	);																				// 按基目录解析并校验本地 CSS 文件路径
	godot::String resolve_html_font_path(
		const godot::String& p_src,
		const godot::String& p_base_dir
	);																				// 解析项目内的动态字体路径
	godot::String resolve_html_svg_clip_path(
		const godot::String& p_url, const godot::String& p_base_dir
	);																				// 解析带非空片段的本地 SVG 文件路径
}
