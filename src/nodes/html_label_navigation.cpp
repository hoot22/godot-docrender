#include "html_label.h"

#include "formats/http_requests.h"
#include "formats/http_url.h"
#include "formats/html/resources/html_resource_paths.h"
#include "editor/dockit_source_editor.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <utility>

using namespace godot;

Error HTMLLabel::navigate(const String& p_url, bool p_replace) {
	const String reference = p_url.strip_edges();
	if (reference.is_empty()) return ERR_INVALID_PARAMETER;

	String resolved;
	if (reference.begins_with("res://") || reference.begins_with("user://") || reference.begins_with("uid://")) {
		const dockit::HTMLResourceBase local_base{ "res://", "res://" };
		resolved = dockit::resolve_html_link_href(reference, local_base);
	}
	else if (dockit::is_http_url(reference) || dockit::is_http_url(current_location)) {
		const String base = current_location.is_empty() ? get_base_uri() : current_location;
		resolved = dockit::resolve_http_url(reference, base);
	}
	else {
		resolved = layout->resolve_link_href(reference);
	}
	if (resolved.is_empty()) return ERR_INVALID_PARAMETER;

	const String target_without_fragment = resolved.get_slice("#", 0);
	const String current_reference = current_location.is_empty() ? get_base_uri() : current_location;
	const String current_without_fragment = current_reference.get_slice("#", 0);
	if (!current_without_fragment.is_empty() && target_without_fragment == current_without_fragment) {
		const int64_t fragment = resolved.find("#");
		if (fragment < 0 || !scroll_to_anchor(resolved.substr(fragment + 1))) return ERR_DOES_NOT_EXIST;
		current_location = resolved;
		if (navigation_position >= 0 && navigation_position < static_cast<int64_t>(navigation_history.size()))
			navigation_history[navigation_position].location = resolved;
		emit_signal("location_changed", current_location);
		return OK;
	}
	if (navigation_request_id != 0) {
		dockit::cancel_http_request(navigation_request_id);
		navigation_request_id = 0;
	}
	const uint64_t sequence = ++navigation_sequence;

	if (dockit::is_http_url(resolved)) {
		const ObjectID label_id(get_instance_id());
		const String request_url = dockit::http_request_url(resolved);
		dockit::HTTPRequestOptions options;
		options.max_body = 8 * 1024 * 1024;
		options.timeout = 12.0;
		navigation_request_id = dockit::queue_http_request(request_url, options,
			[label_id, sequence, resolved, p_replace](const dockit::HTTPResponse& p_response) {
				auto* label = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(label_id));
				if (label) label->on_navigation_response(sequence, resolved, p_replace, p_response.status_code,
					p_response.body, p_response.error);
			});
		return navigation_request_id == 0 ? ERR_UNAVAILABLE : OK;
	}

	const int64_t query = resolved.find("?");
	const int64_t fragment = resolved.find("#");
	const int64_t suffix = query < 0 ? fragment : fragment < 0 ? query : std::min(query, fragment);
	const String path_with_uid = suffix < 0 ? resolved : resolved.left(suffix);
	const auto normalized = dockit::normalize_html_resource_path(path_with_uid);
	if (!normalized) return ERR_INVALID_PARAMETER;
	const String path = *normalized;
	const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		UtilityFunctions::push_warning(String("HTMLLabel navigation could not read '") + path + "'.");
		emit_signal("navigation_failed", resolved, "file_not_found");
		return FileAccess::get_open_error();
	}
	const String html = file->get_as_text();
	apply_navigation_source(resolved, html, p_replace);
	return OK;
}

void HTMLLabel::on_navigation_response(uint64_t p_sequence, const String& p_url, bool p_replace,
	int p_status_code, const PackedByteArray& p_body, const String& p_error) {
	if (p_sequence != navigation_sequence) return;
	navigation_request_id = 0;
	if (!p_error.is_empty() || p_status_code < 200 || p_status_code >= 300) {
		const String error = p_error.is_empty() ? String("http_error") : p_error;
		UtilityFunctions::push_warning(String("HTMLLabel navigation failed for '") + p_url + "': " + error);
		emit_signal("navigation_failed", p_url, error);
		return;
	}
	String html;
	if (!p_body.is_empty() && html.parse_utf8(reinterpret_cast<const char*>(p_body.ptr()), p_body.size()) != OK) {
		UtilityFunctions::push_warning(String("HTMLLabel navigation received invalid UTF-8 from '") + p_url + "'.");
		emit_signal("navigation_failed", p_url, "invalid_utf8");
		return;
	}
	apply_navigation_source(p_url, html, p_replace);
}

void HTMLLabel::apply_navigation_source(const String& p_url, const String& p_text, bool p_replace) {
	if (navigation_history.empty()) {
		navigation_history.push_back({ current_location, text, document, base_dir, source_path });
		navigation_position = 0;
	}

	String source = p_url;
	const int64_t suffix_fragment = source.find("#");
	if (suffix_fragment >= 0) source = source.left(suffix_fragment);
	if (!dockit::is_http_url(source)) {
		const int64_t suffix_query = source.find("?");
		if (suffix_query >= 0) source = source.left(suffix_query);
	}

	NavigationEntry entry{ p_url, p_text, {}, base_dir, source };
	if (!dockit::is_http_url(source)) entry.base_dir = source.get_base_dir();
	else if (entry.base_dir.is_empty()) entry.base_dir = "res://";

	if (p_replace && navigation_position >= 0) {
		navigation_history[navigation_position] = entry;
	}
	else {
		navigation_history.erase(navigation_history.begin() + navigation_position + 1, navigation_history.end());
		navigation_history.push_back(entry);
		navigation_position = static_cast<int64_t>(navigation_history.size()) - 1;
	}

	if (document.is_valid()) {
		document->disconnect("changed", callable_mp(this, &HTMLLabel::document_changed).bind(document->get_instance_id()));
	}
	document.unref();
	text = p_text;
	base_dir = entry.base_dir;
	source_path = entry.source_path;
	current_location = p_url;
	const int64_t fragment = current_location.find("#");
	pending_navigation_anchor = fragment < 0 ? String() : current_location.substr(fragment + 1);
	source_changed();
	emit_signal("location_changed", current_location);
}

void HTMLLabel::navigate_to_history(int64_t p_position) {
	if (p_position < 0 || p_position >= static_cast<int64_t>(navigation_history.size()) || p_position == navigation_position) return;
	if (navigation_request_id != 0) {
		dockit::cancel_http_request(navigation_request_id);
		navigation_request_id = 0;
		++navigation_sequence;
	}
	const NavigationEntry entry = navigation_history[p_position];
	navigation_position = p_position;
	if (document.is_valid()) {
		document->disconnect("changed", callable_mp(this, &HTMLLabel::document_changed).bind(document->get_instance_id()));
	}
	document = entry.document;
	if (document.is_valid()) {
		DockitSourceEditorPlugin::observe_source_document(document);
		document->connect("changed", callable_mp(this, &HTMLLabel::document_changed).bind(document->get_instance_id()));
	}
	text = entry.text;
	base_dir = entry.base_dir;
	source_path = entry.source_path;
	current_location = entry.location;
	const int64_t fragment = current_location.find("#");
	pending_navigation_anchor = fragment < 0 ? String() : current_location.substr(fragment + 1);
	source_changed();
	emit_signal("location_changed", current_location);
}

bool HTMLLabel::back() {
	if (navigation_position <= 0) return false;
	navigate_to_history(navigation_position - 1);
	return true;
}

bool HTMLLabel::forward() {
	if (navigation_position < 0 || navigation_position + 1 >= static_cast<int64_t>(navigation_history.size())) return false;
	navigate_to_history(navigation_position + 1);
	return true;
}

bool HTMLLabel::go(int64_t p_delta) {
	const int64_t target = navigation_position + p_delta;
	if (target < 0 || target >= static_cast<int64_t>(navigation_history.size()) || target == navigation_position) return false;
	navigate_to_history(target);
	return true;
}

String HTMLLabel::get_location() const {
	return current_location.is_empty() ? get_base_uri() : current_location;
}
