"""Run HTML audio regressions in Godot and selected-scene Windows exports."""

import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
from tempfile import TemporaryDirectory
import wave
from math import sin, tau


SUCCESS = "HTML audio checks finished with 0 failure(s)"


def write(path, text):
    path.write_text(text, encoding="utf-8", newline="\n")


def run(command, workspace, expected=None, cwd=None):
    # Use Godot's Windows data-directory lookup within the isolated test project.
    result = subprocess.run(command, env={**os.environ, "APPDATA": str(workspace)}, cwd=cwd, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=120)
    print(result.stdout, end="")
    print(result.stderr, end="")
    if result.returncode or (expected and (expected not in result.stdout or "ERROR:" in result.stderr)):
        raise SystemExit(f"Test command failed: {command}")


def create_project(project, repo, fmod_bin, fmod_runtime, template):
    extension = project / "addons/dockit"
    (extension / "bin").mkdir(parents=True)
    shutil.copyfile(repo / "addons/dockit/dockit.gdextension", extension / "dockit.gdextension")
    library = "bin/dockit.windows.template_debug.x86_64.dll"
    shutil.copyfile(repo / "addons/dockit" / library, extension / library)
    extensions = ["res://addons/dockit/dockit.gdextension"]
    if fmod_bin:
        selected = project / "addons/fmod_player/bin"
        selected.mkdir(parents=True)
        for asset in ("fmod_player.gdextension", "fmod_player.windows.template_debug.x86_64.dll"):
            shutil.copyfile(fmod_bin / asset, selected / asset)
        shutil.copyfile(fmod_runtime, selected / "fmod.dll")
        extensions.append("res://addons/fmod_player/bin/fmod_player.gdextension")
        # Register the plugin's importer before the editor scans the fixture.
        write(selected.parent / "plugin.cfg", """[plugin]
name="FMOD audio fixture"
script="fixture_importer.gd"
""")
        write(selected.parent / "fixture_importer.gd", """@tool
extends EditorPlugin

var importer: EditorImportPlugin


func _enter_tree() -> void:
	importer = ClassDB.instantiate(&"AudioImporterFmod") as EditorImportPlugin
	add_import_plugin(importer)


func _exit_tree() -> void:
	remove_import_plugin(importer)
""")
    (project / ".godot").mkdir()
    write(project / ".godot/extension_list.cfg", "\n".join(extensions) + "\n")
    fmod_settings = """
[editor_plugins]
enabled=PackedStringArray("res://addons/fmod_player/plugin.cfg")

[audio]
fmod/sync_godot_audio_buses=false
""" if fmod_bin else ""
    write(project / "project.godot", """config_version=5

[application]
config/name="Dockit HTML Audio"
run/main_scene="res://main.tscn"

[debug]
file_logging/enable_file_logging=false

[rendering]
renderer/rendering_method="gl_compatibility"
shader_compiler/shader_cache/enabled=false
""" + fmod_settings)
    write(project / "export_presets.cfg", """[preset.0]
name="Windows"
platform="Windows Desktop"
runnable=true
export_filter="scenes"
include_filter="*.gdextension"
exclude_filter=""
export_files=PackedStringArray("res://main.tscn")

[preset.0.options]
custom_template/debug="%s"
binary_format/architecture="x86_64"
binary_format/embed_pck=false
application/modify_resources=false
""" % template.as_posix())
    (project / "media").mkdir()
    (project / "pages").mkdir()
    frames = bytearray()
    for i in range(48000 * 3):
        frames.extend(struct.pack("<h", int(2600 * sin(tau * 440 * i / 48000))))
    with wave.open(str(project / "media/tone.wav"), "wb") as tone:
        tone.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
        tone.writeframes(frames)
    if fmod_bin:
        write(project / "media/tone.wav.import", """[remap]
importer="fmod_audio"
type="FmodAudioStream"

[deps]
source_file="res://media/tone.wav"

[params]
""")
    write(project / "pages/player.html", """<!doctype html>
<style>body { margin: 0 } audio { display: block }</style>
<div id="root">
  <audio id="with_controls" controls>
    <source src="missing.wav">
    <source src="../media/tone.wav">
  </audio>
  <audio id="hidden" src="../media/tone.wav" autoplay></audio>
  <audio id="unavailable" src="missing.wav"></audio>
</div>
""")
    shutil.copyfile(repo / "tests/html/audio.gd", project / "audio.gd")
    write(project / "main.tscn", """[gd_scene load_steps=3 format=3]

[ext_resource type="Script" path="res://audio.gd" id="1"]
[ext_resource type="HTMLDocument" path="res://pages/player.html" id="2"]

[node name="Test" type="Node"]
script = ExtResource("1")

[node name="HTMLLabel" type="HTMLLabel" parent="."]
offset_right = 420.0
offset_bottom = 230.0
document = ExtResource("2")
use_fmod_bus = %s
""" % ("true" if fmod_bin else "false"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("--fmod-bin", type=Path, help="FMOD add-on bin directory containing fmod.dll")
    parser.add_argument("--fmod-runtime", type=Path, help="path to fmod.dll if it is separate from the add-on")
    parser.add_argument("--backend", choices=("godot", "fmod", "both"), default="godot")
    parser.add_argument("--skip-editor", action="store_true", help="run only the exported game")
    parser.add_argument("--work-dir", type=Path, help="parent directory for temporary test projects")
    parser.add_argument("--template", type=Path, default=Path(os.environ["APPDATA"]) /
                        "Godot/export_templates/4.7.2.stable/windows_debug_x86_64.exe")
    args = parser.parse_args()
    if not args.template.is_file():
        parser.error("Install the Windows x64 debug export template or pass --template.")
    if args.backend in ("fmod", "both") and not args.fmod_bin:
        parser.error("--fmod-bin is required for the FMOD backend")
    repo = Path(__file__).resolve().parents[2]
    backends = ("godot", "fmod") if args.backend == "both" else (args.backend,)
    godot = str(args.godot.resolve())
    for backend in backends:
        with TemporaryDirectory(prefix=f"dockit-audio-{backend}-", dir=args.work_dir) as directory:
            workspace = Path(directory)
            project = workspace / "project"
            output = workspace / "export"
            output.mkdir()
            settings = workspace / "Godot"
            settings.mkdir()
            write(settings / "editor_settings-4.7.tres", """[gd_resource type="EditorSettings" format=3]

[resource]
filesystem/on_save/safe_save_on_backup_then_rename = false
""")
            fmod_bin = args.fmod_bin if backend == "fmod" else None
            fmod_runtime = args.fmod_runtime or (fmod_bin / "fmod.dll" if fmod_bin else None)
            create_project(project, repo, fmod_bin, fmod_runtime, args.template.resolve())
            run([godot, "--headless", "--path", str(project), "--import"], workspace)
            sidecar = (project / "media/tone.wav.import").read_text(encoding="utf-8")
            expected_type = "FmodAudioStream" if backend == "fmod" else "AudioStreamWAV"
            if f'type="{expected_type}"' not in sidecar:
                raise SystemExit(f"Wrong audio import type in {backend}:\n{sidecar}")
            uid = re.search(r'^uid="(uid://[^"]+)"', sidecar, re.MULTILINE)
            if not uid:
                raise SystemExit(f"No UID for the {backend} audio fixture")
            html = project / "pages/player.html"
            write(html, html.read_text(encoding="utf-8").replace("../media/tone.wav", uid[1]))
            run([godot, "--headless", "--path", str(project), "--import"], workspace)
            if not args.skip_editor:
                for renderer in ("gl_compatibility", "forward_plus"):
                    run([godot, "--path", str(project), "--rendering-method", renderer, "--quit-after", "600"], workspace, SUCCESS)
            exe = output / "Audio.exe"
            run([godot, "--headless", "--path", str(project), "--export-debug", "Windows", str(exe)], workspace)
            console = output / "Audio.console.exe"
            run([str(console if console.exists() else exe), "--quit-after", "600"], workspace, SUCCESS, cwd=output)


if __name__ == "__main__":
    main()
