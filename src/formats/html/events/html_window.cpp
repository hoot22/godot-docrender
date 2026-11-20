#include "html_window.h"

#include "formats/html/events/html_event.h"
#include "formats/html/dom/html_dom.h"
#include "formats/html/html_layout.h"

using namespace godot;

void HTMLWindow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_document"), &HTMLWindow::get_document);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "document", PROPERTY_HINT_RESOURCE_TYPE, "HTMLDocumentNode",
		PROPERTY_USAGE_NONE), "", "get_document");
}

HTMLLayout* HTMLWindow::get_owner() const {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(layout_id));
	return layout && layout->parse_version == parse_version ? layout : nullptr;
}

bool HTMLWindow::is_valid() const {
	return get_owner() != nullptr;
}

Ref<HTMLDocumentNode> HTMLWindow::get_document() const {
	HTMLLayout* layout = get_owner();
	return layout ? layout->get_document_node() : Ref<HTMLDocumentNode>();
}

namespace {
	void read_event_options(const Dictionary& p_options, bool& r_capture, bool& r_once, bool& r_passive) {
		r_capture = p_options.get("capture", false);
		r_once = p_options.get("once", false);
		r_passive = p_options.get("passive", false);
	}
}

Error HTMLWindow::add_event_listener(const StringName& p_type, const Callable& p_callable, const Dictionary& p_options) {
	HTMLLayout* layout = get_owner();
	if (!layout) return ERR_INVALID_PARAMETER;
	bool capture = false;
	bool once = false;
	bool passive = false;
	read_event_options(p_options, capture, once, passive);
	if (!p_options.has("passive")) {
		passive = p_type == StringName("wheel") || p_type == StringName("touchstart") || p_type == StringName("touchmove");
	}
	return layout->get_dom().add_window_event_listener(p_type, p_callable, capture, once, passive);
}

Error HTMLWindow::remove_event_listener(const StringName& p_type, const Callable& p_callable,
	const Dictionary& p_options) {
	HTMLLayout* layout = get_owner();
	if (!layout) return ERR_INVALID_PARAMETER;
	const bool capture = p_options.get("capture", false);
	return layout->get_dom().remove_window_event_listener(p_type, p_callable, capture);
}

bool HTMLWindow::dispatch_event(const Ref<HTMLEvent>& p_event) {
	if (p_event.is_null()) return true;
	HTMLLayout* layout = get_owner();
	if (!layout) return !p_event->is_default_prevented();
	return layout->get_dom().dispatch_event(Ref<HTMLEventTarget>(this), p_event);
}

bool HTMLWindow::has_event_listener(const StringName& p_type) const {
	HTMLLayout* layout = get_owner();
	return layout ? layout->get_dom().has_window_event_listener(p_type) : false;
}
