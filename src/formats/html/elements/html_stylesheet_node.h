#pragma once

#include <litehtml/html_tag.h>

#include <cstdint>

namespace dockit {
	class HTMLStylesheetNode : public litehtml::html_tag {
	public:
		using html_tag::html_tag;
		uint64_t source_version = 0;
		bool style_disabled = false;
		litehtml::string linked_request_path;
		litehtml::string linked_path;
		litehtml::string linked_text;

		bool accepts_style_text() const;
		void parse_attributes() override;
		void compute_styles(bool p_recursive) override;
		bool removeChild(const litehtml::element::ptr& p_child) override;
	};
}
