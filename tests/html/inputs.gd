extends Node

var _failures: int
@onready var _label: HTMLLabel = $HTMLLabel


func _ready() -> void:
	for _frame: int in range(6):
		await get_tree().process_frame
	_check_types()
	_check_temporal_values()
	_check_validation()
	await _check_forms()
	_check_color()
	_check_type_changes()
	await _check_input_alignment()
	if DisplayServer.get_name() != "headless":
		await _check_picker()
		await _check_color_picker()
		await _check_color_eyedropper()
		await _check_clock_picker()
		await _check_radio_icons()
		await _check_checkbox_icons()
		await _check_image_pixels()
		if "--native-file-picker" not in OS.get_cmdline_user_args():
			await _check_embedded_file_picker()
	if "--native-file-picker" in OS.get_cmdline_user_args():
		await _check_file_picker()
	print("HTML inputs completed with %d failures" % _failures)
	get_tree().quit(0 if _failures == 0 else 1)


func _check_types() -> void:
	var types: PackedStringArray = [
		"text", "search", "tel", "url", "email", "password", "number", "range",
		"date", "month", "week", "time", "datetime-local", "color", "file",
		"checkbox", "radio", "button", "submit", "reset", "image", "hidden",
	]
	for kind: String in types:
		var input: HTMLInputElement = _field(kind)
		_check(input != null and input.type == kind, "input type " + kind)
		if input != null:
			_check(input.get_bounding_rect().has_area() == (kind != "hidden"), "input geometry " + kind)
	_check(_field("unknown").type == "text", "an unknown type uses the text state")


func _check_temporal_values() -> void:
	var date: HTMLInputElement = _field("date")
	_check(date.value == "2024-02-29", "leap date markup")
	var leap: Dictionary = date.get_value_as_date()
	_check(leap.get("year") == 2024 and leap.get("month") == 2 and leap.get("day") == 29, "date parts")
	date.value = "1900-02-29"
	_check(date.value.is_empty() and is_nan(date.value_as_number), "invalid date sanitizes to empty")
	_check(date.set_value_as_date({"year": 2000, "month": 2, "day": 29,}) == OK and date.value == "2000-02-29",
		"date setter preserves valid leap day")
	_check(date.step_up() == OK and date.value == "2000-03-01", "date step crosses month")

	var week: HTMLInputElement = _field("week")
	_check(week.value == "2020-W53" and week.step_up() == OK and week.value == "2021-W01", "ISO weeks cross year")
	week.value = "2021-W53"
	_check(week.value.is_empty(), "year without ISO week 53")

	var month: HTMLInputElement = _field("month")
	_check(month.value_as_number == -1.0 and month.step_up() == OK and month.value == "1970-01", "month numeric epoch")
	var time: HTMLInputElement = _field("time")
	_check(time.value_as_number == 86399999.0 and time.step_up() == OK and time.value == "00:00", "fractional time step")
	time.set_attribute("min", "23:00")
	time.set_attribute("max", "04:00")
	time.value = "02:00"
	_check(time.get_validity().get("valid") == true, "time can span midnight")
	time.value = "12:00"
	_check(time.get_validity().get("valid") == false, "midday falls outside a midnight range")
	var local: HTMLInputElement = _field("datetime-local")
	_check(local.value == "2024-03-31T03:12" and local.get_value_as_date().is_empty(), "local time has a normalized value")


func _check_validation() -> void:
	var text: HTMLInputElement = _field("text")
	text.value = "ABC"
	_check(text.get_validity().get("pattern_mismatch") == false, "pattern valid text")
	text.value = "ABC1"
	_check(text.get_validity().get("pattern_mismatch") == true, "pattern must match whole text")
	text.set_attribute("readonly", "")
	_check(text.get_validity().get("will_validate") == false, "readonly invalid text is not a validation candidate")
	var email: HTMLInputElement = _field("email")
	email.value = "one@example.com, two@example.org"
	_check(email.get_validity().get("valid") == true, "multiple email accepts comma-separated addresses")
	email.value = "one@example.com, example.org"
	_check(email.get_validity().get("type_mismatch") == true, "multiple email checks every address")
	email.value = ""
	var number: HTMLInputElement = _field("number")
	number.value = "3"
	_check(number.get_validity().get("step_mismatch") == true, "off-step number stays editable and invalid")
	_check(number.step_up() == OK and number.value_as_number == 4.0, "step advances to the next permitted number")
	number.value = "20"
	_check(number.get_validity().get("range_overflow") == true, "number overflow")
	number.set_attribute("step", "any")
	_check(number.step_down() == ERR_UNAVAILABLE, "step any rejects a scripted step")
	_check(_field("password").get_selection_start() >= 0 and number.get_selection_start() == -1,
		"text selection API applies only to HTML selection types")


func _check_forms() -> void:
	var form: HTMLFormElement = _label.get_element_by_id("form") as HTMLFormElement
	var file: HTMLInputElement = _field("file")
	file.value = "res://private.txt"
	_check(file.value.is_empty() and file.get_files().is_empty() and file.get_validity().get("value_missing") == true,
		"scripts cannot select arbitrary files by setting value")
	_check(_label.submit_form(form) == ERR_INVALID_DATA, "validation blocks an empty required upload")
	var named: Array[Dictionary] = form.get_form_data(_field("image"))
	_check(_entry(named, "photo.x") == "0" and _entry(named, "photo.y") == "0", "image submit uses default coordinates")
	var empty: HTMLInputFile = _entry(named, "upload") as HTMLInputFile
	_check(empty != null and empty.size == 0 and empty.name.is_empty(), "form data contains the empty upload file")
	_field("image").remove_attribute("name")
	var unnamed: Array[Dictionary] = form.get_form_data(_field("image"))
	_check(_entry(unnamed, "x") == "0" and _entry(unnamed, "y") == "0", "unnamed image emits coordinate names")
	var checkbox: HTMLInputElement = _field("checkbox")
	checkbox.indeterminate = true
	_check(checkbox.matches(":indeterminate"), "checkbox sets its indeterminate pseudo-class")
	await get_tree().process_frame
	await get_tree().process_frame
	_check(_label.activate_element(checkbox) and checkbox.checked and not checkbox.indeterminate,
		"user activation toggles and clears indeterminate")
	_check(form.reset() == OK and not checkbox.checked and _field("hidden").value == "token", "form reset restores input defaults")


func _check_color() -> void:
	var color: HTMLInputElement = _field("color")
	color.value = "red"
	_check(color.value == "#ff0000", "color serializes CSS names to limited sRGB")
	color.value = "not-a-color"
	_check(color.value == "#000000", "invalid color defaults to black")
	color.set_attribute("alpha", "")
	color.set_attribute("colorspace", "display-p3")
	color.value = "color(display-p3 1 0 0 / 0.5)"
	_check(color.value.begins_with("color(display-p3 1") and color.value.contains("/ 0.5"), "P3 and alpha are preserved")


func _check_type_changes() -> void:
	var input: HTMLInputElement = _field("unknown")
	input.value = "edited"
	input.type = "hidden"
	_check(input.value == "edited" and input.get_attribute("value") == "edited", "value is transferred to reflected state")
	input.type = "text"
	_check(input.value == "edited", "value is restored from attribute when entering text mode")
	input.type = "file"
	_check(input.value.is_empty(), "changing to filename mode resets the state")
	input.type = "search"
	_check(input.type == "search" and input.is_valid(), "changing type preserves the element handle")


func _check_picker() -> void:
	var date: HTMLInputElement = _field("date")
	date.value = "2024-02-29"
	date.set_attribute("max", "2024-03-01")
	var month: HTMLInputElement = _field("month")
	month.value = "1969-12"
	var week: HTMLInputElement = _field("week")
	week.value = "2020-W53"
	for _frame: int in range(4):
		await get_tree().process_frame
	var result: Error = date.show_picker()
	_check(result == OK, "opens the date picker (%s)" % error_string(result))
	var panel: Control = _label.get_node_or_null(^"_HTMLTimePicker") as Control
	_check(panel != null and panel.visible and panel.size == Vector2(250.0, 282.0), "date opens a canvas calendar")
	if panel == null:
		return
	var caption: Rect2 = Rect2(8.0, 8.0, 225.0, 58.0)
	await RenderingServer.frame_post_draw
	var chinese_page: PackedByteArray = _picker_region(panel, caption)
	_color_picker_key(KEY_ESCAPE)
	_check(not panel.visible and date.value == "2024-02-29", "cancel leaves date unchanged")
	date.set_attribute("lang", "en-GB")
	for _frame: int in range(2):
		await get_tree().process_frame
	_check(date.show_picker() == OK, "date calendar reopens")
	await RenderingServer.frame_post_draw
	_check(chinese_page == _picker_region(panel, caption), "calendar title and weekday labels stay English with Chinese lang")
	_color_picker_key(KEY_RIGHT)
	_color_picker_key(KEY_RIGHT)
	_color_picker_key(KEY_ENTER)
	_check(not panel.visible and date.value == "2024-03-01", "date preview crosses leap day and respects max")
	date.set_attribute("min", "2000-01-01")
	date.set_attribute("max", "2000-01-02")
	date.value = ""
	for _frame: int in range(2):
		await get_tree().process_frame
	_check(date.show_picker() == OK and panel.visible, "a cleared date with an old range reopens its calendar")
	_color_picker_key(KEY_LEFT)
	_color_picker_key(KEY_ENTER)
	_check(date.value == "2000-01-01", "an empty picker starts at the nearest allowed date instead of a disabled today")
	date.remove_attribute("min")
	date.remove_attribute("max")
	date.value = ""
	for _frame: int in range(2):
		await get_tree().process_frame
	_check(date.show_picker() == OK, "an unconstrained cleared date can reopen its calendar")
	_color_picker_click(panel, Vector2(194.0, 259.0))
	_check(date.value == Time.get_date_string_from_system(), "Today selects the current date after clearing")
	_check(month.show_picker() == OK and panel.visible and panel.size == Vector2(232.0, 182.0),
		"month uses the canvas month grid")
	_color_picker_click(panel, Vector2(205.0, 23.0))
	_color_picker_click(panel, Vector2(39.0, 55.0))
	_check(not panel.visible and month.value == "1970-01", "month grid navigation and clicking commit a month")
	_check(week.show_picker() == OK and panel.visible and panel.size == Vector2(282.0, 282.0),
		"week uses the canvas calendar with ISO week numbers")
	_color_picker_click(panel, Vector2(29.0, 228.0))
	_check(not panel.visible and week.value == "2021-W01", "clicking an ISO week number crosses the year boundary")
	_check(_label.get_node_or_null(^"_HTMLInputPicker") == null, "all date pickers use canvas drawing")


func _picker_region(panel: Control, region: Rect2) -> PackedByteArray:
	var bounds: Rect2i = Rect2i(panel.get_global_transform_with_canvas() * region)
	return get_viewport().get_texture().get_image().get_region(bounds).get_data()


func _check_radio_icons() -> void:
	var radio: HTMLInputElement = _field("radio")
	var bounds: Rect2 = radio.get_bounding_rect()
	var center: Vector2i = Vector2i(bounds.get_center())
	await RenderingServer.frame_post_draw
	var pixels: Image = get_viewport().get_texture().get_image()
	_check(pixels.get_pixelv(center).get_luminance() > 0.9 and
		pixels.get_pixelv(center + Vector2i(0, -9)).get_luminance() < 0.3, "unchecked radio draws the Material outline")
	_check(_label.activate_element(radio) and radio.checked, "activating the radio checks it")
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	var accent: Color = pixels.get_pixelv(center)
	_check(accent.b > 0.8 and accent.r < 0.2, "checked radio draws the Material center dot with the accent color")
	var select: HTMLSelectElement = _label.get_element_by_id("select") as HTMLSelectElement
	_check(select.selected_index == 0 and _label.activate_element(select), "open the menu on its default option")
	var menu: Control = _label.get_node_or_null(^"_HTMLSelectMenu") as Control
	_check(menu != null and menu.visible, "select menu is visible")
	if menu == null:
		return
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	var transform: Transform2D = menu.get_global_transform_with_canvas()
	var first: Vector2i = Vector2i(transform * Vector2(9.0, menu.size.y * 0.25))
	var second: Vector2i = Vector2i(transform * Vector2(9.0, menu.size.y * 0.75))
	_check(pixels.get_pixelv(first).get_luminance() > 0.6 and pixels.get_pixelv(second).get_luminance() > 0.8,
		"select options have no radio icon even when the selected option has transparent text")
	_color_picker_key(KEY_DOWN)
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	_check(pixels.get_pixelv(first).get_luminance() > 0.8 and pixels.get_pixelv(second).get_luminance() > 0.6,
		"keyboard navigation moves the highlight without drawing a radio icon")
	_color_picker_key(KEY_ENTER)
	_check(select.selected_index == 1, "keyboard selection commits the second option")
	for _frame: int in range(2):
		await get_tree().process_frame
	_check(_label.activate_element(select), "reopen the select menu after layout")
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	_check(pixels.get_pixelv(first).get_luminance() > 0.8 and pixels.get_pixelv(second).get_luminance() > 0.6,
		"reopened menu highlights the new selected option without a radio icon")
	_color_picker_key(KEY_ESCAPE)


func _check_checkbox_icons() -> void:
	var checkbox: HTMLInputElement = _field("checkbox")
	var center: Vector2i = Vector2i(checkbox.get_bounding_rect().get_center())
	var frame: Vector2i = center + Vector2i(0, -8)
	var mark: Vector2i = center + Vector2i(3, -2)
	var accent: Color = Color("#0075ff")
	await RenderingServer.frame_post_draw
	var pixels: Image = get_viewport().get_texture().get_image()
	_check(pixels.get_pixelv(frame).get_luminance() < 0.3 and pixels.get_pixelv(center).get_luminance() > 0.9,
		"unchecked checkbox draws the Material outline around an empty center")
	_check(_label.activate_element(checkbox) and checkbox.checked, "activating the checkbox checks it")
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	_check(pixels.get_pixelv(frame).is_equal_approx(accent) and pixels.get_pixelv(mark).get_luminance() > 0.9,
		"checked checkbox draws the accent-colored Material check")
	checkbox.indeterminate = true
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	_check(checkbox.checked and pixels.get_pixelv(mark).is_equal_approx(accent) and
		pixels.get_pixelv(center).get_luminance() > 0.9, "the Material dash takes precedence for an indeterminate checkbox")
	checkbox.set_attribute("disabled", "")
	for _frame: int in range(2):
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	var disabled: Color = pixels.get_pixelv(frame)
	_check(disabled.r > 0.3 and disabled.r < 0.95 and is_equal_approx(disabled.r, disabled.b),
		"the disabled checkbox icon uses the disabled text color")
	checkbox.set_attribute("style", "appearance: none")
	for _frame: int in range(2):
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	_check(pixels.get_pixelv(frame).get_luminance() > 0.9 and pixels.get_pixelv(mark).get_luminance() > 0.9,
		"appearance none suppresses the built-in checkbox icon")


func _check_image_pixels() -> void:
	await RenderingServer.frame_post_draw
	var bounds: Rect2 = _field("image").get_bounding_rect()
	var pixels: Image = get_viewport().get_texture().get_image()
	var point: Vector2i = Vector2i(bounds.get_center())
	_check(get_viewport().get_visible_rect().has_point(Vector2(point)), "the image lies within the viewport")
	var pixel: Color = pixels.get_pixelv(point)
	_check(pixel.g > 0.6 and pixel.r < 0.35, "input image paints the imported green image")
	var received: Array[Dictionary]
	_label.form_submitted.connect(
		func(_form: HTMLFormElement, entries: Array[Dictionary], _submitter: HTMLElement) -> void:
			received.assign(entries),
		CONNECT_ONE_SHOT,
	)
	for state: bool in [true, false]:
		var click: InputEventMouseButton = InputEventMouseButton.new()
		click.button_index = MOUSE_BUTTON_LEFT
		click.pressed = state
		click.position = bounds.position + Vector2(15.0, 9.0)
		get_viewport().push_input(click, true)
	_check(_entry(received, "x") == "10" and _entry(received, "y") == "6",
		"image click submission uses inverse-transformed CSS coordinates")


func _check_color_picker() -> void:
	var swatch: HTMLInputElement = _field("swatch")
	var bounds: Rect2 = swatch.get_bounding_rect()
	var marker: HTMLElement = _label.get_element_by_id("swatch-baseline")
	_check(bounds.size.is_equal_approx(Vector2(50.0, 27.0)), "closed color well has the Blink default size")
	_check(absf(marker.get_bounding_rect().end.y - (bounds.end.y - 6.0)) < 1.0,
		"neighboring text aligns to the bottom of the swatch wrapper")
	await RenderingServer.frame_post_draw
	var pixels: Image = get_viewport().get_texture().get_image()
	var painted: Color = pixels.get_pixelv(Vector2i(bounds.position + Vector2(36.0, 13.0)))
	_check(painted.is_equal_approx(Color("#e66465")), "the closed well paints an unobstructed color sample")
	_check(swatch.show_picker() == OK, "color input opens its canvas picker")
	await get_tree().process_frame
	var panel: Control = _label.get_node_or_null(^"_HTMLColorPicker") as Control
	_check(panel != null and panel.visible, "the color picker is a visible canvas surface")
	_check(_label.get_node_or_null(^"_HTMLInputPicker") == null, "the color picker uses no popup window")
	if panel == null:
		return
	await RenderingServer.frame_post_draw
	pixels = get_viewport().get_texture().get_image()
	var light_point: Vector2i = Vector2i(panel.get_global_rect().position + Vector2(8.0, 8.0))
	var dark_point: Vector2i = Vector2i(panel.get_global_rect().position + Vector2(8.0, 130.0))
	_check(pixels.get_pixelv(light_point).r > 0.8 and pixels.get_pixelv(dark_point).r < 0.2,
		"canvas picker renders the saturation and value gradient")
	_color_picker_click(panel, Vector2(120.0, 80.0))
	_color_picker_key(KEY_ESCAPE)
	_check(not panel.visible and swatch.value == "#e66465", "Escape discards the preview and closes the panel")
	_check(swatch.show_picker() == OK, "color picker reopens after cancel")
	_color_picker_click(panel, Vector2(122.0, 80.0))
	_color_picker_key(KEY_ENTER)
	_check(not panel.visible and swatch.value != "#e66465", "Enter applies the selected color")
	_check(swatch.show_picker() == OK, "opens manual color fields")
	_color_picker_click(panel, Vector2(48.0, 198.0))
	_color_picker_key(KEY_1, 49)
	_color_picker_key(KEY_2, 50)
	_color_picker_key(KEY_ENTER)
	_check(swatch.value.begins_with("#0c"), "manual RGB field applies the typed red channel")


func _check_color_eyedropper() -> void:
	if not DisplayServer.has_feature(DisplayServer.FEATURE_MOUSE):
		return
	var swatch: HTMLInputElement = _field("swatch")
	_check(swatch.show_picker() == OK, "opens the color panel for eyedropper sampling")
	var panel: Control = _label.get_node_or_null(^"_HTMLColorPicker") as Control
	if panel == null:
		return
	await RenderingServer.frame_post_draw
	var original: Image = get_viewport().get_texture().get_image()
	var mouse_mode: Input.MouseMode = Input.mouse_mode
	_color_picker_click(panel, Vector2(20.0, 160.0))
	var lens: TextureRect = _label.get_node_or_null(^"_HTMLColorEyedropper") as TextureRect
	_check(lens != null and lens.visible and panel.visible, "eyedropper shows its lens while retaining the panel")
	_check(Input.mouse_mode == Input.MOUSE_MODE_HIDDEN, "eyedropper hides the mouse cursor")
	if lens == null:
		_color_picker_key(KEY_ESCAPE)
		return

	var overlap: Vector2 = panel.get_global_transform_with_canvas() * Vector2(120.5, 90.5)
	_move_eyedropper(overlap)
	await RenderingServer.frame_post_draw
	var drawn: Image = get_viewport().get_texture().get_image()
	var over_panel: Vector2i = Vector2i(overlap) + Vector2i(11, 0)
	_check(drawn.get_pixelv(over_panel).is_equal_approx(original.get_pixelv(over_panel)),
		"the color panel paints above the magnifier")

	var target: Rect2 = _label.get_element_by_id("eyedropper-target").get_bounding_rect()
	var point: Vector2 = target.get_center().floor() + Vector2(-0.5, 0.5)
	_move_eyedropper(point)
	await RenderingServer.frame_post_draw
	drawn = get_viewport().get_texture().get_image()
	var center: Vector2i = Vector2i(point)
	var picked: Color = original.get_pixelv(center)
	var next: Color = original.get_pixelv(center + Vector2i(1, 0))
	_check(lens.get_global_rect().get_center().is_equal_approx(point),
		"the magnifier follows the mouse and centers the sampled pixel")
	_check(not picked.is_equal_approx(next) and drawn.get_pixelv(center).is_equal_approx(picked) and
		drawn.get_pixelv(center + Vector2i(10, 0)).is_equal_approx(next), "neighboring source pixels appear as separate magnified cells")
	var corner: Vector2i = center + Vector2i(48, 48)
	_check(drawn.get_pixelv(corner).is_equal_approx(original.get_pixelv(corner)),
		"the magnifier clips the raster to a circle")

	var unchanged: String = swatch.value
	_color_picker_key(KEY_ESCAPE)
	_check(not lens.visible and panel.visible and Input.mouse_mode == mouse_mode and swatch.value == unchanged,
		"Escape cancels sampling and restores the previous mouse mode")
	await RenderingServer.frame_post_draw
	_color_picker_click(panel, Vector2(20.0, 160.0))
	_color_picker_click(_label, point)
	_check(not lens.visible and panel.visible and Input.mouse_mode == mouse_mode,
		"sampling restores the mouse without dismissing the color panel")
	_color_picker_key(KEY_ENTER)
	_check(not panel.visible and swatch.value == "#" + picked.to_html(false),
		"Enter applies the center pixel selected by the eyedropper")
	for _frame: int in range(2):
		await get_tree().process_frame
	_check(swatch.show_picker() == OK, "reopens the picker for the visibility cleanup check")
	await RenderingServer.frame_post_draw
	_color_picker_click(panel, Vector2(20.0, 160.0))
	_check(lens.visible, "sampling is active before hiding the label")
	_label.hide()
	_check(not lens.visible and not panel.visible and Input.mouse_mode == mouse_mode,
		"hiding the label closes sampling and restores the mouse mode")
	_label.show()
	for _frame: int in range(2):
		await get_tree().process_frame


func _move_eyedropper(point: Vector2) -> void:
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = point
	get_viewport().push_input(motion, true)


func _check_input_alignment() -> void:
	for _frame: int in range(4):
		await get_tree().process_frame
	for kind: String in ["text", "number", "file"]:
		var bounds: Rect2 = _field("baseline-" + kind).get_bounding_rect()
		var marker: HTMLElement = _label.get_element_by_id("baseline-" + kind + "-marker")
		var baseline: float = marker.get_bounding_rect().end.y
		_check(baseline > bounds.get_center().y and baseline < bounds.end.y - 5.0,
			kind + " shares its centered baseline with adjacent inline text")
	if DisplayServer.get_name() == "headless":
		return
	await RenderingServer.frame_post_draw
	var pixels: Image = get_viewport().get_texture().get_image()
	var input_box: Rect2 = _field("baseline-text").get_bounding_rect()
	var label_box: Rect2 = _label.get_element_by_id("baseline-label").get_bounding_rect()
	var input_ink: Vector2i = _ink_rows(pixels, Rect2(input_box.position, Vector2(14.0, input_box.size.y)))
	var label_ink: Vector2i = _ink_rows(pixels, Rect2(label_box.position, Vector2(14.0, label_box.size.y)))
	_check(input_ink.x >= 0 and input_ink == label_ink,
		"the input glyph and the adjacent label share the same painted rows (%s vs %s)" % [input_ink, label_ink,])


func _ink_rows(pixels: Image, bounds: Rect2) -> Vector2i:
	var rows: Vector2i = Vector2i(-1, -1)
	for y: int in range(floori(bounds.position.y), ceili(bounds.end.y)):
		for x: int in range(floori(bounds.position.x), ceili(bounds.end.x)):
			if pixels.get_pixel(x, y).get_luminance() < 0.25:
				if rows.x < 0:
					rows.x = y
				rows.y = y
				break
	return rows


func _check_clock_picker() -> void:
	# The native picker follows the UI locale, so each clock format needs an explicit fixture locale.
	var original_locale: String = TranslationServer.get_locale()
	TranslationServer.set_locale("en_GB")
	var clock: HTMLInputElement = _field("clock")
	_check(clock.show_picker() == OK, "time input opens its self-drawn list")
	var panel: Control = _label.get_node_or_null(^"_HTMLTimePicker") as Control
	_check(panel != null and panel.visible and panel.size.is_equal_approx(Vector2(116.0, 260.0)),
		"the two-column clock uses Chromium's width and height")
	_check(_label.get_node_or_null(^"_HTMLInputPicker") == null, "time input uses no popup window")
	if panel == null:
		TranslationServer.set_locale(original_locale)
		return
	await RenderingServer.frame_post_draw
	var pixels: Image = get_viewport().get_texture().get_image()
	var pixel: Color = pixels.get_pixelv(Vector2i(panel.get_global_transform_with_canvas() * Vector2(10.0, 12.0)))
	_check(pixel.is_equal_approx(Color("#0075ff")), "the selected row has Chromium's blue highlight")
	_color_picker_key(KEY_RIGHT)
	_color_picker_key(KEY_DOWN)
	_color_picker_key(KEY_ESCAPE)
	_check(panel.visible and clock.value == "08:20", "first Escape restores the original time selection")
	_color_picker_key(KEY_ESCAPE)
	_check(not panel.visible, "second Escape closes the unchanged clock")
	_check(clock.show_picker() == OK, "clock can reopen")
	_color_picker_key(KEY_RIGHT)
	_color_picker_key(KEY_DOWN)
	_color_picker_key(KEY_ENTER)
	_check(not panel.visible and clock.value == "08:35", "step and min offset are respected by the minute column")
	_check(clock.show_picker() == OK, "opens the scrollable clock columns")
	var wheel: InputEventMouseButton = InputEventMouseButton.new()
	wheel.position = panel.get_global_transform_with_canvas() * Vector2(10.0, 16.0)
	wheel.button_index = MOUSE_BUTTON_WHEEL_DOWN
	wheel.pressed = true
	get_viewport().push_input(wheel, true)
	_color_picker_click(panel, Vector2(10.0, 16.0))
	_color_picker_key(KEY_ENTER)
	_check(clock.value == "09:35", "wheel and click select an hour without changing the minute column")
	TranslationServer.set_locale("en_US")
	_check(_field("clock12").show_picker() == OK and panel.size.x == 172.0, "12-hour time exposes an AM/PM column")
	_color_picker_key(KEY_ESCAPE)
	TranslationServer.set_locale("en_GB")
	_check(_field("time").show_picker() == OK and panel.size.x == 228.0, "seconds and milliseconds get independent columns")
	_color_picker_key(KEY_ESCAPE)
	var local: HTMLInputElement = _field("datetime-local")
	_check(local.show_picker() == OK and panel.size.y == 282.0 and panel.size.x > 300.0,
		"local date and time use a combined canvas panel")
	_color_picker_key(KEY_RIGHT)
	_color_picker_key(KEY_ENTER)
	_check(local.value == "2024-04-01T03:12", "the combined picker can change the calendar date while preserving time")
	TranslationServer.set_locale(original_locale)


func _color_picker_click(panel: Control, point: Vector2) -> void:
	for state: bool in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.position = panel.get_global_transform_with_canvas() * point
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = state
		get_viewport().push_input(event, true)


func _color_picker_key(code: Key, character: int = 0) -> void:
	for state: bool in [true, false]:
		var event: InputEventKey = InputEventKey.new()
		event.keycode = code
		event.unicode = character
		event.pressed = state
		get_viewport().push_input(event, true)


func _check_file_picker() -> void:
	print("Pick fixture ", _write_fixture("input-fixture.txt", "sample"))
	var field: HTMLInputElement = _field("file")
	var result: Error = field.show_picker()
	_check(result == OK, "opens a native file picker")
	if result != OK:
		return
	var timeout: SceneTreeTimer = get_tree().create_timer(90.0)
	while field.get_files().is_empty() and timeout.time_left > 0.0:
		await get_tree().process_frame
	var picked: Array[HTMLInputFile] = field.get_files()
	_check(picked.size() == 1, "picker returns the selected file")
	if picked.size() != 1:
		return
	var file: HTMLInputFile = picked[0]
	_check(file.name == "input-fixture.txt" and file.size == 6 and file.read_bytes(1, 3).get_string_from_utf8() == "amp",
		"selected file can be read by range")
	var form: HTMLFormElement = _label.get_element_by_id("form") as HTMLFormElement
	_check(_entry(form.get_form_data(), "upload") == file and not field.get_validity().get("value_missing"),
		"form data carries the selected file")
	_check((field.clone_node() as HTMLInputElement).get_files().is_empty(), "cloning does not copy file access")
	form.reset()
	_check(field.get_files().is_empty() and file.read_bytes(0, 6).get_string_from_utf8() == "sample",
		"form reset clears the selection and retained handles stay readable")


func _check_embedded_file_picker() -> void:
	get_tree().root.gui_embed_subwindows = true
	var window: Window = Window.new()
	window.size = Vector2i(750, 490)
	add_child(window)
	_label.reparent(window, false)
	for _frame: int in range(6):
		await get_tree().process_frame
	var field: HTMLInputElement = _field("file")
	var fixture: String = _write_fixture("input-fixture.txt", "sample")
	var second: String = _write_fixture("second-input.txt", "other")
	_check(field.show_picker() == OK, "opens the embedded file picker")
	var dialog: FileDialog = _label.get_node_or_null(^"_HTMLFilePicker") as FileDialog
	_check(dialog != null and dialog.file_mode == FileDialog.FILE_MODE_OPEN_FILE, "single file picker mode")
	if dialog == null:
		return
	dialog.emit_signal("file_selected", fixture)
	var files: Array[HTMLInputFile] = field.get_files()
	_check(files.size() == 1 and files[0].read_bytes(1, 3).get_string_from_utf8() == "amp", "selected file bytes")
	_check(field.value == "C:\\fakepath\\input-fixture.txt", "file value exposes only a fake path")
	field.set_attribute("multiple", "")
	for _frame: int in range(4):
		await get_tree().process_frame
	_check(field.show_picker() == OK, "opens a second embedded picker")
	dialog = _label.get_node_or_null(^"_HTMLFilePicker") as FileDialog
	_check(dialog != null and dialog.file_mode == FileDialog.FILE_MODE_OPEN_FILES, "multiple file picker mode")
	if dialog == null:
		return
	dialog.emit_signal("files_selected", PackedStringArray([fixture, second,]))
	files = field.get_files()
	_check(files.size() == 2 and files[1].name == "second-input.txt", "multiple files preserve selection order")
	var form: HTMLFormElement = _label.get_element_by_id("form") as HTMLFormElement
	var entries: Array[HTMLInputFile]
	for entry: Dictionary in form.get_form_data():
		if entry.get("name") == "upload":
			entries.append(entry.get("value") as HTMLInputFile)
	_check(entries == files, "form data preserves all selected file handles")
	_check((field.clone_node() as HTMLInputElement).get_files().is_empty(), "cloning does not copy file access")
	await get_tree().process_frame
	_check(field.show_picker() == OK, "reopens a file picker")
	dialog = _label.get_node_or_null(^"_HTMLFilePicker") as FileDialog
	if dialog != null:
		dialog.emit_signal("canceled")
	_check(field.get_files() == files, "cancel preserves the selected file handles")
	await get_tree().process_frame
	_check(field.show_picker() == OK, "opens a picker before the type is changed")
	dialog = _label.get_node_or_null(^"_HTMLFilePicker") as FileDialog
	if dialog != null:
		field.type = "text"
		field.type = "file"
		dialog.emit_signal("files_selected", PackedStringArray([fixture,]))
	_check(field.get_files().is_empty(), "a type change discards a stale file picker result")
	_check(not files.is_empty() and files[0].read_bytes(0, 6).get_string_from_utf8() == "sample",
		"retained file handles remain readable")


func _write_fixture(name: String, contents: String) -> String:
	var path: String = "user://" + name
	var output: FileAccess = FileAccess.open(path, FileAccess.WRITE)
	output.store_string(contents)
	output.close()
	return ProjectSettings.globalize_path(path)


func _field(id: String) -> HTMLInputElement:
	return _label.get_element_by_id(id) as HTMLInputElement


func _entry(entries: Array[Dictionary], name: String) -> Variant:
	for entry: Dictionary in entries:
		if entry.get("name") == name:
			return entry.get("value")
	return null


func _check(condition: bool, message: String) -> void:
	if condition:
		return
	_failures += 1
	push_error(message)
