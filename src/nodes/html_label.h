#pragma once

#include "formats/html/html_document.h"
#include "formats/html/elements/html_form_elements.h"
#include "formats/html/elements/html_input_time.h"
#include "formats/html/elements/html_audio_element.h"
#include "formats/html/elements/html_dialog_element.h"
#include "formats/html/elements/html_video_element.h"
#include "formats/html/text/html_text_selection.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/typed_dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <optional>
#include <array>
#include <map>
#include <utility>
#include <vector>

namespace godot {
	class InputEventKey;
	class InputEventMouseButton;
	class InputEventScreenTouch;
	class InputEventScreenDrag;
	class InputEvent;
	class Timer;
	class FileDialog;
	class TextureRect;
}

namespace dockit {
	class HTMLTextEditor;
}

class HTMLLabel;
class HTMLComponent;
class HTMLBehavior;
class HTMLBehaviorStorage;
class HTMLButtonElement;
class HTMLDocumentNode;
class HTMLFormElement;
class HTMLLabelElement;
class HTMLSelectElement;
class HTMLDataTransfer;
class HTMLDragEvent;

namespace godot { class CanvasLayer; class Window; }
namespace dockit { enum class HTMLVideoPart; }

class HTMLLabelCanvas : public godot::Control {
	GDCLASS(HTMLLabelCanvas, godot::Control)

	friend class ::HTMLLabel;
	enum class PointerKind { MOUSE, TOUCH, PEN };
	struct PointerInfo {
		godot::Vector2 position;
		int button = -1;
		int buttons = 0;
		bool pressed = false;
		bool canceled = false;
		bool shift = false;
		bool ctrl = false;
		bool alt = false;
		bool meta = false;
		int device = 0;
		int touch_index = -1;
		int pointer_id = 0;
		PointerKind kind = PointerKind::MOUSE;
		bool primary = true;
		bool accepts_event = true;
		bool mouse_down_prevented = false;
		int detail = 0;
		godot::Ref<godot::InputEvent> source_event;
		godot::Ref<HTMLElement> target;
	};
	struct TouchPointer {
		int device = 0;
		int index = 0;
		int pointer_id = 0;
		int detail = 0;
		godot::Vector2 start;
		godot::Vector2 last;
		godot::Ref<HTMLElement> target;
		bool dragged = false;
		bool canceled = false;
		bool primary = false;
		bool compatibility_suppressed = false;
	};
	using TouchKey = std::pair<int, int>;
	struct PendingMoveEvent {
		godot::StringName type;
		godot::Ref<HTMLElement> target;
		PointerInfo pointer;
		TouchPointer touch;
		bool is_touch = false;
	};

	::HTMLLabel* label = nullptr;
	godot::Vector2 pressed_position;
	bool pressed_blank = false;
	int mouse_buttons = 0;
	bool mouse_compatibility_suppressed = false;
	int click_count = 0;
	uint64_t last_click_msec = 0;
	uint64_t last_click_version = 0;
	godot::Vector2 last_click_position;
	godot::Ref<HTMLElement> last_click_target;
	std::map<TouchKey, TouchPointer> active_touch_pointers;
	std::map<TouchKey, int> touch_pointer_ids;
	int next_touch_pointer_id = 1;
	std::vector<PendingMoveEvent> pending_move_events;
	bool movement_flush_queued = false;
	struct OutsideDialogPress {
		godot::Ref<HTMLDialogElement> dialog;
		uint64_t open_sequence;
		godot::Vector2 position;
		int device;
		int touch;
	};
	std::optional<OutsideDialogPress> outside_dialog_press;
	godot::Ref<HTMLElement> pressed_element;
	godot::Ref<HTMLElement> pressed_button;
	godot::Ref<HTMLElement> pressed_form_target;
	godot::Ref<HTMLInputElement> pressed_input_decoration;
	int pressed_input_part = 0;
	godot::Ref<HTMLElement> dragging_input;
	struct BoxDrag {
		godot::Ref<HTMLElement> element;
		godot::Vector2i axes;
		dockit::HTMLPlaneTransform inverse;
		godot::Vector2 start_point;
		godot::Vector2 start_size;
		float scrollbar_grab = 0;
		bool vertical_scrollbar = false;
	};
	std::optional<BoxDrag> box_drag;
	std::optional<dockit::HTMLScrollbarDrag> scrollbar_drag;
	std::optional<godot::Vector2> resize_pending;
	bool resize_queued = false;
	bool dragged_resize = false;
	bool applying_box_resize = false;
	bool reconciling_interactions = false;
	bool reconcile_again = false;
	struct InteractionTarget {
		godot::Ref<HTMLElement> element;
		godot::ObjectID owner;
		uint64_t generation;
		uint64_t adoption;
		int64_t revision;
		godot::String input_type;
	};
	std::map<uint64_t, InteractionTarget> interaction_targets;
	godot::Ref<HTMLAudioElement> pressed_audio;
	dockit::HTMLAudioPart pressed_audio_part = dockit::HTMLAudioPart::none;
	godot::Ref<HTMLVideoElement> pressed_video;
	godot::Vector2 video_press_position;
	dockit::HTMLVideoPart pressed_video_part{};
	bool handle_video_button(const godot::InputEventMouseButton& p_button, const godot::Vector2& p_point);
	bool is_video_control_at(const godot::Vector2& p_point) const;
	bool move_video_control(const godot::Vector2& p_point);
	bool handle_video_key(const godot::InputEventKey& p_key);
	bool move_video_focus(bool p_backward);
	void set_video_focus_edge(bool p_backward);
	void cancel_video_press();
	bool handle_audio_button(const godot::InputEventMouseButton& p_button, const godot::Vector2& p_point);
	bool move_audio_control(const godot::Vector2& p_point);
	bool handle_audio_key(const godot::InputEventKey& p_key);
	bool move_audio_focus(bool p_backward);
	void set_audio_focus_edge(bool p_backward);
	godot::Ref<HTMLInputElement> pressed_checkbox;
	godot::Ref<HTMLInputElement> dragging_range;
	std::optional<int32_t> ime_window;
	std::optional<godot::Vector2i> ime_position;
	godot::Timer* caret_blink_timer = nullptr;
	bool caret_window_active = true;
	struct InputMenuRow {
		dockit::HTMLTextLayout caption;
		dockit::HTMLTextLayout shortcut;
		bool enabled = false;
	};
	godot::Control* input_menu = nullptr;
	std::array<InputMenuRow, 6> input_menu_rows;
	godot::Ref<godot::Font> input_menu_font;
	godot::Ref<godot::StyleBoxFlat> input_menu_panel_style;
	godot::Ref<godot::StyleBoxFlat> input_menu_hover_style;
	int32_t input_menu_font_size = 13;
	int input_menu_hover = -1;
	int input_menu_press = -1;
	godot::real_t input_menu_row_height = 28;
	godot::Ref<HTMLElement> input_menu_target;
	bool checkbox_key_pressed = false;
	godot::Ref<HTMLElement> focused_element;
	uint64_t focus_transition = 0;
	uint64_t focused_reset_version = 0;
	std::vector<godot::Ref<HTMLElement>> tab_elements;
	godot::Key activation_key = godot::KEY_NONE;
	std::optional<bool> tab_entry_backward;
	uintptr_t pressed_link = 0;
	int64_t selection_anchor = 0;
	int64_t selection_caret = 0;
	dockit::HTMLSelectionPoint selection_origin;
	std::array<godot::Ref<HTMLRange>, 3> selection_origin_ranges;
	bool selecting = false;
	bool selection_dragged = false;
	bool selection_drag_attempt = false;
	bool element_drag_attempt = false;
	bool native_drag_started = false;
	bool suppress_next_release = false;
	godot::Ref<HTMLElement> drag_source;
	godot::String drag_text;
	double drag_elapsed = 0.0;
	mutable godot::Ref<HTMLElement> drop_target;
	mutable godot::String drop_text;
	mutable bool drop_accepted = false;
	bool has_focusable_elements = false;
	::HTMLLabel* diagnostic_panel = nullptr;
	godot::Timer* diagnostic_timer = nullptr;
	godot::String diagnostic_panel_text;
	godot::Vector2 diagnostic_location;

	void cancel_press();
	void reconcile_interactions_after_edit(bool p_after_layout);
	void remember_interaction_targets();
	void capture_selection_origin();
	void restore_selection_origin();
	int click_detail_for(const godot::Ref<HTMLElement>& p_target, const godot::Vector2& p_position) const;
	void record_click(const godot::Ref<HTMLElement>& p_target, const godot::Vector2& p_position, int p_detail);
	void reset_click_sequence();
	bool handle_dialog_pointer(const godot::Vector2& p_point, bool p_pressed, bool p_cancelled, int p_device, int p_touch = -1);
	void update_dialog_pointer(const godot::Vector2& p_point, int p_device, int p_touch = -1);
	void extend_document_selection(const godot::Vector2& p_point);
	void cancel_click();
	void cancel_button_press();
	void cancel_key_press();
	bool handle_dom_key(const godot::InputEventKey& p_key);
	bool handle_key_default_action(const godot::InputEventKey& p_key);
	void handle_selection_key(const godot::InputEventKey& p_key);
	void accept_pointer_event(const PointerInfo& p_info);
	void handle_pointer_press(const PointerInfo& p_info);
	void handle_pointer_release(const PointerInfo& p_info);
	void handle_pointer_move(const PointerInfo& p_info);
	void queue_pointer_move_event(const godot::StringName& p_type, const godot::Ref<HTMLElement>& p_target,
		const PointerInfo& p_info);
	void queue_touch_move_event(const godot::StringName& p_type, const TouchPointer& p_touch);
	void flush_move_events();
	void handle_touch_press(const godot::InputEventScreenTouch& p_touch);
	void handle_touch_drag(const godot::InputEventScreenDrag& p_drag);
	void handle_touch_release(const godot::InputEventScreenTouch& p_touch);
	PointerInfo make_touch_pointer_info(const TouchPointer& p_touch, const godot::Vector2& p_position,
		bool p_pressed, bool p_canceled) const;
	bool dispatch_touch_event(const godot::StringName& p_type, const TouchPointer& p_changed);
	godot::Ref<godot::InputEventMouseButton> make_touch_mouse_event(const PointerInfo& p_info) const;
	void cancel_touch_pointer(int p_device, int p_index);
	void cancel_all_touch_pointers();
	bool handle_wheel_input(const PointerInfo& p_info, float p_pixels_per_step);
	bool dispatch_pointer_event(
		const godot::StringName& p_type,
		const godot::Ref<HTMLElement>& p_target,
		const godot::InputEventMouseButton& p_event,
		const godot::Vector2& p_point,
		bool p_cancelable
	);
	bool dispatch_pointer_event(
		const godot::StringName& p_type,
		const godot::Ref<HTMLElement>& p_target,
		const PointerInfo& p_info,
		bool p_cancelable,
		const godot::Ref<HTMLElement>& p_related_target = {}
	);
	bool dispatch_hover_events(const godot::TypedArray<HTMLElement>& p_previous, const godot::Vector2& p_position);
	bool dispatch_key_event(const godot::StringName& p_type, const godot::InputEventKey& p_key);
	void dispatch_blur_events(const godot::Ref<HTMLElement>& p_previous, const godot::Ref<HTMLElement>& p_next);
	void dispatch_focus_events(const godot::Ref<HTMLElement>& p_previous, const godot::Ref<HTMLElement>& p_next);
	void toggle_checkbox(const godot::Ref<HTMLInputElement>& p_input);
	bool move_radio(const godot::Ref<HTMLInputElement>& p_input, int p_direction);
	godot::Ref<HTMLElement> get_form_click_target(const godot::Ref<HTMLElement>& p_element) const;
	void move_range(const godot::Ref<HTMLInputElement>& p_input, const godot::Vector2& p_point, bool p_commit);
	bool handle_input_key(const godot::InputEventKey& p_key);
	bool handle_editable_key(const godot::InputEventKey& p_key);
	bool handle_input_decoration(const godot::Ref<HTMLInputElement>& p_input, const godot::InputEventMouseButton& p_event,
		const godot::Vector2& p_point, bool p_accepts_event = true);
	bool open_input_menu(const godot::Ref<HTMLElement>& p_element, const godot::Vector2& p_position, bool p_pointer);
	void close_input_menu();
	void on_input_menu_selected(int p_command);
	void draw_input_menu();
	void input_menu_event(const godot::Ref<godot::InputEvent>& p_event);
	bool handle_input_menu_key(const godot::InputEventKey& p_key);
	godot::Rect2 input_menu_row_rect(int p_command) const;
	void run_input_command(const godot::Ref<HTMLElement>& p_element, int p_command);
	void change_input(const godot::Ref<HTMLElement>& p_input, bool p_update_suggestions = true,
		const godot::StringName& p_input_type = {}, const godot::String& p_data = {}, bool p_is_composing = false);
	void commit_input(const godot::Ref<HTMLElement>& p_input, bool p_deferred = false);
	void set_input_caret(const godot::Ref<HTMLElement>& p_input, const godot::Vector2& p_point, bool p_extend);
	bool begin_resize_drag(const godot::Vector2& p_point);
	bool begin_textarea_scrollbar_drag(const godot::Ref<HTMLElement>& p_element, const godot::Vector2& p_point);
	void move_box_drag(const godot::Vector2& p_point);
	void flush_box_resize();
	void update_input_autoscroll();
	std::optional<CursorShape> box_resize_cursor(const godot::Vector2& p_point) const;
	void set_input_caret_visible(dockit::HTMLTextEditor& p_editor, bool p_visible);
	void restart_caret_blink();
	void stop_caret_blink();
	void blink_caret();
	void update_ime();
	void close_ime();
	void on_ime_update();
	void set_focused_element(const godot::Ref<HTMLElement>& p_button, bool p_keyboard = false, bool p_reveal = true);
	void set_element_pseudo(const godot::Ref<HTMLElement>& p_button, const char* p_name, bool p_enabled);
	void clear_tab_entry();
	void focus_tab_endpoint(bool p_backward);
	void update_focus_mode();
	void update_cursor(const std::optional<godot::Vector2>& p_point);
	void refresh_diagnostic_tooltip(const std::optional<godot::Vector2>& p_point);
	void show_diagnostic_tooltip();
	bool dispatch_drag_event(const godot::StringName& p_type, const godot::Ref<HTMLElement>& p_target,
		const godot::Ref<HTMLDataTransfer>& p_transfer, const godot::Vector2& p_position,
		const godot::Ref<HTMLElement>& p_related_target = {});
	void update_drop_target(const godot::Ref<HTMLElement>& p_target, const godot::String& p_text,
		const godot::Vector2& p_position) const;
	void clear_drop_target(const godot::Vector2& p_position) const;
	godot::Variant begin_selection_drag(const godot::Vector2& p_position, bool p_force_native_drag);
	godot::Variant begin_element_drag(const godot::Vector2& p_position, bool p_force_native_drag);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	void _draw() override;
	bool _has_point(const godot::Vector2& p_point) const override;
	void _gui_input(const godot::Ref<godot::InputEvent>& p_event) override;
	void _process(double p_delta) override;
	godot::Variant _get_drag_data(const godot::Vector2& p_at_position) override;
	bool _can_drop_data(const godot::Vector2& p_at_position, const godot::Variant& p_data) const override;
	void _drop_data(const godot::Vector2& p_at_position, const godot::Variant& p_data) override;
};

class HTMLLabel : public godot::Control {
	GDCLASS(HTMLLabel, godot::Control)
	friend class HTMLBehavior;

public:
	enum VideoBackend {
		VIDEO_BACKEND_AUTO,
		VIDEO_BACKEND_FFMPEG,
		VIDEO_BACKEND_VLC,
	};

	enum AnimationProcessMode {
		ANIMATION_PROCESS_IDLE,
		ANIMATION_PROCESS_PHYSICS,
		ANIMATION_PROCESS_MANUAL,
	};

private:
	friend class ::HTMLLabelCanvas;
	friend class HTMLAudioElement;
	friend class ::HTMLDialogElement;
	friend class HTMLVideoElement;

	godot::Ref<HTMLDocument> document;
	godot::Ref<HTMLLayout> layout;
	struct AudioPlayback {
		godot::Ref<HTMLAudioElement> element;
		godot::PackedStringArray sources;
		godot::Node* player = nullptr;
		bool started = false;
		bool autoplay_attempted = false;
		bool muted = false;
		bool load_started = false;
		bool loaded = false;
	};
	std::vector<AudioPlayback> audio_playbacks;
	struct VideoPlayback {
		godot::Ref<HTMLVideoElement> element;
		godot::PackedStringArray paths;
		godot::Node* player = nullptr;
		VideoBackend backend = VIDEO_BACKEND_AUTO;
		int64_t next_candidate = 0;
		double opening_time = 0;
		double controls_idle = 0;
		bool awaiting_frame = false;
		bool preparing = false;
		bool preparation_attempted = false;
		bool started = false;
		bool autoplay_attempted = false;
		bool applied_muted = false;
		bool load_started = false;
		bool loaded_metadata = false;
		bool loaded_data = false;
		bool playing_event_sent = false;
	};
	std::vector<VideoPlayback> video_playbacks;
	VideoBackend video_backend = VIDEO_BACKEND_AUTO;
	godot::StringName video_bus = "Master";
	bool video_present = false;
	bool video_sync_queued = false;
	godot::Ref<HTMLVideoElement> fullscreen_video;
	godot::ObjectID fullscreen_host;
	godot::ObjectID fullscreen_restore_focus;
	godot::Window* video_window = nullptr;
	godot::CanvasLayer* video_overlay_layer = nullptr;
	godot::Control* video_surface = nullptr;
	dockit::HTMLVideoPart fullscreen_pressed_part{};
	bool fullscreen_keyboard_focus = false;
	godot::Vector2 fullscreen_press_position;
	VideoPlayback* find_video(const HTMLVideoElement& p_element);
	VideoPlayback& video_entry(const godot::Ref<HTMLVideoElement>& p_element);
	bool can_play_video() const;
	godot::Error play_video(const godot::Ref<HTMLVideoElement>& p_element);
	godot::Error open_next_video(VideoPlayback& r_playback, bool p_play);
	void pause_video(const HTMLVideoElement& p_element);
	godot::Error seek_video(const HTMLVideoElement& p_element, double p_seconds);
	void set_video_volume(const HTMLVideoElement& p_element);
	void apply_video_audio(VideoPlayback& r_playback);
	void stop_video_player(VideoPlayback& r_playback);
	void destroy_video(VideoPlayback& r_playback);
	void clear_videos();
	void queue_video_sync();
	void sync_videos();
	void poll_videos(double p_delta);
	bool has_active_video() const;
	void video_finished(uint64_t p_id);
	void notify_video_failure(const godot::Ref<HTMLVideoElement>& p_element, godot::Error p_error);
	static void emit_video_failure(uint64_t p_id, const godot::Ref<HTMLVideoElement>& p_element,
		godot::Error p_error, const godot::String& p_message);
	void redraw_video();
	godot::Error enter_video_fullscreen(const godot::Ref<HTMLVideoElement>& p_element);
	void leave_video_fullscreen();
	void restore_video_focus(uint64_t p_window, uint64_t p_control);
	void draw_video_fullscreen();
	void on_video_fullscreen_input(const godot::Ref<godot::InputEvent>& p_event);
	void wake_video_controls(const HTMLVideoElement& p_element);
	void activate_video_part(const godot::Ref<HTMLVideoElement>& p_element, dockit::HTMLVideoPart p_part);
	void update_video_slider(const godot::Ref<HTMLVideoElement>& p_element, dockit::HTMLVideoPart p_part,
		const godot::Rect2& p_box, float p_x, bool p_commit);
	bool video_key(const godot::Ref<HTMLVideoElement>& p_element, const godot::InputEventKey& p_key, const godot::Rect2& p_box);

	godot::StringName bus = "Master";
	bool use_fmod_bus = false;
	bool audio_sync_queued = false;
	AudioPlayback* find_audio(const HTMLAudioElement& p_element);
	AudioPlayback& audio_entry(const godot::Ref<HTMLAudioElement>& p_element);
	godot::Error prepare_audio(AudioPlayback& p_audio);
	godot::Error play_audio(const godot::Ref<HTMLAudioElement>& p_element);
	void pause_audio(const HTMLAudioElement& p_element);
	void seek_audio(const godot::Ref<HTMLAudioElement>& p_element, double p_time);
	void set_audio_volume(const HTMLAudioElement& p_element);
	void set_audio_player_volume(AudioPlayback& p_audio);
	void destroy_audio(AudioPlayback& p_audio);
	void clear_audio();
	void queue_audio_sync();
	void sync_audio();
	void poll_audio();
	bool has_playing_audio() const;
	void audio_finished(uint64_t p_player);
	bool uses_fmod_audio() const;
	void validate_audio_property(godot::PropertyInfo& p_property) const;
	void connect_audio_buses(bool p_connect);
	godot::PackedStringArray audio_bus_names() const;
	godot::StringName effective_audio_bus() const;
	void audio_buses_changed();
	void audio_bus_renamed(int64_t p_index, const godot::StringName& p_old, const godot::StringName& p_new);
	godot::Control* viewport_control = nullptr;
	godot::Control* viewport_bars = nullptr;
	godot::Control* scroll_content = nullptr;
	::HTMLLabelCanvas* canvas = nullptr;
	godot::Timer* color_scheme_timer = nullptr;
	godot::Control* select_menu = nullptr;
	godot::Ref<HTMLElement> open_select;
	std::vector<dockit::HTMLSelectMenuRow> select_rows;
	int select_highlight = -1;
	int select_pressed = -1;
	int select_scroll = 0;
	int select_visible_rows = 0;
	float select_row_height = 0.0f;
	godot::Ref<HTMLInputElement> picker_input;
	godot::FileDialog* file_dialog = nullptr;
	godot::Control* color_picker_canvas = nullptr;
	godot::TextureRect* color_eyedropper_canvas = nullptr;
	struct ColorEyedropper {
		godot::Ref<godot::Image> frame;
		godot::Rect2 viewport;
		godot::DisplayServer::MouseMode previous_mode;
		godot::DisplayServer::MouseMode hidden_mode;
		bool linear;
		godot::Vector2i pixel{-1, -1};
		bool pressed = false;

		godot::Color sample(const godot::Vector2i& p_pixel) const;
	};
	std::optional<ColorEyedropper> color_eyedropper;
	struct ColorPickerState {
		enum Part { well, hue, opacity, eyedropper, format, channel, preset = channel + 3 };
		enum Format { rgb, hsl, hex };
		std::vector<godot::String> presets;
		godot::String value;
		godot::String editing;
		float h = 0.0f;
		float s = 0.0f;
		float v = 0.0f;
		float a = 1.0f;
		bool alpha = false;
		bool replace_text = false;
		int active = -1;
		int hovered = -1;
		int dragging = -1;
		int format_index = rgb;

		godot::Color color() const;
		godot::Vector2 size() const;
		godot::Rect2 part_rect(int p_part) const;
		godot::String field_text(int p_channel) const;
	};
	ColorPickerState color_picker;
	struct TimePickerState {
		enum Unit { hour, minute, second, millisecond, meridiem };
		struct Column {
			Unit unit;
			std::vector<int> values;
			int selected = 0;
			int first = 0;

			bool loops() const { return values.size() >= 10; }
			int row_index(int p_row) const;
			void scroll(int p_rows);
			void reveal();
		};
		std::vector<Column> columns;
		godot::Dictionary initial;
		godot::String initial_value;
		godot::Vector2i calendar_month;
		godot::Vector2i hovered_cell{-1, -1};
		godot::Vector2i pressed_cell{-1, -1};
		godot::Vector2 press_position;
		dockit::input_time::Kind kind = dockit::input_time::Kind::none;
		bool use_meridiem = false;
		bool sunday_first = false;
		bool year_view = false;
		bool changed = false;
		bool dragged_column = false;
		int active_column = 0;
		int calendar_hover = -1;
		int pressed_calendar = -1;

		bool has_calendar() const { return kind != dockit::input_time::Kind::time; }
		bool has_clock() const { return !has_calendar() || kind == dockit::input_time::Kind::local; }
		int calendar_width() const;
		int calendar_height() const;
	};
	TimePickerState time_picker;
	godot::Control* time_picker_canvas = nullptr;
	godot::Dictionary picker_date;
	uint64_t picker_sequence = 0;
	uint64_t picker_adoption = 0;
	uint64_t picker_reset = 0;
	godot::String picker_type;
	godot::String text;
	godot::String base_dir = "res://";
	godot::String source_path;
	godot::String stylesheet;
	godot::Vector2i viewport_size;
	godot::Vector2i content_size;
	godot::Vector2i viewport_input_axes{1, 1};
	bool wheel_dispatched_in_canvas = false;
	uint64_t wheel_dispatch_sequence = 0;
	std::optional<godot::Vector2> input_mouse_view_position;
	bool input_mouse_position_known = false;
	std::optional<godot::Vector2i> last_window_resize_event_size;
	std::optional<godot::Vector2i> pending_window_resize_event_size;
	std::optional<bool> document_visibility_state;
	std::optional<bool> window_focus_state;
	bool window_resize_event_queued = false;
	godot::Vector2i scroll_offset;
	godot::String render_error;
	std::optional<godot::Vector2i> pending_scroll_offset;
	bool pending_scroll_requested = false;
	godot::Ref<godot::Tween> viewport_scroll_tween;
	bool applying_scroll_tween = false;
	godot::Vector2i dragging_viewport_scrollbar;
	godot::real_t viewport_scrollbar_grab = 0;
	int hovered_viewport_bar = -1;
	int viewport_touch_index = -1;
	godot::Vector2 viewport_touch_motion;
	godot::Vector2 viewport_touch_velocity;
	bool viewport_touch_dragged = false;
	godot::Vector2i visible_viewport_bars;
	godot::Ref<godot::Tween> viewport_pan_snap_timer;
	godot::Vector2i viewport_pan_axes;
	bool source_dirty = true;
	bool behaviors_enabled = false;
	bool behavior_host_in_tree = false;
	bool behavior_mount_scheduled = false;
	bool dispatching_behavior = false;
	bool behavior_source_pending = false;
	uint64_t behavior_version = 0;
	bool behavior_reload_pending = false;
	godot::TypedDictionary<godot::StringName, godot::GDScript> behavior_scripts;
	std::vector<godot::Dictionary> behavior_diagnostics;
	bool behavior_source_error_reported = false;
	std::vector<godot::Ref<HTMLBehavior>> attached_behaviors;
	godot::TypedDictionary<godot::StringName, godot::GDScript> component_scripts;
	std::vector<godot::Ref<HTMLBehavior>> attached_components;
	godot::TypedArray<godot::Dictionary> component_diagnostics;
	bool component_sync_queued = false;
	uint64_t component_version = 0;
	void queue_component_sync();
	static void synchronize_components(uint64_t p_id, uint64_t p_version);
	void detach_components();
	godot::Ref<HTMLBehaviorStorage> session_storage;
	godot::String storage_namespace;
	struct NavigationEntry {
		godot::String location;
		godot::String text;
		godot::Ref<HTMLDocument> document;
		godot::String base_dir;
		godot::String source_path;
	};
	std::vector<NavigationEntry> navigation_history;
	int64_t navigation_position = -1;
	uint64_t navigation_sequence = 0;
	uint64_t navigation_request_id = 0;
	godot::String current_location;
	godot::String pending_navigation_anchor;
	bool parsed = false;
	bool document_ready_events_queued = false;
	bool document_ready_events_sent = false;
	bool refresh_queued = false;
	bool resource_refresh_only = false;
	bool layout_updated = false;
	bool use_node_selection = false;
	bool capturing_node_selection = false;
	bool layout_signal_pending = false;
	bool selection_enabled = true;
	bool drag_and_drop_selection_enabled = true;
	bool transparent_canvas = false;
	bool context_menu_enabled = true;
	bool event_debug = false;
	AnimationProcessMode animation_process_mode = ANIMATION_PROCESS_IDLE;
	bool caret_blink = true;
	double caret_blink_interval = 0.65;
	bool view_update_pending = false;
	bool interaction_queued = false;
	bool gui_hover_dirty = false;
	bool updating_gui_hover = false;
	bool hover_reflow = false;
	bool interaction_styles_dirty = false;
	bool reveal_focused_element = false;
	int hover_passes_remaining = 0;
	dockit::HTMLScrollbarAppearance viewport_scrollbar_style;
	litehtml::color_scheme page_color_scheme = litehtml::color_scheme::light;
	godot::Vector2 viewport_painted_gutters;

	void ensure_controls();
	void poll_preferred_color_scheme();
	void queue_refresh(bool p_resource_only = false);
	void flush_queued_refresh();
	void queue_interaction_style_refresh();
	void on_auto_content_update();
	void on_resource_layout_update();
	void on_transition_frame(bool p_size_changed);
	void update_internal_processing();
	godot::Vector2i get_content_selection_range() const;
	void capture_node_selection();
	void on_node_selection_changed();
	std::optional<godot::Vector2> get_hit_document_position(const godot::Vector2& p_position) const;
	void source_changed();
	void reset_document_source();
	void request_behavior_reset(bool p_source_changed = false);
	void apply_behavior_reset();
	bool can_mount_behaviors() const;
	void queue_behavior_mount();
	void update_behavior_processing();
	void add_behavior_diagnostic(const godot::String& p_src, const godot::String& p_path,
		const godot::String& p_reason);
	void on_navigation_response(uint64_t p_sequence, const godot::String& p_url, bool p_replace,
		int p_status_code, const godot::PackedByteArray& p_body, const godot::String& p_error);
	void apply_navigation_source(const godot::String& p_url, const godot::String& p_text, bool p_replace);
	void navigate_to_history(int64_t p_position);
	void queue_document_ready_events();
	void dispatch_document_ready_events();
	void queue_window_resize_event(const godot::Vector2i& p_viewport_size);
	void dispatch_window_resize_event();
	void update_document_visibility_state(bool p_visible, bool p_dispatch);
	void dispatch_window_focus_event(bool p_focused);
	static void mount_behaviors(uint64_t p_id, uint64_t p_version);
	struct DialogFocus {
		godot::Ref<HTMLDialogElement> dialog;
		godot::Ref<HTMLElement> previous;
		godot::Ref<HTMLDialogElement> previous_modal;
		godot::ObjectID previous_control;
		bool modal = false;
		bool keyboard = false;
		bool pending = true;
	};
	std::vector<DialogFocus> dialog_focus;
	std::vector<std::pair<godot::Ref<HTMLDialogElement>, godot::String>> closed_dialogs;
	godot::Ref<HTMLDialogElement> focused_modal;
	bool dialog_escape_held = false;
	std::vector<godot::RID> accessibility_tree_elements;
	void update_accessibility_tree();
	void clear_accessibility_tree();
	void perform_accessibility_action(uint64_t p_element_id, int32_t p_action);
	void remember_dialog_focus(const godot::Ref<HTMLDialogElement>& p_dialog, bool p_modal);
	void on_dialog_opened();
	void sync_dialog_focus(bool p_has_viewport);
	bool handle_dialog_escape(const godot::InputEventKey& p_key);
	void request_dialog_from_pointer(const godot::Ref<HTMLDialogElement>& p_dialog, uint64_t p_sequence);
	void queue_dialog_closed(const godot::Ref<HTMLDialogElement>& p_dialog, const godot::String& p_value);
	void emit_dialog_closures();
	void document_changed(uint64_t p_document_id);
	void parse_pending_source();
	void on_layout_edited();
	void on_dom_mutated();
	void refresh();
	void draw_viewport_gutters();
	void draw_viewport_scrollbars();
	dockit::HTMLScrollbar viewport_scrollbar(int p_axis) const;
	void set_viewport_bar_hover(int p_axis);
	void place_overlay(godot::Control& p_overlay, const godot::Vector2& p_point);
	bool handle_viewport_touch(const godot::Ref<godot::InputEvent>& p_event);
	void update_view(const godot::Vector2i& p_viewport, const godot::Vector2i& p_content,
		const godot::Vector2i& p_visible_bars, const godot::Vector2i& p_input_axes, bool p_layout_changed,
		const godot::String& p_error = {}, const godot::Vector2& p_painted_gutters = {});
	godot::Rect2 get_visible_rect() const;
	void apply_scroll_target(const dockit::HTMLScrollTarget& p_target, std::optional<float> p_align,
		const dockit::HTMLScrollOptions* p_options = nullptr);
	void advance_viewport_scroll(double p_fraction, godot::Vector2 p_from, godot::Vector2 p_to);
	void cancel_viewport_scroll();
	void on_nested_scroll_advanced();
	void apply_scroll_offset(const godot::Vector2i& p_offset);
	void apply_pending_scroll();
	void on_viewport_gui_input(const godot::Ref<godot::InputEvent>& p_event);
	void on_viewport_mouse_exited();
	void clear_canvas_wheel_dispatch(uint64_t p_sequence);
	void drag_viewport_bar(int p_axis, godot::real_t p_position);
	void settle_viewport_snap(const godot::Vector2i& p_axes);
	void settle_viewport_pan();
	void cancel_viewport_pan_snap();
	void sync_viewport_scroll();
	void request_hover_check();
	void queue_interaction();
	void update_interaction();
	void activate_link(const godot::String& p_uri);
	bool activate_form_control(const godot::Ref<HTMLElement>& p_element, const godot::Vector2i& p_image_coordinates = {});
	godot::Error submit_form_at(const godot::Ref<HTMLFormElement>& p_form, const godot::Ref<HTMLElement>& p_submitter,
		const godot::Vector2i& p_image_coordinates);
	bool dispatch_form_event(const godot::StringName& p_type, const godot::Ref<HTMLFormElement>& p_form,
		const godot::Ref<HTMLElement>& p_submitter);
	bool dispatch_control_event(const godot::StringName& p_type, const godot::Ref<HTMLElement>& p_element);
	bool dispatch_input_event(const godot::StringName& p_type, const godot::Ref<HTMLElement>& p_element,
		const godot::StringName& p_input_type, const godot::String& p_data, bool p_is_composing = false);
	void dispatch_composition_event(const godot::StringName& p_type, const godot::Ref<HTMLElement>& p_element,
		const godot::String& p_data);
	void dispatch_media_event(const godot::StringName& p_type, const godot::Ref<HTMLElement>& p_element);
	void dispatch_media_error_event(const godot::Ref<HTMLElement>& p_element, const godot::String& p_message,
		godot::Error p_error);
	void dispatch_scroll_event(const godot::Ref<HTMLElement>& p_element);
	friend class HTMLInputElement;
	godot::Error open_input_picker(const godot::Ref<HTMLInputElement>& p_element);
	godot::Error open_file_picker(uint64_t p_sequence);
	bool input_picker_valid() const;
	void close_input_picker();
	void on_input_picker_closed(uint64_t p_sequence);
	bool is_time_picker_open() const;
	bool is_canvas_picker_open() const;
	bool place_input_picker_canvas(godot::Control& p_surface, const godot::Vector2& p_size);
	void files_selected(const godot::PackedStringArray& p_paths, uint64_t p_sequence);
	void file_selected(const godot::String& p_path, uint64_t p_sequence);
	void native_files_selected(bool p_accepted, const godot::PackedStringArray& p_paths, int p_filter, uint64_t p_sequence);
	godot::Error show_color_picker();
	void update_color_picker();
	void draw_color_picker();
	void start_color_eyedropper(const godot::Vector2& p_position);
	void stop_color_eyedropper();
	void move_color_eyedropper(const godot::Vector2& p_position);
	bool handle_color_eyedropper_input(const godot::Ref<godot::InputEvent>& p_event);
	bool handle_color_picker_input(const godot::Ref<godot::InputEvent>& p_event);
	void finish_color_picker(bool p_commit);
	void select_color_picker_part(int p_part);
	void update_color_picker_point(int p_part, const godot::Vector2& p_point);
	void update_color_picker_value(const godot::String& p_value);
	void update_color_picker_field();
	void update_color_picker_preview();
	godot::Error show_clock_picker();
	void update_clock_picker();
	void draw_clock_picker();
	bool handle_clock_picker_input(const godot::Ref<godot::InputEvent>& p_event);
	void configure_clock_columns();
	void preview_clock_value();
	void shift_clock_calendar(int p_months);
	bool set_clock_calendar_date(double p_date);
	int clock_calendar_hit(const godot::Vector2& p_point) const;
	void activate_clock_calendar(int p_item);
	void commit_time_picker();
	void commit_picker_value(const godot::String& p_value);
	void submit_from_text(const godot::Ref<HTMLElement>& p_element);
	bool open_suggestion_menu(const godot::Ref<HTMLElement>& p_element);
	bool show_select_menu(const godot::Ref<HTMLElement>& p_element, std::vector<dockit::HTMLSelectMenuRow> p_rows);
	bool handle_listbox_mouse(const godot::Ref<HTMLElement>& p_element, const godot::InputEventMouseButton& p_event, const godot::Vector2& p_point);
	bool open_select_menu(const godot::Ref<HTMLSelectElement>& p_element);
	void close_select_menu();
	void update_select_menu();
	void refresh_select_menu();
	void draw_select_menu();
	void select_menu_input(const godot::Ref<godot::InputEvent>& p_event);
	bool handle_select_key(const godot::InputEventKey& p_key);
	void highlight_select_row(int p_row);
	void commit_select_row(int p_row);

protected:
	static void _bind_methods();
	void _validate_property(godot::PropertyInfo& p_property) const;
	void _notification(int p_what);

public:
	HTMLLabel();
	~HTMLLabel() override;
	bool invoke_inline_event_handler(const godot::Ref<HTMLElement>& p_element, const godot::Ref<HTMLEvent>& p_event);
	void set_bus(const godot::StringName& p_bus);								// 设置音频输出总线
	godot::StringName get_bus() const;											// 返回配置的音频总线
	void set_video_backend(VideoBackend p_backend);								// 选择视频解码后端
	VideoBackend get_video_backend() const;										// 返回视频解码后端设置
	void set_video_bus(const godot::StringName& p_bus);							// 设置视频使用的 Godot 音频总线
	godot::StringName get_video_bus() const;									// 返回视频使用的音频总线
	godot::Ref<HTMLVideoElement> get_fullscreen_video() const;					// 返回当前全屏视频的元素句柄
	void set_use_fmod_bus(bool p_enable);										// 选择可用的 FMOD 播放后端
	bool is_using_fmod_bus() const;												// 返回 FMOD 后端的配置值
	void _draw() override;
	void _gui_input(const godot::Ref<godot::InputEvent>& p_event) override;
	void _input(const godot::Ref<godot::InputEvent>& p_event) override;
	void set_transparent_canvas(bool p_enabled);
	bool is_transparent_canvas() const;
	void set_event_debug(bool p_enabled);
	bool is_event_debug_enabled() const { return event_debug; }
	godot::String get_document_visibility_state() const;
	int64_t get_event_listener_count(const godot::StringName& p_type) const;
	void set_css_diagnostics_enabled(bool p_enabled);							// 设置是否收集 CSS 声明诊断
	bool is_css_diagnostics_enabled() const;									// 返回是否收集 CSS 声明诊断
	godot::TypedArray<godot::Dictionary> get_css_diagnostics() const;			// 返回 CSS 声明诊断的快照

	godot::PackedStringArray _get_configuration_warnings() const override;

	void set_document(const godot::Ref<HTMLDocument>& p_document);				// 设置用于显示的 HTML 文档资源
	godot::Ref<HTMLDocument> get_document() const;								// 返回控件当前关联的文档资源
	void set_behaviors_enabled(bool p_enabled);									// 设置是否挂载登记的文档行为
	bool are_behaviors_enabled() const;											// 返回是否允许挂载文档行为
	void reload_behaviors();
	godot::TypedArray<godot::Dictionary> get_behavior_diagnostics() const;
	void set_component_scripts(const godot::TypedDictionary<godot::StringName, godot::GDScript>& p_scripts);
	godot::TypedDictionary<godot::StringName, godot::GDScript> get_component_scripts() const;
	godot::TypedArray<godot::Dictionary> get_component_diagnostics() const;
	void reload_components();
	void set_behavior_scripts(
		const godot::TypedDictionary<godot::StringName,
		godot::GDScript>& p_scripts
	);																			// 替换行为脚本登记表
	godot::TypedDictionary<godot::StringName, godot::GDScript> get_behavior_scripts() const; // 返回行为脚本登记表快照
	void set_text(const godot::String& p_text);									// 设置控件的内联 HTML 文本
	godot::String get_text() const;												// 返回控件的内联 HTML 文本
	void set_base_dir(const godot::String& p_base_dir);							// 设置内联 HTML 的相对资源路径基准目录
	godot::String get_base_uri() const;											// 返回当前 DOM 的生效基准
	void set_storage_namespace(const godot::String& p_namespace);
	godot::String get_storage_namespace() const;
	godot::Error navigate(const godot::String& p_url, bool p_replace = false);
	bool back();
	bool forward();
	bool go(int64_t p_delta);
	godot::String get_location() const;
	void set_source_path(const godot::String& p_path);							// 设置编辑器实时预览对应的源文件
	godot::String get_base_dir() const;											// 返回内联 HTML 的资源基准目录
	void set_stylesheet(const godot::String& p_stylesheet);						// 设置应用于内联 HTML 的附加 CSS
	godot::String get_stylesheet() const;										// 返回内联 HTML 的附加 CSS
	godot::Vector2i get_content_size() const;									// 返回排版后的内容尺寸
	godot::String get_render_error() const;										// 返回最近的解析或排版错误
	godot::Vector2i get_scroll_offset() const;									// 返回横向和纵向的文档滚动偏移
	void set_scroll_offset(const godot::Vector2i& p_offset);					// 设置滚动偏移并限制在可滚动范围内
	bool scroll_to_anchor(const godot::String& p_id, float p_align = 0.0f);		// 将指定 HTML ID 的锚点滚到视口内的对齐位置
	godot::Ref<HTMLElement> create_element(const godot::String& p_tag_name);	// 创建可插入当前控件文档的元素
	godot::Ref<HTMLDocumentFragment> create_document_fragment();				// 创建可插入当前控件文档的片段
	godot::Ref<HTMLDocumentNode> get_document_node() const;						// 返回当前控件文档的根节点，用于在该节点上注册事件监听器
	godot::Ref<HTMLRange> create_range();										// 为当前文档创建实时节点范围
	godot::Ref<HTMLSelection> get_selection() const;							// 返回与当前控件共享的节点选区
	godot::Ref<HTMLElement> query_selector(
		const godot::String& p_selector
	) const;																	// 返回当前控件文档中第一个匹配元素
	godot::TypedArray<HTMLElement> query_selector_all(
		const godot::String& p_selector
	) const;																	// 返回当前控件文档中的全部匹配元素
	godot::Ref<HTMLElement> get_element_by_id(const godot::String& p_id) const;	// 按区分大小写的 HTML ID 查找当前控件文档中的第一个元素
	godot::Ref<HTMLElement> get_element_at(
		const godot::Vector2& p_position
	) const;																	// 将控件局部坐标换算为文档坐标并命中可见元素
	godot::TypedArray<HTMLElement> get_elements_at(
		const godot::Vector2& p_position
	) const;																	// 按命中顺序返回控件局部坐标下的元素
	godot::Ref<HTMLElement> get_focused_element() const;						// 返回当前聚焦的 DOM 元素
	bool focus_element(const godot::Ref<HTMLElement>& p_element);				// 将有效元素设为 DOM 焦点
	void on_design_mode_changed();
	bool execute_edit_command(const godot::String& p_command, const godot::String& p_value = {});
	bool activate_element(const godot::Ref<HTMLElement>& p_element);			// 激活当前控件中可用的链接或表单控件
	godot::Error submit_form(const godot::Ref<HTMLFormElement>& p_form,
		const godot::Ref<HTMLElement>& p_submitter = {});						// 校验并发出有序的表单数据
	bool scroll_to_element(
		const godot::Ref<HTMLElement>& p_element,
		float p_align = 0.0f
	);																			// 将当前控件的目标元素滚到视口内的对齐位置
	void set_selection_enabled(bool p_enabled);									// 设置是否允许选择文本
	bool is_selection_enabled() const;											// 返回是否允许选择文本
	void set_context_menu_enabled(bool p_enabled);								// 设置文本字段是否显示编辑菜单
	bool is_context_menu_enabled() const;										// 返回文本字段是否显示编辑菜单
	void set_animation_process_mode(AnimationProcessMode p_mode);				// 设置 CSS 动画和过渡的帧更新方式
	AnimationProcessMode get_animation_process_mode() const;					// 返回 CSS 动画和过渡的帧更新方式
	godot::TypedArray<HTMLAnimation> get_animations(
		const godot::Ref<HTMLElement>& p_element = {}
	);																			// 返回当前布局的动画
	godot::Ref<HTMLAnimation> animate(
		const godot::Ref<HTMLElement>& p_element,
		const godot::TypedArray<godot::Dictionary>& p_keyframes,
		const godot::Dictionary& p_options = {}
	);																			// 在当前布局中创建关键帧动画
	godot::Dictionary get_computed_style(
		const godot::Ref<HTMLElement>& p_element,
		const godot::String& p_pseudo = ""
	);																			// 返回计算样式快照
	godot::String get_computed_style_property(
		const godot::Ref<HTMLElement>& p_element,
		const godot::String& p_name,
		const godot::String& p_pseudo = ""
	);																			// 返回指定属性的计算值
	godot::TypedArray<godot::Dictionary> get_stylesheets();						// 返回当前布局的作者样式表
	godot::PackedStringArray get_css_rules(int64_t p_stylesheet);				// 返回指定样式表的顶层规则
	godot::Error insert_rule(
		int64_t p_stylesheet,
		const godot::String& p_rule,
		int64_t p_index = 0
	);																			// 插入 CSS 规则
	godot::Error delete_rule(int64_t p_stylesheet, int64_t p_index);			// 删除 CSS 规则
	bool supports_css(
		const godot::String& p_property_or_condition,
		const godot::String& p_value = ""
	) const;																	// 检查 CSS 声明或条件
	bool supports_selector(const godot::String& p_selector) const;				// 检查 CSS 选择器
	bool matches_media(const godot::String& p_query) const;						// 检查当前视口的媒体条件
	void advance_animation(double p_delta);										// 手动推进 CSS 动画和过渡，时间增量以秒为单位
	void set_caret_blink_enabled(bool p_enabled);								// 设置聚焦文本字段的插入光标是否闪烁
	bool is_caret_blink_enabled() const;										// 返回插入光标是否闪烁
	void set_caret_blink_interval(double p_interval);							// 设置插入光标切换显示状态的间隔，单位为秒
	double get_caret_blink_interval() const;									// 返回插入光标切换显示状态的间隔
	void select_all();															// 选中全部可复制文本
	void clear_selection();														// 清除当前文本选区
	godot::String get_selected_text() const;									// 返回当前选中的文本
	godot::String get_accessible_text() const;									// 返回发布给 Godot 无障碍接口的文本
	godot::String get_plain_text() const;										// 返回文档选择使用的纯文本
	godot::Vector2i get_selection_range() const;								// 返回当前文档选区起止偏移
	void set_selection_range(int64_t p_from, int64_t p_to);						// 按文档纯文本偏移设置选区
	godot::TypedArray<godot::Rect2> get_selection_rects() const;				// 返回文档坐标下的选区矩形
	bool copy_selection() const;												// 将选区中可复制的文字写入剪贴板
	godot::Vector2i find_text(
		const godot::String& p_text,
		int64_t p_from = 0,
		bool p_case_sensitive = false
	) const;																	// 返回下一个匹配的文本范围
	bool scroll_to_text_range(int64_t p_from, int64_t p_to);					// 滚动到范围内第一个可用的排版文本矩形
	bool scroll_element_into_view(
		const godot::Ref<HTMLElement>& p_element,
		const godot::Dictionary& p_options = {}
	);																			// 按指定对齐方式滚动到元素
	void set_drag_and_drop_selection_enabled(bool p_enabled);
	bool is_drag_and_drop_selection_enabled() const { return drag_and_drop_selection_enabled; }
};

VARIANT_ENUM_CAST(::HTMLLabel::AnimationProcessMode);
VARIANT_ENUM_CAST(::HTMLLabel::VideoBackend);
