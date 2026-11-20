#include "http_requests.h"
#include "http_url.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <deque>
#include <map>
#include <utility>

using namespace godot;

namespace dockit {
	namespace {
		constexpr int MAX_ACTIVE = 6;
		constexpr int MAX_REDIRECTS = 8;

		struct Request {
			uint64_t id = 0;
			String url;
			HTTPRequestOptions options;
			std::function<void(const HTTPResponse&)> complete;
			ObjectID request;
			int redirects = 0;
		};
		std::deque<Request> pending;
		std::map<uint64_t, Request> active;
		uint64_t next_id = 0;
		bool start_queued = false;
		bool stopping = false;

		void queue_start();

		void finish(std::map<uint64_t, Request>::iterator p_job, const HTTPResponse& p_response) {
			auto job = std::move(p_job->second);
			active.erase(p_job);
			if (auto* request = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(job.request))) request->queue_free();
			job.complete(p_response);
			queue_start();
		}

		void completed(int p_result, int p_code, const PackedStringArray& p_headers, const PackedByteArray& p_body, uint64_t p_id) {
			const auto entry = active.find(p_id);
			if (entry == active.end()) return;
			auto& job = entry->second;
			if (job.options.track_redirects && p_result == HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED &&
				job.redirects < MAX_REDIRECTS) {
				const String location = http_response_header(p_headers, "Location");
				const String next = location.is_empty() ? String() : resolve_http_url(location, job.url);
				auto* request = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(job.request));
				if (!next.is_empty() && request) {
					++job.redirects;
					job.url = http_request_url(next);
					if (request->request(job.url, job.options.headers, job.options.method, job.options.body) == OK) return;
				}
			}
			const bool transport_error = p_result != HTTPRequest::RESULT_SUCCESS;
			const bool status_error = p_code < 200 || p_code >= 300;
			finish(entry, {job.url, p_code, p_headers, p_body,
				transport_error ? String("request_failed") : status_error ? String("http_error") : String()});
		}

		void start_requests() {
			start_queued = false;
			auto* tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
			auto* root = tree ? tree->get_root() : nullptr;
			while (!stopping && active.size() < MAX_ACTIVE && !pending.empty()) {
				auto job = std::move(pending.front());
				pending.pop_front();
				if (!root || !root->is_inside_tree()) {
					job.complete({job.url, 0, {}, {}, "scene_tree_unavailable"});
					continue;
				}
				auto* request = memnew(HTTPRequest);
				request->set_use_threads(true);
				request->set_timeout(job.options.timeout);
				request->set_body_size_limit(job.options.max_body);
				request->set_max_redirects(job.options.track_redirects ? 0 : MAX_REDIRECTS);
				request->set_process_mode(Node::PROCESS_MODE_ALWAYS);
				job.request = ObjectID(request->get_instance_id());
				const uint64_t token = job.id;
				const auto entry = active.emplace(token, std::move(job)).first;
				request->connect("request_completed", callable_mp_static(&completed).bind(token));
				root->add_child(request, false, Node::INTERNAL_MODE_BACK);
				if (request->request(entry->second.url, entry->second.options.headers,
					entry->second.options.method, entry->second.options.body) != OK) {
					finish(entry, {entry->second.url, 0, {}, {}, "request_start_failed"});
				}
			}
		}

		void queue_start() {
			if (stopping || start_queued || active.size() >= MAX_ACTIVE || pending.empty()) return;
			start_queued = true;
			callable_mp_static(&start_requests).call_deferred();
		}
	}

	String http_response_header(const PackedStringArray& p_headers, const String& p_name) {
		for (const auto& header : p_headers) {
			const auto separator = header.find(":");
			if (separator >= 0 && header.left(separator).strip_edges().nocasecmp_to(p_name) == 0) {
				return header.substr(separator + 1).strip_edges();
			}
		}
		return {};
	}

	uint64_t queue_http_request(const String& p_url, const HTTPRequestOptions& p_options,
		std::function<void(const HTTPResponse&)> p_complete) {
		if (stopping) return 0;
		const uint64_t id = ++next_id;
		pending.push_back({id, p_url, p_options, std::move(p_complete), {}, 0});
		queue_start();
		return id;
	}

	uint64_t queue_http_request(const String& p_url, int p_max_body, bool p_track_redirects,
		std::function<void(const HTTPResponse&)> p_complete) {
		HTTPRequestOptions options;
		options.max_body = p_max_body;
		options.track_redirects = p_track_redirects;
		return queue_http_request(p_url, options, std::move(p_complete));
	}

	void cancel_http_request(uint64_t p_request_id) {
		if (p_request_id == 0) return;
		for (auto it = pending.begin(); it != pending.end(); ++it) {
			if (it->id == p_request_id) {
				pending.erase(it);
				return;
			}
		}
		const auto entry = active.find(p_request_id);
		if (entry == active.end()) return;
		const ObjectID request_id = entry->second.request;
		active.erase(entry);
		if (auto* request = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(request_id))) {
			request->disconnect("request_completed", callable_mp_static(&completed).bind(p_request_id));
			request->cancel_request();
			request->queue_free();
		}
		queue_start();
	}

	void shutdown_http_requests() {
		stopping = true;
		for (const auto& [id, job] : active) {
			if (auto* request = Object::cast_to<HTTPRequest>(ObjectDB::get_instance(job.request))) {
				request->disconnect("request_completed", callable_mp_static(&completed).bind(id));
				request->cancel_request();
				request->queue_free();
			}
		}
		active.clear();
		pending.clear();
	}
}
