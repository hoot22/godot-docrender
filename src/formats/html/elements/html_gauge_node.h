#pragma once

#include <litehtml/html_tag.h>

namespace dockit {
	struct HTMLGaugeReading {
		double fraction;
		bool indeterminate;
		int zone;
	};

	HTMLGaugeReading read_html_gauge(const litehtml::element& p_node);

	class HTMLGaugeNode : public litehtml::html_tag {
	public:
		using litehtml::html_tag::html_tag;
		std::shared_ptr<litehtml::render_item> create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) override;
		void draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
			const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) override;
	};
}
