#pragma once

#include "formats/html/dom/html_document_fragment.h"

#include <litehtml/html_tag.h>

namespace dockit {
	class HTMLTemplateNode final : public litehtml::html_tag {
		friend class HTMLDOM;
		godot::Ref<HTMLDocumentFragment> content;

	public:
		HTMLTemplateNode(const std::shared_ptr<litehtml::document>& p_document,
			const godot::Ref<HTMLDocumentFragment>& p_content);
		const godot::Ref<HTMLDocumentFragment>& get_content() const { return content; }
		// Gumbo's parsed children belong to the fragment, outside the ordinary DOM tree.
		bool appendChild(const litehtml::element::ptr& p_child) override;
	};
}
