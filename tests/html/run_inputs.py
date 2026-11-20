"""Exercise the HTML input types and the imported image submit button in Windows exports."""

import argparse
import os
from pathlib import Path
import shutil
from tempfile import TemporaryDirectory

from run_audio import run, write


def create_project(project, repo, template):
    addon = project / "addons/dockit"
    (addon / "bin").mkdir(parents=True)
    shutil.copyfile(repo / "addons/dockit/dockit.gdextension", addon / "dockit.gdextension")
    library = "bin/dockit.windows.template_debug.x86_64.dll"
    shutil.copyfile(repo / "addons/dockit" / library, addon / library)
    (project / ".godot").mkdir()
    write(project / ".godot/extension_list.cfg", "res://addons/dockit/dockit.gdextension\n")
    write(project / "project.godot", """config_version=5
[application]
config/name="Dockit Input Tests"
run/main_scene="uid://bjfh2y6885rsu"
[display]
window/size/viewport_width=760
window/size/viewport_height=500
[rendering]
renderer/rendering_method="gl_compatibility"
shader_compiler/shader_cache/enabled=false
""")
    write(project / "export_presets.cfg", f"""[preset.0]
name="Windows"
platform="Windows Desktop"
runnable=true
export_filter="scenes"
include_filter="*.gdextension"
exclude_filter=""
export_files=PackedStringArray("res://main.tscn")
[preset.0.options]
custom_template/debug="{template.as_posix()}"
binary_format/architecture="x86_64"
application/modify_resources=false
""")
    write(project / "main.tscn", """[gd_scene load_steps=3 format=3 uid="uid://bjfh2y6885rsu"]

[ext_resource type="Script" path="res://inputs.gd" id="1"]
[ext_resource type="HTMLDocument" path="res://inputs.html" id="2"]

[node name="Inputs" type="Node"]
script = ExtResource("1")

[node name="HTMLLabel" type="HTMLLabel" parent="."]
offset_right = 760.0
offset_bottom = 500.0
document = ExtResource("2")
""")
    write(project / "submit.svg", '<svg xmlns="http://www.w3.org/2000/svg" width="48" height="28">'
          '<rect width="48" height="28" fill="#00ff00"/></svg>')
    write(project / "inputs.html", """<!doctype html>
<style>
form { display: grid; grid-template-columns: repeat(3, 225px); row-gap: 8px; align-items: start; }
#checkbox, #radio { width: 24px; height: 24px; accent-color: #0075ff; }
#select option { color: transparent; }
#eyedropper-target { position: absolute; left: 628px; top: 375px; width: 110px; height: 100px;
  background: linear-gradient(90deg, #13579b 50%, #37a6cd 50%); }
</style>
<form id="form">
<input id="text" type="text" name="text" pattern="[A-Z]{3}" required>
<input id="search" type="search">
<input id="tel" type="tel">
<input id="url" type="url">
<input id="email" type="email" multiple>
<input id="password" type="password">
<input id="number" type="number" min="0" max="10" step="2">
<input id="range" type="range" list="ticks">
<input id="date" type="date" lang="zh-CN" value="2024-02-29">
<input id="month" type="month" value="1969-12">
<input id="week" type="week" value="2020-W53">
<input id="time" type="time" lang="en-GB" value="23:59:59.999" step="0.001">
<input id="datetime-local" type="datetime-local" lang="zh-CN" value="2024-03-31 03:12:00">
<input id="color" type="color" list="colors">
<input id="file" type="file" accept=".txt,text/plain" name="upload" required>
<input id="checkbox" type="checkbox">
<input id="radio" type="radio" name="radio">
<input id="button" type="button" value="Custom">
<input id="submit" type="submit">
<input id="reset" type="reset">
<input id="image" name="photo" type="image" src="submit.svg" alt="Save" formnovalidate
       style="transform: translate(12px, 4px) scale(1.5); transform-origin: 0 0">
<input id="hidden" name="hidden" type="hidden" value="token">
<input id="unknown" type="invalid">
<select id="select"><option value="a">One</option><option value="b">Two</option></select>
</form>
<datalist id="ticks"><option value="0"><option value="25"><option value="100"></datalist>
<datalist id="colors"><option value="red"><option value="blue"></datalist>
<div><input id="swatch" type="color" value="#e66465"> <label for="swatch">Head<i id="swatch-baseline"
 style="display:inline-block; width:1px; height:1px"></i></label></div>
<div><input id="clock" type="time" lang="en-GB" value="08:20" min="00:05" step="900">
<input id="clock12" type="time" lang="en-US" value="14:20"></div>
<div class="baseline-fixture" style="font:16px Arial; color:#000">
<input id="baseline-text" type="text" value="H" style="width:45px;height:34px;font:16px Arial;color:#000;background:white;padding:0;border:0">
<span id="baseline-label">H<i id="baseline-text-marker" style="display:inline-block;width:1px;height:1px"></i></span>
<input id="baseline-number" type="number" value="123" style="height:34px;font:16px Arial;padding:0;border:0">
<i id="baseline-number-marker" style="display:inline-block;width:1px;height:1px"></i>
<input id="baseline-file" type="file" style="height:34px;font:16px Arial;padding:0;border:0">
<i id="baseline-file-marker" style="display:inline-block;width:1px;height:1px"></i>
</div>
<div id="eyedropper-target"></div>
""")
    shutil.copyfile(Path(__file__).with_name("inputs.gd"), project / "inputs.gd")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--renderer", action="append", choices=["gl_compatibility", "forward_plus"])
    parser.add_argument("--native-file-picker", action="store_true", help="Prompt for a fixture file in the exported game")
    parser.add_argument("--template", type=Path, default=Path(os.environ["APPDATA"]) /
                        "Godot/export_templates/4.7.2.stable/windows_debug_x86_64.exe")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    engine = str(args.godot.resolve())
    success = "HTML inputs completed with 0 failures"
    with TemporaryDirectory(prefix="dockit-html-inputs-", dir=args.work_dir, ignore_cleanup_errors=True) as directory:
        workspace = Path(directory)
        project = workspace / "project"
        export = workspace / "export"
        export.mkdir()
        settings = workspace / "Godot"
        settings.mkdir()
        write(settings / "editor_settings-4.7.tres", """[gd_resource type="EditorSettings" format=3]
[resource]
filesystem/on_save/safe_save_on_backup_then_rename = false
""")
        create_project(project, repo, args.template.resolve())
        run([engine, "--headless", "--path", str(project), "--import"], workspace)
        run([engine, "--headless", "--path", str(project), "--quit-after", "180"], workspace, success)
        for renderer in args.renderer or []:
            run([engine, "--path", str(project), "--rendering-method", renderer, "--quit-after", "180"], workspace, success)
        executable = export / "Inputs.exe"
        run([engine, "--headless", "--path", str(project), "--export-debug", "Windows", str(executable)], workspace)
        console = export / "Inputs.console.exe"
        run([str(console if console.exists() else executable), "--headless", "--quit-after", "180"], workspace, success, cwd=export)
        if args.native_file_picker:
            run([str(console if console.exists() else executable), "--", "--native-file-picker"], workspace, success, cwd=export)


if __name__ == "__main__":
    main()
