#pragma once

#include <godot_cpp/classes/font_file.hpp>

namespace dockit {
	godot::Ref<godot::FontFile> load_system_font_face(const godot::String& p_name);
}
