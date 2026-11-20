#include "html_video_node.h"

#include "formats/html/backend/litehtml_container.h"
#include "formats/html/html_layout.h"
#include "nodes/html_label.h"

#include <godot_cpp/classes/input_event.hpp>
#include <litehtml/el_image.h>
#include <litehtml/render_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace godot;
using namespace litehtml;
using namespace dockit;

namespace {
	constexpr std::array video_parts{HTMLVideoPart::play, HTMLVideoPart::position, HTMLVideoPart::mute,
		HTMLVideoPart::volume, HTMLVideoPart::fullscreen};

	String format_video_time(double p_time) {
		const int64_t seconds = static_cast<int64_t>(std::clamp(std::isfinite(p_time) ? p_time : 0.0, 0.0, 35999999.0));
		const auto minute = String::num_int64((seconds / 60) % 60);
		const auto second = String::num_int64(seconds % 60).pad_zeros(2);
		if (seconds >= 3600) return String::num_int64(seconds / 3600) + String(":") + minute.pad_zeros(2) + String(":") + second;
		return minute + String(":") + second;
	}
}

bool HTMLVideoNode::is_replaced() const { return true; }
bool HTMLVideoNode::has_controls() const { return available && get_attr("controls"); }
bool HTMLVideoNode::is_muted() const { return muted.value_or(get_attr("muted") != nullptr); }

void HTMLVideoNode::parse_attributes() {
	if (const char* width = get_attr("width")) map_to_dimension_property(_width_, width);
	if (const char* height = get_attr("height")) map_to_dimension_property(_height_, height);
}

void HTMLVideoNode::get_content_size(litehtml::size& r_size, pixel_t) {
	r_size = {300, 150};
	if (dimensions.x > 0 && dimensions.y > 0) {
		r_size = {pixel_t(dimensions.x), pixel_t(dimensions.y)};
		return;
	}
	// Keep the poster's layout size even when optional decoding is unavailable.
	if (const char* poster = get_attr("poster")) {
		litehtml::size poster_size;
		get_document()->container()->get_css_image_size(poster, nullptr, 0.0, poster_size, css().get_image_orientation());
		if (poster_size.width > 0 && poster_size.height > 0) r_size = poster_size;
	}
}

std::shared_ptr<render_item> HTMLVideoNode::create_render_item(const std::shared_ptr<render_item>& p_parent) {
	auto result = std::make_shared<render_item_image>(shared_from_this());
	result->parent(p_parent);
	return result;
}

Rect2 HTMLVideoNode::part_rect(const Rect2& p_box, HTMLVideoPart p_part) const {
	if (!(fullscreen || has_controls()) || !show_controls || p_box.size.y < 16.0f || p_box.size.x < 24.0f) return {};
	const float inset = std::min(10.0f, p_box.size.x * 0.05f);
	const float left = p_box.position.x + inset;
	const float right = p_box.get_end().x - inset;
	const float height = std::min(36.0f, p_box.size.y * 0.75f);
	const float button = std::min(height, (right - left) / 2.0f);
	const float bottom = p_box.get_end().y;
	const float y = bottom - height;
	switch (p_part) {
		case HTMLVideoPart::play: return Rect2(left, y, button, height);
		case HTMLVideoPart::fullscreen: return Rect2(right - button, y, button, height);
		case HTMLVideoPart::mute:
			return p_box.size.x >= 118.0f ? Rect2(right - button * 2.0f, y, button, height) : Rect2();
		case HTMLVideoPart::volume:
			return p_box.size.x >= 250.0f ? Rect2(right - button * 2.0f - 52.0f, y, 48.0f, height) : Rect2();
		case HTMLVideoPart::position:
			return Rect2(left, std::max(p_box.position.y, y - 13.0f), right - left, std::min(13.0f, y - p_box.position.y));
		default: return {};
	}
}

HTMLVideoPart HTMLVideoNode::part_at(const Rect2& p_box, const Vector2& p_point) const {
	for (const auto part : video_parts) {
		if ((part != HTMLVideoPart::position || seekable) && part_rect(p_box, part).has_point(p_point)) return part;
	}
	return HTMLVideoPart::none;
}

void HTMLVideoNode::draw_controls(const Rect2& p_box, const HTMLVideoControlPainter& p_painter, bool p_focused) const {
	const auto play = part_rect(p_box, HTMLVideoPart::play);
	if (!play.has_area()) return;
	const float gradient_height = std::min(p_box.size.y, play.size.y + 33.0f);
	const float step = gradient_height / 16.0f;
	for (int i = 0; i < 16; ++i) {
		p_painter.fill(Rect2(p_box.position.x, p_box.get_end().y - gradient_height + i * step, p_box.size.x, step),
			Color(0, 0, 0, 0.78f * (i + 1) / 16.0f));
	}
	const auto draw_icon = [&](HTMLVideoPart p_part, HTMLControlIcon p_icon) {
		const auto box = part_rect(p_box, p_part);
		if (!box.has_area()) return;
		const float extent = std::min({22.0f, box.size.x, box.size.y});
		const Vector2 size(extent, extent);
		p_painter.icon(p_icon, Rect2(box.get_center() - size * 0.5f, size));
	};
	const auto draw_track = [&](HTMLVideoPart p_part, double p_value, bool p_enabled) {
		const auto box = part_rect(p_box, p_part);
		if (!box.has_area()) return;
		const Rect2 track(box.position.x, box.get_center().y - 1.5f, box.size.x, 3.0f);
		p_painter.fill(track, Color(1, 1, 1, 0.35f));
		const float width = track.size.x * std::clamp(p_value, 0.0, 1.0);
		if (p_enabled && width > 0.0f) p_painter.fill(Rect2(track.position, Vector2(width, track.size.y)), Color(1, 1, 1));
	};
	if (p_focused) {
		const auto box = part_rect(p_box, focus_part);
		if (box.has_area()) p_painter.fill(box, Color(1, 1, 1, 0.22f));
	}
	draw_icon(HTMLVideoPart::play, paused ? HTMLControlIcon::play : HTMLControlIcon::pause);
	draw_icon(HTMLVideoPart::mute, is_muted() || volume == 0.0 ? HTMLControlIcon::volume_off : HTMLControlIcon::volume_up);
	draw_icon(HTMLVideoPart::fullscreen, fullscreen ? HTMLControlIcon::fullscreen_exit : HTMLControlIcon::fullscreen);
	draw_track(HTMLVideoPart::position, duration > 0 ? seek_preview.value_or(time) / duration : 0, seekable);
	draw_track(HTMLVideoPart::volume, is_muted() ? 0 : volume, true);
	const float time_right = part_rect(p_box, p_box.size.x >= 250.0f ? HTMLVideoPart::volume : HTMLVideoPart::mute).position.x;
	const float time_width = time_right - play.get_end().x - 4.0f;
	if (time_width >= 78.0f) {
		p_painter.text(Rect2(play.get_end().x + 4.0f, play.position.y, time_width, play.size.y),
			format_video_time(seek_preview.value_or(time)) + " / " + format_video_time(duration));
	}
}

void HTMLVideoNode::draw(uint_ptr p_hdc, pixel_t p_x, pixel_t p_y, const position* p_clip,
	const std::shared_ptr<render_item>& p_item) {
	html_tag::draw(p_hdc, p_x, p_y, p_clip, p_item);
	auto pos = p_item->pos();
	pos.x += p_x;
	pos.y += p_y;
	content_box = Rect2(pos.x, pos.y, pos.width, pos.height);
	if (!content_box.has_area() || fullscreen || css().skips_contents()) return;
	auto* container = static_cast<LiteHTMLContainer*>(get_document()->container());
	auto layer = make_replaced_object_layer(*this, pos, *p_item);
	auto black = layer;
	black.origin_box = black.clip_box;
	container->get_paint_recorder().draw_solid_fill(black, web_color(0, 0, 0, 255));
	if (!available) return;
	if (const auto texture = frame.is_valid() ? frame : container->video_poster(get_attr("poster")); texture.is_valid()) {
		container->get_paint_recorder().draw_image(layer, texture);
	}
	draw_controls(content_box, {
		[&](const Rect2& box, const Color& color) { container->draw_input_chrome(content_box, box, color); },
		[&](HTMLControlIcon icon, const Rect2& box) { container->draw_input_icon(icon, content_box, box, Color(1, 1, 1)); },
		[&](const Rect2& box, const String& caption) {
			const auto line = container->shape_input(css().get_font(), caption);
			const auto& metrics = css().get_font_metrics();
			container->draw_input(box, line, Vector2(box.position.x, box.get_center().y + (metrics.ascent - metrics.descent) * 0.5f),
				web_color(255, 255, 255, 255), {}, {}, {});
		},
	}, has_pseudo_class(_id("focus-visible")));
}

void HTMLVideoElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_src"), &HTMLVideoElement::get_src);
	ClassDB::bind_method(D_METHOD("set_src", "src"), &HTMLVideoElement::set_src);
	ClassDB::bind_method(D_METHOD("get_poster"), &HTMLVideoElement::get_poster);
	ClassDB::bind_method(D_METHOD("set_poster", "poster"), &HTMLVideoElement::set_poster);
	ClassDB::bind_method(D_METHOD("has_controls"), &HTMLVideoElement::has_controls);
	ClassDB::bind_method(D_METHOD("set_controls", "controls"), &HTMLVideoElement::set_controls);
	ClassDB::bind_method(D_METHOD("is_autoplay_enabled"), &HTMLVideoElement::is_autoplay_enabled);
	ClassDB::bind_method(D_METHOD("set_autoplay", "autoplay"), &HTMLVideoElement::set_autoplay);
	ClassDB::bind_method(D_METHOD("is_looping"), &HTMLVideoElement::is_looping);
	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &HTMLVideoElement::set_loop);
	ClassDB::bind_method(D_METHOD("is_muted"), &HTMLVideoElement::is_muted);
	ClassDB::bind_method(D_METHOD("set_muted", "muted"), &HTMLVideoElement::set_muted);
	ClassDB::bind_method(D_METHOD("get_width"), &HTMLVideoElement::get_width);
	ClassDB::bind_method(D_METHOD("set_width", "width"), &HTMLVideoElement::set_width);
	ClassDB::bind_method(D_METHOD("get_height"), &HTMLVideoElement::get_height);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &HTMLVideoElement::set_height);
	ClassDB::bind_method(D_METHOD("get_volume"), &HTMLVideoElement::get_volume);
	ClassDB::bind_method(D_METHOD("set_volume", "volume"), &HTMLVideoElement::set_volume);
	ClassDB::bind_method(D_METHOD("get_current_src"), &HTMLVideoElement::get_current_src);
	ClassDB::bind_method(D_METHOD("get_current_time"), &HTMLVideoElement::get_current_time);
	ClassDB::bind_method(D_METHOD("get_duration"), &HTMLVideoElement::get_duration);
	ClassDB::bind_method(D_METHOD("is_paused"), &HTMLVideoElement::is_paused);
	ClassDB::bind_method(D_METHOD("get_video_width"), &HTMLVideoElement::get_video_width);
	ClassDB::bind_method(D_METHOD("get_video_height"), &HTMLVideoElement::get_video_height);
	ClassDB::bind_method(D_METHOD("play"), &HTMLVideoElement::play);
	ClassDB::bind_method(D_METHOD("pause"), &HTMLVideoElement::pause);
	ClassDB::bind_method(D_METHOD("seek", "seconds"), &HTMLVideoElement::seek);
	ClassDB::bind_method(D_METHOD("request_fullscreen"), &HTMLVideoElement::request_fullscreen);
	ClassDB::bind_method(D_METHOD("exit_fullscreen"), &HTMLVideoElement::exit_fullscreen);
	ClassDB::bind_method(D_METHOD("is_fullscreen"), &HTMLVideoElement::is_fullscreen);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "src", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_src", "get_src");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "poster", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_poster", "get_poster");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "controls", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_controls", "has_controls");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_autoplay", "is_autoplay_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_loop", "is_looping");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "muted", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_muted", "is_muted");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "width", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "height", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_volume", "get_volume");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "current_src", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_current_src");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_time", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_current_time");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_duration");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "is_paused");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "video_width", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_video_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "video_height", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_video_height");
}

std::shared_ptr<HTMLVideoNode> HTMLVideoElement::get_video_node() const {
	return std::dynamic_pointer_cast<HTMLVideoNode>(lock_element());
}

HTMLLabel* HTMLVideoElement::playback_host() const {
	const auto* layout = get_owner();
	return layout ? Object::cast_to<HTMLLabel>(ObjectDB::get_instance(layout->video_host)) : nullptr;
}

String HTMLVideoElement::get_src() const { return get_attribute("src"); }
void HTMLVideoElement::set_src(const String& p_src) { set_attribute("src", p_src); }
String HTMLVideoElement::get_poster() const { return get_attribute("poster"); }
void HTMLVideoElement::set_poster(const String& p_poster) { set_attribute("poster", p_poster); }
bool HTMLVideoElement::has_controls() const { return has_attribute("controls"); }
void HTMLVideoElement::set_controls(bool p_enabled) { toggle_attribute("controls", p_enabled); }
bool HTMLVideoElement::is_autoplay_enabled() const { return has_attribute("autoplay"); }
void HTMLVideoElement::set_autoplay(bool p_enabled) { toggle_attribute("autoplay", p_enabled); }
bool HTMLVideoElement::is_looping() const { return has_attribute("loop"); }
void HTMLVideoElement::set_loop(bool p_enabled) { toggle_attribute("loop", p_enabled); }
int32_t HTMLVideoElement::get_width() const { return std::clamp<int64_t>(get_attribute("width").to_int(), 0, INT32_MAX); }
int32_t HTMLVideoElement::get_height() const { return std::clamp<int64_t>(get_attribute("height").to_int(), 0, INT32_MAX); }
void HTMLVideoElement::set_width(int32_t p_width) { set_attribute("width", String::num_int64(std::max(0, p_width))); }
void HTMLVideoElement::set_height(int32_t p_height) { set_attribute("height", String::num_int64(std::max(0, p_height))); }

bool HTMLVideoElement::is_muted() const {
	const auto node = get_video_node();
	return node && node->is_muted();
}

void HTMLVideoElement::set_muted(bool p_enabled) {
	if (const auto node = get_video_node()) {
		node->muted = p_enabled;
		if (auto* host = playback_host()) host->set_video_volume(*this);
	}
}

double HTMLVideoElement::get_volume() const {
	const auto node = get_video_node();
	return node ? node->volume : 1.0;
}

void HTMLVideoElement::set_volume(double p_volume) {
	if (!std::isfinite(p_volume)) return;
	if (const auto node = get_video_node()) {
		node->volume = std::clamp(p_volume, 0.0, 1.0);
		if (auto* host = playback_host()) host->set_video_volume(*this);
	}
}

String HTMLVideoElement::get_current_src() const {
	const auto node = get_video_node();
	return node ? node->current_src : String();
}

double HTMLVideoElement::get_current_time() const {
	const auto node = get_video_node();
	return node ? node->time : 0.0;
}

double HTMLVideoElement::get_duration() const {
	const auto node = get_video_node();
	return node ? node->duration : 0.0;
}

bool HTMLVideoElement::is_paused() const {
	const auto node = get_video_node();
	return !node || node->paused;
}

int32_t HTMLVideoElement::get_video_width() const {
	const auto node = get_video_node();
	return node ? node->dimensions.x : 0;
}

int32_t HTMLVideoElement::get_video_height() const {
	const auto node = get_video_node();
	return node ? node->dimensions.y : 0;
}

Error HTMLVideoElement::play() {
	auto* host = playback_host();
	return host ? host->play_video(Ref<HTMLVideoElement>(this)) : ERR_UNAVAILABLE;
}

void HTMLVideoElement::pause() {
	if (auto* host = playback_host()) host->pause_video(*this);
}

Error HTMLVideoElement::seek(double p_seconds) {
	auto* host = playback_host();
	return host ? host->seek_video(*this, p_seconds) : ERR_UNAVAILABLE;
}

Error HTMLVideoElement::request_fullscreen() {
	auto* host = playback_host();
	return host ? host->enter_video_fullscreen(Ref<HTMLVideoElement>(this)) : ERR_UNAVAILABLE;
}

void HTMLVideoElement::exit_fullscreen() {
	if (auto* host = playback_host(); host && host->get_fullscreen_video().ptr() == this) host->leave_video_fullscreen();
}

bool HTMLVideoElement::is_fullscreen() const {
	const auto node = get_video_node();
	return node && node->fullscreen;
}
