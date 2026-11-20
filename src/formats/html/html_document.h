#pragma once

#include "html_layout.h"

#include <godot_cpp/classes/resource.hpp>

class HTMLDocument : public godot::Resource {
	GDCLASS(HTMLDocument, godot::Resource)
	friend class HTMLLabel;
	friend class HTMLPrintLayout;

	godot::String text;
	godot::String base_dir = "res://";
	godot::String source_path;
	godot::String stylesheet;
	godot::Dictionary textures;
	godot::Dictionary audio_streams;
	godot::Dictionary video_sources;
	godot::Dictionary local_stylesheets;
	godot::Dictionary local_fonts;
	godot::Dictionary image_dpi;
	godot::Dictionary image_orientations;
	godot::Dictionary svg_clip_sources;
	void parse_into(HTMLLayout& p_layout, const godot::String& p_default_stylesheet = {}) const;

protected:
	static void _bind_methods();

public:

	void set_text(const godot::String& p_text);										// 设置资源保存的 HTML 源文本
	godot::String get_text() const;													// 返回资源保存的 HTML 源文本
	
	void set_base_dir(const godot::String& p_dir);									// 设置文档的相对资源路径基准目录
	godot::String get_base_dir() const;												// 返回文档的资源基准目录
	void set_source_path(const godot::String& p_path); // 保存导入的原始 HTML 文件路径
	godot::String get_source_path() const; // 返回导入的原始 HTML 文件路径
	
	void set_stylesheet(const godot::String& p_stylesheet);							// 设置文档的附加 CSS
	godot::String get_stylesheet() const;											// 返回文档的附加 CSS
	
	void set_video_sources(const godot::Dictionary& p_sources); // 保存用于打包的本地视频来源
	godot::Dictionary get_video_sources() const; // 返回视频来源到项目路径的映射
	void set_audio_streams(const godot::Dictionary& p_audio); // 保存导入的音频资源依赖
	godot::Dictionary get_audio_streams() const; // 返回路径到音频资源的映射
	void set_textures(const godot::Dictionary& p_textures);							// 设置导入时收集的项目图片纹理依赖
	godot::Dictionary get_textures() const;											// 返回图片路径到纹理资源映射的副本
	
	void set_local_stylesheets(const godot::Dictionary& p_stylesheets);				// 设置导入时收集的本地样式表文本
	godot::Dictionary get_local_stylesheets() const;								// 返回本地样式表路径到 CSS 文本映射的副本
	void set_local_fonts(const godot::Dictionary& p_fonts);							// 保存导入时收集的字体资源
	godot::Dictionary get_local_fonts() const;										// 返回字体路径到资源映射的副本
	void set_image_dpi(const godot::Dictionary& p_dpi);								// 保存本地位图的 DPI 元数据
	godot::Dictionary get_image_dpi() const;										// 返回位图路径到 DPI 的映射副本
	void set_image_orientations(const godot::Dictionary& p_orientations);			// 保存本地位图的 EXIF 方向
	godot::Dictionary get_image_orientations() const;								// 返回位图路径到 EXIF 方向的映射副本
	void set_svg_clip_sources(const godot::Dictionary& p_sources);					// 保存导入时收集的 SVG 裁剪源
	godot::Dictionary get_svg_clip_sources() const;									// 返回 SVG 文件路径到源文本的映射副本
	
	godot::Ref<HTMLLayout> create_layout() const;									// 从资源当前内容创建独立的已解析布局实例
	godot::Ref<HTMLPrintLayout> create_print_layout(const godot::Dictionary& p_options = {}) const;
};
