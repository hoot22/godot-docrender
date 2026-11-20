#pragma once

#include "formats/html/dom/html_document_fragment.h"

class HTMLTemplateElement : public HTMLElement {
	GDCLASS(HTMLTemplateElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	godot::Ref<HTMLDocumentFragment> get_content() const;	// 返回模板持有的分离片段
};
