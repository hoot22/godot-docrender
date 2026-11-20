#include "html_behavior.h"

#include "formats/http_requests.h"
#include "formats/http_url.h"

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

using namespace godot;

void HTMLBehaviorStorage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_item", "key"), &HTMLBehaviorStorage::has_item);
	ClassDB::bind_method(D_METHOD("get_item", "key"), &HTMLBehaviorStorage::get_item);
	ClassDB::bind_method(D_METHOD("set_item", "key", "value"), &HTMLBehaviorStorage::set_item);
	ClassDB::bind_method(D_METHOD("remove_item", "key"), &HTMLBehaviorStorage::remove_item);
	ClassDB::bind_method(D_METHOD("clear"), &HTMLBehaviorStorage::clear);
}

void HTMLBehaviorStorage::configure_persistent(const String& p_file_path) {
	file_path = p_file_path;
	session_values.clear();
}

void HTMLBehaviorStorage::configure_session() {
	file_path = String();
	session_values.clear();
}

Dictionary HTMLBehaviorStorage::load_values() const {
	if (file_path.is_empty()) return session_values;
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->load(file_path) != OK) return {};
	Dictionary values;
	for (const String& key : config->get_section_keys("values")) {
		const Variant value = config->get_value("values", key, String());
		if (value.get_type() == Variant::STRING) values[key] = value;
	}
	return values;
}

void HTMLBehaviorStorage::save_values(const Dictionary& p_values) {
	if (file_path.is_empty()) {
		session_values = p_values;
		return;
	}
	Ref<ConfigFile> config;
	config.instantiate();
	config->load(file_path);
	config->erase_section("values");
	for (const Variant& key : p_values.keys()) {
		config->set_value("values", String(key), String(p_values[key]));
	}
	const String directory = file_path.get_base_dir();
	const String absolute_directory = ProjectSettings::get_singleton()->globalize_path(directory);
	DirAccess::make_dir_recursive_absolute(absolute_directory);
	config->save(file_path);
}

bool HTMLBehaviorStorage::has_item(const String& p_key) const {
	if (file_path.is_empty()) return session_values.has(p_key);
	Ref<ConfigFile> config;
	config.instantiate();
	return config->load(file_path) == OK && config->has_section_key("values", p_key);
}

String HTMLBehaviorStorage::get_item(const String& p_key) const {
	const Dictionary values = load_values();
	const Variant value = values.get(p_key, String());
	return value.get_type() == Variant::STRING ? String(value) : String();
}

void HTMLBehaviorStorage::set_item(const String& p_key, const String& p_value) {
	if (p_key.is_empty()) return;
	Dictionary values = load_values();
	values[p_key] = p_value;
	save_values(values);
}

void HTMLBehaviorStorage::remove_item(const String& p_key) {
	Dictionary values = load_values();
	values.erase(p_key);
	save_values(values);
}

void HTMLBehaviorStorage::clear() {
	save_values({});
}

void HTMLBehavior::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_document"), &HTMLBehavior::get_document);
	ClassDB::bind_method(D_METHOD("get_label"), &HTMLBehavior::get_label);
	ClassDB::bind_method(D_METHOD("get_local_storage"), &HTMLBehavior::get_local_storage);
	ClassDB::bind_method(D_METHOD("get_session_storage"), &HTMLBehavior::get_session_storage);
	ClassDB::bind_method(D_METHOD("set_timeout", "callback", "delay"), &HTMLBehavior::set_timeout);
	ClassDB::bind_method(D_METHOD("set_interval", "callback", "interval"), &HTMLBehavior::set_interval);
	ClassDB::bind_method(D_METHOD("clear_timer", "id"), &HTMLBehavior::clear_timer);
	ClassDB::bind_method(D_METHOD("on_frame", "callback"), &HTMLBehavior::on_frame);
	ClassDB::bind_method(D_METHOD("cancel_frame", "id"), &HTMLBehavior::cancel_frame);
	ClassDB::bind_method(D_METHOD("request", "url", "options", "callback"), &HTMLBehavior::request);
	ClassDB::bind_method(D_METHOD("cancel_request", "id"), &HTMLBehavior::cancel_request);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "document", PROPERTY_HINT_RESOURCE_TYPE, "HTMLDocumentNode", PROPERTY_USAGE_NONE),
		"", "get_document");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "label", PROPERTY_HINT_NODE_TYPE, "HTMLLabel", PROPERTY_USAGE_NONE),
		"", "get_label");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "local_storage", PROPERTY_HINT_RESOURCE_TYPE, "HTMLBehaviorStorage", PROPERTY_USAGE_NONE),
		"", "get_local_storage");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "session_storage", PROPERTY_HINT_RESOURCE_TYPE, "HTMLBehaviorStorage", PROPERTY_USAGE_NONE),
		"", "get_session_storage");
	GDVIRTUAL_BIND(_attach_document, "document");
	GDVIRTUAL_BIND(_attach, "label");
	GDVIRTUAL_BIND(_detach);
}

HTMLBehavior::~HTMLBehavior() {
	cancel_async_work();
}

Ref<HTMLDocumentNode> HTMLBehavior::get_document() const {
	return document;
}

HTMLLabel* HTMLBehavior::get_label() const {
	return Object::cast_to<HTMLLabel>(ObjectDB::get_instance(label_id));
}

Ref<HTMLBehaviorStorage> HTMLBehavior::get_local_storage() const {
	return local_storage;
}

Ref<HTMLBehaviorStorage> HTMLBehavior::get_session_storage() const {
	return session_storage;
}

int64_t HTMLBehavior::set_timeout(const Callable& p_callback, double p_delay) {
	if (!p_callback.is_valid() || !std::isfinite(p_delay) || p_delay < 0.0 || !get_label()) return 0;
	const int64_t id = ++next_async_id;
	timers.emplace(id, TimerEntry{p_callback, p_delay, 0.0});
	notify_host_processing_changed();
	return id;
}

int64_t HTMLBehavior::set_interval(const Callable& p_callback, double p_interval) {
	if (!p_callback.is_valid() || !std::isfinite(p_interval) || p_interval < 0.0 || !get_label()) return 0;
	const int64_t id = ++next_async_id;
	const double interval = std::max(p_interval, 0.001);
	timers.emplace(id, TimerEntry{p_callback, interval, interval});
	notify_host_processing_changed();
	return id;
}

void HTMLBehavior::clear_timer(int64_t p_id) {
	timers.erase(p_id);
	notify_host_processing_changed();
}

int64_t HTMLBehavior::on_frame(const Callable& p_callback) {
	if (!p_callback.is_valid() || !get_label()) return 0;
	const int64_t id = ++next_async_id;
	frame_callbacks.emplace(id, p_callback);
	notify_host_processing_changed();
	return id;
}

void HTMLBehavior::cancel_frame(int64_t p_id) {
	frame_callbacks.erase(p_id);
	notify_host_processing_changed();
}

int64_t HTMLBehavior::request(const String& p_url, const Dictionary& p_options, const Callable& p_callback) {
	HTMLLabel* label = get_label();
	if (!label || !p_callback.is_valid()) return 0;

	String url = p_url.strip_edges();
	url = dockit::resolve_http_url(url, label->get_base_uri());
	if (!dockit::is_http_url(url)) return 0;

	dockit::HTTPRequestOptions options;
	const String method = String(p_options.get("method", "GET")).to_upper();
	if (method == "GET") options.method = HTTPClient::METHOD_GET;
	else if (method == "HEAD") options.method = HTTPClient::METHOD_HEAD;
	else if (method == "POST") options.method = HTTPClient::METHOD_POST;
	else if (method == "PUT") options.method = HTTPClient::METHOD_PUT;
	else if (method == "DELETE") options.method = HTTPClient::METHOD_DELETE;
	else if (method == "OPTIONS") options.method = HTTPClient::METHOD_OPTIONS;
	else if (method == "PATCH") options.method = HTTPClient::METHOD_PATCH;
	else return 0;

	const Variant headers = p_options.get("headers", Dictionary());
	if (headers.get_type() == Variant::DICTIONARY) {
		const Dictionary values = headers;
		for (const Variant& key : values.keys()) {
			options.headers.push_back(String(key) + ": " + String(values[key]));
		}
	}
	options.body = String(p_options.get("body", String()));
	options.max_body = CLAMP(int(p_options.get("max_body_size", options.max_body)), 1, 32 * 1024 * 1024);
	options.timeout = CLAMP(double(p_options.get("timeout", options.timeout)), 0.1, 120.0);
	options.track_redirects = bool(p_options.get("track_redirects", false));

	const ObjectID behavior_id(get_instance_id());
	const auto request_id_holder = std::make_shared<uint64_t>(0);
	const uint64_t request_id = dockit::queue_http_request(url, options,
		[behavior_id, request_id_holder, p_callback](const dockit::HTTPResponse& p_response) {
			auto* behavior = Object::cast_to<HTMLBehavior>(ObjectDB::get_instance(behavior_id));
			if (behavior) behavior->complete_request(*request_id_holder, p_callback,
				p_response.url, p_response.status_code, p_response.headers, p_response.body, p_response.error);
		});
	if (request_id == 0) return 0;
	*request_id_holder = request_id;
	requests.emplace(request_id, p_callback);
	return static_cast<int64_t>(request_id);
}

void HTMLBehavior::cancel_request(int64_t p_id) {
	const uint64_t request_id = static_cast<uint64_t>(p_id);
	if (!requests.erase(request_id)) return;
	dockit::cancel_http_request(request_id);
}

void HTMLBehavior::process_async(double p_delta) {
	if (label_id.is_null()) return;
	std::vector<int64_t> timer_ids;
	timer_ids.reserve(timers.size());
	for (const auto& [id, timer] : timers) timer_ids.push_back(id);
	for (const int64_t id : timer_ids) {
		auto entry = timers.find(id);
		if (entry == timers.end()) continue;
		entry->second.remaining -= p_delta;
		if (entry->second.remaining > 0.0) continue;
		const Callable callback = entry->second.callback;
		const double interval = entry->second.interval;
		if (interval == 0.0) timers.erase(entry);
		else {
			entry->second.remaining += interval;
			if (entry->second.remaining <= 0.0) entry->second.remaining = interval;
		}
		callback.call();
		if (!ObjectDB::get_instance(get_instance_id())) return;
	}

	std::vector<int64_t> frame_ids;
	frame_ids.reserve(frame_callbacks.size());
	for (const auto& [id, callback] : frame_callbacks) frame_ids.push_back(id);
	for (const int64_t id : frame_ids) {
		const auto entry = frame_callbacks.find(id);
		if (entry == frame_callbacks.end()) continue;
		const Callable callback = entry->second;
		callback.call(p_delta);
		if (!ObjectDB::get_instance(get_instance_id())) return;
	}
	notify_host_processing_changed();
}

void HTMLBehavior::cancel_async_work() {
	for (const auto& [id, callback] : requests) dockit::cancel_http_request(id);
	requests.clear();
	timers.clear();
	frame_callbacks.clear();
	notify_host_processing_changed();
}

bool HTMLBehavior::has_async_work() const {
	return !timers.empty() || !frame_callbacks.empty();
}

void HTMLBehavior::notify_host_processing_changed() {
	if (HTMLLabel* label = get_label()) label->update_internal_processing();
}

void HTMLBehavior::complete_request(uint64_t p_request_id, const Callable& p_callback,
	const String& p_url, int p_status_code, const PackedStringArray& p_headers,
	const PackedByteArray& p_body, const String& p_error) {
	if (!requests.erase(p_request_id) || !get_label()) return;
	Dictionary response;
	response["url"] = p_url;
	response["status_code"] = p_status_code;
	response["headers"] = p_headers;
	response["body"] = p_body;
	response["error"] = p_error;
	p_callback.call(response);
	if (ObjectDB::get_instance(get_instance_id())) notify_host_processing_changed();
}
