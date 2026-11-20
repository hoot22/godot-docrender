#pragma once

#include "html_image_metadata.h"
#include "html_resource_paths.h"
#include "html_remote_stylesheets.h"

#include <litehtml/types.h>

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace dockit {
	bool is_html_stylesheet_link(const char* p_rel);								// 判断 rel 是否声明默认应用的样式表
	std::optional<godot::String> read_html_stylesheet(const godot::String& p_path); // 读取大小受限的本地 UTF-8 样式表
	std::optional<godot::String> read_html_svg_source(const godot::String& p_path); // 读取大小受限的本地 UTF-8 SVG 文件

	class HTMLResources {
		HTMLResourceBase fallback_base;
		HTMLResourceBase document_base;
		godot::Dictionary textures;
		godot::Dictionary local_stylesheets;
		godot::Dictionary local_fonts;
		godot::Dictionary image_dpi;
		godot::Dictionary image_orientations;
		godot::Dictionary svg_clip_sources;
		godot::HashMap<godot::String, godot::Ref<godot::Texture2D>> images;
		godot::HashMap<godot::String, godot::Ref<godot::FontFile>> fonts;
		godot::HashMap<godot::String, HTMLImageMetadata> image_metadata;
		godot::HashSet<godot::String> pending_layout_images;
		godot::HashSet<godot::String> pending_paint_images;
		godot::Callable remote_image_callback;
		godot::HashMap<godot::String, RemoteStylesheet> remote_stylesheets;
		godot::HashSet<godot::String> pending_stylesheets;
		godot::Callable remote_stylesheet_callback;
		bool recording_paint = false;
		int stylesheet_reads = 0;
		int failed_remote_resources = 0;

		const HTMLImageMetadata& get_image_metadata(const godot::String& p_path);
		godot::String get_url_dir(const char* p_css_base) const;
		godot::String resolve_remote_resource(const godot::String& p_url, std::string_view p_css_base) const;
		godot::Ref<godot::Texture2D> load_texture_file(const godot::String& p_path);
		godot::Ref<godot::FontFile> load_font_file(const godot::String& p_path);

	public:
		HTMLResources(const godot::String& p_base_dir, const godot::Dictionary& p_textures,
			const godot::Dictionary& p_local_stylesheets, const godot::Dictionary& p_local_fonts, const godot::Dictionary& p_image_dpi,
			const godot::Dictionary& p_image_orientations, const godot::Dictionary& p_svg_clip_sources);

		static std::string_view stylesheet_path(std::string_view p_css_base);
		void set_source_path(const godot::String& p_path);
		void set_document_base_href(const char* p_href);
		const HTMLResourceBase& get_document_base() const { return document_base; }
		const HTMLResourceBase& get_fallback_base() const { return fallback_base; }
		void reset_stylesheet_reads();
		godot::String resolve_stylesheet_source(const godot::String& p_url, std::string_view p_css_base = {}) const;
		void set_remote_stylesheet_callback(const godot::Callable& p_callback);
		bool receive_remote_stylesheet(const godot::String& p_url, const godot::String& p_final_url, const godot::String& p_text);
		void set_remote_image_callback(const godot::Callable& p_callback);
		void begin_image_layout();
		int pending_resource_count() const;
		int get_failed_remote_resource_count() const { return failed_remote_resources; }
		void begin_image_paint();
		void end_image_paint();
		enum class ImageUpdate { none, paint, layout };
		ImageUpdate receive_remote_image(const godot::String& p_url, const godot::Ref<godot::Texture2D>& p_texture);
		void import_css(std::string& r_text, const std::string& p_url, std::string& r_css_base);
		godot::String resolve_svg_clip_source(std::string_view p_url, const char* p_css_base) const;
		std::optional<godot::String> get_svg_clip_source(const godot::String& p_path) const;
		godot::String resolve_image_source(std::string_view p_source) const; // 返回保留片段的项目图片路径
		bool is_vector_image(const char* p_src, const char* p_css_base) const; // 判断本地图片是否为 SVG
		double get_image_dpi(const char* p_src, const char* p_css_base); // 获取导入保存或从源图片读取的 DPI
		godot::Ref<godot::Texture2D> load_texture(const char* p_src, const char* p_css_base,
			litehtml::css_image_orientation p_orientation = litehtml::css_image_orientation::from_image,
			bool p_affects_layout = true);
		godot::Ref<godot::FontFile> load_font(const char* p_src, const char* p_css_base);
	};
}
