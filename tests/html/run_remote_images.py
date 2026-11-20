"""Exercise shared remote images against a local server with controlled completion order."""

import argparse
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import shutil
import ssl
import struct
import subprocess
from tempfile import TemporaryDirectory
from threading import Thread
import time
import zlib

from run_audio import run, write


def png(width, height, rgb, oversized=False):
    def chunk(tag, data):
        return struct.pack("!I", len(data)) + tag + data + struct.pack("!I", zlib.crc32(tag + data))

    pixels = (b"\0" + bytes(rgb) * width) * height if not oversized else b""
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack("!IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))


def create_project(project, repo, script):
    addon = project / "addons/dockit"
    (addon / "bin").mkdir(parents=True)
    for asset in ("dockit.gdextension", "bin/dockit.windows.template_debug.x86_64.dll"):
        shutil.copyfile(repo / "addons/dockit" / asset, addon / asset)
    shutil.copytree(repo / "addons/dockit/icons", addon / "icons")
    (project / ".godot").mkdir()
    write(project / ".godot/extension_list.cfg", "res://addons/dockit/dockit.gdextension\n")
    write(project / "project.godot", """config_version=5
[application]
run/main_scene="res://main.tscn"
[display]
window/size/viewport_width=280
window/size/viewport_height=200
[rendering]
renderer/rendering_method="gl_compatibility"
""")
    write(project / "main.tscn", f"""[gd_scene load_steps=2 format=3]
[ext_resource type="Script" path="res://{script}" id="1"]
[node name="RemoteImages" type="Node"]
script = ExtResource("1")
""")
    shutil.copyfile(Path(__file__).with_name(script), project / script)
    settings = project.parent / "Godot"
    settings.mkdir()
    write(settings / "editor_settings-4.7.tres", """[gd_resource type="EditorSettings" format=3]
[resource]
filesystem/on_save/safe_save_on_backup_then_rename = false
""")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--https", action="store_true", help="serve HTTPS with an OpenSSL-generated test certificate")
    parser.add_argument("--css", action="store_true", help="exercise remote stylesheets instead of standalone images")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    counts = Counter()
    responses = {
        "/background": (0.22, png(7, 5, (255, 255, 0))),
        "/shared?asset=1": (0.85, png(26, 18, (255, 0, 0))),
        "/corrupt": (0.30, b"This is not an image."),
        "/oversized": (0.30, png(9000, 9000, (0, 0, 0), oversized=True)),
        "/embedded": (1.30, png(26, 72, (255, 0, 0))),
        "/slow": (0.80, png(40, 18, (0, 255, 0))),
        "/fast": (0.05, png(120, 14, (0, 0, 255))),
        "/picture": (0.05, png(14, 12, (255, 0, 255))),
        "/local-choice": (0.05, png(10, 12, (0, 255, 255))),
    }
    css = "text/css; charset=utf-8"
    css_responses = {
        "/early": (0.60, css, b"#probe{width:95px}"),
        "/v2/css/theme?build=1": (0.05, css, b"@import 'nest/child?mode=1' layer(theme);"
                              b"#probe{width:60px} #pic{background:url(../img/tile?shade=dark) no-repeat}"),
        "/v3/sheets/child?mode=1": (0.65, css, b"@import '/v2/css/theme?build=1';"
                                              b"#child{width:29px;background:url(../assets/child-tile.png) no-repeat}"),
        "/v2/img/tile?shade=dark": (0.01, "image/png", png(12, 12, (255, 0, 0))),
        "/v3/assets/child-tile.png": (0.01, "image/png", png(12, 12, (0, 255, 0))),
        "/v2/css/dynamic/tile": (0.01, "image/png", png(12, 12, (0, 0, 255))),
        "/v2/css/waiting": (0.70, css, b"#probe{width:99px!important}"),
        "/stale": (0.80, css, b"#replace{width:99px}"),
        "/replacement": (0.02, css, b"#replace{width:43px}"),
        "/wrong-mime": (0.01, "text/html", b"#probe{width:99px!important}"),
        "/disabled": (0.01, css, b"#off{width:48px}"),
        "/empty": (0.01, css, b""),
        "/inline-sheet": (0.01, css, b"#inline{width:26px}"),
    }
    redirects = {"/entry": "v2/css/theme?build=1", "/v2/css/nest/child?mode=1": "/v3/sheets/child?mode=1"}

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            counts[self.path] += 1
            if args.css and self.path in redirects:
                self.send_response(302)
                self.send_header("Location", redirects[self.path])
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            if args.css and self.path in css_responses:
                delay, mime, body = css_responses[self.path]
            elif not args.css and self.path in responses:
                delay, body = responses[self.path]
                mime = "image/png"
            else:
                self.send_error(404)
                return
            time.sleep(delay)
            self.send_response(200)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, _format, *_args):
            pass

    suite = "css" if args.css else "images"
    with TemporaryDirectory(prefix=f"dockit-remote-{suite}-", dir=args.work_dir, ignore_cleanup_errors=True) as directory:
        workspace = Path(directory)
        create_project(workspace / "project", repo, f"remote_{suite}.gd")
        with ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
            if args.https:
                certificate = workspace / "project/localhost.crt"
                key = workspace / "localhost.key"
                openssl_config = workspace / "openssl.cnf"
                write(openssl_config, "[req]\ndistinguished_name=dn\n[dn]\n")
                subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                                "-config", str(openssl_config),
                                "-subj", "/CN=localhost", "-addext", "subjectAltName=IP:127.0.0.1",
                                "-keyout", str(key), "-out", str(certificate)], check=True,
                               stdout=subprocess.DEVNULL)
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.load_cert_chain(certificate, key)
                server.socket = context.wrap_socket(server.socket, server_side=True)
                config = workspace / "project/project.godot"
                with config.open("a", encoding="utf-8", newline="\n") as file:
                    file.write('\n[network]\ntls/certificate_bundle_override="res://localhost.crt"\n')
            worker = Thread(target=server.serve_forever, daemon=True)
            worker.start()
            scheme = "https" if args.https else "http"
            url = f"{scheme}://127.0.0.1:{server.server_port}"
            godot = str(args.godot.resolve())
            project = str(workspace / "project")
            try:
                run([godot, "--headless", "--path", project, "--import"], workspace)
                for renderer in ("gl_compatibility", "forward_plus"):
                    counts.clear()
                    try:
                        run([godot, "--path", project, "--rendering-method", renderer, "--quit-after", "1200", "--", url],
                            workspace, "HTML remote stylesheet checks passed" if args.css else "HTML remote image checks passed")
                    except SystemExit:
                        print("Observed requests:", dict(counts))
                        raise
                    routes = (*css_responses, *redirects) if args.css else (*responses, "/missing")
                    expected = Counter({key: 1 for key in routes})
                    if counts != expected:
                        raise SystemExit(f"Unexpected request counts using {renderer}: {counts}")
            finally:
                server.shutdown()
                worker.join()


if __name__ == "__main__":
    main()
