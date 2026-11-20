#include "html_label.h"

#include "formats/html/elements/html_video_node.h"
#include "formats/html/html_ui_translations.h"
#include "formats/html/resources/html_resource_paths.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/video_stream.hpp>
#include <godot_cpp/classes/video_stream_player.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/godot.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

using namespace godot;
using dockit::HTMLVideoNode;
using dockit::HTMLVideoPart;
using dockit::HTMLControlIcon;

namespace {
	constexpr double OPEN_TIMEOUT = 8.0;
	constexpr double CONTROLS_TIMEOUT = 2.5;
	constexpr auto PLAYERS = std::array{ HTMLLabel::VIDEO_BACKEND_FFMPEG, HTMLLabel::VIDEO_BACKEND_VLC };
	constexpr auto PARTS = std::array{ HTMLVideoPart::play, HTMLVideoPart::position, HTMLVideoPart::mute,
		HTMLVideoPart::volume, HTMLVideoPart::fullscreen };
	constexpr const char* VLC_PLAYER = "VLCMediaPlayer";
	std::unordered_map<uint64_t, ObjectID> fullscreen_owners;

	GDExtensionInterfaceClassdbConstructObject editor_video_constructor() {
		// ClassDB::instantiate may return a placeholder for runtime extension classes.
		// The native constructor creates the real instance and runs postinitialization.
		static const auto construct = reinterpret_cast<GDExtensionInterfaceClassdbConstructObject>(
			internal::gdextension_interface_get_proc_address("classdb_construct_object"));
		return construct;
	}

	Variant instantiate_video_backend(const StringName& p_class) {
		if (!Engine::get_singleton()->is_editor_hint()) return ClassDB::instantiate(p_class);
		const auto construct = editor_video_constructor();
		if (!construct) return {};
		return internal::get_object_instance_binding(construct(p_class._native_ptr()));
	}

	bool backend_available(HTMLLabel::VideoBackend p_backend) {
		if (Engine::get_singleton()->is_editor_hint() && !editor_video_constructor()) return false;
		if (p_backend == HTMLLabel::VIDEO_BACKEND_FFMPEG) {
			return ClassDB::class_exists("FFmpegVideoStream") && ClassDB::can_instantiate("FFmpegVideoStream") &&
				ClassDB::class_has_method("FFmpegVideoStream", "set_file");
		}
		if (!ClassDB::class_exists(VLC_PLAYER) || !ClassDB::class_exists("VLCMedia") ||
			!ClassDB::can_instantiate(VLC_PLAYER) || !ClassDB::class_has_method("VLCMedia", "load_from_file")) return false;
		for (const char* method : {"set_media", "get_texture", "play", "set_pause", "stop_async", "get_time",
				"get_length", "set_time", "get_state", "is_seekable", "set_bus", "set_volume_db"}) {
			if (!ClassDB::class_has_method(VLC_PLAYER, method)) return false;
		}
		return !ClassDB::class_exists("VLCInstance") || !ClassDB::class_has_method("VLCInstance", "has_instance") ||
			bool(ClassDB::class_call_static("VLCInstance", "has_instance"));
	}

	bool vlc_state(Node& p_player, const StringName& p_state) {
		return ClassDB::class_has_integer_constant(VLC_PLAYER, p_state) &&
			int64_t(p_player.call("get_state")) == ClassDB::class_get_integer_constant(VLC_PLAYER, p_state);
	}

	String local_video_file(const String& p_path) {
		const String packed = dockit::packed_html_video_path(p_path);
		if (!packed.is_empty() && FileAccess::file_exists(packed)) return packed;
		return FileAccess::file_exists(p_path) ? p_path : String();
	}

	bool slider(HTMLVideoPart p_part) {
		return p_part == HTMLVideoPart::position || p_part == HTMLVideoPart::volume;
	}

	Key event_key(const InputEventKey& p_key) {
		return p_key.get_keycode() != KEY_NONE ? p_key.get_keycode() : p_key.get_physical_keycode();
	}

	std::vector<HTMLVideoPart> focus_parts(const HTMLVideoNode& p_node, const Rect2& p_box) {
		std::vector<HTMLVideoPart> parts;
		for (const auto part : PARTS) {
			if ((part != HTMLVideoPart::position || p_node.seekable) && p_node.part_rect(p_box, part).has_area()) parts.push_back(part);
		}
		return parts;
	}
}

void HTMLLabel::set_video_backend(VideoBackend p_backend) {
	if (p_backend < VIDEO_BACKEND_AUTO || p_backend > VIDEO_BACKEND_VLC || p_backend == video_backend) return;
	clear_videos();
	video_backend = p_backend;
	queue_video_sync();
}

HTMLLabel::VideoBackend HTMLLabel::get_video_backend() const { return video_backend; }
StringName HTMLLabel::get_video_bus() const { return video_bus; }
Ref<HTMLVideoElement> HTMLLabel::get_fullscreen_video() const { return fullscreen_video; }

void HTMLLabel::set_video_bus(const StringName& p_bus) {
	video_bus = p_bus;
	for (auto& video : video_playbacks) apply_video_audio(video);
}

bool HTMLLabel::can_play_video() const {
	if (video_backend != VIDEO_BACKEND_AUTO) return backend_available(video_backend);
	return backend_available(VIDEO_BACKEND_FFMPEG) || backend_available(VIDEO_BACKEND_VLC);
}

HTMLLabel::VideoPlayback* HTMLLabel::find_video(const HTMLVideoElement& p_element) {
	const auto found = std::find_if(video_playbacks.begin(), video_playbacks.end(),
		[&](const VideoPlayback& p_entry) { return p_entry.element.ptr() == &p_element; });
	return found == video_playbacks.end() ? nullptr : &*found;
}

HTMLLabel::VideoPlayback& HTMLLabel::video_entry(const Ref<HTMLVideoElement>& p_element) {
	const auto paths = layout->video_paths(*p_element.ptr());
	if (auto* entry = find_video(*p_element.ptr())) {
		if (entry->paths != paths) {
			if (fullscreen_video == p_element) leave_video_fullscreen();
			destroy_video(*entry);
			entry->paths = paths;
			entry->autoplay_attempted = false;
			entry->load_started = false;
			entry->loaded_metadata = false;
			entry->loaded_data = false;
			entry->playing_event_sent = false;
		}
		return *entry;
	}
	return video_playbacks.emplace_back(VideoPlayback{p_element, paths});
}

Error HTMLLabel::open_next_video(VideoPlayback& r_playback, bool p_play) {
	const auto node = r_playback.element->get_video_node();
	if (!node) return ERR_UNCONFIGURED;
	if (!r_playback.load_started) {
		r_playback.load_started = true;
		dispatch_media_event(StringName("loadstart"), r_playback.element);
	}
	while (r_playback.next_candidate < r_playback.paths.size() * int64_t(PLAYERS.size())) {
		const int64_t index = r_playback.next_candidate++;
		const String source = r_playback.paths[index / PLAYERS.size()];
		const auto backend = PLAYERS[index % PLAYERS.size()];
		if ((video_backend != VIDEO_BACKEND_AUTO && video_backend != backend) || !backend_available(backend)) continue;
		const String path = local_video_file(source);
		if (path.is_empty()) continue;

		if (backend == VIDEO_BACKEND_FFMPEG) {
			const Ref<VideoStream> stream = instantiate_video_backend("FFmpegVideoStream");
			if (stream.is_null()) continue;
			stream->call("set_file", path);
			auto* player = memnew(VideoStreamPlayer);
			player->hide();
			add_child(player, false, INTERNAL_MODE_BACK);
			player->set_stream(stream);
			if (player->get_video_texture().is_null()) {
				player->queue_free();
				continue;
			}
			player->connect("finished", callable_mp(this, &HTMLLabel::video_finished).bind(uint64_t(player->get_instance_id())));
			r_playback.player = player;
		}
		else {
			const Ref<Resource> media = ClassDB::class_call_static("VLCMedia", "load_from_file", path);
			if (media.is_null()) continue;
			auto* player = Object::cast_to<Control>(instantiate_video_backend(VLC_PLAYER));
			if (!player) continue;
			player->hide();
			player->set("force_hardware", false);
			add_child(player, false, INTERNAL_MODE_BACK);
			player->call("set_media", media);
			r_playback.player = player;
		}

		r_playback.backend = backend;
		r_playback.awaiting_frame = true;
		r_playback.preparing = !p_play;
		r_playback.opening_time = 0;
		r_playback.controls_idle = 0;
		r_playback.started = p_play;
		if (node->current_src != source) node->duration = 0;
		node->current_src = source;
		node->time = 0;
		node->seekable = false;
		node->paused = !p_play;
		node->show_controls = true;
		apply_video_audio(r_playback);
		// Setting the FFmpeg stream creates the playback and exposes its dimensions.
		// VLC exposes the video size through its first decoded texture.
		if (!p_play && backend == VIDEO_BACKEND_FFMPEG) return OK;
		const Variant result = r_playback.player->call("play");
		if (result.get_type() != Variant::INT || int64_t(result) == 0) return OK;
		stop_video_player(r_playback);
	}
	node->current_src = String();
	node->paused = true;
	node->show_controls = true;
	return r_playback.paths.is_empty() ? ERR_FILE_NOT_FOUND : ERR_CANT_OPEN;
}

Error HTMLLabel::play_video(const Ref<HTMLVideoElement>& p_element) {
	if (!can_play_video()) return ERR_UNAVAILABLE;
	if (!is_inside_tree() || !layout->is_element_connected(p_element)) return ERR_UNCONFIGURED;
	const auto node = p_element->get_video_node();
	if (!node) return ERR_UNCONFIGURED;
	const bool was_paused = node->paused;
	auto& entry = video_entry(p_element);
	entry.autoplay_attempted = true;
	entry.preparation_attempted = true;
	if (entry.preparing) stop_video_player(entry);
	if (entry.player) {
		if (!entry.started) {
			if (entry.backend == VIDEO_BACKEND_FFMPEG) Object::cast_to<VideoStreamPlayer>(entry.player)->set_paused(false);
			entry.player->call("play");
			entry.started = true;
		}
		else if (node->paused) {
			if (entry.backend == VIDEO_BACKEND_FFMPEG) Object::cast_to<VideoStreamPlayer>(entry.player)->set_paused(false);
			else entry.player->call("set_pause", false);
		}
		node->paused = false;
		if (was_paused) dispatch_media_event(StringName("play"), p_element);
		wake_video_controls(*p_element.ptr());
		update_internal_processing();
		return OK;
	}
	entry.next_candidate = 0;
	const Error error = open_next_video(entry, true);
	if (error == OK && was_paused) dispatch_media_event(StringName("play"), p_element);
	update_internal_processing();
	redraw_video();
	if (error != OK) notify_video_failure(p_element, error);
	return error;
}

void HTMLLabel::pause_video(const HTMLVideoElement& p_element) {
	auto* entry = find_video(p_element);
	const auto node = p_element.get_video_node();
	if (!entry || !entry->player || !node || node->paused) return;
	if (entry->backend == VIDEO_BACKEND_FFMPEG) Object::cast_to<VideoStreamPlayer>(entry->player)->set_paused(true);
	else entry->player->call("set_pause", true);
	node->paused = true;
	dispatch_media_event(StringName("pause"), Ref<HTMLVideoElement>(const_cast<HTMLVideoElement*>(&p_element)));
	wake_video_controls(p_element);
	update_internal_processing();
}

Error HTMLLabel::seek_video(const HTMLVideoElement& p_element, double p_seconds) {
	if (!std::isfinite(p_seconds) || p_seconds < 0) return ERR_INVALID_PARAMETER;
	auto* entry = find_video(p_element);
	const auto node = p_element.get_video_node();
	if (!entry || !entry->player || !node || !node->seekable) return ERR_UNAVAILABLE;
	const double target = std::min(p_seconds, node->duration);
	dispatch_media_event(StringName("seeking"), Ref<HTMLVideoElement>(const_cast<HTMLVideoElement*>(&p_element)));
	if (entry->backend == VIDEO_BACKEND_FFMPEG) {
		Object::cast_to<VideoStreamPlayer>(entry->player)->set_stream_position(target);
	}
	else if (int64_t(entry->player->call("set_time", int64_t(std::round(target * 1000.0)), false)) != 0) {
		return FAILED;
	}
	node->time = target;
	dispatch_media_event(StringName("seeked"), Ref<HTMLVideoElement>(const_cast<HTMLVideoElement*>(&p_element)));
	dispatch_media_event(StringName("timeupdate"), Ref<HTMLVideoElement>(const_cast<HTMLVideoElement*>(&p_element)));
	wake_video_controls(p_element);
	return OK;
}

void HTMLLabel::apply_video_audio(VideoPlayback& r_playback) {
	const auto node = r_playback.element->get_video_node();
	if (!node || !r_playback.player) return;
	r_playback.applied_muted = node->is_muted();
	const double volume = (r_playback.preparing || r_playback.applied_muted) ? 0.0 : node->volume;
	const StringName bus_name = AudioServer::get_singleton()->get_bus_index(video_bus) >= 0 ? video_bus : StringName("Master");
	r_playback.player->call("set_bus", bus_name);
	r_playback.player->call("set_volume_db", volume > 0 ? std::max(-80.0, 20.0 * std::log10(volume)) :
		-std::numeric_limits<float>::infinity());
	if (auto* native = Object::cast_to<VideoStreamPlayer>(r_playback.player)) native->set_volume(volume);
}

void HTMLLabel::set_video_volume(const HTMLVideoElement& p_element) {
	if (auto* entry = find_video(p_element)) apply_video_audio(*entry);
	dispatch_media_event(StringName("volumechange"), Ref<HTMLVideoElement>(const_cast<HTMLVideoElement*>(&p_element)));
	redraw_video();
}

void HTMLLabel::stop_video_player(VideoPlayback& r_playback) {
	if (!r_playback.player) return;
	auto* player = std::exchange(r_playback.player, nullptr);
	player->call(r_playback.backend == VIDEO_BACKEND_FFMPEG ? "stop" : "stop_async");
	player->queue_free();
	r_playback.started = false;
	r_playback.awaiting_frame = false;
	r_playback.preparing = false;
	r_playback.playing_event_sent = false;
}

void HTMLLabel::destroy_video(VideoPlayback& r_playback) {
	stop_video_player(r_playback);
	r_playback.preparation_attempted = false;
	if (const auto node = r_playback.element->get_video_node()) {
		node->paused = true;
		node->seekable = false;
		node->seek_preview.reset();
		node->current_src = String();
		node->frame.unref();
		node->time = node->duration = 0;
		node->show_controls = true;
		if (std::exchange(node->dimensions, Vector2i()) != Vector2i()) {
			layout->video_dimensions_changed();
			layout_updated = true;
			queue_refresh();
		}
	}
}

void HTMLLabel::clear_videos() {
	leave_video_fullscreen();
	for (auto& playback : video_playbacks) destroy_video(playback);
	video_playbacks.clear();
}

void HTMLLabel::queue_video_sync() {
	if (is_inside_tree() && !video_sync_queued) {
		video_sync_queued = true;
		callable_mp(this, &HTMLLabel::sync_videos).call_deferred();
	}
}

void HTMLLabel::sync_videos() {
	video_sync_queued = false;
	if (!is_inside_tree() || source_dirty) return;
	const bool available = can_play_video();
	const bool running_game = !Engine::get_singleton()->is_editor_hint();
	const bool prepare_video = running_game || DisplayServer::get_singleton()->get_name() != "headless";
	bool repaint = false;
	for (auto it = video_playbacks.begin(); it != video_playbacks.end();) {
		if (!layout->is_element_connected(it->element)) {
			if (fullscreen_video == it->element) leave_video_fullscreen();
			destroy_video(*it);
			it = video_playbacks.erase(it);
			repaint = true;
			continue;
		}
		const auto paths = layout->video_paths(*it->element.ptr());
		if (paths != it->paths) {
			if (fullscreen_video == it->element) leave_video_fullscreen();
			destroy_video(*it);
			it->paths = paths;
			it->autoplay_attempted = false;
			it->load_started = false;
			it->loaded_metadata = false;
			it->loaded_data = false;
			it->playing_event_sent = false;
			repaint = true;
		}
		if (it->element->is_muted() != it->applied_muted) apply_video_audio(*it);
		++it;
	}
	const auto elements = layout->video_elements();
	video_present = !elements.is_empty();
	for (const Ref<HTMLVideoElement>& element : elements) {
		const auto node = element->get_video_node();
		if (node->available != available) {
			node->available = available;
			repaint = true;
		}
		if (available && running_game && element->is_autoplay_enabled()) {
			if (auto& entry = video_entry(element); !entry.autoplay_attempted) play_video(element);
		}
		else if (prepare_video && node->has_controls()) {
			auto& entry = video_entry(element);
			if (!entry.preparation_attempted) {
				entry.preparation_attempted = true;
				entry.next_candidate = 0;
				open_next_video(entry, false);
			}
		}
	}
	if (repaint) {
		redraw_video();
		queue_refresh();
		queue_interaction();
	}
	update_internal_processing();
}

bool HTMLLabel::has_active_video() const {
	for (const auto& entry : video_playbacks) {
		if (entry.player && (entry.preparing || (entry.started && !entry.element->is_paused()))) return true;
	}
	return false;
}

void HTMLLabel::notify_video_failure(const Ref<HTMLVideoElement>& p_element, Error p_error) {
	const String message = dockit::html_ui_translate(p_error == ERR_FILE_NOT_FOUND ?
		"No supported local video source was found." : "The selected video backends could not open any source.", "media.error");
	const ObjectID id(get_instance_id());
	dispatch_media_error_event(p_element, message, p_error);
	if (ObjectDB::get_instance(id)) callable_mp_static(&HTMLLabel::emit_video_failure).call_deferred(id, p_element, p_error, message);
}

void HTMLLabel::emit_video_failure(uint64_t p_id, const Ref<HTMLVideoElement>& p_element, Error p_error, const String& p_message) {
	if (auto* label = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(ObjectID(p_id))))
		label->emit_signal("video_failed", p_element, p_error, p_message);
}

void HTMLLabel::poll_videos(double p_delta) {
	bool repaint = false;
	bool layout_change = false;
	for (auto& entry : video_playbacks) {
		if (!entry.player) continue;
		const auto node = entry.element->get_video_node();
		if (!node) continue;
		auto* native = Object::cast_to<VideoStreamPlayer>(entry.player);
		const bool vlc = entry.backend == VIDEO_BACKEND_VLC;
		const Ref<Texture2D> texture = native ? native->get_video_texture() : Ref<Texture2D>(entry.player->call("get_texture"));
		const Vector2i size = texture.is_valid() ? Vector2i(texture->get_size()) : Vector2i();
		const bool valid_frame = size.x > 0 && size.y > 0 && (!vlc || size != Vector2i(1, 1));
		const bool failed = (vlc && vlc_state(*entry.player, "STATE_ERROR")) ||
			(entry.awaiting_frame && !valid_frame && (entry.preparing || !node->paused) && (entry.opening_time += p_delta) >= OPEN_TIMEOUT);
		if (failed) {
			const bool play = !entry.preparing;
			stop_video_player(entry);
			node->frame.unref();
			const Error error = open_next_video(entry, play);
			if (play && error != OK) notify_video_failure(entry.element, error);
			repaint = true;
			continue;
		}
		if (valid_frame) {
			if (!entry.loaded_data) {
				entry.loaded_data = true;
				dispatch_media_event(StringName("loadeddata"), entry.element);
				dispatch_media_event(StringName("canplay"), entry.element);
			}
			if (entry.started && !entry.playing_event_sent) {
				entry.playing_event_sent = true;
				dispatch_media_event(StringName("playing"), entry.element);
			}
			if (!entry.preparing && (entry.awaiting_frame || node->frame != texture)) {
				entry.awaiting_frame = false;
				node->frame = texture;
				repaint = true;
			}
			if (node->dimensions != size) {
				node->dimensions = size;
				layout_change = true;
			}
		}
		if (vlc && entry.started && !entry.awaiting_frame && vlc_state(*entry.player, "STATE_STOPPED")) {
			video_finished(uint64_t(entry.player->get_instance_id()));
			repaint = true;
			continue;
		}
		const double length = native ? native->get_stream_length() : double(entry.player->call("get_length")) / 1000.0;
		if (entry.preparing) {
			if (valid_frame) {
				if (std::isfinite(length) && length > 0) {
					if (node->duration != length) dispatch_media_event(StringName("durationchange"), entry.element);
					node->duration = length;
					if (!entry.loaded_metadata) {
						entry.loaded_metadata = true;
						dispatch_media_event(StringName("loadedmetadata"), entry.element);
					}
				}
				stop_video_player(entry);
				repaint = true;
			}
			continue;
		}
		const double time = native ? native->get_stream_position() : double(entry.player->call("get_time")) / 1000.0;
		const bool seekable = std::isfinite(length) && length > 0 && (native || bool(entry.player->call("is_seekable")));
		if (seekable != node->seekable || (std::isfinite(length) && length > 0 && node->duration != length)) {
			const bool duration_changed = std::isfinite(length) && length > 0 && node->duration != length;
			node->seekable = seekable;
			if (duration_changed) {
				node->duration = length;
				dispatch_media_event(StringName("durationchange"), entry.element);
				if (!entry.loaded_metadata) {
					entry.loaded_metadata = true;
					dispatch_media_event(StringName("loadedmetadata"), entry.element);
				}
			}
			repaint = true;
		}
		if (!node->paused && std::isfinite(time) && time >= 0 && std::abs(node->time - time) >= 0.1) {
			node->time = time;
			dispatch_media_event(StringName("timeupdate"), entry.element);
			repaint |= node->show_controls && (node->has_controls() || node->fullscreen);
		}
		const bool focused = node->fullscreen ? video_surface && video_surface->has_focus() && fullscreen_keyboard_focus :
			canvas && canvas->focused_element == entry.element && node->has_pseudo_class(litehtml::_id("focus-visible"));
		const bool hovered = node->fullscreen ? video_surface && video_surface->get_viewport()->gui_get_hovered_control() == video_surface &&
			node->part_at(Rect2(Vector2(), video_surface->get_size()), video_surface->get_local_mouse_position()) != HTMLVideoPart::none :
			canvas && get_viewport()->gui_get_hovered_control() == canvas && canvas->is_video_control_at(canvas->get_local_mouse_position());
		if (!focused && !hovered && !node->paused && !node->seek_preview && node->show_controls && (entry.controls_idle += p_delta) >= CONTROLS_TIMEOUT) {
			node->show_controls = false;
			repaint = true;
		}
	}
	if (layout_change) {
		layout->video_dimensions_changed();
		layout_updated = true;
		queue_refresh();
	}
	if (repaint) redraw_video();
	update_internal_processing();
}

void HTMLLabel::video_finished(uint64_t p_id) {
	for (auto& entry : video_playbacks) {
		if (!entry.player || uint64_t(entry.player->get_instance_id()) != p_id) continue;
		entry.started = false;
		if (const auto node = entry.element->get_video_node()) {
			node->time = node->duration;
			node->paused = true;
			node->show_controls = true;
			if (entry.element->is_looping()) play_video(entry.element);
			else dispatch_media_event(StringName("ended"), entry.element);
		}
		break;
	}
	redraw_video();
	update_internal_processing();
}

void HTMLLabel::redraw_video() {
	layout->repaint_input();
	if (canvas) canvas->queue_redraw();
	if (video_surface) video_surface->queue_redraw();
}

void HTMLLabel::wake_video_controls(const HTMLVideoElement& p_element) {
	const auto node = p_element.get_video_node();
	if (!node) return;
	if (auto* entry = find_video(p_element)) entry->controls_idle = 0;
	if (!std::exchange(node->show_controls, true)) redraw_video();
}

Error HTMLLabel::enter_video_fullscreen(const Ref<HTMLVideoElement>& p_element) {
	if (!can_play_video()) return ERR_UNAVAILABLE;
	if (!is_visible_in_tree() || !layout->is_element_connected(p_element) || !layout->get_element_scroll_bounds(p_element)) return ERR_UNCONFIGURED;
	if (fullscreen_video == p_element) return OK;
	auto* host = get_window();
	if (!host) return ERR_UNAVAILABLE;
	const uint64_t host_id = host->get_instance_id();
	const auto owner = fullscreen_owners.find(host_id);
	if (owner != fullscreen_owners.end() && ObjectDB::get_instance(owner->second)) return ERR_BUSY;

	const auto node = p_element->get_video_node();
	if (!node) return ERR_UNCONFIGURED;
	video_entry(p_element);
	fullscreen_owners[host_id] = ObjectID(get_instance_id());
	fullscreen_host = ObjectID(host_id);
	fullscreen_restore_focus = ObjectID();
	if (const auto* focused = get_viewport()->gui_get_focus_owner()) {
		fullscreen_restore_focus = ObjectID(focused->get_instance_id());
	}
	fullscreen_video = p_element;
	node->fullscreen = true;
	node->focus_part = HTMLVideoPart::play;
	node->show_controls = true;
	fullscreen_pressed_part = HTMLVideoPart::none;
	fullscreen_keyboard_focus = false;

	const auto* engine = Engine::get_singleton();
	const bool editor_preview = engine->is_editor_hint();
	const bool desktop = OS::get_singleton()->has_feature("pc");
	const bool native = desktop && !editor_preview && !engine->is_embedded_in_editor() && !host->is_embedded() &&
		host->get_mode() != Window::MODE_EXCLUSIVE_FULLSCREEN &&
		DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_SUBWINDOWS);
	video_surface = memnew(Control);
	video_surface->set_name("_HTMLVideoFullscreenSurface");
	video_surface->set_mouse_filter(MOUSE_FILTER_STOP);
	video_surface->set_focus_mode(FOCUS_ALL);
	video_surface->set_clip_contents(true);
	video_surface->connect("draw", callable_mp(this, &HTMLLabel::draw_video_fullscreen));
	video_surface->connect("gui_input", callable_mp(this, &HTMLLabel::on_video_fullscreen_input));

	if (native) {
		video_window = memnew(Window);
		video_window->set_name("_HTMLVideoFullscreenWindow");
		video_window->hide();
		video_window->set_force_native(true);
		video_window->set_transient(false);
		video_window->set_initial_position(Window::WINDOW_INITIAL_POSITION_CENTER_OTHER_SCREEN);
		video_window->set_current_screen(host->get_current_screen());
		video_window->set_mode(Window::MODE_FULLSCREEN);
		video_window->connect("close_requested", callable_mp(this, &HTMLLabel::leave_video_fullscreen));
		add_child(video_window, false, INTERNAL_MODE_BACK);
		video_window->add_child(video_surface, false, INTERNAL_MODE_BACK);
		video_surface->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
		video_window->show();
		// The desktop backend can apply the initial size as windowed on first show.
		if (video_window->get_mode() != Window::MODE_FULLSCREEN) video_window->set_mode(Window::MODE_FULLSCREEN);
	}
	else if (editor_preview) {
		add_child(video_surface, false, INTERNAL_MODE_BACK);
		video_surface->set_z_index(100);
		video_surface->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	}
	else {
		video_overlay_layer = memnew(CanvasLayer);
		video_overlay_layer->set_layer(1024);
		add_child(video_overlay_layer, false, INTERNAL_MODE_BACK);
		video_overlay_layer->add_child(video_surface, false, INTERNAL_MODE_BACK);
		video_surface->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	}
	video_surface->grab_focus();
	redraw_video();
	call_deferred("emit_signal", "video_fullscreen_changed", p_element, true);
	return OK;
}

void HTMLLabel::leave_video_fullscreen() {
	if (fullscreen_video.is_null()) return;
	const auto previous = fullscreen_video;
	fullscreen_video.unref();
	const uint64_t host_id = fullscreen_host;
	const uint64_t focus_id = fullscreen_restore_focus;
	fullscreen_owners.erase(host_id);
	fullscreen_host = ObjectID();
	fullscreen_restore_focus = ObjectID();
	fullscreen_pressed_part = HTMLVideoPart::none;
	if (const auto node = previous->get_video_node()) {
		node->fullscreen = false;
		node->seek_preview.reset();
		node->show_controls = true;
	}
	auto* window = std::exchange(video_window, nullptr);
	auto* layer = std::exchange(video_overlay_layer, nullptr);
	auto* surface = std::exchange(video_surface, nullptr);
	surface->hide();
	if (window) {
		window->hide();
		window->queue_free();
	}
	else if (layer) layer->queue_free();
	else surface->queue_free();
	redraw_video();
	if (is_inside_tree() && focus_id) {
		callable_mp(this, &HTMLLabel::restore_video_focus).call_deferred(host_id, focus_id);
	}
	// A signal handler can replace the document. Finish tearing down the session first.
	call_deferred("emit_signal", "video_fullscreen_changed", previous, false);
}

void HTMLLabel::restore_video_focus(uint64_t p_window, uint64_t p_control) {
	if (fullscreen_video.is_valid() || !is_visible_in_tree()) return;
	auto* window = Object::cast_to<Window>(ObjectDB::get_instance(ObjectID(p_window)));
	auto* control = Object::cast_to<Control>(ObjectDB::get_instance(ObjectID(p_control)));
	if (window && window->has_focus() && control && control->is_visible_in_tree() && control->get_window() == window) {
		control->grab_focus();
	}
}

void HTMLLabel::draw_video_fullscreen() {
	if (fullscreen_video.is_null() || !video_surface) return;
	const auto node = fullscreen_video->get_video_node();
	if (!node) return;
	const Rect2 box(Vector2(), video_surface->get_size());
	video_surface->draw_rect(box, Color(0, 0, 0));
	const auto frame = node->frame.is_valid() ? node->frame : layout->video_poster(*fullscreen_video.ptr());
	if (frame.is_valid() && box.has_area()) {
		const Vector2 natural = frame->get_size();
		if (natural.x > 0 && natural.y > 0) {
			const Vector2 extent = natural * std::min(box.size.x / natural.x, box.size.y / natural.y);
			video_surface->draw_texture_rect(frame, Rect2(box.get_center() - extent * 0.5f, extent), false);
		}
	}
	node->draw_controls(box, {
		[&](const Rect2& rect, const Color& color) { video_surface->draw_rect(rect, color); },
		[&](HTMLControlIcon icon, const Rect2& rect) {
			const auto texture = layout->video_control_icon(icon);
			if (texture.is_valid()) video_surface->draw_texture_rect(texture, rect, false);
		},
		[&](const Rect2& rect, const String& text) {
			const auto font = ThemeDB::get_singleton()->get_fallback_font();
			if (font.is_valid()) video_surface->draw_string(font,
				Vector2(rect.position.x, rect.get_center().y + (font->get_ascent(14) - font->get_descent(14)) * 0.5f),
				text, HORIZONTAL_ALIGNMENT_LEFT, rect.size.x, 14);
		},
	}, video_surface->has_focus() && fullscreen_keyboard_focus);
}

void HTMLLabel::activate_video_part(const Ref<HTMLVideoElement>& p_element, HTMLVideoPart p_part) {
	const auto node = p_element->get_video_node();
	if (!node) return;
	switch (p_part) {
		case HTMLVideoPart::play:
			if (node->paused) play_video(p_element);
			else pause_video(*p_element.ptr());
			break;
		case HTMLVideoPart::mute: p_element->set_muted(!node->is_muted()); break;
		case HTMLVideoPart::fullscreen:
			if (node->fullscreen) leave_video_fullscreen();
			else enter_video_fullscreen(p_element);
			break;
		default: break;
	}
}

void HTMLLabel::update_video_slider(const Ref<HTMLVideoElement>& p_element, HTMLVideoPart p_part,
	const Rect2& p_box, float p_x, bool p_commit) {
	const auto node = p_element->get_video_node();
	if (!node || !p_box.has_area()) return;
	const double fraction = std::clamp((p_x - p_box.position.x) / p_box.size.x, real_t(0), real_t(1));
	if (p_part == HTMLVideoPart::position) {
		const double target = node->duration * fraction;
		if (p_commit) {
			node->seek_preview.reset();
			seek_video(*p_element.ptr(), target);
		}
		else node->seek_preview = target;
	}
	else {
		node->volume = fraction;
		if (fraction > 0) node->muted = false;
		set_video_volume(*p_element.ptr());
	}
	redraw_video();
}

bool HTMLLabel::video_key(const Ref<HTMLVideoElement>& p_element, const InputEventKey& p_key, const Rect2&) {
	const auto node = p_element->get_video_node();
	if (!node || !(node->has_controls() || node->fullscreen) || !p_key.is_pressed() ||
		p_key.is_alt_pressed() || p_key.is_ctrl_pressed() || p_key.is_meta_pressed()) return false;
	const Key key = event_key(p_key);
	if (!p_key.is_echo() && (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_SPACE)) {
		activate_video_part(p_element, node->focus_part);
		return true;
	}
	if (!p_key.is_echo() && key == KEY_F) {
		activate_video_part(p_element, HTMLVideoPart::fullscreen);
		return true;
	}
	if ((key == KEY_LEFT || key == KEY_RIGHT) && slider(node->focus_part)) {
		const double direction = key == KEY_LEFT ? -1.0 : 1.0;
		if (node->focus_part == HTMLVideoPart::position) seek_video(*p_element.ptr(), std::max(0.0, node->time + direction * 5.0));
		else p_element->set_volume(node->volume + direction * 0.05);
		return true;
	}
	return false;
}

void HTMLLabel::on_video_fullscreen_input(const Ref<InputEvent>& p_event) {
	const Ref<HTMLVideoElement> element = fullscreen_video;
	if (element.is_null() || !video_surface) return;
	const auto node = element->get_video_node();
	if (!node) return;
	const Rect2 box(Vector2(), video_surface->get_size());
	const Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (!key->is_pressed()) return;
		if (event_key(*key.ptr()) == KEY_ESCAPE) {
			video_surface->accept_event();
			leave_video_fullscreen();
			return;
		}
		fullscreen_keyboard_focus = true;
		wake_video_controls(*element.ptr());
		if (event_key(*key.ptr()) == KEY_TAB) {
			const auto parts = focus_parts(*node, box);
			const auto focused = std::find(parts.begin(), parts.end(), node->focus_part);
			if (!parts.empty()) {
				const int64_t index = focused == parts.end() ? 0 : int64_t(focused - parts.begin());
				node->focus_part = parts[(index + (key->is_shift_pressed() ? parts.size() - 1 : 1)) % parts.size()];
				redraw_video();
			}
			video_surface->accept_event();
			return;
		}
		auto* surface = video_surface;
		if (video_key(element, *key.ptr(), box)) surface->accept_event();
		return;
	}
	const Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid()) {
		if (std::exchange(fullscreen_keyboard_focus, false)) redraw_video();
		wake_video_controls(*element.ptr());
		if (slider(fullscreen_pressed_part)) {
			update_video_slider(element, fullscreen_pressed_part, node->part_rect(box, fullscreen_pressed_part), motion->get_position().x, false);
		}
		video_surface->set_default_cursor_shape(node->part_at(box, motion->get_position()) != HTMLVideoPart::none
			? CURSOR_POINTING_HAND : CURSOR_ARROW);
		return;
	}
	const Ref<InputEventMouseButton> button = p_event;
	if (button.is_null() || button->get_button_index() != MOUSE_BUTTON_LEFT) return;
	video_surface->accept_event();
	const Vector2 point = button->get_position();
	wake_video_controls(*element.ptr());
	const auto hit = node->part_at(box, point);
	const auto part = hit == HTMLVideoPart::none && box.has_point(point) ? HTMLVideoPart::play : hit;
	if (button->is_pressed()) {
		fullscreen_pressed_part = part;
		fullscreen_press_position = point;
		if (slider(part)) update_video_slider(element, part, node->part_rect(box, part), point.x, false);
		return;
	}
	const auto pressed = std::exchange(fullscreen_pressed_part, HTMLVideoPart::none);
	if (button->is_canceled()) {
		node->seek_preview.reset();
		redraw_video();
		return;
	}
	if (slider(pressed)) update_video_slider(element, pressed, node->part_rect(box, pressed), point.x, true);
	else if (part == pressed && point.distance_squared_to(fullscreen_press_position) <= 16.0f) activate_video_part(element, pressed);
}

void HTMLLabelCanvas::cancel_video_press() {
	if (pressed_video.is_valid()) {
		if (const auto node = pressed_video->get_video_node()) node->seek_preview.reset();
		pressed_video.unref();
		label->redraw_video();
	}
	pressed_video_part = HTMLVideoPart::none;
}

bool HTMLLabelCanvas::handle_video_button(const InputEventMouseButton& p_button, const Vector2& p_point) {
	if (!label->video_present || p_button.get_button_index() != MOUSE_BUTTON_LEFT) return false;
	const Ref<HTMLVideoElement> hit = label->get_visible_rect().has_point(p_point) ? label->layout->get_element_at(p_point) : Ref<HTMLElement>();
	const Ref<HTMLVideoElement> element = p_button.is_pressed() ? hit : pressed_video;
	if (element.is_null()) return false;
	const auto node = element->get_video_node();
	const auto point = label->layout->input_local_point(*element.ptr(), p_point);
	if (!node || !point || !node->has_controls() || node->fullscreen) {
		const bool handled = pressed_video.is_valid();
		cancel_video_press();
		return handled;
	}
	label->wake_video_controls(*element.ptr());
	const auto control = node->part_at(node->content_box, *point);
	const auto part = control == HTMLVideoPart::none && node->content_box.has_point(*point) ? HTMLVideoPart::play : control;
	if (p_button.is_pressed()) {
		if (part == HTMLVideoPart::none) return false;
		label->grab_focus();
		set_focused_element(element);
		node->focus_part = part;
		pressed_video = element;
		video_press_position = p_point - label->get_visible_rect().position;
		pressed_video_part = part;
		if (slider(part)) label->update_video_slider(element, part, node->part_rect(node->content_box, part), point->x, false);
		return true;
	}
	const auto pressed = pressed_video_part;
	cancel_video_press();
	if (!p_button.is_canceled()) {
		if (slider(pressed)) label->update_video_slider(element, pressed, node->part_rect(node->content_box, pressed), point->x, true);
		else if (hit == element && part == pressed &&
			(p_point - label->get_visible_rect().position).distance_squared_to(video_press_position) <= 16.0f) {
			label->activate_video_part(element, pressed);
		}
	}
	return true;
}

bool HTMLLabelCanvas::move_video_control(const Vector2& p_point) {
	if (!label->video_present) return false;
	if (pressed_video.is_valid() && slider(pressed_video_part)) {
		const auto node = pressed_video->get_video_node();
		if (node) {
			if (const auto point = label->layout->input_local_point(*pressed_video.ptr(), p_point)) {
				label->update_video_slider(pressed_video, pressed_video_part, node->part_rect(node->content_box, pressed_video_part), point->x, false);
			}
		}
		return true;
	}
	if (label->get_visible_rect().has_point(p_point)) {
		const Ref<HTMLVideoElement> hit = label->layout->get_element_at(p_point);
		if (hit.is_valid()) label->wake_video_controls(*hit.ptr());
	}
	return false;
}

bool HTMLLabelCanvas::is_video_control_at(const Vector2& p_point) const {
	if (!label->video_present || !label->get_visible_rect().has_point(p_point)) return false;
	const Ref<HTMLVideoElement> video = label->layout->get_element_at(p_point);
	const auto node = video.is_valid() ? video->get_video_node() : nullptr;
	if (!node || node->fullscreen) return false;
	const auto local = label->layout->input_local_point(*video.ptr(), p_point);
	return local && node->part_at(node->content_box, *local) != HTMLVideoPart::none;
}

bool HTMLLabelCanvas::handle_video_key(const InputEventKey& p_key) {
	const Ref<HTMLVideoElement> element = focused_element;
	if (element.is_null()) return false;
	const auto node = element->get_video_node();
	if (!node || !label->video_key(element, p_key, node->content_box)) return false;
	label->accept_event();
	return true;
}

bool HTMLLabelCanvas::move_video_focus(bool p_backward) {
	const Ref<HTMLVideoElement> element = focused_element;
	const auto node = element.is_valid() ? element->get_video_node() : nullptr;
	if (!node) return false;
	label->wake_video_controls(*element.ptr());
	const auto parts = focus_parts(*node, node->content_box);
	const auto current = std::find(parts.begin(), parts.end(), node->focus_part);
	if (current == parts.end() || (p_backward ? current == parts.begin() : std::next(current) == parts.end())) return false;
	node->focus_part = p_backward ? *std::prev(current) : *std::next(current);
	label->redraw_video();
	return true;
}

void HTMLLabelCanvas::set_video_focus_edge(bool p_backward) {
	const Ref<HTMLVideoElement> element = focused_element;
	const auto node = element.is_valid() ? element->get_video_node() : nullptr;
	if (!node) return;
	label->wake_video_controls(*element.ptr());
	const auto parts = focus_parts(*node, node->content_box);
	if (!parts.empty()) node->focus_part = p_backward ? parts.back() : parts.front();
	label->redraw_video();
}
