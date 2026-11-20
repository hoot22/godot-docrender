#ifndef LH_STYLESHEET_H
#define LH_STYLESHEET_H

#include "css_selector.h"
#include "formats/html/css/css_parser.h"
#include "formats/html/css/css_font_face.h"
#include "formats/html/css/css_animation.h"
#include "formats/html/css/css_variables.h"

namespace dockit::css { struct StylesheetImport; }

namespace litehtml
{
	class document;

using dockit::css::raw_rule;
using dockit::css::raw_declaration;

class css
{
	css_selector::vector m_selectors;
	dockit::css::CascadeLayers cascade_layers;
	std::map<string, std::pair<int, css_counter_style>> m_counter_styles;
	struct counter_style_rule
	{
		string name;
		css_counter_style style;
		size_t layer_id;
	};
	std::vector<counter_style_rule> m_counter_style_rules;
	struct font_face_rule
	{
		css_font_face face;
		size_t layer_id;
	};
	std::vector<font_face_rule> m_font_face_rules;
	std::vector<css_font_face> m_font_faces;
	struct font_feature_values_rule
	{
		css_font_feature_values_rule values;
		size_t layer_id;
	};
	std::vector<font_feature_values_rule> m_font_feature_values_rules;
	std::vector<css_font_feature_values_rule> m_font_feature_values;
	struct keyframes_rule {
		string name;
		css_keyframes value;
		size_t layer_id;
	};
	std::vector<keyframes_rule> m_keyframe_rules;
	std::map<string, std::pair<int, css_keyframes>> m_keyframes;
	struct property_registration_rule {
		string_id name;
		css_registered_property registration;
		media_query_list_list::ptr media;
	};
	std::vector<property_registration_rule> m_property_registration_rules;
public:

	const css_selector::vector& selectors() const
	{
		return m_selectors;
	}

	void	parse_css_stylesheet(const string& input, const string& baseurl, shared_ptr<document> doc,
		media_query_list_list::ptr media = nullptr, size_t layer_id = 0, const element* implicit_scope_root = nullptr);

	void	sort_selectors(bool author = true);
	const css_counter_style* find_counter_style(const string& name) const
	{
		const auto found = m_counter_styles.find(name);
		return found == m_counter_styles.end() ? nullptr : &found->second.second;
	}
	const std::vector<css_font_face>& font_faces() const { return m_font_faces; }
	const std::vector<css_font_feature_values_rule>& font_feature_values() const { return m_font_feature_values; }
	const css_keyframes* find_keyframes(const string& name) const {
		const auto found = m_keyframes.find(name);
		return found == m_keyframes.end() ? nullptr : &found->second.second;
	}
	void collect_registered_properties(document& doc, css_registered_properties& properties) const;
	bool	has_layers() const { return cascade_layers.has_layers(); }
	bool	has_container_queries() const { return m_has_container_queries; }

private:
	bool m_has_container_queries = false;
	struct RuleContext {
		const string& baseurl;
		const shared_ptr<document>& doc;
		media_query_list_list::ptr media;
		size_t layer_id;
		std::shared_ptr<const dockit::css::NestingSelectorList> nesting;
		std::shared_ptr<const dockit::css::ScopeRule> scope;
		const element* implicit_scope_root;
		bool in_style_rule = false;
		int depth = 0;
		shared_ptr<const container_query> container_queries;
	};
	void	parse_rules(const raw_rule::vector& rules, const RuleContext& context, bool top_level);
	void	parse_block(const css_token_vector& tokens, RuleContext context);
	bool	parse_style_rule(const raw_rule& rule, const RuleContext& context);
	bool	parse_scope_rule(const raw_rule& rule, const RuleContext& context);
	void	apply_declarations(const raw_declaration::vector& declarations, const RuleContext& context);
	bool	apply_import(const dockit::css::StylesheetImport& rule, const string& baseurl, shared_ptr<document> doc, media_query_list_list::ptr media, size_t layer_id);
	void	add_selector(const css_selector::ptr& selector);
};

inline void css::add_selector(const css_selector::ptr& selector)
{
	selector->m_order = (int)m_selectors.size();
	m_has_container_queries |= static_cast<bool>(selector->m_container_query);
	m_selectors.push_back(selector);
}


} // namespace litehtml

#endif  // LH_STYLESHEET_H
