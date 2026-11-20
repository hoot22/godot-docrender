extends SceneTree



func _initialize() -> void:
	call_deferred("_run")



func _run() -> void:
	var document: HTMLDocument = HTMLDocument.new()
	document.text = """
		<!doctype html>
		<html>
		<head>
			<style>
				@page { size: 200px 180px; margin: 10px; }
				@page :first { margin-top: 20px; }
				body { margin: 0; }
				@media screen { .media-check { display: none; } }
				@media print { .media-check { display: block; } }
				@media screen { .screen-only { display: block; } }
				@media print { .screen-only { display: none; } }
				.long { height: 500px; }
				.short { height: 135px; }
				.break-target { break-before: page; }
			</style>
		</head>
		<body>
			<div class="media-check">PRINT_ONLY_TEXT</div>
			<div>中文打印文本</div>
			<div class="screen-only">SCREEN_ONLY_TEXT</div>
			<div class="short">BEFORE_FORCED_BREAK</div>
			<div class="break-target">AFTER_FORCED_BREAK</div>
			<div class="long">PAGED_CONTENT</div>
		</body>
		</html>
	"""
	var screen_layout: HTMLLayout = document.create_layout()
	if screen_layout.layout(Vector2i(200, 180)) != OK:
		_fail("Screen layout unexpectedly failed")
		return
	if not screen_layout.get_plain_text().contains("SCREEN_ONLY_TEXT"):
		_fail("Creating a print layout changed screen media behavior")
		return
	var print_layout: HTMLPrintLayout = document.create_print_layout()
	if not print_layout.is_ready():
		await print_layout.ready
	if print_layout.get_error() != OK:
		_fail("Print layout failed: %s; diagnostics: %s; pages: %d" % [print_layout.get_error_message(), print_layout.get_diagnostics(), print_layout.get_page_count()])
		return
	if print_layout.get_page_count() < 6:
		_fail("Expected forced break pagination to use 6 or more pages, got %d" % print_layout.get_page_count())
		return
	if print_layout.get_page_size(0) != Vector2(200.0, 180.0):
		_fail("CSS @page size was not applied")
		return
	var first_content: Rect2 = print_layout.get_page_content_rect(0)
	var second_content: Rect2 = print_layout.get_page_content_rect(1)
	if first_content != Rect2(10.0, 20.0, 180.0, 150.0):
		_fail("The first page @page margin was not applied: %s" % first_content)
		return
	if second_content != Rect2(10.0, 10.0, 180.0, 160.0):
		_fail("The base @page margin did not carry to later pages: %s" % second_content)
		return
	var override_options: Dictionary = {
		"page_size": Vector2(300.0, 400.0),
		"orientation": "landscape",
		"margin": 5.0,
	}
	var overridden: HTMLPrintLayout = document.create_print_layout(override_options)
	if overridden.get_page_size(0) != Vector2(400.0, 300.0):
		_fail("Explicit print options did not override CSS page size and orientation")
		return
	if overridden.get_page_content_rect(0) != Rect2(5.0, 5.0, 390.0, 290.0):
		_fail("Explicit print margin was not applied")
		return
	if not PDFWriter.is_available():
		_fail("PDFium writer is unavailable in this test build")
		return
	var pdf: PackedByteArray = print_layout.to_pdf()
	if pdf.is_empty():
		_fail("PDF export failed: %s" % print_layout.get_error_message())
		return
	var reader: PDFReader = PDFReader.new()
	if reader.open_buffer(pdf) != OK:
		_fail("PDFReader could not open the exported PDF")
		return
	if reader.get_page_count() != print_layout.get_page_count():
		_fail("PDF page count does not match the print layout")
		return
	var page_info: Dictionary = reader.get_page_info(0)
	if page_info.get("size_pt") != Vector2(150.0, 135.0):
		_fail("PDF page dimensions do not match the CSS page size")
		return
	var first_page_text: String = reader.get_page_text(0)
	if not first_page_text.contains("PRINT_ONLY_TEXT"):
		_fail("Print media query content is missing from the PDF text")
		return
	var has_font_encoding_diagnostic: bool = false
	for diagnostic: String in print_layout.get_diagnostics():
		if diagnostic.contains("could not be encoded"):
			has_font_encoding_diagnostic = true
			break
	if not has_font_encoding_diagnostic:
		_fail("Unsupported PDF text glyphs were not reported")
		return
	for page_index: int in range(reader.get_page_count()):
		if reader.get_page_text(page_index).contains("SCREEN_ONLY_TEXT"):
			_fail("Screen media content leaked into the print PDF")
			return
	var page_objects: Array[Dictionary] = reader.get_page_objects(0)
	var has_vector_text: bool = false
	for page_object: Dictionary in page_objects:
		if page_object.get("text", "") == "PRINT_ONLY_TEXT":
			has_vector_text = true
			break
	if not has_vector_text:
		_fail("PDF text was not preserved as a searchable text object")
		return
	if print_layout.save_pdf("user://print-output.pdf") != OK or not FileAccess.file_exists("user://print-output.pdf"):
		_fail("Saving the PDF to a Godot user:// path failed")
		return
	print("HTML print layout and PDF export checks passed")
	quit(0)



func _fail(message: String) -> void:
	push_error(message)
	quit(1)
