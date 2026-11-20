#include <litehtml/css_parser.h>
#include <litehtml/css_selector.h>

namespace litehtml {

namespace syntax = dockit::css;

syntax::SelectorMode selector_mode(document_mode mode) {
	return mode == quirks_mode ? syntax::SelectorMode::quirks : syntax::SelectorMode::standards;
}

css_selector::vector parse_selector_list(const css_token_vector& tokens, int options, document_mode mode) {
	auto selectors = syntax::parse_selector_list(tokens, options, selector_mode(mode));
	css_selector::vector bound;
	bound.reserve(selectors.size());
	for (auto& selector : selectors) {
		selector->calc_specificity();
		auto rule = std::make_shared<css_selector>();
		rule->m_syntax = std::move(selector);
		bound.push_back(std::move(rule));
	}
	return bound;
}

bool supports_css_selector(const css_token_vector& tokens, document_mode mode) {
	return syntax::supports_css_selector(tokens, selector_mode(mode));
}

bool css_selector::parse(const string& text, document_mode mode) {
	auto parsed = syntax::parse_complex_selector(normalize(text, f_componentize), selector_mode(mode), syntax::strict_mode);
	if (!parsed) {
		return false;
	}
	parsed->calc_specificity();
	*this = {};
	m_syntax = std::move(parsed);
	return true;
}

} // namespace litehtml
