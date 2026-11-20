extends Node

var failures: int
var base_url: String
var frame: SubViewport
var label: HTMLLabel


func _ready() -> void:
	base_url = OS.get_cmdline_user_args()[0]
	frame = SubViewport.new()
	frame.size = Vector2i(240, 160)
	frame.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(frame)
	label = HTMLLabel.new()
	label.size = Vector2(240.0, 160.0)
	var mirror: HTMLLayout = HTMLLayout.new()
	_check(mirror.parse('<link id="mirror" rel="stylesheet" href="%s/entry">' % base_url) == OK,
		"an independent layout can request the same remote stylesheet")
	label.text = """
		<link id="early" rel="stylesheet" href="%s/early">
		<link id="theme" rel="stylesheet" href="%s/entry">
		<link id="replace-link" rel="stylesheet" href="%s/stale">
		<link id="off-link" rel="stylesheet" href="%s/disabled" disabled>
		<link id="wrong-link" rel="stylesheet" href="%s/wrong-mime">
		<link id="empty-link" rel="stylesheet" href="%s/empty">
		<style>@import "%s/inline-sheet"; @import "%s/never" supports(display: impossible-mode);
		html,body{margin:0} div{height:15px}</style>
		<div id="probe"></div><div id="pic"></div><div id="child"></div>
		<div id="cssom"></div><div id="replace"></div><div id="off"></div><div id="inline"></div>
	""" % [base_url, base_url, base_url, base_url, base_url, base_url, base_url, base_url]
	frame.add_child(label)
	await _settle()

	var early: HTMLLinkElement = _link("early")
	var theme: HTMLLinkElement = _link("theme")
	var replacement: HTMLLinkElement = _link("replace-link")
	var off: HTMLLinkElement = _link("off-link")
	_check(early.sheet == null and replacement.sheet == null and off.sheet == null, "pending and initially disabled links expose no sheet")
	replacement.href = base_url + "/replacement"
	_check(await _until(func() -> bool: return theme.sheet != null and _width("probe") == 60.0),
		"the extensionless stylesheet loads through a relative redirect")
	var sheet: HTMLStyleSheet = theme.sheet
	if sheet == null:
		get_tree().quit(1)
		return
	var id: int = sheet.id
	_check(sheet.href == base_url + "/v2/css/theme?build=1" and theme.get_resolved_href() == base_url + "/entry",
		"the sheet uses the response URL and the element retains its requested address")
	var mirror_link: HTMLLinkElement = mirror.get_element_by_id("mirror") as HTMLLinkElement
	_check(mirror_link.sheet != null and mirror_link.sheet != sheet and mirror_link.sheet.href == sheet.href,
		"layouts share the response and retain independent CSSOM handles")
	_check(sheet.get_css_rules()[0].begins_with("@import"), "CSSOM presents the unexpanded import")
	_check(sheet.insert_rule("#cssom{width:23px;background:url(./dynamic/tile) no-repeat}", sheet.get_css_rules().size()) == OK,
		"CSSOM can edit the downloaded stylesheet")

	_check(await _until(func() -> bool: return early.sheet != null and _width("child") == 29.0),
		"both the earlier link and the nested import load")
	_check(_width("probe") == 60.0 and theme.sheet == sheet and _width("cssom") == 23.0,
		"late CSS keeps document order, parent handle and CSSOM edits")
	_check(await _until(func() -> bool: return _pixel("pic").is_equal_approx(Color.RED)), "relative URL uses the redirected directory")
	_check(await _until(func() -> bool: return _pixel("child").is_equal_approx(Color.GREEN)), "imported CSS uses its own redirected directory")
	_check(await _until(func() -> bool: return _pixel("cssom").is_equal_approx(Color.BLUE)), "CSSOM-inserted URLs use the same base")
	_check(_width("replace") == 43.0, "replacing a pending href keeps the newer styles")
	_check(_width("inline") == 26.0, "an inline style can import an absolute remote URL")

	_check(sheet.insert_rule("@import 'waiting';", 0) == OK, "CSSOM can introduce a remote import")
	var pending_import_rules: PackedStringArray = sheet.get_css_rules()
	_check(pending_import_rules[0].contains("waiting"), "the pending import is editable")
	await get_tree().create_timer(0.15).timeout
	_check(sheet.delete_rule(0) == OK, "CSSOM can remove an in-flight import")
	await get_tree().create_timer(0.8).timeout
	_check(_width("probe") == 60.0 and _width("replace") == 43.0, "late removed imports and replaced links do not apply")

	theme.media = "(min-width: 999px)"
	await _settle()
	_check(theme.sheet == sheet and _width("probe") == 95.0, "media changes preserve the sheet and suspend its rules")
	theme.media = ""
	theme.disabled = true
	await _settle()
	_check(theme.sheet == sheet and sheet.disabled and _width("probe") == 95.0, "disable preserves a redirected sheet")
	theme.disabled = false
	off.disabled = false
	_check(await _until(func() -> bool: return off.sheet != null and _width("off") == 48.0), "enabling a disabled link starts its download")
	_check(theme.sheet == sheet and _width("probe") == 60.0 and _width("cssom") == 23.0, "enabling restores existing CSSOM edits")
	_check(_link("wrong-link").sheet == null and _link("empty-link").sheet != null, "wrong MIME is rejected and empty CSS is accepted")
	theme.href = base_url + "/wrong-mime"
	_check(not sheet.is_valid() and sheet.delete_rule(0) == ERR_INVALID_PARAMETER, "a replaced source invalidates its handle immediately")
	theme.href = base_url + "/entry"
	_check(theme.sheet != sheet and theme.get_stylesheet_id() == id and theme.sheet.get_css_rules().size() == 3,
		"restoring the source gets original rules with the legacy ID")
	if failures == 0:
		print("HTML remote stylesheet checks passed")
	get_tree().quit(0 if failures == 0 else 1)


func _link(id: String) -> HTMLLinkElement:
	return label.get_element_by_id(id) as HTMLLinkElement


func _width(id: String) -> float:
	return label.get_element_by_id(id).get_bounding_rect().size.x


func _pixel(id: String) -> Color:
	var point: Vector2 = label.get_element_by_id(id).get_bounding_rect().position
	return frame.get_texture().get_image().get_pixelv(Vector2i(point) + Vector2i(2, 2))


func _settle() -> void:
	for _frame: int in range(3):
		await get_tree().process_frame
	await RenderingServer.frame_post_draw


func _until(condition: Callable) -> bool:
	var deadline: int = Time.get_ticks_msec() + 3500
	while not condition.call() and Time.get_ticks_msec() < deadline:
		await _settle()
	return condition.call()


func _check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		push_error(description)
