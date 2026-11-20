@tool
extends EditorPlugin


func _enter_tree() -> void:
	_check_video.call_deferred()


func _check_video() -> void:
	var selector: String = ProjectSettings.get_setting("dockit_test/video_selector", "#intrinsic")
	var scene_label: HTMLLabel
	var deadline: int = Time.get_ticks_msec() + 12000
	while Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
		var scene: Node = EditorInterface.get_edited_scene_root()
		if scene != null:
			scene_label = scene.find_child("HTMLLabel", true, false) as HTMLLabel
		if scene_label != null:
			break
	if scene_label == null or not await _check_element(scene_label, selector, "edited scene"):
		push_error("Editor video probe could not measure the scene label")
		get_tree().quit(1)
		return

	var script_editor: ScriptEditor = EditorInterface.get_script_editor()
	EditorInterface.set_main_screen_editor("Script")
	deadline = Time.get_ticks_msec() + 12000
	var preview: HTMLLabel
	while Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
		var menu: MenuButton = script_editor.find_child("HTMLPreviewMenu", true, false) as MenuButton
		if menu != null and menu.is_visible_in_tree():
			var popup: PopupMenu = menu.get_popup()
			popup.id_pressed.emit(popup.get_item_id(1))
			preview = script_editor.find_child("HTMLPreviewPanel", true, false) as HTMLLabel
			if preview != null:
				break
	if preview == null or not await _check_element(preview, selector, "HTML preview"):
		push_error("Editor video probe could not measure the source preview")
		get_tree().quit(1)
		return
	print("Editor video dimensions passed.")
	get_tree().quit()


func _check_element(label: HTMLLabel, selector: String, context: String) -> bool:
	var deadline: int = Time.get_ticks_msec() + 12000
	while Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
		var video: HTMLVideoElement = label.query_selector(selector) as HTMLVideoElement
		if video == null or video.video_width <= 0 or video.video_height <= 0:
			continue
		var intrinsic_size: Vector2 = Vector2(video.video_width, video.video_height)
		var box_size: Vector2 = video.get_bounding_rect().size
		if video.paused and video.current_time == 0.0 and box_size.is_equal_approx(intrinsic_size):
			print("Editor video %s measured %s while paused." % [context, intrinsic_size])
			return true
	return false
