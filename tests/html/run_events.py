"""Test DOM event dispatch, cancelable default actions, and document-level delegation."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory

from run_audio import run, write


def run_event_checks(command, workspace):
    result = subprocess.run(command, env={**os.environ, "APPDATA": str(workspace)}, cwd=workspace,
                            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
    print(result.stdout, end="")
    print(result.stderr, end="")
    expected_error = "ERROR: The event is already being dispatched."
    unexpected_errors = [line for line in result.stderr.splitlines()
                         if line.startswith("ERROR:") and line != expected_error]
    if result.returncode or "HTML event checks passed" not in result.stdout or unexpected_errors:
        raise SystemExit(f"Event test command failed (exit {result.returncode}): {command}")


def create_project(project, repo):
    addon = project / "addons/dockit"
    (addon / "bin").mkdir(parents=True)
    for asset in ("dockit.gdextension", "bin/dockit.windows.template_debug.x86_64.dll"):
        shutil.copyfile(repo / "addons/dockit" / asset, addon / asset)
    (project / ".godot").mkdir()
    write(project / ".godot/extension_list.cfg", "res://addons/dockit/dockit.gdextension\n")
    write(project / "project.godot", """config_version=5
[application]
run/main_scene="res://main.tscn"
[display]
window/size/viewport_width=520
window/size/viewport_height=300
[rendering]
renderer/rendering_method="gl_compatibility"
""")
    shutil.copyfile(Path(__file__).with_name("events.gd"), project / "events.gd")
    write(project / "main.tscn", """[gd_scene load_steps=2 format=3]
[ext_resource type="Script" path="res://events.gd" id="1"]
[node name="Events" type="Node"]
script = ExtResource("1")
[node name="Label" type="HTMLLabel" parent="."]
offset_right = 420.0
offset_bottom = 240.0
""")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    godot = str(args.godot.resolve())
    with TemporaryDirectory(prefix="dockit-html-events-", dir=args.work_dir, ignore_cleanup_errors=True) as directory:
        workspace = Path(directory)
        project = workspace / "project"
        settings = workspace / "Godot"
        settings.mkdir()
        write(settings / "editor_settings-4.7.tres", """[gd_resource type="EditorSettings" format=3]
[resource]
filesystem/on_save/safe_save_on_backup_then_rename = false
""")
        create_project(project, repo)
        run([godot, "--headless", "--path", str(project), "--import"], workspace)
        run_event_checks([godot, "--headless", "--path", str(project), "--quit-after", "900"], workspace)


if __name__ == "__main__":
    main()
