extends HTMLBehavior


func _attach(label: HTMLLabel) -> void:
	var p: HTMLElement = label.create_element("p")
	var root: HTMLHtmlElement = document.query_selector("html")
	root.append_child(p)
	p.set_inner_text("Script attached.")
	
