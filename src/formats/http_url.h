#pragma once

#include <godot_cpp/variant/string.hpp>

namespace dockit {
	bool is_http_url(const godot::String& p_url);
	godot::String http_request_url(const godot::String& p_url);
	godot::String resolve_http_url(const godot::String& p_reference, const godot::String& p_base = {});
}
