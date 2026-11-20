#pragma once

#include "formats/html/dom/html_node.h"
#include "nodes/html_label.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/gdvirtual.gen.inc>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <cstdint>
#include <map>
#include <vector>

class HTMLBehaviorStorage : public godot::RefCounted {
	GDCLASS(HTMLBehaviorStorage, godot::RefCounted)

	friend class HTMLLabel;
	friend class HTMLBehavior;
	godot::String file_path;
	godot::Dictionary session_values;

	void configure_persistent(const godot::String& p_file_path);
	void configure_session();
	godot::Dictionary load_values() const;
	void save_values(const godot::Dictionary& p_values);

protected:
	static void _bind_methods();

public:
	bool has_item(const godot::String& p_key) const;
	godot::String get_item(const godot::String& p_key) const;
	void set_item(const godot::String& p_key, const godot::String& p_value);
	void remove_item(const godot::String& p_key);
	void clear();
};

class HTMLBehavior : public godot::RefCounted {
	GDCLASS(HTMLBehavior, godot::RefCounted)

	friend class HTMLLabel;
	godot::Ref<HTMLDocumentNode> document;
	godot::ObjectID label_id;
	godot::Ref<HTMLBehaviorStorage> local_storage;
	godot::Ref<HTMLBehaviorStorage> session_storage;
	struct TimerEntry {
		godot::Callable callback;
		double remaining = 0.0;
		double interval = 0.0;
	};
	std::map<int64_t, TimerEntry> timers;
	std::map<int64_t, godot::Callable> frame_callbacks;
	std::map<uint64_t, godot::Callable> requests;
	int64_t next_async_id = 0;

	GDVIRTUAL1(_attach_document, godot::Ref<HTMLDocumentNode>);
	GDVIRTUAL1(_attach, HTMLLabel *);
	GDVIRTUAL0(_detach);
	void process_async(double p_delta);
	void cancel_async_work();
	bool has_async_work() const;
	void notify_host_processing_changed();
	void complete_request(uint64_t p_request_id, const godot::Callable& p_callback,
		const godot::String& p_url, int p_status_code, const godot::PackedStringArray& p_headers,
		const godot::PackedByteArray& p_body, const godot::String& p_error);

protected:
	static void _bind_methods();

public:
	~HTMLBehavior() override;
	godot::Ref<HTMLDocumentNode> get_document() const; // 返回当前挂载的实时文档节点
	HTMLLabel* get_label() const;
	godot::Ref<HTMLBehaviorStorage> get_local_storage() const;
	godot::Ref<HTMLBehaviorStorage> get_session_storage() const;
	int64_t set_timeout(const godot::Callable& p_callback, double p_delay);
	int64_t set_interval(const godot::Callable& p_callback, double p_interval);
	void clear_timer(int64_t p_id);
	int64_t on_frame(const godot::Callable& p_callback);
	void cancel_frame(int64_t p_id);
	int64_t request(const godot::String& p_url, const godot::Dictionary& p_options,
		const godot::Callable& p_callback);
	void cancel_request(int64_t p_id);
};
