#pragma once

#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace dockit {
	inline constexpr int MAX_HTML_SOURCE_BYTES = 1024 * 1024;

	enum class RemoteStylesheetStatus { loading, ready, failed };

	struct RemoteStylesheet {
		RemoteStylesheetStatus status = RemoteStylesheetStatus::failed;
		godot::String url;
		godot::String text;
	};

	std::optional<godot::String> decode_html_source_text(const godot::PackedByteArray& p_bytes);
	RemoteStylesheet request_remote_stylesheet(const godot::String& p_url, const godot::Callable& p_completed);
	void shutdown_remote_stylesheets();
}
