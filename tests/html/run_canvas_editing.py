"""Exercise the 2D HTML canvas API, contenteditable, and HTML DataTransfer in Godot."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory

from run_audio import write


def run(command, workspace, expected=None):
    result = subprocess.run(command, env={**os.environ, "APPDATA": str(workspace)}, cwd=workspace,
                            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
    print(result.stdout, end="")
    print(result.stderr, end="")
    if result.returncode or (expected and (expected not in result.stdout or "ERROR:" in result.stderr)):
        raise SystemExit(f"Canvas and editing test command failed (exit {result.returncode}): {command}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    if args.work_dir:
        args.work_dir.mkdir(parents=True, exist_ok=True)
    repo = Path(__file__).resolve().parents[2]
    godot = str(args.godot.resolve())
    with TemporaryDirectory(prefix="dockit-html-canvas-editing-", dir=args.work_dir, ignore_cleanup_errors=True) as directory:
        workspace = Path(directory)
        project = workspace / "project"
        settings = workspace / "Godot"
        settings.mkdir()
        write(settings / "editor_settings-4.7.tres", """[gd_resource type="EditorSettings" format=3]
[resource]
filesystem/on_save/safe_save_on_backup_then_rename = false
""")
        addon = project / "addons/dockit"
        (addon / "bin").mkdir(parents=True)
        shutil.copyfile(repo / "addons/dockit/dockit.gdextension", addon / "dockit.gdextension")
        shutil.copyfile(repo / "addons/dockit/bin/dockit.windows.template_debug.x86_64.dll",
                        addon / "bin/dockit.windows.template_debug.x86_64.dll")
        (project / ".godot").mkdir()
        write(project / ".godot/extension_list.cfg", "res://addons/dockit/dockit.gdextension\n")
        write(project / "project.godot", """config_version=5
[application]
config/name="Dockit Canvas Editing"
run/main_scene="res://main.tscn"
[display]
window/size/viewport_width=520
window/size/viewport_height=300
[rendering]
renderer/rendering_method="gl_compatibility"
shader_compiler/shader_cache/enabled=false
""")
        shutil.copyfile(Path(__file__).with_name("canvas_editing.gd"), project / "canvas_editing.gd")
        write(project / "main.tscn", """[gd_scene load_steps=2 format=3]
[ext_resource type="Script" path="res://canvas_editing.gd" id="1"]
[node name="CanvasEditing" type="Node"]
script = ExtResource("1")
[node name="Label" type="HTMLLabel" parent="."]
offset_right = 420.0
offset_bottom = 240.0
""")
        run([godot, "--headless", "--path", str(project), "--import"], workspace)
        run([godot, "--headless", "--path", str(project), "--quit-after", "180"], workspace,
            "HTML canvas and editing checks passed")


if __name__ == "__main__":
    main()
