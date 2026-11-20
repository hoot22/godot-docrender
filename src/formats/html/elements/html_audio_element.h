#pragma once

#include "html_element.h"

class HTMLLabel;

namespace dockit {
	enum class HTMLAudioPart { none, play, position, mute, volume };

	class HTMLAudioNode;
}

class HTMLAudioElement : public HTMLElement {
	GDCLASS(HTMLAudioElement, HTMLElement)
	friend class HTMLLabel;
	friend class HTMLLabelCanvas;

	std::shared_ptr<dockit::HTMLAudioNode> get_audio_node() const;
	HTMLLabel* playback_host() const;

protected:
	static void _bind_methods();

public:
	godot::String get_src() const; // 返回源属性
	void set_src(const godot::String& p_src); // 设置音频源
	bool has_controls() const; // 检查是否显示媒体控件
	void set_controls(bool p_enabled); // 设置是否显示媒体控件
	bool is_autoplay_enabled() const; // 返回自动播放属性
	void set_autoplay(bool p_enabled); // 设置自动播放属性
	bool is_looping() const; // 返回循环播放属性
	void set_loop(bool p_enabled); // 设置循环播放属性
	bool is_muted() const; // 返回当前静音状态
	void set_muted(bool p_muted); // 修改当前静音状态
	double get_volume() const; // 返回零到一之间的音量
	void set_volume(double p_volume); // 设置播放音量
	double get_current_time() const; // 返回以秒为单位的媒体时间
	void set_current_time(double p_time); // 跳转到以秒为单位的媒体时间
	double get_duration() const; // 返回已载入媒体的时长
	godot::String get_current_src() const; // 返回已选择的项目资源路径
	bool is_paused() const; // 返回媒体是否暂停
	godot::Error play(); // 启动或继续播放
	void pause(); // 保留当前位置并暂停
};
