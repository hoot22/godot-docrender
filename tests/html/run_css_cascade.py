"""Run the DOM and HTML cascade checks with the Windows x64 debug extension."""

import argparse
from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory
import zlib


def write_rgba_png(path, width, height, rows, metadata=(), late_metadata=()):
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + b"".join(chunk(kind, payload) for kind, payload in metadata)
        + chunk(b"IDAT", zlib.compress(rows))
        + b"".join(chunk(kind, payload) for kind, payload in late_metadata)
        + chunk(b"IEND", b"")
    )


def write_density_png(path, horizontal, vertical):
    width, height = 48, 24
    rows = (b"\0" + b"\xff\0\0\xff" * width) * height
    write_rgba_png(path, width, height, rows, [(b"pHYs", struct.pack(">IIB", horizontal, vertical, 1))])


def write_orientation_png(path, orientation, late=False):
    width, height = 32, 20
    top = b"\0" + b"\xff\0\0\xff" * 16 + b"\0\xff\0\xff" * 16
    bottom = b"\0" + b"\0\0\xff\xff" * 16 + b"\xff\xff\0\xff" * 16
    rows = top * 10 + bottom * 10
    endian, marker = ("<", b"II") if orientation % 2 else (">", b"MM")
    # TIFF stores the SHORT inline after the entry's 32-bit count.
    exif = marker + struct.pack(endian + "HIHHHIH2xI", 42, 8, 1, 0x0112, 3, 1, orientation, 0)
    metadata = [(b"pHYs", struct.pack(">IIB", 7559, 7559, 1))]
    exif_chunk = (b"eXIf", exif)
    if not late:
        metadata.append(exif_chunk)
    write_rgba_png(path, width, height, rows, metadata, [exif_chunk] if late else [])


def write_border_image_png(path):
    palette = {
        "r": b"\xff\0\0\xff", "b": b"\0\0\xff\xff", "y": b"\xff\xff\0\xff",
        "c": b"\0\xff\xff\xff", "m": b"\xff\0\xff\xff", "w": b"\xff\xff\xff\xff",
        "g": b"\0\xff\0\xff", "o": b"\xee\x82\0\xff", "k": b"\0\0\0\xff",
    }
    rows = (
        ["rrrrmmwwbbbb"] * 4
        + ["kkkkoooocccc"] * 2
        + ["wwwwoooocccc"] * 2
        + ["yyyyggggcccc"] * 4
    )
    write_rgba_png(path, 12, 12, b"".join(b"\0" + b"".join(palette[pixel] for pixel in row) for row in rows))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path, help="path to the Godot console executable")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--renderer", choices=("gl_compatibility", "forward_plus"))
    mode.add_argument("--dom-only", action="store_true", help="run only the headless DOM regression tests")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    library = repo / "addons/dockit/bin/dockit.windows.template_debug.x86_64.dll"
    if not library.is_file():
        parser.error("Build the Windows x64 template_debug target first.")
    if not args.godot.is_file():
        parser.error("Godot executable does not exist.")

    with TemporaryDirectory(prefix="dockit-css-cascade-", ignore_cleanup_errors=True) as test_dir:
        project = Path(test_dir)
        extension = project / "addons/dockit"
        (extension / "bin").mkdir(parents=True)
        shutil.copyfile(library, extension / "bin" / library.name)
        shutil.copyfile(repo / "addons/dockit/dockit.gdextension", extension / "dockit.gdextension")
        (project / ".godot").mkdir()
        (project / ".godot/extension_list.cfg").write_text(
            "res://addons/dockit/dockit.gdextension\n", encoding="utf-8", newline="\n",
        )
        (project / "project.godot").write_text(
            'config_version=5\n\n[application]\nconfig/name="Dockit CSS cascade test"\n'
            '\n[debug]\nfile_logging/enable_file_logging=false\n'
            '\n[rendering]\nshader_compiler/shader_cache/enabled=false\n',
            encoding="utf-8", newline="\n",
        )
        tests = Path(__file__).resolve().parent
        shutil.copyfile(tests / "wpt/dom.gd", project / "dom_wpt.gd")
        # Keep the test process's logs and shader cache in its temporary project.
        environment = {**os.environ, "APPDATA": str(project)}
        dom = subprocess.run(
            [str(args.godot.resolve()), "--headless", "--path", str(project), "--script", "dom_wpt.gd"],
            env=environment, cwd=project, timeout=30, check=False, capture_output=True, text=True, encoding="utf-8",
            errors="replace",
        )
        print(dom.stdout, end="", flush=True)
        print(dom.stderr, end="", file=sys.stderr, flush=True)
        if dom.returncode != 0 or "Dockit DOM WPT ports passed." not in dom.stdout or "ERROR:" in dom.stderr:
            return dom.returncode or 1
        if args.dom_only:
            return 0

        shutil.copyfile(tests / "css_cascade.gd", project / "css_cascade.gd")
        styles = project / "styles"
        (styles / "nested").mkdir(parents=True)
        (styles / "sheet.css").write_text(
            '@import "nested/outer.css";\n@import "between.css";\n@import "nested/inner.css";\n'
            '.from_link { width: 30px; }\n'
            '.import_order { width: 23px; }\n.after_link { width: 13px; }\n'
            '.stylesheet_move { width: 31px; }\n',
            encoding="utf-8", newline="\n",
        )
        (styles / "nested/outer.css").write_text('@import "inner.css";\n', encoding="utf-8", newline="\n")
        (styles / "nested/inner.css").write_text(
            ".order { width: 22px; } .import_order { width: 22px; }\n"
            ".reimport_order { width: 24px; } .unused { width: bogus; }\n"
            "#invalid_nested { width: 58px; width: var(--nested-width); }\n",
            encoding="utf-8", newline="\n",
        )
        (styles / "between.css").write_text(".reimport_order { width: 25px; }\n", encoding="utf-8", newline="\n")
        (styles / "additional.css").write_text(".order { width: 31px; }\n", encoding="utf-8", newline="\n")
        (styles / "media.css").write_text(".link_media { width: 29px; }\n", encoding="utf-8", newline="\n")
        (styles / "typed.css").write_text(
            "#link_type { width: bogus; width: 68px !important; }\n", encoding="utf-8", newline="\n",
        )
        (styles / "conditional.css").write_text("#conditional_import { width: 51px; }\n", encoding="utf-8", newline="\n")
        (styles / "rejected.css").write_text(
            "#conditional_import { width: bogus; width: 99px !important; }\n", encoding="utf-8", newline="\n",
        )
        (styles / "layer.css").write_text(
            '@import "nested/layer-inner.css" layer(components);\n',
            encoding="utf-8", newline="\n",
        )
        (styles / "anonymous-layer.css").write_text(
            "#layer_import { width: 71px; }\n", encoding="utf-8", newline="\n",
        )
        (styles / "nested/layer-inner.css").write_text(
            "#nested_layer_import { width: 74px; } .unused { width: bogus; }\n",
            encoding="utf-8", newline="\n",
        )
        (styles / "layer-media.css").write_text(
            "#import_media { width: 98px; }\n", encoding="utf-8", newline="\n",
        )
        (styles / "counters.css").write_text(
            '@counter-style imported-counter { system: cyclic; symbols: IMPORT; }\n'
            '@counter-style layered { system: cyclic; symbols: imported; }\n',
            encoding="utf-8", newline="\n",
        )
        (styles / "font-alternates.css").write_text(
            '@font-feature-values Probe { @styleset { imported: 4; } }\n', encoding="utf-8", newline="\n",
        )
        (project / "object-fit.svg").write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20">'
            '<rect width="40" height="20" fill="#c5692b"/></svg>\n', encoding="utf-8", newline="\n",
        )
        (project / "sampling.svg").write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="3" height="2">'
            '<path fill="red" d="M0 0h1v1H0z"/><path fill="lime" d="M1 0h1v1H1z"/>'
            '<path fill="blue" d="M2 0h1v1H2z"/><path fill="yellow" d="M0 1h1v1H0z"/>'
            '<path fill="cyan" d="M1 1h1v1H1z"/><path fill="magenta" d="M2 1h1v1H2z"/></svg>\n',
            encoding="utf-8", newline="\n",
        )
        (styles / "nested/inline-resources.md").write_text(
            'Before <img src="../../object%2Dfit.svg" width="20" alt="Preview"> '
            '<a href="#end%2Dpoint">Open &amp; go</a>\n\n' + 'Paragraph\n\n' * 12
            + '<div id="end-point">Target</div>\n',
            encoding="utf-8", newline="\n",
        )
        (project / "image_set.html").write_text(
            '<!doctype html><link rel="stylesheet" href="styles/image-set.css"><div id="imported_set"></div>\n',
            encoding="utf-8", newline="\n",
        )
        (project / "image_function.html").write_text(
            '<!doctype html><link rel="stylesheet" href="styles/image-function.css"><div id="imported_image"></div>\n',
            encoding="utf-8", newline="\n",
        )
        write_density_png(project / "density-live.png", 7559, 7559)
        write_density_png(project / "density-import.png", 7559, 7559)
        write_density_png(project / "density-asymmetric.png", 7559, 3779)
        for orientation in range(1, 9):
            write_orientation_png(project / f"orientation-{orientation}.png", orientation)
        write_orientation_png(project / "orientation-late.png", 6, late=True)
        write_orientation_png(project / "orientation-import.png", 6)
        write_border_image_png(project / "border-image.png")
        shutil.copyfile(project / "border-image.png", project / "border-import.png")
        (styles / "border-image.css").write_text(
            'body { margin: 0; } #imported_border { width: 20px; height: 20px; border: 4px solid black;'
            'border-image: url(../border-import.png) 4 fill; }',
            encoding="utf-8", newline="\n",
        )
        (project / "border_image.html").write_text(
            '<!doctype html><link rel="stylesheet" href="styles/border-image.css"><div id="imported_border"></div>',
            encoding="utf-8", newline="\n",
        )
        for name, width, height, color in (
            ("marker-light.png", 18, 10, b"\xd0\x3a\x73\xff"),
            ("marker-dark.png", 28, 14, b"\x3a\x57\xd0\xff"),
        ):
            write_rgba_png(project / name, width, height, (b"\0" + color * width) * height)
        (styles / "nested/list-markers.css").write_text(
            '''body { margin: 0; background: white; }
ul { margin: 0; padding-left: 56px; font: 20px/30px sans-serif;
    list-style: inside "Fallback: " light-dark(url(../../marker-light.png), image-set("../../marker-dark.png" 2x)); }
li { height: 32px; }
#themed { color-scheme: dark; }
@supports (list-style-image: linear-gradient(red, red)) {
    #gradient { --shape: linear-gradient(#18aa61, #18aa61); list-style: inside var(--shape);
        list-style-image: light-dark(red, url(../../marker-dark.png)); }
}
#outside { list-style: outside image(#af7321); }
#fade { list-style-image: cross-fade(image(blue), image(lime)); }
#missing { list-style: inside url(../../marker-missing.png) "Missing: "; }
#transparent { list-style-image: light-dark(none, image(#d78015)); }
#custom::marker { content: "Custom: "; }
''', encoding="utf-8", newline="\n",
        )
        (project / "list_marker_images.html").write_text(
            '<!doctype html><html style="color-scheme: light"><link rel="stylesheet" href="styles/nested/list-markers.css">'
            '<ul><li id="themed"><span id="themed_word">Theme</span></li><li id="gradient">Gradient</li>'
            '<li id="outside"><span id="outside_word">Outside</span></li><li id="fade">Blend</li>'
            '<li id="missing">Unavailable</li><li id="transparent"><span id="transparent_word">Transparent</span></li>'
            '<li id="custom">Override</li></ul>', encoding="utf-8", newline="\n",
        )
        (styles / "nested/generated-images.css").write_text(
            '''body { margin: 0; color: #111; background: white; }
div, li { height: 30px; }
ul { margin: 0; padding-left: 60px; }
@supports (content: light-dark(linear-gradient(red, blue), image(red))) {
    #sized::before { content: light-dark(linear-gradient(#a33683, #a33683), image(#2174ae));
        display: block; width: 44px; height: 20px; border-radius: 4px; }
}
#mixed::after { --icon: light-dark(url(../../marker-light.png), image-set("../../marker-dark.png" 2x));
    content: "prefix " var(--icon) url(../../marker-missing.png) " suffix"; }
#marker::marker { content: "mark" image-set(url(../../marker-light.png) 2x); }
#blend::after { content: cross-fade(image(blue), image(lime)); display: block; width: 30px; height: 18px;
    transform: translateX(4px); }
#switch::before { content: "Initial text"; }
#switch.image::before { content: image(#aa7318); width: 24px; height: 9px; }
''', encoding="utf-8", newline="\n",
        )
        (project / "generated_css_images.html").write_text(
            '<!doctype html><html style="color-scheme: light"><link rel="stylesheet" href="styles/nested/generated-images.css">'
            '<div id="sized"></div><div id="mixed"></div><ul><li id="marker">Item</li></ul>'
            '<div id="blend"></div><div id="switch"></div>', encoding="utf-8", newline="\n",
        )
        shutil.copyfile(project / "border-image.png", project / "mask-import.png")
        (styles / "mask.css").write_text(
            '@import "nested/mask.css";', encoding="utf-8", newline="\n",
        )
        (styles / "nested/mask.css").write_text(
            'html { background: blue; } body { margin: 0; } div { width: 40px; height: 30px;'
            'background: red; --image: url(../../mask-import.png); mask: var(--image) 0 0 / 20px 30px no-repeat; }',
            encoding="utf-8", newline="\n",
        )
        (project / "mask_image.html").write_text(
            '<!doctype html><link rel="stylesheet" href="styles/mask.css"><div></div>',
            encoding="utf-8", newline="\n",
        )
        (project / "svg-mask-import.svg").write_text(
            '<s:svg xmlns:s="http://www.w3.org/2000/svg" xmlns:l="http://www.w3.org/1999/xlink">'
            '<s:defs><s:rect id="cut-shape" width=".5" height="1" fill="black"/>'
            '<s:mask id="mask-center" maskContentUnits="objectBoundingBox" style="mask-type: alpha">'
            '<s:use l:href="#cut-shape"/></s:mask></s:defs></s:svg>',
            encoding="utf-8", newline="\n",
        )
        (styles / "nested/svg-mask.css").write_text(
            'html { background: blue; } body { margin: 0; } div { width: 40px; height: 30px;'
            'background: red; mask: url(../../svg-mask-import.svg#mask-center) 100% / 1px no-repeat; }',
            encoding="utf-8", newline="\n",
        )
        (project / "svg_mask.html").write_text(
            '<!doctype html><link rel="stylesheet" href="styles/nested/svg-mask.css"><div></div>',
            encoding="utf-8", newline="\n",
        )
        clip_assets = project / "clip-assets"
        clip_assets.mkdir()
        svg_clip = (
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<s:svg xmlns:s="http://www.w3.org/2000/svg" xmlns:l="http://www.w3.org/1999/xlink">'
            '<s:style><![CDATA[.ring { clip-rule: evenodd; } .hidden { display: none; }]]></s:style>'
            '<s:defs><s:path id="outline" class="ring" d="M0 0H60V60H0Z M18 18H42V42H18Z"/>'
            '<s:clipPath id="use-ring" clip-path="url(#limit)">'
            '<s:use l:href="#outline"/><s:rect class="hidden" width="60" height="60"/></s:clipPath>'
            '<s:clipPath id="limit"><s:rect width="50" height="60"/></s:clipPath>'
            '<s:clipPath id="letter"><s:text x="48" y="58" font-size="52" text-anchor="middle">O</s:text></s:clipPath>'
            '<s:line id="guide" x1="10" y1="58" x2="90" y2="58"/>'
            '<s:clipPath id="path-letter"><s:text font-size="48" text-anchor="middle">'
            '<s:textPath l:href="#guide" startOffset="50%">O</s:textPath></s:text></s:clipPath>'
            '<s:clipPath id="source-empty"/></s:defs></s:svg>'
        )
        (clip_assets / "local.svg").write_text(svg_clip, encoding="utf-8", newline="\n")
        (clip_assets / "import.svg").write_text(svg_clip, encoding="utf-8", newline="\n")
        (styles / "clip-path.css").write_text(
            '@import "nested/clip-path.css";', encoding="utf-8", newline="\n",
        )
        (styles / "nested/clip-path.css").write_text(
            'body { margin: 0; background: black; } #external { width: 60px; height: 60px;'
            'background: cyan; --clip: url(../../clip-assets/import.svg#use-ring); clip-path: var(--clip); }',
            encoding="utf-8", newline="\n",
        )
        (project / "external_clip.html").write_text(
            '<!doctype html><link rel="stylesheet" href="styles/clip-path.css"><div id="external"></div>',
            encoding="utf-8", newline="\n",
        )
        (project / "image_orientation.html").write_text(
            '<!doctype html><style>body { margin: 0; }</style>'
            '<img id="imported_orientation" src="orientation-import.png" style="display: block">\n',
            encoding="utf-8", newline="\n",
        )
        (project / "image_resolution.html").write_text(
            '<!doctype html><img id="imported_density" src="density-import.png" style="image-resolution: from-image">\n',
            encoding="utf-8", newline="\n",
        )
        write_rgba_png(project / "template-icon.png", 8, 8, (b"\0" + b"\0\xff\0\xff" * 8) * 8)
        (project / "image_template.html").write_text(
            '<!doctype html><style>html, body { margin: 0; background: black; } img { display: block; }</style>'
            '<main id="target"></main><template id="icon"><img src="template-icon.png"></template>',
            encoding="utf-8", newline="\n",
        )
        responsive_assets = project / "responsive"
        responsive_assets.mkdir()
        for name, width, height, color in (
            ("small", 32, 16, b"\xff\0\0\xff"),
            ("medium", 64, 32, b"\0\xff\0\xff"),
            ("wide", 80, 40, b"\0\0\xff\xff"),
        ):
            write_rgba_png(responsive_assets / (name + ".png"), width, height, (b"\0" + color * width) * height)
        (project / "responsive_picture.html").write_text(
            '<!doctype html><style>body { margin: 0; } img { display:block; }</style><picture>'
            '<source type="image/avif" srcset="responsive/small.png">'
            '<source type="image/png" media="(min-width: 100px)" srcset="responsive/wide.png" width="60" height="27">'
            '<img id="imported_responsive" srcset="responsive/small.png 32w, responsive/medium.png 64w" sizes="50vw">'
            '<source srcset="responsive/small.png"></picture>\n',
            encoding="utf-8", newline="\n",
        )
        (styles / "image-function.css").write_text(
            'body { margin: 0; } #imported_image { width: 8px; height: 8px; '
            'background: image(ltr "../sampling.svg#xywh=1,0,1,1", red) no-repeat; }\n',
            encoding="utf-8", newline="\n",
        )
        (styles / "image-set.css").write_text(
            '@import "nested/candidates.css";\n', encoding="utf-8", newline="\n",
        )
        (styles / "nested/candidates.css").write_text(
            'body { margin: 0; #imported_set { width: 28px; height: 20px; background: '
            'image-set("../set-low.svg", var(--missing, "set-high.svg") 2dppx) no-repeat; } }\n',
            encoding="utf-8", newline="\n",
        )
        for path, width, height, color in (
            (styles / "set-low.svg", 12, 8, "#67bb39"),
            (styles / "nested/set-high.svg", 24, 16, "#d84333"),
        ):
            path.write_text(
                f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">'
                f'<rect width="{width}" height="{height}" fill="{color}"/></svg>\n',
                encoding="utf-8", newline="\n",
            )
        system_fonts = Path(os.environ["WINDIR"]) / "Fonts"
        (project / "fonts").mkdir()
        for filename in ("times.ttf", "cour.ttf", "arialbd.ttf", "georgiai.ttf", "bahnschrift.ttf", "cambria.ttc"):
            shutil.copyfile(system_fonts / filename, project / "fonts" / filename)
        optional_fonts = (
            ("OPTICAL_FONT_UID", "SegUIVar.ttf", "optical.ttf"),
            ("EAST_ASIAN_FONT_UID", "YuGothM.ttc", "east_asian.ttc"),
            ("VARIANT_FONT_UID", "calibri.ttf", "variant.ttf"),
            ("HISTORICAL_FONT_UID", "pala.ttf", "historical.ttf"),
        )
        font_fixtures = {}
        for key, filename, fixture_name in optional_fonts:
            source = system_fonts / filename
            if source.is_file():
                fixture = project / "fonts" / fixture_name
                shutil.copyfile(source, fixture)
                font_fixtures[key] = fixture.with_name(fixture.name + ".import")
        (styles / "font-faces.css").write_text(
            '@import "nested/font-regular.css";\n'
            '@font-face { font-family: "Probe Face"; src: url(../fonts/arialbd.ttf); font-weight: bold; }\n'
            '@font-face { font-family: "Probe Face"; src: url(../fonts/georgiai.ttf); font-style: italic; }\n'
            '@font-face { font-family: "Collection Probe";'
            'src: url("../fonts/cambria.ttc#Cambria%4Dath") format(collection); }\n',
            encoding="utf-8", newline="\n",
        )
        (styles / "nested/font-regular.css").write_text(
            '@font-face { font-family: "Probe Face"; src: url(../../fonts/times.ttf); }\n'
            '@media (max-width: 400px) { @supports font-format( Truetype ) and at-rule(@font-face) {\n'
            '@font-face { font-family: "Probe Face";\n'
            'src: local("Unavailable face"), url(missing.ttf), url(../../fonts/times.ttf) format(svg),\n'
            'url(../../fonts/times.ttf) tech(variations),\n'
            'url("../../fonts/cour.ttf") format("truetype"); } } }\n',
            encoding="utf-8", newline="\n",
        )
        (project / "font_face.html").write_text(
            '<!doctype html><link rel="stylesheet" href="styles/font-faces.css">\n'
            '<style>@supports font-tech(variations) { @font-face { font-family: "Variable Probe";'
            'src: url(fonts/bahnschrift.ttf) format("truetype-variations"); font-weight: 150 650; } }\n'
            '@supports font-format("truetype") or font-tech(incremental) or at-rule(@keyframes) {'
            '.variable { color: black; } }\n'
            'body { margin: 0; background: black; color: white; }'
            'div { font: 40px "probe FACE", serif; width: 7ch; height: 40px; }'
            '#bold { font-weight: bold; } #italic { font-style: italic; }'
            '.variable { font: 40px "Variable Probe", serif; width: 220px; height: 55px; line-height: 55px; }'
            '#light { font-weight: 150; } #heavy { font-weight: 650; } #above { font-weight: 900; }</style>\n'
            '<div id="normal">Normal</div><div id="bold">Bold</div><div id="italic">Italic</div>\n'
            '<div id="light" class="variable">MMMM</div><div id="heavy" class="variable">MMMM</div>'
            '<div id="above" class="variable">MMMM</div>\n',
            encoding="utf-8", newline="\n",
        )
        imported = subprocess.run(
            [str(args.godot.resolve()), "--headless", "--path", str(project), "--import"],
            env=environment, cwd=project, timeout=60, check=False, capture_output=True,
        )
        document_fixtures = {
            "FONT_FACE_UID": project / "font_face.html.import",
            "IMAGE_SET_UID": project / "image_set.html.import",
            "IMAGE_FUNCTION_UID": project / "image_function.html.import",
            "IMAGE_RESOLUTION_UID": project / "image_resolution.html.import",
            "IMAGE_ORIENTATION_UID": project / "image_orientation.html.import",
            "IMAGE_TEMPLATE_UID": project / "image_template.html.import",
            "RESPONSIVE_PICTURE_UID": project / "responsive_picture.html.import",
            "BORDER_IMAGE_UID": project / "border_image.html.import",
            "LIST_MARKER_IMAGES_UID": project / "list_marker_images.html.import",
            "GENERATED_CSS_IMAGES_UID": project / "generated_css_images.html.import",
            "MASK_IMAGE_UID": project / "mask_image.html.import",
            "SVG_MASK_UID": project / "svg_mask.html.import",
            "EXTERNAL_CLIP_UID": project / "external_clip.html.import",
        }
        if imported.returncode != 0 or not all(path.is_file() for path in document_fixtures.values()):
            print(imported.stdout.decode(errors="replace"))
            print(imported.stderr.decode(errors="replace"))
            return imported.returncode or 1
        # A selected-resource export can omit the original stylesheets.
        for stylesheet in (
            styles / "font-faces.css", styles / "nested/font-regular.css",
            styles / "image-set.css", styles / "nested/candidates.css",
            styles / "image-function.css", styles / "border-image.css",
            styles / "nested/list-markers.css", styles / "nested/generated-images.css",
            styles / "mask.css", styles / "nested/mask.css",
            styles / "nested/svg-mask.css",
            styles / "clip-path.css", styles / "nested/clip-path.css",
        ):
            stylesheet.unlink()
        (project / "density-import.png").unlink()
        (project / "orientation-import.png").unlink()
        (project / "border-import.png").unlink()
        (project / "marker-light.png").unlink()
        (project / "marker-dark.png").unlink()
        (project / "mask-import.png").unlink()
        (project / "svg-mask-import.svg").unlink()
        (clip_assets / "import.svg").unlink()
        (project / "template-icon.png").unlink()
        for image in responsive_assets.glob("*.png"):
            image.unlink()
        test = project / "css_cascade.gd"
        script = test.read_text(encoding="utf-8")
        for key, path in document_fixtures.items():
            uid = path.read_text(encoding="utf-8").split('uid="', 1)[1].split('"', 1)[0]
            script = script.replace("{" + key + "}", uid)
        for key, _, _ in optional_fonts:
            fixture_uid = ""
            if key in font_fixtures:
                font_import = font_fixtures[key].read_text(encoding="utf-8")
                fixture_uid = font_import.split('uid="', 1)[1].split('"', 1)[0]
            script = script.replace("{" + key + "}", fixture_uid)
        test.write_text(script, encoding="utf-8", newline="\n")
        renderers = (args.renderer,) if args.renderer else ("gl_compatibility", "forward_plus")
        for renderer in renderers:
            completed = subprocess.run(
                [str(args.godot.resolve()), "--path", str(project), "--rendering-method", renderer,
                 "--script", "css_cascade.gd"],
                env=environment, cwd=project, timeout=60, check=False,
            )
            if completed.returncode != 0:
                return completed.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
