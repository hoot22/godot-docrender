#include "html_preview_editor_plugin.h"

#include "nodes/html_label.h"

#include <godot_cpp/classes/code_edit.hpp>
#include <godot_cpp/classes/input_event.hpp>

namespace godot {
	Control* HTMLPreviewEditorPlugin::create_preview_panel() {
		HTMLLabel* label = memnew(HTMLLabel);
		label->set_name("HTMLPreviewPanel");
		label->set_css_diagnostics_enabled(true);
		return label;
	}

	void HTMLPreviewEditorPlugin::update_preview(CodeEdit* p_source, Control* p_preview, const String& p_path) {
		auto* label = static_cast<HTMLLabel*>(p_preview);
		label->set_base_dir(p_path.get_base_dir());
		label->set_source_path(p_path);
		label->set_text(p_source->get_text());
	}
} // namespace godot
