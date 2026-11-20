#ifndef LH_DOCUMENT_H
#define LH_DOCUMENT_H

#include "stylesheet.h"
#include "types.h"
#include "master_css.h"
#include "encodings.h"
#include "font_description.h"
#include <vector>
#include <array>
#include <unordered_map>

typedef struct GumboInternalOutput GumboOutput;

namespace litehtml
{
	struct css_text
	{
		typedef std::vector<css_text>	vector;

		string	text;
		string	baseurl;
		string	media;
		const element* scope_root = nullptr;
		std::weak_ptr<element> owner;
		bool disabled = false;

		css_text() = default;

		css_text(const char* txt, const char* url, const char* media_str, const element* root)
		{
			text	= txt ? txt : "";
			baseurl	= url ? url : "";
			media	= media_str ? media_str : "";
			scope_root = root;
		}
	};

	class dumper
	{
	public:
		virtual ~dumper() {}
		virtual void begin_node(const string& descr) = 0;
		virtual void end_node() = 0;
		virtual void begin_attrs_group(const string& descr) = 0;
		virtual void end_attrs_group() = 0;
		virtual void add_attr(const string& name, const string& value) = 0;
	};

	class html_tag;
	class render_item;

	class document : public std::enable_shared_from_this<document>
	{
		friend struct dockit_editor;
		friend class render_item;
	public:
		typedef std::shared_ptr<document>	ptr;
		typedef std::weak_ptr<document>		weak_ptr;
	private:
		std::shared_ptr<element>			m_root;
		std::shared_ptr<render_item>		m_root_render;
		std::vector<std::weak_ptr<element>> m_top_layer;
		std::vector<std::shared_ptr<render_item>> m_top_layer_render;
		document_container*					m_container;
		fonts_map							m_fonts;
		std::size_t							m_font_prune_threshold = 256;
		css_text::vector					m_css;
		struct cssom_edit {
			string original;
			string baseurl;
			string replacement;
		};
		std::map<std::weak_ptr<element>, cssom_edit, std::owner_less<>> m_cssom_overrides;
		litehtml::css						m_styles;
		litehtml::web_color					m_def_color;
		litehtml::css						m_master_css;
		litehtml::size						m_size;
		position::vector					m_fixed_boxes;
		std::shared_ptr<element>			m_over_element;
		std::shared_ptr<element>			m_active_element;
		std::list<shared_ptr<render_item>>	m_tabular_elements;
		media_query_list_list::vector		m_media_lists;
		media_features						m_media;
		std::unordered_map<const element*, std::array<pixel_t, 2>> m_query_container_sizes;
		bool m_uses_container_units = false;
		size                                m_viewport_unit_size;
		string								m_lang;
		string								m_culture;
		string								m_text;
		document_mode						m_mode = no_quirks_mode;
		size_t								m_quote_depth = 0;
		uint64_t m_layout_generation = 0;
	public:
		document(document_container* objContainer);
		virtual ~document();

		document_container*				container()	{ return m_container; }
		document_mode					mode() const { return m_mode; }
		size_t quote_depth() const { return m_quote_depth; }
		void set_quote_depth(size_t depth) { m_quote_depth = depth; }
		size_t open_quote() { return m_quote_depth++; }
		std::optional<size_t> close_quote()
		{
			if (m_quote_depth == 0) return std::nullopt;
			return --m_quote_depth;
		}
		const css_counter_style* find_counter_style(const string& name) const
		{
			if (const auto* style = m_styles.find_counter_style(name)) return style;
			return m_master_css.find_counter_style(name);
		}
		bool							has_author_layers() const { return m_styles.has_layers(); }
		const css_keyframes* find_keyframes(const string& name) const {
			if (const auto* frames = m_styles.find_keyframes(name)) return frames;
			return m_master_css.find_keyframes(name);
		}
		css_registered_properties registered_properties();
		bool matches_container_query(const container_query& query, const element& subject, bool include_subject) const;
		std::optional<pixel_t> resolve_container_unit(const element& subject, bool block_axis, bool include_subject);
		uint_ptr						get_font(const font_description& descr, font_metrics* fm);
		font_metrics					initial_font_metrics();
		pixel_t							render(pixel_t max_width, render_type rt = render_all);
		void							draw(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip);
		web_color						get_def_color()	{ return m_def_color; }
		void 							cvt_units(css_length& val, const font_metrics& metrics, pixel_t size, css_length_context context = {}) const;
		pixel_t							to_pixels(const css_length& val, const font_metrics& metrics, pixel_t size, css_length_context context = {}) const;
		pixel_t							width() const;
		pixel_t							height() const;
		pixel_t							content_width() const;
		pixel_t							content_height() const;
		void							add_stylesheet(const char* str, const char* baseurl, const char* media,
										const element* scope_root = nullptr, const std::shared_ptr<element>& owner = {}, bool disabled = false);
		bool							on_mouse_over(pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y, position::vector& redraw_boxes);
		std::vector<scroll_values>		on_scroll(pixel_t dx, pixel_t dy, pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y) const;
		bool							on_lbutton_down(pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y, position::vector& redraw_boxes);
		bool							on_lbutton_up(pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y, position::vector& redraw_boxes);
		bool							on_button_cancel(position::vector& redraw_boxes);
		bool							on_mouse_leave(position::vector& redraw_boxes);
		std::shared_ptr<element>		create_element(const char* tag_name, const string_map& attributes);
		std::shared_ptr<element>		root();
		std::shared_ptr<render_item>	root_render();
		const auto& top_layer() const { return m_top_layer; }
		const auto& top_layer_render() const { return m_top_layer_render; }
		void							get_fixed_boxes(position::vector& fixed_boxes);
		void							add_fixed_box(const position& pos);
		void							add_media_list(media_query_list_list::ptr list);
		bool							media_changed();
		const media_features& get_media_features() const { return m_media; }
		media_features get_viewport_unit_features() const;
		bool							lang_changed();
		bool							match_lang(const string& lang);
		void							add_tabular(const std::shared_ptr<render_item>& el);
		std::shared_ptr<const element>	get_over_element() const { return m_over_element; }

		void							append_children_from_string(element& parent, const char* str, bool replace_existing);
		void							dump(dumper& cout);

		// see doc/document_createFromString.txt
		static document::ptr  createFromString(
			const estring&       str,
			document_container*  container,
			const string&        master_styles = litehtml::master_css,
			const string&        user_styles = "");

	private:
		std::optional<pixel_t> find_container_unit_basis(const element& subject, bool block_axis, bool include_subject) const;
		uint_ptr	add_font(const font_description& descr, font_metrics* fm);

		GumboOutput* parse_html(estring str);
		void create_node(void* gnode, elements_list& elements, bool parseTextNode, bool process_root);
		void append_document_stylesheets();
		void apply_document_author_styles();
		void apply_author_styles(element& root);
		bool update_media_lists(const media_features& features);
		void fix_tables_layout();
		void fix_table_children(const std::shared_ptr<render_item>& el_ptr, style_display disp, const char* disp_str);
		void fix_table_parent(const std::shared_ptr<render_item> & el_ptr, style_display disp, const char* disp_str);
	};

	inline std::shared_ptr<element> document::root()
	{
		return m_root;
	}

	inline std::shared_ptr<render_item> document::root_render()
	{
		return m_root_render;
	}

	inline void document::add_tabular(const std::shared_ptr<render_item>& el)
	{
		m_tabular_elements.push_back(el);
	}
	inline bool document::match_lang(const string& lang)
	{
		return lang == m_lang || lang == m_culture;
	}
}

#endif  // LH_DOCUMENT_H
