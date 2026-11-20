extends Node

var failures: int
@onready var label: HTMLLabel = $Label


func _ready() -> void:
	await _check_dispatch()
	await _check_event_targets()
	await _check_lifecycle_events()
	await _check_delegation()
	await _check_click_and_context_menu()
	await _check_pointer_mouse_events()
	await _check_pointer_capture()
	await _check_double_click_events()
	await _check_wheel_events()
	await _check_touch_events()
	await _check_dom_interactions()
	await _check_keyboard()
	await _check_form()
	await _check_passive_and_recursion()
	if failures == 0:
		print("HTML event checks passed")
	get_tree().quit(0 if failures == 0 else 1)


func _check_event_targets() -> void:
	label.text = """<div id="outer"><button id="inner" type="button">Inner</button></div>"""
	label.size = Vector2(420.0, 240.0)
	await _settle()
	var dom: HTMLDocumentNode = label.get_document_node()
	if dom == null:
		_check(false, "the document node is available for window checks")
		return
	var window: HTMLWindow = dom.get_default_view()
	_check(window != null and window.is_valid(), "the document exposes a window")
	if window == null:
		return
	_check(window.get_document() == dom, "the window reports its document")
	var inner: HTMLElement = label.get_element_by_id("inner")
	var root: HTMLElement = dom.get_document_element()
	var order: Array[String] = []
	var path_during_dispatch: Array[Array] = []
	var on_target: Callable = func(_event: HTMLEvent) -> void:
		order.append("target")
		path_during_dispatch.append(_event.composed_path())

	var window_capture: Callable = func(_event: HTMLEvent) -> void: order.append("window")
	var document_capture: Callable = func(_event: HTMLEvent) -> void: order.append("document")
	var root_capture: Callable = func(_event: HTMLEvent) -> void: order.append("root")
	window.add_event_listener(&"ping", window_capture, {"capture": true})
	dom.add_event_listener(&"ping", document_capture, {"capture": true})
	root.add_event_listener(&"ping", root_capture, {"capture": true})
	inner.add_event_listener(&"ping", on_target)
	var event: HTMLEvent = _event_of(&"ping")
	inner.dispatch_event(event)
	_check(order == ["window", "document", "root", "target"],
		"capture runs window, document, root, then the target")
	_check(path_during_dispatch.size() == 1 and path_during_dispatch[0].size() >= 4 and path_during_dispatch[0][0] == inner,
		"composed_path starts at the target during dispatch")
	_check(path_during_dispatch[0][path_during_dispatch[0].size() - 1] == window,
		"composed_path ends at the window during dispatch")
	_check(event.composed_path().is_empty(), "composed_path is empty after dispatch")

	window.remove_event_listener(&"ping", window_capture, {"capture": true})
	dom.remove_event_listener(&"ping", document_capture, {"capture": true})
	root.remove_event_listener(&"ping", root_capture, {"capture": true})
	order.clear()
	window.add_event_listener(&"ping", window_capture)
	dom.add_event_listener(&"ping", document_capture)
	root.add_event_listener(&"ping", root_capture)
	inner.dispatch_event(_event_of(&"ping"))
	_check(order == ["target", "root", "document", "window"],
		"bubbling runs the target, root, document, then the window")

	var document_hits: Array[int] = []
	var counter: Callable = func(_event: HTMLEvent) -> void: document_hits.append(1)
	dom.add_event_listener(&"counted", counter)
	inner.dispatch_event(_event_of(&"counted"))
	_check(document_hits.size() == 1, "a document listener runs once per dispatch")
	dom.remove_event_listener(&"counted", counter)

	order.clear()
	var window_only: Array[int] = []
	var window_counter: Callable = func(_event: HTMLEvent) -> void: window_only.append(1)
	window.add_event_listener(&"directed", window_counter)
	window.dispatch_event(_event_of(&"directed"))
	_check(window_only.size() == 1, "the window can be the dispatch target")
	_check(order.is_empty(), "a window dispatch does not reach element listeners")
	window.remove_event_listener(&"directed", window_counter)

	# 重新解析文档后，旧窗口句柄与监听器一起失效。
	label.text = "<p>replaced</p>"
	await _settle()
	_check(not window.is_valid(), "replacing the document invalidates the window")
	var replaced_dom: HTMLDocumentNode = label.get_document_node()
	var replaced_window: HTMLWindow = replaced_dom.get_default_view() if replaced_dom != null else null
	_check(replaced_window != null and replaced_window.is_valid() and not replaced_window.has_event_listener(&"ping"),
		"a replaced document starts with a listener-free window")


func _check_lifecycle_events() -> void:
	label.text = "<p id=\"lifecycle\">Lifecycle</p>"
	label.size = Vector2(420.0, 240.0)
	await _settle()
	var document: HTMLDocumentNode = label.get_document_node()
	var window: HTMLWindow = document.get_default_view() if document != null else null
	if document == null or window == null:
		_check(false, "lifecycle events have document and window targets")
		return
	_check(document.visibility_state == "visible", "the document reports its initial effective visibility")
	var resize_events: Array[HTMLEvent] = []
	var property_resize_events: Array[int] = []
	var resize_listener: Callable = func(event: HTMLEvent) -> void:
		resize_events.append(event)
		_check(event.target == window and not event.bubbles and not event.cancelable and not event.composed,
			"resize is a non-bubbling, non-cancelable window event")
	var on_resize: Callable = func(_event: HTMLEvent) -> void: property_resize_events.append(1)
	window.add_event_listener(&"resize", resize_listener)
	window.set("onresize", on_resize)
	_check(window.get("onresize") == on_resize, "window onresize reflects its assigned callable")
	label.size = Vector2(380.0, 220.0)
	await _settle()
	_check(resize_events.size() == 1 and property_resize_events.size() == 1,
		"a completed viewport size change dispatches one resize event to listeners and onresize")
	window.remove_event_listener(&"resize", resize_listener)
	window.set("onresize", null)

	var visibility_states: Array[String] = []
	var visibility_listener: Callable = func(event: HTMLEvent) -> void:
		visibility_states.append(document.visibility_state)
		_check(event.target == document and not event.bubbles and not event.cancelable and not event.composed,
			"visibilitychange targets the document without bubbling or cancellation")
	document.add_event_listener(&"visibilitychange", visibility_listener)
	label.hide()
	await _settle()
	_check(document.visibility_state == "hidden" and visibility_states == ["hidden",],
		"hiding the label updates and dispatches document visibility")
	label.show()
	await _settle()
	_check(document.visibility_state == "visible" and visibility_states == ["hidden", "visible",],
		"showing the label dispatches the matching visibility transition")
	document.remove_event_listener(&"visibilitychange", visibility_listener)

	var focus_events: Array[StringName] = []
	var focus_listener: Callable = func(event: HTMLEvent) -> void:
		focus_events.append(event.type)
		_check(event.target == window and not event.bubbles and not event.cancelable and not event.composed,
			"window focus events are non-bubbling and non-cancelable")
	window.add_event_listener(&"focus", focus_listener)
	window.add_event_listener(&"blur", focus_listener)
	label.notification(NOTIFICATION_WM_WINDOW_FOCUS_IN)
	label.notification(NOTIFICATION_WM_WINDOW_FOCUS_IN)
	label.notification(NOTIFICATION_WM_WINDOW_FOCUS_OUT)
	label.notification(NOTIFICATION_WM_WINDOW_FOCUS_OUT)
	_check(focus_events == [&"focus", &"blur",], "window focus notifications are dispatched once per state change")
	window.remove_event_listener(&"focus", focus_listener)
	window.remove_event_listener(&"blur", focus_listener)


func _check_passive_and_recursion() -> void:
	label.text = """<button id="button" type="button">Button</button>"""
	label.size = Vector2(420.0, 160.0)
	await _settle()
	var button: HTMLElement = label.get_element_by_id("button")
	if button == null:
		_check(false, "the button is available for passive checks")
		return

	var cancelled: Array[bool] = []
	var passive: Callable = func(event: HTMLEvent) -> void:
		cancelled.append(true)
		event.prevent_default()
	button.add_event_listener(&"passive-check", passive, {"passive": true})
	var passive_event: HTMLEvent = _event_of(&"passive-check")
	var not_cancelled: bool = button.dispatch_event(passive_event)
	_check(cancelled.size() == 1, "the passive listener ran")
	_check(not_cancelled and not passive_event.is_default_prevented(),
		"a passive listener cannot cancel the default action")
	button.remove_event_listener(&"passive-check", passive, {"capture": false})
	var document: HTMLDocumentNode = label.get_document_node()
	var window: HTMLWindow = document.get_default_view() if document != null else null
	if window != null:
		var default_passive_hits: Array[StringName] = []
		var default_passive: Callable = func(event: HTMLEvent) -> void:
			default_passive_hits.append(event.type)
			event.prevent_default()
		for event_type: StringName in [&"wheel", &"touchstart", &"touchmove",]:
			window.add_event_listener(event_type, default_passive, {"capture": true})
			var default_event: HTMLEvent = _event_of(event_type)
			_check(button.dispatch_event(default_event) and not default_event.is_default_prevented(),
				"window input listeners default to passive for %s" % event_type)
			window.remove_event_listener(event_type, default_passive, {"capture": true})
		_check(default_passive_hits == [&"wheel", &"touchstart", &"touchmove",],
			"default-passive window listeners run for wheel and touch events")
		var explicit_active: Callable = func(event: HTMLEvent) -> void: event.prevent_default()
		window.add_event_listener(&"wheel", explicit_active, {"passive": false})
		var active_event: HTMLEvent = _event_of(&"wheel")
		_check(not button.dispatch_event(active_event) and active_event.is_default_prevented(),
			"an explicit passive false option overrides the window default")
		window.remove_event_listener(&"wheel", explicit_active)

	var recursive: Array[bool] = []
	var recurse: Callable = func(event: HTMLEvent) -> void:
		recursive.append(button.dispatch_event(event))
	button.add_event_listener(&"recursive", recurse)
	button.dispatch_event(_event_of(&"recursive"))
	_check(recursive.size() == 1 and recursive[0], "re-dispatching the running event is refused")
	button.remove_event_listener(&"recursive", recurse)


func _event_of(type: StringName) -> HTMLEvent:
	var event: HTMLEvent = HTMLEvent.new()
	event.type = type
	event.bubbles = true
	event.cancelable = true
	return event


func _check_dispatch() -> void:
	label.text = """<div id="outer"><div id="mid"><button id="inner" type="button">Inner</button></div></div>"""
	label.size = Vector2(420.0, 240.0)
	await _settle()
	var dom: HTMLDocumentNode = label.get_document_node()
	_check(dom != null, "the label exposes its document node")
	if dom == null:
		return
	var outer: HTMLElement = label.get_element_by_id("outer")
	var mid: HTMLElement = label.get_element_by_id("mid")
	var inner: HTMLElement = label.get_element_by_id("inner")
	var order: Array[String]
	var capture: Array[String]
	var on_capture: Callable = func(event: HTMLEvent) -> void:
		capture.append(event.current_target.get_attribute("id"))
	var on_bubble: Callable = func(event: HTMLEvent) -> void:
		order.append("bubble:" + event.current_target.get_attribute("id"))
	var on_target: Callable = func(event: HTMLEvent) -> void:
		order.append("target:" + event.current_target.get_attribute("id"))
	_check(outer.add_event_listener(&"click", on_capture, {"capture": true}) == OK, "capture listeners register")
	_check(mid.add_event_listener(&"click", on_capture, {"capture": true}) == OK, "nested capture listeners register")
	_check(mid.add_event_listener(&"click", on_bubble) == OK, "bubble listeners register")
	_check(inner.add_event_listener(&"click", on_target) == OK, "target listeners register")
	_check(inner.add_event_listener(&"click", on_target) == OK, "duplicate registrations are ignored")
	_check(inner.has_event_listener(&"click"), "registered listeners are reported")

	var event: HTMLEvent = _event()
	var not_cancelled: bool = inner.dispatch_event(event)
	_check(not_cancelled, "dispatch reports an untouched default action")
	_check(capture == ["outer", "mid"], "capture runs from the document inward")
	_check(order == ["target:inner", "bubble:mid"], "the target runs once and bubbles upward")
	_check(event.target == inner and event.event_phase == HTMLEvent.PHASE_NONE,
		"the target is stable and the phase resets after dispatch")

	order.clear()
	var stop: Callable = func(stopped: HTMLEvent) -> void:
		order.append("stop")
		stopped.stop_propagation()
	mid.add_event_listener(&"click", stop)
	inner.dispatch_event(_event())
	_check(order == ["target:inner", "bubble:mid", "stop"],
		"stop_propagation ends propagation after all listeners on the current node")
	mid.remove_event_listener(&"click", stop)

	order.clear()
	var cancel: Callable = func(cancelled: HTMLEvent) -> void:
		cancelled.prevent_default()
	inner.add_event_listener(&"click", cancel)
	_check(not inner.dispatch_event(_event()), "prevent_default is reported by dispatch")
	inner.remove_event_listener(&"click", cancel)

	var hits: Array[int]
	var once: Callable = func(_cancelled: HTMLEvent) -> void:
		hits.append(1)
	inner.add_event_listener(&"click", once, {"once": true})
	inner.dispatch_event(_event())
	inner.dispatch_event(_event())
	_check(hits.size() == 1, "once listeners run a single time")

	var mutation_order: Array[String] = []
	var removed_during_dispatch: Callable = func(_event: HTMLEvent) -> void: mutation_order.append("removed")
	var added_during_dispatch: Callable = func(_event: HTMLEvent) -> void: mutation_order.append("added")
	var mutate_listeners: Callable = func(_event: HTMLEvent) -> void:
		mutation_order.append("mutator")
		inner.remove_event_listener(&"listener-mutation", removed_during_dispatch)
		inner.add_event_listener(&"listener-mutation", added_during_dispatch)
	inner.add_event_listener(&"listener-mutation", mutate_listeners)
	inner.add_event_listener(&"listener-mutation", removed_during_dispatch)
	inner.dispatch_event(_event_of(&"listener-mutation"))
	_check(mutation_order == ["mutator",], "removed and added listeners do not alter the current dispatch snapshot")
	mutation_order.clear()
	inner.dispatch_event(_event_of(&"listener-mutation"))
	_check(mutation_order == ["mutator", "added",], "listener changes apply to the next dispatch")
	inner.remove_event_listener(&"listener-mutation", mutate_listeners)
	inner.remove_event_listener(&"listener-mutation", added_during_dispatch)
	var handle_hits: Array[int] = []
	var handle_listener: Callable = func(_event: HTMLEvent) -> void: handle_hits.append(1)
	var listener_handle: HTMLListenerHandle = inner.listen(&"handle-cancel", handle_listener)
	listener_handle.cancel()
	inner.dispatch_event(_event_of(&"handle-cancel"))
	_check(handle_hits.is_empty(), "canceling a listener handle removes its listener")

	var clone: HTMLElement = inner.clone_node()
	_check(not clone.has_event_listener(&"click"), "clones do not copy listeners")

	mid.detach()
	dom.body.append_child(mid)
	order.clear()
	inner.dispatch_event(_event())
	_check(order.has("target:inner"), "detaching and reinserting keeps listeners")
	inner.remove()
	_check(not inner.is_valid(), "removing an element invalidates its handle")


func _check_delegation() -> void:
	label.text = """<div id="list"><button id="one" data-action="a" type="button">A</button><button id="two" data-action="b" type="button">B</button></div>"""
	label.size = Vector2(420.0, 200.0)
	await _settle()
	var dom: HTMLDocumentNode = label.get_document_node()
	if dom == null:
		_check(false, "the document node is available for delegation")
		return
	var actions: Array[String]
	var delegate: Callable = func(event: HTMLEvent) -> void:
		var target: HTMLElement = event.target
		var owner: HTMLElement = target.closest("[data-action]") if target != null else null
		if owner != null:
			actions.append(owner.get_attribute("data-action"))
	dom.add_event_listener(&"click", delegate)
	_click("one")
	_click("two")
	_check(actions == ["a", "b"], "one document listener serves every descendant")
	dom.remove_event_listener(&"click", delegate)


func _check_click_and_context_menu() -> void:
	label.text = """<p><a id="link" href="#target">Link</a></p><p id="target">Target</p><p id="plain">Plain</p>"""
	label.size = Vector2(420.0, 240.0)
	await _settle()
	var pressed: Array[String]
	var on_link: Callable = func(uri: String) -> void:
		pressed.append(uri)
	label.link_pressed.connect(on_link)
	var document: HTMLDocumentNode = label.get_document_node()
	var window: HTMLWindow = document.get_default_view() if document != null else null
	var window_click_targets: Array[String] = []
	var observe_window_click: Callable = func(event: HTMLEvent) -> void:
		window_click_targets.append(event.target.get_attribute("id"))
	if window != null:
		window.add_event_listener(&"click", observe_window_click)

	_click("link")
	_check(pressed.size() == 1, "a plain click still reports link_pressed")
	_check(window_click_targets == ["link",], "a real click bubbles from its element to the window")
	if window != null:
		window.remove_event_listener(&"click", observe_window_click)

	pressed.clear()
	var veto: Callable = func(event: HTMLEvent) -> void:
		event.prevent_default()
	var link: HTMLElement = label.get_element_by_id("link")
	link.add_event_listener(&"click", veto)
	_click("link")
	_check(pressed.is_empty(), "cancelling click suppresses link_pressed")
	link.remove_event_listener(&"click", veto)

	var menu: Array[int]
	var on_menu: Callable = func(event: HTMLEvent) -> void:
		menu.append(1)
		event.prevent_default()
	var plain: HTMLElement = label.get_element_by_id("plain")
	plain.add_event_listener(&"contextmenu", on_menu)
	var right: InputEventMouseButton = InputEventMouseButton.new()
	right.button_index = MOUSE_BUTTON_RIGHT
	right.pressed = true
	right.position = plain.get_bounding_rect().get_center()
	get_viewport().push_input(right, true)
	right.pressed = false
	get_viewport().push_input(right, true)
	_check(menu.size() == 1, "a right click dispatches contextmenu")
	plain.remove_event_listener(&"contextmenu", on_menu)
	label.link_pressed.disconnect(on_link)


func _check_pointer_mouse_events() -> void:
	label.text = """<div id="outer"><button id="first" type="button">First</button><button id="second" type="button">Second</button></div>"""
	label.size = Vector2(420.0, 180.0)
	await _settle()
	var dom: HTMLDocumentNode = label.get_document_node()
	var outer: HTMLElement = label.get_element_by_id("outer")
	var first: HTMLElement = label.get_element_by_id("first")
	var second: HTMLElement = label.get_element_by_id("second")
	if dom == null or outer == null or first == null or second == null:
		_check(false, "pointer targets are available")
		return
	var order: Array[String] = []
	var bubble_types: Array[StringName] = [
		&"pointermove",
		&"mousemove",
		&"pointerdown",
		&"mousedown",
		&"pointerup",
		&"mouseup",
		&"click",
		&"pointerout",
		&"mouseout",
		&"pointerover",
		&"mouseover",
	]
	for event_type: StringName in bubble_types:
		var bubble_listener: Callable = func(event: HTMLEvent) -> void:
			order.append("%s:%s" % [event.type, event.target.get_attribute("id")])
			if event is HTMLPointerEvent:
				var pointer: HTMLPointerEvent = event as HTMLPointerEvent
				_check(pointer.pointer_type == &"mouse" and pointer.pointer_id == 0 and pointer.is_primary,
					"mouse pointer events report mouse ID zero as primary")
		dom.add_event_listener(event_type, bubble_listener)
	for element: HTMLElement in [outer, first, second,]:
		for event_type: StringName in [&"pointerenter", &"pointerleave", &"mouseenter", &"mouseleave",]:
			var boundary_listener: Callable = func(event: HTMLEvent) -> void:
				order.append("%s:%s" % [event.type, event.target.get_attribute("id")])
			element.add_event_listener(event_type, boundary_listener)

	var first_motion: InputEventMouseMotion = InputEventMouseMotion.new()
	first_motion.position = first.get_bounding_rect().get_center()
	first_motion.global_position = first_motion.position
	first_motion.relative = first_motion.position
	order.clear()
	get_viewport().push_input(first_motion, true)
	await _settle()
	_check(order == [
		"pointerover:first",
		"pointerenter:outer",
		"pointerenter:first",
		"mouseover:first",
		"mouseenter:outer",
		"mouseenter:first",
		"pointermove:first",
		"mousemove:first",
	], "entering dispatches pointer and mouse boundary events in order")

	order.clear()
	_click("second")
	var click_order: Array[String] = order.duplicate()
	order.clear()
	await _settle()
	_check(order == [
		"pointerout:first",
		"pointerleave:first",
		"mouseout:first",
		"mouseleave:first",
		"pointerover:second",
		"pointerenter:second",
		"mouseover:second",
		"mouseenter:second",
	], "moving between siblings dispatches out and over chains in order")
	_check(click_order == [
		"pointerdown:second",
		"mousedown:second",
		"pointerup:second",
		"mouseup:second",
		"click:second",
	], "mouse press and release dispatch pointer events around compatibility mouse events")

	var cancel_pointerdown: Callable = func(event: HTMLEvent) -> void:
		order.append("cancel:pointerdown")
		event.prevent_default()
	second.add_event_listener(&"pointerdown", cancel_pointerdown)
	order.clear()
	_click("second")
	_check(order == ["cancel:pointerdown", "pointerdown:second", "pointerup:second"],
		"cancelling pointerdown suppresses compatibility mouse events and click")
	second.remove_event_listener(&"pointerdown", cancel_pointerdown)


func _check_pointer_capture() -> void:
	label.text = "<button id=\"capture\">Capture</button><button id=\"release\">Release area</button>"
	label.size = Vector2(420.0, 180.0)
	await _settle()
	var first: HTMLElement = label.get_element_by_id("capture")
	var second: HTMLElement = label.get_element_by_id("release")
	if first == null or second == null:
		_check(false, "pointer capture targets are available")
		return
	var order: Array[String] = []
	var capture_on_down: Callable = func(event: HTMLEvent) -> void:
		if event.type == &"pointerdown":
			var pointer: HTMLPointerEvent = event as HTMLPointerEvent
			_check(first.set_pointer_capture(pointer.pointer_id) == OK, "an active pointer can be captured")
	var observe: Callable = func(event: HTMLEvent) -> void:
		if event.type == &"pointermove" or event.type == &"pointerup":
			order.append("%s:%s" % [event.type, event.target.get_attribute("id")])
		else:
			order.append(String(event.type))
	first.add_event_listener(&"pointerdown", capture_on_down)
	for event_type: StringName in [&"gotpointercapture", &"pointermove", &"pointerup", &"lostpointercapture",]:
		first.add_event_listener(event_type, observe)

	var press: InputEventMouseButton = InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = first.get_bounding_rect().get_center()
	get_viewport().push_input(press, true)
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = second.get_bounding_rect().get_center()
	motion.global_position = motion.position
	motion.relative = motion.position - press.position
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(motion, true)
	await _settle()
	var release: InputEventMouseButton = InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	release.position = motion.position
	get_viewport().push_input(release, true)
	await _settle()
	_check(order == ["gotpointercapture", "pointermove:capture", "pointerup:capture", "lostpointercapture",],
		"capture keeps pointer events on its target and releases after pointerup")
	first.remove_event_listener(&"pointerdown", capture_on_down)
	for event_type: StringName in [&"gotpointercapture", &"pointermove", &"pointerup", &"lostpointercapture",]:
		first.remove_event_listener(event_type, observe)
	var transfer_results: Array[int] = []
	var transfer_on_down: Callable = func(event: HTMLEvent) -> void:
		if event.type == &"pointerdown":
			var pointer: HTMLPointerEvent = event as HTMLPointerEvent
			first.set_pointer_capture(pointer.pointer_id)
	var transfer_on_move: Callable = func(event: HTMLEvent) -> void:
		if event.type == &"pointermove":
			var pointer: HTMLPointerEvent = event as HTMLPointerEvent
			transfer_results.append(second.set_pointer_capture(pointer.pointer_id))
	var transfer_events: Array[String] = []
	var observe_first_transfer: Callable = func(event: HTMLEvent) -> void:
		if event.type in [&"gotpointercapture", &"pointermove", &"lostpointercapture",]:
			transfer_events.append("first:%s" % event.type)
	var observe_second_transfer: Callable = func(event: HTMLEvent) -> void:
		if event.type in [&"gotpointercapture", &"pointermove", &"pointerup", &"lostpointercapture",]:
			transfer_events.append("second:%s" % event.type)
	first.add_event_listener(&"pointerdown", transfer_on_down)
	first.add_event_listener(&"pointermove", transfer_on_move)
	first.add_event_listener(&"gotpointercapture", observe_first_transfer)
	first.add_event_listener(&"pointermove", observe_first_transfer)
	first.add_event_listener(&"lostpointercapture", observe_first_transfer)
	second.add_event_listener(&"gotpointercapture", observe_second_transfer)
	second.add_event_listener(&"pointermove", observe_second_transfer)
	second.add_event_listener(&"pointerup", observe_second_transfer)
	second.add_event_listener(&"lostpointercapture", observe_second_transfer)
	press = InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = first.get_bounding_rect().get_center()
	get_viewport().push_input(press, true)
	motion = InputEventMouseMotion.new()
	motion.position = second.get_bounding_rect().get_center()
	motion.global_position = motion.position
	motion.relative = motion.position - press.position
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(motion, true)
	await _settle()
	var second_motion: InputEventMouseMotion = InputEventMouseMotion.new()
	second_motion.position = first.get_bounding_rect().get_center()
	second_motion.global_position = second_motion.position
	second_motion.relative = second_motion.position - motion.position
	second_motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(second_motion, true)
	await _settle()
	release = InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	release.position = second_motion.position
	get_viewport().push_input(release, true)
	await _settle()
	_check(transfer_results == [OK,] and transfer_events == [
		"first:gotpointercapture",
		"first:pointermove",
		"first:lostpointercapture",
		"second:gotpointercapture",
		"second:pointermove",
		"second:pointerup",
		"second:lostpointercapture",
	], "pointer capture transfers future events and releases the final target")
	first.remove_event_listener(&"pointerdown", transfer_on_down)
	first.remove_event_listener(&"pointermove", transfer_on_move)
	first.remove_event_listener(&"gotpointercapture", observe_first_transfer)
	first.remove_event_listener(&"pointermove", observe_first_transfer)
	first.remove_event_listener(&"lostpointercapture", observe_first_transfer)
	second.remove_event_listener(&"gotpointercapture", observe_second_transfer)
	second.remove_event_listener(&"pointermove", observe_second_transfer)
	second.remove_event_listener(&"pointerup", observe_second_transfer)
	second.remove_event_listener(&"lostpointercapture", observe_second_transfer)
	var removal_order: Array[StringName] = []
	var capture_before_removal: Callable = func(event: HTMLEvent) -> void:
		if event.type == &"pointerdown":
			var pointer: HTMLPointerEvent = event as HTMLPointerEvent
			_check(second.set_pointer_capture(pointer.pointer_id) == OK, "the second pointer can be captured")
	var observe_removal: Callable = func(event: HTMLEvent) -> void:
		if event.type == &"gotpointercapture" or event.type == &"lostpointercapture":
			removal_order.append(event.type)
	second.add_event_listener(&"pointerdown", capture_before_removal)
	second.add_event_listener(&"gotpointercapture", observe_removal)
	second.add_event_listener(&"lostpointercapture", observe_removal)
	press = InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = second.get_bounding_rect().get_center()
	get_viewport().push_input(press, true)
	second.remove()
	_check(removal_order == [&"gotpointercapture", &"lostpointercapture",] and not second.is_valid(),
		"removing a captured target dispatches lostpointercapture before invalidating its handle")
	press.pressed = false
	get_viewport().push_input(press, true)


func _check_double_click_events() -> void:
	label.text = "<button id=\"double-click\" type=\"button\">Double click</button>"
	label.size = Vector2(420.0, 160.0)
	await _settle()
	var target: HTMLElement = label.get_element_by_id("double-click")
	if target == null:
		_check(false, "the double-click target is available")
		return
	var click_details: Array[int] = []
	var down_details: Array[int] = []
	var up_details: Array[int] = []
	var sequence: Array[String] = []
	var on_click: Callable = func(event: HTMLEvent) -> void:
		var mouse_event: HTMLMouseEvent = event as HTMLMouseEvent
		click_details.append(mouse_event.detail)
		sequence.append("click:%d" % mouse_event.detail)
	var on_down: Callable = func(event: HTMLEvent) -> void:
		down_details.append((event as HTMLMouseEvent).detail)
	var on_up: Callable = func(event: HTMLEvent) -> void:
		up_details.append((event as HTMLMouseEvent).detail)
	var on_double_click: Callable = func(event: HTMLEvent) -> void:
		var mouse_event: HTMLMouseEvent = event as HTMLMouseEvent
		sequence.append("dblclick:%d" % mouse_event.detail)
	target.add_event_listener(&"click", on_click)
	target.add_event_listener(&"mousedown", on_down)
	target.add_event_listener(&"mouseup", on_up)
	target.add_event_listener(&"dblclick", on_double_click)

	_click("double-click")
	_click("double-click")
	_check(click_details == [1, 2,], "two quick clicks report click detail one then two")
	_check(down_details == [1, 2,] and up_details == [1, 2,],
		"mousedown and mouseup use the corresponding click detail")
	_check(sequence == ["click:1", "click:2", "dblclick:2",], "dblclick follows the second click")

	await get_tree().create_timer(0.55).timeout
	_click("double-click")
	_check(click_details == [1, 2, 1,], "a click after the time window starts a new sequence")
	_check(sequence == ["click:1", "click:2", "dblclick:2", "click:1",],
		"a new click sequence does not dispatch another dblclick")
	target.remove_event_listener(&"click", on_click)
	target.remove_event_listener(&"mousedown", on_down)
	target.remove_event_listener(&"mouseup", on_up)
	target.remove_event_listener(&"dblclick", on_double_click)


func _check_wheel_events() -> void:
	var source: String = "<div id=\"wheel-target\">Wheel target</div>"
	for index: int in range(30):
		source += "<p>Scrollable line %d</p>" % index
	label.text = source
	label.size = Vector2(420.0, 180.0)
	await _settle()
	var target: HTMLElement = label.get_element_by_id("wheel-target")
	if target == null:
		_check(false, "the wheel target is available")
		return
	var received: Array[HTMLWheelEvent]
	var inspect: Callable = func(event: HTMLEvent) -> void:
		var wheel: HTMLWheelEvent = event as HTMLWheelEvent
		if wheel == null:
			_check(false, "wheel dispatch uses HTMLWheelEvent")
			return
		received.append(wheel)
		_check(wheel.delta.y > 0.0, "wheel down reports a positive pixel delta")
		_check(wheel.delta_mode == HTMLWheelEvent.DELTA_MODE_PIXEL, "wheel deltas use pixel units")
		_check(wheel.target == target, "wheel targets the element under the pointer")
	target.add_event_listener(&"wheel", inspect)
	var input: InputEventMouseButton = InputEventMouseButton.new()
	input.button_index = MOUSE_BUTTON_WHEEL_DOWN
	input.pressed = true
	input.factor = 2.0
	input.position = target.get_bounding_rect().get_center()
	get_viewport().push_input(input, true)
	_check(received.size() == 1, "a wheel input dispatches once across the canvas and viewport")
	_check(is_equal_approx(received[0].delta.y, 72.0), "wheel delta preserves a non-default Godot factor")
	_check(label.get_scroll_offset().y > 0, "an uncancelled wheel input scrolls the document")
	target.remove_event_listener(&"wheel", inspect)

	label.set_scroll_offset(Vector2i.ZERO)
	var veto: Callable = func(event: HTMLEvent) -> void: event.prevent_default()
	target.add_event_listener(&"wheel", veto)
	input = InputEventMouseButton.new()
	input.button_index = MOUSE_BUTTON_WHEEL_DOWN
	input.pressed = true
	input.factor = 1.0
	input.position = target.get_bounding_rect().get_center()
	get_viewport().push_input(input, true)
	_check(label.get_scroll_offset() == Vector2i.ZERO, "cancelling wheel prevents overflow and viewport scrolling")
	target.remove_event_listener(&"wheel", veto)


func _check_touch_events() -> void:
	Input.set_emulate_mouse_from_touch(false)
	var source: String = "<button id=\"touch-target\" type=\"button\">Touch target</button>"
	for index: int in range(30):
		source += "<p>Scrollable touch line %d</p>" % index
	label.text = source
	label.size = Vector2(420.0, 180.0)
	label.set_scroll_offset(Vector2i.ZERO)
	await _settle()
	var dom: HTMLDocumentNode = label.get_document_node()
	var target: HTMLElement = label.get_element_by_id("touch-target")
	if dom == null or target == null:
		_check(false, "the touch target is available")
		return
	var order: Array[String] = []
	var pointer_ids: Array[int] = []
	var inspect: Callable = func(event: HTMLEvent) -> void:
		order.append(String(event.type))
		if event is HTMLPointerEvent:
			var pointer: HTMLPointerEvent = event as HTMLPointerEvent
			_check(pointer.pointer_type == &"touch", "screen input reports a touch pointer type")
			_check(pointer.is_primary, "the first active touch is primary")
			if event.type == &"pointerdown":
				pointer_ids.append(pointer.pointer_id)
	for event_type: StringName in [
		&"pointerdown",
		&"pointerup",
		&"pointermove",
		&"pointercancel",
		&"mousedown",
		&"mouseup",
		&"click",
	]:
		dom.add_event_listener(event_type, inspect)

	var position: Vector2 = target.get_bounding_rect().get_center()
	var touch: InputEventScreenTouch = InputEventScreenTouch.new()
	touch.device = 7
	touch.index = 4
	touch.position = position
	touch.pressed = true
	get_viewport().push_input(touch, true)
	touch.pressed = false
	get_viewport().push_input(touch, true)
	_check(order == ["pointerdown", "mousedown", "pointerup", "mouseup", "click"],
		"a touch tap dispatches pointer and compatibility mouse events")
	_check(pointer_ids.size() == 1 and pointer_ids[0] > 0, "touch IDs do not collide with mouse ID zero")
	var stable_id: int = pointer_ids[0] if not pointer_ids.is_empty() else -1
	order.clear()
	var repeat_touch: InputEventScreenTouch = InputEventScreenTouch.new()
	repeat_touch.device = 7
	repeat_touch.index = 4
	repeat_touch.position = position
	repeat_touch.pressed = true
	get_viewport().push_input(repeat_touch, true)
	repeat_touch.pressed = false
	get_viewport().push_input(repeat_touch, true)
	_check(pointer_ids.size() == 2 and pointer_ids[1] == stable_id,
		"the same device and touch index keep a stable pointer ID")
	for event_type: StringName in [
		&"pointerdown",
		&"pointerup",
		&"pointermove",
		&"pointercancel",
		&"mousedown",
		&"mouseup",
		&"click",
	]:
		dom.remove_event_listener(event_type, inspect)

	var multi_primary: Array[bool] = []
	var multi_ids: Array[int] = []
	var multi_order: Array[StringName] = []
	var touch_counts: Array[int] = []
	var multi_inspect: Callable = func(event: HTMLEvent) -> void:
		multi_order.append(event.type)
		if event.type == &"pointerdown":
			var pointer: HTMLPointerEvent = event as HTMLPointerEvent
			multi_primary.append(pointer.is_primary)
			multi_ids.append(pointer.pointer_id)
		elif event.type == &"touchstart":
			var touch_event: HTMLTouchEvent = event as HTMLTouchEvent
			touch_counts.append(touch_event.touches.size())
	for event_type: StringName in [
		&"pointerdown",
		&"pointerup",
		&"pointercancel",
		&"mousedown",
		&"mouseup",
		&"click",
		&"touchstart",
		&"touchend",
		&"touchcancel",
	]:
		dom.add_event_listener(event_type, multi_inspect)
	var first_touch: InputEventScreenTouch = InputEventScreenTouch.new()
	first_touch.device = 7
	first_touch.index = 11
	first_touch.position = position
	first_touch.pressed = true
	get_viewport().push_input(first_touch, true)
	var second_touch: InputEventScreenTouch = InputEventScreenTouch.new()
	second_touch.device = 7
	second_touch.index = 12
	second_touch.position = position
	second_touch.pressed = true
	get_viewport().push_input(second_touch, true)
	second_touch.pressed = false
	get_viewport().push_input(second_touch, true)
	first_touch.pressed = false
	get_viewport().push_input(first_touch, true)
	_check(multi_primary == [true, false,] and multi_ids.size() == 2 and multi_ids[0] != multi_ids[1],
		"simultaneous touches have distinct pointer IDs and one primary pointer")
	_check(touch_counts == [1, 2,] and multi_order.count(&"mousedown") == 1 and
		multi_order.count(&"mouseup") == 1 and multi_order.count(&"click") == 1,
		"multitouch exposes active touch counts and suppresses secondary compatibility mouse events")
	for event_type: StringName in [
		&"pointerdown",
		&"pointerup",
		&"pointercancel",
		&"mousedown",
		&"mouseup",
		&"click",
		&"touchstart",
		&"touchend",
		&"touchcancel",
	]:
		dom.remove_event_listener(event_type, multi_inspect)

	var cancel_order: Array[StringName] = []
	var capture_cancelled_pointer: Callable = func(event: HTMLEvent) -> void:
		if event.type == &"pointerdown":
			var pointer: HTMLPointerEvent = event as HTMLPointerEvent
			_check(target.set_pointer_capture(pointer.pointer_id) == OK, "a touch pointer can be captured")
	var observe_cancel: Callable = func(event: HTMLEvent) -> void:
		if event.type in [&"gotpointercapture", &"pointercancel", &"pointerup", &"lostpointercapture", &"touchcancel",]:
			cancel_order.append(event.type)
	for event_type: StringName in [
		&"pointerdown",
		&"gotpointercapture",
		&"pointercancel",
		&"pointerup",
		&"lostpointercapture",
		&"touchcancel",
	]:
		dom.add_event_listener(event_type, observe_cancel)
	dom.add_event_listener(&"pointerdown", capture_cancelled_pointer)
	var canceled_touch: InputEventScreenTouch = InputEventScreenTouch.new()
	canceled_touch.device = 7
	canceled_touch.index = 13
	canceled_touch.position = position
	canceled_touch.pressed = true
	get_viewport().push_input(canceled_touch, true)
	canceled_touch.pressed = false
	canceled_touch.canceled = true
	get_viewport().push_input(canceled_touch, true)
	_check(cancel_order == [&"gotpointercapture", &"pointercancel", &"lostpointercapture", &"touchcancel",] and
		not cancel_order.has(&"pointerup"), "canceling a captured touch releases capture without pointerup")
	dom.remove_event_listener(&"pointerdown", capture_cancelled_pointer)
	for event_type: StringName in [
		&"pointerdown",
		&"gotpointercapture",
		&"pointercancel",
		&"pointerup",
		&"lostpointercapture",
		&"touchcancel",
	]:
		dom.remove_event_listener(event_type, observe_cancel)
	for event_type: StringName in [
		&"pointerdown",
		&"pointerup",
		&"pointermove",
		&"pointercancel",
		&"mousedown",
		&"mouseup",
		&"click",
	]:
		dom.add_event_listener(event_type, inspect)

	label.set_scroll_offset(Vector2i.ZERO)
	await _settle()
	var pan_target: HTMLElement = label.get_element_at(Vector2(40.0, 70.0))
	position = Vector2(40.0, 70.0)
	order.clear()
	var pan_touch: InputEventScreenTouch = InputEventScreenTouch.new()
	pan_touch.device = 7
	pan_touch.index = 6
	pan_touch.position = position
	pan_touch.pressed = true
	get_viewport().push_input(pan_touch, true)
	var drag: InputEventScreenDrag = InputEventScreenDrag.new()
	drag.device = 7
	drag.index = 6
	drag.position = position + Vector2(0.0, -20.0)
	drag.relative = Vector2(0.0, -20.0)
	drag.velocity = Vector2(0.0, -120.0)
	get_viewport().push_input(drag, true)
	_check(order.has("pointercancel"), "viewport pan cancels its active touch pointer")
	_check(label.get_scroll_offset().y > 0, "touch scrolling still reaches the viewport")
	pan_touch.pressed = false
	pan_touch.position = drag.position
	get_viewport().push_input(pan_touch, true)
	_check(not order.has("pointerup") and not order.has("click"), "a canceled pan does not synthesize a tap")
	if pan_target == null:
		_check(false, "the touch pan starts over an HTML target")


func _check_keyboard() -> void:
	label.text = """<input id="a"><input id="b">"""
	label.size = Vector2(420.0, 200.0)
	await _settle()
	var first: HTMLElement = label.get_element_by_id("a")
	var second: HTMLElement = label.get_element_by_id("b")
	label.focus_element(first)
	_check(label.get_focused_element() == first, "the first field takes focus")
	var hits: Array[int]
	var veto: Callable = func(event: HTMLEvent) -> void:
		hits.append(1)
		event.prevent_default()
	first.add_event_listener(&"keydown", veto)
	_key(KEY_TAB, true)
	_key(KEY_TAB, false)
	_check(hits.size() >= 1, "keydown reaches the focused element")
	_check(label.get_focused_element() == first, "cancelling keydown stops Tab traversal")
	first.remove_event_listener(&"keydown", veto)
	_key(KEY_TAB, true)
	_key(KEY_TAB, false)
	_check(label.get_focused_element() == second, "Tab still traverses without a listener")


func _check_form() -> void:
	label.text = """<form id="form"><input id="name" name="name" value="x"><button id="send" type="button">Send</button></form>"""
	label.size = Vector2(420.0, 220.0)
	await _settle()
	var submitted: Array[int]
	var on_submit: Callable = func(_form: HTMLFormElement, _entries: Array[Dictionary], _submitter: HTMLElement) -> void:
		submitted.append(1)
	label.form_submitted.connect(on_submit)
	var form: HTMLFormElement = label.get_element_by_id("form")
	_check(label.submit_form(form) == OK and submitted.size() == 1, "a plain submission reaches form_submitted")

	submitted.clear()
	var veto: Callable = func(event: HTMLEvent) -> void:
		event.prevent_default()
	form.add_event_listener(&"submit", veto)
	_check(label.submit_form(form) == OK, "a cancelled submission is not an error")
	_check(submitted.is_empty(), "cancelling submit suppresses form_submitted")
	form.remove_event_listener(&"submit", veto)
	label.form_submitted.disconnect(on_submit)


func _event() -> HTMLEvent:
	var event: HTMLEvent = HTMLEvent.new()
	event.type = &"click"
	event.bubbles = true
	event.cancelable = true
	return event


func _click(id: String) -> void:
	var element: HTMLElement = label.get_element_by_id(id)
	var point: Vector2 = element.get_bounding_rect().get_center()
	for state: bool in [true, false]:
		var click: InputEventMouseButton = InputEventMouseButton.new()
		click.button_index = MOUSE_BUTTON_LEFT
		click.pressed = state
		click.position = point
		get_viewport().push_input(click, true)


func _key(code: Key, pressed: bool) -> void:
	var key: InputEventKey = InputEventKey.new()
	key.keycode = code
	key.pressed = pressed
	get_viewport().push_input(key, true)


func _settle() -> void:
	for _frame: int in range(8):
		await get_tree().process_frame


func _check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		push_error(description)


func _mutation_button(point: Vector2, pressed: bool) -> void:
	var event: InputEventMouseButton = InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = pressed
	event.position = point
	event.global_position = point
	get_viewport().push_input(event, true)


func _mutation_motion(from: Vector2, to: Vector2) -> void:
	var event: InputEventMouseMotion = InputEventMouseMotion.new()
	event.position = to
	event.global_position = to
	event.relative = to - from
	event.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(event, true)


func _check_dom_interactions() -> void:
	label.text = """<style>body { margin: 8px; } input { width: 240px; }</style>
	<input id="range" type="range" min="0" max="100" value="0"><output id="out">Initial</output>
	<input id="focus" value="text"><select id="select"><option id="one">One</option><option id="two">Two</option></select>"""
	label.size = Vector2(420.0, 240.0)
	await _settle()
	var slider: HTMLInputElement = label.get_element_by_id("range") as HTMLInputElement
	var output: HTMLElement = label.get_element_by_id("out")
	var on_input: Callable = func(_event: HTMLEvent) -> void:
		output.set_text_content(slider.value)
	slider.add_event_listener(&"input", on_input)
	var box: Rect2 = slider.get_bounding_rect()
	var start: Vector2 = box.position + Vector2(8.0, box.size.y * 0.5)
	var middle: Vector2 = box.position + Vector2(box.size.x * 0.4, box.size.y * 0.5)
	var end: Vector2 = box.position + Vector2(box.size.x * 0.8, box.size.y * 0.5)
	_mutation_button(start, true)
	_mutation_motion(start, middle)
	await _settle()
	var middle_value: float = slider.value_as_number
	_mutation_motion(middle, end)
	await _settle()
	_check(slider.value_as_number > middle_value and output.get_text_content() == slider.value,
		"range dragging survives synchronous output edits and layout refreshes")
	slider.set_attribute("max", "200")
	await _settle()
	_mutation_motion(end, box.position + Vector2(box.size.x * 0.95, box.size.y * 0.5))
	_check(slider.value_as_number > 100.0, "range dragging uses updated bounds without restarting")
	slider.set_attribute("disabled", "")
	var disabled_value: String = slider.value
	_mutation_motion(end, middle)
	_check(slider.value == disabled_value, "disabling a range ends its built-in drag immediately")
	_mutation_button(middle, false)
	slider.remove_attribute("disabled")
	await _settle()
	var field: HTMLInputElement = label.get_element_by_id("focus") as HTMLInputElement
	label.focus_element(field)
	var revision: int = field.mutation_revision
	output.set_text_content("Unrelated")
	await _settle()
	_check(label.get_focused_element() == field and field.mutation_revision == revision,
		"unrelated edits preserve focus without modifying the focused node revision")
	var on_beforeinput: Callable = func(_event: HTMLEvent) -> void:
		field.set_attribute("readonly", "")
	field.add_event_listener(&"beforeinput", on_beforeinput)
	var key: InputEventKey = InputEventKey.new()
	key.keycode = KEY_X
	key.unicode = 120
	key.pressed = true
	get_viewport().push_input(key, true)
	key.pressed = false
	get_viewport().push_input(key, true)
	_check(field.value == "text", "beforeinput revalidates editability after synchronous DOM callbacks")
	field.remove_event_listener(&"beforeinput", on_beforeinput)
	await _settle()
	var select: HTMLElement = label.get_element_by_id("select")
	_check(label.activate_element(select), "activate the select menu after unrelated mutations")
	await _settle()
	var menu: Control = label.get_node_or_null("_HTMLSelectMenu") as Control
	_check(menu != null and menu.visible, "the mutation fixture opens a select menu")
	output.set_text_content("Menu stays")
	await _settle()
	_check(menu != null and menu.visible, "unrelated edits keep an open select menu")
	label.get_element_by_id("two").set_text_content("Updated option")
	await _settle()
	_check(menu != null and menu.visible, "option edits refresh the open menu")
	select.set_attribute("disabled", "")
	_check(menu != null and not menu.visible, "disabling the owner closes its menu")

	label.text = '<style>body { margin: 8px; font: 16px/24px monospace; }</style><span id="lead">HEAD </span><span id="tail">TAIL WORDS</span><p id="other">Other</p>'
	label.drag_and_drop_selection_enabled = false
	await _settle()
	var lead: HTMLElement = label.get_element_by_id("lead")
	var tail: HTMLElement = label.get_element_by_id("tail")
	start = lead.get_bounding_rect().position + Vector2(1.0, 8.0)
	end = tail.get_bounding_rect().end - Vector2(1.0, 8.0)
	_mutation_button(start, true)
	_mutation_motion(start, tail.get_bounding_rect().get_center())
	label.get_element_by_id("other").set_text_content("Unrelated replacement")
	var text: HTMLTextNode = lead.get_first_child() as HTMLTextNode
	text.set_data("PREFIX HEAD ")
	await _settle()
	end = tail.get_bounding_rect().end - Vector2(1.0, 8.0)
	_mutation_motion(start, end)
	_mutation_button(end, false)
	_check("TAIL WORDS" in label.get_selected_text(), "document drag selection continues after text replacement and reflow")
	label.drag_and_drop_selection_enabled = true

	label.text = '<input id="checkbox" type="checkbox"><output id="out"></output>'
	await _settle()
	var checkbox: HTMLInputElement = label.get_element_by_id("checkbox") as HTMLInputElement
	var disable_on_down: Callable = func(_event: HTMLEvent) -> void:
		checkbox.set_attribute("disabled", "")
	checkbox.add_event_listener(&"mousedown", disable_on_down)
	_click("checkbox")
	_check(not checkbox.checked, "mousedown cannot activate a control disabled by its callback")

	label.text = '<input id="first"><input id="second"><input id="third">'
	await _settle()
	var first: HTMLElement = label.get_element_by_id("first")
	var second: HTMLElement = label.get_element_by_id("second")
	var third: HTMLElement = label.get_element_by_id("third")
	label.focus_element(first)
	var redirected: Callable = func(_event: HTMLEvent) -> void:
		label.focus_element(third)
	first.add_event_listener(&"blur", redirected)
	label.focus_element(second)
	_check(label.get_focused_element() == third, "a nested focus transition wins over the interrupted transition")
	first.remove_event_listener(&"blur", redirected)

	label.text = '<button id="touch">Touch</button><button id="other_touch">Other touch</button>'
	await _settle()
	var touch_target: HTMLElement = label.get_element_by_id("touch")
	var other_touch: HTMLElement = label.get_element_by_id("other_touch")
	var cancelled: Array[StringName] = []
	var remove_on_down: Callable = func(_event: HTMLEvent) -> void:
		touch_target.detach()
	var observe_touch: Callable = func(event: HTMLEvent) -> void:
		cancelled.append(event.type)
	touch_target.add_event_listener(&"pointerdown", remove_on_down)
	touch_target.add_event_listener(&"pointercancel", observe_touch)
	touch_target.add_event_listener(&"pointerup", observe_touch)
	var touch_event: InputEventScreenTouch = InputEventScreenTouch.new()
	touch_event.index = 0
	touch_event.position = touch_target.get_bounding_rect().get_center()
	touch_event.pressed = true
	get_viewport().push_input(touch_event, true)
	touch_event.pressed = false
	get_viewport().push_input(touch_event, true)
	_check(cancelled == [&"pointercancel",], "removal during touch pointerdown cancels once and does not later send pointerup")
	await _settle()
	var remaining: Array[StringName] = []
	var observe_other: Callable = func(event: HTMLEvent) -> void:
		remaining.append(event.type)
	other_touch.add_event_listener(&"pointerup", observe_other)
	other_touch.add_event_listener(&"pointercancel", observe_other)
	touch_event = InputEventScreenTouch.new()
	touch_event.index = 1
	touch_event.position = other_touch.get_bounding_rect().get_center()
	touch_event.pressed = true
	get_viewport().push_input(touch_event, true)
	label.get_document_node().get_body().append_child(touch_target)
	await _settle()
	touch_target.remove_event_listener(&"pointerdown", remove_on_down)
	var secondary: InputEventScreenTouch = InputEventScreenTouch.new()
	secondary.index = 2
	secondary.position = touch_target.get_bounding_rect().get_center()
	secondary.pressed = true
	get_viewport().push_input(secondary, true)
	var secondary_motion: InputEventScreenDrag = InputEventScreenDrag.new()
	secondary_motion.index = 2
	secondary_motion.position = secondary.position + Vector2(12.0, 0.0)
	secondary_motion.relative = Vector2(12.0, 0.0)
	get_viewport().push_input(secondary_motion, true)
	touch_target.detach()
	touch_event.pressed = false
	get_viewport().push_input(touch_event, true)
	_check(remaining == [&"pointerup",] and cancelled.count(&"pointercancel") == 2,
		"removing a secondary pointer cancels only that pointer and preserves the primary stream")
