#ifndef LH_STYLE_H
#define LH_STYLE_H

#include "formats/html/css/css_cascade.h"
#include "formats/html/css/css_parser.h"
#include "formats/html/css/css_property_value.h"
#include "formats/html/css/css_value_helpers.h"

namespace litehtml {

	class html_tag;

	// represents a style block, eg. "color: black; display: inline"
	class style
	{
	public:
		typedef std::shared_ptr<style>		ptr;
		typedef std::vector<style::ptr>		vector;
	private:
		dockit::css::Cascade cascade;
		std::shared_ptr<std::vector<css_diagnostic>> diagnostics;
	public:
		void add(const css_token_vector& tokens, const string& baseurl = "", document_container* container = nullptr, const html_tag* element = nullptr);
		void add(const string& txt,              const string& baseurl = "", document_container* container = nullptr, const html_tag* element = nullptr);
		void add_declarations(const dockit::css::raw_declaration::vector& declarations, const string& baseurl,
			document_container* container, const html_tag* element = nullptr);

		void add_property(string_id name, const css_token_vector& tokens, const string& baseurl = "", bool important = false, document_container* container = nullptr);
		void add_property(string_id name, const string& val,              const string& baseurl = "", bool important = false, document_container* container = nullptr);

		const property_value& get_property(string_id name) const;
		void collect_property_names(std::vector<string_id>& names) const { cascade.collect_property_names(names); }

		void combine(const style& src, dockit::css::CascadePriority priority = {});
		void clear(bool keep_variables = false)
		{
			diagnostics.reset();
			cascade.clear(keep_variables);
		}

		const css_token_vector* get_custom_property(string_id name, bool& local) const;
		const std::vector<css_diagnostic>* get_diagnostics() const { return diagnostics.get(); }
		const std::vector<css_diagnostic>& get_computed_diagnostics() const { return cascade.get_diagnostics(); }
		void snapshot_defaults();
		void resolve_cascade(const html_tag& element, bool use_element_variables = false);

	private:

		void add_declaration(string_id name, const css_token_vector& tokens, const string& baseurl, bool important,
			document_container* container, const css_source& source, const html_tag* element);
		void remember_diagnostic(const css_diagnostic& diagnostic);
	};


} // namespace litehtml

#endif  // LH_STYLE_H
