#include "http_url.h"

#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace dockit {
	namespace {
		int64_t suffix_start(const String& p_url) {
			const auto query = p_url.find("?");
			const auto fragment = p_url.find("#");
			return query >= 0 && (fragment < 0 || query < fragment) ? query : fragment;
		}

		String without_suffix(const String& p_url) {
			const auto suffix = suffix_start(p_url);
			return suffix < 0 ? p_url : p_url.left(suffix);
		}

		String normalize_path(const String& p_path) {
			PackedStringArray result;
			const auto parts = p_path.substr(1).split("/", true);
			for (int64_t i = 0; i < parts.size(); ++i) {
				const auto& part = parts[i];
				const auto dots = part.to_lower().replace("%2e", ".");
				if (dots == "." || dots == "..") {
					if (dots == ".." && !result.is_empty()) result.remove_at(result.size() - 1);
					if (i == parts.size() - 1) result.append("");
				} else {
					result.append(part);
				}
			}
			return "/" + String("/").join(result);
		}
	}

	bool is_http_url(const String& p_url) {
		const String prefix = p_url.left(8).to_lower();
		return prefix.begins_with("http://") || prefix.begins_with("https://");
	}

	String http_request_url(const String& p_url) {
		const auto fragment = p_url.find("#");
		const auto scheme = p_url.find(":");
		return p_url.left(scheme).to_lower() + p_url.substr(scheme, fragment < 0 ? -1 : fragment - scheme);
	}

	String resolve_http_url(const String& p_reference, const String& p_base) {
		const String reference = p_reference.strip_edges();
		if (reference.contains("\n") || reference.contains("\r") || reference.contains("\t")) return {};
		const String reference_path = without_suffix(reference);
		const String suffix = reference.substr(reference_path.length());
		String address;
		if (is_http_url(reference)) {
			address = reference_path;
		} else {
			if (!is_http_url(p_base) || reference_path.get_slice("/", 0).contains(":")) return {};
			const String base = without_suffix(p_base);
			const auto scheme = base.find("://");
			const auto path = base.find("/", scheme + 3);
			const String origin = path < 0 ? base : base.left(path);
			if (reference_path.begins_with("//")) {
				address = base.left(scheme + 1) + reference_path;
			} else if (reference_path.begins_with("/")) {
				address = origin + reference_path;
			} else if (reference_path.is_empty()) {
				if (suffix.begins_with("?")) return base + suffix;
				const auto fragment = p_base.find("#");
				return (fragment < 0 ? p_base : p_base.left(fragment)) + suffix;
			} else {
				address = (path < 0 ? origin + "/" : base.left(base.rfind("/") + 1)) + reference_path;
			}
		}
		const auto scheme = address.find("://");
		const auto path = address.find("/", scheme + 3);
		const auto authority = address.substr(scheme + 3, path < 0 ? -1 : path - scheme - 3);
		if (authority.is_empty() || authority.contains("@") || authority.contains(" ") || address.contains("\\")) return {};
		return address.left(scheme).to_lower() + "://" + authority.to_lower() +
			(path < 0 ? String("/") : normalize_path(address.substr(path))) + suffix;
	}
}
