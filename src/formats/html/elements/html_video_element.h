#pragma once

#include "html_element.h"

#include <godot_cpp/variant/vector2i.hpp>

namespace dockit { class HTMLVideoNode; }
class HTMLLabel;

class HTMLVideoElement : public HTMLElement {
	GDCLASS(HTMLVideoElement, HTMLElement)
	friend class HTMLLabel;
	friend class HTMLLabelCanvas;
	friend class HTMLLayout;

	std::shared_ptr<dockit::HTMLVideoNode> get_video_node() const;
	HTMLLabel* playback_host() const;

protected:
	static void _bind_methods();

public:
	godot::String get_src() const; // 返回视频源属性
	void set_src(const godot::String& p_src); // 设置视频源属性
	godot::String get_poster() const; // 返回海报路径属性
	void set_poster(const godot::String& p_poster); // 设置海报路径属性
	bool has_controls() const; // 返回控件属性
	void set_controls(bool p_enabled); // 设置控件属性
	bool is_autoplay_enabled() const; // 返回自动播放属性
	void set_autoplay(bool p_enabled); // 设置自动播放属性
	bool is_looping() const; // 返回循环播放属性
	void set_loop(bool p_enabled); // 设置循环播放属性
	bool is_muted() const; // 返回当前静音状态
	void set_muted(bool p_enabled); // 设置当前静音状态
	int32_t get_width() const; // 返回 HTML 宽度属性
	void set_width(int32_t p_width); // 设置 HTML 宽度属性
	int32_t get_height() const; // 返回 HTML 高度属性
	void set_height(int32_t p_height); // 设置 HTML 高度属性
	double get_volume() const; // 返回当前线性音量
	void set_volume(double p_volume); // 设置当前线性音量
	godot::String get_current_src() const; // 返回当前选中的视频路径
	double get_current_time() const; // 返回当前播放时间，单位为秒
	double get_duration() const; // 返回视频时长，单位为秒
	bool is_paused() const; // 返回暂停状态
	int32_t get_video_width() const; // 返回解码画面的固有宽度
	int32_t get_video_height() const; // 返回解码画面的固有高度
	godot::Error play(); // 请求播放视频
	void pause(); // 暂停视频
	godot::Error seek(double p_seconds); // 请求跳转到指定时间
	godot::Error request_fullscreen(); // 请求全屏显示视频
	void exit_fullscreen(); // 退出视频的全屏展示
	bool is_fullscreen() const; // 查询视频是否正在全屏展示
};
