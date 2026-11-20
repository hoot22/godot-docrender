#include "formats/html/html_component.h"
#include "formats/html/dom/html_shadow_root.h"
#include "register_types.h"

#include "editor/dockit_source_editor.h"
#include "editor/html_preview_editor_plugin.h"
#include "editor/importers/html_document_importer.h"
#include "editor/importers/latex_document_importer.h"
#include "editor/importers/mermaid_document_importer.h"
#include "editor/importers/pdf_texture_importer.h"
#include "editor/markdown_preview_editor_plugin.h"
#include "editor/mermaid_preview_editor_plugin.h"
#include "editor/script_editor_preview_plugin.h"
#include "formats/html/elements/html_anchor_element.h"
#include "formats/html/elements/html_button_element.h"
#include "formats/html/elements/html_element_types.h"
#include "formats/html/elements/html_metadata_elements.h"
#include "formats/html/elements/html_details_element.h"
#include "formats/html/elements/html_dialog_element.h"
#include "formats/html/elements/html_gauge_elements.h"
#include "formats/html/elements/html_heading_element.h"
#include "formats/html/elements/html_list_elements.h"
#include "formats/html/elements/html_form_elements.h"
#include "formats/html/elements/html_image_element.h"
#include "formats/html/elements/html_canvas_element.h"
#include "formats/html/elements/html_template_element.h"
#include "formats/html/elements/html_table_elements.h"
#include "formats/html/paint/html_canvas_renderer.h"
#include "formats/html/html_document.h"
#include "formats/html/html_print_layout.h"
#include "formats/html/dom/html_document_fragment.h"
#include "formats/html/dom/html_mutation_observer.h"
#include "formats/html/events/html_event.h"
#include "formats/html/events/html_event_target.h"
#include "formats/html/events/html_window.h"
#include "formats/html/css/html_animation.h"
#include "formats/html/css/html_style_sheet.h"
#include "formats/html/html_behavior.h"
#include "formats/remote_images.h"
#include "formats/http_requests.h"
#include "formats/html/resources/html_remote_stylesheets.h"
#include "formats/html/elements/html_audio_element.h"
#include "formats/html/html_ui_translations.h"
#include "formats/html/dom/html_selection.h"
#include "formats/latex/latex_document.h"
#include "formats/latex/latex_document_loader.h"
#include "formats/mermaid/mermaid_document.h"
#include "formats/pdf/reader/pdf_reader.h"
#include "formats/pdf/pdf_texture.h"
#include "formats/pdf/writer/pdf_writer.h"

#include "nodes/html_label.h"
#include "nodes/html_selection_drag_preview.h"
#include "nodes/latex_label.h"
#include "nodes/latex_label_3d.h"
#include "nodes/markdown_label.h"
#include "nodes/mermaid_label.h"
#include "formats/mermaid/mermaid_render.h"
#include "formats/latex/tex_render.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

namespace {
	bool editor_plugins_registered = false;
	Ref<DockitTexLoader> tex_loader;
}

void initialize_dockit_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_INTERNAL_CLASS(MarkdownLabelCanvas);
		GDREGISTER_CLASS(LatexDocument);
		GDREGISTER_CLASS(MermaidDocument);
		GDREGISTER_CLASS(PDFReader);
		GDREGISTER_CLASS(PDFWriter);
		GDREGISTER_CLASS(PDFTexture);
		GDREGISTER_INTERNAL_CLASS(DockitTexLoader);
		GDREGISTER_CLASS(MarkdownLabel);
		GDREGISTER_ABSTRACT_CLASS(HTMLEventTarget);
		GDREGISTER_CLASS(HTMLListenerHandle);
		GDREGISTER_ABSTRACT_CLASS(HTMLNode);
		GDREGISTER_ABSTRACT_CLASS(HTMLDocumentNode);
		GDREGISTER_ABSTRACT_CLASS(HTMLTextNode);
		GDREGISTER_ABSTRACT_CLASS(HTMLCommentNode);
		GDREGISTER_ABSTRACT_CLASS(HTMLRange);
		GDREGISTER_ABSTRACT_CLASS(HTMLSelection);
		GDREGISTER_CLASS(HTMLMutationObserver);
		GDREGISTER_CLASS(HTMLWindow);
		GDREGISTER_CLASS(HTMLEvent);
		GDREGISTER_CLASS(HTMLCustomEvent);
		GDREGISTER_CLASS(HTMLMouseEvent);
		GDREGISTER_CLASS(HTMLWheelEvent);
		GDREGISTER_CLASS(HTMLPointerEvent);
		GDREGISTER_CLASS(HTMLKeyboardEvent);
		GDREGISTER_CLASS(HTMLFocusEvent);
		GDREGISTER_CLASS(HTMLSubmitEvent);
		GDREGISTER_CLASS(HTMLInputEvent);
		GDREGISTER_CLASS(HTMLCompositionEvent);
		GDREGISTER_CLASS(HTMLDataTransfer);
		GDREGISTER_CLASS(HTMLDragEvent);
		GDREGISTER_CLASS(HTMLClipboardEvent);
		GDREGISTER_CLASS(HTMLToggleEvent);
		GDREGISTER_CLASS(HTMLMediaEvent);
		GDREGISTER_CLASS(HTMLErrorEvent);
		GDREGISTER_CLASS(HTMLAnimationEvent);
		GDREGISTER_CLASS(HTMLTransitionEvent);
		GDREGISTER_CLASS(HTMLTouchPoint);
		GDREGISTER_CLASS(HTMLTouchEvent);
		GDREGISTER_ABSTRACT_CLASS(HTMLAnimation);
		GDREGISTER_ABSTRACT_CLASS(HTMLStyleSheet);
		GDREGISTER_CLASS(HTMLBehaviorStorage);
		GDREGISTER_ABSTRACT_CLASS(HTMLElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLHtmlElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLHeadElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLBodyElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLTitleElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLMetaElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLBaseElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLScriptElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLLinkElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLStyleElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLAnchorElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLButtonElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLFormElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLLabelElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLFieldSetElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLLegendElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLSelectElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLOptionElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLOptGroupElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLDivElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLHeadingElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLImageElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLCanvasElement);
		GDREGISTER_CLASS(HTMLCanvasGradient);
		GDREGISTER_CLASS(HTMLCanvasPattern);
		GDREGISTER_CLASS(HTMLCanvasRenderingContext2D);
		GDREGISTER_ABSTRACT_CLASS(HTMLInputElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLInputFile);
		GDREGISTER_ABSTRACT_CLASS(HTMLParagraphElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLSpanElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLPreElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLDataListElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLOutputElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLDetailsElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLDialogCloseRequest);
		GDREGISTER_ABSTRACT_CLASS(HTMLDialogElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLMeterElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLProgressElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLAudioElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLVideoElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLHRElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLBRElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLQuoteElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLMenuElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLDListElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLSectionElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLArticleElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLOListElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLUListElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLLIElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLTextAreaElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLDocumentFragment);
		GDREGISTER_ABSTRACT_CLASS(HTMLTemplateElement);
		GDREGISTER_CLASS(HTMLShadowRoot);
		GDREGISTER_ABSTRACT_CLASS(HTMLSlotElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLTableCaptionElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLTableColElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLTableCellElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLTableRowElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLTableSectionElement);
		GDREGISTER_ABSTRACT_CLASS(HTMLTableElement);
		GDREGISTER_INTERNAL_CLASS(dockit::HTMLCanvasRenderer);
		GDREGISTER_CLASS(HTMLLayout);
		GDREGISTER_VIRTUAL_CLASS(HTMLBehavior);
		GDREGISTER_VIRTUAL_CLASS(HTMLComponent);
		GDREGISTER_CLASS(HTMLDocument);
		GDREGISTER_CLASS(HTMLPrintLayout);
		GDREGISTER_INTERNAL_CLASS(HTMLLabelCanvas);
		GDREGISTER_INTERNAL_CLASS(HTMLSelectionDragPreview);
		GDREGISTER_CLASS(HTMLLabel);
		GDREGISTER_CLASS(TeXRender);
		GDREGISTER_CLASS(LatexLabel);
		GDREGISTER_CLASS(LatexLabel3D);
		GDREGISTER_CLASS(MermaidRender);
		GDREGISTER_CLASS(MermaidLabel);

		tex_loader.instantiate();
		ResourceLoader::get_singleton()->add_resource_format_loader(tex_loader, true);
		dockit::initialize_html_ui_translations();
	}

	else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && Engine::get_singleton()->is_editor_hint()) {
		GDREGISTER_ABSTRACT_CLASS(ScriptEditorPreviewPlugin);
		GDREGISTER_INTERNAL_CLASS(HTMLPreviewEditorPlugin);
		GDREGISTER_INTERNAL_CLASS(MarkdownPreviewEditorPlugin);
		GDREGISTER_INTERNAL_CLASS(MermaidPreviewEditorPlugin);
		EditorPlugins::add_by_type<HTMLPreviewEditorPlugin>();
		EditorPlugins::add_by_type<MarkdownPreviewEditorPlugin>();
		EditorPlugins::add_by_type<MermaidPreviewEditorPlugin>();
		GDREGISTER_INTERNAL_CLASS(LatexDocumentImporter);
		GDREGISTER_INTERNAL_CLASS(HTMLDocumentImporter);
		GDREGISTER_INTERNAL_CLASS(HTMLVideoExportPlugin);
		GDREGISTER_INTERNAL_CLASS(MermaidDocumentImporter);
		if (PDFTextureImporter::is_supported()) {
			GDREGISTER_INTERNAL_CLASS(PDFTextureImporter);
		}
		GDREGISTER_INTERNAL_CLASS(DockitSourceEditorPlugin);
		DockitSourceEditorPlugin::prepare_registration();
		EditorPlugins::add_by_type<DockitSourceEditorPlugin>();
		editor_plugins_registered = true;
	}
}

void uninitialize_dockit_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && editor_plugins_registered) {
		EditorPlugins::remove_by_type<DockitSourceEditorPlugin>();
		EditorPlugins::remove_by_type<MermaidPreviewEditorPlugin>();
		EditorPlugins::remove_by_type<MarkdownPreviewEditorPlugin>();
		EditorPlugins::remove_by_type<HTMLPreviewEditorPlugin>();
		editor_plugins_registered = false;
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		dockit::shutdown_html_ui_translations();
		ResourceLoader::get_singleton()->remove_resource_format_loader(tex_loader);
		dockit::shutdown_http_requests();
		dockit::shutdown_remote_images();
		dockit::shutdown_remote_stylesheets();
		tex_loader.unref();
		PDFReader::shutdown_backend();
		TeXRender::shutdown_renderer();
		dockit::release_html_paint_shader();
	}
}

extern "C" {
	GDExtensionBool GDE_EXPORT dockit_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) {
		godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

		init_obj.register_initializer(initialize_dockit_module);
		init_obj.register_terminator(uninitialize_dockit_module);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
