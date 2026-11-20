extends HTMLBehavior


var a: HTMLInputElement
var b: HTMLInputElement
var result: HTMLOutputElement


func _attach(_label: HTMLLabel) -> void:
	a = document.get_element_by_id("a")
	b = document.get_element_by_id("b")
	result = document.get_element_by_id("result")
	
	a.add_event_listener("input", _update_result)
	b.add_event_listener("input", _update_result)
	
	_update_result(null)
	

func _update_result(event: HTMLEvent) -> void:
	var a_value: float = a.value_as_number
	var b_value: float = b.value_as_number
	result.value = str(a_value + b_value)
