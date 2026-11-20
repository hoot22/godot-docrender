#include "html_component.h"
#include "dom/html_dom.h"
using namespace godot;
void HTMLComponent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_host"), &HTMLComponent::get_host);
	ClassDB::bind_method(D_METHOD("get_shadow_root"), &HTMLComponent::get_shadow_root);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "host", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE, "HTMLElement"), "", "get_host");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shadow_root", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE, "HTMLShadowRoot"), "", "get_shadow_root");
	GDVIRTUAL_BIND(_mount);
	GDVIRTUAL_BIND(_connected);
	GDVIRTUAL_BIND(_disconnected);
	GDVIRTUAL_BIND(_unmount);
}
Ref<HTMLShadowRoot> HTMLComponent::get_shadow_root() const {
	return host.is_valid() && host->is_valid() ? host->get_dom().get_shadow_root(host->lock_element(), false) : Ref<HTMLShadowRoot>();
}
