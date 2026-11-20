#include "html_details_element.h"
#include "html_details_node.h"

#include "formats/html/events/html_event.h"

#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <litehtml/render_item.h>

#include <algorithm>

using namespace godot;
using namespace litehtml;

void HTMLDetailsElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_open"), &HTMLDetailsElement::is_open);
	ClassDB::bind_method(D_METHOD("set_open", "open"), &HTMLDetailsElement::set_open);
	ClassDB::bind_method(D_METHOD("get_name"), &HTMLDetailsElement::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &HTMLDetailsElement::set_name);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "open"), "set_open", "is_open");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ADD_SIGNAL(MethodInfo("toggled", PropertyInfo(Variant::BOOL, "open")));
}

bool HTMLDetailsElement::is_open() const { return has_attribute("open"); }

void HTMLDetailsElement::set_open(bool p_open) {
	const bool was_open = is_open();
	if (was_open == p_open) return;
	const Ref<HTMLDetailsElement> keep_alive(this);
	Ref<HTMLToggleEvent> event;
	event.instantiate();
	event->set_type(StringName("beforetoggle"));
	event->set_bubbles(true);
	event->set_cancelable(true);
	event->set_composed(true);
	event->set_old_state(was_open ? "open" : "closed");
	event->set_new_state(p_open ? "open" : "closed");
	event->mark_trusted();
	dispatch_event(event);
	if (event->is_default_prevented() || !is_valid() || is_open() != was_open) return;
	if (p_open) set_attribute("open", "");
	else remove_attribute("open");
}

String HTMLDetailsElement::get_name() const { return get_attribute("name"); }
void HTMLDetailsElement::set_name(const String& p_name) { set_attribute("name", p_name); }

void HTMLDetailsElement::queue_toggle(bool p_old_open) {
	if (!toggle_pending) toggle_old_open = p_old_open;
	toggle_new_open = is_open();
	if (toggle_pending) return;
	toggle_pending = true;
	callable_mp(this, &HTMLDetailsElement::emit_toggle).call_deferred();
}

void HTMLDetailsElement::emit_toggle() {
	toggle_pending = false;
	if (!is_valid()) return;
	Ref<HTMLToggleEvent> event;
	event.instantiate();
	event->set_type(StringName("toggle"));
	event->set_bubbles(false);
	event->set_composed(true);
	event->set_old_state(toggle_old_open ? "open" : "closed");
	event->set_new_state(toggle_new_open ? "open" : "closed");
	event->mark_trusted();
	const Ref<HTMLDetailsElement> keep_alive(this);
	dispatch_event(event);
	if (is_valid()) emit_signal("toggled", is_open());
}

namespace dockit {
element::ptr details_summary(const element& p_details) {
	const auto& children = p_details.children();
	const auto first = std::find_if(children.begin(), children.end(), [](const auto& child) { return child->tag() == _summary_; });
	return first == children.end() ? element::ptr() : *first;
}

element::ptr details_activation_target(const element::ptr& p_candidate) {
	if (!p_candidate) return {};
	if (p_candidate->tag() == _details_ && !details_summary(*p_candidate)) return p_candidate;
	if (p_candidate->tag() != _summary_) return {};
	const auto parent = p_candidate->parent();
	return parent && parent->tag() == _details_ && details_summary(*parent) == p_candidate ? parent : element::ptr();
}

std::shared_ptr<render_item> HTMLDetailsNode::create_render_item(const std::shared_ptr<render_item>& p_parent) {
	if (get_attr("open")) return html_tag::create_render_item(p_parent);
	const auto summary = details_summary(*this);
	elements_list original;
	original.swap(m_children);
	for (const auto& child : original) {
		if (child == summary || is_generated_tag(child->tag())) m_children.push_back(child);
	}
	auto item = html_tag::create_render_item(p_parent);
	original.swap(m_children);
	return item;
}
}
