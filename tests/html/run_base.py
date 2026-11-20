"""Test HTML document metadata, stylesheet changes and selected-scene Windows exports."""

import argparse
import os
from pathlib import Path
import shutil
from tempfile import TemporaryDirectory
import wave

from run_audio import run, write


def create_project(project, repo, template):
    extension = project / "addons/dockit"
    (extension / "bin").mkdir(parents=True)
    for asset in ("dockit.gdextension", "bin/dockit.windows.template_debug.x86_64.dll"):
        shutil.copyfile(repo / "addons/dockit" / asset, extension / asset)
    (project / ".godot").mkdir()
    write(project / ".godot/extension_list.cfg", "res://addons/dockit/dockit.gdextension\n")
    write(project / "project.godot", """config_version=5

[application]
config/name="Dockit HTML document base"
run/main_scene="res://main.tscn"

[display]
window/size/viewport_width=260
window/size/viewport_height=200

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
binary_format/embed_pck=false
application/modify_resources=false
""")
    for folder in ("pages", "assets/v1/styles/nested", "assets/v1/extras", "assets/v2/styles"):
        (project / folder).mkdir(parents=True)
    for name, width, color in (
        ("assets/v1/icon.svg", 14, "#ff0000"),
        ("assets/v1/inline.svg", 12, "#00ff00"),
        ("assets/v1/styles/linked.svg", 12, "#0000ff"),
        ("assets/v1/styles/nested/nested.svg", 12, "#ff00ff"),
        ("assets/v1/styles/nested/optional.svg", 12, "#ffff00"),
        ("assets/v1/extras/inline-import.svg", 12, "#00ffff"),
        ("assets/v2/icon.svg", 36, "#ffffff"),
        ("pages/icon.svg", 24, "#ffffff"),
    ):
        write(project / name, f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="12">'
              f'<rect width="{width}" height="12" fill="{color}"/></svg>')
    for name, frames in (("assets/v1/tone.wav", 4000), ("assets/v2/tone.wav", 8000)):
        with wave.open(str(project / name), "wb") as audio:
            audio.setparams((1, 2, 8000, 0, "NONE", "not compressed"))
            audio.writeframes(b"\0\0" * frames)
    write(project / "assets/v1/styles/outer.css", '@import "nested/inner.css";\n'
          '#linked { background: url(linked.svg) no-repeat; width: 40px; }\n')
    write(project / "assets/v1/styles/nested/inner.css", '#nested { background-image: url(nested.svg); }\n')
    write(project / "assets/v1/styles/optional.css", '@import "nested/optional.css";\n'
          '#optional-target { width: 44px; }\n')
    write(project / "assets/v1/styles/nested/optional.css",
          '#optional-target { height: 18px; background: url(optional.svg) no-repeat; }\n')
    write(project / "assets/v1/extras/inline.css", '#inline-import { background-image: url(inline-import.svg); }\n')
    write(project / "assets/v2/styles/outer.css", '#linked { width: 70px; }\n')
    write(project / "pages/index.html", """<!doctype html>
<html lang="en"><head>
<title>  First  title  </title>
<template><base href="../assets/not-active/"></template>
<base target="_blank"><base id="first" href="../assets/v1/virtual.html"><base id="next" href="../assets/v2/">
<meta name="viewport" id="meta" content="test"><script id="config" type="application/json">{"enabled":true}</script>
<link rel="stylesheet" href="styles/outer.css">
<link id="optional-sheet" rel="stylesheet" href="styles/optional.css" media="(min-width: 150px)" disabled>
<style>
@import "extras/inline.css";
html, body { margin: 0; background: black; }
img { display: block; }
div.swatch { height: 12px; width: 12px; }
</style></head><body>
<img id="icon" src="icon.svg" srcset="icon.svg 1x">
<div class="swatch" id="inline" style="background-image:url(inline.svg)"></div>
<div class="swatch" id="linked"></div>
<div class="swatch" id="nested"></div>
<div class="swatch" id="inline-import"></div>
<div class="swatch" id="optional-target"></div>
<audio id="tone" src="tone.wav"></audio>
<a id="jump" href="#tail">Jump</a><div style="height:360px"></div><div id="tail">Tail</div>
</body></html>
""")
    shutil.copyfile(Path(__file__).with_name("base.gd"), project / "base.gd")
    write(project / "main.tscn", """[gd_scene load_steps=3 format=3]

[ext_resource type="Script" path="res://base.gd" id="1"]
[ext_resource type="HTMLDocument" path="res://pages/index.html" id="2"]

[node name="BaseTest" type="Node"]
script = ExtResource("1")

[node name="Frame" type="SubViewport" parent="."]
size = Vector2i(220, 172)
render_target_update_mode = 4

[node name="HTMLLabel" type="HTMLLabel" parent="Frame"]
offset_right = 220.0
offset_bottom = 172.0
document = ExtResource("2")
""")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--template", type=Path, default=Path(os.environ["APPDATA"]) /
                        "Godot/export_templates/4.7.2.stable/windows_debug_x86_64.exe")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    with TemporaryDirectory(prefix="dockit-html-base-", dir=args.work_dir, ignore_cleanup_errors=True) as directory:
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
        godot = str(args.godot.resolve())
        run([godot, "--headless", "--path", str(project), "--import"], workspace)
        for renderer in ("gl_compatibility", "forward_plus"):
            run([godot, "--path", str(project), "--rendering-method", renderer, "--quit-after", "900"],
                workspace, "HTML base checks passed")
        exe = export / "Base.exe"
        run([godot, "--headless", "--path", str(project), "--export-debug", "Windows", str(exe)], workspace)
        console = export / "Base.console.exe"
        run([str(console if console.exists() else exe), "--quit-after", "900", "--", "--pck"],
            workspace, "HTML base export checks passed", cwd=export)


if __name__ == "__main__":
    main()
