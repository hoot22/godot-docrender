#include "html_template_node.h"

#include "formats/html/dom/html_dom.h"

namespace dockit {
	HTMLTemplateNode::HTMLTemplateNode(const std::shared_ptr<litehtml::document>& p_document,
		const godot::Ref<HTMLDocumentFragment>& p_content) : litehtml::html_tag(p_document), content(p_content) {}

	bool HTMLTemplateNode::appendChild(const litehtml::element::ptr& p_child) {
		return content->detached_tree->root && content->detached_tree->root->appendChild(p_child);
	}
}
