#ifndef LH_HTML_TAG_H
#define LH_HTML_TAG_H

#include "element.h"
#include "style.h"
#include "background.h"
#include "css_selector.h"
#include "stylesheet.h"
#include "line_box.h"
#include "table.h"

namespace litehtml
{
	class css_replaced_image;

	class html_tag : public element
	{
		friend struct dockit_editor;
		friend struct selector_dom;
		friend class document;
		friend class elements_iterator;
		friend class el_table;
		friend class table_grid;
		friend class line_box;
	public:
		typedef shared_ptr<html_tag>	ptr;
	protected:
		string_id				m_tag;
		string_id				m_id;
		string_vector			m_str_classes;
		vector<string_id>		m_classes;
		style					m_style;
		struct auxiliary_pseudo_styles {
			std::unique_ptr<style> placeholder;
			std::unique_ptr<style> selection;
			std::unique_ptr<css_properties> placeholder_css;
			std::shared_ptr<html_tag> backdrop;
		};
		std::unique_ptr<auxiliary_pseudo_styles> m_auxiliary_pseudos;
		struct cascaded_selection_colors {
			std::optional<web_color> foreground;
			std::optional<web_color> background;
			bool author = false;
		};
		std::shared_ptr<const cascaded_selection_colors> m_selection_colors;
		int match_style_selector(const css_selector& selector, bool apply_pseudo, int& proximity);
		void apply_matched_style(int match, const css_selector& selector, int proximity, bool create_generated);
		bool apply_auxiliary_pseudo(int match, const css_selector& selector, const dockit::css::CascadePriority& priority);
		void compute_selection_colors();
		string_map				m_attrs;
		vector<string_id>		m_pseudo_classes;
		std::unique_ptr<css_replaced_image> m_content_image;
		std::optional<string> m_content_alternative;

		void			apply_inline_style();
		void			select_all(const css_selector& selector, elements_list& res) override;

	public:
		explicit html_tag(const shared_ptr<document>& doc);
		// constructor for anonymous wrapper boxes
		explicit html_tag(const element::ptr& parent, const string& style = "display: block");
		~html_tag() override;

		bool				appendChild(const element::ptr& el) override;
		bool				removeChild(const element::ptr& el) override;
		void				clearRecursive() override;
		string_id			tag() const override;
		string_id			id() const override;
		const char*			get_tagName() const override;
		void				set_tagName(const char* tag) override;
		void				set_data(const char* data) override;
		const vector<string_id>& classes() const { return m_classes; }
		const string_vector& str_classes() const { return m_str_classes; }

		void				set_attr(const char* name, const char* val) override;
		const char*			get_attr(const char* name, const char* def = nullptr) const override;
		void				apply_stylesheet(const litehtml::css& stylesheet) override;
		void				refresh_styles() override;

		bool				is_white_space() const override;
		bool				is_body() const override;
		bool				is_break() const override;

		bool				on_mouse_over() override;
		bool				on_mouse_leave() override;
		bool				on_lbutton_down() override;
		bool				on_lbutton_up(bool is_click) override;
		void				on_click() override;
		bool				set_pseudo_class(string_id cls, bool add) override;
		bool				set_class(const char* pclass, bool add) override;
		bool				is_replaced() const override;
		void				compute_styles(bool recursive = true) override;
		void				draw(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip, const std::shared_ptr<render_item> &ri) override;
		void				draw_background(uint_ptr hdc, pixel_t x, pixel_t y, const position *clip,
									const std::shared_ptr<render_item> &ri) override;
		css_replaced_image* content_image() const { return m_content_image.get(); }
		const std::optional<string>& get_content_alternative() const { return m_content_alternative; }

		const css_properties* get_placeholder_style() const;
		std::shared_ptr<html_tag> backdrop() const { return m_auxiliary_pseudos ? m_auxiliary_pseudos->backdrop : nullptr; }
		bool has_placeholder_color() const;
		struct selection_colors {
			std::optional<web_color> foreground;
			std::optional<web_color> background;
		};
		selection_colors get_selection_colors(const web_color& text_color) const;
		bool has_author_property(string_id name) const;
		bool has_pseudo_class(string_id name) const {
			return std::find(m_pseudo_classes.begin(), m_pseudo_classes.end(), name) != m_pseudo_classes.end();
		}
		const property_value& get_cascaded_property(string_id name) const { return m_style.get_property(name); }
		void collect_property_names(std::vector<string_id>& names) const { m_style.collect_property_names(names); }
		bool				get_custom_property(string_id name, css_token_vector& result) const;

		elements_list&	children();

		int					select(const css_selector::vector& selector_list, bool apply_pseudo = true) override;
		int					select(const string& selector) override;
		int					select(const css_selector& selector, bool apply_pseudo = true) override;

		elements_list		select_all(const string& selector) override;
		elements_list		select_all(const css_selector& selector) override;

		element::ptr		select_one(const string& selector) override;
		element::ptr		select_one(const css_selector& selector) override;

		void				get_text(string& text) const override;
		void				parse_attributes() override;

		void				get_content_size(size& sz, pixel_t max_width) override;
		void				add_style(const style& style) override;

		const background*	get_background(bool own_only = false) override;

		string				dump_get_name() override;

	protected:
		element::ptr get_element_marker(bool create);
		element::ptr		get_element_before(const style& style, bool create);
		element::ptr		get_element_after(const style& style, bool create);

		void map_to_pixel_length_property(string_id prop_name, string attr_value);
		void map_to_pixel_length_property_with_default_value(string_id prop_name, string attr_value, int default_value);
		void map_to_dimension_property(string_id prop_name, string attr_value);
		void map_to_dimension_property_ignoring_zero(string_id prop_name, string attr_value);

	protected:
		void handle_counter_properties();
		void compute_child_styles();
		bool append_content_text(const css_content_item& item, string& text);
		void compute_content_alternative();

	};

	/************************************************************************/
	/*                        Inline Functions                              */
	/************************************************************************/

	inline elements_list& html_tag::children()
	{
		return m_children;
	}


}

#endif  // LH_HTML_TAG_H
