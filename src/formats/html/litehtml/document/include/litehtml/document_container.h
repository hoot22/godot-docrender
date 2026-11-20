#ifndef LH_DOCUMENT_CONTAINER_H
#define LH_DOCUMENT_CONTAINER_H

#include "types.h"
#include "web_color.h"
#include "background.h"
#include "borders.h"
#include "element.h"
#include "font_description.h"
#include "formats/html/css/css_values.h"
#include <memory>
#include <functional>
#include <utility>
#include <vector>

namespace dockit { class HTMLImageAltText; }

namespace litehtml
{
	class html_tag;
	struct property_value;

	struct list_marker
	{
		string			image;
		const char*		baseurl;
		list_style_type	marker_type;
		web_color		color;
		position		pos;
		int				index;
		uint_ptr		font;
	};

	enum mouse_event
	{
		mouse_event_enter,
		mouse_event_leave,
	};

	// call back interface to draw text, images and other elements
	class document_container
	{
	public:
		virtual litehtml::uint_ptr	create_font(const font_description& descr, const document* doc, litehtml::font_metrics* fm) = 0;
		virtual void				delete_font(litehtml::uint_ptr hFont) = 0;
		virtual pixel_t				text_width(const char* text, litehtml::uint_ptr hFont) = 0;
		virtual pixel_t measure_text(const char* text, uint_ptr font, font_metrics&) { return text_width(text, font); }
		virtual void				draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) = 0;
		virtual pixel_t				pt_to_px(float pt) const = 0;
		virtual pixel_t				get_default_font_size() const = 0;
		virtual const char*			get_default_font_name() const = 0;
		virtual pixel_t				get_default_fixed_font_size() const { return get_default_font_size(); }
		virtual void				draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) = 0;
		virtual void				load_image(const char* src, const char* baseurl, bool redraw_on_ready, css_image_orientation orientation = css_image_orientation::from_image) = 0;
		virtual void				get_image_size(const char* src, const char* baseurl, litehtml::size& sz, css_image_orientation orientation = css_image_orientation::from_image) = 0;
		virtual bool				is_image_available(const char* src, const char* baseurl, css_image_orientation orientation);
		virtual std::shared_ptr<dockit::HTMLImageAltText> shape_image_alt(const char*, const html_tag&) { return {}; }
		virtual void				draw_image_alt(dockit::HTMLImageAltText&, const position&, const html_tag&) {}
		virtual double get_image_dpi(const char*, const char*) { return 0; }
		virtual void				get_css_image_size(const char* src, const char* baseurl, double density, litehtml::size& sz, css_image_orientation orientation = css_image_orientation::from_image);
		virtual void				draw_image(litehtml::uint_ptr hdc, const background_layer& layer, const std::string& url, const std::string& base_url) = 0;
		virtual void				draw_solid_fill(litehtml::uint_ptr hdc, const background_layer& layer, const web_color& color) = 0;
		virtual void				draw_linear_gradient(litehtml::uint_ptr hdc, const background_layer& layer, const background_layer::linear_gradient& gradient) = 0;
		virtual void				draw_radial_gradient(litehtml::uint_ptr hdc, const background_layer& layer, const background_layer::radial_gradient& gradient) = 0;
		virtual void				draw_conic_gradient(litehtml::uint_ptr hdc, const background_layer& layer, const background_layer::conic_gradient& gradient) = 0;
		virtual bool draw_border_image(const litehtml::html_tag&, const litehtml::borders&, const litehtml::position&,
			const litehtml::position*, bool, bool) { return false; }
		virtual void				draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) = 0;


		virtual void draw_cross_fade(uint_ptr, const background_layer&, const css_cross_fade&, const string&, const web_color&, int) {}
		virtual void draw_box_shadows(const html_tag&, const position&, const borders&, bool) {}
		virtual text_justify_info get_text_justify_info(const char*, uint_ptr, text_justify) { return {}; }
		virtual std::vector<size_t> get_bidi_visual_order(const char*, uint_ptr, bool,
			const std::vector<std::pair<int64_t, int64_t>>& p_ranges) {
			std::vector<size_t> order(p_ranges.size());
			for (size_t index = 0; index < order.size(); ++index) order[index] = index;
			return order;
		}
		virtual pixel_t trimmed_punctuation_width(const char* text, uint_ptr font, punctuation_trim) { return text_width(text, font); }
		virtual void draw_styled_text(uint_ptr hdc, const char* text, uint_ptr font, web_color color,
			const position& pos, const element&, text_justify_adjustment, punctuation_trim = punctuation_trim::none)
		{ draw_text(hdc, text, font, color, pos); }
		virtual bool begin_render_item(const render_item&, const position*&, bool = false) { return true; }
		virtual void end_render_item() {}
		virtual void draw_text_overflow(const render_item&) {}
		virtual void draw_scrollbars(const render_item&) {}
		virtual void begin_paint_group(const render_item&) {}
		virtual void end_paint_group() {}
		virtual void begin_background_blend() {}
		virtual void begin_background_blend_layer(css_blend_mode) {}
		virtual void set_overflow_clip(const render_item&) {}
		virtual void set_render_item_clip(const position& pos, const border_radiuses& radii, const render_item&)
			{ set_clip(pos, radii); }
		virtual css_source get_css_source(const string&, bool) const { return {}; }
		virtual void report_css_diagnostic(const css_diagnostic&, const html_tag* = nullptr) {}
		virtual const property_value* transition_override(const element*, string_id) const { return nullptr; }
		virtual const property_value* animation_override(const element*, string_id) const { return nullptr; }

		virtual	void				set_caption(const char* caption) = 0;
		virtual	void				set_base_url(const char* base_url) = 0;
		virtual void				link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) = 0;
		virtual void				on_anchor_click(const char* url, const litehtml::element::ptr& el) = 0;
		virtual bool				on_element_click(const litehtml::element::ptr& /*el*/) { return false; };
		virtual void				on_mouse_event(const litehtml::element::ptr& el, litehtml::mouse_event event) = 0;
		virtual	void				set_cursor(const char* cursor) = 0;
		virtual	void				transform_text(litehtml::string& text, litehtml::text_transform tt) = 0;
		virtual void				import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) = 0;
		virtual void				set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) = 0;
		virtual void				del_clip() = 0;
		virtual void				get_viewport(litehtml::position& viewport) const = 0;
		virtual litehtml::element::ptr	create_element( const char* tag_name,
														const litehtml::string_map& attributes,
														const std::shared_ptr<litehtml::document>& doc) = 0;

		virtual void				get_media_features(litehtml::media_features& media) const = 0;
		virtual void				get_language(litehtml::string& language, litehtml::string& culture) const = 0;
		virtual litehtml::string	resolve_color(const litehtml::string& /*color*/) const { return litehtml::string(); }
		virtual void				split_text(const char* text, const std::function<void(const char*)>& on_word, const std::function<void(const char*)>& on_space);

	protected:
		virtual ~document_container() = default;
	};
}

#endif  // LH_DOCUMENT_CONTAINER_H
