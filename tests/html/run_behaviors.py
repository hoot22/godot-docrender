"""Test HTMLBehavior registration, lifecycle, editor isolation, and Windows export."""

import argparse
import os
from pathlib import Path
import shutil
from tempfile import TemporaryDirectory

from run_audio import run, write


def create_project(project, repo, template):
    addon = project / "addons/dockit"
    (addon / "bin").mkdir(parents=True)
    for asset in ("dockit.gdextension", "bin/dockit.windows.template_debug.x86_64.dll"):
        shutil.copyfile(repo / "addons/dockit" / asset, addon / asset)
    (project / ".godot").mkdir()
    write(project / ".godot/extension_list.cfg", "res://addons/dockit/dockit.gdextension\n")
    write(project / "project.godot", """config_version=5
[application]
run/main_scene="uid://cc5pvpfm05j3y"
[display]
window/size/viewport_width=500
window/size/viewport_height=240
[rendering]
renderer/rendering_method="gl_compatibility"
""")
    # A script loaded only by an HTML src must be selected explicitly for export.
    write(project / "export_presets.cfg", f"""[preset.0]
name="Windows"
platform="Windows Desktop"
runnable=true
export_filter="resources"
include_filter="*.gdextension"
exclude_filter=""
export_files=PackedStringArray("res://main.tscn", "res://unregistered.gd")

[preset.0.options]
custom_template/debug="{template.as_posix()}"
binary_format/architecture="x86_64"
application/modify_resources=false
""")
    write(project / "index.html", """<!doctype html>
<html><head>
<script id="first" type="text/gdscript" src="direct"></script>
<script type=" TEXT/GDSCRIPT " src=" direct "></script>
<script type="text/gdscript" src="indirect"></script>
<script type="text/gdscript" src="wrong"></script>
<script type="text/gdscript" src="required"></script>
<script type="text/gdscript" src="missing"></script>
<script type="text/gdscript" src="res://unregistered.gd"></script>
<script type="text/gdscript" src="https://example.invalid/script.gd"></script>
<script type="application/json" src="direct">{"data":true}</script>
</head><body>
<template><script type="text/gdscript" src="direct"></script></template>
<svg><script type="text/gdscript" src="direct"></script></svg>
<p id="marker">A</p><button id="test-button" type="button">Test</button>
</body></html>
""")
    write(project / "behavior.gd.uid", "uid://ct6udxxgpt6ev\n")
    write(project / "behavior.gd", """@tool
class_name TestHTMLBehavior
extends HTMLBehavior

var _host: HTMLLabel


func _attach(label: HTMLLabel) -> void:
	_host = label
	_log("attach:" + _kind() + ":" + _marker())
	var instances: Array[WeakRef] = _host.get_meta(&"instances")
	instances.append(weakref(self))
	_host.button_pressed.connect(_on_button_pressed)


func _detach() -> void:
	_log("detach:" + _kind() + ":" + _marker())
	_host.button_pressed.disconnect(_on_button_pressed)


func _marker() -> String:
	return document.get_element_by_id("marker").get_text_content()


func _kind() -> String:
	return "direct"


func _log(entry: String) -> void:
	var events: Array[String] = _host.get_meta(&"behavior_events")
	events.append(entry)


func _on_button_pressed(_element: HTMLElement) -> void:
	_log("button:" + _kind())
""")
    write(project / "indirect.gd", """extends TestHTMLBehavior


func _kind() -> String:
	return "indirect"
""")
    write(project / "swap.gd", """extends TestHTMLBehavior


func _kind() -> String:
	return "swap"


func _attach(label: HTMLLabel) -> void:
	super(label)
	label.document = label.get_meta(&"replacement") as HTMLDocument
	_log("after_swap:" + _marker())
""")
    write(project / "swap_on_detach.gd", """extends TestHTMLBehavior


func _kind() -> String:
	return "swap_on_detach"


func _detach() -> void:
	super()
	_host.document = _host.get_meta(&"replacement") as HTMLDocument
	_log("after_detach_swap:" + _marker())
""")
    write(project / "wrong.gd", """extends RefCounted


func _init() -> void:
	Engine.set_meta(&"wrong_instantiated", true)
""")
    write(project / "unregistered.gd", """extends TestHTMLBehavior


static func _static_init() -> void:
	Engine.set_meta(&"unregistered_loaded", true)


func _kind() -> String:
	return "path"
""")
    write(project / "required.gd", """extends HTMLBehavior


func _init(_required: String) -> void:
	pass
""")
    write(project / "document_behavior.gd", """extends HTMLBehavior


func _attach_document(dom: HTMLDocumentNode) -> void:
	var target: HTMLElement = dom.query_selector("#marker")
	if document != dom or target != dom.get_element_by_id("marker") or dom.document_element.get_tag_name() != "html":
		return
	var status: HTMLElement = dom.create_element("span")
	status.set_attribute("id", "behavior-status")
	status.append([dom.create_text_node("Created by behavior")])
	var fragment: HTMLDocumentFragment = dom.create_document_fragment()
	fragment.append_child(status)
	dom.body.append_fragment(fragment)
	target.set_attribute("data-attached", "true")
	Engine.set_meta(&"dom_behavior", self)


func _attach(_label: HTMLLabel) -> void:
	Engine.set_meta(&"legacy_behavior_called", true)


func _detach() -> void:
	var target: HTMLElement = document.get_element_by_id("marker")
	Engine.set_meta(&"dom_behavior_detached", target.get_text_content())
""")
    shutil.copyfile(Path(__file__).with_name("behaviors.gd"), project / "behaviors.gd")
    write(project / "main.tscn", """[gd_scene load_steps=10 format=3 uid="uid://cc5pvpfm05j3y"]
[ext_resource type="Script" path="res://behaviors.gd" id="1"]
[ext_resource type="HTMLDocument" path="res://index.html" id="2"]
[ext_resource type="Script" uid="uid://ct6udxxgpt6ev" path="res://behavior.gd" id="3"]
[ext_resource type="Script" path="res://indirect.gd" id="4"]
[ext_resource type="Script" path="res://swap.gd" id="5"]
[ext_resource type="Script" path="res://swap_on_detach.gd" id="6"]
[ext_resource type="Script" path="res://wrong.gd" id="7"]
[ext_resource type="Script" path="res://required.gd" id="8"]
[ext_resource type="Script" path="res://document_behavior.gd" id="9"]
[node name="Behaviors" type="Node"]
script = ExtResource("1")
[node name="Label" type="HTMLLabel" parent="."]
offset_right = 440.0
offset_bottom = 180.0
document = ExtResource("2")
behavior_scripts = Dictionary[StringName, GDScript]({&"direct": ExtResource("3"), &"indirect": ExtResource("4"), &"swap": ExtResource("5"), &"swap_on_detach": ExtResource("6"), &"wrong": ExtResource("7"), &"required": ExtResource("8"), &"dom": ExtResource("9")})
""")


def check_editor(godot, project, workspace):
    addon = project / "addons/behavior_fixture"
    addon.mkdir()
    write(addon / "plugin.cfg", """[plugin]
name="HTML behavior fixture"
script="editor_check.gd"
""")
    write(addon / "editor_check.gd", """@tool
extends EditorPlugin


func _enter_tree() -> void:
	_check_preview.call_deferred()


func _check_preview() -> void:
	var preview: HTMLLabel = HTMLLabel.new()
	var events: Array[String]
	var instances: Array[WeakRef]
	preview.set_meta(&"behavior_events", events)
	preview.set_meta(&"instances", instances)
	preview.behavior_scripts = {&"direct": TestHTMLBehavior}
	preview.behaviors_enabled = true
	preview.text = '<script type="text/gdscript" src="direct"></script><p id="marker">Editor</p>'
	preview.size = Vector2(300.0, 120.0)
	get_editor_interface().get_base_control().add_child(preview)
	for _frame: int in range(8):
		await get_tree().process_frame
	var success: bool = preview.get_element_by_id("marker") != null and events.is_empty()
	if success:
		print("HTML behavior editor checks passed")
	preview.queue_free()
	get_tree().quit(0 if success else 1)
""")
    config = project / "project.godot"
    text = config.read_text(encoding="utf-8")
    try:
        write(config, text + '\n[editor_plugins]\nenabled=PackedStringArray("res://addons/behavior_fixture/plugin.cfg")\n')
        run([godot, "--headless", "--editor", "--path", str(project), "--quit-after", "360"],
            workspace, "HTML behavior editor checks passed")
    finally:
        write(config, text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--template", type=Path, default=Path(os.environ["APPDATA"]) /
                        "Godot/export_templates/4.7.2.stable/windows_debug_x86_64.exe")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    godot = str(args.godot.resolve())
    with TemporaryDirectory(prefix="dockit-html-behaviors-", dir=args.work_dir, ignore_cleanup_errors=True) as directory:
        workspace = Path(directory)
        project, export = workspace / "project", workspace / "export"
        export.mkdir()
        settings = workspace / "Godot"
        settings.mkdir()
        write(settings / "editor_settings-4.7.tres", """[gd_resource type="EditorSettings" format=3]
[resource]
filesystem/on_save/safe_save_on_backup_then_rename = false
""")
        create_project(project, repo, args.template.resolve())
        run([godot, "--headless", "--path", str(project), "--import"], workspace)
        run([godot, "--headless", "--path", str(project), "--quit-after", "360"],
            workspace, "HTML behavior checks passed")
        check_editor(godot, project, workspace)
        executable = export / "Behavior.exe"
        run([godot, "--headless", "--path", str(project), "--export-debug", "Windows", str(executable)], workspace)
        console = export / "Behavior.console.exe"
        run([str(console if console.exists() else executable), "--headless", "--quit-after", "360"],
            workspace, "HTML behavior checks passed", cwd=export)


if __name__ == "__main__":
    main()
