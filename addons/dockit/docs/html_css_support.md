# HTML CSS support

Dockit maintains CSS syntax, selectors, declarations, the cascade, value types, the default stylesheet, and computed styles in its C++ CSS modules. The litehtml v0.10 integration (`9bc84b8`) supplies DOM, viewport, font, and resource data and consumes the resulting computed style. Box-dependent percentages, intrinsic sizes, and transform origins are resolved during layout. This reference applies to `HTMLLayout`, `HTMLLabel`, and HTML blocks in `MarkdownLabel`.

For GDScript queries, element handles, and geometry, see [HTML API and Web standards](html_api_reference.md).

## Native elements

`<details>` supports `:open` and the summary's `::marker`. `<progress>` supports `:indeterminate`. Progress, meters, and range controls paint their native chrome through the HTML renderer. Use `appearance: none` for author-defined backgrounds and borders.

Horizontal ruby supports `ruby-position: over | under`. Text with an annotation is measured as a single segment with one baseline for the base. Adjacent segments can wrap independently. `<rp>` is hidden by the default stylesheet.

## Chromium light defaults

Dockit uses Windows Fluent scrollbar geometry across platforms. The light layout reference was measured at 100% zoom and device scale 1 in Microsoft Edge 157.0.4341.60 on Windows, with Chromium's Blink user-agent stylesheet and Fluent source as references. The browser rendering serves as a baseline for values and paint, and the addon implements the rules with its own CSS and drawing. [Blink default stylesheet](https://raw.githubusercontent.com/chromium/chromium/main/third_party/blink/renderer/core/html/resources/html.css) · [Fluent drawing](https://chromium.googlesource.com/chromium/src/+/main/ui/native_theme/native_theme_fluent.cc) · [Chromium light control colors](https://raw.githubusercontent.com/chromium/chromium/main/ui/color/color_provider_utils.cc)

| Elements | Defaults checked | Dockit integration |
| --- | --- | --- |
| Document, sectioning, paragraphs, headings, quotes, figures | Body margin, block layout, paragraph and heading spacing, quote and figure indentation | `HTMLLabel` and the editor's HTML preview use the root scheme for the default canvas. Set a background in CSS or use `HTMLLabel.transparent_canvas` for scene compositing. Markdown HTML blocks use the host canvas and explicitly supplied fonts. |
| Links and inline elements | Blue links, underline, emphasis and relative font sizes, abbreviation decoration, quote marks, code fonts, preformatted whitespace and highlight colors | Link activation follows existing signals. The host's font families and available fallback glyphs determine text metrics. |
| Lists, description lists, tables and rules | Logical list indentation and nested marker types, blocked terms and descriptions, table spacing and cell padding, header and caption alignment, horizontal rule inset border | Lists and tables retain the existing layout engine and HTML presentational hints. |
| Disclosure and field groups | The summary marker, block layout, fieldset border and padding, and legend inline padding | The legend uses the Dockit fieldset flow. Chromium's special placement through the fieldset top border differs. |
| Inputs, buttons, selects and textareas | Light surfaces, borders, spacing, focus outlines, disabled colors, selection marks and range chrome | Default text inputs use a 169px content width, and textareas continue to size from rows and columns. Native button inputs and selects measure their text. Select menus and list boxes use Dockit's popup and list drawing, so browser-native menu geometry and auxiliary controls can differ. |
| Progress and meter | A 10em progress width, 5em meter width, and a thin rounded visible track | Tracks and fills use the existing gauge painters. Author styling and the animation clock apply in the usual way. |

The defaults participate in the existing pre-author cascade. Author declarations can change the defaults, and `appearance: none` removes default native chrome on applicable controls.

On Windows, the default Latin fonts for standalone HTML are Times New Roman at 16 CSS pixels for ordinary text and Arial for the `sans-serif` generic. Buttons, single-line inputs, and selects default to Arial at 13.333 CSS pixels. The Latin `monospace` preference is Consolas, falling back to Courier New. Its default size is 13 CSS pixels, while textareas keep the explicitly specified control size. The inherited size follows the fixed-width preference when it originates from a default or relative size. Explicit pixel sizes retain their CSS value. [Chromium Windows font preferences](https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/app/resources/locale_settings_win.grd) · [Blink font sizing](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/css/resolver/font_builder.cc)

HTMLLabel uses the document's CSS and built-in palette for its canvas, selection, outer scrollbars, form menus, and diagnostic overlay. To migrate a `background_color` theme override, set the CSS `html` background or enable `transparent_canvas` for a transparent base. Migrate a `selection_color` override to CSS `:root::selection`.

## Color schemes

`color-scheme` inherits and accepts `normal`, `light`, `dark`, and ordered lists such as `light dark`, optionally with `only`. It selects the supported system preference when listed, or the first recognized scheme. `normal` defaults to light. The root controls the canvas and viewport scrollbar. Each element controls its own default form appearance and inner scrollbar. `@media (prefers-color-scheme: dark)` reads the system preference independently of the scheme chosen by the document. Platforms without system color preference support report light. [CSS color schemes](https://www.w3.org/TR/css-color-adjust-1/#color-scheme-prop) · [Media queries](https://www.w3.org/TR/mediaqueries-5/#prefers-color-scheme)

Scheme-aware CSS colors include `Canvas`, `CanvasText`, `Field`, `FieldText`, `ButtonFace`, `ButtonText`, `ButtonBorder`, `GrayText`, `LinkText`, `ActiveText`, `VisitedText`, `Highlight`, `HighlightText`, `AccentColor`, and `AccentColorText`. Use these names or author CSS colors for backgrounds, text, borders, and scrollbar overrides. [CSS system colors](https://www.w3.org/TR/css-color-4/#css-system-colors)

## Accessibility and input preferences

`@media (prefers-reduced-motion: reduce)` reflects Godot's system reduce-animation preference; `no-preference` is its inverse. Dockit does not forcibly disable author animations, so stylesheets choose the reduced-motion treatment. `prefers-contrast: more` reflects Godot's increased-contrast preference, and `no-preference` is reported otherwise. The Godot API does not currently distinguish `less` or `custom` contrast preferences.

`hover` and `any-hover` reflect whether the active display backend reports mouse input. `pointer` selects `fine` when a mouse is available, `coarse` when only touch is available, and `none` otherwise. `any-pointer` reports mouse and touch capabilities independently. These values describe backend capabilities, not the last device that produced an input event. `forced-colors` remains unsupported because the cross-platform Godot API does not expose forced-color mode or its system palette; increased contrast is not treated as forced colors.

`HTMLLabel` publishes an element-level accessibility tree through Godot's `DisplayServer` accessibility API. It maps common HTML and ARIA roles to Godot roles, exposes accessible names, descriptions, values, selected/checked and form states, live regions, table/list metadata, and focus/click/scroll actions. Godot has no dedicated heading or landmark roles, so these are represented as static text or containers with role descriptions. The tree is rebuilt from the current visible document when Godot requests an accessibility update. This integration does not add a platform-native accessibility bridge or promise identical screen-reader output across operating systems.

## Selectors and rules

| Feature | Support |
| --- | --- |
| Basic selectors | Type, class, ID, attribute, selector lists, descendant, child, and sibling combinators |
| Pseudo-classes | `:root`, `:scope`, `:empty`, `:is()`, `:where()`, `:not()`, `:has()`, `:nth-child()` and related structural selectors, `:lang()`, `:dir(ltr | rtl)`, `:link`, and `:any-link`. `:dir()` inherits the nearest `dir` attribute and `dir="auto"` scans common scripts for the first strong direction, defaulting to LTR. Dialogs support `:open` and `:modal`. `HTMLLabel` supplies `:hover`, `:focus`, `:focus-visible`, and `:focus-within`, plus `:active` for buttons, checkboxes, and radios. Form state selectors include `:enabled`, `:disabled`, `:checked`, `:placeholder-shown`, `:required`, `:optional`, `:read-only`, `:read-write`, `:valid`, and `:invalid`. |
| Pseudo-elements | `::before`, `::after`, and `::marker` with generated content in the final compound selector. The legacy `:before` and `:after` spellings are equivalent. `::placeholder` styles the placeholder in text fields. `::selection` styles the selected text and its background. `::backdrop` styles the plate behind a modal dialog, including backgrounds, gradients, and opacity. Sub-pseudo-elements, `::first-line`, and `::first-letter` are unsupported. |
| Cascade | Named and anonymous layers, specificity, scope proximity, source order, `!important`, custom properties, registered custom properties, and `var()`. The `all` shorthand resets properties using the CSS-wide keywords `inherit`, `initial`, `unset`, `revert`, and `revert-layer`. |
| Style sources | Inline `style`, `<style>`, the extra `stylesheet` string, and local project stylesheets loaded through `<link>` or `@import`. |
| Nesting | Nested selectors, `&`, relative combinators, interleaved declarations, and nested `@media`, `@supports`, and `@layer` blocks. |
| At-rules | `@media` queries viewport conditions. `@container` queries ancestor content sizes and computed custom properties. `@supports` tests declarations, selectors, and `at-rule(property)`. `@property` registers selected typed custom properties. `@layer` supports ordering statements and nested blocks. `@scope` bounds rules to a DOM subtree, with optional limits. `@import` accepts local paths with optional layer, `supports(…)`, and media clauses. `@counter-style` defines document-local counter styles. `@font-face` loads project fonts. `@font-feature-values` names font-specific glyph alternates. `@keyframes` defines named CSS animations. |

Registered custom properties support `*`, `<number>`, `<integer>`, `<length>`, `<percentage>`, `<length-percentage>`, `<color>`, and `<custom-ident>` syntax, including `|` alternatives. `inherits` controls whether descendants use the parent's computed value, and `initial-value` supplies the registered default. Invalid typed values fall back to inheritance or the initial value according to that descriptor. Initial values must be computable without element-relative font or container units, `currentColor`, or `var()`. The complete Properties and Values syntax grammar, CSSOM registration APIs, and typed custom-property animation interpolation are not implemented.

## Known unsupported CSS

The following features are not applied by the renderer: multicolumn layout (`columns`, `column-*`), vertical writing (`writing-mode`, `text-orientation`), `::first-line`, `::first-letter`, `unicode-bidi`, `shape-outside`, `shape-margin`, `mask-border`, `background-clip: text`, `@namespace`, `@starting-style`, and scroll-driven `animation-timeline`. `@property` supports only the syntax forms and behavior listed above. `::part` and `::slotted` depend on Shadow DOM and slotting, which are not implemented. `forced-colors` is also unsupported because Godot exposes no cross-platform forced-color mode or palette.

## Properties and values

| Area | Support |
| --- | --- |
| Layout | Block and inline layout, lists, tables, Flexbox, block and inline Grid, box suppression with `display: contents`, dimensions, margins, padding, and static, relative, absolute, fixed, and sticky positioning. The `inset` shorthand sets `top`, `right`, `bottom`, and `left`. |
| Tables | Automatic and fixed column sizing, header and footer ordering, top and bottom captions, and row and column spans. Separated borders support `border-spacing` and `empty-cells`. Collapsed borders are resolved per grid-line segment. `visibility: collapse` removes row and column tracks and clips cells spanning the remaining tracks. Column and row groups support solid backgrounds. [CSS table model](https://www.w3.org/TR/CSS22/tables.html) |
| Form controls | `appearance: auto\|none` controls default decoration for buttons, text fields, checkboxes, radios, and selects. `accent-color` sets default checked colors on checkboxes and radios. Text inputs and textareas support `caret-color`, `::placeholder` color, opacity and font properties, and `::selection` foreground and background colors. [CSS user interface](https://www.w3.org/TR/css-ui-4/) · [CSS pseudo-elements](https://www.w3.org/TR/css-pseudo-4/) |
| Box sizing | `width`, `min-width`, and `max-width` accept `min-content`, `max-content`, `fit-content`, `stretch`, `fit-content()`, and `stretch()`. `aspect-ratio` sets a preferred width-to-height ratio. Logical size properties map to the horizontal writing mode. |
| Transforms | 2D and 3D transform lists, including `perspective()` and `matrix3d()`. Individual `translate`, `rotate`, and `scale` compose with `transform-origin`, `transform-box`, and `backface-visibility`. The `perspective` and `perspective-origin` properties project child planes. Use `transform-style: preserve-3d` for shared depth. |
| Motion paths | `offset`, `offset-path`, `offset-distance`, `offset-position`, `offset-anchor`, and `offset-rotate` control path placement before the `transform` list. |
| Grid | Track lists support intrinsic sizes, named lines, and `repeat()`, including `auto-fill` and `auto-fit`. Grid items support placement by name, line number, or span. Grid supports automatic placement, patterned implicit tracks, item alignment, and track distribution. |
| Flexbox spacing | `row-gap`, `column-gap`, and one- or two-value `gap` work with wrapping and reverse directions. The shorthand sets the row gap first and the column gap second. Values include `normal` (zero in Flexbox), non-negative lengths, and percentages. Percentage gaps use the container's inner size on their axis and resolve to zero when that size depends on the flex items. [CSS Gaps](https://www.w3.org/TR/css-gaps-1/#gaps) |
| Lengths | Absolute, font-relative, and viewport-relative lengths. See the length units and math functions sections below. |
| Colors | Named and hex colors, `rgb()` and `hsl()` variants, `hwb()`, `lab()`, `lch()`, `oklab()`, `oklch()`, predefined `color()` spaces, `color-mix()`, relative color syntax, `light-dark()`, `contrast-color()`, and `currentColor` where applicable. |
| Opacity | `opacity` accepts a number, percentage, or matching CSS math expression. The element and descendants composite as a group, with separate groups for nested values below `1`. |
| Transitions | `transition` and its five longhands animate changes to supported colors, opacity, box geometry, spacing, flex sizing, and transform functions. |
| Animations | `animation` and its eight Level 1 longhands run named keyframes on the scene clock. See the animation section below for animated properties and playback behavior. |
| Element blending | `mix-blend-mode` accepts the CSS blend modes and `plus-lighter`. `isolation: isolate` starts a new stacking context with a transparent backdrop. |
| Filters | `filter` and `backdrop-filter` accept `none` or a chain of the ten CSS filter functions. See the filter effects section below. |
| Image masks | `mask` and its eight longhands apply local images, gradients, and SVG `<mask>` references. Layers support alpha, luminance, and the four CSS compositing operators. |
| Clipping paths | `clip-path` supports box geometry, `inset()`, `rect()`, `xywh()`, `circle()`, `ellipse()`, `polygon()`, `path()`, `shape()`, and references to inline or project-local SVG clipping paths. |
| Backgrounds | Solid colors, local images, and linear, radial, and conic gradients. Two-axis tiling supports `repeat`, `no-repeat`, `round`, and `space`. `background-blend-mode` blends the layers within an element. |
| Image fitting | `object-fit` sizes image pixels inside the element's content box. `object-position` aligns the fitted image using keywords, lengths, percentages, or four-component edge offsets. |
| Image sampling | `image-rendering` supports `auto`, `smooth`, `high-quality`, `crisp-edges`, and `pixelated`. |
| Image orientation | `image-orientation` reads EXIF by default, supports the encoded orientation with `none`, and accepts legacy quarter-turn and flip overrides for content images. |
| Image resolution | `image-resolution` sets the natural CSS size of raster content and background images using an explicit resolution or `from-image`, with optional `snap`. |
| Image candidates | `image-set()` works in `background-image` and the `background` shorthand. Options accept local URLs, path strings, gradients, resolutions, and MIME type hints. |
| Image fallbacks | `image()` supports solid colors, fallback colors, `xywh` image fragments, and `ltr` and `rtl` annotations. `light-dark()` selects an image for background layers, border images, and masks, including inside `image-set()` options. |
| Image blending | `cross-fade()` mixes background images, gradients, and colors with percentage weights. It also works in `image-set()` options. |
| Border images | `border-image` and its five longhands support nine-slice images, center fill, overhang, and four tiling modes. |
| Outlines | `outline`, its three longhands, and `outline-offset` paint focus rings and other outlines around element boxes. |
| Borders and shadows | `solid`, `dashed`, `dotted`, `double`, `groove`, `ridge`, `inset`, and `outset`, with per-side widths and colors and `border-radius`. `box-shadow` supports `inset` and multiple layers. |
| Text | Theme fonts, font size and weight, italic text, wrapping, alignment, and line height. `font-family` accepts a prioritized list of installed family names and generic families, including in the `font` shorthand. `text-decoration` supports underline, overline, and line-through in five styles, with `auto`, `from-font`, length, and percentage thickness. `text-shadow` supports offsets, blur, and multiple layers on glyphs and decorations. Text emphasis paints keyword shapes and custom marks above or below horizontal text. |
| Overflow | `overflow`, its physical and logical longhands, `clip`, and `overflow-clip-margin`. See the overflow section below. |
| Scrolling | `scroll-padding` and `scroll-margin` adjust anchors, element alignment, and keyboard focus. `scroll-behavior` controls navigation animation. `overscroll-behavior` sets scroll chaining at edges. CSS Scroll Snap aligns scroll containers with descendant boxes. |
| Text overflow | `text-overflow: clip \| ellipsis` controls how a block with non-visible overflow paints overflowing inline content. See the text overflow section below. |
| Pointer hit testing | The inherited [`pointer-events`](https://www.w3.org/TR/css-ui-4/#pointer-events-control) property accepts `auto` and `none`. A box with `none` lets pointer hits reach the boxes below. A descendant can restore hits with `auto`. Text selection and keyboard focus remain available. |
| Text selection | `user-select` accepts `auto`, `text`, `none`, and `all`. The `-webkit-user-select` spelling is an alias. `::selection` supports foreground and background colors. |
| Cursor | `HTMLLabel` maps common CSS keywords to Godot cursor shapes. Image cursors and unmapped keywords use the control's fallback. |

## Relational selectors and feature queries

`:has()` matches relative to its subject. Use `:has(.icon)` for descendants, `:has(> .icon)` for children, and `:has(+ .item)` or `:has(~ .item)` for following siblings. A relative selector may continue with more combinators. The argument list contributes its highest specificity. `:where()` matches its selector list with zero specificity. `:empty` follows Selectors Level 4 and ignores comments, generated content, and document whitespace. [Selectors Level 4](https://www.w3.org/TR/selectors-4/#relational)


In an unscoped stylesheet rule, `:scope` selects the document root. In an element query, it selects the element that issued the query. `:nth-child(An+B of S)` and `:nth-last-child(An+B of S)` count matching element siblings, including hidden ones. An invalid selector in the `of` list invalidates that pseudo-class. [Selectors Level 4](https://www.w3.org/TR/selectors-4/#tree-structural-pseudos)

`:lang(en, "*-Hant")` checks the inherited HTML `lang` attribute using case-insensitive subtag matching. `:lang("")` matches untagged content. Language-tag aliases and extlang canonicalization are outside the supported set. Hyperlinks on `<a>` and `<area>` match both `:link` and `:any-link` when `href` is present. Dockit records no browsing history. [Selectors Level 4](https://www.w3.org/TR/selectors-4/#the-lang-pseudo)

Use `@supports (display: grid)` to test a declaration and `@supports selector(.card:has(> img))` to test a selector. Conditions accept `not`, `and`, `or`, and parentheses for grouping. Nest `@supports` and `@media` to require both conditions. Declaration probes use the CSS parser and the known rendering limits. A well-formed `var()` probe checks the declaration syntax before substitution. Selector probes reject unsupported arguments inside otherwise forgiving lists. [CSS Conditional Rules](https://www.w3.org/TR/css-conditional-3/#at-supports) · [Selector feature queries](https://www.w3.org/TR/css-conditional-4/#supports-selector-fn)

`font-format(woff2)`, `font-tech(variations)`, and `at-rule(@font-face)` query support for font formats, font technology, and at-rules. Format queries take a keyword. [CSS Conditional Rules Level 5](https://www.w3.org/TR/css-conditional-5/#extensions)

## Cascade

Document styles from `<style>` and enabled links with the `stylesheet` relation follow document order. Link relations are matched as case-insensitive tokens. Alternate stylesheets are skipped. Each `@import` contributes its rules at the import location. The additional `stylesheet` joins the same author rule set after the document's rules.

A `<style>` element contributes author rules when its `type` is omitted, empty, or a case-insensitive match for `text/css`. Changing the attribute recomputes the cascade. [HTML style block processing](https://html.spec.whatwg.org/multipage/semantics.html#update-a-style-block)

A stylesheet link accepts an omitted or empty `type` and the `text/css` MIME type with optional parameters. Changing the link's type or moving a stylesheet in the document recomputes the author cascade in the resulting document order. [HTML stylesheet links](https://html.spec.whatwg.org/multipage/links.html#link-type-stylesheet)

Dockit compares declarations for each longhand in this order.

1. Author declarations marked `!important` precede normal author declarations.
2. A `style` attribute precedes stylesheet rules of the same importance.
3. Among stylesheet rules with equal importance, determine layer precedence. Later layers win for normal declarations, and earlier layers win for important declarations. Unlayered rules form the final implicit layer.
4. Within a layer, higher selector specificity wins. For instance, a document rule for `#card` takes precedence over an additional rule for `.card`.
5. When the preceding criteria match, fewer ancestor steps from the matched element to the scope root wins. Unscoped rules have an infinite distance.
6. When the preceding criteria match, the later declaration wins, including repeated declarations within a single block. The additional stylesheet comes last.

For example, a matching stylesheet rule with `width: 12rem !important` overrides `style="width: var(--card-width)"`. Adding `!important` to the inline `width` makes the inline declaration win. A value rejected during parsing leaves earlier declarations eligible. A winning value rejected after variable substitution uses `unset` and the earlier stylesheet value stays overridden. The same distinction applies to a shorthand and the longhands it wins. [Invalid values after variable substitution](https://www.w3.org/TR/css-variables-1/#invalid-variables)

`all` accepts `initial`, `inherit`, `unset`, `revert`, and `revert-layer`, including through `var()`. The reset participates in the cascade for each property. It excludes `direction`, `unicode-bidi`, and custom properties. `all: initial` resets a `div` to the initial `display: inline`, whereas `all: revert` can restore the default block display. For paired logical and physical edges, the reset uses the physical declaration, so `all: inherit` inherits the physical edges even when the parent's direction differs. [The all shorthand](https://www.w3.org/TR/css-cascade-5/#all-shorthand) · [Logical property mapping](https://www.w3.org/TR/css-logical-1/#mapping)

Pseudo-elements contribute to the type count. `:is()` and `:not()` take the highest specificity in their argument lists, including valid arguments that did not match. `:nth-child(… of …)` and `:nth-last-child(… of …)` add that highest specificity to their own pseudo-class specificity. [Selectors specificity](https://www.w3.org/TR/selectors-4/#specificity-rules)

Use `@layer base, theme;` to establish order before appending rules with `@layer theme { … }`. Dotted paths such as `theme.buttons` and nested blocks address the same sublayer. Direct rules in a parent form its final implicit sublayer. Each unnamed block or bare `@import "sheet.css" layer;` creates a separate anonymous layer. An import with `layer(name)` appends to the named layer. Layers inside viewport or feature conditions contribute to layer order when those conditions match.

The pre-author baseline contains litehtml defaults, control and theme defaults, and HTML presentational hints. Author declarations take precedence over this baseline. In a document without author layers, both `revert` and `revert-layer` restore the baseline for each affected longhand. When the document uses layers, `revert-layer` first looks in earlier layers in normal layer order, or in the author stylesheets when used in a `style` attribute. If no eligible declaration remains, it restores the baseline. A restored declaration can still use `var()`. [CSS Cascade](https://www.w3.org/TR/css-cascade-5/#cascade-sort)

If a custom property resolves to a CSS-wide keyword, Dockit applies it before substituting values that reference the property. For example, `--card-width: var(--missing, revert)` restores the baseline for `--card-width`, so `width: var(--card-width, 12rem)` sees the restored or inherited value and uses `12rem` if neither is available. [CSS Variables](https://www.w3.org/TR/css-variables-1/#using-variables)

## Nesting

```css
.card {
  color: black;
  > h2 { margin-top: 0; }
  &:hover { color: navy; }
  @media (min-width: 400px) {
    padding: 1rem;
    .icon { width: 2rem; }
  }
}
```

A nested selector can place `&` anywhere a subclass selector is valid. A branch without `&` is relative to the parent selector, with a descendant combinator by default. Leading `>`, `+`, and `~` also use the parent as their anchor. Each `&` has the maximum specificity of the parent selector list and matches its element branches. Outside a nested rule, `&` matches the current scope with zero specificity.

Declarations before, between, and after nested rules keep their source order. Direct declarations inside a nested group match the exact parent selectors, including their pseudo-elements. Stylesheets support up to 128 nested rule blocks. [CSS Nesting](https://www.w3.org/TR/css-nesting-1/)

## Scoped styles

```css
@scope (.card) to (.embedded-card) {
  border-radius: 8px;
  > h2 { margin-top: 0; }
  :scope:hover .action { text-decoration: underline; }
}
```

Each matching root defines a scope that includes itself and its descendants. A matching descendant limit excludes that element and its subtree. Bare selectors inside the block match relative to the root, with a descendant combinator by default. An explicit `:scope` or `&` can address the root or refer to ancestors outside the scope while keeping the matched element within it. Direct declarations and `&` match the root with zero specificity. `:scope` contributes one pseudo-class to specificity. Nested scopes stay within the outer scope, and the innermost root determines proximity.

An omitted root uses the parent of the `<style>` or stylesheet `<link>` element. Sheets without an owner element use the document root. The `@import scope(…)` modifier is outside the supported grammar. [CSS Cascade Level 6](https://drafts.csswg.org/css-cascade-6/#scoped-styles)

## Tables and form controls

Fixed tables use a resolved table width, column declarations, and the first visual row to size tracks. Later cells can overflow their assigned tracks. In the separated border model, hidden empty cells leave the table background visible. Collapsed row and column tracks retain their DOM handles, while spanning cells are clipped to the remaining tracks.

Form decoration uses Dockit's painted controls. Use `appearance: none` to keep the control's interactions and author styling with custom chrome. `::placeholder` accepts color, opacity, and font properties. `::selection` paints text fields and textareas.

`:required` and `:optional` apply to text inputs, checkboxes, radios, textareas, and dropdowns. `:read-write` matches editable text inputs and textareas. Other HTML elements match `:read-only`. `:valid` and `:invalid` reflect the same constraints used for form submission. The supported constraints cover `required`, including radio groups and dropdown placeholders, and user-edited text length in UTF-16 units through `minlength` and `maxlength`. Forms aggregate their owned controls, including controls linked by `form`, and fieldsets aggregate their descendant controls. `novalidate` and `formnovalidate` bypass submission checks while the selectors still reflect the control state. Pattern validation and type-specific constraints remain pending. [HTML selector semantics](https://html.spec.whatwg.org/multipage/semantics-other.html#selector-valid) · [HTML text length constraints](https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#attr-fe-maxlength)

## Display and box generation

An element with `display: contents` keeps its DOM position and inherited styles. Its text, child boxes, and generated `::before` and `::after` boxes participate in the nearest ancestor's layout. In Flexbox and Grid this lets the promoted boxes become items. The element has no client rectangles of its own. Its padding, background, transform, and positioning do not create geometry. Line decorations propagate from ancestor boxes, so declaring an underline on a boxless element creates no underline. The document root computes to `block`. Replaced boxes such as images, inputs, textareas, and selects, plus line-break elements and SVG roots, are suppressed. [CSS Display](https://www.w3.org/TR/css-display-3/#box-generation) · [CSS Text Decoration](https://www.w3.org/TR/css-text-decor-3/#line-decoration)

The supported outer and inner display keywords can appear together, in either order. For example, `inline grid` computes like `inline-grid`, `block flex` like `flex`, and `inline flow-root` like `inline-block`.

## Box sizes

`min-content` uses the available line-break opportunities. `max-content` measures the content without optional line breaks. `fit-content` constrains the available width between those two sizes. `fit-content(…)` replaces the available width with a non-negative length or percentage. `stretch` fills the available width with the margin box, and `stretch(…)` uses the argument as that margin-box size. Percentages in the functions use the containing block's width. For positioned boxes, insets reduce the available width used by the keywords. [CSS Box Sizing Level 3](https://www.w3.org/TR/css-sizing-3/#sizing-values) · [CSS Box Sizing Level 4](https://drafts.csswg.org/css-sizing-4/#sizing-values)

In the supported horizontal writing mode, `inline-size`, `min-inline-size`, and `max-inline-size` share the cascade with their `width` counterparts. `block-size`, `min-block-size`, and `max-block-size` share the cascade with their `height` counterparts and accept the corresponding height values. Box content sizing keywords and functions are currently available on the inline axis.

`aspect-ratio` accepts `auto`, a non-negative number, or a pair such as `16 / 9`. CSS math functions with finite numeric results and `var()` may supply ratio components. A zero component makes the ratio behave as `auto`. A ratio alone uses the dimensions selected by `box-sizing`. Combining `auto` with a ratio uses the content box and gives an image's natural ratio priority when available. An image with numeric HTML `width` and `height` attributes gets the corresponding fallback ratio as a presentational hint. A ratio can supply an automatic dimension as the other dimension changes through layout. An in-flow box with visible overflow can grow to fit its content when the minimum on the ratio-dependent axis is automatic. [CSS preferred aspect ratios](https://drafts.csswg.org/css-sizing-4/#aspect-ratio) · [HTML image dimensions](https://html.spec.whatwg.org/multipage/rendering.html#attributes-for-embedded-content-and-images)

## Logical edges

In horizontal writing, `margin-block`, `padding-block`, and `inset-block` set top and bottom. Their `-inline` equivalents set the left and right edges in LTR and reverse them in RTL. The two-value form sets start then end. A single value applies to both edges. The corresponding `-block-start`, `-block-end`, `-inline-start`, and `-inline-end` longhands use the same value grammar as the physical properties.

`border-block`, `border-inline`, and their individual edge shorthands accept a width, style, and color. Use forms such as `border-inline-width: 2px 4px` to set a single component on the two edges. In the logical corner names, the first `start` or `end` refers to the block axis and the second refers to the inline axis. For example, `border-start-start-radius` selects the top-left corner in LTR and the top-right corner in RTL.

The element's computed direction pairs each logical longhand with a physical longhand. Both declarations participate in the same cascade, including `!important`, layers, and `var()`. A winning logical `inherit` reads the parent's corresponding logical edge, even when parent and child have different directions. A winning physical `inherit` reads the same physical edge. [CSS Logical Properties](https://www.w3.org/TR/css-logical-1/#box)

The `float` and `clear` values `inline-start` and `inline-end` resolve using the direction of the containing block. An inherited value remains logical until placement. [CSS Logical Float and Clear](https://www.w3.org/TR/css-logical-1/#float-clear)

## Containment

`contain` supports `none` and combinations of `layout`, `style`, `paint`, and one of `size` or `inline-size`. The `content` keyword combines layout, style, and paint containment. `strict` also includes size containment. Layout and paint containment establish a formatting context, a stacking context, and a containing block for absolute and fixed descendants. Layout containment keeps visible descendant overflow out of the outer scroll range and uses a synthesized external baseline. Paint containment clips descendants and their pointer hits to the rounded overflow clip edge, including `overflow-clip-margin`. Explicit scroll containers can still scroll their contents.

Style containment scopes descendant counter increments and assignments. Generated content can read enclosing counters. The first update of an enclosing counter creates a fresh nested counter. Quotes start at the surrounding depth, and changes inside the subtree stay local. Ordinary inline boxes can establish these boundaries. [CSS style containment](https://drafts.csswg.org/css-contain-2/#containment-style)

Size containment calculates the affected dimensions as if the box had no contents. `inline-size` applies this rule on the inline axis only. Explicit dimensions and constraints still apply, and an empty Grid retains its explicit tracks. The contents lay out inside the resulting box and can overflow it. Replaced images use the contained intrinsic dimensions for layout. An authored aspect ratio still applies. The size rules exclude non-atomic inlines, tables, and their internal boxes other than captions. [CSS size containment](https://drafts.csswg.org/css-contain-2/#containment-size)

`contain-intrinsic-size` takes one or two dimensions in width and height order. Each accepts a non-negative length or `none`, optionally preceded by `auto`. Lengths can use CSS math. The longhands are `contain-intrinsic-width` and `contain-intrinsic-height`, with `contain-intrinsic-inline-size` and `contain-intrinsic-block-size` mapping to them in horizontal writing. An explicit dimension replaces the empty-content size on the contained axis. An `auto` dimension uses the last laid-out inner size of its uncontained axis while the element skips its contents, with the declared value as a fallback. [CSS Contain Intrinsic Size](https://www.w3.org/TR/css-sizing-4/#intrinsic-size-override)

`content-visibility: hidden` activates size, layout, style, and paint containment. The element keeps its own box and decoration. Its skipped descendants have no committed layout boxes, paint, or focus targets. Replaced content such as image pixels is also skipped. DOM queries still return the descendants, and changing to `visible` lays them out again. The property applies to boxes eligible for size containment. [CSS Content Visibility](https://www.w3.org/TR/css-contain-2/#content-visibility)

`content-visibility: auto` applies layout, style, and paint containment continuously. In `HTMLLayout` and `HTMLLabel`, distant boxes also use size containment and skip laying out and painting their contents. Dockit reveals content within 240 CSS pixels of a viewport or scrollport and can skip it again beyond 320 pixels. Focus and selection keep content relevant. Text remains available to copying, Tab can reveal a skipped control before focusing it, and scrolling to a skipped target uses its placeholder. Styles are still computed for skipped auto content to resolve these interactions. Embedded Markdown HTML blocks keep their contents laid out for the outer document's search and selection. [CSS auto content visibility](https://drafts.csswg.org/css-contain-2/#content-visibility)

Containment on `html` or `body` keeps the body's overflow and background local. Non-atomic inline boxes and internal table boxes other than table cells are excluded from layout and paint containment. [CSS Containment](https://www.w3.org/TR/css-contain-2/#contain-property)

## Container size queries

```css
.card { container: card / inline-size; }
.card-body { display: block; }
@container card (inline-size >= 24em) {
  .card-body { display: grid; grid-template-columns: 1fr 2fr; }
}
```

`container-type` accepts `normal`, `inline-size`, and `size`. The last two establish independent formatting contexts with size containment on the inline axis or on both axes, respectively. `container-name` accepts `none` or a list of case-sensitive names. The `container` shorthand sets the names and the optional type after a slash.

Size queries select the nearest ancestor with any requested name and with containment on every axis required by the condition. The dimensions come from its untransformed content box. `width` and `inline-size` use the horizontal axis, while `height` and `block-size` use the vertical axis. `aspect-ratio` and `orientation` require both axes. Font-relative thresholds use the queried container's font. Conditions support ranges, boolean operators, comma-separated alternatives, and nested queries. A generated child such as `::before` can query its originating element. [CSS Container Queries](https://drafts.csswg.org/css-conditional-5/#container-queries)

The length units `cqw`, `cqh`, `cqi`, `cqb`, `cqmin`, and `cqmax` also work without an `@container` rule. Each axis uses the closest ancestor that supports size queries on that axis, with the viewport size as a fallback. The two axes can use different containers. In declarations, these units resolve for the styled element and inherit as computed lengths. In a query threshold they resolve for the queried container, so their basis comes from that container's ancestors. [Container-relative lengths](https://drafts.csswg.org/css-conditional-5/#container-lengths)

`style(--theme: dark)` compares a computed custom property with a value. `style(--theme)` tests whether it has a value other than its guaranteed-invalid initial value. A style-only query can select an ordinary ancestor. When size and style tests are combined in one condition, both use the same selected container. Values and property names are case-sensitive. Query values support `var()` and its fallback, and declarations anywhere inside the query block use ordinary variable substitution. [Container style queries](https://drafts.csswg.org/css-conditional-5/#style-container)

Size queries can use `var()` in thresholds, for example `@container card (width > var(--breakpoint, 24em))`. Substitution uses the queried container's computed custom properties. It also works inside length calculations and in chained ranges, orientation values, and aspect ratios. Missing variables without a fallback and invalid substituted values make that test unknown, including inside a negated condition. [Container size features](https://drafts.csswg.org/css-conditional-5/#size-container)

Style ranges, queries of standard CSS properties, and `scroll-state()` are pending.

## Image fitting

`object-fit` applies to `<img>`. The default `fill` scales to the content box. `contain` shows the whole image and preserves its natural proportions. `cover` fills the box and crops any excess. `none` uses the image's natural dimensions, and `scale-down` limits `contain` to the natural dimensions. The two-keyword forms `contain scale-down` and `cover scale-down` limit the corresponding fit mode to the natural dimensions. These values affect the painted image. The element's layout size comes from its sizing properties, including `aspect-ratio`. [CSS object fitting](https://www.w3.org/TR/css-images-4/#the-object-fit)

`object-position` defaults to `50% 50%`. Use one or two components such as `left`, `top right`, or `calc(50% + 4px) 25%`. Four-component positions such as `bottom 6px right 25%` set offsets from the named edges. Percentages use the difference between the box and the fitted image along each axis. The box background remains visible where the image has no coverage. The content box and its rounded corners clip the image. Flow-relative position keywords use the element's direction in horizontal writing mode. The three-component form is specific to backgrounds. [CSS object positioning](https://www.w3.org/TR/css-images-3/#the-object-position) · [CSS position values](https://drafts.csswg.org/css-values-5/#position)

## Image sampling

`image-rendering` inherits and controls how local raster textures are sampled in images, backgrounds, and markers. `auto` uses the Godot canvas default. `smooth` uses linear filtering, `high-quality` uses linear filtering with any available texture mipmaps, and `crisp-edges` uses nearest-neighbor filtering. The legacy `optimizeQuality` and `optimizeSpeed` values map to `smooth` and `crisp-edges`, respectively.

`pixelated` first enlarges each axis to its closest positive integer multiple with nearest-neighbor filtering, then linearly samples that intermediate image at the displayed size. This sharpens enlarged pixel art and limits blending to transitions between source pixels. Godot samples the virtual intermediate image directly without allocating a larger texture. [CSS image-rendering](https://www.w3.org/TR/css-images-3/#the-image-rendering)

## Background attachment

`background-attachment` accepts `scroll`, `local`, or `fixed` per layer. The default `scroll` moves with the element box. `local` follows the element's scrolled contents and sizes against its scrollable area. `fixed` uses the document's viewport for positioning and sizing. The image remains visible through the element's background clip. Non-root transformed elements and descendants of transformed elements treat `fixed` as `scroll`. [CSS Backgrounds attachment](https://www.w3.org/TR/css-backgrounds-3/#the-background-attachment) · [CSS Transforms rendering model](https://www.w3.org/TR/css-transforms-1/#transform-rendering)

Wheel input in HTMLLabel and MarkdownLabel's HTML blocks scrolls the nearest eligible `overflow: auto` or `overflow: scroll` box under the pointer. Inner scroll offsets are retained when the same DOM elements are laid out again. Standalone HTMLLayout uses the viewport passed to `layout()` with its origin at the document origin. Each Markdown HTML block has its own viewport.

In HTMLLabel, `textarea` wraps text by default. Use `wrap="off"` or `white-space: pre` for horizontal scrolling. `overflow: auto` shows internal scrollbars as needed.

`resize` accepts `none`, `both`, `horizontal`, `vertical`, `inline`, and `block`. Scroll containers with `overflow: hidden`, `auto`, or `scroll` can show a resize grip even when no scrollbars are visible. Textareas default to `resize: both`. In both color schemes, the grip blends translucent black with an offset white highlight for contrast over the element background. Dragging the grip updates the applicable inline `width` and `height` in pixels, subject to CSS sizing and cascade rules. `inline` and `block` correspond to horizontal and vertical resizing in the supported horizontal writing mode. [Chromium resizer paint](https://raw.githubusercontent.com/chromium/chromium/main/third_party/blink/renderer/core/paint/scrollable_area_painter.cc)

## Background positions

`background-position: right 12px bottom 8px` places the image 12 CSS pixels inward from the right and 8 inward from the bottom. Edge pairs can be reordered and one offset can be omitted in the three-component form. In a two-component value such as `right 8px`, the length is the vertical position. Negative offsets are allowed. Percentages, including those in CSS math, use the positioning area minus the final tile size on each axis, including when that difference is negative. [CSS Backgrounds positioning](https://www.w3.org/TR/css-backgrounds-3/#the-background-position)

`background-position-x` and `background-position-y` accept individual positions such as `right 10px` and `bottom 20%`. Their comma-separated lists cycle independently and can override the corresponding part of either background shorthand. Flow-relative positions and the `background-position-inline` and `background-position-block` properties use horizontal writing mode and respect the element's `direction`. [CSS Backgrounds position longhands](https://drafts.csswg.org/css-backgrounds-4/#propdef-background-position-x) · [CSS Values flow-relative positions](https://drafts.csswg.org/css-values-5/#position)

## Background tiling

`background-repeat` and the `background` shorthand accept one or two tiling keywords per layer. An omitted second keyword uses the first for both axes. `repeat-x` and `repeat-y` place a single tile on the other axis. In the current horizontal writing mode, `repeat-inline` and `repeat-block` select the horizontal and vertical axes, respectively.

`round` adjusts the tile size so a whole number fits in the background positioning area. When only one axis rounds and the other axis uses an automatic size, the other dimension follows the original tile ratio. `space` puts whole tiles against the two edges and distributes gaps equally. If fewer than two tiles fit, one tile is placed according to `background-position` on that axis. Both patterns extend through the painting area selected by `background-clip`.

`background-repeat-x` and `background-repeat-y` cascade as independent lists. `background-repeat-inline` and `background-repeat-block` map to them in horizontal writing mode. Textures, gradients, solid image fallbacks, and cross-fades share the tiling geometry. Background positions and sizes also accept CSS length and percentage math. [CSS Backgrounds 3 tiling and sizing](https://www.w3.org/TR/css-backgrounds-3/#the-background-repeat) · [CSS Backgrounds 4 repeat longhands](https://drafts.csswg.org/css-backgrounds-4/#the-background-repeat)

## Background blending

`background-blend-mode` accepts all 16 CSS blend modes, including `multiply`, `screen`, `soft-light`, `hue`, and `luminosity`. Each image blends with the layers below it and the background color. The element's background forms an isolated stack above the document backdrop. Short mode lists repeat, excess modes are unused, and `none` image entries consume a mode without drawing. The `background` shorthand resets the mode to `normal`.

Blend modes use sRGB components and apply source-over compositing with alpha. Each layer keeps its own positioning, tiling, and clip, including rounded corners. Raster images, gradients, and cross-fades use the same compositing path. [CSS background blending](https://www.w3.org/TR/compositing-1/#background-blend-mode) · [Blend formulas](https://www.w3.org/TR/compositing-1/#blending)

## Element blending and isolation

`mix-blend-mode` blends the element and its painted descendants with content already painted in the enclosing stacking context. It supports the same modes as `background-blend-mode` plus `plus-lighter` for additive compositing of color and alpha. Element filters, clips, and opacity shape the foreground before it blends. Successive blending elements see the preceding result.

`isolation: isolate` creates a stacking context with a transparent initial backdrop. `auto` follows the element's other stacking-context properties. For example, an explicit positioned `z-index`, a transform, or opacity below one still isolates descendant blending. A blend mode other than `normal` and explicit isolation also flatten the element's descendants in a surrounding `preserve-3d` scene. The resulting plane blends at its depth in that scene.

The HTML document is isolated from other Godot canvas items. [CSS stacking context blending](https://www.w3.org/TR/compositing-1/#csscompositingrules_CSS) · [`mix-blend-mode` and `plus-lighter`](https://drafts.csswg.org/compositing-2/#mix-blend-mode) · [`isolation`](https://www.w3.org/TR/compositing-1/#isolation)

Relatively and absolutely positioned boxes with `z-index: auto` paint their in-flow contents together at level zero. Their positioned descendants and nested stacking contexts participate in the enclosing stacking context while retaining ancestor clipping. Painting, blend backdrops, and pointer targeting follow the resulting order. Fixed and sticky boxes create stacking contexts even with `z-index: auto`. [CSS Positioned Layout painting order](https://www.w3.org/TR/css-position-3/#painting-order)

## Outlines

Use `outline: 2px solid currentColor` and `outline-offset: 3px` to draw a ring separated from the border. The shorthand sets `outline-width`, `outline-style`, and `outline-color`. Signed offsets cascade independently and a negative offset moves the ring inward. Very negative offsets are constrained on each axis to keep the ring visible. Outlines contribute ink only, with no change to box dimensions, pointer hit areas, or scrolling.

The width accepts the border width keywords and nonnegative lengths. All border line styles except `hidden` are supported. `outline-style: auto` draws a two-pixel solid ring. Its default color follows the operating system accent when available, with a blue fallback. Explicit CSS colors apply to automatic rings too. For other styles, the automatic color resolves to `currentColor`.

An outline follows the element's rounded border edges and encloses wrapped inline fragments with one ring per rendered box. Outlines paint above the contents of their drawing group and follow element transforms and ancestor overflow clips. The supported colors include the CSS color functions listed above. [CSS Basic User Interface outlines](https://www.w3.org/TR/css-ui-4/#outline)

## Border images

`border-image-source` accepts the supported CSS image values, including local URLs, gradients, `image()`, `image-set()`, and `cross-fade()`. URLs imported through local stylesheets become texture dependencies of the HTML resource. With `none` or an unavailable URL, the ordinary border paints.

Use `border-image: url(frame.png) 24 fill / 12px / 4px round stretch` to cut 24 source pixels from each edge, paint the middle, draw 12 CSS pixel wide slices, and extend the image area by 4 CSS pixels. Slice values can be numbers or percentages. Width accepts lengths, percentages, border-width multipliers, or `auto`. Outset accepts lengths or border-width multipliers. Both shorthands validate as a unit and `border` resets the five border-image longhands.

`border-image-repeat` accepts one or two values. `stretch` fills the region, `repeat` centers and tiles, `round` adjusts tile sizes for a whole number, and `space` keeps whole tiles with equal gaps before, between, and after them. Wrapped inline fragments share horizontal tiling. The rectangular image ignores the element's own border radius and follows ancestor overflow clips and element transforms. An outset affects painting only. Collapsed table borders use ordinary border painting. [CSS Border Images](https://www.w3.org/TR/css-backgrounds-3/#border-images)

## Image orientation

`image-orientation` inherits. The default `from-image` uses the EXIF orientation from PNG, JPEG, or WebP metadata preceding the pixel data. `none` uses the encoded pixels. Imported HTML stores the orientation alongside its texture dependencies so it is available when the original raster file is omitted from an export. Reimport existing HTML files to store this metadata for export.

For content images, the optional angle and `flip` can appear in either order. The angle rounds to the nearest quarter turn, with ties toward positive infinity, and the horizontal flip happens after rotation. Decorative images use EXIF when given a legacy angle or flip. Natural dimensions, fitting, image fragments, and background image sizing use the oriented image. [CSS image orientation](https://www.w3.org/TR/css-images-3/#the-image-orientation)

## Image resolution

Use `image-resolution: 192dpi` or `image-resolution: 2x` to make a raster image's natural CSS width and height half its pixel dimensions. The property inherits and applies to content images and raster sources in backgrounds, including fragments and cross-fades. It accepts the CSS resolution units and math functions. SVG uses its vector dimensions. [CSS image-resolution](https://www.w3.org/TR/css-images-4/#the-image-resolution)

`from-image` reads equal horizontal and vertical densities with physical units from PNG, JPEG, and WebP metadata before pixel data. The explicit resolution in `from-image 2x` supplies a fallback. A plain image defaults to one image pixel per CSS pixel. An `image-set()` selection supplies its declared density or its metadata as the natural density. An explicit `image-resolution` without `from-image` overrides that density. [CSS image metadata](https://www.w3.org/TR/css-images-3/#image-metadata) · [CSS image-set()](https://www.w3.org/TR/css-images-4/#image-set-notation)

`snap` chooses the nearest resolution with a whole-pixel sampling ratio against Dockit's `1dppx` output. The HTML importer stores the source DPI with the document so imported textures retain their natural CSS size in exports.

## Image candidates

Use `image-set("icon.png" 1x, url("icon-2x.png") 2x)` in a background layer. Omitted resolutions count as `1x`. Resolutions accept `x`, `dppx`, `dpi`, `dpcm`, and typed math functions such as `calc(96dpi + 1dppx)`. Resolution media queries use the same units and math. [CSS resolution values](https://www.w3.org/TR/css-values-4/#resolution)

`type()` accepts PNG, JPEG, WebP, and SVG MIME types. Unsupported MIME types are skipped before ties are resolved in declaration order. Dockit chooses the smallest supported density at or above its CSS output resolution of `1dppx`. If every density is lower, it chooses the highest available density. The selected resolution scales a raster image's natural CSS size. SVG and gradients use their usual sizing rules. [CSS image-set()](https://www.w3.org/TR/css-images-4/#image-set-notation)

The HTML importer embeds local URL and string candidates from each collected stylesheet, including direct string fallbacks in `var()`. Relative paths use the stylesheet's directory.

Content images support `srcset` width and pixel-density descriptors, `sizes` media conditions and length math, and `<picture>` sources with `media` and `type`. They use the current layout viewport and the same `1dppx` output density. Width candidates use the first matching source size, with `100vw` as the default. The selected source's dimensions become HTML sizing hints when provided. The importer records local candidates across media branches as texture dependencies. [HTML responsive images](https://html.spec.whatwg.org/multipage/images.html)

If the selected texture is unavailable, an image with nonempty `alt` draws the alternative text inside its existing box. The text uses the computed font, color, line height, alignment, and wrapping mode. It provides automatic box dimensions and follows the content clip and rounded corners. The intrinsic image size stays zero until a usable texture is selected. [HTML alternative text](https://html.spec.whatwg.org/multipage/embedded-content.html#the-img-element)

## Image fallbacks and fragments

Use `image("sprite.png", currentColor)` to paint a solid color if the source cannot load. Use `image(rgb(0 0 255 / 50%))` as an independent solid-color layer. Both forms use the background layer's size, position, clip, and repeat. A successfully loaded transparent image retains its transparency. [CSS image()](https://www.w3.org/TR/css-images-4/#image-notation)

Use a fragment such as `image("sprites.png#xywh=40,0,20,20")` to paint a cropped image with its own natural dimensions. The fragment accepts pixel coordinates, an explicit `pixel:` prefix, or `percent:` coordinates. Crop rectangles intersect the source image bounds. An unrecognized fragment or an empty intersection uses the fallback if specified. The importer records the source texture as a dependency. Dockit caches each cropped texture within the layout. [Media Fragments URI](https://www.w3.org/TR/media-frags/#naming-space)

An annotation such as `image(ltr "arrow.png")` flips the image horizontally in an RTL element. Background image direction follows inherited `direction: ltr | rtl` and HTML `dir="ltr"` or `dir="rtl"` hints. The directional flip is applied after the source's `image-orientation`. [CSS bidi-sensitive images](https://www.w3.org/TR/css-images-4/#image-notation)

## Image blending

Use `cross-fade(url("photo.png") 75%, currentColor)` in a background layer. Omitted weights share what remains up to 100%. If explicit weights sum above 100%, their contributions are normalized. Any remainder after all weights are assigned contributes transparency. The color and alpha channels are averaged in premultiplied sRGB. [CSS cross-fade()](https://www.w3.org/TR/css-images-4/#cross-fade-function)

Images with natural dimensions contribute to the weighted width and height. Colors and gradients fill the resulting tile. Nested cross-fades distribute their weights through the inputs. The background layer's size, position, repeat, and clip apply to the result. Dockit rasterizes composite tiles with up to 2048 pixels per side before drawing them through Godot.

## Length units

- Absolute lengths include `px`, `in`, `cm`, `mm`, `Q`, `pt`, and `pc`. Dockit uses 96 CSS pixels per inch.
- Font-relative lengths include `em`, `ex`, `cap`, `ch`, `ic`, and `lh`. The root equivalents are `rem`, `rex`, `rcap`, `rch`, `ric`, and `rlh`. `lh` uses the computed CSS line height, including a font-metrics value for `normal`. When `font-size` uses a local font-relative unit, it uses the parent's metrics. `lh` in `line-height` also uses the parent's line height. A self-reference at the root uses initial metrics.
- `vw`, `vh`, `vi`, `vb`, `vmin`, and `vmax` follow the layout viewport. Dockit uses horizontal writing, so `vi` follows width and `vb` follows height. The `sv*`, `lv*`, and `dv*` variants use the same viewport because Dockit has a single viewport size per layout pass.

Length-based media queries resolve font units against the initial font metrics. They reevaluate viewport units and length calculations when the viewport changes. [CSS Values and Units Level 4](https://www.w3.org/TR/css-values-4/#lengths)

## Generated content and counters

`::before`, `::after`, and `::marker` accept text, `attr()` text, quotes, counters, and the supported CSS image types in `content`. Images can be mixed with text in the same declaration. A single image in `::before` or `::after` uses the pseudo-element's box, including its dimensions, `object-fit`, and `object-position`. Images without natural dimensions, such as gradients, need a definite size to paint. Relative URLs use the stylesheet's directory. Generated text participates in rendered text, and the generated content stays outside DOM queries. [CSS Generated Content](https://www.w3.org/TR/css-content-3/#content-property)

For example, `content: attr(data-title, "Untitled")` substitutes the element's attribute as literal text and uses the fallback for a missing attribute. `attr(data-title raw-string, "Untitled")` is also supported.

The optional slash in `content` introduces an accessibility alternative, as in `content: url(status.png) / attr(data-status, "Ready")`. Alternatives can concatenate strings, `attr()`, `counter()`, and `counters()`. An empty alternative marks the generated content as decorative. `HTMLLayout.get_accessible_text()` and `HTMLLabel.get_accessible_text()` return the resulting text with HTML image `alt` fallbacks. HTMLLabel publishes it to Godot as a single text value. The part before the slash is used for drawing, search, and selection. [CSS alternative text](https://drafts.csswg.org/css-content-3/#alt)

A single image in `content` can also replace a regular HTML element's rendered contents. Its children and generated boxes are suppressed while the DOM, including link targets and image attributes, remains available. The replacement uses its own natural dimensions or the element's CSS size and honors `object-fit` and `object-position`. Existing `img` elements use the replacement image instead of their HTML dimension hints while the rule applies. Checkboxes and radio buttons can receive a replacement image and retain their checked state. Buttons, text inputs, and select, textarea, progress, and meter controls use their control rendering. [CSS content replacement](https://drafts.csswg.org/css-content-3/#content-replacement)

The inherited `quotes` property accepts `auto`, `none`, `match-parent`, and pairs of strings. Automatic quote pairs follow English, French, German, or Japanese conventions, with English as the default. `open-quote` and `close-quote` track nesting in document order. Their `no-` variants adjust nesting without drawing a mark. [CSS quotation marks](https://www.w3.org/TR/css-content-3/#quotes)

Use `counter-reset`, `counter-increment`, and `counter-set` to create or change named counters. `counter()` reads the innermost value. `counters()` joins values across nested scopes. Both support `decimal`, `decimal-leading-zero`, Latin alphabetic styles, `lower-greek`, Roman numerals, and symbolic styles such as `disc`. The default style is `decimal`. Counters on elements with `display: none` and pseudo-elements without generated content have no effect. [CSS Lists and Counters](https://www.w3.org/TR/css-lists-3/#auto-numbering)

### List markers

`::marker` generates content before `::before` on a list item. Use `content: none` to suppress it, or `content: normal` to use the item's `list-style-image` or `list-style-type`. The type can be a keyword or a quoted string, including in the `list-style` shorthand. The marker accepts inherited text styling such as color, font, and text transformation. `list-style-position: inside` places it in the first line's inline flow. `outside` aligns it with the first line beside the item. [CSS list markers](https://www.w3.org/TR/css-lists-3/#marker-pseudo)

`list-style-image` and the `list-style` shorthand accept local URLs, gradients, `image()`, `image-set()`, `cross-fade()`, and `light-dark()`. The marker uses the image's natural size, with a `1em` square as the default object size for images without natural dimensions. An unavailable image uses the marker string or counter from `list-style-type`. A transparent image from `light-dark()` still occupies its marker box. Relative URLs use the stylesheet's directory. [CSS image markers](https://drafts.csswg.org/css-lists-3/#list-style-image-property)

The `list-item` counter works with `counter()` and `counters()`. HTML ordered lists initialize it from `start` and `reversed`, and a list item's `value` sets its ordinal. The HTML `type` hint participates in the cascade. To control numbering in CSS, set `counter-reset`, `counter-increment`, or `counter-set` with the `list-item` name. [CSS list-item counters](https://www.w3.org/TR/css-lists-3/#list-item-counter)

### Custom counter styles

```css
@counter-style chapter {
  system: extends upper-roman;
  prefix: "[";
  suffix: "] ";
}

ol { list-style: chapter inside; }
```

Counter styles support `cyclic`, `fixed`, `symbolic`, `alphabetic`, `numeric`, `additive`, and `extends`. Text symbols can be strings or identifiers. Anonymous `symbols()` styles use quoted strings. Both kinds of style work with list markers, `counter()`, and `counters()`. The `prefix` and `suffix` descriptors apply to the default marker. The `negative`, `range`, `pad`, and `fallback` descriptors also apply to generated text. Styles defined in imports and conditional rules follow the author layer and source order. Built-in numeric styles include CJK, Arabic-Indic, and several other Unicode decimal systems. Image symbols and `speak-as` are outside the supported grammar. [CSS Counter Styles](https://www.w3.org/TR/css-counter-styles-3/)

## Variables and math functions

```css
:root { --gutter: 16px; }
.panel {
  width: min(100%, calc(48rem - 2 * var(--gutter, 16px)));
  padding: clamp(8px, 2vw, 24px);
}
```

Custom property names are case-sensitive. Values resolve before inheritance. `var()` supports repeated references, nested and empty fallbacks, and detects cycles, including references in fallback arguments. Invalid substitutions use the property's inherited or initial value. Declaration diagnostics identify the affected element and retain the original declaration. [CSS Custom Properties](https://www.w3.org/TR/css-variables-1/#using-variables)

Shorthands validate the full value before applying their longhands. Omitted components reset to their shorthand defaults. A shorthand with `var()` resolves once per element and updates the longhands it still wins. A winning declaration with an invalid substitution uses `unset`. A rollback without an eligible declaration uses `unset`. [CSS Cascading and Inheritance](https://www.w3.org/TR/css-cascade-5/)

Length expressions are supported in `width`, `height`, their `min-` and `max-` variants, `margin`, `padding`, `top`, `right`, `bottom`, `left`, `font-size`, `line-height`, `flex-basis`, Flexbox and Grid gaps, and length entries in Grid track lists. `line-height` also accepts math that produces a unitless number. Percentage arguments use the basis supplied by each layout property, including when the result depends on a nested `sign()` or `atan2()`.

- Arithmetic and comparisons use `calc()`, `min()`, `max()`, and `clamp()`. Put spaces around `+` and `-`. Multiplication takes a number on at least one side. Division accepts a number or a divisor of the same type as the dividend.
- `round()` accepts `nearest`, `up`, `down`, and `to-zero`, with an optional step of 1 for numbers. `mod()` follows the sign of the divisor. `rem()` follows the sign of the dividend.
- Trigonometric functions include `sin()`, `cos()`, `tan()`, `asin()`, `acos()`, `atan()`, and `atan2()`. Forward functions accept an angle or a number in radians. Inverse functions return angles. Other functions include `pow()`, `sqrt()`, `hypot()`, `log()`, `exp()`, `abs()`, and `sign()`.
- Math accepts `e`, `pi`, `infinity`, `-infinity`, and `NaN`. The renderer bounds layout values to its representable range. Transform scalars, color components, and color mixing weights support calculations that can resolve without layout lengths.

[CSS Values and Units](https://www.w3.org/TR/css-values-4/#math)

## Color functions

Use absolute `hwb()`, `lab()`, `lch()`, `oklab()`, and `oklch()` wherever Dockit accepts a color. Components support numbers and percentages where defined by the color space, and hue accepts degrees or an angle in `deg`, `grad`, `rad`, or `turn`. Use `/` to specify alpha. A component set to `none` is treated as zero when painting a color directly. [CSS Color 4](https://www.w3.org/TR/css-color-4/#color-syntax)

`color(display-p3 0.445 0.529 0.891)` selects a predefined space. Supported spaces are `srgb`, `srgb-linear`, `display-p3`, `display-p3-linear`, `a98-rgb`, `prophoto-rgb`, `rec2020`, `xyz-d50`, and `xyz-d65` (also spelled `xyz`). Components outside the nominal gamut are retained for interpolation. [Predefined color spaces](https://www.w3.org/TR/css-color-4/#predefined)

`color-mix(in oklch shorter hue, currentColor 30%, white)` mixes one or more colors in any of these predefined spaces, as well as `lab`, `lch`, `oklab`, `oklch`, `hsl`, and `hwb`. Omitting the interpolation method uses `oklab`. For polar spaces, hue can follow the `shorter`, `longer`, `increasing`, or `decreasing` arc. Missing components carry over when applicable, and non-hue components use premultiplied alpha. [CSS Color 4 interpolation](https://www.w3.org/TR/css-color-4/#interpolation)

Each input can include a weight from `0%` to `100%`. Omitted weights share the remainder. Polar mixes combine colors in source order. Explicit weights are normalized, and when the total is below `100%`, it also scales the output alpha. A total of zero yields a transparent color. Custom properties can supply the inputs or weights. Mixes that contain `currentColor` are resolved in the element that uses the property. In the `color` property itself, they use the inherited text color. Nested mixes retain floating-point colors until painting, when channels are clipped to Dockit's 8-bit sRGB output. [CSS Color 5](https://www.w3.org/TR/css-color-5/#color-mix)

Relative colors use `from <color>` in the modern space-separated forms of `rgb()`, `hsl()`, `hwb()`, `lab()`, `lch()`, `oklab()`, `oklch()`, and `color()` with a predefined color space. For example, `rgb(from currentColor calc(r / 2) g b / calc(alpha * .8))` halves the red channel and scales the alpha. Dockit converts the origin to the destination space before evaluating the channel keywords and calculations. A bare keyword preserves a missing component, while arithmetic uses zero for a missing operand. Omitted alpha comes from the origin. The result retains out-of-gamut channels for further color operations, with alpha limited to `0`–`1`. System colors use the element's color scheme. [CSS relative colors](https://drafts.csswg.org/css-color-5/#relative-colors)

`light-dark(light, dark)` selects a color or image using the element's used `color-scheme`. Both arguments must be colors or both must be images. In the image form, `none` is a transparent image without intrinsic dimensions. Inherited values keep the selected branch, and `currentColor` continues to follow the normal rules for the property that uses it. [CSS `light-dark()`](https://drafts.csswg.org/css-color-5/#light-dark)

`contrast-color(background)` returns opaque white or black, whichever has greater contrast against the input. Dockit compares relative luminance after converting the input to sRGB and clipping its channels. It treats the input as a solid color and chooses white on ties. Both functions accept variables and nested color functions. [CSS `contrast-color()`](https://drafts.csswg.org/css-color-5/#contrast-color)

Markdown HTML blocks use the full HTML color pipeline. Markdown's inline `<span style>` supports constant mixes in `color` and `background-color`.

`opacity` clamps its computed value to the range `0` through `1`. Values below `1` establish a stacking context and composite the painted element and its descendants together. Each element defaults to `1`, and positioned descendants use the usual containing block. An element with zero opacity still participates in layout, selection, and pointer targeting. Large groups render in tiles, with very large surfaces cropped to the visible drawing area. [CSS Opacity](https://www.w3.org/TR/css-color-4/#transparency) · [Compositing groups](https://www.w3.org/TR/compositing-1/#groupcompositing)

## Transitions

Use `transition: width 200ms ease-out, opacity 150ms linear`, or set the property, duration, delay, easing, and behavior with separate longhands. Shorter lists repeat to match the property list. Durations and delays accept seconds and milliseconds. A negative delay starts partway through a transition. Easing supports the standard keywords, `cubic-bezier()`, `steps()`, and `linear()` with intermediate stops. Reversing a running transition starts from its current value and shortens its duration. Style changes can also cancel a transition by removing its property from the list. The initial layout uses the authored values immediately. [CSS Transitions](https://www.w3.org/TR/css-transitions-1/) · [CSS Easing](https://www.w3.org/TR/css-easing-2/)

Supported values include `opacity`, `color`, `background-color`, border colors, and `transform`, plus the following geometry and spacing properties:

- `width`, `height`, and their minimum and maximum constraints.
- All four insets, margins, and paddings, both axes of the corner radii, the border widths, and `outline-width` and `outline-offset`.
- `row-gap`, `column-gap`, both axes of `border-spacing`, `flex-basis`, `flex-grow`, and `flex-shrink`.
- All four sides of `scroll-padding` and `scroll-margin`.

Shorthand targets such as `inset`, `gap`, `flex`, and `border-left` apply to their supported longhands. Supported logical longhands and shorthands follow the element's direction in horizontal writing. The last matching entry supplies the timing for each longhand. Mixed lengths and percentages resolve during layout, and signed properties retain negative values. Keywords such as `auto` and `normal` switch discretely with `transition-behavior: allow-discrete` when a numeric interpolation is unavailable. [CSS Transitions Level 2](https://www.w3.org/TR/css-transitions-2/)

Colors participate in inheritance and `currentColor` resolution. Matching translate, scale, skew, and rotation functions interpolate their parameters, including rotations with complete turns and interpolation from `none`. At the first unmatched function, the remaining transforms are composed and interpolated through 2D or 3D matrix decomposition. Explicit matrix and perspective functions also use decomposition. Percentage translations use the current reference box, including while its size is changing. Transitions on generated pseudo-elements remain pending. [CSS transform interpolation](https://www.w3.org/TR/css-transforms-1/#interpolation-of-transforms) · [3D interpolation](https://www.w3.org/TR/css-transforms-2/#matrix-interpolation)

## Animations

In `HTMLLabel`, the Inspector's `animation_process_mode` controls how both CSS animations and transitions advance. `Idle` is the default, `Physics` uses the physics step, and `Manual` advances with calls to `advance_animation(delta)` in seconds. Changing modes preserves playback progress and remaining delays. Pass `0.0` to apply pending layout changes at the current time.

`@keyframes` accepts case-sensitive names, `from`, `to`, percentages, and comma-separated offset lists. Declarations at the same offset follow source order. Each property uses only its own keyframes and takes its underlying value at omitted endpoints. Keyframes resolve `var()` on the animated element and discard `!important` declarations. When several rules define the same name, the last rule in the highest-priority layer wins. Local `@import`, `@media`, and `@supports` can supply keyframe rules. [CSS Animations keyframes](https://www.w3.org/TR/css-animations-1/#keyframes)

For example, `animation: Pulse 1s ease-in-out 2 alternate both` plays two iterations and keeps the final sampled value. Comma-separated names play in order with replacement, and shorter longhand lists repeat. The eight longhands control the name, duration, delay, easing, iteration count, direction, fill mode, and play state. A keyframe's `animation-timing-function` controls the following segment of the properties declared at that offset. Negative delays start partway through an animation. Fractional counts, `infinite`, all directions and fill modes, and pause and resume are supported. [CSS Animations properties](https://www.w3.org/TR/css-animations-1/#animations)

Animations share the property support and interpolators listed under Transitions. They run on elements and generated `::before` and `::after` boxes. A running transition takes precedence over the animation sample, and author `!important` declarations take precedence over animations. Transition endpoints currently use the underlying authored values when both effects target the same property. Animation events, additional animated properties, non-default composition modes, and scroll-driven timelines remain pending. [CSS Animations cascade](https://www.w3.org/TR/css-animations-1/#animations)

In Markdown HTML blocks, the block's allocation in the surrounding Markdown still uses its initial layout height. Animated geometry is sampled within that allocation.

## Filter effects

`filter` accepts `none` or a space-separated list of `blur()`, `brightness()`, `contrast()`, `drop-shadow()`, `grayscale()`, `hue-rotate()`, `invert()`, `opacity()`, `saturate()`, and `sepia()`. Amounts accept numbers, percentages, or compatible CSS math. A drop shadow uses two offsets, an optional Gaussian standard deviation, and an optional color. The color defaults to `currentColor`.

The renderer applies the functions in declaration order to the element and its painted descendants. Blur and shadow offsets follow the element’s transform. The final group uses ancestor clipping and the `opacity` property. Filter ink can extend beyond the box, while document geometry and pointer targeting use the layout box. A filter creates a stacking context. A filtered element other than the document root also provides a containing block for absolute and fixed descendants. Spatial filters use tiled buffers with a sampling margin of up to 1,024 device pixels per axis. SVG `url()` filter references remain pending. [CSS Filter Effects](https://www.w3.org/TR/filter-effects-1/#FilterProperty)

`backdrop-filter` uses the same functions to filter the pixels already painted within the nearest Backdrop Root. The result is clipped to the element's transformed border box and rounded corners, then drawn below its background and content. Blur reflects samples at the edges of the border box. The combined image then participates in ordinary filtering, clipping, masking, opacity, and blending. A non-root element with a backdrop filter establishes a containing block for absolute and fixed descendants. [Backdrop filters and clipping](https://drafts.csswg.org/filter-effects-2/#BackdropFilterProperty)

The document root, group opacity below one, filters, backdrop filters, masks, clipping paths, and element blend modes other than `normal` establish Backdrop Roots. `will-change` hints for those effects also establish Backdrop Roots. A transform or numeric `z-index` can create a stacking context while keeping access to previously painted pixels within the same Backdrop Root. The HTML document supplies its own backdrop inside the Godot canvas. [Backdrop Root](https://drafts.csswg.org/filter-effects-2/#BackdropRoot)

`will-change` accepts `auto` or a comma-separated list of property names and hints. Transform, filter, and applicable containment hints establish containing blocks for absolute and fixed descendants and establish stacking contexts. A `position` hint establishes an absolute containing block and a stacking context. A `z-index` hint applies to positioned boxes and to Flexbox and Grid items. Transform hints also make fixed backgrounds sample in the local coordinate space. Dockit accepts `scroll-position`, `contents`, and unknown feature names without additional rendering effects. [CSS Will Change](https://www.w3.org/TR/css-will-change-1/#will-change)

## Image masks

`mask-image` accepts a list of CSS images, including project-local textures, gradients, and the supported CSS image functions. An unavailable image contributes a transparent layer. A list containing only `none` disables masking. In a list with another source, `none` contributes a transparent layer.

`mask-position`, `mask-size`, and `mask-repeat` use the same length, percentage, and tiling rules as backgrounds. The initial positioning and painting area is the border box. `mask-origin` selects the positioning box and `mask-clip` sets the painting box. Use `no-clip` to include overflowing ink. The `mask` shorthand sets these properties together with the image, mode, and composite operator. Omitted components reset to their initial values.

`mask-mode` accepts `match-source`, `alpha`, and `luminance`. For CSS images, `match-source` uses alpha. Luminance multiplies the weighted color channels by the source alpha. `mask-composite` accepts `add`, `subtract`, `intersect`, and `exclude`. Short lists repeat to cover all image layers. The bottom layer is placed first and its operator is ignored.

The mask applies to the element and its rendered descendants as a group after filtering and before opacity and element blending. Masking creates a stacking context and leaves layout boxes and pointer hit areas intact. Local mask images use the same import dependencies as background images. `mask-border` is pending. [CSS Masking](https://www.w3.org/TR/css-masking-1/#positioned-masks)

`url(#mask-id)` references an inline SVG `<mask>`. Use `url(local.svg#mask-id)` for a project-local SVG file. SVG masks use their own region and content coordinates through `maskUnits`, `maskContentUnits`, `x`, `y`, `width`, and `height`. CSS mask position, size, repeat, origin, and clip apply to ordinary image layers. With `match-source`, an SVG mask uses its computed `mask-type` and defaults to luminance. SVG luminance can use `color-interpolation: linearRGB`. Imported HTML resources retain the source SVG. SVG artwork uses Godot's SVG rasterizer, with Dockit's SVG text outlines and same-document `<use>` references. SVG `<image>` children and external references inside the source SVG are pending. [SVG mask sources](https://www.w3.org/TR/css-masking-1/#svg-masks)

## Clipping paths

`clip-path` accepts a shape with an optional reference box in either order. The default reference is the border box. The `margin-box`, `border-box`, `padding-box`, and `content-box` keywords can also clip to the selected box on their own, including its corner radii. In HTML, `fill-box` uses the content box, while `stroke-box` and `view-box` use the border box.

`inset()`, `rect()`, and `xywh()` support rounded corners after `round`. Circles and ellipses accept an optional center after `at`, with radii supplied as lengths, percentages, `closest-side`, or `farthest-side`. The path moves with the element's CSS transform, clips its own ink and descendants after filtering, and creates a stacking context. Pointer hits and selection geometry respect the path. Layout boxes and scrollable overflow retain their normal dimensions. Use `none` to remove the path. [CSS Masking](https://www.w3.org/TR/css-masking-1/#the-clip-path) · [CSS Basic Shapes](https://www.w3.org/TR/css-shapes-1/#basic-shape-functions)

`polygon()` closes the supplied list of vertices. Each coordinate can use lengths, percentages of the reference box, or CSS math. The default `nonzero` and optional `evenodd` fill rules apply to concave and self-intersecting outlines, including holes. Nested paths intersect. A polygon with fewer than three vertices has an empty fill. [CSS polygon shape](https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-polygon) · [SVG winding rules](https://www.w3.org/TR/SVG2/painting.html#FillRuleProperty)

`path()` accepts SVG path data in a quoted string, with an optional `nonzero` or `evenodd` fill rule before the string. Commands support absolute and relative coordinates, curves, elliptical arcs, and multiple independent subpaths. The coordinate origin is the top-left of the reference box. Coordinates are CSS pixels. Closing each subpath for clipping is implicit. Drawing, pointer hits, and selection share the flattened curve geometry. [CSS path shape](https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-path) · [SVG path data](https://www.w3.org/TR/SVG11/paths.html#PathData)

`shape()` starts at a `from` position and accepts comma-separated `move`, `line`, `hline`, `vline`, `curve`, `smooth`, `arc`, and `close` commands. Absolute `to` positions use the reference box. Relative `by` offsets start at the preceding command's endpoint. Percentages use the reference box dimensions in either case. Bézier control points can also use offsets from the segment's start, end, or reference-box origin. Arc radii, sweep, size, and rotation follow the CSS Shapes rules. Positions and radii accept lengths, percentages, and CSS math. Variables can supply coordinates or whole commands. The path follows changes to layout and shares the curve geometry used by `path()`. [CSS shape function](https://drafts.csswg.org/css-shapes-1/#shape-function)

`polygon()` also accepts `round <length>` before the first comma, after any fill rule. Each vertex is replaced by a tangent circular arc. The radius is limited per vertex so the arc uses at most half of either adjoining edge. The radius accepts CSS length math.

`clip-path: url(#name)` references an inline SVG `<clipPath>` by its case-sensitive ID. A URL like `url(shapes.svg#name)` references a project-local UTF-8 SVG file. Relative URLs in CSS use the declaring stylesheet's directory. Imported HTML embeds the SVG source so clipping can work in an export without the original file. SVG files are parsed as separate documents. Their embedded styles and same-document `use` references apply.

Supported clip geometry includes `path`, `rect`, `circle`, `ellipse`, `polygon`, `polyline`, and direct `use` references to these shapes. Geometry attributes and SVG transform lists apply in either `userSpaceOnUse` or `objectBoundingBox` coordinates. Inherited `clip-rule` supports `nonzero` and `evenodd`. Hidden children are omitted. A missing or invalid reference leaves the element unclipped, and an empty valid clip removes all paint.

A `clip-path` on a child intersects that child's silhouette before the child shapes are united. A clip on the `<clipPath>` itself intersects the complete union using the original target's reference box. Stylesheet declarations and SVG presentation attributes can use either a URL or a CSS basic shape. Basic shapes on SVG children currently use the graphics bounds for their reference box. An active reference can be nested up to 24 sources, and repeated references on the same branch are skipped. [SVG clipPath and clip-rule](https://www.w3.org/TR/css-masking-1/#svg-clipping-paths)

SVG `<text>` can also supply the clipping silhouette, including through a direct `<use>` reference. Horizontal text and nested `<tspan>` runs use Godot's font fallback and shaping. Position and rotation lists use `x`, `y`, `dx`, `dy`, and `rotate`. CSS and the SVG presentation attribute support inherited `text-anchor: start | middle | end`. The font's actual outline, including the holes inside glyphs, determines painting and hit testing. Bitmap-only glyphs contribute no outline. Vertical text, alternate baselines, and automatic wrapping in SVG text remain pending. [SVG text layout](https://www.w3.org/TR/SVG2/text.html#TextElement)

`<textPath>` follows its inline `path` data or a same-document `href` or `xlink:href` reference to an SVG path or basic shape. Shape transforms affect the guide. `startOffset` accepts lengths and percentages, and `pathLength` calibrates absolute offsets. Each glyph cluster follows the local tangent without losing its outline or combining marks. `side="right"` reverses the guide. An open guide hides glyphs whose centers fall outside it, while one closed subpath can wrap around its seam. Text following the path resumes at its endpoint. Dockit uses aligned glyphs and exact spacing. Outline warping with `method="stretch"` and automatic spacing adjustments are pending. [SVG text on a path](https://www.w3.org/TR/SVG2/text.html#TextPathElement)

## Transforms

```css
.card {
  transform-origin: left top;
  transform: translate(10%, 12px) rotate(8deg) scale(1.1);
}
```

Translation and origin offsets accept lengths, percentages, and length expressions. Percentages and the default center origin use the transform reference box. On HTML elements, `content-box` and `fill-box` use the content box. `border-box`, `stroke-box`, and the default `view-box` use the border box. The optional third length in `transform-origin` has no effect on a 2D transform.

The individual properties compose in the order `translate`, `rotate`, and `scale`, followed by the Motion Path offset and the `transform` list. Independent `translate` accepts an optional Z length. `rotate` accepts an angle with an optional axis keyword or vector. `scale` accepts numbers or equivalent percentages for up to three axes. An explicit identity value establishes a stacking context and a containing block for positioned descendants. `none` removes the corresponding transform. [CSS Transforms Level 2](https://www.w3.org/TR/css-transforms-2/#ctm)

The `transform` list accepts `matrix()`, the XYZ translate, rotate, and scale variants, and `skew()`, `skewX()`, and `skewY()`. Use `translate3d()`, `rotate3d()`, `scale3d()`, or `matrix3d()` to specify multiple axes at once. Composition uses all three axes and the third `transform-origin` length, then projects the result onto the parent plane under the default `transform-style: flat`. `backface-visibility: hidden` excludes a back-facing surface and its descendants from painting and hit testing. Their rectangles and scrollable area remain available. [CSS Transforms rendering model](https://www.w3.org/TR/css-transforms-2/#transform-rendering)

`perspective()` projects the element's transform list about its transform origin. The `perspective` property instead sets a viewing distance for the child planes and establishes a containing block for positioned descendants. Use `perspective-origin` to set that projection's center within the parent's reference box. Both forms accept non-negative distances and clamp distances below one CSS pixel during rendering. Painting and pointer targeting use the visible side of the projected plane, and a box crossing the viewing plane has finite scroll bounds. Projective drawing rasterizes visible source regions to intermediate textures. Spatial filters use the projection's local scale at the element center. [CSS Perspective](https://www.w3.org/TR/css-transforms-2/#perspective-property) · [Perspective-transformed boxes](https://www.w3.org/TR/css-transforms-2/#processing-of-perspective-transformed-boxes)

`transform-style: preserve-3d` lets descendants accumulate transforms in one 3D coordinate space. The renderer splits intersecting planes so each side composites in depth order. Coplanar planes follow CSS paint order. Hit testing uses depth at the pointer position. When a box flattens, its subtree composites on that box's plane in any enclosing scene. Scrollable overflow, opacity below one, filters, `clip-path`, and paint containment force flattening. `overflow: clip` keeps the 3D context under the CSS Transforms draft's grouping rules. [3D rendering contexts](https://drafts.csswg.org/css-transforms-2/#3d-rendering-contexts) · [Grouping property values](https://drafts.csswg.org/css-transforms-2/#grouping-property-values)

Scale factors can be negative, and factors and angles can use CSS math. A singular transform such as `scale(0)` hides the subtree while preserving its place in the flow. Transforms affect painting, element rectangles, links, selection, and scrollable overflow. Non-replaced inline boxes and table columns or column groups can inherit an ancestor's transform as part of its subtree. Use an inline-block for a transform on an inline element itself. In Markdown, downward overflow expands the HTML block and the column bounds clip horizontal overflow. [CSS Transforms Level 1](https://www.w3.org/TR/css-transforms-1/)

Painting and hit testing use the exact clip shapes of up to 16 rounded or transformed overflow ancestors, starting with the closest ancestor. Farther clips use their document-space bounding rectangles. An element's own rounded shape is evaluated separately.

## Motion paths

`offset-path` accepts `path()`, `shape()`, circles, ellipses, rectangles, polygons, and `ray()`. A coordinate box sizes relative shapes against the containing block. A coordinate box on its own follows that box's outline, including its rounded corners. SVG fragments can reference paths and basic shapes in the document or a project-local SVG file.

`offset-distance` accepts lengths, percentages of the path length, and CSS math. Open paths clamp distances to their endpoints, and closed paths wrap them. Rays support their five size keywords, `at` origins, and `contain`. `offset-position` supplies an omitted ray origin or circle or ellipse center. `offset-anchor: auto` uses `transform-origin`. `offset-rotate: auto` follows the path tangent, `reverse` also adds a half turn, and a bare angle fixes the orientation. The `offset` shorthand resets omitted longhands.

Motion changes the element's visual geometry, stacking, and containing-block behavior. Painting, pointer hits, and text selection use the same transform. Curves are sampled from flattened paths. CSS animation and remote SVG sources remain outside the supported set. [CSS Motion Path](https://www.w3.org/TR/motion-1/)

## Grid

```css
.cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
  grid-template-rows: auto;
  gap: 12px 16px;
}

.cards .featured {
  grid-column: 1 / -1;
  grid-row: 1;
}
```

`display: inline-grid` uses the same tracks and item placement as `grid`. An automatic width fits the content within the containing block, subject to intrinsic sizes and `min-width` and `max-width`. The whole grid moves to the next line if it cannot fit alongside preceding text. Text inside each item can wrap within its grid area. The surrounding line uses the grid's first baseline by default, and `vertical-align` can change how the box lines up with its neighbors. [CSS Grid container sizing and baselines](https://www.w3.org/TR/css-grid-2/#grid-container-sizing) · [CSS Inline baseline source](https://www.w3.org/TR/css-inline-3/#baseline-source)

Use a positive integer in `repeat()` to repeat one or more track sizes. For example, `repeat(2, 80px minmax(0, 1fr))` creates four tracks. An axis can declare up to 10,000 explicit tracks. `minmax()` accepts non-negative lengths, percentages, `auto`, `min-content`, and `max-content` on either side, with fractions also accepted as the maximum. [CSS Grid track sizing](https://www.w3.org/TR/css-grid-2/#track-sizes)

`min-content` follows the space needed at the available wrap opportunities, while `max-content` uses the preferred content size. `fit-content()` caps a track's max-content growth at a non-negative length or percentage, subject to the track's automatic minimum. A percentage cap resolves against the grid's final content size on that axis. Intrinsic measurements use the uncapped maximum until that size is known. These track sizes work in explicit templates, integer repeats, and implicit track patterns. A `fit-content()` track takes space up to its limit before content alignment distributes remaining space. [CSS Grid intrinsic track sizes](https://www.w3.org/TR/css-grid-2/#algo-content)

Each axis accepts one `repeat(auto-fill, …)` or `repeat(auto-fit, …)`. Every track in a template containing an automatic repeat needs a length or percentage as at least one of its sizing bounds. Each layout pass counts repetitions using the container's content box and gaps. With only a definite minimum size, it rounds the count up to meet that minimum. Without a definite size constraint, it repeats once. [CSS Grid automatic repetition](https://www.w3.org/TR/css-grid-2/#auto-repeat)

`auto-fill` keeps every repeated track. `auto-fit` collapses repeated tracks that no in-flow item occupies or spans. The remaining tracks have one gap between them, and the grid retains its original line numbers after collapse. [CSS Grid gutters](https://www.w3.org/TR/css-grid-2/#gutters)

Use brackets to name a grid line, as in `[outer content-start] 80px [content-end]`. Line names are case-sensitive. `repeat(3, [card] 80px [card])` produces four distinct `card` lines. `grid-column: 2 card / -1 card` uses the second matching line from the start and the last matching line. A definite edge can anchor a named span such as `grid-column: 1 / span 2 card`. `auto-fit` retains line names when it collapses empty tracks. [CSS Grid line names](https://www.w3.org/TR/css-grid-2/#named-lines) · [CSS Grid line placement](https://www.w3.org/TR/css-grid-2/#line-placement)

```css
.dashboard {
  display: grid;
  grid-template-columns: [frame] 120px minmax(0, 1fr);
  grid-template-areas: "title title" "nav main" "nav .";
}

.title { grid-area: title; }
.main { grid-area: 2 / main-start / span 2 / -1; }
```

Each string in `grid-template-areas` defines one row. Consecutive dots represent one empty cell. Rows must have equal lengths and each named area must fill a rectangle. An area adds `name-start` and `name-end` lines to both axes. The template can extend the explicit grid. `grid-area` sets row start, column start, row end, and column end, in that order. A name on its own applies to all four edges. [CSS Grid named areas](https://www.w3.org/TR/css-grid-2/#grid-template-areas-property) · [CSS Grid placement shorthands](https://www.w3.org/TR/css-grid-2/#placement-shorthands)

`grid-row`, `grid-column`, and their `-start` and `-end` longhands accept names, nonzero integer line numbers, `span`, and `auto`. Positive line numbers count from the start of the explicit grid. Negative line numbers count back from its end, so `grid-column: 1 / -1` spans the explicit grid even when implicit columns exist. Placement coordinates are limited to 10,000 tracks on each side of the explicit origin. An area entirely beyond the limit uses the nearest edge track. [CSS Grid placement](https://www.w3.org/TR/css-grid-2/#placement) · [Large grid limits](https://www.w3.org/TR/css-grid-2/#overlarge-grids)

Explicit areas may overlap. Automatic placement skips occupied areas and processes items in `order` and document order. `grid-auto-flow` defaults to `row`, which fills each row and then adds rows as needed. `column` fills each column and then adds columns. Add `dense` to either direction to revisit earlier holes, or use `dense` alone for `row dense`. Consecutive direct text forms an anonymous item. Whitespace-only runs and out-of-flow children take no cells. A numeric `z-index` establishes a stacking context on a Grid item, including one with `position: static`. [CSS Grid auto-placement](https://www.w3.org/TR/css-grid-2/#auto-placement-algo) · [CSS Grid painting order](https://www.w3.org/TR/css-grid-2/#z-order)

`grid-auto-rows` and `grid-auto-columns` accept a space-separated list of the supported track sizes and default to `auto`. For `grid-auto-columns: 60px 100px`, the first column after the sized tracks takes `60px`, and the closest implicit column before the explicit grid takes `100px`. The pattern repeats in each direction. The forward pattern also sizes any tracks added by `grid-template-areas` that have no size from `grid-template-columns` or `grid-template-rows`. [CSS Grid implicit track sizing](https://www.w3.org/TR/css-grid-2/#auto-tracks)

`row-gap`, `column-gap`, and the `gap` shorthand add spacing between tracks, including between tracks in a spanning item's area. `normal` resolves to zero. Percentage tracks use `auto` and the percentage portion of a gap resolves against zero during intrinsic sizing. Final layout resolves percentages against the content box. [CSS percentage gaps](https://www.w3.org/TR/css-gaps-1/#gap-percent)

`justify-items` and `align-items` set the horizontal and vertical alignment within each grid area. Individual items can override these defaults with `justify-self` and `align-self`. Positions include `start`, `end`, and `center`, with `left` and `right` available on the horizontal axis. Add `safe` before a position to use start alignment when the item overflows its area. `stretch` fills the area along the chosen axis when the item has an automatic size and no automatic margins on that axis. Automatic margins absorb positive free space before alignment. [CSS Grid item alignment](https://www.w3.org/TR/css-grid-2/#grid-align)

`justify-content` and `align-content` align or distribute the tracks within the content box. In addition to positional alignment and `stretch`, they support `space-between`, `space-around`, and `space-evenly`. Distribution adds to the declared gap, and spanning items include the added spacing inside their areas. Collapsed `auto-fit` tracks retain their line numbers and contribute no spacing. [CSS Grid content distribution](https://www.w3.org/TR/css-align-3/#distribution-grid)

`align-items`, `align-self`, `justify-items`, and `justify-self` support `baseline` (equivalent to `first baseline`) and `last baseline` in horizontal, left-to-right layout. Row alignment uses the first or last text baseline, with the bottom border edge supplying one for images and empty boxes. Column alignment synthesizes a baseline at the left border edge. Spanning items join the group at their first or last track, and the alignment adjustment contributes to the size needed across the span. Auto margins take precedence. Percentage sizes and limits that depend on content-sized tracks use safe start or end alignment. [CSS Grid baseline alignment](https://www.w3.org/TR/css-grid-2/#grid-align) · [Baseline grouping](https://www.w3.org/TR/css-align-3/#baseline-sharing-group)

Grid items with `align-content: first baseline` or `last baseline` can align their contents with neighboring items in the same row. Participation requires a content baseline and self-alignment that anchors the matching edge. Extra space is applied inside the box. When the content cannot join the group, the corresponding safe edge applies. Dockit implements this for its Grid layout. Browser behavior for these values varies. [CSS baseline content alignment](https://www.w3.org/TR/css-align-3/#baseline-align-content) · [CSS Working Group resolution](https://github.com/w3c/csswg-drafts/issues/13421)

`place-items`, `place-self`, and `place-content` set the corresponding `align-*` and `justify-*` properties in that order. A single value applies to both axes, as in `place-items: center`. With a single baseline value in `place-content`, the second axis defaults to `start`. Multi-keyword values such as `place-self: safe end unsafe center` use the grammar of the corresponding longhand. [CSS Box Alignment shorthands](https://www.w3.org/TR/css-align-3/#place-content)

`grid-template` sets the explicit rows, columns, and named areas. Use `none`, a pair of track lists such as `40px auto / repeat(3, 1fr)`, or quoted area rows such as `"head head" 40px "nav main" / 12rem 1fr`. In the quoted form, omitted row sizes are `auto`, the optional column list uses individual tracks, and line names on either side of a row boundary share that line. The implicit grid settings remain in effect. [CSS Grid template shorthand](https://www.w3.org/TR/css-grid-2/#grid-template-property)

`grid` accepts the same explicit forms and adds automatic placement forms such as `auto-flow dense 24px / repeat(3, 1fr)` and `40px 40px / auto-flow 100px`. Put `auto-flow` on the left of the slash to create rows or on the right to create columns. The optional `dense` keyword can appear immediately before or after it. Any sizes that follow form the repeating implicit track pattern on that axis. Omitted longhands reset to their initial values: `none` for the explicit grid, `auto` for implicit track sizes, and `row` for the flow direction. Both shorthands preserve the existing gaps. [CSS Grid definition shorthand](https://www.w3.org/TR/css-grid-2/#grid-shorthand)

`grid-template-columns: subgrid` and `grid-template-rows: subgrid` share the parent grid's tracks in the corresponding axis. They also work in the `grid-template` and `grid` shorthands. A grid that cannot adopt parent tracks uses `none` on that axis. Items use local line numbers, inherit the names of the corresponding parent lines, and can add names with brackets or a name-only `repeat()`, including `auto-fill`. Parent template areas are clipped to the subgrid's span. Subgridded axes cannot create implicit tracks, so placements outside the available lines are clamped. [CSS Grid subgrids](https://www.w3.org/TR/css-grid-2/#subgrids)

Descendant items contribute to the sizes of the parent tracks. The subgrid stretches in the shared axis and adjusts inherited track geometry for its outer box edges. With `gap: normal`, the shared axis follows the parent's gaps. An explicit gap is centered on the inherited gutters. [CSS Grid track sizing](https://www.w3.org/TR/css-grid-2/#algo-track-sizing)

Items in shared tracks participate in first- and last-baseline groups alongside items in the parent grid, including through nested subgrids. The baseline adjustment contributes to intrinsic track sizing and accounts for the subgrid's outer edges and gap differences. An item whose percentage size would create an intrinsic sizing cycle uses the corresponding fallback alignment. [CSS Grid alignment](https://www.w3.org/TR/css-grid-2/#alignment)

Inline measurements start with estimates from definite row maxima. After sizing the rows, the renderer measures height-dependent inline content again, including aspect-ratio boxes, images, and wrapping column flex containers. If its minimum content contribution changed, the owning grid resolves the columns again and remeasures the rows when the column geometry changes. Each axis gets at most one additional solve per grid layout pass. Subgrid measurements carry the available sizes across shared and standalone axes. [CSS Grid sizing algorithm](https://www.w3.org/TR/css-grid-2/#algo-grid-sizing)


## Overflow

`overflow` sets the horizontal and vertical axes to the same keyword, or takes two keywords in x/y order. Each axis accepts `visible`, `clip`, `hidden`, `auto`, and `scroll`. The legacy value `overlay` aliases `auto`. In horizontal writing, `overflow-inline` and `overflow-block` map to `overflow-x` and `overflow-y` respectively. If either axis is scrollable, the other axis's `visible` computes to `auto` and its `clip` computes to `hidden`. For example, `overflow: clip auto` allows wheel scrolling vertically and clips horizontal scrolling by wheel.

`overflow: clip visible` clips horizontally at the padding edge and leaves vertical overflow visible. A one-axis strip is rectangular. When both axes clip, their shared edge follows the adjusted border radius. Clipping follows transforms and applies to painting, hit testing, and selection geometry. An axis with `clip` has no internal scroll range. Use `display: flow-root` to establish a block formatting context while clipping, for example to contain floats.

`overflow-clip-margin` selects the `content-box`, `padding-box`, or `border-box` as the origin and accepts a non-negative length in either order. The default is `padding-box 0`. A length can use CSS math or relative units. Each axis using `clip` can paint to the expanded edge. The same shape limits pointer hits. Rounded intersections use the cumulative spread from the border edge. `hidden`, `auto`, and `scroll` continue clipping at the padding box. [Overflow clip edge](https://www.w3.org/TR/css-overflow-3/#overflow-clip-margin)

In `HTMLLabel` and Markdown HTML blocks, scroll boxes respond to wheel input, track clicks, and dragging the scrollbar thumb. Inner overlay bars use the padding box and consume no layout space. Textarea field scrollbars and the native viewport reserve their bar thickness. Fluent bars use 15 CSS pixels across the lane with a 3 px idle or 9 px hovered or pressed visible thumb. Thin bars use 9, 2, and 5 CSS pixels respectively. Pointer hover changes the rounded thumb's visible thickness and keeps the track, scroll offset and hit lane fixed. `auto` shows a bar on each overflowing axis and `scroll` keeps that axis’s bar visible. Textareas retain their field scrollbars. `scrollbar-width` accepts `auto`, `thin`, and `none`. It applies to the scrollbar on the element where it is declared. The `none` value hides the bar while wheel and programmatic scrolling remain available. `scrollbar-color` inherits and accepts `auto` or two colors in thumb/track order. Colors can use `currentColor`. Both properties style the self-drawn textareas as well as ordinary scroll boxes.

In `HTMLLabel`, the viewport takes the root element's overflow modes, or the first displayed `body` child's modes when both root axes are `visible`. The source box uses visible overflow. A viewport axis treats `visible` as `auto` and `clip` as `hidden`. A hidden axis allows anchors and `set_scroll_offset()` to scroll while suppressing direct user scrolling. A `scroll` axis shows a scrollbar even without overflow unless the root hides scrollbars with `scrollbar-width: none`. Standalone `HTMLLayout` and Markdown HTML blocks keep their local CSS scroll boxes. [CSS Overflow Viewport Propagation](https://www.w3.org/TR/css-overflow-3/#overflow-propagation)

The root HTML element also styles the native viewport scrollbars of `HTMLLabel`. `scrollbar-gutter: stable` reserves the vertical scrollbar's width, including when that bar is hidden or the content fits. Add `both-edges` to reserve the same width on the opposite side. Both keywords can appear in either order. The document canvas background extends into the gutters. `scrollbar-width: none` removes the reservation. The `body` element's gutter does not propagate to the viewport. Ordinary CSS scroll boxes use overlay scrollbars, which consume no layout space. [CSS Scrollbar Gutters](https://www.w3.org/TR/css-overflow-3/#scrollbar-gutter-property) · [CSS Scrollbars Styling](https://www.w3.org/TR/css-scrollbars-1/)

Media queries use the viewport before subtracting viewport scrollbars or gutters. Viewport-percentage units in declarations also use that size, reduced only by space reserved unconditionally by the root's `overflow: scroll` or `scrollbar-gutter`. Scrollbars made visible by content overflow or by `body` reduce the layout area without reducing viewport-percentage units. [Media Queries](https://www.w3.org/TR/mediaqueries-4/#width) · [CSS Viewport Units](https://www.w3.org/TR/css-values-4/#viewport-relative-lengths)

`position: sticky` keeps its place in normal flow and paints in its own stacking context. Use `top`, `right`, `bottom`, `left`, or `inset` to keep the box within the nearest scrollport and its containing block. Percentages use the scrollport size on the relevant axis, and `auto` leaves its edge unconstrained. Painting, element rectangles, links, and text selection use the offset position. CSS scroll boxes take precedence over the viewport, including boxes with `overflow: hidden`. Viewport sticky boxes in `HTMLLabel` follow Godot's scroll offset. In Markdown, each HTML block has its own viewport, so sticky elements can follow an internal CSS scroll box. Table rows, row groups, and cells can stick. Shared collapsed borders remain on the table's original grid lines. [CSS sticky positioning](https://www.w3.org/TR/css-position-3/#sticky-pos)

## Scroll positioning

`scroll-padding` sets insets on a scrollport. It accepts one to four non-negative lengths, percentages, or `auto`. Percentages use the scrollport dimension for that axis, and `auto` resolves to zero. In `HTMLLabel`, the root `html` style supplies the viewport insets. `scroll-margin` accepts one to four lengths, including negative lengths. It expands or contracts the axis-aligned bounds of the target's transformed border box in each scroll container. Both properties support physical and logical longhands, `-block` and `-inline` shorthands, CSS math, and custom properties. Logical sides use the element's `direction` in horizontal writing. [CSS Scroll Snap insets and outsets](https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-model)

Anchor navigation and `HTMLLabel.scroll_to_element()` scroll nested CSS containers from the inside out, including containers with `overflow: hidden`. The `align` argument positions the expanded target's top edge within the inset region. Keyboard focus scrolls by the minimum distance needed to reveal the target. In Markdown HTML blocks, internal scrollports apply their CSS insets, and the target's margin also adjusts its position in the Markdown host. Scroll insets and outsets keep the underlying layout, element rectangles, and scroll ranges intact. [CSSOM View scrolling](https://drafts.csswg.org/cssom-view/#scroll-a-target-into-view)

`overscroll-behavior` sets the horizontal and vertical policies with one or two keywords. The physical longhands are `-x` and `-y`. The logical `-inline` and `-block` follow the horizontal writing mode. With `auto` or `chain`, remaining scroll input can reach an ancestor. With `contain` or `none`, it stops at the scroll container's boundary, including on an axis with no overflow. Dockit has no elastic overscroll effect. The root `html` style controls chaining from the HTMLLabel viewport to its Godot ancestors. HTML blocks in Markdown apply the policy to their internal CSS scroll containers. [CSS Overscroll Behavior](https://drafts.csswg.org/css-overscroll-1/#overscroll-behavior-properties)

`scroll-behavior` accepts `auto` for immediate navigation and `smooth` for an eased transition. Each scroll container uses its own computed value for fragment links, explicit anchor and element navigation, and keyboard focus reveal. The root `html` style controls the HTMLLabel viewport. Markdown HTML blocks apply smooth behavior to their internal CSS scroll containers. Wheel and pan input, a scrollbar drag, or a new destination can interrupt the corresponding animation. `HTMLLabel.set_scroll_offset()` applies a viewport offset immediately, with scroll snapping when enabled. [CSS Overflow smooth scrolling](https://www.w3.org/TR/css-overflow-3/#smooth-scrolling)

`scroll-snap-type` accepts `none`, or an axis (`x`, `y`, `block`, `inline`, or `both`) followed by optional `mandatory` or `proximity`. The default strictness for an enabled axis is `proximity`. `scroll-snap-align` accepts one or two values of `none`, `start`, `center`, and `end`, in block and inline order. The nearest scroll container captures each target. The snap area uses its transformed border rectangle with `scroll-margin`, and `scroll-padding` insets the snapport. [CSS Scroll Snap model](https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-model)

Directional wheel steps can stop at the first crossed `scroll-snap-stop: always` target. Direct navigation, panning, and released scrollbar thumbs choose by their endpoint. Touchpad panning moves continuously and settles after a short pause in input. Fragment and element navigation prefer the requested target in its nearest scroll container. A proximity candidate must be within 30% of the snapport length, and an oversized area can be scrolled through when space between its surrounding snap points permits. On relayout, the renderer looks for the previously snapped element before choosing a new target. [CSS Scroll Snap mechanics](https://www.w3.org/TR/css-scroll-snap-1/#snap-mechanics)

## Text overflow

Use `overflow: hidden; white-space: nowrap; text-overflow: ellipsis` to elide a title that exceeds its box width. The initial value `clip` keeps ordinary overflow clipping. The ellipsis reserves space at the line’s visual end by eliding whole grapheme clusters and atomic inline boxes. The first item is kept and clipped if necessary. The ellipsis uses the block’s font and baseline, and existing overflow and rounded clipping apply. The original positions and text are available for link hits, selection, copy, and search. Selecting the full elided range highlights its ellipsis. The property operates in the renderer's horizontal writing mode. [CSS Overflow Level 3](https://www.w3.org/TR/css-overflow-3/#text-overflow)

## Text selection

`user-select: none` prevents starting a selection in the element. Selecting across it skips excluded text in both the highlight and the clipboard. `text` restores ordinary selection, including within a descendant of an excluded element. `all` selects the element's text as a unit. The initial `auto` follows the parent's `none` or `all` behavior and otherwise allows ordinary text selection. Buttons, button-like inputs, selects, meters, and progress indicators default to `none` and accept author overrides. This behavior applies in `HTMLLabel` and in Markdown HTML blocks. Search and DOM text queries still include excluded text. [CSS UI selection](https://www.w3.org/TR/css-ui-4/#content-selection)

Editable inputs and textareas keep their own selection inside the field, including under `user-select: none`. Read-only fields follow the authored selection policy. The `contain` declaration value is outside the supported grammar. [CSS UI selection](https://www.w3.org/TR/css-ui-4/#content-selection)

`::selection` accepts `color` and `background-color` for selected text in `HTMLLabel`, Markdown HTML blocks, and text fields. Unspecified colors inherit from the parent element's selection style. `var()` falls back to the originating element's custom properties, and `background-color: currentColor` uses the selected text color. A foreground-only author rule leaves the selection background transparent if no ancestor supplies a color. Selection styling covers glyphs and their background. Highlight decorations, shadows, and replaced content are pending. [CSS highlight styles](https://www.w3.org/TR/css-pseudo-4/#highlight-cascade) · [Paired defaults](https://www.w3.org/TR/css-pseudo-4/#paired-defaults)

## Underline placement

`text-underline-position` accepts `auto`, `from-font`, and `under`. The `left` and `right` keywords can be used alone or with `under`. In horizontal writing, only `under` selects the text's bottom edge. `text-underline-offset` takes `auto`, a signed length, a percentage of the decorating element's font size, or a length expression. For an explicit offset, the origin is the baseline with `auto`, the font metric with `from-font`, and the text's bottom edge with `under`. [CSS Text Decoration](https://www.w3.org/TR/css-text-decor-4/#underline-offset)

Both properties inherit independently of the `text-decoration` shorthand. An existing underline keeps the offset from the element that started it. The renderer positions its segments using the shaped run's baseline and descent. It does not yet resolve separately originating nested underlines or align their baselines across mixed font sizes.

## Text emphasis

Use `text-emphasis: open sesame currentColor` to set the mark and color, and `text-emphasis-position: under` to place the mark below horizontal text. The longhand `text-emphasis-style` accepts `none`, an open or filled dot, circle, double circle, triangle, or sesame, and custom strings. For custom strings, the first grapheme is used. The shorthand resets omitted style and color and keeps the position independent. [CSS Text Decoration](https://www.w3.org/TR/css-text-decor-3/#emphasis-marks)

Marks center on shaped graphemes, using half-size text and the OpenType ruby feature. CSS-defined spaces, punctuation, and controls are skipped. Marks participate in line spacing, clipping, and text shadows. Text selection and copying use the underlying document text. Vertical writing modes, ruby collision handling, and `text-emphasis-skip` are outside this support.

## Font families

```css
body { font-family: "Segoe UI", Arial, sans-serif; }
code { font-family: "Cascadia Mono", Consolas, monospace; }
```

The renderer tries the listed families in order, then uses the platform's glyph fallback through Godot. This applies to ordinary text, field values and placeholders, buttons, and select options. Quoted names are literal, including punctuation and names such as `"monospace"`. Standalone HTML resolves generic families through installed fonts and keeps bundled Open Sans as a fallback. Markdown can explicitly supply text and code fonts for its embedded HTML blocks. `initial` restores the default, `inherit` and `unset` inherit the parent's list, and `revert` rolls back to the default stylesheet and presentational hints.

Use `@font-face { font-family: "UI Copy"; src: url(fonts/ui.woff2); font-weight: 100 900; }` to name a project font. Paths are relative to the stylesheet or can use `res://`. Sources are tried in order. Supported extensions are TTF, OTF, TTC, OTC, WOFF, and WOFF2. The `format()` hint accepts `truetype`, `opentype`, `collection`, `woff`, and `woff2`. A source marked `tech(variations)` requires a variable font.

`ascent-override`, `descent-override`, and `line-gap-override` accept `normal` or nonnegative percentages of the font size, including percentage math functions. Use the first value for horizontal text. A second value is accepted for upright vertical text, which Dockit does not lay out yet. The first available face supplies these metrics. Ascent and descent affect baseline alignment, and half the line gap goes on each side of a line with `line-height: normal`. The metrics leave glyph sizes and advances unchanged. [CSS Fonts metric overrides](https://www.w3.org/TR/css-fonts-5/#font-metrics-override-desc)

`size-adjust` accepts a nonnegative percentage, including percentage math functions. It scales the glyphs and metrics of the selected face, including metric overrides. Fallback faces use their own sizes, even within the same run of text. With `line-height: normal`, each line includes the metrics of its own glyphs. A zero percentage collapses the selected glyphs' advance and ink while preserving the document text. The adjustment affects glyph-relative units such as `ch` and `ex`. `em` and unitless `line-height` still use the computed `font-size`. Godot's text shaping API uses whole-pixel sizes for HTML glyphs. [CSS Fonts size adjustment](https://www.w3.org/TR/css-fonts-5/#size-adjust-desc)

`font-size-adjust` accepts `none`, a nonnegative number, or `from-font`. A number or `from-font` can be preceded by `ex-height` (the default), `cap-height`, `ch-width`, `ic-width`, or `ic-height`. The property normalizes each selected face independently, including any installed fallback selected by Godot. `from-font` takes the first available face's ratio, including its `size-adjust` multiplier, and passes that number to descendants. Faces without the requested metric use their ordinary size. Glyph-relative units use the effective size, while `em` and unitless `line-height` use the computed `font-size`. The `font` shorthand resets the property. Animations interpolate two numbers with the same metric and switch discretely between different metrics or `none`. [CSS Fonts relative sizing](https://www.w3.org/TR/css-fonts-5/#font-size-adjust-prop)

Font matching uses the declared style (`normal`, `italic`, or `oblique`) and weight. A weight range configures the OpenType `wght` axis when the font provides it. Legacy hints such as `format("truetype-variations")` also require a variable font. Rules respect media conditions, layer order, and source order. Imported HTML records font dependencies across media branches, including linked and nested stylesheets. [CSS Fonts](https://www.w3.org/TR/css-fonts-4/#font-face-rule)

`font-style: oblique 18deg` also works in the `font` shorthand. Omit the angle to request 14 degrees. In `@font-face`, use two angles to declare an inclusive range. Positive and negative angles select different slants. Variable faces with a `slnt` axis use the selected angle, within the face and axis limits. When Dockit synthesizes the slant of an upright font, it caps the shear at 89 degrees. [CSS Fonts style matching](https://www.w3.org/TR/css-fonts-4/#font-style-prop)

`font-width` and its legacy alias `font-stretch` share one cascaded value. They accept nonnegative percentages and the width keywords from `ultra-condensed` through `ultra-expanded`. The `font` shorthand accepts the width keywords and resets the width to `normal` when omitted. In `@font-face`, either descriptor accepts a single width, an inclusive range, or `auto`. Width is matched before style and weight. Variable faces use the OpenType `wdth` axis within the declared and font-supported ranges. Installed families use the widths available through Godot's system font lookup. [CSS Fonts width matching](https://www.w3.org/TR/css-fonts-4/#font-width-prop)

`font-feature-settings` and `font-variation-settings` are inherited and also work as `@font-face` descriptors. Both accept `normal` or comma-separated settings with quoted, case-sensitive four-character OpenType tags. Features accept an optional nonnegative integer, `on`, or `off`. Axes require a number, which can be fractional or negative. Math expressions follow the same numeric types. Repeating a tag uses its last value. The `font` shorthand resets both properties to `normal`, so any settings supplied by the chosen face still apply.

Element settings override the selected face's defaults for the same tag. Explicit axes take precedence over the coordinates derived from `font-weight`, `font-width`, and `font-style` after font selection. Coordinates are limited to the font's intrinsic axis bounds. Unsupported tags have no effect on shaping or font selection. [CSS Fonts feature and variation precedence](https://www.w3.org/TR/css-fonts-4/#feature-variation-precedence)

`font-kerning` accepts `auto`, `normal`, and `none`. `auto` uses the normal font and shaper defaults, including any feature settings on the selected face. `normal` enables kerning in horizontal text and applies proportional alternate spacing when the font supplies it. `none` disables kerning. This property is inherited and the `font` shorthand resets it to `auto`. An explicit `font-feature-settings` entry can override the features set by `font-kerning`. [CSS Fonts kerning](https://www.w3.org/TR/css-fonts-4/#font-kerning-prop)

`font-optical-sizing` is inherited and accepts `auto` or `none`. The `font` shorthand resets it to `auto`. For a font with an `opsz` axis, `auto` uses the rendered font size in points at 96 CSS pixels per inch, limited to the font's axis range. It takes precedence over the `@font-face` descriptor's `opsz` value. `none` skips the automatic adjustment, and an explicit element-level `font-variation-settings` entry for `opsz` takes precedence in either mode. Changes to layout font size update the automatic axis. [CSS Fonts optical sizing](https://www.w3.org/TR/css-fonts-4/#font-optical-sizing-def)

The inherited `font-variant-*` properties below select OpenType features during text shaping. They apply after the selected face's feature defaults and before explicit `font-feature-settings` on the element. Within a property, keywords from distinct categories can be combined in any order. Repeated or conflicting choices in one category invalidate the declaration.

| Property | Supported values |
| --- | --- |
| `font-variant-ligatures` | `normal`, `none`, and enable or disable common, discretionary, and historical ligatures and contextual alternates. Required ligatures stay enabled by the shaper. |
| `font-variant-numeric` | `normal`, `lining-nums` or `oldstyle-nums`, `proportional-nums` or `tabular-nums`, `diagonal-fractions` or `stacked-fractions`, `ordinal`, and `slashed-zero`. |
| `font-variant-east-asian` | `normal`, a JIS or simplified/traditional form, `full-width` or `proportional-width`, and `ruby`. |
| `font-variant-caps` | `normal`, `small-caps`, `all-small-caps`, `petite-caps`, `all-petite-caps`, `unicase`, and `titling-caps`. Petite caps fall back to the corresponding OpenType small-cap feature when unavailable. Synthetic small caps and the synthetic unicase fallback are unsupported. |
| `font-variant-alternates` | `normal`, `historical-forms`, `stylistic()`, `styleset()`, `character-variant()`, `swash()`, `ornaments()`, and `annotation()`. |

`font-variant` combines these groups and resets omitted groups. `font-variant: none` disables optional ligatures and resets the other groups. The `font` shorthand also resets these groups and accepts the CSS2 `small-caps` component. [CSS Fonts variants](https://www.w3.org/TR/css-fonts-4/#font-variant-prop)

`@font-feature-values` defines case-sensitive names for one or more named font families. The definitions combine across stylesheets, and later declarations in the winning cascade layer replace the same name for that family and feature type. The selected face's family determines which definitions apply, including when text falls back to another family. Undefined names leave the glyphs unchanged. `styleset()` and `character-variant()` accept comma-separated names. Stylistic-set definitions can enable multiple sets, and character-variant definitions can supply a selector index and an optional alternate value. For OpenType stylistic sets and character variants, index zero and indices above 99 enable no feature. [CSS Fonts feature values](https://www.w3.org/TR/css-fonts-4/#font-feature-values)

`letter-spacing` and `word-spacing` accept `normal`, lengths, percentages, and compatible CSS math expressions. Both properties are inherited. Percentages resolve against the receiving element's font size, while relative lengths such as `em` are computed where declared. Fractional and negative spacing participate in line breaking, painting, text shadows, and text selection.

`letter-spacing` adds space between shaped clusters and shares spacing across adjacent inline fragments. It suppresses optional ligatures unless an explicit `font-feature-settings` entry enables them. Connected-script handling applies within shaped fragments. `word-spacing` adjusts visible word separators such as ordinary and no-break spaces after white-space processing. Word separators with zero advance and fixed-width spaces such as U+3000 keep their intrinsic advance. [CSS Text spacing](https://www.w3.org/TR/css-text-4/#spacing)

`text-autospace` inherits and defaults to `normal`, which enables both `ideograph-alpha` and `ideograph-numeric`. Dockit uses the same modes for `auto`. These modes add one eighth of the containing font's ideographic advance between eligible adjacent characters on the same line. `no-autospace` disables the feature. The `punctuation` flag adds nonbreaking spacing around French punctuation according to the inherited HTML language. `insert` adds spacing at unspaced boundaries. `replace` also adjusts ordinary U+0020 spaces while preserving them in copied text. Shaping uses Godot's bidi handling inside each text run. Spacing between separate inline runs follows their layout order. [CSS automatic text spacing](https://drafts.csswg.org/css-text-4/#text-autospace)

`text-spacing-trim` inherits and controls fullwidth punctuation at line starts, at line ends, and between adjacent characters. It accepts `normal`, `auto`, `space-all`, `trim-all`, `trim-both`, and one keyword from each of the three position groups. Dockit uses normal behavior for `auto`. `allow-end` trims a closing glyph when its full advance would overflow. `space-first` keeps full width at the start of a paragraph and after a forced break. Punctuation follows the horizontal layout order. The renderer uses OpenType `halt` when available. It can also trim blank portions of a uniform fullwidth font if glyph contours allow it. Proportional glyphs without a trimming feature keep their native advances. [CSS punctuation spacing](https://drafts.csswg.org/css-text-4/#text-spacing-trim)

`text-spacing` sets both properties. `none` sets `space-all` and `no-autospace`. `normal` restores the initial values, and `auto` selects automatic behavior for both. When the shorthand specifies only one of the properties, the other resets to its initial value. [CSS text spacing shorthand](https://drafts.csswg.org/css-text-4/#text-spacing)

`text-transform` accepts `none`, `math-auto`, or one case transform with optional `full-width` and `full-size-kana`. The width and kana transforms can also be used on their own or together. The renderer applies case first, then width, then kana size. The width transform converts preserved spaces to ideographic spaces and leaves collapsed spaces at their ordinary width. `math-auto` applies the mathematical italic mapping to source text nodes that contain a single character. Selection and copy use the source characters, while measuring, drawing, and hit testing use the transformed glyphs. Case conversion uses Godot's simple Unicode mappings. Line-break opportunities still follow the original text runs. [CSS Text transforms](https://www.w3.org/TR/css-text-4/#text-transform-property) · [MathML italic mappings](https://www.w3.org/TR/mathml-core/#italic-mappings)

`text-align` sets `text-align-all` and resets `text-align-last` to `auto`. `justify-all` sets both to `justify`, and `match-parent` uses the parent's computed alignment for each longhand. `text-align-last` applies to the final line and before forced line breaks. With `auto`, a justified paragraph aligns those lines at the start. `start` and `end` follow the current horizontal left-to-right layout. Character-based table alignment requires a separate layout feature. [CSS Text alignment](https://www.w3.org/TR/css-text-4/#text-align-property)

`text-justify` accepts `auto`, `none`, `inter-word`, `inter-character` (also `distribute`), and `ruby`, with the optional `no-compress` modifier. Justification expands spaces or shaped cluster boundaries, including across inline elements. `auto` uses word separators and a script-based heuristic for scripts such as Han, Japanese, Korean, and Thai. `ruby` skips word separators and boundaries between Bopomofo characters. The implementation expands spacing, keeps cursive runs connected, and leaves preserved white-space runs at their original spacing. An unexpandable line uses the last-line alignment, falling back to center if that alignment is `justify`. [CSS justification](https://www.w3.org/TR/css-text-4/#text-justify-property)

`text-indent` accepts a length, percentage, or length expression, with optional `each-line` and `hanging` in any order. `each-line` also indents after forced breaks. `hanging` inverts which lines are indented. Percentages use the block's content width for layout and a zero basis for intrinsic measurement. Negative indentation moves the line start outward. [CSS line indentation](https://www.w3.org/TR/css-text-4/#text-indent-property)

`tab-size` accepts a non-negative number, length, or compatible math expression. The default is `8`. A number scales the block container's space advance, including letter and word spacing. Preserved tabs advance to the next stop measured from the block's content edge, with the CSS `0.5ch` minimum-distance rule. A zero interval produces a zero-width tab. In `pre-wrap`, trailing whitespace hangs at soft wraps and can hang if it overflows at a forced break. Selection and copy preserve each tab as one character. With collapsed whitespace, tabs follow the ordinary space rules. [CSS tab stops](https://www.w3.org/TR/css-text-4/#tab-size-property)

`white-space-collapse` supports `collapse`, `preserve`, `preserve-breaks`, `preserve-spaces`, `break-spaces`, and `discard`. `preserve-spaces` replaces tabs and source newlines with spaces. `discard` removes document whitespace during layout. In `break-spaces`, spaces, tabs, and Unicode space separators keep their advances at line ends. They can wrap after each separator, and the first space in a sequence stays with the preceding word. `text-wrap-mode: wrap` or `nowrap` controls soft breaks separately from whitespace. An inline `nowrap` group can move as a unit to a permitted break. Forced breaks still apply. [CSS white space](https://www.w3.org/TR/css-text-4/#white-space-collapse) · [CSS wrapping mode](https://www.w3.org/TR/css-text-4/#text-wrap-mode)

`text-wrap` accepts a mode and a style in either order. Omitted values reset to `wrap` and `auto`. The `text-wrap-style` values `auto` and `stable` use first-fit wrapping. `balance` evens out the space remaining on lines, `pretty` considers line lengths across the group and penalizes very short final lines, and `avoid-short-last-line` considers adjusting the final break. Forced breaks separate groups, and the number of lines in each group stays the same. Balancing applies to groups of up to ten lines and pretty wrapping to groups of up to 24 lines. Groups with preserved tabs, excessive break opportunities, or flow affected by floats use first-fit wrapping. [CSS wrapping styles](https://www.w3.org/TR/css-text-4/#text-wrap-style)

`word-break` accepts `normal`, `keep-all`, `break-all`, `manual`, and the legacy `break-word`. Normal wrapping recognizes Han, Kana, and Hangul boundaries. `keep-all` suppresses letter-to-letter CJK breaks in line layout and intrinsic sizing. `break-all` treats letters and eligible numeric and Southeast Asian character units like ideographs for wrapping. It adds ordinary break opportunities inside words, including in minimum content sizing, while honoring punctuation restrictions and preserving the original shaping and copy offsets. Graphemes that share a native glyph cluster stay together. The legacy `break-word` behaves as `word-break: normal` with `overflow-wrap: anywhere`. [CSS word breaking](https://www.w3.org/TR/css-text-4/#word-break-property)

Thai, Lao, Khmer, and Myanmar text uses the active Godot text server's dictionaries where available. Lookup can span adjacent inline elements. The script supplies a default dictionary locale, and an inherited `lang` for that language supplies the locale variant. Southeast Asian scripts without an available dictionary use grapheme boundaries. The resulting opportunities contribute to minimum content sizing. `word-break: manual` disables this lexical analysis and its fallback while preserving other normal wrapping rules. With wrapping enabled, `<wbr>` and U+200B ZERO WIDTH SPACE remain explicit opportunities. Phrase-based wrapping with `auto-phrase` remains pending. [CSS analytical word breaking](https://www.w3.org/TR/css-text-4/#analytical-word-breaking)

`line-break` accepts `auto`, `loose`, `normal`, `strict`, and `anywhere`. Dockit uses the normal rules for `auto`. Loose wrapping adds opportunities before small kana and iteration marks, between inseparable punctuation marks, and before U+2010 and U+2013 after ideographs. With a declared Chinese or Japanese writing system, normal and loose allow breaks before U+301C and U+30A0. Loose also permits breaks before selected centered punctuation and East Asian suffixes and after East Asian prefixes. An inherited HTML `lang` identifies the writing system, with an explicit script subtag taking precedence. For example, `ja-Latn` follows the Latin rules and `ko-Hani` follows the Chinese rules. [CSS line breaking strictness](https://www.w3.org/TR/css-text-4/#line-break-property)

With `line-break: anywhere`, wrapping can occur between graphemes inside a word and contributes those opportunities to the minimum content width. `nowrap` still suppresses soft breaks, and `break-spaces` permits wrapping before preserved spaces. Dockit draws line fragments from the original shaped text, preserving source offsets for selection and copying. Graphemes that share a native glyph cluster, such as some ligatures, are kept together. The full Unicode line-breaking algorithm is pending. [CSS line breaking strictness](https://www.w3.org/TR/css-text-4/#line-break-property)

`overflow-wrap` accepts `normal`, `anywhere`, and `break-word`. The latter two can break an unbreakable sequence at a grapheme boundary when the line would overflow and has no ordinary break available. Only `anywhere` includes the fallback opportunities in minimum content sizing. Both preserve maximum content width and reuse the same shaped fragments and text offsets as `line-break: anywhere`. The legacy `word-wrap` spelling maps to `overflow-wrap` in the cascade. [CSS overflow wrapping](https://www.w3.org/TR/css-text-4/#overflow-wrap-property)

`hyphens: manual` is the default and allows a break at an authored U+00AD soft hyphen (`&shy;`). `hyphens: none` suppresses that opportunity. The inherited `hyphenate-character` accepts `auto` (U+2010) or a string, including an empty string. A chosen replacement takes up space on its line and contributes to minimum content sizing. The source character stays in the document text and the replacement appears only in the drawing. `white-space: nowrap` suppresses these breaks, and `word-break: break-all` and `line-break: anywhere` use their own opportunities without painting a hyphen. [CSS hyphenation](https://www.w3.org/TR/css-text-4/#hyphenation)

`hyphens: auto` selects embedded patterns using the inherited HTML `lang`. The dictionaries cover US and British English, three German orthographies, Spanish, French, Irish, Galician, Italian, Portuguese, and Ukrainian. Unqualified `en` uses British rules, and unqualified `de` uses the modern orthography. Unknown languages and unsupported scripts use authored opportunities. Automatic points can span adjacent inline elements and are considered during minimum-content sizing. An authored soft hyphen takes priority. The dictionary can supply additional breaks in an authored segment wider than the available line. Lookup recognizes canonical spellings of the bundled alphabets and preserves the original text. Automatic points stay out of the copied text and respect grapheme and native shaped glyph cluster boundaries. [Hyphenation patterns and licenses](../LiteHTML-LICENSES/hyphenation-patterns.txt)

A soft hyphen separates shaping runs. Ligatures and cursive joining across the marker are pending.

`hyphenate-limit-zone` allows a specified amount of space to remain at an unhyphenated line end before splitting the following word. It accepts lengths, percentages of the current line width, and CSS math. A word at the start of a line can still use an eligible break. `hyphenate-limit-lines` accepts `no-limit` or a nonnegative integer. It limits successive hyphenated lines, including breaks with an empty replacement. An unhyphenated line resets the count. Both properties inherit and apply during balanced line wrapping. The zone and positive line limits leave minimum-content opportunities available. A zero line limit disables them. [CSS hyphenation limits](https://www.w3.org/TR/css-text-4/#hyphenate-limit-zone)

`hyphenate-limit-chars` sets the minimum counts for the whole word, before the break, and after the break. It accepts one to three nonnegative integers or `auto` values. The `auto` counts are `5 2 2`. With two components, the second applies on both sides. Godot supplies word and grapheme boundaries across inline markup, with Unicode word-boundary rules retaining punctuation such as apostrophes inside words. Nonspacing marks, punctuation inside a word, and formatting controls are excluded from the counts. A soft hyphen inside a grapheme is ignored for hyphenation. The limits inherit and affect both line wrapping and minimum-content sizing. [CSS hyphenation character limits](https://www.w3.org/TR/css-text-4/#hyphenate-limit-chars) · [Unicode word boundaries](https://www.unicode.org/reports/tr29/#Word_Boundaries)

`hyphenate-limit-last: always` tries to leave the last full line of a block container unhyphenated. It moves material to the next line when possible and retains a hyphen if the alternative adds overflow or keeps moving the same break down. It also applies to balanced wrapping and leaves intrinsic width contributions available. The initial value is `none`. The `column`, `page`, and `spread` values are accepted. Their effects require fragmentation boundaries, which the continuous HTML renderer currently lacks. [CSS last-line hyphenation limits](https://www.w3.org/TR/css-text-4/#hyphenate-limit-last)

`white-space-trim` accepts `none` or a combination of `discard-before`, `discard-after`, and `discard-inner`. The exterior options remove adjacent collapsible whitespace. `discard-inner` removes leading and trailing whitespace in inline boxes. In block containers it trims through the last leading source newline and from the first trailing source newline, preserving indentation and same-line spaces. Trimming uses the in-flow boxes for layout and selection. `HTMLElement.get_text_content()` still returns the source. [CSS white-space trimming](https://www.w3.org/TR/css-text-4/#white-space-trim)

`white-space` supports `normal`, `pre`, `pre-wrap`, and `pre-line`, or any combination of a collapse value, wrapping mode, and trim value. Omitted longhands reset to `collapse`, `wrap`, and `none`. [CSS white-space shorthand](https://www.w3.org/TR/css-text-4/#white-space-property)

`local("Arial Bold")` and `local(Arial-BoldMT)` select an installed face by its full or PostScript name. Windows uses DirectWrite. On other platforms, the lookup depends on the fonts exposed by Godot's OS and text server APIs. Authors can include a project URL as a fallback. A URL fragment, as in `url(fonts/type.ttc#Type-Bold)`, selects the face with that exact PostScript name. Without a fragment, the first face is used. [CSS Fonts source selection](https://www.w3.org/TR/css-fonts-4/#src-desc)

`unicode-range` accepts comma-separated code points, inclusive intervals, or wildcards such as `U+4??`. Faces with the selected style and weight can cover different ranges in the same family. Overlaps use layer and source order. A face must also contain the glyph, and a grapheme cluster uses one face when possible. The first face whose range includes space supplies line-height metrics. [CSS Fonts character ranges](https://www.w3.org/TR/css-fonts-4/#unicode-range-desc)

Family names use Unicode default case folding without normalization. Godot and the OS provide installed font discovery, localized aliases, and face matching. When a generic family has no available candidate, rendering proceeds through the rest of the list and the document default. [CSS Fonts Level 4](https://www.w3.org/TR/css-fonts-4/#font-family-prop)

Text shaping and system font fallback use the nearest HTML `lang` attribute, including script and region subtags such as `zh-Hant-HK`. The same rule applies to button text and form controls. Placeholder fonts follow their CSS and use the host control's language. When the language is absent or explicitly empty, shaping uses the Godot project locale, or the OS locale if no project locale is available. Family names in the CSS list retain their declared priority.

On Windows, standalone HTML with a Simplified or Traditional Chinese language uses Chromium's installed family preferences for the default font and the `serif`, `sans-serif`, `monospace`, and `cursive` generics. The preferences also apply to Mandarin (`cmn`) and Cantonese (`yue`), with explicit `Hans` or `Hant` script subtags taking precedence. A generic resolves to a whole font, including its Latin glyphs. The `font-family` list keeps its declared order. If no family in the list is available, the localized document default is used. For a declared Chinese language, Han characters and East Asian punctuation missing from the selected families use the installed Chinese sans-serif preference before falling through to DirectWrite. Markdown HTML blocks keep their configured text and code fonts for Latin text. [Chromium Windows font preferences](https://chromium.googlesource.com/chromium/src/+/main/chrome/app/resources/locale_settings_win.grd) · [Chromium Windows glyph fallback](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/fonts/win/font_fallback_win.cc)

## Declaration diagnostics

The editor's HTML source preview outlines visible elements with CSS declaration diagnostics. Hover over an outlined element to read the details in an HTML tooltip. For your own `HTMLLabel`, enable **Debug → CSS Diagnostics Enabled** in the Inspector or set `css_diagnostics_enabled` from GDScript. The same setting on a standalone `HTMLLayout` enables outlines in `draw()`.

`HTMLElement.get_css_diagnostics()` returns the declarations associated with that element. `HTMLLabel.get_css_diagnostics()` and `HTMLLayout.get_css_diagnostics()` include the full document's results, including declarations in rules that match no visible element. Each entry includes the property, value, and source. Local stylesheets also include their `res://` path.

| Kind | Example |
| --- | --- |
| `unknown_property` | `writing-mode: vertical-rl` |
| `unsupported_value` | `grid-template-areas: "head head" "head ."` |

Diagnostics cover declarations reached by the stylesheet parser. For a declaration using `var()`, the value is checked when Dockit resolves it on an element. Drawing diagnostics identify the winning declaration after shorthand expansion. Use `get_render_error()` for parse and layout failures.
