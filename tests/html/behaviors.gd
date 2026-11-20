extends Node

var failures: int
@onready var label: HTMLLabel = $Label


func _ready() -> void:
	_prepare(label)
	await _settle()
	_check(not label.behaviors_enabled and _events(label).is_empty(), "the scene defaults to disabled behaviors")
	var registry: Dictionary[StringName, GDScript] = label.behavior_scripts
	var edited: Dictionary[StringName, GDScript] = label.behavior_scripts
	edited.erase(&"direct")
	_check(label.behavior_scripts.has(&"direct"), "reading the registry returns a snapshot")

	label.behaviors_enabled = true
	await _settle()
	_check(_events(label) == ["attach:direct:A", "attach:direct:A", "attach:indirect:A", "attach:path:A"],
		"aliases and local paths mount in declaration order, and duplicate tags create separate instances")
	_check(Engine.has_meta(&"unregistered_loaded") and not Engine.has_meta(&"wrong_instantiated"),
		"a local unregistered path loads, while a registered script with the wrong base is not constructed")
	var original_instances: Array[WeakRef] = label.get_meta(&"instances")
	var original_refs: Array[WeakRef] = original_instances.duplicate()
	_check(original_refs.size() == 4 and original_refs[0].get_ref() != original_refs[1].get_ref(),
		"duplicate declarations are independently instantiated")
	_check(label.activate_element(label.get_element_by_id("test-button")), "button activates through the host DOM")
	_check(_events(label).slice(4) == ["button:direct", "button:direct", "button:indirect", "button:path"],
		"the attached scripts receive button_pressed")
	_events(label).clear()

	var declaration: HTMLScriptElement = label.get_element_by_id("first") as HTMLScriptElement
	declaration.src = "indirect"
	label.size.x += 30.0
	var script_tag: HTMLElement = label.create_element("script")
	script_tag.set_attribute("type", "text/gdscript")
	script_tag.set_attribute("src", "direct")
	label.query_selector("body").append_child(script_tag)
	await _settle()
	_check(_events(label).is_empty(), "DOM changes and viewport updates do not mount or detach scripts")

	var second_document: HTMLDocument = _document("B", "indirect")
	label.document = second_document
	_check(_events(label) == ["detach:path:A", "detach:indirect:A", "detach:direct:A", "detach:direct:A"],
		"replacement detaches in reverse order with the old DOM still available")
	for reference: WeakRef in original_refs:
		_check(reference.get_ref() == null, "detached instances are released")
	await _settle()
	_check(_events(label).back() == "attach:indirect:B", "replacement mounts the new document")
	_events(label).clear()

	var sibling: HTMLLabel = HTMLLabel.new()
	sibling.size = Vector2(300.0, 150.0)
	sibling.behaviors_enabled = true
	sibling.behavior_scripts = registry
	sibling.document = second_document
	_prepare(sibling)
	add_child(sibling)
	await _settle()
	_check(_events(sibling) == ["attach:indirect:B"] and _events(label).is_empty(),
		"two hosts sharing a document maintain independent instances")
	second_document.text = _markup("Reloaded", "direct")
	await _settle()
	for host: HTMLLabel in [label, sibling]:
		_check(_events(host).slice(-2) == ["detach:indirect:B", "attach:direct:Reloaded"],
			"the document changed signal replaces each host's behavior")
	sibling.queue_free()

	_events(label).clear()
	label.behaviors_enabled = false
	_check(_events(label) == ["detach:direct:Reloaded"], "disabling detaches immediately")
	_check(label.activate_element(label.get_element_by_id("test-button")), "DOM is still interactive when behavior is off")
	_check(_events(label).size() == 1, "the detach hook disconnects the old button handler")
	label.behaviors_enabled = true
	await _settle()
	_check(_events(label).back() == "attach:direct:Reloaded", "enabling mounts the current document again")
	_events(label).clear()

	var reduced: Dictionary[StringName, GDScript] = {&"direct": registry[&"direct"]}
	label.behavior_scripts = reduced
	_check(_events(label) == ["detach:direct:Reloaded"], "replacing the registry detaches the previous instances")
	reduced.clear()
	await _settle()
	_check(_events(label).back() == "attach:direct:Reloaded", "setter keeps a registry snapshot")
	label.behavior_scripts = label.behavior_scripts
	_events(label).clear()
	await _settle()
	_check(_events(label).is_empty(), "identical registry does not trigger remount")
	remove_child(label)
	_check(_events(label) == ["detach:direct:Reloaded"], "exiting the tree detaches once")
	add_child(label)
	await _settle()
	_check(_events(label).back() == "attach:direct:Reloaded", "reentering the tree mounts a fresh instance")

	await _check_reentry(registry)
	await _check_document_behavior()
	if failures == 0:
		print("HTML behavior checks passed")
	get_tree().quit(0 if failures == 0 else 1)


func _check_reentry(registry: Dictionary[StringName, GDScript]) -> void:
	label.behaviors_enabled = false
	label.behavior_scripts = registry
	var replacement: HTMLDocument = _document("Target", "direct")
	label.set_meta(&"replacement", replacement)
	label.document = _document("Before", "swap", "indirect")
	_events(label).clear()
	label.behaviors_enabled = true
	await _settle()
	_check(_events(label) == [
		"attach:swap:Before",
		"after_swap:Before",
		"detach:swap:Before",
		"attach:direct:Target",
	], "a source change from _attach preserves the old DOM through cleanup and skips the remaining old entries")

	label.document = _document("Leaving", "swap_on_detach")
	await _settle()
	_events(label).clear()
	label.document = _document("Superseded", "indirect")
	await _settle()
	_check(_events(label) == [
		"detach:swap_on_detach:Leaving",
		"after_detach_swap:Leaving",
		"attach:direct:Target",
	], "a source change from _detach completes cleanup before mounting the final source")


func _check_document_behavior() -> void:
	label.document = _document("Direct DOM", "dom")
	await _settle()
	var behavior: HTMLBehavior = Engine.get_meta(&"dom_behavior", null) as HTMLBehavior
	_check(behavior != null, "_attach_document runs without an HTMLLabel argument")
	if behavior == null:
		return
	var dom: HTMLDocumentNode = behavior.document
	var status: HTMLElement = label.get_element_by_id("behavior-status")
	_check(status != null and status.get_text_content() == "Created by behavior",
		"direct DOM creation is visible to the host")
	_check(dom.query_selector_all("#behavior-status") == [status] and
		dom.get_element_by_id("marker").has_attribute("data-attached"),
		"direct queries share the live DOM and existing handles")
	_check(not Engine.has_meta(&"legacy_behavior_called"), "only the document attach hook runs when both hooks exist")
	label.document = _document("Replacement", "")
	_check(Engine.get_meta(&"dom_behavior_detached") == "Direct DOM", "_detach still has access to the old DOM")
	_check(behavior.document == null, "the instance releases its document after _detach")
	await _settle()
	_check(not dom.is_valid() and dom.query_selector_all("p").is_empty() and dom.create_element("p") == null,
		"stale document handles cannot query or edit the replacement")
	Engine.remove_meta(&"dom_behavior")
	Engine.remove_meta(&"dom_behavior_detached")


func _prepare(host: HTMLLabel) -> void:
	var events: Array[String]
	var instances: Array[WeakRef]
	host.set_meta(&"behavior_events", events)
	host.set_meta(&"instances", instances)


func _events(host: HTMLLabel) -> Array[String]:
	var events: Array[String] = host.get_meta(&"behavior_events")
	return events


func _markup(marker: String, script_name: String) -> String:
	return '<script type="text/gdscript" src="%s"></script><p id="marker">%s</p><button id="test-button" type="button">Test</button>' % [
		script_name,
		marker,
	]


func _document(marker: String, script_name: String, tail: String = "") -> HTMLDocument:
	var document: HTMLDocument = HTMLDocument.new()
	document.text = _markup(marker, script_name)
	if not tail.is_empty():
		document.text += '<script type="text/gdscript" src="%s"></script>' % tail
	return document


func _settle() -> void:
	for _frame: int in range(8):
		await get_tree().process_frame


func _check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		push_error(description)
