extends SceneTree

const SOURCE: String = """
<!doctype html>
<style>
	html, body { margin: 0; background: white; }
	#outer { width: 168px; height: 118px; border: 0; padding: 6px; background: #f00000; }
	#outer::backdrop { background: #2040c0; }
	#inner { width: 108px; height: 54px; border: 0; padding: 6px; background: #f0c000; }
	#inner::backdrop { background: linear-gradient(90deg, #008000, #008000); opacity: 0.75; }
	#clipped { overflow: hidden; width: 5px; height: 5px; transform: translateX(2px); }
	#spacer { height: 1000px; }
</style>
<button id="opener" type="button" style="position:fixed;inset:0 auto auto 0">Open</button>
<div id="clipped"><dialog id="outer"><input id="edit" autofocus><button id="tail" type="button">Tail</button>
	<select id="choice" tabindex="-1"><option>Alpha</option><option>Beta</option></select>
	<span>Outer text</span></dialog></div>
<dialog id="inner"><button id="confirm" type="button">Confirm</button></dialog>
<dialog id="markup" open><button id="regular" type="button">Regular</button></dialog>
<div id="spacer">Background text</div>
"""
const FORM_SOURCE: String = """
<!doctype html>
<button id="form-opener" type="button">Open</button>
<dialog id="outer-form-dialog">
	<form id="outer-form" method="DiAlOg">
		<input id="required-field" name="title" required autofocus>
		<button id="save" name="decision" value="save">Save</button>
		<input id="cancel" type="submit" value="" formnovalidate>
		<button id="normal-submit" formmethod="get" formnovalidate>Submit normally</button>
	</form>
	<form id="implicit-form" method="dialog"><input id="implicit-field"></form>
	<dialog id="inner-form-dialog">
		<form id="inner-form" method="post">
			<button id="confirm-submit" name="decision" value="confirm" formmethod="DIALOG">Confirm</button>
		</form>
	</dialog>
</dialog>
<button id="external-submit" form="outer-form" formnovalidate>External submit</button>
<form id="no-dialog" method="dialog"><button id="no-dialog-submit" value="unused">Submit</button></form>
"""
const CLOSE_REQUEST_SOURCE: String = """
<!doctype html>
<style>
	html, body { margin: 0; }
	dialog { width: 110px; height: 60px; }
</style>
<button id="request-opener" type="button">Open</button>
<dialog id="request-outer"><button id="request-outer-focus" type="button">Outer</button></dialog>
<dialog id="request-inner" closedby="AnY"><button id="request-inner-focus" type="button">Inner</button></dialog>
"""

var failures: int
var frame: SubViewport
var label: HTMLLabel
var closed_values: Array[String] = []
var restored_focus: Array[HTMLElement] = []
var request_dialogs: Array[HTMLDialogElement]
var request_reasons: Array[int]
var last_request: HTMLDialogCloseRequest
var veto_close_request: bool


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	frame = SubViewport.new()
	frame.size = Vector2i(360, 260)
	frame.gui_disable_input = false
	frame.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(frame)
	var native: Button = Button.new()
	native.position = Vector2(0, 220)
	native.size = Vector2(42, 32)
	frame.add_child(native)
	label = HTMLLabel.new()
	label.size = Vector2(320, 200)
	label.text = SOURCE
	frame.add_child(label)
	await _settle()

	var outer: HTMLDialogElement = label.get_element_by_id("outer") as HTMLDialogElement
	var inner: HTMLDialogElement = label.get_element_by_id("inner") as HTMLDialogElement
	var regular: HTMLDialogElement = label.get_element_by_id("markup") as HTMLDialogElement
	var opener: HTMLElement = label.get_element_by_id("opener")
	var edit: HTMLElement = label.get_element_by_id("edit")
	var tail: HTMLElement = label.get_element_by_id("tail")
	var confirm: HTMLElement = label.get_element_by_id("confirm")
	_check(outer != null and inner != null and regular != null, "dialog handle factory")
	if failures:
		quit(1)
		return
	regular.open = false
	_check(outer.get_bounding_rect() == Rect2(), "closed dialog has no box")
	_check(regular.show() == OK and regular.matches(":open") and not regular.is_modal(), "regular show")
	_check(regular.show_modal() == ERR_INVALID_PARAMETER, "regular dialog rejects modal reopen")
	regular.open = false
	var detached: HTMLDialogElement = outer.clone_node(true) as HTMLDialogElement
	_check(detached.show_modal() == ERR_INVALID_PARAMETER, "detached modal rejected")

	native.grab_focus()
	label.set_scroll_offset(Vector2i(0, 270))
	await _settle()
	var size_before: Vector2i = label.get_content_size()
	outer.closed.connect(_on_closed)
	inner.closed.connect(_on_closed)
	_check(outer.show_modal() == OK and outer.show_modal() == OK, "modal show is idempotent")
	_check(outer.show() == ERR_INVALID_PARAMETER and outer.matches(":modal"), "modal state and mode guard")
	await _settle()
	_check(label.get_focused_element() == edit, "autofocus inside transformed and clipped ancestor")
	_check(label.get_content_size() == size_before and label.get_scroll_offset().y == 270, "modal preserves document size and scroll")
	var modal_center: Vector2 = outer.get_bounding_rect().get_center() - Vector2(label.get_scroll_offset())
	_check(modal_center.distance_to(Vector2(154, 100)) < 8.0, "viewport-centered modal at document scroll offset")
	_check(label.get_element_at(Vector2(7, 8)) == null, "background is excluded from hit testing")
	_check(not label.activate_element(opener) and not label.focus_element(opener), "background script interaction blocked")
	_check(_pixel(Vector2i(8, 10)).is_equal_approx(Color("#2040c0")), "backdrop paints over background")
	_check(not label.get_computed_style_property(outer, "background-color", "::backdrop").is_empty(),
		"backdrop has a computed style")
	_check(_pixel(Vector2i(125, 130)).r > 0.8, "modal paints above its backdrop")
	var wheel: InputEventMouseButton = InputEventMouseButton.new()
	wheel.button_index = MOUSE_BUTTON_WHEEL_DOWN
	wheel.position = Vector2(8, 10)
	wheel.pressed = true
	frame.push_input(wheel, true)
	await _settle()
	_check(label.get_scroll_offset().y == 270, "wheel over backdrop cannot scroll the document")
	label.select_all()
	_check("Outer text" in label.get_selected_text() and "Background text" not in label.get_selected_text(),
		"select all is scoped to modal text")
	label.clear_selection()
	label.set_scroll_offset(Vector2i(0, 310))
	await _settle()
	_check((outer.get_bounding_rect().get_center() - Vector2(label.get_scroll_offset())).is_equal_approx(modal_center),
		"modal stays fixed after viewport scroll")
	_check(label.get_element_at(edit.get_bounding_rect().get_center() - Vector2(label.get_scroll_offset())) == edit,
		"hit testing follows the fixed modal after scroll")
	_key(KEY_TAB)
	_check(label.get_focused_element() == tail, "Tab advances in dialog")
	_key(KEY_TAB)
	_check(label.get_focused_element() == edit, "Tab wraps in dialog")
	_key(KEY_TAB, true)
	_check(label.get_focused_element() == tail, "Shift+Tab wraps backwards")
	var choice: HTMLElement = label.get_element_by_id("choice")
	_check(label.focus_element(choice), "script can focus a negative-tabindex control in the modal")
	_key(KEY_SPACE)
	_key(KEY_ESCAPE, false, true)
	await _settle()
	_check(outer.open and label.get_focused_element() == choice, "Escape dismisses an open select menu before the dialog")
	_check(label.focus_element(tail), "restore a known opener for the nested dialog")

	inner.return_value = "kept"
	_check(inner.show_modal() == OK, "nested modal opens")
	await _settle()
	_check(label.get_focused_element() == confirm, "top dialog gains focus")
	_check(not label.focus_element(edit), "lower modal is excluded from focus")
	_check(_pixel(Vector2i(8, 10)).g > _pixel(Vector2i(8, 10)).b, "top gradient backdrop composites above lower backdrop")
	_key(KEY_ESCAPE, false, true)
	await _settle()
	_check(not inner.open and outer.open and label.get_focused_element() == tail, "Escape closes only top dialog and restores its opener")
	_check(closed_values == ["kept"] and restored_focus == [tail], "closed signal follows focus restoration")
	_key(KEY_ESCAPE)
	await _settle()
	_check(not outer.open and frame.gui_get_focus_owner() == native, "closing final modal restores external Godot focus")

	_check(label.focus_element(opener), "background focus is available again")
	_check(outer.show_modal() == OK, "reopen after dismissal")
	await _settle()
	_check(inner.show_modal() == OK, "reopen nested modal")
	await _settle()
	outer.open = false
	await _settle()
	_check(not outer.is_modal() and inner.is_modal() and label.get_focused_element() == confirm,
		"removing a covered modal does not steal focus")
	_check(inner.close("") == OK and inner.close("ignored") == OK, "close is idempotent")
	await _settle()
	_check(label.get_focused_element() == opener and closed_values == ["kept", "", ""], "covered removal preserves the original restore point")
	_check(outer.show_modal() == OK, "open before DOM removal")
	await _settle()
	_check(outer.detach() == OK and not outer.is_modal(), "detaching clears the modal layer")
	await _settle()
	_check(label.get_focused_element() == opener, "detaching a modal restores focus")
	var standalone: HTMLLayout = HTMLLayout.new()
	_check(standalone.parse("<dialog id='empty'></dialog>") == OK, "standalone layout parses a dialog")
	var empty: HTMLDialogElement = standalone.get_element_by_id("empty") as HTMLDialogElement
	empty.return_value = "adopted"
	_check(empty.show_modal() == OK and standalone.layout(Vector2i(160, 100)) == OK,
		"standalone layout can open and lay out a modal")
	var clone: HTMLDialogElement = empty.clone_node(true) as HTMLDialogElement
	_check(clone.open and not clone.is_modal() and clone.return_value.is_empty(), "cloning copies the open attribute")
	var destination: HTMLLayout = HTMLLayout.new()
	_check(destination.parse("<main></main>") == OK and destination.adopt_node(empty) == empty, "adopt modal into another layout")
	_check(empty.open and not empty.is_modal() and not empty.is_connected() and empty.return_value == "adopted",
		"adoption removes modal membership and keeps the handle and result")
	label.text = "<button>Replacement</button>"
	await _settle()
	_check(not outer.is_valid() and not inner.is_valid(), "reparse invalidates previous dialog handles")
	await _check_dialog_forms()
	await _check_close_requests()
	if failures == 0:
		print("HTML dialog checks passed")
	quit(0 if failures == 0 else 1)


func _check_dialog_forms() -> void:
	label.text = FORM_SOURCE
	label.set_scroll_offset(Vector2i.ZERO)
	closed_values.clear()
	restored_focus.clear()
	await _settle()
	var outer: HTMLDialogElement = label.get_element_by_id("outer-form-dialog") as HTMLDialogElement
	var inner: HTMLDialogElement = label.get_element_by_id("inner-form-dialog") as HTMLDialogElement
	var outer_form: HTMLFormElement = label.get_element_by_id("outer-form") as HTMLFormElement
	var opener: HTMLElement = label.get_element_by_id("form-opener")
	var field: HTMLInputElement = label.get_element_by_id("required-field") as HTMLInputElement
	var save: HTMLElement = label.get_element_by_id("save")
	var cancel: HTMLElement = label.get_element_by_id("cancel")
	var confirm: HTMLElement = label.get_element_by_id("confirm-submit")
	var submitted: Array[HTMLElement]
	var validation_failed: Array[HTMLElement]
	var submitted_entries: Array[Array]
	var closed_count_at_submit: Array[int]
	var form_close_requests: Array[HTMLDialogCloseRequest]
	label.form_submitted.connect(func(_form: HTMLFormElement, entries: Array[Dictionary], submitter: HTMLElement) -> void:
		submitted.append(submitter)
		submitted_entries.append(entries)
		closed_count_at_submit.append(closed_values.size()))
	label.form_validation_failed.connect(func(_form: HTMLFormElement, control: HTMLElement) -> void:
		validation_failed.append(control))
	outer.close_requested.connect(func(request: HTMLDialogCloseRequest) -> void:
		form_close_requests.append(request)
		request.prevent_default())
	inner.close_requested.connect(func(request: HTMLDialogCloseRequest) -> void:
		form_close_requests.append(request)
		request.prevent_default())
	outer.closed.connect(_on_closed)
	inner.closed.connect(_on_closed)
	outer.return_value = "original"
	_check(label.focus_element(opener) and outer.show_modal() == OK, "open dialog form from a document control")
	await _settle()
	_check(label.get_focused_element() == field, "form autofocus uses the text field")
	_key(KEY_ENTER)
	await _settle()
	_check(outer.open and outer.return_value == "original" and validation_failed == [field] and submitted.is_empty(),
		"Enter validates the default submitter and leaves the dialog open on failure")

	var normal: HTMLElement = label.get_element_by_id("normal-submit")
	_check(label.submit_form(outer_form, normal) == OK and outer.open and submitted == [normal],
		"formmethod allows a normal submission inside a dialog form")
	_check(inner.show_modal() == OK, "open a dialog nested under the outer dialog")
	await _settle()
	_click(confirm)
	await _settle()
	_check(not inner.open and inner.return_value == "confirm" and outer.open and outer.return_value == "original",
		"submitter formmethod closes only the nearest ancestor dialog")
	_check(label.get_focused_element() == field and restored_focus == [field] and closed_values == ["confirm"],
		"the nested submission restores focus before the closed signal")
	_check(submitted == [normal, confirm] and submitted_entries.back() == [{"name": "decision", "value": "confirm"}],
		"dialog form submission preserves the submitter and ordered entries")
	_check(closed_count_at_submit.back() == 0, "form_submitted arrives before the dialog closed signal")

	_check(label.focus_element(cancel), "focus the formnovalidate submit input")
	_key(KEY_SPACE)
	await _settle()
	_check(not outer.open and outer.return_value.is_empty() and validation_failed == [field] and submitted.back() == cancel,
		"formnovalidate bypasses the empty required field and an explicit empty value clears the result")
	_check(label.get_focused_element() == opener and closed_values == ["confirm", ""], "form submission restores the original opener")

	_check(outer.show_modal() == OK, "reopen for a valid implicit submission")
	await _settle()
	field.value = "Ada"
	_key(KEY_ENTER)
	await _settle()
	_check(not outer.open and outer.return_value == "save" and submitted.back() == save,
		"Enter submits using the first submit button and its value")
	_check(submitted_entries.back() == [
		{"name": "title", "value": "Ada"},
		{"name": "decision", "value": "save"},
	],
		"Enter preserves the ordered field and submitter entries")

	_check(outer.show_modal() == OK, "reopen for implicit submission without a button")
	await _settle()
	_check(label.focus_element(label.get_element_by_id("implicit-field")), "focus the form with no submit button")
	_key(KEY_ENTER)
	await _settle()
	_check(not outer.open and outer.return_value == "save" and submitted.back() == null,
		"implicit submission without a button preserves the previous result")

	_check(outer.show() == OK, "open a non-modal dialog for an externally associated submitter")
	await _settle()
	var external: HTMLElement = label.get_element_by_id("external-submit")
	_check(label.submit_form(outer_form, external) == OK and not outer.open and outer.return_value == "save",
		"an external submitter closes the form's dialog and an omitted value preserves its result")
	var no_dialog: HTMLFormElement = label.get_element_by_id("no-dialog") as HTMLFormElement
	var no_dialog_submit: HTMLElement = label.get_element_by_id("no-dialog-submit")
	_check(label.submit_form(no_dialog, no_dialog_submit) == OK and submitted.back() == no_dialog_submit and outer.return_value == "save",
		"a dialog form without an ancestor still emits the ordinary form notification")
	_check(form_close_requests.is_empty(), "method dialog submits directly without emitting close_requested")
	await _settle()


func _check_close_requests() -> void:
	label.text = CLOSE_REQUEST_SOURCE
	closed_values.clear()
	restored_focus.clear()
	await _settle()
	var outer: HTMLDialogElement = label.get_element_by_id("request-outer") as HTMLDialogElement
	var inner: HTMLDialogElement = label.get_element_by_id("request-inner") as HTMLDialogElement
	var opener: HTMLElement = label.get_element_by_id("request-opener")
	var outer_focus: HTMLElement = label.get_element_by_id("request-outer-focus")
	var inner_focus: HTMLElement = label.get_element_by_id("request-inner-focus")
	var outside: Vector2 = Vector2(8.0, 12.0)
	outer.closed.connect(_on_closed)
	inner.closed.connect(_on_closed)
	outer.close_requested.connect(_on_close_requested.bind(outer))
	inner.close_requested.connect(_on_close_requested.bind(inner))
	_check(outer.closed_by == "none" and inner.closed_by == "any", "closedby normalizes keywords and defaults to none outside modal")
	_check(outer.request_close("unused") == OK and request_reasons.is_empty(), "request on a closed dialog is a no-op")
	_check(label.focus_element(opener) and outer.show_modal() == OK, "open the modal from its original opener")
	await _settle()
	_check(outer.closed_by == "closerequest", "modal closedby Auto allows Escape")
	outer.closed_by = "invalid"
	_check(outer.closed_by == "closerequest" and outer.get_attribute("closedby") == "invalid",
		"invalid closedby uses the modal default while preserving the raw attribute")
	await _settle()
	_pointer(outside, true)
	_pointer(outside, false)
	_check(outer.open and request_reasons.is_empty(), "default modal backdrop click emits no request")
	veto_close_request = true
	_key(KEY_ESCAPE, false, true)
	await _settle()
	_check(request_reasons == [HTMLDialogCloseRequest.ESCAPE] and request_dialogs == [outer] and outer.open,
		"Escape emits only one vetoable request per key press")
	_check(last_request.is_default_prevented() and last_request.return_value == null and label.get_focused_element() == outer_focus,
		"Escape veto preserves focus and the optional result")

	outer.closed_by = "any"
	_check(inner.show_modal() == OK, "open another modal above the first")
	await _settle()
	inner.closed_by = "none"
	await _settle()
	_key(KEY_ESCAPE)
	_pointer(outside, true)
	_pointer(outside, false)
	_check(request_dialogs == [outer] and inner.open and outer.open, "the top dialog's none policy does not dismiss the lower modal")
	inner.closed_by = "any"
	inner.set_attribute("style", "pointer-events: none")
	await _settle()
	var inside: Vector2 = inner.get_bounding_rect().get_center()
	_pointer(inside, true)
	_pointer(inside, false)
	_pointer(inside, true)
	_pointer(outside, false)
	_pointer(outside, true)
	_pointer(inside, false)
	_pointer(outside, true)
	_pointer(outside + Vector2(25.0, 0.0), false)
	_check(request_dialogs == [outer] and inner.open, "interior presses and drags cannot dismiss through the backdrop")
	inner.return_value = "original"
	_pointer(outside, true)
	_pointer(outside, false)
	_check(request_reasons.back() == HTMLDialogCloseRequest.BACKDROP and request_dialogs.back() == inner and inner.open
		and inner.return_value == "original" and label.get_focused_element() == inner_focus, "backdrop requests can be vetoed on the top dialog")

	_pointer(outside, true)
	_check(inner.close() == OK, "close the active dialog while the pointer is held")
	await _settle()
	_pointer(outside, false)
	_check(request_dialogs == [outer, inner] and outer.open, "a release after the modal stack changed cannot dismiss the next dialog")
	_check(inner.show_modal() == OK, "reopen the inner modal")
	await _settle()
	veto_close_request = false
	_pointer(outside, true)
	_pointer(outside, false)
	await _settle()
	_check(not inner.open and outer.open and request_dialogs == [outer, inner, inner], "accepted backdrop request closes only the top dialog")
	_check(restored_focus.back() == outer_focus and label.get_focused_element() == outer_focus, "backdrop dismissal restores the lower dialog focus")

	outer.closed_by = "none"
	outer.return_value = "keep"
	veto_close_request = true
	_check(outer.request_close("ignored") == ERR_SKIP and outer.open and outer.return_value == "keep",
		"script request emits even for closedby none and allows a veto")
	_check(last_request.reason == HTMLDialogCloseRequest.SCRIPT and last_request.return_value == "ignored",
		"script request exposes its proposed result")
	veto_close_request = false
	outer.close_requested.connect(_reopen_on_close_request.bind(outer), CONNECT_ONE_SHOT)
	_check(outer.request_close("stale") == OK and outer.open and outer.return_value == "keep",
		"a listener can close and reopen without the old request closing the new instance")
	_check(outer.request_close("") == OK and not outer.open and outer.return_value.is_empty(), "accepted script request applies an explicit empty result")
	last_request.prevent_default()
	_check(not last_request.is_default_prevented(), "a retained request cannot veto after synchronous dispatch")
	await _settle()
	_check(label.get_focused_element() == opener, "script dismissal restores the original opener")

	label.text = """
		<!doctype html><button id="normal-opener" type="button">Outside</button>
		<dialog id="normal-first" open closedby="any"><p>First</p></dialog>
		<dialog id="normal-last" open closedby="closerequest"><p>Last</p></dialog>
	"""
	await _settle()
	var first: HTMLDialogElement = label.get_element_by_id("normal-first") as HTMLDialogElement
	var last: HTMLDialogElement = label.get_element_by_id("normal-last") as HTMLDialogElement
	var normal_opener: HTMLElement = label.get_element_by_id("normal-opener")
	_check(label.focus_element(normal_opener), "focus the label for non-modal requests")
	_key(KEY_ESCAPE)
	await _settle()
	_check(not last.open and first.open, "Escape targets the last open non-modal dialog from the source")
	var pressed_outside: Array[HTMLElement]
	label.button_pressed.connect(func(element: HTMLElement) -> void: pressed_outside.append(element), CONNECT_ONE_SHOT)
	_click(normal_opener)
	await _settle()
	_check(not first.open and pressed_outside == [normal_opener],
		"a click outside a non-modal dialog still activates the clicked control")
	first.open = true
	await _settle()
	var was_emulating_mouse: bool = Input.emulate_mouse_from_touch
	Input.emulate_mouse_from_touch = false
	_touch(outside, true)
	_touch(outside, false)
	Input.emulate_mouse_from_touch = was_emulating_mouse
	await _settle()
	_check(not first.open, "a primary touch dismisses a dialog even with mouse emulation disabled")


func _on_close_requested(request: HTMLDialogCloseRequest, dialog: HTMLDialogElement) -> void:
	last_request = request
	request_dialogs.append(dialog)
	request_reasons.append(request.reason)
	if veto_close_request:
		request.prevent_default()


func _reopen_on_close_request(_request: HTMLDialogCloseRequest, dialog: HTMLDialogElement) -> void:
	dialog.close()
	dialog.show_modal()


func _on_closed(value: String) -> void:
	closed_values.append(value)
	restored_focus.append(label.get_focused_element())


func _settle() -> void:
	for _iteration: int in range(4):
		await process_frame
	await RenderingServer.frame_post_draw


func _pixel(position: Vector2i) -> Color:
	return frame.get_texture().get_image().get_pixelv(position)


func _click(element: HTMLElement) -> void:
	var position: Vector2 = element.get_bounding_rect().get_center() - Vector2(label.get_scroll_offset())
	_pointer(position, true)
	_pointer(position, false)


func _pointer(position: Vector2, pressed: bool) -> void:
	var event: InputEventMouseButton = InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.position = position
	event.pressed = pressed
	frame.push_input(event, true)


func _touch(position: Vector2, pressed: bool) -> void:
	var event: InputEventScreenTouch = InputEventScreenTouch.new()
	event.position = position
	event.pressed = pressed
	frame.push_input(event, true)


func _key(code: Key, shift: bool = false, repeat: bool = false) -> void:
	var event: InputEventKey = InputEventKey.new()
	event.keycode = code
	event.shift_pressed = shift
	event.pressed = true
	frame.push_input(event, true)
	if repeat:
		event.echo = true
		frame.push_input(event, true)
		event.echo = false
	event.pressed = false
	frame.push_input(event, true)


func _check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		push_error(description)
