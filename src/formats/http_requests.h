#pragma once

#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <functional>
#include <cstdint>

namespace dockit {
	struct HTTPResponse {
		godot::String url;
		int status_code = 0;
		godot::PackedStringArray headers;
		godot::PackedByteArray body;
		godot::String error;
	};

	struct HTTPRequestOptions {
		godot::PackedStringArray headers;
		godot::HTTPClient::Method method = godot::HTTPClient::METHOD_GET;
		godot::String body;
		int max_body = 8 * 1024 * 1024;
		double timeout = 12.0;
		bool track_redirects = false;
	};

	godot::String http_response_header(const godot::PackedStringArray& p_headers, const godot::String& p_name);
	uint64_t queue_http_request(const godot::String& p_url, const HTTPRequestOptions& p_options,
		std::function<void(const HTTPResponse&)> p_complete);
	uint64_t queue_http_request(const godot::String& p_url, int p_max_body, bool p_track_redirects,
		std::function<void(const HTTPResponse&)> p_complete);
	void cancel_http_request(uint64_t p_request_id);
	void shutdown_http_requests();
}
