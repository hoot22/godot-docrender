#include "html_audio_node.h"

#include "formats/html/backend/litehtml_container.h"
#include "formats/html/html_layout.h"
#include "nodes/html_label.h"

#include <godot_cpp/classes/input_event.hpp>
#include <litehtml/document.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace godot;
using namespace litehtml;
using dockit::HTMLAudioNode;
using dockit::HTMLAudioPart;
using dockit::HTMLControlIcon;

namespace {
	String format_time(double p_time) {
		if (!std::isfinite(p_time) || p_time < 0) return "0:00";
		const int64_t seconds = static_cast<int64_t>(std::min(p_time, 35999999.0));
		const String minutes = String::num_int64(seconds / 60);
		return minutes + String(":") + String::num_int64(seconds % 60).pad_zeros(2);
	}

	constexpr std::array parts{HTMLAudioPart::play, HTMLAudioPart::position, HTMLAudioPart::mute, HTMLAudioPart::volume};
	constexpr float CONTROL_WIDTH = 30.0f;

	Rect2 row_rect(const Rect2& p_box, float p_x, float p_width) {
		return Rect2(p_x, p_box.position.y, p_width, p_box.size.y);
	}
}

bool HTMLAudioNode::has_controls() const { return get_attr("controls") != nullptr; }

bool HTMLAudioNode::is_muted() const {
	return mute_overridden ? muted : get_attr("muted") != nullptr;
}

Rect2 HTMLAudioNode::part_rect(HTMLAudioPart p_part) const {
	if (!content_box.has_area() || !has_controls()) return {};
	const float width = content_box.size.x;
	const float inset = std::min(8.0f, width * 0.15f);
	const float start = content_box.position.x + inset;
	const float end = content_box.get_end().x - inset;
	const bool show_volume = width >= 268.0f;
	const bool show_mute = width >= 98.0f;
	const bool show_time = width >= 210.0f;
	const float volume_width = show_volume ? 42.0f : 0.0f;
	const float mute_width = show_mute ? CONTROL_WIDTH : 0.0f;
	const float position_start = start + CONTROL_WIDTH + (show_time ? 78.0f : 0.0f);
	const float position_end = end - mute_width - volume_width;

	switch (p_part) {
		case HTMLAudioPart::play: return row_rect(content_box, start, std::min(CONTROL_WIDTH, end - start));
		case HTMLAudioPart::position:
			return position_end > position_start + 18.0f ? row_rect(content_box, position_start, position_end - position_start) : Rect2();
		case HTMLAudioPart::mute:
			return show_mute ? row_rect(content_box, position_end, mute_width) : Rect2();
		case HTMLAudioPart::volume:
			return show_volume ? row_rect(content_box, end - volume_width, volume_width) : Rect2();
		case HTMLAudioPart::none: return {};
	}
	return {};
}

HTMLAudioPart HTMLAudioNode::part_at(const Vector2& p_point) const {
	for (const auto part : parts) {
		if (part_rect(part).has_point(p_point)) return part;
	}
	return HTMLAudioPart::none;
}

void HTMLAudioNode::compute_styles(bool p_recursive) {
	html_tag::compute_styles(p_recursive);
	if (!has_controls()) css_w().set_display(display_none);
}

std::shared_ptr<render_item> HTMLAudioNode::create_render_item(const std::shared_ptr<render_item>& p_parent) {
	auto children = std::exchange(m_children, {});
	auto result = html_tag::create_render_item(p_parent);
	m_children.swap(children);
	return result;
}

void HTMLAudioNode::draw(uint_ptr p_hdc, pixel_t p_x, pixel_t p_y, const position* p_clip,
		const std::shared_ptr<render_item>& p_item) {
	html_tag::draw(p_hdc, p_x, p_y, p_clip, p_item);
	const auto pos = p_item->pos();
	content_box = Rect2(pos.x + p_x, pos.y + p_y, pos.width, pos.height);
	if (!has_controls() || !content_box.has_area()) return;
	auto* container = static_cast<dockit::LiteHTMLContainer*>(get_document()->container());
	const auto ink = css().get_color().to_srgb();
	const Color foreground(ink.red / 255.0f, ink.green / 255.0f, ink.blue / 255.0f, ink.alpha / 255.0f);
	const Color track_color(foreground.r, foreground.g, foreground.b, 0.26f);
	const auto draw = [&](const Rect2& p_box, const Color& p_color, float p_radius = 0.0f) {
		if (p_box.has_area()) container->draw_input_chrome(content_box, p_box, p_color, p_radius);
	};
	const auto draw_track = [&](const Rect2& p_part, double p_value, bool p_enabled) {
		if (!p_part.has_area()) return;
		const Rect2 track(p_part.position.x + 5.0f, p_part.get_center().y - 2.0f, p_part.size.x - 10.0f, 4.0f);
		if (!track.has_area()) return;
		draw(track, track_color, 2.0f);
		if (!p_enabled) return;
		const float progress = std::clamp(p_value, 0.0, 1.0) * track.size.x;
		draw(Rect2(track.position, Vector2(progress, 4.0f)), foreground, 2.0f);
		draw(Rect2(track.position.x + progress - 3.0f, track.position.y - 1.0f, 6.0f, 6.0f), foreground, 3.0f);
	};
	if (has_pseudo_class(_id("focus-visible"))) {
		const auto focused = part_rect(focus_part);
		if (focused.has_area()) {
			draw(Rect2(focused.position.x + 2.0f, focused.get_center().y - 14.0f, focused.size.x - 4.0f, 28.0f),
				track_color, 14.0f);
		}
	}
	const auto play = part_rect(HTMLAudioPart::play);
	const auto draw_icon = [&](HTMLControlIcon p_icon, const Rect2& p_part) {
		if (!p_part.has_area()) return;
		const float extent = std::min({20.0f, p_part.size.x, p_part.size.y});
		const Vector2 size(extent, extent);
		container->draw_input_icon(p_icon, content_box, Rect2(p_part.get_center() - size * 0.5f, size), foreground);
	};
	draw_icon(paused ? HTMLControlIcon::play : HTMLControlIcon::pause, play);
	if (content_box.size.x >= 210.0f) {
		const String time_text = format_time(time) + String(" / ") + format_time(duration);
		const auto line = container->shape_input(css().get_font(), time_text);
		const auto& metrics = css().get_font_metrics();
		const Rect2 time_box = row_rect(content_box, play.get_end().x + 1.0f, 76.0f);
		container->draw_input(time_box, line,
			Vector2(time_box.position.x, time_box.get_center().y + (metrics.ascent - metrics.descent) * 0.5f),
			ink, {}, {}, {});
	}
	draw_track(part_rect(HTMLAudioPart::position), duration > 0 ? time / duration : 0, duration > 0);
	draw_icon(is_muted() || volume == 0.0 ? HTMLControlIcon::volume_off : HTMLControlIcon::volume_up,
		part_rect(HTMLAudioPart::mute));
	draw_track(part_rect(HTMLAudioPart::volume), is_muted() ? 0 : volume, true);
}

void HTMLAudioElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_src"), &HTMLAudioElement::get_src);
	ClassDB::bind_method(D_METHOD("set_src", "src"), &HTMLAudioElement::set_src);
	ClassDB::bind_method(D_METHOD("has_controls"), &HTMLAudioElement::has_controls);
	ClassDB::bind_method(D_METHOD("set_controls", "enabled"), &HTMLAudioElement::set_controls);
	ClassDB::bind_method(D_METHOD("is_autoplay_enabled"), &HTMLAudioElement::is_autoplay_enabled);
	ClassDB::bind_method(D_METHOD("set_autoplay", "enabled"), &HTMLAudioElement::set_autoplay);
	ClassDB::bind_method(D_METHOD("is_looping"), &HTMLAudioElement::is_looping);
	ClassDB::bind_method(D_METHOD("set_loop", "enabled"), &HTMLAudioElement::set_loop);
	ClassDB::bind_method(D_METHOD("is_muted"), &HTMLAudioElement::is_muted);
	ClassDB::bind_method(D_METHOD("set_muted", "muted"), &HTMLAudioElement::set_muted);
	ClassDB::bind_method(D_METHOD("get_volume"), &HTMLAudioElement::get_volume);
	ClassDB::bind_method(D_METHOD("set_volume", "volume"), &HTMLAudioElement::set_volume);
	ClassDB::bind_method(D_METHOD("get_current_time"), &HTMLAudioElement::get_current_time);
	ClassDB::bind_method(D_METHOD("set_current_time", "time"), &HTMLAudioElement::set_current_time);
	ClassDB::bind_method(D_METHOD("get_duration"), &HTMLAudioElement::get_duration);
	ClassDB::bind_method(D_METHOD("get_current_src"), &HTMLAudioElement::get_current_src);
	ClassDB::bind_method(D_METHOD("is_paused"), &HTMLAudioElement::is_paused);
	ClassDB::bind_method(D_METHOD("play"), &HTMLAudioElement::play);
	ClassDB::bind_method(D_METHOD("pause"), &HTMLAudioElement::pause);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "src"), "set_src", "get_src");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "controls"), "set_controls", "has_controls");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "is_autoplay_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "is_looping");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "muted"), "set_muted", "is_muted");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume"), "set_volume", "get_volume");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_time"), "set_current_time", "get_current_time");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration"), "", "get_duration");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "current_src"), "", "get_current_src");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "", "is_paused");
}

std::shared_ptr<HTMLAudioNode> HTMLAudioElement::get_audio_node() const {
	return std::dynamic_pointer_cast<HTMLAudioNode>(lock_element());
}

HTMLLabel* HTMLAudioElement::playback_host() const {
	auto* owner = get_owner();
	return owner && is_connected() ? Object::cast_to<HTMLLabel>(ObjectDB::get_instance(owner->audio_host)) : nullptr;
}

String HTMLAudioElement::get_src() const { return get_attribute("src"); }
void HTMLAudioElement::set_src(const String& p_src) { set_attribute("src", p_src); }
bool HTMLAudioElement::has_controls() const { return has_attribute("controls"); }
void HTMLAudioElement::set_controls(bool p_enabled) { toggle_attribute("controls", p_enabled); }
bool HTMLAudioElement::is_autoplay_enabled() const { return has_attribute("autoplay"); }
void HTMLAudioElement::set_autoplay(bool p_enabled) { toggle_attribute("autoplay", p_enabled); }
bool HTMLAudioElement::is_looping() const { return has_attribute("loop"); }
void HTMLAudioElement::set_loop(bool p_enabled) { toggle_attribute("loop", p_enabled); }
bool HTMLAudioElement::is_muted() const { const auto node = get_audio_node(); return node && node->is_muted(); }

void HTMLAudioElement::set_muted(bool p_muted) {
	if (const auto node = get_audio_node()) {
		node->muted = p_muted;
		node->mute_overridden = true;
		if (auto* host = playback_host()) host->set_audio_volume(*this);
	}
}

double HTMLAudioElement::get_volume() const { const auto node = get_audio_node(); return node ? node->volume : 1.0; }

void HTMLAudioElement::set_volume(double p_volume) {
	if (!std::isfinite(p_volume)) return;
	if (const auto node = get_audio_node()) {
		node->volume = std::clamp(p_volume, 0.0, 1.0);
		if (auto* host = playback_host()) host->set_audio_volume(*this);
	}
}

double HTMLAudioElement::get_current_time() const { const auto node = get_audio_node(); return node ? node->time : 0.0; }
void HTMLAudioElement::set_current_time(double p_time) {
	if (!std::isfinite(p_time)) return;
	if (auto* host = playback_host()) host->seek_audio(Ref<HTMLAudioElement>(this), p_time);
}

double HTMLAudioElement::get_duration() const { const auto node = get_audio_node(); return node ? node->duration : 0.0; }
String HTMLAudioElement::get_current_src() const { const auto node = get_audio_node(); return node ? node->current_src : String(); }
bool HTMLAudioElement::is_paused() const { const auto node = get_audio_node(); return !node || node->paused; }

Error HTMLAudioElement::play() {
	auto* host = playback_host();
	return host ? host->play_audio(Ref<HTMLAudioElement>(this)) : ERR_UNCONFIGURED;
}

void HTMLAudioElement::pause() { if (auto* host = playback_host()) host->pause_audio(*this); }
