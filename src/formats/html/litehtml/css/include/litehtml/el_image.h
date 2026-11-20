#ifndef LH_EL_IMAGE_H
#define LH_EL_IMAGE_H

#include "html_tag.h"
#include "document.h"
#include "types.h"
#include "formats/html/resources/html_image_sources.h"

namespace dockit { class HTMLImageAltText; }

namespace litehtml
{
	background_layer make_replaced_object_layer(html_tag& p_owner, const position& p_box, render_item& p_item);
	class css_replaced_image {
		background source;
	public:
		void set(const image& value, const string& base);
		size natural_size(document_container& container) const;
		void draw(html_tag& owner, uint_ptr hdc, const position& box, render_item& render);
	};

	class el_image : public html_tag
	{
		string	m_src;
		string	m_baseurl;
		string m_current_src;
		std::optional<dockit::HTMLImageSelection> m_selection;
		std::shared_ptr<dockit::HTMLImageAltText> m_alt_text;
		std::unique_ptr<css_replaced_image> m_generated_image;
		bool m_list_marker = false;
		const string& image_source() const;
	public:
		el_image(const document::ptr& doc, const string& baseurl = {});
		void set_generated_image(const image& value, const string& base, bool list_marker); // 更新匿名图片的 CSS 来源
		bool select_source(dockit::HTMLImageSelection p_selection); // 更新本轮选图并报告尺寸提示是否变化
		void commit_source(); // 发布最近一次成功排版的图片来源
		const string& get_current_src() const { return m_current_src; }
		dockit::HTMLImageAltText* get_alt_text() const { return m_alt_text.get(); }

		bool	is_replaced() const override;
		void	parse_attributes() override;
		void	compute_styles(bool recursive = true) override;
		void	draw(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip, const std::shared_ptr<render_item> &ri) override;
		void	get_content_size(size& sz, pixel_t max_width) override;
		string	dump_get_name() override;

		std::shared_ptr<render_item> create_render_item(const std::shared_ptr<render_item>& parent_ri) override;

	};
}

#endif  // LH_EL_IMAGE_H
