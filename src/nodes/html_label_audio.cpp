#include "html_label.h"
#include "formats/html/elements/html_audio_node.h"
#include "formats/html/html_ui_translations.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace godot;
using dockit::HTMLAudioPart;

namespace {
	PackedStringArray godot_audio_bus_names() {
		auto* server = AudioServer::get_singleton();
		PackedStringArray names;
		for (int32_t i = 0; i < server->get_bus_count(); ++i) names.append(String(server->get_bus_name(i)));
		return names;
	}

	Object* fmod_server() {
		auto* engine = Engine::get_singleton();
		if (!engine->has_singleton("FmodServer") || !ClassDB::class_exists("FmodAudioStreamPlayer") ||
			!ClassDB::class_exists("FmodAudioStream") || !ClassDB::class_has_method("FmodServer", "get_bus_names")) return nullptr;
		return engine->get_singleton("FmodServer");
	}

	Ref<Resource> private_audio_stream(const Ref<Resource>& p_resource, bool p_fmod) {
		if (p_resource.is_null()) return {};
		if (p_fmod ? !p_resource->is_class("FmodAudioStream") : !Object::cast_to<AudioStream>(p_resource.ptr())) return {};
		const Ref<Resource> stream = p_resource->duplicate();
		if (stream.is_null()) return {};
		// End-of-stream handling applies the current HTML loop flag independently of imported defaults.
		if (auto* wav = Object::cast_to<AudioStreamWAV>(stream.ptr())) {
			wav->set_loop_mode(AudioStreamWAV::LOOP_DISABLED);
		}
		else if (p_fmod) {
			const auto loop_flags = ClassDB::class_get_integer_constant("FmodAudioStream", "MODE_LOOP") |
				ClassDB::class_get_integer_constant("FmodAudioStream", "MODE_LOOP_BIDI");
			stream->call("set_mode_flags", int64_t(stream->call("get_mode_flags")) & ~loop_flags);
		}
		else if (stream->has_method("set_loop")) {
			stream->call("set_loop", false);
		}
		return stream;
	}

	constexpr std::array AUDIO_PARTS{ HTMLAudioPart::play, HTMLAudioPart::position, HTMLAudioPart::mute, HTMLAudioPart::volume };

	bool is_audio_slider(HTMLAudioPart p_part) {
		return p_part == HTMLAudioPart::position || p_part == HTMLAudioPart::volume;
	}

	void move_audio_slider(HTMLAudioElement& p_element, HTMLAudioPart p_part, const Rect2& p_box, float p_x) {
		if (p_box.size.x <= 10.0f) return;
		const float fraction = std::clamp((p_x - p_box.position.x - 5.0f) / (p_box.size.x - 10.0f), 0.0f, 1.0f);
		if (p_part == HTMLAudioPart::position) p_element.set_current_time(p_element.get_duration() * fraction);
		else {
			p_element.set_volume(fraction);
			if (fraction > 0.0f && p_element.is_muted()) p_element.set_muted(false);
		}
	}
}

bool HTMLLabel::uses_fmod_audio() const {
	return use_fmod_bus && fmod_server();
}

PackedStringArray HTMLLabel::audio_bus_names() const {
	if (uses_fmod_audio()) return ClassDB::class_call_static("FmodServer", "get_bus_names");
	return godot_audio_bus_names();
}

StringName HTMLLabel::effective_audio_bus() const {
	if (!uses_fmod_audio()) {
		return AudioServer::get_singleton()->get_bus_index(bus) >= 0 ? bus : StringName("Master");
	}
	return audio_bus_names().has(String(bus)) ? bus : StringName("Master");
}

void HTMLLabel::validate_audio_property(PropertyInfo& p_property) const {
	if (p_property.name == StringName("bus")) p_property.hint_string = String(",").join(audio_bus_names());
	if (p_property.name == StringName("video_bus")) p_property.hint_string = String(",").join(godot_audio_bus_names());
	if (p_property.name == StringName("use_fmod_bus") && !fmod_server()) p_property.usage &= ~PROPERTY_USAGE_EDITOR;
}

void HTMLLabel::connect_audio_buses(bool p_connect) {
	const auto layout_changed = callable_mp(this, &HTMLLabel::audio_buses_changed);
	const auto renamed = callable_mp(this, &HTMLLabel::audio_bus_renamed);
	const auto update = [p_connect](Object* p_server, const StringName& p_signal, const Callable& p_callback) {
		if (!p_server || !p_server->has_signal(p_signal)) return;
		const bool connected = p_server->is_connected(p_signal, p_callback);
		if (p_connect && !connected) p_server->connect(p_signal, p_callback);
		else if (!p_connect && connected) p_server->disconnect(p_signal, p_callback);
		};
	update(AudioServer::get_singleton(), "bus_layout_changed", layout_changed);
	update(AudioServer::get_singleton(), "bus_renamed", renamed);
	update(fmod_server(), "bus_layout_changed", layout_changed);
	if (p_connect) notify_property_list_changed();
}

void HTMLLabel::audio_bus_renamed(int64_t, const StringName&, const StringName&) {
	audio_buses_changed();
}

void HTMLLabel::audio_buses_changed() {
	const auto effective = effective_audio_bus();
	for (auto& playback : audio_playbacks) {
		if (playback.player) playback.player->call("set_bus", effective);
	}
	for (auto& playback : video_playbacks) apply_video_audio(playback);
	notify_property_list_changed();
}

void HTMLLabel::set_bus(const StringName& p_bus) {
	if (bus == p_bus) return;
	bus = p_bus;
	audio_buses_changed();
}

StringName HTMLLabel::get_bus() const { return bus; }

void HTMLLabel::set_use_fmod_bus(bool p_enable) {
	if (use_fmod_bus == p_enable) return;
	const bool was_fmod = uses_fmod_audio();
	use_fmod_bus = p_enable;
	if (was_fmod != uses_fmod_audio()) {
		for (auto& playback : audio_playbacks) destroy_audio(playback);
		if (layout.is_valid()) layout->repaint_input();
		update_internal_processing();
	}
	notify_property_list_changed();
}

bool HTMLLabel::is_using_fmod_bus() const { return use_fmod_bus; }

HTMLLabel::AudioPlayback* HTMLLabel::find_audio(const HTMLAudioElement& p_element) {
	const auto found = std::find_if(audio_playbacks.begin(), audio_playbacks.end(),
		[&](const AudioPlayback& p_audio) { return p_audio.element.ptr() == &p_element; });
	return found == audio_playbacks.end() ? nullptr : &*found;
}

HTMLLabel::AudioPlayback& HTMLLabel::audio_entry(const Ref<HTMLAudioElement>& p_element) {
	const auto sources = layout->audio_sources(*p_element.ptr());
	if (auto* entry = find_audio(*p_element.ptr())) {
		if (entry->sources != sources) {
			destroy_audio(*entry);
			entry->sources = sources;
			entry->autoplay_attempted = false;
			entry->load_started = false;
			entry->loaded = false;
		}
		return *entry;
	}
	audio_playbacks.push_back({ p_element, sources });
	return audio_playbacks.back();
}

Error HTMLLabel::prepare_audio(AudioPlayback& p_audio) {
	if (p_audio.player) return OK;
	const auto node = p_audio.element->get_audio_node();
	if (!node || !is_inside_tree() || !layout->is_element_connected(p_audio.element)) return ERR_UNCONFIGURED;
	if (!p_audio.load_started) {
		p_audio.load_started = true;
		dispatch_media_event(StringName("loadstart"), p_audio.element);
	}
	const bool fmod = uses_fmod_audio();
	for (const String& path : p_audio.sources) {
		const auto stream = private_audio_stream(layout->audio_resource(path, fmod), fmod);
		if (stream.is_null()) continue;
		Node* player = nullptr;
		if (fmod) {
			Object* instance = ClassDB::instantiate("FmodAudioStreamPlayer");
			player = Object::cast_to<Node>(instance);
		}
		else {
			player = memnew(AudioStreamPlayer);
		}
		if (!player) return ERR_UNAVAILABLE;
		add_child(player, false, INTERNAL_MODE_BACK);
		player->call("set_stream", stream);
		if (fmod && !bool(player->call("preload_stream"))) {
			player->queue_free();
			continue;
		}
		player->call("set_bus", effective_audio_bus());
		player->connect("finished", callable_mp(this, &HTMLLabel::audio_finished).bind(uint64_t(player->get_instance_id())));
		p_audio.player = player;
		node->current_src = path;
		const double duration = stream->call("get_length");
		node->duration = std::isfinite(duration) && duration > 0.0 ? duration : 0.0;
		set_audio_player_volume(p_audio);
		if (!p_audio.loaded) {
			p_audio.loaded = true;
			dispatch_media_event(StringName("loadedmetadata"), p_audio.element);
			dispatch_media_event(StringName("loadeddata"), p_audio.element);
			dispatch_media_event(StringName("canplay"), p_audio.element);
		}
		return OK;
	}
	dispatch_media_error_event(p_audio.element,
		dockit::html_ui_translate("No supported audio source could be opened.", "media.error"), ERR_FILE_NOT_FOUND);
	return ERR_FILE_NOT_FOUND;
}

Error HTMLLabel::play_audio(const Ref<HTMLAudioElement>& p_element) {
	if (!is_inside_tree() || !layout->is_element_connected(p_element)) return ERR_UNCONFIGURED;
	auto& audio = audio_entry(p_element);
	audio.autoplay_attempted = true;
	const Error error = prepare_audio(audio);
	if (error != OK) return error;
	const auto node = p_element->get_audio_node();
	const bool was_paused = node->paused;
	if (audio.started) {
		audio.player->call("set_stream_paused", false);
	}
	else {
		if (node->duration > 0.0 && node->time >= node->duration) node->time = 0.0;
		audio.player->call("set_stream_paused", false);
		audio.player->call("play", node->time);
		if (!bool(audio.player->call("is_playing"))) return ERR_CANT_CREATE;
		audio.started = true;
	}
	node->paused = false;
	if (was_paused) {
		dispatch_media_event(StringName("play"), p_element);
		dispatch_media_event(StringName("playing"), p_element);
	}
	layout->repaint_input();
	update_internal_processing();
	return OK;
}

void HTMLLabel::pause_audio(const HTMLAudioElement& p_element) {
	auto* entry = find_audio(p_element);
	const auto node = p_element.get_audio_node();
	if (!entry || !entry->started || !node || node->paused) return;
	node->time = entry->player->call("get_playback_position");
	entry->player->call("set_stream_paused", true);
	node->paused = true;
	dispatch_media_event(StringName("pause"), entry->element);
	layout->repaint_input();
	update_internal_processing();
}

void HTMLLabel::seek_audio(const Ref<HTMLAudioElement>& p_element, double p_time) {
	if (!is_inside_tree() || !layout->is_element_connected(p_element)) return;
	auto& audio = audio_entry(p_element);
	if (prepare_audio(audio) != OK) return;
	const auto node = p_element->get_audio_node();
	dispatch_media_event(StringName("seeking"), p_element);
	node->time = std::clamp(p_time, 0.0, node->duration);
	if (audio.started && node->paused) {
		// The built-in player only seeks while actively playing.
		audio.player->call("stop");
		audio.started = false;
	}
	else if (audio.started) {
		audio.player->call("seek", node->time);
	}
	dispatch_media_event(StringName("seeked"), p_element);
	dispatch_media_event(StringName("timeupdate"), p_element);
	layout->repaint_input();
}

void HTMLLabel::set_audio_player_volume(AudioPlayback& p_audio) {
	if (!p_audio.player) return;
	p_audio.muted = p_audio.element->is_muted();
	const double linear = p_audio.muted ? 0.0 : p_audio.element->get_volume();
	const double db = linear > 0.0 ? 20.0 * std::log10(linear) : -std::numeric_limits<double>::infinity();
	p_audio.player->call("set_volume_db", db);
}

void HTMLLabel::set_audio_volume(const HTMLAudioElement& p_element) {
	if (auto* entry = find_audio(p_element)) set_audio_player_volume(*entry);
	dispatch_media_event(StringName("volumechange"), Ref<HTMLAudioElement>(const_cast<HTMLAudioElement*>(&p_element)));
	layout->repaint_input();
}

void HTMLLabel::destroy_audio(AudioPlayback& p_audio) {
	if (p_audio.player) {
		p_audio.player->call("stop");
		p_audio.player->queue_free();
		p_audio.player = nullptr;
	}
	p_audio.started = false;
	p_audio.load_started = false;
	p_audio.loaded = false;
	if (const auto node = p_audio.element->get_audio_node()) {
		node->current_src = String();
		node->paused = true;
		node->duration = 0.0;
		node->time = 0.0;
	}
}

void HTMLLabel::clear_audio() {
	for (auto& playback : audio_playbacks) destroy_audio(playback);
	audio_playbacks.clear();
}

void HTMLLabel::queue_audio_sync() {
	if (!is_inside_tree() || audio_sync_queued) return;
	audio_sync_queued = true;
	callable_mp(this, &HTMLLabel::sync_audio).call_deferred();
}

void HTMLLabel::sync_audio() {
	audio_sync_queued = false;
	if (!is_inside_tree() || source_dirty) return;
	bool repaint = false;
	for (auto entry = audio_playbacks.begin(); entry != audio_playbacks.end();) {
		if (!layout->is_element_connected(entry->element)) {
			destroy_audio(*entry);
			entry = audio_playbacks.erase(entry);
			repaint = true;
			continue;
		}
		const auto sources = layout->audio_sources(*entry->element.ptr());
		if (entry->sources != sources) {
			destroy_audio(*entry);
			entry->sources = sources;
			entry->autoplay_attempted = false;
			repaint = true;
		}
		if (entry->muted != entry->element->is_muted()) {
			set_audio_player_volume(*entry);
			repaint = true;
		}
		++entry;
	}
	if (!Engine::get_singleton()->is_editor_hint()) {
		for (const Ref<HTMLAudioElement>& element : layout->audio_elements()) {
			if (!element->is_autoplay_enabled()) continue;
			auto& entry = audio_entry(element);
			if (!entry.autoplay_attempted) play_audio(element);
		}
	}
	if (repaint) layout->repaint_input();
	update_internal_processing();
}

bool HTMLLabel::has_playing_audio() const {
	return std::any_of(audio_playbacks.begin(), audio_playbacks.end(), [](const AudioPlayback& p_audio) {
		return p_audio.started && !p_audio.element->is_paused();
		});
}

void HTMLLabel::poll_audio() {
	bool repaint = false;
	for (auto& playback : audio_playbacks) {
		const auto node = playback.element->get_audio_node();
		if (!node || node->paused || !playback.started) continue;
		const double next = playback.player->call("get_playback_position");
		if (std::isfinite(next) && std::abs(node->time - next) >= 0.25) {
			node->time = next;
			dispatch_media_event(StringName("timeupdate"), playback.element);
			repaint |= node->has_controls();
		}
	}
	if (repaint) layout->repaint_input();
}

void HTMLLabel::audio_finished(uint64_t p_player) {
	for (auto& playback : audio_playbacks) {
		if (!playback.player || uint64_t(playback.player->get_instance_id()) != p_player) continue;
		if (const auto node = playback.element->get_audio_node()) {
			playback.started = false;
			node->paused = true;
			node->time = node->duration;
			if (playback.element->is_looping()) play_audio(playback.element);
			else dispatch_media_event(StringName("ended"), playback.element);
			layout->repaint_input();
		}
		break;
	}
	update_internal_processing();
}

bool HTMLLabelCanvas::handle_audio_button(const InputEventMouseButton& p_button, const Vector2& p_point) {
	if (p_button.get_button_index() != MOUSE_BUTTON_LEFT) return false;
	const Ref<HTMLAudioElement> hit = label->get_visible_rect().has_point(p_point) ?
		label->layout->get_element_at(p_point) : Ref<HTMLElement>();
	const auto node = hit.is_valid() ? hit->get_audio_node() : nullptr;
	const auto local = node ? label->layout->input_local_point(*hit.ptr(), p_point) : std::nullopt;
	const auto part = local ? node->part_at(*local) : HTMLAudioPart::none;
	if (p_button.is_pressed()) {
		if (part == HTMLAudioPart::none) return false;
		cancel_press();
		label->grab_focus();
		set_focused_element(hit);
		pressed_audio = hit;
		pressed_audio_part = part;
		node->focus_part = part;
		if (part == HTMLAudioPart::position) label->prepare_audio(label->audio_entry(hit));
		if (is_audio_slider(part)) move_audio_slider(*hit.ptr(), part, node->part_rect(part), local->x);
		label->layout->repaint_input();
		return true;
	}
	if (pressed_audio.is_null()) return false;
	const auto pressed = pressed_audio;
	const auto pressed_part = pressed_audio_part;
	if (!p_button.is_canceled() && is_audio_slider(pressed_part)) move_audio_control(p_point);
	pressed_audio.unref();
	pressed_audio_part = HTMLAudioPart::none;
	if (!p_button.is_canceled() && pressed == hit && pressed_part == part) {
		if (part == HTMLAudioPart::play) {
			if (pressed->is_paused()) pressed->play();
			else pressed->pause();
		}
		else if (part == HTMLAudioPart::mute) pressed->set_muted(!pressed->is_muted());
	}
	return true;
}

bool HTMLLabelCanvas::move_audio_control(const Vector2& p_point) {
	if (pressed_audio.is_null()) return false;
	if (is_audio_slider(pressed_audio_part)) {
		const auto node = pressed_audio->get_audio_node();
		const auto local = node ? label->layout->input_local_point(*pressed_audio.ptr(), p_point) : std::nullopt;
		if (local) move_audio_slider(*pressed_audio.ptr(), pressed_audio_part, node->part_rect(pressed_audio_part), local->x);
	}
	return true;
}

bool HTMLLabelCanvas::handle_audio_key(const InputEventKey& p_key) {
	const Ref<HTMLAudioElement> audio = focused_element;
	const auto node = audio.is_valid() ? audio->get_audio_node() : nullptr;
	if (!node || !node->has_controls() || p_key.is_alt_pressed() || p_key.is_ctrl_pressed() || p_key.is_meta_pressed()) return false;
	const Key key = p_key.get_keycode() != KEY_NONE ? p_key.get_keycode() : p_key.get_physical_keycode();
	if (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_SPACE) {
		if (p_key.is_pressed() && !p_key.is_echo()) {
			if (node->focus_part == HTMLAudioPart::mute) audio->set_muted(!audio->is_muted());
			else if (audio->is_paused()) audio->play();
			else audio->pause();
		}
	}
	else if (p_key.is_pressed() && (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN)) {
		const int direction = key == KEY_RIGHT || key == KEY_UP ? 1 : -1;
		if (node->focus_part == HTMLAudioPart::position) audio->set_current_time(audio->get_current_time() + direction * 5.0);
		else if (node->focus_part == HTMLAudioPart::volume) audio->set_volume(audio->get_volume() + direction * 0.05);
		else return false;
	}
	else return false;
	label->accept_event();
	return true;
}

bool HTMLLabelCanvas::move_audio_focus(bool p_backward) {
	const Ref<HTMLAudioElement> audio = focused_element;
	const auto node = audio.is_valid() ? audio->get_audio_node() : nullptr;
	if (!node || !node->has_controls()) return false;
	const auto current = std::find(AUDIO_PARTS.begin(), AUDIO_PARTS.end(), node->focus_part);
	const int index = current - AUDIO_PARTS.begin();
	const int step = p_backward ? -1 : 1;
	for (int next = index + step; next >= 0 && next < int(AUDIO_PARTS.size()); next += step) {
		if (!node->part_rect(AUDIO_PARTS[next]).has_area()) continue;
		node->focus_part = AUDIO_PARTS[next];
		set_element_pseudo(focused_element, "focus-visible", true);
		label->layout->repaint_input();
		return true;
	}
	return false;
}

void HTMLLabelCanvas::set_audio_focus_edge(bool p_backward) {
	const Ref<HTMLAudioElement> audio = focused_element;
	const auto node = audio.is_valid() ? audio->get_audio_node() : nullptr;
	if (!node) return;
	node->focus_part = HTMLAudioPart::play;
	if (p_backward) {
		for (auto part = AUDIO_PARTS.rbegin(); part != AUDIO_PARTS.rend(); ++part) {
			if (!node->part_rect(*part).has_area()) continue;
			node->focus_part = *part;
			break;
		}
	}
	label->layout->repaint_input();
}
