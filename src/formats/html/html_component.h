#pragma once
#include "html_behavior.h"
#include "dom/html_shadow_root.h"

class HTMLComponent : public HTMLBehavior {
	GDCLASS(HTMLComponent, HTMLBehavior)
	friend class HTMLLabel;
	godot::Ref<HTMLElement> host;
	bool connected = false;
	GDVIRTUAL0(_mount);
	GDVIRTUAL0(_connected);
	GDVIRTUAL0(_disconnected);
	GDVIRTUAL0(_unmount);
protected:
	static void _bind_methods();
public:
	godot::Ref<HTMLElement> get_host() const { return host; }
	godot::Ref<HTMLShadowRoot> get_shadow_root() const;
};
