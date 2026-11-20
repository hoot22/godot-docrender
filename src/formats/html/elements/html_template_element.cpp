#include "html_template_element.h"
#include "html_template_node.h"

using namespace godot;

void HTMLTemplateElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_content"), &HTMLTemplateElement::get_content);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "content", PROPERTY_HINT_NONE, "",
		PROPERTY_USAGE_DEFAULT, "HTMLDocumentFragment"), "", "get_content");
}

Ref<HTMLDocumentFragment> HTMLTemplateElement::get_content() const {
	const auto element = lock_element();
	const auto* node = dynamic_cast<const dockit::HTMLTemplateNode*>(element.get());
	return node ? node->get_content() : Ref<HTMLDocumentFragment>();
}
