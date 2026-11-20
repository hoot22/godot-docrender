#pragma once
#include <vector>

#include "html_document_fragment.h"

class HTMLShadowRoot : public HTMLDocumentFragment {
	GDCLASS(HTMLShadowRoot, HTMLDocumentFragment)
	friend class dockit::HTMLDOM;
	std::weak_ptr<litehtml::element> host;
	godot::String mode;
	godot::String slot_assignment;
	bool delegates_focus = false;
	std::vector<std::weak_ptr<litehtml::element>> previous_slots;
protected:
	static void _bind_methods();
public:
	godot::Ref<HTMLElement> get_host() const;
	godot::String get_mode() const { return mode; }
	godot::String get_slot_assignment() const { return slot_assignment; }
	bool get_delegates_focus() const { return delegates_focus; }
};

class HTMLSlotElement : public HTMLElement {
	GDCLASS(HTMLSlotElement, HTMLElement)
protected:
	static void _bind_methods();
public:
	godot::String get_name() const { return get_attribute("name"); }
	void set_name(const godot::String& p_name) { set_attribute("name", p_name); }
	godot::TypedArray<HTMLNode> assigned_nodes(bool p_flatten = false) const;
	godot::TypedArray<HTMLElement> assigned_elements(bool p_flatten = false) const;
	godot::Error assign(const godot::Array& p_nodes);
};
