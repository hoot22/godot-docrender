extends Control


class GodotTextSource extends Control:
	var text_value: String = "From Godot"


	func _draw() -> void:
		draw_rect(Rect2(Vector2.ZERO, size), Color("#4c7799"))
		draw_string(ThemeDB.fallback_font, Vector2(4.0, 28.0), text_value, HORIZONTAL_ALIGNMENT_LEFT, -1.0, 12, Color.WHITE)


	func _get_drag_data(_at_position: Vector2) -> Variant:
		var preview: Label = Label.new()
		preview.text = text_value
		set_drag_preview(preview)
		return text_value


var html_label: HTMLLabel
var line_edit: LineEdit
var status: Label
var source: GodotTextSource


func _ready() -> void:
	html_label = HTMLLabel.new()
	html_label.position = Vector2(16.0, 16.0)
	html_label.size = Vector2(300.0, 145.0)
	html_label.text = """<p style="font-family:Arial;font-size:22px;color:#1248aa">Drag text</p><div id="drag-source" draggable="true" style="margin-top:8px;padding:8px;background:#cceeff">Drag this HTML element</div><div id="dropzone" style="margin-top:8px;padding:8px;background:#dddddd">HTML drop zone</div>"""
	add_child(html_label)

	line_edit = LineEdit.new()
	line_edit.position = Vector2(16.0, 190.0)
	line_edit.size = Vector2(300.0, 36.0)
	line_edit.placeholder_text = "Drop selected HTML text here"
	add_child(line_edit)

	source = GodotTextSource.new()
	source.position = Vector2(370.0, 64.0)
	source.size = Vector2(90.0, 48.0)
	add_child(source)

	status = Label.new()
	status.position = Vector2(16.0, 232.0)
	status.size = Vector2(440.0, 24.0)
	status.text = "Drag selected text or the blue HTML element to LineEdit; drag From Godot into the gray area"
	add_child(status)

	await get_tree().process_frame
	html_label.set_selection_range(0, 4)
	var document: HTMLDocumentNode = html_label.get_document_node()
	document.add_event_listener(&"dragstart", _on_html_drag_start)
	document.add_event_listener(&"dragend", _on_html_drag_end)
	document.add_event_listener(&"dragover", _on_html_drag_over)
	document.add_event_listener(&"drop", _on_html_drop)
	print("HTML drag-and-drop manual fixture ready")


func _on_html_drag_start(event: HTMLDragEvent) -> void:
	event.data_transfer.set_data("application/x-dockit-demo", "custom HTML drag payload")
	status.text = "HTML dragstart: " + event.data_transfer.get_data("text/plain")


func _on_html_drag_end(_event: HTMLDragEvent) -> void:
	status.text = "HTML dragend"


func _on_html_drag_over(event: HTMLDragEvent) -> void:
	event.prevent_default()
	status.text = "HTML dragover: " + str(event.data_transfer.types)


func _on_html_drop(event: HTMLDragEvent) -> void:
	status.text = "HTML drop: " + event.data_transfer.get_data("text/plain") + " / " + event.data_transfer.get_data("application/x-dockit-demo")
