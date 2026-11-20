extends HTMLBehavior

func _attach(_label: HTMLLabel) -> void:
	var canvas: HTMLCanvasElement = document.get_element_by_id("canvas")
	var ctx: HTMLCanvasRenderingContext2D = canvas.get_context("2d")
	ctx.fill_style = "green"
	ctx.fill_rect(10.0, 10.0, 100.0, 100.0)
