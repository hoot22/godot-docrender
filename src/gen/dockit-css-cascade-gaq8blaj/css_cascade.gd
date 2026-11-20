extends SceneTree

const DOCUMENT: String = """
<!doctype html><html><head>
<style id="leading_styles">
  body { margin: 0; }
  .probe { display: block; width: 10px; height: 4px; }
  .from_link { width: 12px; }
  .order { width: 12px; }
  .import_order { width: 12px; }
  .link_media { width: 12px; }
  .after_link { width: 12px; }
  .reimport_order { width: 12px; }
  .stylesheet_move { width: 21px; }
</style>
<link id="main_sheet" rel="  StYlEsHeEt&#9;next " href="styles/sheet.css">
<link rel="styleSheet" href="styles/media.css" media="(min-width: 400px)">
<link rel="not-stylesheet" href="styles/rejected.css">
<link rel="stylesheet alternate" href="styles/rejected.css" title="Alternate">
<link id="typed_sheet" rel="stylesheet" href="styles/typed.css" type="text/plain">
<style>
  .after_link { width: 34px; }
  #specific { width: 32px; }
  .tie { width: 12px; }
  .rule_important { width: 36px !important; }
  .spaced_important { width: 54px ! /* comment */ ImPoRtAnT; }
  .spaced_important { width: 13px; }
  #invalid_deferred { width: 55px; width: var(--size) ! invalid; }
  #unused_fallback { width: 10px; width: var(--size, calc()); }
  #spaced_shorthand { width: 15px; border: var(--border) ! important; }
  #spaced_shorthand { border-left-width: 10px; }
  .baseline_var { text-align: left; }
  #unlayered_rollback, #unlayered_var_rollback { width: 99px !important; }
  #unlayered_custom { --Width: 99px; }
  #pseudo_weight::before { content: "PSEUDO"; }
  .pseudo_weight:before { content: "WRONG"; }
  i:where(.pseudo_weight)::after { content: "TWO"; }
  :where(i.pseudo_weight)::after { content: "ONE"; }
  #important_specific { width: 38px !important; }
  #normal_specific { width: 12px; }
  .repeat { width: 12px; width: 42px; width: bogus; }
  .oversized { width: 25px; padding: 3px; }
  @supports (background-clip: content-box, padding-box) { #background_layers { width: 53px; } }
  #var_specific { --Width: 43px; }
  .var_important { --Width: 44px !important; }
  .inherit_var { --Width: 45px; }
  .shorthand { width: 20px; border: var(--border) !important; }
  .shorthand { border-left-width: 9px; }
  .invalid_var { width: 42px; width: var(--bad); }
  .ignored_var { width: var(--bad); }
  :is(.is_case, #absent) { width: 46px; }
  .not_case:not(#absent, .absent) { width: 47px; }
  .nth_case:nth-child(1 of .nth_case, #absent) { width: 48px; }
  .nth_last_case:nth-last-child(1 of .nth_last_case, #absent) { width: 49px; }
  #pseudo::before { content: "SPECIFIC"; }
  #pseudo::after { content: var(--after); }
  .pseudo::after { content: "IMPORTANT" !important; }
  #pseudo_tie:before { content: "DOCUMENT"; }
  #pseudo_reset::before, #pseudo_reset::after { content: "REMOVED_GENERATED_TEXT"; }
  #pseudo_host::before { content: "HOST"; }
  #pseudo_host > #pseudo_child::before { content: "CHILD"; }
  #pseudo_host::before > #pseudo_child { content: "INVALID_ANCESTOR"; }
  #pseudo_host::before + #pseudo_sibling { width: 99px; }
  #pseudo_host::before::after { content: "INVALID_CHAIN"; }
  ::before { color: #123456; }
  section::before { content: ""; }
  section::after { content: ""; }
  #aligned { text-align: left; }
  #custom_rollback { --base: 86px; }
  .computed_box { padding: 3px; width: 25px; }
  .computed_box { padding: var(--padding); }
  #valid_box { padding-left: 7px; }
  @media (min-width: 400px) { #specific { width: 50px; } }
</style>
<style type="">.style_type { width: 65px; }</style>
<style type="TeXt/CsS">.style_type { width: 66px; }</style>
<style id="inactive_style" type="text/plain">#style_type { width: 67px !important; }</style>
</head><body>
<div id="from_link" class="probe from_link"></div>
<div id="style_type" class="probe style_type"></div>
<div id="order" class="probe order"></div>
<div id="import_order" class="probe import_order"></div>
<div id="invalid_nested" class="probe nested_deferred" style="--nested-width: bogus"></div>
<div id="after_link" class="probe after_link"></div>
<div id="reimport_order" class="probe reimport_order"></div>
<div id="stylesheet_move" class="probe stylesheet_move"></div>
<div id="link_type" class="probe"></div>
<div id="link_media" class="probe link_media"></div>
<div id="specific" class="probe specific"></div>
<div id="tie" class="probe tie"></div>
<div id="inline_normal" class="probe tie" style="width: 35px"></div>
<div id="rule_important" class="probe rule_important" style="width: 12px"></div>
<div id="inline_important" class="probe rule_important" style="width: 37px !important; width: 12px"></div>
<div id="spaced_important" class="probe spaced_important" style="width: 12px"></div>
<div id="spaced_inline" class="probe spaced_important" style="width: 56px ! important; width: 12px"></div>
<div id="invalid_deferred" class="probe" style="--size: 93px"></div>
<div id="unused_fallback" class="probe" style="--size: 57px"></div>
<div id="spaced_shorthand" class="probe" style="--border: 2px solid black; border-right-width: 5px ! important"></div>
<div id="inline_important_vs_normal" class="probe tie" style="width: 39px !important"></div>
<div id="important_specific" class="probe important_specific"></div>
<div id="normal_specific" class="probe normal_specific"></div>
<div id="repeat" class="probe repeat"></div>
<div id="oversized_literal" class="probe oversized" style="padding: 1e50px !important"></div>
<div id="oversized_variable" class="probe oversized"
     style="--oversized: 1e50px; padding: var(--oversized) !important"></div>
<div id="background_layers" class="probe"></div>
<div id="var_specific" class="probe variable" style="width: var(--Width)"></div>
<div id="var_important" class="probe var_important" style="width: var(--Width); --Width: 1px"></div>
<div class="inherit_var"><div id="inherit_var" class="probe" style="--Width: 1px; --Width: revert; width: var(--Width)"></div></div>
<div id="shorthand" class="probe shorthand" style="--border: 2px solid black; border-right-width: 5px !important"></div>
<div id="invalid_var" class="probe invalid_var" style="--bad: bogus"></div>
<div id="ignored_var" class="probe ignored_var" style="width: 41px; --bad: bogus"></div>
<div id="unset_rollback" class="probe" style="width: revert"></div>
<div id="unlayered_rollback" class="probe" style="width: revert-layer !important"></div>
<div id="unlayered_var_rollback" class="probe" style="width: var(--missing, revert-layer) !important"></div>
<div class="inherit_var">
  <div id="unlayered_custom" class="probe" style="--Width: revert-layer; width: var(--Width)"></div>
</div>
<i id="pseudo_weight" class="pseudo_weight">text</i>
<div id="is_case" class="probe is_case extra"></div>
<div id="not_case" class="probe not_case extra"></div>
<section><div id="nth_case" class="probe nth_case extra"></div></section>
<section><div id="nth_last_case" class="probe nth_last_case extra"></div></section>
<div id="pseudo" class="pseudo" style='--after: "AFTER"'>middle</div>
<div id="pseudo_tie">suffix</div>
<div id="pseudo_reset">host text</div>
<div id="pseudo_host"><span id="pseudo_child">node</span></div>
<div id="pseudo_sibling" style="height: 4px"></div>
<div style="--base: 60px">
  <div id="custom_rollback" class="probe"
       style="--base: revert; --result: calc(var(--base) + 2px); width: var(--result)"></div>
  <div id="custom_var_rollback" class="probe"
       style="--base: var(--missing, revert); --result: calc(var(--base) + 3px); width: var(--result)"></div>
  <div id="custom_var_inherit" class="probe"
       style="--base: var(--missing, inherit); width: var(--base, 56px)"></div>
  <div id="custom_var_unset" class="probe"
       style="--base: var(--missing, unset); width: var(--base, 56px)"></div>
  <div id="custom_var_initial" class="probe"
       style="--base: var(--missing, initial); width: var(--base, 56px)"></div>
</div>
<div id="custom_var_no_baseline" class="probe" style="--base: var(--missing, revert-layer); width: var(--base, 64px)"></div>
<div id="valid_box" class="probe computed_box" style="--padding: 1px 2px"></div>
<div id="invalid_box" class="probe computed_box" style="--padding: 1px bogus; padding-left: 7px"></div>
<div id="rollback_box" class="probe computed_box" style="--undo: revert; padding: var(--undo); padding-left: 7px"></div>
<p id="aligned" align="right" style="text-align: var(--missing, revert)"><span id="aligned_text">align</span></p>
<p align="right"><span id="reference_text">align</span></p>
<p class="baseline_var" align="var(--alignment)" style="--alignment: right; text-align: var(--missing, revert-layer)">
  <span id="baseline_var_text">align</span>
</p>
</body></html>
"""

const ADDITIONAL: String = """
@import "styles/additional.css";
.probe.specific { width: 12px; }
.tie { width: 33px; }
.important_specific { width: 12px !important; }
.normal_specific { width: 40px !important; }
.variable { --Width: 1px; }
.nested_deferred { width: 61px; }
.is_case.extra, .not_case.extra, .nth_case.extra, .nth_last_case.extra { width: 12px; }
.pseudo::before { content: "OVERRIDDEN"; }
#pseudo::after { content: var(--after); }
#pseudo_tie::before { content: var(--prefix, "ADDITIONAL"); }
#pseudo_reset::before, #pseudo_reset::after { content: var(--rollback, revert-layer) !important; }
@media (min-width: 400px) and (max-width: 450px) { .link_media { width: 40px; } }
#pseudo_child::before { content: "WRONG_CHILD"; }
#pseudo_sibling { width: 63px; }
.unused { width: bogus; }
"""

const FEATURE_QUERIES: String = """
<!doctype html><style>
@import "styles/conditional.css" supports(display: grid) (min-width: 300px);
@import "styles/rejected.css" supports(display: unsupported);
@import "styles/conditional.css" supports((color: color-mix(red, blue))) (max-width: 299px);
body { margin: 0; }
@supports (display: grid) and ((width: calc(5px + 5px)) or (unknown: yes)) {
  #conditional { width: 41px; }
  @media (min-width: 400px) {
    @supports (color: var(--color) !important) { #conditional { width: 43px; } }
  }
}
@supports not (display: unsupported) { #negated { width: 42px; } }
@supports (display: grid)and/**/(width: 2px) { #compact { width: 50px; } }
@supports (display: grid) and (width: 2px) or (display: grid) { #negated { width: 99px; } }
@supports not future-query() { #future { width: 44px; } }
@supports (width: 10px; height: 20px) { #future { width: 99px; } }
@supports (text-emphasis-style: dot) { #emphasis_support { width: 53px; } }
@supports (border-top-style: groove) { #bevel_support { width: 52px; } }
@supports selector(.parent:has(> .leaf)) { #selector_support { width: 45px; } }
@supports selector(:is(.supported, :unsupported-example)) { #selector_support { width: 99px; } }
@supports selector(.supported, .other) { #selector_support { width: 99px; } }
@supports selector(.related:has(> :is(.leaf, q|div))) { #selector_support { width: 99px; } }
#specificity { width: 12px; }
:where(#specificity) { width: 99px; }
.related:has(> .leaf, #absent) { width: 46px; }
.related.special { width: 99px; }
.sibling:has(+ .next > .leaf) { width: 47px; }
.sibling:has(~ .later .leaf) { height: 48px; }
.related:has(.leaf:has(span)) { width: 99px; }
.related:has(::before) { width: 99px; }
.related:has(> .leaf)::before { content: "RELATIVE"; }
.empty:empty { width: 49px; }
.nonempty:empty { width: 99px; }
</style>
<div id="conditional"></div><div id="negated"></div><div id="future"></div><div id="compact"></div>
<div id="selector_support"></div><div id="emphasis_support"></div><div id="conditional_import"></div><div id="specificity"></div><div id="bevel_support"></div>
<div id="related" class="related special"><span class="leaf"></span></div>
<div id="sibling" class="sibling"></div><!-- Adjacent selectors skip DOM comments -->
<div class="next"><span class="leaf"></span></div><div></div><div class="later"><span class="leaf"></span></div>
<div id="empty" class="empty"> <!-- No content --> </div><div class="nonempty" id="nonempty"><span></span></div>
"""

const LAYERS: String = """
<!doctype html><style>
@layer base, theme;
@import "styles/layer.css" layer(base.widgets) supports(display: grid);
@import "styles/anonymous-layer.css" layer;
@import "styles/layer-media.css" layer(media_first) (min-width: 400px);
@import "styles/rejected.css" layer(rejected) supports(display: unsupported);
@layer after_import;
@import "styles/conditional.css" layer(after_import);
@layer default { #default_layer { width: 72px; } }
body { margin: 0; }
@media (min-width: 400px) { @layer breakpoint; }
@layer stable, breakpoint;
@layer optional, media_first;
@layer theme {
  .normal { width: 51px; }
  #important, #inline_layer_important { width: 91px !important; }
  #rollback { width: var(--undo, revert-layer); }
  #rollback_var { --size: revert-layer; }
  #rollback_var_deferred, #rollback_var_cycle { --size: var(--undo, revert-layer); }
  #rollback_chain { width: var(--undo, revert-layer); }
  #layer_shorthand { margin: var(--undo, revert-layer); margin-right: 7px; }
  #layer_pseudo::before { content: "THEME"; }
  #bad_layer_value { width: var(--missing); }
}
@layer base {
  #normal, #outside, #inline_layer_normal { width: 92px; }
  .important, .inline_layer_important { width: 52px !important; }
  #rollback { width: var(--size, 53px); }
  #rollback_var { --size: 54px; }
  #rollback_var_deferred { --size: calc(var(--source) + 2px); }
  #rollback_var_cycle { --size: var(--next, 98px); --next: var(--size, 99px); }
  #rollback_chain { width: 55px; }
  #important_rollback { width: revert-layer !important; }
  #layer_shorthand { margin: 3px 4px 5px 6px; }
  #layer_pseudo::before { content: "BASE"; }
  #bad_layer_value { width: 93px; }
  .widgets_parent { width: 56px; }
}
@layer theme.deep {
  #rollback_chain { width: revert-layer; }
}
@layer base.widgets { #widgets_parent { width: 95px; } }
@layer { #anonymous { width: 57px; } }
@layer { .anonymous { width: 58px; } }
@layer stable { #media_order { width: 59px; } }
@layer breakpoint { #media_order { width: 60px; } }
@layer optional { #import_media { width: 61px; } }
@layer media_first { #import_media { width: 62px; } }
@layer malformed..name { #normal { width: 99px !important; } }
@layer Case { #case_name { width: 63px; } }
@layer case { .case_name { width: 64px; } }
#outside { width: 65px; }
#conditional_import { width: revert-layer; }
#important { width: 99px !important; }
#rollback_var { width: var(--size); }
#rollback_var_deferred { --source: 70px; width: var(--size); }
#rollback_var_cycle { width: var(--size, 73px); }
#inline_rollback, #inline_rollback_important { width: 66px; }
#inline_rollback_important { width: 67px !important; }
#layer_shorthand { width: 10px; }
</style>
<div id="normal" class="normal"></div>
<div id="important" class="important"></div>
<div id="outside"></div>
<div id="inline_layer_normal" style="width: 68px"></div>
<div id="inline_layer_important" class="inline_layer_important" style="width: 69px !important"></div>
<div id="rollback"></div>
<div id="rollback_var"></div>
<div id="rollback_var_deferred"></div>
<div id="rollback_var_cycle"></div>
<div id="rollback_chain"></div>
<div id="important_rollback"></div>
<div id="layer_shorthand"></div>
<div id="layer_pseudo">content</div>
<div id="bad_layer_value"></div>
<div id="widgets_parent" class="widgets_parent"></div>
<div id="layer_import"></div>
<div id="nested_layer_import"></div>
<div id="conditional_import"></div>
<div id="default_layer"></div>
<div id="anonymous" class="anonymous"></div>
<div id="media_order"></div>
<div id="import_media"></div>
<div id="case_name" class="case_name"></div>
<div id="inline_rollback" style="width: revert-layer"></div>
<div id="inline_rollback_important" style="width: revert-layer !important"></div>
"""

const ADDITIONAL_LAYERS: String = """
@layer base.widgets {
  #layer_import { width: 95px; }
  @layer components { #nested_layer_import { width: 73px; } }
}
@layer older {
  #important_rollback { width: 98px !important; }
}
@layer theme {
  .normal { width: 70px; }
  #layer_pseudo::after { content: "ADDITIONAL"; }
}
"""


var _failures: int


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	_check_layout()
	_check_inline_reflow()
	_check_css_features()
	_check_selector_semantics()
	await _check_css_nesting()
	await _check_css_scope()
	_check_element_subclasses()
	_check_list_element_subclasses()
	_check_detached_subtrees()
	await _check_dom_layout_isolation()
	await _check_dom_child_edits()
	await _check_dom_clone()
	await _check_form_clone()
	await _check_dom_import()
	await _check_dom_adopt()
	await _check_adopted_input_focus()
	await _check_disconnected_input_focus()
	_check_document_fragments()
	await _check_fragment_reflow()
	await _check_imported_template()
	await _check_markdown_markup_resources()
	_check_layers()
	await _check_all_shorthand()
	_check_box_and_transform_functions()
	await _check_motion_paths()
	await _check_individual_transforms()
	await _check_flat_3d_transforms()
	await _check_perspective_transforms()
	await _check_preserved_3d_contexts()
	_check_math_functions()
	_check_length_units()
	await _check_display_contents()
	_check_grid_intrinsic_tracks()
	await _check_subgrid()
	_check_subgrid_baselines()
	_check_grid_cross_axis_sizing()
	_check_intrinsic_box_widths()
	_check_aspect_ratio()
	await _check_object_sizing()
	await _check_image_rendering()
	await _check_image_set()
	await _check_annotated_image()
	await _check_cross_fade()
	await _check_background_blend_modes()
	await _check_element_blend_modes()
	await _check_positioned_stacking_levels()
	await _check_image_resolution()
	await _check_image_orientation()
	await _check_border_image()
	await _check_group_opacity()
	await _check_css_transitions()
	await _check_html_label_animation_process_mode()
	await _check_transform_matrix_transitions()
	await _check_css_animations()
	await _check_layout_property_motion()
	await _check_text_property_motion()
	_check_font_axis_motion()
	_check_animated_font_cache()
	await _check_filters()
	await _check_backdrop_filters()
	await _check_will_change()
	await _check_user_select()
	await _check_selection_colors()
	await _check_masks()
	await _check_svg_masks()
	await _check_clip_paths()
	await _check_polygon_clip_paths()
	await _check_rounded_polygon_clip_paths()
	await _check_svg_clip_paths()
	await _check_inline_svg_clip_sources()
	await _check_external_svg_clip_sources()
	await _check_nested_svg_clip_sources()
	await _check_svg_text_clips()
	await _check_svg_text_path_clips()
	await _check_responsive_clip_shapes()
	await _check_outlines()
	await _check_background_tiling()
	await _check_image_positions()
	await _check_background_attachment()
	await _check_embedded_background_attachment()
	_check_generated_content()
	_check_style_containment()
	_check_size_containment()
	_check_container_size_queries()
	_check_container_units()
	_check_container_style_queries()
	_check_container_query_variables()
	await _check_hidden_content_visibility()
	await _check_auto_content_visibility()
	await _check_generated_image()
	await _check_list_markers()
	_check_counter_styles()
	await _check_font_faces()
	await _check_oblique_angles()
	_check_font_width()
	_check_opentype_settings()
	_check_font_variants()
	_check_named_font_alternates()
	_check_historical_forms()
	_check_east_asian_variants()
	_check_optical_sizing()
	await _check_text_spacing()
	_check_text_autospace()
	_check_punctuation_spacing()
	await _check_text_transform()
	await _check_line_alignment()
	await _check_text_indent_and_tabs()
	await _check_white_space()
	await _check_white_space_trim()
	_check_text_wrap_styles()
	_check_cjk_word_break()
	_check_line_break_strictness()
	await _check_line_break_anywhere()
	_check_overflow_wrap()
	_check_word_break_all()
	_check_lexical_word_breaks()
	await _check_discretionary_hyphenation()
	_check_hyphenation_limits()
	_check_hyphenation_character_limits()
	_check_hyphenation_last_line()
	await _check_automatic_hyphenation()
	await _check_controls()
	await _check_element_activation()
	await _check_forms()
	await _check_extended_html_elements()
	await _check_gauge_and_ruby_paint()
	await _check_form_state_selectors()
	_check_table_elements()
	await _check_table_layout()
	await _check_table_css()
	await _check_form_css()
	await _check_caret_blink()
	_check_image_element()
	await _check_responsive_images()
	await _check_image_alt_text()
	_check_pointer_events()
	await _check_text_overflow()
	await _check_overflow_axes()
	await _check_overflow_clip_margin()
	await _check_overflow_scrollbars()
	await _check_flow_relative_edges()
	_check_logical_floats()
	await _check_layout_and_paint_containment()
	await _check_viewport_overflow()
	await _check_scrollbar_gutter()
	await _check_scrollbar_styles()
	await _check_scroll_insets()
	await _check_overscroll_behavior()
	await _check_smooth_scroll_behavior()
	await _check_scroll_snap()
	await _check_sticky_positioning()
	await _check_author_hover()
	await _check_focus_within()
	await _check_dom_input()
	await _check_text_input_batch()
	await _check_text_undo()
	await _check_input_context_menu()
	await _check_textarea()
	await _check_text_emphasis()
	await _check_underline_position()
	await _check_color_spaces()
	print("CSS cascade checks finished with %d failure(s)" % _failures)
	quit(0 if _failures == 0 else 1)


func _check_layout() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	if layout.parse(DOCUMENT, "res://", ADDITIONAL) != OK:
		_check(false, "Parse cascade fixture: " + layout.get_render_error())
		return
	_check(layout.layout(Vector2i(320, 500)) == OK, "Lay out the narrow viewport")
	var widths: Dictionary[String, float] = {
		"from_link": 30.0,
		"style_type": 66.0,
		"order": 31.0,
		"import_order": 23.0,
		"invalid_nested": 320.0,
		"after_link": 34.0,
		"reimport_order": 24.0,
		"stylesheet_move": 31.0,
		"link_type": 10.0,
		"link_media": 12.0,
		"specific": 32.0,
		"tie": 33.0,
		"inline_normal": 35.0,
		"rule_important": 36.0,
		"inline_important": 37.0,
		"spaced_important": 54.0,
		"spaced_inline": 56.0,
		"invalid_deferred": 55.0,
		"unused_fallback": 57.0,
		"spaced_shorthand": 22.0,
		"inline_important_vs_normal": 39.0,
		"important_specific": 38.0,
		"normal_specific": 40.0,
		"repeat": 42.0,
		"oversized_literal": 31.0,
		"oversized_variable": 25.0,
		"background_layers": 53.0,
		"var_specific": 43.0,
		"var_important": 44.0,
		"inherit_var": 45.0,
		"shorthand": 27.0,
		"invalid_var": 320.0,
		"ignored_var": 41.0,
		"unset_rollback": 320.0,
		"unlayered_rollback": 320.0,
		"unlayered_var_rollback": 320.0,
		"unlayered_custom": 45.0,
		"is_case": 46.0,
		"not_case": 47.0,
		"nth_case": 48.0,
		"nth_last_case": 49.0,
		"custom_rollback": 62.0,
		"custom_var_rollback": 63.0,
		"custom_var_inherit": 60.0,
		"custom_var_unset": 60.0,
		"custom_var_initial": 56.0,
		"custom_var_no_baseline": 64.0,
		"valid_box": 34.0,
		"invalid_box": 32.0,
		"rollback_box": 32.0,
		"pseudo_sibling": 63.0,
	}
	for id: String in widths:
		var actual: float = _rect(layout, id).size.x
		_check(is_equal_approx(actual, widths[id]), "%s width is %s, expected %s" % [id, actual, widths[id]])
	var rendered: String = layout.get_plain_text()
	_check("SPECIFICmiddleIMPORTANT" in rendered and "OVERRIDDEN" not in rendered,
		"Pseudo-elements compare importance before specificity")
	_check("ADDITIONALsuffix" in rendered, "Additional pseudo-element rule wins a source-order tie")
	_check("host text" in rendered and "REMOVED_GENERATED_TEXT" not in rendered,
		"A winning variable rollback clears author content on both generated pseudo-elements")
	_check("PSEUDOtextTWO" in rendered, "Legacy and double-colon pseudo-elements use the same specificity")
	_check("HOSTCHILDnode" in rendered and "INVALID_" not in rendered,
		"Only the selector's supported terminal pseudo-element receives the declaration")
	var alignment: float = _rect(layout, "aligned_text").position.x
	_check(alignment > 100.0 and is_equal_approx(alignment, _rect(layout, "reference_text").position.x),
		"Variable revert restores the HTML align hint")
	_check(is_equal_approx(_rect(layout, "baseline_var_text").position.x, alignment),
		"The restored baseline declaration resolves custom properties")
	_check(_has_diagnostic(layout, "width", "bogus", "style_element"), "Invalid literal reported despite losing")
	_check(_has_diagnostic(layout, "width", "var(--bad)", "style_element"), "Invalid computed value retains its tokens")
	_check(layout.get_element_by_id("ignored_var").get_css_diagnostics().is_empty(),
		"A losing variable declaration is not evaluated")
	_check(_has_diagnostic(layout, "width", "bogus", "local_stylesheet", "res://styles/nested/inner.css"),
		"Nested import diagnostics retain the stylesheet path")
	_check(_has_diagnostic(layout, "width", "var(--nested-width)", "local_stylesheet", "res://styles/nested/inner.css"),
		"A winning variable declaration retains its nested import path after substitution fails")
	_check(_has_diagnostic(layout, "width", "bogus", "stylesheet"), "Additional CSS diagnostics retain their source")
	_check(_has_diagnostic(layout, "padding", "var(--padding)", "style_element"),
		"An invalid computed shorthand reports the original declaration")
	_check(_has_diagnostic(layout, "padding", "var(--oversized)", "inline_style"),
		"An out-of-range length in a winning shorthand reports its original variable expression")
	_check(not _has_diagnostic(layout, "width", "bogus", "local_stylesheet", "res://styles/rejected.css"),
		"A rel token containing stylesheet as a substring does not load CSS")

	var original: HTMLElement = layout.get_element_by_id("specific")
	_check(layout.layout(Vector2i(420, 500)) == OK and is_equal_approx(_rect(layout, "link_media").size.x, 40.0),
		"The additional stylesheet wins a source-order tie while both media conditions match")
	_check(layout.layout(Vector2i(480, 500)) == OK, "Reflow across the media breakpoint")
	_check(is_equal_approx(original.get_bounding_rect().size.x, 50.0), "Matched media rules use the same cascade")
	_check(is_equal_approx(_rect(layout, "link_media").size.x, 29.0), "Linked stylesheet media condition")
	_check(original.set_attribute("id", "changed") == OK, "Update an element ID")
	_check(layout.layout(Vector2i(480, 500)) == OK and original.is_valid(), "Reflow after a DOM mutation")
	_check(is_equal_approx(original.get_bounding_rect().size.x, 12.0), "DOM mutation recomputes the rule winner")
	var link: HTMLElement = layout.get_element_by_id("main_sheet")
	_check(link.set_attribute("rel", "next") == OK, "Remove the stylesheet relationship")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "from_link").size.x, 12.0),
		"Recomputing the cascade removes rules from detached stylesheet relationships")
	_check(link.set_attribute("rel", "next STYLESHEET") == OK, "Restore the stylesheet relationship")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "from_link").size.x, 30.0),
		"Recomputing the cascade restores linked rules in document order")
	_check(link.set_attribute("disabled", "") == OK, "Disable the linked stylesheet")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "from_link").size.x, 12.0),
		"Disabling a linked sheet removes its declarations from the cascade")
	_check(link.remove_attribute("disabled") == OK, "Enable the linked stylesheet")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "from_link").size.x, 30.0),
		"Enabling a linked sheet restores its source order")
	var style: HTMLElement = layout.get_element_by_id("inactive_style")
	_check(style.set_attribute("type", "") == OK, "Enable an embedded stylesheet with an empty CSS type")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "style_type").size.x, 67.0),
		"The newly enabled stylesheet participates in the author cascade")
	_check(style.set_attribute("type", "text/plain") == OK, "Change the embedded stylesheet to a non-CSS type")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "style_type").size.x, 66.0),
		"Changing the stylesheet type removes its previous winning declarations")
	var typed_link: HTMLElement = layout.get_element_by_id("typed_sheet")
	_check(not _has_diagnostic(layout, "width", "bogus", "local_stylesheet", "res://styles/typed.css"),
		"An unsupported link type contributes no CSS diagnostics")
	_check(typed_link.set_attribute("type", "TeXt/CsS; charset=utf-8") == OK, "Enable the linked CSS type")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "link_type").size.x, 68.0),
		"A link that gains a supported MIME type contributes its author declarations")
	_check(_has_diagnostic(layout, "width", "bogus", "local_stylesheet", "res://styles/typed.css"),
		"An enabled link contributes diagnostics with its resource path")
	_check(typed_link.set_attribute("type", "text/plain") == OK, "Restore the unsupported linked stylesheet type")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "link_type").size.x, 10.0),
		"A link that loses its supported MIME type stops contributing author declarations")
	_check(not _has_diagnostic(layout, "width", "bogus", "local_stylesheet", "res://styles/typed.css"),
		"Changing the link type also removes stale diagnostics")
	var leading_style: HTMLElement = layout.get_element_by_id("leading_styles")
	_check(leading_style.get_parent().append_child(leading_style) == OK,
		"Move the leading style element after the linked stylesheet")
	_check(layout.layout(Vector2i(480, 500)) == OK and is_equal_approx(_rect(layout, "stylesheet_move").size.x, 21.0),
		"Moving the style element changes the source-order winner")


func _check_inline_reflow() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """
		<style>
			body { margin: 0; }
			#probe { width: 40px !important; height: 8px; }
		</style>
		<div id="probe" style="--scale: 25vw; width: var(--scale) !important; height: nonsense"></div>
	"""
	if layout.parse(source) != OK or layout.layout(Vector2i(200, 100)) != OK:
		_check(false, "Prepare inline cascade reflow fixture: " + layout.get_render_error())
		return
	var probe: HTMLElement = layout.get_element_by_id("probe")
	_check(probe.get_bounding_rect().size.is_equal_approx(Vector2(50.0, 8.0)),
		"The inline important variable wins while the invalid inline height leaves the author height")
	_check(probe.get_css_diagnostics().size() == 1 and _has_diagnostic(layout, "height", "nonsense", "inline_style"),
		"The invalid inline declaration retains its source")
	_check(layout.layout(Vector2i(240, 100)) == OK, "Resize with the same author rules")
	_check(probe.get_bounding_rect().size.is_equal_approx(Vector2(60.0, 8.0)),
		"A viewport-only reflow updates the winning inline length")
	_check(probe.get_css_diagnostics().size() == 1, "A viewport-only reflow retains one inline diagnostic")
	_check(probe.set_attribute("style", "width: 90px; height: 12px") == OK, "Replace inline declarations")
	_check(layout.layout(Vector2i(240, 100)) == OK, "Reflow after replacing inline declarations")
	_check(probe.get_bounding_rect().size.is_equal_approx(Vector2(40.0, 12.0)),
		"Editing inline style withdraws its previous important winner")
	_check(probe.get_css_diagnostics().is_empty(), "Editing inline style clears stale diagnostics")
	_check(probe.remove_attribute("style") == OK and layout.layout(Vector2i(240, 100)) == OK,
		"Remove the inline declaration block and reflow")
	_check(probe.get_bounding_rect().size.is_equal_approx(Vector2(40.0, 8.0)),
		"Removing inline style restores the stylesheet winners")


func _check_css_features() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(FEATURE_QUERIES) == OK and layout.layout(Vector2i(320, 480)) == OK,
		"Lay out the selector and feature-query fixture")
	var widths: Dictionary[String, float] = {
		"conditional": 41.0,
		"negated": 42.0,
		"future": 44.0,
		"compact": 50.0,
		"selector_support": 45.0,
		"conditional_import": 51.0,
		"specificity": 12.0,
		"related": 46.0,
		"sibling": 47.0,
		"empty": 49.0,
		"bevel_support": 52.0,
		"emphasis_support": 53.0,
		"nonempty": 320.0,
	}
	for id: String in widths:
		_check(is_equal_approx(_rect(layout, id).size.x, widths[id]), "Feature query or selector width for %s" % id)
	_check(is_equal_approx(_rect(layout, "sibling").size.y, 48.0), "General sibling and descendant relative selector")
	_check(layout.query_selector("div:has(> .leaf)") == layout.get_element_by_id("related"), "Query with :has()")
	_check(layout.query_selector("div:has(> :is(.leaf, :unsupported-example))") == layout.get_element_by_id("related"),
		"An inner :is() remains forgiving during normal matching")
	_check(layout.query_selector_all(":where(.next, .later, :unsupported-example)").size() == 2,
		":where() parses forgiving selector lists")
	_check("RELATIVE" in layout.get_plain_text(), "A relational selector styles a pseudo-element")
	_check(layout.get_css_diagnostics().is_empty(), "Feature probes and rejected imports leave diagnostics empty")
	_check(layout.layout(Vector2i(480, 480)) == OK, "Lay out nested media and feature queries")
	_check(is_equal_approx(_rect(layout, "conditional").size.x, 43.0), "Nested @media and @supports")
	_check(layout.layout(Vector2i(280, 480)) == OK, "Lay out the alternate conditional import")
	_check(is_equal_approx(_rect(layout, "conditional_import").size.x, 51.0), "Parenthesized import supports condition")
	var leaf: HTMLElement = layout.query_selector(".related > .leaf")
	_check(leaf.set_attribute("class", "other") == OK and layout.layout(Vector2i(280, 480)) == OK,
		"Mutate a relative selector's matched child")
	_check(not layout.get_element_by_id("related").matches(":has(> .leaf)"), "Relative matching follows DOM changes")
	_check(is_equal_approx(_rect(layout, "related").size.x, 99.0), "Relational styles update on mutation")
	_check("RELATIVE" not in layout.get_plain_text(), "Generated content disappears with its relative match")


func _check_selector_semantics() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><html lang="en-US"><head><style>
	body { margin: 0; }
	#list > div, #languages > div { height: 5px; }
	:scope > body #scope_style { width: 31px; }
	#latin:lang(EN, "*-Hant") { width: 32px; }
	#traditional:lang("*-HANT-TW") { width: 33px; }
	#extension:lang("*-Hant") { width: 99px; }
	#unknown:lang("") { width: 34px; }
	#unknown:lang("*") { width: 99px; }
	#custom:lang(xyzzy) { width: 35px; }
	#list > :nth-child(2n+1 of .pick) { width: 36px; }
	#list > :nth-last-child(2 of .pick, .other) { height: 38px; }
	#list > :nth-child(1 of .pick, :unknown-example) { width: 99px; }
	#list > :nth-child(1 of) { width: 99px; }
	#list > :nth-child(2n 1) { width: 99px; }
	#list > :nth-child(2147483647n+1) { height: 39px; }
	@supports selector(:scope > :lang(en, "*-Hant")) { #feature { width: 40px; } }
	@supports selector(:lang(en fr)) { #feature { width: 99px; } }
	@supports selector(:nth-child(2.5n + 1)) { #feature { width: 99px; } }
	@supports selector(:nth-child(1 of .pick, :unknown-example)) { #feature { width: 99px; } }
	</style></head><body>
	<div id="scope_style"></div>
	<div id="languages">
		<div id="latin"></div>
		<div id="traditional" lang="zh-Hant-TW"></div>
		<div id="extension" lang="zh-x-Hant"></div>
		<div id="unknown" lang=""><span id="inherits_unknown">unknown</span></div>
		<div id="custom" lang="xyzzy"></div>
	</div>
	<div id="list"><div id="first" class="pick"></div><!-- skip -->
		<div id="second" class="other"></div><div id="third" class="pick"></div>
		<div id="fourth" class="pick" style="display: none"></div>
	</div>
	<div id="feature"></div>
	<a id="link" href="https://example.test/">link</a><a id="no_href">text</a>
	<map name="map"><area id="area" href="#scope_style"></map>
	</body></html>"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(220, 380)) == OK, "Lay out selector semantics")
	var widths: Dictionary[String, float] = {
		"scope_style": 31.0,
		"latin": 32.0,
		"traditional": 33.0,
		"extension": 220.0,
		"unknown": 34.0,
		"custom": 35.0,
		"first": 36.0,
		"third": 220.0,
		"feature": 40.0,
	}
	for id: String in widths:
		var width: float = _rect(layout, id).size.x
		_check(is_equal_approx(width, widths[id]), "Selector width for %s is %s, expected %s" % [id, width, widths[id]])
	_check(is_equal_approx(_rect(layout, "first").size.y, 39.0), "Large An+B coefficients do not overflow")
	_check(is_equal_approx(_rect(layout, "third").size.y, 38.0), "nth-last-child counts matching hidden siblings")
	var root_element: HTMLElement = layout.query_selector(":scope")
	var languages: HTMLElement = layout.get_element_by_id("languages")
	var latin: HTMLElement = layout.get_element_by_id("latin")
	var unknown: HTMLElement = layout.get_element_by_id("inherits_unknown")
	_check(root_element.matches("html:root:first-child:last-of-type:nth-child(1 of html)"), "Structural selectors match the root")
	_check(languages.query_selector(":scope > :lang(en-us)") == latin, "Element query uses its own scope")
	_check(languages.query_selector("html:scope #latin") == null, "Element query has exactly one scope root")
	_check(latin.closest(":scope") == latin and layout.query_selector(":scope") == root_element, "Queries restore the previous scope")
	var detached: HTMLElement = layout.create_element("section")
	var detached_child: HTMLElement = layout.create_element("span")
	_check(detached.append_child(detached_child) == OK, "Build a detached subtree")
	_check(detached.matches(":scope:only-child") and detached.query_selector(":scope > span") == detached_child,
		"Detached nodes use the explicit query scope")
	_check(languages.append_child(detached) == OK and detached_child.closest("#languages :scope") == detached_child,
		"Moving an element preserves the handle and lets closest inspect ancestors outside the scope")
	_check(unknown.matches(':lang("")') and not unknown.matches(':lang("*")'), "An empty language stops inheritance")
	_check(root_element.matches(r":lang(\65 n)"), "Escapes in language ranges are decoded by the tokenizer")
	_check(layout.query_selector(r"#list > :nth-child(2\6e +1)") == layout.get_element_by_id("first"), "An+B accepts escaped units")
	_check(layout.query_selector("#list > :nth-child(+n - 1 of .pick)") == layout.get_element_by_id("first"), "An+B accepts adjacent plus")
	_check(layout.query_selector_all(":any-link").size() == 2, "Hyperlink pseudo-class covers anchors and areas")
	_check(layout.get_element_by_id("area").matches(":link") and not layout.get_element_by_id("no_href").matches(":link"),
		"Link pseudo-class requires href")
	var link: HTMLElement = layout.get_element_by_id("link")
	_check(link.remove_attribute("href") == OK and not link.matches(":any-link"), "Removing href updates queries immediately")
	_check(languages.set_attribute("lang", "ja-JP") == OK and latin.matches(":lang(ja)"), "Language mutation is inherited immediately")
	_check(layout.layout(Vector2i(220, 380)) == OK and is_equal_approx(_rect(layout, "latin").size.x, 220.0),
		"Language mutation also invalidates the stylesheet match")
	var quirks: HTMLLayout = HTMLLayout.new()
	_check(quirks.parse('<div id="Mixed" class="Case" data-empty=""></div>') == OK, "Parse selector attributes in quirks mode")
	var mixed: HTMLElement = quirks.query_selector("#mixed.case")
	_check(mixed != null and mixed.matches('[id="Mixed"][class="Case"][data-empty=""]'),
		"Quirks selectors fold ID and class while preserving raw attribute values")
	_check(mixed != null and not mixed.matches('[id="mixed"], [class="case"], [data-missing=""]'),
		"Attribute matching distinguishes original case, missing attributes and empty values")


func _check_element_subclasses() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><div id="container"><p id="paragraph"><span id="inline">Text</span></p></div>
		<div id="destination"></div><aside id="other"></aside>"""
	_check(layout.parse(html) == OK, "Parse the element subclass fixture")
	var container: HTMLElement = layout.get_element_by_id("container")
	var paragraph: HTMLElement = layout.query_selector("#paragraph")
	var span: HTMLElement = layout.query_selector("#inline")
	_check(container is HTMLDivElement and paragraph is HTMLParagraphElement and span is HTMLSpanElement,
		"Element queries return typed div, paragraph and span handles")
	if container == null or paragraph == null or span == null:
		return
	var children: Array[HTMLElement] = container.get_children()
	_check(children == [paragraph] and span.get_parent() == paragraph and paragraph.get_parent() == container,
		"Parent and child traversal reuse the typed handles through the base type")
	var other: HTMLElement = layout.get_element_by_id("other")
	_check(other != null and other.get_class() == "HTMLElement", "Unmapped tags keep the generic element type")
	_check(layout.create_element("div") is HTMLDivElement and layout.create_element("p") is HTMLParagraphElement and
		layout.create_element("span") is HTMLSpanElement, "Detached elements use the same type mapping")

	var heading_markup: String = ""
	for level: int in range(1, 7):
		heading_markup += "<h%d>Heading %d</h%d>" % [level, level, level]
	_check(container.set_inner_html(heading_markup) == OK, "Parse all six heading levels as a fragment")
	var headings: Array[HTMLElement] = container.query_selector_all("h1, h2, h3, h4, h5, h6")
	_check(headings.size() == 6, "Heading selector returns all six heading elements")
	for level: int in range(1, 7):
		var created: HTMLHeadingElement = layout.create_element("H%d" % level) as HTMLHeadingElement
		_check(created != null and created.get_level() == level, "Detached heading has level %d" % level)
		var parsed: HTMLHeadingElement = container.query_selector("h%d" % level) as HTMLHeadingElement
		_check(parsed != null and parsed.get_level() == level, "Parsed heading has level %d" % level)

	var heading: HTMLHeadingElement = container.query_selector("h3") as HTMLHeadingElement
	var destination: HTMLElement = layout.get_element_by_id("destination")
	if heading == null or destination == null:
		return
	_check(destination.append_child(heading) == OK and destination.query_selector("h3") == heading,
		"Moving a heading to another parent preserves its handle")
	_check(layout.layout(Vector2i(320, 600)) == OK and layout.layout(Vector2i(240, 600)) == OK,
		"Lay out the same document at two viewport widths")
	var bounds: Rect2 = heading.get_bounding_rect()
	_check(bounds.has_area() and layout.get_element_at(bounds.get_center()) == heading and heading.get_level() == 3,
		"Hit testing after reflow returns the same typed heading handle")
	_check(layout.parse("<h3>Replacement</h3>") == OK and not heading.is_valid() and heading.get_level() == 0 and
		layout.query_selector("h3") != heading, "Reparsing invalidates the old heading handle")


func _check_list_element_subclasses() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><pre id="code">  X
  Y</pre><aside id="sidebar"></aside>
<ol id="ordered" reversed="false"><li>Alpha</li><li>Bravo</li><li>Charlie</li></ol>
<ul id="unordered"><li>Delta</li></ul>"""
	_check(layout.parse(html) == OK, "Parse preformatted text and list elements")
	var pre: HTMLPreElement = layout.get_element_by_id("code") as HTMLPreElement
	var aside: HTMLElement = layout.get_element_by_id("sidebar")
	var ordered: HTMLOListElement = layout.get_element_by_id("ordered") as HTMLOListElement
	var unordered: HTMLUListElement = layout.get_element_by_id("unordered") as HTMLUListElement
	var middle: HTMLLIElement = layout.query_selector("#ordered > li:nth-child(2)") as HTMLLIElement
	var last: HTMLLIElement = layout.query_selector("#ordered > li:last-child") as HTMLLIElement
	_check(pre != null and pre.get_text_content() == "  X\n  Y", "Preformatted elements preserve spaces and line breaks")
	_check(aside != null and aside.get_class() == "HTMLElement", "Aside uses the shared element type")
	_check(unordered != null and unordered.get_children()[0] is HTMLLIElement,
		"Unordered lists return typed items through child traversal")
	_check(ordered != null and middle != null and last != null, "Ordered lists and their items have typed handles")
	if ordered == null or middle == null or last == null:
		return

	var viewport_size: Vector2i = Vector2i(320, 600)
	_check(ordered.start == 1 and ordered.reversed and ordered.type == "" and middle.value == 0 and last.type == "",
		"List properties reflect defaults and boolean attribute presence")
	_check(layout.layout(viewport_size) == OK and "3. Alpha" in layout.get_plain_text(),
		"Reversed lists without a start attribute render from the item count")
	ordered.start = 6
	ordered.type = "A"
	middle.value = 3
	last.type = "i"
	_check(ordered.get_attribute("start") == "6" and ordered.get_attribute("type") == "A"
		and middle.get_attribute("value") == "3" and last.get_attribute("type") == "i",
		"List property setters update their HTML attributes")
	_check(layout.layout(viewport_size) == OK, "Reflow after changing list properties")
	var text: String = layout.get_plain_text()
	_check("F. Alpha" in text and "C. Bravo" in text and "ii. Charlie" in text,
		"List properties update start, value and marker styles")
	ordered.reversed = false
	_check(not ordered.has_attribute("reversed") and layout.layout(viewport_size) == OK
		and "iv. Charlie" in layout.get_plain_text(), "Clearing reversed restores increasing numbering")
	_check(middle.get_parent() == ordered and ordered.get_children()[1] == middle,
		"Reflow keeps the original list and item handles")

	var detached: HTMLOListElement = layout.create_element("OL") as HTMLOListElement
	var detached_item: HTMLLIElement = layout.create_element("li") as HTMLLIElement
	_check(layout.create_element("pre") is HTMLPreElement and layout.create_element("ul") is HTMLUListElement
		and detached != null and detached_item != null, "Detached elements use the same type mapping")
	if detached != null and detached_item != null:
		detached.start = -4
		detached.reversed = true
		detached_item.value = -8
		_check(detached.start == -4 and detached.has_attribute("reversed") and detached_item.value == -8,
			"Detached list attributes reflect signed numbers and boolean presence")
		_check(detached.set_attribute("start", " \t+3suffix") == OK and detached.start == 3
			and detached_item.set_attribute("value", "+-2") == OK and detached_item.value == 0
			and detached.set_attribute("start", "2147483648") == OK and detached.start == 1,
			"Integer reflection handles HTML number parsing and out-of-range values")
	_check(ordered.clone_node(true) is HTMLOListElement, "List clones use the same typed handle factory")


func _check_detached_subtrees() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""<!doctype html><body><div id="panel" data-mode="original"><p id="row">
		<span id="leaf">Before</span></p></div><div id="target"></div></body>""") == OK,
		"Parse the detached subtree fixture")
	var document_root: HTMLElement = layout.query_selector("html")
	var panel: HTMLElement = layout.get_element_by_id("panel")
	var row: HTMLElement = layout.get_element_by_id("row")
	var leaf: HTMLElement = layout.get_element_by_id("leaf")
	var target: HTMLElement = layout.get_element_by_id("target")
	if document_root == null or panel == null or row == null or leaf == null or target == null:
		_check(false, "Query the detached subtree fixture")
		return
	_check(document_root.is_connected() and leaf.is_connected() and document_root.detach() == ERR_INVALID_PARAMETER,
		"Connection reflects the parsed tree before layout, and the document root cannot be detached")
	var fresh: HTMLElement = layout.create_element("span")
	_check(fresh != null and fresh.is_valid() and not fresh.is_connected() and fresh.detach() == OK,
		"A freshly created element has a valid detached handle")
	_check(layout.layout(Vector2i(300, 150)) == OK and leaf.get_bounding_rect().has_area(), "Lay out the connected subtree")
	_check(panel.detach() == OK and panel.detach() == OK and panel.get_parent() == null and
		not panel.is_connected() and not row.is_connected() and not leaf.is_connected() and
		layout.get_element_by_id("leaf") == null, "Detach updates tree membership immediately and can be repeated")
	_check(panel.query_selector("#leaf") == leaf and panel.query_selector("p") == row and leaf.get_parent() == row,
		"Detached subtree queries reuse the existing descendant handles")
	_check(panel.set_attribute("data-mode", "edited") == OK and leaf.set_text_content("After") == OK and
		layout.layout(Vector2i(300, 150)) == OK and not leaf.get_bounding_rect().has_area(),
		"Edit detached content while its geometry stays out of the layout")
	_check(row.detach() == OK and panel.query_selector("#leaf") == null and row.query_selector("#leaf") == leaf and
		panel.append_child(row) == OK and panel.query_selector("p") == row and not leaf.is_connected(),
		"Splitting and merging detached trees preserves descendant identity")
	_check(target.append_child(panel) == OK and layout.get_element_by_id("panel") == panel and
		layout.get_element_by_id("row") == row and layout.get_element_by_id("leaf") == leaf,
		"The original root and all live descendant handles survive reinsertion")
	_check(panel.detach() == OK, "Detach the same tree again")

	panel = null
	row = null
	var recovered_row: HTMLElement = leaf.get_parent()
	var recovered_panel: HTMLElement = recovered_row.get_parent() if recovered_row != null else null
	_check(recovered_panel != null and recovered_panel.get_attribute("data-mode") == "edited" and
		leaf.get_text_content() == "After", "A descendant handle alone retains the detached root and its edits")
	if recovered_panel == null:
		return
	_check(target.append_child(recovered_panel) == OK and recovered_panel.is_connected() and
		recovered_row.is_connected() and leaf.is_connected() and layout.get_element_by_id("panel") == recovered_panel and
		layout.get_element_by_id("row") == recovered_row and layout.get_element_by_id("leaf") == leaf and
		layout.layout(Vector2i(300, 150)) == OK and leaf.get_bounding_rect().has_area(),
		"Reinsertion keeps the live handles and restores geometry")
	_check(recovered_panel.remove() == OK and not recovered_panel.is_valid() and not recovered_row.is_valid() and
		not leaf.is_valid() and not leaf.is_connected() and leaf.detach() == ERR_INVALID_PARAMETER,
		"Legacy remove still invalidates the subtree and invalid handles reject detach")
	layout.clear()
	_check(not fresh.is_valid() and not fresh.is_connected(), "Clearing the layout invalidates detached handles too")


func _check_dom_layout_isolation() -> void:
	var document: HTMLDocument = HTMLDocument.new()
	document.text = """<!doctype html><body><div style="height:1px"></div>
		<div id="shared" style="width:40px;height:10px"><span id="leaf">Original</span></div></body>"""
	var first: HTMLLayout = document.create_layout()
	var second: HTMLLayout = document.create_layout()
	var viewport_size: Vector2i = Vector2i(250, 100)
	_check(first.layout(viewport_size) == OK and second.layout(viewport_size) == OK, "Lay out two views of one HTMLDocument")
	var first_body: HTMLElement = first.query_selector("body")
	var first_node: HTMLElement = first.get_element_by_id("shared")
	var first_leaf: HTMLElement = first.get_element_by_id("leaf")
	var second_body: HTMLElement = second.query_selector("body")
	var second_node: HTMLElement = second.get_element_by_id("shared")
	if first_body == null or first_node == null or first_leaf == null or second_body == null or second_node == null:
		_check(false, "Query both views of the shared document")
		return
	_check(first_node != second_node and first_node.detach() == OK and first.layout(viewport_size) == OK,
		"Detach an element in one view and settle its layout")
	_check(first_node.set_attribute("style", "width:60px;height:10px") == OK and
		first_leaf.set_text_content("Edited") == OK and first_body.get_bounding_rect().has_area(),
		"Detached-only edits keep the connected layout current")
	_check(second_body.append_child(first_node) == ERR_INVALID_PARAMETER and
		second_node.get_text_content() == "Original" and is_equal_approx(second_node.get_bounding_rect().size.x, 40.0),
		"Cross-layout moves are rejected and the other view retains its content and geometry")
	_check(first_body.append_child(first_node) == OK and first.get_element_by_id("shared") == first_node and
		first.get_element_by_id("leaf") == first_leaf, "Reinsertion reuses the original handles")
	await process_frame
	await process_frame
	_check(is_equal_approx(first_node.get_bounding_rect().size.x, 60.0) and first_leaf.get_text_content() == "Edited" and
		"Original" in document.text, "The DOM edit schedules reflow for its layout and preserves the resource text")
	_check(first_body.append_child(first_node) == OK and first_node.get_bounding_rect().has_area(),
		"Appending the already last child keeps the current geometry")

	var detached: HTMLElement = first.create_element("span")
	_check(first_node.set_attribute("style", "width:65px;height:10px") == OK, "Queue reflow for the old document")
	_check(first.parse('<!doctype html><div id="replacement" style="width:70px;height:10px"></div>') == OK and
		first.layout(viewport_size) == OK, "Replace the document before its queued reflow runs")
	var replacement: HTMLElement = first.get_element_by_id("replacement")
	_check(not first_node.is_valid() and not first_leaf.is_valid() and detached != null and not detached.is_valid(),
		"Reparsing invalidates the old connected and detached handles")
	if replacement == null:
		_check(false, "Query the replacement document")
		return
	var observed_unlaid: Array[bool] = []
	var observe_layout: Callable = func() -> void:
		observed_unlaid.append(not replacement.get_bounding_rect().has_area())
	observe_layout.call_deferred()
	_check(replacement.set_attribute("style", "width:90px;height:10px") == OK, "Queue reflow for the replacement document")
	await process_frame
	await process_frame
	_check(observed_unlaid.size() == 1 and observed_unlaid[0],
		"The old deferred callback leaves the new document pending until its own callback")
	_check(is_equal_approx(replacement.get_bounding_rect().size.x, 90.0) and
		is_equal_approx(second_node.get_bounding_rect().size.x, 40.0),
		"The replacement reflows independently and the shared view stays current")


func _check_dom_child_edits() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><style>
		#parent > div { height: 20px; }
		#parent > :nth-child(1) { width: 31px; }
		#parent > :nth-child(2) { width: 52px; }
		#parent > :nth-child(3) { width: 73px; }
		#parent::after { content: '!'; }
		</style><div id="parent"><div id="a"><div id="leaf">Leaf</div><span id="keep">Keep</span></div><!--mark--><div id="b">B</div><div id="c">C</div>Tail</div>"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(250, 150)) == OK, "Lay out the DOM child edit fixture")
	var parent: HTMLElement = layout.get_element_by_id("parent")
	var a: HTMLElement = layout.get_element_by_id("a")
	var b: HTMLElement = layout.get_element_by_id("b")
	var c: HTMLElement = layout.get_element_by_id("c")
	var leaf: HTMLElement = layout.get_element_by_id("leaf")
	var keep: HTMLElement = layout.get_element_by_id("keep")
	var document_root: HTMLElement = layout.query_selector("html")
	if parent == null or a == null or b == null or c == null or leaf == null or keep == null or document_root == null:
		_check(false, "Query all child edit fixture elements")
		return
	_check(parent.insert_before(b, a) == OK and parent.get_children() == [b, a, c] and
		"<!--mark-->" in parent.get_inner_html(), "Moving a sibling before another preserves text and comments")
	await process_frame
	await process_frame
	_check(is_equal_approx(b.get_bounding_rect().size.x, 31.0) and is_equal_approx(a.get_bounding_rect().size.x, 52.0),
		"Deferred layout resolves structural styles in the new sibling order")
	_check(parent.insert_before(b, b) == OK and parent.insert_before(b, a) == OK and
		parent.replace_child(a, a) == OK and a.get_bounding_rect().has_area(),
		"Self edits and insertion at an existing position keep current geometry")
	_check(parent.insert_before(c, leaf) == ERR_INVALID_PARAMETER and
		parent.replace_child(c, leaf) == ERR_INVALID_PARAMETER and parent.remove_child(leaf) == ERR_INVALID_PARAMETER and
		a.insert_before(parent, leaf) == ERR_INVALID_PARAMETER and parent.get_children() == [b, a, c] and
		leaf.get_parent() == a and a.get_bounding_rect().has_area(),
		"Invalid references and ancestor cycles leave both trees and layout untouched")

	_check(parent.replace_child(leaf, a) == OK and parent.get_children() == [b, leaf, c] and
		leaf.is_connected() and layout.get_element_by_id("leaf") == leaf and
		a.is_valid() and not a.is_connected() and a.get_parent() == null and keep.get_parent() == a and a.get_children() == [keep],
		"Replacing a child with its descendant detaches only the remaining subtree")
	a = null
	var detached: HTMLElement = keep.get_parent()
	if detached == null:
		_check(false, "A descendant handle retains the replaced subtree")
		return
	_check(parent.remove_child(c) == OK and c.is_valid() and not c.is_connected() and
		layout.get_element_by_id("c") == null and parent.get_children() == [b, leaf],
		"Removing a direct child preserves its handle and removes it from document queries")
	await process_frame
	await process_frame
	_check(is_equal_approx(leaf.get_bounding_rect().size.x, 52.0), "The replacement takes the removed child's layout position")
	_check(detached.insert_before(document_root) == ERR_INVALID_PARAMETER and
		detached.replace_child(document_root, keep) == ERR_INVALID_PARAMETER and document_root.is_connected(),
		"A detached destination cannot move the document root")
	_check(detached.insert_before(c, keep) == OK and detached.get_children() == [c, keep] and
		detached.replace_child(keep, c) == OK and detached.get_children() == [keep] and
		detached.remove_child(keep) == OK and detached.get_children().is_empty() and
		c.is_valid() and keep.is_valid() and not keep.is_connected() and leaf.get_bounding_rect().has_area(),
		"Edits between detached trees keep their handles and leave document geometry current")

	_check(parent.insert_before(c) == OK and parent.append_child(c) == OK and
		parent.get_children() == [b, leaf, c] and parent.get_text_content().ends_with("TailC") and c.is_connected() and
		layout.get_element_by_id("c") == c, "Omitting the reference appends the same handle after real text and before generated content")
	await process_frame
	await process_frame
	_check(is_equal_approx(c.get_bounding_rect().size.x, 73.0) and layout.get_plain_text().strip_edges().ends_with("!"),
		"Reinsertion reflows and preserves the trailing pseudo-element")
	_check(c.remove() == OK and not c.is_valid(), "Legacy remove still invalidates the handle")


func _check_dom_clone() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><style>#source::before { content: 'Generated'; }</style>
		<div id="source" data-empty="" style="width:80px"><span>One</span>  <!--mark--><b>Two</b> tail</div>"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(300, 100)) == OK, "Lay out the DOM clone fixture")
	var source: HTMLDivElement = layout.get_element_by_id("source") as HTMLDivElement
	var body: HTMLElement = layout.query_selector("body")
	if source == null or body == null:
		_check(false, "Query the DOM clone fixture")
		return
	var shallow: HTMLDivElement = source.clone_node() as HTMLDivElement
	var deep: HTMLDivElement = source.clone_node(true) as HTMLDivElement
	if shallow == null or deep == null:
		_check(false, "Cloning returns the correct element type")
		return
	_check(shallow != source and shallow.get_attributes() == source.get_attributes() and shallow.get_inner_html().is_empty() and
		shallow.get_parent() == null and not shallow.is_connected(), "A shallow clone has its own handle and copies only source attributes")
	_check(deep != source and deep != shallow and deep.get_inner_html() == source.get_inner_html() and
		"<!--mark-->" in deep.get_inner_html() and "Generated" not in deep.get_text_content(),
		"A deep clone preserves text, whitespace, comments and child order")
	var child: HTMLElement = deep.query_selector("span")
	if child == null:
		_check(false, "Query the clone descendant")
		return
	_check(child != source.query_selector("span") and child.set_text_content("Edited") == OK and
		source.get_text_content() == "One  Two tail" and source.get_bounding_rect().has_area(),
		"Editing a detached clone leaves the source text and layout current")
	deep = null
	var retained: HTMLElement = child.get_parent()
	if retained == null:
		_check(false, "A descendant handle keeps the detached clone alive")
		return
	_check(retained.set_attribute("id", "copy") == OK and body.append_child(retained) == OK and
		layout.get_element_by_id("copy") == retained and retained.query_selector("span") == child,
		"Reinserting a clone preserves its root and descendant handles")
	await process_frame
	await process_frame
	_check(is_equal_approx(retained.get_bounding_rect().size.x, 80.0), "The clone acquires geometry after reflow")
	layout.clear()
	_check(retained.clone_node() == null and not child.is_valid() and not shallow.is_valid(),
		"Clearing a layout invalidates connected and detached clones")


func _check_form_clone() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><body><textarea id="clean">Initial
Value</textarea><div id="fields"><input id="text" value="Default">
		<input id="check" type="checkbox" checked><textarea id="dirty">Default area</textarea></div></body>"""
	_check(layout.parse(html) == OK, "Parse the form clone fixture")
	var clean: HTMLTextAreaElement = layout.get_element_by_id("clean") as HTMLTextAreaElement
	var fields: HTMLElement = layout.get_element_by_id("fields")
	var text: HTMLInputElement = layout.get_element_by_id("text") as HTMLInputElement
	var checkbox: HTMLInputElement = layout.get_element_by_id("check") as HTMLInputElement
	var dirty: HTMLTextAreaElement = layout.get_element_by_id("dirty") as HTMLTextAreaElement
	var body: HTMLElement = layout.query_selector("body")
	if clean == null or fields == null or text == null or checkbox == null or dirty == null or body == null:
		_check(false, "Query the form clone fixture")
		return
	_check(clean.value == "Initial\nValue", "A parsed textarea exposes its current value before layout")
	clean.set_selection_range(1, 6)
	var shallow: HTMLTextAreaElement = clean.clone_node() as HTMLTextAreaElement
	if shallow == null:
		_check(false, "Clone a clean textarea without its child text")
		return
	_check(shallow.default_value.is_empty() and shallow.value == clean.value and
		shallow.get_selection_start() == 0 and shallow.get_selection_end() == 0 and clean.get_selection_end() == 6,
		"A shallow textarea clone has no default text and starts with a fresh caret and the source live value")
	_check(layout.layout(Vector2i(320, 240)) == OK and body.append_child(shallow) == OK, "Insert the shallow textarea clone")
	await process_frame
	await process_frame
	_check(shallow.value == "Initial\nValue" and shallow.default_value.is_empty() and shallow.get_bounding_rect().has_area(),
		"The unedited shallow textarea keeps its current value after deferred reflow")
	_check(shallow.set_attribute("rows", "3") == OK and layout.layout(Vector2i(290, 240)) == OK and
		shallow.value == "Initial\nValue", "Restyling the clone preserves its current value")
	shallow.default_value = "New default"
	_check(shallow.value == "New default" and clean.value == "Initial\nValue", "Changing the clone's child text follows its clean value flag")

	var clean_text: HTMLInputElement = text.clone_node() as HTMLInputElement
	_check(clean_text != null and clean_text.set_attribute("value", "Other default") == OK and
		clean_text.value == "Other default" and text.value == "Default", "A clean input clone still follows its value attribute")
	text.value = "Current text"
	text.set_selection_range(2, 6)
	checkbox.checked = false
	dirty.value = "Current area"
	dirty.set_selection_range(2, 7)
	var deep: HTMLElement = fields.clone_node(true)
	if deep == null:
		_check(false, "Clone the form subtree")
		return
	var cloned_text: HTMLInputElement = deep.query_selector("#text") as HTMLInputElement
	var cloned_check: HTMLInputElement = deep.query_selector("#check") as HTMLInputElement
	var cloned_area: HTMLTextAreaElement = deep.query_selector("#dirty") as HTMLTextAreaElement
	if cloned_text == null or cloned_check == null or cloned_area == null:
		_check(false, "Deep clones preserve specialized form element types")
		return
	_check(cloned_text.set_attribute("value", "Changed default") == OK and cloned_text.value == "Current text" and
		cloned_text.get_selection_start() == 0 and cloned_text.get_selection_end() == 0,
		"A dirty input clone keeps the live value and resets the selection")
	_check(not cloned_check.checked and cloned_check.has_attribute("checked") and
		cloned_check.remove_attribute("checked") == OK and cloned_check.set_attribute("checked", "") == OK and
		not cloned_check.checked, "A checkbox clone preserves checkedness separately from the checked attribute")
	_check(cloned_area.default_value == "Default area" and cloned_area.value == "Current area" and
		cloned_area.get_selection_start() == 0 and cloned_area.get_selection_end() == 0,
		"A deep textarea clone retains both the default text and live value and resets the selection")
	cloned_area.default_value = "Changed default"
	_check(cloned_area.value == "Current area" and dirty.default_value == "Default area",
		"Changing the cloned textarea default preserves its dirty value and leaves the source unchanged")


func _check_dom_import() -> void:
	var source: HTMLLayout = HTMLLayout.new()
	var target: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><textarea id="clean">Initial text</textarea>
		<div id="card" data-origin="source"><input id="text" value="Default">
		<!--mark--><input id="check" type="checkbox" checked><textarea id="dirty">Default area</textarea>
		<img id="image" src="orientation-1.png"></div>"""
	var viewport_size: Vector2i = Vector2i(320, 240)
	_check(source.parse(html, "res://styles", "#card { width: 110px; }") == OK and source.layout(viewport_size) == OK,
		"Lay out the source document for import")
	var card: HTMLElement = source.get_element_by_id("card")
	var clean: HTMLTextAreaElement = source.get_element_by_id("clean") as HTMLTextAreaElement
	var text: HTMLInputElement = source.get_element_by_id("text") as HTMLInputElement
	var checkbox: HTMLInputElement = source.get_element_by_id("check") as HTMLInputElement
	var dirty: HTMLTextAreaElement = source.get_element_by_id("dirty") as HTMLTextAreaElement
	if card == null or clean == null or text == null or checkbox == null or dirty == null:
		_check(false, "Query the source elements for import")
		return
	_check(target.import_node(card) == null, "Import needs a successfully parsed target")
	_check(target.parse("", "res://", "#card { width: 70px; }") == OK and target.layout(viewport_size) == OK and
		target.import_node(null) == null and target.query_selector("html") == null,
		"An invalid import leaves the parsed empty target untouched")

	text.value = "Live text"
	checkbox.checked = false
	dirty.value = "Live area"
	dirty.set_selection_range(1, 4)
	var imported: HTMLElement = target.import_node(card, true)
	var shallow: HTMLTextAreaElement = target.import_node(clean) as HTMLTextAreaElement
	var body: HTMLElement = target.query_selector("body")
	if imported == null or shallow == null or body == null:
		_check(false, "Import initializes an empty target and preserves element types")
		return
	_check(imported != card and not imported.is_connected() and target.get_element_by_id("card") == null and
		imported.get_inner_html() == card.get_inner_html() and card.is_connected(),
		"Cross-layout import makes a detached copy with source attributes and child order")
	_check(body.append_child(card) == ERR_INVALID_PARAMETER and shallow.default_value.is_empty() and
		shallow.value == "Initial text", "Cross-layout insertion requires import and keeps the shallow textarea value")
	var imported_text: HTMLInputElement = imported.query_selector("#text") as HTMLInputElement
	var imported_checkbox: HTMLInputElement = imported.query_selector("#check") as HTMLInputElement
	var imported_area: HTMLTextAreaElement = imported.query_selector("#dirty") as HTMLTextAreaElement
	var image: HTMLImageElement = imported.query_selector("#image") as HTMLImageElement
	if imported_text == null or imported_checkbox == null or imported_area == null or image == null:
		_check(false, "Query the imported subtree")
		return
	_check(imported.set_attribute("data-origin", "target") == OK and card.get_attribute("data-origin") == "source",
		"Editing imported attributes leaves the original unchanged")
	source.clear()
	_check(not card.is_valid() and imported.is_valid() and imported.query_selector("#text") == imported_text and
		target.import_node(card) == null, "Clearing the source retires its handles while imported handles remain valid")
	_check(imported_text.set_attribute("value", "New default") == OK and imported_text.value == "Live text" and
		imported_checkbox.has_attribute("checked") and imported_checkbox.remove_attribute("checked") == OK and
		imported_checkbox.set_attribute("checked", "") == OK and not imported_checkbox.checked and
		imported_area.get_selection_end() == 0, "Imported form controls preserve live state and reset selection")
	imported_area.default_value = "New area default"
	_check(imported_area.value == "Live area" and body.append_child(imported) == OK and body.append_child(shallow) == OK and
		target.get_element_by_id("card") == imported, "A dirty imported textarea keeps its value and the copies can be inserted")
	await process_frame
	await process_frame
	_check(shallow.value == "Initial text" and shallow.get_bounding_rect().has_area() and
		is_equal_approx(imported.get_bounding_rect().size.x, 70.0) and image.current_src == "res://orientation-1.png" and
		image.get_intrinsic_size().x > 0.0, "Target reflow keeps the clean textarea value and uses target styles and resources")
	shallow.default_value = "Target default"
	_check(shallow.value == "Target default", "The imported clean textarea still follows changes to its default text")


func _check_dom_adopt() -> void:
	var source: HTMLLayout = HTMLLayout.new()
	var target: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><div id="card" style="height:100px"><input id="text" value="Default">
		<textarea id="area">Default area</textarea><input id="check" type="checkbox" checked></div>
		<div id="source-marker" style="height:12px"></div>"""
	var size: Vector2i = Vector2i(320, 250)
	_check(source.parse(html) == OK and source.layout(size) == OK, "Lay out the source for adoption")
	var card: HTMLElement = source.get_element_by_id("card")
	var text: HTMLInputElement = source.get_element_by_id("text") as HTMLInputElement
	var area: HTMLTextAreaElement = source.get_element_by_id("area") as HTMLTextAreaElement
	var checkbox: HTMLInputElement = source.get_element_by_id("check") as HTMLInputElement
	var marker: HTMLElement = source.get_element_by_id("source-marker")
	if card == null or text == null or area == null or checkbox == null or marker == null:
		_check(false, "Query the adoption fixture")
		return
	_check(target.adopt_node(card) == null and card.is_connected() and card.get_bounding_rect().has_area(),
		"Adoption into an unparsed target leaves the source tree and geometry intact")
	_check(target.parse("") == OK and target.adopt_node(source.query_selector("html")) == null and
		target.query_selector("html") == null and source.get_element_by_id("card") == card,
		"Rejecting the document root leaves both the source and parsed empty target intact")
	_check(target.parse('<!doctype html><div id="target-marker" style="height:12px"></div>',
		"res://", "#card { width:90px }") == OK and target.layout(size) == OK, "Lay out the target for adoption")
	var body: HTMLElement = target.query_selector("body")
	var target_marker: HTMLElement = target.get_element_by_id("target-marker")
	if body == null or target_marker == null:
		_check(false, "Query the adoption target")
		return
	text.value = "Live input"
	text.set_selection_range(1, 5)
	area.value = "Live area"
	area.set_selection_range(2, 7)
	checkbox.checked = false
	_check(source.layout(size) == OK, "Settle the source form layout before moving it")
	var marker_y: float = marker.get_bounding_rect().position.y
	_check(target.adopt_node(card) == card and not card.is_connected() and card.get_parent() == null and
		source.get_element_by_id("card") == null and target.get_element_by_id("card") == null and
		card.query_selector("#text") == text and card.query_selector("#area") == area and
		card.query_selector("#check") == checkbox, "Adoption moves a detached subtree while preserving every live handle")
	_check(not marker.get_bounding_rect().has_area() and target_marker.get_bounding_rect().has_area(),
		"Moving a connected subtree invalidates only the source geometry")
	area.default_value = "Other area default"
	_check(text.set_attribute("value", "Other input default") == OK and text.value == "Live input" and
		text.get_selection_start() == 1 and text.get_selection_end() == 5 and
		area.value == "Live area" and area.get_selection_start() == 2 and area.get_selection_end() == 7 and
		checkbox.remove_attribute("checked") == OK and checkbox.set_attribute("checked", "") == OK and not checkbox.checked,
		"Adoption preserves form values, modification flags and text selection")
	await process_frame
	await process_frame
	_check(marker.get_bounding_rect().position.y < marker_y and target_marker.get_bounding_rect().has_area(),
		"The source reflows after removal while the target retains its geometry")
	source.clear()
	_check(card.is_valid() and text.is_valid() and body.append_child(card) == OK and
		target.get_element_by_id("text") == text and not target_marker.get_bounding_rect().has_area(),
		"Clearing the source leaves adopted handles valid and connecting them invalidates the target")
	await process_frame
	await process_frame
	_check(is_equal_approx(card.get_bounding_rect().size.x, 90.0), "The inserted subtree reflows with the target styles")

	_check(source.parse("<!doctype html><div></div>") == OK and source.layout(size) == OK, "Prepare a detached adoption source")
	var detached: HTMLElement = source.create_element("div")
	var child: HTMLElement = source.create_element("span")
	var contents: HTMLElement = source.query_selector("div")
	if detached == null or child == null or contents == null:
		_check(false, "Create and query the detached adoption fixture")
		return
	_check(detached.append_child(child) == OK and target.adopt_node(child) == child and detached.get_children().is_empty() and
		contents.get_bounding_rect().size.x > 0.0 and card.get_bounding_rect().has_area(),
		"Moving a child out of a detached tree keeps both layouts current")
	_check(target.adopt_node(card) == card and not card.is_connected() and card.query_selector("#text") == text,
		"Adopting within the same layout detaches the subtree and retains its handles")
	target.clear()
	_check(not card.is_valid() and not child.is_valid() and detached.is_valid(),
		"The target owns migrated handles and clearing it retires only those handles")


func _check_adopted_input_focus() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 150)
	root.add_child(viewport)
	var source: HTMLLabel = HTMLLabel.new()
	source.size = Vector2(300.0, 150.0)
	source.text = '<div id="group"><input id="first" value="A"><input id="second" value="B"></div>'
	viewport.add_child(source)
	var target: HTMLLayout = HTMLLayout.new()
	_check(target.parse("") == OK, "Parse the focus adoption target")
	await process_frame
	await process_frame
	var group: HTMLElement = source.get_element_by_id("group")
	var first: HTMLInputElement = source.get_element_by_id("first") as HTMLInputElement
	var second: HTMLInputElement = source.get_element_by_id("second") as HTMLInputElement
	if group == null or first == null or second == null:
		_check(false, "Find the inputs for the focus adoption test")
		viewport.queue_free()
		return
	var commits: Array[HTMLElement] = []
	source.input_committed.connect(func(element: HTMLElement) -> void: commits.append(element))
	_check(source.focus_element(first), "Focus the first source field")
	first.set_selection_range(1, 1)
	_send_dom_key(viewport, KEY_NONE, 0x78)
	_check(first.value == "Ax" and source.focus_element(second), "Changing focus queues the first field's commit")
	second.set_selection_range(1, 1)
	_send_dom_key(viewport, KEY_NONE, 0x79)
	_check(second.value == "By" and source.get_focused_element() == second, "The second field has uncommitted text and focus")
	_check(target.adopt_node(group) == group and source.get_focused_element() == null and not second.matches(":focus"),
		"Adoption clears source focus immediately")
	await process_frame
	await process_frame
	_check(commits.is_empty() and first.value == "Ax" and second.value == "By",
		"Adoption cancels queued and pending source commits while preserving the edited values")
	viewport.queue_free()


func _check_disconnected_input_focus() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 240)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(300.0, 240.0)
	label.text = """<div id="fields"><input id="fragment" value="A">
		<input id="detach" value="A"><textarea id="replace">A</textarea><input id="remove" value="A"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var parent: HTMLElement = label.get_element_by_id("fields")
	var fragment: HTMLDocumentFragment = label.create_document_fragment()
	var replacement: HTMLElement = label.create_element("span")
	if parent == null or fragment == null or replacement == null:
		_check(false, "Set up the disconnection focus fixture")
		viewport.queue_free()
		return
	var input_commits: Array[HTMLElement] = []
	var textarea_commits: Array[HTMLElement] = []
	label.input_committed.connect(func(element: HTMLElement) -> void: input_commits.append(element))
	label.textarea_committed.connect(func(element: HTMLElement) -> void: textarea_commits.append(element))
	var expected_input_commits: Array[HTMLElement] = []
	var expected_textarea_commits: Array[HTMLElement] = []
	for operation: String in ["fragment", "detach", "replace", "remove",]:
		var field: HTMLElement = label.get_element_by_id(operation)
		if field == null or not label.focus_element(field):
			_check(false, "Focus the field for %s" % operation)
			viewport.queue_free()
			return
		_send_dom_key(viewport, KEY_NONE, 0x78)
		var result: Error
		match operation:
			"fragment":
				_check(parent.append_child(field) == OK and label.get_focused_element() == field and field.matches(":focus"),
					"Reordering a connected input retains focus")
				result = fragment.append_child(field)
				expected_input_commits.append(field)
			"detach":
				result = field.detach()
				expected_input_commits.append(field)
			"replace":
				result = parent.replace_child(replacement, field)
				expected_textarea_commits.append(field)
			_:
				result = field.remove()
		_check(result == OK and not field.is_connected() and label.get_focused_element() == null and not field.matches(":focus"),
			"%s clears DOM focus and the focus pseudo-class before returning" % operation)
		if operation == "fragment":
			_check(parent.append_fragment(fragment) == OK and field.is_connected() and label.get_focused_element() == null,
				"Reinserting the input in the same frame does not restore focus")
		await process_frame
		await process_frame
		_check(input_commits == expected_input_commits and textarea_commits == expected_textarea_commits,
			"%s follows the pending input commit policy" % operation)
	viewport.queue_free()


func _check_document_fragments() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.create_document_fragment() == null, "An unparsed layout cannot create a fragment")
	_check(layout.parse('<!doctype html><div id="target" style="height:20px"><span id="ref">Reference</span></div><img id="void">')
		== OK and layout.layout(Vector2i(240, 140)) == OK, "Set up the fragment document")
	var target: HTMLElement = layout.get_element_by_id("target")
	var reference: HTMLElement = layout.get_element_by_id("ref")
	var void_element: HTMLElement = layout.get_element_by_id("void")
	var fragment: HTMLDocumentFragment = layout.create_document_fragment()
	var a: HTMLElement = layout.create_element("div")
	var b: HTMLElement = layout.create_element("div")
	var descendant: HTMLElement = layout.create_element("span")
	var holder: HTMLElement = layout.create_element("div")
	if target == null or reference == null or void_element == null or fragment == null or a == null or b == null or descendant == null or holder == null:
		_check(false, "Create the fragment and query the test nodes")
		return
	_check(a.append_child(descendant) == OK and fragment.append_child(a) == OK and fragment.append_child(b) == OK and
		fragment.get_children() == [a, b] and a.get_parent() == null and not a.is_connected() and descendant.get_parent() == a,
		"Fragments hold detached elements in order and retain the original handles")
	var foreign: HTMLLayout = HTMLLayout.new()
	_check(foreign.parse("") == OK, "Parse an empty layout for a foreign fragment")
	var foreign_fragment: HTMLDocumentFragment = foreign.create_document_fragment()
	var foreign_element: HTMLElement = foreign.create_element("div")
	_check(foreign_fragment != null and foreign_element != null and foreign_fragment.append_child(foreign_element) == OK and
		target.append_fragment(foreign_fragment) == ERR_INVALID_PARAMETER and foreign_fragment.get_children() == [foreign_element] and
		fragment.append_child(foreign_element) == ERR_INVALID_PARAMETER,
		"Fragments and their children are scoped to one layout")
	_check(descendant.append_fragment(fragment) == ERR_INVALID_PARAMETER and void_element.append_fragment(fragment) == ERR_INVALID_PARAMETER and
		target.insert_fragment_before(fragment, descendant) == ERR_INVALID_PARAMETER and fragment.get_children() == [a, b] and
		target.get_children() == [reference] and target.get_bounding_rect().has_area(),
		"Cycles, void elements and invalid references leave the whole fragment and document unchanged")
	_check(holder.append_fragment(fragment) == OK and fragment.get_children().is_empty() and holder.get_children() == [a, b] and
		descendant.get_parent() == a and target.get_bounding_rect().has_area(),
		"Inserting into a detached element moves the whole fragment without invalidating the layout")
	_check(fragment.append_child(a) == OK and fragment.append_child(b) == OK and holder.get_children().is_empty() and
		target.insert_fragment_before(fragment, reference) == OK and fragment.get_children().is_empty() and
		target.get_children() == [a, b, reference] and a.get_parent() == target and a.is_connected(),
		"A reused fragment inserts its children before the reference in their original order")
	_check(fragment.append_child(reference) == OK and not reference.is_connected() and target.get_children() == [a, b],
		"Appending a previously connected element moves it into the fragment")
	layout.clear()
	_check(fragment.get_children().is_empty() and fragment.append_child(reference) == ERR_INVALID_PARAMETER and
		not a.is_valid() and not descendant.is_valid() and not reference.is_valid(),
		"Clearing a layout invalidates its fragment and the handles it contains")


func _check_fragment_reflow() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 180)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(300.0, 180.0)
	label.text = '<div id="target" style="height:120px"><span id="reference">Reference</span></div>'
	viewport.add_child(label)
	await process_frame
	await process_frame
	var target: HTMLElement = label.get_element_by_id("target")
	var reference: HTMLElement = label.get_element_by_id("reference")
	var fragment: HTMLDocumentFragment = label.create_document_fragment()
	if target == null or reference == null or fragment == null:
		_check(false, "Create a fragment for a connected HTMLLabel")
		viewport.queue_free()
		return
	var updates: Array[bool] = []
	label.layout_changed.connect(func() -> void: updates.append(true))
	var children: Array[HTMLElement] = []
	for index: int in range(3):
		var item: HTMLElement = label.create_element("p")
		if item == null:
			_check(false, "Create a fragment child")
			viewport.queue_free()
			return
		_check(item.set_text_content("Child %d" % index) == OK and fragment.append_child(item) == OK, "Populate the fragment")
		children.append(item)
	await process_frame
	await process_frame
	_check(updates.is_empty() and target.get_bounding_rect().has_area(), "Assembling detached fragment children leaves the layout current")
	_check(target.insert_fragment_before(fragment, reference) == OK and fragment.get_children().is_empty() and
		target.append_fragment(fragment) == OK, "Insert the entire fragment, then append the empty fragment")
	await process_frame
	await process_frame
	await process_frame
	children.append(reference)
	_check(updates.size() == 1 and target.get_children() == children and children[0].get_bounding_rect().has_area(),
		"A nonempty batch triggers exactly one layout update and the empty batch triggers none")
	viewport.queue_free()


func _check_imported_template() -> void:
	var document: HTMLDocument = ResourceLoader.load("{IMAGE_TEMPLATE_UID}") as HTMLDocument
	_check(document != null, "Load a template document with an imported image dependency")
	if document == null:
		return
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(64, 48)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.document = document
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var template: HTMLTemplateElement = label.get_element_by_id("icon") as HTMLTemplateElement
	var target: HTMLElement = label.get_element_by_id("target")
	_check(template != null and target != null, "The imported template and its target are visible to queries")
	if template == null or target == null:
		viewport.queue_free()
		return
	_check(label.query_selector("img") == null, "The imported template image stays out of document queries")
	_check_pixel(viewport.get_texture().get_image(), Vector2(3.0, 3.0), Color.BLACK, "The template has no visible pixels")
	var updates: Array[bool] = []
	label.layout_changed.connect(func() -> void: updates.append(true))
	var content: HTMLDocumentFragment = template.content
	_check(template.set_inner_html('<img src="template-icon.png" alt="Icon">') == OK,
		"Edit template markup in a connected HTMLLabel")
	await process_frame
	await process_frame
	_check(updates.is_empty(), "Editing template content does not relayout HTMLLabel")
	var instance: HTMLDocumentFragment = content.clone_node(true)
	_check(instance != null and target.append_fragment(instance) == OK, "Insert a copy of the imported template")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: HTMLImageElement = label.query_selector("img") as HTMLImageElement
	_check(updates.size() == 1 and image != null and image.get_intrinsic_size() == Vector2(8.0, 8.0)
		and image.get_bounding_rect().has_area(),
		"A template instance reflows once and resolves an embedded image after the source is removed")
	if image != null and image.get_bounding_rect().has_area():
		_check_pixel(viewport.get_texture().get_image(), image.get_bounding_rect().get_center(), Color.LIME,
			"The template's embedded image is painted after insertion")
	viewport.queue_free()


func _check_markdown_markup_resources() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(240, 100)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: MarkdownLabel = MarkdownLabel.new()
	label.size = Vector2(240.0, 100.0)
	label.parse_html = true
	var test_icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		label.add_theme_icon_override(icon, test_icon)
	viewport.add_child(label)
	var loaded: Error = label.load_markdown_file("res://styles/nested/inline-resources.md")
	_check(loaded == OK, "Open the Markdown fixture with its own base directory")
	if loaded != OK:
		viewport.queue_free()
		return
	var image_bounds: Rect2i
	for frame: int in range(30):
		await process_frame
		await RenderingServer.frame_post_draw
		var output: Image = viewport.get_texture().get_image()
		image_bounds = _color_bounds(output, Rect2i(Vector2i.ZERO, viewport.size), Color("#c5692b"))
		if image_bounds.has_area():
			break
	_check(image_bounds.has_area(), "Markdown paints an inline HTML image from its percent-encoded relative path")
	label.select_all()
	_check("Open & go" in label.get_selected_text(), "Markdown decodes entities in inline HTML")
	var uris: Array[String] = []
	label.link_pressed.connect(func(uri: String) -> void: uris.append(uri))
	var scroll: VScrollBar = label.get_v_scroll_bar()
	scroll.value = 0.0
	label.activate_link_uri("#end%2Dpoint")
	_check(label.has_anchor("end-point") and scroll.value > 0.0 and uris == ["#end%2Dpoint"],
		"Markdown resolves an encoded anchor and emits its original URI")
	viewport.queue_free()


func _check_css_nesting() -> void:
	var html: String = """<!doctype html><style>
		body { margin: 0; }
		#strong, .widget {
			article { width: 47px; }
			> .card, + .peer { height: 18px; }
			.theme & { padding-top: 4px; }
			div& { width: 108px; }
			& .slot & .leaf { width: 39px; }
			&Suffix { width: 399px; }
		}
		.theme .widget article { width: 75px; }
		.widgetSuffix { width: 21px; }
		.mixed {
			width: 20px;
			& { width: 30px; }
			width: 40px;
			> .never, { height: 99px; }
			@media (min-width: 300px) {
				width: 50px;
				@supports selector(&) {
					& { width: 60px; }
					width: 70px;
				}
			}
			height: 11px;
			--tokens: { color: red; };
			:where(&) { height: 99px; }
		}
		.layered {
			@layer visual, base;
			@layer base { width: 31px !important; }
			@layer visual { width: 34px !important; }
		}
		#unmatched {
			:is(:undefined(&), .external) { width: 44px; }
		}
		.relation { :has(> &) { padding-bottom: 9px; } }
		.pseudo { position: absolute; left: 200px; top: 0; width: 32px; height: 20px; background: black; }
		.pseudo::before {
			content: ""; display: block; width: 12px; height: 16px; background: red;
			@media (min-width: 300px) { background: lime; }
			& { background: blue; }
		}
		.hover { position: absolute; left: 8px; top: 104px; width: 140px; height: 24px; }
		.chip { width: 20px; height: 24px; }
		.hover:hover { @media (min-width: 200px) { > .chip { width: 56px; } } }
	</style><div class="theme"><div class="widget" id="widget">
		<article class="card" id="card"></article><div class="slot"><div class="widget"><div class="leaf" id="leaf"></div></div></div>
	</div><div class="peer" id="peer"></div></div><div class="widgetSuffix" id="suffix"></div>
	<div class="mixed" id="mixed"></div><div class="layered" id="layered"></div>
	<div class="external" id="external"></div><div id="ancestor"><div class="relation"></div></div>
	<div class="pseudo"></div><div class="hover"><div class="chip" id="chip"></div></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse(html) == OK and layout.layout(Vector2i(240, 190)) == OK, "Lay out nested CSS rules")
	_check(_rect(layout, "card").size == Vector2(47.0, 18.0) and is_equal_approx(_rect(layout, "peer").size.y, 18.0),
		"Bare type selectors and leading combinators nest with the parent list's maximum specificity")
	_check(is_equal_approx(_rect(layout, "leaf").size.x, 39.0) and is_equal_approx(_rect(layout, "widget").size.x, 108.0) and
		is_equal_approx(_rect(layout, "suffix").size.x, 21.0), "Nesting supports multiple ampersands and requires type selectors first")
	_check(is_equal_approx(_rect(layout, "card").position.y, 4.0), "An explicit ampersand can follow an ancestor selector")
	_check(_rect(layout, "mixed").size == Vector2(40.0, 11.0), "Interleaved declarations retain source order and :where has zero specificity")
	_check(is_equal_approx(_rect(layout, "layered").size.x, 31.0), "Nested layer blocks retain importance order and ignore nested layer statements")
	_check(is_equal_approx(_rect(layout, "external").size.x, 44.0) and is_equal_approx(_rect(layout, "ancestor").size.y, 9.0),
		"A nesting token inside :is or :has controls the relative scope")
	var widget: HTMLElement = layout.get_element_by_id("widget")
	_check(layout.query_selector("&").get_tag_name() == "html" and widget.matches("&") and
		widget.query_selector("& > .card") == layout.get_element_by_id("card"), "An unbound ampersand matches the query's scope")
	_check(layout.layout(Vector2i(340, 190)) == OK and _rect(layout, "mixed").size == Vector2(70.0, 11.0),
		"Nested conditional declarations and rules are ordered after a media relayout")
	var deep_rules: String = ".deep {" + "&& {".repeat(42) + "width: 27px;" + "}".repeat(43)
	_check(layout.parse("<!doctype html><div class='deep' id='deep'></div>", "res://", deep_rules) == OK and
		layout.layout(Vector2i(240, 190)) == OK and is_equal_approx(_rect(layout, "deep").size.x, 27.0),
		"Repeated nesting selectors share parent matches at deep nesting levels")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(340, 160)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(340.0, 160.0)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(204.0, 5.0), Color.LIME, "Direct declarations in a nested group match the parent's pseudo-element")
	_check_pixel(image, Vector2(225.0, 5.0), Color.BLACK, "Explicit nesting uses the parent's element selectors")
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = Vector2(14.0, 112.0)
	motion.global_position = motion.position
	viewport.push_input(motion, true)
	await process_frame
	await process_frame
	await process_frame
	_check(is_equal_approx(label.get_element_by_id("chip").get_bounding_rect().size.x, 56.0),
		"Hover updates reach rules whose parent selector has no declarations of its own")
	viewport.queue_free()


func _check_css_scope() -> void:
	var html: String = """<!doctype html><style>
		body { margin: 0; }
		.item, .local { width: 10px; height: 10px; }
		@scope (#near) {
			.item { width: var(--size, 40px); }
			.force { height: 30px !important; }
		}
		@scope (.outer) {
			.item { width: 60px; }
			.force { height: 80px; }
			#specific { width: 65px; }
		}
		.item { width: 90px; }
		@scope (.outer) to (.outer:scope) {
			width: 210px;
			@media (min-width: 280px) { width: 212px; }
			:SCOPE::before { content: "Scope root"; }
			@scope (> #near) to (> .limit) {
				> .item { height: 23px; }
				article :scope .inset { width: 75px; }
				.limit, .excluded { width: 99px; }
			}
		}
		@scope (.outer) to (.limit) {
			@scope (.inner) { .excluded { height: 99px; } }
		}
		@scope (.shared) { .nearer .distant { width: 37px; } }
		article { @scope (#near) { & { height: 100px; } } }
		@scope (.outer::before) { .item { width: 999px !important; } }
		@scope (.outer) to (.limit::after) { .item { width: 999px !important; } }
		@supports at-rule(@scope) {
			@scope (.outer) { #supports { width: 45px; } }
		}
	</style>
	<article class="outer shared" id="outer">
		<div class="shared nearer" id="near" style="--size: 43px">
			<div class="item" id="tie"></div><div class="item force" id="specific"></div>
			<div class="item distant" id="distant"></div>
			<div><div class="item inset" id="inset"></div></div>
			<div class="item limit" id="limit"><div class="inner"><div class="item excluded" id="excluded"></div></div></div>
			<div class="item supports" id="supports"></div>
		</div>
	</article>
	<div class="item" id="outside"></div>
	<section id="local_left">
		<style id="local_sheet">@scope to (.stop) { .local { width: 41px; } }</style>
		<div class="local" id="local_a"></div><div class="stop"><div class="local" id="local_excluded"></div></div>
	</section>
	<section id="local_right"><div class="local" id="local_b"></div></section>"""
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse(html) == OK and layout.layout(Vector2i(300, 400)) == OK, "Lay out scoped CSS rules")
	var expected: Dictionary[String, Vector2] = {
		"near": Vector2(212.0, 100.0),
		"tie": Vector2(43.0, 23.0),
		"specific": Vector2(65.0, 30.0),
		"distant": Vector2(37.0, 23.0),
		"inset": Vector2(75.0, 10.0),
		"limit": Vector2(43.0, 10.0),
		"excluded": Vector2(43.0, 10.0),
		"supports": Vector2(45.0, 23.0),
		"outside": Vector2(90.0, 10.0),
		"local_a": Vector2(41.0, 10.0),
		"local_b": Vector2(10.0, 10.0),
		"local_excluded": Vector2(10.0, 10.0),
	}
	for id: String in expected:
		_check(_rect(layout, id).size.is_equal_approx(expected[id]), "Scoped CSS size for %s" % id)
	_check(is_equal_approx(_rect(layout, "outer").size.x, 212.0), "Direct declarations apply to the scope root")
	_check("Scope root" in layout.get_plain_text(), "Scoped pseudo-elements match the inclusive root")
	_check(layout.layout(Vector2i(260, 400)) == OK and is_equal_approx(_rect(layout, "outer").size.x, 210.0),
		"Direct scoped declarations follow media conditions")
	var parent: HTMLElement = layout.get_element_by_id("local_right")
	var sheet: HTMLElement = layout.get_element_by_id("local_sheet")
	_check(parent.append_child(sheet) == OK, "Move the owner of an implicitly scoped stylesheet")
	await process_frame
	await process_frame
	_check(is_equal_approx(_rect(layout, "local_a").size.x, 10.0) and is_equal_approx(_rect(layout, "local_b").size.x, 41.0),
		"An implicit scope follows its stylesheet owner's parent")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(200, 100)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(200.0, 100.0)
	label.text = """<!doctype html><style>
		body { margin: 0; }
		.trigger, .boundary { width: 100px; height: 30px; }
		.dynamic { width: 20px; height: 20px; }
		@scope (.trigger:hover) { .dynamic { width: 50px; } }
		@scope (.bounded) to (.boundary:hover) { .dynamic { width: 50px; } }
	</style><div class="trigger"><div class="dynamic" id="start"></div></div>
	<div class="bounded"><div class="boundary"><div class="dynamic" id="end"></div></div></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var hover_steps: Array[Vector2] = [
		Vector2(70.0, 10.0), Vector2(70.0, 40.0), Vector2(160.0, 80.0),
	]
	var widths: Array[Vector2] = [
		Vector2(50.0, 50.0), Vector2(20.0, 20.0), Vector2(20.0, 50.0),
	]
	viewport.notify_mouse_entered()
	for step: int in hover_steps.size():
		var motion: InputEventMouseMotion = InputEventMouseMotion.new()
		motion.position = hover_steps[step]
		motion.global_position = motion.position
		motion.relative = motion.position - viewport.get_mouse_position()
		viewport.push_input(motion, true)
		await process_frame
		await process_frame
		await process_frame
		var actual: Vector2 = Vector2(_label_rect(label, "start").size.x, _label_rect(label, "end").size.x)
		_check(actual.is_equal_approx(widths[step]), "Scoped hover at %s gives %s, expected %s" % [
			motion.position, actual, widths[step],
		])
	viewport.queue_free()


func _check_all_shorthand() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """<!doctype html><style>
		body { margin: 0; color: red; }
		#before { width: 70px; all: initial; display: block; }
		#after { all: unset; display: block; width: 71px; }
		#all_important { width: 72px; all: unset !important; display: block !important; }
		#named_important { width: 73px !important; all: unset; display: block; }
		#inline, #inline_important { width: 74px !important; }
		#invalid_literal { width: 75px; all: block; }
		#invalid_var { width: 76px; all: var(--missing); display: block; }
		#wrong_var { --wrong: 4px; width: 77px; all: var(--wrong); display: block; }
		#custom { --width: 78px; all: var(--missing, initial); display: block; width: var(--width); }
		#rollback { display: none; all: revert; }
		#flow_parent { direction: ltr; margin-left: 16px; margin-right: 34px; width: 180px; }
		#flow_reset { direction: rtl; margin-inline-end: 90px; all: inherit; width: 80px; }
		#flow_later { direction: rtl; all: inherit; margin-inline-end: 7px; width: 80px; }
		#type { font-size: 32px; }
		#initial_reference { font-size: initial; width: 4em; }
		#unset_font { all: unset; display: block; width: 4em; }
		@layer base, theme;
		@layer base {
			#layer_reset { width: 79px; }
			#layer_var { width: 80px; }
			#earlier_all { all: initial; display: block; width: 4em; }
			#layer_inline { width: 81px; }
			#layer_logical { margin-inline-start: 12px; width: 40px; }
		}
		@layer theme {
			#layer_reset { all: revert-layer; }
			#layer_var { all: var(--missing, revert-layer); }
			#earlier_all { font-size: revert-layer; }
			#layer_logical { all: revert-layer; }
		}
		@supports (all: var(--reset)) and (all: initial) and (not (all: block)) { #supports { width: 82px; } }
	</style><div id="before">a</div><div id="after">a</div><div id="all_important">a</div><div id="named_important">a</div>
	<div id="inline" style="all: initial; display: block">a</div>
	<div id="inline_important" style="all: initial !important; display: block !important">a</div>
	<div id="invalid_literal">a</div><div id="invalid_var">a</div><div id="wrong_var">a</div>
	<div id="custom">a</div><div id="rollback">a</div>
	<div id="flow_parent"><div id="flow_reset">a</div><div id="flow_later">a</div></div>
	<div id="type"><div id="initial_reference">a</div><div id="unset_font">a</div><div id="earlier_all">a</div></div>
	<div id="layer_reset">a</div><div id="layer_var">a</div><div id="layer_inline" style="all: revert-layer">a</div>
	<div id="layer_logical">a</div><div id="supports">a</div>"""
	_check(layout.parse(source) == OK and layout.layout(Vector2i(320, 550)) == OK, "Parse and lay out the all shorthand")
	var widths: Dictionary[String, float] = {
		"before": 320.0,
		"after": 71.0,
		"all_important": 320.0,
		"named_important": 73.0,
		"inline": 74.0,
		"inline_important": 320.0,
		"invalid_literal": 75.0,
		"invalid_var": 320.0,
		"wrong_var": 320.0,
		"custom": 78.0,
		"rollback": 320.0,
		"unset_font": 128.0,
		"layer_reset": 79.0,
		"layer_var": 80.0,
		"layer_inline": 81.0,
		"supports": 82.0,
	}
	for id: String in widths:
		var actual: float = _rect(layout, id).size.x
		_check(is_equal_approx(actual, widths[id]), "CSS all width for %s is %s, expected %s" % [id, actual, widths[id]])
	_check(is_equal_approx(_rect(layout, "earlier_all").size.x, _rect(layout, "initial_reference").size.x),
		"A later named rollback can pick an earlier all declaration")
	var parent_x: float = _rect(layout, "flow_parent").position.x
	_check(is_equal_approx(_rect(layout, "flow_reset").position.x - parent_x, 16.0),
		"All inherits the physical margin even when a lower-priority logical declaration is present")
	_check(is_equal_approx(_rect(layout, "flow_later").position.x - parent_x, 7.0),
		"All preserves direction and lets a later logical declaration win")
	_check(is_equal_approx(_rect(layout, "layer_logical").position.x, 12.0), "All can roll back to a logical declaration")
	for value: String in ["block", "var(--missing)", "var(--wrong)",]:
		_check(_has_diagnostic(layout, "all", value, "style_element"), "The invalid all value %s retains its source" % value)
	_check(layout.get_css_diagnostics().size() == 3, "All diagnoses each invalid declaration once")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(160, 65)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		html { background: white; } body { margin: 8px; color: red; font: 26px monospace; }
		#initial { background-color: lime; opacity: 0; all: initial; font: 26px monospace; }
		#unset { background-color: lime; all: unset; }
		#inline_box { all: initial; }
	</style><span id="initial">MMM</span><span id="unset">MMM</span><div id="inline_box">M</div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var initial: Rect2i = Rect2i(_label_rect(label, "initial"))
	var unset: Rect2i = Rect2i(_label_rect(label, "unset"))
	_check(_color_bounds(image, initial, Color.BLACK).has_area() and not _color_bounds(image, initial, Color.LIME).has_area(),
		"All initial resets text color, background, and opacity")
	_check(_color_bounds(image, unset, Color.RED).has_area() and not _color_bounds(image, unset, Color.LIME).has_area(),
		"All unset uses inherited text color and initial background")
	_check(_label_rect(label, "inline_box").position.x > float(unset.position.x), "All initial resets a div's default block display")
	viewport.queue_free()


func _check_layers() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(LAYERS, "res://", ADDITIONAL_LAYERS) == OK and layout.layout(Vector2i(320, 500)) == OK,
		"Parse and lay out cascade layers")
	var widths: Dictionary[String, float] = {
		"normal": 70.0,
		"important": 52.0,
		"outside": 65.0,
		"inline_layer_normal": 68.0,
		"inline_layer_important": 69.0,
		"rollback": 53.0,
		"rollback_var": 54.0,
		"rollback_var_deferred": 72.0,
		"rollback_var_cycle": 73.0,
		"rollback_chain": 55.0,
		"important_rollback": 320.0,
		"bad_layer_value": 320.0,
		"widgets_parent": 56.0,
		"layer_import": 71.0,
		"nested_layer_import": 73.0,
		"conditional_import": 320.0,
		"default_layer": 72.0,
		"anonymous": 58.0,
		"media_order": 60.0,
		"import_media": 62.0,
		"case_name": 64.0,
		"inline_rollback": 66.0,
		"inline_rollback_important": 67.0,
	}
	for id: String in widths:
		var width: float = _rect(layout, id).size.x
		_check(is_equal_approx(width, widths[id]), "Layer width for %s is %s, expected %s" % [id, width, widths[id]])
	_check(is_equal_approx(_rect(layout, "layer_shorthand").position.x, 6.0),
		"Deferred shorthand rolls back only its winning longhands")
	_check("THEMEcontentADDITIONAL" in layout.get_plain_text(), "Generated pseudo-elements use cascade layers")
	_check(_has_diagnostic(layout, "width", "var(--missing)", "style_element"),
		"An invalid winning substitution in a layer retains the declaration diagnostic")
	_check(_has_diagnostic(layout, "width", "bogus", "local_stylesheet", "res://styles/nested/layer-inner.css"),
		"Nested layered imports retain their diagnostic paths")
	_check(not _has_diagnostic(layout, "width", "bogus", "local_stylesheet", "res://styles/rejected.css"),
		"A rejected layered import leaves no diagnostics")
	_check(layout.layout(Vector2i(480, 500)) == OK, "Reflow cascade layers across a media breakpoint")
	_check(is_equal_approx(_rect(layout, "media_order").size.x, 59.0), "A media condition changes layer registration order")
	_check(is_equal_approx(_rect(layout, "import_media").size.x, 61.0),
		"A media condition activates a layered import at its source position")
	_check(layout.layout(Vector2i(320, 500)) == OK, "Deactivate media layers")
	_check(is_equal_approx(_rect(layout, "media_order").size.x, 60.0), "Media layers can be removed from the ordering")


func _check_box_and_transform_functions() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """
<!doctype html><style>
body { margin: 0; }
div { position: absolute; width: 20px; height: 10px; transform-origin: 0 0; }
#inset { --offsets: 5px auto auto calc(2px + 3px); inset: var(--offsets); }
#matrix { inset: 10px auto auto 50px; transform: matrix(2, 0, 0, 3, 4, 6); }
#skew_x { inset: 60px auto auto 20px; transform: skewX(45deg); }
#skew_y { inset: 80px auto auto 20px; transform: skewY(45deg); }
#skew { inset: 100px auto auto 20px; transform: skew(45deg, 45deg); }
#combined { inset: 145px auto auto 20px; transform: translateX(5px) skew(45deg, 0); }
#invalid { inset: 170px auto auto 20px; transform: translateX(5px); transform: matrix(1,0,0,1,5px,0); }
</style>
<div id="inset" style="left: 15px; inset: 1px bad"></div>
<div id="matrix"></div><div id="skew_x"></div><div id="skew_y"></div><div id="skew"></div>
<div id="combined"></div><div id="invalid"></div>
"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(250, 240)) == OK, "Lay out affine and inset fixture")
	var rectangles: Dictionary[String, Rect2] = {
		"inset": Rect2(15.0, 5.0, 20.0, 10.0),
		"matrix": Rect2(54.0, 16.0, 40.0, 30.0),
		"skew_x": Rect2(20.0, 60.0, 30.0, 10.0),
		"skew_y": Rect2(20.0, 80.0, 20.0, 30.0),
		"skew": Rect2(20.0, 100.0, 30.0, 30.0),
		"combined": Rect2(25.0, 145.0, 30.0, 10.0),
		"invalid": Rect2(25.0, 170.0, 20.0, 10.0),
	}
	for id: String in rectangles:
		var actual: Rect2 = _rect(layout, id)
		_check(actual.is_equal_approx(rectangles[id]), "%s bounds are %s, expected %s" % [id, actual, rectangles[id]])
	_check(layout.get_element_at(Vector2(22.0, 68.0)) != layout.get_element_by_id("skew_x"),
		"Hit testing skips the skewed rectangle's empty corner")
	_check(layout.get_element_at(Vector2(36.0, 68.0)) == layout.get_element_by_id("skew_x"),
		"Hit testing finds the skewed rectangle")


func _check_display_contents() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """
<!doctype html><style>
html { display: contents; }
body { margin: 0; background: black; }
#grid { display: block grid; grid-template: 18px / 40px 55px 65px; }
#wrapper { --box: contents; display: var(--box); position: relative; padding: 40px;
  transform: translateX(100px); opacity: 0; color: #345678; content-visibility: hidden; }
#wrapper.boxed { display: block; position: static; padding: 0; transform: none; opacity: 1;
  content-visibility: visible; }
#wrapper::before { content: ""; display: block; background: #832146; }
#inner, #flex_wrapper, #absolute_wrapper { display: contents; }
#grid_link { color: inherit; background: currentColor; }
#end { background: #176543; }
#flex { display: flex block; height: 14px; }
#flex_wrapper > * { flex: 0 0 35px; }
#flex_a { display: inline grid; grid-template-columns: 1fr; background: #754024; }
#flex_b { display: block; display: inline contents; background: #254075; }
#positioned { position: relative; width: 150px; height: 20px; }
#absolute_wrapper { position: relative; padding: 30px; }
#absolute { position: absolute; right: 3px; width: 12px; height: 10px; background: #565656; }
#hidden_image, #hidden_select, #hidden_textarea { display: contents; }
#boxless_link { display: contents; }
#link_child { display: inline flow-root; width: 24px; height: 12px; background: #127348; }
.decor { position: absolute; top: 75px; color: white; font: 16px/20px sans-serif;
  text-decoration: overline solid #44cc44 2px; }
.decor_wrapper { display: contents; text-decoration: underline solid red 2px; }
#hidden_text { position: absolute; left: 175px; top: 75px; font: 16px/20px sans-serif; }
#hidden_text span { display: contents; visibility: hidden; color: magenta; }
@supports (display: contents) and (display: inline flex) {
  #supports { display: block flow-root; width: 17px; height: 5px; }
}
</style>
<div id="grid"><div id="wrapper"><div id="inner">
  <a id="grid_link" href="#supports">Click</a></div></div><div id="end"></div></div>
<div id="flex"><div id="flex_wrapper"><div id="flex_a"></div><div id="flex_b"></div></div></div>
<div id="positioned"><div id="absolute_wrapper"><div id="absolute"></div></div></div>
<img id="hidden_image" alt="Unexpected image fallback">
<select id="hidden_select"><option>Unexpected option</option></select>
<textarea id="hidden_textarea">Unexpected field</textarea>
<div id="supports"></div>
<div><a id="boxless_link" href="#supports"><span id="link_child"></span></a></div>
<div class="decor" style="left: 0"><span class="decor_wrapper">Some<span> text</span></span></div>
<div class="decor" style="left: 80px">Some<span> text</span></div>
<div id="hidden_text"><span>Hidden</span></div>
"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(240, 100)) == OK, "Lay out display: contents fixture")
	var wrapper: HTMLElement = layout.get_element_by_id("wrapper")
	var link: HTMLElement = layout.get_element_by_id("grid_link")
	_check(not wrapper.get_bounding_rect().has_area() and wrapper.get_client_rects().is_empty(),
		"A contents element has no client rectangles")
	_check(wrapper.query_selector(":scope > #inner > #grid_link") == link and link.get_parent().get_parent() == wrapper,
		"Nested contents elements remain in the DOM ancestry")
	_check(link.get_bounding_rect().is_equal_approx(Rect2(40.0, 0.0, 55.0, 18.0))
		and _rect(layout, "end").is_equal_approx(Rect2(95.0, 0.0, 65.0, 18.0)),
		"Grid places generated content and descendants in box-tree order")
	_check(layout.get_link_at(Vector2(90.0, 11.0)) == "#supports",
		"Link hit testing follows promoted grid items")
	var child_hit: Vector2 = _rect(layout, "link_child").get_center()
	_check(layout.get_element_at(child_hit) == layout.get_element_by_id("link_child")
		and layout.get_link_at(child_hit) == "#supports"
		and layout.get_element_by_id("boxless_link").get_client_rects().is_empty(),
		"Hit testing finds the rendered child and resolves the link through its DOM ancestor")
	var text_offset: int = layout.get_plain_text().find("Click")
	_check(text_offset >= 0 and not layout.get_selection_rects(text_offset, text_offset + 5).is_empty(),
		"Text selection preserves the promoted item's text geometry")
	_check(_rect(layout, "flex_a").is_equal_approx(Rect2(0.0, 18.0, 35.0, 14.0))
		and _rect(layout, "flex_b").is_equal_approx(Rect2(35.0, 18.0, 35.0, 14.0)),
		"Flex items are blockified and sized under nested contents, and invalid display keeps its fallback")
	_check(is_equal_approx(_rect(layout, "absolute").position.x, 135.0),
		"The positioned contents wrapper does not establish a containing block")
	for id: String in ["hidden_image", "hidden_select", "hidden_textarea",]:
		_check(layout.get_element_by_id(id).get_client_rects().is_empty(),
			"Contents suppresses the replaced box for %s" % id)
	_check(not layout.get_plain_text().contains("Unexpected") and is_equal_approx(_rect(layout, "supports").size.x, 17.0),
		"Suppressed controls have no rendered text and feature queries recognize the new display grammar")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(240, 100)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(240.0, 100.0)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(10.0, 10.0), Color("#832146"), "Generated box under display: contents")
	_check_pixel(image, Vector2(90.0, 10.0), Color("#345678"), "Inherited color under display: contents")
	_check_pixel(image, Vector2(120.0, 10.0), Color("#176543"), "Sibling box after display: contents")
	var decorated: Rect2i = Rect2i(0, 74, 78, 22)
	_check(_color_bounds(image, decorated, Color("#44cc44")).has_area()
		and image.get_region(decorated).get_data() == image.get_region(Rect2i(80, 74, 78, 22)).get_data(),
		"Decoration follows the box tree for both direct text and child elements")
	_check(image.get_region(Rect2i(175, 74, 60, 22)).get_data() == image.get_region(Rect2i(175, 20, 60, 22)).get_data(),
		"Boxless wrappers still pass visibility to their direct text")
	viewport.queue_free()

	wrapper.set_attribute("class", "boxed")
	_check(layout.layout(Vector2i(240, 100)) == OK and wrapper.get_bounding_rect().has_area()
		and is_equal_approx(_rect(layout, "end").position.x, 40.0),
		"Changing contents to block restores the wrapper's own grid placement")
	wrapper.remove_attribute("class")
	_check(layout.layout(Vector2i(240, 100)) == OK and wrapper.get_client_rects().is_empty()
		and layout.get_element_by_id("grid_link") == link and is_equal_approx(link.get_bounding_rect().position.x, 40.0),
		"Returning to contents rebuilds geometry and retains node handles")


func _check_math_functions() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var widths: Dictionary[String, float] = {
		"round(35px, 10px)": 40.0,
		"round(up, 31px, 10px)": 40.0,
		"round(down, 39px, 10px)": 30.0,
		"calc(50px + round(to-zero, -19px, 10px))": 40.0,
		"calc(round(-1.5) * -10px)": 10.0,
		"mod(-13px, 5px)": 2.0,
		"calc(20px + rem(-13px, 5px))": 17.0,
		"calc(20px + mod(13px, -5px))": 18.0,
		"calc(40px * sin(pi / 6))": 20.0,
		"calc(40px * cos(60deg))": 20.0,
		"calc(20px * tan(50grad))": 20.0,
		"calc(30px * sin(asin(1)))": 30.0,
		"calc(30px * sin(acos(0)))": 30.0,
		"calc(25px * sin(atan(1) + 45deg))": 25.0,
		"calc(20px * sin(atan2(1in, 96px) + 45deg))": 20.0,
		"calc(2px * pow(3, 2) + sqrt(16) * 1px)": 22.0,
		"hypot(3px, 4px, 12px)": 13.0,
		"calc((log(100, 10) + log(e) + exp(0)) * 5px)": 20.0,
		"calc(abs(-25px) + sign(-2em) * 5px)": 20.0,
		"calc(10px * (2in / 96px))": 20.0,
		"calc(10px * (1em / 1px))": 120.0,
		"calc(10px * (180deg / 0.5turn))": 10.0,
		"min(44px, calc(infinity * 1px))": 44.0,
		"calc(20px * max(0, sign(-0)))": 0.0,
		"calc(20px + rem(2px, 0px))": 0.0,
		"calc(20px + pow(NaN, 0) * 1px)": 0.0,
		"calc(20px + min(3, hypot(infinity, NaN)) * 1px)": 0.0,
		"round(23px)": 19.0,
		"sin(10px)": 19.0,
		"pow(2px, 2)": 19.0,
		"hypot(3px, 4deg)": 19.0,
	}
	var html: String = """<!doctype html><style>
	body { margin: 0; }
	.probe { height: 4px; font-size: 12px; }
	#responsive { width: round(down, 37%, 10px); }
	#sign_basis { width: calc(50px + sign(50% - 100px) * 10px); }
	#angle_scale { width: 10px; height: 15px; transform: rotate(atan2(1, 0)) scale(sqrt(4)); }
	</style>"""
	var index: int
	for expression: String in widths:
		html += '<div class="probe" id="math_%d" style="width: 19px; width: %s"></div>' % [index, expression]
		index += 1
	html += '<div class="probe" id="responsive"></div><div class="probe" id="sign_basis"></div><div id="angle_scale"></div>'
	_check(layout.parse(html) == OK and layout.layout(Vector2i(180, 420)) == OK, "Lay out CSS math function fixture")
	index = 0
	for expression: String in widths:
		var actual: float = _rect(layout, "math_%d" % index).size.x
		_check(is_equal_approx(actual, widths[expression]),
			"%s gives width %s, expected %s" % [expression, actual, widths[expression]])
		index += 1
	_check(is_equal_approx(_rect(layout, "responsive").size.x, 60.0), "round() uses the containing width for percentages")
	_check(is_equal_approx(_rect(layout, "sign_basis").size.x, 40.0), "sign() keeps the basis of a percentage-length argument")
	_check(_rect(layout, "angle_scale").size.is_equal_approx(Vector2(30.0, 20.0)), "Transforms use computed numbers and angles")
	_check(layout.layout(Vector2i(300, 420)) == OK, "Relayout math functions at a larger width")
	_check(is_equal_approx(_rect(layout, "responsive").size.x, 110.0), "round() follows viewport changes")
	_check(is_equal_approx(_rect(layout, "sign_basis").size.x, 60.0), "sign() reevaluates when the basis crosses zero")
	_check(_has_diagnostic(layout, "width", "pow(2px , 2)", "inline_style"), "Math type errors retain declaration diagnostics")


func _check_length_units() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """<!doctype html>
<style>
html { font: calc(150% + 1rem) / calc(1em + 2px) monospace; }
body { margin: 0; }
.probe { height: 2px; }
#media { width: 10px; }
@media (min-width: 20rem) { #media { width: 20px; } }
#dynamic_media { width: 10px; }
@media (calc(100dvh + 1px) < width <= 110lvw) { #dynamic_media { width: 30px; } }
.line { font: 20px / calc(3 / 2) sans-serif; }
#parent_line { font-size: .5lh; line-height: calc(1lh + 2px); width: 1em; height: 1lh; }
#inherited_line { font-size: 10px; width: 1rlh; height: 1lh; }
#numeric_line { font-size: 10px; width: 1rem; height: 1lh; }
</style>
<div id="absolute" class="probe" style="width: 127Q"></div>
<div id="dynamic_media" class="probe"></div><div id="media" class="probe"></div>
<div id="math" class="probe" style="--span: 20svw; width: calc(var(--span) + 10dvb + 127q)"></div>
<div class="line">
  <div id="parent_line"><div id="inherited_line"></div></div>
  <div id="numeric_line"></div>
</div>
<div id="root_units" style="width: calc(1rch + 1rcap); height: calc(1rex + 1ric)"></div>
<div id="font_units" style="width: calc(1ch + 1cap); height: calc(1ex + 1ic)"></div>
<div id="normal_line" style="font-size: 20px; line-height: normal; height: 1lh"></div>
<div id="emphasis_line" style="font-size: 20px; line-height: normal; text-emphasis: dot; height: 1lh"></div>
"""
	var axis_units: Dictionary[String, String] = {
		"w": "w",
		"h": "h",
		"i": "w",
		"b": "h",
		"min": "min",
		"max": "max",
	}
	for prefix: String in ["v", "sv", "lv", "dv"]:
		for axis: String in axis_units:
			var unit: String = prefix + axis
			html += '<div id="%s" class="probe" style="width: 10%s"></div>' % [unit, unit]
	_check(layout.parse(html) == OK, "Parse viewport and font-relative lengths")
	for viewport: Vector2i in [Vector2i(400, 240), Vector2i(200, 360), Vector2i(420, 410)]:
		_check(layout.layout(viewport) == OK, "Lay out length units at %s" % viewport)
		var reference: Dictionary[String, float] = {
			"w": viewport.x * 0.1,
			"h": viewport.y * 0.1,
			"min": mini(viewport.x, viewport.y) * 0.1,
			"max": maxi(viewport.x, viewport.y) * 0.1,
		}
		for prefix: String in ["v", "sv", "lv", "dv"]:
			for axis: String in axis_units:
				var unit: String = prefix + axis
				var expected: float = reference[axis_units[axis]]
				_check(is_equal_approx(_rect(layout, unit).size.x, expected), "%s resolves at %s" % [unit, viewport])
		var expected_math: float = viewport.x * 0.2 + viewport.y * 0.1 + 120.0
		_check(is_equal_approx(_rect(layout, "math").size.x, expected_math), "Math mixes viewport and absolute units")
		_check(is_equal_approx(_rect(layout, "media").size.x, 20.0 if viewport.x >= 320 else 10.0),
			"Media queries resolve rem against the initial font size")
		_check(is_equal_approx(_rect(layout, "dynamic_media").size.x, 30.0 if viewport.x > viewport.y + 1 else 10.0),
			"Media query range recalculates both viewport-dependent bounds")
		_check(_rect(layout, "parent_line").size.is_equal_approx(Vector2(15.0, 32.0)), "lh uses parent metrics in font-size and line-height")
		_check(_rect(layout, "inherited_line").size.is_equal_approx(Vector2(42.0, 32.0)), "Inherited length line-height is already computed")
		_check(_rect(layout, "numeric_line").size.is_equal_approx(Vector2(40.0, 15.0)), "Inherited unitless line-height uses the child's font size")
		var root_units: Vector2 = _rect(layout, "root_units").size
		_check(root_units.x > 0.0 and root_units.y > 0.0 and root_units.is_equal_approx(_rect(layout, "font_units").size),
			"Local and root glyph units resolve to the same metrics with the same font")
		var normal: float = _rect(layout, "normal_line").size.y
		_check(normal > 0.0 and is_equal_approx(normal, _rect(layout, "emphasis_line").size.y),
			"Normal lh uses the font metrics without extra space added by text emphasis")
	_check(is_equal_approx(_rect(layout, "absolute").size.x, 120.0), "127Q equals 120 CSS pixels")
	_check(layout.get_css_diagnostics().is_empty(), "Supported lengths leave no diagnostics")


func _check_grid_intrinsic_tracks() -> void:
	var font_fixture: HTMLDocument = load("{FONT_FACE_UID}")
	_check(font_fixture != null, "Load the intrinsic sizing font fixture")
	if font_fixture == null:
		return
	var assets: Dictionary = font_fixture.get("_local_fonts")
	var font: FontFile = assets["res://fonts/cour.ttf"]
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", assets)
	document.text = """<!doctype html><style>
		@font-face { font-family: GridMono; src: url(fonts/cour.ttf); }
		body { margin: 0; font: 20px / 24px GridMono; }
		.grid { display: grid; width: 40ch; column-gap: 1ch; }
		.inline { display: inline-grid; }
		.basic { grid-template-columns: min-content max-content fit-content(calc(4ch + 2ch)) auto; }
		.floors { grid-template-columns: repeat(3, fit-content(2ch)); }
		.relaxed { min-width: 0; }
		.scrollable { overflow: auto; }
		.bounds { grid-template-columns: minmax(min-content, 2ch) minmax(max-content, min-content)
			minmax(0, min-content) minmax(min-content, 1fr); }
		.percent { grid-template-columns: fit-content(25%); }
		.capped_spans { grid-template-columns: fit-content(2ch) fit-content(4ch); }
		#typed_spans { grid-template-columns: min-content max-content; }
		.span { grid-column: 1 / -1; }
		.fractional { width: 12ch; grid-template-columns: min-content minmax(min-content, 1fr); }
		#flex_span { grid-column: 1 / -1; }
		.rows { height: 200px; grid-template-rows: min-content max-content fit-content(36px) auto; }
		.rows .relaxed { min-height: 0; }
		.implicit { grid: auto-flow max-content / 3ch; grid-auto-columns: min-content fit-content(6ch); }
		.auto_repeat { width: 18ch; grid-template-columns: repeat(auto-fill, minmax(5ch, max-content)); }
		.invalid { grid-template-columns: 4ch; grid-template-columns: repeat(auto-fit, max-content); }
		.bad_cap { grid-template-columns: 4ch; grid-template-columns: fit-content(-10px); }
		#condition { width: 1ch; height: 1px; }
		@supports (grid-template-columns: repeat(2, fit-content(6ch) min-content)) and
			(not (grid-template-columns: repeat(auto-fill, fit-content(6ch)))) and
			(not (grid-template-columns: minmax(fit-content(6ch), 1fr))) { #condition { width: 5ch; } }
	</style>
	<div class="grid basic">
		<div id="min">AAAA BBBB</div><div id="max">AAAA BBBB</div>
		<div id="fit">AAAA BBBB</div><div id="auto">AAAA BBBB</div>
	</div>
	<div class="grid floors">
		<div id="floor">AAAA BBBB</div><div id="relaxed" class="relaxed">AAAA BBBB</div>
		<div id="scrollable" class="scrollable">AAAA BBBB</div>
	</div>
	<div class="grid bounds">
		<div id="min_bound" class="relaxed">AAAA BBBB</div><div id="max_bound">AAAA BBBB</div>
		<div id="min_limit">AAAA BBBB</div><div id="fraction">AAAA BBBB</div>
	</div>
	<div class="grid percent"><div id="percent">AAAA BBBB CCCC</div></div>
	<div class="inline percent" id="inline_percent"><div id="inline_track">AAAA BBBB CCCC</div></div>
	<div class="grid capped_spans">
		<div id="cap_a"></div><div id="cap_b"></div><div class="span">AAAA BBBB CCCC</div>
	</div>
	<div class="grid capped_spans">
		<div id="floor_a"></div><div id="floor_b"></div><div class="span">AAAAAAAA</div>
	</div>
	<div class="grid" id="typed_spans">
		<div id="span_min"></div><div id="span_max"></div><div class="span">AAAA BBBB</div>
	</div>
	<div class="grid fractional">
		<div id="flex_min"></div><div id="flex_track"></div><div id="flex_span">AAAAAAAA</div>
	</div>
	<div class="grid rows">
		<div id="row_min">A<br>B</div><div id="row_max">A<br>B</div>
		<div id="row_fit" class="relaxed">A<br>B</div><div id="row_auto">A<br>B</div>
	</div>
	<div class="grid implicit">
		<div id="implicit_min" style="grid-column: 2">AAAA BBBB</div>
		<div id="implicit_fit" style="grid-column: 3">AAAA BBBB</div>
		<div id="implicit_repeat" style="grid-column: 4">AAAA BBBB</div>
	</div>
	<div class="grid auto_repeat">
		<div id="repeat_first">AA</div><div>AA</div><div id="repeat_third">AA</div><div id="repeat_fourth">AA</div>
	</div>
	<div class="grid invalid"><div id="invalid"></div></div>
	<div class="grid bad_cap"><div id="bad_cap"></div></div><div id="condition"></div>
	"""
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(850, 900)) == OK, "Lay out intrinsic Grid track sizes")
	var ch: float = font.get_char_size(48, 20).x
	var widths: Dictionary[String, float] = {
		"min": 4.0,
		"max": 9.0,
		"fit": 6.0,
		"auto": 18.0,
		"floor": 4.0,
		"relaxed": 2.0,
		"scrollable": 2.0,
		"min_bound": 4.0,
		"max_bound": 9.0,
		"min_limit": 4.0,
		"fraction": 20.0,
		"percent": 10.0,
		"inline_percent": 14.0,
		"inline_track": 4.0,
		"cap_a": 2.0,
		"cap_b": 4.0,
		"floor_a": 2.5,
		"floor_b": 4.5,
		"span_min": 1.5,
		"span_max": 6.5,
		"flex_min": 0.0,
		"flex_track": 11.0,
		"implicit_min": 4.0,
		"implicit_fit": 6.0,
		"implicit_repeat": 4.0,
		"repeat_first": 5.0,
		"invalid": 4.0,
		"bad_cap": 4.0,
		"condition": 5.0,
	}
	for id: String in widths:
		var width: float = _rect(layout, id).size.x
		_check(absf(width - widths[id] * ch) < 0.15, "Grid %s width is %s, expected %s" % [id, width, widths[id] * ch])
	_check(_rect(layout, "repeat_third").position.y == _rect(layout, "repeat_first").position.y and
		_rect(layout, "repeat_fourth").position.y > _rect(layout, "repeat_first").position.y,
		"Auto-repeat accepts an intrinsic maximum with a fixed minimum")
	var heights: Dictionary[String, float] = {"row_min": 48.0, "row_max": 48.0, "row_fit": 36.0, "row_auto": 68.0,}
	for id: String in heights:
		var height: float = _rect(layout, id).size.y
		_check(absf(height - heights[id]) < 0.15, "Grid %s height is %s, expected %s" % [id, height, heights[id]])
	_check(_has_diagnostic(layout, "grid-template-columns", "repeat(auto-fit , max-content)", "style_element") and
		_has_diagnostic(layout, "grid-template-columns", "fit-content(-10px)", "style_element"),
		"Invalid intrinsic track syntax leaves the valid declaration in place: %s" % [layout.get_css_diagnostics()])


func _check_subgrid() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """
<!doctype html><style>
body { margin: 0; }
#outer { display:grid; grid-template:20px 20px / [left] auto [middle] 1fr [right]; column-gap:10px; }
#track_measure { grid-column:1; grid-row:1; }
#shared { display:grid; grid-column:1 / -1; grid-row:2; --columns:subgrid [local]; grid-template-columns:var(--columns);
  grid-template-rows:20px; column-gap:20px; padding:0 7px; width:1px; max-width:2px; justify-self:end; }
#contribution { grid-column:left / middle; width:68px; }
#align_track { grid-column:middle / right; background:#12643f; }
#rows { display:grid; width:160px; grid-template:[first] auto [second] auto [end] / 50px 1fr; row-gap:8px; }
#shared_both { display:grid; grid-template:subgrid / subgrid; grid-area:1 / 1 / -1 / -1; row-gap:18px; }
#first_row { grid-row:first / second; height:29px; }
#nested_both { display:grid; grid:subgrid / subgrid; grid-area:second / 1 / end / -1; height:1px; align-self:end; }
#second_row { grid-row:-1 / span 2; grid-column:2; height:37px; }
#names { display:grid; grid-template:12px / [parent_start] 30px [parent_middle] 40px [parent_next] 50px 60px; }
#names_shared { display:grid; grid-column:1 / -1; grid-template-columns:subgrid [front] repeat(2,[named]) repeat(auto-fill,[spare]) [tail]; }
#from_parent { grid-column:parent_middle / parent_next; grid-row:1; }
#local_names { grid-column:2 named / spare; grid-row:1; }
#clamped { grid-column:99 / span 4; grid-row:1; }
#fallback { display:grid; grid-template-columns:subgrid [unused]; grid-auto-columns:24px 36px; }
#standalone { grid-column:2; height:10px; }
#areas { display:grid; grid-template:". promo promo promo" 10px / 20px 30px 40px 50px; }
#area_shared { display:grid; grid-column:1 / 4; grid-template-columns:subgrid; }
#from_area { grid-column:promo; height:10px; }
#edge_gap { display:inline-grid; grid-template-columns:auto auto; column-gap:10px; }
#edge_shared { display:grid; grid-column:1 / -1; grid-template-columns:subgrid; padding-left:20px; }
#edge_content { grid-column:2; width:50px; height:10px; }
#fit_parent { display:grid; width:200px; grid-template-columns:repeat(auto-fit,[repeated] 40px); column-gap:10px; }
#fit_shared { display:grid; grid-column:2 / 4; grid-template-columns:subgrid; }
#fit_cell { grid-column:2 repeated / -1; height:10px; }
#image_parent { display:grid; grid-template-columns:80px; }
#grid_image { display:grid; grid-template-columns:subgrid; width:12px; height:10px; }
@supports (grid-template-columns: subgrid repeat(auto-fit,[invalid])) {
  #clamped { grid-column:1; }
}
</style>
<div id="outer"><div id="track_measure"></div><div id="shared">
  <div id="contribution"></div><div id="align_track"></div></div></div>
<div id="rows"><div id="shared_both"><div id="first_row"></div>
  <div id="nested_both"><a id="second_row" href="#outer">Second</a></div></div></div>
<div id="names"><div id="names_shared"><div id="from_parent"></div><div id="local_names"></div><div id="clamped"></div></div></div>
<div id="fallback"><div id="standalone"></div></div>
<div id="areas"><div id="area_shared"><div id="from_area"></div></div></div>
<div><div id="edge_gap"><div id="edge_shared"><div id="edge_content"></div></div></div></div>
<div id="fit_parent"><div id="fit_shared"><div id="fit_cell"></div></div></div>
<div id="image_parent"><img id="grid_image" alt="Image"></div>
"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(200, 240)) == OK, "Lay out subgrid fixtures")
	_check(_rect(layout, "track_measure").is_equal_approx(Rect2(0.0, 0.0, 80.0, 20.0))
		and _rect(layout, "shared").is_equal_approx(Rect2(0.0, 20.0, 200.0, 20.0)),
		"Child width, edge padding and half the gap difference contribute to the parent track")
	_check(_rect(layout, "contribution").is_equal_approx(Rect2(7.0, 20.0, 68.0, 20.0))
		and _rect(layout, "align_track").is_equal_approx(Rect2(95.0, 20.0, 98.0, 20.0)),
		"Local gaps are centered on the inherited gutters")
	_check(_rect(layout, "rows").is_equal_approx(Rect2(0.0, 40.0, 160.0, 84.0))
		and _rect(layout, "nested_both").is_equal_approx(Rect2(0.0, 87.0, 160.0, 37.0))
		and _rect(layout, "second_row").is_equal_approx(Rect2(50.0, 87.0, 110.0, 37.0)),
		"Nested descendants contribute to auto row sizing after inheriting the solved columns")
	_check(layout.get_link_at(Vector2(60.0, 95.0)) == "#outer", "Hit testing uses the shared track geometry")
	var second_text: int = layout.get_plain_text().find("Second")
	_check(second_text >= 0 and not layout.get_selection_rects(second_text, second_text + 6).is_empty(),
		"Subgrid leaves its text selectable at the inherited track position")
	_check(is_equal_approx(_rect(layout, "from_parent").position.x, 30.0)
		and is_equal_approx(_rect(layout, "local_names").position.x, 70.0)
		and is_equal_approx(_rect(layout, "local_names").size.x, 50.0)
		and is_equal_approx(_rect(layout, "clamped").position.x, 120.0)
		and is_equal_approx(_rect(layout, "clamped").size.x, 60.0),
		"Inherited and repeated line names resolve within the subgrid, and overflow placement clamps to its tracks")
	_check(is_equal_approx(_rect(layout, "standalone").position.x, 24.0)
		and is_equal_approx(_rect(layout, "standalone").size.x, 36.0),
		"A grid without a grid parent falls back to standalone implicit tracks")
	_check(is_equal_approx(_rect(layout, "from_area").position.x, 20.0)
		and is_equal_approx(_rect(layout, "from_area").size.x, 70.0),
		"Named parent areas are clipped to the portion spanned by the subgrid")
	_check(is_equal_approx(_rect(layout, "edge_gap").size.x, 70.0),
		"Padding on an empty outer track contributes across the neighboring occupied track")
	_check(is_equal_approx(_rect(layout, "fit_shared").size.x, 90.0)
		and is_equal_approx(_rect(layout, "fit_cell").position.x, 50.0)
		and is_equal_approx(_rect(layout, "fit_cell").size.x, 40.0),
		"Auto-fit retains the names and positions of the tracks spanned by a subgrid")
	_check(is_equal_approx(_rect(layout, "grid_image").size.x, 12.0),
		"An image remains a replaced box when its display value names a grid")
	_check(layout.get_css_diagnostics().is_empty(), "Valid subgrid grammar passes CSS diagnostics")
	_check(layout.layout(Vector2i(180, 240)) == OK and is_equal_approx(_rect(layout, "align_track").size.x, 78.0),
		"Resizing the parent updates the adopted track geometry")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(200, 240)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(200.0, 240.0)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), Vector2(110.0, 25.0), Color("#12643f"),
		"HTMLLabel paints the item in the adopted track")
	viewport.queue_free()


func _check_subgrid_baselines() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """
<!doctype html><style>
body { margin:0; }
#vertical { display:grid; grid-template: auto auto / 30px 30px 30px; row-gap:12px; }
#outside_first { grid-area:1 / 1; align-self:first baseline; height:35px; }
#outside_last { grid-area:2 / 1; align-self:last baseline; height:33px; }
#rows_shared { display:grid; grid-template:subgrid / subgrid; grid-area:1 / 2 / -1 / -1;
  padding:6px 0 8px; row-gap:20px; }
#inside_first { grid-area:1 / 1; align-self:first baseline; height:20px; }
#inside_last { grid-area:2 / 1; align-self:last baseline; height:16px; }
#deep_shared { display:grid; grid-template:subgrid / subgrid; grid-area:1 / 2 / -1 / -1;
  padding:2px 0 3px; }
#deep_first { grid-area:1 / 1; align-self:first baseline; height:31px; }
#deep_last { grid-area:2 / 1; align-self:last baseline; height:11px; }
#horizontal { display:grid; grid-template:20px 20px / 100px; }
#outside_column { grid-row:1; justify-self:last baseline; width:30px; }
#columns_shared { display:grid; grid-row:2; grid-template-columns:subgrid; padding:0 8px 0 5px; }
#inside_column { justify-self:last baseline; width:10px; height:10px; }
#cyclic { display:grid; grid-template-columns:30px 30px; }
#outside_cyclic { grid-area:1 / 1; align-self:first baseline; height:40px; }
#cyclic_shared { display:grid; grid-area:1 / 2; grid-template-rows:subgrid; }
#inside_cyclic { align-self:first baseline; height:50%; }
#text_grid { display:grid; grid-template:10px auto 10px / 70px 70px; row-gap:6px; }
#text_outer { grid-area:2 / 1; align-self:first baseline; font:16px/23px sans-serif; padding-bottom:24px; }
#text_shared { display:grid; grid-area:1 / 2 / -1; grid-template-rows:subgrid; row-gap:14px; }
#text_inner { grid-row:2; align-self:first baseline; font:28px/35px sans-serif; }
.baseline_marker { display:inline-block; width:1px; height:1px; vertical-align:baseline; }
</style>
<div id="vertical">
	<div id="outside_first"></div><div id="outside_last"></div>
	<div id="rows_shared">
		<div id="inside_first"></div><div id="inside_last"></div>
		<div id="deep_shared"><div id="deep_first"></div><div id="deep_last"></div></div>
	</div>
</div>
<div id="horizontal">
	<div id="outside_column"></div>
	<div id="columns_shared"><div id="inside_column"></div></div>
</div>
<div id="cyclic">
	<div id="outside_cyclic"></div><div id="cyclic_shared"><div id="inside_cyclic"></div></div>
</div>
<div id="text_grid">
	<div id="text_outer"><span id="outer_marker" class="baseline_marker"></span>Small</div>
	<div id="text_shared"><div id="text_inner"><span id="inner_marker" class="baseline_marker"></span>Large</div></div>
</div>
"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(180, 300)) == OK, "Lay out shared baseline fixtures")
	var first: Rect2 = _rect(layout, "outside_first")
	var last: Rect2 = _rect(layout, "outside_last")
	_check(is_equal_approx(first.position.y, 4.0)
		and is_equal_approx(first.end.y, _rect(layout, "inside_first").end.y)
		and is_equal_approx(first.end.y, _rect(layout, "deep_first").end.y),
		"The deepest first baseline moves its ancestor's neighboring grid item")
	_check(is_equal_approx(last.position.y, 55.0)
		and is_equal_approx(last.end.y, _rect(layout, "inside_last").end.y)
		and is_equal_approx(last.end.y, _rect(layout, "deep_last").end.y)
		and is_equal_approx(_rect(layout, "vertical").size.y, 99.0),
		"Last baselines share the trailing track edge and increase its intrinsic size")
	_check(is_equal_approx(_rect(layout, "outside_column").position.x, 70.0)
		and is_equal_approx(_rect(layout, "inside_column").position.x, 70.0),
		"Last column baselines include subgrid edge padding and share a horizontal position")
	_check(is_equal_approx(_rect(layout, "inside_cyclic").position.y, _rect(layout, "cyclic").position.y)
		and is_equal_approx(_rect(layout, "inside_cyclic").size.y, 20.0),
		"A percentage-height item uses start fallback when its inherited track depends on intrinsic sizing")
	var outer_marker: Rect2 = _rect(layout, "outer_marker")
	var inner_marker: Rect2 = _rect(layout, "inner_marker")
	_check(outer_marker.has_area() and inner_marker.has_area() and is_equal_approx(outer_marker.end.y, inner_marker.end.y),
		"Different font sizes align their text baselines across an adjusted interior gutter")


func _check_grid_cross_axis_sizing() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """
<!doctype html><style>
body { margin:0; }
.ratio { height:100%; aspect-ratio:2 / 1; }
#fixed { display:inline-grid; grid-template:minmax(0,40px) / max-content; }
#distributed { display:inline-grid; height:100px; grid-template:20px 20px / max-content;
  row-gap:10px; align-content:space-between; }
#distributed_ratio { grid-row:1 / -1; }
#resized { display:grid; width:150px; grid-template-columns:max-content minmax(0,1fr); }
#resized > * { grid-row:1; }
#wrapping, #height_anchor { grid-column:2; }
#height_anchor { align-self:start; height:30px; }
#wrapping { display:flex; flex-wrap:wrap; align-self:start; }
#wrapping > i { flex:0 0 40px; height:20px; }
#column_wrap { display:inline-grid; grid-template:minmax(0,1fr) / max-content 50px; height:45px; }
#vertical_flex { display:flex; flex-flow:column wrap; column-gap:4px; height:100%; }
#vertical_flex > i { flex:0 0 20px; width:20px; }
#vertical_height { height:45px; }
#shared_rows { display:inline-grid; grid-template-columns:max-content 20px; }
#shared_row_grid { display:grid; grid-template:subgrid / max-content; }
#shared_height { height:36px; }
#shared_columns { display:inline-grid; grid-template-columns:max-content 20px; }
#shared_column_grid { display:grid; grid-template:1fr / subgrid; height:100%; }
#shared_col_height { height:30px; }
#interlaced { display:inline-grid; grid-template:30px / max-content 20px; }
#both_axes { display:grid; grid-template:subgrid / subgrid; }
#interlaced_cols { display:grid; grid-template:1fr / subgrid; height:100%; }
</style>
<div><div id="fixed"><div class="ratio" id="fixed_ratio"></div></div></div>
<div id="resized">
	<div class="ratio" id="resized_ratio"></div>
	<div id="wrapping"><i></i><i></i><i></i></div><div id="height_anchor"></div>
</div>
<div><div id="column_wrap">
	<div id="vertical_flex"><i></i><i></i><i></i></div><div id="vertical_height"></div>
</div></div>
<div><div id="shared_rows">
	<div id="shared_row_grid"><div class="ratio" id="shared_ratio"></div></div><div id="shared_height"></div>
</div></div>
<div><div id="shared_columns">
	<div id="shared_column_grid"><div class="ratio" id="shared_col_ratio"></div></div><div id="shared_col_height"></div>
</div></div>
<div><div id="distributed"><div class="ratio" id="distributed_ratio"></div></div></div>
<div><div id="interlaced">
	<div id="both_axes"><div id="interlaced_cols"><div class="ratio" id="interlaced_ratio"></div></div></div><div id="interlaced_marker"></div>
</div></div>
"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(250, 330)) == OK, "Lay out cross-axis sizing fixtures")
	_check(is_equal_approx(_rect(layout, "fixed").size.x, 80.0)
		and is_equal_approx(_rect(layout, "fixed_ratio").size.x, 80.0),
		"Intrinsic column sizing uses a row with a definite maximum in its first pass")
	_check(is_equal_approx(_rect(layout, "wrapping").position.x, 60.0)
		and is_equal_approx(_rect(layout, "wrapping").size.y, 40.0)
		and is_equal_approx(_rect(layout, "resized_ratio").size.x, 80.0),
		"Columns use the first row height, then rows use the new columns without repeating the cycle")
	_check(is_equal_approx(_rect(layout, "vertical_height").position.x, 44.0)
		and is_equal_approx(_rect(layout, "vertical_flex").size.x, 44.0),
		"A wrapping column flex container contributes its width at the resolved row height")
	_check(is_equal_approx(_rect(layout, "shared_height").position.x, 72.0)
		and is_equal_approx(_rect(layout, "shared_ratio").size.x, 72.0),
		"A row subgrid sizes its standalone columns using its adopted row height")
	_check(is_equal_approx(_rect(layout, "shared_col_height").position.x, 60.0)
		and is_equal_approx(_rect(layout, "shared_col_ratio").size.x, 60.0),
		"A column subgrid resolves its standalone rows before returning revised contributions")
	_check(is_equal_approx(_rect(layout, "distributed").size.x, 200.0),
		"The first pass includes distributed space between definite spanned rows")
	_check(is_equal_approx(_rect(layout, "interlaced_marker").position.x, 60.0)
		and is_equal_approx(_rect(layout, "interlaced_ratio").size.x, 60.0),
		"Resolved row and column bases pass through a two-axis subgrid to a standalone row")
	layout.get_element_by_id("shared_col_height").set_attribute("style", "height:34px")
	_check(layout.layout(Vector2i(250, 330)) == OK and is_equal_approx(_rect(layout, "shared_col_ratio").size.x, 68.0),
		"Reflow calculates cross-axis contributions again after a height change")


func _check_intrinsic_box_widths() -> void:
	var font_fixture: HTMLDocument = load("{FONT_FACE_UID}")
	_check(font_fixture != null, "Load the intrinsic sizing font fixture")
	if font_fixture == null:
		return
	var assets: Dictionary = font_fixture.get("_local_fonts")
	var font: FontFile = assets["res://fonts/cour.ttf"]
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", assets)
	document.text = """<!doctype html><style>
		@font-face { font-family: MeasureMono; src: url(fonts/cour.ttf); }
		body { margin: 0; font: 20px / 24px MeasureMono; }
		.min { width: min-content; }
		.max { width: max-content; }
		.fit { width: fit-content; }
		.narrow { width: 6ch; }
		.medium { width: 12ch; }
		table { border-spacing: 0; font: inherit; }
		td { padding: 0; }
		@supports (inline-size: stretch(70%)) { #logical { inline-size: max-content !important; } }
	</style>
	<div id="min" class="min">AAAA BBBB</div>
	<div id="max" class="max">AAAA BBBB</div>
	<div class="narrow"><div id="fit" class="fit">AAAA BBBB</div></div>
	<div style="width: 2ch"><div id="fit_floor" class="fit">AAAA BBBB</div></div>
	<div id="fit_ceiling" class="fit">AAAA BBBB</div>
	<div id="fit_limit" style="--cap: calc(4ch + 2ch); width: fit-content(var(--cap))">AAAA BBBB</div>
	<div class="medium"><div id="fit_percent" style="width: fit-content(50%)">AAAA BBBB</div></div>
	<div id="min_bound" style="width: 1ch; min-width: max-content">AAAA BBBB</div>
	<div id="max_bound" style="width: 10ch; max-width: min-content">AAAA BBBB</div>
	<div id="both_bounds" style="width: 10ch; min-width: max-content; max-width: min-content">AAAA BBBB</div>
	<div id="border_box" class="max" style="box-sizing: border-box; padding: 0 1ch; border: 2px solid">AAAA BBBB</div>
	<div class="medium"><div id="stretch" style="width: stretch; margin: 0 1ch; padding: 0 1ch">AAAA BBBB</div></div>
	<div class="medium"><div id="stretch_limit" style="width: stretch(75%); margin: 0 1ch; padding: 0 1ch">AAAA BBBB</div></div>
	<div id="fit_outlines" style="width: fit-content(9ch); margin: 0 1ch; padding: 0 1ch">AAAA BBBB</div>
	<div id="percentage_child" class="max"><div style="width: 50%">AAAA BBBB</div></div>
	<div id="percentage_minimum" class="max"><div style="min-width: calc(50% + 1ch)">AAAA BBBB</div></div>
	<div id="flex_min" class="min" style="display: flex; flex-wrap: wrap; gap: 1ch">
		<div>AAAA</div><div>BBBBB</div></div>
	<div id="grid_min" class="min" style="display: grid; grid-template-columns: auto auto; gap: 1ch">
		<div>AAAA</div><div>BBBBB</div></div>
	<div class="narrow" style="display: flex"><div id="flex_shrink" class="max">AAAA BBBB</div></div>
	<div class="medium" style="display: flex"><div id="flex_basis_max" class="min" style="flex: 0 0 max-content">AAAA BBBB</div></div>
	<div class="medium" style="display: flex">
		<div id="flex_bounds" style="width: 4ch; min-width: max-content; max-width: min-content">AAAA BBBB</div></div>
	<div class="medium" style="display: flex; flex-direction: column">
		<div id="flex_column_item" class="fit">AAAA BBBB</div></div>
	<div style="display: grid; grid-template-columns: 12ch">
		<div id="grid_item" class="fit">AAAA BBBB</div></div>
	<div style="display: grid; grid-template-columns: min-content">
		<div class="max" style="display: grid">AAAA BBBB</div><div id="grid_contribution">probe</div></div>
	<table id="table_min" class="min"><tr><td>AAAA BBBB</td></tr></table>
	<table id="table_max" class="max"><tr><td>AAAA BBBB</td></tr></table>
	<table id="table_max_bound" style="width: 14ch; max-width: max-content"><tr><td>AAAA BBBB</td></tr></table>
	<table id="table_min_bound" style="width: 4ch; min-width: max-content"><tr><td>AAAA BBBB</td></tr></table>
	<div id="logical" style="width: 2ch; max-inline-size: 6ch; --minimum: 5ch; min-inline-size: var(--minimum);
		min-block-size: 55px; height: 20px">AAAA BBBB</div>
	<div id="logical_order" style="inline-size: max-content; width: 7ch; block-size: 66px; max-block-size: 41px">AAAA BBBB</div>
	<div style="width: 50%"><div id="responsive" class="fit">AAAA BBBB</div></div>
	<div id="viewport_measure" class="max"><div style="width: 40vw; height: 1px"></div></div>
	<div class="medium" style="position: relative; height: 100px">
		<div id="positioned_max" class="max" style="position: absolute; left: 3ch; right: 3ch; top: 0">AAAA BBBB</div>
		<div id="positioned_fit" class="fit" style="position: absolute; left: 3ch; right: 3ch; bottom: 0">AAAA BBBB</div>
		<div id="positioned_cap" style="position: absolute; left: 0; right: 0; top: 24px; max-width: min-content">AAAA BBBB</div>
		<div id="positioned_percent" style="position: absolute; left: 3ch; right: 3ch; top: 0; width: stretch(75%)">AAAA BBBB</div>
	</div>
	<img id="image_min" src="intrinsic-image.png" style="width: min-content">
	<div class="narrow" style="display: flex">
		<img id="image_flex" src="intrinsic-image.png" style="width: max-content; min-width: 0"></div>
	<div id="invalid" style="width: 5ch; width: fit-content(-1ch)">AAAA BBBB</div>"""
	document.text += '<div id="deep" class="max">' + '<div class="max">'.repeat(24) + "AAAA BBBB" + "</div>".repeat(25)
	var image: Image = Image.create_empty(120, 60, false, Image.FORMAT_RGBA8)
	var texture: ImageTexture = ImageTexture.create_from_image(image)
	document.set("_textures", {"res://intrinsic-image.png": texture,})
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(600, 800)) == OK, "Lay out intrinsic box widths")
	var ch: float = font.get_char_size(48, 20).x
	var widths: Dictionary[String, float] = {
		"min": 4.0 * ch,
		"max": 9.0 * ch,
		"fit": 6.0 * ch,
		"fit_floor": 4.0 * ch,
		"fit_ceiling": 9.0 * ch,
		"fit_limit": 6.0 * ch,
		"fit_percent": 6.0 * ch,
		"min_bound": 9.0 * ch,
		"max_bound": 4.0 * ch,
		"both_bounds": 9.0 * ch,
		"border_box": 11.0 * ch + 4.0,
		"stretch": 10.0 * ch,
		"stretch_limit": 7.0 * ch,
		"fit_outlines": 7.0 * ch,
		"percentage_child": 9.0 * ch,
		"percentage_minimum": 9.0 * ch,
		"flex_min": 5.0 * ch,
		"grid_min": 10.0 * ch,
		"flex_shrink": 6.0 * ch,
		"flex_basis_max": 9.0 * ch,
		"flex_bounds": 9.0 * ch,
		"flex_column_item": 9.0 * ch,
		"grid_item": 9.0 * ch,
		"grid_contribution": 9.0 * ch,
		"table_min": 4.0 * ch,
		"table_max": 9.0 * ch,
		"table_max_bound": 9.0 * ch,
		"table_min_bound": 9.0 * ch,
		"logical": 6.0 * ch,
		"logical_order": 7.0 * ch,
		"responsive": 9.0 * ch,
		"viewport_measure": 240.0,
		"positioned_max": 9.0 * ch,
		"positioned_fit": 6.0 * ch,
		"positioned_cap": 4.0 * ch,
		"positioned_percent": 9.0 * ch,
		"image_min": 120.0,
		"image_flex": 6.0 * ch,
		"invalid": 5.0 * ch,
		"deep": 9.0 * ch,
	}
	for id: String in widths:
		var actual: float = _rect(layout, id).size.x
		_check(absf(actual - widths[id]) <= 0.1, "Intrinsic width for %s is %s, expected %s" % [id, actual, widths[id]])
	_check(is_equal_approx(_rect(layout, "min").size.y, 48.0), "Min-content wraps at the space")
	_check(is_equal_approx(_rect(layout, "max").size.y, 24.0), "Max-content keeps both words on one line")
	_check(is_equal_approx(_rect(layout, "logical").size.y, 55.0), "Logical minimum block size overrides the smaller height")
	_check(is_equal_approx(_rect(layout, "logical_order").size.y, 41.0), "Logical maximum block size caps block-size")
	_check(is_equal_approx(_rect(layout, "positioned_fit").size.y, 48.0), "Positioned fit-content wraps within the insets")
	_check(is_equal_approx(_rect(layout, "image_flex").size.y, 3.0 * ch), "Flex shrinking preserves the image's natural ratio")
	_check(_has_diagnostic(layout, "width", "fit-content(-1ch)", "inline_style"), "Report a negative fit-content limit")
	_check(layout.layout(Vector2i(144, 800)) == OK, "Reflow intrinsic boxes after a viewport resize")
	_check(is_equal_approx(_rect(layout, "responsive").size.x, 72.0), "Fit-content tracks the resized containing block")
	_check(is_equal_approx(_rect(layout, "viewport_measure").size.x, 57.6), "Relayout remeasures changed viewport units")


func _check_aspect_ratio() -> void:
	var document: HTMLDocument = HTMLDocument.new()
	var image: Image = Image.create_empty(120, 60, false, Image.FORMAT_RGBA8)
	var texture: ImageTexture = ImageTexture.create_from_image(image)
	document.set("_textures", {"res://ratio-source.png": texture,})
	document.text = """<!doctype html><style>
		body { margin: 0; }
		.stage { width: 160px; }
		.grid { display: grid; grid-template-columns: 120px; }
		.flex { display: flex; align-items: start; }
		img { display: block; }
		@supports (aspect-ratio: auto calc(4 + 2) / 3) {
			#math { aspect-ratio: var(--ratio); }
		}
	</style>
	<div id="from_width" style="width: 120px; aspect-ratio: 2 / 1"></div>
	<div id="from_height" style="height: 45px; aspect-ratio: 2"></div>
	<div class="stage"><div id="fill" style="aspect-ratio: 1.6"></div></div>
	<div id="both" style="width: 80px; height: 55px; aspect-ratio: 2"></div>
	<div id="border" style="width: 120px; box-sizing: border-box; padding: 6px; border: 4px solid; aspect-ratio: 2"></div>
	<div id="content" style="width: 120px; padding: 6px; border: 4px solid; aspect-ratio: 2"></div>
	<div id="auto_border" style="width: 120px; box-sizing: border-box; padding: 10px; aspect-ratio: auto 2"></div>
	<div id="math" style="width: 120px; --ratio: calc(4 + 2) / 3 auto"></div>
	<div id="zero" style="width: 120px; aspect-ratio: 10 / 0"><div style="height: 10px"></div></div>
	<div id="invalid" style="width: 120px; aspect-ratio: 2; aspect-ratio: -3 / 1"></div>
	<div id="wide" style="height: 25px; aspect-ratio: 2"><div style="width: 130px; height: 10px"></div></div>
	<div id="wide_zero" style="height: 25px; aspect-ratio: 2; min-width: 0"><div style="width: 130px; height: 10px"></div></div>
	<div id="tall" style="width: 100px; aspect-ratio: 2"><div style="height: 90px"></div></div>
	<div id="hidden" style="width: 100px; aspect-ratio: 2; overflow: hidden"><div style="height: 90px"></div></div>
	<div id="tall_zero" style="width: 100px; aspect-ratio: 2; min-height: 0"><div style="height: 90px"></div></div>
	<div class="stage"><div id="max_height" style="aspect-ratio: 2; max-height: 35px"></div></div>
	<div id="min_width" style="height: 25px; aspect-ratio: 2; min-width: 80px"></div>
	<div style="height: 100px"><div id="percent_height" style="height: 50%; aspect-ratio: 3"></div></div>
	<div style="width: 100px; height: 100px; aspect-ratio: 2">
		<div id="inherit" style="height: 20px; aspect-ratio: inherit"></div></div>
	<span id="inline_block" style="display: inline-block; width: 80px; aspect-ratio: 2"></span>
	<div class="grid" style="grid-template-rows: 180px 60px">
		<div id="grid_normal" style="aspect-ratio: 2"></div>
		<div id="grid_fixed_height" style="height: 40px; aspect-ratio: 2"></div>
	</div>
	<div class="grid" id="grid_container" style="width: 150px; aspect-ratio: 3; align-content: start"><div></div></div>
	<div class="flex" style="width: 60px"><div id="flex_shrink" style="width: 100px; aspect-ratio: 2"></div></div>
	<div class="flex"><div id="flex_height" style="height: 40px; aspect-ratio: 2"></div></div>
	<div style="display: flex; flex-direction: column; width: 120px"><div id="flex_column" style="aspect-ratio: 2"></div></div>
	<div class="stage" style="position: relative; height: 120px">
		<div id="absolute_height" style="position: absolute; bottom: 0; width: 80px; aspect-ratio: 2"></div>
		<div id="absolute_width" style="position: absolute; right: 0; height: 40px; aspect-ratio: 2"></div>
		<div id="absolute_four_insets" style="position: absolute; inset: 10px; aspect-ratio: 2"></div>
		<div id="absolute_vertical_insets" style="position: absolute; inset: 10px auto 10px 0; aspect-ratio: 2"></div>
	</div>
	<div id="fixed_insets" style="position: fixed; inset: 12px 460px auto 20px; aspect-ratio: 2"></div>
	<table id="table_ratio" style="width: 120px; aspect-ratio: 2; border-spacing: 0"><tr><td>X</td></tr></table>
	<div id="inline_grid_ratio" style="display: inline-grid; aspect-ratio: 2"><div style="width: 80px; height: 10px"></div></div>
	<div id="percentage_child" style="width: 120px; aspect-ratio: 2"><div id="ratio_child" style="height: 50%"></div></div>
	<img id="image_override" src="ratio-source.png" style="width: 80px; aspect-ratio: 1">
	<img id="image_natural" src="ratio-source.png" style="width: 80px; aspect-ratio: 1 auto">
	<img id="image_auto" src="ratio-source.png" style="aspect-ratio: 1">
	<img id="image_border" src="ratio-source.png" style="height: 80px; box-sizing: border-box; padding: 10px; aspect-ratio: 2">
	<img id="image_missing" src="missing.png" style="width: 80px; aspect-ratio: auto 2">
	<img id="image_inline" src="ratio-source.png" style="display: inline; width: 80px; aspect-ratio: 1">
	<img id="image_hint" src="missing.png" width="80" height="20" style="width: 120px; height: auto; aspect-ratio: revert">
	<img id="image_hint_natural" src="ratio-source.png" width="80" height="20" style="width: 120px; height: auto">
	<img id="image_hint_override" src="missing.png" width="80" height="20" style="width: 120px; height: auto; aspect-ratio: 1">
	<div class="flex" style="width: 200px"><img id="flex_image_auto" src="ratio-source.png" style="aspect-ratio: 1"></div>"""
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(600, 800)) == OK, "Lay out preferred aspect ratios")
	var sizes: Dictionary[String, Vector2] = {
		"from_width": Vector2(120.0, 60.0),
		"from_height": Vector2(90.0, 45.0),
		"fill": Vector2(160.0, 100.0),
		"both": Vector2(80.0, 55.0),
		"border": Vector2(120.0, 60.0),
		"content": Vector2(140.0, 80.0),
		"auto_border": Vector2(120.0, 70.0),
		"math": Vector2(120.0, 60.0),
		"zero": Vector2(120.0, 10.0),
		"invalid": Vector2(120.0, 60.0),
		"wide": Vector2(130.0, 25.0),
		"wide_zero": Vector2(50.0, 25.0),
		"tall": Vector2(100.0, 90.0),
		"hidden": Vector2(100.0, 50.0),
		"tall_zero": Vector2(100.0, 50.0),
		"max_height": Vector2(70.0, 35.0),
		"min_width": Vector2(80.0, 25.0),
		"percent_height": Vector2(150.0, 50.0),
		"inherit": Vector2(40.0, 20.0),
		"inline_block": Vector2(80.0, 40.0),
		"grid_normal": Vector2(120.0, 60.0),
		"grid_fixed_height": Vector2(80.0, 40.0),
		"grid_container": Vector2(150.0, 50.0),
		"flex_shrink": Vector2(60.0, 30.0),
		"flex_height": Vector2(80.0, 40.0),
		"flex_column": Vector2(120.0, 60.0),
		"absolute_height": Vector2(80.0, 40.0),
		"absolute_width": Vector2(80.0, 40.0),
		"absolute_four_insets": Vector2(140.0, 70.0),
		"absolute_vertical_insets": Vector2(200.0, 100.0),
		"fixed_insets": Vector2(120.0, 60.0),
		"table_ratio": Vector2(120.0, 60.0),
		"inline_grid_ratio": Vector2(80.0, 40.0),
		"ratio_child": Vector2(120.0, 30.0),
		"image_override": Vector2(80.0, 80.0),
		"image_natural": Vector2(80.0, 40.0),
		"image_auto": Vector2(120.0, 120.0),
		"image_border": Vector2(160.0, 80.0),
		"image_missing": Vector2(80.0, 40.0),
		"image_inline": Vector2(80.0, 80.0),
		"image_hint": Vector2(120.0, 30.0),
		"image_hint_natural": Vector2(120.0, 60.0),
		"image_hint_override": Vector2(120.0, 120.0),
		"flex_image_auto": Vector2(120.0, 120.0),
	}
	for id: String in sizes:
		var actual: Vector2 = _rect(layout, id).size
		_check(actual.distance_to(sizes[id]) <= 0.1, "Aspect ratio for %s produced %s, expected %s" % [id, actual, sizes[id]])
	_check(_has_diagnostic(layout, "aspect-ratio", "-3 / 1", "inline_style"), "Diagnose a negative literal ratio")
	var fill: HTMLElement = layout.get_element_by_id("fill")
	_check(fill.set_attribute("style", "aspect-ratio: 2") == OK and layout.layout(Vector2i(600, 800)) == OK,
		"Reflow after editing the preferred aspect ratio")
	_check(fill.get_bounding_rect().size.is_equal_approx(Vector2(160.0, 80.0)), "A ratio edit updates the existing element")


func _check_object_sizing() -> void:
	var red: Color = Color("#ef3340")
	var blue: Color = Color("#3258df")
	var gold: Color = Color("#ffbc36")
	var green: Color = Color("#1eb677")
	var background: Color = Color("#493457")
	var source: Image = Image.create_empty(40, 20, false, Image.FORMAT_RGBA8)
	source.fill_rect(Rect2i(0, 0, 20, 10), red)
	source.fill_rect(Rect2i(20, 0, 20, 10), blue)
	source.fill_rect(Rect2i(0, 10, 20, 10), gold)
	source.fill_rect(Rect2i(20, 10, 20, 10), green)
	var texture: ImageTexture = ImageTexture.create_from_image(source)
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_textures", { "res://object.png": texture, })
	document.text = """<!doctype html><style>
		body { margin: 0; display: grid; grid-template-columns: repeat(6, 82px); grid-auto-rows: 72px;
			align-items: start; justify-items: start; }
		img { display: block; width: 40px; height: 40px; background: #493457; }
		.large { width: 60px; height: 40px; }
		#upscale { object-fit: cover; }
		@supports (object-fit: scale-down cover) and (object-position: bottom 2px right 10%) {
			#upscale { object-fit: scale-down cover; }
		}
	</style>
	<img id="fill" src="object.png" style="width: 20px">
	<img id="contain" src="object.png" style="object-fit: contain">
	<img id="cover" src="object.png" style="object-fit: cover">
	<img id="cover_left" src="object.png" style="object-fit: cover; object-position: left">
	<img id="cover_right" src="object.png" style="object-fit: cover; object-position: top right">
	<img id="down_large" class="large" src="object.png" style="object-fit: scale-down">
	<img id="down_small" src="object.png" style="width: 20px; height: 20px; object-fit: contain scale-down">
	<img id="upscale" class="large" src="object.png">
	<img id="none" class="large" src="object.png" style="object-fit: none; object-position: 1em 25%; font-size: 12px">
	<img id="edge" class="large" src="object.png" style="height: 46px; object-fit: none;
		--position: bottom calc(4px + 2px) right 25%; object-position: var(--position)">
	<img id="negative_space" src="object.png" style="object-fit: cover; object-position: calc(100% - 4px) center">
	<img id="fill_offset" src="object.png" style="object-position: 6px 0">
	<img id="rounded" src="object.png" style="object-fit: cover; border-radius: 15px">
	<img id="flipped" src="object.png" style="object-fit: cover; transform: scaleX(-1)">
	<img id="padded" src="object.png" style="padding: 5px; border: 3px solid black; object-fit: contain">
	<img id="invalid" src="object.png" style="object-fit: contain; object-fit: fill scale-down;
		object-position: bottom; object-position: right 3px bottom">
	<img id="box_ratio" src="object.png" style="height: auto; aspect-ratio: 1; object-fit: contain">
	<img id="none_crop" src="object.png" style="width: 20px; height: 10px; object-fit: none; object-position: left bottom">
	<div id="window" style="width: 28px; height: 28px; overflow: hidden; border-radius: 9px">
		<img src="object.png" style="object-fit: cover">
	</div>"""
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(520, 345)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(520.0, 260.0)
	label.document = document
	viewport.add_child(label)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(0.0, 264.0)
	markdown.size = Vector2(220.0, 75.0)
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken"]:
		markdown.add_theme_icon_override(icon, texture)
	markdown.parse_html = true
	markdown.text = """
<div><img src="object-fit.svg" style="display: block; width: 40px; height: 40px;
  object-fit: contain; object-position: bottom; background: #493457"></div>
"""
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var painted: Image = viewport.get_texture().get_image()
	var samples: Array[Array] = [
		["fill", Vector2(4.0, 5.0), red],
		["fill", Vector2(15.0, 32.0), green],
		["contain", Vector2(4.0, 5.0), background],
		["contain", Vector2(4.0, 14.0), red],
		["cover", Vector2(5.0, 5.0), red],
		["cover", Vector2(32.0, 5.0), blue],
		["cover_left", Vector2(30.0, 5.0), red],
		["cover_right", Vector2(5.0, 5.0), blue],
		["down_large", Vector2(4.0, 13.0), background],
		["down_large", Vector2(12.0, 13.0), red],
		["down_small", Vector2(4.0, 2.0), background],
		["down_small", Vector2(4.0, 7.0), red],
		["upscale", Vector2(5.0, 14.0), background],
		["upscale", Vector2(14.0, 14.0), red],
		["none", Vector2(10.0, 8.0), background],
		["none", Vector2(14.0, 8.0), red],
		["edge", Vector2(13.0, 24.0), background],
		["edge", Vector2(18.0, 24.0), red],
		["edge", Vector2(18.0, 42.0), background],
		["negative_space", Vector2(2.0, 5.0), blue],
		["negative_space", Vector2(38.0, 5.0), background],
		["fill_offset", Vector2(3.0, 5.0), background],
		["fill_offset", Vector2(9.0, 5.0), red],
		["rounded", Vector2(10.0, 10.0), red],
		["flipped", Vector2(8.0, 5.0), blue],
		["padded", Vector2(5.0, 20.0), background],
		["padded", Vector2(12.0, 20.0), red],
		["invalid", Vector2(4.0, 16.0), background],
		["invalid", Vector2(4.0, 23.0), red],
		["box_ratio", Vector2(4.0, 5.0), background],
		["box_ratio", Vector2(4.0, 14.0), red],
		["none_crop", Vector2(6.0, 4.0), gold],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var expected: Color = sample[2]
		_check_pixel(painted, _label_rect(label, id).position + offset, expected, "Object paint for %s at %s" % [id, offset])
	_check(_label_rect(label, "box_ratio").size.is_equal_approx(Vector2(40.0, 40.0)),
		"Fitting an object preserves the ratio-sized element box")
	var rounded_corner: Vector2i = Vector2i(_label_rect(label, "rounded").position + Vector2.ONE)
	_check(not painted.get_pixelv(rounded_corner).is_equal_approx(red), "Rounded clipping masks an overflowing object corner")
	var window: Vector2 = _label_rect(label, "window").position
	_check_pixel(painted, window + Vector2(8.0, 8.0), red, "The fitted image draws inside an ancestor's rounded clip")
	_check(not _same_color(painted.get_pixelv(Vector2i(window + Vector2(32.0, 10.0))), blue),
		"The ancestor clips the image outside its box")
	var markdown_image: Rect2i = _color_bounds(painted, Rect2i(0, 264, 220, 75), Color("#c5692b"))
	_check(markdown_image.size == Vector2i(40, 20), "Markdown HTML fits the imported SVG at its natural proportions")
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(520, 260)) == OK, "Lay out the object diagnostics fixture")
	_check(_has_diagnostic(layout, "object-fit", "fill scale-down", "inline_style"), "Report an invalid fit combination")
	_check(_has_diagnostic(layout, "object-position", "right 3px bottom", "inline_style"), "Reject a three-component position")
	var cover: HTMLElement = label.get_element_by_id("cover_left")
	var before: Rect2 = _label_rect(label, "cover_left")
	_check(cover.set_attribute("style", "object-fit: cover; object-position: right") == OK, "Update an image's object position")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check(_label_rect(label, "cover_left").is_equal_approx(before), "Changing object position preserves the element bounds")
	_check_pixel(viewport.get_texture().get_image(), before.position + Vector2(30.0, 5.0), blue,
		"An object-position edit changes the crop immediately")
	viewport.queue_free()


func _check_image_rendering() -> void:
	var source: Image = Image.create_empty(3, 2, false, Image.FORMAT_RGBA8)
	var colors: Array[Color] = [Color.RED, Color.LIME, Color.BLUE, Color.YELLOW, Color.CYAN, Color.MAGENTA,]
	for index: int in colors.size():
		source.set_pixel(index % 3, floori(float(index) / 3.0), colors[index])
	var texture: ImageTexture = ImageTexture.create_from_image(source)
	var sheet: Image = Image.create_empty(5, 4, false, Image.FORMAT_RGBA8)
	sheet.fill(Color.WHITE)
	sheet.blit_rect(source, Rect2i(0, 0, 3, 2), Vector2i.ONE)
	var atlas: AtlasTexture = AtlasTexture.new()
	atlas.atlas = ImageTexture.create_from_image(sheet)
	atlas.region = Rect2(1.0, 1.0, 3.0, 2.0)
	var mipmap_image: Image = Image.create_empty(8, 8, false, Image.FORMAT_RGBA8)
	mipmap_image.fill(Color.BLUE)
	mipmap_image.fill_rect(Rect2i(3, 3, 2, 2), Color.RED)
	_check(mipmap_image.generate_mipmaps() == OK, "Generate the mipmap fixture")
	var mipmap_texture: ImageTexture = ImageTexture.create_from_image(mipmap_image)
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_textures", {
		"res://sampling.png": texture,
		"res://atlas.png": atlas,
		"res://mipmap.png": mipmap_texture,
	})
	document.text = """<!doctype html><style>
		body { margin: 0; background: #222222; display: grid; grid-template-columns: repeat(5, 32px);
			grid-auto-rows: 20px; align-items: start; justify-items: start; }
		img { display: block; width: 8px; height: 5px; }
		#repeated { width: 24px; height: 5px; background: url(sampling.png) repeat-x left top / 8px 5px;
			image-rendering: pixelated; }
		#inherited { --filter: pixelated; image-rendering: var(--filter); }
		#integer, #rounded { image-rendering: pixelated; width: 9px; height: 6px; }
		#rounded { width: 12px; height: 8px; border-radius: 4px; }
		.minified { width: 1px; height: 1px; image-rendering: smooth; }
		@supports (image-rendering: high-quality) { #mipmapped { image-rendering: high-quality; } }
	</style>
	<img id="crisp" src="sampling.png" style="image-rendering: crisp-edges">
	<img id="smooth" src="sampling.png" style="image-rendering: smooth">
	<div id="inherited"><img id="fractional" src="sampling.png"></div>
	<img id="integer" src="sampling.png">
	<img id="atlas" src="atlas.png" style="image-rendering: pixelated">
	<div id="repeated"></div>
	<img id="speed" src="sampling.png" style="image-rendering: optimizeSpeed">
	<img id="quality" src="sampling.png" style="image-rendering: optimizeQuality">
	<img id="zoom" src="sampling.png" style="width: 6px; height: 6px; image-rendering: pixelated;
		transform: scaleX(1.5); transform-origin: left top">
	<img id="rounded" src="sampling.png">
	<img id="linear_minified" class="minified" src="mipmap.png">
	<img id="mipmapped" class="minified" src="mipmap.png">
	<img id="invalid_sampling" src="sampling.png" style="image-rendering: crisp-edges; image-rendering: interpolate">
	<div id="generated" style="image-rendering: pixelated; transform: scale(3); transform-origin: left top">
		<style>#generated::before { content: url(sampling.png); display: block; line-height: 0; }</style>
	</div>
	<img id="automatic" src="sampling.png" style="image-rendering: auto">"""
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(170, 165)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport.canvas_item_default_texture_filter = Viewport.DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(165.0, 75.0)
	label.document = document
	viewport.add_child(label)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(0.0, 80.0)
	markdown.size = Vector2(150.0, 85.0)
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken"]:
		markdown.add_theme_icon_override(icon, texture)
	markdown.parse_html = true
	markdown.text = """
<div><img src="sampling.svg" style="display: block; image-rendering: pixelated; width: 9px; height: 6px"></div>
"""
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw

	var result: Image = viewport.get_texture().get_image()
	var blended: Color = Color.RED.lerp(Color.LIME, 0.3125)
	var smooth: Color = Color.RED.lerp(Color.LIME, 0.0625).lerp(Color.YELLOW.lerp(Color.CYAN, 0.0625), 0.1)
	var samples: Array[Array] = [
		["crisp", Vector2(2.0, 1.0), Color.RED],
		["smooth", Vector2.ONE, smooth],
		["fractional", Vector2.ONE, Color.RED],
		["fractional", Vector2(2.0, 1.0), blended],
		["fractional", Vector2(0.0, 2.0), Color.RED.lerp(Color.YELLOW, 0.5)],
		["integer", Vector2(2.0, 1.0), Color.RED],
		["integer", Vector2(3.0, 1.0), Color.LIME],
		["atlas", Vector2.ZERO, Color.RED],
		["atlas", Vector2(7.0, 4.0), Color.MAGENTA],
		["repeated", Vector2(2.0, 1.0), blended],
		["repeated", Vector2(10.0, 1.0), blended],
		["repeated", Vector2(18.0, 1.0), blended],
		["speed", Vector2(2.0, 1.0), Color.RED],
		["quality", Vector2.ONE, smooth],
		["zoom", Vector2(2.0, 1.0), Color.RED],
		["zoom", Vector2(3.0, 1.0), Color.LIME],
		["rounded", Vector2(2.0, 2.0), Color.RED],
		["linear_minified", Vector2.ZERO, Color.RED],
		["invalid_sampling", Vector2(2.0, 1.0), Color.RED],
		["automatic", Vector2(2.0, 1.0), Color.RED],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var color: Color = sample[2]
		_check_pixel(result, _label_rect(label, id).position + offset, color, "Image sampling for %s" % id)
	var rounded_corner: Color = result.get_pixelv(Vector2i(_label_rect(label, "rounded").position))
	_check(_same_color(rounded_corner, Color("#222222")), "Pixelated sampling preserves rounded clipping, got %s" % rounded_corner)
	var minified: Color = result.get_pixelv(Vector2i(_label_rect(label, "mipmapped").position))
	_check(minified.r < 0.25 and minified.b > 0.8, "High-quality minification uses available mipmaps, got %s" % minified)
	var generated: Rect2i = Rect2i(Vector2i(_label_rect(label, "generated").position), Vector2i(22, 18))
	_check(_color_bounds(result, generated, Color.RED).size == Vector2i(3, 3),
		"A generated image inherits pixelated filtering")
	var markdown_red: Rect2i = _color_bounds(result, Rect2i(0, 80, 150, 85), Color.RED)
	_check(markdown_red.size == Vector2i(3, 3), "Markdown HTML uses pixelated image filtering, got %s" % markdown_red)

	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(165, 75)) == OK, "Lay out the image-rendering diagnostics fixture")
	_check(_has_diagnostic(layout, "image-rendering", "interpolate", "inline_style"), "Report an unsupported sampling value")
	var inherited: HTMLElement = label.get_element_by_id("inherited")
	_check(inherited.set_attribute("style", "image-rendering: smooth") == OK, "Change the inherited image sampling mode")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), _label_rect(label, "fractional").position + Vector2.ONE, smooth,
		"Changing an ancestor's sampling mode refreshes the child image")
	viewport.queue_free()


func _check_image_set() -> void:
	var red_image: Image = Image.create_empty(20, 12, false, Image.FORMAT_RGBA8)
	red_image.fill(Color.RED)
	var green_image: Image = Image.create_empty(28, 16, false, Image.FORMAT_RGBA8)
	green_image.fill(Color.LIME)
	var blue_image: Image = Image.create_empty(24, 16, false, Image.FORMAT_RGBA8)
	blue_image.fill(Color.BLUE)
	var red_texture: ImageTexture = ImageTexture.create_from_image(red_image)
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_textures", {
		"res://red.png": red_texture,
		"res://green.png": ImageTexture.create_from_image(green_image),
		"res://blue.png": ImageTexture.create_from_image(blue_image),
		"res://vector.svg": red_texture,
	})
	document.text = """<!doctype html><style>
		body { margin: 0; display: grid; grid-template-columns: repeat(5, 34px); grid-auto-rows: 23px; }
		.probe { width: 29px; height: 20px; background-color: #444444; background-repeat: no-repeat; }
		#nearest { background-image: image-set("red.png" 2x, url(green.png) 96dpi, "blue.png" .75dppx); }
		#first { background-image: image-set("red.png" type("image/avif"), "green.png" type("IMAGE/PNG") 1x,
			"blue.png" 96dpi); }
		#math { background-image: image-set("red.png" calc(96dpi + 1dppx)); }
		#below { background-image: image-set("red.png" .25x, "blue.png" .75x); }
		#vector { background-image: image-set(url(vector.svg) 2x); }
		#shorthand { background: image-set(linear-gradient(lime, lime) 1x, "red.png" 2x) no-repeat top left / 9px 7px,
			image-set(url(blue.png) 2x) no-repeat top left / 19px 13px #444444; }
		#variable { --density: calc(96dpi + 1dppx); background-image: image-set(var(--missing, "red.png") var(--density)); }
		#invalid { background-image: url(green.png); background-image: image-set("red.png" -1x, "blue.png" 2x); }
		#invalid_var { --wrong: -1x; background-image: url(green.png); background-image: image-set("red.png" var(--wrong)); }
		#unsupported { background-image: url(green.png); background-image: image-set("red.png" type("image/avif")); }
		#zero { background-image: url(green.png); background-image: image-set("red.png" 0dppx); }
		#clamped { background-image: url(green.png); background-image: image-set("red.png" calc(0x - 1x)); }
		#missing { background-image: image-set("missing.png", "green.png" 2x); }
		#dpcm { background-image: image-set("red.png" calc(19200dpcm / 254)); }
		@supports (background-image: image-set(url(red.png) 2x)) {
			@media (resolution: calc(48dpi + .5dppx)) {
				#query { background-image: image-set("green.png"); }
			}
			@media (resolution > calc(2dppx)) { #query { background-image: url(red.png); } }
		}
	</style>"""
	for id: String in ["nearest", "first", "math", "below", "vector", "shorthand", "variable", "invalid", "invalid_var",
		"unsupported", "zero", "clamped", "missing", "dpcm", "query",]:
		document.text += '<div class="probe" id="%s"></div>' % id

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(340, 90)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(175.0, 90.0)
	label.document = document
	viewport.add_child(label)
	var imported: HTMLDocument = ResourceLoader.load("{IMAGE_SET_UID}") as HTMLDocument
	_check(imported != null, "Load the imported image-set document by UID")
	var imported_label: HTMLLabel = HTMLLabel.new()
	if imported != null:
		var images: Dictionary = imported.get("_textures")
		_check(images.has("res://styles/set-low.svg") and images.has("res://styles/nested/set-high.svg"),
			"The importer embeds both the selected string candidate and the unused var() fallback candidate")
		imported_label.position = Vector2(180.0, 0.0)
		imported_label.size = Vector2(150.0, 80.0)
		imported_label.document = imported
		viewport.add_child(imported_label)
	else:
		imported_label.free()
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw

	var output: Image = viewport.get_texture().get_image()
	var empty: Color = Color("#444444")
	var samples: Array[Array] = [
		["nearest", Vector2(2.0, 2.0), Color.LIME],
		["first", Vector2(2.0, 2.0), Color.LIME],
		["math", Vector2(9.0, 5.0), Color.RED],
		["math", Vector2(10.0, 6.0), empty],
		["below", Vector2(26.0, 17.0), Color.BLUE],
		["vector", Vector2(19.0, 11.0), Color.RED],
		["shorthand", Vector2(8.0, 6.0), Color.LIME],
		["shorthand", Vector2(12.0, 9.0), Color.BLUE],
		["shorthand", Vector2(22.0, 16.0), empty],
		["variable", Vector2(9.0, 5.0), Color.RED],
		["variable", Vector2(12.0, 8.0), empty],
		["invalid", Vector2(2.0, 2.0), Color.LIME],
		["invalid_var", Vector2(2.0, 2.0), empty],
		["unsupported", Vector2(2.0, 2.0), empty],
		["zero", Vector2(2.0, 2.0), empty],
		["clamped", Vector2(2.0, 2.0), empty],
		["missing", Vector2(2.0, 2.0), empty],
		["dpcm", Vector2(9.0, 5.0), Color.RED],
		["dpcm", Vector2(12.0, 8.0), empty],
		["query", Vector2(2.0, 2.0), Color.LIME],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var color: Color = sample[2]
		_check_pixel(output, _label_rect(label, id).position + offset, color, "Image-set choice for %s" % id)
	if imported != null:
		var point: Vector2 = imported_label.position + _label_rect(imported_label, "imported_set").position + Vector2(3.0, 3.0)
		_check_pixel(output, point, Color("#67bb39"), "A nested rule in an imported stylesheet keeps its relative image-set URL")
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(175, 90)) == OK, "Lay out the image-set diagnostics fixture")
	_check(layout.get_element_by_id("invalid").get_css_diagnostics().size() == 1 and
		layout.get_element_by_id("invalid_var").get_css_diagnostics().size() == 1,
		"Invalid literal and substituted resolutions each report one diagnostic")
	_check(layout.get_element_by_id("unsupported").get_css_diagnostics().is_empty(),
		"An unsupported MIME type leaves a syntactically valid image-set")
	viewport.queue_free()


func _check_annotated_image() -> void:
	var sprite: Image = Image.create_empty(12, 8, false, Image.FORMAT_RGBA8)
	sprite.fill_rect(Rect2i(0, 0, 4, 8), Color.RED)
	sprite.fill_rect(Rect2i(4, 0, 4, 8), Color.LIME)
	sprite.fill_rect(Rect2i(8, 0, 4, 8), Color.BLUE)
	var transparent: Image = Image.create_empty(12, 8, false, Image.FORMAT_RGBA8)
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_textures", {
		"res://sprite.png": ImageTexture.create_from_image(sprite),
		"res://alpha.png": ImageTexture.create_from_image(transparent),
	})
	document.text = """<!doctype html><style>
		body { margin: 0; display: grid; grid-template-columns: repeat(5, 36px); grid-auto-rows: 22px; }
		.probe { width: 30px; height: 19px; background-color: #404040; background-repeat: no-repeat; }
		#solid { color: lime; background: image(currentColor) no-repeat 3px 2px / 10px 6px #404040; }
		#repeat_color { background: image(blue) repeat-x 0 2px / 4px 4px #404040; }
		#missing { background-image: image("missing.png", lime); }
		#bad_fragment { background-image: image(url("sprite.png#t=1,2"), yellow); }
		#clip { background-image: image("sprite.png#xywh=4,0,4,8", red); }
		#clip_percent { background-image: image("sprite.png#xywh=percent:50,0,50,100"); }
		#clip_edge { background-image: image(url("sprite.png#xywh=pixel:11,0,20,1"), red); }
		#clip_empty { background-image: image("sprite.png#xywh=12,0,1,1", yellow); }
		#clip_repeat { background: image("sprite.png#xywh=4,0,8,8") repeat-x #404040; }
		#mirrored, #override { background-image: image(ltr "sprite.png"); }
		#same { direction: rtl; background-image: image(rtl url(sprite.png)); }
		#neutral { background-image: image(url(sprite.png)); }
		#override { direction: ltr; }
		#nested_set { background-image: image-set(image(rtl "sprite.png#xywh=4,0,8,8", yellow) 1x, url(sprite.png) 2x); }
		#alpha { background-image: image("alpha.png", lime); }
		#overlay { background-image: image(rgb(0 0 255 / 50%)), url(sprite.png); }
		#invalid { background-image: url(sprite.png); background-image: image(ltr "sprite.png", bogus); }
		#inherited { background-image: image(ltr "sprite.png"); }
		@supports (background-image: image("fallback.png", currentColor)) {
			#variable { color: lime; background-image: image(var(--asset, "missing.png"), currentColor); }
		}
	</style>
	<div class="probe" id="solid"></div><div class="probe" id="repeat_color"></div>
	<div class="probe" id="missing"></div><div class="probe" id="bad_fragment"></div>
	<div class="probe" id="clip"></div><div class="probe" id="clip_percent"></div>
	<div class="probe" id="clip_edge"></div><div class="probe" id="clip_empty"></div>
	<div class="probe" id="clip_repeat"></div><div class="probe" id="mirrored" dir="RTL"></div>
	<div class="probe" id="same"></div><div class="probe" id="neutral" dir="rtl"></div>
	<div class="probe" id="override" dir="rtl"></div><div class="probe" id="nested_set"></div>
	<div class="probe" id="alpha"></div><div class="probe" id="overlay"></div>
	<div class="probe" id="invalid"></div><div class="probe" id="variable"></div>
	<section dir="rtl"><div class="probe" id="inherited"></div></section>"""

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 100)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(180.0, 100.0)
	label.document = document
	viewport.add_child(label)
	var imported: HTMLDocument = ResourceLoader.load("{IMAGE_FUNCTION_UID}") as HTMLDocument
	var imported_label: HTMLLabel = HTMLLabel.new()
	imported_label.position = Vector2(190.0, 0.0)
	imported_label.size = Vector2(100.0, 50.0)
	imported_label.document = imported
	viewport.add_child(imported_label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw

	var result: Image = viewport.get_texture().get_image()
	var empty: Color = Color("#404040")
	var samples: Array[Array] = [
		["solid", Vector2(3.0, 2.0), Color.LIME],
		["solid", Vector2(12.0, 7.0), Color.LIME],
		["solid", Vector2(13.0, 7.0), empty],
		["repeat_color", Vector2(26.0, 3.0), Color.BLUE],
		["repeat_color", Vector2(26.0, 7.0), empty],
		["missing", Vector2(28.0, 18.0), Color.LIME],
		["bad_fragment", Vector2(1.0, 1.0), Color.YELLOW],
		["clip", Vector2(3.0, 7.0), Color.LIME],
		["clip", Vector2(4.0, 7.0), empty],
		["clip_percent", Vector2(1.0, 1.0), Color.LIME],
		["clip_percent", Vector2(5.0, 7.0), Color.BLUE],
		["clip_edge", Vector2.ZERO, Color.BLUE],
		["clip_edge", Vector2(1.0, 0.0), empty],
		["clip_empty", Vector2(1.0, 1.0), Color.YELLOW],
		["clip_repeat", Vector2(10.0, 3.0), Color.LIME],
		["clip_repeat", Vector2(14.0, 3.0), Color.BLUE],
		["mirrored", Vector2(1.0, 1.0), Color.BLUE],
		["mirrored", Vector2(10.0, 1.0), Color.RED],
		["same", Vector2(1.0, 1.0), Color.RED],
		["neutral", Vector2(1.0, 1.0), Color.RED],
		["override", Vector2(1.0, 1.0), Color.RED],
		["nested_set", Vector2(1.0, 1.0), Color.BLUE],
		["nested_set", Vector2(7.0, 1.0), Color.LIME],
		["alpha", Vector2(1.0, 1.0), empty],
		["overlay", Vector2(1.0, 1.0), Color("#800080")],
		["invalid", Vector2(1.0, 1.0), Color.RED],
		["variable", Vector2(1.0, 1.0), Color.LIME],
		["inherited", Vector2(1.0, 1.0), Color.BLUE],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var color: Color = sample[2]
		_check_pixel(result, _label_rect(label, id).position + offset, color, "Annotated image for %s" % id)
	_check(imported != null, "Load the imported image() document by UID")
	if imported != null:
		var textures: Dictionary = imported.get("_textures")
		_check(textures.has("res://sampling.svg"), "Import a bare image() path with xywh as an image dependency")
		var top_left: Vector2 = imported_label.position + _label_rect(imported_label, "imported_image").position
		_check_pixel(result, top_left, Color.LIME, "The embedded stylesheet paints the cropped asset after import")
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(180, 100)) == OK, "Lay out the image() diagnostics fixture")
	_check(layout.get_element_by_id("invalid").get_css_diagnostics().size() == 1, "Report an invalid image() fallback color")

	var direction_override: HTMLElement = label.get_element_by_id("override")
	_check(direction_override.set_attribute("style", "direction: revert") == OK, "Revert the explicit CSS direction")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), _label_rect(label, "override").position + Vector2.ONE, Color.BLUE,
		"Direction rollback restores the HTML hint and flips the image")
	viewport.queue_free()


func _check_cross_fade() -> void:
	var red: Image = Image.create_empty(8, 4, false, Image.FORMAT_RGBA8)
	red.fill(Color.RED)
	var blue: Image = Image.create_empty(24, 12, false, Image.FORMAT_RGBA8)
	blue.fill(Color.BLUE)
	var stripe: Image = Image.create_empty(12, 4, false, Image.FORMAT_RGBA8)
	stripe.fill_rect(Rect2i(0, 0, 4, 4), Color.RED)
	stripe.fill_rect(Rect2i(4, 0, 4, 4), Color.LIME)
	stripe.fill_rect(Rect2i(8, 0, 4, 4), Color.BLUE)
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_textures", {
		"res://mix-red.png": ImageTexture.create_from_image(red),
		"res://mix-blue.png": ImageTexture.create_from_image(blue),
		"res://mix-stripe.png": ImageTexture.create_from_image(stripe),
	})
	document.text = """<!doctype html><style>
		body { margin: 0; display: grid; grid-template-columns: repeat(5, 36px); grid-auto-rows: 22px; }
		div { width: 30px; height: 19px; background-color: black; background-repeat: no-repeat; }
		#omitted { background-image: cross-fade(red 25%, 25% lime, blue); }
		#overflow { background-image: cross-fade(red 80%, blue 80%, lime); }
		#leftover { background-color: white; background-image: cross-fade(red 25%); }
		#premul { background-image: cross-fade(red 40%, rgb(0 255 0 / 50%) 20%, transparent 40%); }
		#natural { background-image: cross-fade(url(mix-red.png) 75%, url(mix-blue.png) 25%); }
		#tint { background-image: cross-fade(white 80%, url(mix-red.png) 20%); }
		#linear { background: cross-fade(linear-gradient(to right, red 50%, blue 50%), lime)
			no-repeat top left / 20px 10px black; }
		#radial { background: cross-fade(radial-gradient(circle closest-side, red 30%, blue 30%), lime)
			no-repeat top left / 20px 10px black; }
		#conic { background: cross-fade(conic-gradient(red 50%, blue 50%), lime)
			no-repeat top left / 20px 10px black; }
		#nested { background-image: cross-fade(red 20%, cross-fade(20% lime, 80% blue) 50%, transparent 30%); }
		#set { background-image: cross-fade(image-set("mix-red.png" 1x, "mix-blue.png" 2x), lime); }
		#set_outer { background-image: image-set(cross-fade(image("missing.png", blue), lime) 1x, url(mix-red.png) 2x); }
		#tiled { background: cross-fade(image(ltr "mix-stripe.png#xywh=4,0,8,4"), red) repeat-x black;
			direction: rtl; image-rendering: crisp-edges; }
		#invalid { background-image: image(lime); background-image: cross-fade(red 101%, blue); }
		#missing { background-image: cross-fade(url(missing.png) 50%, blue 50%); }
		#zero { background-image: cross-fade(url(mix-blue.png) 0%, blue 0%); }
		#nested_size { background-image: cross-fade(cross-fade(url(mix-red.png) 25%, white 75%), url(mix-blue.png)); }
		@supports (background-image: cross-fade(red, blue)) {
			#math { --weight: calc(10% + 15%); color: lime; background-image: cross-fade(currentColor var(--weight), blue); }
		}
	</style>"""
	for id: String in ["omitted", "overflow", "leftover", "premul", "natural", "tint", "linear", "radial", "conic",
		"nested", "set", "set_outer", "tiled", "invalid", "missing", "zero", "nested_size", "math",]:
		document.text += '<div id="%s"></div>' % id

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(180, 90)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(180.0, 90.0)
	label.document = document
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	var samples: Array[Array] = [
		["omitted", Vector2.ONE, Color("#404080")],
		["overflow", Vector2.ONE, Color("#800080")],
		["leftover", Vector2.ONE, Color("#ffbfbf")],
		["premul", Vector2.ONE, Color("#661a00")],
		["natural", Vector2(11.0, 5.0), Color("#bf0040")],
		["natural", Vector2(12.0, 5.0), Color.BLACK],
		["natural", Vector2(11.0, 6.0), Color.BLACK],
		["tint", Vector2(7.0, 3.0), Color("#ffcccc")],
		["tint", Vector2(8.0, 0.0), Color.BLACK],
		["linear", Vector2(3.0, 4.0), Color("#808000")],
		["linear", Vector2(15.0, 4.0), Color("#008080")],
		["radial", Vector2(10.0, 4.0), Color("#808000")],
		["radial", Vector2.ONE, Color("#008080")],
		["conic", Vector2(15.0, 2.0), Color("#808000")],
		["conic", Vector2(4.0, 2.0), Color("#008080")],
		["nested", Vector2.ONE, Color("#331a66")],
		["set", Vector2.ONE, Color("#808000")],
		["set_outer", Vector2.ONE, Color("#008080")],
		["tiled", Vector2(9.0, 2.0), Color("#800080")],
		["tiled", Vector2(13.0, 2.0), Color("#808000")],
		["tiled", Vector2(13.0, 4.0), Color.BLACK],
		["invalid", Vector2.ONE, Color.LIME],
		["missing", Vector2.ONE, Color("#000080")],
		["zero", Vector2.ONE, Color.BLACK],
		["nested_size", Vector2(20.0, 9.0), Color("#8060df")],
		["nested_size", Vector2(21.0, 9.0), Color.BLACK],
		["math", Vector2.ONE, Color("#0040bf")],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var color: Color = sample[2]
		_check_pixel(output, _label_rect(label, id).position + offset, color, "Cross-fade for %s" % id)
	var diagnostics: HTMLLayout = document.create_layout()
	diagnostics.css_diagnostics_enabled = true
	_check(diagnostics.layout(Vector2i(180, 90)) == OK, "Lay out the cross-fade diagnostics fixture")
	_check(diagnostics.get_element_by_id("invalid").get_css_diagnostics().size() == 1,
		"Reject a cross-fade weight outside the percentage range")
	viewport.queue_free()


func _check_background_blend_modes() -> void:
	var expected: Dictionary[String, String] = {
		"normal": "cc4d1a",
		"multiply": "332714",
		"screen": "d9a6c6",
		"overlay": "664e8c",
		"darken": "404d1a",
		"lighten": "cc80bf",
		"color-dodge": "ffb7d5",
		"color-burn": "100000",
		"hard-light": "b24d27",
		"soft-light": "666799",
		"difference": "8c33a5",
		"exclusion": "a67fb1",
		"hue": "b75d38",
		"saturation": "2b85dd",
		"color": "d25320",
		"luminosity": "3a7ab9",
	}
	var source: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.swatch { position: absolute; width: 34px; height: 24px; background-color: #4080bf;
			background-image: linear-gradient(#cc4d1a,#cc4d1a); }
		.extra { position: absolute; top: 86px; width: 34px; height: 24px; }
		#cycle { left: 8px; background-color: #808080;
			background-image: linear-gradient(#ff80ff,#ff80ff), none, linear-gradient(#80ff80,#80ff80), linear-gradient(red,red);
			background-blend-mode: multiply, screen; }
		#truncated { left: 46px; background-blend-mode: multiply, screen; }
		#variable { left: 84px; --mode: screen; background-blend-mode: var(--mode); }
		#reset { left: 122px; --bg: linear-gradient(#cc4d1a,#cc4d1a) #4080bf;
			background-blend-mode: multiply; background: var(--bg); }
		#important { left: 160px; background-blend-mode: multiply !important;
			background: linear-gradient(#cc4d1a,#cc4d1a) #4080bf; }
		#invalid { left: 198px; background-blend-mode: screen; background-blend-mode: screen, plus-lighter; }
		#isolated { left: 236px; background-image: linear-gradient(red,red); background-blend-mode: multiply; }
		#fade { left: 274px; background-color: #4080bf; background-image: cross-fade(#cc4d1a 100%);
			background-blend-mode: screen; }
		#bottom { position: absolute; left: 8px; top: 130px; width: 34px; height: 24px; background: white; }
		#alpha { left: 8px; top: 130px; background-color: rgb(0 0 255 / 50%);
			background-image: linear-gradient(rgb(255 0 0 / 50%),rgb(255 0 0 / 50%)); background-blend-mode: multiply; }
		#rounded { left: 62px; top: 130px; width: 30px; height: 24px; padding: 6px; border-radius: 12px;
			background-color: blue; background-image: linear-gradient(red,red), none;
			background-clip: content-box, border-box; background-blend-mode: screen; }
		#projected { left: 125px; top: 130px; width: 70px; height: 36px; background-color: #4080bf;
			background-image: linear-gradient(#cc4d1a,#cc4d1a); background-blend-mode: screen;
			transform: perspective(100px) rotateY(45deg); transform-origin: top left; }
		#inline_case { position: absolute; left: 205px; top: 132px; width: 39px; line-height: 22px;
			font: 16px monospace; }
		#inline { color: transparent; background-color: blue; background-image: linear-gradient(red,red);
			background-blend-mode: screen; }
	</style>
	<div id="cycle" class="extra"></div><div id="truncated" class="extra swatch"></div>
	<div id="variable" class="extra swatch"></div><div id="reset" class="extra swatch"></div>
	<div id="important" class="extra swatch"></div><div id="invalid" class="extra swatch"></div>
	<div id="isolated" class="extra"></div><div id="fade" class="extra"></div>
	<div id="bottom"></div><div id="alpha" class="extra"></div>
	<div id="rounded" class="extra"></div><div id="projected" class="extra"></div>
	<div id="inline_case"><span id="inline">One Two</span></div>"""
	var points: Dictionary[String, Vector2] = {}
	var index: int
	for mode: String in expected:
		var left: int = 8 + (index % 8) * 38
		var top: int = (12 if index < 8 else 48)
		source += '<div class="swatch" style="left:%dpx;top:%dpx;background-blend-mode:%s"></div>' % [left, top, mode]
		points[mode] = Vector2(left + 12, top + 8)
		index += 1

	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(320, 180)) == OK, "Lay out CSS background blends")
	_check(_has_diagnostic(layout, "background-blend-mode", "screen , plus-lighter", "style_element"),
		"Invalid background blend lists leave earlier declarations in effect")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 180)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	for mode: String in expected:
		_check_pixel(image, points[mode], Color(expected[mode]), "Background blend " + mode)
	_check_pixel(image, Vector2(20.0, 94.0), Color("804040"), "The mode list cycles over all image slots, including none")
	_check_pixel(image, Vector2(58.0, 94.0), Color(expected["multiply"]), "Surplus modes are unused")
	_check_pixel(image, Vector2(96.0, 94.0), Color(expected["screen"]), "A substituted mode is parsed in the cascade")
	_check_pixel(image, Vector2(134.0, 94.0), Color(expected["normal"]), "The background shorthand resets the mode after var substitution")
	_check_pixel(image, Vector2(172.0, 94.0), Color(expected["multiply"]), "A higher priority blend survives the shorthand reset")
	_check_pixel(image, Vector2(210.0, 94.0), Color(expected["screen"]), "Invalid trailing modes reject the entire declaration")
	_check_pixel(image, Vector2(248.0, 94.0), Color.RED, "The document backdrop is outside the isolated background stack")
	_check_pixel(image, Vector2(286.0, 94.0), Color(expected["screen"]), "Cross-fade images participate in background blending")
	_check_pixel(image, Vector2(20.0, 138.0), Color(0.5, 0.25, 0.5), "Partially transparent layers preserve uncovered color")
	_check_pixel(image, Vector2(63.0, 131.0), Color.BLACK, "Blended layers honor rounded corners")
	_check_pixel(image, Vector2(82.0, 133.0), Color.BLUE, "Only the bottom color covers the border-box inset")
	_check_pixel(image, Vector2(82.0, 143.0), Color.MAGENTA, "The content-box background blends with the bottom color")
	_check_pixel(image, Vector2(132.0, 141.0), Color(expected["screen"]), "Projected background layers still blend together")
	var fragments: Array[Rect2] = label.get_element_by_id("inline").get_client_rects()
	_check(fragments.size() == 2, "Wrapped inline background produces two fragments")
	for fragment: Rect2 in fragments:
		_check_pixel(image, fragment.get_center().floor(), Color.MAGENTA, "Each inline fragment blends its own background")
	viewport.queue_free()

	if RenderingServer.get_current_rendering_method() == "forward_plus":
		var hdr_viewport: SubViewport = SubViewport.new()
		hdr_viewport.size = Vector2i(320, 180)
		hdr_viewport.use_hdr_2d = true
		hdr_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		root.add_child(hdr_viewport)
		var hdr_label: HTMLLabel = HTMLLabel.new()
		hdr_label.size = Vector2(hdr_viewport.size)
		hdr_label.text = source
		hdr_viewport.add_child(hdr_label)
		await process_frame
		await process_frame
		await RenderingServer.frame_post_draw
		var hdr_image: Image = hdr_viewport.get_texture().get_image()
		for mode: String in ["normal", "multiply", "screen", "color"]:
			var pixel: Color = hdr_image.get_pixelv(Vector2i(points[mode])).linear_to_srgb()
			_check(_same_color(pixel, Color(expected[mode])), "HDR background blend %s yields %s" % [mode, pixel])
		for point: Vector2i in [Vector2i(286, 94), Vector2i(132, 141)]:
			var pixel: Color = hdr_image.get_pixelv(point).linear_to_srgb()
			_check(_same_color(pixel, Color(expected["screen"])), "HDR blend through an intermediate surface at %s yields %s" % [point, pixel])
		hdr_viewport.queue_free()


func _check_element_blend_modes() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.slot { position: absolute; width: 52px; height: 30px; background: #4080bf; top: 10px; }
		.ink { position: absolute; inset: 0; background: #cc4d1a; }
		#multiply { left: 8px; } #multiply .ink { mix-blend-mode: multiply; }
		#variable { left: 70px; } #variable .ink { --blend: screen; mix-blend-mode: var(--blend); }
		#hue { left: 132px; } #hue .ink { mix-blend-mode: hue; }
		#plus { left: 194px; } #plus .ink { mix-blend-mode: plus-lighter; }
		#under { position: absolute; left: 8px; top: 58px; width: 302px; height: 30px; background: cyan; }
		.holder { position: absolute; top: 58px; width: 52px; height: 30px; }
		.holder .ink { background: red; mix-blend-mode: multiply; }
		#auto { left: 8px; isolation: auto; }
		#isolate { left: 70px; isolation: isolate; }
		#stack { left: 132px; z-index: 0; }
		#transformed { left: 194px; transform: translateX(0px); }
		#opacity { left: 256px; opacity: .5; }
		#sequence, #plus_alpha, #mask, #whole_source, #round { top: 108px; background: blue; }
		#sequence { left: 8px; }
		#sequence .ink { background: red; }
		#sequence .ink:first-child { mix-blend-mode: screen; }
		#sequence .ink:last-child { mix-blend-mode: difference; }
		#plus_alpha { left: 70px; isolation: isolate; background: none; }
		#plus_alpha .ink:first-child { background: rgb(0 0 255 / 50%); }
		#plus_alpha .ink:last-child { background: red; opacity: .5; mix-blend-mode: plus-lighter; }
		#mask { left: 132px; }
		#mask .ink { background: red; mix-blend-mode: screen; opacity: .5; filter: grayscale(1); clip-path: circle(8px); }
		#whole_source { left: 194px; }
		#whole_source .ink { background: red; mix-blend-mode: screen; opacity: .5; }
		#whole_source .child { position: absolute; inset: 0 50% 0 0; background: lime; }
		#round { left: 256px; border-radius: 12px; overflow: hidden; }
		#round .ink { background: yellow; mix-blend-mode: screen; }
		#depth { left: 8px; top: 160px; width: 106px; height: 40px; background: blue; transform-style: preserve-3d; }
		#depth .ink:first-child { background: blue; }
		#depth .ink:last-child { background: red; mix-blend-mode: screen; transform: rotateY(42deg); }
		#invalid { left: 256px; } #invalid .ink { mix-blend-mode: screen; mix-blend-mode: hue screen; isolation: isolate auto; }
	</style>
	<div id="multiply" class="slot"><div class="ink"></div></div>
	<div id="variable" class="slot"><div class="ink"></div></div>
	<div id="hue" class="slot"><div class="ink"></div></div>
	<div id="plus" class="slot"><div class="ink"></div></div>
	<div id="invalid" class="slot"><div class="ink"></div></div>
	<div id="under"></div>
	<div id="auto" class="holder"><div class="ink"></div></div>
	<div id="isolate" class="holder"><div class="ink"></div></div>
	<div id="stack" class="holder"><div class="ink"></div></div>
	<div id="transformed" class="holder"><div class="ink"></div></div>
	<div id="opacity" class="holder"><div class="ink"></div></div>
	<div id="sequence" class="slot"><div class="ink"></div><div class="ink"></div></div>
	<div id="plus_alpha" class="slot"><div class="ink"></div><div class="ink"></div></div>
	<div id="mask" class="slot"><div class="ink"></div></div>
	<div id="whole_source" class="slot"><div class="ink"><div class="child"></div></div></div>
	<div id="round" class="slot"><div class="ink"></div></div>
	<div id="depth" class="slot"><div class="ink"></div><div class="ink" id="depth_ink"></div></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(330, 220)) == OK, "Lay out element blends")
	_check(_has_diagnostic(layout, "mix-blend-mode", "hue screen", "style_element")
		and _has_diagnostic(layout, "isolation", "isolate auto", "style_element"),
		"Mixed or repeated keywords are invalid for blend and isolation")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(330, 220)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(34.0, 25.0), Color("332714"), "Multiply uses the element's stacking backdrop")
	_check_pixel(image, Vector2(96.0, 25.0), Color("d9a6c6"), "Variable resolves to an element blend mode")
	_check_pixel(image, Vector2(158.0, 25.0), Color("b75d38"), "Hue blends nonseparable sRGB components")
	_check_pixel(image, Vector2(220.0, 25.0), Color("ffcdd9"), "Plus-lighter adds premultiplied color")
	_check_pixel(image, Vector2(282.0, 25.0), Color("d9a6c6"), "Invalid mixed keywords preserve the earlier mode")
	_check_pixel(image, Vector2(34.0, 73.0), Color.BLACK, "An automatic z-index shares the enclosing backdrop")
	_check_pixel(image, Vector2(96.0, 73.0), Color.RED, "Explicit isolation starts with a transparent backdrop")
	_check_pixel(image, Vector2(158.0, 73.0), Color.RED, "An explicit z-index isolates descendants")
	_check_pixel(image, Vector2(220.0, 73.0), Color.RED, "An identity transform establishes an isolated context")
	_check_pixel(image, Vector2(282.0, 73.0), Color(0.5, 0.5, 0.5), "An opacity group blends internally before composition")
	_check_pixel(image, Vector2(34.0, 123.0), Color.BLUE, "A second blender samples the previously blended result")
	_check_pixel(image, Vector2(96.0, 123.0), Color(0.5, 0.0, 0.5), "Plus-lighter adds source and backdrop alpha")
	_check_pixel(image, Vector2(158.0, 123.0), Color(0.1063, 0.1063, 1.0), "Filter, clip and opacity apply before blending")
	_check_pixel(image, Vector2(142.0, 123.0), Color.BLUE, "The blend leaves the document unchanged beyond the clip path")
	_check_pixel(image, Vector2(208.0, 123.0), Color(0.0, 0.5, 1.0), "A blending element includes its descendants as a group")
	_check_pixel(image, Vector2(232.0, 123.0), Color(0.5, 0.0, 1.0), "Group opacity attenuates the rest of the source")
	_check_pixel(image, Vector2(257.0, 109.0), Color.BLACK, "An overflow corner clips element blending")
	_check_pixel(image, Vector2(282.0, 123.0), Color.WHITE, "The same blend is visible inside the overflow clip")
	_check_pixel(image, Vector2(36.0, 180.0), Color.MAGENTA, "A split 3D plane blends in front of the backdrop")
	_check_pixel(image, Vector2(88.0, 180.0), Color.BLUE, "The other side of the 3D intersection is behind the backdrop")
	_check(label.get_element_at(Vector2(36.0, 180.0)) == label.get_element_by_id("depth_ink"),
		"Depth hit testing survives element blending")

	label.hide()
	var backdrop: ColorRect = ColorRect.new()
	backdrop.size = Vector2(viewport.size)
	backdrop.color = Color.CYAN
	viewport.add_child(backdrop)
	var standalone: HTMLLayout = HTMLLayout.new()
	_check(standalone.parse('<div style="width:30px;height:30px;background:red;mix-blend-mode:multiply"></div>') == OK
		and standalone.layout(viewport.size) == OK, "Lay out transparent HTML over a Godot canvas")
	var canvas: Node2D = Node2D.new()
	canvas.draw.connect(func() -> void: standalone.draw(canvas, Vector2(80.0, 30.0)))
	viewport.add_child(canvas)
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(92.0, 42.0), Color.RED, "The document blend backdrop excludes surrounding Godot canvas items")
	_check_pixel(image, Vector2(135.0, 42.0), Color.CYAN, "The surrounding Godot canvas remains visible")
	viewport.queue_free()

	if RenderingServer.get_current_rendering_method() == "forward_plus":
		var hdr_viewport: SubViewport = SubViewport.new()
		hdr_viewport.size = Vector2i(330, 220)
		hdr_viewport.use_hdr_2d = true
		hdr_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		root.add_child(hdr_viewport)
		var hdr_label: HTMLLabel = HTMLLabel.new()
		hdr_label.size = Vector2(hdr_viewport.size)
		hdr_label.text = source
		hdr_viewport.add_child(hdr_label)
		await process_frame
		await process_frame
		await RenderingServer.frame_post_draw
		var hdr_image: Image = hdr_viewport.get_texture().get_image()
		var hdr_cases: Dictionary[Vector2i, Color] = {
			Vector2i(34, 25): Color("332714"),
			Vector2i(158, 25): Color("b75d38"),
			Vector2i(220, 25): Color("ffcdd9"),
			Vector2i(96, 73): Color.RED,
			Vector2i(34, 123): Color.BLUE,
			Vector2i(96, 123): Color(0.5, 0.0, 0.5),
			Vector2i(36, 180): Color.MAGENTA,
		}
		for point: Vector2i in hdr_cases:
			var pixel: Color = hdr_image.get_pixelv(point).linear_to_srgb()
			_check(_same_color(pixel, hdr_cases[point]), "HDR element blend at %s yields %s" % [point, pixel])
		hdr_viewport.queue_free()


func _check_positioned_stacking_levels() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.stage { position: absolute; z-index: 0; top: 10px; width: 84px; height: 40px; background: white; }
		.wrapper { position: relative; left: 6px; top: 4px; width: 66px; height: 28px; }
		.fore { position: absolute; left: 8px; top: 6px; width: 38px; height: 20px; background: lime; z-index: 3; }
		.under { position: absolute; inset: 6px; z-index: 1; background: blue; }
		#relative, #negative { left: 10px; }
		#absolute, #float_group { left: 110px; }
		#isolated, #clipped { left: 210px; }
		#tie, #table_group { left: 310px; }
		#negative, #float_group, #clipped, #table_group { top: 75px; }
		#absolute .wrapper { position: absolute; }
		#isolated .wrapper { z-index: 0; }
		#tie .fore, #tie .under { z-index: 2; }
		#negative .wrapper { background: red; }
		#negative .fore { z-index: -1; }
		#float_group .wrapper { position: static; float: left; }
		#float_group .inline { display: inline-block; width: 76px; height: 30px; }
		#clipped .wrapper { overflow: hidden; border-radius: 10px; width: 38px; height: 24px; }
		#clipped .fore { inset: 0 auto auto 0; width: 62px; height: 25px; }
		.stage table { border-spacing: 0; }
		.stage td { position: relative; width: 66px; height: 28px; padding: 0; }
		#table_row, #table_section { top: 132px; }
		#table_row { left: 10px; } #table_row tr { position: relative; z-index: 2; }
		#table_section { left: 110px; } #table_section tbody { position: relative; z-index: 0; }
	</style>
	<div class="stage" id="relative"><div class="wrapper" id="relative_wrapper"><div id="relative_fore" class="fore"></div></div><div class="under"></div></div>
	<div class="stage" id="absolute"><div class="wrapper"><div id="absolute_fore" class="fore"></div></div><div class="under"></div></div>
	<div class="stage" id="isolated"><div class="wrapper"><div class="fore"></div></div><div class="under" id="isolated_under"></div></div>
	<div class="stage" id="tie"><div class="wrapper"><div class="fore"></div></div><div class="under" id="tie_under"></div></div>
	<div class="stage" id="negative"><div class="wrapper" id="negative_wrapper"><div class="fore"></div></div></div>
	<div class="stage" id="float_group"><div class="wrapper"><span class="inline"><span id="float_fore" class="fore"></span></span></div><div class="under"></div></div>
	<div class="stage" id="clipped"><div class="wrapper"><div class="fore" id="clipped_fore"></div></div><div class="under" id="clipped_under"></div></div>
	<div class="stage" id="table_group"><table><tr><td><div class="fore" id="table_fore"></div></td></tr></table><div class="under"></div></div>
	<div class="stage" id="table_row"><table><tr><td><div class="fore" id="row_fore"></div></td></tr></table><div class="under"></div></div>
	<div class="stage" id="table_section"><table><tbody><tr><td><div class="fore"></div></td></tr></tbody></table><div class="under" id="section_under"></div></div>"""
	var hits: Dictionary[Vector2, String] = {
		Vector2(30.0, 26.0): "relative_fore",
		Vector2(130.0, 26.0): "absolute_fore",
		Vector2(230.0, 26.0): "isolated_under",
		Vector2(330.0, 26.0): "tie_under",
		Vector2(30.0, 92.0): "negative_wrapper",
		Vector2(130.0, 92.0): "float_fore",
		Vector2(217.0, 81.0): "clipped_under",
		Vector2(240.0, 90.0): "clipped_fore",
		Vector2(264.0, 90.0): "clipped_under",
		Vector2(330.0, 92.0): "table_fore",
		Vector2(30.0, 150.0): "row_fore",
		Vector2(130.0, 150.0): "section_under",
	}
	var colors: Dictionary[Vector2, Color] = {
		Vector2(30.0, 26.0): Color.LIME,
		Vector2(130.0, 26.0): Color.LIME,
		Vector2(230.0, 26.0): Color.BLUE,
		Vector2(330.0, 26.0): Color.BLUE,
		Vector2(30.0, 92.0): Color.RED,
		Vector2(130.0, 92.0): Color.LIME,
		Vector2(217.0, 81.0): Color.BLUE,
		Vector2(240.0, 90.0): Color.LIME,
		Vector2(264.0, 90.0): Color.BLUE,
		Vector2(330.0, 92.0): Color.LIME,
		Vector2(30.0, 150.0): Color.LIME,
		Vector2(130.0, 150.0): Color.BLUE,
	}
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(414, 180)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	for point: Vector2 in hits:
		var target: HTMLElement = label.get_element_at(point)
		_check(target == label.get_element_by_id(hits[point]),
			"The layout hit at %s follows the stacking context, expected %s" % [point, hits[point]])
		_check_pixel(image, point, colors[point], "Positioned painting at " + hits[point])

	var wrapper: HTMLElement = label.get_element_by_id("relative_wrapper")
	wrapper.set_attribute("style", "z-index: 0")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), Vector2(30.0, 26.0), Color.BLUE, "Changing the ancestor to z-index zero isolates its child")
	_check(label.get_element_at(Vector2(30.0, 26.0)) != label.get_element_by_id("relative_fore"),
		"Style edits also rebuild the hit-test order")

	label.text = source + "<style>#relative_fore { background: yellow; mix-blend-mode: multiply; }</style>"
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), Vector2(30.0, 26.0), Color.BLACK,
		"A lifted positioned element blends with the lower sibling in its enclosing stacking context")
	for point: Vector2 in hits:
		_check(label.get_element_at(point) == label.get_element_by_id(hits[point]),
			"The recorded paint hit at %s follows the same stacking order" % point)
	viewport.queue_free()


func _check_image_resolution() -> void:
	var generated: Image = Image.create_empty(48, 24, false, Image.FORMAT_RGBA8)
	generated.fill(Color.RED)
	var texture: ImageTexture = ImageTexture.create_from_image(generated)
	if not _write_image_dpi_fixtures():
		_check(false, "Write JPEG and WebP metadata fixtures")
		return
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_textures", {
		"res://density-default.png": texture,
		"res://density-jfif.jpg": texture,
		"res://density-exif.jpg": texture,
		"res://density-exif.webp": texture,
		"res://density-late.webp": texture,
		"res://density-small.png": ImageTexture.create_from_image(generated.get_region(Rect2i(0, 0, 24, 12))),
		"res://density-vector.svg": texture,
	})
	document.text = """<!doctype html><style>
		body { margin: 0; display: grid; grid-template-columns: repeat(5, 72px); grid-auto-rows: 36px;
			align-items: start; justify-items: start; }
		img { display: block; }
		.resolution_parent { image-resolution: 192dpi; }
		.tile { width: 60px; height: 28px; background-color: #303030; background-repeat: no-repeat; }
		#cascade { image-resolution: 2x; image-resolution: snap; }
		#fit { width: 32px; height: 24px; object-fit: none; background-color: #303030; image-resolution: 2x; }
		#background { background-image: url(density-live.png); image-resolution: from-image; }
		#set_density { background-image: image-set(url(density-live.png) 3x); }
		#set_metadata { background-image: image-set(url(density-live.png)); }
		#set_override { background-image: image-set(url(density-live.png) 4x); image-resolution: 2x; }
		#set_preferred { background-image: image-set(url(density-live.png) 4x); image-resolution: from-image 2x; }
		#blend { background-image: cross-fade(white, url(density-live.png)); image-resolution: from-image; }
		#crop { background-image: image("density-live.png#xywh=0,0,24,12"); image-resolution: from-image; }
		@supports (image-resolution: from-image calc(96dpi + 1dppx) snap) {
			#dynamic { --density: calc(96dpi + 1dppx); image-resolution: var(--density) snap; }
		}
	</style>
	<img id="default" src="density-live.png">
	<div class="resolution_parent"><img id="inherited" src="density-default.png"></div>
	<img id="metadata" src="density-live.png" style="image-resolution: from-image 3x">
	<img id="fallback" src="density-default.png" style="image-resolution: from-image 3x">
	<img id="invalid_metadata" src="density-asymmetric.png" style="image-resolution: from-image 3x">
	<img id="override" src="density-live.png" style="image-resolution: 3x">
	<img id="snap" src="density-default.png" style="image-resolution: 260dpi snap">
	<img id="snap_low" src="density-default.png" style="image-resolution: snap 0.75x">
	<img id="fractional" src="density-default.png" style="image-resolution: 0.75x">
	<img id="vector" src="density-vector.svg" style="image-resolution: 3x">
	<img id="cascade" src="density-default.png">
	<img id="dynamic" src="density-default.png">
	<img id="jfif" src="density-jfif.jpg" style="image-resolution: from-image 2x">
	<img id="exif_jpeg" src="density-exif.jpg" style="image-resolution: from-image 2x">
	<img id="exif_webp" src="density-exif.webp" style="image-resolution: from-image 2x">
	<img id="fit" src="density-live.png">
	<div id="background" class="tile"></div><div id="set_density" class="tile"></div>
	<div id="set_metadata" class="tile"></div><div id="set_override" class="tile"></div>
	<div id="set_preferred" class="tile"></div><div id="blend" class="tile"></div><div id="crop" class="tile"></div>
	<img id="snap_exact" src="density-small.png" style="image-resolution: snap 0.5x">
	<img id="late_metadata" src="density-late.webp" style="image-resolution: from-image 2x">"""
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(360, 182)) == OK, "Lay out the image resolution fixture")
	var sizes: Dictionary[String, Vector2] = {
		"default": Vector2(48.0, 24.0),
		"inherited": Vector2(24.0, 12.0),
		"metadata": Vector2(24.0, 12.0),
		"fallback": Vector2(16.0, 8.0),
		"invalid_metadata": Vector2(16.0, 8.0),
		"override": Vector2(16.0, 8.0),
		"snap": Vector2(16.0, 8.0),
		"snap_low": Vector2(48.0, 24.0),
		"fractional": Vector2(64.0, 32.0),
		"vector": Vector2(48.0, 24.0),
		"cascade": Vector2(24.0, 12.0),
		"dynamic": Vector2(24.0, 12.0),
		"jfif": Vector2(16.0, 8.0),
		"exif_jpeg": Vector2(12.0, 6.0),
		"exif_webp": Vector2(12.0, 6.0),
		"snap_exact": Vector2(48.0, 24.0),
		"late_metadata": Vector2(24.0, 12.0),
	}
	for id: String in sizes:
		_check(_rect(layout, id).size.distance_to(sizes[id]) < 0.01,
			"Natural image resolution for %s is %s, expected %s" % [id, _rect(layout, id).size, sizes[id]])
	_check(_has_diagnostic(layout, "image-resolution", "snap", "style_element"), "Reject snap without a resolution source")
	var imported: HTMLDocument = ResourceLoader.load("{IMAGE_RESOLUTION_UID}") as HTMLDocument
	_check(imported != null and not FileAccess.file_exists("res://density-import.png"),
		"Load the imported density fixture without the original PNG")
	if imported != null:
		var saved: Dictionary = imported.get("_image_dpi")
		var dpi: float = float(saved.get("res://density-import.png", 0.0))
		_check(absf(dpi - 192.0) < 0.01, "The HTML importer preserves the image density")
		var imported_layout: HTMLLayout = imported.create_layout()
		_check(imported_layout.layout(Vector2i(100, 80)) == OK and
			_rect(imported_layout, "imported_density").size.distance_to(Vector2(24.0, 12.0)) < 0.01,
			"An imported image still uses its density after removal of the original PNG")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(360, 182)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(360.0, 182.0)
	label.document = document
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	var fit: Vector2 = _label_rect(label, "fit").position
	_check_pixel(output, fit + Vector2(3.0, 8.0), Color("#303030"), "Object-fit none uses the density-adjusted image width")
	_check_pixel(output, fit + Vector2(4.0, 8.0), Color.RED, "The smaller object is centered in its layout box")
	var tile_widths: Dictionary[String, int] = {
		"background": 24, "set_density": 16, "set_metadata": 24, "set_override": 24,
		"set_preferred": 12, "blend": 24, "crop": 12,
	}
	for id: String in tile_widths:
		var origin: Vector2 = _label_rect(label, id).position
		var expected: Color = Color("#ff8080") if id == "blend" else Color.RED
		_check_pixel(output, origin + Vector2(float(tile_widths[id] - 1), 1.0), expected, "Density-adjusted tile for %s" % id)
		_check_pixel(output, origin + Vector2(float(tile_widths[id]), 1.0), Color("#303030"), "Tile ends at the CSS width for %s" % id)
	viewport.queue_free()


func _check_image_orientation() -> void:
	var markup: String = """<!doctype html><style>
		body { margin: 0; display: grid; grid-template-columns: repeat(5, 52px); grid-auto-rows: 42px;
			align-items: start; justify-items: start; }
		img { display: block; }
		.tile { width: 42px; height: 39px; background: #303030 no-repeat; }
		#invalid { image-orientation: none; image-orientation: 90deg flip flip; }
		#fit { width: 32px; height: 34px; object-fit: none; background: #303030; }
		#crop { background-image: image("orientation-6.png#xywh=0,0,10,16"); }
		#decorative { background-image: image-set(url("orientation-6.png") 1x); image-orientation: 180deg; }
		#decorative_none { background-image: cross-fade(white, url("orientation-6.png")); image-orientation: none; }
		#generated::before { content: url("orientation-6.png"); image-orientation: none; }
	</style>"""
	for orientation: int in range(1, 9):
		markup += '<img id="exif%d" src="orientation-%d.png">' % [orientation, orientation]
	markup += """
	<div style="image-orientation: none"><img id="inherited" src="orientation-6.png"></div>
	<img id="invalid" src="orientation-6.png">
	<img id="manual" src="orientation-6.png" style="--angle: calc(50grad + 0turn); image-orientation: flip var(--angle)">
	<img id="round_up" src="orientation-6.png" style="image-orientation: -45deg">
	<img id="manual_flip" src="orientation-6.png" style="image-orientation: flip">
	<img id="density" src="orientation-6.png" style="image-resolution: from-image">
	<img id="late" src="orientation-late.png">
	<img id="fit" src="orientation-6.png">
	<div id="crop" class="tile"></div>
	<div id="decorative" class="tile"></div>
	<div id="decorative_none" class="tile"></div>
	<div id="generated"></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(markup) == OK and layout.layout(Vector2i(260, 210)) == OK, "Lay out image orientation fixtures")
	for orientation: int in range(1, 9):
		var extent: Vector2 = Vector2(32.0, 20.0) if orientation <= 4 else Vector2(20.0, 32.0)
		_check(_rect(layout, "exif%d" % orientation).size.is_equal_approx(extent),
			"EXIF orientation %d uses the oriented natural size" % orientation)
	for id: String in ["inherited", "invalid", "round_up", "manual_flip", "late", "generated"]:
		_check(is_equal_approx(_rect(layout, id).size.x, 32.0), "%s uses the encoded width" % id)
	_check(_rect(layout, "density").size.distance_to(Vector2(10.0, 16.0)) < 0.01,
		"Density and orientation both apply when pHYs precedes eXIf")
	_check(_has_diagnostic(layout, "image-orientation", "90deg flip flip", "style_element"),
		"An invalid image-orientation declaration preserves the earlier winner")

	var imported: HTMLDocument = ResourceLoader.load("{IMAGE_ORIENTATION_UID}") as HTMLDocument
	_check(imported != null and not FileAccess.file_exists("res://orientation-import.png"),
		"Load the imported orientation fixture without the source PNG")
	if imported != null:
		var imported_layout: HTMLLayout = imported.create_layout()
		_check(imported_layout.layout(Vector2i(100, 80)) == OK and
			_rect(imported_layout, "imported_orientation").size.is_equal_approx(Vector2(20.0, 32.0)),
			"Imported HTML retains image orientation alongside its texture dependency")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(260, 210)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(260.0, 210.0)
	label.text = markup
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	var corners: Array[String] = [
		"rgby", "gryb", "ybgr", "byrg", "rbgy", "bryg", "ygbr", "gyrb",
	]
	var palette: Dictionary[String, Color] = {
		"r": Color.RED,
		"g": Color.GREEN,
		"b": Color.BLUE,
		"y": Color.YELLOW,
	}
	for orientation: int in range(1, 10):
		var id: String = "exif%d" % orientation if orientation <= 8 else "manual"
		var box: Rect2 = _label_rect(label, id)
		var corner_order: String = corners[orientation - 1] if orientation <= 8 else corners[4]
		for corner: int in range(4):
			var x: float = 3.0 if corner % 2 == 0 else box.size.x - 4.0
			var y: float = 3.0 if corner < 2 else box.size.y - 4.0
			_check_pixel(output, box.position + Vector2(x, y), palette[corner_order[corner]],
				"%s paints the oriented corner %d" % [id, corner])
	var fit: Vector2 = _label_rect(label, "fit").position
	_check_pixel(output, fit + Vector2(5.0, 3.0), Color("#303030"), "Object-fit uses the rotated width")
	_check_pixel(output, fit + Vector2(8.0, 3.0), Color.BLUE, "Object-fit centers the oriented pixels")
	var crop: Vector2 = _label_rect(label, "crop").position
	_check_pixel(output, crop + Vector2(4.0, 4.0), Color.BLUE, "Image fragments are cropped after orientation")
	_check_pixel(output, crop + Vector2(10.0, 4.0), Color("#303030"), "The cropped tile keeps the fragment dimensions")
	var decorative: Vector2 = _label_rect(label, "decorative").position
	_check_pixel(output, decorative + Vector2(4.0, 4.0), Color.BLUE, "Decorative image-set uses EXIF despite a legacy angle")
	var blend: Vector2 = _label_rect(label, "decorative_none").position
	_check_pixel(output, blend + Vector2(4.0, 4.0), Color("#ff8080"), "Cross-fade uses the original pixels with image-orientation none")
	_check_pixel(output, blend + Vector2(24.0, 3.0), Color("#80ff80"), "Cross-fade sizes its tile from the unrotated source")
	viewport.queue_free()
	_check_encoded_image_orientation()


func _check_encoded_image_orientation() -> void:
	var pixels: Image = Image.create_empty(32, 20, false, Image.FORMAT_RGB8)
	pixels.fill(Color.RED)
	var tiff: PackedByteArray = [0x49, 0x49, 42, 0, 8, 0, 0, 0, 1, 0,
		0x12, 0x01, 3, 0, 1, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0,]
	var jpeg_source: PackedByteArray = pixels.save_jpg_to_buffer()
	var jpeg: PackedByteArray = [0xff, 0xd8, 0xff, 0xe1, 0, 34, 69, 120, 105, 102, 0, 0,]
	jpeg.append_array(tiff)
	jpeg.append_array(jpeg_source.slice(2))

	var webp_source: PackedByteArray = pixels.save_webp_to_buffer()
	var webp: PackedByteArray = webp_source.slice(0, 12)
	var vp8x: PackedByteArray = [86, 80, 56, 88, 10, 0, 0, 0, 8, 0, 0, 0, 31, 0, 0, 19, 0, 0,]
	var exif: PackedByteArray = [69, 88, 73, 70, 26, 0, 0, 0,]
	webp.append_array(vp8x)
	webp.append_array(exif)
	webp.append_array(tiff)
	webp.append_array(webp_source.slice(12))
	webp.encode_u32(4, webp.size() - 8)

	var fixtures: Dictionary[String, PackedByteArray] = {
		"orientation.jpg": jpeg,
		"orientation.webp": webp,
	}
	var textures: Dictionary[String, Texture2D] = {}
	var markup: String = '<!doctype html><style>img { display: block; }</style>'
	for name: String in fixtures:
		var fixture_path: String = "res://" + name
		var file: FileAccess = FileAccess.open(fixture_path, FileAccess.WRITE)
		var decoded: Image = Image.new()
		var error: Error = decoded.load_jpg_from_buffer(fixtures[name]) if name.ends_with(".jpg") else decoded.load_webp_from_buffer(fixtures[name])
		if file == null or not file.store_buffer(fixtures[name]) or error != OK:
			_check(false, "Read and decode the EXIF fixture for %s" % name)
			return
		file.close()
		_check(decoded.get_size() == Vector2i(32, 20), "Godot decodes %s in its encoded orientation" % name)
		textures[fixture_path] = ImageTexture.create_from_image(decoded)
		markup += '<img id="%s" src="%s">' % [name, name]
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_textures", textures)
	document.text = markup
	var layout: HTMLLayout = document.create_layout()
	_check(layout.layout(Vector2i(100, 100)) == OK, "Lay out real JPEG and WebP EXIF images")
	for name: String in fixtures:
		_check(_rect(layout, name).size.is_equal_approx(Vector2(20.0, 32.0)),
			"Image orientation uses the EXIF tag in %s" % name)


func _check_border_image() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: #303030; display: grid; grid-template-columns: repeat(5, 72px);
			grid-auto-rows: 60px; justify-items: start; align-items: start; }
		.box { width: 26px; height: 24px; margin: 6px; border: 4px solid #008855;
			background-color: #303030; image-rendering: crisp-edges; }
		#stretch { border-image: url(border-image.png) 4; }
		#fill { border-image: fill calc(100% / 3) / 2 / 2px round stretch url(border-image.png); border-radius: 9px; }
		#repeat { border-image: url(border-image.png) 4 repeat stretch; }
		#space { border-image: url(border-image.png) 4 / 8px space stretch; }
		#round { border-image: url(border-image.png) 4 / 8px round; }
		#no_room { border-image: url(border-image.png) 4 / 15px space; }
		#fallback { border-image: url(missing-border-image.png) 4; }
		#reset { border-image: url(border-image.png) 4 fill; border: 4px solid #008855; }
		#variable { --frame: url(border-image.png) 4 fill / auto; border-image: var(--frame);
			border-image-slice: bogus; border-image-width: calc(4px + 2%) 4px; }
		#outset_only { border-image: url(border-image.png) 4 // 2; }
		#oriented { border-image: url(orientation-6.png) 50% / 4px; }
		#generated { --source: linear-gradient(red, red); border-image: var(--source) 25% / 4px; }
		#absolute_width { border: none; border-image: url(border-image.png) 4 / 4px; }
		#translated { border-image: url(border-image.png) 4; transform: translate(10px, 4px); }
		.wrap { width: 70px; margin: 8px; font: 14px monospace; line-height: 24px; }
		#longhands { border-image-source: url(border-image.png); border-image-slice: 4 fill;
			border-image-width: 4px; border-image-outset: 3px; border-image-repeat: stretch; border-top: 4px dashed blue; }
		#hires { image-resolution: 2x; border-image: url(border-image.png) 4 / auto; }
		#clip_frame { width: 32px; height: 32px; margin: 6px; overflow: hidden; border-radius: 12px; }
		#clipped { width: 24px; height: 24px; border: 4px solid black;
			border-image: url(border-image.png) 4 fill / 1 / 3px; image-rendering: crisp-edges; }
		#wrapped { border: 4px solid transparent; border-image: url(border-image.png) 4 stretch;
			image-rendering: crisp-edges; color: transparent; }
	</style>
	<div class="box" id="stretch"></div><div class="box" id="fill"></div>
	<div class="box" id="repeat"></div><div class="box" id="space"></div><div class="box" id="round"></div>
	<div class="box" id="fallback"></div><div class="box" id="reset"></div><div class="box" id="variable"></div>
	<div class="box" id="outset_only"></div><div class="box" id="oriented"></div><div class="box" id="generated"></div>
	<div class="box" id="absolute_width"></div><div class="box" id="translated"></div><div class="box" id="no_room"></div>
	<div class="wrap"><span id="wrapped">AAAAAA<br>BBBBBB</span></div>
	<div class="box" id="longhands"></div><div class="box" id="hires"></div>
	<div id="clip_frame"><div id="clipped"></div></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(360, 240)) == OK, "Lay out border image fixture")
	_check(_rect(layout, "fill").size.is_equal_approx(_rect(layout, "stretch").size),
		"Border image width and outset preserve CSS box geometry")
	var fill: Rect2 = _rect(layout, "fill")
	var overhang_hit: HTMLElement = layout.get_element_at(fill.position - Vector2(1.0, 1.0))
	_check(overhang_hit == null or overhang_hit.get_attribute("id") != "fill", "Border image outset does not receive hits")
	_check(_has_diagnostic(layout, "border-image-slice", "bogus", "style_element"), "Invalid slices use CSS diagnostics")

	var imported: HTMLDocument = ResourceLoader.load("{BORDER_IMAGE_UID}") as HTMLDocument
	_check(imported != null and not FileAccess.file_exists("res://border-import.png"),
		"Load the border image resource without the original CSS and PNG")
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(420, 240)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(360.0, 240.0)
	label.text = source
	viewport.add_child(label)
	var imported_label: HTMLLabel = HTMLLabel.new()
	imported_label.position = Vector2(370.0, 0.0)
	imported_label.size = Vector2(40.0, 40.0)
	imported_label.document = imported
	viewport.add_child(imported_label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	var samples: Array[Array] = [
		["stretch", Vector2(1.0, 1.0), Color.RED, "Top-left corner"],
		["stretch", Vector2(33.0, 1.0), Color.BLUE, "Top-right corner"],
		["stretch", Vector2(1.0, 30.0), Color.YELLOW, "Bottom-left corner"],
		["stretch", Vector2(33.0, 30.0), Color.CYAN, "Bottom-right corner"],
		["stretch", Vector2(16.0, 16.0), Color("#303030"), "Center is omitted without fill"],
		["stretch", Vector2(28.0, 1.0), Color.WHITE, "Stretch preserves the full source edge"],
		["fill", Vector2(16.0, 16.0), Color("#ee8200"), "Fill paints over the CSS background"],
		["fill", Vector2(-1.0, -1.0), Color.RED, "Outset paints beyond even a rounded border box"],
		["repeat", Vector2(4.0, 1.0), Color.MAGENTA, "Repeat centers a tile and crops the first repetition"],
		["repeat", Vector2(5.0, 1.0), Color.WHITE, "Repeat reaches the second region of a partial tile"],
		["repeat", Vector2(7.0, 1.0), Color.MAGENTA, "A new repeat starts at the centered phase"],
		["space", Vector2(8.0, 1.0), Color("#303030"), "Space inserts a leading gap"],
		["space", Vector2(9.0, 1.0), Color.MAGENTA, "Space keeps a whole tile"],
		["space", Vector2(25.0, 1.0), Color("#303030"), "Space gives the trailing gap the same width"],
		["no_room", Vector2(16.0, 1.0), Color("#303030"), "Space drops partial side tiles"],
		["round", Vector2(16.0, 1.0), Color.WHITE, "Round scales the first tile to half the side"],
		["round", Vector2(17.0, 1.0), Color.MAGENTA, "Round starts the second tile with no gap"],
		["fallback", Vector2(1.0, 1.0), Color("#008855"), "Missing source uses the ordinary border"],
		["reset", Vector2(1.0, 1.0), Color("#008855"), "The border shorthand resets border-image"],
		["variable", Vector2(16.0, 16.0), Color("#ee8200"), "Variable shorthand and longhand both cascade"],
		["outset_only", Vector2(-6.0, -5.0), Color.RED, "Double slash and border-width multipliers"],
		["oriented", Vector2(1.0, 1.0), Color.BLUE, "Nine-slice uses the oriented source"],
		["generated", Vector2(1.0, 1.0), Color.RED, "Gradient border uses a generated image source"],
		["absolute_width", Vector2(1.0, 1.0), Color.RED, "Absolute image width can paint with no ordinary border"],
		["translated", Vector2(1.0, 1.0), Color.RED, "Border image follows the element transform"],
		["longhands", Vector2(-1.0, -1.0), Color.RED, "Longhands survive a single-side border declaration"],
		["longhands", Vector2(16.0, 16.0), Color("#ee8200"), "Independent longhands paint the center"],
		["hires", Vector2(1.0, 1.0), Color.RED, "Auto width uses the natural density-adjusted slice size"],
		["hires", Vector2(2.0, 2.0), Color("#303030"), "A density of two halves the four-pixel auto border width"],
		["clipped", Vector2(0.0, 0.0), Color("#303030"), "An ancestor's rounded clip masks the outset image"],
		["clipped", Vector2(16.0, 16.0), Color("#ee8200"), "The filled center remains inside the ancestor clip"],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var color: Color = sample[2]
		var description: String = sample[3]
		_check_pixel(output, _label_rect(label, id).position + offset, color, description)
	var inline_element: HTMLElement = label.get_element_by_id("wrapped")
	var fragments: Array[Rect2] = inline_element.get_client_rects()
	_check(fragments.size() == 2, "The bordered inline wraps into two fragments")
	if fragments.size() == 2:
		_check_pixel(output, fragments[0].position + Vector2(1.0, 1.0), Color.RED, "The first inline fragment has a corner")
		_check_pixel(output, fragments[0].position + Vector2(fragments[0].size.x - 3.0, 1.0), Color.MAGENTA,
			"The first inline fragment uses the first half of the continuous edge")
		_check_pixel(output, fragments[1].position + Vector2(3.0, 1.0), Color.WHITE,
			"The second inline fragment continues the edge without restarting it")
	if imported != null:
		_check_pixel(output, imported_label.position + Vector2(1.0, 1.0), Color.RED,
			"An imported stylesheet's border image paints from the retained texture")
	viewport.queue_free()


func _check_outlines() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: #303030; display: grid; grid-template-columns: repeat(5, 80px);
			grid-auto-rows: 64px; justify-items: start; align-items: start; }
		.box { width: 32px; height: 24px; margin: 12px; background: blue; }
		#positive { outline-offset: calc(2px + 2px); color: yellow; outline: solid calc(1px + 2px); overflow: hidden; }
		#negative { position: relative; outline: 3px solid red; outline-offset: -6px; }
		#negative > div { position: absolute; inset: 0; background: blue; }
		#negative_limit { outline: 3px solid lime; outline-offset: -1000px; }
		#rounded { border-radius: 8px; outline: 2px solid magenta; outline-offset: 2px; }
		#double { --outline: 6px double cyan; outline: var(--outline); outline-offset: 2px; }
		#dashed { outline: 2px dashed yellow; }
		#auto { outline: 9px auto lime; }
		#accent { outline: auto; }
		#invalid { outline: 2px solid red; outline: 9px auto auto auto; outline-style: hidden; outline-offset: 7%; }
		#transformed { outline: 2px solid yellow; transform: translate(8px, 4px) scale(1.5); }
		#wrap { margin: 10px; width: 58px; font: 14px monospace; line-height: 22px; }
		#inline_link { outline: 2px solid red; text-decoration: none; color: white; }
		#clip { width: 32px; height: 28px; margin: 12px; overflow: hidden; border-radius: 9px; }
		#clipped { width: 26px; height: 20px; margin: 2px; background: blue; outline: 3px solid red; }
		#focus { margin: 12px; background: blue; box-shadow: none; }
		#focus:focus-visible { outline: 2px solid white; outline-offset: 2px; box-shadow: none; }
		#parent { outline-width: thick; outline-style: solid; outline-color: currentColor; color: yellow; outline-offset: -2px; }
		#inherits { width: 14px; height: 12px; position: relative; left: 8px; top: 6px; color: lime;
			outline-width: inherit; outline-style: inherit; outline-color: inherit; outline-offset: inherit; }
	</style>
	<div class="box" id="positive"></div><div class="box" id="negative"><div></div></div>
	<div class="box" id="negative_limit"></div><div class="box" id="rounded"></div><div class="box" id="double"></div>
	<div class="box" id="dashed"></div><div class="box" id="auto"></div><div class="box" id="accent"></div>
	<div class="box" id="invalid"></div><div class="box" id="transformed"></div>
	<div id="wrap"><a id="inline_link" href="#positive">ABCD<br>EFGH</a></div>
	<div id="clip"><div id="clipped"></div></div>
	<button id="focus" type="button">Focus</button>
	<div class="box" id="parent"><div id="inherits"></div></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(400, 192)) == OK, "Lay out CSS outlines")
	var positive: HTMLElement = layout.get_element_by_id("positive")
	var positive_box: Rect2 = positive.get_bounding_rect()
	var content_size: Vector2i = layout.get_content_size()
	_check(positive_box.size.is_equal_approx(Vector2(32.0, 24.0)), "Outline width never contributes to box geometry")
	var hit: HTMLElement = layout.get_element_at(positive_box.position + Vector2(8.0, -6.0))
	_check(hit == null or hit.get_attribute("id") != "positive", "Outline ink is outside the hit area")
	_check(positive.set_attribute("style", "outline-offset: 1000px") == OK and layout.layout(Vector2i(400, 192)) == OK,
		"Update the outline offset")
	_check(layout.get_content_size() == content_size, "A large outline does not expand the scrollable range")
	_check(_has_diagnostic(layout, "outline-style", "hidden", "style_element") and
		_has_diagnostic(layout, "outline-offset", "7%", "style_element") and
		_has_diagnostic(layout, "outline", "9px auto auto auto", "style_element"), "Invalid outlines use CSS diagnostics")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(400, 192)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(400.0, 192.0)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	var samples: Array[Array] = [
		["positive", Vector2(12.0, -6.0), Color.YELLOW, "Positive offset, math width and currentColor"],
		["positive", Vector2(12.0, -2.0), Color("#303030"), "Positive offset leaves a gap"],
		["negative", Vector2(12.0, 2.0), Color.BLUE, "Negative offset leaves the outside clear"],
		["negative", Vector2(12.0, 3.0), Color.RED, "Negative outline paints over positioned descendants"],
		["negative", Vector2(12.0, 6.0), Color.BLUE, "The inside of the negative outline stays clear"],
		["negative_limit", Vector2(16.0, 12.0), Color.LIME, "Large negative offsets keep a visible shape"],
		["negative_limit", Vector2(12.0, 12.0), Color.BLUE, "Large negative offsets clamp around the center"],
		["rounded", Vector2(-4.0, -4.0), Color("#303030"), "The rounded corner cuts away the outset corner"],
		["rounded", Vector2(16.0, -3.0), Color.MAGENTA, "The rounded outline preserves its width along straight edges"],
		["double", Vector2(16.0, -8.0), Color.CYAN, "Double outline outer stroke"],
		["double", Vector2(16.0, -6.0), Color("#303030"), "Double outline gap"],
		["double", Vector2(16.0, -4.0), Color.CYAN, "Double outline inner stroke"],
		["auto", Vector2(16.0, -2.0), Color.LIME, "Auto pattern uses a solid focus outline with author color"],
		["auto", Vector2(16.0, -3.0), Color("#303030"), "Auto pattern uses its own thickness"],
		["invalid", Vector2(16.0, -1.0), Color.RED, "Invalid shorthand and longhands preserve the earlier value"],
		["transformed", Vector2(24.0, -2.0), Color.YELLOW, "Outline follows the visual transform"],
		["clipped", Vector2(-1.0, -1.0), Color("#303030"), "Ancestor rounded overflow clips the outline"],
		["clipped", Vector2(10.0, -1.0), Color.RED, "The rest of the outline remains visible inside the clip"],
		["inherits", Vector2(7.0, 0.0), Color.LIME, "Inherited outline color resolves against the new currentColor"],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var color: Color = sample[2]
		var description: String = sample[3]
		_check_pixel(output, _label_rect(label, id).position + offset, color, description)
	var accent: Color = DisplayServer.get_accent_color()
	if is_zero_approx(accent.a):
		accent = Color("#2563eb")
	_check_pixel(output, _label_rect(label, "accent").position + Vector2(16.0, -1.0), accent,
		"Lone auto uses the display's accent color or Dockit's fallback")

	var dash: Vector2 = _label_rect(label, "dashed").position
	var yellow_pixels: int
	for x: int in range(32):
		if output.get_pixelv(Vector2i(dash) + Vector2i(x, -1)).is_equal_approx(Color.YELLOW):
			yellow_pixels += 1
	_check(yellow_pixels >= 4 and yellow_pixels <= 29, "Dashes alternate strokes and gaps")
	var link: HTMLElement = label.get_element_by_id("inline_link")
	var link_box: Rect2 = link.get_bounding_rect()
	_check(link.get_client_rects().size() == 2, "The outline example wraps into two link fragments")
	_check_pixel(output, link_box.position + Vector2(-1.0, link_box.size.y / 2.0), Color.RED,
		"Wrapped links have a single connected enclosure")

	var button: HTMLElement = label.get_element_by_id("focus")
	var button_box: Rect2 = button.get_bounding_rect()
	var focus_sample: Vector2 = button_box.position + Vector2(button_box.size.x / 2.0, -3.0)
	_check_pixel(output, focus_sample, Color("#303030"), "Focus outline is clear before keyboard input")
	label.grab_focus()
	var tab: InputEventKey = InputEventKey.new()
	tab.keycode = KEY_TAB
	tab.pressed = true
	viewport.push_input(tab)
	_check(link.matches(":focus-visible"), "Tab first reaches the link in document order")
	viewport.push_input(tab)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check(button.matches(":focus-visible"), "Tab matches the button's focus-visible rule")
	_check(button.get_bounding_rect().is_equal_approx(button_box), "Focus outlines preserve the button's dimensions")
	_check_pixel(viewport.get_texture().get_image(), focus_sample, Color.WHITE, "Keyboard focus paints the requested outline")
	viewport.queue_free()


func _check_background_tiling() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: #303030; display: grid; grid-template-columns: repeat(5, 96px);
			grid-auto-rows: 60px; align-items: start; justify-items: start; }
		.tile { width: 44px; height: 44px; background-color: #303030; image-rendering: crisp-edges; }
		#spaced { background: #303030 url(density-live.png) 6px 11px / 8px 8px space no-repeat; }
		#few { width: 13px; background-image: url(density-live.png); background-size: 8px 8px;
			background-position: right 2px; background-repeat: space no-repeat; }
		#oversized { width: 13px; background-image: url(orientation-1.png); background-size: 20px 10px;
			background-position: right top; background-repeat: space no-repeat; }
		#both_space { background-image: url(density-live.png); background-size: 8px 10px; background-repeat: space; }
		#round_x { background-image: url(density-live.png); background-size: calc(8px + 8px) auto;
			background-repeat: round no-repeat; }
		#round_y { height: 43px; background-image: url(density-live.png); background-size: auto 16px;
			background-repeat: no-repeat round; }
		#round_both { height: 42px; background: url(orientation-1.png) left top / 16px 16px round; }
		#round_position { width: 54px; background-image: url(orientation-1.png); background-size: 17px 10px;
			background-position: 25% 0%; background-repeat: round no-repeat; background-repeat: repeat-x space; }
		#gradient { background: linear-gradient(to right, red 50%, blue 50%) left top / 8px 10px space no-repeat; }
		#fallback { background-image: image("missing-tile.png", red); background-size: 8px 8px;
			background-repeat-x: space; background-repeat-y: no-repeat; }
		#blend { background: cross-fade(white, url(density-live.png)) left top / 8px 8px space no-repeat; }
		#layers { background-image: image(red), image(lime), image(blue); background-size: 8px 7px, 10px 7px;
			background-position: 0 0, 0 12px, 0 24px; background-repeat-x: space, no-repeat; background-repeat-y: no-repeat; }
		#padding { background: url(density-live.png) left top / 8px 8px space no-repeat content-box border-box;
			padding: 0 6px; border: 2px solid transparent; }
		#var { --modes: space no-repeat; background-image: image(red); background-size: 8px 10px;
			background-repeat: var(--modes); background-repeat-block: space; background-repeat-x: repeat-y; }
		#alias { background: image(red) 0 0 / 8px 8px repeat-block; }
		#reset { background-repeat-inline: no-repeat; background-repeat-y: no-repeat;
			background: image(red) 0 0 / 8px 8px; }
		#math { background: image(red) calc(100% - 2px) calc(3px + 3px) / calc(4px + 4px) 8px no-repeat; }
		#clipped { border-radius: 10px; background: url(density-live.png) 0 0 / 8px 10px space no-repeat; }
		#flipped { direction: rtl; background: image(ltr "orientation-1.png") 0 0 / 8px 8px space no-repeat; }
		#fractional { background: image(red) 0 0 / calc(100% / 3) 8px space no-repeat; }
		#zero { background: image(red) 0 0 / 0px 10px space; }
	</style>
	<div class="tile" id="spaced"></div><div class="tile" id="few"></div><div class="tile" id="oversized"></div>
	<div class="tile" id="both_space"></div><div class="tile" id="round_x"></div><div class="tile" id="round_y"></div>
	<div class="tile" id="round_both"></div><div class="tile" id="round_position"></div><div class="tile" id="gradient"></div>
	<div class="tile" id="fallback"></div><div class="tile" id="blend"></div><div class="tile" id="layers"></div>
	<div class="tile" id="padding"></div><div class="tile" id="var"></div><div class="tile" id="alias"></div>
	<div class="tile" id="reset"></div><div class="tile" id="math"></div><div class="tile" id="clipped"></div>
	<div class="tile" id="flipped"></div><div class="tile" id="zero"></div>
	<div class="tile" id="fractional"></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(480, 300)) == OK, "Lay out spaced and rounded backgrounds")
	_check(_has_diagnostic(layout, "background-repeat", "repeat-x space", "style_element") and
		_has_diagnostic(layout, "background-repeat-x", "repeat-y", "style_element"), "Mixed directional keywords are rejected")
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(480, 300)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(480.0, 300.0)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	var clear: Color = Color("#303030")
	var samples: Array[Array] = [
		["spaced", Vector2(0.0, 12.0), Color.RED, "Space anchors the first tile and ignores horizontal position"],
		["spaced", Vector2(8.0, 12.0), clear, "Space shows a horizontal gap"],
		["spaced", Vector2(43.0, 18.0), Color.RED, "The last tile touches the positioning area's far edge"],
		["spaced", Vector2(9.0, 19.0), clear, "The vertical no-repeat axis honors its position"],
		["few", Vector2(4.0, 3.0), clear, "A single spaced image honors the requested position"],
		["few", Vector2(5.0, 3.0), Color.RED, "The fallback tile touches the right edge"],
		["oversized", Vector2(0.0, 2.0), Color.RED, "An oversized tile can be clipped on the fallback axis"],
		["oversized", Vector2(12.0, 2.0), Color.GREEN, "The oversized image is aligned at the requested edge"],
		["both_space", Vector2(43.0, 43.0), Color.RED, "Space fills both dimensions to the last edges"],
		["both_space", Vector2(7.0, 10.0), clear, "Space creates an independent vertical gap"],
		["round_x", Vector2(43.0, 6.0), Color.RED, "Round fits three tiles across the full width"],
		["round_x", Vector2(43.0, 7.0), clear, "An automatic height follows the rounded tile width"],
		["round_y", Vector2(27.0, 42.0), Color.RED, "Round fits a whole number of tiles vertically"],
		["round_y", Vector2(29.0, 42.0), clear, "An automatic width follows the rounded tile height"],
		["round_both", Vector2(14.0, 13.0), Color.YELLOW, "Round keeps fractional horizontal tile edges"],
		["round_both", Vector2(15.0, 14.0), Color.RED, "The second tile starts in both axes"],
		["round_position", Vector2(0.0, 1.0), Color.GREEN, "Round uses the scaled size to resolve position"],
		["round_position", Vector2(10.0, 1.0), Color.RED, "The rounded repeat begins at the resolved position"],
		["gradient", Vector2(7.0, 1.0), Color.BLUE, "Gradient sizing uses the unspaced tile width"],
		["gradient", Vector2(8.0, 1.0), clear, "A gradient tile leaves the same gap as a texture"],
		["gradient", Vector2(9.0, 1.0), Color.RED, "A gradient restarts after its gap"],
		["fallback", Vector2(8.0, 1.0), clear, "A solid fallback leaves the same gap"],
		["fallback", Vector2(9.0, 1.0), Color.RED, "A solid fallback fills a spaced tile"],
		["blend", Vector2(8.0, 1.0), clear, "A cross-fade leaves each gap transparent"],
		["blend", Vector2(9.0, 1.0), Color("#ff8080"), "A cross-fade repeats the blended tile"],
		["layers", Vector2(9.0, 3.0), Color.RED, "First layer uses the first horizontal repetition value"],
		["layers", Vector2(11.0, 13.0), clear, "Second layer uses the second horizontal repetition value"],
		["layers", Vector2(43.0, 25.0), Color.BLUE, "Third layer cycles the horizontal list independently"],
		["padding", Vector2(0.0, 3.0), Color.RED, "Space extends its pattern past a narrower positioning area"],
		["padding", Vector2(7.0, 3.0), clear, "The extended pattern preserves its gap"],
		["padding", Vector2(8.0, 3.0), Color.RED, "Space uses the content edge as its origin"],
		["var", Vector2(8.0, 20.0), clear, "The winning horizontal component still comes from var()"],
		["var", Vector2(9.0, 20.0), Color.RED, "The logical block longhand overrides only the vertical component"],
		["alias", Vector2(3.0, 16.0), Color.RED, "Repeat-block tiles vertically in horizontal writing"],
		["alias", Vector2(9.0, 16.0), clear, "Repeat-block places only one tile horizontally"],
		["reset", Vector2(19.0, 16.0), Color.RED, "Background shorthand restores repeat on both axes"],
		["math", Vector2(33.0, 7.0), clear, "Math positions keep the gap before a tile"],
		["math", Vector2(34.0, 7.0), Color.RED, "Math positions and sizes resolve against their correct bases"],
		["clipped", Vector2(0.0, 0.0), clear, "Rounded clipping masks the gapped image shader"],
		["clipped", Vector2(3.0, 8.0), Color.RED, "Spaced tiles paint inside the rounded clip"],
		["flipped", Vector2(0.0, 1.0), Color.GREEN, "Direction flips each tile's pixels"],
		["flipped", Vector2(8.0, 1.0), clear, "A flipped image still leaves its gap at the same position"],
		["flipped", Vector2(9.0, 1.0), Color.GREEN, "Each spaced directional tile starts on its flipped side"],
		["zero", Vector2(0.0, 1.0), clear, "A zero-size tile paints nothing"],
		["fractional", Vector2(20.0, 1.0), Color.RED, "Space keeps three mathematically exact fractional tiles"],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var expected: Color = sample[2]
		var description: String = sample[3]
		_check_pixel(output, _label_rect(label, id).position + offset, expected, description)
	viewport.queue_free()


func _check_image_positions() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: #303030; display: grid; grid-template-columns: repeat(5, 96px);
			grid-auto-rows: 66px; align-items: start; justify-items: start; }
		.tile { width: 56px; height: 48px; background-color: #303030;
			background-image: image(red); background-size: 10px 8px;
			background-repeat: no-repeat; image-rendering: crisp-edges; }
		#four { background-position: right 12px bottom 8px; }
		#reversed { background: image(lime) bottom 25% right calc(10% + 1.4px) / 10px 8px no-repeat #303030; }
		#three { background-position: bottom 5px left; }
		#center_right { background-position: center right 8px; }
		#negative_free_space { background-image: url(orientation-1.png); background-size: 64px 16px;
			background-position: right 25% center; }
		#negative_offset { background-position: left -3px bottom -2px; }
		#legacy_pair { background-position: right 7px; background-position: top 7px; }
		#layers { background-image: image(red), image(lime), image(blue); background-size: 8px 6px;
			background-position-x: right 4px, left 12px; background-position-y: top 4px, center, bottom 3px; }
		#var { --position: right 9px bottom 4px; background-position: var(--position);
			background-position-x: left 11px !important; background-position-y: right 1px; }
		#invalid { background-position: right 6px top 5px;
			background-position: left 2px right 3px; background-position-x: 6px right; }
		#logical_pair { direction: rtl; background-position: x-start y-end; }
		#logical_axes { direction: rtl; background-position-inline: start 3px; background-position-block: end 6px; }
		#logical_offset { direction: rtl; background-position-inline: 6px; background-position-block: 7px; }
		#logical_four { direction: rtl; background-position: block-end 4px inline-end 8px; }
		#generic_four { direction: rtl; background-position: end 4px start 2px; }
		#logical_initial { direction: rtl; background-position: left top; background-position-inline: initial; }
		#parent { direction: rtl; background-position: inline-start 4px block-end 8px; }
		#inherited { direction: ltr; background-position: inherit; }
		#round { width: 60px; background: url(orientation-1.png) right 4px bottom 3px / 24px 10px round no-repeat; }
		#space { width: 16px; background-position: right 2px bottom 4px; background-repeat: space no-repeat; }
		#origin { padding: 5px; border: 2px solid transparent; background-origin: content-box;
			background-position: right 12px bottom 8px; }
		#object { width: 56px; height: 48px; background: #303030; image-resolution: 2x;
			image-rendering: crisp-edges; direction: rtl; object-fit: none; object-position: inline-end 6px block-end 7px; }
		#reset { background-position-x: right 8px; background-position-block: end;
			background: image(red) no-repeat; background-size: 10px 8px; }
		#incompatible { background-position: left 9px top 6px; background-position: left block-end; }
		#logical_var { direction: rtl; --pos: start 3px; background-position-inline: var(--pos);
			background-position-block: var(--missing, end 5px); }
	</style>
	<div class="tile" id="four"></div><div class="tile" id="reversed"></div><div class="tile" id="three"></div>
	<div class="tile" id="center_right"></div><div class="tile" id="negative_free_space"></div>
	<div class="tile" id="negative_offset"></div><div class="tile" id="legacy_pair"></div><div class="tile" id="layers"></div>
	<div class="tile" id="var"></div><div class="tile" id="invalid"></div>
	<div class="tile" id="logical_pair"></div><div class="tile" id="logical_axes"></div><div class="tile" id="logical_offset"></div>
	<div class="tile" id="logical_four"></div><div class="tile" id="generic_four"></div>
	<div id="parent"><div class="tile" id="inherited"></div></div>
	<div class="tile" id="logical_initial"></div><div class="tile" id="round"></div><div class="tile" id="space"></div>
	<div class="tile" id="origin"></div><img id="object" src="orientation-1.png">
	<div class="tile" id="reset"></div><div class="tile" id="incompatible"></div><div class="tile" id="logical_var"></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(480, 330)) == OK, "Lay out edge offsets and position longhands")
	_check(_has_diagnostic(layout, "background-position", "top 7px", "style_element") and
		_has_diagnostic(layout, "background-position", "left 2px right 3px", "style_element") and
		_has_diagnostic(layout, "background-position", "left block-end", "style_element") and
		_has_diagnostic(layout, "background-position-y", "right 1px", "style_element") and
		_has_diagnostic(layout, "background-position-x", "6px right", "style_element"),
		"Conflicting axes, mixed keyword families, and reversed offsets produce diagnostics")
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(480, 330)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(480.0, 330.0)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	var clear: Color = Color("#303030")
	var samples: Array[Array] = [
		["four", Vector2(34.0, 32.0), Color.RED, "Offsets are measured inward from right and bottom"],
		["four", Vector2(33.0, 32.0), clear, "The tile begins exactly at the right offset"],
		["reversed", Vector2(40.0, 30.0), Color.LIME, "Reversed pairs work in the background shorthand using math"],
		["reversed", Vector2(39.0, 30.0), clear, "Percentage offset uses the remaining horizontal space"],
		["three", Vector2(0.0, 35.0), Color.RED, "An omitted offset is zero on its edge"],
		["center_right", Vector2(38.0, 20.0), Color.RED, "Center can pair with an edge and offset"],
		["negative_free_space", Vector2(25.0, 17.0), Color.RED, "Percentages work with negative free space"],
		["negative_free_space", Vector2(26.0, 17.0), Color.GREEN, "The resulting tile aligns at the negative basis"],
		["negative_offset", Vector2(0.0, 41.0), clear, "A negative bottom offset moves the tile outward"],
		["negative_offset", Vector2(0.0, 42.0), Color.RED, "The left edge can be moved outward independently"],
		["legacy_pair", Vector2(46.0, 7.0), Color.RED, "A two-value position still assigns its length to the vertical axis"],
		["layers", Vector2(44.0, 4.0), Color.RED, "First axis list entry locates the first layer"],
		["layers", Vector2(12.0, 21.0), Color.LIME, "Each axis computes its own list position"],
		["layers", Vector2(44.0, 39.0), Color.BLUE, "A short horizontal list cycles independently"],
		["var", Vector2(11.0, 36.0), Color.RED, "Substituted position preserves the winning physical longhand"],
		["invalid", Vector2(40.0, 5.0), Color.RED, "Invalid positions preserve the earlier valid declaration"],
		["logical_pair", Vector2(46.0, 40.0), Color.RED, "Flow-relative axis keywords read RTL and horizontal writing"],
		["logical_axes", Vector2(43.0, 34.0), Color.RED, "Logical longhands resolve offsets from their requested edges"],
		["logical_offset", Vector2(40.0, 7.0), Color.RED, "A logical length alone starts on the logical start side"],
		["logical_four", Vector2(8.0, 36.0), Color.RED, "Logical edge pairs are accepted in either axis order"],
		["generic_four", Vector2(44.0, 36.0), Color.RED, "Unqualified edge pairs are in block then inline order"],
		["inherited", Vector2(42.0, 32.0), Color.RED, "Inheritance uses the parent's computed physical edges"],
		["logical_initial", Vector2(46.0, 0.0), Color.RED, "Logical initial comes from the inline start side"],
		["round", Vector2(15.0, 36.0), Color.GREEN, "Right offset is based on the rounded tile's width"],
		["round", Vector2(16.0, 36.0), Color.RED, "Rounded tiles repeat from the same resolved origin"],
		["space", Vector2(3.0, 37.0), clear, "One spaced tile honors edge offsets"],
		["space", Vector2(4.0, 37.0), Color.RED, "The single tile begins at the right offset"],
		["origin", Vector2(41.0, 39.0), Color.RED, "Edge offsets are relative to the selected background origin"],
		["object", Vector2(5.0, 32.0), clear, "Object positioning uses the same edge geometry"],
		["object", Vector2(6.0, 32.0), Color.RED, "Object direction resolves logical edges with the fitted size"],
		["reset", Vector2(0.0, 0.0), Color.RED, "Background shorthand resets both position axes"],
		["incompatible", Vector2(9.0, 6.0), Color.RED, "An incompatible logical pair leaves the earlier declaration active"],
		["logical_var", Vector2(43.0, 35.0), Color.RED, "Logical single-axis shorthands accept var() and its fallback"],
	]
	for sample: Array in samples:
		var id: String = sample[0]
		var offset: Vector2 = sample[1]
		var expected: Color = sample[2]
		var description: String = sample[3]
		_check_pixel(output, _label_rect(label, id).position + offset, expected, description)
	viewport.queue_free()


func _check_background_attachment() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 290)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(260.0, 104.0)
	label.text = """<!doctype html><style>
		body { margin: 0; background: #303030; }
		.panel { position: absolute; top: 20px; width: 110px; height: 116px; }
		#fixed { left: 16px; padding: 7px; border-radius: 8px; image-rendering: crisp-edges;
			background: url(density-live.png) 44px 48px / 20px 18px fixed no-repeat content-box border-box; }
		#scroll { left: 144px; background: image(blue) 10px 28px / 20px 18px scroll no-repeat; }
		#fixed_gradient { position: absolute; top: 15px; left: 120px; width: 23px; height: 120px;
			background: linear-gradient(lime, lime) 123px 50px / 8px 12px fixed no-repeat; }
		#spacer { width: 340px; height: 230px; }
	</style><div id="spacer"></div><div class="panel" id="fixed"></div>
	<div class="panel" id="scroll"></div><div id="fixed_gradient"></div>"""
	viewport.add_child(label)
	var nested: HTMLLabel = HTMLLabel.new()
	nested.position = Vector2(0.0, 124.0)
	nested.size = Vector2(290.0, 150.0)
	nested.text = """<!doctype html><style>
		body { margin: 0; background: #303030; }
		.inner { position: absolute; top: 10px; width: 72px; height: 46px; overflow: auto; }
		.content { width: 132px; height: 142px; }
		#local { left: 10px; background: image(red) 10px 55px / 20% 8px local no-repeat; }
		#scroll_inner { left: 100px; background: image(blue) 10px 19px / 20% 8px scroll no-repeat; }
		#inner_fixed { left: 190px; }
		#inner_fixed_child { height: 142px;
			background: cross-fade(lime, lime) 205px 25px / 12px 10px fixed no-repeat; }
		#hidden { left: 10px; top: 70px; overflow: hidden;
			background: image(lime) 10px 12px / 20% 8px local no-repeat; }
		#transform { position: absolute; left: 100px; top: 70px; transform: translate(8px, 5px); }
		#transform_child { width: 72px; height: 46px; background: image(red) 9px 10px / 12px 8px fixed no-repeat; }
	</style>
	<div id="local" class="inner"><div id="local_content" class="content"></div></div>
	<div id="scroll_inner" class="inner"><div id="scroll_content" class="content"></div></div>
	<div id="inner_fixed" class="inner"><div id="inner_fixed_child"></div></div>
	<div id="hidden" class="inner"><div id="hidden_content" class="content"></div></div>
	<div id="transform"><div id="transform_child"></div></div>"""
	viewport.add_child(nested)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	var clear: Color = Color("#303030")
	var fixed_point: Vector2 = Vector2(45.0, 49.0)
	var gradient_point: Vector2 = Vector2(124.0, 51.0)
	_check_pixel(output, fixed_point, Color.RED, "Fixed texture ignores the element's content-box origin")
	_check_pixel(output, gradient_point, Color.LIME, "Fixed gradient starts from the viewport origin")
	_check_pixel(output, Vector2(155.0, 49.0), Color.BLUE, "Scroll attachment starts from the element's box")
	_check_pixel(output, nested.position + Vector2(22.0, 30.0), clear, "Local tile starts below the inner viewport")
	_check_pixel(output, nested.position + Vector2(112.0, 30.0), Color.BLUE, "Scroll background is initially visible inside its box")
	_check_pixel(output, nested.position + Vector2(206.0, 27.0), Color.LIME, "Fixed cross-fade is initially visible through the inner clip")
	_check_pixel(output, nested.position + Vector2(44.0, 83.0), Color.LIME, "Hidden overflow still sets the local positioning area's size")
	var transformed_box: Rect2 = _label_rect(nested, "transform_child")
	_check_pixel(output, nested.position + transformed_box.position + Vector2(10.0, 11.0), Color.RED,
		"A transformed ancestor makes a descendant's fixed attachment use the element box")

	var inner_content: HTMLElement = nested.get_element_by_id("local_content")
	var inner_y: float = inner_content.get_bounding_rect().position.y
	var original_extent: Vector2i = nested.get_content_size()
	var hidden_before: Rect2 = _label_rect(nested, "hidden_content")
	_push_html_wheel(viewport, nested.position + Vector2(24.0, 34.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, nested.position + Vector2(114.0, 34.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, nested.position + Vector2(206.0, 34.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, nested.position + Vector2(24.0, 86.0), MOUSE_BUTTON_WHEEL_DOWN)
	label.set_scroll_offset(Vector2i(12, 30))
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	output = viewport.get_texture().get_image()
	_check(label.get_scroll_offset() == Vector2i(12, 30), "The label scrolls on both axes")
	_check_pixel(output, fixed_point, Color.RED, "Fixed image stays pinned during native label scrolling")
	_check_pixel(output, gradient_point, Color.LIME, "Fixed gradient stays pinned during native label scrolling")
	_check_pixel(output, Vector2(143.0, 19.0), Color.BLUE, "Scroll background moves with the outer box")
	_check_pixel(output, Vector2(45.0, 19.0), clear, "The no-repeat fixed image does not repeat outside its viewport tile")
	_check(is_equal_approx(inner_content.get_bounding_rect().position.y, inner_y - 36.0),
		"Wheel input moves the content of the inner scroll box")
	_check(_label_rect(nested, "hidden_content").is_equal_approx(hidden_before), "Hidden overflow does not consume wheel input")
	_check(nested.get_content_size() == original_extent and nested.get_scroll_offset() == Vector2i.ZERO,
		"Inner scrolling preserves the document extent and outer offset")
	_check_pixel(output, nested.position + Vector2(44.0, 30.0), Color.RED, "The local tile scrolls into view and sizes against the scrollable width")
	_check_pixel(output, nested.position + Vector2(47.0, 30.0), clear, "The scrollable width still bounds the local tile")
	_check_pixel(output, nested.position + Vector2(112.0, 30.0), Color.BLUE, "The scroll attachment stays at the inner border")
	_check_pixel(output, nested.position + Vector2(124.0, 30.0), clear, "The scroll attachment sizes against the visible box")
	_check_pixel(output, nested.position + Vector2(206.0, 27.0), Color.LIME, "The fixed cross-fade also stays pinned in a nested scroller")

	var local: HTMLElement = nested.get_element_by_id("local")
	_check(local.set_attribute("data-changed", "yes") == OK, "Restyle the already scrolled element")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check(is_equal_approx(inner_content.get_bounding_rect().position.y, inner_y - 36.0),
		"Stylesheet changes restore the element's scroll offset")
	_check_pixel(viewport.get_texture().get_image(), nested.position + Vector2(44.0, 30.0), Color.RED,
		"Restyling keeps the local attachment at the same scrolled position")
	var content_left: float = inner_content.get_bounding_rect().position.x
	_check(local.set_attribute("style", "background-position-x: right 10px") == OK, "Move the local tile near the scrollable right edge")
	await process_frame
	await process_frame
	_push_html_wheel(viewport, nested.position + Vector2(24.0, 34.0), MOUSE_BUTTON_WHEEL_RIGHT)
	await process_frame
	await RenderingServer.frame_post_draw
	_check(is_equal_approx(inner_content.get_bounding_rect().position.x, content_left - 36.0),
		"Horizontal wheel input moves the inner content")
	_check_pixel(viewport.get_texture().get_image(), nested.position + Vector2(71.0, 30.0), Color.RED,
		"Horizontal scrolling brings the right-aligned local tile into view")
	viewport.queue_free()


func _check_embedded_background_attachment() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var source: String = """<!doctype html><style>
		body { margin: 0; background: #303030; }
		div { width: 200px; height: 130px; padding: 10px;
			background: image(lime) right 8px bottom 6px / 8px 8px fixed no-repeat content-box border-box; }
	</style><div></div>"""
	_check(layout.parse(source) == OK and layout.layout(Vector2i(164, 100)) == OK, "Lay out a standalone fixed background")
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(240, 200)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var artwork: Node2D = Node2D.new()
	viewport.add_child(artwork)
	artwork.draw.connect(func() -> void:
		layout.draw(artwork, Vector2(11.0, 12.0), Rect2(11.0, 12.0, 164.0, 100.0))
	)

	var label: MarkdownLabel = MarkdownLabel.new()
	label.parse_html = true
	label.position = Vector2(0.0, 122.0)
	label.size = Vector2(170.0, 78.0)
	var test_texture: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		label.add_theme_icon_override(icon, test_texture)
	label.text = """<div style="width: 70px; height: 34px; overflow: auto;
		background: #224466 image(red) 5px 42px / 14px 8px local no-repeat;">
		<div style="height: 110px;"></div>
	</div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	_check_pixel(output, Vector2(160.0, 99.0), Color.LIME,
		"Standalone fixed sizes and offsets use the layout viewport and the drawing offset")
	_check_pixel(output, Vector2(158.0, 99.0), Color("#303030"), "Fixed ignores the positioning box even in standalone drawing")
	var top_left: Vector2i = Vector2i(-1, -1)
	for y: int in range(122, 198):
		for x: int in range(160):
			if output.get_pixel(x, y).is_equal_approx(Color("#224466")):
				top_left = Vector2i(x, y)
				break
		if top_left.x >= 0:
			break
	_check(top_left.x >= 0, "Markdown shows the HTML block's scroller")
	if top_left.x >= 0:
		_push_html_wheel(viewport, Vector2(top_left + Vector2i(18, 16)), MOUSE_BUTTON_WHEEL_DOWN)
		await process_frame
		await RenderingServer.frame_post_draw
		_check_pixel(viewport.get_texture().get_image(), Vector2(top_left + Vector2i(6, 7)), Color.RED,
			"The Markdown HTML block scrolls its local background")
	viewport.queue_free()


func _push_html_wheel(viewport: SubViewport, position: Vector2, index: MouseButton, factor: float = 1.0) -> void:
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = position
	motion.global_position = position
	viewport.push_input(motion, true)
	var event: InputEventMouseButton = InputEventMouseButton.new()
	event.button_index = index
	event.pressed = true
	event.position = position
	event.global_position = position
	event.factor = factor
	viewport.push_input(event, true)


func _write_image_dpi_fixtures() -> bool:
	var tiff: PackedByteArray = []
	tiff.resize(66)
	tiff[0] = 0x49
	tiff[1] = 0x49
	tiff.encode_u16(2, 42)
	tiff.encode_u32(4, 8)
	tiff.encode_u16(8, 3)
	for index: int in range(2):
		var entry: int = 10 + index * 12
		var rational: int = 50 + index * 8
		tiff.encode_u16(entry, 0x011a + index)
		tiff.encode_u16(entry + 2, 5)
		tiff.encode_u32(entry + 4, 1)
		tiff.encode_u32(entry + 8, rational)
		tiff.encode_u32(rational, 384)
		tiff.encode_u32(rational + 4, 1)
	tiff.encode_u16(34, 0x0128)
	tiff.encode_u16(36, 3)
	tiff.encode_u32(38, 1)
	tiff.encode_u16(42, 2)
	var jpeg: PackedByteArray = [0xff, 0xd8, 0xff, 0xe1, 0, 74, 69, 120, 105, 102, 0, 0,]
	jpeg.append_array(tiff)
	jpeg.append_array(PackedByteArray([0xff, 0xd9,]))
	var webp: PackedByteArray = "RIFF".to_ascii_buffer()
	webp.resize(20)
	webp.encode_u32(4, tiff.size() + 12)
	for index: int in range(8):
		webp[8 + index] = "WEBPEXIF".unicode_at(index)
	webp.encode_u32(16, tiff.size())
	webp.append_array(tiff)
	var late_webp: PackedByteArray = webp.slice(0, 12)
	late_webp.encode_u32(4, tiff.size() + 20)
	late_webp.append_array(PackedByteArray([86, 80, 56, 76, 0, 0, 0, 0,]))
	late_webp.append_array(webp.slice(12))
	var jfif: PackedByteArray = [0xff, 0xd8, 0xff, 0xe0, 0, 16, 74, 70, 73, 70, 0, 1, 2, 1,
		1, 32, 1, 32, 0, 0, 0xff, 0xd9,]
	var files: Dictionary[String, PackedByteArray] = {
		"res://density-jfif.jpg": jfif,
		"res://density-exif.jpg": jpeg,
		"res://density-exif.webp": webp,
		"res://density-late.webp": late_webp,
	}
	for path: String in files:
		var file: FileAccess = FileAccess.open(path, FileAccess.WRITE)
		if file == null or not file.store_buffer(files[path]):
			return false
	return true


func _check_generated_content() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = r"""<!doctype html><html lang="de"><style>
body { margin: 0; counter-reset: chapter 1; }
q::before { content: open-quote; }
q::after { content: close-quote; }
.brackets { quotes: "[" "]" "(" ")"; }
.silent::before { content: no-open-quote; }
.silent::after { content: no-close-quote; }
.no_quotes { quotes: none; }
.auto_quotes { quotes: auto; }
.early::before { content: close-quote "EARLY"; }
.label::before { content: "<\"" attr(data-name) "\" >"; }
.fallback::after { content: var(--text, attr(data-missing raw-string, "[fallback]")); }
.empty::after { content: attr(data-empty, "wrong"); }
.bad::before { content: "VALID"; content: counter(Bad, 123); }
.invalid_attr::after { content: "VALIDATTR"; content: attr(data-name bad); }
.count::before { content: "{" counter(chapter, upper-roman) "}"; }
.nested::before { content: "{" counters(chapter, ",", decimal-leading-zero) "}"; }
.negative::after { counter-reset: Negative -1; content: counter(Negative, decimal-leading-zero); }
.hidden { display: none; counter-set: chapter 99; }
.hidden_pseudo::before { content: none; counter-increment: chapter 100; }
.implicit::before { content: counter(Unassigned) "A"; }
.implicit::after { counter-increment: Unassigned 2; content: counter(Unassigned) "B"; }
.responsive { display: none; counter-increment: chapter 4; }
@media (min-width: 400px) { .responsive { display: block; } }
</style><body>
<div class="early"></div>
<div class="brackets"><q>one<q>two<q>three</q></q></q></div>
<div class="brackets"><q class="silent">silent<q>inside</q></q></div>
<div class="no_quotes"><q>plain<q class="auto_quotes">inside</q></q></div>
<div><q>Deutsch</q></div>
<div id="attribute_name" class="label" data-name="A&#92;B">value</div>
<div class="fallback">missing</div><div class="empty" data-empty="">empty</div>
<div class="bad">literal</div><div class="invalid_attr">invalid</div>
<div class="count" style="counter-increment: chapter 7; counter-set: chapter 4">start</div>
<div class="hidden"><div style="counter-increment: chapter 2"></div></div>
<div class="hidden_pseudo"></div>
<div style="counter-reset: chapter 2"><div class="nested" style="counter-increment: chapter">inner</div></div>
<div class="nested" style="counter-reset: chapter 6">replace</div>
<div class="responsive"></div><div class="count">final</div>
<div class="implicit"></div>
<div class="negative">negative</div>
</body></html>"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(340, 460)) == OK, "Lay out generated content")
	var rendered: String = layout.get_plain_text()
	for snippet: String in ["EARLY", "[one(two(three))]", "silent(inside)", "plain‚inside‘", "„Deutsch“",
			'<"A\\B" >value', "missing[fallback]", "empty", "VALIDliteral", "invalidVALIDATTR", "{IV}start",
			"{04,03}inner", "{04,06}replace", "{VI}final", "0A2B", "negative-1"]:
		_check(snippet in rendered, "Generated text contains %s" % snippet)
	_check("wrong" not in rendered and "‚EARLY" not in rendered, "Empty attributes and unmatched closing quotes")
	_check(layout.get_element_by_id("attribute_name").get_text_content() == "value", "Generated content stays outside DOM text")
	_check(layout.get_css_diagnostics().size() == 2, "Invalid content functions produce diagnostics: %s" % [layout.get_css_diagnostics()])
	_check(layout.layout(Vector2i(350, 460)) == OK and layout.get_plain_text() == rendered,
		"Reflow keeps quotes and counters reproducible without a style change")
	_check(layout.layout(Vector2i(420, 460)) == OK and "{X}final" in layout.get_plain_text(),
		"Media changes recalculate counters in document order")
	var attribute: HTMLElement = layout.get_element_by_id("attribute_name")
	_check(attribute.set_attribute("data-name", 'C)"D') == OK and layout.layout(Vector2i(420, 460)) == OK and
		'<"C)"D" >value' in layout.get_plain_text(), "Updating an attribute refreshes generated text literally")


func _check_style_containment() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """<!doctype html><style>
		body { counter-reset: section 10; quotes: "A" "Z" "B" "Y" "C" "X" "D" "W"; }
		#scope { contain: style; counter-increment: section; }
		@media (min-width: 400px) { #scope { contain: none; } }
		.scope { contain: style; }
		.step { counter-increment: section 2; }
		.read::before, .pseudo_step::before {
			content: "(" counters(section, ".") ")";
		}
		#scope::before { content: "(" counters(section, ".") ")ENTER"; }
		#scope::after { content: "(" counters(section, ".") ")LEAVE"; }
		.pseudo_step::before { counter-increment: section 3; }
		.open::before, #quotes::before { content: open-quote; }
		.close::after, #quotes::after { content: close-quote; }
		.silent::before { content: no-open-quote open-quote no-close-quote close-quote open-quote; }
		#content { width: 60px; height: 28px; }
		@supports (contain: content) and (contain: style paint layout) and (not (contain: content style)) {
			#content { contain: content; }
		}
		#fixed { position: fixed; width: 80px; height: 20px; left: 12px; top: 4px; counter-set: section 99; }
	</style>
	<div class="read">START</div>
	<div id="scope">
		<div class="read step">STEP</div>
		<div class="scope">
			<div class="read" style="counter-set: section 7">SET</div>
			<div class="read step">NESTED</div>
		</div>
		<div class="read step">RESUME</div>
		<div class="read" style="counter-reset: section 30">RESET</div>
		<div class="pseudo_step">PSEUDO</div>
	</div>
	<div class="read step">OUT</div>
	<div class="open close"><span id="quotes" class="scope"><span id="nested_quotes" class="scope"><span class="silent"></span></span></span></div>
	<div id="content"><div id="fixed"></div></div>
	<div class="read">END</div>"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(360, 420)) == OK, "Lay out style containment")
	var rendered: String = layout.get_plain_text()
	for snippet: String in [
		"(10)START",
		"(11)ENTER",
		"(11.2)STEP",
		"(11.2.7)SET",
		"(11.2.9)NESTED",
		"(11.4)RESUME",
		"(11.30)RESET",
		"(11.33)PSEUDO",
		"(11.33)LEAVE",
		"(13)OUT",
		"ABDXCYZ",
		"(13)END",
	]:
		_check(snippet in rendered, "Style containment renders %s in %s" % [snippet, rendered])
	var fixed: HTMLElement = layout.get_element_by_id("fixed")
	var origin: Vector2 = layout.get_element_by_id("content").get_bounding_rect().position
	_check(fixed.get_bounding_rect().position == origin + Vector2(12.0, 4.0),
		"Contain content includes layout containment and is accepted by feature queries")
	_check(layout.get_element_at(origin + Vector2(15.0, 8.0)) == fixed and
		layout.get_element_at(origin + Vector2(70.0, 8.0)) != fixed, "Contain content includes paint containment")
	_check(layout.get_css_diagnostics().is_empty(), "Supported containment values produce no declaration diagnostics")
	_check(layout.layout(Vector2i(420, 420)) == OK and "(17)OUT" in layout.get_plain_text(),
		"Removing style containment at a media breakpoint merges the descendant increments into the outer counter")
	_check(layout.layout(Vector2i(360, 420)) == OK and layout.get_plain_text() == rendered,
		"Restoring style containment rebuilds the original counter and quote scopes")
	var nested_quotes: HTMLElement = layout.get_element_by_id("nested_quotes")
	_check(nested_quotes.set_attribute("style", "contain: none") == OK and layout.layout(Vector2i(360, 420)) == OK and
		"ABDXCXZ" in layout.get_plain_text(), "Quote scoping follows DOM style mutations on a non-atomic inline")

	var list: HTMLLayout = HTMLLayout.new()
	var ordered: String = """<!doctype html><style>
		li::before { content: "[" counter(list-item) "]"; }
	</style><ol reversed start="10"><li>FIRST</li><div style="contain: style">
		<li>LOCAL1</li><li value="4">LOCAL2</li><li>LOCAL3</li>
	</div><li>LAST</li></ol>"""
	_check(list.parse(ordered) == OK and list.layout(Vector2i(360, 260)) == OK, "Lay out list ordinals in a style scope")
	for snippet: String in [
		"[10]FIRST",
		"[1]LOCAL1",
		"[4]LOCAL2",
		"[5]LOCAL3",
		"[9]LAST",
	]:
		_check(snippet in list.get_plain_text(), "List ordinal isolation renders %s" % snippet)


func _check_container_size_queries() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><style>
		body { margin: 0; }
		#outer { container: Outer alias / size; box-sizing: border-box; width: 270px; height: 130px;
			padding: 10px; border: 5px solid; font-size: 20px; margin-left: 27px; }
		#inner { --query: Inner / inline-size; container: var(--query); width: 140px; font-size: 10px; }
		.probe { width: 13px; height: 4px; }
		#fixed { position: fixed; left: 9px; top: 5px; }
		#responsive { container-type: inline-size; width: 50%; }
		#promoted { width: 80px; }
		#flagged { container-name: flag; }
		@container Inner (100px < inline-size <= 14em) { #named { width: 41px; } }
		@container (min-width: 200px) { #nearest { width: 99px; } }
		@container (block-size >= 100px) { #both { width: 42px; } }
		@container Outer (orientation: landscape) and (aspect-ratio > 2 / 1) { #ratio { width: 43px; } }
		@container Outer (width = 240px) { #exact { width: 44px; } }
		@container outer (width > 0px) { #case { width: 99px; } }
		@container Inner (width < 90px), alias (width > 200px) { #comma { width: 45px; } }
		@container Inner (width >= 140px) {
			@container Outer (height = 100px) { #chain { width: 46px; } }
			#inner { width: 190px; }
			#inner::before { content: "CONTAINER PSEUDO"; }
		}
		@container (not (unsupported-feature: 1)) { #unknown { width: 99px; } }
		@container flag { #name_only { width: 47px; } }
		@supports at-rule(@container) {
			@container (width >= 210px) {
				#resized { width: 48px; }
				#promoted { container: special / inline-size; }
			}
			@container special (width = 80px) { #dependent { width: 49px; } }
		}
	</style>
	<div id="outer"><div id="inner">
		<div class="probe" id="named"></div><div class="probe" id="nearest"></div>
		<div class="probe" id="both"></div><div class="probe" id="ratio"></div>
		<div class="probe" id="exact"></div><div class="probe" id="case"></div>
		<div class="probe" id="comma"></div><div class="probe" id="chain"></div>
		<div class="probe" id="unknown"></div><div class="probe" id="fixed"></div>
	</div></div>
	<div id="flagged"><div class="probe" id="name_only"></div></div>
	<div id="responsive"><div class="probe" id="resized"></div>
		<div id="promoted"><div class="probe" id="dependent"></div></div>
	</div>"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(360, 360)) == OK,
		"Lay out size container queries")
	var expected_widths: Dictionary[String, float] = {
		"named": 41.0,
		"nearest": 13.0,
		"both": 42.0,
		"ratio": 43.0,
		"exact": 44.0,
		"case": 13.0,
		"comma": 45.0,
		"chain": 46.0,
		"unknown": 13.0,
		"name_only": 47.0,
		"inner": 140.0,
		"resized": 13.0,
		"dependent": 13.0,
	}
	for id: String in expected_widths:
		var width: float = _rect(layout, id).size.x
		_check(width == expected_widths[id], "Container query width for %s was %s" % [id, width])
	_check("CONTAINER PSEUDO" in layout.get_plain_text(), "A generated pseudo-element can query its originating element")
	_check(_rect(layout, "fixed").position == Vector2(9.0, 5.0),
		"Container type alone does not capture fixed-position descendants")
	_check(layout.layout(Vector2i(520, 360)) == OK and _rect(layout, "resized").size.x == 48.0
		and _rect(layout, "dependent").size.x == 49.0, "Viewport resize updates queries and newly eligible containers")
	_check(layout.layout(Vector2i(360, 360)) == OK and _rect(layout, "resized").size.x == 13.0
		and _rect(layout, "dependent").size.x == 13.0, "Both responsive rules stop matching when the viewport shrinks")
	var inner: HTMLElement = layout.get_element_by_id("inner")
	_check(inner.set_attribute("style", "container-type: normal") == OK and layout.layout(Vector2i(360, 360)) == OK,
		"Recompute container eligibility after an inline style change")
	_check(_rect(layout, "named").size.x == 13.0 and _rect(layout, "nearest").size.x == 99.0
		and "CONTAINER PSEUDO" not in layout.get_plain_text(),
		"Removing inline containment updates named, anonymous and pseudo-element queries")
	_check(inner.remove_attribute("style") == OK and layout.layout(Vector2i(360, 360)) == OK
		and _rect(layout, "named").size.x == 41.0 and _rect(layout, "nearest").size.x == 13.0,
		"Restoring the inner container updates the same element handles at a fixed viewport size")


func _check_container_units() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var markup: String = """<!doctype html><style>
		body { margin: 0; }
		#fallback { width: 25cqw; height: 10cqh; }
		#outer { container-type: size; width: 300px; height: 200px; }
		#inner { container-type: inline-size; width: 60cqi; }
		#split { --relative: 20cqw; width: calc(10% + var(--relative)); height: 15cqb; }
		#range { width: 20cqmin; height: 25cqmax; }
		#inheritance { container-type: inline-size; width: 25cqw; }
		#inherited { width: inherit; height: 1px; }
		#badge { container-type: inline-size; width: 50px; }
		#badge::before { content: ""; display: block; height: 40cqw; }
		#zero { container-type: size; width: 0; height: 0; }
		#zero_child { width: 5cqi; height: 5cqb; }
		#shift { position: absolute; left: 0; top: 0; transform: translate(10cqw, 10cqh);
			width: 5cqi; height: 5cqb; }
	</style>
	<div id="fallback"></div>
	<div id="outer"><div id="inner">
		<div id="split"></div><div id="range"></div>
		<div id="inheritance"><div id="inherited"></div></div>
		<div id="badge"></div><div id="zero"><div id="zero_child"></div></div>
		<div id="shift"></div>
	</div></div>"""
	_check(layout.parse(markup) == OK and layout.layout(Vector2i(320, 500)) == OK,
		"Lay out container units without a container query")
	var expected_sizes: Dictionary[String, Vector2] = {
		"fallback": Vector2(80.0, 50.0),
		"inner": Vector2(180.0, 101.0),
		"split": Vector2(54.0, 30.0),
		"range": Vector2(36.0, 50.0),
		"inherited": Vector2(45.0, 1.0),
		"badge": Vector2(50.0, 20.0),
		"zero_child": Vector2.ZERO,
	}
	for id: String in expected_sizes:
		var actual: Vector2 = _rect(layout, id).size
		_check(actual.is_equal_approx(expected_sizes[id]), "Container unit size for %s was %s" % [id, actual])
	_check(_rect(layout, "shift").is_equal_approx(Rect2(18.0, 20.0, 9.0, 10.0)),
		"Transforms and fixed CSS lengths use the same query container axes")
	var outer: HTMLElement = layout.get_element_by_id("outer")
	_check(outer.set_attribute("style", "width: 240px; height: 100px") == OK and
		layout.layout(Vector2i(320, 500)) == OK, "Resize nested query containers at a fixed viewport")
	_check(_rect(layout, "split").size.is_equal_approx(Vector2(43.2, 15.0)) and
		_rect(layout, "range").size.is_equal_approx(Vector2(20.0, 36.0)),
		"Container units update after ancestor-only edits and resolve the axes independently")
	_check(layout.layout(Vector2i(400, 350)) == OK and _rect(layout, "fallback").size == Vector2(100.0, 35.0),
		"Container unit viewport fallback follows viewport changes")

	var query: HTMLLayout = HTMLLayout.new()
	_check(query.parse("""<!doctype html><style>
		body { margin: 0; }
		#outer { container-type: size; width: 300px; height: 200px; }
		#inner { container-type: inline-size; width: 180px; }
		#gate { container: gate / size; width: 90px; height: 20px; }
		#probe { width: 1px; height: 8px; }
		@container gate (width = 50cqi) and (width = 45cqb) {
			#probe { height: 30px; width: calc(10cqw + 2px); }
		}
	</style><div id="outer"><div id="inner"><div id="gate"><div id="probe"></div></div></div></div>""") == OK
		and query.layout(Vector2i(320, 400)) == OK and _rect(query, "probe").size == Vector2(11.0, 30.0),
		"Units in a query threshold use the queried container's own ancestors")
	var query_outer: HTMLElement = query.get_element_by_id("outer")
	_check(query_outer.set_attribute("style", "height: 100px") == OK and
		query.layout(Vector2i(320, 400)) == OK and _rect(query, "probe").size == Vector2(1.0, 8.0),
		"Changing a threshold's container-relative value invalidates the query")


func _check_container_style_queries() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var markup: String = """<!doctype html><style>
		body { margin: 0; --inherited: shared; }
		#card { container-name: card; --target: Dark; --theme: var(--target); --empty: ;
			--words: some text; --cycle: var(--cycle); }
		#tray { container-type: inline-size; width: 170px; --theme: Light; }
		.probe { width: 11px; height: 4px; }
		@container card style(--theme: Dark !important) {
			#named { width: 41px; }
			#promoted { --derived: match; }
			#card::before { content: "STYLE QUERY PSEUDO"; }
		}
		@container style(--theme: Light) { #nearest { width: 42px; } }
		@container style(--theme: Dark) { #nearest { width: 99px; } }
		@container card style(--theme: dark) { #wrong_case { width: 99px; } }
		@container card style(--Theme) { #wrong_case { width: 98px; } }
		@container card style((--theme: var(--target)) and (--empty: )) { #combined { width: 43px; } }
		@container card style((--theme: var(--missing, nowhere)) or (--words: some   text)) { #either { width: 44px; } }
		@container card style(not (--cycle)) { #cycle { width: 45px; } }
		@container card style(--missing: var(--unknown)) { #missing { width: 46px; } }
		@container card style(--inherited: unset) { #inherited { width: 47px; } }
		@container card style(--cycle: initial) { #initial { width: 48px; } }
		@container card not style(--missing: revert-layer) { #rollback { width: 49px; } }
		@container (width > 120px) and style(--theme: Light) { #mixed { width: 50px; } }
		@container style(--derived) { #dependent { width: 51px; } }
		@container card not style(color: red) { #unknown { width: 99px; } }
	</style><div id="card"><section id="tray">
		<div class="probe" id="named"></div><div class="probe" id="nearest"></div>
		<div class="probe" id="wrong_case"></div><div class="probe" id="combined"></div>
		<div class="probe" id="either"></div><div class="probe" id="cycle"></div>
		<div class="probe" id="missing"></div><div class="probe" id="inherited"></div>
		<div class="probe" id="initial"></div><div class="probe" id="rollback"></div>
		<div class="probe" id="mixed"></div><div class="probe" id="unknown"></div>
		<div id="promoted"><div class="probe" id="dependent"></div></div>
	</section></div>"""
	_check(layout.parse(markup) == OK and layout.layout(Vector2i(320, 500)) == OK,
		"Apply style queries to ordinary and size query containers")
	var expected_widths: Dictionary[String, float] = {
		"named": 41.0,
		"nearest": 42.0,
		"wrong_case": 11.0,
		"combined": 43.0,
		"either": 44.0,
		"cycle": 45.0,
		"missing": 46.0,
		"inherited": 47.0,
		"initial": 48.0,
		"rollback": 49.0,
		"mixed": 50.0,
		"dependent": 51.0,
		"unknown": 11.0,
	}
	for id: String in expected_widths:
		var width: float = _rect(layout, id).size.x
		_check(width == expected_widths[id], "Container style query width for %s was %s" % [id, width])
	_check("STYLE QUERY PSEUDO" in layout.get_plain_text(), "The generated pseudo-element queries its originating style")
	var card: HTMLElement = layout.get_element_by_id("card")
	_check(card.set_attribute("style", "--target: Silver") == OK and layout.layout(Vector2i(320, 500)) == OK,
		"Reevaluate style queries after changing a container variable")
	_check(_rect(layout, "named").size.x == 11.0 and _rect(layout, "dependent").size.x == 11.0
		and _rect(layout, "combined").size.x == 43.0 and "STYLE QUERY PSEUDO" not in layout.get_plain_text(),
		"Changing an ancestor variable updates direct, dependent and pseudo-element queries")


func _check_container_query_variables() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var markup: String = """<!doctype html><style>
		body { margin: 0; --inherited-limit: 180px; }
		#card { container: card / size; width: 200px; height: 100px; font-size: 16px;
			--min: var(--inherited-limit); --max: 240px; --padding: 1em; --invalid: initial;
			--broken: red; --orientation: landscape; --ratio: 4 / 2; --n: 3; --d: 2;
			--injection: 0px < width; }
		.probe { width: 11px; height: 4px; }
		#fallback { --inherited-limit: 280px; }
		@container card (width > var(--invalid, var(--inherited-limit, 500px))) { #fallback { width: 41px; } }
		@container card (min-width: calc(var(--min) + var(--padding))) { #math { width: 42px; } }
		@container card (var(--min) < width <= var(--max)) { #range { width: 43px; } }
		@container card (width < var(--limit, 48cqw)) { #relative { width: 44px; } }
		@container card (orientation: var(--orientation)) { #orientation { width: 45px; } }
		@container card (aspect-ratio: var(--ratio)) { #ratio { width: 46px; } }
		@container card (aspect-ratio = calc(var(--n) + 1) / var(--d)) { #ratio_parts { width: 47px; } }
		@container card not (width > var(--broken)) { #negation { width: 48px; } }
		@container card (width > var(--broken)) or (width = 200px) { #alternative { width: 49px; } }
		@container card not (width > var(--injection)) { #injection { width: 99px; } }
		@media not (width > var(--inherited-limit)) { #media { width: 99px; } }
	</style><div id="card">
		<div class="probe" id="fallback"></div><div class="probe" id="math"></div>
		<div class="probe" id="range"></div><div class="probe" id="relative"></div>
		<div class="probe" id="orientation"></div><div class="probe" id="ratio"></div>
		<div class="probe" id="ratio_parts"></div><div class="probe" id="negation"></div>
		<div class="probe" id="alternative"></div><div class="probe" id="injection"></div>
		<div class="probe" id="media"></div>
	</div>"""
	_check(layout.parse(markup) == OK and layout.layout(Vector2i(500, 320)) == OK,
		"Lay out size queries whose thresholds use custom properties")
	var expected_widths: Dictionary[String, float] = {
		"fallback": 41.0,
		"math": 42.0,
		"range": 43.0,
		"relative": 44.0,
		"orientation": 45.0,
		"ratio": 46.0,
		"ratio_parts": 47.0,
		"negation": 11.0,
		"alternative": 49.0,
		"injection": 11.0,
		"media": 11.0,
	}
	for id: String in expected_widths:
		var width: float = _rect(layout, id).size.x
		_check(width == expected_widths[id], "Container query variable width for %s was %s" % [id, width])
	var card: HTMLElement = layout.get_element_by_id("card")
	_check(card.set_attribute("style", "--min: 210px; --broken: 220px; --orientation: portrait") == OK
		and layout.layout(Vector2i(500, 320)) == OK, "Reevaluate query thresholds at a fixed container size")
	_check(_rect(layout, "math").size.x == 11.0 and _rect(layout, "range").size.x == 11.0
		and _rect(layout, "orientation").size.x == 11.0 and _rect(layout, "negation").size.x == 48.0,
		"Changing query input variables can stop and start matches in the same update")
	_check(layout.layout(Vector2i(400, 320)) == OK and _rect(layout, "relative").size.x == 11.0,
		"Container units inside a substituted threshold retain their viewport fallback")


func _check_size_containment() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """<!doctype html><style>
		body { margin: 0; font-size: 10px; }
		#block { width: 145px; contain: size; contain-intrinsic-size: 90px 24px; }
		#big { width: 200px; height: 60px; }
		#fill { contain: inline-size; contain-intrinsic-width: 35px; }
		#fill > div { width: 500px; height: 33px; }
		#inline { display: inline-block; contain: inline-size; contain-intrinsic-size: auto 4.5em 390px; }
		#inline > div { width: 120px; height: 63px; }
		#flex { display: inline-flex; flex-direction: column; justify-content: end; contain: size;
			contain-intrinsic-size: var(--flex-width, 85px) 40px; }
		#flex_child { width: 140px; height: 15px; flex: none; }
		#flex_row { display: flex; contain: size; width: 120px; contain-intrinsic-height: 40px; align-items: center; }
		#flex_row_child { width: 25px; height: 14px; }
		#grid, #grid_fallback, #grid_inline { display: inline-grid; contain: size; grid-template: 35px / 90px; }
		#grid_fallback { contain-intrinsic-size: 50px 24px; }
		#grid_inline { contain: inline-size; grid-template-rows: auto; }
		#grid > div, #grid_fallback > div, #grid_inline > div { width: 140px; height: 80px; }
		#percent { width: 80px; contain: size; contain-intrinsic-height: 80px; }
		#percent_child { height: 50%; }
		#percent_child > div { height: 17px; }
		#ratio { width: 100px; aspect-ratio: 2; contain: size; }
		#ratio > div { height: 116px; }
		#clamped { display: inline-block; contain: size; min-width: 55px; max-height: 36px;
			contain-intrinsic-size: auto none calc(60px - 10px); }
		#explicit { contain: size; width: 55px; height: 28px; contain-intrinsic-size: 300px; }
		#image { width: 60px; contain: size; border-bottom: 1px solid black; }
		#image_fallback { contain: size; contain-intrinsic-size: 45px 18px; }
		#image_ratio { width: 64px; contain: size; aspect-ratio: auto 2; }
		#grid_parent { display: grid; grid-template-columns: max-content; justify-items: start; }
		#grid_item { contain: size; contain-intrinsic-inline-size: 64px; contain-intrinsic-block-size: 26px; }
		#grid_item > div { width: 210px; height: 95px; }
		#flex_parent { display: flex; align-items: start; }
		#flex_item { contain: size; contain-intrinsic-size: 62px 24px; }
		#flex_item > div { width: 175px; height: 109px; }
		#flex_explicit { contain: size; width: 32px; contain-intrinsic-size: 62px 24px; }
		#flex_max { contain: size; flex-basis: max-content; contain-intrinsic-size: 42px 24px; }
		table { border-spacing: 0; contain: size; }
		td { padding: 0; contain: size; }
		#cell_child { width: 75px; height: 26px; }
		#strict { width: 60px; contain-intrinsic-height: 32px; }
		@supports (contain: strict) and (contain-intrinsic-size: auto none 0) and (not (contain: size inline-size)) {
			#strict { contain: strict; contain: size inline-size; contain-intrinsic-size: 10% 64px; }
		}
		#strict_child { width: 100px; height: 45px; }
	</style>
	<div id="block"><div id="big"></div></div>
	<div id="fill"><div></div></div>
	<div><div id="inline"><div></div></div></div>
	<div><div id="flex"><div id="flex_child"></div></div></div>
	<div id="flex_row"><div id="flex_row_child"></div></div>
	<div><div id="grid"><div></div></div><div id="grid_fallback"><div></div></div><div id="grid_inline"><div></div></div></div>
	<div id="percent"><div id="percent_child"><div></div></div></div>
	<div id="ratio"><div></div></div>
	<div><div id="clamped"></div></div>
	<div id="explicit"></div>
	<img id="image" src="object-fit.svg">
	<img id="image_fallback" src="object-fit.svg">
	<img id="image_ratio" src="object-fit.svg">
	<div id="grid_parent"><div id="grid_item"><div></div></div></div>
	<div id="flex_parent"><div id="flex_item"><div></div></div><div id="flex_explicit"></div><div id="flex_max"></div></div>
	<table id="table"><tr><td id="cell"><div id="cell_child"></div></td></tr></table>
	<div id="strict"><div id="strict_child"></div></div>"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(360, 650)) == OK, "Lay out size containment")
	var expected: Dictionary[String, Vector2] = {
		"block": Vector2(145.0, 24.0),
		"big": Vector2(200.0, 60.0),
		"fill": Vector2(360.0, 33.0),
		"inline": Vector2(45.0, 63.0),
		"flex": Vector2(85.0, 40.0),
		"flex_row": Vector2(120.0, 40.0),
		"grid": Vector2(90.0, 35.0),
		"grid_fallback": Vector2(50.0, 24.0),
		"grid_inline": Vector2(90.0, 80.0),
		"percent": Vector2(80.0, 80.0),
		"percent_child": Vector2(80.0, 17.0),
		"ratio": Vector2(100.0, 50.0),
		"clamped": Vector2(55.0, 36.0),
		"explicit": Vector2(55.0, 28.0),
		"image": Vector2(60.0, 1.0),
		"image_fallback": Vector2(45.0, 18.0),
		"image_ratio": Vector2(64.0, 32.0),
		"grid_item": Vector2(64.0, 26.0),
		"flex_item": Vector2(62.0, 24.0),
		"flex_explicit": Vector2(32.0, 24.0),
		"flex_max": Vector2(42.0, 24.0),
		"table": Vector2(75.0, 26.0),
		"cell": Vector2(75.0, 26.0),
		"strict": Vector2(60.0, 32.0),
	}
	for id: String in expected:
		var actual: Vector2 = layout.get_element_by_id(id).get_bounding_rect().size
		_check(actual == expected[id], "Size containment at #%s measures %s (expected %s)" % [id, actual, expected[id]])
	var flex_y: float = layout.get_element_by_id("flex").get_bounding_rect().position.y
	var flex_child_y: float = layout.get_element_by_id("flex_child").get_bounding_rect().position.y
	_check(flex_child_y - flex_y == 25.0, "A contained flex column positions children inside its used height")
	var flex_row_y: float = layout.get_element_by_id("flex_row").get_bounding_rect().position.y
	var flex_row_child_y: float = layout.get_element_by_id("flex_row_child").get_bounding_rect().position.y
	_check(flex_row_child_y - flex_row_y == 13.0, "A contained flex row aligns items using its used cross size")
	var strict: HTMLElement = layout.get_element_by_id("strict")
	var strict_child: HTMLElement = layout.get_element_by_id("strict_child")
	var strict_origin: Vector2 = strict.get_bounding_rect().position
	_check(layout.get_element_at(strict_origin + Vector2(10.0, 12.0)) == strict_child and
		layout.get_element_at(strict_origin + Vector2(80.0, 12.0)) != strict_child, "Strict containment clips its contents")
	_check(layout.get_css_diagnostics().size() == 2, "Invalid containment values preserve earlier values")
	var contained: HTMLElement = layout.get_element_by_id("block")
	_check(contained.set_attribute("style", "contain: none") == OK and layout.layout(Vector2i(360, 650)) == OK and
		contained.get_bounding_rect().size == Vector2(145.0, 60.0), "Removing containment restores auto height from content")


func _check_hidden_content_visibility() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """<!doctype html><style>
		body { margin: 0; counter-reset: n 9; }
		#cold, #warm { display: inline-block; contain-intrinsic-size: auto 52px auto none; }
		#cold { content-visibility: hidden; content-visibility: wrong; counter-increment: n; }
		#cold > div { width: 135px; height: 95px; counter-set: n 100; }
		#cold::before { content: "GENERATED"; }
		#warm { contain-intrinsic-height: auto 14px; }
		#warm > div { width: 70px; height: 42px; }
		#next::before { content: "(" counter(n) ")"; }
		@supports (content-visibility: hidden) and (content-visibility: visible) {
			#cold { contain-intrinsic-height: auto 21px; }
		}
		#exempt { content-visibility: hidden; }
	</style>
	<div><div id="cold"><div id="cold_child">SKIPPED</div></div></div>
	<div><div id="warm"><div id="warm_child">WARM</div></div></div>
	<div id="next">NEXT <span id="exempt">INLINE</span></div>"""
	var size: Vector2i = Vector2i(320, 240)
	_check(layout.parse(html) == OK and layout.layout(size) == OK, "Lay out hidden contents")
	var cold: HTMLElement = layout.get_element_by_id("cold")
	var cold_child: HTMLElement = layout.get_element_by_id("cold_child")
	var warm: HTMLElement = layout.get_element_by_id("warm")
	var warm_child: HTMLElement = layout.get_element_by_id("warm_child")
	var rendered: String = layout.get_plain_text()
	_check(cold.get_bounding_rect().size == Vector2(52.0, 21.0), "An initially hidden box uses its declared fallback sizes")
	_check(cold_child.is_connected() and cold_child.get_text_content() == "SKIPPED" and
		cold_child.get_client_rects().is_empty(), "Hidden descendants keep their DOM handles without committed boxes")
	_check("SKIPPED" not in rendered and "GENERATED" not in rendered and "(10)NEXT INLINE" in rendered,
		"Hidden contents do not paint or change outside counters and a non-atomic inline is exempt")
	_check(layout.get_css_diagnostics().size() == 1, "Invalid content-visibility leaves the earlier declaration in the cascade")
	_check(warm.get_bounding_rect().size == Vector2(70.0, 42.0), "A visible box measures its contents")
	_check(warm.set_attribute("style", "display: none") == OK and layout.layout(size) == OK and
		warm.get_client_rects().is_empty(), "Display none removes the box")
	_check(warm.set_attribute("style", "content-visibility: hidden") == OK and layout.layout(size) == OK and
		warm.get_bounding_rect().size == Vector2(70.0, 42.0), "Remembered size survives a period without a principal box")
	_check(warm_child.set_attribute("style", "width: 93px; height: 65px") == OK and layout.layout(size) == OK and
		warm.get_bounding_rect().size == Vector2(70.0, 42.0) and warm_child.get_client_rects().is_empty(),
		"Edits inside a hidden subtree preserve the remembered box")
	_check(warm.set_attribute("style", "content-visibility: visible") == OK and layout.layout(size) == OK and
		warm_child == layout.get_element_by_id("warm_child") and warm.get_bounding_rect().size == Vector2(93.0, 65.0),
		"Revealing a subtree lays out edits and preserves descendant identity")
	_check(warm.set_attribute("style", "content-visibility: hidden; contain-intrinsic-height: 29px") == OK and
		layout.layout(size) == OK and warm.get_bounding_rect().size == Vector2(93.0, 29.0),
		"Removing auto from one axis clears only that axis's remembered size")
	_check(warm.set_attribute("style", "content-visibility: hidden") == OK and layout.layout(size) == OK and
		warm.get_bounding_rect().size == Vector2(93.0, 14.0),
		"Reintroducing auto on a hidden axis uses its fallback")

	var destination: HTMLLayout = HTMLLayout.new()
	_check(destination.parse('<div id="destination"></div>') == OK, "Parse a destination for hidden nodes")
	_check(warm.set_attribute("style", "display: inline-block; content-visibility: hidden; contain-intrinsic-size: auto 52px auto 14px") == OK,
		"Set the intrinsic fallback on the element before transferring it")
	var destination_element: HTMLElement = destination.get_element_by_id("destination")
	var copied: HTMLElement = destination.import_node(warm, true)
	_check(destination_element.append_child(copied) == OK and destination.adopt_node(warm) == warm and
		destination_element.append_child(warm) == OK and destination.layout(size) == OK and
		copied.get_bounding_rect().size == Vector2(52.0, 14.0) and warm.get_bounding_rect().size == Vector2(93.0, 14.0),
		"Adoption transfers remembered size with the original node, while a clone uses the fallback")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(190, 105)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: white; }
		#cover { content-visibility: hidden; width: 70px; height: 35px; background: blue; border: 2px solid red; }
		#target { display: block; width: 120px; height: 50px; padding: 0; margin: 0; border: 0; background: lime; }
		#photo { display: block; content-visibility: hidden; width: 40px; height: 20px; background: cyan; }
	</style><div id="cover"><button id="target" type="button">Reveal</button></div>
	<img id="photo" src="object-fit.svg">"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var cover: HTMLElement = label.get_element_by_id("cover")
	var target: HTMLElement = label.get_element_by_id("target")
	var photo: HTMLElement = label.get_element_by_id("photo")
	var cover_box: Rect2 = cover.get_bounding_rect()
	var photo_box: Rect2 = photo.get_bounding_rect()
	_check_pixel(image, cover_box.get_center(), Color.BLUE, "The skipped host paints its own background")
	_check_pixel(image, cover_box.position + Vector2(1.0, 1.0), Color.RED, "The skipped host paints its own border")
	_check_pixel(image, photo_box.get_center(), Color.CYAN, "A replaced host paints its background and skips its image")
	_check(label.get_element_at(cover_box.get_center()) == cover and not label.scroll_to_anchor("target") and
		not label.focus_element(target), "Skipped descendants cannot be hit, scrolled to, or focused")
	cover.set_attribute("style", "content-visibility: visible")
	photo.set_attribute("style", "content-visibility: visible")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check_pixel(image, cover_box.position + Vector2(6.0, 30.0), Color.LIME, "Revealing contents restores the button paint")
	_check_pixel(image, photo_box.get_center(), Color("#c5692b"), "Revealing contents restores replaced image paint")
	_check(label.get_element_at(cover_box.get_center()) == target and label.focus_element(target),
		"Revealing contents restores hit testing and focus on the same handle")
	viewport.queue_free()


func _check_auto_content_visibility() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; }
		.auto { content-visibility: auto; contain-intrinsic-height: auto 45px; }
		.space { height: 1040px; }
		button { display: block; box-sizing: border-box; width: 100px; height: 72px; margin: 0; user-select: text; }
		#fixed { position: fixed; left: 7px; top: 4px; width: 10px; height: 10px; }
		@supports (content-visibility: auto) { #far { background: blue; } }
	</style>
	<section class="auto" id="near"><button id="first" type="button">FIRST</button><div id="fixed"></div></section>
	<div class="space"></div>
	<section class="auto" id="far"><button id="second" type="button">SECOND</button></section>
	<div class="space"></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(200, 120)) == OK, "Lay out auto content with a standalone viewport")
	var section: HTMLElement = layout.get_element_by_id("far")
	var child: HTMLElement = layout.get_element_by_id("second")
	_check(section.get_bounding_rect().size.y == 45.0 and child.get_client_rects().is_empty(),
		"An initially distant auto box uses its intrinsic fallback")
	_check("FIRST" in layout.get_plain_text() and "SECOND" in layout.get_plain_text(),
		"Distant content remains in the selectable text")
	_check(layout.get_element_by_id("fixed").get_bounding_rect().position ==
		layout.get_element_by_id("near").get_bounding_rect().position + Vector2(7.0, 4.0),
		"Auto content creates the fixed containing block while showing its contents")
	_check(layout.get_css_diagnostics().is_empty() and layout.layout(Vector2i(200, 1290)) == OK and
		child.get_bounding_rect().size.y == 72.0, "Growing the viewport reveals previously skipped boxes")
	_check(layout.layout(Vector2i(200, 120)) == OK and section.get_bounding_rect().size.y == 72.0 and
		child.get_client_rects().is_empty(), "Shrinking the viewport reuses the last rendered size")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(200, 120)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	var first: HTMLElement = label.get_element_by_id("first")
	var second: HTMLElement = label.get_element_by_id("second")
	_check(second.get_client_rects().is_empty() and label.scroll_to_anchor("second"), "A skipped anchor scrolls to its placeholder")
	await process_frame
	await process_frame
	await process_frame
	_check(second.get_bounding_rect().size.y == 72.0 and label.get_scroll_offset().y > 1000,
		"Scrolling to the placeholder reveals the anchored element")
	label.set_scroll_offset(Vector2i.ZERO)
	await process_frame
	await process_frame
	await process_frame
	_check(second.get_client_rects().is_empty() and label.focus_element(first), "The distant control skips again after scrolling back")
	_send_dom_key(viewport, KEY_TAB)
	await process_frame
	await process_frame
	await process_frame
	_check(label.get_focused_element() == second and second.get_bounding_rect().size.y == 72.0 and label.get_scroll_offset().y > 1000,
		"Tab reveals an offscreen control before bringing it into view")
	label.set_scroll_offset(Vector2i.ZERO)
	await process_frame
	await process_frame
	await process_frame
	_check(second.get_bounding_rect().size.y == 72.0, "Focus keeps distant contents relevant")
	label.release_focus()
	await process_frame
	await process_frame
	await process_frame
	_check(second.get_client_rects().is_empty(), "Losing focus lets the distant contents skip again")
	label.select_all()
	_check("SECOND" in label.get_selected_text(), "Select all includes skipped auto contents")
	await process_frame
	await process_frame
	await process_frame
	_check(second.get_bounding_rect().size.y == 72.0 and "SECOND" in label.get_selected_text(),
		"Selection keeps distant contents relevant")
	label.clear_selection()
	await process_frame
	await process_frame
	await process_frame
	_check(second.get_client_rects().is_empty(), "Clearing selection releases distant contents")

	label.text = """<!doctype html><style>
		body { margin: 0; }
		#port { width: 150px; height: 90px; overflow: auto; }
		.space { height: 720px; }
		#auto { content-visibility: auto; contain-intrinsic-height: 36px; }
		#hit { width: 85px; height: 55px; background: red; }
	</style><div id="port"><div class="space"></div><div id="auto"><div id="hit"></div></div><div class="space"></div></div>"""
	await process_frame
	await process_frame
	var nested: HTMLElement = label.get_element_by_id("hit")
	_check(nested.get_client_rects().is_empty() and label.scroll_to_element(nested), "A nested scroll port can navigate to an auto placeholder")
	await process_frame
	await process_frame
	await process_frame
	var nested_rect: Rect2 = nested.get_bounding_rect()
	var nested_hit: HTMLElement = label.get_element_at(Vector2(18.0, 18.0))
	var outer_scroll: Vector2i = label.get_scroll_offset()
	_check(nested_rect.size.y == 55.0 and nested_hit == nested and outer_scroll == Vector2i.ZERO,
		"Nested scrolling reveals contents without moving the outer viewport (box %s, hit %s, scroll %s)" %
		[nested_rect, nested_hit, outer_scroll,])
	label.hide()

	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.size = Vector2(viewport.size)
	markdown.parse_html = true
	var icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for name: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(name, icon)
	markdown.text = """<div><div style="height: 1000px"></div><div style="content-visibility: auto">NEEDLE</div></div>"""
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	_check(markdown.search("NEEDLE", 0, 0, 0).x >= 0 and markdown.scroll_to_search_result(),
		"Embedded Markdown blocks keep distant content searchable and scrollable in the outer document")
	viewport.queue_free()


func _check_generated_image() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(160, 100)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)

	var texture_image: Image = Image.create_empty(16, 12, false, Image.FORMAT_RGBA8)
	texture_image.fill(Color("#389f89"))
	var texture: ImageTexture = ImageTexture.create_from_image(texture_image)
	var document: HTMLDocument = HTMLDocument.new()
	document.text = '<link rel="stylesheet" href="styles/sheet.css"><div id="generated_image"></div>'
	document.set("_textures", { "res://styles/nested/picture.png": texture, })
	document.set("_local_stylesheets", {
		"res://styles/sheet.css": '@import "nested/content.css"; ',
		"res://styles/nested/content.css": '''
			body { margin: 0; }
			#generated_image::before { content: var(--picture); }
			#generated_image { --picture: url("picture.png"); }
		''',
	})
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(140.0, 90.0)
	label.document = document
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check(_color_bounds(image, Rect2i(Vector2i.ZERO, viewport.size), Color("#389f89")).size == Vector2i(16, 12),
		"A nested stylesheet resolves generated images from its own directory")
	_check(label.query_selector("img") == null, "A generated image stays outside DOM queries")
	var image_owner: HTMLElement = label.get_element_at(Vector2(5.0, 5.0))
	_check(image_owner != null and image_owner.get_attribute("id") == "generated_image", "Generated image hits return the originating element")
	viewport.queue_free()


func _check_list_markers() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 310)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)

	var html: String = """<!doctype html><style>
body { margin: 0; }
ol, ul { margin: 0; padding-left: 72px; }
li { line-height: 24px; }
li::marker { content: '(' counters(list-item, '.', decimal) ') '; color: #a020d0; font-weight: bold; }
li::before { content: '|'; }
#inside { list-style-position: inside; }
#hidden_marker::marker { content: none; }
#default_marker::marker { content: normal; color: #259955; }
#no_default::marker, #typed::marker, #typed_override::marker, #text_marker::marker, #quoted_type::marker { content: normal; }
#typed { list-style-type: revert; }
#typed_override { list-style-type: decimal; }
#text_marker { list-style: inside '>> '; }
#quoted_type { list-style-type: '§ '; }
#inside::marker { content: attr(data-label) ' '; color: #cf281f; display: none; width: 99px; }
#block_child::marker { content: '[B]'; color: #1735cd; }
#ordinary::marker { content: 'WRONG'; }
#step { counter-increment: list-item 2; }
</style>
<ol start="3">
  <li id="outside"><span id="outside_word">First</span><ol><li>Nested</li></ol></li>
  <li id="inside" data-label="Wide marker" value="8"><span id="inside_word">Second</span></li>
  <li id="step">Step</li>
  <li id="hidden_marker">Suppressed</li>
  <li id="default_marker">Default</li>
  <li id="block_child"><div>Block child</div></li>
</ol>
<ol reversed><li>Descending</li><li>Last</li></ol>
<ol type="A" start="4"><li id="typed">Typed</li><li id="typed_override">CSS type</li></ol>
<ul><li id="text_marker">Literal</li><li id="quoted_type">Quoted</li><li id="no_default" style="list-style-type: none">No default</li></ul>
<div id="ordinary">Ordinary</div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse(html) == OK and layout.layout(Vector2i(300, 300)) == OK, "Lay out list markers")
	var text: String = layout.get_plain_text()
	for fragment: String in ["(3) |First", "(3.1) |Nested", "Wide marker |Second", "(10) |Step",
			"|Suppressed", "12. |Default", "[B]", "(2) |Descending", "(1) |Last", "D. |Typed", "5. |CSS type",
			">> |Literal", "§ |Quoted", "|No default", "Ordinary"]:
		_check(fragment in text, "Rendered markers contain %s" % fragment)
	_check("WRONG" not in text and "(11)" not in text, "Marker rules only generate visible markers for list items")
	_check(layout.get_element_by_id("inside").get_text_content() == "Second", "Markers stay outside DOM text")
	_check(layout.query_selector_all("ol > li:first-child").size() == 4, "Marker nodes stay outside structural selectors")
	_check(_rect(layout, "inside_word").position.x > _rect(layout, "outside_word").position.x + 55.0,
		"An inside marker occupies its text width in the first line")
	var inside: HTMLElement = layout.get_element_by_id("inside")
	_check(inside.set_attribute("data-label", "New") == OK and layout.layout(Vector2i(290, 300)) == OK and
		"New |Second" in layout.get_plain_text(), "Attribute changes refresh marker content")

	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(320.0, 310.0)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var outside: Rect2 = _label_rect(label, "outside")
	var outside_pixels: Rect2i = _color_bounds(image, Rect2i(0, 0, 120, 24), Color("#a020d0"))
	_check(outside_pixels.has_area() and outside_pixels.end.x < outside.position.x,
		"An outside marker paints to the left of the list item")
	var inside_pixels: Rect2i = _color_bounds(image, Rect2i(Vector2i.ZERO, viewport.size), Color("#cf281f"))
	_check(inside_pixels.has_area() and inside_pixels.position.x >= _label_rect(label, "inside").position.x,
		"An inside marker inherits text styling and paints inside the content box")
	var block_pixels: Rect2i = _color_bounds(image, Rect2i(Vector2i.ZERO, viewport.size), Color("#1735cd"))
	_check(block_pixels.has_area() and block_pixels.end.x < _label_rect(label, "block_child").position.x,
		"An outside marker also paints beside block content")
	viewport.queue_free()


func _check_counter_styles() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """<!doctype html><style>
@import "styles/counters.css" layer(imported);
@layer imported, preferred;
@layer preferred {
  @counter-style layered { system: cyclic; symbols: high; }
}
@layer imported {
  @counter-style layered { system: cyclic; symbols: low; }
}
@counter-style digits { system: numeric; symbols: X Y; prefix: '{'; suffix: '} '; negative: '(' ')'; pad: 5 '0'; }
@counter-style limited { system: fixed 2; symbols: A B; range: 2 2; fallback: upper-roman; prefix: '['; suffix: '] '; }
@counter-style inherited { system: extends digits; prefix: '<'; suffix: '> '; }
@counter-style additive { system: additive; additive-symbols: 5 V, 2 X, 0 Z; fallback: decimal; }
@counter-style cycle-one { system: extends cycle-two; suffix: ') '; }
@counter-style cycle-two { system: extends cycle-one; pad: 3 '0'; }
@counter-style fallback-one { system: fixed; symbols: A; fallback: fallback-two; }
@counter-style fallback-two { system: fixed; symbols: B; fallback: fallback-one; }
@counter-style Repeat { symbols: R S; }
@counter-style repeat { system: cyclic; symbols: small; }
@counter-style repeated-rule { system: fixed; symbols: early; prefix: '!'; }
@counter-style repeated-rule { system: cyclic; symbols: late; symbols: 42; }
@counter-style bounded-output { system: symbolic; symbols: 'x'; prefix: '['; suffix: '] '; }
@counter-style max-int { system: fixed 2147483647; symbols: max; }
@counter-style responsive { system: fixed; symbols: narrow; }
@media (min-width: 400px) { @counter-style responsive { system: cyclic; symbols: wide; } }
@supports (list-style-type: symbols(cyclic '*')) { @counter-style conditional { symbols: YES; } }
@counter-style decimal { system: cyclic; symbols: WRONG; }
body { margin: 0; }
ol { margin: 0; padding: 0; list-style-position: inside; }
.digits { list-style-type: var(--counter-style, digits); }
.limited { list-style-type: limited; }
.inherited { list-style: inherited inside; }
.literal::before { counter-reset: sample 7; content: counter(sample, additive) '/' counter(sample, Repeat) '/'
  counter(sample, repeat) '/' counter(sample, cycle-two) '/' counter(sample, fallback-one) '/'
  counter(sample, symbols(alphabetic 'a' 'b')) '/' counter(sample, missing-style) ': '; }
.named::before { counter-reset: sample 1; content: counter(sample, layered) ' ' counter(sample, repeated-rule) ' '
  counter(sample, responsive) ' ' counter(sample, conditional) ' ' counter(sample, imported-counter) ' '; }
.bad::before { content: 'valid '; content: counter(sample, symbols(numeric 'one')); }
.local::before { counter-reset: sample 32; content: counter(sample, CJK-DECIMAL) '/' counter(sample, arabic-indic) '/'
  counter(sample, cjk-heavenly-stem) ': '; }
.large::before { counter-reset: sample 2000000000; content: counter(sample, bounded-output) ': '; }
.precision::before { counter-reset: sample 2147483647; content: counter(sample, max-int) ': '; }
</style>
<ol class="digits" start="-3"><li>minus</li></ol>
<ol class="inherited" start="3"><li>extended</li></ol>
<ol class="limited" start="2"><li>fixed</li><li>fallback</li></ol>
<ol style="list-style-type: symbols(cyclic 'red' 'blue')" start="0"><li>anonymous</li></ol>
<ol style="list-style-type: cycle-one"><li>cycle</li></ol>
<div class="literal">values</div><div class="named">names</div><div class="bad">invalid</div>
<div class="local">local</div><div class="large">large</div><div class="precision">precise</div>"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(360, 320)) == OK, "Lay out custom counter styles")
	var rendered: String = layout.get_plain_text()
	for fragment: String in ["{(0YY)} minus", "<000YY> extended", "[A] fixed", "[III] fallback", "blue anonymous",
			"1) cycle", "VX/RRRR/small/007/7/aaa/7: values", "high late narrow YES IMPORT names", "valid invalid",
			"三二/٣٢/三二: local", "2000000000: large", "max: precise"]:
		_check(fragment in rendered, "Custom counter output contains %s" % fragment)
	_check(layout.get_css_diagnostics().size() == 1, "Invalid symbols() keeps the previous content declaration")
	_check(layout.layout(Vector2i(430, 320)) == OK and "high late wide YES IMPORT names" in layout.get_plain_text(),
		"Media changes update counter style definitions")
	var separate: HTMLLayout = HTMLLayout.new()
	_check(separate.parse("<ol style='list-style: inside digits'><li>separate</li></ol>") == OK and
		separate.layout(Vector2i(360, 320)) == OK and "1. separate" in separate.get_plain_text(),
		"Counter style definitions stay in their own document")


func _check_font_faces() -> void:
	var document: HTMLDocument = load("{FONT_FACE_UID}")
	_check(document != null, "The HTML importer creates a document with font assets")
	if document == null:
		return
	var assets: Dictionary = document.get("_local_fonts")
	_check(assets.size() == 6 and assets.values().all(func(value: Variant) -> bool: return value is FontFile),
		"HTML imports font resources from every media branch in nested stylesheets")
	if assets.size() != 6:
		return
	var regular: FontFile = assets["res://fonts/times.ttf"]
	var narrow: FontFile = assets["res://fonts/cour.ttf"]
	var bold: FontFile = assets["res://fonts/arialbd.ttf"]
	var italic: FontFile = assets["res://fonts/georgiai.ttf"]
	var layout: HTMLLayout = document.create_layout()
	_check(layout.layout(Vector2i(320, 240)) == OK, "Lay out imported font faces")
	var element: HTMLElement = layout.get_element_by_id("normal")
	_check(is_equal_approx(element.get_bounding_rect().size.x, 7.0 * narrow.get_char_size(48, 40).x),
		"Case-insensitive family and ordered URL fallback select the mobile face")
	_check(is_equal_approx(_rect(layout, "bold").size.x, 7.0 * bold.get_char_size(48, 40).x),
		"Font weight selects the authored bold face")
	_check(is_equal_approx(_rect(layout, "italic").size.x, 7.0 * italic.get_char_size(48, 40).x),
		"Font style selects the authored italic face")
	_check(layout.layout(Vector2i(480, 240)) == OK and element.is_valid(),
		"Reflow across an authored font media breakpoint")
	_check(is_equal_approx(element.get_bounding_rect().size.x, 7.0 * regular.get_char_size(48, 40).x),
		"Changing the active faces invalidates cached font metrics")
	_check(layout.layout(Vector2i(320, 240)) == OK and
		is_equal_approx(element.get_bounding_rect().size.x, 7.0 * narrow.get_char_size(48, 40).x),
		"Reactivating the narrow face restores its metrics")
	_check_font_ranges(assets, regular, narrow, bold, italic)
	_check_local_font_faces(assets, regular, narrow, bold, italic)
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 320)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(320.0, 320.0)
	label.document = document
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var light: float = _font_ink(image, _label_rect(label, "light"))
	var heavy: float = _font_ink(image, _label_rect(label, "heavy"))
	var above: float = _font_ink(image, _label_rect(label, "above"))
	_check(light > 0.0 and heavy > light * 1.15,
		"The variable font paints thicker glyphs at the higher weight (%s vs %s)" % [heavy, light])
	_check(absf(heavy - above) < 1.0, "The variable weight is clamped to the descriptor range")
	viewport.queue_free()


func _check_local_font_faces(assets: Dictionary, regular: FontFile, narrow: FontFile, bold: FontFile,
		italic: FontFile) -> void:
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", assets)
	document.text = """<!doctype html><style>
		@font-face { font-family: FullName; src: local("Arial Bold"), url(fonts/cour.ttf); }
		@font-face { font-family: PostScript; src: local(Georgia-Italic); font-style: italic; }
		@font-face { font-family: Missing; src: local("Arial Regular"), local("Uninstalled Dockit face"), url(fonts/cour.ttf); }
		@font-face { font-family: WrongStyle; src: local("Georgia"), url(fonts/cour.ttf); font-style: italic; }
		@font-face { font-family: MathFace; src: local(CambriaMath), url(fonts/cour.ttf); }
		@font-face { font-family: CambriaFace; src: local(Cambria), url(fonts/cour.ttf); }
		@font-face { font-family: ProjectMath; src: url(fonts/cambria.ttc#Cambria%4Dath) format(collection); }
		@font-face { font-family: ProjectDefault; src: url(fonts/cambria.ttc) format(collection); }
		@font-face { font-family: InvalidFragment; src: url(fonts/cambria.ttc#Cambria%20Math),
			url(fonts/cambria.ttc#cambriamath), url(fonts/cambria.ttc#missing), url(fonts/cour.ttf); }
		body { margin: 0; font-size: 40px; }
		div { width: 7ch; height: 40px; }
		#full_name { font-family: FullName; }
		#postscript { font-family: PostScript; font-style: italic; }
		#missing { font-family: Missing; }
		#math { font-family: MathFace; }
		#cambria { font-family: CambriaFace; }
		#project_math { font-family: ProjectMath; }
		#project_default { font-family: ProjectDefault; }
		#invalid_fragment { font-family: InvalidFragment; }
		#family_only { font-family: WrongStyle; font-style: italic; }
		#literal { font-family: "Times New Roman"; }
		@font-face { font-family: "Times New Roman"; src: local(inherit), local(serif), local(""); }
	</style>
	<div id="full_name"></div><div id="postscript"></div><div id="missing"></div>
	<div id="math"></div><div id="cambria"></div><div id="family_only"></div><div id="literal"></div>
	<div id="project_math"></div><div id="project_default"></div><div id="invalid_fragment"></div>"""
	var layout: HTMLLayout = document.create_layout()
	_check(layout.layout(Vector2i(420, 440)) == OK, "Lay out installed and project font-face sources")
	var cambria_math: SystemFont = SystemFont.new()
	cambria_math.font_names = PackedStringArray(["Cambria Math"])
	var cambria: SystemFont = SystemFont.new()
	cambria.font_names = PackedStringArray(["Cambria"])
	var georgia: SystemFont = SystemFont.new()
	georgia.font_names = PackedStringArray(["Georgia"])
	var collection: FontFile = assets["res://fonts/cambria.ttc"]
	var project_math: FontFile = FontFile.new()
	project_math.data = collection.data
	project_math.set_face_index(0, 1)
	var server: TextServer = TextServerManager.get_primary_interface()
	var probe: RID = server.create_font()
	server.font_set_data(probe, collection.data)
	server.font_set_face_index(probe, 1)
	var names: Dictionary = server.font_get_ot_name_strings(probe).get("en", {})
	server.free_rid(probe)
	_check(names.get("postscript_name") == "CambriaMath", "Reference the second face in the project collection")
	var expected: Dictionary[String, float] = {
		"full_name": 7.0 * bold.get_char_size(48, 40).x,
		"postscript": 7.0 * italic.get_char_size(48, 40).x,
		"missing": 7.0 * narrow.get_char_size(48, 40).x,
		"math": 7.0 * cambria_math.get_char_size(48, 40).x,
		"cambria": 7.0 * cambria.get_char_size(48, 40).x,
		"project_math": 7.0 * project_math.get_char_size(48, 40).x,
		"project_default": 7.0 * cambria.get_char_size(48, 40).x,
		"invalid_fragment": 7.0 * narrow.get_char_size(48, 40).x,
		"family_only": 7.0 * georgia.get_char_size(48, 40).x,
		"literal": 7.0 * regular.get_char_size(48, 40).x,
	}
	for id: String in expected:
		var actual: float = _rect(layout, id).size.x
		_check(is_equal_approx(actual, expected[id]), "Installed font %s width %s vs %s" % [id, actual, expected[id]])


func _check_font_ranges(assets: Dictionary, regular: FontFile, narrow: FontFile, bold: FontFile,
		italic: FontFile) -> void:
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", assets)
	document.text = """<!doctype html><style>
		@layer base, overrides;
		@layer base {
			@font-face { font-family: Composite; src: url(fonts/times.ttf); unicode-range: U+0-ff; }
		}
		@layer overrides {
			@font-face { font-family: Composite; src: url(fonts/arialbd.ttf); unicode-range: U+4?, U+00E9; }
			@font-face { font-family: Composite; src: url(fonts/cour.ttf); unicode-range: u+49, U+30-39, U+10FFFF;
				unicode-range: U+10f-100; unicode-range: U+110000; unicode-range: U+??????;
				unicode-range: U+4?0; unicode-range: U+4 - 9; }
			@font-face { font-family: Composite; src: url(fonts/georgiai.ttf); unicode-range: U+5A; font-weight: bold; }
		}
		@layer base {
			@font-face { font-family: Composite; src: url(fonts/georgiai.ttf); unicode-range: U+49; }
		}
		@font-face { font-family: Secondary; src: url(fonts/georgiai.ttf); }
		@font-face { font-family: SpaceProbe; src: url(fonts/times.ttf); unicode-range: U+0-ff; }
		@font-face { font-family: SpaceProbe; src: url(fonts/cour.ttf); unicode-range: U+4?; }
		body { margin: 0; font: 40px Composite, Secondary, sans-serif; }
		span { display: inline-block; margin-right: 5px; }
		#metric { width: 7ch; }
		#ranged_height { font-family: SpaceProbe; width: 1ex; height: 1ex; }
		#reference_height { font-family: Composite; width: 1ex; height: 1ex; }
	</style>
	<div><span id="overlap">I</span><span id="wildcard">M</span><span id="base">Z</span>
		<span id="nested">IMZ</span><span id="fallback">Ω</span><span id="accented">é</span></div>
	<div><span id="metric"></span><span id="ranged_height"></span><span id="reference_height"></span></div>"""
	var layout: HTMLLayout = document.create_layout()
	_check(layout.layout(Vector2i(400, 180)) == OK, "Lay out the composite font")
	var expected: Dictionary[String, float] = {
		"overlap": narrow.get_char_size(73, 40).x,
		"wildcard": bold.get_char_size(77, 40).x,
		"base": regular.get_char_size(90, 40).x,
		"fallback": italic.get_char_size(937, 40).x,
		"accented": bold.get_char_size(233, 40).x,
		"metric": 7.0 * narrow.get_char_size(48, 40).x,
	}
	expected["nested"] = expected["overlap"] + expected["wildcard"] + expected["base"]
	for id: String in expected:
		var actual: float = _rect(layout, id).size.x
		_check(is_equal_approx(actual, expected[id]), "Composite font %s width %s vs %s" % [id, actual, expected[id]])
	_check(is_equal_approx(_rect(layout, "ranged_height").size.x, _rect(layout, "reference_height").size.x),
		"A face that excludes space supplies glyphs but does not supply ex metrics")
	var plain: String = layout.get_plain_text()
	var start: int = plain.find("IMZ")
	_check(start >= 0 and layout.get_selection_rects(start, start + 3).size() > 0,
		"The mixed-font text retains its original indices and selection geometry")


func _check_oblique_angles() -> void:
	var document: HTMLDocument = load("{FONT_FACE_UID}")
	if document == null:
		return
	var assets: Dictionary = document.get("_local_fonts")
	var regular: FontFile = assets["res://fonts/times.ttf"]
	var narrow: FontFile = assets["res://fonts/cour.ttf"]
	var bold: FontFile = assets["res://fonts/arialbd.ttf"]
	var fixture: HTMLDocument = HTMLDocument.new()
	fixture.set("_local_fonts", assets)
	fixture.text = """<!doctype html><style>
		@font-face { font-family: Slanted; src: url(fonts/times.ttf); font-style: oblique 8deg 1deg; }
		@font-face { font-family: Slanted; src: url(fonts/cour.ttf); font-style: oblique 17deg 24deg; }
		@font-face { font-family: Slanted; src: url(fonts/arialbd.ttf); font-style: oblique -25deg -5deg;
			font-style: oblique -91deg; }
		@font-face { font-family: Upright; src: url(fonts/cour.ttf); }
		body { margin: 0; background: black; color: white; font: 40px Slanted; }
		.geometry { width: 7ch; height: 4px; }
		.ink { font: 48px/63px Upright; height: 63px; padding-left: 50px; }
		#weak { font-style: oblique 10deg; }
		#strong { font: oblique 12deg 40px Slanted; }
		#default { font-style: oblique; }
		#zero { font-style: oblique calc(20deg - 20deg); }
		#reverse { font-style: oblique calc(-100grad / 5); }
		#inside { font-style: var(--slope); --slope: oblique 0.05turn; }
		#invalid { font-style: oblique 5deg; font-style: oblique 91deg; }
		#right { font-style: oblique 32deg; }
		#left { font-style: oblique -32deg; }
	</style>
	<div id="weak" class="geometry"></div><div id="strong" class="geometry"></div>
	<div id="default" class="geometry"></div><div id="zero" class="geometry"></div>
	<div id="reverse" class="geometry"></div><div id="inside" class="geometry"></div>
	<div id="invalid" class="geometry"></div>
	<div id="right" class="ink">I</div><div id="upright" class="ink">I</div><div id="left" class="ink">I</div>"""
	var layout: HTMLLayout = fixture.create_layout()
	_check(layout.layout(Vector2i(300, 260)) == OK, "Lay out angled font styles")
	var expected: Dictionary[String, float] = {
		"weak": 7.0 * regular.get_char_size(48, 40).x,
		"strong": 7.0 * narrow.get_char_size(48, 40).x,
		"default": 7.0 * narrow.get_char_size(48, 40).x,
		"zero": 7.0 * regular.get_char_size(48, 40).x,
		"reverse": 7.0 * bold.get_char_size(48, 40).x,
		"inside": 7.0 * narrow.get_char_size(48, 40).x,
		"invalid": 7.0 * regular.get_char_size(48, 40).x,
	}
	for id: String in expected:
		var actual: float = _rect(layout, id).size.x
		_check(is_equal_approx(actual, expected[id]), "Oblique %s width %s vs %s" % [id, actual, expected[id]])

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 260)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(300.0, 260.0)
	label.document = fixture
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var right: float = _ink_center_x(image, _label_rect(label, "right"))
	var upright: float = _ink_center_x(image, _label_rect(label, "upright"))
	var left: float = _ink_center_x(image, _label_rect(label, "left"))
	_check(right > upright + 3.0 and upright > left + 3.0,
		"Synthetic oblique applies the requested sign with independent font caches: %s, %s, %s" %
		[right, upright, left])
	viewport.queue_free()


func _check_font_width() -> void:
	var source: HTMLDocument = load("{FONT_FACE_UID}")
	if source == null:
		return
	var assets: Dictionary = source.get("_local_fonts")
	var regular: FontFile = assets["res://fonts/times.ttf"]
	var narrow: FontFile = assets["res://fonts/cour.ttf"]
	var bold: FontFile = assets["res://fonts/arialbd.ttf"]
	var variable: FontFile = assets["res://fonts/bahnschrift.ttf"]
	var fixture: HTMLDocument = HTMLDocument.new()
	fixture.set("_local_fonts", assets)
	fixture.text = """<!doctype html><style>
		@font-face { font-family: Widths; src: url(fonts/times.ttf); font-width: normal; }
		@font-face { font-family: Widths; src: url(fonts/cour.ttf); font-stretch: 80% 65%; }
		@font-face { font-family: Widths; src: url(fonts/arialbd.ttf); font-width: extra-expanded; }
		@font-face { font-family: Limits; src: url(fonts/bahnschrift.ttf); font-width: 90% 80%; }
		@font-face { font-family: Automatic; src: url(fonts/bahnschrift.ttf); font-width: auto; }
		body { margin: 0; font: 40px Widths; }
		div { width: 7ch; height: 5px; }
		#keyword { font-width: condensed; }
		#prefer_narrow { font-width: 98%; font-weight: bold; }
		#prefer_wide { font-width: 101%; }
		#alias { font-width: 100%; font-stretch: var(--measure); --measure: calc(50% + 25%); }
		#earlier_important { font-stretch: 75% !important; font-width: 100%; }
		#later_important { font-stretch: 75% !important; font-width: 100% !important; }
		#reset { font-stretch: 75%; font: 40px Widths; }
		#shorthand { font: oblique 12deg small-caps 700 condensed 40px Widths; }
		#inherit_parent { font-stretch: condensed; }
		#inherit { font-width: expanded; font-stretch: inherit; }
		#invalid { font-width: condensed; font-stretch: -10%; }
		#invalid_variable { font-stretch: var(--missing); }
		#limited_lower { font-family: Limits; font-width: 50%; }
		#limited_middle { font-family: Limits; font-stretch: 85%; }
		#limited_upper { font-family: Limits; font-width: 125%; }
		#automatic { font-family: Automatic; font-width: 75%; }
	</style>
	<div id="keyword"></div><div id="prefer_narrow"></div><div id="prefer_wide"></div>
	<div id="alias"></div><div id="earlier_important"></div><div id="later_important"></div>
	<div id="reset"></div><div id="shorthand"></div><section id="inherit_parent"><div id="inherit"></div></section>
	<div id="invalid"></div><div id="invalid_variable"></div>
	<div id="limited_lower"></div><div id="limited_middle"></div><div id="limited_upper"></div>
	<div id="automatic"></div>"""
	var layout: HTMLLayout = fixture.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(480, 280)) == OK, "Lay out font-width and its legacy alias")
	var plain_width: float = 7.0 * regular.get_char_size(48, 40).x
	var narrow_width: float = 7.0 * narrow.get_char_size(48, 40).x
	var expected: Dictionary[String, float] = {
		"keyword": narrow_width,
		"prefer_narrow": narrow_width,
		"prefer_wide": 7.0 * bold.get_char_size(48, 40).x,
		"alias": narrow_width,
		"earlier_important": narrow_width,
		"later_important": plain_width,
		"reset": plain_width,
		"shorthand": narrow_width,
		"inherit": narrow_width,
		"invalid": narrow_width,
		"invalid_variable": plain_width,
		"limited_lower": _variable_width_metric(variable, 80.0),
		"limited_middle": _variable_width_metric(variable, 85.0),
		"limited_upper": _variable_width_metric(variable, 90.0),
		"automatic": _variable_width_metric(variable, 75.0),
	}
	for id: String in expected:
		var actual: float = _rect(layout, id).size.x
		_check(is_equal_approx(actual, expected[id]), "Font width %s is %s vs %s" % [id, actual, expected[id]])
	_check(expected["limited_lower"] < expected["limited_upper"], "The test font exposes visible width changes")
	_check(_has_diagnostic(layout, "font-stretch", "-10%", "style_element"),
		"Invalid legacy values diagnose the original property name")
	_check(_has_diagnostic(layout, "font-stretch", "var(--missing)", "style_element"),
		"Invalid deferred legacy values retain the original property name")


func _variable_width_metric(font: FontFile, width: float) -> float:
	var variation: FontVariation = FontVariation.new()
	variation.base_font = font
	variation.variation_opentype = {0x77647468: width,}
	return 7.0 * variation.get_char_size(48, 40).x


func _check_opentype_settings() -> void:
	var source: HTMLDocument = load("{FONT_FACE_UID}")
	if source == null:
		return
	var assets: Dictionary = source.get("_local_fonts")
	var regular: FontFile = assets["res://fonts/times.ttf"]
	var variable: FontFile = assets["res://fonts/bahnschrift.ttf"]
	var fixture: HTMLDocument = HTMLDocument.new()
	fixture.set("_local_fonts", assets)
	fixture.text = """<!doctype html><style>
		@font-face { font-family: Regular; src: url(fonts/times.ttf); }
		@font-face { font-family: Quiet; src: url(fonts/times.ttf); font-feature-settings: "kern" off; }
		@font-face { font-family: Lead; src: url(fonts/cour.ttf); unicode-range: U+0058;
			font-feature-settings: "kern" off; }
		@font-face { font-family: Axes; src: url(fonts/bahnschrift.ttf); font-width: 85% 90%;
			font-variation-settings: "wdth" 84; font-variation-settings: "wdth"; }
		body { margin: 0; font: 40px Regular; }
		span { display: inline-block; }
		#unaffected, #invalid_literal, .plain_off { font-feature-settings: "kern" off; }
		#invalid_literal { font-feature-settings: kern; }
		#normal, #face_normal { font-feature-settings: normal; }
		#reset { font: var(--reset); --reset: 40px Regular; }
		#computed { font-feature-settings: var(--setting); --setting: "kern" off, "kern" calc(0.5), "zzzz" 2; }
		#face, #face_normal, #face_override { font-family: Quiet; }
		#face_override { font-feature-settings: "kern"; }
		#fallback { font-family: Lead, Regular; }
		#system { font-family: "Times New Roman"; font-feature-settings: "kern" 0; }
		.kerning_none { font-kerning: none; }
		#kerning_auto { font-kerning: auto; }
		#kerning_reset { font: 40px Regular; }
		#kerning_var { font-kerning: var(--kerning); --kerning: none; }
		#kerning_face { font-family: Quiet; font-kerning: normal; }
		#kerning_explicit { font-kerning: normal; font-feature-settings: "kern" 0; }
		.axis { font-family: Axes; width: 7ch; height: 5px; }
		#axis_override { font-variation-settings: "wdth" 100, "wdth" calc(150 / 2), "ZZZZ" -1.5; }
		.axis_parent { font-variation-settings: "wdth" 75; }
		#axis_normal { font-variation-settings: normal; }
		#axis_invalid { font-variation-settings: var(--missing); }
		#axis_clamp { font-variation-settings: "wdth" 200; }
		#axis_reset { font-variation-settings: "wdth" 75; font: 40px Axes; }
		#support { width: 10px; height: 5px; }
		@supports (font-feature-settings: "kern" 0) and (font-variation-settings: "wdth" 80.5) {
			#support { width: 17px; }
		}
	</style>
	<div><span id="default">AVAV</span><span id="unaffected">AVAV</span></div>
	<div class="plain_off"><span id="inherit">AVAV</span><span id="normal">AVAV</span>
		<span id="reset">AVAV</span><span id="computed">AVAV</span><span id="invalid_literal">AVAV</span></div>
	<div><span id="face">AVAV</span><span id="face_normal">AVAV</span>
		<span id="face_override">AVAV</span><span id="system">AVAV</span><span id="fallback">AVAV</span></div>
	<div class="kerning_none"><span id="kerning_inherit">AVAV</span><span id="kerning_auto">AVAV</span>
		<span id="kerning_reset">AVAV</span><span id="kerning_var">AVAV</span>
		<span id="kerning_face">AVAV</span><span id="kerning_explicit">AVAV</span></div>
	<div class="axis" id="axis_default"></div><div class="axis" id="axis_override"></div>
	<div class="axis" id="axis_invalid"></div><div class="axis" id="axis_clamp"></div>
	<div class="axis" id="axis_reset"></div>
	<section class="axis_parent"><div class="axis" id="axis_inherit"></div><div class="axis" id="axis_normal"></div></section>
	<div id="support"></div>"""
	var layout: HTMLLayout = fixture.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(700, 260)) == OK, "Lay out OpenType feature and variation settings")
	var disabled: FontVariation = FontVariation.new()
	disabled.base_font = regular
	disabled.opentype_features = {0x6b65726e: 0,}
	var with_kerning: float = regular.get_string_size("AVAV", HORIZONTAL_ALIGNMENT_LEFT, -1.0, 40).x
	var without_kerning: float = disabled.get_string_size("AVAV", HORIZONTAL_ALIGNMENT_LEFT, -1.0, 40).x
	_check(without_kerning - with_kerning > 1.0, "The fixture font changes advances when kerning is disabled")
	for id: String in [
		"default", "normal", "reset", "computed", "face_override", "fallback",
		"kerning_auto", "kerning_reset", "kerning_face",
	]:
		var actual: float = _rect(layout, id).size.x
		_check(absf(actual - with_kerning) <= 1.0, "%s uses kerning: %s vs %s" % [id, actual, with_kerning])
	for id: String in [
		"unaffected", "inherit", "invalid_literal", "face", "face_normal", "system",
		"kerning_inherit", "kerning_var", "kerning_explicit",
	]:
		var actual: float = _rect(layout, id).size.x
		_check(absf(actual - without_kerning) <= 1.0, "%s disables kerning: %s vs %s" % [id, actual, without_kerning])
	var widths: Dictionary[String, float] = {
		"axis_default": 84.0,
		"axis_override": 75.0,
		"axis_invalid": 84.0,
		"axis_clamp": 100.0,
		"axis_reset": 84.0,
		"axis_inherit": 75.0,
		"axis_normal": 84.0,
	}
	for id: String in widths:
		var expected: float = _variable_width_metric(variable, widths[id])
		var actual: float = _rect(layout, id).size.x
		_check(is_equal_approx(actual, expected), "OpenType axis %s is %s vs %s" % [id, actual, expected])
	_check(is_equal_approx(_rect(layout, "support").size.x, 17.0), "@supports recognizes both settings")
	_check(_has_diagnostic(layout, "font-feature-settings", "kern", "style_element"), "Unquoted tags are invalid")
	_check(_has_diagnostic(layout, "font-variation-settings", "var(--missing)", "style_element"),
		"Invalid variables in axis settings retain their original declaration")


func _check_font_variants() -> void:
	const FONT_UID: String = "{VARIANT_FONT_UID}"
	const LIGATURE_SAMPLE: String = "officeaffliction"
	const NUMERIC_SAMPLE: String = "1111890"
	const CAPS_SAMPLE: String = "Titlecase"
	const COMBINED_SAMPLE: String = LIGATURE_SAMPLE + NUMERIC_SAMPLE
	if FONT_UID.is_empty():
		print("Skipping Latin font variant fixture because Calibri is unavailable")
		return
	var regular: FontFile = load(FONT_UID)
	_check(regular != null, "Load the Latin font variant fixture")
	if regular == null:
		return
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", {"res://fonts/variant.ttf": regular,})
	document.text = """<!doctype html><style>
		@font-face { font-family: Probe; src: url(fonts/variant.ttf); }
		@font-face { font-family: Quiet; src: url(fonts/variant.ttf); font-feature-settings: "liga" 0; }
		body { margin: 0; font: 48px Probe; }
		span { display: inline-block; }
		.ligatures-off { font-variant-ligatures: no-common-ligatures; }
		#ligature_reset { font: 48px Probe; }
		#ligature_override { font-feature-settings: "liga" on; }
		#face_initial, #face_override { font-family: Quiet; }
		#face_override { font-variant-ligatures: common-ligatures; }
		#numeric { font-variant-numeric: proportional-nums; }
		#conflict { font-variant-numeric: proportional-nums; font-variant-numeric: tabular-nums proportional-nums; }
		#combined { --forms: no-common-ligatures tabular-nums; font-variant: var(--forms);
			font-variant-numeric: proportional-nums; }
		#caps { font-variant-caps: all-small-caps; }
		#petite { font-variant-caps: all-petite-caps; }
		#legacy { font: small-caps 48px Probe; }
		.numeric { font-variant-numeric: proportional-nums; }
		#none { font-variant: none; }
		#none_explicit { font-variant: none; font-feature-settings: "liga" on; }
		#tracked, #tracked_ligature { letter-spacing: 3px; }
		#tracked_ligature { font-feature-settings: "liga" on; }
		#support { width: 19px; height: 4px; }
		@supports (font-variant: small-caps tabular-nums historical-ligatures) and
			(font-variant-numeric: slashed-zero oldstyle-nums ordinal stacked-fractions proportional-nums) and
			(not (font-variant-ligatures: normal no-common-ligatures)) {
			#support { width: 41px; }
		}
	</style>
	<div><span id="ligature_initial">{ligatures}</span></div>
	<div class="ligatures-off"><span id="ligature_inherit">{ligatures}</span></div>
	<div class="ligatures-off"><span id="ligature_reset">{ligatures}</span></div>
	<div class="ligatures-off"><span id="ligature_override">{ligatures}</span></div>
	<div><span id="face_initial">{ligatures}</span><span id="face_override">{ligatures}</span></div>
	<div><span id="numeric">{numeric}</span><span id="conflict">{numeric}</span></div>
	<div><span id="combined">{combined}</span></div>
	<div><span id="caps">{caps}</span><span id="petite">{caps}</span><span id="legacy">{caps}</span></div>
	<div class="numeric"><span id="none">{combined}</span></div>
	<div class="numeric"><span id="none_explicit">{combined}</span></div><div id="support"></div>
	<div><span id="tracked">{ligatures}</span><span id="tracked_ligature">{ligatures}</span></div>
	""".replace("{ligatures}", LIGATURE_SAMPLE).replace("{numeric}", NUMERIC_SAMPLE) \
		.replace("{caps}", CAPS_SAMPLE).replace("{combined}", COMBINED_SAMPLE)
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(950, 850)) == OK, "Lay out high-level OpenType feature properties")
	var ligatures: float = _feature_text_width(regular, LIGATURE_SAMPLE, {})
	var no_ligatures: float = _feature_text_width(regular, LIGATURE_SAMPLE, {"liga": 0, "clig": 0,})
	var ligature_line: TextLine = TextLine.new()
	ligature_line.add_string(LIGATURE_SAMPLE, regular, 48)
	var text_server: TextServer = TextServerManager.get_primary_interface()
	var ligature_clusters: int = text_server.shaped_text_get_glyph_count(ligature_line.get_rid())
	var proportional: float = _feature_text_width(regular, NUMERIC_SAMPLE, {"pnum": 1,})
	var small_caps: float = _feature_text_width(regular, CAPS_SAMPLE, {"smcp": 1, "c2sc": 1,})
	_check(absf(ligatures - no_ligatures) > 0.1, "The fixture has measurable common ligatures")
	_check(absf(proportional - _feature_text_width(regular, NUMERIC_SAMPLE, {})) > 0.1,
		"The fixture has measurable proportional digits")
	_check(absf(small_caps - _feature_text_width(regular, CAPS_SAMPLE, {})) > 0.1,
		"The fixture has measurable small capitals")
	var expected: Dictionary[String, float] = {
		"ligature_initial": ligatures,
		"ligature_inherit": no_ligatures,
		"ligature_reset": ligatures,
		"ligature_override": ligatures,
		"face_initial": no_ligatures,
		"face_override": ligatures,
		"numeric": proportional,
		"conflict": proportional,
		"combined": _feature_text_width(regular, COMBINED_SAMPLE, {"liga": 0, "clig": 0, "pnum": 1,}),
		"caps": small_caps,
		"petite": small_caps,
		"legacy": _feature_text_width(regular, CAPS_SAMPLE, {"smcp": 1,}),
		"none": _feature_text_width(regular, COMBINED_SAMPLE, {"liga": 0, "clig": 0, "calt": 0, "dlig": 0, "hlig": 0,}),
		"none_explicit": _feature_text_width(regular, COMBINED_SAMPLE,
			{"liga": 1, "clig": 0, "calt": 0, "dlig": 0, "hlig": 0,}),
		"support": 41.0,
		"tracked": no_ligatures + float(LIGATURE_SAMPLE.length() - 1) * 3.0,
		"tracked_ligature": ligatures + float(ligature_clusters - 1) * 3.0,
	}
	for id: String in expected:
		var width: float = _rect(layout, id).size.x
		_check(absf(width - expected[id]) <= 0.1, "Font variant %s is %s vs %s" % [id, width, expected[id]])
	_check(_has_diagnostic(layout, "font-variant-numeric", "tabular-nums proportional-nums", "style_element"),
		"Conflicting numeric alternatives are rejected atomically")


func _check_named_font_alternates() -> void:
	const FONT_UID: String = "{VARIANT_FONT_UID}"
	const SET_SAMPLE: String = "Monogram"
	const STYLISTIC_SAMPLE: String = "QwertyAmpersand&"
	if FONT_UID.is_empty():
		print("Skipping named font alternates because Calibri is unavailable")
		return
	var font: FontFile = load(FONT_UID)
	_check(font != null, "Load the named font alternates fixture")
	if font == null:
		return
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", {"res://fonts/variant.ttf": font,})
	document.text = """<!doctype html><style>
		@import "styles/font-alternates.css";
		@layer base, theme;
		@font-face { font-family: Probe; src: url(fonts/variant.ttf); }
		@font-face { font-family: Twin; src: url(fonts/variant.ttf); }
		@font-face { font-family: Unmapped; src: url(fonts/variant.ttf); }
		@font-face { font-family: Partial; src: url(fonts/variant.ttf); unicode-range: U+0058; }
		@layer theme { @font-feature-values "pRoBe" { @styleset { chosen: 4; } } }
		@layer base { @font-feature-values Probe { @styleset { chosen: 1; } } }
		@font-feature-values Probe, Twin {
			@styleset { CaseName: 4; combined: 1 4; noop: 4; responsive: 1; valid: 4; disabled: 4; }
			@stylistic { custom: 1; custom: 1 2; }
			@unknown { ignored: 4; }
		}
		@font-feature-values probe { @styleset { noop: 0 100 125; valid: -1; } }
		@font-feature-values serif, Probe { @styleset { invalid-family-list: 4; } }
		@media (min-width: 800px) { @font-feature-values Probe { @styleset { responsive: 4; } } }
		body { margin: 0; font: 48px Probe; }
		span { display: inline-block; }
		#layered, .inherited { font-variant-alternates: styleset(chosen); }
		#case_name { font-variant-alternates: styleset(CaseName); }
		#case_mismatch { font-variant-alternates: styleset(casename); }
		#family_list { font-family: Twin; font-variant-alternates: styleset(combined); }
		#unmapped { font-family: Unmapped; font-variant-alternates: styleset(CaseName); }
		#fallback { font-family: Partial, Probe; font-variant-alternates: styleset(chosen); }
		#many { font-variant-alternates: styleset(imported, combined, unknown); }
		#imported { font-variant-alternates: styleset(imported); }
		#noop { font-variant-alternates: styleset(noop); }
		#bad_family { font-variant-alternates: styleset(invalid-family-list); }
		#bad_definition { font-variant-alternates: styleset(valid); }
		#responsive { font-variant-alternates: styleset(responsive); }
		#overridden { font-variant-alternates: styleset(disabled); font-feature-settings: "ss04" 0; }
		#stylistic { font-variant-alternates: stylistic(custom); }
		#shorthand { --v: tabular-nums styleset(chosen); font-variant: var(--v); }
		#font_reset { font: 48px Probe; }
		#variant_reset { font-variant: normal; }
		#invalid { font-variant-alternates: styleset(chosen); font-variant-alternates: styleset(chosen) styleset(noop); }
		#support { width: 19px; height: 4px; }
		@supports at-rule(@font-feature-values) and
			(font-variant-alternates: historical-forms character-variant(First, Other) swash(S) ornaments(O) annotation(A)) and
			(not (font-variant-alternates: stylistic(first, second))) {
			#support { width: 53px; }
		}
	</style>
	<div><span id="base">{set}</span><span id="layered">{set}</span></div>
	<div><span id="case_name">{set}</span><span id="case_mismatch">{set}</span></div>
	<div><span id="family_list">{set}</span><span id="unmapped">{set}</span></div>
	<div><span id="fallback">{set}</span><span id="many">{set}</span><span id="noop">{set}</span></div>
	<div><span id="imported">{set}</span></div>
	<div><span id="bad_family">{set}</span><span id="bad_definition">{set}</span></div>
	<div><span id="responsive">{set}</span><span id="overridden">{set}</span></div>
	<div><span id="stylistic">{stylistic}</span></div>
	<div><span id="shorthand">{set}</span><span id="invalid">{set}</span></div>
	<div class="inherited"><span id="inherit">{set}</span><span id="font_reset">{set}</span>
		<span id="variant_reset">{set}</span></div><div id="support"></div>
	""".replace("{set}", SET_SAMPLE).replace("{stylistic}", STYLISTIC_SAMPLE.xml_escape())
	var layout: HTMLLayout = document.create_layout()
	_check(layout.layout(Vector2i(760, 850)) == OK, "Lay out named OpenType alternates")
	var base: float = _feature_text_width(font, SET_SAMPLE, {})
	var ss04: float = _feature_text_width(font, SET_SAMPLE, {"ss04": 1,})
	var salt: float = _feature_text_width(font, STYLISTIC_SAMPLE, {"salt": 1,})
	_check(absf(base - ss04) > 0.1, "Fixture provides a measurable stylistic set")
	_check(absf(salt - _feature_text_width(font, STYLISTIC_SAMPLE, {})) > 1.0, "Fixture provides a measurable stylistic alternate")
	var expected: Dictionary[String, float] = {
		"base": base,
		"layered": ss04,
		"case_name": ss04,
		"case_mismatch": base,
		"family_list": _feature_text_width(font, SET_SAMPLE, {"ss01": 1, "ss04": 1,}),
		"unmapped": base,
		"fallback": ss04,
		"many": _feature_text_width(font, SET_SAMPLE, {"ss01": 1, "ss04": 1,}),
		"imported": ss04,
		"noop": base,
		"bad_family": base,
		"bad_definition": ss04,
		"responsive": _feature_text_width(font, SET_SAMPLE, {"ss01": 1,}),
		"overridden": base,
		"stylistic": salt,
		"shorthand": ss04,
		"invalid": ss04,
		"inherit": ss04,
		"font_reset": base,
		"variant_reset": base,
		"support": 53.0,
	}
	for id: String in expected:
		var width: float = _rect(layout, id).size.x
		_check(absf(width - expected[id]) <= 0.1, "Named font alternate %s is %s vs %s" % [id, width, expected[id]])
	_check(layout.layout(Vector2i(860, 850)) == OK and absf(_rect(layout, "responsive").size.x - ss04) <= 0.1,
		"Activating a conditional definition refreshes text shaping")
	_check(layout.layout(Vector2i(760, 850)) == OK and absf(_rect(layout, "responsive").size.x - expected["responsive"]) <= 0.1,
		"Removing a conditional definition restores the earlier feature value")


func _check_historical_forms() -> void:
	const FONT_UID: String = "{HISTORICAL_FONT_UID}"
	const SAMPLE: String = "QwertyAmpersand&"
	if FONT_UID.is_empty():
		print("Skipping historical forms because Palatino Linotype is unavailable")
		return
	var font: FontFile = load(FONT_UID)
	_check(font != null, "Load the historical forms fixture")
	if font == null:
		return
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", {"res://fonts/historical.ttf": font,})
	document.text = """<!doctype html><style>
		@font-face { font-family: History; src: url(fonts/historical.ttf); }
		body { margin: 0; font: 48px History; }
		span { display: inline-block; }
		#historical { font-variant-alternates: historical-forms; }
	</style><span id="historical">%s</span>""" % SAMPLE.xml_escape()
	var layout: HTMLLayout = document.create_layout()
	_check(layout.layout(Vector2i(640, 360)) == OK, "Lay out historical forms")
	var expected: float = _feature_text_width(font, SAMPLE, {"hist": 1,})
	_check(absf(expected - _feature_text_width(font, SAMPLE, {})) > 1.0, "Fixture provides measurable historical forms")
	_check(absf(_rect(layout, "historical").size.x - expected) <= 0.1, "Historical forms work without a named definition")


func _check_east_asian_variants() -> void:
	const FONT_UID: String = "{EAST_ASIAN_FONT_UID}"
	const SAMPLE: String = "AA11"
	if FONT_UID.is_empty():
		print("Skipping East Asian font fixture because YuGothM.ttc is unavailable")
		return
	var font: FontFile = load(FONT_UID)
	_check(font != null and font.get_supported_feature_list().has(0x66776964), "Fixture font provides full-width forms")
	if font == null:
		return
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", {"res://fonts/east_asian.ttc": font,})
	document.text = """<!doctype html><style>
		@font-face { font-family: Eastern; src: url(fonts/east_asian.ttc); }
		body { margin: 0; font: 48px Eastern; }
		span { display: inline-block; }
		#full, #overridden { font-variant-east-asian: jis04 full-width ruby; }
		#overridden { font-feature-settings: "fwid" off; }
		#shorthand { font-variant: full-width lining-nums; }
		#support { width: 19px; height: 4px; }
		@supports (font-variant-east-asian: traditional proportional-width ruby) and
			(not (font-variant-east-asian: simplified traditional)) {
			#support { width: 43px; }
		}
	</style>
	<div><span id="full">{sample}</span><span id="overridden">{sample}</span></div>
	<div><span id="shorthand">{sample}</span></div><div id="support"></div>
	""".replace("{sample}", SAMPLE)
	var layout: HTMLLayout = document.create_layout()
	_check(layout.layout(Vector2i(540, 380)) == OK, "Lay out East Asian variants")
	var full: float = _feature_text_width(font, SAMPLE, {"fwid": 1, "jp04": 1, "ruby": 1,})
	var overridden: float = _feature_text_width(font, SAMPLE, {"fwid": 0, "jp04": 1, "ruby": 1,})
	_check(absf(full - overridden) > 1.0, "The fixture has measurable East Asian width forms")
	_check(absf(_rect(layout, "full").size.x - full) <= 0.1, "East Asian options combine across categories")
	_check(absf(_rect(layout, "overridden").size.x - overridden) <= 0.1, "Explicit features override East Asian forms")
	var shorthand: float = _feature_text_width(font, SAMPLE, {"fwid": 1, "lnum": 1,})
	_check(absf(_rect(layout, "shorthand").size.x - shorthand) <= 0.1, "Shorthand accepts East Asian and numeric forms")
	_check(is_equal_approx(_rect(layout, "support").size.x, 43.0), "East Asian categories reject conflicting alternatives")


func _feature_text_width(font: Font, sample: String, features: Dictionary) -> float:
	var variation: FontVariation = FontVariation.new()
	variation.base_font = font
	variation.opentype_features = features
	return variation.get_string_size(sample, HORIZONTAL_ALIGNMENT_LEFT, -1.0, 48).x


func _check_optical_sizing() -> void:
	const FONT_UID: String = "{OPTICAL_FONT_UID}"
	const OPTICAL_AXIS: int = 0x6f70737a
	const SAMPLE: String = "MMMMmmmmminimum"
	if FONT_UID.is_empty():
		print("Skipping optical sizing fixture because SegUIVar.ttf is unavailable")
		return
	var font: FontFile = load(FONT_UID)
	_check(font != null and font.get_supported_variation_list().has(OPTICAL_AXIS), "Fixture font exposes the opsz axis")
	if font == null or not font.get_supported_variation_list().has(OPTICAL_AXIS):
		return
	var fixture: HTMLDocument = HTMLDocument.new()
	fixture.set("_local_fonts", {"res://fonts/optical.ttf": font,})
	fixture.text = """<!doctype html><style>
		@font-face { font-family: Optical; src: url(fonts/optical.ttf); }
		@font-face { font-family: FixedOptical; src: url(fonts/optical.ttf); font-variation-settings: "opsz" 8; }
		body { margin: 0; font: 32px Optical; }
		span { display: inline-block; }
		#small, #responsive { font-size: 16px; }
		#large { font-size: 64px; }
		.off { font-optical-sizing: var(--optical); --optical: none; }
		#invalid { font-optical-sizing: normal; }
		#reset { font: 32px Optical; }
		#explicit { font-variation-settings: "opsz" 31; }
		#descriptor, #descriptor_auto, #descriptor_override { font-family: FixedOptical; }
		#descriptor_override { font-variation-settings: "opsz" 31; }
		@media (min-width: 800px) { #responsive { font-size: 32px; } }
	</style>
	<div><span id="small">{sample}</span></div><div><span id="auto">{sample}</span></div>
	<div><span id="large">{sample}</span></div><div><span id="responsive">{sample}</span></div>
	<div class="off"><span id="none">{sample}</span></div><div class="off"><span id="invalid">{sample}</span></div>
	<div class="off"><span id="reset">{sample}</span></div><div class="off"><span id="explicit">{sample}</span></div>
	<div class="off"><span id="descriptor">{sample}</span></div><div><span id="descriptor_auto">{sample}</span></div>
	<div><span id="descriptor_override">{sample}</span></div>
	""".replace("{sample}", SAMPLE)
	var layout: HTMLLayout = fixture.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(760, 540)) == OK, "Lay out automatic and explicit optical sizing")
	var baseline: float = font.get_string_size(SAMPLE, HORIZONTAL_ALIGNMENT_LEFT, -1.0, 32).x
	var automatic: float = _optical_text_width(font, SAMPLE, 32, 24.0)
	_check(absf(baseline - automatic) > 1.0, "Optical sizing changes actual glyph advances")
	var expected: Dictionary[String, float] = {
		"small": _optical_text_width(font, SAMPLE, 16, 12.0),
		"auto": automatic,
		"large": _optical_text_width(font, SAMPLE, 64, 36.0),
		"responsive": _optical_text_width(font, SAMPLE, 16, 12.0),
		"none": baseline,
		"invalid": baseline,
		"reset": automatic,
		"explicit": _optical_text_width(font, SAMPLE, 32, 31.0),
		"descriptor": _optical_text_width(font, SAMPLE, 32, 8.0),
		"descriptor_auto": automatic,
		"descriptor_override": _optical_text_width(font, SAMPLE, 32, 31.0),
	}
	for id: String in expected:
		var actual: float = _rect(layout, id).size.x
		_check(absf(actual - expected[id]) <= 1.0, "Optical size %s is %s vs %s" % [id, actual, expected[id]])
	_check(_has_diagnostic(layout, "font-optical-sizing", "normal", "style_element"), "Optical sizing reports invalid values")
	_check(layout.layout(Vector2i(880, 540)) == OK and absf(_rect(layout, "responsive").size.x - automatic) <= 1.0,
		"Media changes select the optical size for the new font size")
	var installed: SystemFont = SystemFont.new()
	installed.font_names = PackedStringArray(["Segoe UI Variable",])
	if installed.get_supported_variation_list().has(OPTICAL_AXIS):
		var system_layout: HTMLLayout = HTMLLayout.new()
		var html: String = """<!doctype html><span id="system" style="display: inline-block;
			font: 32px 'Segoe UI Variable'">%s</span>""" % SAMPLE
		var system_width: float = _optical_text_width(installed, SAMPLE, 32, 24.0)
		_check(system_layout.parse(html) == OK and system_layout.layout(Vector2i(760, 540)) == OK and
			absf(_rect(system_layout, "system").size.x - system_width) <= 1.0, "Installed fonts use automatic sizing")


func _optical_text_width(font: Font, sample: String, size: int, optical_size: float) -> float:
	var variation: FontVariation = FontVariation.new()
	variation.base_font = font
	variation.variation_opentype = {0x6f70737a: optical_size,}
	return variation.get_string_size(sample, HORIZONTAL_ALIGNMENT_LEFT, -1.0, size).x


func _check_text_spacing() -> void:
	var source: HTMLDocument = load("{FONT_FACE_UID}")
	if source == null:
		return
	var assets: Dictionary = source.get("_local_fonts")
	var font: FontFile = assets["res://fonts/cour.ttf"]
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", assets)
	document.text = """<!doctype html><style>
		@font-face { font-family: Mono; src: url(fonts/cour.ttf); }
		body { margin: 0; font: 20px / 28px Mono; }
		.probe { display: inline-block; }
		#letter, #selected { letter-spacing: 2.5px; }
		#negative { letter-spacing: -2.5px; }
		#separators, #nbsp, #fixed { word-spacing: 3.25px; }
		#negative_words { word-spacing: -3.25px; }
		#ascii { letter-spacing: 3.25px; word-spacing: 3.25px; }
		.inherited { letter-spacing: calc(10% + 1px); word-spacing: 0.25em; }
		.big { font-size: 40px; }
		#em_parent { letter-spacing: 0.15em; }
		#outside { letter-spacing: 2px; }
		#inside { letter-spacing: 6px; }
		.wrap { width: 41px; }
		#wrap, #wrap_closed { word-spacing: 8px; }
		#cursive { letter-spacing: 7px; }
		#invalid { letter-spacing: 2.5px; letter-spacing: auto; }
		#variable { --spacing: 2.5px; letter-spacing: var(--spacing); }
	</style>
	<div><span class="probe" id="letter">ABCD</span><span class="probe" id="negative">ABCD</span></div>
	<div><span class="probe" id="separators">A    B C   </span></div>
	<div><span class="probe" id="negative_words">A B C</span></div>
	<div><span class="probe" id="nbsp">A&nbsp;B</span><span class="probe" id="ascii">A B</span></div>
	<div><span class="probe" id="fixed">A&#x3000;B</span><span class="probe" id="fixed_base">A&#x3000;B</span></div>
	<div class="inherited"><span class="probe big" id="percent">A B</span></div>
	<div id="em_parent"><span class="probe big" id="em">AB</span></div>
	<div><span class="probe" id="outside"><span id="left">M</span><span id="inside">N</span></span></div>
	<div class="wrap" id="wrap">A B</div><div class="wrap" id="wrap_base">A B</div>
	<div class="wrap" id="wrap_closed"><span id="wrap_first">A </span><span>B</span></div>
	<div><span class="probe" id="cursive">سلام</span><span class="probe" id="cursive_base">سلام</span></div>
	<div><span class="probe" id="invalid">ABCD</span><span class="probe" id="variable">ABCD</span></div>
	<div><span class="probe" id="selected">WXYZ</span></div>
	"""
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(620, 650)) == OK, "Lay out fractional letter and word spacing")
	var unit: float = font.get_char_size(65, 20).x
	var large_unit: float = font.get_char_size(65, 40).x
	var expected: Dictionary[String, float] = {
		"letter": unit * 4.0 + 7.5,
		"negative": unit * 4.0 - 7.5,
		"separators": unit * 5.0 + 6.5,
		"negative_words": unit * 5.0 - 6.5,
		"nbsp": unit * 3.0 + 3.25,
		"ascii": unit * 3.0 + 9.75,
		"fixed": _rect(layout, "fixed_base").size.x,
		"percent": large_unit * 3.0 + 15.0,
		"em": large_unit * 2.0 + 3.0,
		"outside": unit * 2.0 + 4.0,
		"cursive": _rect(layout, "cursive_base").size.x,
		"invalid": unit * 4.0 + 7.5,
		"variable": unit * 4.0 + 7.5,
	}
	for id: String in expected:
		var width: float = _rect(layout, id).size.x
		_check(absf(width - expected[id]) <= 0.1, "Text spacing %s is %s vs %s" % [id, width, expected[id]])
	var left: Rect2 = _rect(layout, "left")
	var right: Rect2 = _rect(layout, "inside")
	var adjacent: int = layout.get_plain_text().find("MN")
	var adjacent_rects: Array[Rect2] = layout.get_selection_rects(adjacent, adjacent + 2)
	_check(adjacent_rects.size() == 2 and absf(adjacent_rects[1].position.x - adjacent_rects[0].end.x - 4.0) <= 0.1,
		"Adjacent elements share half their letter spacing")
	_check(absf(left.size.x - unit - 1.0) <= 0.1 and absf(right.size.x - unit - 3.0) <= 0.1,
		"Inline boxes include their own half of the shared spacing")
	_check(is_equal_approx(_rect(layout, "wrap").size.y, 56.0) and is_equal_approx(_rect(layout, "wrap_base").size.y, 28.0),
		"Word spacing participates in line breaking")
	_check(is_equal_approx(_rect(layout, "wrap_first").size.x, unit), "A wrapped line trims spaces before inline end markers")
	_check(_has_diagnostic(layout, "letter-spacing", "auto", "style_element"), "Invalid spacing preserves the earlier declaration")
	var offset: int = layout.get_plain_text().find("WXYZ")
	var selected: Rect2 = _rect(layout, "selected")
	var highlights: Array[Rect2] = layout.get_selection_rects(offset, offset + 4)
	_check(offset >= 0 and highlights.size() == 1 and absf(highlights[0].size.x - selected.size.x) <= 0.1,
		"Selection uses the same spaced width as layout")
	var caret: Vector2 = Vector2(selected.position.x + unit * 2.0 + 3.75, selected.get_center().y)
	_check(layout.hit_test_text(caret) == offset + 2, "Hit testing maps spaced glyphs back to the DOM text")
	await _check_text_spacing_paint(assets)


func _check_text_autospace() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """
		<!doctype html><style>
			body { margin: 0; font-size: 32px; line-height: 40px; }
			span.probe { display: inline-block; }
			.none { text-autospace: no-autospace; }
			.alpha { text-autospace: var(--spacing); --spacing: ideograph-alpha; }
			.numeric { text-autospace: ideograph-numeric; }
			.replace { text-autospace: replace; }
			.letter { letter-spacing: 5px; }
			.word { word-spacing: 7px; }
			.french { text-autospace: punctuation; }
			.french.replace { text-autospace: replace punctuation; }
			#second { text-autospace: no-autospace; }
			#edge_left { padding-right: 7px; }
		</style>
		<div><span id="ic" class="probe">水</span></div>
		<div><span id="none" class="probe none">水A水1</span></div>
		<div><span id="normal" class="probe">水A水1</span></div>
		<div><span id="alpha" class="probe alpha">水A水1</span></div>
		<div><span id="numeric" class="probe numeric">水A水1</span></div>
		<div><span id="fullwidth" class="probe">水Ａ水１</span></div>
		<div><span id="fullwidth_ref" class="probe none">水Ａ水１</span></div>
		<div><span id="letters" class="probe letter">水A水1</span></div>
		<div><span id="letters_ref" class="probe letter none">水A水1</span></div>
		<div><span id="replace" class="probe replace">水 R</span></div>
		<div><span id="replace_ref" class="probe">水R</span></div>
		<div><span id="words" class="probe replace word">水    W</span></div>
		<div><span id="words_ref" class="probe">水W</span></div>
		<div><span id="insert" class="probe" style="text-autospace: insert">水 R</span></div>
		<div><span id="insert_ref" class="probe none">水 R</span></div>
		<div><span id="nbsp" class="probe replace">水&nbsp;R</span></div>
		<div><span id="nbsp_ref" class="probe none">水&nbsp;R</span></div>
		<div lang="fr-CA"><span id="french" class="probe french">Bonjour!</span></div>
		<div lang="fr"><span id="fr_replace" class="probe french replace">Bonjour !</span></div>
		<div><span id="fr_ref" class="probe none">Bonjour&#8239;!</span></div>
		<div lang="fr"><span id="quotes" class="probe french">«Mot»</span></div>
		<div><span id="quotes_ref" class="probe none">«&nbsp;Mot&nbsp;»</span></div>
		<div lang="en"><span id="english" class="probe french">Bonjour!</span></div>
		<div><span id="english_ref" class="probe none">Bonjour!</span></div>
		<div id="resize"><a id="first" href="#link">水</a><span id="second">A</span></div>
		<div id="fr_wrap" lang="fr" class="french replace"><span id="fr_word">Bonjour</span> <span id="fr_mark">!</span></div>
		<div><span id="edge_left">水</span><span id="edge_right">A</span></div>
		<div><span id="invalid_autospace" class="probe alpha"
			style="text-autospace: ideograph-alpha ideograph-alpha">水A水1</span></div>
	"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(480, 1200)) == OK, "Lay out CSS text-autospace probes")
	var ic: float = _rect(layout, "ic").size.x
	var step: float = ic / 8.0
	var unspaced: float = _rect(layout, "none").size.x
	var expected: Dictionary[String, float] = {
		"normal": unspaced + 3.0 * step,
		"alpha": unspaced + 2.0 * step,
		"numeric": unspaced + step,
		"invalid_autospace": unspaced + 2.0 * step,
		"fullwidth": _rect(layout, "fullwidth_ref").size.x,
		"letters": _rect(layout, "letters_ref").size.x + 3.0 * step,
		"replace": _rect(layout, "replace_ref").size.x,
		"words": _rect(layout, "words_ref").size.x + 7.0,
		"insert": _rect(layout, "insert_ref").size.x,
		"nbsp": _rect(layout, "nbsp_ref").size.x,
		"french": _rect(layout, "fr_ref").size.x,
		"fr_replace": _rect(layout, "fr_ref").size.x,
		"quotes": _rect(layout, "quotes_ref").size.x,
		"english": _rect(layout, "english_ref").size.x,
	}
	for id: String in expected:
		var width: float = _rect(layout, id).size.x
		_check(absf(width - expected[id]) < 0.15, "Autospace %s width %s vs %s" % [id, width, expected[id]])
	var plain: String = layout.get_plain_text()
	_check("水 W" in plain, "A replaced sequence of collapsible spaces copies as one ordinary space")
	var authored_space: int = plain.find("水 R") + 1
	var selected: Array[Rect2] = layout.get_selection_rects(authored_space, authored_space + 1)
	_check(authored_space > 0 and selected.size() == 1 and absf(selected[0].size.x - step) < 0.15,
		"An authored space stays selectable at its replacement width")
	_check("Bonjour\u202f!" in plain and "«Mot»" in plain and "Bonjour !" in plain and "Bonjour!" in plain,
		"Autospace preserves authored spaces and keeps inserted spacing out of copied text")
	var first: Rect2 = _rect(layout, "first")
	var second: Rect2 = _rect(layout, "second")
	var gap_point: Vector2 = Vector2(first.end.x + step / 2.0, first.get_center().y)
	_check(absf(second.position.x - first.end.x - step) < 0.15 and layout.get_link_at(gap_point).is_empty()
		and layout.get_link_at(first.get_center()) == "#link", "The shared boundary gap lies outside the link")
	_check(_rect(layout, "edge_right").position.x <= _rect(layout, "edge_left").end.x + 0.15,
		"An intervening inline edge suppresses automatic spacing")
	var resize: HTMLElement = layout.get_element_by_id("resize")
	var fr_wrap: HTMLElement = layout.get_element_by_id("fr_wrap")
	var cut: float = first.size.x + second.size.x + step / 2.0
	var word_width: float = _rect(layout, "fr_word").size.x
	_check(resize.set_attribute("style", "width: %spx" % cut) == OK and
		fr_wrap.set_attribute("style", "width: %spx" % word_width) == OK and
		layout.layout(Vector2i(480, 1200)) == OK, "Reflow a line that no longer fits the automatic gap")
	first = _rect(layout, "first")
	second = _rect(layout, "second")
	_check(second.position.y > first.position.y and absf(second.position.x - first.position.x) < 0.15,
		"Autospace contributes to wrapping and vanishes at the new line start")
	_check(is_equal_approx(_rect(layout, "fr_word").position.y, _rect(layout, "fr_mark").position.y),
		"French punctuation stays with its preceding word across a replaced space")
	_check(_has_diagnostic(layout, "text-autospace", "ideograph-alpha ideograph-alpha", "inline_style"),
		"Duplicate autospace keywords use declaration diagnostics")


func _check_punctuation_spacing() -> void:
	const FONT_UID: String = "{EAST_ASIAN_FONT_UID}"
	if FONT_UID.is_empty():
		return
	var font: FontFile = load(FONT_UID)
	_check(font != null, "Load font for CSS punctuation spacing")
	if font == null:
		return
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", {"res://fonts/east_asian.ttc": font,})
	document.text = """<!doctype html><style>
		@font-face { font-family: Eastern; src: url(fonts/east_asian.ttc); }
		body { margin: 0; font: 48px / 56px Eastern; }
		.spaced { text-spacing-trim: space-all; }
		#trim_start { text-spacing-trim: trim-start; }
		#trim_end { text-spacing-trim: trim-end; }
		#first_line { text-spacing-trim: space-first; width: 110px; }
		#tight, #allowed, #spaced_end { width: 75px; }
		#spaced_end { text-spacing-trim: space-start space-adjacent space-end; }
		#shorthand { --spacing: trim-all no-autospace; text-spacing: var(--spacing); }
		#reset { text-spacing: none; }
		#override { font-feature-settings: "halt" off; text-spacing-trim: trim-all; }
		#invalid { text-spacing-trim: trim-start space-start; }
		.transformed { display: inline-block; text-transform: full-width; }
		#transformed { text-spacing-trim: trim-all; }
		#intrinsic_grid { display: grid; grid-template-columns: min-content; width: 240px; }
		#support { width: 16px; height: 1px; }
		@supports (text-spacing: space-end trim-start no-autospace) and
			(not (text-spacing: trim-all space-end)) { #support { width: 49px; } }
	</style>
	<div class="spaced"><span id="full_open">「</span>水<span id="full_end">。</span></div>
	<div id="trim_start"><span id="start_mark">「</span>水</div>
	<div id="trim_end">水<span id="end_mark">。</span></div>
	<div><span id="first_open">「</span><span id="second_open">『</span>水</div>
	<div><span id="first_close">」</span><span id="second_close">』</span>水</div>
	<div><span id="space_close">」</span>　<span id="space_open">「</span>水</div>
	<div id="first_line"><span id="first_mark">「</span>永水<span id="soft_mark">「</span>山<br><span id="forced_mark">「</span>山</div>
	<div id="allowed">水<span id="allowed_mark">。</span></div>
	<div id="spaced_end">水<span id="spaced_end_mark">。</span></div>
	<div id="shorthand"><span id="always_open">「</span>A<span id="always_end">。</span>水</div>
	<div id="reset"><span id="reset_open">「</span><span id="reset_next">『</span>水</div>
	<div id="override"><span id="override_glyph">「</span>水</div>
	<div id="invalid"><span id="invalid_glyph">「</span>水</div>
		<div><span id="transformed" class="transformed">(A)</span><span id="transformed_ref" class="transformed spaced">(A)</span></div>
		<div id="intrinsic_grid"><div id="intrinsic">水。</div></div>
	<div id="support"></div>
	"""
	var layout: HTMLLayout = document.create_layout()
	layout.css_diagnostics_enabled = true
	_check(layout.layout(Vector2i(620, 900)) == OK, "Lay out CJK punctuation spacing")
	var full_open: float = _rect(layout, "full_open").size.x
	var full_end: float = _rect(layout, "full_end").size.x
	var trim_open: float = _rect(layout, "start_mark").size.x
	var trim_end: float = _rect(layout, "end_mark").size.x
	_check(trim_open < full_open - 10.0 and trim_end < full_end - 10.0,
		"Trimmed punctuation reduces the advance at either line edge: %s/%s, %s/%s" % [trim_open, full_open, trim_end, full_end])
	var expected: Dictionary[String, float] = {
		"first_open": full_open,
		"second_open": trim_open,
		"first_close": trim_end,
		"second_close": full_end,
		"space_close": trim_end,
		"space_open": trim_open,
		"first_mark": full_open,
		"soft_mark": trim_open,
		"forced_mark": full_open,
		"allowed_mark": trim_end,
		"spaced_end_mark": full_end,
		"always_open": trim_open,
		"always_end": trim_end,
		"reset_open": full_open,
		"reset_next": full_open,
		"override_glyph": full_open,
		"invalid_glyph": full_open,
		"support": 49.0,
	}
	for id: String in expected:
		var width: float = _rect(layout, id).size.x
		_check(absf(width - expected[id]) < 0.15, "Punctuation %s width %s vs %s" % [id, width, expected[id]])
	_check(_rect(layout, "soft_mark").position.y > _rect(layout, "first_mark").position.y,
		"Space-first trims opening punctuation on a soft-wrapped line")
	_check(_rect(layout, "transformed").size.x + 30.0 < _rect(layout, "transformed_ref").size.x,
		"Fullwidth punctuation produced by a text transform uses the same trimming")
	var intrinsic_width: float = _rect(layout, "intrinsic").size.x
	_check(absf(intrinsic_width - full_open - trim_end) < 0.15,
		"Grid min-content sizing uses the smallest fitting terminal punctuation advance: %s" % intrinsic_width)
	_check(_has_diagnostic(layout, "text-spacing-trim", "trim-start space-start", "style_element"),
		"Conflicting punctuation modes report a declaration diagnostic")
	var plain: String = layout.get_plain_text()
	var offset: int = plain.find("「A。水")
	var picked: Array[Rect2] = layout.get_selection_rects(offset, offset + 1)
	_check(offset >= 0 and picked.size() == 1 and absf(picked[0].size.x - trim_open) < 0.15,
		"Trimmed punctuation stays selectable under its original character")


func _check_text_spacing_paint(assets: Dictionary) -> void:
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", assets)
	document.text = """<!doctype html><style>
		@font-face { font-family: Mono; src: url(fonts/cour.ttf); }
		body { margin: 0; font: 28px / 32px Mono; background: black; color: cyan; }
		#spaced { height: 65px; letter-spacing: 8.25px; text-shadow: 0 28px magenta; }
	</style><div id="spaced">MMMM</div><div>MMMM</div>"""
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(160, 110)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.document = document
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var spaced: Rect2i = _color_bounds(image, Rect2i(0, 0, 150, 31), Color.CYAN)
	var base: Rect2i = _color_bounds(image, Rect2i(0, 65, 150, 40), Color.CYAN)
	var shadow: Rect2i = _color_bounds(image, Rect2i(0, 31, 150, 33), Color.MAGENTA)
	_check(spaced.has_area() and base.has_area() and absf(float(spaced.size.x - base.size.x) - 24.75) <= 2.0,
		"Painted glyphs use the fractional advances: %s vs %s" % [spaced, base])
	_check(shadow.has_area() and absi(spaced.size.x - shadow.size.x) <= 2,
		"Text-shadow uses the spaced glyph positions: %s vs %s" % [spaced, shadow])
	viewport.queue_free()


func _check_text_transform() -> void:
	var html: String = """<!doctype html><style>
		@font-face { font-family: MathProbe; src: url(fonts/cambria.ttc#Cambria%4Dath); }
		body { margin: 0; background: black; color: white; font: 24px / 32px MathProbe, sans-serif; }
		.row { position: relative; height: 32px; }
		.reference { position: absolute; top: 0; left: 240px; }
		.combined { --transform: full-size-kana full-width uppercase; text-transform: var(--transform); }
		.width { text-transform: full-width; }
		.kana { text-transform: full-size-kana; }
		.math { text-transform: math-auto; }
		.preserved { white-space: pre; }
		.invalid { text-transform: uppercase; text-transform: lowercase uppercase; text-transform: none full-width; }
		#duplicates { text-transform: var(--duplicate); --duplicate: full-width full-width; }
		#generated::before { content: 'α'; text-transform: math-auto; }
		#generated_ref::before { content: '𝛼'; }
	</style>
	<div class="row"><span id="combined" class="combined">aBｧㇰ𛅑</span><span class="reference">ＡＢアクゑ</span></div>
	<div class="row"><span class="kana">ぇｨㇿ𛅧</span><span class="reference">えｲロン</span></div>
	<div class="row"><span class="width">£ﾡ￩ｶﾞ</span><span class="reference">￡ㄱ←ガ</span></div>
	<div class="row"><span class="width preserved">A B</span><span class="reference preserved">Ａ　Ｂ</span></div>
	<div class="row"><span class="width">A  B</span><span class="reference">Ａ Ｂ</span></div>
	<div class="row"><span class="math">h</span><span class="reference">ℎ</span></div>
	<div class="row"><span id="math" class="math">ϴ</span><span class="reference">𝛳</span></div>
	<div class="row"><span id="math_multi" class="math">x y</span><span id="math_ref" class="reference">x y</span></div>
	<div class="row"><span class="math">∞</span><span class="reference">∞</span></div>
	<div class="row"><span id="generated"></span><span id="generated_ref" class="reference"></span></div>
	<div class="row"><span class="invalid">test</span><span class="reference">TEST</span></div>
	<div class="row"><span id="duplicates" class="width">ab</span><span class="reference">ab</span></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(html) == OK and layout.layout(Vector2i(480, 384)) == OK, "Lay out transformed text")
	var plain: String = layout.get_plain_text()
	_check(plain.begins_with("aBｧㇰ𛅑") and plain.count("ϴ") == 1 and plain.count("α") == 1,
		"Selection text preserves the original characters of styled and generated runs")
	var offset: int = plain.find("aBｧㇰ𛅑")
	var highlights: Array[Rect2] = layout.get_selection_rects(offset, offset + 5)
	var combined: Rect2 = _rect(layout, "combined")
	var bounds: Rect2
	for highlight: Rect2 in highlights:
		bounds = bounds.merge(highlight) if bounds.has_area() else highlight
	_check(bounds.has_area() and absf(bounds.size.x - combined.size.x) <= 0.1,
		"Selection covers the measured width across transformed text fragments")
	_check(layout.hit_test_text(Vector2(combined.end.x - 0.01, combined.get_center().y)) == offset + 5,
		"The transformed run maps hits back to source offsets")
	var math_offset: int = plain.find("ϴ")
	var math_box: Rect2 = _rect(layout, "math")
	_check(layout.get_selection_rects(math_offset, math_offset + 1).size() == 1 and
		layout.hit_test_text(Vector2(math_box.end.x - 0.01, math_box.get_center().y)) == math_offset + 1,
		"A supplementary mathematical glyph keeps one selectable source position")
	_check(_has_diagnostic(layout, "text-transform", "lowercase uppercase", "style_element") and
		_has_diagnostic(layout, "text-transform", "none full-width", "style_element") and
		_has_diagnostic(layout, "text-transform", "var(--duplicate)", "style_element"),
		"Conflicting, exclusive and duplicate values follow the declaration diagnostics")
	var math_multi: HTMLElement = layout.get_element_by_id("math_multi")
	var math_reference: HTMLElement = layout.get_element_by_id("math_ref")
	_check(math_multi.set_text_content("h") == OK and math_reference.set_text_content("ℎ") == OK and
		layout.layout(Vector2i(480, 384)) == OK and
		is_equal_approx(math_multi.get_bounding_rect().size.x, math_reference.get_bounding_rect().size.x),
		"A text node replaced through the DOM uses its new source length for math-auto")
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(480, 384)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	for row: int in range(12):
		var actual: Image = image.get_region(Rect2i(0, row * 32, 220, 32))
		var reference: Image = image.get_region(Rect2i(240, row * 32, 220, 32))
		_check(actual.get_data() == reference.get_data() and _font_ink(actual, Rect2(0, 0, 220, 32)) > 0.0,
			"Text transform row %d paints the reference glyphs" % row)
	label.select_all()
	_check(label.get_selected_text() == plain, "HTMLLabel copies the original text instead of the CSS-transformed glyphs")
	viewport.queue_free()


func _check_line_alignment() -> void:
	var html: String = """<!doctype html><style>
		body { margin: 0; background: black; color: white; font: 20px / 24px "Courier New", monospace; }
		div { width: 144px; }
		#wrapped { text-align: justify; text-align-last: center; text-justify: inter-word; }
		#justify_all { text-align: justify-all; text-justify: inter-word; }
		#none { text-align: justify-all; text-justify: none; }
		#reset { text-align-last: right; --alignment: justify; text-align: var(--alignment); }
		#parent_align { text-align-all: start; text-align-last: end; }
		#match { text-align: match-parent; }
		#full_width, #split { text-align: justify-all; }
		#full_width { text-justify: inter-character; }
		#split { text-justify: none; }
		@supports (text-justify: no-compress distribute) {
			#split { text-justify: var(--justify); --justify: no-compress distribute; }
		}
		#mixed { text-align: justify-all; text-justify: inter-character; }
		#exempt { text-justify: none; }
		#no_opportunity { text-align: justify; text-align-last: end; text-justify: inter-word; }
		#auto_cjk, #ruby, #cursive { text-align: justify-all; }
		#ruby { text-justify: ruby; }
		#cursive { text-justify: inter-character; }
		#resize { width: min(100%, 144px); text-align: justify-all; text-justify: inter-character; }
	</style>
	<div id="wrapped"><span id="wrapped_start">AA</span> <span id="wrapped_end">BB</span> <span id="before_break">CCCCCCCCC</span><br><span id="final">DD</span></div>
	<div id="justify_all"> <span id="words_start">AA</span> <span id="words_end">BB </span> </div>
	<div id="none"><span id="none_start">AA</span> <span id="none_end">BB</span></div>
	<div id="reset"><span id="reset_text">AA</span></div>
	<div id="parent_align"><div id="match"><span id="match_text">AA</span></div></div>
	<div id="full_width"><span id="characters">WXYZ</span></div>
	<div id="split"><span>WX</span><span id="split_end">YZ</span></div>
	<div id="mixed"><span id="exempt">WX</span><span id="mixed_end">YZ</span></div>
	<div id="no_opportunity"><span id="fallback">WWWWWWWW</span> <span>XXXXXXXX</span></div>
	<div id="auto_cjk">一二三</div>
	<div id="ruby"><span id="ruby_text">A B</span></div>
	<div id="cursive"><span id="cursive_text">سلام</span></div>
	<div id="resize"><span id="resize_text">TEXT</span></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(html) == OK and layout.layout(Vector2i(180, 420)) == OK, "Lay out default and last line alignment")
	var first: Rect2 = _rect(layout, "wrapped_start")
	var last: Rect2 = _rect(layout, "wrapped_end")
	_check(is_zero_approx(first.position.x) and absf(last.end.x - 144.0) <= 0.1 and absf(first.size.x - last.size.x) <= 0.1,
		"Inter-word justification expands only the gap on the first line")
	for id: String in ["before_break", "final", "ruby_text", "cursive_text"]:
		var box: Rect2 = _rect(layout, id)
		_check(box.has_area() and absf(box.get_center().x - 72.0) <= 0.1, "%s is centered on its own line" % id)
	var words: Rect2 = _rect(layout, "words_end")
	_check(is_zero_approx(_rect(layout, "words_start").position.x) and absf(words.end.x - 144.0) <= 0.1,
		"Justify-all fills the final line after trimming trailing spaces inside a span")
	var no_justify_start: Rect2 = _rect(layout, "none_start")
	var no_justify_end: Rect2 = _rect(layout, "none_end")
	_check(absf(no_justify_start.position.x + no_justify_end.end.x - 144.0) <= 0.1 and no_justify_start.position.x > 0.0,
		"Text-justify none follows the unexpandable-line alignment rule")
	_check(is_zero_approx(_rect(layout, "reset_text").position.x), "The deferred text-align shorthand resets text-align-last")
	_check(absf(_rect(layout, "match_text").end.x - 144.0) <= 0.1, "Match-parent inherits the final line's end alignment")
	_check(absf(_rect(layout, "fallback").end.x - 144.0) <= 0.1, "An unexpandable wrapped line uses text-align-last")
	var characters: Rect2 = _rect(layout, "characters")
	var split_end: Rect2 = _rect(layout, "split_end")
	_check(absf(characters.size.x - 144.0) <= 0.1 and absf(split_end.end.x - 144.0) <= 0.1,
		"Inter-character spacing expands both within a shaped run and across inline boundaries")
	_check(absf(_rect(layout, "exempt").size.x - first.size.x) <= 0.1 and absf(_rect(layout, "mixed_end").end.x - 144.0) <= 0.1,
		"A nested text-justify override keeps that run at its natural width")
	var plain: String = layout.get_plain_text()
	var char_offset: int = plain.find("WXYZ")
	var first_half: Array[Rect2] = layout.get_selection_rects(char_offset, char_offset + 2)
	_check(first_half.size() == 1 and absf(first_half[0].end.x - 72.0) <= 0.1 and
		layout.hit_test_text(Vector2(first_half[0].end.x, first_half[0].get_center().y)) == char_offset + 2,
		"Hit testing and selection use justified cluster advances")
	var cjk_offset: int = plain.find("一二三")
	var cjk: Array[Rect2] = layout.get_selection_rects(cjk_offset, cjk_offset + 3)
	_check(cjk.size() == 3 and absf(cjk[2].end.x - 144.0) <= 0.1, "Auto justification distributes space between Han glyphs")
	_check(not _has_diagnostic(layout, "text-justify", "var(--justify)", "style_element"), "Text-justify accepts variables and modifiers")
	var resized: HTMLElement = layout.get_element_by_id("resize_text")
	_check(is_equal_approx(resized.get_bounding_rect().size.x, 144.0) and
		layout.layout(Vector2i(101, 420)) == OK and is_equal_approx(resized.get_bounding_rect().size.x, 101.0) and
		layout.layout(Vector2i(180, 420)) == OK and is_equal_approx(resized.get_bounding_rect().size.x, 144.0),
		"Reflow computes fresh justification for the same text and element handle")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(180, 420)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var whole_pixels: Image = image.get_region(Rect2i(0, roundi(_rect(layout, "full_width").position.y), 144, 24))
	var split_pixels: Image = image.get_region(Rect2i(0, roundi(_rect(layout, "split").position.y), 144, 24))
	_check(whole_pixels.get_data() == split_pixels.get_data() and _font_ink(whole_pixels, Rect2(0, 0, 144, 24)) > 0.0,
		"Painted inter-character spacing is invariant across plain inline tags")
	viewport.queue_free()


func _check_text_indent_and_tabs() -> void:
	var html: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px "Courier New", monospace; background: black; color: white; }
		.paragraph { width: 120px; }
		#each { --indent: each-line calc(10% + 3px); text-indent: var(--indent); }
		#hanging { text-indent: hanging 15px; }
		#both { text-indent: hanging each-line 15px; }
		#preline { white-space: pre-line; text-indent: 15px each-line; }
		#mixed_indent { text-indent: 15px; }
		#border_box { box-sizing: border-box; width: 140px; padding: 0 10px; text-indent: 10%; }
		#negative { margin-left: 20px; text-indent: -12px; }
		#intrinsic { display: inline-block; text-indent: calc(10% + 2px); }
		#duplicate { text-indent: 7px; text-indent: hanging 5px hanging; }
		#viewport_indent { width: min(100%, 160px); text-indent: 25%; }
		#float_indent { display: flow-root; text-indent: 15px each-line; }
		#late_float { float: right; width: 30px; height: 15px; }
		.tabs { white-space: pre; }
		.big { font-size: 40px; }
		#fraction { tab-size: calc(2 + .5); }
		#fixed { tab-size: 2em; }
		#fixed > span { font-size: 10px; }
		#tracked { tab-size: 4; letter-spacing: 2px; word-spacing: 3px; }
		#tight { tab-size: calc(1ch + 1px); }
		#zero { tab-size: 0; }
		#ordinary { tab-size: 0; }
		#bad_tab { tab-size: 24px; tab-size: -1; tab-size: 50%; }
		#wrap_tab { width: 62px; white-space: pre-wrap; tab-size: 30px; }
		#soft_tab, #forced_tab { width: 62px; white-space: pre-wrap; tab-size: 60px; text-align: right; }
		#float_tab { display: flow-root; tab-size: 24px; }
		#float_box { float: left; width: 32px; height: 24px; }
		#decorated { tab-size: 80px; }
		#tab_link { color: lime; text-decoration: underline; }
	</style>
	<div class="paragraph" id="each"><span id="each_start">AA</span> <span id="each_wrap">BBBBBBBB</span><br><span id="each_forced">CC</span></div>
	<div class="paragraph" id="hanging"><span id="hang_start">AA</span> <span id="hang_wrap">BBBBBBBB</span><br><span id="hang_forced">CC</span></div>
	<div class="paragraph" id="both"><span id="both_start">AA</span> <span id="both_wrap">BBBBBBBB</span><br><span id="both_forced">CC</span></div>
	<div id="preline">AA\n<span id="preline_forced">BB</span></div>
	<div id="mixed_indent"><span id="anon_start">AA</span><div>Block</div><span id="anon_after">BB</span></div>
	<div id="border_box"><span id="content_box">AA</span></div>
	<div id="negative"><span id="negative_text">AA</span></div>
	<div><span id="intrinsic">AAA</span></div>
	<div id="duplicate"><span id="duplicate_text">AA</span></div>
	<div id="viewport_indent"><span id="responsive_indent">AA</span></div>
	<div id="float_indent"><span id="before_float_break">AA</span><br><span id="after_break_and_float">BB</span><div id="late_float"></div></div>
	<div class="tabs"><span id="unit">A</span>\t<span id="default_tab">B</span>\t<span id="second_tab">C</span></div>
	<div class="tabs" id="fraction">AAA\t<span id="fraction_text">B</span></div>
	<div class="tabs" id="fixed"><span>A\t<span id="fixed_text">B</span></span></div>
	<div class="tabs" id="tracked">A<span class="big">\t</span><span id="tracked_text">B</span></div>
	<div class="tabs" id="tight">A\t<span id="tight_text">B</span></div>
	<div class="tabs" id="zero">ZERO\t<span id="zero_text">TAB</span></div>
	<div id="ordinary">A\t<span id="ordinary_text">B</span></div>
	<div class="tabs" id="bad_tab">A\t<span id="bad_text">B</span></div>
	<div id="wrap_tab">AAAA\t<span id="wrap_text">BBBB</span>\t<span id="wrapped_stop">C</span></div>
	<div id="soft_tab"><span id="soft_ending_tab">A</span>\tBBBB</div>
	<div id="forced_tab"><span id="forced_ending_tab">A</span>\t<br>BBBB</div>
	<div class="tabs" id="float_tab"><div id="float_box"></div>A\t<span id="float_text">B</span></div>
	<div class="tabs" id="decorated">A<a id="tab_link" href="#tab">\t</a>B</div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var viewport_size: Vector2i = Vector2i(230, 1024)
	_check(layout.parse(html) == OK and layout.layout(viewport_size) == OK, "Lay out indented lines and tab stops")
	var unit: float = _rect(layout, "unit").size.x
	var positions: Dictionary[String, float] = {
		"each_start": 15.0,
		"each_wrap": 0.0,
		"each_forced": 15.0,
		"hang_start": 0.0,
		"hang_wrap": 15.0,
		"hang_forced": 15.0,
		"both_start": 0.0,
		"both_wrap": 15.0,
		"both_forced": 0.0,
		"preline_forced": 15.0,
		"anon_start": 15.0,
		"anon_after": 0.0,
		"content_box": 22.0,
		"negative_text": 8.0,
		"duplicate_text": 7.0,
		"responsive_indent": 40.0,
		"after_break_and_float": 15.0,
		"default_tab": unit * 8.0,
		"second_tab": unit * 16.0,
		"fraction_text": unit * 5.0,
		"fixed_text": 40.0,
		"tracked_text": (unit + 5.0) * 4.0,
		"tight_text": (unit + 1.0) * 2.0,
		"zero_text": unit * 4.0,
		"ordinary_text": unit * 2.0,
		"bad_text": 24.0,
		"wrap_text": 0.0,
		"wrapped_stop": 0.0,
		"soft_ending_tab": 62.0 - unit,
		"forced_ending_tab": 2.0,
		"float_text": 72.0,
	}
	for id: String in positions:
		var actual: float = _rect(layout, id).position.x
		_check(is_equal_approx(actual, positions[id]), "%s starts at %s instead of %s" % [id, actual, positions[id]])
	_check(absf(_rect(layout, "intrinsic").size.x - unit * 3.0 - 2.0) <= 0.1,
		"Intrinsic width includes the fixed part of the indent and gives percentages a zero basis")
	_check(_rect(layout, "each_wrap").position.y > _rect(layout, "each_start").position.y and
		_rect(layout, "wrapped_stop").position.y > _rect(layout, "wrap_text").position.y,
		"Soft wrapping uses the line's indent and recomputes tabs from the new line position")
	_check(_rect(layout, "after_break_and_float").position.y > _rect(layout, "before_float_break").position.y,
		"A late float keeps the preceding forced break when rebuilding the current line")
	var responsive_indent: HTMLElement = layout.get_element_by_id("responsive_indent")
	_check(layout.layout(Vector2i(101, viewport_size.y)) == OK and
		is_equal_approx(responsive_indent.get_bounding_rect().position.x, 25.25) and
		layout.layout(viewport_size) == OK and is_equal_approx(responsive_indent.get_bounding_rect().position.x, 40.0),
		"Percentage indentation follows the current content width across reflows")
	_check(_has_diagnostic(layout, "text-indent", "hanging 5px hanging", "style_element") and
		_has_diagnostic(layout, "tab-size", "-1", "style_element") and
		_has_diagnostic(layout, "tab-size", "50%", "style_element"),
		"Invalid indent modifiers and tab sizes preserve preceding declarations")
	var plain: String = layout.get_plain_text()
	_check("ZERO\tTAB" in plain and "A B" in plain and plain.ends_with("A\tB"),
		"Copied preserved and zero-width tabs remain single characters")
	var tab_offset: int = plain.rfind("\t")
	var tab_box: Rect2 = _rect(layout, "tab_link")
	var selection: Array[Rect2] = layout.get_selection_rects(tab_offset, tab_offset + 1)
	_check(selection.size() == 1 and absf(selection[0].size.x - tab_box.size.x) <= 0.1 and
		layout.hit_test_text(Vector2(tab_box.end.x - 1.0, tab_box.get_center().y)) == tab_offset + 1,
		"A preserved tab has one selectable interval across its full advance")
	_check(layout.get_link_at(tab_box.get_center()) == "#tab", "Tab advances participate in inline link hit testing")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = viewport_size
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var y: int = roundi(_rect(layout, "decorated").position.y)
	var underline_area: Rect2i = Rect2i(0, y, 95, 24).intersection(Rect2i(Vector2i.ZERO, image.get_size()))
	var underline: Rect2i = _color_bounds(image, underline_area, Color.LIME)
	_check(underline.has_area() and absi(underline.position.x - roundi(unit)) <= 1 and absi(underline.end.x - 80) <= 1,
		"The underline paints the gap occupied by a preserved tab")
	label.select_all()
	_check(label.get_selected_text() == plain, "HTMLLabel copies the original tab characters")
	viewport.queue_free()


func _check_white_space() -> void:
	var html: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px "Courier New", monospace; background: black; color: white; }
		#spaces { width: 3ch; white-space: break-spaces; }
		#spaces a { text-decoration: none; background: lime; }
		#lookback { width: 4ch; white-space: wrap break-spaces; }
		#no_prior { width: 2ch; white-space: break-spaces; }
		#tabs { width: 42px; white-space: break-spaces; tab-size: 30px; }
		#preserve { --space: preserve-spaces nowrap; white-space: var(--space); }
		#preserve_break { white-space: nowrap; white-space-collapse: preserve-breaks; }
		#discard { white-space-collapse: discard; }
		#local_nowrap { width: 6ch; }
		#local_group { text-wrap-mode: nowrap; }
		#local_wrap { width: 4ch; text-wrap-mode: nowrap; }
		#local_wrap > span { white-space: collapse wrap; }
		#inherited { white-space: pre; width: 3ch; }
		#inherited > div { text-wrap-mode: wrap; }
		#shorthand { white-space-collapse: preserve; text-wrap-mode: nowrap; }
		#shorthand > div { white-space: normal; }
		#unicode_spaces { width: 25px; white-space-collapse: break-spaces; }
		#forced { white-space: nowrap; }
		#bad { white-space: wrap preserve; white-space: preserve nowrap wrap; text-wrap-mode: balance; }
		.width_probe { display: grid; width: 1px; grid-template-columns: 1fr; }
		#break_minimum { white-space: break-spaces; }
		#nowrap_minimum span { text-wrap-mode: nowrap; }
	</style>
	<div id="spaces"><span id="unit_ws">A</span><a href="#space" id="space_one"> </a><a href="#space" id="space_two"> </a><a href="#space" id="space_three"> </a><a href="#space" id="space_four"> </a><span id="after_spaces">B</span></div>
	<div id="lookback"><span id="before_group">AA</span> <span id="word_group">B</span><span id="held_space"> </span>C</div>
	<div id="no_prior"><span id="before_required">AA</span><span id="required"> </span></div>
	<div id="tabs"><span id="first_tab_word">A</span><span id="first_ws_tab">\t</span><span id="second_ws_tab">\t</span><span id="after_tabs">B</span></div>
	<div id="preserve">START\t\n  <span id="preserve_end">END</span></div>
	<div id="preserve_break">BREAK  \t \n  <span id="preserved_next">NEXT</span></div>
	<div id="discard">REMOVE \t\n <span id="after_discard">SPACES</span></div>
	<div id="local_nowrap"><span id="outside_nowrap">AA</span> <span id="local_group"><span id="first_group_word">BB</span> <span id="last_group_word">CC</span></span></div>
	<div id="local_wrap"><span><span id="first_local_word">AA</span> <span id="next_local_word">BB</span></span></div>
	<div id="inherited"><div>AA  <span id="inherited_end">BB</span></div></div>
	<div id="shorthand"><div>AA  <span id="shorthand_end">BB</span></div></div>
	<div id="unicode_spaces"><span id="before_en">A</span><span id="first_en">&ensp;</span><span id="second_en">&ensp;</span>B</div>
	<div id="forced"><span id="forced_before">AA</span>&#x2028;<span id="forced_after">BB</span></div>
	<div id="bad">A  <span id="bad_ws_end">B</span></div>
	<div class="width_probe"><div id="break_minimum">WW  Q</div></div>
	<div class="width_probe"><div id="nowrap_minimum"><span>WX YZ</span> Q</div></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(html) == OK and layout.layout(Vector2i(220, 600)) == OK, "Lay out CSS whitespace modes")
	var unit: float = _rect(layout, "unit_ws").size.x
	var first_line: float = _rect(layout, "space_one").position.y
	var second_line: float = _rect(layout, "space_three").position.y
	_check(second_line > first_line and is_equal_approx(_rect(layout, "space_two").position.y, first_line) and
		is_equal_approx(_rect(layout, "space_four").position.y, second_line) and
		is_equal_approx(_rect(layout, "after_spaces").position.x, unit * 2.0),
		"Break-spaces wraps between consecutive spaces while preserving their advances")
	_check(_rect(layout, "word_group").position.y > _rect(layout, "before_group").position.y and
		is_equal_approx(_rect(layout, "held_space").position.y, _rect(layout, "word_group").position.y) and
		is_equal_approx(_rect(layout, "word_group").position.x, 0.0),
		"A first preserved space stays with its word by using the preceding wrap opportunity")
	_check(is_equal_approx(_rect(layout, "required").position.y, _rect(layout, "before_required").position.y) and
		is_equal_approx(_rect(layout, "required").end.x, unit * 3.0),
		"The first space can overflow with its word when no prior wrap opportunity exists")
	_check(_rect(layout, "second_ws_tab").position.y > _rect(layout, "first_ws_tab").position.y and
		is_equal_approx(_rect(layout, "second_ws_tab").size.x, 30.0) and
		is_equal_approx(_rect(layout, "after_tabs").position.x, 30.0),
		"A preserved tab wraps after the previous tab and recalculates its advance on the new line")
	_check(is_equal_approx(_rect(layout, "preserve_end").position.x, unit * 9.0) and
		is_equal_approx(_rect(layout, "preserved_next").position.x, 0.0),
		"Preserve-spaces converts tabs and newlines while preserve-breaks retains the newline")
	_check(is_equal_approx(_rect(layout, "after_discard").position.x, unit * 6.0) and
		layout.get_element_by_id("discard").get_text_content() == "REMOVE \t\n SPACES",
		"Discard removes CSS whitespace during layout and retains DOM text")
	_check(_rect(layout, "first_group_word").position.y > _rect(layout, "outside_nowrap").position.y and
		is_equal_approx(_rect(layout, "first_group_word").position.y, _rect(layout, "last_group_word").position.y) and
		is_equal_approx(_rect(layout, "first_group_word").position.x, 0.0),
		"An inherited nowrap span moves to a previous allowed break and stays on one line")
	_check(_rect(layout, "next_local_word").position.y > _rect(layout, "first_local_word").position.y and
		is_equal_approx(_rect(layout, "inherited_end").position.x, 0.0) and
		is_equal_approx(_rect(layout, "shorthand_end").position.x, unit * 3.0),
		"A local wrap mode and the white-space shorthand override inherited wrapping and collapsing independently")
	_check(_rect(layout, "second_en").position.y > _rect(layout, "first_en").position.y and
		_rect(layout, "forced_after").position.y > _rect(layout, "forced_before").position.y,
		"Unicode separators can wrap in break-spaces and mandatory Unicode breaks override nowrap")
	_check(is_equal_approx(_rect(layout, "bad_ws_end").position.x, unit * 3.0) and
		_has_diagnostic(layout, "white-space", "preserve nowrap wrap", "style_element") and
		_has_diagnostic(layout, "text-wrap-mode", "balance", "style_element"),
		"Invalid whitespace values retain the previous valid declaration")
	_check(is_equal_approx(_rect(layout, "break_minimum").size.x, unit * 3.0) and
		is_equal_approx(_rect(layout, "nowrap_minimum").size.x, unit * 5.0),
		"Grid min-content sizing includes the word's first preserved space and all text in a nowrap span")
	var plain: String = layout.get_plain_text()
	_check(plain.begins_with("A    B") and "START    END" in plain and "BREAK\nNEXT" in plain and "REMOVESPACES" in plain,
		"Copy keeps preserved spacing and omits discarded whitespace without inserting newlines at soft wraps")
	var second_space: Rect2 = _rect(layout, "space_two")
	var selected_space: Array[Rect2] = layout.get_selection_rects(2, 3)
	_check(selected_space.size() == 1 and absf(selected_space[0].size.x - unit) <= 0.1 and
		layout.get_link_at(second_space.get_center()) == "#space", "Trailing preserved spaces are selectable and clickable")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(220, 300)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, second_space.get_center(), Color.LIME, "A trailing break-spaces box paints at its measured position")
	_check_pixel(image, _rect(layout, "space_three").get_center(), Color.LIME, "A wrapped space paints at its new line position")
	viewport.queue_free()


func _check_white_space_trim() -> void:
	var html: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px "Courier New", monospace; background: black; color: white; }
		.pre { white-space: pre; }
		.trim_inner { white-space-trim: discard-inner; }
		#outer_trim { white-space-trim: discard-after discard-before; }
		#outer_trim, #inline_trim { background: lime; color: black; text-decoration: none; }
		#inline_trim { --trim: preserve discard-inner nowrap; white-space: var(--trim); }
		#reset { white-space-trim: discard-inner; white-space: pre; }
		#explicit_break { white-space: preserve discard-inner; }
		#joined { width: 2ch; }
		#joined_end { white-space-trim: discard-before; }
		#preserved_edges { white-space-trim: discard-before discard-after; }
		#invalid { white-space-trim: discard-inner; white-space-trim: none discard-before; }
		#invalid { white-space: preserve discard-inner discard-inner; }
		#generated_term::before { content: ', '; white-space-trim: discard-before; }
		#outside_breaks { white-space: pre-line; }
		#after_source_break { white-space-trim: discard-before; }
		@media (max-width: 180px) { #media { white-space-trim: discard-inner; } }
	</style>
	<div id="external">LEFT<span> \t</span><a href="#trim" id="outer_trim">M</a><span><i> </i></span><span id="external_end">RIGHT</span></div>
	<div id="nested">A<a href="#nested" id="inline_trim"><b>  \n<i id="trimmed_word">X</i>\t</b>  </a>B</div>
	<div id="block_trim" class="pre trim_inner">\t\n \n  <span id="block_start">P</span> \n  <span id="block_end">Q</span>  \n\t \n</div>
	<div id="single_line" class="pre trim_inner">  <span id="single_word">W</span>  </div>
	<div id="non_inherited" class="pre trim_inner">A<span>  <b id="ordinary_child">Y</b>  </span>B</div>
	<div>A<span id="reset">  <b id="reset_word">Z</b>  </span>B</div>
	<div id="explicit_before">START<span id="explicit_break"> \n<br><b id="after_explicit">LINE</b>\n \t</span></div>
	<div id="joined"><span id="joined_start">AA</span> <span id="joined_end">BB</span></div>
	<div id="preserved_line" class="pre">A <span id="preserved_edges">B</span> <b id="preserved_end">C</b></div>
	<div class="pre">A<span id="invalid">  <b id="invalid_word">V</b>  </span>B</div>
	<div class="pre">A<span id="media">  <b id="media_word">T</b>  </span>B</div>
	<div>FIRST <span id="generated_term">SECOND</span></div>
	<div id="outside_breaks">PROTECTED \n<span id="after_source_break">NEXT</span></div>
	<div class="pre trim_inner"> \n</div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var viewport_size: Vector2i = Vector2i(235, 500)
	_check(layout.parse(html) == OK and layout.layout(viewport_size) == OK, "Lay out the whitespace trimming modes")
	var unit: float = _rect(layout, "trimmed_word").size.x
	var external: Rect2 = _rect(layout, "outer_trim")
	var inner: Rect2 = _rect(layout, "inline_trim")
	_check(external.has_area() and is_equal_approx(external.position.x, unit * 4.0) and
		is_equal_approx(_rect(layout, "external_end").position.x, unit * 5.0),
		"Both exterior edges trim collapsible spaces across nested inline boundaries")
	_check(inner.has_area() and is_equal_approx(inner.position.x, unit) and is_equal_approx(inner.size.x, unit),
		"Inline discard-inner trims nested preserved spaces, tabs and source line breaks")
	var block: Rect2 = _rect(layout, "block_trim")
	var first: Rect2 = _rect(layout, "block_start")
	var last: Rect2 = _rect(layout, "block_end")
	_check(is_equal_approx(first.position.x, unit * 2.0) and is_equal_approx(last.position.x, first.position.x) and
		is_equal_approx(last.position.y - first.position.y, 24.0) and is_equal_approx(block.size.y, 48.0),
		"Block trimming removes whole boundary blank lines and preserves the first line's indentation")
	_check(is_equal_approx(_rect(layout, "single_word").position.x, unit * 2.0),
		"A single-line preformatted block keeps spaces when no source line break is present")
	_check(is_equal_approx(_rect(layout, "ordinary_child").position.x, unit * 3.0) and
		is_equal_approx(_rect(layout, "reset_word").position.x, unit * 3.0),
		"Trim is non-inherited and an omitted shorthand value resets it")
	_check(_rect(layout, "after_explicit").position.y > _rect(layout, "explicit_before").position.y and
		is_equal_approx(_rect(layout, "preserved_end").position.x, unit * 4.0),
		"Exterior trimming retains preserved spaces and interior trimming retains explicit break elements")
	_check(is_equal_approx(_rect(layout, "joined_end").position.y, _rect(layout, "joined_start").position.y) and
		is_equal_approx(_rect(layout, "joined_end").position.x, unit * 2.0),
		"Trimming the only space inside an ASCII word removes its soft wrap opportunity")
	_check(is_equal_approx(_rect(layout, "invalid_word").position.x, unit) and
		_has_diagnostic(layout, "white-space-trim", "none discard-before", "style_element") and
		_has_diagnostic(layout, "white-space", "preserve discard-inner discard-inner", "style_element"),
		"Invalid trim combinations preserve valid declarations")
	var plain: String = layout.get_plain_text()
	_check(plain.begins_with("LEFTMRIGHT\nAXB\n  P \n  Q  ") and
		"START\nLINE" in plain and "PROTECTED\nNEXT" in plain,
		"Copied text includes the displayed spaces and just the surviving breaks")
	_check("FIRST, SECOND" in plain, "Trim on a generated inline box reaches adjacent collapsible whitespace")
	var trimmed_element: HTMLElement = layout.get_element_by_id("inline_trim")
	_check(trimmed_element.get_text_content() == "  \nX\t  ", "Visual trimming leaves original DOM text intact")
	var selected: Array[Rect2] = layout.get_selection_rects(4, 5)
	_check(selected.size() == 1 and is_equal_approx(selected[0].position.x, external.position.x) and
		layout.get_link_at(external.get_center()) == "#trim", "Selection and link geometry follow the trimmed positions")
	_check(layout.layout(Vector2i(170, viewport_size.y)) == OK and
		is_equal_approx(_rect(layout, "media_word").position.x, unit) and
		layout.layout(viewport_size) == OK and is_equal_approx(_rect(layout, "media_word").position.x, unit * 3.0),
		"Media restyles trim and restore whitespace without replacing the DOM")
	_check(trimmed_element.set_attribute("style", "white-space-trim:none") == OK and
		layout.layout(viewport_size) == OK and trimmed_element.is_valid() and "  \nX\t  " in layout.get_plain_text(),
		"DOM style mutations restore the original whitespace runs")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = viewport_size
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport_size)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, external.position + Vector2(1.0, 1.0), Color.LIME, "Trimmed exterior spacing matches the painted box")
	_check_pixel(image, inner.position + Vector2(1.0, 1.0), Color.LIME, "Trimmed interior spacing matches the painted box")
	label.select_all()
	_check(label.get_selected_text() == plain, "HTMLLabel copies the trimmed layout text")
	viewport.queue_free()


func _check_text_wrap_styles() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var styles: Dictionary[String, String] = {
		"auto": "text-wrap-style: auto",
		"stable": "text-wrap: stable",
		"balance": "text-wrap: wrap balance",
		"pretty": "--strategy: pretty wrap; text-wrap: var(--strategy)",
		"tail": "text-wrap-style: avoid-short-last-line",
		"nowrap": "text-wrap: balance nowrap",
		"reset_mode": "text-wrap-mode: nowrap; text-wrap: balance",
		"reset_style": "text-wrap-style: balance; text-wrap: wrap",
		"invalid": "text-wrap: balance; text-wrap: pretty balance",
	}
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px monospace; line-height: 24px; }
		div { width: 11.4ch; }
		#hard { text-wrap: balance; text-align: right; }
		#clamped_tail { text-wrap-style: avoid-short-last-line; }
		@supports (text-wrap: pretty) { #supported { text-wrap-style: balance; } }
	</style>"""
	for id: String in styles:
		source += (
			'<div id="%s" style="%s"><span id="%s-a">AA</span> BB CC ' +
			'<a id="%s-d" href="#hard">DD</a> <span id="%s-e">EE</span></div>'
		) % [id, styles[id], id, id, id]
	source += """<div id="hard"><span id="hard-a">AA</span> BB CC <span id="hard-d">DD</span> <span id="hard-e">EE</span><br>
		<span id="hard-f">FF</span> GG HH <span id="hard-i">II</span> <span id="hard-j">JJ</span></div>
		<div id="clamped_tail"><span id="tail_a">XX</span> XX XX <span id="tail_d">XX</span>
			<span id="tail_e">XX</span> XX XX <span id="tail_h">XX</span> <span id="tail_i">XX</span></div>
		<div id="supported">AA BB CC <span id="supported-d">DD</span> <span id="supported-e">EE</span></div>"""
	if layout.parse(source) != OK or layout.layout(Vector2i(260, 800)) != OK:
		_check(false, "Prepare text-wrap fixture: " + layout.get_render_error())
		return
	for id: String in ["auto", "stable", "reset_style"]:
		_check(is_equal_approx(_rect(layout, id + "-a").position.y, _rect(layout, id + "-d").position.y) and
			_rect(layout, id + "-e").position.y > _rect(layout, id + "-d").position.y,
			"%s uses the ordinary first-fit break" % id)
	for id: String in ["balance", "pretty", "tail", "reset_mode", "invalid"]:
		var link: Rect2 = _rect(layout, id + "-d")
		_check(link.position.y > _rect(layout, id + "-a").position.y and
			is_equal_approx(link.position.y, _rect(layout, id + "-e").position.y),
			"%s moves the fourth word to the last line" % id)
		_check(layout.get_link_at(link.get_center()) == "#hard", "%s moves the link hit region" % id)
		_check(is_equal_approx(_rect(layout, id).size.y, _rect(layout, "auto").size.y) and
			is_equal_approx(_rect(layout, id).size.x, _rect(layout, "auto").size.x),
			"%s preserves the line count and box width" % id)
	_check(is_equal_approx(_rect(layout, "nowrap-a").position.y, _rect(layout, "nowrap-e").position.y),
		"An explicit nowrap mode disables soft breaks")
	_check(_has_diagnostic(layout, "text-wrap", "pretty balance", "inline_style"),
		"A duplicate text-wrap style is invalid")
	_check(is_equal_approx(_rect(layout, "supported-d").position.y, _rect(layout, "supported-e").position.y),
		"The text-wrap shorthand participates in supports conditions")
	_check(_rect(layout, "hard-d").position.y > _rect(layout, "hard-a").position.y and
		is_equal_approx(_rect(layout, "hard-d").position.y, _rect(layout, "hard-e").position.y) and
		_rect(layout, "hard-i").position.y > _rect(layout, "hard-f").position.y and
		is_equal_approx(_rect(layout, "hard-i").position.y, _rect(layout, "hard-j").position.y),
		"Forced breaks separate independently balanced line groups")
	_check(is_equal_approx(_rect(layout, "hard-j").end.x, _rect(layout, "hard").end.x),
		"Balanced text still aligns to the full box width")
	_check(is_equal_approx(_rect(layout, "tail_a").position.y, _rect(layout, "tail_d").position.y) and
		_rect(layout, "tail_e").position.y > _rect(layout, "tail_d").position.y and
		is_equal_approx(_rect(layout, "tail_h").position.y, _rect(layout, "tail_i").position.y),
		"The last-line strategy leaves the earlier line alone")
	var balance: HTMLElement = layout.get_element_by_id("balance")
	_check(balance.set_attribute("style", "text-wrap: balance; width: 16ch") == OK and
		layout.layout(Vector2i(260, 800)) == OK, "Reflow balanced text after a width change")
	_check(is_equal_approx(_rect(layout, "balance-a").position.y, _rect(layout, "balance-e").position.y),
		"Balancing does not introduce unnecessary breaks in a single line")


func _check_cjk_word_break() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """<!doctype html><style>
		body { margin: 0; font-size: 20px; line-height: 26px; }
		p { margin: 0; width: 2.45em; }
		.keep { word-break: keep-all; }
		#nowrap { text-wrap-mode: nowrap; }
		#hidden wbr { display: none; }
		@supports (word-break: keep-all) { #supported { word-break: keep-all; } }
		.grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 20px; }
	</style>
	<p id="normal">日本語字</p>
	<p id="keep" class="keep">日本語字</p>
	<p id="supported">日本語字</p>
	<p id="korean">한글문자</p>
	<p id="korean_keep" class="keep">한글문자</p>
	<p id="kana">カタカナ</p>
	<p id="kana_keep" class="keep">カタカナ</p>
	<p id="punct_open">日<span id="open_mark">「</span><span id="open_next">本</span>語</p>
	<p id="punct_close">日<span id="close_prev">本</span><span id="close_mark">、</span>語</p>
	<p id="explicit" class="keep"><span id="wbr_first">日本</span><wbr><a id="wbr_last" href="#normal">語字</a></p>
	<p id="zwsp" class="keep"><span id="zwsp_first">日本</span>&#x200B;<span id="zwsp_last">語字</span></p>
	<p id="hidden" class="keep">日本<wbr>語字</p>
	<p id="nowrap" class="keep">日本<wbr>語字</p>
	<p id="leading" class="keep"><wbr>日本語字</p>
	<p id="only_wbr"><wbr></p>
	<p id="latin" class="keep" style="font-family: monospace; width: 2.5ch"><span>AB</span><wbr><span>CD</span></p>
	<p id="invalid" class="keep" style="word-break: bogus">日本語字</p>
	<div class="grid"><span id="normal_track">日本語字</span><i></i></div>
	<div class="grid keep"><span id="keep_track">日本語字</span><i></i></div>
	"""
	if layout.parse(source) != OK or layout.layout(Vector2i(260, 800)) != OK:
		_check(false, "Prepare CJK word-break fixture: " + layout.get_render_error())
		return
	var checks: Dictionary[String, String] = {
		"normal": "keep",
		"korean": "korean_keep",
		"kana": "kana_keep",
	}
	for id: String in checks:
		var wrapped: Rect2 = _rect(layout, id)
		var unwrapped: Rect2 = _rect(layout, checks[id])
		_check(wrapped.size.y > unwrapped.size.y, "%s wraps normally and inherits keep-all" % id)
	_check(is_equal_approx(_rect(layout, "supported").size.y, _rect(layout, "keep").size.y),
		"Keep-all participates in supports conditions")
	_check(is_equal_approx(_rect(layout, "open_mark").position.y, _rect(layout, "open_next").position.y),
		"An opening bracket stays with the following character")
	_check(is_equal_approx(_rect(layout, "close_prev").position.y, _rect(layout, "close_mark").position.y),
		"A closing punctuation mark stays with the preceding character")
	var link: Rect2 = _rect(layout, "wbr_last")
	_check(link.position.y > _rect(layout, "wbr_first").position.y and layout.get_link_at(link.get_center()) == "#normal",
		"A wbr remains breakable with keep-all, and the moved link is hittable")
	_check(_rect(layout, "zwsp_last").position.y > _rect(layout, "zwsp_first").position.y,
		"A zero-width space remains breakable with keep-all")
	_check(_rect(layout, "latin").size.y > _rect(layout, "keep").size.y,
		"A wbr also introduces an opportunity within a Latin word")
	_check("日本\u200b語字" in layout.get_plain_text() and layout.get_element_by_id("zwsp").get_text_content() == "日本\u200b語字",
		"A zero-width space survives copying without a synthetic line feed")
	for id: String in ["hidden", "nowrap", "leading", "invalid"]:
		_check(is_equal_approx(_rect(layout, id).size.y, _rect(layout, "keep").size.y),
			"%s preserves the unbroken run" % id)
	_check(is_zero_approx(_rect(layout, "only_wbr").size.y), "A lone wbr does not create a line")
	_check(_has_diagnostic(layout, "word-break", "bogus", "inline_style"), "Reject invalid word-break values")
	_check(_rect(layout, "keep_track").size.x > _rect(layout, "normal_track").size.x,
		"Keep-all uses the unbroken CJK run for the grid's minimum width")
	var element: HTMLElement = layout.get_element_by_id("keep")
	_check(element.set_attribute("style", "word-break: var(--missing, normal)") == OK and
		layout.layout(Vector2i(260, 800)) == OK, "Update word-break through the DOM")
	_check(is_equal_approx(element.get_bounding_rect().size.y, _rect(layout, "normal").size.y),
		"Normal restores the CJK break opportunities after a DOM update")


func _check_line_break_strictness() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """<!doctype html><style>
		body { margin: 0; font-size: 20px; line-height: 26px; }
		p { margin: 0; width: 360px; }
		article { line-break: loose; }
		#responsive { line-break: loose; }
		#invalid { line-break: strict; line-break: bogus; }
		#feature { line-break: strict; }
		@supports (line-break: loose) { #feature { line-break: loose; } }
	</style><article lang="ja-JP">"""
	var cases: Array[Dictionary] = [
		{"id": "kana_loose", "left": "本", "right": "ぁ", "split": true},
		{"id": "kana_normal", "left": "本", "right": "ぁ", "split": false, "style": "line-break: normal"},
		{"id": "kana_keep", "left": "本", "right": "ぁ", "split": true, "style": "word-break: keep-all"},
		{"id": "iteration", "left": "本", "right": "々", "split": true},
		{"id": "ellipse_loose", "left": "…", "right": "…", "split": true},
		{"id": "ellipse_auto", "left": "…", "right": "…", "split": false, "style": "line-break: auto"},
		{"id": "wave_normal", "left": "本", "right": "〜", "split": true, "style": "line-break: normal"},
		{"id": "wave_strict", "left": "本", "right": "〜", "split": false, "style": "--mode: strict; line-break: var(--mode)"},
		{"id": "wave_latin", "left": "本", "right": "〜", "split": false, "lang": "ja-Latn"},
		{"id": "wave_hanja", "left": "本", "right": "〜", "split": true, "lang": "ko-Hani"},
		{"id": "wave_untagged", "left": "本", "right": "〜", "split": false, "lang": ""},
		{"id": "dash", "left": "本", "right": "‐", "split": true},
		{"id": "dash_latin", "left": "B", "right": "‐", "split": false},
		{"id": "feature", "left": "本", "right": "・", "split": true, "lang": "zh-Hant", "style": ""},
		{"id": "center_strict", "left": "本", "right": "・", "split": false, "style": "line-break: strict"},
		{"id": "suffix", "left": "８", "right": "％", "split": true},
		{"id": "suffix_unknown", "left": "８", "right": "％", "split": false, "lang": "en"},
		{"id": "prefix", "left": "￥", "right": "８", "split": true},
		{"id": "prefix_normal", "left": "￥", "right": "８", "split": false, "style": "line-break: normal"},
		{"id": "opening", "left": "「", "right": "本", "split": false},
		{"id": "closing", "left": "本", "right": "、", "split": false},
		{"id": "invalid", "left": "本", "right": "〜", "split": false},
		{"id": "responsive", "left": "本", "right": "・", "split": true},
	]
	for case: Dictionary in cases:
		var id: String = case.id
		var language: String = ' lang="%s"' % case.lang if case.has("lang") else ""
		var leading: String = "A" if id == "dash_latin" else "日"
		source += ('<p id="%s"%s style="%s"><span id="%s-first">%s</span>' +
			'<span id="%s-left">%s</span><span id="%s-right">%s</span>語</p>') % [
			id, language, case.get("style", ""), id, leading, id, case.left, id, case.right,
		]
	source += "</article><p id='direct' lang='ja' style='line-break: loose'>日本・語</p>"
	if layout.parse(source) != OK or layout.layout(Vector2i(400, 1600)) != OK:
		_check(false, "Prepare line-break fixture: " + layout.get_render_error())
		return
	for case: Dictionary in cases:
		var id: String = case.id
		var width: float = _rect(layout, id + "-left").end.x - _rect(layout, id + "-first").position.x
		var row: HTMLElement = layout.get_element_by_id(id)
		var style: String = "%s; width: %.3fpx" % [case.get("style", ""), width + 0.2]
		_check(row.set_attribute("style", style) == OK, "Set the exact line width for " + id)
	var direct: HTMLElement = layout.get_element_by_id("direct")
	var width: float = _rect(layout, "kana_loose-left").end.x - _rect(layout, "kana_loose-first").position.x
	_check(direct.set_attribute("style", "line-break: loose; width: %.3fpx" % (width + 0.2)) == OK,
		"Resize adjacent text in a single source node")
	_check(layout.layout(Vector2i(400, 1600)) == OK, "Lay out all line-break modes at a narrow width")
	for case: Dictionary in cases:
		var id: String = case.id
		var left: Rect2 = _rect(layout, id + "-left")
		var right: Rect2 = _rect(layout, id + "-right")
		var split: bool = right.position.y > left.position.y
		_check(split == case.split, "line-break %s has split=%s, expected %s" % [id, split, case.split])
	_check("日本・語" in layout.get_plain_text() and direct.get_bounding_rect().size.y > 26.0,
		"Line breaking splits source runs at punctuation without inserting copied line feeds")
	_check(_has_diagnostic(layout, "line-break", "bogus", "style_element"),
		"An invalid later line-break declaration leaves strict mode in effect")
	var responsive: HTMLElement = layout.get_element_by_id("responsive")
	_check(responsive.set_attribute("lang", "ja-Latn") == OK and layout.layout(Vector2i(400, 1600)) == OK,
		"Reflow after updating the writing system")
	_check(is_equal_approx(_rect(layout, "responsive-left").position.y, _rect(layout, "responsive-right").position.y),
		"Changing lang recomputes the break opportunities")


func _check_line_break_anywhere() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; }
		.probe { width: 5.5ch; }
		.anywhere { line-break: anywhere; }
		#nowrap { white-space: nowrap; }
		#clusters { width: 1.5ch; }
		#spaces { width: 3.5ch; white-space: break-spaces; }
		.grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 20px; }
		@supports (line-break: anywhere) { #feature { line-break: anywhere; } }
	</style>
	<div id="normal" class="probe">abcdefghijklmnop</div>
	<div id="feature" class="probe"><a id="wrapped" href="#normal">abcdefghijklmnop</a></div>
	<div id="nowrap" class="probe anywhere">qwertyuiopasdf<br>qwertyuiopasdf</div>
	<div id="clusters" class="anywhere">e&#769;e&#769;e&#769;</div>
	<div id="spaces" class="anywhere">AAA B</div>
	<div class="grid"><span id="normal_track">abcdefghijklmnop</span><i></i></div>
	<div class="grid anywhere"><span id="anywhere_track">abcdefghijklmnop</span><i></i></div>
	<div id="responsive" class="anywhere"><span id="long_token">ABCDEFGHIJKLMNO</span></div>
	"""
	if layout.parse(source) != OK or layout.layout(Vector2i(100, 700)) != OK:
		_check(false, "Prepare anywhere line-break fixture: " + layout.get_render_error())
		return
	var normal: Rect2 = _rect(layout, "normal")
	var wrapped: HTMLElement = layout.get_element_by_id("wrapped")
	var rects: Array[Rect2] = wrapped.get_client_rects()
	_check(rects.size() == 4 and is_equal_approx(_rect(layout, "feature").size.y, normal.size.y * 4.0),
		"Anywhere wraps a long word into four lines")
	for rect: Rect2 in rects:
		_check(layout.get_link_at(rect.get_center()) == "#normal", "Every wrapped link fragment is hittable")
	_check(is_equal_approx(_rect(layout, "nowrap").size.y, normal.size.y * 2.0),
		"Nowrap still honors the forced break")
	_check(_rect(layout, "anywhere_track").size.x < _rect(layout, "normal_track").size.x,
		"Anywhere contributes grapheme boundaries to Grid minimum sizing")
	var plain: String = layout.get_plain_text()
	var start: int = plain.find("abcdefghijklmnop", 16)
	var last: Array[Rect2] = layout.get_selection_rects(start + 15, start + 16)
	_check(start == 17 and last.size() == 1, "Soft wrapping preserves the source and the last letter is selectable")
	if last.size() == 1:
		_check(layout.hit_test_text(Vector2(last[0].end.x - 0.01, last[0].get_center().y)) == start + 16,
			"The last painted fragment maps back to its source offset")
	var combined: int = plain.find("e\u0301e\u0301e\u0301")
	var first: Array[Rect2] = layout.get_selection_rects(combined, combined + 2)
	var second: Array[Rect2] = layout.get_selection_rects(combined + 2, combined + 4)
	_check(combined >= 0 and first.size() == 1 and second.size() == 1 and first[0].position.y < second[0].position.y,
		"Each accented grapheme stays together across line breaks")
	var spaces: int = plain.find("AAA B")
	var space_rects: Array[Rect2] = layout.get_selection_rects(spaces + 3, spaces + 4)
	_check(space_rects.size() == 1 and space_rects[0].position.y > _rect(layout, "spaces").position.y + 20.0,
		"Break-spaces permits wrapping before the first space")
	var narrow: Rect2 = _rect(layout, "responsive")
	_check(layout.layout(Vector2i(350, 700)) == OK and narrow.size.y > _rect(layout, "responsive").size.y,
		"Reflow merges text fragments again at a wider viewport")
	_check(layout.get_plain_text() == plain, "Viewport-only reflow preserves source text offsets")
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(140, 90)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<style>
		body { margin: 0; font: 20px/24px monospace; color: white; background: black; }
		div { width: 5.5ch; line-break: anywhere; text-shadow: 72px 0 magenta; }
	</style><div>MMMMMMMMMMMM</div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var ink_bounds: Callable = func(region: Rect2i, shadow: bool) -> Rect2i:
		var ink: Rect2i
		for y: int in range(region.position.y, region.end.y):
			for x: int in range(region.position.x, region.end.x):
				var color: Color = image.get_pixel(x, y)
				var visible: bool = color.r >= 0.25 and color.b >= 0.25
				if visible and (color.g <= 0.07 if shadow else color.g >= 0.25):
					var pixel: Rect2i = Rect2i(x, y, 1, 1)
					ink = ink.merge(pixel) if ink.has_area() else pixel
		return ink
	for row: int in range(3):
		var glyphs: Rect2i = ink_bounds.call(Rect2i(0, row * 24, 69, 24), false)
		var shadow: Rect2i = ink_bounds.call(Rect2i(71, row * 24, 69, 24), true)
		_check(glyphs.has_area() and shadow.has_area() and absi(glyphs.size.x - shadow.size.x) <= 2,
			"Line %d paints both the shaped fragment and its shadow: %s and %s" % [row + 1, glyphs, shadow])
	viewport.queue_free()


func _check_overflow_wrap() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; }
		.probe { width: 5.5ch; }
		.anywhere { overflow-wrap: anywhere; }
		.break_word { overflow-wrap: break-word; }
		.legacy { word-break: break-word; overflow-wrap: normal; }
		.priority { width: 8.5ch; }
		.grid { display: grid; width: 20px; grid-template-columns: auto minmax(0, 1fr); }
		.preferred_grid { display: inline-grid; grid-template-columns: auto; }
		.fixed_grid { display: grid; grid-template-columns: 5.5ch; }
		@supports (word-wrap: anywhere) { #supported { overflow-wrap: anywhere; } }
	</style>
	<div class="probe" id="ow_normal">abcdefghijklmnop</div>
	<div class="probe anywhere" id="ow_anywhere"><a id="ow_link" href="#ow_normal">abcdefghijklmnop</a></div>
	<div class="probe break_word" id="ow_break_word">abcdefghijklmnop</div>
	<div class="probe legacy" id="ow_legacy">abcdefghijklmnop</div>
	<div class="probe" id="supported">abcdefghijklmnop</div>
	<div class="probe" id="alias" style="--value: break-word; overflow-wrap: normal;
		word-wrap: var(--value) !important; overflow-wrap: normal">abcdefghijklmnop</div>
	<div class="probe" id="alias_reset" style="word-wrap: anywhere; overflow-wrap: normal">abcdefghijklmnop</div>
	<div class="probe" id="alias_invalid" style="overflow-wrap: anywhere; word-wrap: bogus">abcdefghijklmnop</div>
	<div class="probe anywhere" id="ow_nowrap" style="white-space: nowrap">abcdefghijklmnop</div>
	<div class="priority anywhere"><span id="ow_prefix">abcd</span> <span id="ow_next">ghijkl</span></div>
	<div class="priority break_word"><span id="ow_long_prefix">abcd</span> <span id="ow_long">ghijklmnopqr</span></div>
	<div class="priority" style="line-break: anywhere"><span id="lb_prefix">abcd</span> <span id="lb_next">ghijkl</span></div>
	<div class="grid"><span id="ow_track_normal">abcdefghijklmnop</span><i></i></div>
	<div class="grid anywhere"><span id="ow_track_anywhere">abcdefghijklmnop</span><i></i></div>
	<div class="grid break_word"><span id="ow_track_break_word">abcdefghijklmnop</span><i></i></div>
	<div class="grid legacy"><span id="ow_track_legacy">abcdefghijklmnop</span><i></i></div>
	<div class="preferred_grid anywhere"><span id="ow_track_preferred">abcdefghijklmnop</span></div>
	<div class="fixed_grid break_word"><span id="ow_track_fixed">abcdefghijklmnop</span></div>
	"""
	if layout.parse(source) != OK or layout.layout(Vector2i(420, 1100)) != OK:
		_check(false, "Prepare overflow-wrap fixture: " + layout.get_render_error())
		return
	var line_height: float = _rect(layout, "ow_normal").size.y
	for id: String in ["ow_anywhere", "ow_break_word", "ow_legacy", "supported", "alias", "alias_invalid"]:
		_check(is_equal_approx(_rect(layout, id).size.y, line_height * 4.0),
			"%s uses the grapheme fallback" % id)
	for id: String in ["alias_reset", "ow_nowrap"]:
		_check(is_equal_approx(_rect(layout, id).size.y, line_height),
			"%s keeps an unbroken line" % id)
	_check(_has_diagnostic(layout, "word-wrap", "bogus", "inline_style"), "Alias diagnostics keep the declared spelling")
	var next_word: HTMLElement = layout.get_element_by_id("ow_next")
	_check(next_word.get_client_rects().size() == 1 and
		next_word.get_bounding_rect().position.y > _rect(layout, "ow_prefix").position.y,
		"An ordinary space moves the entire next word onto the next line")
	var long_word: HTMLElement = layout.get_element_by_id("ow_long")
	_check(long_word.get_client_rects().size() == 2 and
		long_word.get_bounding_rect().position.y > _rect(layout, "ow_long_prefix").position.y,
		"An overlong word uses the ordinary break and then falls back on a fresh line")
	_check(is_equal_approx(_rect(layout, "lb_next").position.y, _rect(layout, "lb_prefix").position.y),
		"Line-break anywhere treats a word boundary and grapheme boundaries equally")
	var normal_width: float = _rect(layout, "ow_track_normal").size.x
	var anywhere_width: float = _rect(layout, "ow_track_anywhere").size.x
	_check(anywhere_width < normal_width and is_equal_approx(_rect(layout, "ow_track_legacy").size.x, anywhere_width),
		"Anywhere and legacy word-break break-word reduce the minimum content width")
	var break_word_width: float = _rect(layout, "ow_track_break_word").size.x
	_check(is_equal_approx(break_word_width, normal_width),
		"Break-word preserves minimum content width: %s versus %s" % [break_word_width, normal_width])
	var preferred_width: float = _rect(layout, "ow_track_preferred").size.x
	_check(is_equal_approx(preferred_width, normal_width),
		"An inline grid uses the full preferred width with anywhere: %s versus %s" % [preferred_width, normal_width])
	_check(is_equal_approx(_rect(layout, "ow_track_fixed").size.y, line_height * 4.0),
		"Break-word still wraps once Grid assigns the final track width")
	var link: HTMLElement = layout.get_element_by_id("ow_link")
	for fragment: Rect2 in link.get_client_rects():
		_check(layout.get_link_at(fragment.get_center()) == "#ow_normal", "Each fallback line keeps the link hit area")
	var plain: String = layout.get_plain_text()
	_check(plain.contains("abcdefghijklmnop"), "Fallback wrapping preserves source text without a hyphen")
	var row: HTMLElement = layout.get_element_by_id("ow_anywhere")
	_check(row.set_attribute("style", "overflow-wrap: normal") == OK and layout.layout(Vector2i(420, 1100)) == OK,
		"Remove overflow break positions through the DOM")
	_check(is_equal_approx(row.get_bounding_rect().size.y, line_height) and layout.get_plain_text() == plain,
		"Reflow removes fallback lines and preserves source offsets")


func _check_word_break_all() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; }
		.probe { width: 2.5ch; }
		.break_all { word-break: var(--mode, break-all); }
		.grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 20px; }
		@supports (word-break: break-all) { #wb_supported { word-break: break-all; } }
	</style>
	<div class="probe" id="wb_normal">abcdef</div>
	<div class="probe break_all" id="wb_letters">abcdef</div>
	<div class="probe break_all" id="wb_numbers">123456</div>
	<div class="probe" id="wb_supported">abcdef</div>
	<div class="probe break_all" id="wb_nowrap" style="white-space: nowrap">abcdef</div>
	<div class="probe break_all" id="wb_open">a<span id="wb_opening">(</span><span id="wb_opened">b</span></div>
	<div class="probe break_all" id="wb_close">a<span id="wb_closed">b</span><span id="wb_closing">)</span>c</div>
	<div class="probe break_all">a<span id="wb_money">$</span><span id="wb_amount">1</span></div>
	<div class="probe break_all">a<span id="wb_fraction">1</span><span id="wb_percent">%</span></div>
	<div class="probe break_all">x<span id="wb_glue_left">A</span>&#x2011;<span id="wb_glue_right">B</span></div>
	<div class="probe break_all" style="width: 2.1ch; line-break: loose"><span id="wb_loose_left">AB</span><span id="wb_loose_hyphen">‐</span>C</div>
	<div class="probe break_all" style="width: 2.1ch; line-break: strict">A<span id="wb_strict_left">B</span><span id="wb_strict_hyphen">‐</span>C</div>
	<div class="probe break_all" style="width: 1.2ch" id="wb_accents">e&#769;e&#769;e&#769;</div>
	<div class="probe break_all" id="wb_thai">กขคงจฉชซ</div>
	<div class="probe break_all" id="wb_arabic" style="font-family: Arial">ببببببببب</div>
	<div class="probe break_all" style="width: 8.5ch"><span id="wb_prefix">abcd</span> <a id="wb_link" href="#wb_normal">ghijkl</a></div>
	<div class="grid"><span id="wb_track_normal">abcdef</span><i></i></div>
	<div class="grid break_all"><span id="wb_track_break">abcdef</span><i></i></div>
	"""
	if layout.parse(source) != OK or layout.layout(Vector2i(320, 1000)) != OK:
		_check(false, "Prepare word-break break-all fixture: " + layout.get_render_error())
		return
	var line_height: float = _rect(layout, "wb_normal").size.y
	for id: String in ["wb_letters", "wb_numbers", "wb_supported", "wb_accents"]:
		_check(is_equal_approx(_rect(layout, id).size.y, line_height * 3.0),
			"%s wraps into three lines" % id)
	_check(is_equal_approx(_rect(layout, "wb_nowrap").size.y, line_height),
		"Nowrap suppresses break-all opportunities")
	var punctuation: Dictionary[String, String] = {
		"wb_opening": "wb_opened",
		"wb_closed": "wb_closing",
		"wb_money": "wb_amount",
		"wb_fraction": "wb_percent",
		"wb_glue_left": "wb_glue_right",
		"wb_strict_left": "wb_strict_hyphen",
	}
	for id: String in punctuation:
		_check(is_equal_approx(_rect(layout, id).position.y, _rect(layout, punctuation[id]).position.y),
			"%s and %s stay on the same line" % [id, punctuation[id]])
	_check(_rect(layout, "wb_loose_hyphen").position.y > _rect(layout, "wb_loose_left").position.y,
		"Loose line breaking allows a hyphen to start the next line after remapping letters")
	var link: HTMLElement = layout.get_element_by_id("wb_link")
	var fragments: Array[Rect2] = link.get_client_rects()
	_check(fragments.size() == 2 and is_equal_approx(_rect(layout, "wb_link").position.y, _rect(layout, "wb_prefix").position.y),
		"Break-all can use a word-internal break on the same line as the previous word")
	for fragment: Rect2 in fragments:
		_check(layout.get_link_at(fragment.get_center()) == "#wb_normal", "A break-all fragment retains its link")
	_check(_rect(layout, "wb_track_break").size.x < _rect(layout, "wb_track_normal").size.x,
		"Break-all contributes word-internal breaks to the minimum content width")
	var plain: String = layout.get_plain_text()
	var offset: int = plain.find("e\u0301e\u0301e\u0301")
	var accent_bounds: Array[Rect2] = layout.get_selection_rects(offset, offset + 2)
	_check(offset >= 0 and accent_bounds.size() == 1,
		"A selected accent and its base occupy one visible grapheme")
	for id: String in ["wb_arabic", "wb_thai"]:
		var element: HTMLElement = layout.get_element_by_id(id)
		_check(element.get_bounding_rect().size.y > line_height and plain.contains(element.get_text_content()),
			"%s wraps without changing copied text" % id)


func _check_lexical_word_breaks() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var samples: Dictionary[String, String] = {
		"th": "ภาษาไทยสวยงามมาก",
		"lo": "ພາສາລາວເປັນພາສາ",
		"km": "ភាសាខ្មែរជាភាសាមួយ",
		"my": "မြန်မာဘာသာစကား",
	}
	var first_words: Dictionary[String, String] = {
		"th": "ภาษา",
		"lo": "ພາສາ",
		"km": "ភាសាខ្មែរ",
		"my": "မြန်မာဘာသာ",
	}
	var source: String = """<!doctype html><style>
		body { margin: 0; font-size: 24px; line-height: 30px; }
		.probe { width: 350px; }
		.manual { word-break: manual; }
		.lexical_grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 20px; }
		@supports (word-break: manual) { #manual_support { word-break: manual; } }
	</style>"""
	for language: String in samples:
		source += ('<section lang="%s"><div id="%s_normal" class="probe">%s</div>' +
			'<div id="%s_manual" class="probe manual">%s</div><span id="%s_first">%s</span></section>') % [
			language, language, samples[language], language, samples[language], language, first_words[language],
		]
	source += """<article lang="th-TH">
		<div class="probe" id="markup"><span id="split_word_start">ภา</span><span id="split_word">ษา</span><a id="next_word" href="#th_normal">ไทย</a></div>
		<div class="probe" id="manual_support">ภาษาไทยสวยงามมาก</div>
		<div class="probe manual" id="explicit_wbr">ภาษา<wbr>ไทย</div>
		<div class="probe manual" id="explicit_zwsp">ภาษา&#x200b;ไทย</div>
		<div class="probe" id="single_word" style="width: 1px">ภาษา</div>
		<div class="manual" id="manual_kana" style="width: 1.5em">カタカナ</div>
		<div class="lexical_grid"><span id="lexical_track">ภาษาไทยสวยงามมาก</span><i></i></div>
		<div class="lexical_grid manual"><span id="manual_track">ภาษาไทยสวยงามมาก</span><i></i></div>
	</article>
	<div id="fallback_script" style="width: 1px">ᨠᨡᨢᨣ</div>
	<div id="fallback_manual" class="manual" style="width: 1px">ᨠᨡᨢᨣ</div>"""
	if layout.parse(source) != OK or layout.layout(Vector2i(1100, 1600)) != OK:
		_check(false, "Prepare lexical line breaking fixture: " + layout.get_render_error())
		return
	var word_widths: Dictionary[String, float] = {}
	for language: String in samples:
		word_widths[language] = _rect(layout, language + "_first").size.x
		_check(word_widths[language] > 0.0, "Measure the first %s word before editing" % language)
	for language: String in samples:
		for suffix: String in ["_normal", "_manual"]:
			var probe: HTMLElement = layout.get_element_by_id(language + suffix)
			_check(probe.set_attribute("style", "width: %.3fpx" % (word_widths[language] + 0.25)) == OK,
				"Set the line width to the first %s word" % language)
	var thai_width: float = word_widths["th"]
	var markup: HTMLElement = layout.get_element_by_id("markup")
	_check(markup.set_attribute("style", "width: %.3fpx" % (thai_width - 0.25)) == OK, "Narrow the markup fixture")
	for id: String in ["manual_support", "explicit_wbr", "explicit_zwsp"]:
		_check(layout.get_element_by_id(id).set_attribute("style", "width: %.3fpx" % (thai_width + 0.25)) == OK,
			"Set the line width for " + id)
	_check(layout.layout(Vector2i(1100, 1600)) == OK, "Reflow with the lexical widths")
	var plain: String = layout.get_plain_text()
	for language: String in samples:
		var normal: Rect2 = _rect(layout, language + "_normal")
		var manual: Rect2 = _rect(layout, language + "_manual")
		_check(normal.size.y > manual.size.y, "%s uses dictionary breaks in normal mode" % language)
		var offset: int = plain.find(samples[language])
		var first: Array[Rect2] = layout.get_selection_rects(offset, offset + first_words[language].length())
		_check(offset >= 0 and first.size() == 1, "The first %s word stays on one line and is selectable" % language)
	var manual_height: float = _rect(layout, "th_manual").size.y
	_check(is_equal_approx(_rect(layout, "single_word").size.y, manual_height),
		"A known dictionary can return a single word without falling back to grapheme breaks")
	_check(is_equal_approx(_rect(layout, "manual_support").size.y, manual_height),
		"Word-break manual participates in feature queries")
	_check(_rect(layout, "explicit_wbr").size.y > manual_height and _rect(layout, "explicit_zwsp").size.y > manual_height,
		"Manual preserves explicit wbr and zero-width space opportunities")
	_check(_rect(layout, "manual_kana").size.y > manual_height, "Manual preserves ordinary Kana line breaks")
	_check(_rect(layout, "lexical_track").size.x < _rect(layout, "manual_track").size.x,
		"Grid minimum sizing uses dictionary boundaries")
	_check(_rect(layout, "fallback_script").size.y > _rect(layout, "fallback_manual").size.y,
		"A script without a dictionary can still wrap at grapheme boundaries")
	var word_start: Rect2 = _rect(layout, "split_word_start")
	var first_word: Rect2 = _rect(layout, "split_word")
	var link: Rect2 = _rect(layout, "next_word")
	_check(is_equal_approx(first_word.position.y, word_start.position.y) and
		link.position.y > first_word.position.y and layout.get_link_at(link.get_center()) == "#th_normal",
		"Dictionary lookup crosses inline markup and preserves the link: %s, %s, %s" % [word_start, first_word, link])
	_check(markup.get_text_content() == "ภาษาไทย", "Lexical fragments leave source text intact")
	_check(markup.set_attribute("style", "width: %.3fpx; word-break: manual" % (thai_width - 0.25)) == OK and
		layout.layout(Vector2i(1100, 1600)) == OK, "Switch the inline markup to manual mode")
	_check(is_equal_approx(_rect(layout, "next_word").position.y, _rect(layout, "split_word").position.y),
		"Changing to manual removes previously computed dictionary breaks")


func _check_discretionary_hyphenation() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; }
		.probe { width: 6ch; }
		.grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 1px; }
	</style>
	<div id="reference" class="probe">abcdefgh</div>
	<div id="manual" class="probe"><a href="#reference">abc<span id="default_mark">&shy;</span>defgh</a></div>
	<div id="disabled" class="probe" style="hyphens: none"><span>abc&shy;defgh</span></div>
	<div id="nowrap" class="probe" style="white-space: nowrap">abc&shy;defgh</div>
	<div id="auto" class="probe" style="hyphens: auto">abc&shy;defgh</div>
	<div id="focused" class="probe" style="hyphenate-character: var(--suffix); --suffix: '=='">
		ab<span id="earlier_mark">&shy;</span>cde<span id="later_mark">&shy;</span>fgh</div>
	<div id="at_start" class="probe">&shy;abcdefgh</div>
	<div id="empty" class="probe" style="hyphenate-character: ''">abc<span id="empty_mark">&shy;</span>defgh</div>
	<div id="break_all" class="probe" style="word-break: break-all">abc<span id="break_all_mark">&shy;</span>defgh</div>
	<div id="anywhere" class="probe" style="line-break: anywhere">abc<span id="anywhere_mark">&shy;</span>defgh</div>
	<div id="oversize" class="probe" style="overflow-wrap: anywhere; hyphenate-character: '============'">
		abc<span id="oversize_mark">&shy;</span>defgh</div>
	<div class="probe" style="text-wrap: balance; hyphenate-character: '='">
		abc<span id="balanced_first">&shy;</span>de<span id="balanced_second">&shy;</span>fgh</div>
	<div class="grid" style="hyphenate-character: '=='"><span id="track">abc&shy;def</span><i></i></div>
	<div class="grid" style="hyphens: none"><span id="disabled_track">abc&shy;def</span><i></i></div>
	<div class="grid"><span id="reference_track">abcdef</span><i></i></div>"""
	if layout.parse(source) != OK or layout.layout(Vector2i(240, 640)) != OK:
		_check(false, "Prepare discretionary hyphenation fixture: " + layout.get_render_error())
		return
	var line_height: float = _rect(layout, "reference").size.y
	for id: String in ["disabled", "nowrap", "at_start"]:
		_check(is_equal_approx(_rect(layout, id).size.y, line_height), id + " keeps its original line")
	for id: String in ["manual", "auto", "empty", "break_all", "anywhere", "oversize"]:
		_check(is_equal_approx(_rect(layout, id).size.y, 2.0 * line_height), id + " wraps into two lines")
	for id: String in ["empty_mark", "break_all_mark", "anywhere_mark", "later_mark", "oversize_mark", "balanced_second"]:
		_check(_rect(layout, id).size.x == 0.0, id + " stays visually empty")
	var mark: Rect2 = _rect(layout, "default_mark")
	_check(mark.has_area() and layout.get_link_at(mark.get_center()) == "#reference",
		"The discretionary glyph occupies space inside the wrapped link")
	_check(_rect(layout, "earlier_mark").size.x > mark.size.x * 1.5,
		"Break selection accounts for the width of the full replacement string")
	_check(_rect(layout, "balanced_first").has_area(), "Balanced text can choose and paint an earlier hyphenation point")
	var minimum: float = _rect(layout, "track").size.x
	var unsplit: float = _rect(layout, "reference_track").size.x
	_check(is_equal_approx(minimum * 6.0, unsplit * 5.0), "Min-content includes the replacement glyphs: %s vs %s" % [minimum, unsplit])
	_check(is_equal_approx(_rect(layout, "disabled_track").size.x, unsplit), "Disabling hyphenation preserves the unsplit minimum")
	var text: String = layout.get_plain_text()
	var start: int = text.find("abc\u00addefgh")
	var before: Array[Rect2] = layout.get_selection_rects(start, start + 3)
	var after: Array[Rect2] = layout.get_selection_rects(start + 4, start + 9)
	_check(start >= 0 and before.size() == 1 and after.size() == 1 and after[0].position.y > before[0].position.y,
		"Selection follows both lines while the soft hyphen keeps its source offset")
	_check("==" not in text and "‐" not in text and layout.get_css_diagnostics().is_empty(),
		"Painted hyphens never become document text")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(400, 100)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(200, 100)
	label.text = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; color: white; background: black; }
		#paint { width: 6ch; hyphenate-character: '=='; }
		#paint_mark { color: lime; }
	</style><div id="paint">abc<span id="paint_mark">&shy;</span>defgh</div>"""
	viewport.add_child(label)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(200, 0)
	markdown.size = Vector2(200, 100)
	markdown.parse_html = true
	var icon_texture: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken"]:
		markdown.add_theme_icon_override(icon, icon_texture)
	markdown.text = """<div>
<style>
body { margin: 0; background: black; color: white; }
.measure { font: 20px/24px monospace; width: 6ch; hyphenate-character: '=='; }
.mark { color: lime; }
</style>
<div class="measure">abc<span class="mark">&shy;</span>defgh</div>
</div>"""
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var marker_ink: Callable = func(bounds: Rect2i) -> bool:
		for y: int in range(bounds.position.y, bounds.end.y):
			for x: int in range(bounds.position.x, bounds.end.x):
				var pixel: Color = image.get_pixel(x, y)
				if pixel.g > 0.3 and pixel.r < 0.05 and pixel.b < 0.05:
					return true
		return false
	_check(marker_ink.call(Rect2i(_label_rect(label, "paint_mark"))),
		"HTMLLabel paints the inherited custom hyphen in the marker's foreground color")
	_check(marker_ink.call(Rect2i(200, 0, 200, 100)), "Markdown HTML blocks inherit and paint the same hyphen")
	_check(label.get_element_by_id("paint").set_attribute("style", "width: 12ch") == OK, "Widen the discretionary fixture")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check(_label_rect(label, "paint_mark").size.x == 0.0, "Reflow removes the painted hyphen when the word fits")
	viewport.queue_free()


func _check_hyphenation_limits() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; hyphenate-character: '='; }
		.zone { width: 10ch; }
		.six { width: 6ch; }
		.balance { width: 9ch; text-wrap: balance; }
		.grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 1px; }
		@supports (hyphenate-limit-zone: calc(5% + 1ch)) and (hyphenate-limit-lines: 0) {
		  #conditional { hyphenate-limit-lines: calc(1 - 1); }
		}
	</style>
	<div class="zone" id="ten_characters">aaaaaa bb<span id="default_zone">&shy;</span>cccccc</div>
	<div id="zoned" style="hyphenate-limit-zone: 50%">
	  <div class="zone">aaaaaa bb<span id="large_zone">&shy;</span>cccccc</div>
	</div>
	<div class="zone" style="hyphenate-limit-zone: calc(25% + 1ch)">
	  aaaaaa bb<span id="small_zone">&shy;</span>cccccc</div>
	<div class="zone" style="text-wrap: balance; hyphenate-limit-zone: 65%">
	  aaaa bbb<span id="balanced_zone">&shy;</span>ccccccc</div>
	<div class="zone" style="hyphenate-limit-zone: 50%">
	  <i style="float: left; width: 3ch; height: 24px"></i>aaa bb<span id="float_zone">&shy;</span>cccccc</div>
	<div class="six" style="hyphenate-limit-zone: 100%">abc<span id="no_ordinary">&shy;</span>defgh</div>
	<div class="six" style="hyphenate-limit-lines: no-limit">
	  abcd<span id="unlimited_first">&shy;</span>efgh<span id="unlimited_second">&shy;</span>ijkl</div>
	<div style="hyphenate-limit-lines: var(--limit); --limit: 1">
	  <div class="six">abcd<span id="limited_first">&shy;</span>efgh<span id="limited_second">&shy;</span>ijkl<br>
	    mnop<span id="after_forced">&shy;</span>qrst</div>
	  <div class="six">abcd<span id="natural_first">&shy;</span>efgh xyz<span id="after_ordinary">&shy;</span>mnop</div>
	</div>
	<div class="six" style="hyphenate-limit-lines: 1; hyphenate-character: ''">
	  <span id="empty_head">abcd</span>&shy;<span id="empty_middle">efgh</span>&shy;<span id="empty_end">ijkl</span></div>
	<div class="six" id="conditional">abcd<span id="zero_limit">&shy;</span>efgh</div>
	<div class="balance">aaaa bbb<span id="balanced_unlimited_a">&shy;</span>ccc
	  dd<span id="balanced_unlimited_b">&shy;</span>eeee ff</div>
	<div class="balance" style="hyphenate-limit-lines: 1">aaaa bbb<span id="balanced_limited_a">&shy;</span>ccc
	  dd<span id="balanced_limited_b">&shy;</span>eeee ff</div>
	<div class="grid" style="hyphenate-limit-zone: 100%; hyphenate-limit-lines: 1">
	  <span id="normal_minimum">abc&shy;def</span><i></i></div>
	<div class="grid" style="hyphenate-limit-lines: 0; hyphenate-limit-lines: 1.5">
	  <span id="disabled_minimum">abc&shy;def</span><i></i></div>"""
	if layout.parse(source) != OK or layout.layout(Vector2i(260, 880)) != OK:
		_check(false, "Prepare hyphenation limit fixture: " + layout.get_render_error())
		return
	for id: String in ["default_zone", "small_zone", "float_zone", "no_ordinary", "unlimited_first", "unlimited_second",
		"limited_first", "after_forced", "natural_first", "after_ordinary", "balanced_unlimited_a", "balanced_unlimited_b"]:
		_check(_rect(layout, id).has_area(), id + " permits the discretionary glyph")
	for id: String in ["large_zone", "balanced_zone", "limited_second", "zero_limit", "balanced_limited_a", "balanced_limited_b"]:
		_check(_rect(layout, id).size.x == 0.0, id + " suppresses the discretionary glyph")
	var empty_middle_y: float = _rect(layout, "empty_middle").position.y
	_check(empty_middle_y > _rect(layout, "empty_head").position.y and
		is_equal_approx(empty_middle_y, _rect(layout, "empty_end").position.y),
		"A break with an empty replacement still consumes one hyphenated line")
	_check(_rect(layout, "normal_minimum").size.x < _rect(layout, "disabled_minimum").size.x,
		"Line-relative limits preserve applicable minimum-content opportunities")
	_check(_has_diagnostic(layout, "hyphenate-limit-lines", "1.5", "inline_style"), "A fractional literal is invalid")
	var ten_characters: float = _rect(layout, "ten_characters").size.x
	var container: HTMLElement = layout.get_element_by_id("zoned")
	_check(container.set_attribute("style", "hyphenate-limit-zone: 0") == OK and layout.layout(Vector2i(260, 880)) == OK and
		_rect(layout, "large_zone").has_area(), "Updating an inherited zone recomputes the line breaks")

	var responsive: HTMLLayout = HTMLLayout.new()
	var responsive_source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; hyphenate-limit-zone: 45%; }
	</style><div>aaaaaa bb<span id="responsive_mark">&shy;</span>cccccc</div>"""
	_check(responsive.parse(responsive_source) == OK and responsive.layout(Vector2i(roundi(ten_characters), 96)) == OK,
		"Lay out the percentage zone at ten character widths")
	var responsive_mark: HTMLElement = responsive.get_element_by_id("responsive_mark")
	_check(responsive_mark != null and responsive_mark.get_bounding_rect().size.x == 0.0,
		"The percentage zone permits leaving four character widths empty")
	_check(responsive.layout(Vector2i(roundi(ten_characters * 1.2), 96)) == OK and responsive_mark != null and
		responsive_mark.get_bounding_rect().has_area(), "Resizing resolves the percentage against the new line width")


func _check_hyphenation_character_limits() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; hyphenate-character: '='; }
		.probe { width: 6ch; }
		.grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 1px; }
		@supports (hyphenate-limit-chars: auto calc(2.5) auto) { #supports { hyphenate-limit-chars: auto calc(2.5); } }
	</style>
	<div class="probe" style="hyphenate-limit-chars: 8">abc<span id="equal_total">&shy;</span>defgh</div>
	<div class="probe" style="hyphenate-limit-chars: 9">abc<span id="short_total">&shy;</span>defgh</div>
	<div class="probe" style="hyphenate-limit-chars: 8 4">abcd<span id="symmetric_fit">&shy;</span>efgh</div>
	<div class="probe" style="hyphenate-limit-chars: auto 4">abcde<span id="symmetric_short">&shy;</span>fgh</div>
	<div class="probe" style="hyphenate-limit-chars: 9 4 5"><b>ab</b><span>cd</span><span id="inline_word">&shy;</span><a href="#supports">efghi</a></div>
	<div class="probe" style="hyphenate-limit-chars: 9 5 4">abcd<span id="short_before">&shy;</span>efghi</div>
	<div class="probe" style="hyphenate-limit-chars: 9 4 6">abcd<span id="short_after">&shy;</span>efghi</div>
	<div class="probe" style="hyphenate-limit-chars: 6 auto 3">a<span id="auto_prefix">&shy;</span>bcdefgh</div>
	<div class="probe" style="hyphenate-limit-chars: 0 1 1">a<span id="one_character">&shy;</span>bcdefgh</div>
	<div class="probe" style="hyphenate-limit-chars: auto 4 3">a&#769;bc<span id="combining_short">&shy;</span>defg</div>
	<div class="probe" style="hyphenate-limit-chars: auto 3 3">a&#769;bc<span id="combining_fit">&shy;</span>defg</div>
	<div class="probe" style="hyphenate-limit-chars: 0 1 1">a<span id="inside_grapheme">&shy;</span>&#769;bcdefg</div>
	<div class="probe" lang="en" style="hyphenate-limit-chars: auto 4 3">d’ef<span id="punctuation_short">&shy;</span>ghijk</div>
	<div class="probe" lang="en" style="hyphenate-limit-chars: auto 3 3">d’ef<span id="punctuation_fit">&shy;</span>ghijk</div>
	<div class="probe" style="hyphenate-limit-chars: 8 4 4">1.234<span id="decimal_fit">&shy;</span>5678</div>
	<div class="probe" style="hyphenate-limit-chars: 8 4 4">ab_cd<span id="connector_fit">&shy;</span>efgh</div>
	<div style="width: 8ch; hyphenate-limit-chars: 9 2 2">zzzz,ab<span id="separate_word">&shy;</span>cdefgh</div>
	<div class="probe" id="supports">abcde<span id="supported_math">&shy;</span>fg</div>
	<div class="probe" id="inherited_limit" style="hyphenate-limit-chars: 9; --limit: 8 3 5">
	  abc<span id="inherited_mark">&shy;</span>defgh<br>abc<span style="hyphenate-limit-chars: var(--limit)" id="marker_override">&shy;</span>defgh</div>
	<div class="probe" style="hyphenate-limit-chars: 9; hyphenate-limit-chars: 8.5">abc<span id="invalid_minimum">&shy;</span>defgh</div>
	<div class="grid" style="hyphenate-limit-chars: 7"><span id="minimum_with_cut">abc&shy;defg</span><i></i></div>
	<div class="grid" style="hyphenate-limit-chars: 8"><span id="minimum_without_cut">abc&shy;defg</span><i></i></div>"""
	if layout.parse(source) != OK or layout.layout(Vector2i(260, 1000)) != OK:
		_check(false, "Prepare hyphenation character limits: " + layout.get_render_error())
		return
	for id: String in ["equal_total", "symmetric_fit", "inline_word", "one_character", "combining_fit",
		"punctuation_fit", "decimal_fit", "connector_fit", "marker_override"]:
		_check(_rect(layout, id).has_area(), id + " meets all character limits")
	for id: String in ["short_total", "symmetric_short", "short_before", "short_after", "auto_prefix", "combining_short",
		"inside_grapheme", "punctuation_short", "separate_word", "supported_math", "inherited_mark", "invalid_minimum"]:
		_check(_rect(layout, id).size.x == 0.0, id + " fails a character limit")
	_check(_has_diagnostic(layout, "hyphenate-limit-chars", "8.5", "inline_style"), "A decimal literal is invalid")
	_check(_rect(layout, "minimum_with_cut").size.x < _rect(layout, "minimum_without_cut").size.x,
		"The character limit also applies to intrinsic sizing")
	var container: HTMLElement = layout.get_element_by_id("inherited_limit")
	_check(container.set_attribute("style", "hyphenate-limit-chars: initial; --limit: 8 3 5") == OK and
		layout.layout(Vector2i(260, 1000)) == OK and _rect(layout, "inherited_mark").has_area(),
		"Changing the inherited limit updates the available break on the same element")


func _check_hyphenation_last_line() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; hyphenate-character: '='; }
		.ten { width: 10ch; }
		.six { width: 6ch; }
		.protected { hyphenate-limit-last: always; }
		.grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 1px; }
		@supports (hyphenate-limit-last: spread) { #conditional { hyphenate-limit-last: always; } }
	</style>
	<div class="ten">aaaaaa bb<span id="ordinary_mark">&shy;</span>cccccc</div>
	<div class="protected" id="inherited">
	  <div class="ten">aaaaaa <a id="moved_link" href="#conditional">bb<span id="limited_mark">&shy;</span>cccccc</a></div>
	  <div class="ten" style="hyphenate-limit-last: none">aaaaaa bb<span id="none_mark">&shy;</span>cccccc</div>
	</div>
	<div class="ten" id="conditional">aaaaaa bb<span id="supported_mark">&shy;</span>cccccc</div>
	<div class="ten" style="hyphenate-limit-last: column">aaaaaa bb<span id="column_mark">&shy;</span>cccccc</div>
	<div class="ten" style="hyphenate-limit-last: page">aaaaaa bb<span id="page_mark">&shy;</span>cccccc</div>
	<div class="ten" style="hyphenate-limit-last: spread">aaaaaa bb<span id="spread_mark">&shy;</span>cccccc</div>
	<div class="ten protected">
	  aaaaaa bb<span id="early_group">&shy;</span>cccccc<br>dddddd ee<span id="final_group">&shy;</span>ffffff</div>
	<div class="ten protected">
	  aaaaaa bb<span id="early_inline">&shy;</span>cccccc<div>middle</div>dddddd ee<span id="final_inline">&shy;</span>ffffff</div>
	<div class="ten protected" id="extended_tail">
	  aaaaa bbb<span id="first_trial">&shy;</span>ccc dd<span id="second_trial">&shy;</span>eeee</div>
	<div class="ten protected" style="text-wrap: balance">aaaa bbb<span id="balanced_last">&shy;</span>ccccccc</div>
	<div style="width: 11ch; text-wrap: balance">aaaa bbbb cccc ddd<span id="balance_adds_hyphen">&shy;</span>ee ff</div>
	<div class="protected" style="width: 11ch; text-wrap: balance">aaaa bbbb cccc ddd<span id="balance_respects_last">&shy;</span>ee ff</div>
	<div class="ten protected">
	  <i id="last_float" style="float: left; width: 2ch; height: 48px"></i>
	  aaa <b id="floated_word">bb<span id="floated_mark">&shy;</span>cccccc</b></div>
	<div class="six protected">abcd<span id="overflow_fallback">&shy;</span>efgh</div>
	<div class="six protected" style="overflow-wrap: anywhere">abcd<span id="emergency_wrap">&shy;</span>efgh</div>
	<div class="ten protected" style="hyphenate-limit-last: unknown">aaaaaa bb<span id="invalid_last">&shy;</span>cccccc</div>
	<div class="grid"><span id="normal_intrinsic">abc&shy;defg</span><i></i></div>
	<div class="grid protected"><span id="limited_intrinsic">abc&shy;defg</span><i></i></div>"""
	if layout.parse(source) != OK or layout.layout(Vector2i(260, 1080)) != OK:
		_check(false, "Prepare last-line hyphenation limits: " + layout.get_render_error())
		return
	for id: String in ["ordinary_mark", "none_mark", "column_mark", "page_mark", "spread_mark", "early_group",
		"early_inline", "balance_adds_hyphen", "overflow_fallback"]:
		_check(_rect(layout, id).has_area(), id + " keeps its break opportunity")
	for id: String in ["limited_mark", "supported_mark", "final_group", "final_inline", "first_trial", "second_trial",
		"balanced_last", "balance_respects_last", "floated_mark", "emergency_wrap", "invalid_last"]:
		_check(_rect(layout, id).size.x == 0.0, id + " avoids painting on the last full line")
	var link: HTMLElement = layout.get_element_by_id("moved_link")
	var link_boxes: Array[Rect2] = link.get_client_rects()
	_check(link_boxes.size() == 1 and layout.get_link_at(link_boxes[0].get_center()) == "#conditional" and
		link.get_text_content() == "bb\u00adcccccc", "The replay moves the entire linked word and preserves its source text")
	_check(is_equal_approx(_rect(layout, "extended_tail").size.y, 72.0), "A longer tail protects its newly penultimate line")
	var floated: Rect2 = _rect(layout, "last_float")
	var word: Rect2 = _rect(layout, "floated_word")
	_check(floated.has_area() and is_equal_approx(word.position.x, floated.end.x) and word.position.y > floated.position.y + 20.0,
		"Replaying the terminal lines preserves the float and the remaining width beside it")
	_check(is_equal_approx(_rect(layout, "normal_intrinsic").size.x, _rect(layout, "limited_intrinsic").size.x),
		"Last-line protection leaves intrinsic sizing opportunities available")
	_check(_has_diagnostic(layout, "hyphenate-limit-last", "unknown", "inline_style"), "An invalid literal keeps the earlier rule")
	var container: HTMLElement = layout.get_element_by_id("inherited")
	_check(container.set_attribute("style", "--end: none; hyphenate-limit-last: var(--end)") == OK and
		layout.layout(Vector2i(260, 1080)) == OK and _rect(layout, "limited_mark").has_area(),
		"The last-line limit follows inherited variable updates without stale line state")


func _check_automatic_hyphenation() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var source: String = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; hyphenate-character: '='; }
		.probe { width: 8ch; hyphens: auto; }
		.grid { display: grid; grid-template-columns: auto minmax(0, 1fr); width: 1px; }
	</style>
	<div lang="en-US" class="probe" id="american">dictionary</div>
	<div lang="en-Latn-GB" class="probe" id="british">dictionary</div>
	<div class="probe" id="unknown_language">dictionary</div>
	<div lang="en-Cyrl" class="probe" id="other_script">dictionary</div>
	<div lang="en-US" class="probe" id="manual_style" style="hyphens: manual">dictionary</div>
	<div lang="en-US" class="probe" id="disabled" style="hyphenate-limit-chars: 20">dictionary</div>
	<div lang="en-US" class="probe" id="line_limit" style="hyphenate-limit-lines: 0">dictionary</div>
	<div lang="en-US" class="probe" id="nowrap" style="white-space: nowrap">dictionary</div>
	<div class="probe" id="inline_word" lang="en-US"><a id="dictionary_link" href="#american">dictio</a><span id="dictionary_tail">nary</span></div>
	<div class="probe" id="german" lang="de-DE">Donaudampfschifffahrt</div>
	<div class="probe" id="french" lang="fr-CA">électricité</div>
	<div class="probe" id="decomposed_french" lang="fr-CA">e&#769;lectricite&#769;</div>
	<div class="probe" id="spanish" lang="es-MX">electricidad</div>
	<div class="probe" id="galician" lang="gl">galeguización</div>
	<div class="probe" id="italian" lang="it">interessante</div>
	<div lang="en-US" class="probe">represe<span id="preferred_mark">&shy;</span>ntation</div>
	<div lang="en-US" class="probe" id="needs_fallback">rep<span id="fallback_mark">&shy;</span>resentation</div>
	<div id="fallback_manual" class="probe" style="hyphens: manual">rep&shy;resentation</div>
	<div lang="en-US" class="grid" style="hyphens: auto"><span id="auto_min">dictionary</span><i></i></div>
	<div lang="en-US" class="grid"><span id="manual_min">dictionary</span><i></i></div>
	<div lang="en-US" class="probe" style="width: 13ch; hyphenate-limit-last: always">aaaaaa <a id="whole_word" href="#american">dictionary</a></div>
	"""
	if layout.parse(source) != OK or layout.layout(Vector2i(260, 1000)) != OK:
		_check(false, "Prepare automatic hyphenation: " + layout.get_render_error())
		return
	var plain: String = layout.get_plain_text()
	var american: int = plain.find("dictionary")
	var british: int = plain.find("dictionary", american + 10)
	var us_letter: Array[Rect2] = layout.get_selection_rects(american + 6, american + 7)
	var gb_letter: Array[Rect2] = layout.get_selection_rects(british + 6, british + 7)
	_check(american == 0 and british == 11 and us_letter.size() == 1 and gb_letter.size() == 1 and
		us_letter[0].position.y > _rect(layout, "american").position.y + 20.0 and
		gb_letter[0].position.y < _rect(layout, "british").position.y + 20.0,
		"US and British dictionaries produce different cuts without inserting characters into selectable text")
	for id: String in ["unknown_language", "other_script", "manual_style", "disabled", "line_limit", "nowrap"]:
		_check(is_equal_approx(_rect(layout, id).size.y, 24.0), id + " retains a single line")
	for id: String in ["german", "french", "spanish", "galician", "italian"]:
		_check(_rect(layout, id).size.y > 24.0, id + " uses its inherited language dictionary")
	_check(is_equal_approx(_rect(layout, "decomposed_french").size.y, _rect(layout, "french").size.y) and
		"e\u0301lectricite\u0301" in plain, "Canonical equivalents share dictionary points while keeping source text")
	var link: Rect2 = _rect(layout, "dictionary_link")
	var tail: Rect2 = _rect(layout, "dictionary_tail")
	_check(tail.position.y > link.position.y + 20.0 and
		layout.get_link_at(Vector2(link.end.x - 1.0, link.get_center().y)) == "#american",
		"A break at an inline boundary keeps the inserted glyph inside the preceding link")
	_check(_rect(layout, "preferred_mark").has_area() and _rect(layout, "fallback_mark").has_area(),
		"Authored breaks take precedence over dictionary points")
	_check(_rect(layout, "needs_fallback").size.y > _rect(layout, "fallback_manual").size.y,
		"Dictionary points are available inside an authored segment that is still too long")
	_check(_rect(layout, "auto_min").size.x < _rect(layout, "manual_min").size.x,
		"Automatically hyphenated words contribute their minimum line width to Grid")
	_check(layout.get_element_by_id("whole_word").get_client_rects().size() == 1,
		"The terminal-line limit moves the whole automatically hyphenated word")
	_check(layout.get_element_by_id("inline_word").set_attribute("lang", "und") == OK and layout.layout(Vector2i(260, 1000)) == OK and
		is_equal_approx(_rect(layout, "inline_word").size.y, 24.0), "Changing inherited language withdraws the previous points")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(180, 80)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; font: 20px/24px monospace; color: white; background: black; }
		div { width: 8ch; hyphens: auto; hyphenate-character: '='; }
		#ink { color: lime; }
	</style><div lang="en-US"><span id="ink">dictio</span>nary</div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var ink: Rect2i = Rect2i(_label_rect(label, "ink"))
	var last_cell: int = roundi(float(ink.size.x) / 7.0)
	var painted: bool
	for y: int in range(ink.position.y, ink.end.y):
		for x: int in range(ink.end.x - last_cell, ink.end.x):
			var pixel: Color = image.get_pixel(x, y)
			painted = painted or (pixel.g > 0.3 and pixel.r < 0.05 and pixel.b < 0.05)
	_check(painted, "HTMLLabel paints the automatic replacement at the end of the first line")
	viewport.queue_free()


func _ink_center_x(image: Image, box: Rect2) -> float:
	var ink: float
	var moment: float
	for y: int in range(roundi(box.position.y), roundi(box.end.y)):
		for x: int in range(roundi(box.position.x), roundi(box.end.x)):
			var amount: float = image.get_pixel(x, y).r
			ink += amount
			moment += amount * float(x)
	return moment / ink if ink > 0.0 else 0.0


func _font_ink(image: Image, box: Rect2) -> float:
	var ink: float
	for y: int in range(roundi(box.position.y), roundi(box.end.y)):
		for x: int in range(roundi(box.position.x), roundi(box.end.x)):
			ink += image.get_pixel(x, y).r
	return ink


func _check_controls() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(500, 350)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)

	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(250, 280)
	label.stylesheet = """
		.specific_pixel { background: red; }
		.tie_pixel { background: #35ae71; }
		.clip_literal { background-clip: content-box; }
	"""
	label.text = """
		<style>
		  body { margin: 0; }
		  @layer base, theme;
		  @layer theme { #layered_pixel { background: #2196f3; } }
		  @layer base { #reverted, #layered_pixel { background: red; } }
		  #specific_pixel { background: #17a2b8; }
		  .tie_pixel { background: red; }
		  #parent { color: red; background-color: color-mix(in srgb, currentColor, white); }
		  .bevel { display: inline-block; width: 28px; height: 25px; border: 8px solid #808080; }
		  .clip_literal, .clip_variable {
		    width: 18px; height: 10px; padding: 4px; border: 2px solid transparent;
		    background: #b53078 border-box;
		  }
		</style>
		<button id="reverted" type="button" style="background: var(--missing, revert)">Button</button>
		<button id="reference" type="button">Button</button>
		<div id="layered_pixel" style="width: 20px; height: 10px"></div>
		<div id="specific_pixel" class="specific_pixel" style="width: 20px; height: 10px"></div>
		<div id="tie_pixel" class="tie_pixel" style="width: 20px; height: 10px"></div>
		<div id="parent" style="width: 100px; height: 30px">
		  <div id="child" style="color: blue; background-color: inherit; width: 50px; height: 30px"></div>
		</div>
		<div><div id="groove" class="bevel" style="border-style: groove; border-radius: 12px"></div>
		  <div id="ridge" class="bevel" style="border-style: ridge"></div>
		  <div id="inset" class="bevel" style="border-style: inset"></div>
		  <div id="outset" class="bevel" style="border-style: outset"></div></div>
		<div id="clip_literal" class="clip_literal"></div>
		<div id="clip_variable" class="clip_variable"
		     style="--clip: content-box; background-clip: var(--clip); background-clip: bogus"></div>
		<div id="aspect_control" style="width: 60px; aspect-ratio: 3; background: #f34a80"></div>
	"""
	viewport.add_child(label)

	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(250, 0)
	markdown.size = Vector2(240, 240)
	var theme_font: SystemFont = SystemFont.new()
	theme_font.font_names = PackedStringArray(["Consolas"])
	markdown.add_theme_font_override("text_font", theme_font)
	markdown.add_theme_font_size_override("text_font_size", 28)
	var test_texture: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken"]:
		markdown.add_theme_icon_override(icon, test_texture)
	markdown.parse_html = true
	markdown.text = """
<div>
<style>
@layer base, theme;
@layer theme { .themed { background: #edc231; } }
@layer base { #themed { background: red; font: 10px serif; } }
</style>
<div id="themed" class="themed" style="font: var(--missing, revert); width: 5ch; height: 1em">Theme</div>
<div style="width: 60px; aspect-ratio: 3; background: #f34a80"></div>
</div>
"""
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var reverted: Rect2 = _label_rect(label, "reverted")
	var reference: Rect2 = _label_rect(label, "reference")
	_check(reverted.has_area() and reverted.size.is_equal_approx(reference.size), "Button rollback restores default geometry")
	_check_pixel(image, reverted.position + Vector2(5.0, 5.0), Color("#f3f4f6"), "Button rollback restores the default fill")
	_check_pixel(image, reference.position + Vector2(5.0, 5.0), Color("#f3f4f6"), "Button default fill")
	_check_pixel(image, _label_rect(label, "layered_pixel").position + Vector2(5.0, 5.0),
		Color("#2196f3"), "HTMLLabel paints the highest priority layer")
	_check_pixel(image, _label_rect(label, "specific_pixel").position + Vector2(5.0, 5.0),
		Color("#17a2b8"), "HTMLLabel document ID rule wins over the additional class rule")
	_check_pixel(image, _label_rect(label, "tie_pixel").position + Vector2(5.0, 5.0),
		Color("#35ae71"), "HTMLLabel additional class rule wins a source-order tie")
	var parent: Rect2 = _label_rect(label, "parent")
	_check_pixel(image, parent.position + Vector2(15.0, 15.0), Color("#8080ff"), "Inherited mix uses the child's currentColor")
	_check_pixel(image, parent.position + Vector2(75.0, 15.0), Color("#ff8080"), "Parent mix uses its own currentColor")
	var dark: Color = Color("#535353")
	var light: Color = Color("#acacac")
	var groove: Vector2 = _label_rect(label, "groove").position
	var ridge: Vector2 = _label_rect(label, "ridge").position
	var inset: Vector2 = _label_rect(label, "inset").position
	var outset: Vector2 = _label_rect(label, "outset").position
	_check_pixel(image, groove + Vector2(20.0, 2.0), dark, "Rounded groove outer band")
	_check_pixel(image, groove + Vector2(20.0, 6.0), light, "Rounded groove inner band")
	_check_pixel(image, ridge + Vector2(20.0, 2.0), light, "Ridge outer band")
	_check_pixel(image, ridge + Vector2(20.0, 6.0), dark, "Ridge inner band")
	_check_pixel(image, inset + Vector2(20.0, 2.0), dark, "Inset top edge")
	_check_pixel(image, inset + Vector2(41.0, 20.0), light, "Inset right edge")
	_check_pixel(image, outset + Vector2(20.0, 2.0), light, "Outset top edge")
	_check_pixel(image, outset + Vector2(41.0, 20.0), dark, "Outset right edge")
	for id: String in ["clip_literal", "clip_variable"]:
		var border_box: Rect2i = Rect2i(_label_rect(label, id))
		var expected: Rect2i = Rect2i(border_box.position + Vector2i(6, 6), Vector2i(18, 10))
		var painted: Rect2i = _color_bounds(image, border_box, Color("#b53078"))
		_check(painted == expected, "%s longhand clips the shorthand background to the content box, got %s" % [id, painted])
	var theme_box: Rect2i = _color_bounds(image, Rect2i(250, 0, 240, 200), Color("#edc231"))
	var theme_width: int = roundi(5.0 * theme_font.get_string_size("0", HORIZONTAL_ALIGNMENT_LEFT, -1.0, 28).x)
	_check(theme_box.size == Vector2i(theme_width, 28), "Markdown HTML rollback restores the theme font, box=%s" % theme_box)
	var ratio_box: Rect2 = _label_rect(label, "aspect_control")
	_check(ratio_box.size.is_equal_approx(Vector2(60.0, 20.0)), "HTMLLabel uses the preferred aspect ratio")
	_check(_color_bounds(image, Rect2i(250, 0, 240, 200), Color("#f34a80")).size == Vector2i(60, 20),
		"Markdown HTML paints the box at the preferred aspect ratio")
	viewport.queue_free()


func _check_element_activation() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 180)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(320.0, 180.0)
	label.text = """<!doctype html><style>
		body { margin: 0; }
		a, button { display: block; }
		#command span { display: inline-block; width: 20px; }
	</style><a id="anchor" href="#end%2Dpoint"><span id="old_child">Go</span></a>
	<a id="empty" href="">Empty</a><a id="hidden" href="#end-point" style="display: none">Hidden</a>
	<button id="command" type="BUTTON"><span id="left">Left</span><span id="right">Right</span></button>
	<button id="submit" type="submit">Submit</button>
	<div id="end-point" style="margin-top: 420px; height: 200px"></div>"""
	viewport.add_child(label)
	var uris: Array[String] = []
	var offsets: Array[int] = []
	var commands: Array[HTMLElement] = []
	var clicks: Array[HTMLElement] = []
	label.link_pressed.connect(func(uri: String) -> void:
		uris.append(uri)
		offsets.append(label.get_scroll_offset().y)
	)
	label.button_pressed.connect(func(element: HTMLElement) -> void: commands.append(element))
	label.element_clicked.connect(func(element: HTMLElement) -> void: clicks.append(element))
	await process_frame
	await process_frame

	var anchor: HTMLAnchorElement = label.get_element_by_id("anchor") as HTMLAnchorElement
	var empty: HTMLAnchorElement = label.get_element_by_id("empty") as HTMLAnchorElement
	var button: HTMLButtonElement = label.get_element_by_id("command") as HTMLButtonElement
	var submit: HTMLButtonElement = label.get_element_by_id("submit") as HTMLButtonElement
	_check(anchor != null and empty != null and button != null and submit != null, "Queries return typed anchors and buttons")
	if anchor == null or empty == null or button == null or submit == null:
		viewport.queue_free()
		return
	var old_child: HTMLElement = anchor.get_children()[0]
	_check(anchor.text == "Go" and anchor.href == "#end%2Dpoint", "Anchor properties read its text and raw URI")
	anchor.text = "Updated"
	button.name = "action"
	button.value = "open"
	await process_frame
	await process_frame
	_check(anchor.text == "Updated" and not old_child.is_valid() and button.type == "BUTTON" and
		button.get_attribute("name") == "action" and button.get_attribute("value") == "open",
		"Typed setters share the existing DOM mutation path")
	_check(label.activate_element(anchor) and uris == ["#end%2Dpoint"] and offsets.size() == 1 and offsets[0] > 0,
		"Script activation scrolls to the decoded fragment before emitting the original URI")
	_check(label.activate_element(button) and commands == [button] and clicks.is_empty(),
		"Script activation emits the existing typed button instance and leaves pointer clicks alone")
	label.set_scroll_offset(Vector2i.ZERO)
	await process_frame
	await process_frame

	var hidden: HTMLElement = label.get_element_by_id("hidden")
	var detached: HTMLAnchorElement = label.create_element("a") as HTMLAnchorElement
	if detached != null:
		detached.href = "#end-point"
	var foreign: HTMLLayout = HTMLLayout.new()
	foreign.parse('<a href="foreign">Foreign</a>')
	foreign.layout(Vector2i(120, 100))
	_check(detached != null and not label.activate_element(detached) and not label.activate_element(hidden) and
		not label.activate_element(submit) and not label.activate_element(foreign.query_selector("a")),
		"Activation rejects detached, hidden, unsupported and foreign elements")
	_check(not label.activate_element(empty) and label.focus_element(empty), "An empty href can receive focus but cannot activate")
	_send_dom_key(viewport, KEY_ENTER)
	var empty_center: Vector2 = empty.get_bounding_rect().get_center()
	_drag_pointer(viewport, empty_center, empty_center)
	_check(uris == ["#end%2Dpoint"], "Keyboard and mouse also skip empty href")

	empty.href = "guide.html"
	await process_frame
	await process_frame
	_check(label.focus_element(empty), "A link with a relative URI retains its focus behavior")
	_send_dom_key(viewport, KEY_ENTER)
	_check(uris == ["#end%2Dpoint", "guide.html"], "Enter emits the raw relative URI")
	empty.remove_attribute("href")
	button.disabled = true
	_check(button.is_disabled() and button.has_attribute("disabled") and not label.activate_element(button),
		"An explicitly disabled button cannot activate")
	await process_frame
	await process_frame
	_check(button.matches(":disabled") and not label.focus_element(button), "The typed disabled setter updates pseudo-classes and focus")
	button.disabled = false
	await process_frame
	await process_frame
	_check(not button.has_attribute("disabled") and button.matches(":enabled") and label.focus_element(button),
		"Enabling through the typed property restores the current button")
	_send_dom_key(viewport, KEY_SPACE)
	_send_dom_key(viewport, KEY_SPACE, 0, false, false)
	var left: Rect2 = _label_rect(label, "left")
	var right: Rect2 = _label_rect(label, "right")
	_drag_pointer(viewport, Vector2(left.end.x - 1.0, left.get_center().y), Vector2(right.position.x + 1.0, right.get_center().y))
	_check(commands == [button, button, button], "Space and a click across button descendants each activate the same button once")
	viewport.queue_free()


func _check_image_element() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var source: String = """<!doctype html><img id="sample" src="orientation-6.png" alt="Example" width="80" height="30"
		style="width: 50px; height: 40px; image-resolution: 2dppx">"""
	_check(layout.parse(source) == OK, "Parse the typed image fixture")
	var image: HTMLImageElement = layout.get_element_by_id("sample") as HTMLImageElement
	_check(image != null, "Image queries return the specialized handle")
	if image == null:
		return
	_check(image.src == "orientation-6.png" and image.alt == "Example" and image.width_hint == 80 and image.height_hint == 30,
		"Image properties read the original source and dimension hints")
	_check(image.get_intrinsic_size() == Vector2.ZERO and layout.layout(Vector2i(200, 100)) == OK and
		image.get_intrinsic_size().is_equal_approx(Vector2(10.0, 16.0)) and
		image.get_bounding_rect().size.is_equal_approx(Vector2(50.0, 40.0)),
		"Image intrinsic size honors orientation and density while CSS controls the visual size")
	image.src = "density-live.png#xywh=0,0,20,8"
	image.alt = "Changed"
	image.width_hint = 25
	image.height_hint = 15
	_check(image.get_intrinsic_size() == Vector2.ZERO and layout.layout(Vector2i(200, 100)) == OK and
		image.get_intrinsic_size().is_equal_approx(Vector2(10.0, 4.0)) and layout.query_selector("img") == image and
		image.get_attribute("alt") == "Changed" and image.get_attribute("width") == "25" and image.get_attribute("height") == "15",
		"Changing source and hints keeps the same handle and reports the cropped image after layout")
	var detached: HTMLImageElement = layout.create_element("img") as HTMLImageElement
	_check(detached != null and detached.get_intrinsic_size() == Vector2.ZERO, "Detached images also have typed handles")
	image.src = "missing-image.png"
	_check(layout.layout(Vector2i(200, 100)) == OK and image.get_intrinsic_size() == Vector2.ZERO,
		"A missing image returns zero intrinsic size")
	layout.clear()
	_check(not image.is_valid() and image.get_intrinsic_size() == Vector2.ZERO, "An invalidated image handle returns zero size")


func _check_responsive_images() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><style>body { margin: 0; }</style>
		<img id="width" src="sampling.svg" srcset="orientation-1.png 32w, density-live.png 48w"
			sizes="(width <= 32px) calc(100vw - 4px), 100vw">
		<img id="density" src="orientation-1.png" srcset="density-live.png 2x">
		<img id="fragment" srcset="orientation-1.png#xywh=0,0,12,8, density-live.png 2x">"""
	_check(layout.parse(html) == OK, "Parse responsive image markup")
	var width_image: HTMLImageElement = layout.get_element_by_id("width") as HTMLImageElement
	var density_image: HTMLImageElement = layout.get_element_by_id("density") as HTMLImageElement
	var fragment: HTMLImageElement = layout.get_element_by_id("fragment") as HTMLImageElement
	if width_image == null or density_image == null or fragment == null:
		_check(false, "Responsive images use the specialized handles")
		return
	_check(width_image.current_src == "" and "32w" in width_image.srcset and "calc(" in width_image.sizes,
		"Responsive properties are readable before the first layout")
	_check(layout.layout(Vector2i(24, 200)) == OK and width_image.current_src == "res://orientation-1.png" and
		width_image.get_intrinsic_size().is_equal_approx(Vector2(20.0, 12.5)),
		"Media size and viewport math determine the width descriptor density")
	_check(layout.layout(Vector2i(28, 200)) == OK and width_image.current_src == "res://orientation-1.png" and
		width_image.get_intrinsic_size().is_equal_approx(Vector2(24.0, 15.0)),
		"The same URL reports a new intrinsic size as its size-dependent density changes")
	_check(layout.layout(Vector2i(40, 200)) == OK and width_image.current_src == "res://density-live.png" and
		width_image.get_intrinsic_size().is_equal_approx(Vector2(40.0, 20.0)),
		"Crossing a candidate boundary selects the next width and preserves the image handle")
	_check(density_image.current_src == "res://orientation-1.png" and
		density_image.get_intrinsic_size().is_equal_approx(Vector2(32.0, 20.0)),
		"The src attribute supplies the implicit 1x candidate")
	_check(fragment.current_src == "res://orientation-1.png#xywh=0,0,12,8" and
		fragment.get_intrinsic_size().is_equal_approx(Vector2(12.0, 8.0)),
		"Commas in image fragments survive srcset parsing and current_src")
	density_image.srcset = "sampling.svg 1e999x, orientation-1.png 1x 2x, density-live.png 2x, orientation-1.png 2x"
	density_image.remove_attribute("src")
	width_image.sizes = "auto, 80%, 24px"
	_check(layout.layout(Vector2i(40, 200)) == OK and density_image.current_src == "res://density-live.png" and
		density_image.get_intrinsic_size().is_equal_approx(Vector2(24.0, 12.0)) and
		width_image.current_src == "res://orientation-1.png" and is_equal_approx(width_image.get_intrinsic_size().x, 24.0),
		"Invalid candidates and sizes recover, with the first equal-density source winning")
	density_image.srcset = "missing-responsive.png 1x"
	_check(density_image.current_src == "res://density-live.png", "Pending edits retain the last committed source")
	_check(layout.layout(Vector2i(40, 200)) == OK and density_image.current_src == "res://missing-responsive.png" and
		density_image.get_intrinsic_size() == Vector2.ZERO, "A selected missing image retains its URL with zero intrinsic size")

	var document: HTMLDocument = ResourceLoader.load("{RESPONSIVE_PICTURE_UID}") as HTMLDocument
	_check(document != null and not FileAccess.file_exists("res://responsive/wide.png"),
		"Load imported responsive HTML without its original image files")
	if document == null:
		return
	var path: String = ResourceUID.get_id_path(ResourceUID.text_to_id("{RESPONSIVE_PICTURE_UID}"))
	var dependencies: PackedStringArray = ResourceLoader.get_dependencies(path)
	for name: String in ["small", "medium", "wide"]:
		_check("res://responsive/%s.png" % name in "\n".join(dependencies), "Imported HTML includes the %s texture dependency" % name)
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(180, 100)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(60.0, 100.0)
	label.document = document
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var imported: HTMLImageElement = label.get_element_by_id("imported_responsive") as HTMLImageElement
	_check(imported != null and imported.current_src == "res://responsive/small.png",
		"Small view uses the img srcset after skipping unsuitable sources")
	if imported != null:
		_check_pixel(viewport.get_texture().get_image(), Vector2(3.0, 3.0), Color.RED, "Small imported candidate is painted")
		label.size.x = 90.0
		await process_frame
		await process_frame
		_check(imported.current_src == "res://responsive/medium.png", "Growing within a media branch selects another width candidate")
		label.size.x = 140.0
		await process_frame
		await process_frame
		await RenderingServer.frame_post_draw
		_check(imported.current_src == "res://responsive/wide.png" and imported.get_bounding_rect().size == Vector2(60.0, 27.0) and
			imported.get_intrinsic_size() == Vector2(80.0, 40.0), "Picture media change applies the selected source dimensions")
		_check_pixel(viewport.get_texture().get_image(), Vector2(3.0, 3.0), Color.BLUE, "Wide imported candidate replaces the drawn texture")
		var source: HTMLElement = label.query_selector('source[type="image/png"]')
		_check(source != null and not source.get_bounding_rect().has_area() and
			source.set_attribute("media", "(min-width: 200px)") == OK, "The picture source is queryable and has no layout box")
		await process_frame
		await process_frame
		_check(imported.current_src == "res://responsive/medium.png" and imported.get_bounding_rect().size.is_equal_approx(Vector2(70.0, 35.0)) and
			label.get_element_by_id("imported_responsive") == imported, "Editing source media restores the img hints and keeps its handle")
	viewport.queue_free()


func _check_image_alt_text() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 320)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(260.0, 320.0)
	label.text = """<!doctype html><style>
		body { margin: 0; }
		img { display: block; color: lime; font: 18px/24px monospace; }
		#swap { width: 104px; }
		.clip { width: 96px; height: 29px; font-size: 22px; line-height: 27px; }
		#round { border-radius: 20px; }
		#zero { width: 48px; height: 24px; }
	</style>
	<picture>
		<source media="(min-width: 280px)" type="image/png" srcset="density-live.png">
		<img id="swap" src="missing-alt.png" alt="English 中文 words 更多文字 here">
	</picture>
	<img id="auto" src="density-live.png#xywh=1000,1000,10,10" alt="Missing 图">
	<img id="empty" src="missing-alt.png" alt="">
	<img id="missing" src="missing-alt.png">
	<img id="square" class="clip" src="missing-alt.png" alt="████████████">
	<img id="round" class="clip" src="missing-alt.png" alt="████████████">
	<img id="zero" srcset="density-live.png 48w" sizes="0px" alt="Wrong fallback">"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var swap: HTMLImageElement = label.get_element_by_id("swap") as HTMLImageElement
	if swap == null:
		_check(false, "Query the picture fallback handle")
		viewport.queue_free()
		return
	var fallback_box: Rect2 = swap.get_bounding_rect()
	var image: Image = viewport.get_texture().get_image()
	_check(swap.get_intrinsic_size() == Vector2.ZERO and swap.current_src == "res://missing-alt.png" and
		fallback_box.size.x == 104.0 and fallback_box.size.y >= 48.0,
		"The failed candidate keeps its URL and uses wrapped text for auto height")
	var ink: Rect2i = _color_bounds(image, Rect2i(fallback_box), Color.LIME)
	_check(ink.has_area() and ink.size.y > 24, "Mixed English and Chinese alt text is drawn on multiple lines")
	var auto_box: Rect2 = _label_rect(label, "auto")
	_check(auto_box.size.x > 24.0 and auto_box.size.y == 24.0 and
		_color_bounds(image, Rect2i(auto_box), Color.LIME).has_area(),
		"An invalid crop uses the fallback text for both automatic axes")
	_check(not _label_rect(label, "empty").has_area() and not _label_rect(label, "missing").has_area(),
		"Empty and missing alt omit the text fallback")
	var square: Rect2i = Rect2i(_label_rect(label, "square"))
	var rounded: Rect2i = Rect2i(_label_rect(label, "round"))
	var radius: float = float(rounded.size.y) * 0.5
	var square_corner_ink: int = 0
	var rounded_corner_ink: int = 0
	for y: int in range(floori(radius)):
		for x: int in range(floori(radius)):
			var point: Vector2i = Vector2i(x, y)
			if (Vector2(point) + Vector2(0.5, 0.5)).distance_to(Vector2(radius, radius)) <= radius + 1.0:
				continue
			if _same_color(image.get_pixelv(square.position + point), Color.LIME):
				square_corner_ink += 1
			if _same_color(image.get_pixelv(rounded.position + point), Color.LIME):
				rounded_corner_ink += 1
	_check(square.size == Vector2i(96, 29) and rounded.size == square.size and
		square_corner_ink > 0 and rounded_corner_ink == 0 and _color_bounds(image, rounded, Color.LIME).has_area(),
		"Fixed-size fallback text obeys the rounded clip (square=%s, rounded=%s, corner ink=%s / %s)" %
			[square, rounded, square_corner_ink, rounded_corner_ink])
	_check_pixel(image, _label_rect(label, "zero").get_center(), Color.RED,
		"A valid zero-slot image with a fixed CSS box still paints its texture")

	label.size.x = 310.0
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var restored_box: Rect2 = swap.get_bounding_rect()
	image = viewport.get_texture().get_image()
	_check(label.get_element_by_id("swap") == swap and swap.current_src == "res://density-live.png" and
		swap.get_intrinsic_size() == Vector2(48.0, 24.0) and restored_box.size == Vector2(104.0, 52.0),
		"Selecting a usable candidate keeps the handle and restores natural image sizing")
	_check_pixel(image, restored_box.get_center(), Color.RED, "The valid image replaces the alternative text")
	_check(not _color_bounds(image, Rect2i(restored_box), Color.LIME).has_area(), "The old alternative text is cleared")

	label.size.x = 260.0
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check(swap.is_valid() and swap.get_intrinsic_size() == Vector2.ZERO and swap.get_bounding_rect() == fallback_box and
		_color_bounds(image, Rect2i(fallback_box), Color.LIME).has_area(),
		"Switching back to a failed candidate restores the fallback geometry and text")
	viewport.queue_free()


func _check_pointer_events() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	for transform: String in ["none", "translateX(5px)"]:
		_check(layout.parse("""
			<style>
				@supports (pointer-events: none) { #overlay { pointer-events: var(--hits, none); } }
			</style>
			<div style="position: relative; width: 220px; height: 120px;">
				<a id="behind" href="#behind"
					style="position: absolute; inset: 0; display: block;">Behind</a>
				<div id="overlay" style="position: absolute; inset: 0; transform: %s;">
					<a id="pass" href="#pass" style="display: block; width: 140px; height: 35px;">Pass through</a>
					<a id="restore" href="#restore" style="display: block; width: 140px; height: 35px;
						pointer-events: initial;">Restored</a>
				</div>
			</div>
		""" % transform) == OK and layout.layout(Vector2i(240, 140)) == OK, "Pointer-events document lays out")
		var pass_box: HTMLElement = layout.get_element_by_id("pass")
		var restored: HTMLElement = layout.get_element_by_id("restore")
		var pass_rect: Rect2 = pass_box.get_bounding_rect()
		_check(layout.get_element_at(pass_rect.get_center()) == layout.get_element_by_id("behind") and
			layout.get_link_at(pass_rect.get_center()) == "#behind", "Inherited pointer-events passes hits through")
		_check(layout.get_element_at(restored.get_bounding_rect().get_center()) == restored and
			layout.get_link_at(restored.get_bounding_rect().get_center()) == "#restore",
			"An eligible descendant restores hits inside a pointer-transparent box")
		_check(layout.get_plain_text().contains("Pass through") and layout.hit_test_text(
			pass_rect.position + Vector2(32.0, 10.0)) >= layout.get_plain_text().find("Pass through"),
			"Pointer transparency preserves text, layout and text hit testing")
		layout.get_element_by_id("overlay").set_attribute("style", "position:absolute; inset:0; --hits:auto;")
		_check(layout.layout(Vector2i(240, 140)) == OK and layout.get_link_at(
			pass_box.get_bounding_rect().get_center()) == "#pass", "Changing pointer-events updates hits")


func _check_text_overflow() -> void:
	var html: String = """<!doctype html><style>
		body { margin: 0; color: white; background: black; font: 24px / 32px Consolas, monospace; }
		.row { width: 5.2ch; height: 32px; overflow: hidden; white-space: nowrap; }
		.cut { text-overflow: var(--mode, ellipsis); }
		a { color: inherit; text-decoration: none; }
		.badge { display: inline-block; width: 4ch; height: 19px; background: red; }
		.shift { transform: translateX(20px); }
		.tiny { width: 0.7ch; }
		.scroll { overflow: auto; }
	</style>
	<div class="row cut" id="cut"><a id="title" href="#foo">A&#x301;<span>BCD</span>EFGHIJK</a></div>
	<div class="row" id="reference">A&#x301;<span>BCD</span>…</div>
	<div class="row cut shift" id="atomic">AB<a class="badge" id="badge" href="#badge"></a>CD</div>
	<div class="row shift" id="atomic_ref">AB…</div>
	<div class="row cut tiny" id="tiny">ABC</div>
	<div class="row tiny" id="tiny_ref">ABC</div>
	<div class="row scroll cut" id="scroll_cut">ABCDEFGHIJKL</div>
	<div class="row scroll" id="scroll_ref">ABCDEFGHIJKL</div>
	"""
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse(html) == OK and layout.layout(Vector2i(180, 270)) == OK, "Text overflow document lays out")
	var source: String = layout.get_plain_text()
	var start: int = source.find("ÁBCDEFGHIJK")
	_check(start >= 0, "Elision preserves the original string and combining marks for copy and search")
	var tail: Array[Rect2] = layout.get_selection_rects(start + 5, start + 12)
	_check(tail.size() == 1 and tail[0].has_area(), "Selection of the elided tail highlights the ellipsis")
	if not tail.is_empty():
		_check(layout.get_link_at(tail[0].get_center()) == "#foo", "The ellipsis keeps the original link hit area")
	var badge: Rect2 = _rect(layout, "badge")
	_check(layout.get_link_at(badge.position + Vector2(2.0, 4.0)) == "#badge",
		"Eliding an atomic inline does not change its geometry or pointer target")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(180, 270)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var pairs: Array[Vector2i] = [
		Vector2i(0, 32),
		Vector2i(64, 96),
		Vector2i(128, 160),
	]
	for pair: Vector2i in pairs:
		var drawn: Image = image.get_region(Rect2i(0, pair.x, 160, 32))
		var reference: Image = image.get_region(Rect2i(0, pair.y, 160, 32))
		_check(drawn.get_data() == reference.get_data(), "Text ellipsis matches its reference row at %d" % pair.x)

	var scroll_cut: Rect2 = _label_rect(label, "scroll_cut")
	var scroll_ref: Rect2 = _label_rect(label, "scroll_ref")
	var cut_row: Rect2i = Rect2i(0, roundi(scroll_cut.position.y), 160, 32)
	var ref_row: Rect2i = Rect2i(0, roundi(scroll_ref.position.y), 160, 32)
	_check(image.get_region(cut_row).get_data() != image.get_region(ref_row).get_data(),
		"A scrollable line displays an ellipsis while its end is clipped")
	for _step: int in 4:
		_push_html_wheel(viewport, scroll_cut.get_center(), MOUSE_BUTTON_WHEEL_RIGHT)
		_push_html_wheel(viewport, scroll_ref.get_center(), MOUSE_BUTTON_WHEEL_RIGHT)
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check(image.get_region(cut_row).get_data() == image.get_region(ref_row).get_data(),
		"Scrolling to the inline end replaces the ellipsis with the actual text")
	viewport.queue_free()


func _check_overflow_axes() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var base: String = """<!doctype html><style>
		body { margin: 0; } .box { width: 25px; height: 25px; }
		.child { width: 110px; height: 110px; }
	</style><div class="box" style="%s"><div class="child" id="child"></div></div>"""
	for sample: Vector2i in [Vector2i(0, 1), Vector2i(1, 0)]:
		var declaration: String = "overflow-x: visible; overflow-y: clip" if sample.x else "overflow: clip visible"
		_check(layout.parse(base % declaration) == OK and layout.layout(Vector2i(60, 60)) == OK, "A mixed clip strip lays out")
		var child: HTMLElement = layout.get_element_by_id("child")
		var point: Vector2 = Vector2.ONE * 12.0 + Vector2(sample) * 28.0
		_check(layout.get_element_at(point) == child and
			layout.get_element_at(Vector2.ONE * 52.0 - Vector2(sample) * 36.0) != child,
			"A clip strip accepts hits beyond its visible axis and rejects them on its clipped axis")
		var extent: Vector2i = layout.get_content_size()
		_check(extent[sample.max_axis_index()] >= 110 and extent[sample.min_axis_index()] <= 60,
			"Only overflow on the visible axis extends the document")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(220, 220)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		.frame { position: absolute; width: 36px; height: 24px; }
		.fill { width: 120px; height: 58px; }
		#xclip { top: 10px; left: 12px; overflow: clip visible; padding: 6px;
			border: 2px solid black; border-radius: 20px; }
		#yclip { top: 10px; left: 100px; overflow-x: visible; overflow-y: clip; }
		#turn { top: 95px; left: 110px; overflow: clip visible;
			transform-origin: 0 0; transform: rotate(90deg); }
		#scroll { top: 158px; left: 12px; overflow: var(--axes); --axes: clip auto; }
		#auto { top: 158px; left: 90px; overflow-y: hidden; }
		.ellipsis { top: 196px; width: 4.2ch; height: 4px; color: white;
			font: 16px / 22px Consolas, monospace; white-space: nowrap; }
		#short { left: 12px; overflow: clip visible; text-overflow: ellipsis; }
		#short_ref { left: 86px; }
	</style>
	<div class="frame" id="xclip"><div class="fill" id="red" style="background: red"></div></div>
	<div class="frame" id="yclip"><div class="fill" id="green" style="background: lime"></div></div>
	<div class="frame" id="turn"><div class="fill" id="blue" style="background: blue"></div></div>
	<div class="frame" id="scroll"><div class="fill" id="down"></div></div>
	<div class="frame" id="auto"><div class="fill" id="right"></div></div>
	<div class="frame ellipsis" id="short">ABCDEFGHIJ</div>
	<div class="frame ellipsis" id="short_ref">ABC…</div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(24.0, 62.0), Color.RED, "X clipping leaves vertical paint visible")
	_check_pixel(image, Vector2(60.0, 25.0), Color.RED, "The overflow clip reaches the padding edge")
	_check_pixel(image, Vector2(63.0, 25.0), Color.BLACK, "X clipping cuts off paint beyond the padding edge")
	_check_pixel(image, Vector2(153.0, 26.0), Color.GREEN, "Y clipping leaves horizontal paint visible")
	_check_pixel(image, Vector2(112.0, 42.0), Color.BLACK, "Y clipping cuts off vertical paint")
	_check_pixel(image, Vector2(64.0, 107.0), Color.BLUE, "A strip clips in its rotated local coordinates")
	_check_pixel(image, Vector2(87.0, 146.0), Color.BLACK, "The rotated strip rejects paint across its clipped axis")
	_check(label.get_element_at(Vector2(64.0, 107.0)) == label.get_element_by_id("blue") and
		label.get_element_at(Vector2(87.0, 146.0)) != label.get_element_by_id("blue"),
		"Rotated strip hit tests agree with clipping")

	var ellipsis_line: Image = image.get_region(Rect2i(12, 196, 48, 22))
	var reference_line: Image = image.get_region(Rect2i(86, 196, 48, 22))
	var ink_count: int
	var mask_difference: int
	for y: int in ellipsis_line.get_height():
		for x: int in ellipsis_line.get_width():
			var painted: bool = ellipsis_line.get_pixel(x, y).r > 0.45
			if painted:
				ink_count += 1
			if painted != (reference_line.get_pixel(x, y).r > 0.45):
				mask_difference += 1
	# The ellipsis is shaped separately, so its antialiasing may round differently.
	_check(ink_count > 100 and mask_difference <= 8, "An ellipsis and its retained text paint beyond the visible Y edge")

	var down: HTMLElement = label.get_element_by_id("down")
	var right: HTMLElement = label.get_element_by_id("right")
	var down_before: Vector2 = down.get_bounding_rect().position
	var right_before: Vector2 = right.get_bounding_rect().position
	for wheel: MouseButton in [MOUSE_BUTTON_WHEEL_DOWN, MOUSE_BUTTON_WHEEL_RIGHT]:
		_push_html_wheel(viewport, Vector2(24.0, 170.0), wheel)
		_push_html_wheel(viewport, Vector2(102.0, 170.0), wheel)
	var down_shift: Vector2 = down_before - down.get_bounding_rect().position
	var right_shift: Vector2 = right_before - right.get_bounding_rect().position
	_check(is_zero_approx(down_shift.x) and down_shift.y > 0.0,
		"A clip axis computes to hidden when paired with auto and ignores horizontal wheel input")
	_check(right_shift.x > 0.0 and is_zero_approx(right_shift.y),
		"A visible axis computes to auto when paired with hidden and consumes horizontal wheel input")
	viewport.queue_free()


func _check_overflow_clip_margin() -> void:
	var html: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.frame { position: absolute; width: 30px; height: 24px; overflow: clip; font-size: 10px;
			padding: 6px; border: 4px solid transparent; }
		.child { width: 66px; height: 53px; background: red; }
		#content { top: 16px; left: 16px; overflow-clip-margin: content-box 3px;
			overflow-clip-margin: content-box -2px; }
		#padding { top: 16px; left: 100px; --edge: 0.6em padding-box; overflow-clip-margin: var(--edge);
			overflow-clip-margin: calc(3% + 1px); }
		#round { top: 16px; left: 184px; border-radius: 14px; overflow-clip-margin: border-box 6px; }
		#strip { top: 90px; left: 16px; overflow-clip-margin: padding-box 5px; border-radius: 20px; }
		@supports (overflow-inline: clip) and (overflow: overlay) {
			#strip { overflow-inline: clip; overflow-block: visible; }
		}
		#hidden { top: 90px; left: 100px; overflow: hidden; overflow-clip-margin: border-box 12px; }
	</style>
	<div class="frame" id="content"><div class="child" id="content_child"></div></div>
	<div class="frame" id="padding"><div class="child"></div></div>
	<div class="frame" id="round"><div class="child" id="rounded_child"></div></div>
	<div class="frame" id="strip"><div class="child" id="strip_child"></div></div>
	<div class="frame" id="hidden"><div class="child"></div></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(html) == OK and layout.layout(Vector2i(260, 170)) == OK, "Overflow clip margins lay out")
	_check(_has_diagnostic(layout, "overflow-clip-margin", "content-box -2px", "style_element") and
		_has_diagnostic(layout, "overflow-clip-margin", "calc(3% + 1px)", "style_element"),
		"Clip margins reject negative lengths and percentage expressions")
	var content_child: HTMLElement = layout.get_element_by_id("content_child")
	var rounded_child: HTMLElement = layout.get_element_by_id("rounded_child")
	var strip_child: HTMLElement = layout.get_element_by_id("strip_child")
	_check(layout.get_element_at(Vector2(58.0, 38.0)) == content_child and
		layout.get_element_at(Vector2(60.0, 38.0)) != content_child,
		"The content-box clip margin clips hits at its expanded edge")
	_check(layout.get_element_at(Vector2(236.0, 37.0)) == rounded_child and
		layout.get_element_at(Vector2(237.0, 64.0)) != rounded_child,
		"Expanded rounded corners clip pointer hits with the same shape as paint")
	_check(layout.get_element_at(Vector2(65.0, 149.0)) == strip_child and
		layout.get_element_at(Vector2(68.0, 149.0)) != strip_child,
		"The logical overflow strip applies clip margins on the inline axis")

	for display: String in ["block", "flow-root"]:
		_check(layout.parse("""<!doctype html><style>
			body { margin: 0; }
			#container { display: %s; width: 50px; overflow: clip; }
			#float { float: left; width: 20px; height: 28px; }
		</style><div id="container"><div id="float"></div></div>""" % display) == OK and
			layout.layout(Vector2i(120, 80)) == OK, "A clipped float lays out")
		var height: float = _rect(layout, "container").size.y
		_check(is_equal_approx(height, 28.0 if display == "flow-root" else 0.0),
			"Only flow-root contains the float, display=%s height=%s" % [display, height])

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(260, 170)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(58.0, 38.0), Color.RED, "Content-box clip margin paints up to its edge")
	_check_pixel(image, Vector2(60.0, 38.0), Color.BLACK, "Content-box clip margin cuts off overflow past its edge")
	_check_pixel(image, Vector2(149.0, 38.0), Color.RED, "Variable padding-box margin accepts a font-relative length")
	_check_pixel(image, Vector2(153.0, 38.0), Color.BLACK, "Padding-box margin stops before further ink")
	_check_pixel(image, Vector2(236.0, 37.0), Color.RED, "Border-box clip margin shows ink beyond the border")
	_check_pixel(image, Vector2(237.0, 64.0), Color.BLACK, "The expanded rounded corner excludes overflow")
	_check_pixel(image, Vector2(65.0, 149.0), Color.RED, "One-axis clip margin preserves vertical ink")
	_check_pixel(image, Vector2(68.0, 149.0), Color.BLACK, "One-axis clip margin cuts off horizontal ink")
	_check_pixel(image, Vector2(148.0, 111.0), Color.BLACK, "Overflow hidden uses the original padding box")
	viewport.queue_free()


func _check_overflow_scrollbars() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 288)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(320.0, 178.0)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		.box { position: absolute; background: #222222; }
		#vertical { top: 8px; left: 8px; width: 84px; height: 60px; overflow: hidden auto; }
		#vertical a { display: block; height: 180px; background: lime; }
		#cover { display: none; top: 8px; left: 84px; width: 8px; height: 60px; z-index: 1; }
		#spin { top: 8px; left: 200px; width: 68px; height: 50px; overflow: hidden auto;
			transform-origin: 0 0; transform: rotate(90deg); }
		#spin div { height: 150px; background: teal; }
		#fixed { top: 8px; left: 224px; width: 56px; height: 50px; overflow: hidden scroll; }
		#fits { top: 72px; left: 224px; width: 56px; height: 50px; overflow: hidden auto; }
		#outer { top: 86px; left: 8px; width: 84px; height: 60px; overflow: hidden auto; }
		#inner { width: 55px; height: 45px; overflow: hidden auto; background: #556677; }
		#inner div { height: 150px; }
		#oversize { height: 130px; }
		#horizontal { top: 94px; left: 106px; width: 94px; height: 48px; overflow: auto hidden; }
		#horizontal div { width: 218px; height: 44px; background: #2a3750; }
	</style>
	<div class="box" id="vertical"><a href="#wrong" id="link"></a></div>
	<div class="box" id="cover"></div>
	<div class="box" id="spin"><div id="rotated_child"></div></div>
	<div class="box" id="fixed"></div><div class="box" id="fits"></div>
	<div class="box" id="outer"><div id="inner"><div id="inner_child"></div></div><div id="oversize"></div></div>
	<div class="box" id="horizontal"><div id="wide"></div></div>"""
	viewport.add_child(label)
	var links: Array[String] = []
	label.link_pressed.connect(func(uri: String) -> void: links.append(uri))

	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.parse_html = true
	markdown.position = Vector2(0.0, 180.0)
	markdown.size = Vector2(260.0, 96.0)
	var test_texture: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(icon, test_texture)
	markdown.text = """<div style="background: #244c30; width: 70px; height: 48px; overflow: auto">
		<div style="height: 72px;"></div><div style="height: 100px; background: #ee3061;"></div>
	</div>"""
	viewport.add_child(markdown)

	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check(not image.get_pixel(275, 20).is_equal_approx(Color("#222222")) and
		image.get_pixel(275, 88).is_equal_approx(Color("#222222")),
		"Forced scroll paints a track when content fits and auto leaves it hidden")

	var link: HTMLElement = label.get_element_by_id("link")
	var start_y: float = link.get_bounding_rect().position.y
	_drag_pointer(viewport, Vector2(88.0, 13.0), Vector2(88.0, 76.0))
	_check(is_equal_approx(start_y - link.get_bounding_rect().position.y, 120.0),
		"Dragging the vertical thumb reaches the end of the inner scroll range")
	_drag_pointer(viewport, Vector2(88.0, 10.0), Vector2(88.0, 10.0))
	_check(is_equal_approx(start_y, link.get_bounding_rect().position.y) and links.is_empty(),
		"Clicking near the beginning of the track scrolls back without activating the link")

	var rotated: HTMLElement = label.get_element_by_id("rotated_child")
	var rotated_x: float = rotated.get_bounding_rect().position.x
	_drag_pointer(viewport, Vector2(195.0, 72.0), Vector2(143.0, 72.0))
	_check(is_equal_approx(rotated.get_bounding_rect().position.x - rotated_x, 100.0),
		"The rotated thumb maps pointer movement through the inverse transform")

	var inner_child: HTMLElement = label.get_element_by_id("inner_child")
	var inner_y: float = inner_child.get_bounding_rect().position.y
	_drag_pointer(viewport, Vector2(58.0, 91.0), Vector2(58.0, 134.0))
	_check(is_equal_approx(inner_y - inner_child.get_bounding_rect().position.y, 105.0) and
		is_equal_approx(label.get_element_by_id("inner").get_bounding_rect().position.y, 86.0),
		"Dragging a nested scroller moves the inner box contents independently")
	var wide: HTMLElement = label.get_element_by_id("wide")
	var wide_x: float = wide.get_bounding_rect().position.x
	_drag_pointer(viewport, Vector2(114.0, 138.0), Vector2(206.0, 138.0))
	_check(is_equal_approx(wide_x - wide.get_bounding_rect().position.x, 124.0),
		"The horizontal scrollbar moves content only along X")

	label.get_element_by_id("cover").set_attribute("style", "display: block")
	await process_frame
	await process_frame
	_drag_pointer(viewport, Vector2(88.0, 13.0), Vector2(88.0, 76.0))
	_check(is_equal_approx(start_y, link.get_bounding_rect().position.y),
		"An overlapping box painted above a scrollbar receives pointer input")

	var markdown_origin: Vector2i = Vector2i(-1, -1)
	for y: int in range(180, 276):
		for x: int in 170:
			if image.get_pixel(x, y).is_equal_approx(Color("#244c30")):
				markdown_origin = Vector2i(x, y)
				break
		if markdown_origin.x >= 0:
			break
	_check(markdown_origin.x >= 0, "Markdown HTML paints the scroll box")
	if markdown_origin.x >= 0:
		var thumb: Vector2 = Vector2(markdown_origin + Vector2i(66, 4))
		_drag_pointer(viewport, thumb, thumb + Vector2(0.0, 42.0))
		await RenderingServer.frame_post_draw
		_check_pixel(viewport.get_texture().get_image(), Vector2(markdown_origin + Vector2i(6, 8)),
			Color("#ee3061"), "Markdown HTML scrollbar dragging reveals the end of the box")
	viewport.queue_free()


func _check_scroll_insets() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(220, 160)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(220.0, 160.0)
	label.css_diagnostics_enabled = true
	label.text = """<!doctype html><style>
		@layer base { html { scroll-padding-block: 12px 10%; } }
		@layer override { html { scroll-padding-top: var(--rollback, revert-layer); } }
		html { direction: var(--flow, rtl); scrollbar-width: none;
			scroll-padding-top: -1px; scroll-padding-left: 25px; scroll-padding-inline-end: 24px; }
		body { margin: 0; width: 520px; height: 780px; }
		#target { position: absolute; top: 280px; left: 270px; width: 32px; height: 20px;
			transform: translate(2px, 4px); scroll-margin: 5px;
			scroll-margin-block: var(--outset); --outset: 9px -5px;
			scroll-margin-inline-end: 6px; scroll-margin-left: -4px;
			scroll-margin-bottom: 10%; }
		#fixed { position: fixed; top: 6px; right: 5px; width: 16px; height: 14px; }
	</style><a id="jump" href="#target">Jump</a>
	<div id="target" tabindex="0"></div><div id="fixed"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var target: HTMLElement = label.get_element_by_id("target")
	var initial: Rect2 = target.get_bounding_rect()
	_check(label.activate_element(label.get_element_by_id("jump")), "Activate a link to a CSS scroll target")
	await process_frame
	var expected: Vector2i = Vector2i(initial.position + Vector2(-20.0, -21.0))
	_check(label.get_scroll_offset() == expected, "Scroll margins and viewport padding set anchor offsets to %s" % expected)
	_check(target.get_bounding_rect().is_equal_approx(initial), "Scroll insets leave document-space element bounds unchanged")
	_check(label.scroll_to_element(target, 0.5), "Align a transformed element in the optimal viewing region")
	await process_frame
	var offset: Vector2i = label.get_scroll_offset()
	_check(offset.y == expected.y - 66, "Percentage scroll padding reduces the available aligned height")
	_check(label.scroll_to_anchor("fixed") and label.get_scroll_offset() == offset,
		"Navigating to a viewport-fixed target preserves the viewport offset")
	var document_root: HTMLElement = label.query_selector("html")
	document_root.set_attribute("style", "direction: ltr")
	await process_frame
	await process_frame
	label.set_scroll_offset(Vector2i.ZERO)
	_check(label.scroll_to_anchor("target") and label.get_scroll_offset().x == expected.x - 1,
		"Changing direction remaps the logical edge and exposes the physical left inset")
	var invalid: Dictionary[String, String] = {
		"scroll-padding-top": "-1px",
		"scroll-margin-bottom": "10%",
	}
	for diagnostic: Dictionary in label.get_css_diagnostics():
		if diagnostic["kind"] == "unsupported_value" and invalid.get(diagnostic["property"]) == diagnostic["value"]:
			invalid.erase(diagnostic["property"])
	_check(invalid.is_empty(), "Reject direct negative padding and percentage scroll margins")

	label.text = """<!doctype html><style>
		html { scrollbar-width: none; scroll-padding-top: 7px; scroll-padding-bottom: 6px; }
		body { margin: 0; height: 850px; }
		#outer { margin-top: 160px; width: 150px; height: 110px; overflow: hidden;
			scroll-padding: 11px 0 8px; transform: scale(.75); transform-origin: left top; }
		#inner { width: 140px; height: 70px; overflow: auto; scrollbar-width: none;
			scroll-padding: 20% 0 10px; }
		#nested { appearance: none; padding: 0; border: 0; display: block; width: 32px; height: 20px;
			transform: translateY(4px); scroll-margin: 4px 0 2px; }
	</style>
	<div id="outer"><div style="height: 140px"></div><div id="inner">
		<div style="height: 180px"></div><button type="button" id="nested">Focus</button><div style="height: 180px"></div>
	</div><div style="height: 170px"></div></div>"""
	await process_frame
	await process_frame
	var nested: HTMLElement = label.get_element_by_id("nested")
	_check(label.focus_element(nested), "A clipped button can receive keyboard focus in a nested scroller")
	await process_frame
	await process_frame
	_check(label.get_focused_element() == nested and
		label.get_element_at(nested.get_bounding_rect().get_center() - Vector2(label.get_scroll_offset())) == nested,
		"Keyboard focus scrolls both inner containers until the transformed target is hittable")
	_check(label.scroll_to_anchor("nested"), "Scroll the same nested target to the leading edge")
	await process_frame
	var outer: Rect2 = _label_rect(label, "outer")
	var inner: Rect2 = _label_rect(label, "inner")
	var box: Rect2 = nested.get_bounding_rect()
	_check(is_equal_approx(box.position.y - inner.position.y, 13.5) and
		is_equal_approx(box.position.y - outer.position.y, 11.25) and
		absf(box.position.y - label.get_scroll_offset().y - 11.0) < 1.0,
		"The nested scrollports apply insets before mapping bounds into the HTML viewport")
	viewport.queue_free()

	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.size = Vector2(220.0, 120.0)
	markdown.parse_html = true
	var icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for name: StringName in [
		&"task_checked",
		&"task_unchecked",
		&"file_broken",
	]:
		markdown.add_theme_icon_override(name, icon)
	root.add_child(markdown)
	var source: String = """<div style="height:210px"></div>
<div id="css-anchor" style="scroll-margin-top:%spx; height:24px">Target</div>
<div style="height:360px"></div>
"""
	markdown.text = source % 0
	await process_frame
	await process_frame
	_check(markdown.scroll_to_anchor("css-anchor"), "Find a Markdown HTML anchor")
	var previous: float = markdown.get_v_scroll_bar().value
	markdown.text = source % 15
	await process_frame
	await process_frame
	_check(markdown.scroll_to_anchor("css-anchor") and is_equal_approx(markdown.get_v_scroll_bar().value, previous - 15.0),
		"HTML scroll margin adjusts the Markdown anchor's position")
	markdown.queue_free()


func _check_overscroll_behavior() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(390, 220)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var outer: ScrollContainer = ScrollContainer.new()
	outer.size = Vector2(390.0, 220.0)
	viewport.add_child(outer)
	var content: Control = Control.new()
	content.custom_minimum_size = Vector2(700.0, 530.0)
	outer.add_child(content)
	var html: HTMLLabel = HTMLLabel.new()
	html.size = Vector2(370.0, 180.0)
	html.text = """<!doctype html><style>
		html { scrollbar-width: none; }
		body { margin: 0; width: 560px; height: 510px; }
		.port { position: absolute; top: 15px; width: 46px; height: 50px; overflow: auto; scrollbar-width: none; }
		.fill { width: 56px; height: 60px; background: green; }
		#contained { left: 8px; overscroll-behavior: var(--edges); --edges: auto contain; }
		#default { left: 66px; overscroll-behavior: chain; }
		#empty { left: 124px; overscroll-behavior: contain;
			overscroll-behavior: auto invalid; background: green; }
		#hidden { left: 182px; overflow: hidden; overscroll-behavior: contain; }
		#clip { left: 240px; overflow: clip; overscroll-behavior: none; }
		#both { left: 298px; overscroll-behavior: auto; overscroll-behavior-inline: none; }
		#field { position: absolute; top: 90px; left: 8px; height: 34px; width: 100px; resize: none;
			overflow: auto; overscroll-behavior-block: contain; }
	</style>
	<div class="port" id="contained"><div class="fill" id="moving"></div></div>
	<div class="port" id="default"><div class="fill"></div></div><div class="port" id="empty"></div>
	<div class="port" id="hidden"><div class="fill"></div></div><div class="port" id="clip"><div class="fill"></div></div>
	<div class="port" id="both"><div class="fill" id="diagonal"></div></div><textarea id="field">Short</textarea>"""
	content.add_child(html)
	await process_frame
	await process_frame
	var start: float = _label_rect(html, "moving").position.y
	_push_html_wheel(viewport, Vector2(20.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(is_equal_approx(start - _label_rect(html, "moving").position.y, 10.0) and html.get_scroll_offset() == Vector2i.ZERO,
		"Containment lets the inner scroller reach its boundary and absorbs its residual wheel delta")
	_push_html_wheel(viewport, Vector2(20.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, Vector2(134.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, Vector2(194.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, Vector2(20.0, 102.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(html.get_scroll_offset() == Vector2i.ZERO and outer.scroll_vertical == 0,
		"An exhausted scroller, a zero-range scroller, overflow hidden, and textarea contain the wheel")
	_push_html_wheel(viewport, Vector2(82.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(html.get_scroll_offset().y > 0, "Chain forwards residual wheel distance after an inner scroller moves")
	html.set_scroll_offset(Vector2i.ZERO)
	await process_frame
	_push_html_wheel(viewport, Vector2(254.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(html.get_scroll_offset().y > 0, "Overflow clip leaves the ancestor scroller in the chain")
	html.set_scroll_offset(Vector2i.ZERO)
	await process_frame
	var diagonal_start: Vector2 = _label_rect(html, "diagonal").position
	var pan: InputEventPanGesture = InputEventPanGesture.new()
	pan.position = Vector2(310.0, 26.0)
	pan.delta = Vector2(2.0, 2.0)
	viewport.push_input(pan, true)
	_check(diagonal_start - _label_rect(html, "diagonal").position == Vector2(10.0, 10.0) and
		html.get_scroll_offset().x == 0 and html.get_scroll_offset().y > 0,
		"A diagonal pan chains only the uncontained axis")

	html.text = """<!doctype html><style>
		html { scrollbar-width: none; overscroll-behavior: auto none; }
		body { margin: 0; height: 500px; overscroll-behavior: none; }
	</style><div style="height: 500px"></div>"""
	await process_frame
	await process_frame
	var html_root: HTMLElement = html.query_selector("html")
	_push_html_wheel(viewport, Vector2(30.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(html.get_scroll_offset().y > 0, "Viewport containment preserves local wheel scrolling")
	html.set_scroll_offset(Vector2i(0, 900))
	await process_frame
	await process_frame
	_push_html_wheel(viewport, Vector2(30.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(outer.scroll_vertical == 0, "The HTML root stops scroll chaining to a Godot ancestor")
	html_root.set_attribute("style", "overscroll-behavior-block: auto")
	await process_frame
	await process_frame
	_push_html_wheel(viewport, Vector2(30.0, 26.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(outer.scroll_vertical > 0, "The body's containment is local and does not propagate to the viewport")
	viewport.queue_free()

	var markdown_view: SubViewport = SubViewport.new()
	markdown_view.size = Vector2i(220, 120)
	markdown_view.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(markdown_view)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.size = Vector2(220.0, 120.0)
	markdown.parse_html = true
	var icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for name: StringName in [
		&"task_checked",
		&"task_unchecked",
		&"file_broken",
	]:
		markdown.add_theme_icon_override(name, icon)
	markdown_view.add_child(markdown)
	var markdown_source: String = """<div style="width:120px;height:54px;overflow:auto;overscroll-behavior:%s;
		background:#b5423a;scrollbar-width:none;"><div style="height:66px"></div></div>

<div style="height:420px"></div>
"""
	var policies: Array[String] = [
		"contain",
		"auto",
	]
	for policy: String in policies:
		markdown.text = markdown_source % policy
		markdown.get_v_scroll_bar().value = 0.0
		await process_frame
		await process_frame
		await RenderingServer.frame_post_draw
		var image: Image = markdown_view.get_texture().get_image()
		var box: Rect2i = _color_bounds(image, Rect2i(Vector2i.ZERO, markdown_view.size), Color("#b5423a"))
		_check(box.has_area(), "Locate the Markdown HTML scrollport")
		if box.has_area():
			_push_html_wheel(markdown_view, Vector2(box.get_center()), MOUSE_BUTTON_WHEEL_DOWN)
			_check((markdown.get_v_scroll_bar().value > 0.0) == (policy == "auto"),
				"The Markdown host observes the HTML scrollport's %s policy" % policy)
	markdown_view.queue_free()


func _check_smooth_scroll_behavior() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(220, 160)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(220.0, 160.0)
	label.text = """<!doctype html><style>
		html { scrollbar-width: none; scroll-padding-top: 15px; scroll-behavior: var(--behavior, smooth); }
		body { margin: 0; height: 1300px; }
		.target { position: absolute; left: 10px; width: 30px; height: 20px; scroll-margin-top: 5px; }
	</style><a id="jump" href="#first">Jump</a>
	<div class="target" id="first" style="top: 360px"></div>
	<div class="target" id="second" style="top: 850px"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var first: HTMLElement = label.get_element_by_id("first")
	var second: HTMLElement = label.get_element_by_id("second")
	var destination: int = roundi(first.get_bounding_rect().position.y - 20.0)
	_check(label.activate_element(label.get_element_by_id("jump")) and label.get_scroll_offset().y == 0,
		"Root smooth behavior schedules a fragment scroll")
	var sampled_intermediate: bool
	var end: int = Time.get_ticks_msec() + 600
	while label.get_scroll_offset().y != destination and Time.get_ticks_msec() < end:
		await process_frame
		var offset: int = label.get_scroll_offset().y
		sampled_intermediate = sampled_intermediate or (offset > 0 and offset < destination)
	_check(sampled_intermediate and label.get_scroll_offset().y == destination,
		"Smooth navigation interpolates and ends at the exact padded and margined destination")
	_check(label.scroll_to_element(second), "Start another smooth viewport scroll")
	await create_timer(0.06).timeout
	_push_html_wheel(viewport, Vector2(42.0, 24.0), MOUSE_BUTTON_WHEEL_UP)
	var interrupted: Vector2i = label.get_scroll_offset()
	await create_timer(0.3).timeout
	_check(label.get_scroll_offset() == interrupted and interrupted.y < roundi(second.get_bounding_rect().position.y),
		"An ordinary wheel scroll interrupts the viewport animation")
	_check(label.scroll_to_element(second) and label.scroll_to_element(first), "Retarget the same scroll container")
	await create_timer(0.3).timeout
	_check(label.get_scroll_offset().y == destination, "A newer scroll request replaces the previous destination")
	_check(label.scroll_to_element(second), "Start an animation before setting an exact control offset")
	label.set_scroll_offset(Vector2i(0, 55))
	await create_timer(0.3).timeout
	_check(label.get_scroll_offset() == Vector2i(0, 55), "Setting the exact offset cancels CSS navigation motion")

	label.text = """<!doctype html><style>
		html { scrollbar-width: none; scroll-padding-top: 8px; }
		body { margin: 0; height: 800px; scroll-behavior: smooth; }
		#inner { margin-top: 180px; width: 140px; height: 80px; overflow: hidden;
			scroll-padding-top: 16px; scroll-behavior: smooth; }
		#nested { height: 20px; scroll-margin-top: 4px; }
	</style><div id="inner"><div style="height: 170px"></div><div id="nested">Target</div>
	<div style="height: 200px"></div></div>"""
	await process_frame
	await process_frame
	var inner: HTMLElement = label.get_element_by_id("inner")
	var nested: HTMLElement = label.get_element_by_id("nested")
	var original: float = nested.get_bounding_rect().position.y - inner.get_bounding_rect().position.y
	_check(label.scroll_to_anchor("nested") and label.get_scroll_offset().y > 0 and
		is_equal_approx(nested.get_bounding_rect().position.y - inner.get_bounding_rect().position.y, original),
		"The viewport and nested scroll box honor their own computed behavior")
	await create_timer(0.3).timeout
	var target_top: float = nested.get_bounding_rect().position.y
	_check(is_equal_approx(target_top - inner.get_bounding_rect().position.y, 20.0) and
		is_equal_approx(target_top - label.get_scroll_offset().y, 12.0),
		"The viewport destination uses the inner animation's final geometry")
	viewport.queue_free()

	var markdown_view: SubViewport = SubViewport.new()
	markdown_view.size = Vector2i(220, 120)
	markdown_view.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(markdown_view)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.size = Vector2(220.0, 120.0)
	markdown.parse_html = true
	var icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for name: StringName in [
		&"task_checked",
		&"task_unchecked",
		&"file_broken",
	]:
		markdown.add_theme_icon_override(name, icon)
	markdown.text = """<div style="width:120px;height:54px;overflow:hidden;scroll-behavior:smooth;">
		<div style="height:90px;"></div><div id="inside" style="height:24px;background:#db462e;">Aim</div><div style="height:90px;"></div></div>"""
	markdown_view.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var visible: Rect2i = Rect2i(Vector2i.ZERO, markdown_view.size)
	_check(_color_bounds(markdown_view.get_texture().get_image(), visible, Color("#db462e")).size == Vector2i.ZERO and
		markdown.scroll_to_anchor("inside"), "Locate a smooth scroll target inside a clipped Markdown HTML block")
	await create_timer(0.3).timeout
	await RenderingServer.frame_post_draw
	_check(_color_bounds(markdown_view.get_texture().get_image(), visible, Color("#db462e")).has_area(),
		"Markdown paints the anchor after the nested smooth scroll finishes")
	markdown_view.queue_free()


func _check_scroll_snap() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(230, 165)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(230.0, 160.0)
	label.text = """<!doctype html><style>
		html { scrollbar-width: none; scroll-snap-type: var(--axis, block mandatory); scroll-padding-top: 10px; }
		body { width: 220px; height: 900px; margin: 0; }
		.tile { position: absolute; left: 0; width: 100px; height: 30px; scroll-snap-align: start none; }
		#one { top: 0; }
		#stop { top: 120px; scroll-snap-stop: always; scroll-margin-top: 5px; }
		#three { top: 260px; scroll-snap-align: center none; }
		#last { top: 870px; }
		#outside { left: 900px; top: 70px; }
	</style><div class="tile" id="one"></div><div class="tile" id="stop"></div>
	<div class="tile" id="three"></div><div class="tile" id="last"></div><div class="tile" id="outside"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	_push_html_wheel(viewport, Vector2(14.0, 18.0), MOUSE_BUTTON_WHEEL_DOWN, 14.0)
	_check(label.get_scroll_offset() == Vector2i(0, 105),
		"A directional scroll stops at the first crossed stop, with padding and margin")
	await process_frame
	_push_html_wheel(viewport, Vector2(14.0, 18.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(label.get_scroll_offset() == Vector2i(0, 190),
		"The next wheel step leaves the starting snap point and selects the next visible point")
	label.set_scroll_offset(Vector2i(0, 0))
	_check(label.scroll_to_anchor("three", 1.0) and label.get_scroll_offset() == Vector2i(0, 190),
		"Fragment navigation prefers its target's snap area and ignores intervening always stops")
	var third: HTMLElement = label.get_element_by_id("three")
	third.set_attribute("style", "top:340px")
	await process_frame
	await process_frame
	_check(label.get_scroll_offset() == Vector2i(0, 270), "Relayout keeps the same snapped element as its position changes")
	_check(label.scroll_to_anchor("last") and label.get_scroll_offset().y == 740,
		"An unreachable alignment snaps as far as the scroll range allows")
	label.set_scroll_offset(Vector2i.ZERO)
	await process_frame
	var pan: InputEventPanGesture = InputEventPanGesture.new()
	pan.position = Vector2(14.0, 18.0)
	pan.delta = Vector2(0.0, 4.0)
	viewport.push_input(pan, true)
	_check(label.get_scroll_offset().y == 80, "Touchpad panning moves continuously before settling")
	await create_timer(0.18).timeout
	_check(label.get_scroll_offset().y == 105, "The viewport snaps after the pan gesture is idle")

	label.text = """<!doctype html><style>
		html { scrollbar-width: none; scroll-snap-type: y mandatory; }
		body { margin: 0; height: 1050px; }
		div { position: absolute; width: 100px; scroll-snap-align: start; }
	</style><div style="top:0;height:30px"></div><div style="top:220px;height:410px"></div>
	<div style="top:800px;height:40px"></div>"""
	await process_frame
	await process_frame
	label.set_scroll_offset(Vector2i(0, 360))
	_check(label.get_scroll_offset() == Vector2i(0, 360), "An oversized snap area lets the user scroll within its contents")

	label.text = """<!doctype html><style>
		body { margin: 0; }
		#port { position: absolute; left: 10px; top: 8px; width: 100px; height: 110px; overflow: auto;
			scroll-snap-type: inline mandatory; scroll-padding-left: 8px; scrollbar-width: none; direction: rtl; }
		.card { position: absolute; width: 80px; height: 65px; scroll-snap-align: none end; }
		#outer { position: absolute; left: 150px; top: 8px; width: 70px; height: 110px; overflow: auto;
			scroll-snap-type: y proximity; }
		.marker { width: 50px; height: 36px; scroll-snap-align: start; }
	</style><div id="port"><div style="position:relative;width:430px;height:80px">
	<div class="card" style="left:0"></div><div class="card" id="side" style="left:130px; transform:translateX(7px);scroll-margin-left:4px"></div>
	<div class="card" style="left:265px"></div>
	</div></div><div id="outer">
	<div class="marker" id="marker"></div><div style="height:88px"></div><div class="marker"></div>
	<div style="height:135px"></div><div class="marker"></div><div style="height:89px"></div></div>"""
	await process_frame
	await process_frame
	var side: HTMLElement = label.get_element_by_id("side")
	_push_html_wheel(viewport, Vector2(35.0, 28.0), MOUSE_BUTTON_WHEEL_RIGHT)
	_check(is_equal_approx(side.get_bounding_rect().position.x, 22.0),
		"Horizontal snapping resolves logical end in RTL and measures transformed snap areas")
	var marker: HTMLElement = label.get_element_by_id("marker")
	_push_html_wheel(viewport, Vector2(162.0, 24.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(is_equal_approx(marker.get_bounding_rect().position.y, -28.0), "Proximity allows a step far from the next snap point")
	_push_html_wheel(viewport, Vector2(162.0, 24.0), MOUSE_BUTTON_WHEEL_DOWN, 2.0)
	_check(is_equal_approx(marker.get_bounding_rect().position.y, -116.0), "Proximity snaps when the endpoint nears a target")
	await process_frame
	_drag_pointer(viewport, Vector2(216.0, 54.0), Vector2(216.0, 105.0))
	var snapped_y: float = marker.get_bounding_rect().position.y
	_check(is_equal_approx(snapped_y, -287.0),
		"Releasing an inner scrollbar snaps to the nearby endpoint, got %s" % snapped_y)
	pan.position = Vector2(162.0, 24.0)
	pan.delta = Vector2(0.0, -4.0)
	viewport.push_input(pan, true)
	_check(is_equal_approx(marker.get_bounding_rect().position.y, -143.0), "The inner scroller also pans continuously")
	await create_timer(0.18).timeout
	_check(is_equal_approx(marker.get_bounding_rect().position.y, -116.0), "An inner pan settles at the nearby snap point")

	label.text = """<!doctype html><style>
		html { scrollbar-width:none; scroll-snap-type:both mandatory; }
		body { margin:0; width:800px; height:800px; }
		.point { position:absolute; width:50px; height:50px; scroll-snap-align:start center; }
	</style><div class="point" style="left:0;top:0"></div><div class="point" id="diagonal_target" style="left:380px;top:370px"></div>"""
	await process_frame
	await process_frame
	var target: Rect2 = _label_rect(label, "diagonal_target")
	var scroller: ScrollContainer = label.get_node("_HTMLScroll") as ScrollContainer
	var expected: Vector2i = Vector2i(roundi(target.get_center().x - scroller.get_h_scroll_bar().page / 2.0), roundi(target.position.y))
	label.set_scroll_offset(Vector2i(285, 355))
	await process_frame
	await process_frame
	_check(label.get_scroll_offset() == expected, "Diagonal snap gives %s, expected %s for %s in %s with ranges %s" % [
		label.get_scroll_offset(), expected, target, label.get_content_size(),
		Vector2(scroller.get_h_scroll_bar().max_value, scroller.get_v_scroll_bar().max_value),
	])
	label.text = """<!doctype html><style>
		html { scrollbar-width: none; }
		body { width: 120px; height: 60px; margin: 0; }
	</style><div style="width: 60px; height: 30px"></div>"""
	label.set_scroll_offset(Vector2i(500, 500))
	await process_frame
	await process_frame
	_check(label.get_scroll_offset() == Vector2i.ZERO and
		scroller.get_h_scroll_bar().max_value <= scroller.get_h_scroll_bar().page and
		scroller.get_v_scroll_bar().max_value <= scroller.get_v_scroll_bar().page,
		"Replacing an overflowing document refreshes hidden scrollbar ranges before applying a queued offset")
	viewport.queue_free()

	var markdown_view: SubViewport = SubViewport.new()
	markdown_view.size = Vector2i(220, 120)
	markdown_view.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(markdown_view)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.size = Vector2(220.0, 120.0)
	markdown.parse_html = true
	var icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for name: StringName in [
		&"task_checked",
		&"task_unchecked",
		&"file_broken",
	]:
		markdown.add_theme_icon_override(name, icon)
	markdown.text = """<div style="height:48px;width:100px;overflow:auto;scroll-snap-type:y mandatory;scrollbar-width:none">
	<div style="height:56px;scroll-snap-align:start;background:#40a85c"></div>
	<div style="height:50px;scroll-snap-align:start;background:#df2042"></div>
	</div>"""
	markdown_view.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var region: Rect2i = Rect2i(Vector2i.ZERO, markdown_view.size)
	var page: Rect2i = _color_bounds(markdown_view.get_texture().get_image(), region, Color("#40a85c"))
	_check(page.has_area(), "Locate the first snap page in Markdown")
	if page.has_area():
		_push_html_wheel(markdown_view, Vector2(page.get_center()), MOUSE_BUTTON_WHEEL_DOWN)
		await RenderingServer.frame_post_draw
		var output: Image = markdown_view.get_texture().get_image()
		_check(_color_bounds(output, region, Color("#df2042")).size.y == page.size.y and
			_color_bounds(output, region, Color("#40a85c")).size.y == 0,
			"Markdown uses the internal HTML container's complete next snap page")
	markdown_view.queue_free()


func _check_flow_relative_edges() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """<!doctype html><style>
		body { margin: 0; }
		#arena { position: relative; width: 200px; height: 310px; }
		#probe {
			position: absolute; direction: var(--direction, ltr);
			inset-inline: 16px auto; inset-block: 10px auto;
			margin-inline: 7px 11px; margin-block: 3px 5px;
			width: 40px; height: 25px; padding-inline: 4px 6px; padding-block: 2px 8px;
			border-inline: solid 2px; border-inline-width: 1px 3px; border-block: 2px solid;
		}
		#priority {
			position: absolute; inset-block-start: 70px; left: 10px; width: 20px; height: 10px;
			margin-left: 9px; margin-inline-start: 16px;
			padding-left: 3px; padding-inline-start: var(--inline, 8px) !important; padding-left: 100px;
			padding-inline: 8px -1px;
		}
		@layer base, theme;
		@layer base { #layered { padding-left: 8px; border-left: 4px solid; } }
		@layer theme { #layered {
			padding-inline: var(--reset, revert-layer);
			border-inline-start-width: 7px; border-left-width: var(--reset, revert-layer);
		} }
		#layered { position: absolute; left: 0; inset-block: 100px auto; width: 20px; height: 10px; }
		#inheritance { position: absolute; left: 0; top: 170px; padding-left: 9px; padding-right: 2px; direction: ltr; }
		#child { direction: rtl; padding-inline-start: inherit; padding-left: inherit; width: 20px; height: 10px; }
		@supports (border-inline-end: 2px solid blue) and (inset-block: 20px 15px) {
			#child { margin-block-start: 1px; }
		}
	</style><div id="arena"><div id="probe"></div><div id="priority"></div><div id="layered"></div>
	<div id="inheritance"><div id="child"></div></div></div>"""
	var size: Vector2i = Vector2i(240, 340)
	_check(layout.parse(html) == OK and layout.layout(size) == OK, "Lay out physical and flow-relative declarations")
	_check(_rect(layout, "probe") == Rect2(23.0, 13.0, 54.0, 39.0), "Logical side pairs use LTR inline start and end")
	var probe: HTMLElement = layout.get_element_by_id("probe")
	probe.set_attribute("style", "--direction: rtl")
	_check(layout.layout(size) == OK and _rect(layout, "probe") == Rect2(123.0, 13.0, 54.0, 39.0),
		"Changing direction through var() remaps inline insets, margins and padding")
	_check(_rect(layout, "priority") == Rect2(26.0, 70.0, 28.0, 10.0),
		"Logical and physical declarations respect source order and !important")
	_check(_has_diagnostic(layout, "padding-inline", "8px -1px", "style_element"), "An invalid logical pair reports the complete declaration")
	_check(_rect(layout, "layered").size.x == 32.0, "Variable rollback finds the prior physical edge in the same logical group")
	_check(_rect(layout, "child").size.x == 38.0, "Logical inheritance mirrors across direction and physical inheritance keeps its side")
	_check(_rect(layout, "child").position.y == 171.0, "@supports uses the same grammar for logical border and inset shorthands")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(150, 70)
	viewport.transparent_bg = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; }
		#frame {
			position: absolute; top: 10px; left: 10px; width: 40px; height: 40px; direction: rtl;
			border-inline: 4px solid; border-inline-color: #3366ff #ff9933;
			border-block: 3px solid lime; border-block-end-width: 5px;
		}
		#corner {
			position: absolute; top: 10px; left: 90px; width: 44px; height: 44px;
			direction: rtl; background: red; border-radius: 0;
			border-start-start-radius: calc(12px + 8px) 20px; border-end-end-radius: 20px;
		}
	</style><div id="frame"></div><div id="corner"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(11.0, 28.0), Color("#ff9933"), "The RTL inline-end border paints on the left")
	_check_pixel(image, Vector2(55.0, 28.0), Color("#3366ff"), "The RTL inline-start border paints on the right")
	_check_pixel(image, Vector2(32.0, 11.0), Color.GREEN, "A logical block border paints the top edge")
	_check_pixel(image, Vector2(91.0, 11.0), Color.RED, "The top-left corner stays square in RTL")
	_check(image.get_pixel(132, 11).a < 0.1 and image.get_pixel(91, 52).a < 0.1,
		"Logical corner radii round the RTL top-right and bottom-left corners")
	viewport.queue_free()


func _check_logical_floats() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><style>
		body { margin: 0; direction: ltr; }
		.zone { display: flow-root; width: 120px; height: 85px; }
		#logical { direction: var(--flow, rtl); }
		#start { float: var(--start-edge, inline-start); width: 20px; height: 25px; direction: ltr; }
		#end { float: inline-end; width: 22px; height: 60px; direction: ltr; }
		.clear { width: 15px; height: 5px; direction: ltr; }
		@supports (clear: inline-start) and (float: inline-end) {
			#after_start { clear: inline-start; }
			#after_end { clear: inline-end; }
		}
		#holder { float: inline-start; width: 80px; height: 55px; direction: rtl; }
		#inherited { float: inherit; width: 20px; height: 12px; direction: ltr; }
		#line { direction: rtl; font-size: 14px; line-height: 18px; }
		#wrapped_float { float: inline-start; width: 18px; height: 45px; }
	</style>
	<section class="zone" id="logical">
		<div id="start"></div><div id="end"></div>
		<div class="clear" id="after_start"></div><div class="clear" id="after_end"></div>
	</section>
	<section class="zone" id="mixed"><div id="holder"><div id="inherited"></div></div></section>
	<section class="zone" id="line"><span dir="ltr">
		<span id="wrapped_float"></span>First<br id="clear_break" style="clear: inline-start"><span id="after_break">After</span>
	</span></section>"""
	var size: Vector2i = Vector2i(180, 290)
	_check(layout.parse(html) == OK and layout.layout(size) == OK, "Lay out logical float and clear values")
	_check(_rect(layout, "start").position.x == 100.0 and _rect(layout, "end").position.x == 0.0,
		"RTL containing blocks map inline-start to the right and inline-end to the left")
	_check(_rect(layout, "after_start").position.y == 25.0 and _rect(layout, "after_end").position.y == 60.0,
		"Clear uses the containing block's direction to select the matching float")
	_check(_rect(layout, "holder").position.x == 0.0 and _rect(layout, "inherited").position.x == 60.0,
		"Inheritance preserves the logical float value for remapping in a different containing block")
	_check(_rect(layout, "wrapped_float").position.x == 102.0,
		"An intervening inline element with a different direction does not determine the float's side")
	var cleared_start: Rect2 = _rect(layout, "after_break")
	var line_break: HTMLElement = layout.get_element_by_id("clear_break")
	line_break.set_attribute("style", "clear: right")
	_check(layout.layout(size) == OK and _rect(layout, "after_break") == cleared_start,
		"A line break with inline-start clearance matches right clearance in this RTL block")
	line_break.set_attribute("style", "clear: inline-end")
	var end_laid_out: bool = layout.layout(size) == OK
	var cleared_end: Rect2 = _rect(layout, "after_break")
	line_break.set_attribute("style", "clear: left")
	_check(end_laid_out and layout.layout(size) == OK and _rect(layout, "after_break") == cleared_end and
		cleared_end.position.y < cleared_start.position.y,
		"Inline-end clearance on the break matches left clearance and skips the right float")
	var zone: HTMLElement = layout.get_element_by_id("logical")
	zone.set_attribute("style", "--flow: ltr")
	_check(layout.layout(size) == OK, "Reflow after changing the containing block's direction")
	_check(_rect(layout, "start").position.x == 0.0 and _rect(layout, "end").position.x == 98.0,
		"An LTR containing block maps the same logical values to the opposite edges")
	_check(_rect(layout, "after_start").position.y == 25.0 and _rect(layout, "after_end").position.y == 60.0,
		"Logical clearance follows the remapped floats after a direction change")


func _check_layout_and_paint_containment() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(255, 185)
	viewport.transparent_bg = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.css_diagnostics_enabled = true
	label.text = """<!doctype html><style>
		body { margin: 0; background: #121c28; }
		#scene { position: relative; width: 230px; height: 170px; padding-top: 10px; }
		#layout { contain: var(--isolate, layout); width: 60px; height: 42px; margin-left: 20px; }
		#abs { position: absolute; left: 8px; top: 5px; width: 92px; height: 18px; background: red; z-index: 20; }
		#far { position: absolute; left: 0; top: 0; width: 10px; height: 10px; transform: translateX(650px); }
		#cover { position: absolute; left: 40px; top: 12px; width: 20px; height: 22px; background: lime; z-index: 1; }
		#paint_position { position: absolute; left: 142px; top: 10px; }
		#paint { contain: paint; width: 50px; height: 44px; padding: 4px; border-radius: 20px; overflow-clip-margin: 6px; }
		#fixed { position: fixed; top: 0; left: 0; width: 100px; height: 62px; background: #3366ff; }
		#float_test { position: absolute; left: 10px; top: 87px; width: 110px; }
		#outside_float { float: left; width: 36px; height: 58px; }
		#isolated { contain: layout paint; width: 60px; }
		#inside_float { float: left; width: 12px; height: 23px; }
		#baseline { position: absolute; left: 135px; top: 95px; display: flex; align-items: baseline; }
		#baseline > div { width: 35px; height: 46px; }
		#baseline_box { contain: layout; contain: paint paint; }
		@supports (contain: paint layout) { #isolated { padding-bottom: 2px; } }
	</style><div id="scene">
		<div id="layout"><div id="abs"></div><div id="far"></div></div><div id="cover"></div>
		<div id="paint_position"><div id="paint"><div id="fixed"></div></div></div>
		<div id="float_test"><div id="outside_float"></div><div id="isolated"><div id="inside_float"></div></div></div>
		<div id="baseline"><div id="baseline_box">Text</div><div id="empty_box"></div></div>
	</div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var abs_box: Rect2 = _label_rect(label, "abs")
	var paint_box: Rect2 = _label_rect(label, "paint")
	_check(abs_box.position == _label_rect(label, "layout").position + Vector2(8.0, 5.0),
		"Layout containment establishes the containing block for absolute descendants")
	_check(_label_rect(label, "fixed").position == paint_box.position,
		"Paint containment establishes the containing block for fixed descendants")
	_check(label.get_content_size().x <= 255 and _label_rect(label, "far").position.x >= 650.0,
		"Layout containment keeps transformed overflow out of the outer scroll range")
	_check(_label_rect(label, "isolated") == Rect2(46.0, 87.0, 60.0, 25.0),
		"Containment establishes an independent float context and supports combined keywords")
	_check(_label_rect(label, "baseline_box").position.y == _label_rect(label, "empty_box").position.y,
		"Layout containment suppresses the item's descendant baseline")
	var invalid: bool
	for diagnostic: Dictionary in label.get_css_diagnostics():
		if diagnostic.get("property") == "contain" and diagnostic.get("value") == "paint paint":
			invalid = true
	_check(invalid, "Repeating a containment keyword invalidates the declaration")

	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(46.0, 21.0), Color.GREEN, "The contained z-index stays below a sibling stacking context")
	_check_pixel(image, Vector2(95.0, 21.0), Color.RED, "Layout containment preserves visible ink outside its box")
	var inside_clip_margin: Vector2 = Vector2(paint_box.end.x + 3.0, paint_box.get_center().y)
	var outside_clip_margin: Vector2 = Vector2(paint_box.end.x + 9.0, paint_box.get_center().y)
	_check_pixel(image, inside_clip_margin, Color("#3366ff"), "Paint containment honors overflow-clip-margin")
	_check_pixel(image, outside_clip_margin, Color("#121c28"), "Paint containment clips descendant overflow")
	_check_pixel(image, paint_box.position, Color("#121c28"), "Paint containment rounds the clipping corners")
	_check(label.get_element_at(inside_clip_margin) == label.get_element_by_id("fixed") and
		label.get_element_at(outside_clip_margin) != label.get_element_by_id("fixed") and
		label.get_element_at(paint_box.position + Vector2(0.5, 0.5)) != label.get_element_by_id("fixed"),
		"Paint containment uses the same clipping edge for pointer hits")

	for mode: String in ["layout", "style",]:
		label.text = """<!doctype html><style>
			body { margin: 0; width: 100px; height: 80px; contain: %s; overflow: auto; background: #b02040; }
			#extra { width: 30px; height: 180px; }
		</style><div id="extra"></div>""" % mode
		await process_frame
		await process_frame
		await RenderingServer.frame_post_draw
		image = viewport.get_texture().get_image()
		_check(image.get_pixel(150, 25).a < 0.1, "%s containment keeps the body background local" % mode)
		var previous_y: float = _label_rect(label, "extra").position.y
		_push_html_wheel(viewport, Vector2(15.0, 25.0), MOUSE_BUTTON_WHEEL_DOWN)
		_check(label.get_scroll_offset() == Vector2i.ZERO and _label_rect(label, "extra").position.y < previous_y,
			"%s containment keeps body overflow and scrolling local" % mode)
	viewport.queue_free()


func _check_viewport_overflow() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(240, 150)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { width: 160px; height: 55px; margin: 0; overflow: hidden; }
		#target { position: absolute; left: 320px; top: 280px; width: 50px; height: 40px; background: red; }
	</style><div id="target"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var scroller: ScrollContainer = label.get_node("_HTMLScroll") as ScrollContainer
	_check(label.get_content_size() == Vector2i(370, 320), "Body overflow moves to the viewport and leaves its box unclipped")
	_check(not scroller.get_h_scroll_bar().visible and not scroller.get_v_scroll_bar().visible,
		"Hidden body overflow suppresses both viewport scrollbars")
	_push_html_wheel(viewport, Vector2(16.0, 16.0), MOUSE_BUTTON_WHEEL_DOWN, 8.0)
	var pan: InputEventPanGesture = InputEventPanGesture.new()
	pan.position = Vector2(16.0, 16.0)
	pan.delta = Vector2(4.0, 4.0)
	viewport.push_input(pan, true)
	_check(label.get_scroll_offset() == Vector2i.ZERO, "Hidden viewport axes suppress wheel and pan movement")
	_check(label.scroll_to_anchor("target") and label.get_scroll_offset() == Vector2i(130, 170),
		"Anchor navigation can scroll past the body's own box on hidden viewport axes")
	await process_frame
	await process_frame
	_check(label.get_element_at(Vector2(200.0, 120.0)) == label.get_element_by_id("target"),
		"Hidden-axis navigation moves the painted canvas and its hit coordinates")
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), Vector2(200.0, 120.0), Color.RED, "Hidden-axis navigation reveals the target")
	var body: HTMLElement = label.query_selector("body")
	body.set_attribute("style", "overflow: auto")
	await process_frame
	await process_frame
	_check(scroller.get_h_scroll_bar().visible and scroller.get_v_scroll_bar().visible and
		label.get_scroll_offset() == Vector2i(130, 170),
		"Changing hidden overflow back to auto transfers the position to the native scrollbars")
	body.set_attribute("style", "overflow: hidden")
	await process_frame
	await process_frame
	_check(not scroller.get_h_scroll_bar().visible and not scroller.get_v_scroll_bar().visible and
		label.get_scroll_offset() == Vector2i(130, 170),
		"Changing auto overflow to hidden retains the viewport position without native scroll ranges")

	label.text = """<!doctype html><style>
		html { overflow: auto hidden; }
		body { margin: 0; width: 500px; height: 430px; }
	</style>"""
	await process_frame
	await process_frame
	_check(scroller.get_h_scroll_bar().visible and not scroller.get_v_scroll_bar().visible,
		"The root controls the two scrollbars independently")
	_push_html_wheel(viewport, Vector2(16.0, 16.0), MOUSE_BUTTON_WHEEL_DOWN, 8.0)
	_check(label.get_scroll_offset() == Vector2i.ZERO, "A blocked vertical wheel leaves the horizontal axis unchanged")
	_push_html_wheel(viewport, Vector2(16.0, 16.0), MOUSE_BUTTON_WHEEL_RIGHT, 8.0)
	_check(label.get_scroll_offset() == Vector2i(240, 0), "The other viewport axis remains directly scrollable")
	label.set_scroll_offset(Vector2i(120, 90))
	await process_frame
	_check(label.get_scroll_offset() == Vector2i(120, 90), "One request can set both the native and the hidden-axis offsets")

	label.text = """<!doctype html><style>
		html { overflow: scroll; }
		body { width: 64px; height: 64px; margin: 0; overflow: hidden; }
		div { width: 350px; height: 400px; }
	</style><div></div>"""
	await process_frame
	await process_frame
	_check(scroller.get_h_scroll_bar().visible and scroller.get_v_scroll_bar().visible,
		"Root overflow scroll forces both viewport scrollbars even when the body fits")
	label.set_scroll_offset(Vector2i(120, 90))
	_check(label.get_scroll_offset() == Vector2i.ZERO, "Explicit root overflow leaves the body's local clipping in place")
	label.query_selector("html").set_attribute("style", "overflow: clip")
	await process_frame
	await process_frame
	_check(not scroller.get_h_scroll_bar().visible and not scroller.get_v_scroll_bar().visible,
		"Setting root overflow clip updates the native scrollbar policy")
	viewport.queue_free()


func _check_scrollbar_gutter() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(240, 150)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		html { --gutters: both-edges stable; scrollbar-gutter: var(--gutters); scrollbar-color: #e8a100 transparent; }
		body { margin: 0; background: linear-gradient(#35679a, #35679a); }
		#unit { width: 100vw; height: 20px; background: red; }
		#media { width: 80px; height: 1px; }
		@media (width: 240px) and (height: 150px) and (min-width: 100vw) {
			#media { width: 120px; }
		}
	</style><div id="unit"></div><div id="media"></div><div id="extent"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var scroller: ScrollContainer = label.get_node("_HTMLScroll") as ScrollContainer
	var bar_width: float = scroller.get_v_scroll_bar().get_combined_minimum_size().x
	var reserved_width: float = viewport.size.x - 2.0 * bar_width
	_check(not scroller.get_v_scroll_bar().visible and is_equal_approx(scroller.get_h_scroll_bar().page, reserved_width),
		"Stable both-edges reserves two gutters without displaying a scrollbar")
	_check(_label_rect(label, "unit").size.x == reserved_width, "Unconditional gutters reduce the viewport unit basis")
	_check(_label_rect(label, "media").size.x == 120.0, "Media dimensions include the native gutters")
	var in_gutter: Vector2 = Vector2(bar_width / 2.0, 10.0)
	var in_content: Vector2 = Vector2(bar_width + 2.0, 10.0)
	_check(label.get_element_at(in_gutter) == null and label.get_element_at(in_content) == label.get_element_by_id("unit"),
		"Content hit testing starts after the leading gutter")
	await RenderingServer.frame_post_draw
	var output: Image = viewport.get_texture().get_image()
	_check_pixel(output, in_gutter, Color("#35679a"), "The canvas gradient extends to the leading gutter")
	_check_pixel(output, Vector2(240.0 - bar_width / 2.0, 10.0), Color("#35679a"),
		"The canvas gradient extends to the trailing gutter")
	_check_pixel(output, in_content, Color.RED, "Document content paints inside the gutters")

	label.get_element_by_id("extent").set_attribute("style", "height: 320px")
	await process_frame
	await process_frame
	_check(scroller.get_v_scroll_bar().visible and is_equal_approx(scroller.get_h_scroll_bar().page, reserved_width),
		"Showing the scrollbar consumes its reserved gutter without resizing the scrollport")
	_check(_label_rect(label, "media").size.x == 120.0, "Showing the native scrollbar keeps the media rule matched")
	_check(not scroller.get_h_scroll_bar().visible, "The stable viewport width avoids a redundant horizontal scrollbar")
	await RenderingServer.frame_post_draw
	output = viewport.get_texture().get_image()
	_check_pixel(output, Vector2(240.0 - bar_width / 2.0, 115.0), Color("#35679a"),
		"The canvas background also paints under a transparent scrollbar track")

	var html: HTMLElement = label.query_selector("html")
	html.set_attribute("style", "scrollbar-gutter: auto")
	await process_frame
	await process_frame
	_check(_label_rect(label, "unit").size.x == 240.0 and scroller.get_v_scroll_bar().visible,
		"A conditional scrollbar leaves vw based on the full viewport")
	_check(_label_rect(label, "media").size.x == 120.0, "A conditional scrollbar still preserves the media viewport")
	html.set_attribute("style", "scrollbar-gutter: auto; overflow: auto scroll")
	await process_frame
	await process_frame
	_check(_label_rect(label, "unit").size.x == 240.0 - bar_width and not scroller.get_h_scroll_bar().visible,
		"A forced scrollbar on the root reduces vw without overflowing an exact viewport fit")
	var unit: HTMLElement = label.get_element_by_id("unit")
	unit.set_attribute("style", "width: calc(200vw - 235px)")
	await process_frame
	await process_frame
	_check(not scroller.get_h_scroll_bar().visible, "Conditional bars are decided after the unconditional viewport-unit reduction")
	unit.remove_attribute("style")

	html.set_attribute("style", "scrollbar-width: none")
	await process_frame
	await process_frame
	_check(not scroller.get_v_scroll_bar().visible and _label_rect(label, "unit").size.x == 240.0,
		"Scrollbar-width none removes the stable gutters")

	html.set_attribute("style", "overflow: hidden; scrollbar-gutter: stable")
	label.layout_direction = Control.LAYOUT_DIRECTION_RTL
	await process_frame
	await process_frame
	_check(_label_rect(label, "unit").size.x == 240.0 - bar_width and not scroller.get_v_scroll_bar().visible,
		"A hidden axis can reserve a stable gutter")
	_check(label.get_element_at(in_gutter) == null and label.get_element_at(in_content) == label.get_element_by_id("unit"),
		"A single stable gutter follows the native scrollbar side in RTL")

	label.text = """<!doctype html><style>
		body { margin: 0; overflow: scroll; scrollbar-gutter: stable both-edges; }
		#unit { width: 100vw; height: 20px; }
	</style><div id="unit"></div>"""
	await process_frame
	await process_frame
	_check(scroller.get_v_scroll_bar().visible and scroller.get_h_scroll_bar().page == 240.0 - bar_width and
		_label_rect(label, "unit").size.x == 240.0,
		"Body overflow can force native bars without propagating its gutter or changing viewport units")
	viewport.queue_free()


func _check_scrollbar_styles() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(360, 245)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(360.0, 114.0)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; scrollbar-color: currentColor #2855d8;
			scrollbar-width: none; color: pink; }
		div.scroller { position: absolute; top: 8px; width: 80px; height: 48px;
			overflow: hidden auto; background: #252525; }
		.child { height: 155px; }
		#thin { left: 12px; color: #ff481e; scrollbar-width: thin; scrollbar-color: red; }
		#normal { left: 110px; color: #39ed35; }
		#invisible { left: 208px; scrollbar-width: none; }
		textarea { display: block; position: absolute; top: 8px; left: 300px; width: 45px; height: 48px;
			padding: 0; border: none; resize: none; color: #ffffff; background: #252525;
			scrollbar-width: thin; --tones: #ee25c9 #79dc33; scrollbar-color: var(--tones);
			overflow: hidden scroll; }
	</style>
	<div class="scroller" id="thin"><div class="child"></div></div>
	<div class="scroller" id="normal"><div class="child"></div></div>
	<div class="scroller" id="invisible"><div class="child" id="moving"></div></div>
	<textarea>One
Two
Three
Four
Five
Six
Seven</textarea>"""
	viewport.add_child(label)

	var port: HTMLLabel = HTMLLabel.new()
	port.position = Vector2(0.0, 128.0)
	port.size = Vector2(180.0, 104.0)
	port.text = """<!doctype html><style>body { margin: 0; scrollbar-width: none; scrollbar-color: orange purple; }</style>
		<div style="height:360px;">Viewport scrollbar</div>"""
	viewport.add_child(port)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var pixels: Image = viewport.get_texture().get_image()
	_check_pixel(pixels, Vector2(85.0, 16.0), Color("#252525"), "Thin leaves the original outer part of the track clear")
	_check_pixel(pixels, Vector2(89.0, 16.0), Color("#ff481e"), "Thin still paints a usable currentColor thumb")
	_check_pixel(pixels, Vector2(89.0, 41.0), Color("#2855d8"), "The second scrollbar color paints the track")
	_check_pixel(pixels, Vector2(184.0, 16.0), Color("#39ed35"), "Color inherits and resolves on the scroller, while width uses auto")
	_check_pixel(pixels, Vector2(285.0, 16.0), Color("#252525"), "Scrollbar-width none hides the inner track")
	_check_pixel(pixels, Vector2(341.0, 16.0), Color("#ee25c9"), "Textarea thumb uses its CSS color")
	_check_pixel(pixels, Vector2(341.0, 41.0), Color("#79dc33"), "Textarea track uses its CSS color")
	var moving: HTMLElement = label.get_element_by_id("moving")
	var original_y: float = moving.get_bounding_rect().position.y
	_push_html_wheel(viewport, Vector2(228.0, 24.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(moving.get_bounding_rect().position.y < original_y, "Scrollbar-width none preserves scrolling")

	var scroll: ScrollContainer = port.get_node("_HTMLScroll") as ScrollContainer
	var bar: VScrollBar = scroll.get_v_scroll_bar()
	var default_width: float = bar.size.x
	var html: HTMLElement = port.query_selector("html")
	_check(bar.visible and not bar.has_theme_stylebox_override("scroll"),
		"Viewport scrollbar settings come from html rather than body")
	html.set_attribute("style", "scrollbar-width: thin; scrollbar-color: #ec34ee #25c9a4;")
	await process_frame
	await process_frame
	var styled_thumb: StyleBoxFlat = bar.get_theme_stylebox("grabber") as StyleBoxFlat
	var styled_track: StyleBoxFlat = bar.get_theme_stylebox("scroll") as StyleBoxFlat
	_check(bar.visible and bar.size.x > 0.0 and bar.size.x < default_width and
		styled_thumb != null and styled_track != null and styled_thumb.bg_color.is_equal_approx(Color("#ec34ee")) and
		styled_track.bg_color.is_equal_approx(Color("#25c9a4")),
		"Root styles reduce the viewport scrollbar width and apply the thumb and track colors")

	html.set_attribute("style", "scrollbar-width: none")
	await process_frame
	await process_frame
	_push_html_wheel(viewport, Vector2(80.0, 160.0), MOUSE_BUTTON_WHEEL_DOWN)
	await process_frame
	_check(not bar.visible and port.get_scroll_offset().y > 0, "Viewport scrolling stays active with its scrollbar hidden")

	html.set_attribute("style", "scrollbar-width: auto; scrollbar-color: auto")
	await process_frame
	await process_frame
	_check(bar.visible and is_equal_approx(bar.size.x, default_width) and not bar.has_theme_stylebox_override("scroll"),
		"CSS auto restores the native viewport scrollbar theme")
	viewport.queue_free()


func _check_css_transitions() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	var html: String = """
<!doctype html><style>
body { margin:0; background:white; }
#wide { width:20px; height:10px; --timing:width 120s linear -60s; transition:var(--timing); }
#multi { width:20px; height:10px; transition-property:height,width;
  transition-duration:120s; transition-delay:-60s; transition-timing-function:linear; }
#moving { position:absolute; left:100px; top:25px; width:40px; height:14px;
  transform:none; transition:transform 120s linear -60s; }
#rotating { position:absolute; left:175px; top:40px; width:20px; height:10px; transform-origin:0 0;
  transform:rotate(0deg); transition:transform 120s linear -60s; }
#easing { width:20px; height:6px; transition:width 120s linear(0, .25 50%, 1) -60s; }
#stepped { width:20px; height:6px; transition:width 120s steps(4,jump-end) -36s; }
#discrete { height:6px; transition:width 120s allow-discrete linear -30s; }
#no_discrete { height:6px; transition:width 120s linear -30s; }
#alpha { position:absolute; left:10px; top:65px; width:20px; height:14px;
  background:red; opacity:0.2; transition:opacity 120s linear -60s; }
#paint { position:absolute; left:40px; top:65px; width:20px; height:14px;
  background:red; transition:background-color 120s cubic-bezier(0,0,1,1) -60s; }
#ink { position:absolute; left:70px; top:65px; color:red; transition:color 120s linear -60s; }
#ink_child { width:20px; height:14px; background-color:currentColor; }
.next #wide { width:20%; }
.next #multi { width:60px; height:30px; }
.next #moving { transform:translateX(50%); }
.next #rotating { transform:rotate(1turn); }
.next #easing, .next #stepped { width:60px; }
.next #discrete, .next #no_discrete { width:60px; }
.next #alpha { opacity:0.8; }
.next #paint { background-color:blue; }
.next #ink { color:blue; }
@supports (transition: width 1s steps(1,jump-none)) {
  #wide { width:70px; }
}
</style>
<body id="scene">
	<div id="wide"></div><div id="multi"></div><div id="easing"></div><div id="stepped"></div>
	<div id="discrete"></div><div id="no_discrete"></div>
	<a id="moving" href="#destination"></a>
	<div id="rotating"></div>
	<div id="alpha"></div><div id="paint"></div><div id="ink"><div id="ink_child"></div></div>
</body>
"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(200, 120)) == OK, "Lay out the initial CSS transition styles")
	_check(is_equal_approx(_rect(layout, "wide").size.x, 20.0), "First layout does not start a CSS transition")
	_check(layout.get_css_diagnostics().is_empty(), "Transition shorthand, longhands and easing functions parse")
	var scene: HTMLElement = layout.get_element_by_id("scene")
	scene.set_attribute("class", "next")
	_check(layout.layout(Vector2i(200, 120)) == OK, "Lay out the initial transition sample")
	_check(is_equal_approx(_rect(layout, "wide").size.x, 30.0)
		and _rect(layout, "multi").size.is_equal_approx(Vector2(40.0, 20.0)),
		"Negative delay samples mixed units and repeats shorter transition lists")
	_check(is_equal_approx(_rect(layout, "easing").size.x, 30.0)
		and is_equal_approx(_rect(layout, "stepped").size.x, 30.0),
		"Piecewise linear and step easing pick the matching intermediate sizes")
	_check(is_equal_approx(_rect(layout, "discrete").size.x, 200.0)
		and is_equal_approx(_rect(layout, "no_discrete").size.x, 60.0),
		"Only allow-discrete holds an intrinsic width through the first half of the transition")
	_check(is_equal_approx(_rect(layout, "moving").position.x, 110.0)
		and layout.get_link_at(Vector2(145.0, 30.0)) == "#destination",
		"Transform interpolation from none updates the box and link hit area together")
	_check(_rect(layout, "rotating").position.is_equal_approx(Vector2(155.0, 30.0)),
		"Rotation keeps complete turns until interpolation")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(200, 120)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var canvas: Node2D = Node2D.new()
	canvas.draw.connect(func() -> void: layout.draw(canvas))
	viewport.add_child(canvas)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(18.0, 70.0), Color(1.0, 0.5, 0.5), "Mid-transition group opacity")
	_check_pixel(image, Vector2(48.0, 70.0), Color(0.5, 0.0, 0.5), "Mid-transition background color")
	_check_pixel(image, Vector2(78.0, 70.0), Color(0.5, 0.0, 0.5), "A descendant paints inherited animated currentColor")

	scene.remove_attribute("class")
	_check(layout.layout(Vector2i(200, 120)) == OK and absf(_rect(layout, "wide").size.x - 25.0) < 0.1,
		"Reversing a running transition samples the current value and shortens its remaining duration")
	layout.get_element_by_id("wide").set_attribute("style", "transition-property:none")
	_check(layout.layout(Vector2i(200, 120)) == OK and is_equal_approx(_rect(layout, "wide").size.x, 20.0),
		"Removing the transition property cancels its running transition")
	layout.clear()
	viewport.queue_free()

	var short_transition: HTMLLayout = HTMLLayout.new()
	var short_html: String = """<!doctype html><style>
		body { margin:0; }
		#short { width:20px; height:10px; transition:width 80ms linear 25ms; }
		.changed #short { width:60px; }
	</style><body id="short_body"><div id="short"></div></body>"""
	_check(short_transition.parse(short_html) == OK and short_transition.layout(Vector2i(100, 80)) == OK,
		"Lay out a transition with a positive delay")
	short_transition.get_element_by_id("short_body").set_attribute("class", "changed")
	_check(short_transition.layout(Vector2i(100, 80)) == OK and is_equal_approx(_rect(short_transition, "short").size.x, 20.0),
		"A positive delay holds the starting value")
	var parallel_transition: HTMLLayout = HTMLLayout.new()
	_check(parallel_transition.parse(short_html) == OK and parallel_transition.layout(Vector2i(100, 80)) == OK,
		"Lay out a second document with an independent transition")
	parallel_transition.get_element_by_id("short_body").set_attribute("class", "changed")
	_check(parallel_transition.layout(Vector2i(100, 80)) == OK, "Start concurrent transitions in two layouts")
	await create_timer(0.25).timeout
	_check(is_equal_approx(_rect(short_transition, "short").size.x, 60.0)
		and is_equal_approx(_rect(parallel_transition, "short").size.x, 60.0),
		"The scene timeline completes independent layout transitions without another edit")

	var label_viewport: SubViewport = SubViewport.new()
	label_viewport.size = Vector2i(100, 80)
	root.add_child(label_viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(100.0, 80.0)
	label.text = """<!doctype html><style>
		body { margin:0; }
		#box { width:20px; height:14px; transition:width 120s linear -60s; }
		.changed #box { width:60px; }
	</style><body id="label_body"><div id="box"></div></body>"""
	label_viewport.add_child(label)
	await process_frame
	await process_frame
	label.get_element_by_id("label_body").set_attribute("class", "changed")
	await process_frame
	await process_frame
	_check(absf(label.get_element_by_id("box").get_bounding_rect().size.x - 40.0) < 0.1,
		"HTMLLabel defers the edit and keeps its intermediate geometry")
	label_viewport.queue_free()


func _check_html_label_animation_process_mode() -> void:
	var label: HTMLLabel = HTMLLabel.new()
	_check(label.animation_process_mode == HTMLLabel.ANIMATION_PROCESS_IDLE, "HTMLLabel defaults to the Idle animation mode")
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_MANUAL
	var packed: PackedScene = PackedScene.new()
	_check(packed.pack(label) == OK, "Pack an HTMLLabel with Manual selected")
	var restored: HTMLLabel = packed.instantiate() as HTMLLabel
	_check(restored.animation_process_mode == HTMLLabel.ANIMATION_PROCESS_MANUAL, "Manual mode survives scene serialization")
	restored.free()
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_PHYSICS

	var previous_rate: int = Engine.physics_ticks_per_second
	var previous_scale: float = Engine.time_scale
	Engine.physics_ticks_per_second = 10
	Engine.time_scale = 0.5
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(480, 100)
	root.add_child(viewport)
	label.size = Vector2(480.0, 100.0)
	label.text = """<!doctype html><style>
		body { margin:0; }
		div { width:20px; height:10px; }
		#transition { transition:width 4s linear .4s; }
		@keyframes Expand { from { width:20px; } to { width:420px; } }
		.go #animation { animation:Expand 4s linear .4s both; }
		.go #transition { width:420px; }
	</style><body id="scene"><div id="animation"></div><div id="transition"></div></body>"""
	viewport.add_child(label)
	await label.layout_changed
	label.get_element_by_id("scene").set_attribute("class", "go")
	await label.layout_changed

	var elapsed: Array[float] = [0.0,]
	var idle_tick: Callable = func() -> void:
		if label.animation_process_mode == HTMLLabel.ANIMATION_PROCESS_IDLE:
			elapsed[0] += root.get_process_delta_time()
	var physics_tick: Callable = func() -> void:
		if label.animation_process_mode == HTMLLabel.ANIMATION_PROCESS_PHYSICS:
			elapsed[0] += root.get_physics_process_delta_time()
	var check_widths: Callable = func(expected: float, message: String) -> void:
		var animation_width: float = label.get_element_by_id("animation").get_bounding_rect().size.x
		var transition_width: float = label.get_element_by_id("transition").get_bounding_rect().size.x
		_check(absf(animation_width - expected) < 0.2 and absf(transition_width - expected) < 0.2,
			"%s (animation %s, transition %s, expected %s)" % [message, animation_width, transition_width, expected])
	process_frame.connect(idle_tick)
	physics_frame.connect(physics_tick)
	_check(label.is_physics_processing_internal() and not label.is_processing_internal(),
		"Physics mode ticks CSS effects only in physics processing")
	while elapsed[0] < 0.11:
		await RenderingServer.frame_post_draw
	check_widths.call(20.0, "The starting width is held during the physics-driven delay")
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_MANUAL
	_check(not label.is_processing_internal() and not label.is_physics_processing_internal(),
		"Manual mode disables both automatic animation callbacks")
	label.advance_animation(0.08)
	elapsed[0] += 0.08
	check_widths.call(20.0, "Entering Manual and stepping part of the remaining delay holds the starting values")
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_IDLE
	_check(label.is_processing_internal() and not label.is_physics_processing_internal(),
		"Idle mode switches the active frame callback")
	while elapsed[0] < 0.30:
		await RenderingServer.frame_post_draw
	check_widths.call(20.0, "Switching to Idle preserves the remaining delay")
	while elapsed[0] < 0.52:
		await RenderingServer.frame_post_draw
	var before_switch: float = 20.0 + (elapsed[0] - 0.4) * 100.0
	check_widths.call(before_switch, "Both effects advance with the selected idle frame deltas")
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_MANUAL
	await physics_frame
	await process_frame
	check_widths.call(before_switch, "Idle and physics frames leave the Manual sample in place")
	label.advance_animation(0.2)
	elapsed[0] += 0.2
	check_widths.call(before_switch + 20.0, "Manual advancement samples keyframes and transitions synchronously")
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_PHYSICS
	check_widths.call(before_switch + 20.0, "Switching from Manual to Physics preserves the sampled progress")
	var next_sample: float = elapsed[0] + 0.15
	while elapsed[0] < next_sample:
		await RenderingServer.frame_post_draw
	check_widths.call(20.0 + (elapsed[0] - 0.4) * 100.0,
		"Both effects resume from the same time and advance with the scaled physics step")
	label.advance_animation(0.05)
	elapsed[0] += 0.05
	check_widths.call(20.0 + (elapsed[0] - 0.4) * 100.0, "An explicit step can supplement Physics updates")

	process_frame.disconnect(idle_tick)
	physics_frame.disconnect(physics_tick)
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_MANUAL
	label.text = label.text.replace("420px", "620px")
	label.advance_animation(0.0)
	check_widths.call(20.0, "A zero step applies pending source changes")
	label.get_element_by_id("scene").set_attribute("class", "go")
	label.advance_animation(0.5)
	check_widths.call(35.0, "The first manual step includes the delay of a pending style change")
	label.advance_animation(4.0)
	check_widths.call(620.0, "A large manual step reaches the final values of both effects")
	_check(label.get_content_size().x >= 620, "Manual advancement refreshes the content size before returning")

	viewport.queue_free()
	Engine.time_scale = previous_scale
	Engine.physics_ticks_per_second = previous_rate


func _check_transform_matrix_transitions() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><style>
		body { margin:0; background:white; }
		.box { position:absolute; display:block; left:60px; top:20px; width:20px; height:10px;
			transform-origin:0 0; transition:transform 120s linear -60s; }
		#different { width:40px; background:red; transform:translateX(50%) rotate(0deg);
			transition:transform 120s linear -60s, width 120s linear -60s; }
		#prefix { top:95px; transform:rotate(0deg) scale(1); }
		#matrix { left:140px; top:50px; transform:matrix(1, 0, 0, 1, 0, 0) rotate(0deg); }
		#reflection { left:220px; top:55px; transform:matrix(-1, 0, 0, 1, 0, 0); }
		#axes { top:110px; transform:rotateX(90deg); }
		#spatial { left:140px; top:110px; transform:perspective(200px) translateZ(0px); }
		#projective { left:140px; top:135px; transform:matrix3d(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1); }
		#singular { left:220px; top:85px; transform:translateX(5px) scale(0); transition-delay:60s; }
		#discrete { left:220px; top:110px; transform:translateX(5px) scale(0); transition-delay:-30s;
			transition-behavior:allow-discrete; }
		.changed #different { width:80px; transform:rotate(90deg); }
		.changed #prefix { transform:rotate(1turn) translateX(40px); }
		.changed #matrix { transform:matrix(1, 0, 0, 1, 40, 0) rotate(1turn); }
		.changed #reflection { transform:matrix(1, 0, 0, -1, 0, 0); }
		.changed #axes { transform:rotateY(90deg); }
		.changed #spatial { transform:translateZ(100px); }
		.changed #projective { transform:matrix3d(1,0,0,0, 1,1,0,0, 0,0,1,-.01, 10,0,20,.8); }
		.changed #singular, .changed #discrete { transform:translateX(25px) translateY(0); }
	</style><body id="scene">
		<a class="box" id="different" href="#transformed"></a>
		<div class="box" id="prefix"></div><div class="box" id="matrix"></div>
		<div class="box" id="reflection"></div><div class="box" id="axes"></div>
		<div class="box" id="spatial"></div><div class="box" id="projective"></div>
		<a class="box" id="singular" href="#jump"></a><a class="box" id="discrete" href="#hold"></a>
	</body>"""
	var viewport_size: Vector2i = Vector2i(300, 160)
	_check(layout.parse(html) == OK and layout.layout(viewport_size) == OK, "Lay out matrix transition endpoints")
	var scene: HTMLElement = layout.get_element_by_id("scene")
	scene.set_attribute("class", "changed")
	_check(layout.layout(viewport_size) == OK, "Sample matrix transitions")
	var unit: float = sqrt(0.5)
	var different: Rect2 = _rect(layout, "different")
	_check(different.position.is_equal_approx(Vector2(75.0 - 10.0 * unit, 20.0))
		and different.size.is_equal_approx(Vector2.ONE * 70.0 * unit)
		and layout.get_link_at(Vector2(90.0, 43.0)) == "#transformed"
		and layout.get_link_at(Vector2(113.0, 22.0)).is_empty(),
		"Matrix fallback uses the current reference width and exact rotated link hits")
	_check(_rect(layout, "prefix").position.is_equal_approx(Vector2(20.0, 85.0))
		and _rect(layout, "matrix").position.is_equal_approx(Vector2(140.0, 40.0)),
		"Matrix interpolation preserves complete turns in matching functions before and after it")
	var reflection: Rect2 = _rect(layout, "reflection")
	_check(reflection.position.is_equal_approx(Vector2(210.0, 35.0))
		and reflection.size.is_equal_approx(Vector2(10.0, 20.0)),
		"2D decomposition interpolates reflections without shrinking through zero")
	_check(_rect(layout, "axes").size.is_equal_approx(Vector2(50.0 / 3.0, 40.0 / 3.0))
		and _rect(layout, "spatial").size.is_equal_approx(Vector2(20.0, 10.0) / 0.875),
		"3D decomposition applies quaternion rotation and projective translation")
	var projective: Rect2 = _rect(layout, "projective")
	_check(projective.position.is_equal_approx(Vector2(140.0 + 6.25 / 1.0625, 135.0))
		and projective.size.is_equal_approx(Vector2(28.125, 11.25) / 1.0625),
		"Explicit matrix3d interpolation normalizes perspective and preserves shear")
	_check(layout.get_link_at(Vector2(250.0, 90.0)) == "#jump"
		and layout.get_link_at(Vector2(250.0, 115.0)).is_empty(),
		"A singular matrix skips the transition and its delay unless allow-discrete is set")
	var viewport: SubViewport = SubViewport.new()
	viewport.size = viewport_size
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var canvas: Node2D = Node2D.new()
	canvas.draw.connect(func() -> void: layout.draw(canvas))
	viewport.add_child(canvas)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(90.0, 43.0), Color.RED, "The interpolated matrix paints inside the rotated link")
	_check_pixel(image, Vector2(113.0, 22.0), Color.WHITE, "The rotated link leaves the corner of its bounds clear")
	scene.remove_attribute("class")
	_check(layout.layout(viewport_size) == OK and absf(_rect(layout, "prefix").position.x - 50.0) < 0.5,
		"Reversing a matrix transition samples the currently interpolated transform")
	layout.clear()
	viewport.queue_free()


func _check_css_animations() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><style>
		body { margin:0; background:white; }
		.box { width:20px; height:12px; }
		@keyframes Grow {
			-5% { width:500px; }
			50% { width:35px; color:blue; animation-timing-function:steps(2,end); }
			50.0% { width:var(--mid); width:500px !important; }
			to { width:100px; }
		}
		@keyframes Range { from { width:10px; } to { width:110px; } }
		@keyframes Solid { from, to { width:100px; } }
		@keyframes "none" { to { width:65px; } }
		@keyframes Move { to { transform:translateX(40px); } }
		@keyframes Paint { from { color:red; } to { color:blue; } }
		@layer low, high;
		@layer high { @keyframes Layered { to { width:73px; } } }
		@layer low { @keyframes Layered { to { width:89px; } } }
		@media (min-width:260px) { @keyframes Responsive { to { width:95px; } } }
		@supports (animation:3s ease-in Zoom) { @keyframes Feature { to { width:75px; } } }
		#middle { --mid:60px; animation:Grow 120s linear -30s paused; }
		#steps { --mid:60px; animation:Grow 120s linear -75s paused; }
		#stack { --mid:60px; animation: Solid 0s both, Grow 120s linear -30s paused; }
		#direction { animation:Range 120s linear -150s 2 alternate paused; }
		#fraction { animation:Range 120s linear -240s 1.25 alternate forwards paused; }
		#backwards { animation:Range 120s linear 15s backwards reverse paused; }
		#quoted { animation:0s forwards "none"; }
		#case_sensitive { animation:range 0s forwards; }
		#important { width:33px !important; animation:Range 0s forwards; }
		#layered { animation:Layered 0s forwards; }
		#media { animation:Responsive 0s forwards; }
		#feature { animation:Feature 0s forwards; }
		#none { animation: Range 0s; }
		#zero { animation:Range 120s linear 0s 0 both paused; }
		#longhands { animation-name:Solid, Range; animation-duration:120s; animation-delay:-60s;
			animation-timing-function:linear; animation-play-state:paused; }
		#moving { display:block; transform:rotate(0deg); --motion:120s linear -60s Move paused;
			animation:var(--motion); }
		#paint { position:absolute; left:230px; top:12px; animation:Paint 120s linear -60s paused; }
		#paint > div { width:20px; height:12px; background:currentColor; }
		#cancel { animation:Solid 0s forwards; transition:width 120s 60s; }
		.cancelled #cancel { animation-name:none; }
		@supports (animation-duration:-1s) { #none { width:45px; } }
	</style><body id="scene">
		<div class="box" id="middle"></div><div class="box" id="steps"></div><div class="box" id="stack"></div>
		<div class="box" id="direction"></div><div class="box" id="fraction"></div><div class="box" id="backwards"></div>
		<div class="box" id="quoted"></div><div class="box" id="case_sensitive"></div><div class="box" id="important"></div>
		<div class="box" id="layered"></div><div class="box" id="media"></div><div class="box" id="feature"></div>
		<div class="box" id="none"></div><div class="box" id="zero"></div><div class="box" id="longhands"></div>
		<a class="box" id="moving" href="#motion"></a><div class="box" id="cancel"></div>
		<div id="paint"><div></div></div>
	</body>"""
	layout.css_diagnostics_enabled = true
	_check(layout.parse(html) == OK and layout.layout(Vector2i(300, 250)) == OK, "Apply CSS keyframes on first layout")
	_check(layout.get_css_diagnostics().is_empty(), "Recognize animation shorthand, all longhands and keyframe syntax")
	_check(is_equal_approx(_rect(layout, "middle").size.x, 40.0)
		and is_equal_approx(_rect(layout, "steps").size.x, 60.0)
		and is_equal_approx(_rect(layout, "stack").size.x, 80.0),
		"Keyframes merge repeated offsets, resolve custom properties and use per-segment easing and underlying values")
	_check(is_equal_approx(_rect(layout, "direction").size.x, 85.0)
		and is_equal_approx(_rect(layout, "fraction").size.x, 85.0)
		and is_equal_approx(_rect(layout, "backwards").size.x, 110.0)
		and is_equal_approx(_rect(layout, "zero").size.x, 10.0),
		"Direction, backwards fill, and fractional and zero iteration counts control sampling")
	_check(is_equal_approx(_rect(layout, "quoted").size.x, 65.0)
		and is_equal_approx(_rect(layout, "case_sensitive").size.x, 20.0)
		and is_equal_approx(_rect(layout, "important").size.x, 33.0)
		and is_equal_approx(_rect(layout, "none").size.x, 20.0)
		and is_equal_approx(_rect(layout, "longhands").size.x, 60.0),
		"Animation names preserve case and quoted keywords, list values repeat, and important styles win")
	_check(is_equal_approx(_rect(layout, "layered").size.x, 73.0)
		and is_equal_approx(_rect(layout, "media").size.x, 95.0)
		and is_equal_approx(_rect(layout, "feature").size.x, 75.0)
		and is_equal_approx(_rect(layout, "cancel").size.x, 100.0),
		"Layer order, media and feature queries pick among named keyframe rules")
	var moving: Rect2 = _rect(layout, "moving")
	_check(is_equal_approx(moving.position.x, 20.0) and layout.get_link_at(moving.get_center()) == "#motion",
		"Animation transform samples also move link hit geometry")
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 250)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var canvas: Node2D = Node2D.new()
	canvas.draw.connect(func() -> void: layout.draw(canvas))
	viewport.add_child(canvas)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), Vector2(235.0, 16.0), Color(0.5, 0.0, 0.5),
		"Keyframe color is inherited by the descendant's currentColor background")
	layout.get_element_by_id("scene").set_attribute("class", "cancelled")
	_check(layout.layout(Vector2i(250, 250)) == OK and is_equal_approx(_rect(layout, "media").size.x, 20.0)
		and is_equal_approx(_rect(layout, "cancel").size.x, 20.0),
		"Removing the matching rule or animation name restores the underlying value without starting a transition")
	layout.clear()
	viewport.queue_free()

	var timed: HTMLLayout = HTMLLayout.new()
	var timed_html: String = """<!doctype html><style>
		@keyframes Timer { from { width:10px; } to { width:110px; } }
		#clock { width:20px; height:12px; animation:Timer 600ms linear -150ms both paused; }
		.play #clock { animation-play-state:running; }
		#generated { display:inline-block; }
		#generated::before { content:''; display:block; height:12px; animation:Timer 600ms linear both; }
		.play #generated::before { animation-play-state:paused; }
	</style><body id="scene"><div id="clock"></div><div id="generated"></div></body>"""
	var viewport_size: Vector2i = Vector2i(180, 80)
	_check(timed.parse(timed_html) == OK and timed.layout(viewport_size) == OK, "Create a paused scene animation")
	await create_timer(0.10).timeout
	_check(is_equal_approx(_rect(timed, "clock").size.x, 35.0), "Paused time stays at the negative delay sample")
	var generated_width: float = _rect(timed, "generated").size.x
	_check(generated_width > 10.0, "Generated content advances its own keyframe animation")
	timed.get_element_by_id("scene").set_attribute("class", "play")
	_check(timed.layout(viewport_size) == OK and is_equal_approx(_rect(timed, "clock").size.x, 35.0),
		"Playing a paused animation resumes from the same time")
	await create_timer(0.1).timeout
	_check(_rect(timed, "clock").size.x > 35.0 and _rect(timed, "clock").size.x < 110.0,
		"The scene advances keyframes without a new edit")
	var paused_generated_width: float = _rect(timed, "generated").size.x
	_check(paused_generated_width >= generated_width and paused_generated_width < 110.0,
		"Pausing a generated pseudo-element keeps its time across a style rebuild")
	await create_timer(0.55).timeout
	_check(is_equal_approx(_rect(timed, "clock").size.x, 110.0),
		"The final keyframe remains applied with forwards fill")
	_check(is_equal_approx(_rect(timed, "generated").size.x, paused_generated_width),
		"The generated pseudo-element stays paused while the host timeline advances")
	timed.clear()


func _check_layout_property_motion() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var viewport_size: Vector2i = Vector2i(240, 290)
	var html: String = """<!doctype html><style>
		body { position:relative; margin:0; background:white; }
		#offset { position:absolute; top:20px; left:-60px; width:10px; height:10px;
			transition:inset 120s linear -60s; }
		.go #offset { top:40px; left:10%; }
		#logical { position:absolute; direction:rtl; inset-block-start:60px; inset-inline-start:10px;
			width:20px; height:10px; transition:inset-inline 120s linear -60s, inset-inline-start 120s linear -30s; }
		.go #logical { inset-inline-start:50px; }
		#edges { position:absolute; top:60px; left:60px; width:40px; height:10px;
			margin:-12px -8px -2px; padding:4px; border:2px solid red;
			transition:margin 120s linear -60s, padding 120s linear -60s, border-left 120s linear -60s; }
		.go #edges { margin:-28px -24px; padding:12px; border-left-width:10px; }
		#cap { position:absolute; top:100px; left:20px; width:5px; height:90px;
			min-inline-size:20px; max-block-size:30px;
			transition:min-inline-size 120s linear -60s, max-block-size 120s linear -60s; }
		.go #cap { min-inline-size:60px; max-block-size:70px; }
		#grid { position:absolute; left:100px; top:100px; display:grid; width:100px;
			grid-template-columns:20px 20px; grid-auto-rows:10px; gap:4px 8px;
			transition:gap 120s linear -60s, column-gap 120s linear -30s; }
		.go #grid { gap:20px 40px; }
		#flex { position:absolute; top:160px; display:flex; width:130px; column-gap:0;
			transition:column-gap 120s linear -60s; }
		.go #flex { column-gap:20px; }
		#flex > div, #shrink > div { min-width:0; height:10px; }
		#grow { flex:0 1 20px; transition:flex 120s linear -60s; }
		.go #grow { flex:4 1 60px; }
		#grow_other { flex:1 1 20px; }
		@keyframes Shrink { from { flex-shrink:1; } to { flex-shrink:3; } }
		#shrink { position:absolute; top:180px; display:flex; width:100px; }
		#shrink > div { flex:0 1 80px; }
		#shrinking { animation:Shrink 120s linear -60s paused; }
		@keyframes Shift {
			from { left:-40px; margin-left:-24px; padding-block:4px; }
			to { left:10%; margin-left:-8px; padding-block:20px; }
		}
		#shifting { position:absolute; top:200px; width:40px; height:10px; animation:Shift 120s linear -60s paused; }
		#table { position:absolute; top:240px; left:10px; border-collapse:separate; border-spacing:0;
			transition:border-spacing 120s linear -60s; }
		#table td { padding:0; width:20px; height:10px; }
		.go #table { border-spacing:20px 10px; }
		#radius { position:absolute; top:200px; left:170px; width:40px; height:40px; background:red;
			border-radius:0; transition:border-radius 120s linear -60s; }
		.go #radius { border-radius:40px 0 0 0 / 20px 0 0 0; }
		#outline { position:absolute; top:250px; left:160px; width:40px; height:20px; background:red;
			outline:2px solid blue; outline-offset:-2px;
			transition:outline 120s linear -60s, outline-offset 120s linear -60s; }
		.go #outline { outline-width:10px; outline-offset:-6px; }
	</style><body id="scene">
		<div id="offset"></div><div id="logical"></div><div id="edges"></div><div id="cap"></div>
		<div id="grid"><div id="grid_first"></div><div id="grid_second"></div><div id="grid_third"></div></div>
		<div id="flex"><div id="grow"></div><div id="grow_other"></div></div>
		<div id="shrink"><div id="shrinking"></div><div id="shrink_other"></div></div><div id="shifting"></div>
		<table id="table"><tr><td id="cell_first"></td><td id="cell_second"></td></tr><tr><td id="cell_third"></td></tr></table>
		<div id="radius"></div><div id="outline"></div>
	</body>"""
	_check(layout.parse(html) == OK and layout.layout(viewport_size) == OK, "Lay out the initial box and spacing values")
	layout.get_element_by_id("scene").set_attribute("class", "go")
	_check(layout.layout(viewport_size) == OK, "Apply interpolated layout property values")
	_check(_rect(layout, "offset").position.is_equal_approx(Vector2(-18.0, 30.0))
		and _rect(layout, "logical").position.is_equal_approx(Vector2(200.0, 60.0)),
		"Inset transitions resolve mixed units and map logical shorthand and longhand targets in RTL")
	var edges: Rect2 = _rect(layout, "edges")
	_check(edges.position.is_equal_approx(Vector2(44.0, 40.0)) and edges.size.is_equal_approx(Vector2(64.0, 30.0)),
		"Margins retain their negative values while padding and side border widths grow")
	_check(_rect(layout, "cap").size.is_equal_approx(Vector2(40.0, 50.0)),
		"Logical min and max size names animate their physical constraints")
	var first_grid_position: Vector2 = _rect(layout, "grid_first").position
	_check((_rect(layout, "grid_second").position - first_grid_position).is_equal_approx(Vector2(36.0, 0.0))
		and (_rect(layout, "grid_third").position - first_grid_position).is_equal_approx(Vector2(0.0, 22.0)),
		"Grid recomputes both gaps and uses the timing of the last matching transition entry")
	_check(is_equal_approx(_rect(layout, "grow").size.x, 80.0)
		and is_equal_approx(_rect(layout, "grow_other").position.x, 90.0),
		"Flex basis, gap and grow factors above one change free space distribution")
	_check(is_equal_approx(_rect(layout, "shrinking").size.x, 40.0)
		and is_equal_approx(_rect(layout, "shrink_other").size.x, 60.0)
		and _rect(layout, "shifting").is_equal_approx(Rect2(-24.0, 200.0, 40.0, 34.0)),
		"Keyframes share the flex and signed box length interpolators")
	var first_cell_position: Vector2 = _rect(layout, "cell_first").position
	_check((_rect(layout, "cell_second").position - first_cell_position).is_equal_approx(Vector2(30.0, 0.0))
		and (_rect(layout, "cell_third").position - first_cell_position).is_equal_approx(Vector2(0.0, 15.0)),
		"Both axes of border spacing participate in table layout")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = viewport_size
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var canvas: Node2D = Node2D.new()
	canvas.draw.connect(func() -> void: layout.draw(canvas))
	viewport.add_child(canvas)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(171.0, 201.0), Color.WHITE, "The interpolated radius clips a corner pixel")
	_check_pixel(image, Vector2(191.0, 201.0), Color.RED, "The interpolated radius retains the flat top edge")
	_check_pixel(image, Vector2(159.0, 260.0), Color.BLUE, "The animated outline width extends past its signed offset")
	layout.clear()
	viewport.queue_free()

	var scroll_viewport: SubViewport = SubViewport.new()
	scroll_viewport.size = Vector2i(240, 100)
	root.add_child(scroll_viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(240.0, 100.0)
	label.text = """<!doctype html><style>
		html { scrollbar-width:none; scroll-padding-block-start:8px;
			transition:scroll-padding-block 120s linear -60s; }
		html.go { scroll-padding-block-start:20%; }
		body { margin:0; position:relative; height:400px; }
		#target { position:absolute; top:160px; width:20px; height:10px; scroll-margin-block-start:-12px;
			transition:scroll-margin-block 120s linear -60s; }
		.go #target { scroll-margin-block-start:4px; }
	</style><div id="target"></div>"""
	scroll_viewport.add_child(label)
	await process_frame
	await process_frame
	_check(label.scroll_to_anchor("target") and label.get_scroll_offset().y == 164,
		"Use the initial scroll margin and padding for an anchor")
	label.query_selector("html").set_attribute("class", "go")
	await process_frame
	await process_frame
	_check(label.scroll_to_anchor("target") and absi(label.get_scroll_offset().y - 150) <= 1,
		"Anchor navigation uses animated signed scroll margins and mixed-unit scroll padding")
	scroll_viewport.queue_free()


func _check_text_property_motion() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var viewport_size: Vector2i = Vector2i(260, 350)
	var html: String = """<!doctype html><style>
		body { margin:0; background:white; font:20px/24px "Courier New", monospace; }
		.row { white-space:nowrap; }
		.probe { display:inline-block; }
		#size { font:400 12px/1 "Courier New", monospace; transition:font 120s linear -60s; }
		.go #size { font:800 28px/2 "Courier New", monospace; }
		#size_ref { font:600 20px/1.5 "Courier New", monospace; }
		.big { font-size:32px; }
		#track { letter-spacing:normal; word-spacing:-8px;
			transition:letter-spacing 120s linear -60s, word-spacing 120s linear -60s; }
		.go #track { letter-spacing:20%; word-spacing:-10%; }
		#track_ref { letter-spacing:10%; word-spacing:calc(-4px - 5%); }
		#wrap { width:45px; word-spacing:normal; transition:word-spacing 120s linear -60s; }
		.go #wrap { word-spacing:24px; }
		#line_px { line-height:20px; transition:line-height 120s linear -60s; }
		.go #line_px { line-height:40px; }
		.line_kind { line-height:1; transition:line-height 120s linear -30s; }
		#line_hold { transition-behavior:allow-discrete; }
		.go .line_kind { line-height:40px; }
		#indent { width:120px; text-indent:-10px each-line; transition:text-indent 120s linear -60s; }
		.go #indent { text-indent:25% each-line; }
		#hang { text-indent:10px; transition:text-indent 120s linear -30s allow-discrete; }
		.go #hang { text-indent:30px hanging each-line; }
		.tabs { white-space:pre; }
		@keyframes Tabs { from { tab-size:2; } to { tab-size:6; } }
		#tabs { animation:Tabs 120s linear -60s paused; }
		#tabs_ref { tab-size:4; }
		#tabs_switch { tab-size:2; transition:tab-size 120s linear -30s allow-discrete; }
		.go #tabs_switch { tab-size:48px; }
		#tabs_start { tab-size:2; }
		.decoration { position:absolute; top:310px; line-height:32px; text-decoration-line:underline; }
		#underline { left:10px; text-decoration-thickness:2px; text-underline-offset:-2px; text-decoration-color:red;
			transition:text-decoration 120s linear -60s, text-underline-offset 120s linear -60s; }
		.go #underline { text-decoration-thickness:6px; text-underline-offset:6px; text-decoration-color:blue; }
		#underline_ref { left:100px; text-decoration-thickness:4px; text-underline-offset:2px;
			text-decoration-color:rgb(50% 0% 50%); }
	</style><body id="scene">
		<div class="row"><span class="probe" id="size">SIZE<br><span class="big" id="large">LARGE</span></span>
			<span class="probe" id="size_ref">SIZE<br><span class="big" id="large_ref">LARGE</span></span></div>
		<div class="row" id="track"><span class="probe big" id="track_text">ABC D</span></div>
		<div class="row" id="track_ref"><span class="probe big" id="track_ref_text">ABC D</span></div>
		<div id="wrap">X <span id="wrap_end">Y</span></div>
		<div id="line_px">A<br>B</div>
		<div class="line_kind" id="line_hold">A</div><div class="line_kind" id="line_jump">A</div>
		<div id="indent"><span id="first">First</span><br><span id="second">Second</span></div>
		<div id="hang"><span id="hang_first">First</span><br><span id="hang_second">Second</span></div>
		<div class="tabs" id="tabs">A	<span id="tabbed">B</span></div>
		<div class="tabs" id="tabs_ref">A	<span id="tabbed_ref">B</span></div>
		<div class="tabs" id="tabs_switch">A	<span id="tab_switch_end">B</span></div>
		<div class="tabs" id="tabs_start">A	<span id="tab_start_end">B</span></div>
		<div class="decoration" id="underline">ABC</div><div class="decoration" id="underline_ref">ABC</div>
	</body>"""
	_check(layout.parse(html) == OK and layout.layout(viewport_size) == OK, "Lay out the initial text and font values")
	_check(is_equal_approx(_rect(layout, "wrap").size.y, 24.0), "The initial line fits before spacing grows")
	layout.get_element_by_id("scene").set_attribute("class", "go")
	_check(layout.layout(viewport_size) == OK, "Apply interpolated text and font values")
	_check(_rect(layout, "size").size.is_equal_approx(_rect(layout, "size_ref").size)
		and absf(_rect(layout, "large").position.y - _rect(layout, "large_ref").position.y) < 0.01,
		"The font shorthand animates size and weight and inherits the sampled line-height multiplier")
	_check(is_equal_approx(_rect(layout, "track_text").size.x, _rect(layout, "track_ref_text").size.x),
		"Normal spacing interpolates from zero and inherited percentages use the descendant's font size")
	_check(is_equal_approx(_rect(layout, "wrap").size.y, 48.0)
		and _rect(layout, "wrap_end").position.y > _rect(layout, "wrap").position.y + 20.0,
		"Animated spacing makes the text reflow onto a second line")
	_check(is_equal_approx(_rect(layout, "line_px").size.y, 60.0)
		and is_equal_approx(_rect(layout, "line_hold").size.y, 20.0)
		and is_equal_approx(_rect(layout, "line_jump").size.y, 40.0),
		"Line-height interpolates lengths and switches discretely between number and length types")
	_check(is_equal_approx(_rect(layout, "first").position.x, 10.0)
		and is_equal_approx(_rect(layout, "second").position.x, 10.0)
		and is_equal_approx(_rect(layout, "hang_first").position.x, 10.0)
		and is_zero_approx(_rect(layout, "hang_second").position.x),
		"Text-indent mixes signed lengths and percentages and preserves each-line or switches flags discretely")
	_check(is_equal_approx(_rect(layout, "tabbed").position.x, _rect(layout, "tabbed_ref").position.x)
		and is_equal_approx(_rect(layout, "tab_switch_end").position.x, _rect(layout, "tab_start_end").position.x),
		"Tab stops follow animated space multipliers and keep their type at a discrete sample")
	var text_position: int = layout.get_plain_text().find("X Y")
	var selection: Array[Rect2] = layout.get_selection_rects(text_position, text_position + 3)
	_check(text_position >= 0 and selection.size() == 2 and selection[1].position.y > selection[0].position.y,
		"Text selection geometry follows the animated line breaks")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = viewport_size
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var canvas: Node2D = Node2D.new()
	canvas.draw.connect(func() -> void: layout.draw(canvas))
	viewport.add_child(canvas)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var color: Color = Color(0.5, 0.0, 0.5)
	var underline: Rect2i = _color_bounds(image, Rect2i(10, 310, 60, 32), color)
	var underline_reference: Rect2i = _color_bounds(image, Rect2i(100, 310, 60, 32), color)
	_check(underline.has_area() and underline.size == underline_reference.size
		and underline.position + Vector2i(90, 0) == underline_reference.position,
		"Animated decoration color, thickness, and offset match a static midpoint in the painted output")
	layout.clear()
	viewport.queue_free()


func _check_font_axis_motion() -> void:
	var source: HTMLDocument = load("{FONT_FACE_UID}")
	if source == null:
		return
	var assets: Dictionary = source.get("_local_fonts")
	var variable: FontFile = assets["res://fonts/bahnschrift.ttf"]
	var document: HTMLDocument = HTMLDocument.new()
	document.set("_local_fonts", assets)
	document.text = """<!doctype html><style>
		@font-face { font-family: Axes; src:url(fonts/bahnschrift.ttf); font-width:75% 100%; font-weight:300 700; }
		body { margin:0; font:400 40px/1 Axes; }
		#axis { width:7ch; height:12px; font-width:75%; transition:font-stretch 120s linear -60s; }
		.go #axis { font-width:100%; }
	</style><body id="scene"><div id="axis"></div></body>"""
	var layout: HTMLLayout = document.create_layout()
	var size: Vector2i = Vector2i(300, 80)
	_check(layout.layout(size) == OK and absf(_rect(layout, "axis").size.x - _variable_width_metric(variable, 75.0)) <= 1.0,
		"The variable font starts at its specified width axis")
	layout.get_element_by_id("scene").set_attribute("class", "go")
	_check(layout.layout(size) == OK and absf(_rect(layout, "axis").size.x - _variable_width_metric(variable, 87.5)) <= 1.0,
		"The font-stretch alias animates font-width and recomputes ch from the fractional width axis")
	layout.clear()


func _check_animated_font_cache() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><style>
		body { margin:0; font:20px/24px monospace; }
		span { display:inline-block; }
		@keyframes Tracking { from { letter-spacing:0px; } to { letter-spacing:var(--end, 0px); } }
		#changing { animation:Tracking 120s linear -60s paused; }
	</style><div><span id="changing">TEXT</span><span id="stable">TEXT</span></div>"""
	var size: Vector2i = Vector2i(350, 80)
	_check(layout.parse(html) == OK and layout.layout(size) == OK, "Create an animation with a resolved font at each sample")
	var changing: HTMLElement = layout.get_element_by_id("changing")
	var base_width: float = _rect(layout, "changing").size.x
	for step: int in range(1, 291):
		changing.set_attribute("style", "--end:%spx" % (step * 0.06))
		if layout.layout(size) != OK:
			_check(false, "Reflow animated tracking across hundreds of distinct font descriptions")
			return
	_check(absf(_rect(layout, "changing").size.x - base_width - 26.1) <= 0.1
		and is_equal_approx(_rect(layout, "stable").size.x, base_width),
		"Repeated animated font updates keep the live text metrics after font-cache pruning")
	layout.clear()


func _check_group_opacity() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(232, 128)
	viewport.transparent_bg = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(232.0, 128.0)
	label.text = """<!doctype html><style>
		body { margin: 0; }
		#backdrop { position: absolute; inset: 0 0 auto; height: 106px; background: black; }
		.group { position: absolute; top: 8px; width: 52px; height: 40px; background: red; }
		.blue { position: absolute; left: 19px; top: 5px; width: 30px; height: 28px; background: blue; z-index: 9; }
		#group { left: 8px; --fade: 25%; opacity: calc(var(--fade) * 2); }
		#nested { left: 68px; opacity: 50%; }
		#nested .blue { opacity: inherit; }
		#outside { position: absolute; z-index: 1; top: 14px; left: 39px; width: 12px; height: 10px; background: lime; }
		#zero { left: 128px; opacity: -2; }
		#clipped { left: 172px; transform: translate(5px, 0); background: blue; opacity: .5;
			border-radius: 14px; overflow: hidden; }
		#gradient { width: 70px; height: 48px; background: linear-gradient(red, red); }
		#positioned_parent { position: absolute; top: 58px; left: 8px; width: 100px; height: 34px; }
		#static_alpha { margin-left: 24px; width: 48px; height: 30px; opacity: .5; }
		#absolute_child { position: absolute; top: 4px; left: 5px; width: 10px; height: 10px; background: white; }
		#transparent, #rgba { position: absolute; top: 110px; width: 18px; height: 12px; }
		#transparent { background: blue; opacity: .5; }
		#rgba { left: 24px; background: rgb(0 0 255 / 50%); }
	</style><div id="backdrop"></div><div class="group" id="group"><div class="blue"></div></div>
	<div class="group" id="nested"><div class="blue"></div></div><div id="outside"></div>
	<a class="group" id="zero" href="#activate">Invisible link</a>
	<div class="group" id="clipped"><div id="gradient"></div></div>
	<div id="positioned_parent"><div id="static_alpha"><div id="absolute_child"></div></div></div>
	<div id="transparent"></div><div id="rgba"></div>"""
	var links: Array[String] = []
	label.link_pressed.connect(func(uri: String) -> void: links.append(uri))
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(13.0, 24.0), Color(0.5, 0.0, 0.0), "CSS math opacity applies to the group background")
	_check_pixel(image, Vector2(31.0, 24.0), Color(0.0, 0.0, 0.5), "An opaque child covers the parent before group blending")
	_check_pixel(image, Vector2(94.0, 24.0), Color(0.25, 0.0, 0.25), "Nested inherited opacity uses two isolated groups")
	_check_pixel(image, Vector2(44.0, 17.0), Color.LIME, "A child z-index stays inside its opacity context")
	_check_pixel(image, Vector2(157.0, 43.0), Color.BLACK, "Opacity clamps negative values to zero")
	_check_pixel(image, Vector2(177.0, 8.0), Color.BLACK, "Rounded clipping follows a transformed opacity group")
	_check_pixel(image, Vector2(190.0, 23.0), Color(0.5, 0.0, 0.0), "A clipped gradient is composited with the rest of its group")
	_check_pixel(image, Vector2(16.0, 65.0), Color(0.5, 0.5, 0.5), "An absolute descendant of an opacity group uses its positioned ancestor")
	_check(_same_color(image.get_pixel(6, 115), image.get_pixel(30, 115)) and
		absf(image.get_pixel(6, 115).a - 0.5) < 0.02, "The group preserves premultiplied color and alpha on a transparent canvas")
	_drag_pointer(viewport, Vector2(152.0, 35.0), Vector2(152.0, 35.0))
	_check(links == ["#activate"], "Zero opacity preserves pointer targeting")

	label.get_element_by_id("group").set_attribute("style", "opacity: 150%;")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), Vector2(31.0, 24.0), Color.BLUE,
		"Opacity clamps to one and removes the composite when the style changes")

	viewport.size = Vector2i(2180, 40)
	label.size = Vector2(2180.0, 40.0)
	label.text = """<!doctype html><style>body { margin: 0; background: black; }
		#wide { width: 2160px; height: 20px; background: red; opacity: .5; }
	</style><div id="wide"></div>"""
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(2047.0, 8.0), Color(0.5, 0.0, 0.0), "The first opacity tile fills its last column")
	_check_pixel(image, Vector2(2048.0, 8.0), Color(0.5, 0.0, 0.0), "The next opacity tile begins without a seam")
	viewport.queue_free()


func _check_filters() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(236, 132)
	viewport.transparent_bg = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(236.0, 132.0)
	label.css_diagnostics_enabled = true
	label.text = """<!doctype html><style>
		body { margin: 0; }
		#backdrop { position: absolute; left: 0; top: 0; width: 236px; height: 104px; background: black; }
		.swatch { position: absolute; top: 8px; width: 24px; height: 24px; background: red; }
		#sequence { left: 8px; --value: 50%; filter: invert() brightness(var(--value)); opacity: .5; }
		#gray { left: 40px; filter: grayscale(130%); }
		#sepia { left: 72px; filter: sepia(); }
		#hue { left: 104px; filter: hue-rotate(.5turn); }
		#saturation { left: 136px; filter: saturate(0); }
		#contrast { left: 168px; background: #404040; filter: brightness(200%) contrast(2); }
		#invalid { left: 200px; filter: invert(1); filter: invert(-1) brightness(1); }
		#blur { position: absolute; left: 12px; top: 60px; width: 16px; height: 16px; background: white; filter: blur(2px); }
		#shadow { position: absolute; left: 54px; top: 60px; width: 14px; height: 16px; background: red; color: lime;
			transform-origin: 0 0; transform: scaleX(1.5); filter: drop-shadow(10px 0 1px); }
		#round { position: absolute; left: 112px; top: 60px; width: 36px; height: 26px; overflow: hidden; border-radius: 10px; }
		#ink { position: absolute; left: -15px; top: 0; width: 12px; height: 16px; background: red; filter: drop-shadow(17px 0 lime); }
		#container { margin: 58px 0 0 164px; width: 52px; height: 30px; filter: brightness(); }
		#abs, #fixed { background: white; width: 6px; height: 6px; top: 5px; }
		#abs { position: absolute; left: 3px; }
		#fixed { position: fixed; right: 4px; }
		#outside { position: absolute; left: -12px; top: 90px; width: 10px; height: 10px; background: red;
			filter: drop-shadow(20px 0 lime); }
		#alpha, #ref { position: absolute; top: 110px; width: 16px; height: 10px; }
		#alpha { left: 8px; background: lime; filter: opacity(calc(25% * 2)); }
		#ref { left: 32px; background: rgb(0 255 0 / 50%); }
	</style><div id="backdrop"></div><div class="swatch" id="sequence"></div><div class="swatch" id="gray"></div>
	<div class="swatch" id="sepia"></div><div class="swatch" id="hue"></div><div class="swatch" id="saturation"></div>
	<div class="swatch" id="contrast"></div><div class="swatch" id="invalid"></div><div id="blur"></div><div id="shadow"></div>
	<div id="round"><div id="ink"></div></div><div id="container"><div id="abs"></div><div id="fixed"></div></div>
	<div id="outside"></div><div id="alpha"></div><div id="ref"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(20.0, 20.0), Color(0.0, 0.25, 0.25), "Filters execute in source order before group opacity")
	_check_pixel(image, Vector2(52.0, 20.0), Color(0.2126, 0.2126, 0.2126), "Grayscale clamps above 100 percent")
	_check_pixel(image, Vector2(84.0, 20.0), Color(0.393, 0.349, 0.272), "Sepia uses its default amount")
	_check_pixel(image, Vector2(116.0, 20.0), Color(0.0, 0.426, 0.426), "Hue rotation resolves angle units")
	_check_pixel(image, Vector2(148.0, 20.0), Color(0.213, 0.213, 0.213), "Zero saturation yields the luminance channel")
	_check_pixel(image, Vector2(180.0, 20.0), Color(0.504, 0.504, 0.504), "Brightness and contrast accept amounts above one")
	_check_pixel(image, Vector2(212.0, 20.0), Color.CYAN, "An invalid filter list preserves the previous declaration")
	var blur: HTMLElement = label.get_element_by_id("blur")
	var halo_pixel: float = image.get_pixel(10, 68).r
	_check(halo_pixel > 0.12 and halo_pixel < 0.48 and image.get_pixel(20, 68).r > 0.97,
		"Gaussian blur spreads color past the source edge and preserves the center")
	_check(blur.get_bounding_rect() == Rect2(12.0, 60.0, 16.0, 16.0) and label.get_element_at(Vector2(10.0, 68.0)) != blur,
		"Filter ink leaves the element's layout box and pointer area intact")
	_check_pixel(image, Vector2(83.0, 68.0), Color.LIME, "Drop shadows use currentColor and transform the local offset")
	_check_pixel(image, Vector2(60.0, 68.0), Color.RED, "A drop shadow composites behind its source")
	_check_pixel(image, Vector2(117.0, 70.0), Color.LIME, "An ancestor clips a filtered element after its source has been filtered")
	_check_pixel(image, Vector2(115.0, 60.0), Color.BLACK, "The ancestor's rounded corner clips the composited filter")
	_check_pixel(image, Vector2(13.0, 94.0), Color.LIME, "Filter ink from an offscreen source can enter the viewport")
	_check(_same_color(image.get_pixel(12, 114), image.get_pixel(36, 114)) and
		absf(image.get_pixel(12, 114).a - 0.5) < 0.02, "The opacity filter preserves the output alpha")
	var container_rect: Rect2 = label.get_element_by_id("container").get_bounding_rect()
	var abs_rect: Rect2 = label.get_element_by_id("abs").get_bounding_rect()
	var fixed_rect: Rect2 = label.get_element_by_id("fixed").get_bounding_rect()
	_check(abs_rect.position - container_rect.position == Vector2(3.0, 5.0) and
		fixed_rect.position - container_rect.position == Vector2(42.0, 5.0),
		"A filter establishes the containing block for absolute and fixed descendants")
	_check(label.get_element_by_id("invalid").get_css_diagnostics().size() == 1,
		"The failed declaration reports an unsupported filter value")

	viewport.size = Vector2i(2180, 132)
	label.size = Vector2(2180.0, 132.0)
	label.text = """<!doctype html><style>body { margin: 0; background: black; }
		#wide { width: 2160px; height: 124px; position: relative; filter: blur(24px); }
		#wide div { position: absolute; top: 12px; width: 80px; height: 96px; background: white; }
	</style><div id="wide"><div style="left: 8px;"></div><div style="left: 2008px;"></div></div>"""
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	var before_seam: float = image.get_pixel(2047, 60).r
	var after_seam: float = image.get_pixel(2048, 60).r
	_check(before_seam > 0.8 and absf(before_seam - after_seam) < 0.02, "Wide blurs carry a sampling margin across the tile boundary")
	viewport.queue_free()


func _check_backdrop_filters() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(358, 224)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.css_diagnostics_enabled = true
	label.text = """<!doctype html><style>
		html { background: blue; } body { margin: 0; }
		.tile { position: absolute; top: 10px; width: 60px; height: 40px; }
		#basic { left: 10px; --invert: invert(100%); backdrop-filter: var(--invert); border-radius: 12px; }
		#basic > b { position: absolute; left: 10px; top: 10px; width: 14px; height: 20px; background: red; }
		#same, #again { left: 95px; backdrop-filter: invert(); }
		#opacity { left: 175px; backdrop-filter: invert(); opacity: .5; }
		#invalid { left: 255px; backdrop-filter: invert(); backdrop-filter: blur(-1px); }
		#over { position: absolute; left: 275px; top: 22px; width: 20px; height: 20px; background: lime; }
		#transformed { left: 10px; top: 68px; transform: translateX(4px); z-index: 0; }
		#transformed > b { display: block; width: 50px; height: 40px; backdrop-filter: invert(); }
		#isolated { left: 95px; top: 68px; opacity: .5; }
		#isolated > i { position: absolute; width: 30px; height: 40px; background: red; }
		#isolated > b { position: absolute; inset: 0; backdrop-filter: invert(); }
		#nested { left: 175px; top: 68px; backdrop-filter: invert(); }
		#nested > b { position: absolute; inset: 3px; backdrop-filter: invert(); }
		#filters { left: 255px; top: 68px; backdrop-filter: invert(); filter: invert(); }
		#filters > b { position: absolute; left: 10px; width: 14px; height: 30px; background: red; }
		#stripe, #blur { left: 10px; top: 126px; }
		#stripe { background: linear-gradient(to right, red 50%, white 50%); }
		#blur { backdrop-filter: blur(4px); }
		#rotated { left: 108px; top: 128px; width: 44px; height: 26px; transform: rotate(35deg); backdrop-filter: invert(); }
		#clip { left: 175px; top: 126px; overflow: hidden; border-radius: 14px; }
		#clip > b { position: absolute; inset: 0; backdrop-filter: invert(); }
		#container { margin: 126px 0 0 255px; width: 60px; height: 40px; backdrop-filter: brightness(1); }
		#abs, #fixed { width: 5px; height: 6px; top: 2px; }
		#abs { position: absolute; left: 3px; } #fixed { position: fixed; right: 4px; }
		#depth { position: absolute; left: 10px; top: 180px; width: 106px; height: 30px;
			background: red; transform-style: preserve-3d; }
		#depth i, #depth b { position: absolute; inset: 0; }
		#depth i { background: blue; }
		#depth b { transform: rotateY(42deg); backdrop-filter: invert(); }
		#blend { position: absolute; left: 150px; top: 180px; width: 84px; height: 30px; isolation: isolate; }
		#blend i { position: absolute; inset: 0 50% 0 0; background: red; mix-blend-mode: multiply; }
		#blend b { position: absolute; inset: 0; backdrop-filter: invert(); }
		@supports (backdrop-filter: invert()) { #container { background: lime; } }
	</style><div id="basic" class="tile"><b></b></div><div id="same" class="tile"></div>
	<div id="again" class="tile"></div><div id="opacity" class="tile"></div>
	<div id="invalid" class="tile"></div><div id="over"></div>
	<div id="transformed" class="tile"><b></b></div><div id="isolated" class="tile"><i></i><b></b></div>
	<div id="nested" class="tile"><b></b></div><div id="filters" class="tile"><b></b></div>
	<div id="stripe" class="tile"></div><div id="blur" class="tile"></div>
	<div id="rotated" class="tile"></div><div id="clip" class="tile"><b></b></div>
	<div id="container"><div id="abs"></div><div id="fixed"></div></div>
	<div id="depth"><i></i><b></b></div><div id="blend"><i></i><b></b></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(55.0, 25.0), Color.YELLOW, "Backdrop filtering reads earlier ink behind a transparent box")
	_check_pixel(image, Vector2(25.0, 25.0), Color.RED, "The element's foreground is drawn after backdrop filtering")
	_check_pixel(image, Vector2(11.0, 11.0), Color.BLUE, "Backdrop filtering respects the element's rounded border")
	_check_pixel(image, Vector2(122.0, 25.0), Color.BLUE, "Successive filters read the preceding composited result")
	_check_pixel(image, Vector2(205.0, 25.0), Color(0.5, 0.5, 0.5), "Group opacity attenuates the filtered backdrop")
	_check_pixel(image, Vector2(266.0, 33.0), Color.YELLOW, "An invalid declaration preserves the earlier filter")
	_check_pixel(image, Vector2(283.0, 33.0), Color.LIME, "Later content is excluded from backdrop capture")
	_check_pixel(image, Vector2(43.0, 83.0), Color.YELLOW, "A transform and z-index do not establish a Backdrop Root")
	_check_pixel(image, Vector2(105.0, 83.0), Color(0.0, 0.5, 1.0), "A backdrop root exposes its own earlier content")
	_check_pixel(image, Vector2(140.0, 83.0), Color.BLUE, "An opacity backdrop root excludes earlier content from outside")
	_check_pixel(image, Vector2(200.0, 84.0), Color.YELLOW, "A nested filter cannot read its ancestor's filtered backdrop")
	_check_pixel(image, Vector2(268.0, 85.0), Color.CYAN, "The foreground filter applies after backdrop and foreground composition")
	_check_pixel(image, Vector2(301.0, 85.0), Color.BLUE, "The foreground filter also applies to the filtered backdrop")
	_check_pixel(image, Vector2(11.0, 146.0), Color.RED, "Blur reflects the backdrop at the filter's border instead of sampling outside")
	var boundary: Color = image.get_pixel(40, 146)
	_check(boundary.r > 0.99 and boundary.g > 0.3 and boundary.g < 0.7, "Backdrop blur mixes both sides of an interior edge")
	_check_pixel(image, Vector2(130.0, 140.0), Color.YELLOW, "The backdrop uses the transformed border quad")
	_check_pixel(image, Vector2(108.0, 128.0), Color.BLUE, "The rotated filter leaves pixels outside its border quad alone")
	_check_pixel(image, Vector2(177.0, 128.0), Color.BLUE, "An ancestor's rounded overflow clips backdrop output")
	_check_pixel(image, Vector2(209.0, 138.0), Color.YELLOW, "Filtered pixels inside the ancestor clip remain visible")
	_check_pixel(image, Vector2(277.0, 147.0), Color.LIME, "Backdrop filter declarations work in feature queries")
	_check_pixel(image, Vector2(36.0, 196.0), Color.YELLOW, "The front slice of an intersecting plane filters the plane behind it")
	_check_pixel(image, Vector2(94.0, 196.0), Color.BLUE, "The plane in front hides the rear slice of a backdrop filter")
	_check_pixel(image, Vector2(177.0, 196.0), Color.CYAN, "A backdrop filter can sample local blended ink")
	_check_pixel(image, Vector2(211.0, 196.0), Color.YELLOW, "An isolated blend context also exposes its external backdrop to filters")
	var container: Rect2 = label.get_element_by_id("container").get_bounding_rect()
	_check(label.get_element_by_id("abs").get_bounding_rect().position - container.position == Vector2(3.0, 2.0) and
		label.get_element_by_id("fixed").get_bounding_rect().position - container.position == Vector2(51.0, 2.0),
		"A backdrop filter establishes the containing block for absolute and fixed descendants")
	var invalid: HTMLElement = label.get_element_by_id("invalid")
	_check(invalid.get_css_diagnostics().size() == 1, "The invalid backdrop filter reports one diagnostic")

	viewport.size = Vector2i(2176, 88)
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>html { background: blue; } body { margin: 0; }
		.tile { position: absolute; inset: 0 auto auto 0; width: 2160px; height: 70px; }
		#stripe { background: linear-gradient(to right, red 2048px, white 2048px); }
		#blur { backdrop-filter: blur(8px); }
	</style><div class="tile" id="stripe"></div><div class="tile" id="blur"></div>"""
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	var before_seam: float = image.get_pixel(2047, 35).g
	var after_seam: float = image.get_pixel(2048, 35).g
	_check(before_seam > 0.4 and before_seam < 0.6 and after_seam > before_seam and absf(before_seam - after_seam) < 0.1,
		"Backdrop filters retain neighboring samples across viewport tiles, got %s and %s" % [before_seam, after_seam])
	viewport.queue_free()


func _check_will_change() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var position_cases: Dictionary[String, Vector2i] = {
		"transform": Vector2i(1, 1),
		"TrAnSlAtE": Vector2i(1, 1),
		"transform-style": Vector2i(1, 1),
		"perspective": Vector2i(1, 1),
		"offset": Vector2i(1, 1),
		"offset-position": Vector2i(1, 1),
		"filter": Vector2i(1, 1),
		"backdrop-filter": Vector2i(1, 1),
		"contain": Vector2i(1, 1),
		"content-visibility": Vector2i(1, 1),
		"position": Vector2i(1, 0),
		"opacity": Vector2i.ZERO,
		"backface-visibility": Vector2i.ZERO,
		"offset-distance": Vector2i.ZERO,
		"mask": Vector2i.ZERO,
		"z-index": Vector2i.ZERO,
		"scroll-position, contents, --transform, unknown-property": Vector2i.ZERO,
		"auto": Vector2i.ZERO,
	}
	var frame: String = """<!doctype html><style>
		body { margin: 0; } #host { margin: 10px 20px; width: 60px; height: 40px; }
		#absolute, #fixed { top: 2px; left: 3px; width: 5px; height: 5px; }
		#absolute { position: absolute; } #fixed { position: fixed; }
	</style><div id="host" style="will-change:%s"><b id="absolute"></b><b id="fixed"></b></div>"""
	for hint: String in position_cases:
		_check(layout.parse(frame % hint) == OK and layout.layout(Vector2i(280, 150)) == OK,
			"Lay out will-change: %s" % hint)
		var host: Vector2 = layout.get_element_by_id("host").get_bounding_rect().position
		var absolute: Vector2 = layout.get_element_by_id("absolute").get_bounding_rect().position
		var fixed: Vector2 = layout.get_element_by_id("fixed").get_bounding_rect().position
		var result: Vector2i = Vector2i(absolute - host == Vector2(3.0, 2.0), fixed - host == Vector2(3.0, 2.0))
		_check(result == position_cases[hint], "Positioning with will-change: %s gives %s" % [hint, result])

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(458, 188)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.css_diagnostics_enabled = true
	label.text = """<!doctype html><style>
		html { background: blue; } body { margin: 0; }
		.tile { position: absolute; top: 10px; width: 62px; height: 38px; z-index: 0; }
		.a { left: 10px; } .b { left: 86px; } .c { left: 162px; }
		.d { left: 238px; } .e { left: 314px; } .f { left: 390px; }
		.host { width: 58px; height: 36px; }
		.relative { position: relative; }
		.high, .cover { position: absolute; inset: 7px auto auto 10px; width: 30px; height: 24px; }
		.high { background: lime; z-index: 3; } .cover { background: red; z-index: 1; }
		#opacity_host { --effect: opacity; will-change: var(--effect); will-change: opacity, none; }
		#static_host, #relative_host, #grid_host, #flex_host { will-change: z-index; }
		.grid { display: grid; } .flex { display: flex; }
		.glass { top: 68px; } .glass b { position: absolute; inset: 0; backdrop-filter: invert(); }
		.glass i { position: absolute; inset: 0 50% 0 0; background: red; }
		#opacity_root { will-change: opacity; } #filter_root { will-change: filter; }
		#mask_root { will-change: mask; } #blend_root { will-change: mix-blend-mode; }
		#transform_root { will-change: transform; } #isolate_root { will-change: isolation; }
		.banner { top: 132px; background: fixed top left / 40px 48px
			repeating-linear-gradient(to bottom, red 0 24px, blue 24px 48px); }
		#transform_bg { will-change: transform; }
		#supports { top: 132px; background: blue; }
		@supports (will-change: opacity, contents, --duration, future-feature) { #supports { background: lime; } }
		@supports (will-change: contents, default) { #supports { background: yellow; } }
	</style>
	<div class="tile a"><div class="host relative"><b class="high"></b></div><b class="cover"></b></div>
	<div class="tile b"><div class="host relative" id="opacity_host"><b class="high"></b></div><b class="cover"></b></div>
	<div class="tile c"><div class="host" id="static_host"><b class="high"></b></div><b class="cover"></b></div>
	<div class="tile d"><div class="host relative" id="relative_host"><b class="high"></b></div><b class="cover"></b></div>
	<div class="tile e"><div class="grid"><div class="host" id="grid_host"><b class="high"></b></div></div><b class="cover"></b></div>
	<div class="tile f"><div class="flex"><div class="host" id="flex_host"><b class="high"></b></div></div><b class="cover"></b></div>
	<div class="tile glass a" id="opacity_root"><i></i><b></b></div><div class="tile glass b" id="filter_root"><b></b></div>
	<div class="tile glass c" id="mask_root"><b></b></div><div class="tile glass d" id="blend_root"><b></b></div>
	<div class="tile glass e" id="transform_root"><b></b></div><div class="tile glass f" id="isolate_root"><b></b></div>
	<div class="tile banner a"></div><div class="tile banner b" id="transform_bg"></div><div class="tile c" id="supports"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var stacking: Array[Color] = [Color.LIME, Color.RED, Color.LIME, Color.RED, Color.RED, Color.RED,]
	for index: int in range(stacking.size()):
		_check_pixel(image, Vector2(30.0 + 76.0 * index, 25.0), stacking[index], "Will-change stacking case %d" % index)
	_check_pixel(image, Vector2(20.0, 80.0), Color.CYAN, "An opacity hint exposes earlier local paint")
	_check_pixel(image, Vector2(59.0, 80.0), Color.BLUE, "An opacity hint excludes external paint from the backdrop")
	_check_pixel(image, Vector2(106.0, 80.0), Color.BLUE, "A filter hint forms a Backdrop Root")
	_check_pixel(image, Vector2(182.0, 80.0), Color.BLUE, "The mask shorthand forms a Backdrop Root")
	_check_pixel(image, Vector2(258.0, 80.0), Color.BLUE, "An element blending hint forms a Backdrop Root")
	_check_pixel(image, Vector2(334.0, 80.0), Color.YELLOW, "A transform hint keeps access to the outer backdrop")
	_check_pixel(image, Vector2(410.0, 80.0), Color.YELLOW, "An isolation hint does not form a Backdrop Root")
	_check_pixel(image, Vector2(30.0, 137.0), Color.BLUE, "A fixed background samples at the viewport")
	_check_pixel(image, Vector2(106.0, 137.0), Color.RED, "A transform hint anchors a fixed background to its element")
	_check_pixel(image, Vector2(182.0, 137.0), Color.LIME, "The hint accepts custom names and rejects reserved features")

	var opacity_host: HTMLElement = label.get_element_by_id("opacity_host")
	_check(opacity_host.get_css_diagnostics().size() == 1, "One invalid hint list is diagnosed")
	_check(opacity_host.set_attribute("style", "will-change: auto") == OK, "Clear the stacking hint through the DOM")
	var opacity_root: HTMLElement = label.get_element_by_id("opacity_root")
	_check(opacity_root.set_attribute("style", "will-change: auto") == OK, "Clear the backdrop hint through the DOM")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(106.0, 25.0), Color.LIME, "Removing a hint rebuilds stacking order")
	_check_pixel(image, Vector2(59.0, 80.0), Color.YELLOW, "Removing a hint restores outer backdrop sampling")
	viewport.queue_free()


func _check_user_select() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 180)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.css_diagnostics_enabled = true
	var selection_color: Color = Color("#d200d2")
	label.add_theme_color_override("selection_color", selection_color)
	label.text = """<!doctype html><style>
		html { background: white; } body { margin: 8px; font: 16px/24px monospace; }
		#blocked { --policy: none; -webkit-user-select: var(--policy); }
		#allowed { user-select: text; }
		#atomic { user-select: all; }
	</style><div><span id="lead">HEAD</span></div>
	<div id="blocked"><span id="hidden">HIDDEN</span> <span id="allowed">REOPEN</span> SECRET</div>
	<div><span id="atomic">TOKEN <b id="nested">ALL</b></span></div>
	<div><span id="end">END</span></div>
	<button id="button">COMMAND</button>
	<input id="field" style="user-select: none" value="Text">
	<input id="read_only" style="user-select: none" value="Text" readonly>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	label.select_all()
	await RenderingServer.frame_post_draw
	var all_text: String = label.get_selected_text()
	_check("HEAD" in all_text and "REOPEN" in all_text and "TOKEN ALL" in all_text and "END" in all_text and
		"HIDDEN" not in all_text and "SECRET" not in all_text and "COMMAND" not in all_text,
		"Copying a range excludes user-select none and includes a descendant that restores text selection")
	var hidden: Rect2 = label.get_element_by_id("hidden").get_bounding_rect()
	var allowed: Rect2 = label.get_element_by_id("allowed").get_bounding_rect()
	var pixels: Image = viewport.get_texture().get_image()
	_check(not _color_bounds(pixels, Rect2i(hidden), selection_color).has_area() and
		_color_bounds(pixels, Rect2i(allowed), selection_color).has_area(), "CSS exclusion applies to selection paint")
	var lead: Rect2 = label.get_element_by_id("lead").get_bounding_rect()
	_drag_pointer(viewport, hidden.get_center(), lead.get_center())
	_check(label.get_selected_text() == all_text, "Starting on excluded text preserves an existing selection")
	var nested: Rect2 = label.get_element_by_id("nested").get_bounding_rect()
	_drag_pointer(viewport, nested.get_center(), nested.get_center())
	_check(label.get_selected_text() == "TOKEN ALL", "Clicking a descendant selects the outer atomic span")
	_drag_pointer(viewport, lead.position + Vector2(0.5, lead.size.y * 0.5), hidden.get_center())
	_check(label.get_selected_text().strip_edges() == "HEAD", "Dragging into excluded text stops at its leading edge")
	_drag_pointer(viewport, lead.position + Vector2(0.5, lead.size.y * 0.5), nested.get_center())
	_check("TOKEN ALL" in label.get_selected_text() and "HIDDEN" not in label.get_selected_text(),
		"Dragging through an excluded range into atomic text includes the whole atomic span")
	var field: HTMLInputElement = label.get_element_by_id("field") as HTMLInputElement
	_check(label.focus_element(field), "Focus the editable field in the selection test")
	_send_text_history_key(viewport, KEY_A)
	_check(field.get_selection_start() == 0 and field.get_selection_end() == 4,
		"Editable fields keep an independent selection under user-select none")
	var read_only: HTMLInputElement = label.get_element_by_id("read_only") as HTMLInputElement
	_check(label.focus_element(read_only), "Focus the read-only field in the selection test")
	_send_text_history_key(viewport, KEY_A)
	_check(read_only.get_selection_start() == read_only.get_selection_end(), "Read-only fields honor user-select none")
	_check(label.get_element_by_id("blocked").set_attribute("style", "user-select: text") == OK,
		"Override the prefixed declaration through the DOM")
	await process_frame
	await process_frame
	label.select_all()
	_check("HIDDEN" in label.get_selected_text() and label.get_css_diagnostics().is_empty(),
		"A restyle updates copy filtering and accepts the alias and variable")
	label.queue_free()
	await process_frame

	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.size = Vector2(viewport.size)
	markdown.parse_html = true
	markdown.text = "Before\n\n<div>Start <span style='user-select: none'>HIDDEN</span> End</div>\n\nAfter"
	var test_icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(icon, test_icon)
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	markdown.select_all()
	var markdown_text: String = markdown.get_selected_text()
	_check("Before" in markdown_text and "Start" in markdown_text and "End" in markdown_text and "After" in markdown_text and
		"HIDDEN" not in markdown_text, "Markdown copying uses the HTML block's selection policy")
	viewport.queue_free()


func _check_selection_colors() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(340, 184)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.add_theme_color_override("selection_color", Color.MAGENTA)
	label.text = """<!doctype html><style>
		html { background: white; } body { margin: 8px; color: black; font: 28px/38px monospace; }
		:root::selection { color: red; background: lime; }
		#child { --selected-ink: blue; }
		#child::selection { color: var(--selected-ink); }
		#clear::selection { color: currentColor; background-color: initial; }
		#skip { user-select: none; }
		@layer base, theme;
		@layer base { #reset::selection { color: blue; background: cyan; } }
		@layer theme { #reset::selection { color: yellow; background: revert-layer; } }
	</style><div><span id="parent">MMM</span> <span id="child">MMM</span></div>
	<div><span id="clear">MMM</span> <span id="skip">MMM</span> <span id="reset">MMM</span></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	label.select_all()
	await RenderingServer.frame_post_draw
	var pixels: Image = viewport.get_texture().get_image()
	var parent: Rect2i = Rect2i(label.get_element_by_id("parent").get_bounding_rect())
	var child: Rect2i = Rect2i(label.get_element_by_id("child").get_bounding_rect())
	var clear: Rect2i = Rect2i(label.get_element_by_id("clear").get_bounding_rect())
	var skip: Rect2i = Rect2i(label.get_element_by_id("skip").get_bounding_rect())
	var reset: Rect2i = Rect2i(label.get_element_by_id("reset").get_bounding_rect())
	_check(_color_bounds(pixels, parent, Color.LIME).has_area() and _color_bounds(pixels, parent, Color.RED).has_area(),
		"Root selection colors inherit into ordinary text")
	_check(_color_bounds(pixels, child, Color.LIME).has_area() and _color_bounds(pixels, child, Color.BLUE).has_area(),
		"A child selection inherits the parent's highlight background and reads variables from the child")
	_check(not _color_bounds(pixels, clear, Color.LIME).has_area() and not _color_bounds(pixels, clear, Color.MAGENTA).has_area() and
		_color_bounds(pixels, clear, Color.BLACK).has_area(), "An explicit transparent selection uses the originating currentColor")
	_check(not _color_bounds(pixels, skip, Color.LIME).has_area() and _color_bounds(pixels, skip, Color.BLACK).has_area(),
		"Unselectable text retains its normal paint")
	_check(_color_bounds(pixels, reset, Color.CYAN).has_area() and _color_bounds(pixels, reset, Color.YELLOW).has_area(),
		"A selection longhand can roll back independently within the cascade")

	label.text = """<!doctype html><style>
		html { background: white; } body { margin: 8px; color: black; font: 36px/48px monospace; }
		#translated { width: 200px; overflow: hidden; border-radius: 12px; transform: translate(22px, 4px); }
		#partial::selection { color: red; }
		@media (min-width: 352px) { #partial::selection { color: transparent; background-color: lime; } }
	</style><div id="translated"><span id="partial">MMMMMM</span></div>"""
	await process_frame
	await process_frame
	var partial: HTMLElement = label.get_element_by_id("partial")
	var bounds: Rect2 = partial.get_bounding_rect()
	var midpoint: Vector2 = bounds.get_center()
	_drag_pointer(viewport, Vector2(bounds.position.x + 0.5, midpoint.y), midpoint)
	await RenderingServer.frame_post_draw
	pixels = viewport.get_texture().get_image()
	var first: Rect2i = Rect2i(Rect2(bounds.position, Vector2(bounds.size.x * 0.47, bounds.size.y)))
	var last: Rect2i = Rect2i(Rect2(bounds.position + Vector2(bounds.size.x * 0.53, 0.0), Vector2(bounds.size.x * 0.47, bounds.size.y)))
	_check(label.get_selected_text() == "MMM" and _color_bounds(pixels, first, Color.RED).has_area() and
		_color_bounds(pixels, last, Color.BLACK).has_area() and not _color_bounds(pixels, last, Color.RED).has_area(),
		"A partial selection recolors only the selected part of a shaped word in a transformed clip")
	_check(not _color_bounds(pixels, first, Color.MAGENTA).has_area(), "A foreground-only rule suppresses the theme selection background")
	viewport.size = Vector2i(360, 184)
	label.size = Vector2(viewport.size)
	await process_frame
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	pixels = viewport.get_texture().get_image()
	_check(label.get_selected_text() == "MMM" and _color_bounds(pixels, first, Color.LIME).has_area() and
		not _color_bounds(pixels, first, Color.BLACK).has_area() and _color_bounds(pixels, last, Color.BLACK).has_area(),
		"A media rule changes the existing selection and a transparent foreground replaces selected glyphs")
	label.queue_free()
	await process_frame

	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.size = Vector2(viewport.size)
	markdown.parse_html = true
	markdown.text = "<div><style>div::selection { color: red; background-color: lime; }</style>MMMM</div>"
	var test_icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(icon, test_icon)
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	markdown.select_all()
	await RenderingServer.frame_post_draw
	pixels = viewport.get_texture().get_image()
	var view_bounds: Rect2i = Rect2i(Vector2i.ZERO, viewport.size)
	_check(_color_bounds(pixels, view_bounds, Color.LIME).has_area() and _color_bounds(pixels, view_bounds, Color.RED).has_area(),
		"Markdown HTML paints authored selection colors")
	viewport.queue_free()


func _check_masks() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: blue; }
		.tile { position: absolute; top: 10px; width: 60px; height: 36px; background: red; }
		#alpha { left: 10px; --cut: linear-gradient(to right, black 50%, transparent 50%);
			mask: var(--cut); mask: white; }
		#alpha > b { display: block; height: 16px; background: lime; }
		#luminance { left: 90px; mask: linear-gradient(to right, black 50%, white 50%) luminance; }
		#missing { left: 170px; mask-image: url(nonexistent-mask.png); }
		#image { left: 250px; mask-image: url(css-mask-source.png); mask-size: 100% 100%; }
		.layers { top: 65px; mask: linear-gradient(black, black) left top / 40px 100% no-repeat,
			linear-gradient(black, black) right top / 40px 100% no-repeat; }
		#add { left: 10px; } #subtract { left: 90px; mask-composite: subtract; }
		#intersect { left: 170px; mask-composite: intersect; } #exclude { left: 250px; mask-composite: exclude; }
		#extend { left: 10px; top: 125px; transform: translateX(8px); mask: linear-gradient(black, black) no-clip; }
		#extend > b { position: absolute; left: 55px; width: 24px; height: 20px; background: lime; }
		#repeat { left: 130px; top: 125px; width: 65px; mask: linear-gradient(black, black) center / 14px 16px space no-repeat; }
		#origin { left: 225px; top: 125px; width: 30px; padding: 10px; border: 5px solid lime;
			mask: linear-gradient(black, black) top left / 16px 100% no-repeat content-box border-box; }
		#fallback { left: 310px; top: 125px; mask-image: none; mask: none, linear-gradient(black, black); }
		#round, #gray, #all-none, #one-line, #two-lines { top: 183px; }
		#round { left: 10px; border-radius: 18px; mask: linear-gradient(white, white); }
		#round > b { display: block; height: 36px; background: lime; }
		#gray { left: 90px; mask: linear-gradient(red, red) luminance; }
		#all-none { left: 170px; mask-image: none, none; }
		#one-line, #two-lines { position: absolute; font: 20px monospace; color: transparent; line-height: 28px; }
		#one-line { left: 250px; } #two-lines { left: 310px; }
		#one-line span { background: red; mask: linear-gradient(white, white); }
		#two-lines span { background: red; mask: linear-gradient(to right, black 50%, transparent 50%); }
		@supports (mask-image: linear-gradient(black, transparent)) {
			#fallback { mask-composite: subtract; }
		}
	</style><div class="tile" id="alpha"><b id="child"></b></div><div class="tile" id="luminance"></div>
	<div class="tile" id="missing"></div><div class="tile" id="image"></div>
	<div class="tile layers" id="add"></div><div class="tile layers" id="subtract"></div>
	<div class="tile layers" id="intersect"></div><div class="tile layers" id="exclude"></div>
	<div class="tile" id="extend"><b></b></div><div class="tile" id="repeat"></div>
	<div class="tile" id="origin"></div><div class="tile" id="fallback"></div>
	<div class="tile" id="round"><b></b></div><div class="tile" id="gray"></div>
	<div class="tile" id="all-none"></div><div id="one-line"><span id="single">AA</span></div>
	<div id="two-lines"><span id="wrapped">AA<br>AA</span></div>"""
	var alpha: Image = Image.create_empty(60, 36, false, Image.FORMAT_RGBA8)
	alpha.fill_rect(Rect2i(0, 0, 30, 36), Color.BLACK)
	var texture: ImageTexture = ImageTexture.create_from_image(alpha)
	var document: HTMLDocument = HTMLDocument.new()
	document.text = source
	document.set("_textures", { "res://css-mask-source.png": texture })
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(390, 287)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(390.0, 247.0)
	label.css_diagnostics_enabled = true
	label.document = document
	viewport.add_child(label)
	var imported: HTMLDocument = ResourceLoader.load("{MASK_IMAGE_UID}") as HTMLDocument
	_check(imported != null and not FileAccess.file_exists("res://mask-import.png"),
		"Load the mask from the imported document's dependencies after removing CSS and PNG sources")
	var imported_label: HTMLLabel = HTMLLabel.new()
	imported_label.position = Vector2(0.0, 247.0)
	imported_label.size = Vector2(50.0, 40.0)
	imported_label.document = imported
	viewport.add_child(imported_label)
	var standalone: HTMLLayout = HTMLLayout.new()
	_check(standalone.parse("""<style>body { margin: 0; background: blue; }
		div { width: 40px; height: 30px; }
		#masked { filter: drop-shadow(30px 0 lime); opacity: 0.5;
			mask: linear-gradient(black, black) right top / 10px 100% no-repeat; }
		#masked div { width: 10px; background: red; }
	</style><div id="masked"><div></div></div>""") == OK and standalone.layout(Vector2i(50, 40)) == OK,
		"Lay out a mask over filtered content in a standalone layout")
	var canvas: Node2D = Node2D.new()
	var draw_box: Rect2 = Rect2(60.0, 247.0, 50.0, 40.0)
	canvas.draw.connect(func() -> void: standalone.draw(canvas, draw_box.position, draw_box))
	viewport.add_child(canvas)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(120.0, 247.0)
	markdown.size = Vector2(100.0, 40.0)
	markdown.parse_html = true
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(icon, texture)
	markdown.text = ('<div style="width:40px;height:30px;background:blue"><div style="width:40px;height:30px;' +
		'background:red;mask:linear-gradient(black,black) left top / 20px 100% no-repeat"></div></div>')
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(23.0, 15.0), Color.LIME, "The parent mask preserves the covered part of a child")
	_check_pixel(image, Vector2(54.0, 15.0), Color.BLUE, "The parent mask removes the uncovered part of a child")
	_check(label.get_element_at(Vector2(54.0, 15.0)) == label.get_element_by_id("child"),
		"The transparent part of a CSS mask remains part of its element's hit area")
	_check_pixel(image, Vector2(100.0, 25.0), Color.BLUE, "Black luminance masks out the source")
	_check_pixel(image, Vector2(135.0, 25.0), Color.RED, "White luminance keeps the source")
	_check_pixel(image, Vector2(190.0, 25.0), Color.BLUE, "An unavailable mask image is transparent")
	_check_pixel(image, Vector2(265.0, 25.0), Color.RED, "An HTML resource texture supplies mask alpha")
	_check_pixel(image, Vector2(296.0, 25.0), Color.BLUE, "The transparent side of the texture masks out the source")
	var operators: Array[Array] = [
		[Color.RED, Color.RED, Color.RED],
		[Color.RED, Color.BLUE, Color.BLUE],
		[Color.BLUE, Color.RED, Color.BLUE],
		[Color.RED, Color.BLUE, Color.RED],
	]
	for index: int in range(operators.size()):
		for sample: int in range(3):
			var expected: Color = operators[index][sample]
			_check_pixel(image, Vector2(20.0 + 80.0 * index + 20.0 * sample, 80.0), expected,
				"Mask composite operation %d at sample %d" % [index, sample])
	_check_pixel(image, Vector2(92.0, 138.0), Color.LIME, "A transformed no-clip mask covers overflowing ink")
	_check_pixel(image, Vector2(136.0, 143.0), Color.RED, "Mask tiles use their declared size")
	_check_pixel(image, Vector2(145.0, 143.0), Color.BLUE, "Space repeat leaves transparent gaps")
	_check_pixel(image, Vector2(153.0, 143.0), Color.RED, "The next spaced mask tile covers the source")
	_check_pixel(image, Vector2(238.0, 151.0), Color.BLUE, "The mask origin excludes the border and padding")
	_check_pixel(image, Vector2(243.0, 151.0), Color.RED, "A content-box mask origin aligns the image tile")
	_check_pixel(image, Vector2(340.0, 151.0), Color.BLUE, "A none layer participates in subtraction")
	_check_pixel(image, Vector2(12.0, 185.0), Color.BLUE, "The rounded masking area clips descendant pixels")
	_check_pixel(image, Vector2(41.0, 200.0), Color.LIME, "The rounded mask keeps ink at the center")
	_check_pixel(image, Vector2(110.0, 200.0), Color("#3600c9"), "Mask luminance scales opacity by channel weight")
	_check_pixel(image, Vector2(190.0, 200.0), Color.RED, "A list containing only none has no mask")
	var single: HTMLElement = label.get_element_by_id("single")
	_check_pixel(image, single.get_bounding_rect().get_center(), Color.RED, "A single non-atomic inline provides its mask box")
	var wrapped: HTMLElement = label.get_element_by_id("wrapped")
	var fragments: Array[Rect2] = wrapped.get_client_rects()
	_check(fragments.size() == 2, "The masked inline has two fragments")
	if fragments.size() == 2:
		_check_pixel(image, fragments[0].get_center(), Color.RED, "The first fragment uses the first half of the sliced mask")
		_check_pixel(image, fragments[1].get_center(), Color.BLUE, "The last fragment uses the second half of the sliced mask")
	_check_pixel(image, Vector2(9.0, 258.0), Color.RED, "The imported mask URL resolves inside a nested stylesheet")
	_check_pixel(image, Vector2(28.0, 258.0), Color.BLUE, "The imported mask preserves its size and repeat mode")
	_check_pixel(image, Vector2(66.0, 254.0), Color.BLUE, "A mask can remove the input to a drop shadow")
	_check_pixel(image, Vector2(95.0, 254.0), Color(0.0, 0.5, 0.5), "The mask applies after the filter and before group opacity")
	var markdown_region: Rect2i = Rect2i(120, 247, 100, 40)
	var covered: Rect2i = _color_bounds(image, markdown_region, Color.RED)
	var revealed: Rect2i = _color_bounds(image, markdown_region, Color.BLUE)
	_check(covered.size.x == 20 and revealed.size.x == 20 and covered.end.x == revealed.position.x,
		"Markdown HTML blocks draw the two sides of the mask, got %s and %s" % [covered, revealed])
	var diagnostics: Array[Dictionary] = label.get_css_diagnostics()
	_check(diagnostics.size() == 1 and diagnostics[0].get("property") == "mask" and
		diagnostics[0].get("kind") == "unsupported_value", "Invalid mask shorthand preserves the earlier variable declaration")
	viewport.queue_free()


func _check_svg_masks() -> void:
	var source: String = """<!doctype html><style>
		html { background: blue; } body { margin: 0; }
		.tile { position: absolute; top: 10px; width: 60px; height: 40px; background: red; }
		#object { left: 10px; mask: url(#cut%2Dbox) 100% 100% / 1px 1px no-repeat content-box; }
		#use { left: 90px; mask-image: url(#preferred); }
		#override { left: 170px; mask: url(#preferred) luminance; }
		#gradient { left: 250px; mask: url(#gradient-cut); }
		#linear { left: 315px; width: 25px; mask: url(#linear-gray); }
		#overflow { top: 64px; left: 15px; mask: url(#overflow-cut); transform: translateX(10px); }
		#overflow > b { position: absolute; left: -12px; width: 100px; height: 40px; background: lime; }
		#missing { top: 64px; left: 110px; mask: url(#wrong-target); }
		#empty { top: 64px; left: 190px; mask: url(#empty-cut); }
		#text { top: 64px; left: 270px; mask: url(#letters) alpha; }
		#preferred { mask-type: alpha !important; }
	</style>
	<svg style="display:none;" xmlns="http://www.w3.org/2000/svg">
		<defs>
			<rect id="wrong-target" width="12" height="16" fill="black"/>
			<linearGradient id="stops" x2="100%" y2="0%">
				<stop offset="0.5" stop-color="black"/><stop offset="0.5" stop-color="white"/>
			</linearGradient>
			<mask id="cut-box" x=".2" y="0%" width="50%" height="1" maskContentUnits="objectBoundingBox" mask-type="alpha">
				<rect width="50%" height="1" fill="black"/>
			</mask>
			<mask id="preferred" mask-type="luminance" style="mask-type: luminance; opacity:0">
				<use href="#wrong-target" x="10" y="4"/>
			</mask>
			<mask id="gradient-cut" maskUnits="userSpaceOnUse" x="10" y="0" width="45" height="40">
				<rect width="60" height="40" fill="url(#stops)"/>
			</mask>
			<g fill="#808080" color-interpolation="linearRGB">
				<mask id="linear-gray"><rect width="60" height="40"/></mask>
			</g>
			<mask id="overflow-cut" maskContentUnits="objectBoundingBox"><rect x="-1" width="3" height="1" fill="white"/></mask>
			<mask id="empty-cut"></mask>
			<mask id="letters"><text x="4" y="30" font-size="28" fill="black">MASK</text></mask>
		</defs>
	</svg>
	<div id="object" class="tile"></div><div id="use" class="tile"></div><div id="override" class="tile"></div>
	<div id="gradient" class="tile"></div><div id="linear" class="tile"></div><div id="overflow" class="tile"><b></b></div>
	<div id="missing" class="tile"></div><div id="empty" class="tile"></div><div id="text" class="tile"></div>"""
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(344, 155)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(344.0, 110.0)
	label.text = source
	viewport.add_child(label)
	var imported: HTMLDocument = ResourceLoader.load("{SVG_MASK_UID}") as HTMLDocument
	_check(imported != null and not FileAccess.file_exists("res://svg-mask-import.svg"),
		"The imported mask document retains its external SVG source")
	var imported_label: HTMLLabel = HTMLLabel.new()
	imported_label.position = Vector2(0.0, 110.0)
	imported_label.size = Vector2(48.0, 40.0)
	imported_label.document = imported
	viewport.add_child(imported_label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var pixels: Image = viewport.get_texture().get_image()
	_check_pixel(pixels, Vector2(17.0, 27.0), Color.BLUE, "The object bounding box mask region clips covered ink")
	_check_pixel(pixels, Vector2(30.0, 27.0), Color.RED, "Normalized SVG lengths and percentage widths use the target size")
	_check_pixel(pixels, Vector2(44.0, 27.0), Color.BLUE, "The normalized shape ends inside the mask region")
	_check_pixel(pixels, Vector2(105.0, 20.0), Color.RED, "Match source uses the computed mask-type and draws use references")
	_check_pixel(pixels, Vector2(95.0, 20.0), Color.BLUE, "A use offset shifts the mask's coverage")
	_check_pixel(pixels, Vector2(185.0, 20.0), Color.BLUE, "An explicit luminance mode overrides the mask type")
	_check_pixel(pixels, Vector2(275.0, 26.0), Color.BLUE, "An SVG gradient can remove pixels through luminance")
	_check_pixel(pixels, Vector2(290.0, 26.0), Color.RED, "An SVG gradient preserves pixels above its luminance stop")
	_check_pixel(pixels, Vector2(306.0, 26.0), Color.BLUE, "A user-space mask region clips its source")
	_check_pixel(pixels, Vector2(323.0, 26.0), Color("#3700c8"), "SVG masks inherit fill and convert luminance in linear RGB")
	_check_pixel(pixels, Vector2(17.0, 84.0), Color.BLUE, "Default SVG region excludes distant overflow")
	_check_pixel(pixels, Vector2(21.0, 84.0), Color.LIME, "The mask region can include overflow outside a transformed element")
	_check_pixel(pixels, Vector2(123.0, 84.0), Color.BLUE, "A fragment that isn't a mask produces no coverage")
	_check_pixel(pixels, Vector2(203.0, 84.0), Color.BLUE, "An empty mask produces no coverage")
	var glyphs: Rect2i = _color_bounds(pixels, Rect2i(270, 64, 60, 40), Color.RED)
	_check(glyphs.has_area() and glyphs.size.y < 40, "SVG mask text contributes shaped glyph outlines")
	_check_pixel(pixels, Vector2(9.0, 122.0), Color.RED, "A local SVG mask resolves through the imported nested stylesheet")
	_check_pixel(pixels, Vector2(30.0, 122.0), Color.BLUE, "An external SVG mask clips after the SVG file is removed")
	var mask: HTMLElement = label.get_element_by_id("cut-box")
	_check(mask.set_attribute("x", "40%") == OK, "Edit the inline mask region through the DOM")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	pixels = viewport.get_texture().get_image()
	_check_pixel(pixels, Vector2(30.0, 27.0), Color.BLUE, "Changing the SVG source invalidates the recorded mask")
	_check_pixel(pixels, Vector2(37.0, 27.0), Color.RED, "The new mask region uses the same coordinate system")
	viewport.queue_free()


func _check_clip_paths() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 168)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		#behind { position: absolute; inset: 0; display: block; }
		.tile { position: absolute; box-sizing: border-box; width: 64px; height: 64px; }
		.top { top: 12px; } .bottom { top: 92px; }
		#circle { left: 12px; background: red; --clip: circle(45%); clip-path: var(--clip); }
		#circle_link { position: absolute; inset: 0; z-index: 2; }
		#front { position: absolute; left: 35px; top: 32px; width: 4px; height: 8px; background: white; z-index: 1; }
		#ellipse { left: 92px; background: blue; clip-path: ellipse(26px 14px at right 7px bottom 12px); }
		#inset { left: 168px; background: lime; border: 4px solid transparent;
			clip-path: padding-box inset(5px 10px round 12px / 6px); clip-path: ellipse(20px); }
		#box { left: 244px; background: yellow; border: 4px solid red; padding: 8px; border-radius: 15px;
			clip-path: content-box; }
		#rect { left: 12px; background: orange; clip-path: rect(8px 50px auto 14px round 4px); }
		#xywh { left: 92px; background: cyan; clip-path: xywh(25% 10px 30px 40px); }
		#filter { left: 168px; clip-path: inset(0 0 0 20px); filter: drop-shadow(32px 0 lime); }
		#ink { position: absolute; left: 0; top: 20px; width: 12px; height: 18px; background: red; }
		#rotate { left: 244px; background: magenta; transform: rotate(90deg); clip-path: inset(0 0 50% 0); }
	</style><a id="behind" href="#behind"></a>
	<div class="tile top" id="circle"><a id="circle_link" href="#circle"></a></div>
	<div id="front"></div><div class="tile top" id="ellipse"></div><div class="tile top" id="inset"></div>
	<div class="tile top" id="box"></div><div class="tile bottom" id="rect"></div>
	<div class="tile bottom" id="xywh"></div><div class="tile bottom" id="filter"><div id="ink"></div></div>
	<div class="tile bottom" id="rotate"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(14.0, 14.0), Color.BLACK, "Circle clips its own corner and link")
	_check_pixel(image, Vector2(44.0, 44.0), Color.RED, "Circle keeps its center")
	_check_pixel(image, Vector2(37.0, 35.0), Color.WHITE, "Clip path isolates the stacking order of its descendants")
	_check_pixel(image, Vector2(149.0, 64.0), Color.BLUE, "Ellipse uses a four-component center position")
	_check_pixel(image, Vector2(98.0, 38.0), Color.BLACK, "Ellipse clips the part outside its axes")
	_check_pixel(image, Vector2(201.0, 42.0), Color.LIME, "Rounded inset resolves against the padding box")
	_check_pixel(image, Vector2(183.0, 22.0), Color.BLACK, "A fitted inset retains its rounded corners after an invalid declaration")
	_check_pixel(image, Vector2(276.0, 44.0), Color.YELLOW, "A geometry box can be used on its own")
	_check_pixel(image, Vector2(250.0, 28.0), Color.BLACK, "Content box clips border and padding paint")
	_check_pixel(image, Vector2(34.0, 125.0), Color.ORANGE, "Rect resolves explicit and automatic edge coordinates")
	_check_pixel(image, Vector2(18.0, 125.0), Color.BLACK, "Rect clips before the explicit left edge")
	_check_pixel(image, Vector2(126.0, 121.0), Color.CYAN, "XYWH resolves its position and size")
	_check_pixel(image, Vector2(102.0, 121.0), Color.BLACK, "XYWH excludes the area before its origin")
	_check_pixel(image, Vector2(204.0, 120.0), Color.LIME, "The clip path applies after filtering")
	_check_pixel(image, Vector2(174.0, 120.0), Color.BLACK, "The unshifted source is outside the clip path")
	_check_pixel(image, Vector2(292.0, 125.0), Color.MAGENTA, "The clip path rotates with its element")
	_check_pixel(image, Vector2(258.0, 125.0), Color.BLACK, "The rotated clip excludes the other half")
	_check(label.get_element_at(Vector2(14.0, 14.0)) == label.get_element_by_id("behind") and
		label.get_element_at(Vector2(44.0, 44.0)) == label.get_element_by_id("circle_link"),
		"Pointer hits fall through a clipped corner and reach the inner link at the center")
	_check(label.get_element_at(Vector2(258.0, 125.0)) == label.get_element_by_id("behind") and
		label.get_element_at(Vector2(292.0, 125.0)) == label.get_element_by_id("rotate"),
		"Rotated clip paint and hit geometry agree")
	_check(label.get_element_by_id("circle").get_bounding_rect() == Rect2(12.0, 12.0, 64.0, 64.0),
		"Clip path preserves the CSSOM bounding rectangle")
	viewport.queue_free()

	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""<!doctype html><style>
		body { margin: 0; }
		#text { width: 160px; font: 20px monospace; white-space: nowrap; clip-path: inset(0 90px 0 0); }
		#overflow { width: 20px; height: 20px; clip-path: circle(5px); }
		#child { width: 14px; height: 14px; transform: translateX(210px); }
	</style><div id="text">CLIPPED SELECTION</div><div id="overflow"><div id="child"></div></div>""") == OK
		and layout.layout(Vector2i(180, 100)) == OK, "Lay out selection and visual overflow under a clip path")
	var selection: Array[Rect2] = layout.get_selection_rects(0, layout.get_plain_text().length())
	_check(not selection.is_empty(), "Clipped text remains selectable")
	for rect: Rect2 in selection:
		_check(rect.end.x <= 70.1, "Selection geometry stops at the clip-path edge")
	_check(layout.get_content_size().x >= 224, "Clip path leaves transformed scrollable overflow intact")


func _check_polygon_clip_paths() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(324, 166)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	var ring: String = """0 0, 100% 0, 100% 100%, 0 100%, 0 0,
		25% 25%, 75% 25%, 75% 75%, 25% 75%, 25% 25%, 0 0"""
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		#behind { position: absolute; inset: 0; display: block; }
		.tile { position: absolute; box-sizing: border-box; width: 64px; height: 64px; }
		#concave { left: 12px; top: 12px; background: lime; border: 4px solid red;
			clip-path: polygon(0 0, 100%% 0, 100%% 40%%, 40%% 40%%, 40%% 100%%, 0 100%%) padding-box;
			clip-path: polygon(evenodd 0 0, 100%% 0, 100%% 100%%); }
		#evenodd { left: 88px; top: 12px; background: cyan; clip-path: polygon(evenodd, %s); }
		#nonzero { left: 164px; top: 12px; background: blue; clip-path: polygon(nonzero, %s); }
		#rotated { left: 240px; top: 12px; background: magenta; transform: rotate(90deg);
			clip-path: polygon(0 0, 100%% 0, 0 100%%); }
		#parent_clip { left: 12px; top: 88px; clip-path: polygon(0 0, 100%% 0, 0 100%%); }
		#child_clip { position: absolute; inset: 0; display: block; background: yellow;
			clip-path: polygon(25%% 0, 100%% 0, 100%% 100%%, 25%% 100%%); }
		#multirow { left: 88px; top: 88px; background: white;
			clip-path: polygon(%s100%% 0, 100%% 100%%, 0 100%%); }
		#degenerate { left: 164px; top: 88px; background: red; clip-path: polygon(0 0, 100%% 100%%); }
		#crossing { left: 240px; top: 88px; background: orange;
			clip-path: polygon(0 0, 100%% 100%%, 0 100%%, 100%% 0); }
	</style><a id="behind" href="#behind"></a><div class="tile" id="concave"></div>
	<div class="tile" id="evenodd"></div><div class="tile" id="nonzero"></div><div class="tile" id="rotated"></div>
	<div class="tile" id="parent_clip"><a id="child_clip" href="#child_clip"></a></div>
	<div class="tile" id="multirow"></div><div class="tile" id="degenerate"></div>
	<div class="tile" id="crossing"></div>""" % [ring, ring, "0 0, ".repeat(1030)]
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(22.0, 65.0), Color.LIME, "Concave polygon keeps its lower arm")
	_check_pixel(image, Vector2(53.0, 65.0), Color.BLACK, "Concave polygon cuts out its notch")
	_check_pixel(image, Vector2(13.0, 35.0), Color.BLACK, "Polygon coordinates use the padding box")
	_check_pixel(image, Vector2(95.0, 44.0), Color.CYAN, "Even-odd polygon fills its outer ring")
	_check_pixel(image, Vector2(120.0, 44.0), Color.BLACK, "Even-odd crossing counts cut out a hole")
	_check_pixel(image, Vector2(196.0, 44.0), Color.BLUE, "Nonzero winding fills the same center")
	_check_pixel(image, Vector2(288.0, 31.0), Color.MAGENTA, "Polygon rotates with the clipped element")
	_check_pixel(image, Vector2(250.0, 64.0), Color.BLACK, "The rotated polygon rejects the outside corner")
	_check_pixel(image, Vector2(36.0, 105.0), Color.YELLOW, "Nested polygons paint their intersection")
	_check_pixel(image, Vector2(24.0, 105.0), Color.BLACK, "Nested clip rejects points outside the inner polygon")
	_check_pixel(image, Vector2(64.0, 125.0), Color.BLACK, "Nested clip rejects points outside the outer polygon")
	_check_pixel(image, Vector2(120.0, 122.0), Color.WHITE, "A long vertex list crosses the texture row boundary")
	_check_pixel(image, Vector2(196.0, 122.0), Color.BLACK, "A two-point polygon has no fill")
	_check_pixel(image, Vector2(271.0, 101.0), Color.ORANGE, "Crossing edges preserve the top lobe")
	_check_pixel(image, Vector2(271.0, 139.0), Color.ORANGE, "Crossing edges preserve the bottom lobe")
	_check_pixel(image, Vector2(252.0, 120.0), Color.BLACK, "Crossing edges leave the sides unfilled")
	var behind: HTMLElement = label.get_element_by_id("behind")
	_check(label.get_element_at(Vector2(53.0, 65.0)) == behind and
		label.get_element_at(Vector2(120.0, 44.0)) == behind and
		label.get_element_at(Vector2(196.0, 44.0)) == label.get_element_by_id("nonzero"),
		"Pointer hits follow concavity and the chosen fill rule")
	_check(label.get_element_at(Vector2(36.0, 105.0)) == label.get_element_by_id("child_clip") and
		label.get_element_at(Vector2(64.0, 125.0)) == behind, "Nested clip paths share paint and link hit geometry")
	viewport.queue_free()

	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""<!doctype html><style>
		body { margin: 0; }
		#clip { width: 168px; height: 28px; font: 20px / 24px monospace;
			clip-path: polygon(evenodd, 0 -10px, 168px -10px, 168px 38px, 0 38px, 0 -10px,
				24px -2px, 96px -2px, 96px 26px, 24px 26px, 24px -2px, 0 -10px); }
	</style><div id="clip">ABCDEFGHIJK</div>""") == OK and layout.layout(Vector2i(180, 80)) == OK,
		"Lay out text under a polygon with a hole")
	_check(not layout.get_selection_rects(0, 1).is_empty() and layout.get_selection_rects(3, 5).is_empty()
		and not layout.get_selection_rects(9, 10).is_empty(),
		"Visible selection rectangles exclude the winding-rule hole")


func _check_rounded_polygon_clip_paths() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(326, 164)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		#behind { position: absolute; inset: 0; display: block; }
		.tile { position: absolute; top: 12px; width: 64px; height: 64px; }
		#rounded { left: 12px; background: lime; font-size: 24px; --r: calc(0.5em + 4px);
			clip-path: polygon(evenodd round var(--r), 0 0, 100% 0, 100% 100%, 0 100%);
			clip-path: polygon(round 5%, 0 0, 100% 0, 100% 100%, 0 100%);
			clip-path: polygon(round 0 0 0, 100% 0, 100% 100%, 0 100%);
			clip-path: polygon(round -1px, 0 0, 100% 0, 100% 100%, 0 100%); }
		#concave { left: 88px; background: cyan; clip-path: polygon(round 12px,
			0 0, 64px 0, 64px 32px, 32px 32px, 32px 64px, 0 64px); }
		#limited { left: 164px; height: 24px; background: yellow;
			clip-path: polygon(round 100px, 0 0, 64px 0, 64px 24px, 0 24px); }
		#duplicate { left: 240px; background: red; clip-path: polygon(round 16px,
			0 0, 0 64px, 64px 64px, 64px 0, 64px 0, 0 0); }
		#zero { top: 88px; left: 12px; background: white;
			clip-path: polygon(round 0, 0 0, 100% 0, 100% 100%, 0 100%); }
		#triangle { top: 88px; left: 88px; background: orange;
			clip-path: polygon(round 16px, 0 0, 64px 0, 0 64px); }
	</style><a id="behind" href="#behind"></a><a class="tile" id="rounded" href="#rounded"></a>
	<div class="tile" id="concave"></div><div class="tile" id="limited"></div>
	<div class="tile" id="duplicate"></div><div class="tile" id="zero"></div><div class="tile" id="triangle"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(14.0, 14.0), Color.BLACK, "Polygon rounding clips a convex corner")
	_check_pixel(image, Vector2(19.0, 19.0), Color.LIME, "Polygon rounding keeps the inside of a circular arc")
	_check_pixel(image, Vector2(121.0, 45.0), Color.CYAN, "Polygon rounding fills the tip of a concave notch")
	_check_pixel(image, Vector2(128.0, 52.0), Color.BLACK, "The rounded concave notch stays open")
	_check_pixel(image, Vector2(166.0, 14.0), Color.BLACK, "An oversized radius still rounds the corner")
	_check_pixel(image, Vector2(186.0, 14.0), Color.YELLOW, "Short adjoining edges clamp an oversized radius")
	_check_pixel(image, Vector2(242.0, 14.0), Color.BLACK, "Rounded polygons tolerate repeated and closing vertices")
	_check_pixel(image, Vector2(247.0, 19.0), Color.RED, "Reversing polygon winding preserves the same rounded arc")
	_check_pixel(image, Vector2(14.0, 90.0), Color.WHITE, "A zero rounding radius leaves the corner square")
	_check_pixel(image, Vector2(144.0, 90.0), Color.BLACK, "An acute vertex is rounded")
	_check_pixel(image, Vector2(127.0, 96.0), Color.ORANGE, "The acute corner radius respects its angle and edge lengths")
	_check(label.get_element_at(Vector2(14.0, 14.0)) == label.get_element_by_id("behind")
		and label.get_element_at(Vector2(19.0, 19.0)) == label.get_element_by_id("rounded")
		and label.get_element_at(Vector2(121.0, 45.0)) == label.get_element_by_id("concave"),
		"Rounded polygons share the convex and concave outlines between painting and hit testing")
	viewport.queue_free()


func _check_inline_svg_clip_sources() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 164)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		#behind { position: absolute; inset: 0; display: block; }
		.tile { position: absolute; top: 12px; width: 80px; height: 60px; }
		.second { top: 88px; }
		.first { left: 12px; }
		.middle { left: 108px; }
		.last { left: 204px; }
		#union { background: cyan; clip-path: url("#Ring%20and%20Bar"); }
		#normalized { background: lime; --clip: url(#normalized_source); clip-path: var(--clip); }
		#use_target { background: yellow; clip-path: url(#reuse); }
		#missing { background: red; clip-path: url(#missing_source); }
		#empty { background: blue; clip-path: url(#empty_source); }
		#invalid { background: white; clip-path: url(#other_element); }
		.ring { clip-rule: evenodd; }
		.excluded { display: none; }
	</style>
	<svg width="0" height="0"><defs>
		<clipPath id="Ring and Bar" class="ring">
			<path d="M0 0H62V60H0Z M12 12H48V44H12Z"/>
			<rect id="bar" x="29" y="24" width="44" height="10"/>
			<rect class="excluded" x="74" width="6" height="60"/>
		</clipPath>
		<clipPath id="normalized_source" clipPathUnits="objectBoundingBox" transform="translate(.1 .1)">
			<ellipse cx=".3" cy=".3" rx=".2" ry=".2"/>
		</clipPath>
		<polygon id="triangle" points="0,0 42,0 0,42"/>
		<clipPath id="reuse" transform="translate(10 6)">
			<use xlink:href="#triangle" x="4" y="3"/>
		</clipPath>
		<clipPath id="empty_source"/>
	</defs></svg>
	<span id="other_element"></span><a id="behind" href="#behind"></a>
	<a class="tile first" id="union" href="#union"></a>
	<div class="tile middle" id="normalized"></div><div class="tile last" id="use_target"></div>
	<div class="tile first second" id="missing"></div><div class="tile middle second" id="empty"></div>
	<div class="tile last second" id="invalid"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(18.0, 18.0), Color.CYAN, "Inline SVG path fills its outer ring")
	_check_pixel(image, Vector2(34.0, 30.0), Color.BLACK, "Inherited clip-rule preserves each SVG shape's hole")
	_check_pixel(image, Vector2(48.0, 41.0), Color.CYAN, "A sibling SVG rect fills part of the hole")
	_check_pixel(image, Vector2(66.0, 41.0), Color.CYAN, "Overlapping SVG clipping children form a union")
	_check_pixel(image, Vector2(89.0, 41.0), Color.BLACK, "Display-none children do not contribute to the SVG clip")
	_check_pixel(image, Vector2(140.0, 35.0), Color.LIME, "Object bounds map the transformed unit ellipse")
	_check_pixel(image, Vector2(140.0, 51.0), Color.BLACK, "A normalized ellipse scales each axis independently")
	_check_pixel(image, Vector2(162.0, 35.0), Color.BLACK, "Object-bounds clipping uses the original target width")
	_check_pixel(image, Vector2(222.0, 25.0), Color.YELLOW, "A use child references basic geometry and translates it")
	_check_pixel(image, Vector2(209.0, 19.0), Color.BLACK, "The original unshifted triangle is not part of the clip")
	_check_pixel(image, Vector2(20.0, 96.0), Color.RED, "A missing SVG reference leaves the element visible")
	_check_pixel(image, Vector2(120.0, 96.0), Color.BLACK, "An empty SVG clip removes all paint")
	_check_pixel(image, Vector2(212.0, 96.0), Color.WHITE, "A fragment on a non-clip element leaves the element visible")
	_check(label.get_element_at(Vector2(34.0, 30.0)) == label.get_element_by_id("behind")
		and label.get_element_at(Vector2(48.0, 41.0)) == label.get_element_by_id("union")
		and label.get_element_at(Vector2(66.0, 41.0)) == label.get_element_by_id("union"),
		"Pointer hits use the same independent winding fills as the SVG clip shader")
	var bar: HTMLElement = label.get_element_by_id("bar")
	_check(bar.set_attribute("width", "2") == OK, "Edit the hidden clip source through the DOM")
	var normalized: HTMLElement = label.get_element_by_id("normalized")
	_check(normalized.set_attribute("style", "width: 96px") == OK, "Resize the clipped element")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var edited: Image = viewport.get_texture().get_image()
	_check_pixel(edited, Vector2(48.0, 41.0), Color.BLACK,
		"SVG clip geometry is rebuilt after an attribute edit on its source")
	_check_pixel(edited, Vector2(162.0, 35.0), Color.LIME,
		"Object-bounds clipping is recomputed for the resized target")
	viewport.queue_free()

	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""<!doctype html><style>
		body { margin: 0; }
		#text { clip-path: url(#text_clip); width: 140px; font: 20px / 24px monospace; }
		</style><svg><defs><clipPath id="text_clip">
		<rect width="20" height="30"/><rect x="84" width="36" height="30"/>
		</clipPath></defs></svg><div id="text">ABCDEFGHIJK</div>""") == OK
		and layout.layout(Vector2i(180, 60)) == OK, "Lay out text clipped by two separate SVG children")
	_check(not layout.get_selection_rects(0, 1).is_empty()
		and layout.get_selection_rects(3, 5).is_empty()
		and not layout.get_selection_rects(7, 8).is_empty(),
		"Selection geometry respects gaps in the SVG union")


func _check_external_svg_clip_sources() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(248, 80)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var imported: HTMLDocument = ResourceLoader.load("{EXTERNAL_CLIP_UID}") as HTMLDocument
	_check(imported != null and not FileAccess.file_exists("res://clip-assets/import.svg"),
		"Load an external SVG clip after the SVG and its declaring stylesheets are removed")
	var imported_label: HTMLLabel = HTMLLabel.new()
	imported_label.size = Vector2(64.0, 80.0)
	imported_label.document = imported
	viewport.add_child(imported_label)
	var label: HTMLLabel = HTMLLabel.new()
	label.position = Vector2(64.0, 0.0)
	label.size = Vector2(184.0, 80.0)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		.tile { position: absolute; top: 0; width: 60px; height: 60px; }
		#live { left: 0; clip-path: url(clip-assets/local.svg#use-ring); background: lime; }
		#missing { left: 62px; clip-path: url(clip-assets/local.svg#missing); background: yellow; }
		#empty { left: 124px; clip-path: url(clip-assets/local.svg#source-empty); background: red; }
	</style><svg><defs><clipPath id="use-ring"/></defs></svg>
	<div class="tile" id="live"></div><div class="tile" id="missing"></div><div class="tile" id="empty"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(8.0, 8.0), Color.CYAN, "An imported clip resolves the nested stylesheet's path")
	_check_pixel(image, Vector2(30.0, 30.0), Color.BLACK, "Imported SVG styles and use references stay inside their own document")
	_check_pixel(image, Vector2(55.0, 8.0), Color.BLACK, "An imported SVG resolves its own nested clip references")
	_check_pixel(image, Vector2(72.0, 8.0), Color.LIME, "Load a live local SVG independently of matching IDs in the HTML")
	_check_pixel(image, Vector2(94.0, 30.0), Color.BLACK, "External SVG styles and XLink references preserve the hole")
	_check_pixel(image, Vector2(135.0, 8.0), Color.YELLOW, "A missing ID in a valid local SVG leaves the target unclipped")
	_check_pixel(image, Vector2(196.0, 8.0), Color.BLACK, "A valid empty external clip removes all target paint")
	var live: HTMLElement = label.get_element_by_id("live")
	_check(label.get_element_at(Vector2(8.0, 8.0)) == live and label.get_element_at(Vector2(30.0, 30.0)) != live,
		"External SVG clips affect element hits")
	viewport.queue_free()


func _check_nested_svg_clip_sources() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(284, 156)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		#behind { position: absolute; inset: 0; display: block; }
		.tile { position: absolute; top: 8px; width: 80px; height: 64px; }
		#root_clip { left: 8px; background: cyan; clip-path: url(#normalized_outer); }
		#child_clip { left: 100px; background: yellow; clip-path: url(#siblings); }
		#reused { left: 192px; background: lime; clip-path: url(#reuse); }
		#simple { left: 8px; top: 84px; background: magenta; clip-path: url(#basic); }
		#cyclic { left: 100px; top: 84px; background: white; clip-path: url(#cycle); }
		#empty { left: 192px; top: 84px; background: red; clip-path: url(#gone); }
		.round { clip-path: circle(14px at center); }
	</style><svg width="0" height="0"><defs>
		<clipPath id="normalized_outer" clipPathUnits="objectBoundingBox" clip-path="url(#normalized_inner)">
			<rect width=".5" height=".5"/>
		</clipPath>
		<clipPath id="normalized_inner" clipPathUnits="objectBoundingBox"><circle cx=".25" cy=".25" r=".25"/></clipPath>
		<clipPath id="siblings">
			<rect x="8" y="6" width="54" height="50" transform="translate(2 1)" clip-path="url(#right_half)"/>
			<rect y="22" width="30" height="12"/>
		</clipPath>
		<clipPath id="right_half" clipPathUnits="objectBoundingBox"><rect x=".5" width=".5" height="1"/></clipPath>
		<rect id="reusable" width="55" height="50"/>
		<clipPath id="reuse"><use href="#reusable" x="12" y="6" clip-path="url(#slice)"/></clipPath>
		<clipPath id="slice"><rect width="12" height="50"/></clipPath>
		<clipPath id="basic"><rect class="round" x="10" y="6" width="60" height="48"/></clipPath>
		<clipPath id="cycle" clip-path="url(#cycle)"><rect width="30" height="40"/></clipPath>
		<clipPath id="gone" clip-path="url(#empty_source)"><rect width="80" height="64"/></clipPath>
		<clipPath id="empty_source"/>
	</defs></svg><a id="behind" href="#behind"></a>
	<div class="tile" id="root_clip"></div><div class="tile" id="child_clip"></div>
	<div class="tile" id="reused"></div><div class="tile" id="simple"></div>
	<div class="tile" id="cyclic"></div><div class="tile" id="empty"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(28.0, 24.0), Color.CYAN, "Clip on a clipPath uses the original object's reference box")
	_check_pixel(image, Vector2(12.0, 12.0), Color.BLACK, "The nested normalized path cuts the outer rectangle's corner")
	_check_pixel(image, Vector2(118.0, 23.0), Color.BLACK, "A clip child is clipped in its own transformed coordinates")
	_check_pixel(image, Vector2(118.0, 36.0), Color.YELLOW, "A sibling can fill the area clipped from another shape")
	_check_pixel(image, Vector2(142.0, 23.0), Color.YELLOW, "Object-bounds clipping retains the transformed child's right half")
	_check_pixel(image, Vector2(210.0, 23.0), Color.LIME, "User-space clipping follows use x and y translations")
	_check_pixel(image, Vector2(222.0, 23.0), Color.BLACK, "The use instance is clipped to its translated slice")
	_check_pixel(image, Vector2(48.0, 114.0), Color.MAGENTA, "CSS basic shapes clip SVG silhouettes")
	_check_pixel(image, Vector2(24.0, 96.0), Color.BLACK, "The basic shape removes a corner of the SVG silhouette")
	_check_pixel(image, Vector2(112.0, 96.0), Color.WHITE, "A self-referencing clip terminates at the cyclic edge")
	_check_pixel(image, Vector2(140.0, 96.0), Color.BLACK, "The valid outer silhouette remains after ignoring the cycle")
	_check_pixel(image, Vector2(206.0, 96.0), Color.BLACK, "An empty nested clip removes the whole outer silhouette")
	_check(label.get_element_at(Vector2(118.0, 23.0)) == label.get_element_by_id("behind")
		and label.get_element_at(Vector2(118.0, 36.0)) == label.get_element_by_id("child_clip")
		and label.get_element_at(Vector2(28.0, 24.0)) == label.get_element_by_id("root_clip"),
		"Nested SVG intersections and sibling unions match the painted hit regions")
	viewport.queue_free()

	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""<!doctype html><style>
		body { margin: 0; }
		#text { clip-path: url(#nested_text); width: 140px; font: 20px / 24px monospace; }
		</style><svg><defs>
		<clipPath id="nested_text"><rect width="136" height="30" clip-path="url(#sides)"/></clipPath>
		<clipPath id="sides"><rect width="20" height="30"/><rect x="84" width="36" height="30"/></clipPath>
		</defs></svg><div id="text">ABCDEFGHIJK</div>""") == OK
		and layout.layout(Vector2i(180, 60)) == OK, "Lay out text clipped by an SVG child clip")
	_check(not layout.get_selection_rects(0, 1).is_empty()
		and layout.get_selection_rects(3, 5).is_empty()
		and not layout.get_selection_rects(7, 8).is_empty(),
		"Selection geometry respects nested intersections and gaps in the union")


func _check_svg_text_clips() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 162)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		.tile { position: absolute; top: 8px; width: 96px; height: 70px; background: lime; }
		.second { top: 88px; }
		#letter { left: 8px; clip-path: url(#letter_source); }
		#pair { left: 112px; clip-path: url(#pair_source); }
		#rotate { left: 216px; clip-path: url(#rotate_source); }
		#scaled { left: 8px; clip-path: url(#scaled_source); }
		#external { left: 112px; clip-path: url(clip-assets/local.svg#letter); }
		#reused { left: 216px; clip-path: url(#reuse_source); }
		.centered { text-anchor: middle; }
	</style><svg width="0" height="0"><defs>
	<clipPath id="letter_source" font-size="52"><text id="glyph" class="centered" x="48" y="58">O</text></clipPath>
	<clipPath id="pair_source"><text font-size="38" x="4 54" y="53"><tspan display="none">O</tspan><tspan>O</tspan>O</text></clipPath>
	<clipPath id="rotate_source"><text font-size="38" x="68" y="17" rotate="180">O</text></clipPath>
	<clipPath id="scaled_source" clipPathUnits="objectBoundingBox">
		<text x=".5" y=".8" font-size=".65" text-anchor="middle">O</text>
	</clipPath>
	<clipPath id="reuse_source"><use href="#glyph" x="8"/></clipPath>
	</defs></svg>
	<div class="tile" id="letter"></div><div class="tile" id="pair"></div><div class="tile" id="rotate"></div>
	<div class="tile second" id="scaled"></div><div class="tile second" id="external"></div>
	<div class="tile second" id="reused"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var letter: Rect2i = _color_bounds(image, Rect2i(8, 8, 96, 70), Color.LIME)
	_check(letter.size.x >= 24 and letter.size.y >= 30 and absi(letter.get_center().x - 56) <= 2,
		"SVG clipping text uses the inherited font size and its middle anchor")
	_check_pixel(image, Vector2(letter.get_center()), Color.BLACK, "The hole in O is outside the glyph silhouette")
	var ink: Rect2i = _color_bounds(image, Rect2i(letter.position, Vector2i(letter.size.x, 1)), Color.LIME)
	var target: HTMLElement = label.get_element_by_id("letter")
	_check(ink.has_area() and label.get_element_at(Vector2(ink.get_center()) + Vector2(0.5, 0.5)) == target
		and label.get_element_at(Vector2(letter.get_center())) != target, "SVG glyph ink and holes agree with hit testing")
	var first: Rect2i = _color_bounds(image, Rect2i(112, 8, 46, 70), Color.LIME)
	var second: Rect2i = _color_bounds(image, Rect2i(162, 8, 46, 70), Color.LIME)
	_check(first.has_area() and second == Rect2i(first.position + Vector2i(50, 0), first.size),
		"Per-character x positioning passes across tspan boundaries and skips display:none")
	var rotated: Rect2i = _color_bounds(image, Rect2i(216, 8, 96, 70), Color.LIME)
	_check(rotated.has_area() and rotated.position.y >= 23 and rotated.end.y >= 45,
		"Per-character rotation moves the glyph below its baseline within font overshoot: %s" % [rotated])
	var scaled: Rect2i = _color_bounds(image, Rect2i(8, 88, 96, 70), Color.LIME)
	_check(scaled.size.x >= 30 and scaled.size.y >= 24 and absi(scaled.get_center().x - 56) <= 2,
		"Fractional font sizes stay proportional to the object bounding box")
	var external: Rect2i = _color_bounds(image, Rect2i(112, 88, 96, 70), Color.LIME)
	var reused: Rect2i = _color_bounds(image, Rect2i(216, 88, 96, 70), Color.LIME)
	_check(external == Rect2i(letter.position + Vector2i(104, 80), letter.size),
		"Project-local SVG sources preserve text and presentation attributes")
	_check(reused == Rect2i(letter.position + Vector2i(216, 80), letter.size),
		"Use references preserve the text contours and offset them")
	viewport.queue_free()


func _check_motion_paths() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.mover { position: absolute; width: 20px; height: 10px; background: lime; }
		#path { left: 10px; top: 12px; offset-path: path("M0 0 H80 V40");
			offset-distance: calc(50% + 10px); offset-anchor: left top; }
		#tangent { left: 10px; top: 50px; offset-path: path("M0 0 V60"); offset-distance: 100%; }
		#compound { left: 160px; top: 20px; offset-path: path("M0 0 H70"); offset-distance: 30px;
			offset-anchor: left top; offset-rotate: 0deg; transform-origin: left top; transform: translateY(8px); }
		#reverse { left: 70px; top: 55px; offset-path: path("M0 0 H60"); offset-distance: 20px;
			offset-anchor: left top; offset-rotate: reverse; }
		#closed { left: 40px; top: 72px;
			--motion: auto path("M0 0 H40 Z") -25% 0deg / left top; offset: var(--motion); }
		#clamp { left: 100px; top: 72px; offset: right bottom path("M0 0 H40") 200% 0deg / left top; }
		#reset { left: 180px; top: 72px; offset: path("M0 0 H50") 100% / left top; offset: center; }
		#box { position: absolute; left: 120px; top: 100px; width: 160px; height: 90px;
			padding: 8px; border: 4px solid blue; }
		#ray { width: 20px; height: 20px; offset-path: ray(90deg sides contain at center) content-box;
			offset-distance: 100%; offset-anchor: center; offset-rotate: 0deg; }
		#polygon { offset-path: polygon(0 0, 100% 0, 100% 100%) content-box;
			offset-distance: 50px; offset-anchor: left top; offset-rotate: 0deg; }
		#circle { offset-path: circle(20px) content-box; offset-position: 25% 50%;
			offset-anchor: left top; offset-rotate: 0deg; }
		#circle_tangent { offset: circle(20px at center) content-box / left top; }
		#box_edge { offset: padding-box 0deg / left top; }
		#shape { offset: shape(from 0 0, hline to 50%, vline by 20px) content-box 100% 0deg / left top; }
		#url { offset: url(clip-assets/local.svg#guide) content-box 50% 0deg / left top; }
		#support { width: 33px; }
		@supports (offset: path("M0 0H20") 50% reverse / center) { #support { width: 60px; } }
		#bad { left: 240px; top: 24px; offset-path: ray(45deg contain contain); offset-anchor: normal; }
	</style><div id="path" class="mover"></div><div id="tangent" class="mover"></div>
	<div id="compound" class="mover"></div><div id="reverse" class="mover"></div>
	<div id="closed" class="mover"></div><div id="clamp" class="mover"></div><div id="reset" class="mover"></div>
	<div id="box"><div id="ray" class="mover"></div><div id="polygon" class="mover"></div>
	<div id="circle" class="mover"></div><div id="circle_tangent" class="mover"></div>
	<div id="box_edge" class="mover"></div><div id="shape" class="mover"></div><div id="url" class="mover"></div>
	</div><div id="support"></div><div id="bad" class="mover"></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(350, 236)) == OK, "Lay out the CSS motion path fixture")
	var expected: Dictionary[String, Rect2] = {
		"path": Rect2(80.0, 12.0, 20.0, 10.0),
		"tangent": Rect2(5.0, 100.0, 10.0, 20.0),
		"compound": Rect2(190.0, 28.0, 20.0, 10.0),
		"reverse": Rect2(70.0, 45.0, 20.0, 10.0),
		"closed": Rect2(60.0, 72.0, 20.0, 10.0),
		"clamp": Rect2(140.0, 72.0, 20.0, 10.0),
		"reset": Rect2(180.0, 72.0, 20.0, 10.0),
		"ray": Rect2(272.0, 147.0, 20.0, 20.0),
		"polygon": Rect2(182.0, 112.0, 20.0, 10.0),
		"circle": Rect2(192.0, 157.0, 20.0, 10.0),
		"circle_tangent": Rect2(222.0, 157.0, 10.0, 20.0),
		"box_edge": Rect2(124.0, 104.0, 20.0, 10.0),
		"shape": Rect2(212.0, 132.0, 20.0, 10.0),
		"url": Rect2(182.0, 170.0, 20.0, 10.0),
	}
	for id: String in expected:
		var actual: Rect2 = _rect(layout, id)
		_check(actual.is_equal_approx(expected[id]), "Motion path %s resolves to %s, expected %s" % [id, actual, expected[id]])
	_check(_rect(layout, "support").size.x == 60.0, "Feature queries accept valid motion path shorthands")
	_check(_has_diagnostic(layout, "offset-path", "ray(45deg contain contain)", "style_element")
		and _has_diagnostic(layout, "offset-anchor", "normal", "style_element"),
		"Invalid motion path and anchor values trigger declaration diagnostics")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(350, 236)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(84.0, 16.0), Color.LIME, "Motion path paints at the sampled point")
	_check_pixel(image, Vector2(14.0, 16.0), Color.BLACK, "Motion paint leaves the element's original position")
	_check(label.get_element_at(Vector2(84.0, 16.0)) == label.get_element_by_id("path")
		and label.get_element_at(Vector2(14.0, 16.0)) != label.get_element_by_id("path"),
		"Motion geometry updates pointer hits")
	viewport.queue_free()


func _check_individual_transforms() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.tile { position: absolute; width: 40px; height: 20px; background: lime; }
		#separate { left: 20px; top: 20px; transform-origin: left top;
			scale: 200% 50%; rotate: z 90deg; translate: 30px 4px 0; transform: translateX(10px); }
		#motion { left: 100px; top: 20px; width: 24px; height: 12px; transform-origin: left top;
			translate: 20px 5px; rotate: 90deg; scale: 2 1 100%;
			offset: path("M0 0 H30") 100% 0deg / left top; transform: translateX(3px); }
		.reference { left: 220px; width: 60px; height: 20px; padding: 10px; border: 5px solid lime;
			translate: 50% 50%; scale: 200% calc(30% + 20%); transform-origin: left top 20px; }
		#content { top: 20px; transform-box: content-box; }
		#border { top: 70px; transform-box: border-box; }
		#fill { top: 118px; transform-box: fill-box; }
		#stroke { top: 166px; transform-box: stroke-box; }
		#list { left: 220px; top: 220px; width: 60px; padding: 10px; border: 5px solid lime;
			transform-box: content-box; transform: translate(100%, 100%); }
		#axis { left: 80px; top: 220px; width: 30px; height: 10px;
			transform-origin: left top; rotate: 90deg 0 0 -2; }
		#singular { left: 25px; top: 157px; scale: 0; }
		#reset { left: 128px; top: 192px; translate: 99px; translate: none; }
		#invalid { left: 90px; top: 152px; translate: 5px; translate: 8px 0 2%;
			rotate: q 45deg; scale: 1 1 2 3; transform-box: padding-box; }
		#percent { left: 25px; top: 200px; --shift: calc(50% + 2px); translate: var(--shift); }
		#identity, #inert { width: 65px; height: 20px; margin: 25px 0 0 155px; }
		#identity { translate: 0px; }
		#inert { translate: none; transform-box: content-box; }
		#fixed_local, #fixed_viewport { position: fixed; width: 4px; height: 4px; }
		#fixed_local { left: 5px; top: 4px; }
		#fixed_viewport { left: 9px; top: 7px; }
	</style><div id="identity"><div id="fixed_local"></div></div><div id="inert"><div id="fixed_viewport"></div></div>
	<div id="separate" class="tile"></div><div id="motion" class="tile"></div>
	<div id="content" class="tile reference"></div><div id="border" class="tile reference"></div>
	<div id="fill" class="tile reference"></div><div id="stroke" class="tile reference"></div>
	<div id="list" class="tile"></div><div id="axis" class="tile"></div><div id="singular" class="tile"></div>
	<div id="reset" class="tile"></div><div id="invalid" class="tile"></div><div id="percent" class="tile"></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(480, 320)) == OK, "Lay out independent CSS transforms")
	var expected: Dictionary[String, Rect2] = {
		"separate": Rect2(40.0, 44.0, 10.0, 80.0),
		"motion": Rect2(108.0, 91.0, 12.0, 48.0),
		"content": Rect2(235.0, 37.5, 180.0, 25.0),
		"border": Rect2(265.0, 95.0, 180.0, 25.0),
		"fill": Rect2(235.0, 135.5, 180.0, 25.0),
		"stroke": Rect2(265.0, 191.0, 180.0, 25.0),
		"list": Rect2(280.0, 240.0, 90.0, 50.0),
		"axis": Rect2(80.0, 190.0, 10.0, 30.0),
		"reset": Rect2(128.0, 192.0, 40.0, 20.0),
		"invalid": Rect2(95.0, 152.0, 40.0, 20.0),
		"percent": Rect2(47.0, 200.0, 40.0, 20.0),
		"fixed_viewport": Rect2(9.0, 7.0, 4.0, 4.0),
	}
	for id: String in expected:
		var actual: Rect2 = _rect(layout, id)
		_check(actual.is_equal_approx(expected[id]), "Individual transform %s resolves to %s, expected %s" % [id, actual, expected[id]])
	_check(_rect(layout, "fixed_local").position == _rect(layout, "identity").position + Vector2(5.0, 4.0),
		"An explicit identity transform establishes a fixed containing block")
	_check(not _rect(layout, "singular").has_area(), "A singular independent scale hides the element")
	_check(_has_diagnostic(layout, "translate", "8px 0 2%", "style_element")
		and _has_diagnostic(layout, "rotate", "q 45deg", "style_element")
		and _has_diagnostic(layout, "scale", "1 1 2 3", "style_element")
		and _has_diagnostic(layout, "transform-box", "padding-box", "style_element"),
		"Malformed transforms and invalid reference boxes participate in diagnostics")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(480, 320)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(44.0, 55.0), Color.LIME, "Paint follows independent transforms")
	_check_pixel(image, Vector2(25.0, 25.0), Color.BLACK, "The transform moves paint away from the original layout box")
	_check(label.get_element_at(Vector2(44.0, 55.0)) == label.get_element_by_id("separate")
		and label.get_element_at(Vector2(25.0, 25.0)) != label.get_element_by_id("separate"),
		"Hit testing follows the composed transformation")
	viewport.queue_free()


func _check_flat_3d_transforms() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.tile { position: absolute; width: 40px; height: 20px; background: lime; transform-origin: left top; }
		#tilt { left: 20px; top: 20px; transform: rotate3d(0, 200, 0, 60deg); }
		#pivot { left: 100px; top: 20px; transform-origin: left top 20px; transform: rotateX(60deg); }
		#depth { left: 20px; top: 60px; transform: rotateY(90deg) translateZ(25px) rotateY(-90deg); }
		#individual { left: 100px; top: 60px; translate: 4px 2px 12px; rotate: y 90deg; scale: 1 1 2;
			transform: translateZ(calc(3px + 2px)) rotateY(-90deg); }
		#parent { left: 20px; top: 100px; transform: rotateY(60deg); transform-style: flat; }
		#nested { left: 0; top: 0; transform: rotateY(60deg); }
		#matrix { left: 100px; top: 100px; transform: matrix3d(1,0,0,0, 0,2,0,0, 0,0,2,0, 20,10,6,2); }
		#under, #hidden { left: 20px; top: 150px; }
		#under { background: blue; }
		#hidden { transform-origin: center; transform: rotateY(180deg); backface-visibility: hidden; }
		#hidden_child { left: 0; top: 0; }
		#shown { left: 80px; top: 150px; transform-origin: center; transform: rotateY(180deg); }
		#mirror { left: 140px; top: 150px; transform-origin: center; transform: scaleX(-1);
			backface-visibility: hidden; }
		#singular { left: 200px; top: 150px; transform: scaleZ(0); }
		#edge { left: 260px; top: 150px; transform: rotateY(90deg); }
		#fallback { left: 20px; top: 190px; transform: translate3d(5px, 0, 10px);
			transform: matrix3d(1,0,0,0); }
		@supports (transform: scale3d(1, 1, 2)) { #fallback { width: 45px; } }
		#scroll { left: 20px; top: 230px; transform-origin: center;
			transform: translateX(400px) rotateY(180deg); backface-visibility: hidden; }
	</style>
	<div id="tilt" class="tile"></div><div id="pivot" class="tile"></div>
	<div id="depth" class="tile"></div><div id="individual" class="tile"></div>
	<div id="parent" class="tile"><div id="nested" class="tile"></div></div><div id="matrix" class="tile"></div>
	<div id="under" class="tile"></div><div id="hidden" class="tile"><div id="hidden_child" class="tile"></div></div>
	<div id="shown" class="tile"></div><div id="mirror" class="tile"></div><div id="singular" class="tile"></div>
	<div id="edge" class="tile"></div><div id="fallback" class="tile"></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source + '<div id="scroll" class="tile"></div>') == OK
		and layout.layout(Vector2i(320, 260)) == OK, "Lay out affine 3D transforms")
	var expected: Dictionary[String, Rect2] = {
		"tilt": Rect2(20.0, 20.0, 20.0, 20.0),
		"pivot": Rect2(100.0, 20.0 + sqrt(3.0) * 10.0, 40.0, 10.0),
		"depth": Rect2(45.0, 60.0, 40.0, 20.0),
		"individual": Rect2(114.0, 62.0, 80.0, 20.0),
		"nested": Rect2(20.0, 100.0, 10.0, 20.0),
		"matrix": Rect2(110.0, 105.0, 20.0, 20.0),
		"hidden": Rect2(20.0, 150.0, 40.0, 20.0),
		"singular": Rect2(200.0, 150.0, 40.0, 20.0),
		"edge": Rect2(260.0, 150.0, 0.0, 20.0),
		"fallback": Rect2(25.0, 190.0, 45.0, 20.0),
	}
	for id: String in expected:
		var actual: Rect2 = _rect(layout, id)
		_check(actual.is_equal_approx(expected[id]), "Flat 3D transform %s resolves to %s, expected %s" % [id, actual, expected[id]])
	_check(layout.get_content_size().x >= 460, "A hidden back face still contributes to scrollable overflow")
	_check(_has_diagnostic(layout, "transform", "matrix3d(1 , 0 , 0 , 0)", "style_element"),
		"A malformed matrix leaves the earlier valid transform in effect")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 260)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(30.0, 160.0), Color.BLUE, "A hidden back face leaves the underlying element visible")
	_check_pixel(image, Vector2(90.0, 160.0), Color.LIME, "A back face is visible by default")
	_check_pixel(image, Vector2(150.0, 160.0), Color.LIME, "A 2D mirror keeps its front face visible")
	_check_pixel(image, Vector2(215.0, 160.0), Color.BLACK, "A singular 3D transform suppresses paint")
	_check(label.get_element_at(Vector2(30.0, 160.0)) == label.get_element_by_id("under"),
		"A hidden back face and its descendants leave the underlying element hittable")
	_check(label.get_element_at(Vector2(150.0, 160.0)) == label.get_element_by_id("mirror"),
		"A reflected 2D element with backface-visibility hidden remains hittable")
	_check(label.get_element_at(Vector2(215.0, 160.0)) != label.get_element_by_id("singular"),
		"A singular 3D transform suppresses hits")
	viewport.queue_free()


func _check_perspective_transforms() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.tile { display: block; position: absolute; width: 40px; height: 20px; background: lime; transform-origin: left top; }
		#tilt { left: 20px; top: 20px; width: 80px; height: 60px;
			transform: perspective(var(--depth, 100px)) rotateY(60deg); color: lime; }
		#matrix { left: 310px; top: 20px; transform: matrix3d(1,0,0,.005, 0,1,0,0, 0,0,1,0, 0,0,0,1); }
		#parent { position: absolute; left: 110px; top: 20px; width: 160px; height: 95px;
			perspective: 100px; perspective-origin: right 80px bottom 75px; }
		#child { position: fixed; left: 90px; top: 20px; transform: translateZ(50px); }
		#behind { left: 20px; top: 120px; transform: perspective(100px) translateZ(200px); }
		#rounded { left: 110px; top: 120px; width: 80px; height: 70px; background: none;
			border-radius: 20px; overflow: hidden; transform: perspective(120px) rotateY(-35deg); }
		#rounded_child { left: 0; top: 0; width: 100%; height: 100%; opacity: .5; }
		#outer { left: 235px; top: 120px; width: 80px; height: 40px; background: none;
			transform: matrix3d(1,0,0,.03, 0,1,0,0, 0,0,1,0, 0,0,0,1); }
		#inner { left: 0; top: 0; width: 80px; height: 40px;
			transform: matrix3d(1,0,0,-.02, 0,1,0,0, 0,0,1,0, 0,0,0,1); }
		#cross { left: 20px; top: 205px; width: 80px; height: 30px; transform: perspective(45px) rotateY(-60deg); }
		#invalid { left: 310px; top: 130px; perspective: -9px; perspective-origin: left 1px top 2px 4px;
			transform: perspective(none) translateX(5px); transform: perspective(50%); }
	</style>
	<a id="tilt" class="tile" href="#child">Tilt</a><div id="matrix" class="tile"></div>
	<div id="parent"><div id="child" class="tile"></div></div><div id="behind" class="tile"></div>
	<div id="rounded" class="tile"><div id="rounded_child" class="tile"></div></div><div id="invalid" class="tile"></div>
	<div id="outer" class="tile"><div id="inner" class="tile"></div></div>
	<div id="cross" class="tile"></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(source) == OK and layout.layout(Vector2i(360, 250)) == OK, "Lay out CSS perspective")
	var width: float = 40.0 / (1.0 + 0.4 * sqrt(3.0))
	var expected: Dictionary[String, Rect2] = {
		"tilt": Rect2(20.0, 20.0, width, 60.0),
		"matrix": Rect2(310.0, 20.0, 40.0 / 1.2, 20.0),
		"child": Rect2(210.0, 40.0, 80.0, 40.0),
		"invalid": Rect2(315.0, 130.0, 40.0, 20.0),
	}
	for id: String in expected:
		var actual: Rect2 = _rect(layout, id)
		_check(actual.is_equal_approx(expected[id]), "Perspective transform %s resolves to %s, expected %s" % [id, actual, expected[id]])
	_check(not _rect(layout, "behind").has_area(), "A plane behind the viewer has no visible rectangle")
	_check(layout.get_content_size().x > 360, "A plane crossing the horizon extends scrollable overflow")
	var offset: int = layout.get_plain_text().find("Tilt")
	var selected: Array[Rect2] = layout.get_selection_rects(offset, offset + 4)
	_check(offset >= 0 and selected.size() == 1 and _rect(layout, "tilt").encloses(selected[0]),
		"Text selection geometry follows the perspective projection")
	_check(_has_diagnostic(layout, "perspective", "-9px", "style_element")
		and _has_diagnostic(layout, "perspective-origin", "left 1px top 2px 4px", "style_element")
		and _has_diagnostic(layout, "transform", "perspective(50%)", "style_element"),
		"Perspective diagnostics reject negative distances, percentages and malformed origins")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(360, 250)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(28.0, 45.0), Color.LIME, "Projective link paint follows its plane")
	_check_pixel(image, Vector2(41.0, 75.0), Color.BLACK, "The unpainted corner of the projected bounds stays empty")
	_check_pixel(image, Vector2(230.0, 55.0), Color.LIME, "Parent perspective projects a fixed child about the configured origin")
	_check_pixel(image, Vector2(30.0, 130.0), Color.BLACK, "Paint rejects a plane behind the viewer")
	_check_pixel(image, Vector2(112.0, 122.0), Color.BLACK, "Perspective preserves a rounded overflow clip")
	_check_pixel(image, Vector2(155.0, 150.0), Color(0.0, 0.5, 0.0), "Perspective composites opacity inside the same clip")
	_check_pixel(image, Vector2(260.0, 135.0), Color.LIME, "Nested projection paints the front of both planes")
	_check_pixel(image, Vector2(273.0, 135.0), Color.BLACK, "Nested projection does not paint through an ancestor's horizon")
	_check_pixel(image, Vector2(180.0, 218.0), Color.LIME, "The front side of a horizon-crossing plane remains painted")
	_check(label.get_element_at(Vector2(28.0, 45.0)) == label.get_element_by_id("tilt")
		and label.get_element_at(Vector2(41.0, 75.0)) != label.get_element_by_id("tilt"),
		"Perspective link hit testing follows the projected quadrilateral")
	_check(label.get_element_at(Vector2(180.0, 218.0)) == label.get_element_by_id("cross"),
		"Perspective hit testing retains the visible side of a horizon-crossing plane")
	_check(label.get_element_at(Vector2(260.0, 135.0)) == label.get_element_by_id("inner")
		and label.get_element_at(Vector2(273.0, 135.0)) != label.get_element_by_id("inner"),
		"Nested projection also rejects hits beyond the ancestor horizon")

	label.hide()
	var standalone: HTMLLayout = HTMLLayout.new()
	_check(standalone.parse("""<!doctype html><body style="margin:0;background:black">
		<div style="width:80px;height:60px;background:lime;transform-origin:top left;
		transform:perspective(100px) rotateY(60deg)"></div>""") == OK
		and standalone.layout(viewport.size) == OK, "Lay out projected content for an independent canvas")
	var canvas: Node2D = Node2D.new()
	canvas.position = Vector2(500.0, 0.0)
	canvas.draw.connect(func() -> void: standalone.draw(canvas))
	viewport.add_child(canvas)
	await process_frame
	await RenderingServer.frame_post_draw
	canvas.position = Vector2.ZERO
	await process_frame
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), Vector2(8.0, 20.0), Color.LIME,
		"A CanvasItem can move projected content into view without requesting a redraw")
	viewport.queue_free()


func _check_preserved_3d_contexts() -> void:
	var source: String = """<!doctype html><style>
		body { margin: 0; background: black; }
		.scene { position: absolute; transform-style: preserve-3d; }
		.face { position: absolute; width: 48px; height: 24px; background: lime; }
		#shared, #flat, #clipped { top: 16px; width: 80px; height: 44px; transform-origin: left top; transform: rotateY(60deg); }
		#shared { left: 20px; } #flat { left: 120px; overflow: hidden; } #clipped { left: 220px; overflow: clip; }
		.cancel_rotation { left: 0; top: 0; transform-origin: left top; transform: rotateY(-60deg); }
		#camera { position: absolute; left: 320px; top: 16px; width: 80px; height: 60px;
			perspective: 200px; perspective-origin: left top; }
		#rig { left: 8px; top: 8px; width: 60px; height: 30px; transform: translateZ(20px); }
		#lens { left: 0; top: 0; width: 32px; height: 20px; transform: translateZ(30px); }
		#depth, #crossing, #coplanar { top: 80px; width: 120px; height: 60px; background: blue; }
		#depth { left: 20px; } #crossing { left: 160px; } #coplanar { left: 300px; }
		#near, #far, #tie_first, #tie_last { left: 8px; top: 8px; width: 80px; height: 36px; }
		#near { transform: translateZ(20px); z-index: -5; }
		#far { background: red; transform: translateZ(-20px); z-index: 10; }
		#negative { left: 98px; top: 8px; width: 16px; transform: translateZ(-10px); }
		#slant_left, #slant_right { left: 10px; top: 8px; width: 100px; height: 40px; }
		#slant_left { transform: rotateY(45deg); }
		#slant_right { transform: rotateY(-45deg); background: red; opacity: .5; }
		#tie_first { transform: translateZ(4px); z-index: -1; }
		#tie_last { transform: translateZ(4px); z-index: 1; background: yellow; }
		#flip { left: 20px; top: 170px; width: 48px; height: 24px; transform: rotateY(180deg); }
		#front_face, #rear_face { left: 0; top: 0; backface-visibility: hidden; }
		#rear_face { transform: rotateY(180deg); background: red; }
		#isolated { left: 120px; top: 170px; width: 80px; height: 50px; }
		#flattening_face { left: 0; top: 0; width: 80px; height: 50px; transform: translateZ(-10px); opacity: .8; }
		#nested_context { left: 0; top: 0; width: 80px; height: 50px; }
		#nested_front { left: 0; top: 0; width: 80px; height: 50px; background: red; transform: translateZ(200px); }
		#outer_front { left: 8px; top: 8px; width: 64px; height: 36px; transform: translateZ(5px); z-index: -1; }
		#hidden_scene { left: 230px; top: 170px; width: 48px; height: 24px; transform: rotateY(180deg);
			backface-visibility: hidden; }
		#hidden_scene_child { left: 0; top: 0; transform: rotateY(180deg); }
	</style>
	<div class="scene" id="shared"><div id="shared_child" class="face cancel_rotation"></div></div>
	<div class="scene" id="flat"><div id="flat_child" class="face cancel_rotation"></div></div>
	<div class="scene" id="clipped"><div id="clipped_child" class="face cancel_rotation"></div></div>
	<div id="camera"><div class="scene" id="rig"><div id="lens" class="face"></div></div></div>
	<div class="scene" id="depth"><div id="near" class="face"></div><div id="far" class="face"></div>
		<div id="negative" class="face"></div></div>
	<div class="scene" id="crossing"><div id="slant_left" class="face"></div><div id="slant_right" class="face"></div></div>
	<div class="scene" id="coplanar"><div id="tie_last" class="face"></div><div id="tie_first" class="face"></div></div>
	<div class="scene" id="flip"><div id="front_face" class="face"></div><div id="rear_face" class="face"></div></div>
	<div class="scene" id="isolated"><div id="outer_front" class="face"></div>
		<div class="scene" id="flattening_face"><div class="scene" id="nested_context">
			<div id="nested_front" class="face"></div></div></div></div>
	<div class="scene" id="hidden_scene"><div id="hidden_scene_child" class="face"></div></div>"""
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse(source) == OK and layout.layout(Vector2i(440, 245)) == OK, "Lay out preserved 3D contexts")
	var expected: Dictionary[String, Rect2] = {
		"shared_child": Rect2(20.0, 16.0, 48.0, 24.0),
		"flat_child": Rect2(120.0, 16.0, 12.0, 24.0),
		"clipped_child": Rect2(220.0, 16.0, 48.0, 24.0),
		"lens": Rect2(320.0 + 8.0 / 0.75, 16.0 + 8.0 / 0.75, 32.0 / 0.75, 20.0 / 0.75),
	}
	for id: String in expected:
		var actual: Rect2 = _rect(layout, id)
		_check(actual.is_equal_approx(expected[id]), "3D context geometry %s resolves to %s, expected %s" % [id, actual, expected[id]])

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(440, 245)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = source
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(45.0, 28.0), Color.LIME, "Nested transformations cancel in a shared 3D context")
	_check_pixel(image, Vector2(144.0, 28.0), Color.BLACK, "Scrollable overflow flattens the child")
	_check_pixel(image, Vector2(245.0, 28.0), Color.LIME, "Overflow clip keeps the shared depth context inside its clip")
	_check_pixel(image, Vector2(350.0, 36.0), Color.LIME, "Ancestor perspective sees both Z translations")
	_check_pixel(image, Vector2(40.0, 105.0), Color.LIME, "Depth beats z-index for overlapping planes in the same context")
	_check_pixel(image, Vector2(125.0, 100.0), Color.BLUE, "The root background participates in depth ordering")
	_check_pixel(image, Vector2(195.0, 105.0), Color.LIME, "Intersection shows the left plane on the left")
	_check_pixel(image, Vector2(245.0, 105.0), Color(0.5, 0.0, 0.5), "Intersection blends the right plane on the right")
	_check_pixel(image, Vector2(330.0, 105.0), Color.YELLOW, "Coplanar faces use CSS paint order")
	_check_pixel(image, Vector2(45.0, 182.0), Color.RED, "Back-face visibility uses the accumulated matrix")
	_check_pixel(image, Vector2(140.0, 192.0), Color.LIME, "A nested scene stays on the enclosing flattened plane")
	_check_pixel(image, Vector2(250.0, 182.0), Color.BLACK, "A hidden back face hides the preserving subtree")
	var hit_cases: Dictionary[Vector2, String] = {
		Vector2(40.0, 105.0): "near",
		Vector2(125.0, 100.0): "depth",
		Vector2(195.0, 105.0): "slant_left",
		Vector2(245.0, 105.0): "slant_right",
		Vector2(330.0, 105.0): "tie_last",
		Vector2(45.0, 182.0): "rear_face",
		Vector2(140.0, 192.0): "outer_front",
	}
	for point: Vector2 in hit_cases:
		_check(label.get_element_at(point) == label.get_element_by_id(hit_cases[point]),
			"Pointer targeting follows the top 3D plane at %s" % point)
	_check(label.get_element_at(Vector2(250.0, 182.0)) != label.get_element_by_id("hidden_scene_child"),
		"A hidden preserving subtree has no pointer targets")
	viewport.queue_free()


func _check_svg_text_path_clips() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(424, 280)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		.tile { position: absolute; top: 8px; width: 96px; height: 84px; background: lime; }
		.row2 { top: 100px; } .row3 { top: 192px; }
		.c1 { left: 8px; } .c2 { left: 112px; } .c3 { left: 216px; } .c4 { left: 320px; }
		#flat { clip-path: url(#flat_source); }
		#calibrated { clip-path: url(#calibrated_source); }
		#inline { clip-path: url(#inline_source); }
		#external { clip-path: url(clip-assets/local.svg#path-letter); }
		#vertical { clip-path: url(#vertical_source); }
		#reverse { clip-path: url(#reverse_source); }
		#closed { clip-path: url(#closed_source); }
		#open { clip-path: url(#open_source); }
		#gap { clip-path: url(#gap_source); }
		#curve { clip-path: url(#curve_source); }
		#after { clip-path: url(#after_source); }
		#invalid_path { clip-path: url(#invalid_source); }
	</style><svg width="0" height="0"><defs font-size="48">
		<path id="guide" d="M5 29 H45" transform="scale(2)" pathLength="20"/>
		<line id="baseline" x1="10" y1="58" x2="90" y2="58"/>
		<clipPath id="flat_source"><text x="50" y="58" text-anchor="middle">O</text></clipPath>
		<clipPath id="calibrated_source"><text text-anchor="middle">
			<textPath href="#guide" startOffset="10">O</textPath>
		</text></clipPath>
		<clipPath id="inline_source"><text text-anchor="middle">
			<textPath href="#unused" path="M10 58 H90 Q 20" startOffset="50%">O</textPath>
		</text></clipPath>
		<clipPath id="vertical_source"><text text-anchor="middle">
			<textPath path="M30 4 V76" startOffset="50%"><tspan>O</tspan></textPath>
		</text></clipPath>
		<clipPath id="reverse_source"><text text-anchor="middle">
			<textPath path="M10 17 H90" startOffset="50%" side="right">O</textPath>
		</text></clipPath>
		<clipPath id="closed_source"><text><textPath path="M20 56 H80 V72 H20 Z" startOffset="100%">O</textPath></text></clipPath>
		<clipPath id="open_source"><text><textPath path="M20 56 H80 V72 H20 V56" startOffset="100%">O</textPath></text></clipPath>
		<clipPath id="gap_source"><text text-anchor="middle">
			<textPath path="M10 58 H30 M54 58 H90" startOffset="50%">O</textPath>
		</text></clipPath>
		<clipPath id="curve_source"><text text-anchor="middle">
			<textPath path="M10 70 C10 20 90 20 90 70" startOffset="50%">O</textPath>
		</text></clipPath>
		<clipPath id="after_source"><text font-size="30"><textPath path="M5 58 H55" startOffset="100%">O</textPath>O</text></clipPath>
		<clipPath id="invalid_source"><text><textPath href="#flat_source">O</textPath></text></clipPath>
	</defs></svg>
	<div id="flat" class="tile c1"></div><div id="calibrated" class="tile c2"></div>
	<div id="inline" class="tile c3"></div><div id="external" class="tile c4"></div>
	<div id="vertical" class="tile c1 row2"></div><div id="reverse" class="tile c2 row2"></div>
	<div id="closed" class="tile c3 row2"></div><div id="open" class="tile c4 row2"></div>
	<div id="gap" class="tile c1 row3"></div><div id="curve" class="tile c2 row3"></div>
	<div id="after" class="tile c3 row3"></div><div id="invalid_path" class="tile c4 row3"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var flat: Rect2i = _color_bounds(image, Rect2i(8, 8, 96, 84), Color.LIME)
	_check(flat.has_area(), "The straight SVG glyph reference is visible")
	var calibrated: Rect2i = _color_bounds(image, Rect2i(112, 8, 96, 84), Color.LIME)
	_check(calibrated == Rect2i(flat.position + Vector2i(104, 0), flat.size),
		"Path transforms change the measured path without scaling glyphs, and pathLength calibrates startOffset")
	var inline_path: Rect2i = _color_bounds(image, Rect2i(216, 8, 96, 84), Color.LIME)
	_check(inline_path == Rect2i(flat.position + Vector2i(208, 0), flat.size),
		"Inline path data uses the valid segment prefix and resolves percentage startOffset")
	var external: Rect2i = _color_bounds(image, Rect2i(320, 8, 96, 84), Color.LIME)
	_check(external == Rect2i(flat.position + Vector2i(312, 0), flat.size),
		"External SVG textPath resolves a basic shape in its own document")
	var vertical: Rect2i = _color_bounds(image, Rect2i(8, 100, 96, 84), Color.LIME)
	_check(vertical.has_area() and vertical.position.x >= 36 and vertical.end.x > 61,
		"A vertical guide rotates the horizontal glyph around its own baseline")
	_check_pixel(image, Vector2(vertical.get_center()), Color.BLACK, "The letter hole remains transparent on a path")
	var top: Rect2i = _color_bounds(image, Rect2i(vertical.position, Vector2i(vertical.size.x, 1)), Color.LIME)
	var target: HTMLElement = label.get_element_by_id("vertical")
	_check(top.has_area() and label.get_element_at(Vector2(top.get_center()) + Vector2(0.5, 0.5)) == target
		and label.get_element_at(Vector2(vertical.get_center())) != target, "Path-positioned glyphs share geometry with hit testing")
	var reverse: Rect2i = _color_bounds(image, Rect2i(112, 100, 96, 84), Color.LIME)
	_check(reverse.has_area() and reverse.position.y >= 115 and reverse.end.y > 145,
		"Reversing the guide places the upside-down glyph on the other side")
	var closed: Rect2i = _color_bounds(image, Rect2i(216, 100, 96, 84), Color.LIME)
	var open: Rect2i = _color_bounds(image, Rect2i(320, 100, 96, 84), Color.LIME)
	_check(closed.has_area() and not open.has_area(),
		"Text wraps across closepath and stays hidden beyond an open subpath")
	var gap: Rect2i = _color_bounds(image, Rect2i(8, 192, 96, 84), Color.LIME)
	_check(gap == Rect2i(flat.position + Vector2i(12, 184), flat.size), "Moves between subpaths add no distance")
	var curve: Rect2i = _color_bounds(image, Rect2i(112, 192, 96, 84), Color.LIME)
	_check(curve.has_area() and absi(curve.get_center().x - 162) <= 2 and curve.end.y < 233,
		"A cubic guide places the letter at half the measured curve length")
	var after: Rect2i = _color_bounds(image, Rect2i(216, 192, 96, 84), Color.LIME)
	_check(after.has_area() and after.position.x >= 270 and after.end.x < 302,
		"Plain text after textPath begins at the end of the path")
	_check(not _color_bounds(image, Rect2i(320, 192, 96, 84), Color.LIME).has_area(),
		"An invalid guide produces no glyph outline")
	viewport.queue_free()


func _check_svg_clip_paths() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(324, 188)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		#behind { position: absolute; inset: 0; display: block; }
		.tile { position: absolute; box-sizing: border-box; width: 64px; height: 64px; }
		.top { top: 10px; } .bottom { top: 94px; }
		#curves { left: 12px; background: red; transform: scale(1.1); transform-origin: 0 0;
			--shape: path("M0 32 C0 12 12 0 32 0 S64 12 64 32 Q64 64 32 64 T0 32 Z");
			clip-path: var(--shape); }
		#arc { left: 88px; background: lime; clip-path: path("M6 32 A26 20 0 0 1 58 32 a26 20 0 01-52 0z"); }
		#corrected { left: 164px; background: magenta; clip-path: path("M6 32 A10 6 0 0 1 58 32 A10 6 0 0 1 6 32Z"); }
		#large { left: 240px; background: orange; clip-path: path("M54 32 A22 22 0 1 1 32 10 L32 32 Z"); }
		#hole { left: 12px; background: cyan; clip-path: path(evenodd, "M0 0H64V64H0Z M16 16h32v32h-32"); }
		#solid { left: 88px; background: blue; clip-path: path(nonzero, "M0 0H64V64H0Z M16 16h32v32h-32"); }
		#reference { left: 164px; background: yellow; border: 4px solid red;
			clip-path: path("m10 10 30 0 0 30-30 0") padding-box; }
		#invalid { left: 240px; background: white; clip-path: path("M.5.5 4e1 0 0 4e1");
			clip-path: path("M0 0 A10 10 0 2 1 20 20"); }
	</style><a id="behind" href="#behind"></a>
	<div class="tile top" id="curves"></div><div class="tile top" id="arc"></div>
	<div class="tile top" id="corrected"></div><div class="tile top" id="large"></div>
	<div class="tile bottom" id="hole"></div><div class="tile bottom" id="solid"></div>
	<div class="tile bottom" id="reference"></div><div class="tile bottom" id="invalid"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(26.0, 23.0), Color.RED, "Cubic and smooth cubic commands paint the upper curve")
	_check_pixel(image, Vector2(24.0, 64.0), Color.RED, "Quadratic and smooth quadratic commands paint the lower curve")
	_check_pixel(image, Vector2(14.0, 12.0), Color.BLACK, "The transformed Bézier path clips its corner")
	_check_pixel(image, Vector2(120.0, 26.0), Color.LIME, "Absolute and relative arcs fill an ellipse")
	_check_pixel(image, Vector2(120.0, 18.0), Color.BLACK, "An elliptical arc excludes the area above its radius")
	_check_pixel(image, Vector2(196.0, 29.0), Color.MAGENTA, "An arc expands radii too small to reach its endpoint")
	_check_pixel(image, Vector2(272.0, 58.0), Color.ORANGE, "The large-arc flag includes the lower part of the circle")
	_check_pixel(image, Vector2(285.0, 29.0), Color.BLACK, "The sweep leaves a cutout next to the closing segment")
	_check_pixel(image, Vector2(18.0, 125.0), Color.CYAN, "The first subpath is filled around the hole")
	_check_pixel(image, Vector2(44.0, 125.0), Color.BLACK, "An implicitly closed subpath can form an even-odd hole")
	_check_pixel(image, Vector2(120.0, 125.0), Color.BLUE, "Nonzero winding fills both same-direction subpaths")
	_check_pixel(image, Vector2(197.0, 125.0), Color.YELLOW, "Relative move and repeated coordinates use the padding-box origin")
	_check_pixel(image, Vector2(175.0, 125.0), Color.BLACK, "The SVG path does not start at the border edge")
	_check_pixel(image, Vector2(250.0, 104.0), Color.WHITE, "Malformed SVG arc flags preserve the last valid clip-path declaration")
	_check_pixel(image, Vector2(287.0, 137.0), Color.BLACK, "Adjacent decimals and exponents parse without extending the triangular clip")
	_check(label.get_element_at(Vector2(44.0, 125.0)) == label.get_element_by_id("behind") and
		label.get_element_at(Vector2(120.0, 125.0)) == label.get_element_by_id("solid") and
		label.get_element_at(Vector2(120.0, 26.0)) == label.get_element_by_id("arc"),
		"Pointer hits share the arc and multi-contour winding geometry")
	var reference: HTMLElement = label.get_element_by_id("reference")
	_check(reference.set_attribute("style", "width: 80px; height: 80px") == OK, "Resize the clipped element")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(197.0, 125.0), Color.YELLOW, "Path coordinates retain their CSS pixel size")
	_check_pixel(image, Vector2(215.0, 125.0), Color.BLACK, "A larger reference box does not scale path coordinates")
	viewport.queue_free()

	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""<!doctype html><style>
		body { margin: 0; }
		#clip { width: 168px; height: 28px; font: 20px / 24px monospace;
			clip-path: path(evenodd, "M0 -10H168V38H0Z M24 -2H96V26H24"); }
	</style><div id="clip">ABCDEFGHIJK</div>""") == OK and layout.layout(Vector2i(180, 80)) == OK,
		"Lay out text clipped by multiple SVG contours")
	_check(not layout.get_selection_rects(0, 1).is_empty() and layout.get_selection_rects(3, 5).is_empty()
		and not layout.get_selection_rects(9, 10).is_empty(), "Selection intervals exclude the SVG path hole")


func _check_responsive_clip_shapes() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 170)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		#behind { position: absolute; inset: 0; display: block; }
		.tile { position: absolute; box-sizing: border-box; width: 64px; height: 64px; }
		.top { top: 12px; } .bottom { top: 94px; }
		#responsive { left: 12px; background: lime; border: 4px solid red; --pad: 5px;
			clip-path: content-box shape(from var(--pad) top, hline to calc(50% + var(--pad)),
				vline to bottom, hline by -50%, close); }
		#curves { left: 108px; background: red;
			clip-path: shape(from left center, curve to center top with 0px 12px from origin / -20px 0px from end,
				smooth to right center with 64px 12px, curve by -50% 50% with 0px 32px from start,
				smooth by -50% -50%, close); }
		#arcs { left: 188px; background: magenta;
			clip-path: shape(from 10% 50%, arc by 80% 0 cw of 40% 30% rotate 0deg,
				arc to 10% 50% of -40% -30% cw, close); }
		#rotated_arc { left: 260px; background: orange;
			clip-path: shape(from 6px 32px, arc by 52px 0 rotate .25turn of 20px 26px,
				arc to 6px 32px of 20px 26px rotate .25turn, close); }
		#hole { left: 12px; background: cyan;
			clip-path: shape(evenodd from top left, hline to right, vline to bottom, hline to left, close,
				move by 25% 25%, hline by 50%, vline by 50%, hline by -50%); }
		#rtl { left: 108px; background: blue; direction: rtl;
			clip-path: shape(from x-start top, hline by -50%, vline to y-end, hline to x-start, close); }
		#invalid { left: 188px; background: white;
			clip-path: shape(from 0 0, line to 30px 0, line to 0 30px);
			clip-path: shape(from 0 0, hline to bottom); clip-path: shape(from 0 0, arc to 40px 40px cw); }
	</style><a id="behind" href="#behind"></a>
	<div class="tile top" id="responsive"></div><div class="tile top" id="curves"></div>
	<div class="tile top" id="arcs"></div><div class="tile top" id="rotated_arc"></div>
	<div class="tile bottom" id="hole"></div><div class="tile bottom" id="rtl"></div>
	<div class="tile bottom" id="invalid"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(45.0, 42.0), Color.LIME, "CSS variables and calculations resolve against the content box")
	_check_pixel(image, Vector2(58.0, 42.0), Color.BLACK, "The first shape is clipped before the rest of the element")
	_check_pixel(image, Vector2(120.0, 24.0), Color.RED, "Cubic and smooth cubic shape commands use anchored controls")
	_check_pixel(image, Vector2(120.0, 60.0), Color.RED, "Relative quadratic and smooth shape commands preserve the lower curve")
	_check_pixel(image, Vector2(110.0, 14.0), Color.BLACK, "The responsive Bézier path excludes its corner")
	_check_pixel(image, Vector2(220.0, 29.0), Color.MAGENTA, "Percentage arc radii resolve on their respective axes")
	_check_pixel(image, Vector2(220.0, 21.0), Color.BLACK, "The arc leaves the area beyond its vertical radius empty")
	_check_pixel(image, Vector2(292.0, 29.0), Color.ORANGE, "Arc rotation and the default counterclockwise sweep")
	_check_pixel(image, Vector2(18.0, 125.0), Color.CYAN, "Move after close starts an independent contour")
	_check_pixel(image, Vector2(44.0, 125.0), Color.BLACK, "An open contour is closed for even-odd clipping")
	_check_pixel(image, Vector2(151.0, 123.0), Color.BLUE, "Logical starting edge follows RTL direction")
	_check_pixel(image, Vector2(120.0, 123.0), Color.BLACK, "A relative horizontal segment keeps its signed offset in RTL")
	_check_pixel(image, Vector2(192.0, 98.0), Color.WHITE, "Invalid axis and missing radii preserve the previous shape")
	_check_pixel(image, Vector2(215.0, 122.0), Color.BLACK, "The surviving triangle closes implicitly")
	_check(label.get_element_at(Vector2(44.0, 125.0)) == label.get_element_by_id("behind"),
		"A compound shape lets pointer hits pass through its hole")
	var responsive: HTMLElement = label.get_element_by_id("responsive")
	_check(responsive.set_attribute("style", "width: 96px") == OK, "Resize the responsive shape's reference box")
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(58.0, 42.0), Color.LIME, "A new width recomputes the percentage-based outline")
	_check(label.get_element_at(Vector2(58.0, 42.0)) == responsive,
		"The responsive outline updates pointer hits after a layout change")
	viewport.queue_free()


func _check_sticky_positioning() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 220)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(190.0, 100.0)
	label.text = """<!doctype html><style>
		html { scrollbar-width: none; }
		body { margin: 0; background: black; }
		section { height: 160px; }
		#space { height: 46px; }
		a { display: block; position: sticky; top: 10%; height: 24px; background: #e02040; color: white; }
		#after { height: 270px; background: #202040; }
	</style><section><div id="space"></div><a id="sticky" href="#target">Sticky</a>
		<div id="flow" style="height: 20px;"></div></section><div id="after"></div>"""
	var links: Array[String] = []
	label.link_pressed.connect(func(uri: String) -> void: links.append(uri))
	viewport.add_child(label)
	var inner: HTMLLabel = HTMLLabel.new()
	inner.position = Vector2(190.0, 0.0)
	inner.size = Vector2(130.0, 100.0)
	inner.text = """<!doctype html><style>
		body { margin: 0; background: black; }
		#port { overflow: auto; scrollbar-width: none; width: 126px; height: 100px; background: #202020; }
		#section { height: 140px; width: 200px; }
		#pin { position: sticky; top: 10%; left: 8px; width: 50px; height: 20px; background: #23c091; }
	</style><div id="port"><div id="section"><div style="height: 30px;"></div>
		<a id="pin" href="#inner" style="display: block;">Pin</a></div><div style="height: 160px;"></div></div>"""
	viewport.add_child(inner)
	var table: HTMLLabel = HTMLLabel.new()
	table.position = Vector2(0.0, 105.0)
	table.size = Vector2(150.0, 96.0)
	table.text = """<!doctype html><style>
		body { margin: 0; }
		#table_port { width: 130px; height: 90px; overflow: auto; scrollbar-width: none; }
		table { width: 120px; border-spacing: 0; }
		thead { position: sticky; top: 0; z-index: 2; background: #8050d0; }
		tbody tr:first-child { position: sticky; top: 22px; z-index: 1; background: #2070b0; }
		th, td { padding: 0; height: 22px; }
		tbody tr:last-child td { position: sticky; bottom: 0; height: 32px; background: #205050; }
	</style><div id="table_port"><div style="height: 30px;"></div><table>
		<thead id="heading"><tr><th>Title</th></tr></thead><tbody>
		<tr id="row"><td>Row</td></tr><tr><td style="height: 180px;">Ordinary</td></tr>
		<tr><td id="cell">Cell</td></tr></tbody></table></div>"""
	viewport.add_child(table)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(165.0, 105.0)
	markdown.size = Vector2(150.0, 110.0)
	markdown.parse_html = true
	var test_texture: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(icon, test_texture)
	markdown.text = """<div style="width: 110px; height: 70px; overflow: auto; scrollbar-width: none; background: #123456;">
		<div style="height: 24px;"></div>
		<div style="position: sticky; top: 4px; height: 16px; background: #f0b020;">Markdown</div>
		<div style="height: 140px;"></div>
	</div>"""
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	var sticky: HTMLElement = label.get_element_by_id("sticky")
	_check(is_equal_approx(sticky.get_bounding_rect().position.y, 46.0), "Sticky starts at its normal position")
	label.set_scroll_offset(Vector2i(0, 60))
	await process_frame
	await process_frame
	_check(is_equal_approx(sticky.get_bounding_rect().position.y, 70.0) and
		is_equal_approx(label.get_element_by_id("flow").get_bounding_rect().position.y, 70.0),
		"Host scrolling resolves sticky percentages against the viewport and preserves normal flow")
	_check(label.get_element_at(Vector2(70.0, 15.0)) == sticky, "The sticky box wins hits over the in-flow box below")
	_drag_pointer(viewport, Vector2(70.0, 15.0), Vector2(70.0, 15.0))
	_check(links == ["#target"], "The sticky link activates from its visual position")
	await RenderingServer.frame_post_draw
	_check_pixel(viewport.get_texture().get_image(), Vector2(70.0, 15.0), Color("#e02040"),
		"The host paints the sticky border box at its scrolled position")
	label.set_scroll_offset(Vector2i(0, 144))
	await process_frame
	await process_frame
	_check(is_equal_approx(sticky.get_bounding_rect().position.y, 136.0),
		"The sticky border box stops at the containing block's end")

	var pin: HTMLElement = inner.get_element_by_id("pin")
	_push_html_wheel(viewport, Vector2(260.0, 60.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, Vector2(260.0, 60.0), MOUSE_BUTTON_WHEEL_RIGHT)
	_check(pin.get_bounding_rect().position == Vector2(8.0, 10.0) and
		inner.get_element_at(Vector2(26.0, 16.0)) == pin and inner.get_scroll_offset() == Vector2i.ZERO,
		"Both sticky axes use the nearest internal scrollport and its own offset")
	_push_html_wheel(viewport, Vector2(260.0, 60.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, Vector2(260.0, 60.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, Vector2(260.0, 60.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(is_equal_approx(pin.get_bounding_rect().position.y, -24.0) and
		inner.get_element_at(Vector2(26.0, 16.0)) != pin, "Sticky moves out through the inner scroll clip at the section end")

	_push_html_wheel(viewport, Vector2(75.0, 160.0), MOUSE_BUTTON_WHEEL_DOWN)
	_push_html_wheel(viewport, Vector2(75.0, 160.0), MOUSE_BUTTON_WHEEL_DOWN)
	_check(is_equal_approx(table.get_element_by_id("heading").get_bounding_rect().position.y, 0.0) and
		is_equal_approx(table.get_element_by_id("row").get_bounding_rect().position.y, 22.0) and
		is_equal_approx(table.get_element_by_id("cell").get_bounding_rect().position.y, 58.0),
		"Sticky table groups, rows, and cells use the table's containing area")
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, Vector2(75.0, 110.0), Color("#8050d0"), "The sticky header keeps its own painted stacking context")
	_check_pixel(image, Vector2(75.0, 132.0), Color("#2070b0"), "The sticky table row paints over the table body")
	var markdown_origin: Vector2i = Vector2i(-1, -1)
	for y: int in range(105, 175):
		for x: int in range(165, 300):
			if image.get_pixel(x, y).is_equal_approx(Color("#123456")):
				markdown_origin = Vector2i(x, y)
				break
		if markdown_origin.x >= 0:
			break
	_check(markdown_origin.x >= 0, "Markdown draws the sticky fixture's scroll box")
	if markdown_origin.x >= 0:
		_push_html_wheel(viewport, Vector2(markdown_origin + Vector2i(90, 50)), MOUSE_BUTTON_WHEEL_DOWN)
		await RenderingServer.frame_post_draw
		_check_pixel(viewport.get_texture().get_image(), Vector2(markdown_origin + Vector2i(95, 8)), Color("#f0b020"),
			"Markdown HTML blocks paint sticky content after scrolling their own box")
	viewport.queue_free()

	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""<!doctype html><style>body { margin: 0; }
		#bottom { position: sticky; bottom: 10px; height: 24px; }
		#tall { position: sticky; top: 5px; bottom: 10px; height: 150px; }
	</style><div style="height: 180px;"></div><div id="bottom">Bottom</div>
		<div id="tall">Tall</div><div style="height: 120px;"></div>""") == OK and
		layout.layout(Vector2i(200, 100)) == OK, "Lay out bottom and oversized sticky boxes")
	_check(is_equal_approx(_rect(layout, "bottom").position.y, 66.0) and
		is_equal_approx(_rect(layout, "tall").position.y, 5.0), "Bottom offsets apply initially and an oversized box can reduce the end inset")
	var start: int = layout.get_plain_text().find("Bottom")
	var selection: Array[Rect2] = layout.get_selection_rects(start, start + 6)
	_check(not selection.is_empty() and selection[0].position.y >= 66.0 and selection[0].get_center().y < 90.0,
		"Selection geometry follows sticky text")
	_check(layout.parse("""<!doctype html><style>body { margin: 0; }
		main { display: grid; grid-template-rows: 140px 80px; align-items: start; }
		#grid_pin { grid-row: 2; position: sticky; bottom: 10px; height: 24px; }
	</style><main><div id="grid_pin"></div></main>""") == OK and
		layout.layout(Vector2i(200, 100)) == OK and is_equal_approx(_rect(layout, "grid_pin").position.y, 140.0),
		"A sticky Grid item stays inside its assigned grid area")


func _check_author_hover() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(240, 160)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(240.0, 160.0)
	label.text = """<style>
		body { margin: 0; }
		.card { display: block; width: 80px; height: 40px; }
	</style><div id="hover_card" class="card"></div><div style="height: 200px"></div>
	<div id="hover_second" class="card"></div><div style="height: 160px"></div>"""
	label.stylesheet = ".card:hover { width: 110px; } #hover_card:is(:hover) { width: 130px; }"
	viewport.add_child(label)
	await process_frame
	await process_frame
	var card: HTMLElement = label.get_element_by_id("hover_card")
	_check(is_equal_approx(card.get_bounding_rect().size.x, 80.0), "The document rule applies without hover")
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = Vector2(15.0, 15.0)
	motion.global_position = motion.position
	viewport.push_input(motion, true)
	await process_frame
	await process_frame
	await process_frame
	_check(card.matches(":hover"), "The control updates the real element's hover state")
	_check(is_equal_approx(card.get_bounding_rect().size.x, 130.0), "Additional rules trigger hover restyling and share specificity")
	label.set_scroll_offset(Vector2i(0, 240))
	await process_frame
	await process_frame
	await process_frame
	var second: HTMLElement = label.get_element_by_id("hover_second")
	_check(not card.matches(":hover") and second.matches(":hover"), "Scrolling updates the hover match under a stationary pointer")
	_check(is_equal_approx(second.get_bounding_rect().size.x, 110.0), "Scrolling activates the newly hovered rule")
	viewport.queue_free()


func _check_focus_within() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(240, 160)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(240, 160)
	label.text = """<style>
	body { margin: 0; }
	.panel { width: 80px; }
	.panel:focus-within { width: 130px; }
	.panel:has(> button:focus)::before { content: attr(data-focus-label); }
	</style>
	<div class="panel" id="first_panel" data-focus-label="FIRST-FOCUSED"><button type="button" id="first_button">First</button></div>
	<div class="panel" id="second_panel" data-focus-label="SECOND-FOCUSED"><button type="button" id="second_button">Second</button></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	_check(label.query_selector(".panel:focus-within") == null, "No HTML element is focused initially")
	label.select_all()
	_check("FOCUSED" not in label.get_selected_text(), "The relational pseudo-element starts inactive")
	label.grab_focus()
	var tab: InputEventKey = InputEventKey.new()
	tab.keycode = KEY_TAB
	tab.pressed = true
	viewport.push_input(tab)
	await process_frame
	await process_frame
	var first: HTMLElement = label.get_element_by_id("first_panel")
	var second: HTMLElement = label.get_element_by_id("second_panel")
	var button: HTMLElement = label.get_element_by_id("first_button")
	_check(button.matches(":focus:focus-within"), "The focused button itself matches :focus-within")
	_check(button.matches(":enabled:not(:disabled)"), "An enabled button exposes its interaction state")
	_check(first.matches(":focus-within") and not second.matches(":focus-within"), "Only ancestors of the focused button match")
	_check(is_equal_approx(first.get_bounding_rect().size.x, 130.0), "Focus-within restyles the matching ancestor")
	label.select_all()
	_check("FIRST-FOCUSED" in label.get_selected_text(), "Focus activates a relational generated-content rule")
	var space: InputEventKey = InputEventKey.new()
	space.keycode = KEY_SPACE
	space.pressed = true
	viewport.push_input(space)
	await process_frame
	_check(button.matches(":active") and first.matches(":has(> button:active)"), "Pressing Space activates the focused button")
	space.pressed = false
	viewport.push_input(space)
	await process_frame
	_check(not button.matches(":active"), "Releasing Space clears the active match")
	viewport.push_input(tab)
	await process_frame
	await process_frame
	_check(not first.matches(":focus-within") and second.matches(":focus-within"), "Tab updates focus ancestry")
	_check(is_equal_approx(first.get_bounding_rect().size.x, 80.0) and is_equal_approx(second.get_bounding_rect().size.x, 130.0),
		"Moving focus refreshes both ancestor styles")
	label.select_all()
	_check("FIRST-FOCUSED" not in label.get_selected_text() and "SECOND-FOCUSED" in label.get_selected_text(),
		"Moving focus replaces the generated content")
	label.release_focus()
	await process_frame
	await process_frame
	_check(label.query_selector(".panel:focus-within") == null, "Releasing focus clears the match")
	label.select_all()
	_check("FOCUSED" not in label.get_selected_text(), "Releasing focus removes the relational generated content")
	_check(button.set_attribute("disabled", "") == OK, "Disable the first button")
	await process_frame
	await process_frame
	_check(button.matches(":disabled:not(:enabled)"), "Disabling a button updates selector state")
	viewport.queue_free()


func _send_dom_key(viewport: SubViewport, key: Key, unicode: int = 0, shift: bool = false, pressed: bool = true) -> void:
	var event: InputEventKey = InputEventKey.new()
	event.keycode = key
	event.unicode = unicode
	event.shift_pressed = shift
	event.pressed = pressed
	viewport.push_input(event)


func _check_form_state_selectors() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""<!doctype html><form id="owner" novalidate>
		<fieldset id="group" disabled><legend><input id="legend" required></legend><input id="disabled" required></fieldset>
		<input id="readonly" required readonly>
		<input id="hidden" required type="hidden">
		<input id="toggle" required type="checkbox">
		<input id="radio_one" type="radio" name="group" required><input id="radio_two" type="radio" name="group">
		<select id="choice" required><option value="">Choose</option><option value="known">Known</option></select>
		<button id="submit">Send</button><button id="reset" type="reset">Reset</button>
	</form><form id="other"></form><input id="external" required form="owner">
	<p id="plain" required>Plain</p><template id="template"><input id="inert_missing" required></template>""") == OK,
		"Parse a document with form constraint selectors")
	var owner: HTMLFormElement = layout.get_element_by_id("owner") as HTMLFormElement
	var other: HTMLFormElement = layout.get_element_by_id("other") as HTMLFormElement
	var group: HTMLElement = layout.get_element_by_id("group")
	var legend: HTMLInputElement = layout.get_element_by_id("legend") as HTMLInputElement
	var disabled: HTMLElement = layout.get_element_by_id("disabled")
	var read_only: HTMLElement = layout.get_element_by_id("readonly")
	var hidden: HTMLElement = layout.get_element_by_id("hidden")
	var toggle: HTMLInputElement = layout.get_element_by_id("toggle") as HTMLInputElement
	var radio_one: HTMLInputElement = layout.get_element_by_id("radio_one") as HTMLInputElement
	var radio_two: HTMLInputElement = layout.get_element_by_id("radio_two") as HTMLInputElement
	var choice: HTMLSelectElement = layout.get_element_by_id("choice") as HTMLSelectElement
	var external: HTMLInputElement = layout.get_element_by_id("external") as HTMLInputElement
	var plain: HTMLElement = layout.get_element_by_id("plain")
	_check(owner.matches(":invalid") and other.matches(":valid") and group.matches(":invalid") and legend.matches(":required:read-write:invalid"),
		"Form and fieldset validity include a first-legend control even under a disabled fieldset")
	_check(disabled.matches(":required:read-only:not(:valid):not(:invalid)") and read_only.matches(":required:read-only:not(:valid):not(:invalid)"),
		"Disabled and read-only controls retain optionality but are barred from validation")
	_check(hidden.matches(":read-only:not(:required):not(:optional):not(:valid):not(:invalid)") and
		plain.matches(":read-only:not(:required):not(:optional):not(:valid):not(:invalid)"), "Hidden controls and ordinary HTML lack these input constraints")
	_check(layout.get_element_by_id("submit").matches(":valid:read-only") and layout.get_element_by_id("reset").matches(":not(:valid):not(:invalid)"),
		"Submit buttons participate in validation")
	_check(radio_one.matches(":required:invalid") and radio_two.matches(":optional:invalid") and toggle.matches(":required:invalid") and choice.matches(":required:invalid"),
		"Checkboxes, radio groups, and the required dropdown placeholder report missing values")
	legend.value = "present"
	toggle.checked = true
	radio_two.checked = true
	choice.selected_index = 1
	_check(group.matches(":valid") and legend.matches(":valid") and radio_one.matches(":valid") and radio_two.matches(":valid") and choice.matches(":valid"),
		"Live control values immediately update their own, their peers', and their fieldset's selectors")
	_check(owner.matches(":invalid") and external.set_attribute("form", "other") == OK and owner.matches(":valid") and other.matches(":invalid"),
		"An associated control updates validity according to its current form owner")
	_check(external.remove_attribute("required") == OK and external.matches(":optional:valid") and other.matches(":valid"),
		"Removing required updates the control and its form immediately")
	_check(owner.reset() == OK and owner.matches(":invalid") and group.matches(":invalid") and radio_two.matches(":invalid") and choice.matches(":invalid"),
		"Form reset updates all related validity states")
	var fresh: HTMLInputElement = layout.create_element("input") as HTMLInputElement
	var template: HTMLTemplateElement = layout.get_element_by_id("template") as HTMLTemplateElement
	var inert: HTMLElement = template.content.query_selector(":invalid")
	_check(fresh.matches(":optional:valid:read-write") and inert != null and inert.get_attribute("id") == "inert_missing",
		"Detached elements and template contents expose their own constraint state")
	_check(layout.query_selector_all(":invalid").has(legend) and not layout.query_selector_all(":invalid").has(inert),
		"Document queries preserve isolation from template content")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(320, 220)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>body { margin: 0; }
		#summary { width: 90px; height: 10px; }
		@supports selector(form:has(:invalid)) { html:has(form:invalid) #summary { width: 120px; } }
	</style><form id="form"><input id="text" required minlength="3" maxlength="4">
	<textarea id="area" minlength="2"></textarea><button type="submit">Send</button></form><div id="summary"></div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var form: HTMLFormElement = label.get_element_by_id("form") as HTMLFormElement
	var text_input: HTMLInputElement = label.get_element_by_id("text") as HTMLInputElement
	var area: HTMLTextAreaElement = label.get_element_by_id("area") as HTMLTextAreaElement
	_check(_label_rect(label, "summary").size.x == 120.0 and area.matches(":optional:valid"), "Invalid forms affect stylesheet rules and optional empty values are valid")
	text_input.value = "x"
	_check(text_input.matches(":valid") and form.matches(":valid"), "A script-set value does not trigger length validation")
	text_input.value = ""
	await process_frame
	await process_frame
	_check(label.focus_element(text_input), "Focus the required field for a user edit")
	_send_dom_key(viewport, KEY_NONE, 0x1F642)
	_check(text_input.value.length() == 1 and text_input.matches(":invalid"), "A user edit is checked in UTF-16 units against minlength")
	_send_dom_key(viewport, KEY_X, 0x78)
	_check(text_input.matches(":valid") and form.matches(":valid"), "A second user edit satisfies the UTF-16 minimum")
	await process_frame
	await process_frame
	_check(_label_rect(label, "summary").size.x == 90.0, "A validity transition updates related layout through CSS")
	_check(text_input.set_attribute("maxlength", "2") == OK and text_input.matches(":invalid") and label.submit_form(form) == ERR_INVALID_DATA,
		"Shrinking maxlength after a user edit updates selectors and submission through the same validation")
	_check(form.set_attribute("novalidate", "") == OK and label.submit_form(form) == OK and form.matches(":invalid"),
		"Novalidate bypasses submission checks while keeping the CSS validity state")
	text_input.value = "manual"
	await process_frame
	await process_frame
	_check(text_input.matches(":valid") and label.focus_element(area), "A script assignment clears user length constraints")
	_send_dom_key(viewport, KEY_X, 0x78)
	await process_frame
	await process_frame
	_check(area.matches(":invalid") and label.focus_element(text_input) and area.matches(":invalid"),
		"Optional nonempty textareas keep user length constraints after focus leaves")
	_check(form.reset() == OK and area.matches(":optional:valid") and form.matches(":invalid"), "Reset restores empty optional and required validity separately")
	viewport.queue_free()


func _check_extended_html_elements() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var markup: String = """<!doctype html><style>
	body { margin: 0; }
	ruby { font-size: 28px; }
	#under { ruby-position: under; }
	q { quotes: "[" "]" "(" ")"; }
	</style>
	<main id="main"><article id="article"><section id="section"><mark>Highlight</mark></section></article></main>
	<aside id="aside">Other</aside><search id="search">Search</search>
	<dl id="definition"><dt>Key</dt><dd>Value</dd></dl><menu id="menu"><li>Item</li></menu>
	<blockquote id="quotation" cite="doc">Quote</blockquote><q>First <q>Second</q></q><br id="break"><hr id="rule">
	<label id="gauge_label" for="progress">Progress</label><progress id="progress" max="8">Fallback</progress>
	<meter id="meter" min="2" max="8" value="9" low="3" high="6" optimum="5"></meter>
	<details id="first" name="group" open><summary id="title">One</summary><div id="body">Details</div></details>
	<details id="second" name="group" open><summary>Two</summary><div id="other">Second body</div></details>
	<p><ruby><span id="base">漢字</span><rp>(</rp><rt id="note">かんじ</rt><rp>)</rp></ruby>
	<ruby id="under"><span id="base_under">中文</span><rt id="note_under">zhōng wén</rt></ruby></p>"""
	_check(layout.parse(markup) == OK and layout.layout(Vector2i(360, 650)) == OK, "Lay out the extended HTML elements")
	var article: HTMLArticleElement = layout.get_element_by_id("article") as HTMLArticleElement
	var section: HTMLSectionElement = layout.get_element_by_id("section") as HTMLSectionElement
	var description_list: HTMLDListElement = layout.get_element_by_id("definition") as HTMLDListElement
	var menu: HTMLMenuElement = layout.get_element_by_id("menu") as HTMLMenuElement
	var quotation: HTMLQuoteElement = layout.get_element_by_id("quotation") as HTMLQuoteElement
	var line_break: HTMLBRElement = layout.get_element_by_id("break") as HTMLBRElement
	var rule: HTMLHRElement = layout.get_element_by_id("rule") as HTMLHRElement
	var progress: HTMLProgressElement = layout.get_element_by_id("progress") as HTMLProgressElement
	var meter: HTMLMeterElement = layout.get_element_by_id("meter") as HTMLMeterElement
	var first: HTMLDetailsElement = layout.get_element_by_id("first") as HTMLDetailsElement
	var second: HTMLDetailsElement = layout.get_element_by_id("second") as HTMLDetailsElement
	if [article, section, description_list, menu, quotation, line_break, rule, progress, meter, first, second,].has(null):
		_check(false, "Create typed handles for all extended HTML elements")
		return
	_check(section.get_parent() == article and article.clone_node() is HTMLArticleElement
		and layout.create_element("section") is HTMLSectionElement and layout.get_element_by_id("aside").get_class() == "HTMLElement",
		"Sectioning handles use the existing factory")
	_check(_rect(layout, "main").size.x == 360.0 and _rect(layout, "search").size.x == 360.0
		and "[First (Second)]" in layout.get_plain_text(), "Default block layout and nested quote marks apply")
	menu.compact = true
	description_list.compact = true
	line_break.clear = "left"
	rule.no_shade = true
	rule.width = "30"
	_check(menu.has_attribute("compact") and description_list.has_attribute("compact")
		and line_break.get_attribute("clear") == "left" and rule.has_attribute("noshade") and rule.get_attribute("width") == "30"
		and quotation.cite == "doc", "Base element properties reflect source attributes")
	_check(progress.position == -1.0 and progress.matches(":indeterminate") and progress.get_labels().size() == 1
		and "Fallback" not in layout.get_plain_text(), "Progress distinguishes indeterminate state and has an associated label")
	progress.value = 4.0
	_check(is_equal_approx(progress.position, 0.5) and meter.value == 8.0 and meter.low == 3.0 and meter.high == 6.0,
		"Progress and meter normalize current values")
	_check(first.open and first.matches(":open") and not second.open and layout.get_element_by_id("other").get_client_rects().is_empty(),
		"A disclosure group starts with one open element")
	second.open = true
	_check(not first.open and second.open and layout.layout(Vector2i(360, 650)) == OK
		and layout.get_element_by_id("body").get_client_rects().is_empty() and _rect(layout, "other").has_area(),
		"Opening a peer keeps closed content queryable and removes its geometry")
	var peer: HTMLDetailsElement = layout.create_element("details") as HTMLDetailsElement
	peer.name = "group"
	peer.open = true
	var fragment: HTMLDocumentFragment = layout.create_document_fragment()
	_check(fragment.append_child(peer) == OK and second.get_parent().insert_fragment_before(fragment, second) == OK
		and second.open and not peer.open, "A moved disclosure closes when an existing group peer is open")
	_check(layout.layout(Vector2i(360, 650)) == OK, "Reflow the moved disclosure")
	_check(_rect(layout, "note").has_area() and _rect(layout, "note_under").has_area()
		and _rect(layout, "note").end.y <= _rect(layout, "base").end.y
		and _rect(layout, "note_under").position.y > _rect(layout, "base_under").position.y
		and layout.query_selector("rp").get_client_rects().is_empty(),
		"Horizontal ruby draws under and over annotations and hides fallback parentheses")

	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(460, 460)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(viewport.size)
	label.text = """<!doctype html><style>input, select { display:block; } details { margin: 8px; }</style>
	<details id="widget"><summary id="summary"><span>Open</span></summary><input id="inside"></details>
	<input id="query" list="choices" value="al" type="search"><datalist id="choices">
	<option value="alpha"><option value="beta"><option value="alone" disabled></datalist>
	<form id="form"><input type="email" id="email" name="email" value="wrong">
	<input type="number" id="number" name="count" value="2" min="0" max="8" step="2">
	<input type="range" id="range" min="0" max="10" value="4" step="2">
	<input type="password" id="password" value="secret">
	<select id="multi" name="items" size="3" multiple>
	<option id="option_a" selected value="a">A</option><option id="option_b" selected value="b">B</option>
	<option disabled selected value="c">C</option><option value="d">D</option></select>
	<output id="output" for="number" name="result">Initial</output>
	<input type="submit" id="send" name="action" value="save">
	<input type="reset" id="resetter"></form>"""
	viewport.add_child(label)
	var submissions: Array[Array] = []
	label.form_submitted.connect(func(_form: HTMLFormElement, entries: Array[Dictionary], submitter: HTMLElement) -> void:
		submissions.append([entries, submitter])
	)
	await process_frame
	await process_frame
	var widget: HTMLDetailsElement = label.get_element_by_id("widget") as HTMLDetailsElement
	var summary: HTMLElement = label.get_element_by_id("summary")
	var inside: HTMLElement = label.get_element_by_id("inside")
	var search: HTMLInputElement = label.get_element_by_id("query") as HTMLInputElement
	var suggestions: HTMLDataListElement = label.get_element_by_id("choices") as HTMLDataListElement
	var form: HTMLFormElement = label.get_element_by_id("form") as HTMLFormElement
	var email: HTMLInputElement = label.get_element_by_id("email") as HTMLInputElement
	var number: HTMLInputElement = label.get_element_by_id("number") as HTMLInputElement
	var slider: HTMLInputElement = label.get_element_by_id("range") as HTMLInputElement
	var output: HTMLOutputElement = label.get_element_by_id("output") as HTMLOutputElement
	var multi: HTMLSelectElement = label.get_element_by_id("multi") as HTMLSelectElement
	var submitter: HTMLInputElement = label.get_element_by_id("send") as HTMLInputElement
	if [widget, summary, inside, search, suggestions, form, email, number, slider, output, multi, submitter,].has(null):
		_check(false, "Find the extended controls in HTMLLabel")
		viewport.queue_free()
		return
	_check(not label.focus_element(inside) and label.focus_element(summary), "Only the summary is focusable in a closed disclosure")
	_send_dom_key(viewport, KEY_ENTER)
	await process_frame
	await process_frame
	_check(widget.open and label.focus_element(inside), "Keyboard activation reveals the disclosure contents")
	_check(search.type == "search" and suggestions.get_options().size() == 3 and label.focus_element(search),
		"A typed text field can use its suggestion list")
	_send_dom_key(viewport, KEY_DOWN)
	_send_dom_key(viewport, KEY_ENTER)
	_check(search.value == "alpha", "A keyboard suggestion changes the field")
	_send_text_history_key(viewport, KEY_Z)
	_check(search.value == "al", "Choosing one suggestion is a single undo operation")
	_check(email.matches(":invalid") and label.submit_form(form, submitter) == ERR_INVALID_DATA, "Email constraints apply to submission")
	email.value = "user@example.test"
	_check(label.focus_element(number), "Focus the numeric field")
	_send_dom_key(viewport, KEY_UP)
	_check(number.value_as_number == 4.0, "Number input uses step arithmetic")
	_check(label.focus_element(slider), "Focus the range field")
	_send_dom_key(viewport, KEY_END)
	_check(slider.value_as_number == 10.0, "End moves the range thumb to its upper value")
	var slider_rect: Rect2 = _label_rect(label, "range")
	_drag_pointer(viewport, slider_rect.get_center(), Vector2(slider_rect.position.x + 1.0, slider_rect.get_center().y))
	_check(slider.value_as_number == 0.0, "Dragging the range thumb updates the value")
	var password: HTMLInputElement = label.get_element_by_id("password") as HTMLInputElement
	_check(password != null and label.focus_element(password), "Focus the masked password field")
	if password != null:
		password.set_selection_range(0, password.value.length())
		var old_clipboard: String = DisplayServer.clipboard_get()
		DisplayServer.clipboard_set("clipboard sentinel")
		var clipboard_before_copy: String = DisplayServer.clipboard_get()
		_send_text_history_key(viewport, KEY_C)
		_send_text_history_key(viewport, KEY_X)
		_check(password.value == "secret" and DisplayServer.clipboard_get() == clipboard_before_copy,
			"Password copy and cut keep the secret out of the clipboard")
		DisplayServer.clipboard_set(old_clipboard)
	_check(multi.multiple and multi.size == 3 and multi.get_selected_options().size() == 3 and multi.value == "a",
		"Multiple selections have a stable first value and retain disabled selected options")
	output.value = "Changed"
	output.default_value = "Reset text"
	var cloned_output: HTMLOutputElement = output.clone_node() as HTMLOutputElement
	_check(cloned_output != null and cloned_output.value == "Changed" and cloned_output.default_value == "Reset text"
		and output.get_form() == form and output in form.get_controls(), "An output keeps both values through a shallow clone")
	await process_frame
	await process_frame
	_check(label.activate_element(submitter) and submissions.size() == 1 and submissions[0][1] == submitter,
		"An input submitter uses the existing form signal")
	var item_values: Array[String] = []
	for entry: Dictionary in form.get_form_data(submitter):
		if entry["name"] == "items":
			item_values.append(entry["value"])
	_check(item_values == ["a", "b"], "Multiple selections submit ordered entries and skip disabled options")
	var list_rect: Rect2 = _label_rect(label, "multi")
	var first_row: Vector2 = Vector2(list_rect.get_center().x, list_rect.position.y + 8.0)
	_drag_pointer(viewport, first_row, first_row)
	_check(label.get_focused_element() == multi and multi.value == "a", "A list box row accepts pointer input")
	await process_frame
	await process_frame
	_send_dom_key(viewport, KEY_DOWN, 0, true)
	_check(multi.value == "a" and (multi.get_options()[1] as HTMLOptionElement).selected,
		"Shift+Down extends the list box selection")
	multi.selected_index = 1
	_check(multi.get_selected_options().size() == 1 and multi.value == "b", "Assigning selected_index selects just one option")
	await process_frame
	await process_frame
	var resetter: HTMLInputElement = label.get_element_by_id("resetter") as HTMLInputElement
	_check(resetter != null and label.activate_element(resetter) and output.value == "Reset text" and multi.get_selected_options().size() == 3,
		"An input reset restores the output and all default selections")
	viewport.queue_free()


func _check_gauge_and_ruby_paint() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(400, 130)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(195.0, 130.0)
	label.animation_process_mode = HTMLLabel.ANIMATION_PROCESS_MANUAL
	label.text = """<!doctype html><style>body { margin:0; }
		progress { display:block; width:140px; height:12px; accent-color:#0f6b34; }
		meter { display:block; width:140px; height:12px; accent-color:#124ca0; }
		#custom { appearance:none; background-color:#a120bb; }
		</style>
		<progress id="advance"></progress><meter value="0.5"></meter><progress id="custom"></progress>"""
	viewport.add_child(label)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(200.0, 0.0)
	markdown.size = Vector2(200.0, 130.0)
	markdown.parse_html = true
	var test_icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(icon, test_icon)
	markdown.text = """<div>
	<style>progress { width:100px; height:10px; accent-color:#e04818; }</style>
	<progress value="0.5"></progress><ruby>漢<rt>かん</rt></ruby>
	</div>"""
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var gauge_region: Rect2i = Rect2i(0, 0, 180, 40)
	var first: Rect2i = _color_bounds(image, gauge_region, Color("#0f6b34"))
	_check(first.size.x > 0 and _color_bounds(image, gauge_region, Color("#124ca0")).size.x == 70
		and _color_bounds(image, gauge_region, Color("#a120bb")).size.x == 140,
		"Gauge tracks paint determinate fill and respect native appearance and accent color")
	label.advance_animation(0.5)
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	var moved: Rect2i = _color_bounds(image, gauge_region, Color("#0f6b34"))
	_check(moved.has_area() and moved.position.x > first.position.x, "Manual animation moves the indeterminate fill")
	_check(_color_bounds(image, Rect2i(200, 0, 200, 130), Color("#e04818")).has_area(),
		"Markdown HTML blocks paint the same progress element")
	markdown.select_all()
	_check("漢" in markdown.get_selected_text() and "かん" in markdown.get_selected_text(),
		"Markdown HTML selection includes the ruby base and annotation")
	viewport.queue_free()


func _check_forms() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(400, 320)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(400.0, 320.0)
	label.text = """<!doctype html><style>body { margin: 8px; } select { display: block; }</style>
		<input form="order" id="external" name="entry" value="outside">
		<form id="order">
		<label id="explicit" for="text"><span id="caption">Name</span></label>
		<input id="text" name="entry" required><input id="hidden" type="hidden" style="display:block" name="token" value="42">
		<fieldset id="group" disabled><legend id="legend"><input name="allowed" value="yes"></legend>
		<input id="disabled" name="filtered" value="no"></fieldset>
		<label id="implicit"><input id="check" type="checkbox" name="confirm" required>Confirm</label>
		<input id="first" type="radio" name="choice" value="one" checked required>
		<input id="second" type="radio" name="choice" value="two">
		<select id="select" name="size" required><option value="">Choose</option>
		<optgroup id="unavailable" label="Unavailable" disabled><option value="disabled">Disabled</option></optgroup>
		<optgroup id="available" label="Sizes"><option id="medium" value="m">Medium</option><option value="l">Large</option></optgroup></select>
		<textarea id="notes" name="notes">Default</textarea>
		<button id="submit" name="task" value="save">Save</button>
		<button id="skip" type="submit" formnovalidate>Skip validation</button>
		<button id="reset" type="reset">Reset</button></form><div style="height:420px"></div>"""
	viewport.add_child(label)
	var submitted: Array[Array] = []
	var invalid: Array[HTMLElement] = []
	var resets: Array[HTMLFormElement] = []
	var changes: Array[HTMLSelectElement] = []
	label.form_submitted.connect(func(form: HTMLFormElement, entries: Array[Dictionary], button: HTMLElement) -> void:
		submitted.append([form, entries, button])
	)
	label.form_validation_failed.connect(func(_form: HTMLFormElement, control: HTMLElement) -> void: invalid.append(control))
	label.form_reset.connect(func(form: HTMLFormElement) -> void: resets.append(form))
	label.select_changed.connect(func(select: HTMLSelectElement) -> void: changes.append(select))
	await process_frame
	await process_frame
	var form: HTMLFormElement = label.get_element_by_id("order") as HTMLFormElement
	var text: HTMLInputElement = label.get_element_by_id("text") as HTMLInputElement
	var hidden: HTMLInputElement = label.get_element_by_id("hidden") as HTMLInputElement
	var explicit_label: HTMLLabelElement = label.get_element_by_id("explicit") as HTMLLabelElement
	var implicit_label: HTMLLabelElement = label.get_element_by_id("implicit") as HTMLLabelElement
	var fieldset: HTMLFieldSetElement = label.get_element_by_id("group") as HTMLFieldSetElement
	var legend: HTMLLegendElement = label.get_element_by_id("legend") as HTMLLegendElement
	var disabled: HTMLElement = label.get_element_by_id("disabled")
	var check: HTMLInputElement = label.get_element_by_id("check") as HTMLInputElement
	var first: HTMLInputElement = label.get_element_by_id("first") as HTMLInputElement
	var second: HTMLInputElement = label.get_element_by_id("second") as HTMLInputElement
	var select: HTMLSelectElement = label.get_element_by_id("select") as HTMLSelectElement
	var medium: HTMLOptionElement = label.get_element_by_id("medium") as HTMLOptionElement
	var optgroup: HTMLOptGroupElement = label.get_element_by_id("available") as HTMLOptGroupElement
	var notes: HTMLTextAreaElement = label.get_element_by_id("notes") as HTMLTextAreaElement
	var submit: HTMLButtonElement = label.get_element_by_id("submit") as HTMLButtonElement
	var reset: HTMLButtonElement = label.get_element_by_id("reset") as HTMLButtonElement
	if [form, text, hidden, explicit_label, implicit_label, fieldset, legend, disabled, check, first, second, select,
			medium, optgroup, notes, submit, reset,].has(null):
		_check(false, "Create typed handles for the form and its controls")
		viewport.queue_free()
		return
	_check(explicit_label.html_for == "text" and explicit_label.get_control() == text and implicit_label.get_control() == check,
		"Explicit and implicit labels point to their controls")
	_check(fieldset.get_form() == form and legend.get_form() == form and optgroup.label == "Sizes"
		and select.get_options().size() == 4 and select.get_form() == form, "Forms expose the group and select relationships")
	_check(disabled.matches(":disabled") and legend.query_selector("input:enabled") != null and not label.focus_element(disabled),
		"Only the first legend exempts its controls from fieldset disability")
	_check(hidden.value == "42" and hidden.get_client_rects().is_empty() and not label.focus_element(hidden),
		"Hidden fields have a value but no geometry or focus")
	_check(label.submit_form(form) == ERR_INVALID_DATA and invalid == [text], "Report the first required text field")
	var caption: Vector2 = _label_rect(label, "caption").get_center()
	_drag_pointer(viewport, caption, caption)
	_check(label.get_focused_element() == text, "Clicking a label descendant focuses the associated field")
	text.value = "inside"
	_check(label.activate_element(implicit_label) and check.checked, "Activating an implicit label toggles the checkbox")
	_check(label.submit_form(form) == ERR_INVALID_DATA and invalid == [text, select], "Required select rejects its placeholder")
	await process_frame
	await process_frame
	_check(label.activate_element(label.get_element_by_id("skip")) and submitted.size() == 1,
		"The submit button can skip validation")
	_check(label.focus_element(first), "Focus the checked radio")
	_send_dom_key(viewport, KEY_RIGHT)
	_check(second.checked and not first.checked and label.get_focused_element() == second,
		"Arrow keys move focus and selection together")
	await process_frame
	await process_frame
	_check(label.focus_element(select), "Focus the HTML select")
	_send_dom_key(viewport, KEY_SPACE)
	_send_dom_key(viewport, KEY_DOWN)
	_send_dom_key(viewport, KEY_ENTER)
	_check(medium.selected and select.selected_index == 2 and select.value == "m" and changes == [select],
		"Keyboard selection skips a disabled option group and emits once")
	await process_frame
	await process_frame
	_check(label.activate_element(select), "Open the self-drawn menu for a pointer selection")
	_send_dom_key(viewport, KEY_END)
	var menu: Control = label.get_node_or_null("_HTMLSelectMenu") as Control
	await process_frame
	if menu != null and menu.is_visible_in_tree():
		var last_row: Vector2 = menu.position + Vector2(menu.size.x / 2.0, menu.size.y * 0.9)
		_drag_pointer(viewport, last_row, last_row)
	_check(select.value == "l" and changes == [select, select],
		"The self-drawn menu accepts a pointer selection (value %s, changes %d)" % [select.value, changes.size()])
	select.value = "m"
	_check(changes.size() == 2, "Selecting by script keeps the user signal quiet")
	await process_frame
	await process_frame
	_check(label.focus_element(text), "Focus the text input before implicit submit")
	_send_dom_key(viewport, KEY_ENTER)
	var expected: Array[Dictionary] = [
		{"name": "entry", "value": "outside"},
		{"name": "entry", "value": "inside"},
		{"name": "token", "value": "42"},
		{"name": "allowed", "value": "yes"},
		{"name": "confirm", "value": "on"},
		{"name": "choice", "value": "two"},
		{"name": "size", "value": "m"},
		{"name": "notes", "value": "Default"},
		{"name": "task", "value": "save"},
	]
	_check(submitted.size() == 2 and submitted.back() == [form, expected, submit],
		"Enter submits ordered entries, duplicate names, and only the active submit button")
	submit.disabled = true
	await process_frame
	await process_frame
	_check(label.focus_element(text), "Refocus the text field with a disabled default submitter")
	_send_dom_key(viewport, KEY_ENTER)
	_check(submitted.size() == 2, "Enter stops at the disabled default submitter")
	submit.disabled = false
	await process_frame
	await process_frame
	_check(label.activate_element(select), "Open the select before scrolling its label")
	if menu != null:
		var menu_y: float = menu.position.y
		label.set_scroll_offset(Vector2i(0, 20))
		await process_frame
		await process_frame
		_check(menu.is_visible_in_tree() and is_equal_approx(menu_y - menu.position.y, 20.0),
			"The open menu follows its trigger when the label scrolls")
	notes.value = "Edited"
	await process_frame
	await process_frame
	_check(label.activate_element(reset) and resets == [form] and notes.value == "Default" and text.value.is_empty()
		and first.checked and not second.checked and not check.checked and select.selected_index == 0,
		"The reset button restores all default states and emits once")
	_check(text.set_attribute("value", "New default") == OK and text.value == "New default",
		"Reset clears the text field's dirty state")
	viewport.queue_free()


func _check_dom_input() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(310, 160)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var before: Button = Button.new()
	var after: Button = Button.new()
	var label: HTMLLabel = HTMLLabel.new()
	label.position = Vector2(0, 25)
	label.size = Vector2(310, 130)
	label.text = """<style>body { margin: 0; } #field { transform: rotate(5deg); color: #ff0000; } #command { display: block; margin-top: 120px; }</style>
		<section><a id="link" href="#field" tabindex="2">Jump</a>
		<input id="field" value="A中" maxlength="4" tabindex="1">
		<input id="check" type="checkbox" checked>
		<input id="disabled" disabled>
		<span id="extra" tabindex="0">Extra</span>
		<span id="negative" tabindex="-1">Negative</span>
		<button type="button" id="command">Command</button></section>"""
	var controls: Control = Control.new()
	viewport.add_child(controls)
	controls.add_child(before)
	controls.add_child(label)
	controls.add_child(after)
	after.position.y = 155.0
	before.focus_next = before.get_path_to(label)
	after.focus_previous = after.get_path_to(label)
	label.focus_next = label.get_path_to(after)
	label.focus_previous = label.get_path_to(before)
	var changes: Array[HTMLInputElement] = []
	var commits: Array[HTMLInputElement] = []
	var links: Array[String] = []
	label.input_changed.connect(func(element: HTMLInputElement) -> void: changes.append(element))
	label.input_committed.connect(func(element: HTMLInputElement) -> void: commits.append(element))
	label.link_pressed.connect(func(uri: String) -> void: links.append(uri))
	await process_frame
	await process_frame
	var field: HTMLInputElement = label.get_element_by_id("field") as HTMLInputElement
	var checkbox: HTMLInputElement = label.query_selector("#check") as HTMLInputElement
	var link: HTMLElement = label.get_element_by_id("link")
	var extra: HTMLElement = label.get_element_by_id("extra")
	var command: HTMLElement = label.get_element_by_id("command")
	_check(field != null and checkbox != null, "Input queries return specialized handles")
	if field == null or checkbox == null:
		viewport.queue_free()
		return
	_check(field.get_bounding_rect().has_area() and field.value == "A中", "An atomic input has a CSS box and default value")
	_check(checkbox.checked and checkbox.matches(":checked"), "The checked attribute supplies initial checkedness")
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var ink_pixels: int
	for y: int in range(image.get_height()):
		for x: int in range(image.get_width()):
			var pixel: Color = image.get_pixel(x, y)
			if pixel.r > 0.7 and pixel.g < 0.3 and pixel.b < 0.3:
				ink_pixels += 1
	_check(ink_pixels > 0, "The rotated text input draws its CSS-colored glyphs inside its clip")
	var click: InputEventMouseButton = InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.position = field.get_bounding_rect().get_center() + label.position
	click.pressed = true
	viewport.push_input(click)
	click.pressed = false
	viewport.push_input(click)
	_check(label.get_focused_element() == field and field.get_selection_start() == field.get_selection_end(),
		"The painted field receives pointer focus through the rotated fragment")
	before.grab_focus()
	_send_dom_key(viewport, KEY_TAB)
	await process_frame
	_check(label.has_focus() and label.get_focused_element() == field and field.matches(":focus-visible"),
		"Tab enters the HTMLLabel at the first positive tabindex")
	field.set_selection_range(1, 2)
	_send_dom_key(viewport, KEY_NONE, 0x6587)
	_check(field.value == "A文" and field.get_attribute("value") == "A中" and changes == [field],
		"Unicode replaces the selected range without changing the default value")
	await process_frame
	_send_dom_key(viewport, KEY_NONE, 0x1f642)
	_send_dom_key(viewport, KEY_X, 0x78)
	_check(field.value == "A文🙂", "Maxlength counts UTF-16 units and accepts a complete emoji")
	_send_dom_key(viewport, KEY_BACKSPACE)
	_check(field.value == "A文", "Backspace removes a whole emoji")
	_send_dom_key(viewport, KEY_ENTER)
	_check(commits == [field], "Enter commits the current user edit once")
	field.set_attribute("value", "default")
	_check(field.value == "A文", "A dirty text input ignores updated default attributes")
	await process_frame
	await process_frame
	_send_dom_key(viewport, KEY_TAB)
	_check(label.get_focused_element() == link, "Positive tabindex is sorted before default elements")
	_send_dom_key(viewport, KEY_ENTER)
	_check(links == ["#field"], "Enter dispatches the original href")
	_send_dom_key(viewport, KEY_TAB)
	_check(label.get_focused_element() == checkbox, "Default-order Tab reaches the checkbox")
	_send_dom_key(viewport, KEY_SPACE)
	_send_dom_key(viewport, KEY_SPACE, 0, false, false)
	_check(not checkbox.checked and checkbox.has_attribute("checked") and not checkbox.matches(":checked"),
		"Space changes live checkedness and leaves the content attribute intact")
	await process_frame
	await process_frame
	_send_dom_key(viewport, KEY_TAB)
	_check(label.get_focused_element() == extra, "Tab skips a disabled control and reaches a generic tab stop")
	_send_dom_key(viewport, KEY_TAB)
	_check(label.get_focused_element() == command, "Negative tabindex is skipped and buttons stay in the same order")
	await process_frame
	await process_frame
	_check(label.get_scroll_offset().y > 0, "Keyboard focus scrolls an out-of-view DOM element into view")
	_send_dom_key(viewport, KEY_TAB)
	_check(after.has_focus(), "Tab leaves the DOM for the configured next Godot control")
	_send_dom_key(viewport, KEY_TAB, 0, true)
	_check(label.get_focused_element() == command, "Shift+Tab reenters at the last DOM element")
	_check(label.focus_element(label.get_element_by_id("negative")), "A negative tabindex permits programmatic focus")
	_check(not label.focus_element(label.get_element_by_id("disabled")), "Disabled fields reject programmatic focus")
	var dynamic_input: HTMLInputElement = label.create_element("input") as HTMLInputElement
	_check(dynamic_input != null and dynamic_input.set_attribute("value", "live") == OK and dynamic_input.value == "live",
		"New detached inputs support defaults and specialized methods")

	var edited: HTMLInputElement = label.get_element_by_id("field") as HTMLInputElement
	edited.value = "e\u0301"
	edited.set_selection_range(2, 2)
	await process_frame
	await process_frame
	_check(label.focus_element(edited), "The rotated text field can receive focus again")
	_send_dom_key(viewport, KEY_BACKSPACE)
	_check(edited.value == "", "Backspace removes a combining-character sequence as one unit")
	var clipboard: String = DisplayServer.clipboard_get()
	edited.value = "Aאב"
	_send_dom_key(viewport, KEY_END)
	_send_dom_key(viewport, KEY_HOME, 0, true)
	var copy: InputEventKey = InputEventKey.new()
	copy.keycode = KEY_C
	copy.physical_keycode = KEY_C
	copy.ctrl_pressed = true
	copy.pressed = true
	_check(edited.get_selection_start() == 0 and edited.get_selection_end() == 3,
		"Keyboard selection reports logical offsets for bidirectional text")
	DisplayServer.clipboard_set("Dockit clipboard probe")
	if DisplayServer.clipboard_get() == "Dockit clipboard probe":
		viewport.push_input(copy)
		_check(DisplayServer.clipboard_get() == "Aאב", "Copy uses the field's logical text order")
		DisplayServer.clipboard_set(clipboard)
	edited.value = ""
	edited.set_attribute("readonly", "")
	await process_frame
	await process_frame
	_send_dom_key(viewport, KEY_X, 0x78)
	_check(edited.value == "", "Readonly input retains focus and rejects typing")
	edited.set_attribute("disabled", "")
	await process_frame
	await process_frame
	_check(label.get_focused_element() == null, "Disabling the focused field clears DOM focus")
	var foreign: HTMLLayout = HTMLLayout.new()
	foreign.parse("<input>")
	foreign.layout(Vector2i(310, 90))
	_check(not label.focus_element(foreign.query_selector("input")), "Cross-layout input focus is rejected")
	var source: HTMLDocument = HTMLDocument.new()
	source.text = "<input id='shared' value='default'>"
	label.document = source
	var other: HTMLLabel = HTMLLabel.new()
	other.size = Vector2(310, 90)
	other.document = source
	controls.add_child(other)
	await process_frame
	await process_frame
	var local_value: HTMLInputElement = label.query_selector("#shared") as HTMLInputElement
	var other_value: HTMLInputElement = other.query_selector("#shared") as HTMLInputElement
	local_value.value = "edited"
	_check(other_value.value == "default", "Labels share source and keep live input state local")
	viewport.queue_free()


func _drag_pointer(viewport: SubViewport, from: Vector2, to: Vector2) -> void:
	var click: InputEventMouseButton = InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.position = from
	click.pressed = true
	viewport.push_input(click)
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = to
	motion.relative = to - from
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	viewport.push_input(motion)
	click.position = to
	click.pressed = false
	viewport.push_input(click)


func _check_table_elements() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("<table id='table'><!--title--></table>") == OK, "Parse an empty HTML table")
	var table: HTMLTableElement = layout.get_element_by_id("table") as HTMLTableElement
	if table == null:
		_check(false, "Parsed table uses the specialized handle")
		return
	_check(table.insert_row(-2) == null and table.insert_row(1) == null and table.delete_row(-1) == OK and
		table.get_t_bodies().is_empty(), "Invalid empty-table insertion leaves the tree untouched")
	var body_row: HTMLTableRowElement = table.insert_row()
	var caption: HTMLTableCaptionElement = table.create_caption()
	var head: HTMLTableSectionElement = table.create_t_head()
	var foot: HTMLTableSectionElement = table.create_t_foot()
	var head_row: HTMLTableRowElement = head.insert_row()
	var foot_row: HTMLTableRowElement = foot.insert_row()
	var body: HTMLTableSectionElement = table.get_t_bodies()[0]
	var second: HTMLTableSectionElement = table.create_t_body()
	var group: HTMLTableColElement = layout.create_element("colgroup") as HTMLTableColElement
	var column: HTMLTableColElement = layout.create_element("col") as HTMLTableColElement
	group.append_child(column)
	table.insert_before(group, head)
	_check(body_row.get_parent() == body and table.create_caption() == caption and table.create_t_head() == head and
		table.create_t_foot() == foot and table.get_inner_html().begins_with("<caption>") and
		table.get_t_bodies() == [body, second], "Create the specialized table parts in their DOM positions")
	var rows: Array[HTMLTableRowElement] = table.get_rows()
	_check(rows == [head_row, body_row, foot_row] and foot_row.row_index == 2 and foot_row.section_row_index == 0,
		"Table rows group head, body and foot while section indices use the immediate parent")
	var hidden: HTMLTableRowElement = layout.create_element("tr") as HTMLTableRowElement
	hidden.set_attribute("style", "display: none")
	table.insert_before(hidden, foot)
	_check(rows.size() == 3 and table.get_rows() == [head_row, body_row, hidden, foot_row] and hidden.section_row_index == 2,
		"Table snapshots exclude no hidden rows and direct row indices use the table")
	var last: HTMLTableRowElement = table.insert_row()
	var before: HTMLTableRowElement = table.insert_row(1)
	_check(last.get_parent() == foot and before.get_parent() == body and before.section_row_index == 0,
		"Table insertion uses the targeted row's actual parent")
	var th: HTMLTableCellElement = layout.create_element("th") as HTMLTableCellElement
	th.scope = "CoL"
	th.abbr = "Short"
	head_row.append_child(th)
	var td: HTMLTableCellElement = head_row.insert_cell(0)
	td.headers = "a b"
	td.col_span = 1500
	td.row_span = 0
	column.span = 2
	group.set_attribute("span", "garbage")
	_check(td.get_tag_name() == "td" and td.cell_index == 0 and th.cell_index == 1 and head_row.get_cells() == [td, th] and
		td.col_span == 1000 and td.get_attribute("colspan") == "1500" and td.row_span == 0 and td.headers == "a b" and
		th.scope == "col" and th.abbr == "Short" and column.span == 2 and group.span == 1,
		"Cell and column properties reflect source attributes and keep stable typed handles")
	td.set_inner_html("<table><tbody><tr><td>Nested</td></tr></tbody></table>")
	_check(table.get_rows().size() == 6 and head_row.get_cells().size() == 2,
		"Table and cell collections exclude nested tables")
	_check(table.set_t_head(second) == ERR_INVALID_PARAMETER and table.t_head == head and
		table.delete_row(6) == ERR_INVALID_PARAMETER and head_row.delete_cell(2) == ERR_INVALID_PARAMETER,
		"Invalid setters and indexes leave the DOM untouched")
	var other: HTMLLayout = HTMLLayout.new()
	other.parse("<table><thead></thead></table>")
	var foreign_head: HTMLTableSectionElement = other.query_selector("thead") as HTMLTableSectionElement
	_check(table.set_t_head(foreign_head) == ERR_INVALID_PARAMETER and table.t_head == head,
		"Table part setters reject a handle from another layout")
	_check(head_row.delete_cell(0) == OK and td.is_valid() and td.cell_index == -1 and head_row.append_child(td) == OK and
		head_row.get_cells()[1] == td and table.delete_caption() == OK and caption.is_valid() and not caption.is_connected(),
		"Table deletion detaches elements and allows reinsertion with their existing handles")
	_check(table.set_caption(caption) == OK and table.caption == caption and table.delete_row(-1) == OK and
		last.is_valid() and not last.is_connected() and last.row_index == -1,
		"A detached caption can be restored and deleting the last row preserves its handle")
	_check(table.set_t_foot(null) == OK and table.t_foot == null and foot.is_valid() and foot_row.get_parent() == foot,
		"Clearing a table section detaches its subtree with its existing handles")
	layout.clear()
	_check(not table.is_valid() and not foot.is_valid() and table.get_rows().is_empty() and foot_row.row_index == -1,
		"Clearing the layout invalidates typed table handles")


func _check_colliding_table_spans() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var markup: String = """<table style="border-spacing: 0; width: 240px">
		<colgroup span="2" style="width: 100px"></colgroup>
		<tbody><tr><td style="width: 35px; height: 20px"></td><td id="occupied" rowspan="2" style="width: 40px"></td></tr>
		<tr><td id="collision" colspan="2" style="height: 20px"></td></tr></tbody>
		<tbody><tr><td id="huge" rowspan="9999999999999999" colspan="9999999999999999" style="height: 16px"></td></tr></tbody>
	</table>"""
	_check(layout.parse(markup) == OK and layout.layout(Vector2i(500, 200)) == OK,
		"Lay out intersecting spans and saturate oversized markup attributes")
	var occupied: Rect2 = _rect(layout, "occupied")
	var cell: Rect2 = _rect(layout, "collision")
	var huge: HTMLTableCellElement = layout.get_element_by_id("huge") as HTMLTableCellElement
	_check(cell.has_area() and cell.position.x >= occupied.end.x - 0.01,
		"A colspan skips an occupied interval even when its first column was free")
	_check(huge.col_span == 1000 and huge.row_span == 65534 and huge.get_bounding_rect().size.y < 30.0,
		"Oversized spans are bounded and stay in their row group")
	var sized: String = """<style>body { margin: 0 } td { padding: 0 }</style>
		<table style="width: 50%; border-spacing: 0"><colgroup><col style="width: 40%"><col></colgroup>
		<tr style="height: 42px"><td id="height" rowspan="2" style="height: 60px"></td><td>Head</td></tr>
		<tr><td style="height: 12px"></td></tr></table>"""
	_check(layout.parse(sized) == OK and layout.layout(Vector2i(400, 200)) == OK,
		"Lay out a spanning cell with a specified row height and percentage column width")
	var initial: Rect2 = _rect(layout, "height")
	_check(is_equal_approx(initial.size.y, 60.0) and initial.size.x >= 80.0,
		"Spanning cell height credits existing row heights and the column meets its percentage minimum: %s" % initial.size)
	_check(layout.layout(Vector2i(800, 200)) == OK and _rect(layout, "height").size.x >= 160.0,
		"Percentage column widths resolve against the resized table")


func _check_table_layout() -> void:
	_check_colliding_table_spans()
	var markup: String = """<table id="table" style="border-spacing: 4px; background: black">
		<caption id="caption" style="caption-side: bottom">Caption</caption>
		<colgroup id="columns" style="width: 170px; background: red"><col style="width: 80px; background: lime"><col span="2"></colgroup>
		<tfoot><tr><td id="foot" colspan="3" style="height: 24px">Foot</td></tr></tfoot>
		<tbody><tr><td id="first" style="height: 28px"></td><td id="span" rowspan="0" colspan="2"></td></tr>
		<tr><td id="next" style="height: 28px"></td></tr></tbody>
		<thead><tr><th id="head" colspan="3" style="height: 24px"><a id="link" href="#foot">Picker</a></th></tr></thead>
		<tbody><tr><td id="second" style="height: 24px; background: blue"></td><td id="after" rowspan="99999999" colspan="0"></td></tr></tbody>
	</table>"""
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(640, 240)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(320.0, 240.0)
	label.text = markup
	viewport.add_child(label)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(320.0, 0.0)
	markdown.size = Vector2(320.0, 240.0)
	markdown.parse_html = true
	markdown.text = markup
	var icon_texture: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(icon, icon_texture)
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var head: Rect2 = _label_rect(label, "head")
	var first: Rect2 = _label_rect(label, "first")
	var next: Rect2 = _label_rect(label, "next")
	var span: Rect2 = _label_rect(label, "span")
	var second: Rect2 = _label_rect(label, "second")
	var after: Rect2 = _label_rect(label, "after")
	var foot: Rect2 = _label_rect(label, "foot")
	var caption: Rect2 = _label_rect(label, "caption")
	_check(head.position.y < first.position.y and first.position.y < second.position.y and second.position.y < foot.position.y and
		foot.position.y < caption.position.y, "Header, body, footer and bottom caption use visual table order")
	_check(is_equal_approx(first.position.x, next.position.x) and span.position.x > first.end.x and
		is_equal_approx(span.end.y, next.end.y) and is_equal_approx(second.position.x, first.position.x) and
		is_equal_approx(after.end.y, second.end.y), "Combined spans occupy full ranges and stop at their row group")
	_check(first.size.x >= 80.0 and head.size.x >= 170.0, "Column and column group width hints contribute to table sizing")
	_check(label.get_element_by_id("columns").get_client_rects().is_empty(), "Column styling has no hit-test box of its own")
	var painted: Image = viewport.get_texture().get_image()
	_check_pixel(painted, first.get_center(), Color.LIME, "Column color paints behind a transparent cell")
	_check_pixel(painted, span.get_center(), Color.RED, "Column group color covers the originating spanning cell")
	_check_pixel(painted, second.get_center(), Color.BLUE, "Cell color paints on top of its column layers")
	_check(_color_bounds(painted, Rect2i(320, 0, 320, 240), Color.LIME).has_area(), "Markdown HTML paints column styles")
	var link: HTMLElement = label.get_element_by_id("link")
	var activated: Array[String] = []
	label.link_pressed.connect(func(uri: String) -> void: activated.append(uri))
	var link_point: Vector2 = link.get_bounding_rect().get_center()
	_drag_pointer(viewport, link_point, link_point)
	label.select_all()
	_check(activated == ["#foot"] and "Picker" in label.get_selected_text(), "Reordered table cells remain clickable and selectable")
	var table: HTMLTableElement = label.get_element_by_id("table") as HTMLTableElement
	var section: HTMLTableSectionElement = table.get_t_bodies()[0]
	var updates: Array[bool] = []
	label.layout_changed.connect(func() -> void: updates.append(true))
	var added: HTMLTableRowElement = section.insert_row()
	var added_cell: HTMLTableCellElement = added.insert_cell()
	added_cell.set_text_content("Added")
	await process_frame
	await process_frame
	_check(updates.size() == 1 and added_cell.get_bounding_rect().has_area() and _label_rect(label, "foot").position.y > foot.position.y,
		"Connected DOM insertion updates spanning cells and later groups in one reflow")
	viewport.queue_free()


func _check_table_css() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(760, 500)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(380.0, 500.0)
	label.text = """<style>
		body { margin: 0; background: white }
		table { margin-bottom: 12px }
		#fixed { width: 300px; table-layout: var(--layout); --layout: fixed; border-spacing: 0 }
		#fixed td { height: 16px; padding: 0 }
		#empty { border-spacing: 4px; empty-cells: hide; background: white }
		#empty td { width: 40px; height: 20px; background: red }
		#shared { width: 150px; table-layout: fixed; border-collapse: collapse }
		#shared td { height: 20px; background: white }
		#left { border-right: 6px solid blue }
		#right { border-left: 4px double red }
		#no_edge { border-right: hidden }
		#ignored { border-left: 8px solid red }
		#tracks { width: 140px; table-layout: fixed; border-spacing: 0 }
		#tracks td { height: 12px }
	</style>
	<table id="fixed"><col style="width: 20%"><col><col>
		<tr><td id="fraction"></td><td id="second_width" style="width: 90px"></td><td id="third_width"></td></tr>
		<tr><td style="width: 500px; overflow: hidden">This later row must leave the widths alone.</td><td></td><td></td></tr>
	</table>
	<table id="empty"><tr id="empty_row"><td>   </td><td><!-- comment --></td></tr>
		<tr><td id="empty_cell"></td><td id="shown_cell"><em></em></td></tr>
	</table>
	<table id="shared"><tr><td id="left"></td><td id="right"></td></tr>
		<tr><td id="no_edge"></td><td id="ignored"></td></tr>
	</table>
	<table id="tracks"><col id="first_track" style="width: 40px"><col id="second_track" style="width: 100px">
		<tr><td id="gone"><a href="#kept">Hidden</a></td><td id="kept">Keep</td></tr>
		<tr id="row_to_fold" style="height: 25px"><td id="spanning" rowspan="2" colspan="2" style="visibility: visible">Spanning</td></tr><tr></tr>
	</table>"""
	viewport.add_child(label)
	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(380.0, 0.0)
	markdown.size = Vector2(380.0, 500.0)
	markdown.parse_html = true
	markdown.text = """<table style="width: 220px; table-layout: fixed; border-collapse: collapse">
	<tr><td style="width: 100px; border-right: 8px solid blue">Left</td><td style="border-left: 4px solid red">Right</td></tr>
	</table>"""
	var icon: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon_name: StringName in [&"task_checked", &"task_unchecked", &"file_broken",]:
		markdown.add_theme_icon_override(icon_name, icon)
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_check(is_equal_approx(_label_rect(label, "fraction").size.x, 60.0) and
		is_equal_approx(_label_rect(label, "second_width").size.x, 90.0) and
		is_equal_approx(_label_rect(label, "third_width").size.x, 150.0),
		"Fixed table widths use columns and the first visual row")
	var image: Image = viewport.get_texture().get_image()
	_check_pixel(image, _label_rect(label, "empty_cell").get_center(), Color.WHITE, "A hidden empty cell leaves its table background")
	_check_pixel(image, _label_rect(label, "shown_cell").get_center(), Color.RED, "An in-flow empty element counts as cell content")
	_check(_label_rect(label, "empty_row").size.y == 0.0, "A row of hidden empty cells has zero height")
	_check(_color_bounds(image, Rect2i(380, 0, 380, 200), Color.BLUE).has_area() and
		not _color_bounds(image, Rect2i(380, 0, 380, 200), Color.RED).has_area(),
		"Markdown HTML blocks use the same collapsed border resolver")
	var left: Rect2 = _label_rect(label, "left")
	_check_pixel(image, Vector2(left.end.x - 2.0, left.get_center().y), Color.BLUE, "Wider collapsed borders win once across the shared line")
	var ignored: Rect2 = _label_rect(label, "ignored")
	_check_pixel(image, Vector2(ignored.position.x + 1.0, ignored.get_center().y), Color.WHITE, "A hidden shared edge defeats a wider visible border")
	var gone: HTMLElement = label.get_element_by_id("gone")
	var kept: HTMLElement = label.get_element_by_id("kept")
	var table_before: Rect2 = _label_rect(label, "tracks")
	label.get_element_by_id("first_track").set_attribute("style", "width: 40px; visibility: collapse")
	label.get_element_by_id("row_to_fold").set_attribute("style", "height: 25px; visibility: collapse")
	await process_frame
	await process_frame
	var table_after: Rect2 = _label_rect(label, "tracks")
	_check(gone.is_valid() and gone.get_client_rects().is_empty() and kept.get_bounding_rect().position.x < table_before.position.x + 40.0 and
		is_equal_approx(table_before.size.x - table_after.size.x, 40.0) and table_after.size.y < table_before.size.y,
		"Collapsing tracks compresses the table while the DOM handles remain valid")
	_check(label.get_element_at(kept.get_bounding_rect().get_center()) == kept,
		"Hit testing uses the compressed cell position")
	var standalone: HTMLLayout = HTMLLayout.new()
	_check(standalone.parse("""<body style="margin: 0"><table style="width: 120px; table-layout: fixed">
	<tr><td>Short</td></tr><tr><td id="overflowing"><div style="width: 250px">Wide</div></td></tr>
	</table></body>""") == OK and standalone.layout(Vector2i(160, 70)) == OK,
		"Measure a fixed table with later cell overflow outside HTMLLabel")
	_check(_rect(standalone, "overflowing").size.x < 130.0 and standalone.get_content_size().x >= 250,
		"Later cell overflow extends the scrollable document without widening tracks")
	viewport.queue_free()


func _check_form_css() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(340, 230)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(340.0, 230.0)
	label.text = """<style>
		body { margin: 0; background: white; --accent: #00ee44; accent-color: var(--accent); }
		@layer placeholders {
			#area::placeholder { color: var(--placeholder-tone); opacity: 100%; font-size: 18px }
		}
		@layer field {
			#text::placeholder { color: red !important; opacity: 100%; font-size: 20px }
			#text:focus::placeholder { color: #cc0000 !important }
			section::selection { color: white; background-color: #ff00ff }
			#area::placeholder { color: var(--placeholder-rollback, revert-layer) }
			#area { --placeholder-tone: #aa00aa; }
		}
		#text { width: 180px; height: 32px; appearance: none; border: 2px solid blue; caret-color: #ee8800 }
		#text::placeholder { color: green }
		#area { display: block; width: 140px; height: 70px; caret-color: transparent }
		#check, #radio { width: 24px; height: 24px }
		#plain { appearance: none; width: 24px; height: 24px; border: 2px solid black }
	</style>
	<section><input id="text" placeholder="Hello placeholder"><br>
	<input id="check" type="checkbox" checked><input id="radio" type="radio" checked>
	<input id="plain" type="checkbox" checked>
	<textarea id="area" placeholder="Textarea placeholder text wraps"></textarea></section>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var field: HTMLInputElement = label.get_element_by_id("text") as HTMLInputElement
	var plain: HTMLInputElement = label.get_element_by_id("plain") as HTMLInputElement
	var area: HTMLElement = label.get_element_by_id("area")
	var field_box: Rect2 = field.get_bounding_rect()
	var plain_box: Rect2 = plain.get_bounding_rect()
	_check(_color_bounds(image, Rect2i(area.get_bounding_rect()), Color("#aa00aa")).size.y > 18,
		"The multiline placeholder wraps with its pseudo-element font and color restored by a variable rollback")
	_check(_color_bounds(image, Rect2i(field_box), Color.RED).has_area(), "Layered placeholder color is painted")
	_check(_color_bounds(image, Rect2i(_label_rect(label, "check")), Color("#00ee44")).has_area() and
		_color_bounds(image, Rect2i(_label_rect(label, "radio")), Color("#00ee44")).has_area(),
		"An inherited variable colors checked checkbox and radio chrome")
	_check_pixel(image, plain_box.get_center(), Color.WHITE, "Appearance none leaves a checkbox without built-in decoration")
	_check_pixel(image, plain_box.position + Vector2(1.0, 10.0), Color.BLACK, "Appearance none preserves the author's border")
	_check(label.focus_element(plain), "An unstyled checkbox can still take DOM focus")
	_send_dom_key(viewport, KEY_SPACE)
	_send_dom_key(viewport, KEY_SPACE, 0, false, false)
	_check(not plain.checked, "Keyboard toggle works with appearance none")
	await process_frame
	await process_frame
	_check(label.focus_element(field), "An unstyled text field can still take DOM focus")
	await process_frame
	await RenderingServer.frame_post_draw
	_check(_color_bounds(viewport.get_texture().get_image(), Rect2i(field_box), Color("#cc0000")).has_area(),
		"Focus updates the styled placeholder")
	field.value = "A中 אב"
	field.set_selection_range(0, field.value.length())
	await process_frame
	await RenderingServer.frame_post_draw
	image = viewport.get_texture().get_image()
	_check(_color_bounds(image, Rect2i(field_box), Color.MAGENTA).has_area(), "The editable field inherits ancestor selection styling")
	_check(_color_bounds(image, Rect2i(field_box), Color("#ee8800")).has_area(), "The editable field paints its CSS caret color")
	_check(not _color_bounds(image, Rect2i(field_box), Color("#cc0000")).has_area(), "A value replaces the placeholder")
	var diagnostics: HTMLLayout = HTMLLayout.new()
	diagnostics.css_diagnostics_enabled = true
	_check(diagnostics.parse("""<style>input::placeholder { opacity: 50% }</style>
		<input id="diagnosed" placeholder="Valid" style="caret-color: bogus; appearance: auto">
		<div id="valid_opacity" style="opacity: 0.6"></div><table style="table-layout: bogus"></table>""") == OK and
		diagnostics.layout(Vector2i(250, 100)) == OK, "Lay out the CSS diagnostic fixture")
	_check(_has_diagnostic(diagnostics, "caret-color", "bogus", "inline_style") and
		_has_diagnostic(diagnostics, "table-layout", "bogus", "inline_style"), "Invalid new property values retain diagnostics")
	_check(diagnostics.get_element_by_id("diagnosed").get_css_diagnostics().size() == 1 and
		diagnostics.get_element_by_id("valid_opacity").get_css_diagnostics().is_empty(),
		"Both placeholder and element opacity are supported without rendering diagnostics")
	viewport.queue_free()


func _check_text_input_batch() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(330, 160)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(330.0, 160.0)
	label.text = """
		<style>:placeholder-shown { background: silver }</style>
		<input id="single" placeholder="Type here">
		<textarea id="multi" placeholder="Type here"></textarea>
	"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var single: HTMLInputElement = label.get_element_by_id("single") as HTMLInputElement
	var multi: HTMLTextAreaElement = label.get_element_by_id("multi") as HTMLTextAreaElement
	var committed: Array[String] = []
	label.input_committed.connect(func(element: HTMLInputElement) -> void: committed.append(element.value))

	_check(label.focus_element(single), "Focus the empty input before delivering a batched IME result")
	for index: int in range("hello world".length()):
		_send_dom_key(viewport, KEY_NONE, "hello world".unicode_at(index))
	_check(single.value == "hello world", "The input receives all characters while placeholder styles are pending")
	_send_dom_key(viewport, KEY_ENTER)
	_check(committed == ["hello world"], "A following Enter submits the complete input value")

	await process_frame
	await process_frame
	_check(label.focus_element(multi), "Focus the empty textarea before delivering a batched IME result")
	for index: int in range("こんにちは😀".length()):
		_send_dom_key(viewport, KEY_NONE, "こんにちは😀".unicode_at(index))
	_check(multi.value == "こんにちは😀", "The textarea receives a complete Unicode result in one input batch")
	_send_dom_key(viewport, KEY_ENTER)
	_check(multi.value == "こんにちは😀\n", "A following Enter still inserts a newline in the textarea")

	single.value = ""
	await process_frame
	await process_frame
	_check(label.focus_element(single), "Focus the input for a Unicode event carrying an Enter key code")
	_send_dom_key(viewport, KEY_ENTER, "a".unicode_at(0))
	_send_dom_key(viewport, KEY_KP_ENTER, "b".unicode_at(0))
	_check(single.value == "ab" and committed == ["hello world"],
		"Committed characters with Enter metadata stay in the field without submitting")
	viewport.queue_free()


func _send_text_history_key(viewport: SubViewport, key: Key, shifted: bool = false) -> void:
	var event: InputEventKey = InputEventKey.new()
	event.keycode = key
	event.physical_keycode = key
	event.ctrl_pressed = true
	event.shift_pressed = shifted
	event.pressed = true
	viewport.push_input(event)


func _check_text_undo() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(300, 160)
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(300.0, 160.0)
	label.text = "<input id='single' value='h suffix'><textarea id='multi'>pre OLD\nend</textarea>"
	viewport.add_child(label)
	await process_frame
	await process_frame
	var single: HTMLInputElement = label.get_element_by_id("single") as HTMLInputElement
	var multi: HTMLTextAreaElement = label.get_element_by_id("multi") as HTMLTextAreaElement
	var input_changes: Array[String] = []
	label.input_changed.connect(func(element: HTMLInputElement) -> void: input_changes.append(element.value))

	_check(label.focus_element(single), "Focus the input for history shortcuts")
	single.set_selection_range(0, 1)
	var result: String = "hello 世界😀"
	_send_dom_key(viewport, KEY_ENTER, result.unicode_at(0))
	for index: int in range(1, result.length()):
		_send_dom_key(viewport, KEY_NONE, result.unicode_at(index))
	_check(single.value == result + " suffix", "The IME result replaces a selection that matches its first character")

	await process_frame
	_send_dom_key(viewport, KEY_X, "x".unicode_at(0))
	_send_text_history_key(viewport, KEY_Z)
	_check(single.value == result + " suffix", "Typing after a result creates a separate history entry")
	var before_undo: int = input_changes.size()
	_send_text_history_key(viewport, KEY_Z)
	_check(single.value == "h suffix" and single.get_selection_start() == 0 and single.get_selection_end() == 1,
		"One undo restores the text and selection before the entire IME commit")
	_check(input_changes.size() == before_undo + 1, "Undoing a grouped result emits one input change")
	_send_text_history_key(viewport, KEY_Z, true)
	_check(single.value == result + " suffix" and single.get_selection_start() == result.length() and
		single.get_selection_end() == result.length(), "Shift+Ctrl+Z restores the complete result and insertion point")
	_send_text_history_key(viewport, KEY_Y)
	_check(single.value == result + "x suffix", "Ctrl+Y redoes the next history entry")
	_send_text_history_key(viewport, KEY_Z)
	_send_dom_key(viewport, KEY_Q, "q".unicode_at(0))
	_send_text_history_key(viewport, KEY_Y)
	_check(single.value == result + "q suffix", "A new edit discards the redo branch")

	_check(label.focus_element(multi), "Focus the textarea for multiline history shortcuts")
	multi.set_selection_range(4, 7)
	var composition: String = "日本語"
	for index: int in range(composition.length()):
		_send_dom_key(viewport, KEY_NONE, composition.unicode_at(index))
	_send_dom_key(viewport, KEY_ENTER)
	_check(multi.value == "pre 日本語\n\nend", "The textarea handles a result and a separate newline edit")
	_send_text_history_key(viewport, KEY_Z)
	_check(multi.value == "pre 日本語\nend", "Undo first removes the added newline")
	_send_text_history_key(viewport, KEY_Z)
	_check(multi.value == "pre OLD\nend" and multi.get_selection_start() == 4 and multi.get_selection_end() == 7,
		"One undo restores the original multiline selection")
	_send_text_history_key(viewport, KEY_Y)
	multi.set_attribute("readonly", "")
	_send_text_history_key(viewport, KEY_Z)
	_check(multi.value == "pre 日本語\nend", "Readonly fields ignore history shortcuts")
	multi.remove_attribute("readonly")
	multi.value = "scripted"
	_send_text_history_key(viewport, KEY_Z)
	_check(multi.value == "scripted", "A value assigned from script starts a fresh history")
	viewport.queue_free()


func _right_click_field(viewport: SubViewport, field: HTMLElement) -> void:
	var event: InputEventMouseButton = InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_RIGHT
	event.position = field.get_bounding_rect().get_center()
	event.pressed = true
	viewport.push_input(event)
	event.pressed = false
	viewport.push_input(event)


func _check_input_context_menu() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(400, 310)
	viewport.gui_embed_subwindows = true
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(400.0, 310.0)
	label.text = """
		<input id="single" value="alpha"><br>
		<textarea id="multi" readonly>second field</textarea><br>
		<input id="disabled" value="disabled" disabled>
	"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var single: HTMLInputElement = label.get_element_by_id("single") as HTMLInputElement
	var multi: HTMLTextAreaElement = label.get_element_by_id("multi") as HTMLTextAreaElement
	var disabled: HTMLElement = label.get_element_by_id("disabled")
	_check(label.context_menu_enabled, "The input context menu is enabled by default")
	single.set_selection_range(1, 3)

	_right_click_field(viewport, single)
	var menu: PopupMenu = label.get_node_or_null("_HTMLScroll/_HTMLContent/_HTMLCanvas/_HTMLInputMenu") as PopupMenu
	_check(menu != null and menu.visible and label.get_focused_element() == single,
		"Right-click opens the popup and focuses the input")
	if menu == null or not menu.visible:
		viewport.queue_free()
		return
	_check(menu.item_count == 8 and not menu.is_item_disabled(4) and menu.is_item_disabled(0) and menu.is_item_disabled(1),
		"The menu offers selection commands and disables empty history")
	_check(menu.get_item_accelerator(4) != KEY_NONE, "Menu accelerators use the Godot Input Map")
	_check(single.get_selection_start() == 1 and single.get_selection_end() == 3, "Right-click preserves the field selection")
	menu.emit_signal("id_pressed", menu.get_item_id(7))
	_check(not menu.visible and single.get_selection_start() == 0 and single.get_selection_end() == 5,
		"Select All acts on the field and closes the menu")

	_send_dom_key(viewport, KEY_X, "x".unicode_at(0))
	await process_frame
	_right_click_field(viewport, single)
	_check(menu.visible and not menu.is_item_disabled(0), "An edited input offers Undo in its menu")
	menu.emit_signal("id_pressed", menu.get_item_id(0))
	_check(single.value == "alpha" and single.get_selection_end() == 5, "Menu Undo restores text and selection")
	_right_click_field(viewport, single)
	_check(menu.visible and not menu.is_item_disabled(1), "Undo makes Redo available in the menu")
	menu.emit_signal("id_pressed", menu.get_item_id(1))
	_check(single.value == "x", "Menu Redo reapplies the edit")

	multi.set_selection_range(1, 4)
	_right_click_field(viewport, multi)
	_check(menu.visible and label.get_focused_element() == multi and menu.is_item_disabled(3) and
		menu.is_item_disabled(5) and not menu.is_item_disabled(4) and not menu.is_item_disabled(7),
		"Readonly textareas allow Copy and Select All and disable editing")
	label.context_menu_enabled = false
	_check(not menu.visible, "Disabling the context menu closes the popup")
	_right_click_field(viewport, multi)
	_check(not menu.visible, "Right-click cannot open a disabled context menu")
	label.context_menu_enabled = true
	_right_click_field(viewport, disabled)
	_check(not menu.visible, "Disabled fields do not open a context menu")
	await process_frame
	var menu_key: InputEventKey = InputMap.action_get_events(&"ui_menu")[0].duplicate() as InputEventKey
	menu_key.pressed = true
	viewport.push_input(menu_key)
	_check(menu.visible and label.get_focused_element() == multi, "The ui_menu input action opens the focused textarea menu")
	multi.remove()
	_check(not menu.visible, "Removing the menu target closes the popup")
	viewport.queue_free()


func _check_caret_blink() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(240, 150)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(240.0, 150.0)
	label.text = """
		<style>
			input, textarea { display: block; caret-color: magenta; }
		</style>
		<input id="single" value="text">
		<textarea id="multi">text</textarea>
	"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	var single: HTMLElement = label.get_element_by_id("single")
	var multi: HTMLElement = label.get_element_by_id("multi")
	var caret_is_visible: Callable = func(element: HTMLElement) -> bool:
		var image: Image = viewport.get_texture().get_image()
		return _color_bounds(image, Rect2i(element.get_bounding_rect()), Color.MAGENTA).has_area()
	var layout_updates: Array[bool] = []
	label.layout_changed.connect(func() -> void: layout_updates.append(true))

	var caret_group_found: bool
	for property: Dictionary in label.get_property_list():
		if property.name == &"Caret":
			caret_group_found = (int(property.usage) & PROPERTY_USAGE_GROUP) != 0
	_check(caret_group_found and label.caret_blink and is_equal_approx(label.caret_blink_interval, 0.65),
		"The Caret inspector group exposes the default blink settings")
	_check(label.focus_element(single), "Focus the input to start caret blinking")
	await RenderingServer.frame_post_draw
	_check(caret_is_visible.call(single), "The caret starts visible")
	await create_timer(0.72).timeout
	await RenderingServer.frame_post_draw
	_check(not caret_is_visible.call(single), "The caret is hidden after the first blink interval")
	await create_timer(0.65).timeout
	await RenderingServer.frame_post_draw
	_check(caret_is_visible.call(single), "The caret appears again after the second interval")
	var layout_count: int = layout_updates.size()
	await create_timer(0.65).timeout
	await RenderingServer.frame_post_draw
	_check(not caret_is_visible.call(single) and layout_updates.size() == layout_count,
		"Blinking updates the caret without reflow")
	_send_dom_key(viewport, KEY_END)
	await RenderingServer.frame_post_draw
	_check(caret_is_visible.call(single), "Keyboard movement reveals a hidden caret immediately")

	_check(label.focus_element(multi), "Move focus to the textarea")
	await process_frame
	await RenderingServer.frame_post_draw
	_check(not caret_is_visible.call(single) and caret_is_visible.call(multi),
		"Focus change hides the old caret and reveals the new one")
	await create_timer(0.72).timeout
	await RenderingServer.frame_post_draw
	_check(not caret_is_visible.call(multi), "The textarea uses the same blink interval")

	label.caret_blink = false
	await RenderingServer.frame_post_draw
	_check(caret_is_visible.call(multi), "Disabling blink reveals the focused caret immediately")
	var interval_usage: int
	for property: Dictionary in label.get_property_list():
		if property.name == &"caret_blink_interval":
			interval_usage = int(property.usage)
	_check((interval_usage & PROPERTY_USAGE_EDITOR) == 0 and (interval_usage & PROPERTY_USAGE_STORAGE) != 0,
		"Disabling blink hides the Inspector interval and keeps it serializable")
	await create_timer(0.72).timeout
	await RenderingServer.frame_post_draw
	_check(caret_is_visible.call(multi), "The focused caret stays visible with blinking disabled")

	label.caret_blink_interval = 0.24
	label.caret_blink = true
	await create_timer(0.32).timeout
	await RenderingServer.frame_post_draw
	_check(not caret_is_visible.call(multi), "Reenabling blink uses the interval set while disabled")
	label.caret_blink_interval = 0.52
	await RenderingServer.frame_post_draw
	_check(caret_is_visible.call(multi), "Changing the active interval restarts at the visible phase")
	await create_timer(0.33).timeout
	await RenderingServer.frame_post_draw
	_check(caret_is_visible.call(multi), "The new longer interval replaces the previous timer")
	await create_timer(0.25).timeout
	await RenderingServer.frame_post_draw
	_check(not caret_is_visible.call(multi), "The caret switches off at the updated interval")
	viewport.queue_free()


func _check_textarea() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(340, 210)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(340, 210)
	label.text = """<style>body { margin: 0; }
		textarea { width: 112px; height: 64px; line-height: 20px; box-sizing: border-box; vertical-align: top; }
		#editor { max-width: 160px; resize: horizontal; }
		#after { display: block; }
		</style><textarea id="editor" style="color: red; height: 64px">
First &amp; line
第二行</textarea><input id="after" value="Next">"""
	viewport.add_child(label)
	var changes: Array[HTMLTextAreaElement] = []
	var commits: Array[HTMLTextAreaElement] = []
	label.textarea_changed.connect(func(element: HTMLTextAreaElement) -> void: changes.append(element))
	label.textarea_committed.connect(func(element: HTMLTextAreaElement) -> void: commits.append(element))
	await process_frame
	await process_frame

	var field: HTMLTextAreaElement = label.get_element_by_id("editor") as HTMLTextAreaElement
	_check(field != null, "A textarea query returns the specialized element")
	if field == null:
		viewport.queue_free()
		return
	_check(field.default_value == "First & line\n第二行" and field.value == field.default_value,
		"Textarea initial value decodes entities and keeps newlines after the opening newline")
	label.select_all()
	_check("First" not in label.get_selected_text(), "Textarea text belongs to the field's own selection")
	var detached: HTMLTextAreaElement = label.create_element("textarea") as HTMLTextAreaElement
	detached.default_value = " a\n  b\n"
	_check(detached.value == " a\n  b\n", "Detached textareas retain whitespace in their default value")
	_check(detached.set_inner_html(" a  &amp;\n  <b>text</b>") == OK and detached.value == " a  &\n  <b>text</b>",
		"Textarea fragment parsing keeps RCDATA and literal markup")
	detached.value = "live\r\nvalue"
	detached.default_value = "new default"
	_check(detached.value == "live\nvalue" and detached.default_value == "new default",
		"A dirty value is independent of its child text and normalizes newlines")

	field.value = "a\nb\nc"
	_check(label.focus_element(field), "Textarea participates in DOM focus")
	field.set_selection_range(1, 1)
	_send_dom_key(viewport, KEY_DOWN, 0, true)
	_check(field.get_selection_start() == 1 and field.get_selection_end() == 3,
		"Shift+Down extends selection across an explicit newline")
	_send_dom_key(viewport, KEY_ENTER)
	_check(field.value == "a\n\nc" and changes == [field] and commits.is_empty(),
		"Enter replaces the selection with a newline and keeps the textarea active")
	field.set_selection_range(0, 3)
	await process_frame
	await process_frame

	var box: Rect2 = field.get_bounding_rect()
	var handle: Vector2 = box.end - Vector2(5, 5)
	_drag_pointer(viewport, handle, handle + Vector2(95, 25))
	await process_frame
	await process_frame
	_check(is_equal_approx(field.get_bounding_rect().size.x, 160.0) and is_equal_approx(field.get_bounding_rect().size.y, box.size.y),
		"Horizontal resize respects max-width and retains the original height")
	_check("color: red" in field.get_attribute("style") and "height: 64px" in field.get_attribute("style"),
		"Resizing retains the other inline declarations")
	_check(field.value == "a\n\nc" and label.get_focused_element() == field and
		field.get_selection_start() == 0 and field.get_selection_end() == 3,
		"Resizing preserves live text, selection and DOM focus")
	_send_dom_key(viewport, KEY_TAB)
	await process_frame
	_check(label.get_focused_element() == label.get_element_by_id("after") and commits == [field],
		"Tab leaves the textarea and commits the field once")

	field.value = "Alpha Beta Gamma Delta Epsilon Zeta Eta Theta"
	label.focus_element(field)
	field.set_selection_range(0, 0)
	await process_frame
	_send_dom_key(viewport, KEY_DOWN, 0, true)
	_check(field.get_selection_end() > 0 and field.get_selection_end() < field.value.length(),
		"A soft wrap provides a caret stop on the next visual line")
	field.set_attribute("wrap", "off")
	field.set_selection_range(0, 0)
	await process_frame
	await process_frame
	_send_dom_key(viewport, KEY_DOWN, 0, true)
	_check(field.get_selection_end() == 0, "wrap=off keeps a long paragraph on a single horizontal line")
	field.remove_attribute("wrap")
	field.value = "0\n1\n2\n3\n4\n5\n6"
	field.set_selection_range(0, 0)
	await process_frame
	await process_frame
	var text_point: Vector2 = field.get_bounding_rect().position + Vector2(5, 10)
	_drag_pointer(viewport, text_point, text_point)
	_check(field.get_selection_start() <= 1, "The top of an unscrolled textarea hits the first line")
	var wheel: InputEventMouseButton = InputEventMouseButton.new()
	wheel.button_index = MOUSE_BUTTON_WHEEL_DOWN
	wheel.position = text_point
	wheel.factor = 1.0
	wheel.pressed = true
	viewport.push_input(wheel)
	_drag_pointer(viewport, text_point, text_point)
	_check(field.get_selection_start() >= 2 and label.get_scroll_offset() == Vector2i.ZERO,
		"Wheel scrolling moves the text hit position inside the textarea")

	field.set_attribute("style", "resize: vertical; width: 112px; height: 64px; transform: rotate(5deg); border-radius: 4px")
	await process_frame
	await process_frame
	var angle: float = deg_to_rad(5.0)
	box = field.get_bounding_rect()
	handle = field.get_bounding_rect().get_center() + Vector2(51, 27).rotated(angle)
	_drag_pointer(viewport, handle, handle + Vector2(18, 20).rotated(angle))
	await process_frame
	await process_frame
	var expected_height_gain: float = 20.0 * cos(angle)
	_check("width: 112px" in field.get_attribute("style") and
		absf(field.get_bounding_rect().size.y - box.size.y - expected_height_gain) < 1.1,
		"The rotated resize handle applies the pointer delta along the allowed local axis: %s" % field.get_attribute("style"))

	field.set_attribute("style", "width: 112px; height: 64px; resize: both")
	await process_frame
	await process_frame
	box = field.get_bounding_rect()
	handle = box.end - Vector2(5.0, 5.0)
	var click: InputEventMouseButton = InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.position = handle
	click.pressed = true
	viewport.push_input(click)
	var previous: Vector2 = handle
	var offsets: Array[Vector2] = [
		Vector2(24.0, 12.0),
		Vector2(36.0, 28.0),
		Vector2(-16.0, -20.0),
	]
	for delta: Vector2 in offsets:
		var motion: InputEventMouseMotion = InputEventMouseMotion.new()
		motion.position = handle + delta
		motion.relative = motion.position - previous
		motion.button_mask = MOUSE_BUTTON_MASK_LEFT
		viewport.push_input(motion)
		previous = motion.position
		await process_frame
		await process_frame
		_check(field.get_bounding_rect().size.is_equal_approx(box.size + delta),
			"A held textarea resize handle follows each motion after reflow: %s" % delta)
	click.position = previous
	click.pressed = false
	viewport.push_input(click)
	viewport.queue_free()


func _check_text_emphasis() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(490, 320)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(490, 320)
	label.css_diagnostics_enabled = true
	label.text = """<style>
	body { margin: 0; font-family: 'Consolas', monospace; font-size: 30px; background: black; color: white; }
	.row { width: 400px; height: 54px; }
	#over { --mark: "\\25A0"; text-emphasis: var(--mark) #ff0000; text-emphasis-position: over; }
	#under { text-emphasis: "#ignored" #00ff00; text-emphasis-position: left under; }
	#under > span { text-emphasis-color: #00ffff; text-shadow: 12px 0 #8800ff; }
	#none { text-emphasis: red; }
	#reset > span { color: #ffff00; }
	#reset { text-emphasis: filled dot magenta; }
	#reset > span { text-emphasis: filled dot; }
	#wrap { width: 95px; line-height: 18px; text-emphasis: open circle #f8a600; }
	#wrap > span { text-emphasis-position: under; }
	#clip { position: absolute; left: 415px; top: 12px; width: 51px; height: 27px; border-radius: 9px;
		overflow: hidden; text-emphasis: "X" #0066ff; }
	</style>
	<div id="over" class="row">Á A, #</div>
	<div id="under" class="row"><span>A</span></div>
	<div id="none" class="row">ABC</div>
	<div id="reset" class="row"><span>A</span></div>
	<div id="wrap">A A <span>A A</span> A A</div>
	<div id="clip">AAA</div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var over: Rect2i = Rect2i(_label_rect(label, "over"))
	var under: Rect2i = Rect2i(_label_rect(label, "under"))
	var reset: Rect2i = Rect2i(_label_rect(label, "reset"))
	var none: Rect2i = Rect2i(_label_rect(label, "none"))
	var clip: Rect2i = Rect2i(_label_rect(label, "clip"))
	var marks: PackedInt32Array = _colored_column_centers(image, over, Color.RED)
	_check(marks.size() == 3, "Emphasis paints three marks for accents, spaces and punctuation, got %s" % [marks])
	if marks.size() == 3:
		_check(marks[1] - marks[0] > 25 and marks[2] - marks[1] > 38, "Punctuation and spaces do not get a mark")
	var upper_mark: Rect2i = _color_bounds(image, over, Color.RED)
	var upper_base: Rect2i = _color_bounds(image, over, Color.WHITE)
	_check(upper_mark.has_area() and upper_base.has_area() and upper_mark.end.y <= upper_base.position.y, "Over marks paint above text")
	var lower_mark: Rect2i = _color_bounds(image, under, Color.CYAN)
	var lower_base: Rect2i = _color_bounds(image, under, Color.WHITE)
	_check(lower_mark.has_area() and lower_base.has_area() and lower_mark.position.y >= lower_base.end.y,
		"Under marks paint below text and inherit the emphasis string")
	_check(_color_bounds(image, Rect2i(under.position.x + 12, lower_mark.position.y, 12, lower_mark.size.y), Color("#8800ff")).has_area(),
		"Text shadow includes the emphasis glyph")
	_check(not _color_bounds(image, reset, Color.MAGENTA).has_area() and _color_bounds(image, reset, Color.YELLOW).has_area(),
		"Shorthand resets omitted emphasis color to currentColor")
	_check(_color_bounds(image, none, Color.RED).size == Vector2i.ZERO, "Shorthand resets omitted emphasis style")
	_check(_label_rect(label, "wrap").size.y > 60.0, "Wrapped lines reserve space for marks on both sides")
	_check(_color_bounds(image, clip, Color("#0066ff")).has_area() and not _color_bounds(image,
		Rect2i(clip.end.x, clip.position.y, 24, clip.size.y), Color("#0066ff")).has_area(), "Emphasis obeys overflow clipping")
	_check(label.get_css_diagnostics().is_empty(), "Supported emphasis leaves no diagnostics")

	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse('<p style="text-emphasis: &quot;*&quot; red">Alpha Beta</p>') == OK and layout.layout(Vector2i(200, 100)) == OK,
		"Lay out selectable text with emphasis")
	_check(layout.get_plain_text().strip_edges() == "Alpha Beta", "Emphasis marks do not enter copied text")
	_check(not layout.get_selection_rects(0, 5).is_empty(), "Base text keeps selection geometry")
	viewport.queue_free()


func _colored_column_centers(image: Image, region: Rect2i, expected: Color) -> PackedInt32Array:
	var positions: PackedInt32Array = PackedInt32Array()
	var start: int = -1
	for x: int in range(region.position.x, region.end.x + 1):
		var found: bool
		if x < region.end.x:
			for y: int in range(region.position.y, region.end.y):
				if _same_color(image.get_pixel(x, y), expected):
					found = true
					break
		if found and start < 0:
			start = x
		elif not found and start >= 0:
			positions.append(roundi(float(start + x - 1) / 2.0))
			start = -1
	return positions


func _check_underline_position() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(460, 330)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(460, 330)
	label.css_diagnostics_enabled = true
	label.text = """<!doctype html><style>
	  body { margin: 0; font: 24px Consolas, monospace; background: black; color: white; }
	  .line { position: absolute; width: 65px; height: 57px; }
	  .line, .decoration { text-decoration: underline #ff2200 2px; }
	  #zero, #under_zero { text-underline-offset: 0; }
	  #positive { --offset: calc(4px + 0.25em); text-underline-offset: var(--offset); }
	  #negative { text-underline-offset: -4px; }
	  #auto, #font, #font_shift, #left, #invalid { text-underline-offset: auto; }
	  #font, #font_shift { text-underline-position: from-font; }
	  #font_shift { text-underline-offset: 0.25em; }
	  #left { text-underline-position: left; }
	  #under_zero, #under_shift { text-underline-position: under right; }
	  #under_shift { text-underline-offset: 25%; text-shadow: 0 5px cyan; }
	  #invalid { text-underline-position: left right; text-underline-offset: 2; }
	  #outer { text-underline-offset: 9px; }
	  #outer span { text-underline-offset: 0; }
	  #fraction { text-underline-offset: 25%; }
	  #length { text-underline-offset: 0.25em; }
	  .decorator { position: absolute; width: 65px; height: 62px; }
	  .decoration { font-size: 32px; }
	  #small { text-underline-offset: 0; }
	  #plain_over, #offset_over { text-decoration-line: overline; }
	  #offset_over { text-underline-offset: 12px; }
	</style>
	<div class="line" id="zero" style="left: 0; top: 0">gyp</div>
	<div class="line" id="positive" style="left: 75px; top: 0">gyp</div>
	<div class="line" id="negative" style="left: 150px; top: 0">gyp</div>
	<div class="line" id="auto" style="left: 225px; top: 0">gyp</div>
	<div class="line" id="font" style="left: 300px; top: 0">gyp</div>
	<div class="line" id="font_shift" style="left: 375px; top: 0">gyp</div>
	<div class="line" id="under_zero" style="left: 0; top: 60px">gyp</div>
	<div class="line" id="under_shift" style="left: 75px; top: 60px">gyp</div>
	<div class="line" id="left" style="left: 150px; top: 60px">gyp</div>
	<div class="line" id="invalid" style="left: 225px; top: 60px">gyp</div>
	<div class="line" id="outer" style="left: 300px; top: 60px">g<span>y</span>p</div>
	<div class="decorator" id="length" style="left: 0; top: 135px"><span class="decoration">gyp</span></div>
	<div class="decorator" id="fraction" style="left: 75px; top: 135px"><span class="decoration">gyp</span></div>
	<div class="decorator" id="small" style="left: 150px; top: 135px"><span class="decoration">gyp</span></div>
	<div class="line" id="plain_over" style="left: 0; top: 220px">gyp</div>
	<div class="line" id="offset_over" style="left: 75px; top: 220px">gyp</div>"""
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var relative_y: Dictionary[String, int] = {}
	var rules: Dictionary[String, Rect2i] = {}
	for id: String in ["zero", "positive", "negative", "auto", "font", "font_shift", "under_zero", "under_shift",
		"left", "invalid", "outer", "length", "fraction", "small", "plain_over", "offset_over"]:
		var region: Rect2i = Rect2i(_label_rect(label, id))
		var rule: Rect2i = _color_bounds(image, region, Color("#ff2200"))
		_check(rule.has_area(), "Underline fixture paints %s" % id)
		relative_y[id] = rule.position.y - region.position.y
		rules[id] = rule
	_check(relative_y["positive"] - relative_y["zero"] == 10, "Computed offset is measured from the baseline")
	_check(relative_y["negative"] - relative_y["zero"] == -4, "Negative offset moves inward")
	_check(relative_y["font_shift"] - relative_y["font"] == 6, "from-font adds the offset to the font metric")
	_check(relative_y["under_zero"] > relative_y["zero"] and relative_y["under_shift"] - relative_y["under_zero"] == 6,
		"Under positions the line below the text and uses the author offset")
	_check(relative_y["auto"] == relative_y["left"] and relative_y["auto"] == relative_y["invalid"],
		"Left uses auto in horizontal writing and invalid declarations are rejected")
	_check(relative_y["length"] - relative_y["small"] == 6 and relative_y["fraction"] - relative_y["small"] == 8,
		"Inherited lengths stay absolute and percentages resolve on the decorating element")
	_check(relative_y["outer"] - relative_y["zero"] == 9 and rules["outer"].size.y == 2,
		"The author offset stays with the originating underline across inline children")
	var shifted_rule: Rect2i = rules["under_shift"]
	_check_pixel(image, Vector2(shifted_rule.get_center() + Vector2i(0, 5)), Color.CYAN, "Shadow follows the offset underline")
	_check(relative_y["plain_over"] == relative_y["offset_over"], "Underline offset has no effect on overline")
	_check(label.get_css_diagnostics().size() == 2, "Two invalid underline values produce diagnostics")
	viewport.queue_free()


func _check_color_spaces() -> void:
	# CSS Color 4 supplies reference values for the non-neutral XYZ, Lab, P3 and Rec. 2020 samples
	var swatches: Dictionary[String, String] = {
		"hwb(120 30% 50%)": "#4d804d",
		"hwb(45 40% 80%)": "#555555",
		"hwb(0.5turn 30 50)": "#4d8080",
		"lab(57.9% 11.4 -53.7)": "#6c88ea",
		"lch(57.9% 36.5978% 281.9854deg)": "#6c88ea",
		"oklab(65% 0 -37.5%)": "#6c88ea",
		"oklch(65% 0.15 270)": "#6c88ea",
		"color(display-p3 0.445 0.529 0.891)": "#6c88ea",
		"color(display-p3-linear 0.25 0.25 0.25)": "#898989",
		"color(rec2020 0.522 0.557 0.892)": "#6c88ea",
		"color(a98-rgb 50% 0.5 0.5)": "#818181",
		"color(prophoto-rgb 0.5 0.5 0.5)": "#929292",
		"color(xyz-d50 0.2004941 0.140872 0.4470838)": "#7654cd",
		"color(xyz 0.216595 0.1459994 0.594365)": "#7654cd",
		"color(srgb-linear 25% 0.25 0.25)": "#898989",
		"color(srgb none 60% 0.25 / 50%)": "#004c20",
		"color(srgb-linear none 0.25 0.25)": "#008989",
		"color-mix(in display-p3-linear, black, white)": "#bcbcbc",
		"color-mix(in lab, black, white)": "#777777",
		"color-mix(in hsl longer hue, red, blue)": "#00ff00",
		"color-mix(in hwb, white, lime)": "#80ff80",
		"color-mix(in srgb, color(srgb 1.5 0 0), black)": "#bf0000",
		"color-mix(in srgb, red, lime, blue)": "#555555",
		"color(srgb sqrt(0.25) calc(2 * 25%) abs(-50%))": "#808080",
		"rgb(calc(2 * 64) 50% 128)": "#808080",
		"rgba(calc(100%), min(0%, 10%), 0%, calc(1 / 2))": "#800000",
		"hsl(calc(1turn / 3), calc(2 * 50%), min(60%, 50%))": "#00ff00",
		"hwb(atan2(1, 0) min(20%, 40%) calc(5 * 2%) / calc(1 / 2))": "#467319",
		"color-mix(in srgb, red calc(25% + 25%), lime, blue)": "#804040",
		"color-mix(in srgb, red calc(-5%), lime calc(120%))": "#00ff00",
		"oklch(65% 0.15 calc(0.75turn))": "#6c88ea",
		"color-mix(in srgb, red 50%, lime, blue)": "#804040",
		"color-mix(in srgb, 100% red, lime 100%, blue)": "#808000",
		"color-mix(in srgb, red 20%, lime 20%, blue 20%)": "#333333",
		"color-mix(in srgb, red 0%, lime 0%, blue 0%)": "#000000",
		"color-mix(in srgb-linear, white 50%)": "#808080",
		"color-mix(in srgb, transparent, lime, currentColor)": "#00aa00",
		"color-mix(in hsl increasing hue, red, lime, blue)": "#00ff00",
		"color-mix(in srgb, color(srgb none 0.4 0 / none), color(srgb 0.8 0 0 / 0.5))": "#661a00",
		"color-mix(in srgb, color-mix(in display-p3-linear, color(display-p3-linear 2 0 0), black), currentColor)": "#8c6500",
	}
	var html: String = """<style>body { margin: 0; background: black; color: lime; }
		.swatch { position: absolute; width: 12px; height: 12px; }
		@supports (color: color(xyz-d65 0 0 0)) { #positive { background: lime; } }
		@supports (color: color(oklab 0.8 0 0)) { #invalid { background: red; } }
		</style>"""
	var points: Dictionary[String, Vector2] = {}
	var index: int
	for expression: String in swatches:
		var position: Vector2i = Vector2i((index % 8) * 15, floori(float(index) / 8.0) * 15)
		html += '<div class="swatch" style="left: %dpx; top: %dpx; --swatch: %s; background: var(--swatch)"></div>' % [
			position.x, position.y, expression,
		]
		points[expression] = Vector2(position + Vector2i(5, 5))
		index += 1
	html += """<div class="swatch" id="positive" style="left: 0; top: 75px"></div>
		<div class="swatch" id="invalid" style="left: 15px; top: 75px; background: lime; background: hwb(0, 0%, 0%)"></div>"""
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(140, 90)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(140, 90)
	label.text = html
	viewport.add_child(label)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	for expression: String in swatches:
		_check_pixel(image, points[expression], Color(swatches[expression]), expression)
	_check_pixel(image, Vector2(5.0, 80.0), Color.LIME, "Feature query accepts a predefined color space")
	_check_pixel(image, Vector2(20.0, 80.0), Color.LIME, "Invalid color declaration preserves the earlier valid value")
	viewport.queue_free()


func _rect(layout: HTMLLayout, id: String) -> Rect2:
	var element: HTMLElement = layout.get_element_by_id(id)
	return element.get_bounding_rect() if element != null else Rect2()


func _label_rect(label: HTMLLabel, id: String) -> Rect2:
	var element: HTMLElement = label.get_element_by_id(id)
	return element.get_bounding_rect() if element != null else Rect2()


func _has_diagnostic(layout: HTMLLayout, property: String, value: String, source: String, path: String = "") -> bool:
	for entry: Dictionary in layout.get_css_diagnostics():
		if entry["kind"] == "unsupported_value" and entry["property"] == property and entry["value"] == value:
			if entry["source"] == source and entry["path"] == path:
				return true
	return false


func _check_pixel(image: Image, position: Vector2, expected: Color, description: String) -> void:
	var actual: Color = image.get_pixelv(Vector2i(position))
	_check(_same_color(actual, expected), "%s at %s is %s, expected %s" % [description, position, actual, expected])


func _color_bounds(image: Image, region: Rect2i, expected: Color) -> Rect2i:
	var bounds: Rect2i
	for y: int in range(region.position.y, region.end.y):
		for x: int in range(region.position.x, region.end.x):
			if _same_color(image.get_pixel(x, y), expected):
				var pixel: Rect2i = Rect2i(x, y, 1, 1)
				bounds = bounds.merge(pixel) if bounds.has_area() else pixel
	return bounds


func _same_color(first: Color, second: Color) -> bool:
	const TOLERANCE: float = 2.1 / 255.0
	return absf(first.r - second.r) <= TOLERANCE and absf(first.g - second.g) <= TOLERANCE and absf(first.b - second.b) <= TOLERANCE


func _check(condition: bool, description: String) -> void:
	if not condition:
		_failures += 1
		push_error(description)
