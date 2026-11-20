extends Node

var failures: int
@onready var frame: SubViewport = $Frame
@onready var label: HTMLLabel = $Frame/HTMLLabel


func _ready() -> void:
	await _settle()
	var from_pck: bool = "--pck" in OS.get_cmdline_user_args()
	if from_pck:
		_check(not FileAccess.file_exists("res://pages/index.html"), "export loads imported HTML from its resource remap")
		_check(not FileAccess.file_exists("res://assets/v1/styles/outer.css"), "raw CSS is excluded from the selected-scene export")
		_check(not ResourceLoader.exists("res://assets/v2/icon.svg"), "unused assets are outside the scene's export dependencies")
	var layout: HTMLLayout = label.document.create_layout()
	_check(layout.get_document_element() is HTMLHtmlElement and layout.get_head() is HTMLHeadElement
		and layout.get_body() is HTMLBodyElement, "document accessors return the three structural types")
	var title: HTMLTitleElement = layout.query_selector("title") as HTMLTitleElement
	_check(title.text == "  First  title  " and layout.get_title() == "First title", "title text preserves original whitespace")
	title.text = "  New\t title "
	_check(layout.get_title() == "New title", "changing typed title updates the document title")
	var meta: HTMLMetaElement = label.get_element_by_id("meta") as HTMLMetaElement
	_check(meta.name == "viewport" and meta.content == "test", "meta reflects name and content")
	meta.http_equiv = "refresh"
	meta.charset = "utf-8"
	meta.media = "screen"
	_check(meta.get_attribute("http-equiv") == "refresh" and meta.get_attribute("charset") == "utf-8"
		and meta.get_attribute("media") == "screen", "metadata setters reflect through DOM attributes")
	var script: HTMLScriptElement = label.get_element_by_id("config") as HTMLScriptElement
	_check(script.type == "application/json" and JSON.parse_string(script.text) == {"enabled": true},
		"script keeps the inline data available to GDScript")
	script.text = "{\"tag\":\"<p>\"}"
	script.src = "data.js"
	script.async = true
	script.defer = true
	script.no_module = true
	_check(script.text.contains("<p>") and script.get_attribute("src") == "data.js" and script.has_attribute("async")
		and script.has_attribute("defer") and script.has_attribute("nomodule"), "script setters preserve raw text and reflect attributes")
	await _settle()

	var first: HTMLBaseElement = label.get_element_by_id("first") as HTMLBaseElement
	var second: HTMLBaseElement = label.get_element_by_id("next") as HTMLBaseElement
	var icon: HTMLImageElement = label.get_element_by_id("icon") as HTMLImageElement
	var linked: HTMLElement = label.get_element_by_id("linked")
	var jump: HTMLAnchorElement = label.get_element_by_id("jump") as HTMLAnchorElement
	var tone: HTMLAudioElement = label.get_element_by_id("tone") as HTMLAudioElement
	_check(first.href == "../assets/v1/virtual.html" and first.get_resolved_href() == "res://assets/v1/virtual.html"
		and second.get_resolved_href() == "res://assets/v2/" and label.get_base_uri() == "res://assets/v1/virtual.html"
		and layout.get_document_node().get_base_uri() == label.get_base_uri(),
		"the first connected base with an href wins, even after a target-only base and template")
	_check(icon.current_src == "res://assets/v1/icon.svg" and icon.get_intrinsic_size().x == 14.0, "relative srcset resolves through the base")
	_check(tone.play() == OK and tone.current_src == "res://assets/v1/tone.wav" and is_equal_approx(tone.duration, 0.5),
		"relative audio plays through the base")
	tone.pause()
	_check(is_equal_approx(linked.get_bounding_rect().size.x, 40.0), "the external stylesheet loaded through the base")
	_check_pixel("icon", Color.RED)
	_check_pixel("inline", Color.GREEN)
	_check_pixel("linked", Color.BLUE)
	_check_pixel("nested", Color.MAGENTA)
	_check_pixel("inline-import", Color.CYAN)
	await _check_link_stylesheet(from_pck)
	_check_inline_stylesheet()
	_check_shared_stylesheets()
	_check(jump.href == "#tail" and jump.get_resolved_href() == "res://assets/v1/virtual.html#tail", "script can read the resolved link")
	var clicked_uris: Array[String]
	label.link_pressed.connect(func(uri: String) -> void: clicked_uris.append(uri), CONNECT_ONE_SHOT)
	_check(label.activate_element(jump) and clicked_uris == ["#tail"] and label.get_scroll_offset().y > 0,
		"existing local anchors and link_pressed retain the original href")
	if not from_pck:
		await _check_base_edits(layout, first, second, icon, linked, tone)
	else:
		first.href = ""
		_check(label.get_base_uri() == "res://pages/index.html", "export retains the original source URL for an empty base")
	if failures == 0:
		print("HTML base export checks passed" if from_pck else "HTML base checks passed")
	get_tree().quit(0 if failures == 0 else 1)


func _check_base_edits(
	layout: HTMLLayout, first: HTMLBaseElement, second: HTMLBaseElement, icon: HTMLImageElement,
	linked: HTMLElement, tone: HTMLAudioElement,
) -> void:
	var detached: HTMLBaseElement = layout.create_element("base") as HTMLBaseElement
	detached.href = "res://unused/"
	_check(layout.get_base_uri() == "res://assets/v1/virtual.html", "a detached base does not affect the document")
	var link: HTMLLinkElement = label.query_selector("link[href='styles/outer.css']") as HTMLLinkElement
	var original_sheet: HTMLStyleSheet = link.sheet
	first.href = "../assets/v2/"
	_check(not original_sheet.is_valid() and link.sheet.href == "res://assets/v2/styles/outer.css",
		"changing the effective linked path through the base invalidates the old sheet")
	_check(label.get_base_uri() == "res://assets/v2/", "base mutations update URL queries immediately")
	await _settle()
	_check(icon.current_src == "res://assets/v2/icon.svg" and icon.get_intrinsic_size().x == 36.0
		and is_equal_approx(linked.get_bounding_rect().size.x, 70.0), "base mutation reselects images and loads the new stylesheet")
	_check(tone.play() == OK and tone.current_src == "res://assets/v2/tone.wav" and is_equal_approx(tone.duration, 1.0),
		"audio resolves again when the document base changes")
	tone.pause()
	first.href = "https://example.invalid/"
	await _settle()
	_check(label.get_base_uri() == "res://pages/index.html" and icon.get_intrinsic_size().x == 24.0,
		"blocked first base falls back to the source file without picking a later base")
	_check(first.detach() == OK and second.is_connected() and label.get_base_uri() == "res://assets/v2/",
		"detaching the first base promotes the next")
	second.href = ""
	_check(label.get_base_uri() == "res://pages/index.html", "an empty href resolves to the source file")
	second.href = "user://skin/"
	_check(label.get_base_uri() == "user://skin/", "an explicit user resource directory is accepted")
	second.href = "../../../../escape/"
	_check(label.get_base_uri() == "res://pages/index.html", "the resolver rejects paths above the Godot scheme root")
	_check(layout.parse("<base href='assets/'><img src='icon.svg'>", "res://pages") == OK
		and layout.get_base_uri() == "res://pages/assets/", "inline layouts resolve from the supplied directory")
	_check(layout.parse("<svg><base href='ignored/'></base><title>SVG</title></svg><math><base href='ignored/'/></math>",
		"res://pages") == OK and layout.get_base_uri() == "res://pages/", "foreign base elements cannot change the HTML resource base")
	_check(not (layout.query_selector("svg title") is HTMLTitleElement) and layout.get_title().is_empty(),
		"foreign titles keep their generic handles and stay out of the document title")
	var svg: HTMLElement = layout.query_selector("svg")
	_check(svg.set_inner_html("<base href='ignored/'/>") == OK and layout.get_base_uri() == "res://pages/",
		"base elements inserted into a foreign fragment stay out of the resource context")


func _check_link_stylesheet(from_pck: bool) -> void:
	var link: HTMLLinkElement = label.get_element_by_id("optional-sheet") as HTMLLinkElement
	var target: HTMLElement = label.get_element_by_id("optional-target")
	var href: String = link.href
	_check(link.rel == "stylesheet" and link.get_resolved_href() == "res://assets/v1/styles/optional.css",
		"typed link resolves the original href through the document base")
	_check(link.disabled and link.sheet == null and link.get_stylesheet_id() == 0
		and target.get_bounding_rect().size == Vector2(12.0, 12.0),
		"the initially disabled link has no associated sheet and its rules do not apply")
	if from_pck:
		_check(not FileAccess.file_exists("res://assets/v1/styles/optional.css"), "initially disabled CSS uses its baked text in the PCK")
	link.disabled = false
	await _settle()
	var id: int = link.get_stylesheet_id()
	var sheet: HTMLStyleSheet = link.sheet
	_check(id > 0 and target.get_bounding_rect().size == Vector2(44.0, 18.0), "enabling the linked sheet applies its nested import")
	_check(sheet.is_valid() and sheet.id == id and sheet.owner_node == link and sheet.href == link.get_resolved_href(),
		"linked sheet exposes its associated element, loaded URL and legacy ID")
	_check_pixel("optional-target", Color.YELLOW)
	var rule_count: int = label.get_css_rules(id).size()
	_check(sheet.insert_rule("#optional-target { width: 60px; }", rule_count) == OK
		and sheet.get_css_rules() == label.get_css_rules(id), "sheet and legacy CSSOM share the linked rules")
	link.media = "(min-width: 999px)"
	await _settle()
	_check(link.sheet == sheet and sheet.media == link.media and label.get_css_rules(id).size() == rule_count + 1
		and target.get_bounding_rect().size.x == 12.0, "media changes suspend CSSOM rules without discarding them")
	link.media = ""
	link.disabled = true
	await _settle()
	_check(link.sheet == sheet and sheet.disabled and label.get_css_rules(id).size() == rule_count + 1
		and target.get_bounding_rect().size.x == 12.0, "disabling an associated link keeps its ID and rules")
	link.disabled = false
	await _settle()
	_check(target.get_bounding_rect().size.x == 60.0, "re-enabling the link restores the edited rule")
	link.href = "styles/missing.css"
	_check(not sheet.is_valid() and sheet.get_css_rules().is_empty(), "changing href invalidates the old sheet immediately")
	link.href = href
	var replacement: HTMLStyleSheet = link.sheet
	_check(replacement != sheet and replacement.id == id and replacement.get_css_rules().size() == rule_count,
		"restoring href creates a new sheet and preserves the legacy ID")
	_check(sheet.insert_rule("#optional-target { width: 99px; }", rule_count) == ERR_INVALID_PARAMETER
		and sheet.delete_rule(0) == ERR_INVALID_PARAMETER, "obsolete link handles cannot edit the replacement stylesheet")
	await _settle()
	_check(link.get_stylesheet_id() == id and label.get_css_rules(id).size() == rule_count
		and target.get_bounding_rect().size.x == 44.0, "changing href and restoring it in the same frame clears the old edits")
	link.href = "styles/missing.css"
	link.href = href
	_check(not replacement.is_valid() and link.sheet != replacement, "batched href changes retire a sheet even without intermediate reads")
	replacement = link.sheet
	link.type = "text/plain"
	await _settle()
	_check(link.sheet == null and not replacement.is_valid() and link.get_stylesheet_id() == 0
		and target.get_bounding_rect().size.x == 12.0,
		"a non-CSS link type removes the associated sheet")


func _check_inline_stylesheet() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var original: String = "#item { width: 35px; height: 10px; }"
	var viewport_size: Vector2i = Vector2i(180, 100)
	_check(layout.parse("<style id='style'>%s</style><div id='item'></div>" % original) == OK
		and layout.layout(viewport_size) == OK, "inline stylesheet fixture parses and lays out")
	var style: HTMLStyleElement = layout.get_element_by_id("style") as HTMLStyleElement
	var item: HTMLElement = layout.get_element_by_id("item")
	var id: int = style.get_stylesheet_id()
	var sheet: HTMLStyleSheet = style.sheet
	_check(id > 0 and sheet.id == id and sheet.owner_node == style and sheet.href.is_empty()
		and style.text == original and item.get_bounding_rect().size.x == 35.0, "inline style exposes its text and sheet ID")
	_check(layout.insert_rule(id, "#item { width: 90px; }", 1) == OK, "CSSOM edits the inline stylesheet by its element ID")
	style.media = "(min-width: 999px)"
	layout.layout(viewport_size)
	_check(style.sheet == sheet and sheet.media == style.media and sheet.get_css_rules().size() == 2
		and item.get_bounding_rect().size.x != 90.0, "inline media changes retain CSSOM edits")
	style.media = ""
	style.disabled = true
	layout.layout(viewport_size)
	_check(style.sheet == sheet and sheet.disabled and not style.has_attribute("disabled")
		and sheet.get_css_rules().size() == 2 and item.get_bounding_rect().size.x != 90.0,
		"inline disabled controls the associated sheet without changing HTML attributes")
	_check(sheet.delete_rule(1) == OK and sheet.insert_rule("#item { width: 90px; }", 1) == OK,
		"the inline sheet can be edited while disabled")
	style.disabled = false
	layout.layout(viewport_size)
	_check(item.get_bounding_rect().size.x == 90.0, "re-enabling inline style restores the edited rule")
	style.text = "#item { width: 50px; }"
	style.text = original
	_check(not sheet.is_valid() and sheet.id == 0 and sheet.owner_node == null,
		"style text edits invalidate the original handle even when changed back before a CSSOM read")
	var replacement: HTMLStyleSheet = style.sheet
	_check(replacement != sheet and replacement.id == id and sheet.delete_rule(0) == ERR_INVALID_PARAMETER
		and sheet.insert_rule("#item { width: 55px; }") == ERR_INVALID_PARAMETER,
		"obsolete inline handles cannot edit the replacement stylesheet")
	layout.layout(viewport_size)
	_check(style.get_stylesheet_id() == id and layout.get_css_rules(id).size() == 1
		and item.get_bounding_rect().size.x == 35.0, "replacing style text clears old edits even when restoring the same text")
	_check(layout.insert_rule(id, "#item { width: 90px; }", 1) == OK, "CSSOM edits the replacement inline stylesheet")
	var text_node: HTMLTextNode = style.get_first_child() as HTMLTextNode
	text_node.data = "#item { width: 50px; height: 10px; }"
	layout.layout(viewport_size)
	_check(not replacement.is_valid() and layout.get_css_rules(id).size() == 1 and item.get_bounding_rect().size.x == 50.0,
		"Text node edits through the DOM invalidate inline CSSOM rules")
	style.type = "text/plain"
	layout.layout(viewport_size)
	_check(style.get_stylesheet_id() == 0 and item.get_bounding_rect().size.x != 50.0, "non-CSS style text has no associated stylesheet")
	style.type = "text/css"
	var before_detach: HTMLStyleSheet = style.sheet
	var parent: HTMLElement = style.get_parent()
	_check(style.detach() == OK and parent.append_child(style) == OK, "the original style element can be reinserted")
	var after_detach: HTMLStyleSheet = style.sheet
	_check(not before_detach.is_valid() and after_detach != before_detach and after_detach.id == id,
		"detaching and reinserting in one frame does not reactivate a stylesheet handle")
	var other: HTMLLayout = HTMLLayout.new()
	_check(other.parse("<p>Another document</p>") == OK and other.adopt_node(style) == style
		and other.get_head().append_child(style) == OK, "a stylesheet owner can move across layouts")
	var adopted: HTMLStyleSheet = style.sheet
	_check(not after_detach.is_valid() and adopted != after_detach and adopted.id != id
		and after_detach.delete_rule(0) == ERR_INVALID_PARAMETER, "adoption creates a sheet belonging to the target layout")
	other.clear()
	_check(not adopted.is_valid() and adopted.get_css_rules().is_empty(), "clearing the layout invalidates its sheets")


func _check_shared_stylesheets() -> void:
	var first: HTMLLayout = label.document.create_layout()
	var second: HTMLLayout = label.document.create_layout()
	var first_style: HTMLStyleElement = first.query_selector("style") as HTMLStyleElement
	var second_style: HTMLStyleElement = second.query_selector("style") as HTMLStyleElement
	var first_sheet: HTMLStyleSheet = first_style.sheet
	var second_sheet: HTMLStyleSheet = second_style.sheet
	var rules: PackedStringArray = second_sheet.get_css_rules()
	_check(first_sheet != second_sheet and first_sheet.insert_rule("div { color: red; }") == ERR_INVALID_DATA
		and second_sheet.get_css_rules() == rules, "inserting a rule before an import uses the existing CSSOM validation")
	_check(first_sheet.insert_rule("div { color: red; }", rules.size()) == OK
		and first_sheet.get_css_rules().size() == rules.size() + 1 and second_sheet.get_css_rules() == rules,
		"shared HTML documents have independent stylesheet rules and handles")
	var original_instance_id: int = first_sheet.get_instance_id()
	first_sheet = null
	first_style.media = "screen"
	first_sheet = first_style.sheet
	_check(first_sheet.get_instance_id() == original_instance_id, "the layout keeps the current sheet identity between reads")
	first = null
	_check(not first_sheet.is_valid() and second_sheet.is_valid(), "holding a sheet does not retain its layout")


func _check_pixel(id: String, expected: Color) -> void:
	var element: HTMLElement = label.get_element_by_id(id)
	var sample: Vector2i = Vector2i(element.get_bounding_rect().position) + Vector2i(4, 4)
	var actual: Color = frame.get_texture().get_image().get_pixelv(sample)
	_check(actual.is_equal_approx(expected), "%s uses the baked relative resource (%s)" % [id, actual])


func _settle() -> void:
	for _step: int in range(4):
		await get_tree().process_frame
	await RenderingServer.frame_post_draw


func _check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		push_error(description)
