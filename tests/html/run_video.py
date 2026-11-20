"""Exercise optional HTML video decoders and selected-scene Windows PCK exports."""

import argparse
import os
from pathlib import Path
import shutil
from tempfile import TemporaryDirectory

from run_audio import run, write


SUCCESS = "HTML video checks finished with 0 failure(s)"
EDITOR_SUCCESS = "Editor video dimensions passed."


def install_backend(project, addon):
    if addon is None:
        return []
    addon = addon.resolve()
    destination = project / "addons" / addon.name
    shutil.copytree(addon, destination, ignore=shutil.ignore_patterns("*.a"))
    return [f"res://{path.relative_to(project).as_posix()}" for path in destination.rglob("*.gdextension")]


def fixture(project, repo, media, ffmpeg, vlc, template, mode):
    addon = project / "addons/dockit"
    (addon / "bin").mkdir(parents=True)
    for asset in ("dockit.gdextension", "bin/dockit.windows.template_debug.x86_64.dll"):
        shutil.copyfile(repo / "addons/dockit" / asset, addon / asset)
    extensions = ["res://addons/dockit/dockit.gdextension"]
    extensions += install_backend(project, ffmpeg)
    extensions += install_backend(project, vlc)
    (project / ".godot").mkdir()
    write(project / ".godot/extension_list.cfg", "\n".join(extensions) + "\n")
    write(project / "project.godot", """config_version=5

[application]
config/name="Dockit HTML video"
run/main_scene="res://main.tscn"

[display]
window/size/viewport_width=420
window/size/viewport_height=300
window/subwindows/embed_subwindows=true

[debug]
file_logging/enable_file_logging=false

[rendering]
renderer/rendering_method="gl_compatibility"
shader_compiler/shader_cache/enabled=false
""")
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
    shutil.copyfile(media, project / "media/movie.mp4")
    write(project / "media/movie.mp4.uid", "uid://b41jy2q2ed4s2\n")
    write(project / "media/poster.svg", '<svg xmlns="http://www.w3.org/2000/svg" width="240" height="136">'
          '<path fill="#ea1880" d="M0 0h240v136H0z"/></svg>')
    write(project / "pages/player.html", """<!doctype html>
<style>html, body { margin: 0 }</style>
<video id="movie" width="240" height="136" controls poster="../media/poster.svg">
  <source src="missing.mp4">
  <source src="../media/movie.mp4" type="video/mp4">
</video>
<p id="after">After the video</p>
<video id="uid_movie" src="uid://b41jy2q2ed4s2" width="100" height="40"></video>
<video id="intrinsic" src="../media/movie.mp4" controls></video>
""")
    shutil.copyfile(repo / "tests/html/video.gd", project / "video.gd")
    write(project / "main.tscn", """[gd_scene load_steps=3 format=3]

[ext_resource type="Script" path="res://video.gd" id="1"]
[ext_resource type="HTMLDocument" path="res://pages/player.html" id="2"]

[node name="Test" type="Node"]
script = ExtResource("1")

[node name="Frame" type="SubViewport" parent="."]
size = Vector2i(360, 248)
render_target_update_mode = 4

[node name="HTMLLabel" type="HTMLLabel" parent="Frame"]
offset_right = 360.0
offset_bottom = 248.0
document = ExtResource("2")
video_backend = %d
""" % mode)


def check_editor(project, repo, godot, workspace):
    addon = project / "addons/video_editor_probe"
    addon.mkdir()
    shutil.copyfile(repo / "tests/html/video_editor.gd", addon / "video_editor.gd")
    write(addon / "plugin.cfg", '[plugin]\nname="Video editor probe"\nscript="video_editor.gd"\n')
    settings = project / "project.godot"
    write(settings, settings.read_text(encoding="utf-8") +
          '\n[editor_plugins]\nenabled=PackedStringArray("res://addons/video_editor_probe/plugin.cfg")\n')
    editor_cache = project / ".godot/editor"
    editor_cache.mkdir(exist_ok=True)
    for renderer in ("gl_compatibility", "forward_plus"):
        write(editor_cache / "editor_layout.cfg",
              '[EditorNode]\nopen_scenes=PackedStringArray("res://main.tscn")\ncurrent_scene="res://main.tscn"\n'
              '[ScriptEditor]\nopen_scripts=["res://pages/player.html"]\nselected_script="res://pages/player.html"\n')
        run([godot, "--editor", "--path", str(project), "--rendering-method", renderer, "res://main.tscn"],
            workspace, EDITOR_SUCCESS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("media", type=Path, help="path to an MP4 with video and audible audio")
    parser.add_argument("--ffmpeg-addon", type=Path)
    parser.add_argument("--vlc-addon", type=Path)
    parser.add_argument("--backend", choices=("none", "ffmpeg", "vlc", "auto"), default="none")
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--template", type=Path, default=Path(os.environ["APPDATA"]) /
                        "Godot/export_templates/4.7.2.stable/windows_debug_x86_64.exe")
    parser.add_argument("--skip-export", action="store_true")
    parser.add_argument("--check-editor", action="store_true", help="also verify the scene view and live HTML preview")
    args = parser.parse_args()
    if args.backend in ("ffmpeg", "auto") and not args.ffmpeg_addon:
        parser.error("--ffmpeg-addon is required for this backend")
    if args.backend in ("vlc", "auto") and not args.vlc_addon:
        parser.error("--vlc-addon is required for this backend")
    if not args.skip_export and not args.template.is_file():
        parser.error("Install the Windows x64 debug export template or pass --template")
    repo = Path(__file__).resolve().parents[2]
    # The Windows editor's shader cache can keep files pending deletion after exit.
    with TemporaryDirectory(prefix=f"dockit-video-{args.backend}-", dir=args.work_dir,
                            ignore_cleanup_errors=args.check_editor) as directory:
        workspace = Path(directory)
        project = workspace / "project"
        output = workspace / "export"
        output.mkdir()
        settings = workspace / "Godot"
        settings.mkdir()
        write(settings / "editor_settings-4.7.tres", """[gd_resource type="EditorSettings" format=3]

[resource]
docks/filesystem/other_file_extensions = "mp4"
filesystem/on_save/safe_save_on_backup_then_rename = false
""")
        fixture(project, repo, args.media, args.ffmpeg_addon, args.vlc_addon, args.template.resolve(),
                {"none": 0, "ffmpeg": 1, "vlc": 2, "auto": 0}[args.backend])
        godot = str(args.godot.resolve())
        run([godot, "--headless", "--path", str(project), "--import"], workspace)
        expected = ["--unavailable"] if args.backend == "none" else []
        for renderer in ("gl_compatibility", "forward_plus"):
            run([godot, "--path", str(project), "--rendering-method", renderer, "--quit-after", "1800", "--", *expected], workspace, SUCCESS)
        if args.backend == "ffmpeg":
            run([godot, "--path", str(project), "--rendering-method", "gl_compatibility", "--quit-after", "1800",
                 "--", "--embedded-window"], workspace, SUCCESS)
        if not args.skip_export:
            exe = output / "Video.exe"
            run([godot, "--headless", "--path", str(project), "--export-debug", "Windows", str(exe)], workspace)
            console = output / "Video.console.exe"
            run([str(console if console.exists() else exe), "--quit-after", "1800", "--", "--pck", *expected],
                workspace, SUCCESS, cwd=output)
        if args.check_editor and args.backend != "none":
            check_editor(project, repo, godot, workspace)


if __name__ == "__main__":
    main()
