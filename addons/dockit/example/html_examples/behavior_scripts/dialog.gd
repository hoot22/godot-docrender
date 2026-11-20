extends HTMLBehavior


var update_button: HTMLButtonElement
var fav_dialog: HTMLDialogElement
var output_box: HTMLOutputElement
var select_element: HTMLSelectElement
var confirm_button: HTMLButtonElement


func _attach(_label: HTMLLabel) -> void:
	update_button = document.get_element_by_id("updateDetails")
	fav_dialog = document.get_element_by_id("favDialog")
	output_box = document.query_selector("output")
	select_element = document.query_selector("select")
	confirm_button = document.query_selector("#confirmBtn")

	fav_dialog.hidden = true
	
	update_button.add_event_listener(&"click", _on_update_button_clicked)
	select_element.add_event_listener(&"change", _on_select_element_changed)
	fav_dialog.add_event_listener(&"close", _on_fav_dialog_closed)


func _on_update_button_clicked(event: HTMLEvent) -> void:
	var show_error: int = fav_dialog.show_modal()
	if show_error != OK:
		output_box.value = "Could not open the dialog: %s" % error_string(show_error)
	
	
func _on_select_element_changed(event: HTMLEvent) -> void:
	confirm_button.value = select_element.value
	

func _on_fav_dialog_closed(event: HTMLEvent) -> void:
	output_box.value = "%s button clicked - %s" % [fav_dialog.return_value, Time.get_datetime_string_from_system()]
