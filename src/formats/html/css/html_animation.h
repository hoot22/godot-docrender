#pragma once

#include "css_playback.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/object_id.hpp>

class HTMLLayout;
class HTMLElement;

class HTMLAnimation : public godot::RefCounted {
	GDCLASS(HTMLAnimation, godot::RefCounted)
	friend class HTMLLayout;

	godot::ObjectID owner;
	uint64_t version = 0;
	uint64_t track = 0;
	bool transition = false;

	HTMLLayout* get_layout() const;
	std::optional<dockit::css::PlaybackInfo> get_info() const;
	bool control(dockit::css::PlaybackCommand p_command, double p_value = 0.0);

protected:
	static void _bind_methods();

public:
	bool is_valid() const; // 检查动画是否仍属于当前布局
	godot::Ref<HTMLElement> get_target() const; // 返回动画目标元素
	godot::String get_name() const; // 返回 CSS 动画名称或过渡属性名
	godot::String get_kind() const; // 返回 CSS 动画、过渡或脚本动画的类型
	godot::String get_pseudo_element() const; // 返回目标伪元素名称
	godot::String get_play_state() const; // 返回当前播放状态
	double get_duration() const; // 返回动画有效区间的总时长
	double get_current_time() const; // 返回当前播放时间，单位为秒
	bool set_current_time(double p_time); // 跳转至指定时间，单位为秒
	bool play(); // 播放或继续动画
	bool pause(); // 在当前位置暂停动画
	bool cancel(); // 清除动画效果并将动画置为空闲
	bool finish(); // 跳转至当前播放方向的结束位置
	bool reverse(); // 反向播放动画
};
