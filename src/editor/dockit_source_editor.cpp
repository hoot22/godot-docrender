#include "dockit_source_editor.h"

#include "formats/html/html_document.h"
#include "formats/latex/latex_document.h"
#include "formats/mermaid/mermaid_document.h"

#include <godot_cpp/classes/code_edit.hpp>
#include <godot_cpp/classes/editor_file_dialog.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_file_system_directory.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_toaster.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/file_system_dock.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script_editor.hpp>
#include <godot_cpp/classes/script_editor_base.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/weak_ref.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <mutex>

namespace godot {
	namespace {
		Ref<HTMLDocumentImporter> startup_html_importer;
		Ref<LatexDocumentImporter> startup_latex_importer;
		Ref<MermaidDocumentImporter> startup_mermaid_importer;
		Ref<PDFTextureImporter> startup_pdf_importer;
		std::mutex observed_documents_lock;
		HashMap<String, Ref<WeakRef>> observed_documents;

		StringName get_document_type(const String& p_path) {
			const String extension = p_path.get_extension().to_lower();
			if (extension == "tex") {
				return LatexDocument::get_class_static();
			}
			if (extension == "html" || extension == "htm") {
				return HTMLDocument::get_class_static();
			}
			if (extension == "mmd" || extension == "mermaid") {
				return MermaidDocument::get_class_static();
			}
			return {};
		}

		bool is_source_extension(const String& p_path) {
			return !get_document_type(p_path).is_empty();
		}

		bool is_imported_source(const String& p_path) {
			const StringName type = get_document_type(p_path);
			return p_path.begins_with("res://") && !type.is_empty() &&
				EditorInterface::get_singleton()->get_resource_filesystem()->get_file_type(p_path) == String(type);
		}

		CodeEdit* get_code_editor(ScriptEditorBase* p_tab) {
			return p_tab == nullptr ? nullptr : Object::cast_to<CodeEdit>(p_tab->get_base_editor());
		}

		void observe_document(const String& p_path, const Ref<Resource>& p_document) {
			const std::lock_guard lock(observed_documents_lock);
			const Ref<WeakRef> weak_document = UtilityFunctions::weakref(p_document);
			observed_documents.insert(p_path, weak_document);
		}

		Ref<Resource> get_displaced_document(const String& p_path) {
			const std::lock_guard lock(observed_documents_lock);
			const auto* weak_document = observed_documents.getptr(p_path);
			if (weak_document == nullptr) {
				return {};
			}
			const Ref<Resource> document = (*weak_document)->get_ref();
			if (document.is_null() || (!document->get_path().is_empty() && document->get_path() != p_path)) {
				observed_documents.erase(p_path);
				return {};
			}
			return document;
		}

		void restore_source_cache(const String& p_path, const Ref<Resource>& p_imported = {}) {
			const Ref<Resource> cached = ResourceLoader::get_singleton()->get_cached_ref(p_path);
			if (cached.is_valid() && cached->is_class("TextFile")) {
				const Ref<Resource> document = p_imported.is_valid() ? p_imported : get_displaced_document(p_path);
				// Keep the path on the editing buffer for native save/reload, without
				// registering it under the same key as the imported resource.
				cached->set_path("");
				cached->set_path_cache(p_path);
				if (document.is_valid()) {
					document->take_over_path(p_path);
				}
			}
		}

		void observe_cached_sources(EditorFileSystemDirectory* p_directory) {
			ResourceLoader* loader = ResourceLoader::get_singleton();
			for (int32_t index = 0; index < p_directory->get_file_count(); ++index) {
				const String path = p_directory->get_file_path(index);
				const StringName type = get_document_type(path);
				if (type.is_empty() || p_directory->get_file_type(index) != type) {
					continue;
				}
				const Ref<Resource> document = loader->get_cached_ref(path);
				if (document.is_valid() && document->get_class() == type) {
					observe_document(path, document);
				}
			}
			for (int32_t index = 0; index < p_directory->get_subdir_count(); ++index) {
				observe_cached_sources(p_directory->get_subdir(index));
			}
		}

		void collect_sources_for_import(EditorFileSystemDirectory* p_directory, PackedStringArray& r_paths) {
			for (int32_t index = 0; index < p_directory->get_file_count(); ++index) {
				const String path = p_directory->get_file_path(index);
				const StringName document_type = get_document_type(path);
				if (document_type.is_empty()) {
					continue;
				}
				const String sidecar = path + String(".import");
				if (!FileAccess::file_exists(sidecar)) {
					const StringName type = p_directory->get_file_type(index);
					if (type.is_empty() || type == document_type || type == StringName("TextFile") || type == StringName("OtherFile")) {
						r_paths.append(path);
					}
					continue;
				}
				Ref<ConfigFile> config;
				config.instantiate();
				if (config->load(sidecar) != OK) {
					continue;
				}
				const String importer = config->get_value("remap", "importer", "");
				const int64_t version = config->get_value("remap", "importer_version", 0);
				const bool outdated_html = importer == "dockit.html_document" &&
					version < HTMLDocumentImporter::FORMAT_VERSION;
				if (importer == "dockit.source" || outdated_html) {
					r_paths.append(path);
				}
			}
			for (int32_t index = 0; index < p_directory->get_subdir_count(); ++index) {
				collect_sources_for_import(p_directory->get_subdir(index), r_paths);
			}
		}
	} // namespace

	HashMap<uint64_t, String> DockitSourceEditorPlugin::source_tabs;

	void DockitSourceEditorPlugin::observe_source_document(const Ref<Resource>& p_document) {
		if (p_document.is_null() || !Engine::get_singleton()->is_editor_hint()) {
			return;
		}
		const String path = p_document->get_path();
		if (!path.begins_with("res://") || get_document_type(path) != p_document->get_class() ||
			ResourceLoader::get_singleton()->get_cached_ref(path) != p_document) {
			return;
		}
		observe_document(path, p_document);
	}

	String DockitSourceEditorPlugin::get_source_path(uint64_t p_tab_id) {
		const String* path = source_tabs.getptr(p_tab_id);
		return path != nullptr ? *path : String();
	}

	void DockitSourceEditorPlugin::prepare_registration() {
		const EditorInterface* editor = EditorInterface::get_singleton();
		if (editor == nullptr || editor->get_resource_filesystem() == nullptr) {
			return;
		}

		// Godot can discover this extension during its first scan. It defers
		// creating editor plugins, so register the importers before that scan continues.
		DockitSourceEditorPlugin* registrar = memnew(DockitSourceEditorPlugin);
		registrar->install_importers();
		startup_html_importer = registrar->html_importer;
		startup_latex_importer = registrar->latex_importer;
		startup_mermaid_importer = registrar->mermaid_importer;
		startup_pdf_importer = registrar->pdf_importer;
		memdelete(registrar);
	}

	void DockitSourceEditorPlugin::install_importers() {
		html_importer.instantiate();
		html_importer->set_translation_domain("godot.editor");
		add_import_plugin(html_importer);
		latex_importer.instantiate();
		mermaid_importer.instantiate();
		latex_importer->set_translation_domain("godot.editor");
		mermaid_importer->set_translation_domain("godot.editor");
		add_import_plugin(latex_importer);
		add_import_plugin(mermaid_importer);
		if (PDFTextureImporter::is_supported()) {
			pdf_importer.instantiate();
			pdf_importer->set_translation_domain("godot.editor");
			add_import_plugin(pdf_importer);
		}
	}

	void DockitSourceEditorPlugin::_enter_tree() {
		editor_translations.load();
		video_exporter.instantiate();
		add_export_plugin(video_exporter);
		if (startup_latex_importer.is_valid()) {
			html_importer = startup_html_importer;
			startup_html_importer.unref();
			latex_importer = startup_latex_importer;
			mermaid_importer = startup_mermaid_importer;
			pdf_importer = startup_pdf_importer;
			startup_latex_importer.unref();
			startup_mermaid_importer.unref();
			startup_pdf_importer.unref();
		} else {
			install_importers();
		}

		const Callable import_sources = callable_mp(this, &DockitSourceEditorPlugin::import_existing_sources);
		// Reimport opens a progress dialog, which requires a regular editor frame.
		get_tree()->connect("process_frame", import_sources, CONNECT_ONE_SHOT);

		const Dictionary version = Engine::get_singleton()->get_version_info();
		if (int64_t(version["major"]) == 4 && int64_t(version["minor"]) == 7 && int64_t(version["patch"]) == 2) {
			connect("scene_changed", callable_mp(this, &DockitSourceEditorPlugin::observe_restored_scene_sources), CONNECT_ONE_SHOT);
			callable_mp(this, &DockitSourceEditorPlugin::attach_source_editor).call_deferred();
		}
	}

	void DockitSourceEditorPlugin::observe_restored_scene_sources(Node* p_scene) {
		if (p_scene == nullptr) {
			return;
		}
		// Godot loads the open scenes before restoring ScriptEditor tabs. Retain
		// the identities of cached documents whose paths TextFile can take over.
		EditorFileSystemDirectory* directory = EditorInterface::get_singleton()->get_resource_filesystem()->get_filesystem();
		if (directory != nullptr) {
			observe_cached_sources(directory);
		}
	}

	void DockitSourceEditorPlugin::import_existing_sources() {
		if (!is_inside_tree()) {
			return;
		}
		EditorFileSystem* filesystem = EditorInterface::get_singleton()->get_resource_filesystem();
		if (filesystem->is_scanning()) {
			get_tree()->connect("process_frame", callable_mp(this, &DockitSourceEditorPlugin::import_existing_sources), CONNECT_ONE_SHOT);
			return;
		}

		// The filesystem cache may skip checking a changed importer when the
		// source and sidecar are unchanged. Reimport stale sources with their UIDs.
		PackedStringArray paths;
		collect_sources_for_import(filesystem->get_filesystem(), paths);
		if (!paths.is_empty()) {
			filesystem->reimport_files(paths);
		}
	}

	void DockitSourceEditorPlugin::_exit_tree() {
		detach_source_editor();
		remove_export_plugin(video_exporter);
		video_exporter.unref();
		{
			const std::lock_guard lock(observed_documents_lock);
			observed_documents.clear();
		}
		remove_import_plugin(html_importer);
		html_importer.unref();
		if (pdf_importer.is_valid()) {
			remove_import_plugin(pdf_importer);
			pdf_importer.unref();
		}
		remove_import_plugin(mermaid_importer);
		remove_import_plugin(latex_importer);
		mermaid_importer.unref();
		latex_importer.unref();
		editor_translations.unload();
	}

	void DockitSourceEditorPlugin::_set_window_layout(const Ref<ConfigFile>& p_configuration) {
		if (p_configuration.is_valid() && p_configuration->has_section_key("ScriptEditor", "open_scripts")) {
			const Array paths = p_configuration->get_value("ScriptEditor", "open_scripts");
			callable_mp(this, &DockitSourceEditorPlugin::restore_open_source_paths).call_deferred(paths);
		}
	}

	void DockitSourceEditorPlugin::attach_source_editor() {
		if (!is_inside_tree()) {
			return;
		}
		EditorInterface* editor = EditorInterface::get_singleton();
		ScriptEditor* scripts = editor->get_script_editor();
		FileSystemDock* dock = editor->get_file_system_dock();
		if (scripts == nullptr || dock == nullptr) {
			return;
		}

		// The private native callbacks are available through their connections
		// in Godot 4.7.2. Resolve them before changing any dock behavior.
		for (const Dictionary& connection : scripts->get_incoming_connections()) {
			const Signal signal = connection["signal"];
			Object* sender = signal.get_object();
			if (sender == nullptr || int64_t(connection["flags"]) != 0) {
				continue;
			}
			if (signal.get_name() == StringName("result_selected") && sender->is_class("FindInFiles")) {
				open_text_result = connection["callable"];
			} else if (signal.get_name() == StringName("file_selected")) {
				const EditorFileDialog* dialog = Object::cast_to<EditorFileDialog>(sender);
				if (dialog != nullptr && dialog->get_parent() == scripts) {
					file_selection = { signal, connection["callable"] };
				}
			} else if (signal.get_name() == StringName("item_selected") && Object::cast_to<ItemList>(sender) != nullptr) {
				script_list_id = sender->get_instance_id();
			} else if (signal.get_name() == StringName("tab_changed") && Object::cast_to<TabContainer>(sender) != nullptr) {
				script_tabs_id = sender->get_instance_id();
			}
		}
		for (const Dictionary& connection : dock->get_incoming_connections()) {
			const Signal signal = connection["signal"];
			if (signal.get_name() != StringName("item_activated") || int64_t(connection["flags"]) != 0) {
				continue;
			}
			if (Object::cast_to<Tree>(signal.get_object()) != nullptr) {
				tree_activation = { signal, connection["callable"] };
			} else if (Object::cast_to<ItemList>(signal.get_object()) != nullptr) {
				list_activation = { signal, connection["callable"] };
			}
		}

		if (!open_text_result.is_valid() || !file_selection.callback.is_valid() ||
			!tree_activation.callback.is_valid() || !list_activation.callback.is_valid() ||
			script_list_id == 0 || script_tabs_id == 0 || !scripts->has_method("close_file")) {
			return;
		}

		tree_activation.signal.disconnect(tree_activation.callback);
		tree_activation.signal.connect(callable_mp(this, &DockitSourceEditorPlugin::activate_tree_item));
		list_activation.signal.disconnect(list_activation.callback);
		list_activation.signal.connect(callable_mp(this, &DockitSourceEditorPlugin::activate_list_item));
		file_selection.signal.disconnect(file_selection.callback);
		file_selection.signal.connect(callable_mp(this, &DockitSourceEditorPlugin::select_file));
		ObjectDB::get_instance(script_tabs_id)->connect("child_order_changed",
			callable_mp(this, &DockitSourceEditorPlugin::sync_open_sources), CONNECT_DEFERRED);
		sync_open_sources();
	}

	void DockitSourceEditorPlugin::detach_source_editor() {
		auto restore = [](NativeConnection& p_connection, const Callable& p_hook) {
			if (p_connection.signal.get_object() != nullptr && p_connection.signal.is_connected(p_hook)) {
				p_connection.signal.disconnect(p_hook);
				p_connection.signal.connect(p_connection.callback);
			}
		};
		restore(tree_activation, callable_mp(this, &DockitSourceEditorPlugin::activate_tree_item));
		restore(list_activation, callable_mp(this, &DockitSourceEditorPlugin::activate_list_item));
		restore(file_selection, callable_mp(this, &DockitSourceEditorPlugin::select_file));
		if (Object* tabs = ObjectDB::get_instance(script_tabs_id)) {
			const Callable sync = callable_mp(this, &DockitSourceEditorPlugin::sync_open_sources);
			if (tabs->is_connected("child_order_changed", sync)) {
				tabs->disconnect("child_order_changed", sync);
			}
		}
		source_tabs.clear();
	}

	void DockitSourceEditorPlugin::activate_tree_item() {
		Tree* tree = Object::cast_to<Tree>(tree_activation.signal.get_object());
		TreeItem* item = tree == nullptr ? nullptr : tree->get_selected();
		if (item == nullptr || !open_imported_source(item->get_metadata(0))) {
			tree_activation.callback.call();
		}
	}

	void DockitSourceEditorPlugin::activate_list_item(int32_t p_index) {
		ItemList* list = Object::cast_to<ItemList>(list_activation.signal.get_object());
		if (list == nullptr || p_index < 0 || p_index >= list->get_item_count() ||
			!open_imported_source(list->get_item_metadata(p_index))) {
			list_activation.callback.call(p_index);
		}
	}

	bool DockitSourceEditorPlugin::open_imported_source(const Variant& p_metadata) {
		if (p_metadata.get_type() != Variant::STRING || !is_imported_source(p_metadata)) {
			return false;
		}
		callable_mp(this, &DockitSourceEditorPlugin::open_source_text).call_deferred(String(p_metadata));
		return true;
	}

	void DockitSourceEditorPlugin::open_source_text(const String& p_path) {
		if (!is_inside_tree() || !FileAccess::file_exists(p_path)) {
			return;
		}
		EditorInterface* editor = EditorInterface::get_singleton();
		ScriptEditor* scripts = editor->get_script_editor();
		ResourceLoader* loader = ResourceLoader::get_singleton();
		Ref<Resource> imported;
		if (is_imported_source(p_path)) {
			restore_source_cache(p_path);
			imported = loader->load(p_path, String(get_document_type(p_path)));
		}

		int32_t line = 0;
		int32_t column = 0;
		for (const KeyValue<uint64_t, String>& entry : source_tabs) {
			if (entry.value == p_path) {
				const CodeEdit* code = get_code_editor(Object::cast_to<ScriptEditorBase>(ObjectDB::get_instance(entry.key)));
				if (code != nullptr) {
					line = code->get_caret_line();
					column = code->get_caret_column();
				}
				break;
			}
		}
		ScriptEditorBase* previous = scripts->get_current_editor();
		editor->set_main_screen_editor("Script");
		open_text_result.call(p_path, line + 1, column, column);
		if (is_source_extension(p_path)) {
			ScriptEditorBase* current = scripts->get_current_editor();
			if (current != previous && current != nullptr && current->is_class("TextEditor")) {
				remember_source(current, p_path);
			}
			restore_source_cache(p_path, imported);
		}
	}

	void DockitSourceEditorPlugin::sync_open_sources() {
		ItemList* list = Object::cast_to<ItemList>(ObjectDB::get_instance(script_list_id));
		TabContainer* tabs = Object::cast_to<TabContainer>(ObjectDB::get_instance(script_tabs_id));
		if (!is_inside_tree() || list == nullptr || tabs == nullptr) {
			return;
		}
		for (int32_t i = 0; i < list->get_item_count(); ++i) {
			const Variant index = list->get_item_metadata(i);
			const String path = list->get_item_tooltip(i);
			if (index.get_type() != Variant::INT || int64_t(index) < 0 || int64_t(index) >= tabs->get_tab_count() ||
				!is_imported_source(path)) {
				continue;
			}
			ScriptEditorBase* tab = Object::cast_to<ScriptEditorBase>(tabs->get_tab_control(index));
			if (tab != nullptr && tab->is_class("TextEditor")) {
				remember_source(tab, path);
				restore_source_cache(path);
			}
		}
	}

	void DockitSourceEditorPlugin::restore_open_source_paths(const Array& p_paths) {
		if (!is_inside_tree() || !open_text_result.is_valid()) {
			return;
		}
		// Layout restoration can create TextFiles before ScriptEditor rebuilds
		// its visible list. The layout already contains their unfiltered paths.
		for (const Variant& entry : p_paths) {
			const String path = entry.get_type() == Variant::DICTIONARY ? String(Dictionary(entry).get("path", "")) : String(entry);
			if (is_imported_source(path)) {
				restore_source_cache(path);
			}
		}
		sync_open_sources();
	}

	void DockitSourceEditorPlugin::remember_source(ScriptEditorBase* p_tab, const String& p_path) {
		const uint64_t id = p_tab->get_instance_id();
		if (!source_tabs.has(id)) {
			p_tab->connect("tree_exiting", callable_mp(this, &DockitSourceEditorPlugin::forget_source).bind(id), CONNECT_ONE_SHOT);
		}
		source_tabs[id] = p_path;
	}

	void DockitSourceEditorPlugin::forget_source(uint64_t p_tab_id) {
		source_tabs.erase(p_tab_id);
	}

	void DockitSourceEditorPlugin::select_file(const String& p_path) {
		EditorInterface* editor = EditorInterface::get_singleton();
		ScriptEditor* scripts = editor->get_script_editor();
		const EditorFileDialog* dialog = Object::cast_to<EditorFileDialog>(file_selection.signal.get_object());
		ScriptEditorBase* tab = scripts->get_current_editor();
		CodeEdit* code = get_code_editor(tab);
		const String* original = tab == nullptr ? nullptr : source_tabs.getptr(tab->get_instance_id());
		// The raw title is the untranslated source string set by Godot's Save As
		// action. The same dialog also serves Open, New Text File and theme actions.
		if (dialog == nullptr || dialog->get_file_mode() != EditorFileDialog::FILE_MODE_SAVE_FILE ||
			dialog->get_title() != "Save File As..." || original == nullptr || code == nullptr) {
			file_selection.callback.call(p_path);
			return;
		}

		const String from = *original;
		const String to = ProjectSettings::get_singleton()->localize_path(p_path);
		if (from != to && PackedStringArray(scripts->call("get_unsaved_files")).has(to)) {
			editor->get_editor_toaster()->push_toast("The destination has unsaved changes in ScriptEditor.", EditorToaster::SEVERITY_ERROR);
			return;
		}
		const int32_t line = code->get_caret_line();
		const int32_t column = code->get_caret_column();
		const int32_t first_visible_line = code->get_first_visible_line();
		{
			const Ref<FileAccess> file = FileAccess::open(to, FileAccess::WRITE);
			if (file.is_null() || !file->store_string(code->get_text())) {
				editor->get_editor_toaster()->push_toast("Could not save " + to, EditorToaster::SEVERITY_ERROR);
				return;
			}
		}

		// Reopen at the new path so Godot tracks subsequent saves and disk
		// changes against the new file, including when both paths stay open.
		scripts->call("close_file", from);
		if (from != to) {
			scripts->call("close_file", to);
		}
		EditorFileSystem* filesystem = editor->get_resource_filesystem();
		filesystem->update_file(to);
		if (to.begins_with("res://") && is_source_extension(to) && !filesystem->is_scanning()) {
			PackedStringArray paths;
			paths.append(to);
			filesystem->reimport_files(paths);
		}
		open_source_text(to);
		if (CodeEdit* reopened = get_code_editor(scripts->get_current_editor())) {
			reopened->set_caret_line(line, false);
			reopened->set_caret_column(column, false);
			reopened->set_line_as_first_visible(first_visible_line);
		}
	}
} // namespace godot
