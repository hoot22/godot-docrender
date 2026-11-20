extends Node

var failures: int
var base_url: String


func _ready() -> void:
	base_url = OS.get_cmdline_user_args()[0]
	var frame: SubViewport = SubViewport.new()
	frame.size = Vector2i(240, 160)
	frame.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(frame)

	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.size = Vector2(240.0, 160.0)
	markdown.text = "![Shared](%s/shared?asset=1)" % base_url
	add_child(markdown)
	_check(markdown.get_content_height() > 0.0, "Markdown starts loading the shared URL")

	var html: HTMLLabel = HTMLLabel.new()
	html.position.y = 32.0
	html.size = Vector2(240.0, 128.0)
	html.text = """
		<style>html,body{margin:0} img{display:block}</style>
		<img id="shared" src="%s/shared?asset=1" alt="Waiting for image">
		<img id="bad" src="%s/corrupt" alt="Invalid image">
		<img id="missing" src="%s/missing" alt="Missing image">
		<img id="large" src="%s/oversized" alt="Oversized image">
	""" % [base_url, base_url, base_url, base_url]
	frame.add_child(html)

	var paint_only: HTMLLabel = HTMLLabel.new()
	paint_only.size = Vector2(240.0, 32.0)
	paint_only.text = """
		<style>html,body{margin:0}
		#bg{width:40px;height:24px;background:#000 url('%s/background') no-repeat right bottom}</style>
		<div id="bg"></div>
	""" % base_url
	var layout_events: Array[int] = [0]
	paint_only.layout_changed.connect(func() -> void: layout_events[0] += 1)
	frame.add_child(paint_only)

	await _settle()
	markdown.queue_free()

	var embedded: MarkdownLabel = MarkdownLabel.new()
	embedded.size = Vector2(240.0, 160.0)
	embedded.parse_html = true
	embedded.text = "<div><img style='display:block' src='%s/embedded' alt='Waiting'></div>\n\nFollowing paragraph" % base_url
	add_child(embedded)
	await _settle()
	var embedded_height: float = embedded.get_content_height()

	var shared: HTMLImageElement = html.get_element_by_id("shared") as HTMLImageElement
	var background: HTMLElement = paint_only.get_element_by_id("bg")
	var original_background_rect: Rect2 = background.get_bounding_rect()
	var original_alt_rect: Rect2 = shared.get_bounding_rect()
	var initial_layout_events: int = layout_events[0]
	_check(shared.get_intrinsic_size() == Vector2.ZERO and original_alt_rect.has_area(), "pending img uses its alt box")
	_check(await _until(func() -> bool: return _pixel(frame, Vector2i(36, 21)).is_equal_approx(Color.YELLOW)),
		"background is painted using its downloaded natural size and right/bottom positioning (%s, %s)"
		% [_pixel(frame, Vector2i(36, 21)), original_background_rect])
	_check(_pixel(frame, Vector2i(30, 21)).is_equal_approx(Color.BLACK) and
		background.get_bounding_rect() == original_background_rect and layout_events[0] == initial_layout_events,
		"background completion leaves geometry and layout_changed untouched (%s, %s, %s -> %s)"
		% [_pixel(frame, Vector2i(30, 21)), background.get_bounding_rect(), initial_layout_events, layout_events[0]])
	var shared_loaded: Callable = func() -> bool:
		return shared.get_intrinsic_size() == Vector2(26.0, 18.0) and shared.get_bounding_rect().size == Vector2(26.0, 18.0)
	_check(await _until(shared_loaded), "the shared request survives Markdown canvas deletion (%s, %s)"
		% [shared.get_intrinsic_size(), shared.get_bounding_rect()])
	_check(shared.get_bounding_rect().size == Vector2(26.0, 18.0) and shared.get_bounding_rect() != original_alt_rect,
		"ready img replaces alt and reflows to its intrinsic size")
	for id: String in ["bad", "missing", "large"]:
		var image: HTMLImageElement = html.get_element_by_id(id) as HTMLImageElement
		_check(image.get_intrinsic_size() == Vector2.ZERO and image.get_bounding_rect().has_area(), id + " preserves alt on failure")
	_check(await _until(func() -> bool: return embedded.get_content_height() > embedded_height + 30.0),
		"a downloaded HTML block image updates the enclosing Markdown layout (%s -> %s)"
		% [embedded_height, embedded.get_content_height()])

	await _check_responsive()
	if failures == 0:
		print("HTML remote image checks passed")
	get_tree().quit(0 if failures == 0 else 1)


func _check_responsive() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse("""
		<style>html,body{margin:0} img{display:block}</style>
		<img id="responsive" alt="Responsive" src="%s/unselected"
			srcset="%s/slow 40w, %s/fast 120w" sizes="(max-width: 110px) 40px, 120px">
		<picture><source type="image/avif" srcset="%s/unselected 1x">
			<source type="image/png" media="(min-width: 150px)" srcset="%s/picture 1x">
			<img id="picture" src="%s/local-choice"></picture>
	""" % [base_url, base_url, base_url, base_url, base_url, base_url]) == OK, "responsive remote fixture parses")
	_check(layout.layout(Vector2i(100, 100)) == OK, "initial responsive layout succeeds")
	var image: HTMLImageElement = layout.get_element_by_id("responsive") as HTMLImageElement
	var picture: HTMLImageElement = layout.get_element_by_id("picture") as HTMLImageElement
	_check(image.current_src == base_url + "/slow" and picture.current_src == base_url + "/local-choice",
		"narrow layout selects the sized srcset candidate and picture fallback")
	await _settle()
	_check(layout.layout(Vector2i(200, 100)) == OK and image.current_src == base_url + "/fast"
		and picture.current_src == base_url + "/picture", "wider layout selects another srcset and picture source")
	var sources_loaded: Callable = func() -> bool:
		return image.get_intrinsic_size() == Vector2(120.0, 14.0) and picture.get_intrinsic_size() == Vector2(14.0, 12.0)
	_check(await _until(sources_loaded), "new sources load first")
	await get_tree().create_timer(1.0).timeout
	_check(image.current_src == base_url + "/fast" and image.get_intrinsic_size() == Vector2(120.0, 14.0),
		"late original response cannot replace the current selection")
	layout.layout(Vector2i(100, 100))
	_check(image.get_intrinsic_size() == Vector2(40.0, 18.0), "switching back uses the cached original candidate")
	image.srcset = "%s/slow 2x" % base_url
	image.src = ""
	layout.layout(Vector2i(100, 100))
	_check(image.get_intrinsic_size() == Vector2(20.0, 9.0), "the same texture can use a different srcset density")


func _pixel(frame: SubViewport, point: Vector2i) -> Color:
	return frame.get_texture().get_image().get_pixelv(point)


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
