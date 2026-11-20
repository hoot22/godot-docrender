#pragma once

#include "formats/html/html_layout.h"

#include <godot_cpp/classes/control.hpp>

class HTMLSelectionDragPreview : public godot::Control {
	GDCLASS(HTMLSelectionDragPreview, godot::Control)

	godot::Ref<HTMLLayout> layout;
	godot::Vector2i selection;
	godot::Rect2 selection_bounds;
	bool draw_element = false;

protected:
	static void _bind_methods() {}

public:
	HTMLSelectionDragPreview() = default;
	HTMLSelectionDragPreview(const godot::Ref<HTMLLayout>& p_layout, const godot::Vector2i& p_selection,
		const godot::Rect2& p_bounds, bool p_draw_element = false);
	void _draw() override;
};
