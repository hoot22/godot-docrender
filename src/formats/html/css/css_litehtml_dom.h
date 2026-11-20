#pragma once

#include "css_selector_matcher.h"

#include <string>

namespace litehtml {

class element;

const dockit::css::SelectorDOM& selector_dom_view();
int selector_match_flags(unsigned result);
const char* get_element_language(const element& node);
std::string compute_auto_direction(const element& node);
bool matches_element_language(const element& node, const std::vector<std::string>& ranges);

} // namespace litehtml
