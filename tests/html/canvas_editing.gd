extends Node

var failures: int
@onready var label: HTMLLabel = $Label


func _ready() -> void:
	label.size = Vector2(420.0, 240.0)
	label.text = """<style>body { margin: 0; } canvas { display: block; background: lime; }</style>
<canvas id="paint" width="18" height="14" style="width: 18px; height: 14px"></canvas>
<div id="editable" contenteditable="true">hello</div>"""
	await _settle()
	_check_canvas()
	await _check_editable_dom()
	await _check_keyboard_edit()
	_check_data_transfer()
	if failures == 0:
		print("HTML canvas and editing checks passed")
	get_tree().quit(0 if failures == 0 else 1)


func _check_canvas() -> void:
	var canvas: HTMLCanvasElement = label.get_element_by_id("paint") as HTMLCanvasElement
	_check(canvas != null, "canvas elements have a typed GDScript handle")
	if canvas == null:
		return
	_check(canvas.width == 18 and canvas.height == 14, "intrinsic dimensions come from width and height")
	var context: HTMLCanvasRenderingContext2D = canvas.get_context("2d")
	_check(context != null and canvas.get_context("webgl") == null, "the 2D context is cached and unsupported contexts return null")
	if context == null:
		return
	context.fill_style = "#ff0000"
	context.fill_rect(2.0, 3.0, 5.0, 4.0)
	var image: Image = canvas.get_image()
	_check(image.get_width() == 18 and image.get_height() == 14, "the backing image keeps intrinsic pixel size")
	_check(image.get_pixel(3, 4).is_equal_approx(Color.RED), "fill_rect paints the requested pixels")
	_check(is_zero_approx(image.get_pixel(0, 0).a), "canvas pixels start transparent")
	context.save()
	context.translate(1.0, 0.0)
	context.fill_style = Color.BLUE
	context.fill_rect(8.0, 2.0, 2.0, 2.0)
	context.restore()
	_check(context.get_image().get_pixel(9, 2).is_equal_approx(Color.BLUE), "state save and translate affect drawing")
	var export_png: String = canvas.to_data_url("image/png")
	var fallback: String = canvas.to_data_url("image/unknown")
	_check(export_png.begins_with("data:image/png;base64,") and fallback.begins_with("data:image/png;base64,"),
		"PNG export and unsupported MIME fallback return data URLs")
	canvas.width = 20
	_check(canvas.width == 20 and is_zero_approx(context.get_image().get_pixel(3, 4).a), "changing intrinsic width clears pixels and context state")
	canvas.width = 80
	canvas.height = 32
	var gradient: HTMLCanvasGradient = context.create_linear_gradient(0.0, 0.0, 8.0, 0.0)
	_check(gradient.add_color_stop(0.0, Color.RED) == OK and gradient.add_color_stop(1.0, Color.BLUE) == OK,
		"linear gradients accept ordered color stops")
	context.fill_style = gradient
	context.fill_rect(0.0, 0.0, 8.0, 4.0)
	var gradient_image: Image = context.get_image()
	_check(gradient_image.get_pixel(0, 1).r > gradient_image.get_pixel(0, 1).b and
		gradient_image.get_pixel(7, 1).b > gradient_image.get_pixel(7, 1).r, "linear gradient styles interpolate across fill pixels")
	var tile: Image = Image.create(2, 2, false, Image.FORMAT_RGBA8)
	tile.fill(Color.MAGENTA)
	var pattern: HTMLCanvasPattern = context.create_pattern(tile, "repeat")
	context.fill_style = pattern
	context.fill_rect(10.0, 0.0, 4.0, 4.0)
	_check(context.get_image().get_pixel(11, 1).is_equal_approx(Color.MAGENTA), "image patterns repeat across filled areas")
	context.save()
	context.translate(40.0, 0.0)
	context.draw_image(tile, 0.0, 0.0)
	context.restore()
	_check(context.get_image().get_pixel(40, 0).is_equal_approx(Color.MAGENTA), "draw_image applies the current transform")
	context.fill_style = Color.RED
	context.fill_rect(20.0, 10.0, 8.0, 8.0)
	context.save()
	context.translate(24.0, 14.0)
	context.rotate(PI / 4.0)
	context.clear_rect(-2.0, -2.0, 4.0, 4.0)
	context.restore()
	_check(is_zero_approx(context.get_image().get_pixel(24, 14).a) and
		context.get_image().get_pixel(22, 12).a > 0.0, "clear_rect respects the transformed quadrilateral")
	context.clear_rect(0.0, 0.0, 80.0, 32.0)
	context.fill_style = Color.WHITE
	context.font_size = 16
	context.fill_text("A", 2.0, 20.0)
	var text_image: Image = context.get_image()
	var found_glyph: bool
	for y in text_image.get_height():
		for x in text_image.get_width():
			if text_image.get_pixel(x, y).a > 0.0:
				found_glyph = true
				break
		if found_glyph:
			break
	_check(found_glyph, "fill_text rasterizes through the active Godot Font into canvas pixels")
	context.fill_style = Color.GREEN
	context.fill_rect(1.0, 1.0, 3.0, 3.0)
	await _settle()
	var canvas_box: Rect2 = canvas.get_bounding_rect()
	_check(canvas_box.size.x >= 18.0 and canvas_box.size.y >= 14.0,
		"canvas backing pixels flow through the element's CSS replaced-element box")


func _check_editable_dom() -> void:
	var document: HTMLDocumentNode = label.get_document_node()
	var editor: HTMLElement = label.get_element_by_id("editable")
	_check(editor != null and editor.is_content_editable(), "contenteditable true is exposed as an inherited DOM property")
	_check(document != null and document.design_mode == "off", "designMode defaults to off")
	if document != null:
		document.design_mode = "on"
		_check(document.body.is_content_editable(), "designMode makes body editable")
		var body_focus: bool = label.focus_element(document.body)
		_check(body_focus and document.active_element == document.body,
			"designMode refreshes host focus candidates for the editable body")
		document.design_mode = "off"
		await _settle()
	var focused: bool = label.focus_element(editor)
	_check(focused and document.active_element == editor, "focused editable elements become document.activeElement")
	var range: HTMLRange = label.create_range()
	range.select_node_contents(editor)
	var selection: HTMLSelection = label.get_selection()
	selection.remove_all_ranges()
	selection.add_range(range)
	_check(label.execute_edit_command("bold") and editor.get_inner_html().contains("<strong>hello</strong>"),
		"execute_edit_command wraps selected editable content")


func _check_keyboard_edit() -> void:
	var editor: HTMLElement = label.get_element_by_id("editable")
	if editor == null:
		return
	var strong: HTMLElement = editor.query_selector("strong")
	var text: HTMLTextNode = strong.get_child_nodes()[0] as HTMLTextNode if strong != null else null
	if text == null:
		return
	var selection: HTMLSelection = label.get_selection()
	selection.collapse(text, 2)
	label.focus_element(editor)
	var key: InputEventKey = InputEventKey.new()
	key.pressed = true
	key.keycode = KEY_X
	key.unicode = 120
	Input.parse_input_event(key)
	await get_tree().process_frame
	await get_tree().process_frame
	_check(editor.get_text_content() == "hexllo", "keyboard text input inserts at the contenteditable DOM caret")


func _check_data_transfer() -> void:
	var transfer: HTMLDataTransfer = HTMLDataTransfer.new()
	_check(transfer.set_data("text/plain", "plain") == OK and transfer.set_data("text/html", "<b>rich</b>") == OK,
		"DataTransfer accepts plain text and HTML")
	var types: PackedStringArray = transfer.types
	_check(transfer.get_data("text/plain") == "plain" and transfer.get_data("text/html") == "<b>rich</b>"
		and types.has("text/plain") and types.has("text/html"), "DataTransfer exposes both MIME payloads")
	transfer.effect_allowed = "copyMove"
	transfer.drop_effect = "copy"
	_check(transfer.effect_allowed == "copyMove" and transfer.drop_effect == "copy", "drag effects are script-visible")


func _settle() -> void:
	for _frame in 3:
		await get_tree().process_frame


func _check(condition: bool, description: String) -> void:
	if condition:
		return
	failures += 1
	push_error(description)
