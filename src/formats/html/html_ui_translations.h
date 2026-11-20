#pragma once

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace dockit {
	void initialize_html_ui_translations();
	void shutdown_html_ui_translations();
	godot::String html_ui_translate(const godot::String& p_message, const godot::StringName& p_context);
}
