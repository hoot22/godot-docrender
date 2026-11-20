#include "remote_images.h"
#include "http_requests.h"
#include "http_url.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

using namespace godot;

namespace dockit {
	namespace {
		constexpr int MAX_ENTRIES = 128;
		constexpr int MAX_BODY_BYTES = 8 * 1024 * 1024;
		constexpr int64_t MAX_PIXELS = 16 * 1024 * 1024;
		constexpr int64_t MAX_CACHE_BYTES = 128 * 1024 * 1024;
		constexpr uint64_t RETRY_DELAY_MS = 20'000;

		bool supported_size(int64_t p_width, int64_t p_height) {
			return p_width > 0 && p_height > 0 && p_width <= 8192 && p_height <= 8192 &&
				p_width * p_height <= MAX_PIXELS;
		}

		bool jpeg_size_supported(std::string_view p_bytes) {
			const auto byte = [&](size_t p_pos) { return static_cast<uint8_t>(p_bytes[p_pos]); };
			for (size_t pos = 2; pos + 3 < p_bytes.size() && byte(pos++) == 0xff;) {
				const auto marker = byte(pos++);
				if (marker == 0xff) { --pos; continue; }
				if (marker == 0xda || marker == 0xd9) return false;
				if (marker == 1 || (marker >= 0xd0 && marker <= 0xd7)) continue;
				const size_t length = (size_t(byte(pos)) << 8) | byte(pos + 1);
				if (length < 2 || length > p_bytes.size() - pos) return false;
				if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
					return length >= 8 && supported_size((byte(pos + 5) << 8) | byte(pos + 6),
						(byte(pos + 3) << 8) | byte(pos + 4));
				}
				pos += length;
			}
			return false;
		}

		bool webp_size_supported(std::string_view p_bytes) {
			const auto byte = [&](size_t p_pos) { return static_cast<uint8_t>(p_bytes[p_pos]); };
			const auto little = [&](size_t p_pos, size_t p_count) {
				uint32_t number = 0;
				for (size_t i = 0; i < p_count; ++i) number |= uint32_t(byte(p_pos + i)) << (8 * i);
				return number;
			};
			// WebP starts with its extended header or the first image chunk.
			if (p_bytes.size() < 25 || p_bytes.substr(8, 4) != "WEBP") return false;
			const auto tag = p_bytes.substr(12, 4);
			const auto size = little(16, 4);
			if (size > p_bytes.size() - 20) return false;
			if (tag == "VP8X" && size >= 10) {
				return supported_size(little(24, 3) + 1, little(27, 3) + 1);
			}
			if (tag == "VP8L" && size >= 5 && byte(20) == 0x2f) {
				const auto dims = little(21, 4);
				return supported_size((dims & 0x3fff) + 1, ((dims >> 14) & 0x3fff) + 1);
			}
			return tag == "VP8 " && size >= 10 && byte(23) == 0x9d && byte(24) == 1 && byte(25) == 0x2a &&
				supported_size(little(26, 2) & 0x3fff, little(28, 2) & 0x3fff);
		}

		Ref<Texture2D> decode_texture(const PackedByteArray& p_body) {
			if (p_body.is_empty()) return {};
			const std::string_view bytes(reinterpret_cast<const char*>(p_body.ptr()), p_body.size());
			const auto byte = [&](size_t p_pos) { return static_cast<uint8_t>(bytes[p_pos]); };
			const auto big32 = [&](size_t p_pos) {
				return (uint32_t(byte(p_pos)) << 24) | (uint32_t(byte(p_pos + 1)) << 16) |
					(uint32_t(byte(p_pos + 2)) << 8) | byte(p_pos + 3);
			};
			// Read dimensions before asking the decoder to allocate a pixel buffer.
			Ref<Image> image;
			image.instantiate();
			Error error = ERR_FILE_UNRECOGNIZED;
			if (bytes.substr(0, 8) == "\x89PNG\r\n\x1a\n" && bytes.size() >= 24 && bytes.substr(12, 4) == "IHDR" &&
				supported_size(big32(16), big32(20))) {
				error = image->load_png_from_buffer(p_body);
			} else if (bytes.substr(0, 2) == "\xff\xd8" && jpeg_size_supported(bytes)) {
				error = image->load_jpg_from_buffer(p_body);
			} else if (bytes.substr(0, 4) == "RIFF" && webp_size_supported(bytes)) {
				error = image->load_webp_from_buffer(p_body);
			} else if ((bytes.substr(0, 6) == "GIF87a" || bytes.substr(0, 6) == "GIF89a") && bytes.size() >= 10 &&
				supported_size(byte(6) | (byte(7) << 8), byte(8) | (byte(9) << 8)) &&
				ClassDB::class_exists("GIFTexture")) {
				const Ref<Resource> decoded = ClassDB::class_call_static("GIFTexture", "load_from_buffer", p_body);
				const Ref<Texture2D> texture = decoded;
				return texture;
			}
			if (error != OK || !supported_size(image->get_width(), image->get_height())) return {};
			return ImageTexture::create_from_image(image);
		}

		struct Entry {
			RemoteImage image{RemoteImageStatus::loading, {}, {}};
			std::vector<Callable> listeners;
			uint64_t used = 0;
			uint64_t retry_at = 0;
			int64_t bytes = 0;
		};

		HashMap<String, Entry> entries;
		int64_t cache_bytes = 0;

		void evict(const String& p_keep, bool p_need_room) {
			while (cache_bytes > MAX_CACHE_BYTES || (p_need_room && entries.size() >= MAX_ENTRIES)) {
				auto oldest = entries.end();
				for (auto it = entries.begin(); it != entries.end(); ++it) {
					if (it->key == p_keep || it->value.image.status == RemoteImageStatus::loading) continue;
					if (oldest == entries.end() || it->value.used < oldest->value.used) oldest = it;
				}
				if (oldest == entries.end()) break;
				cache_bytes -= oldest->value.bytes;
				const String key = oldest->key;
				entries.erase(key);
			}
		}

		void finish(const String& p_url, const Ref<Texture2D>& p_texture, const String& p_error) {
			auto& entry = entries[p_url];
			const bool ready = p_texture.is_valid();
			entry.image = {ready ? RemoteImageStatus::ready : RemoteImageStatus::failed, p_texture, p_error};
			entry.used = Time::get_singleton()->get_ticks_msec();
			entry.retry_at = entry.used + RETRY_DELAY_MS;
			entry.bytes = ready ? int64_t(p_texture->get_width()) * p_texture->get_height() * 4 : 0;
			cache_bytes += entry.bytes;
			auto listeners = std::move(entry.listeners);
			evict(p_url, false);
			for (const auto& listener : listeners) {
				if (listener.is_valid()) listener.call_deferred(p_url, p_texture);
			}
		}
	}

	bool is_remote_image_url(const String& p_url) { return is_http_url(p_url); }

	String remote_image_request_url(const String& p_url) { return http_request_url(p_url); }

	RemoteImage request_remote_image(const String& p_url, const Callable& p_completed) {
		if (!is_remote_image_url(p_url)) return {};
		const String url = remote_image_request_url(p_url);
		const auto now = Time::get_singleton()->get_ticks_msec();
		auto* entry = entries.getptr(url);
		if (entry && entry->image.status == RemoteImageStatus::failed && now >= entry->retry_at) {
			entries.erase(url);
			entry = nullptr;
		}
		if (!entry) {
			evict({}, true);
			if (entries.size() >= MAX_ENTRIES) return {RemoteImageStatus::failed, {}, "request_queue_full"};
			entries.insert(url, Entry{});
			entry = entries.getptr(url);
			queue_http_request(url, MAX_BODY_BYTES, false, [url](const HTTPResponse& p_response) {
				if (!p_response.error.is_empty()) {
					finish(url, {}, p_response.error);
					return;
				}
				const auto texture = decode_texture(p_response.body);
				finish(url, texture, texture.is_valid() ? String() : String("decode_failed"));
			});
		}
		entry->used = now;
		if (entry->image.status == RemoteImageStatus::loading && p_completed.is_valid() &&
			std::find(entry->listeners.begin(), entry->listeners.end(), p_completed) == entry->listeners.end()) {
			entry->listeners.push_back(p_completed);
		}
		return entry->image;
	}

	void shutdown_remote_images() {
		entries.clear();
		cache_bytes = 0;
	}
}
