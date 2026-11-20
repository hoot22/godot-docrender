#include "html_video_export_plugin.h"

#include "formats/html/html_document.h"
#include "formats/html/resources/html_asset_collector.h"
#include "formats/html/resources/html_resource_paths.h"

#include <godot_cpp/classes/editor_export_platform.hpp>
#include <godot_cpp/classes/editor_export_preset.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

namespace godot {
String HTMLVideoExportPlugin::_get_name() const { return "Dockit HTML video"; }

void HTMLVideoExportPlugin::_export_begin(const PackedStringArray&, bool, const String&, uint32_t) {
	included_media.clear();
}

void HTMLVideoExportPlugin::_export_file(const String& p_path, const String& p_type, const PackedStringArray&) {
	if (p_type != "HTMLDocument") return;
	const Ref<HTMLDocument> document = ResourceLoader::get_singleton()->load(p_path, "HTMLDocument", ResourceLoader::CACHE_MODE_IGNORE);
	if (document.is_null()) return;

	Dictionary sources = document->get_video_sources();
	if (sources.is_empty() && document->get_text().containsn("<video")) {
		sources = dockit::collect_html_assets(document->get_text(), document->get_base_dir(), document->get_source_path()).video_sources;
	}
	const auto preset = get_export_preset();
	const auto exclusions = preset.is_valid() ? preset->get_exclude_filter().split(",", false) : PackedStringArray();
	const Array paths = sources.values();
	for (int64_t i = 0; i < paths.size(); ++i) {
		const String path = paths[i];
		if (!path.begins_with("res://") || dockit::normalize_html_resource_path(path) != path || included_media.has(path)) continue;
		bool excluded = false;
		for (const String& pattern : exclusions) {
			const String filter = pattern.strip_edges();
			if (path.matchn(filter) || path.trim_prefix("res://").matchn(filter)) {
				excluded = true;
				break;
			}
		}
		if (excluded) continue;
		const auto file = FileAccess::open(path, FileAccess::READ);
		if (file.is_null()) continue;
		const auto bytes = file->get_buffer(file->get_length());
		if (bytes.is_empty() || uint64_t(bytes.size()) != file->get_length()) continue;
		add_file(dockit::packed_html_video_path(path), bytes, false);
		included_media.insert(path);
	}
}

void HTMLVideoExportPlugin::_export_end() {
	included_media.clear();
}
}
