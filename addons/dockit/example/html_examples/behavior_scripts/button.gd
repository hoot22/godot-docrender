extends HTMLBehavior

func _on_click() -> void:
	var p: HTMLElement = document.query_selector("p")
	if p:
		p.set_inner_text("Clicked on %s" % Time.get_datetime_string_from_system())
