extends HTMLComponent

var _button: HTMLElement
var _title: HTMLElement


func _mount() -> void:
	_button = shadow_root.query_selector("button")
	_title = host.query_selector("[slot='title']")
	if _button != null:
		_button.add_event_listener(&"click", _on_button_clicked)


func _unmount() -> void:
	if _button != null and _button.is_valid():
		_button.remove_event_listener(&"click", _on_button_clicked)


func _on_button_clicked(_event: HTMLEvent) -> void:
	if _title != null and _title.is_valid():
		_title.set_text_content("Updated from GDScript")
