#pragma once

#include <godot_cpp/classes/editor_export_plugin.hpp>
#include <godot_cpp/templates/hash_set.hpp>

namespace godot {
class HTMLVideoExportPlugin : public EditorExportPlugin {
	GDCLASS(HTMLVideoExportPlugin, EditorExportPlugin)

	HashSet<String> included_media;

protected:
	static void _bind_methods() {}

public:
	String _get_name() const override;
	void _export_begin(const PackedStringArray&, bool, const String&, uint32_t) override;
	void _export_file(const String& p_path, const String& p_type, const PackedStringArray&) override;
	void _export_end() override;
};
}
