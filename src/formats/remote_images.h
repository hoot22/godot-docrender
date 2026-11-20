#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace dockit {
	enum class RemoteImageStatus { loading, ready, failed };

	struct RemoteImage {
		RemoteImageStatus status = RemoteImageStatus::failed;
		godot::Ref<godot::Texture2D> texture;
		godot::String error;
	};

	bool is_remote_image_url(const godot::String& p_url);
	godot::String remote_image_request_url(const godot::String& p_url);
	RemoteImage request_remote_image(const godot::String& p_url, const godot::Callable& p_completed = {});
	void shutdown_remote_images();
}
