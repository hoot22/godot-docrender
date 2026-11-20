"""Run the HTML dialog regression with both Godot renderers."""

import argparse
from pathlib import Path
import shutil
from tempfile import TemporaryDirectory

from run_audio import run, write


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    with TemporaryDirectory(prefix="dockit-dialog-", dir=args.work_dir, ignore_cleanup_errors=True) as directory:
        project = Path(directory)
        addon = project / "addons/dockit"
        (addon / "bin").mkdir(parents=True)
        for asset in ("dockit.gdextension", "bin/dockit.windows.template_debug.x86_64.dll"):
            shutil.copyfile(repo / "addons/dockit" / asset, addon / asset)
        (project / ".godot").mkdir()
        write(project / ".godot/extension_list.cfg", "res://addons/dockit/dockit.gdextension\n")
        write(project / "project.godot", """config_version=5

[display]
window/size/viewport_width=400
window/size/viewport_height=300

[rendering]
shader_compiler/shader_cache/enabled=false
""")
        shutil.copyfile(Path(__file__).with_name("dialog.gd"), project / "dialog.gd")
        for renderer in ("gl_compatibility", "forward_plus"):
            run([str(args.godot.resolve()), "--path", str(project), "--rendering-method", renderer,
                 "--script", "res://dialog.gd", "--quit-after", "900"], project, "HTML dialog checks passed")


if __name__ == "__main__":
    main()
