#include "html_remote_stylesheets.h"

#include "formats/http_requests.h"

#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include <algorithm>
#include <vector>

using namespace godot;

namespace dockit {
	namespace {
		constexpr int MAX_ENTRIES = 128;
		constexpr uint64_t RETRY_DELAY_MS = 20'000;

		struct Entry {
			RemoteStylesheet sheet{RemoteStylesheetStatus::loading, {}, {}};
			std::vector<Callable> listeners;
			uint64_t used = 0;
		};

		HashMap<String, Entry> sheets;

		void trim(const String& p_keep, int p_limit) {
			while (sheets.size() > p_limit) {
				auto oldest = sheets.end();
				for (auto it = sheets.begin(); it != sheets.end(); ++it) {
					if (it->key == p_keep || it->value.sheet.status == RemoteStylesheetStatus::loading) continue;
					if (oldest == sheets.end() || it->value.used < oldest->value.used) oldest = it;
				}
				if (oldest == sheets.end()) return;
				const String key = oldest->key;
				sheets.erase(key);
			}
		}

		void completed(const String& p_request_url, const HTTPResponse& p_response) {
			auto& entry = sheets[p_request_url];
			const auto mime = http_response_header(p_response.headers, "Content-Type").get_slice(";", 0).strip_edges();
			const auto text = p_response.error.is_empty() && (mime.is_empty() || mime.nocasecmp_to("text/css") == 0)
				? decode_html_source_text(p_response.body) : std::nullopt;
			entry.sheet = text ? RemoteStylesheet{RemoteStylesheetStatus::ready, p_response.url, *text} : RemoteStylesheet{};
			entry.used = Time::get_singleton()->get_ticks_msec();
			auto listeners = std::move(entry.listeners);
			trim(p_request_url, MAX_ENTRIES);
			for (const auto& listener : listeners) {
				if (listener.is_valid()) listener.call_deferred(p_request_url, entry.sheet.url, entry.sheet.text);
			}
		}
	}

	std::optional<String> decode_html_source_text(const PackedByteArray& p_bytes) {
		String text;
		if (p_bytes.size() > MAX_HTML_SOURCE_BYTES || (!p_bytes.is_empty() &&
			(p_bytes.has(0) || text.parse_utf8(reinterpret_cast<const char*>(p_bytes.ptr()), p_bytes.size()) != OK))) return std::nullopt;
		return text;
	}

	RemoteStylesheet request_remote_stylesheet(const String& p_url, const Callable& p_completed) {
		const auto now = Time::get_singleton()->get_ticks_msec();
		auto* entry = sheets.getptr(p_url);
		if (entry && entry->sheet.status == RemoteStylesheetStatus::failed && now - entry->used >= RETRY_DELAY_MS) {
			sheets.erase(p_url);
			entry = nullptr;
		}
		if (!entry) {
			trim({}, MAX_ENTRIES - 1);
			sheets.insert(p_url, Entry{});
			entry = sheets.getptr(p_url);
			queue_http_request(p_url, MAX_HTML_SOURCE_BYTES, true, [p_url](const HTTPResponse& p_response) {
				completed(p_url, p_response);
			});
		}
		if (entry->sheet.status != RemoteStylesheetStatus::failed) entry->used = now;
		if (entry->sheet.status == RemoteStylesheetStatus::loading && p_completed.is_valid() &&
			std::find(entry->listeners.begin(), entry->listeners.end(), p_completed) == entry->listeners.end()) {
			entry->listeners.push_back(p_completed);
		}
		return entry->sheet;
	}

	void shutdown_remote_stylesheets() { sheets.clear(); }
}
