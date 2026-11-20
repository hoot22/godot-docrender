#ifndef LH_CSS_SELECTOR_H
#define LH_CSS_SELECTOR_H

#include "string_id.h"
#include "style.h"
#include "media_query.h"
#include "css_tokenizer.h"
#include "formats/html/css/css_selectors.h"

namespace dockit::css { struct ScopeRule; }

namespace litehtml
{
	class css_selector // complex selector: div + p
	{
	public:
		using ptr    = shared_ptr<css_selector>;
		using vector = std::vector<css_selector::ptr>;

	public:
		std::shared_ptr<const dockit::css::Selector> m_syntax;
		std::shared_ptr<const dockit::css::ScopeRule> m_scope;
		int							m_order = 0;
		size_t						m_layer_id = 0;
		int							m_layer = css_default_layer;
		media_query_list_list::ptr	m_media_query;
		shared_ptr<const container_query> m_container_query;
		style::ptr					m_style;

	public:
		bool parse(const string& text, document_mode mode);
		bool is_media_valid() const;
		dockit::css::CascadePriority cascade_priority(int proximity) const
		{
			// Baseline rules and HTML hints share the established application order.
			if (m_layer == css_default_layer) return {};
			return {m_layer, m_syntax->m_specificity, m_order, proximity};
		}
	};

	inline bool css_selector::is_media_valid() const
	{
		if(!m_media_query)
		{
			return true;
		}
		return m_media_query->is_used();
	}


	//////////////////////////////////////////////////////////////////////////

	inline bool operator > (const css_selector& v1, const css_selector& v2)
	{
		const auto& left = v1.m_syntax->m_specificity;
		const auto& right = v2.m_syntax->m_specificity;
		return left == right ? v1.m_order > v2.m_order : left > right;
	}

	inline bool operator < (const css_selector& v1, const css_selector& v2)
	{
		const auto& left = v1.m_syntax->m_specificity;
		const auto& right = v2.m_syntax->m_specificity;
		return left == right ? v1.m_order < v2.m_order : left < right;
	}

	inline bool operator > (const css_selector::ptr& v1, const css_selector::ptr& v2)
	{
		return (*v1 > *v2);
	}

	inline bool operator < (const css_selector::ptr& v1, const css_selector::ptr& v2)
	{
		return (*v1 < *v2);
	}

	//////////////////////////////////////////////////////////////////////////

	class used_selector
	{
	public:
		typedef std::unique_ptr<used_selector>	ptr;
		typedef std::vector<used_selector::ptr>	vector;

		css_selector::ptr	m_selector;
		bool				m_used;

		used_selector(const css_selector::ptr& selector, bool used)
		{
			m_used		= used;
			m_selector	= selector;
		}
	};


	dockit::css::SelectorMode selector_mode(document_mode mode);
	bool supports_css_selector(const css_token_vector& tokens, document_mode mode);
	css_selector::vector parse_selector_list(const css_token_vector& tokens, int options, document_mode mode);
}

#endif  // LH_CSS_SELECTOR_H
