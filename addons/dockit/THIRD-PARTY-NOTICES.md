# Third-party notices

## PDFium

On Windows x64, `PDFReader`, `PDFWriter`, and the PDF texture importer use `bin/pdfium.dll` from `pdfium-win-x64.tgz`, release `chromium/8152` (PDFium `159.0.8152.0`). Windows x64 export presets with the `dockit_pdf_reader` feature include this DLL and the `PDFium-LICENSES/` directory. The SHA-256 of the DLL from this archive is `ba884cc5c6f2c3cd7c2ecca9dce36c2e341f78eb586abd691a2bbc017aa9871e`. [Upstream release](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium/8152)

- The binary distribution's MIT license. [Package license](PDFium-LICENSES/LICENSE)
- The PDFium license and notices included for the PDFium code. [PDFium notice](PDFium-LICENSES/licenses/pdfium.txt)
- The complete set of notices supplied for components in the archive, including FreeType, ICU, HarfBuzz, libjpeg-turbo, libpng, and zlib. [Component notices](PDFium-LICENSES/licenses/)

## Merman

Windows x64 MSVC builds with Mermaid support use the Merman `0.9.3` native full SDK. The SDK includes Merman's MIT and Apache 2.0 license texts, its component notices, and instructions for obtaining the corresponding source. [SDK notices](Merman-LICENSES/THIRD_PARTY_NOTICES.md) · [Component licenses](Merman-LICENSES/THIRD_PARTY_LICENSES/) · [Corresponding source](Merman-LICENSES/SOURCE_AVAILABILITY.md)

## Built-in dependencies

- The HTML audio controls and textarea grip use the supplied Google Material Icons SVGs. Dockit renders them in the control color and mirrors the resize icon horizontally. [Upstream icons and license](https://github.com/google/material-design-icons) · [Apache 2.0 license](icons/material_icons/LICENSE.txt)

- godot-cpp provides the C++ bindings for the GDExtension. [MIT license](godot-cpp-LICENSE.md)
- Dockit embeds the Open Sans Semibold font from Godot's source distribution as a fallback for HTML text. [SIL Open Font License](OpenSans-LICENSE.txt)
- MicroTeX, TinyXML-2, and the embedded math fonts are used for LaTeX formula rendering. [Licenses and notices](MicroTeX-LICENSES.txt)
- litehtml v0.10 and its bundled Gumbo parser provide HTML parsing and layout for `HTMLDocument`. Both are compiled into the extension. [litehtml BSD 3-Clause license](LiteHTML-LICENSES/litehtml.txt) · [Gumbo Apache 2.0 license](LiteHTML-LICENSES/gumbo.txt)
- Dockit uses Unicode 16.0.0 case folding to match CSS font family names, and character categories and normalization for emphasis marks and hyphenation. [Unicode license](LiteHTML-LICENSES/unicode-case-folding.txt) · [Case folding data](https://www.unicode.org/Public/16.0.0/ucd/CaseFolding.txt) · [Character data](https://www.unicode.org/Public/16.0.0/ucd/UnicodeData.txt)
- Dockit uses Unicode 15.1 line-break and East Asian width classes for CSS punctuation wrapping and word-break classes to keep punctuation inside words during hyphenation. Copyright © 2023 Unicode, Inc. [Unicode license](LiteHTML-LICENSES/unicode-case-folding.txt) · [Line break data](https://www.unicode.org/Public/15.1.0/ucd/LineBreak.txt) · [East Asian width data](https://www.unicode.org/Public/15.1.0/ucd/EastAsianWidth.txt) · [Word break data](https://www.unicode.org/Public/15.1.0/ucd/auxiliary/WordBreakProperty.txt)
- Dockit embeds 12 hyphenation dictionaries in the HYB format, identical to the files distributed in Chromium 142.0.7444.59. The individual terms and attributions accompany the addon. [Hyphenation pattern licenses](LiteHTML-LICENSES/hyphenation-patterns.txt) · [Pinned upstream files](https://chromium.googlesource.com/chromium/src/+/142.0.7444.59/third_party/hyphenation-patterns/)
