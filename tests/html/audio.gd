extends Node

const TONE: String = "res://media/tone.wav"

@onready var label: HTMLLabel = $HTMLLabel

var failures: int


func _ready() -> void:
	_run.call_deferred()


func _run() -> void:
	var fmod: bool = label.use_fmod_bus
	var expected_class: String = "FmodAudioStreamPlayer" if fmod else "AudioStreamPlayer"
	var fmod_meter: Object
	_check(Engine.has_singleton(&"FmodServer") == fmod, "Expected backend is installed")
	var fmod_visible: bool = false
	for property: Dictionary in label.get_property_list():
		if property["name"] == &"use_fmod_bus":
			fmod_visible = (int(property["usage"]) & PROPERTY_USAGE_EDITOR) != 0
	_check(fmod_visible == fmod, "Inspector shows the FMOD option only when the extension is available")
	_check(ResourceLoader.exists(TONE), "Original res:// path resolves to imported audio")
	if not OS.has_feature("editor"):
		_check(not FileAccess.file_exists(TONE), "Export contains the imported resource and remap")
	if fmod:
		var server: Object = Engine.get_singleton(&"FmodServer")
		var buses: Resource = server.call(&"get_audio_bus_layout")
		buses.call(&"add_bus", &"HTML Test")
		_check(server.call(&"set_audio_bus_layout", buses) == OK, "FMOD accepts its independent test bus layout")
		var audio_bus: Object = server.call(&"get_audio_bus", &"HTML Test")
		if audio_bus == null:
			_finish()
			return
		var channel_group: Object = audio_bus.call(&"get_bus")
		fmod_meter = channel_group.call(&"get_dsp", 0)
		_check(fmod_meter != null, "The FMOD output bus exposes a metering DSP")
		if fmod_meter != null:
			fmod_meter.call(&"set_metering_enabled", true, true)
	else:
		AudioServer.add_bus()
		AudioServer.set_bus_name(AudioServer.bus_count - 1, &"HTML Test")

	label.bus = &"HTML Test"
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_MANUAL
	await get_tree().process_frame
	await get_tree().process_frame
	var audio: HTMLAudioElement = label.get_element_by_id("with_controls") as HTMLAudioElement
	var hidden_audio: HTMLAudioElement = label.get_element_by_id("hidden") as HTMLAudioElement
	var unavailable: HTMLAudioElement = label.get_element_by_id("unavailable") as HTMLAudioElement
	_check(audio != null and hidden_audio != null and unavailable != null, "Media nodes use typed handles")
	if audio == null or hidden_audio == null or unavailable == null:
		_finish()
		return
	_check(is_equal_approx(audio.get_bounding_rect().size.x, 300.0), "Chrome width applies by default")
	_check(is_equal_approx(audio.get_bounding_rect().size.y, 54.0), "Chrome height applies by default")
	_check(hidden_audio.get_bounding_rect().size == Vector2.ZERO, "Audio without controls has no layout box")
	_check(not hidden_audio.paused and hidden_audio.current_src == TONE, "Runtime autoplay works with no controls")
	await RenderingServer.frame_post_draw
	var still: Image = get_viewport().get_texture().get_image()
	var icon: Vector2i = Vector2i(audio.get_bounding_rect().position + Vector2(22.0, 27.0))
	_check(still.get_pixelv(icon).r < 0.4, "Draw the embedded Material play icon")
	hidden_audio.pause()
	_check(unavailable.play() != OK and unavailable.current_src.is_empty(), "Missing source reports an error")
	var sample: AudioStreamWAV = AudioStreamWAV.new()
	sample.format = AudioStreamWAV.FORMAT_16_BITS
	sample.mix_rate = 8000
	var frames: PackedByteArray
	frames.resize(8000)
	sample.data = frames
	_check(sample.save_to_wav("user://dockit-tone.wav") == OK, "Create a user:// audio file")
	unavailable.src = "user://dockit-tone.wav"
	_check(unavailable.play() == OK and unavailable.current_src == "user://dockit-tone.wav" and
		is_equal_approx(unavailable.duration, 0.5), "Play an unimported user:// WAV")
	unavailable.pause()
	_check(audio.play() == OK and not audio.paused and audio.current_src == TONE and audio.duration > 2.0,
		"Source fallback opens the imported resource through its UID")
	await get_tree().create_timer(0.25).timeout
	_check(audio.current_time > 0.05, "Media time advances with CSS animation in Manual mode")
	var player: Node = _find_player(expected_class, &"HTML Test", 0.05)
	_check(player != null, "Selected player is active on the configured bus")
	if fmod_meter != null:
		var levels: Dictionary = fmod_meter.call(&"get_metering_info")
		var output: Dictionary = levels.get("output", {})
		_check(float(output.get("max_peak", 0.0)) > 0.001, "The FMOD bus receives an audio signal")
	elif not fmod:
		var bus_index: int = AudioServer.get_bus_index(&"HTML Test")
		_check(AudioServer.get_bus_peak_volume_left_db(bus_index, 0) > -60.0, "The Godot bus receives an audio signal")
	audio.pause()
	var paused_time: float = audio.current_time
	await get_tree().create_timer(0.1).timeout
	_check(audio.paused and is_equal_approx(audio.current_time, paused_time), "Pause preserves playback position")
	audio.current_time = 1.0
	audio.volume = 0.5
	audio.muted = true
	_check(is_equal_approx(audio.current_time, 1.0) and is_equal_approx(audio.volume, 0.5) and audio.muted,
		"Seek, volume, and mute update the element state")
	audio.muted = false
	_check(audio.play() == OK, "Paused stream resumes")
	await get_tree().create_timer(0.12).timeout
	_check(audio.current_time > 1.0, "Resume starts from the position set while paused")
	if player != null:
		label.bus = &"Missing Bus"
		_check(player.get(&"bus") == &"Master" and label.bus == &"Missing Bus", "Missing bus falls back during playback")
		label.bus = &"HTML Test"
		_check(player.get(&"bus") == &"HTML Test", "Bus change reaches the active player")
	var root_handle: HTMLElement = label.get_element_by_id("root")
	_check(root_handle.append_child(audio) == OK, "Move the connected media node")
	await get_tree().process_frame
	_check(not audio.paused, "Moving within the document preserves playback")
	audio.pause()
	var center: Vector2 = audio.get_bounding_rect().position + Vector2(23.0, 27.0)
	_click(center)
	await get_tree().process_frame
	_check(not audio.paused, "Painted play button activates")
	audio.pause()
	_check(label.focus_element(audio), "Controls participate in DOM focus")
	var enter: InputEventKey = InputEventKey.new()
	enter.keycode = KEY_ENTER
	enter.pressed = true
	get_viewport().push_input(enter)
	await get_tree().process_frame
	_check(not audio.paused, "Keyboard can activate the audio controls")
	audio.pause()
	var timeline: Vector2 = audio.get_bounding_rect().position + Vector2(156.0, 27.0)
	_click(timeline)
	_check(audio.current_time > 1.0 and audio.current_time < audio.duration, "Timeline click seeks within the track")
	_check(audio.set_style_property("width", "72px") == OK, "Set narrow player width through CSS")
	await get_tree().process_frame
	await get_tree().process_frame
	_check(is_equal_approx(audio.get_bounding_rect().size.x, 72.0), "Narrow layout uses authored width")
	_click(audio.get_bounding_rect().position + Vector2(22.0, 27.0))
	_check(not audio.paused, "Narrow player keeps the play button")
	audio.pause()
	_check(audio.set_style_property("color-scheme", "dark") == OK, "Set the player color scheme")
	_check(audio.set_style_property("transform-origin", "0 0") == OK and
		audio.set_style_property("transform", "translate(90px, 16px) rotate(90deg)") == OK,
		"Transform the media control")
	await get_tree().process_frame
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var rotated: Rect2 = audio.get_bounding_rect()
	var frame: Image = get_viewport().get_texture().get_image()
	var skin: Color = frame.get_pixelv(Vector2i(rotated.position + Vector2(9.0, 35.0)))
	_check(skin.r < 0.45 and skin.g < 0.45, "Dark color scheme paints a dark audio background")
	_click(rotated.position + Vector2(27.0, 22.0))
	_check(not audio.paused, "Rotated play button uses transformed hit coordinates")
	audio.loop = true
	audio.current_time = audio.duration - 0.08
	await get_tree().create_timer(0.40).timeout
	_check(not audio.paused and audio.current_time < audio.duration - 0.1, "Loop restarts at the beginning")
	audio.src = "missing.wav"
	await get_tree().process_frame
	await get_tree().process_frame
	_check(audio.paused and audio.current_src.is_empty(), "Changing src releases the previous playback")
	label.document = null
	label.text = "Replaced"
	await get_tree().process_frame
	await get_tree().process_frame
	_check(not audio.is_valid() and _find_player(expected_class, &"HTML Test", 0.0) == null,
		"Replacing the document releases internal players")
	_finish()


func _click(position: Vector2) -> void:
	for pressed: bool in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.position = position
		event.pressed = pressed
		get_viewport().push_input(event)


func _find_player(player_class: String, output_bus: StringName, time: float) -> Node:
	for child: Node in label.get_children(true):
		if child.is_class(player_class) and child.get(&"bus") == output_bus and child.call(&"get_playback_position") >= time:
			return child
	return null


func _check(condition: bool, message: String) -> void:
	if condition:
		return
	failures += 1
	push_error(message)


func _finish() -> void:
	print("HTML audio checks finished with %d failure(s)" % failures)
	get_tree().quit(failures)
