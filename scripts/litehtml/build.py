from pathlib import Path

from SCons.Errors import UserError


# These implementations are built with the Dockit HTML sources.
MIGRATED_CSS_SOURCES = {
    "src/background.cpp",
    "src/css_borders.cpp",
    "src/css_length.cpp",
    "src/css_parser.cpp",
    "src/css_properties.cpp",
    "src/css_selector.cpp",
    "src/css_tokenizer.cpp",
    "src/gradient.cpp",
    "src/media_query.cpp",
    "src/string_id.cpp",
    "src/style.cpp",
    "src/stylesheet.cpp",
    "src/web_color.cpp",
}


def find_litehtml_overrides(env):
    root = Path("src/formats/html/litehtml")
    overrides = {}
    for path in sorted(root.rglob("*")):
        if path.suffix not in (".h", ".cpp") or not path.is_file():
            continue
        # The first component groups replacements by responsibility.
        upstream_path = Path(*path.relative_to(root).parts[1:]).as_posix()
        if upstream_path in overrides:
            raise UserError(f"Multiple litehtml replacements for {upstream_path}: "
                            f"{overrides[upstream_path]} and {path}")
        overrides[upstream_path] = env.File(path.as_posix())
    return overrides


def embed_hyphenation_patterns(target, source, env):
    with Path(str(target[0])).open("w", encoding="utf-8", newline="\n") as output:
        output.write('#include "formats/html/layout/html_hyphenation_dictionary.h"\n\nnamespace dockit {\nnamespace {\n')
        for index, pattern in enumerate(source):
            data = Path(str(pattern)).read_bytes()
            output.write(f"constexpr uint8_t dictionary_{index}[] = {{\n")
            for start in range(0, len(data), 24):
                output.write("\t" + ", ".join(f"0x{byte:02x}" for byte in data[start:start + 24]) + ",\n")
            output.write("};\n")
        output.write("constexpr HyphenationPatternData dictionaries[] = {\n")
        for index, pattern in enumerate(source):
            locale = Path(str(pattern)).stem.removeprefix("hyph-")
            output.write(f'\t{{ "{locale}", dictionary_{index}, sizeof(dictionary_{index}) }},\n')
        output.write("};\n}\n\n")
        output.write("const HyphenationPatternData* find_hyphenation_patterns(std::string_view locale) {\n"
                     "\tfor (const auto& dictionary : dictionaries) {\n"
                     "\t\tif (dictionary.locale == locale) return &dictionary;\n"
                     "\t}\n\treturn nullptr;\n}\n}\n")


def embed_default_font(target, source, env):
    data = Path(str(source[0])).read_bytes()
    with Path(str(target[0])).open("w", encoding="utf-8", newline="\n") as output:
        output.write("#include <cstdint>\n\nnamespace dockit {\nconstexpr uint8_t html_default_font[] = {\n")
        for offset in range(0, len(data), 24):
            output.write("\t" + ", ".join(f"0x{byte:02x}" for byte in data[offset:offset + 24]) + ",\n")
        output.write("};\n}\n")


def embed_html_control_icons(target, source, env):
    with Path(str(target[0])).open("w", encoding="utf-8", newline="\n") as output:
        output.write("#pragma once\n\nnamespace dockit::control_icons {\n")
        for icon in source:
            path = Path(str(icon))
            svg = path.read_text(encoding="utf-8-sig").strip()
            output.write(f'inline constexpr char {path.stem}[] = R"svg({svg})svg";\n')
        output.write("}\n")


def build_litehtml(env, html_sources, enable_cpp_exceptions):
    litehtml = "src/thirdparty/litehtml"
    gumbo = f"{litehtml}/src/gumbo"
    staged_dir = "src/gen/html"
    staged_include = f"{staged_dir}/include"
    if not Path(litehtml, "include/litehtml.h").is_file() or not Path(gumbo, "include/gumbo.h").is_file():
        raise UserError("Missing litehtml or its bundled Gumbo. Run git submodule update --init --recursive.")

    html_env = env.Clone()
    overrides = find_litehtml_overrides(html_env)

    staged_headers = []
    for header in html_env.Glob(f"{litehtml}/include/*.h") + html_env.Glob(f"{litehtml}/include/litehtml/*.h"):
        relative = Path(str(header)).relative_to(litehtml).as_posix()
        staged_headers += html_env.InstallAs(f"{staged_dir}/{relative}", overrides.pop(relative, header))

    html_env.Append(CPPPATH=[staged_include, f"{staged_include}/litehtml", f"{litehtml}/src", f"{gumbo}/include"])
    # litehtml uses C++ exceptions during HTML encoding detection.
    enable_cpp_exceptions(html_env)
    if html_env.get("is_msvc", False):
        html_env.AppendUnique(CXXFLAGS=["/permissive-"])
    html_object = html_env.StaticObject if env["platform"] == "ios" else html_env.SharedObject
    html_objects = list(html_object(html_sources))
    font_header = html_env.Command(f"{staged_dir}/html_default_font.gen.h",
                                   "src/thirdparty/html-fonts/OpenSans_SemiBold.woff2",
                                   html_env.Action(embed_default_font, "Embedding the default HTML font"))
    html_env.Depends([obj for obj in html_objects if obj.name.startswith("litehtml_fonts.")], font_header)
    icon_files = [f"addons/dockit/icons/material_icons/{name}.svg"
                  for name in ("play_arrow", "pause", "volume_up", "volume_off", "resize_window", "fullscreen",
                               "fullscreen_exit", "arrow_drop_down", "arrow_drop_up", "arrow_left", "arrow_right",
                               "radio_button_checked", "calendar_today", "schedule", "colorize", "unfold_more",
                               "radio_button_unchecked", "check_box", "check_box_outline_blank", "indeterminate_check_box",
                               "close_small")]
    icon_header = html_env.Command(f"{staged_dir}/html_control_icons.gen.h", icon_files,
                                   html_env.Action(embed_html_control_icons, "Embedding HTML control icons"))
    html_env.Depends([obj for obj in html_objects if obj.name.startswith("html_paint_recorder.")], icon_header)
    patterns = sorted(html_env.Glob("src/thirdparty/hyphenation-patterns/hyph-*.hyb"))
    if not patterns:
        raise UserError("Missing Dockit hyphenation patterns.")
    embedded_patterns = html_env.Command(f"{staged_dir}/src/hyphenation_patterns.gen.cpp", patterns,
                                         html_env.Action(embed_hyphenation_patterns, "Embedding HTML hyphenation patterns"))
    html_env.Depends(embedded_patterns, html_env.File(__file__))
    html_objects += html_object(embedded_patterns)

    for source in html_env.Glob(f"{litehtml}/src/*.cpp"):
        relative = Path(str(source)).relative_to(litehtml)
        if relative.as_posix() in MIGRATED_CSS_SOURCES:
            continue
        object_path = f"{staged_dir}/{relative.with_suffix('').as_posix()}{env['OBJSUFFIX']}"
        html_objects += html_object(object_path, overrides.pop(relative.as_posix(), source))

    if overrides:
        raise UserError(f"litehtml replacements have no matching upstream file: {', '.join(sorted(overrides))}")
    html_env.Depends(html_objects, staged_headers)

    gumbo_env = env.Clone()
    gumbo_env.Append(CPPPATH=[f"{gumbo}/include", f"{gumbo}/include/gumbo"])
    if gumbo_env.get("is_msvc", False):
        gumbo_env.Append(CPPPATH=[f"{gumbo}/visualc/include"])
        gumbo_env.AppendUnique(CCFLAGS=["/wd4244", "/wd4267"])
    else:
        gumbo_env.AppendUnique(CFLAGS=["-std=c99"])
    gumbo_object = gumbo_env.StaticObject if env["platform"] == "ios" else gumbo_env.SharedObject
    gumbo_objects = []
    for source in gumbo_env.Glob(f"{gumbo}/*.c"):
        relative = Path(str(source)).relative_to(litehtml).with_suffix("")
        gumbo_objects += gumbo_object(f"{staged_dir}/{relative.as_posix()}{env['OBJSUFFIX']}", source)
    return html_objects + gumbo_objects
