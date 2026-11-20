#include "html_animation.h"

#include "formats/html/html_layout.h"

using namespace godot;
using dockit::css::PlaybackCommand;
using dockit::css::PlaybackInfo;
using dockit::css::PlaybackState;

void HTMLAnimation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &HTMLAnimation::is_valid);
	ClassDB::bind_method(D_METHOD("get_target"), &HTMLAnimation::get_target);
	ClassDB::bind_method(D_METHOD("get_name"), &HTMLAnimation::get_name);
	ClassDB::bind_method(D_METHOD("get_kind"), &HTMLAnimation::get_kind);
	ClassDB::bind_method(D_METHOD("get_pseudo_element"), &HTMLAnimation::get_pseudo_element);
	ClassDB::bind_method(D_METHOD("get_play_state"), &HTMLAnimation::get_play_state);
	ClassDB::bind_method(D_METHOD("get_duration"), &HTMLAnimation::get_duration);
	ClassDB::bind_method(D_METHOD("get_current_time"), &HTMLAnimation::get_current_time);
	ClassDB::bind_method(D_METHOD("set_current_time", "time"), &HTMLAnimation::set_current_time);
	ClassDB::bind_method(D_METHOD("play"), &HTMLAnimation::play);
	ClassDB::bind_method(D_METHOD("pause"), &HTMLAnimation::pause);
	ClassDB::bind_method(D_METHOD("cancel"), &HTMLAnimation::cancel);
	ClassDB::bind_method(D_METHOD("finish"), &HTMLAnimation::finish);
	ClassDB::bind_method(D_METHOD("reverse"), &HTMLAnimation::reverse);
}

HTMLLayout* HTMLAnimation::get_layout() const {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(owner));
	return layout && layout->parse_version == version ? layout : nullptr;
}

std::optional<PlaybackInfo> HTMLAnimation::get_info() const {
	const auto layout = get_layout();
	return layout ? layout->animation_info(track, transition) : std::nullopt;
}

bool HTMLAnimation::control(PlaybackCommand p_command, double p_value) {
	auto* layout = get_layout();
	return layout && layout->control_animation(track, transition, p_command, p_value);
}

bool HTMLAnimation::is_valid() const { return get_info().has_value(); }

Ref<HTMLElement> HTMLAnimation::get_target() const {
	const auto info = get_info();
	const auto layout = get_layout();
	return info && layout ? layout->wrap_hit_element(info->target) : Ref<HTMLElement>();
}

String HTMLAnimation::get_name() const {
	const auto info = get_info();
	return info ? String::utf8(info->name.c_str()) : String();
}

String HTMLAnimation::get_kind() const {
	const auto info = get_info();
	return info ? (transition ? "transition" : info->scripted ? "script" : "animation") : "";
}

String HTMLAnimation::get_pseudo_element() const {
	const auto info = get_info();
	return info ? String::utf8(info->pseudo.c_str()) : String();
}

String HTMLAnimation::get_play_state() const {
	const auto info = get_info();
	if (!info) return "idle";
	switch (info->state) {
		case PlaybackState::idle: return "idle";
		case PlaybackState::running: return "running";
		case PlaybackState::paused: return "paused";
		case PlaybackState::finished: return "finished";
	}
	return "idle";
}

double HTMLAnimation::get_duration() const {
	const auto info = get_info();
	return info ? info->duration : 0.0;
}

double HTMLAnimation::get_current_time() const {
	const auto info = get_info();
	return info ? info->time : 0.0;
}

bool HTMLAnimation::set_current_time(double p_time) { return control(PlaybackCommand::seek, p_time); }
bool HTMLAnimation::play() { return control(PlaybackCommand::play); }
bool HTMLAnimation::pause() { return control(PlaybackCommand::pause); }
bool HTMLAnimation::cancel() { return control(PlaybackCommand::cancel); }
bool HTMLAnimation::finish() { return control(PlaybackCommand::finish); }
bool HTMLAnimation::reverse() { return control(PlaybackCommand::reverse); }
