#ifndef LH_EL_BEFORE_AFTER_H
#define LH_EL_BEFORE_AFTER_H

#include "html_tag.h"

namespace litehtml
{
	class el_image;

	class el_before_after_base : public html_tag
	{
	public:
		el_before_after_base(const std::shared_ptr<document>& doc, string_id tag);
		void compute_styles(bool recursive) override;
	protected:
		void resolve_content(const css_generated_content& content, bool list_marker = false);
	private:
		// Keep generated nodes when only the viewport changes.
		using ContentPart = std::variant<string, css_image_ptr>;
		std::vector<ContentPart> materialized_content;
		std::vector<std::weak_ptr<el_image>> materialized_images;
		void add_text(const string& text);
		void add_image(const image& value, const string& base, bool list_marker);
	};

	class el_before : public el_before_after_base
	{
	public:
		explicit el_before(const std::shared_ptr<document>& doc) : el_before_after_base(doc, __tag_before_)
		{

		}
	};

	class el_after : public el_before_after_base
	{
	public:
		explicit el_after(const std::shared_ptr<document>& doc) : el_before_after_base(doc, __tag_after_)
		{

		}
	};
	class el_marker : public el_before_after_base
	{
	public:
		explicit el_marker(const std::shared_ptr<document>& doc);
		void compute_styles(bool recursive) override;
		void draw(uint_ptr, pixel_t, pixel_t, const position*, const std::shared_ptr<render_item>&) override {}
	};


}

#endif  // LH_EL_BEFORE_AFTER_H
