#pragma once

#include <litehtml/html_tag.h>

namespace dockit {
	class HTMLRubyNode : public litehtml::html_tag {
	public:
		using litehtml::html_tag::html_tag;
		std::shared_ptr<litehtml::render_item> create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) override;
	};
}
