"""Check imported document references, editor tab restoration and label refresh."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory


PROBE = '''@tool
extends EditorPlugin

const SOURCE_MARKERS: Array[String] = [
	"Shared document",
	"LaTeX reference",
	"LaTeX 3D reference",
	"Mermaid MMD reference",
	"Mermaid long extension reference",
]


func _enter_tree() -> void:
	if OS.get_environment("DOCKIT_REFERENCE_PROBE") == "1":
		_probe.call_deferred()


func _probe() -> void:
	var scene: Node
	var restored_sources: Dictionary[String, bool] = {}
	for frame: int in range(240):
		await get_tree().process_frame
		scene = EditorInterface.get_edited_scene_root()
		for editor: ScriptEditorBase in EditorInterface.get_script_editor().get_open_script_editors():
			var source: CodeEdit = editor.get_base_editor() as CodeEdit
			if editor.is_class("TextEditor") and source != null:
				for marker: String in SOURCE_MARKERS:
					if source.text.contains(marker):
						restored_sources[marker] = true
		if scene != null and scene.has_node("Second") and restored_sources.size() == SOURCE_MARKERS.size():
			break
	if scene == null or not scene.has_node("Second") or restored_sources.size() != SOURCE_MARKERS.size():
		push_error("Editor did not restore the fixture scene and source tabs: %s" % [restored_sources])
		get_tree().quit(1)
		return
	for frame: int in range(4):
		await get_tree().process_frame
	var first: HTMLLabel = scene.get_node("First") as HTMLLabel
	var second: HTMLLabel = scene.get_node("Second") as HTMLLabel
	var latex: LatexLabel = scene.get_node("Formula") as LatexLabel
	var latex_3d: LatexLabel3D = scene.get_node("Formula3D") as LatexLabel3D
	var mermaid: MermaidLabel = scene.get_node("Diagram") as MermaidLabel
	var packed: PackedScene = PackedScene.new()
	if packed.pack(scene) != OK or ResourceSaver.save(packed, "res://roundtrip.tscn") != OK:
		push_error("Could not save the restored scene")
		get_tree().quit(1)
		return
	var saved: String = FileAccess.get_file_as_string("res://roundtrip.tscn")
	var embedded: bool = '[sub_resource type="HTMLDocument"' in saved and 'text = "Embedded document"' in saved
	if first.document != second.document or not embedded:
		push_error("Editor changed HTML resource sharing or its embedded resource classification")
		get_tree().quit(1)
		return
	var expected: Dictionary[String, Resource] = {
		"res://source.html": first.document,
		"res://source.tex": latex.document,
		"res://source_3d.tex": latex_3d.document,
		"res://source.mmd": mermaid.document,
		"res://other.mermaid": scene.get(&"other_document") as Resource,
	}
	var passed: bool = true
	for path: String in expected:
		var document: Resource = expected[path]
		if document == null:
			push_error("Editor did not restore the document at %s" % path)
			passed = false
			continue
		var external: bool = false
		for line: String in saved.split("\\n", false):
			if line.begins_with('[ext_resource type="%s"' % document.get_class()) and 'path="%s"' % path in line:
				external = true
				break
		var cache_matches: bool = ResourceLoader.get_cached_ref(path) == document
		if document.resource_path != path or not cache_matches or not external:
			push_error("Changed reference for %s (resource_path=%s, cache_matches=%s, external=%s)" % [
				path, document.resource_path, cache_matches, external,
			])
			passed = false
	if not passed or not await _check_document_refresh(latex, latex_3d, mermaid):
		get_tree().quit(1)
		return
	var merman_status: String = "passed" if MermaidRender.is_available() else "unavailable"
	print("Dockit editor source resource references passed. Merman rendering: %s." % merman_status)
	get_tree().quit()


func _check_document_refresh(latex: LatexLabel, latex_3d: LatexLabel3D, mermaid: MermaidLabel) -> bool:
	var latex_document: LatexDocument = latex.document
	var latex_3d_document: LatexDocument = latex_3d.document
	var mermaid_document: MermaidDocument = mermaid.document
	var has_merman: bool = MermaidRender.is_available()
	var initial_width: float = latex.get_minimum_size().x
	if initial_width <= 0.0 or not latex.get_render_error().is_empty():
		push_error("The imported LaTeX expression did not render")
		return false
	var quad: QuadMesh = latex_3d.mesh as QuadMesh
	if quad == null or quad.size.x <= 0.0 or not latex_3d.get_render_error().is_empty():
		push_error("The imported LaTeX expression did not generate a 3D quad")
		return false
	var initial_width_3d: float = quad.size.x
	if has_merman and not await _wait_for_diagram(mermaid):
		return false

	latex_document.set(&"text", "")
	latex_3d_document.set(&"text", "")
	mermaid_document.set(&"text", "")
	await get_tree().process_frame
	var empty_width: float = latex.get_minimum_size().x
	if empty_width >= initial_width or mermaid.get_texture() != null or not mermaid.get_render_error().is_empty():
		push_error("The labels did not refresh when the document resources changed")
		return false
	if latex_3d.mesh != null or not latex_3d.get_render_error().is_empty():
		push_error("LatexLabel3D kept its mesh after the document was emptied")
		return false

	var edited_formula: String = "x + y + z + 1234567890\\n"
	var edited_diagram: String = "flowchart LR\\n  A[Reimported] --> B[Diagram]\\n"
	var edited_sources: Dictionary[String, String] = {
		latex_document.resource_path: edited_formula,
		latex_3d_document.resource_path: edited_formula,
		mermaid_document.resource_path: edited_diagram,
	}
	for path: String in edited_sources:
		var file: FileAccess = FileAccess.open(path, FileAccess.WRITE)
		if file == null or not file.store_string(edited_sources[path]):
			push_error("Could not update %s for reimport" % path)
			return false
		file.close()
	EditorInterface.get_resource_filesystem().reimport_files(PackedStringArray(edited_sources.keys()))
	await get_tree().process_frame
	if latex.document != latex_document or latex_document.get_text() != edited_formula \\
			or latex_3d.document != latex_3d_document or latex_3d_document.get_text() != edited_formula \\
			or mermaid.document != mermaid_document or mermaid_document.get_text() != edited_diagram:
		push_error("Reimport did not update the same document resources")
		return false
	var reimported_width: float = latex.get_minimum_size().x
	if reimported_width <= initial_width or not latex.get_render_error().is_empty():
		push_error("The reimported LaTeX expression did not render")
		return false
	quad = latex_3d.mesh as QuadMesh
	if quad == null or quad.size.x <= initial_width_3d or not latex_3d.get_render_error().is_empty():
		push_error("LatexLabel3D did not expand its quad after reimport")
		return false
	var reimported_width_3d: float = quad.size.x
	if has_merman and not await _wait_for_diagram(mermaid):
		return false

	var previous_diagram: Texture2D = mermaid.get_texture()
	latex.document = null
	latex_3d.document = null
	mermaid.document = null
	await get_tree().process_frame
	var fallback_width: float = latex.get_minimum_size().x
	if latex.text != "z" or mermaid.text != "flowchart LR; A[Fallback]" \\
			or fallback_width <= empty_width or fallback_width >= reimported_width:
		push_error("Clearing the document did not restore the local text")
		return false
	quad = latex_3d.mesh as QuadMesh
	if latex_3d.text != "w" or quad == null or quad.size.x <= 0.0 or quad.size.x >= reimported_width_3d:
		push_error("Clearing the LatexLabel3D document did not restore the local formula")
		return false
	if has_merman and (not await _wait_for_diagram(mermaid) or mermaid.get_texture() == previous_diagram):
		push_error("Clearing the Mermaid document did not render the local diagram")
		return false
	return true


func _wait_for_diagram(label: MermaidLabel) -> bool:
	var deadline: int = Time.get_ticks_msec() + 5000
	while label.get_texture() == null and label.get_render_error().is_empty() and Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
	if label.get_texture() == null:
		push_error("The Mermaid diagram did not render: %s" % label.get_render_error())
		return false
	return true
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path, help="path to the Godot 4.7.2 console executable")
    args = parser.parse_args()
    godot = args.godot.resolve()
    addon = Path(__file__).resolve().parents[2] / "addons/dockit"
    library = addon / "bin/dockit.windows.template_debug.x86_64.dll"
    if not godot.is_file() or not library.is_file():
        parser.error("Use the Godot console executable and build the Windows x64 template_debug target first.")

    with TemporaryDirectory(prefix="dockit-editor-reference-", ignore_cleanup_errors=True) as folder:
        root = Path(folder)
        project = root / "project"
        profile = root / "profile"
        for directory in (project / "addons/dockit/bin", project / "addons/probe", project / ".godot/editor", profile):
            directory.mkdir(parents=True)
        shutil.copyfile(library, project / "addons/dockit/bin" / library.name)
        shutil.copyfile(addon / "dockit.gdextension", project / "addons/dockit/dockit.gdextension")

        def write(path, contents):
            (project / path).write_text(contents, encoding="utf-8", newline="\n")

        write("project.godot", 'config_version=5\n[application]\nconfig/name="Dockit editor reference test"\n'
              '[editor_plugins]\nenabled=PackedStringArray("res://addons/probe/plugin.cfg")\n')
        write(".godot/extension_list.cfg", "res://addons/dockit/dockit.gdextension\n")
        write("addons/probe/plugin.cfg", '[plugin]\nname="Reference Probe"\nscript="probe.gd"\n')
        write("addons/probe/probe.gd", PROBE)
        write("source.html", "<p>Shared document</p>\n")
        write("source.tex", "% LaTeX reference\nx + y\n")
        write("source_3d.tex", "% LaTeX 3D reference\na + b\n")
        write("source.mmd", "%% Mermaid MMD reference\ngraph TD\n  A --> B\n")
        write("other.mermaid", "%% Mermaid long extension reference\ngraph TD\n  C --> D\n")
        write("document_references.gd", '''extends Control

@export var other_document: MermaidDocument
''')
        env = {**os.environ, "APPDATA": str(profile), "DOCKIT_REFERENCE_PROBE": "0"}
        command = [str(godot), "--headless", "--path", str(project)]
        imported = subprocess.run([*command, "--import"], env=env, cwd=project, timeout=60, capture_output=True)
        sources = ("source.html", "source.tex", "source_3d.tex", "source.mmd", "other.mermaid")
        if imported.returncode or not all((project / f"{source}.import").is_file() for source in sources):
            print(imported.stdout.decode(errors="replace"), imported.stderr.decode(errors="replace"))
            return imported.returncode or 1

        write("main.tscn", '''[gd_scene format=3]

[ext_resource type="HTMLDocument" path="res://source.html" id="1_external"]
[ext_resource type="Script" path="res://document_references.gd" id="2_script"]
[ext_resource type="LatexDocument" path="res://source.tex" id="3_latex"]
[ext_resource type="MermaidDocument" path="res://source.mmd" id="4_mermaid"]
[ext_resource type="MermaidDocument" path="res://other.mermaid" id="5_other"]
[ext_resource type="LatexDocument" path="res://source_3d.tex" id="6_latex_3d"]

[sub_resource type="HTMLDocument" id="HTMLDocument_embedded"]
text = "Embedded document"

[node name="Root" type="Control"]
script = ExtResource("2_script")
other_document = ExtResource("5_other")

[node name="First" type="HTMLLabel" parent="."]
document = ExtResource("1_external")

[node name="Second" type="HTMLLabel" parent="."]
document = ExtResource("1_external")

[node name="Embedded" type="HTMLLabel" parent="."]
document = SubResource("HTMLDocument_embedded")

[node name="Formula" type="LatexLabel" parent="."]
text = "z"
document = ExtResource("3_latex")

[node name="Formula3D" type="LatexLabel3D" parent="."]
text = "w"
document = ExtResource("6_latex_3d")

[node name="Diagram" type="MermaidLabel" parent="."]
text = "flowchart LR; A[Fallback]"
document = ExtResource("4_mermaid")
''')
        write(".godot/editor/editor_layout.cfg", '[EditorNode]\nopen_scenes=PackedStringArray("res://main.tscn")\n'
              'current_scene="res://main.tscn"\n[ScriptEditor]\n'
              'open_scripts=["res://source.html", "res://source.tex", "res://source_3d.tex", "res://source.mmd", "res://other.mermaid"]\n'
              'selected_script="res://source.html"\n')
        result = subprocess.run([*command, "--editor", "--quit-after", "600"],
                                env={**env, "DOCKIT_REFERENCE_PROBE": "1"}, cwd=project,
                                timeout=60, capture_output=True)
        output = result.stdout.decode(errors="replace")
        summary = next((line for line in output.splitlines() if "Dockit editor source resource references passed." in line), None)
        if result.returncode or summary is None:
            print(output, result.stderr.decode(errors="replace"))
            return result.returncode or 1
        print(summary)
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
