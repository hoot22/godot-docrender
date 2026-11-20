extends Node

var failures: int
@onready var label: HTMLLabel = $Label


func _ready() -> void:
	label.text = """<div id="host"><template shadowrootmode="open">
		<style>span { display:block; width:100px; height:20px; }</style>
		<slot name="title">Fallback</slot><slot></slot>
		<span id="private">Private</span></template>
		<b id="title" slot="title">Title</b><i id="body">Body</i></div>"""
	await _settle()
	var host: HTMLElement = label.get_element_by_id("host")
	var title: HTMLElement = label.get_element_by_id("title")
	var body: HTMLElement = label.get_element_by_id("body")
	var root: HTMLShadowRoot = host.shadow_root
	_check(root != null, "declarative root exists")
	if root == null:
		get_tree().quit(1)
		return
	_check(label.get_element_by_id("private") == null, "document queries exclude shadow content")
	var private_node: HTMLElement = root.get_element_by_id("private")
	_check(private_node != null and private_node.is_connected(), "shadow nodes are connected")
	_check(private_node.get_parent_node() == root, "shadow parent is a root")
	_check(root.get_parent_node() == null, "shadow root has no DOM parent")
	_check(host.append_fragment(root) == ERR_INVALID_PARAMETER, "shadow root cannot be consumed as a fragment")
	_check(root.clone_node(true) == null, "shadow root cannot be cloned directly")
	_check(title.get_parent() == host and body.get_parent() == host, "projection preserves DOM parents")
	var named: HTMLSlotElement = root.query_selector("slot[name]") as HTMLSlotElement
	_check(named.assigned_elements() == [title], "named assignment")
	_check(title.assigned_slot == named, "assigned_slot returns the matching slot")
	_check(private_node.get_bounding_rect().size == Vector2(100.0, 20.0), "shadow stylesheet applies during layout")
	var changes: Array[int] = []
	named.add_event_listener(&"slotchange", func(_event: HTMLEvent) -> void: changes.append(1))
	title.set_attribute("slot", "missing")
	_check(named.assigned_nodes().is_empty(), "assignment updates synchronously")
	await _settle()
	_check(changes.size() == 1, "slotchange batches delivery")
	_check(named.assigned_nodes(true).size() == 1, "flatten returns fallback text")
	var inside: Array[HTMLEventTarget] = []
	var outside: Array[HTMLEventTarget] = []
	private_node.add_event_listener(&"probe", func(event: HTMLEvent) -> void: inside.append(event.target))
	host.add_event_listener(&"probe", func(event: HTMLEvent) -> void: outside.append(event.target))
	var event: HTMLEvent = HTMLEvent.new()
	event.type = &"probe"
	event.bubbles = true
	private_node.dispatch_event(event)
	_check(inside.size() == 1 and outside.is_empty(), "non-composed event stops at shadow boundary")
	event.composed = true
	private_node.dispatch_event(event)
	_check(outside == [host], "composed event retargets to host")
	var manual_host: HTMLElement = label.create_element("x-manual")
	host.append_child(manual_host)
	var manual: HTMLShadowRoot = manual_host.attach_shadow("closed", "manual")
	var slot_node: HTMLSlotElement = label.create_element("slot") as HTMLSlotElement
	manual.append_child(slot_node)
	var content: HTMLElement = label.create_element("p")
	manual_host.append_child(content)
	_check(slot_node.assign([content]) == OK, "manual assignment accepted")
	_check(slot_node.assigned_elements() == [content], "manual assignment returns original node")
	_check(manual_host.shadow_root == null and content.assigned_slot == null, "closed root is hidden")
	var component_script: GDScript = GDScript.new()
	component_script.source_code = """extends HTMLComponent
func _mount() -> void:
	host.set_attribute("data-mounted", "yes")
	var root: HTMLShadowRoot = host.attach_shadow()
	var text: HTMLTextNode = document.create_text_node("Component content")
	root.append_child(text)


func _connected() -> void:
	host.set_attribute("data-connected", "yes")


func _disconnected() -> void:
	host.set_attribute("data-connected", "no")
"""
	_check(component_script.reload() == OK, "component script compiles")
	var registry: Dictionary[StringName, GDScript] = {
		&"x-test": component_script,
	}
	label.component_scripts = registry
	label.behaviors_enabled = true
	var component_host: HTMLElement = label.create_element("x-test")
	host.append_child(component_host)
	await _settle()
	_check(component_host.get_attribute("data-mounted") == "yes", "dynamic component mounts")
	_check(component_host.get_attribute("data-connected") == "yes", "component connects")
	_check(component_host.shadow_root != null, "component creates a root")
	component_host.detach()
	await _settle()
	_check(component_host.get_attribute("data-connected") == "no", "component disconnects")
	host.append_child(component_host)
	await _settle()
	_check(component_host.get_attribute("data-connected") == "yes", "component reconnects")
	label.text = """<script type="text/gdscript" component="x-test" src="uid://cbi5udkegh7oy"></script>
		<script type="text/gdscript" component="x-test" src="uid://cbi5udkegh7oy"></script>
		<x-test id="declared"></x-test>"""
	await _settle()
	var declared: HTMLElement = label.get_element_by_id("declared")
	_check(declared.get_attribute("data-definition") == "document", "document declaration overrides label registry")
	_check(declared.get_attribute("data-mounted").is_empty(), "overridden label component was not mounted")
	var diagnostics: Array[Dictionary] = label.get_component_diagnostics()
	_check(diagnostics.size() == 1 and diagnostics[0].get("reason") == "duplicate_declaration", "duplicate declaration reports a diagnostic")
	if failures == 0:
		print("HTML shadow DOM checks passed")
	get_tree().quit(0 if failures == 0 else 1)


func _settle() -> void:
	for index: int in range(8):
		await get_tree().process_frame


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error(message)
