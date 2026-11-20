#pragma once

#include <litehtml/html_tag.h>

namespace dockit {
	litehtml::element::ptr details_summary(const litehtml::element& p_details);
	litehtml::element::ptr details_activation_target(const litehtml::element::ptr& p_candidate);

	class HTMLDetailsNode : public litehtml::html_tag {
	public:
		using litehtml::html_tag::html_tag;
		std::shared_ptr<litehtml::render_item> create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) override;
	};
}
