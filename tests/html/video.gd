extends Node

const SUCCESS: String = "HTML video checks finished with 0 failure(s)"
const SOURCE: String = "res://media/movie.mp4"
const POSTER_COLOR: Color = Color("#ea1880")

@onready var label: HTMLLabel = $Frame/HTMLLabel
@onready var frame: SubViewport = $Frame
var _failures: int


func _ready() -> void:
	_run.call_deferred()


func _run() -> void:
	var bus_index: int = AudioServer.get_bus_count()
	AudioServer.add_bus()
	AudioServer.set_bus_name(bus_index, &"TestVideo")
	label.video_bus = &"TestVideo"
	if OS.get_cmdline_user_args().has("--embedded-window"):
		var embedded: Window = Window.new()
		embedded.size = frame.size
		add_child(embedded)
		frame.reparent(embedded)
		_check(embedded.is_embedded(), "The fixture creates a window embedded in the game")
	await get_tree().process_frame
	await get_tree().process_frame
	var video: HTMLVideoElement = label.get_element_by_id("movie") as HTMLVideoElement
	var unavailable: bool = OS.get_cmdline_user_args().has("--unavailable")
	var exported: bool = OS.get_cmdline_user_args().has("--pck")
	var alias: String = "res://.dockit/video/" + SOURCE.sha256_text() + ".mp4"
	_check(video != null, "The document loads the specialized video handle")
	_check(label.get_element_by_id("after") != null, "HTML following the media still loads")
	if exported:
		_check(FileAccess.file_exists(alias), "The selected-scene export includes raw video bytes")
		_check(not FileAccess.file_exists(SOURCE), "Playback uses the packaged alias without the original source")
	if video == null:
		get_tree().quit(1)
		return
	_check(video.controls and video.width == 240 and video.height == 136, "HTML media attributes reach the element")
	_check(video.get_bounding_rect().size.is_equal_approx(Vector2(240.0, 136.0)), "Video dimensions participate in layout")
	var created: HTMLVideoElement = label.create_element("video") as HTMLVideoElement
	_check(created != null and not created.is_connected(), "DOM creation returns a video handle")
	var pixel: Color = await _center_pixel()
	if unavailable:
		_check(pixel.r < 0.01 and pixel.g < 0.01 and pixel.b < 0.01, "No backend paints black even with a colorful poster")
		_check(not label.focus_element(video), "No backend exposes media controls to keyboard focus")
		_check(video.play() == ERR_UNAVAILABLE and video.request_fullscreen() == ERR_UNAVAILABLE,
			"Unavailable playback and fullscreen return ERR_UNAVAILABLE")
		_check(label.get_fullscreen_video() == null, "Unavailable playback creates no video window")
	else:
		_check(pixel.r > 0.8 and pixel.b > 0.35 and pixel.g < 0.2, "The poster appears while playback is idle")
		var intrinsic: HTMLVideoElement = label.get_element_by_id("intrinsic") as HTMLVideoElement
		var deadline: int = Time.get_ticks_msec() + 11000
		var preparation_audible: bool = false
		while intrinsic != null and (video.video_width == 0 or intrinsic.video_width == 0) and Time.get_ticks_msec() < deadline:
			await get_tree().process_frame
			preparation_audible = preparation_audible or AudioServer.get_bus_peak_volume_left_db(bus_index, 0) > -90.0
		await get_tree().process_frame
		await get_tree().process_frame
		_check(video.video_width > 0 and video.video_height > 0 and video.current_src == SOURCE,
			"Metadata is available before the play button is pressed")
		_check(video.paused and video.current_time == 0.0 and not preparation_audible,
			"Metadata preparation leaves the timeline paused at zero and the video silent")
		_check(intrinsic != null and intrinsic.video_width > 0 and intrinsic.video_height > 0 and
			intrinsic.get_bounding_rect().size.is_equal_approx(Vector2(intrinsic.video_width, intrinsic.video_height)),
			"An unsized video uses its intrinsic size while paused")
		pixel = await _center_pixel()
		_check(pixel.r > 0.8 and pixel.b > 0.35 and pixel.g < 0.2, "The poster survives metadata preparation")
		await _check_playback(video, bus_index)
	if _failures == 0:
		print(SUCCESS)
	get_tree().quit(_failures)


func _check_playback(video: HTMLVideoElement, bus_index: int) -> void:
	var requested_backend: int = label.video_backend
	var failed: Array[String] = []
	label.video_failed.connect(func(_video: HTMLVideoElement, _error: int, message: String) -> void: failed.append(message))
	var changes: Array[bool] = []
	label.video_fullscreen_changed.connect(func(_video: HTMLVideoElement, active: bool) -> void: changes.append(active))
	var fixture_bus_heard: bool = false
	frame.notify_mouse_entered()
	var click: InputEventMouseButton = InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.position = Vector2(25.0, 116.0)
	click.pressed = true
	frame.push_input(click, true)
	click.pressed = false
	frame.push_input(click, true)
	_check(not video.paused, "The built-in play button starts the second source after a missing candidate")
	var deadline: int = Time.get_ticks_msec() + 11000
	while Time.get_ticks_msec() < deadline and failed.is_empty():
		await get_tree().process_frame
		if AudioServer.get_bus_peak_volume_left_db(bus_index, 0) > -65.0:
			fixture_bus_heard = true
		if fixture_bus_heard and video.video_width > 0 and video.current_time > 0.4:
			break
	_check(failed.is_empty(), "Playback did not report a terminal error: %s" % [failed])
	_check(video.current_src == SOURCE and video.video_width > 0 and video.video_height > 0 and video.duration > 1.0,
		"The backend opens the source and reports metadata")
	_check(fixture_bus_heard, "Video audio uses the configured Godot bus")
	var first_frame: Image = await _snapshot()
	var frame_has_picture: bool = false
	for y: int in range(12, 85, 8):
		for x: int in range(16, 220, 8):
			var pixel: Color = first_frame.get_pixel(x, y)
			if pixel.get_luminance() > 0.06 and not pixel.is_equal_approx(POSTER_COLOR):
				frame_has_picture = true
	_check(frame_has_picture, "The decoded frame reaches the inline HTML paint path")
	video.pause()
	await get_tree().process_frame
	_check(video.paused, "Playback pauses")
	var paused_at: float = video.current_time
	await get_tree().create_timer(0.12).timeout
	_check(is_equal_approx(paused_at, video.current_time), "The paused clock is stable")
	var target: float = video.duration * 0.65
	_check(video.seek(target) == OK and absf(video.current_time - target) < 0.5, "Seeking updates the paused timeline")
	video.volume = 0.4
	video.muted = true
	_check(is_equal_approx(video.volume, 0.4) and video.muted, "Volume and mute are independent")

	var host: Window = label.get_window()
	var previous: Window.Mode = host.mode
	var previous_size: Vector2i = host.size
	var previous_pos: Vector2i = host.position
	_check(video.request_fullscreen() == OK and label.get_fullscreen_video() == video and video.is_fullscreen(),
		"The video enters fullscreen using the same handle")
	var uid_video: HTMLVideoElement = label.get_element_by_id("uid_movie") as HTMLVideoElement
	_check(uid_video.request_fullscreen() == ERR_BUSY, "A host window presents one fullscreen video at a time")
	await get_tree().process_frame
	var video_window: Window = label.find_child("_HTMLVideoFullscreenWindow", true, false) as Window
	if not Engine.is_embedded_in_editor() and not host.is_embedded() and DisplayServer.has_feature(DisplayServer.FEATURE_SUBWINDOWS):
		_check(video_window != null and video_window.get_window_id() != host.get_window_id() and
			video_window.mode == Window.MODE_FULLSCREEN and video_window.current_screen == host.current_screen,
			"Desktop playback creates a native fullscreen window: %s, host mode %s, embedded %s, children %s" %
			[[video_window.get_window_id(), video_window.mode] if video_window else [], host.mode, host.is_embedded(), label.get_children(true)])
	else:
		var overlay: Control = label.find_child("_HTMLVideoFullscreenSurface", true, false) as Control
		_check(video_window == null and overlay != null and overlay.size.is_equal_approx(Vector2(frame.size)),
			"Embedded playback fills the owning viewport")
	_check(host.mode == previous and host.size == previous_size and host.position == previous_pos,
		"Entering video fullscreen preserves the host window's geometry and mode")
	host.mode = Window.MODE_MAXIMIZED
	await get_tree().process_frame
	if video_window:
		video_window.close_requested.emit()
	else:
		video.exit_fullscreen()
	await get_tree().process_frame
	await get_tree().process_frame
	_check(host.mode == Window.MODE_MAXIMIZED and not video.is_fullscreen() and label.get_fullscreen_video() == null,
		"Closing the video leaves the app's window-mode change intact")
	_check(video.paused and absf(video.current_time - target) < 0.5 and changes == [true, false],
		"Fullscreen exit preserves the paused seek position and emits both signals")
	video.loop = true
	_check(video.seek(video.duration - 0.4) == OK and video.play() == OK, "Playback resumes close to the end of the video")
	deadline = Time.get_ticks_msec() + 7000
	while Time.get_ticks_msec() < deadline and video.current_time > video.duration * 0.5:
		await get_tree().process_frame
	_check(not video.paused and video.current_time < video.duration * 0.5, "Looping restarts the same media")
	video.pause()
	_check(uid_video.play() == OK and uid_video.current_src == SOURCE, "A UID video source resolves to the exported local file")
	uid_video.pause()
	label.video_backend = HTMLLabel.VIDEO_BACKEND_VLC if requested_backend == HTMLLabel.VIDEO_BACKEND_FFMPEG else HTMLLabel.VIDEO_BACKEND_FFMPEG
	if not (ClassDB.class_exists(&"VLCMediaPlayer") and ClassDB.class_exists(&"FFmpegVideoStream")):
		await get_tree().process_frame
		_check(video.play() == ERR_UNAVAILABLE, "An explicitly selected missing backend does not fall back")
	label.video_backend = requested_backend
	await get_tree().process_frame
	await get_tree().process_frame
	_check(video.request_fullscreen() == OK, "The video can re-enter fullscreen after the backend selection changes")
	var surface: Control = label.find_child("_HTMLVideoFullscreenSurface", true, false) as Control
	label.queue_free()
	await get_tree().process_frame
	await get_tree().process_frame
	_check(not is_instance_valid(surface) and not video.is_fullscreen(), "Removing the label releases the fullscreen surface")


func _snapshot() -> Image:
	await RenderingServer.frame_post_draw
	return frame.get_texture().get_image()


func _center_pixel() -> Color:
	var image: Image = await _snapshot()
	return image.get_pixel(55, 48)


func _check(condition: bool, description: String) -> void:
	if not condition:
		_failures += 1
		push_error(description)
