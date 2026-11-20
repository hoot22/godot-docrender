// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_selector_matcher.h"

#include "css_syntax_util.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace dockit::css {

struct NestingMatchCache {
	std::unordered_map<const NestingSelectorList*, std::unordered_map<SelectorNode, unsigned>> matches;
};

namespace {

constexpr std::string_view whitespace = " \n\r\t\f";

bool contains_word(std::string_view value, std::string_view word) {
	size_t start = 0;
	while (start < value.size()) {
		auto end = value.find_first_of(" \n\r\t\f\"", start);
		// Preserve the existing word splitter's treatment of double-quoted spans.
		while (end != value.npos && value[end] == '"') {
			end = value.find('"', end + 1);
			if (end != value.npos) end = value.find_first_of(" \n\r\t\f\"", end + 1);
		}
		if (value.substr(start, end == value.npos ? end : end - start) == word) return true;
		if (end == value.npos) break;
		start = value.find_first_not_of(whitespace, end + 1);
	}
	return false;
}

bool match_attribute(const SelectorDOM& dom, SelectorNode node, const AttributeSelector& selector) {
	const auto raw = dom.attribute(node, selector.name);
	if (!raw) return false;
	if (selector.matcher == attribute_exists) return true;
	const auto folded = selector.caseless_match ? detail::lowcase(std::string(*raw)) : std::string();
	const std::string_view value = selector.caseless_match ? std::string_view(folded) : *raw;
	const std::string_view expected = selector.value;
	switch (selector.matcher) {
		case attribute_equals:
			return value == expected;
		case attribute_contains_string:
			return !expected.empty() && value.find(expected) != value.npos;
		case attribute_contains_word:
			return !expected.empty() && contains_word(value, expected);
		case attribute_starts_with_string:
			return !expected.empty() && value.substr(0, expected.size()) == expected;
		case attribute_starts_with_string_hyphen:
			return value == expected || (value.size() > expected.size() && value[expected.size()] == '-' &&
				value.substr(0, expected.size()) == expected);
		case attribute_ends_with_string:
			return !expected.empty() && value.size() >= expected.size() &&
				value.substr(value.size() - expected.size()) == expected;
		default:
			return false;
	}
}

bool matches_index(int64_t index, int a, int b) {
	const int64_t delta = index - b;
	return delta == 0 || (a != 0 && (delta > 0) == (a > 0) && delta % a == 0);
}

vector<std::string_view> language_subtags(std::string_view value) {
	vector<std::string_view> subtags;
	size_t start = 0;
	for (;;) {
		const auto end = value.find('-', start);
		subtags.push_back(value.substr(start, end == value.npos ? end : end - start));
		if (end == value.npos) return subtags;
		start = end + 1;
	}
}

// RFC 4647 extended filtering permits skipped subtags up to an extension singleton.
bool matches_language_range(std::string_view language, std::string_view range) {
	if (language.empty() || range.empty()) return language.empty() && range.empty();
	if (language == range || range == "*") return true;
	const auto tag = language_subtags(language);
	const auto parts = language_subtags(range);
	if (parts[0] != "*" && parts[0] != tag[0]) return false;

	size_t tag_index = 1;
	for (size_t index = 1; index < parts.size(); ++index) {
		if (parts[index] == "*") continue;
		while (tag_index < tag.size() && tag[tag_index] != parts[index]) {
			if (tag[tag_index++].size() <= 1) return false;
		}
		if (tag_index == tag.size()) return false;
		++tag_index;
	}
	return true;
}

unsigned match_compound(const SelectorDOM& dom, SelectorNode node, const CompoundSelector& selector,
	const MatchContext& context);

bool match_relative(const SelectorDOM& dom, SelectorNode anchor, const Selector& selector,
	const MatchContext& context) {
	struct Step {
		const CompoundSelector* compound;
		css_combinator combinator;
	};
	vector<Step> steps;
	for (auto part = &selector; part; part = part->m_left.get()) {
		steps.push_back({&part->m_right, part->m_left ? part->m_combinator : part->m_anchor_combinator});
	}
	std::reverse(steps.begin(), steps.end());

	struct Pending {
		SelectorNode from;
		size_t index;
	};
	vector<Pending> pending{{anchor, 0}};
	vector<std::unordered_set<SelectorNode>> visited(steps.size());
	while (!pending.empty()) {
		const auto [from, index] = pending.back();
		pending.pop_back();
		if (index == steps.size()) return true;
		// Earlier axes can reach the same node with the same remaining steps.
		if (!visited[index].insert(from).second) continue;
		const auto [compound, combinator] = steps[index];
		const auto enqueue_match = [&](SelectorNode node) {
			if (match_compound(dom, node, *compound, context)) pending.push_back({node, index + 1});
		};
		if (combinator == combinator_descendant || combinator == combinator_child) {
			dom.visit_children(from, false, [&](SelectorNode child) {
				if (dom.kind(child) == SelectorNodeKind::element) {
					if (combinator == combinator_descendant) pending.push_back({child, index});
					enqueue_match(child);
				}
				return false;
			});
		} else if (const auto parent = dom.parent(from)) {
			bool after_anchor = false;
			dom.visit_children(parent, false, [&](SelectorNode sibling) {
				if (sibling == from) {
					after_anchor = true;
				} else if (after_anchor && dom.kind(sibling) == SelectorNodeKind::element) {
					enqueue_match(sibling);
					return combinator == combinator_adjacent_sibling;
				}
				return false;
			});
		}
	}
	return false;
}

bool has_focus_within(const SelectorDOM& dom, SelectorNode node) {
	vector<SelectorNode> pending{node};
	while (!pending.empty()) {
		const auto current = pending.back();
		pending.pop_back();
		if (dom.has_pseudo_class(current, "focus")) return true;
		dom.visit_children(current, false, [&](SelectorNode child) {
			if (dom.kind(child) == SelectorNodeKind::element) pending.push_back(child);
			return false;
		});
	}
	return false;
}

bool match_structural(const SelectorDOM& dom, SelectorNode node, const AttributeSelector& selector,
	const MatchContext& context) {
	const auto& name = selector.name;
	const bool only = detail::is_one_of(name, "only-child", "only-of-type");
	const bool reverse = detail::is_one_of(name, "last-child", "last-of-type", "nth-last-child", "nth-last-of-type");
	const bool of_type = detail::is_one_of(name, "first-of-type", "last-of-type", "only-of-type", "nth-of-type", "nth-last-of-type");
	const bool nth = detail::is_one_of(name, "nth-child", "nth-last-child", "nth-of-type", "nth-last-of-type");
	const int a = nth ? selector.a : 0;
	const int b = nth ? selector.b : 1;
	const auto parent = dom.parent(node);
	if (!parent) {
		return matches_index(1, a, b) && (selector.selector_list.empty() ||
			match_selector_list(dom, node, selector.selector_list, context));
	}
	if (!dom.is_matchable(parent)) return false;

	int64_t count = 0;
	bool matched = false;
	dom.visit_children(parent, reverse, [&](SelectorNode child) {
		if (dom.kind(child) != SelectorNodeKind::element) return false;
		const bool included = of_type ? dom.same_tag(child, node) :
			(selector.selector_list.empty() || match_selector_list(dom, child, selector.selector_list, context));
		if (included) ++count;
		if (only) return count > 1;
		if (child != node) return false;
		matched = included && matches_index(count, a, b);
		return true;
	});
	return only ? count <= 1 : matched;
}

bool match_pseudo_class(const SelectorDOM& dom, SelectorNode node, const AttributeSelector& selector,
	const MatchContext& context) {
	const auto& name = selector.name;
	if (name == "scope") return context.scope ? node == context.scope : !dom.parent(node);
	if (detail::is_one_of(name, "link", "any-link")) {
		return detail::is_one_of(dom.tag(node), "a", "area") && dom.attribute(node, "href").has_value();
	}
	if (name == "open") return (dom.tag(node) == "details" || dom.tag(node) == "dialog") && dom.attribute(node, "open").has_value();
	if (name == "indeterminate") return (dom.tag(node) == "progress" && !dom.attribute(node, "value").has_value()) ||
		dom.has_pseudo_class(node, name);
	if (name == "focus-within") return has_focus_within(dom, node);
	if (name == "has") {
		return std::any_of(selector.selector_list.begin(), selector.selector_list.end(),
			[&](const auto& relative) { return match_relative(dom, node, *relative, context); });
	}
	if (detail::is_one_of(name, "is", "where", "not")) {
		const bool matched = match_selector_list(dom, node, selector.selector_list, context) != match_none;
		return name == "not" ? !matched : matched;
	}
	if (name == "empty") {
		bool empty = true;
		dom.visit_children(node, false, [&](SelectorNode child) {
			const auto kind = dom.kind(child);
			empty = kind != SelectorNodeKind::element && kind != SelectorNodeKind::text;
			return !empty;
		});
		return empty;
	}
	if (name == "lang") {
		return matches_language(dom, node, selector.language_ranges);
	}
	if (name == "dir") return dom.matches_direction(node, selector.value);
	if (detail::is_one_of(name, "first-child", "first-of-type", "last-child", "last-of-type", "only-child",
		"only-of-type", "nth-child", "nth-of-type", "nth-last-child", "nth-last-of-type")) {
		return match_structural(dom, node, selector, context);
	}
	return dom.has_pseudo_class(node, name);
}

bool can_match_virtual_scope(const CompoundSelector& selector) {
	if (selector.m_has_type_selector) return false;
	bool allows_scope = false;
	for (const auto& attribute : selector.m_attrs) {
		if (attribute.type == select_nesting && !attribute.nesting) {
			allows_scope = true;
			continue;
		}
		if (attribute.type != select_pseudo_class) return false;
		const auto& name = attribute.name;
		// A relational selector can test the root when the compound also targets :scope.
		if (name == "has") continue;
		if (name != "scope" && (!detail::is_one_of(name, "is", "where", "not") ||
			std::none_of(attribute.selector_list.begin(), attribute.selector_list.end(),
				[](const auto& argument) { return can_match_virtual_scope(argument->m_right); }))) {
			return false;
		}
		allows_scope = true;
	}
	return allows_scope;
}

unsigned match_nesting_parent(const SelectorDOM& dom, SelectorNode node, const NestingSelectorList& parent,
	const MatchContext& context) {
	if (!context.nesting_cache) {
		NestingMatchCache cache;
		auto nested = context;
		nested.nesting_cache = &cache;
		return match_nesting_parent(dom, node, parent, nested);
	}
	// Repeated ampersands can refer to the same parent and node across several levels.
	auto& matches = context.nesting_cache->matches[&parent];
	if (const auto found = matches.find(node); found != matches.end()) return found->second;
	const auto result = match_selector_list(dom, node, parent.element_selectors, context);
	matches.emplace(node, result);
	return result;
}

unsigned match_compound(const SelectorDOM& dom, SelectorNode node, const CompoundSelector& selector,
	const MatchContext& context) {
	if (!node || !dom.is_matchable(node) || (selector.m_tag != "*" && selector.m_tag != dom.tag(node))) {
		return match_none;
	}
	if (context.virtual_scope && node == context.scope && !can_match_virtual_scope(selector)) {
		return match_none;
	}
	unsigned result = match_element;
	for (const auto& attribute : selector.m_attrs) {
		switch (attribute.type) {
			case select_class:
				if (!dom.has_class(node, attribute.name)) return match_none;
				break;
			case select_id:
				if (dom.id(node) != attribute.name) return match_none;
				break;
			case select_pseudo_element:
				if (attribute.name == "before") result |= match_before;
				else if (attribute.name == "after") result |= match_after;
				else if (attribute.name == "marker") result |= match_marker;
				else if (attribute.name == "placeholder") result |= match_placeholder;
				else if (attribute.name == "selection") result |= match_selection;
				else if (attribute.name == "backdrop") result |= match_backdrop;
				else return match_none;
				break;
			case select_pseudo_class:
				if (context.mode == MatchMode::candidate) result |= match_deferred_pseudo_class;
				else if (!match_pseudo_class(dom, node, attribute, context)) return match_none;
				break;
			case select_nesting:
				if (attribute.nesting) {
					const auto matched = match_nesting_parent(dom, node, *attribute.nesting, context);
					if (!matched) return match_none;
					result |= matched & match_deferred_pseudo_class;
				} else if (context.scope ? node != context.scope : dom.parent(node) != nullptr) {
					return match_none;
				}
				break;
			case select_attr:
				if (!match_attribute(dom, node, attribute)) return match_none;
				break;
		}
	}
	return result;
}

} // namespace

std::optional<std::string_view> inherited_language(const SelectorDOM& dom, SelectorNode node) {
	for (; node; node = dom.parent(node)) {
		if (auto value = dom.attribute(node, "lang")) return value;
	}
	return {};
}

bool matches_language(const SelectorDOM& dom, SelectorNode node, const vector<string>& ranges) {
	const auto language = detail::lowcase(std::string(inherited_language(dom, node).value_or(std::string_view())));
	return std::any_of(ranges.begin(), ranges.end(),
		[&](const auto& range) { return matches_language_range(language, range); });
}

unsigned match_selector(const SelectorDOM& dom, SelectorNode node, const Selector& selector,
	const MatchContext& context) {
	const auto right = match_compound(dom, node, selector.m_right, context);
	if (!right || !selector.m_left) return right;

	const auto parent = dom.parent(node);
	if (!parent) return match_none;
	const auto& left = *selector.m_left;
	if (selector.m_combinator == combinator_child) {
		const auto result = match_selector(dom, parent, left, context);
		return result ? right | result : match_none;
	}
	if (selector.m_combinator == combinator_descendant) {
		for (auto ancestor = parent; ancestor && dom.is_matchable(ancestor); ancestor = dom.parent(ancestor)) {
			if (const auto result = match_selector(dom, ancestor, left, context)) {
				return right | (result & match_deferred_pseudo_class);
			}
		}
		return match_none;
	}
	if (!dom.is_matchable(parent)) return match_none;

	const bool adjacent = selector.m_combinator == combinator_adjacent_sibling;
	if (!adjacent && selector.m_combinator != combinator_general_sibling) return match_none;
	SelectorNode previous = nullptr;
	unsigned result = match_none;
	bool found_subject = false;
	dom.visit_children(parent, false, [&](SelectorNode child) {
		if (dom.kind(child) != SelectorNodeKind::element) return false;
		if (child == node) {
			found_subject = true;
			if (adjacent && previous) result = match_selector(dom, previous, left, context);
			return true;
		}
		if (adjacent) previous = child;
		else if (!result) result = match_selector(dom, child, left, context);
		return false;
	});
	return found_subject && result ? right | (result & match_deferred_pseudo_class) : match_none;
}

unsigned match_selector_list(const SelectorDOM& dom, SelectorNode node, const Selector::vector& selectors,
	const MatchContext& context) {
	for (const auto& selector : selectors) {
		if (const auto result = match_selector(dom, node, *selector, context)) return result;
	}
	return match_none;
}

namespace {

struct ScopeRoot {
	SelectorNode node;
	int distance;
};

bool inside_scope_limit(const SelectorDOM& dom, SelectorNode subject, SelectorNode root,
	const Selector::vector& limits) {
	if (limits.empty()) return false;
	const MatchContext context{root};
	for (auto node = subject; node != root; node = dom.parent(node)) {
		if (match_selector_list(dom, node, limits, context)) return true;
	}
	return false;
}

vector<ScopeRoot> find_scope_roots(const SelectorDOM& dom, SelectorNode subject, const ScopeRule& scope,
	MatchMode mode) {
	const auto outer = scope.parent ? find_scope_roots(dom, subject, *scope.parent, mode) :
		vector<ScopeRoot>{{nullptr, std::numeric_limits<int>::max()}};
	if (outer.empty()) return {};

	vector<ScopeRoot> roots;
	int distance = 0;
	for (auto node = subject; node && distance <= outer.back().distance; node = dom.parent(node), ++distance) {
		if (dom.kind(node) != SelectorNodeKind::element) continue;
		const bool matches = scope.start.empty() ? node == scope.implicit_root :
			std::any_of(outer.begin(), outer.end(), [&](const ScopeRoot& bound) {
				if (bound.distance < distance) return false;
				NestingMatchCache cache;
				return match_selector_list(dom, node, scope.start, {bound.node, mode, false, &cache}) != match_none;
			});
		if (matches && (mode == MatchMode::candidate || !inside_scope_limit(dom, subject, node, scope.end))) {
			roots.push_back({node, distance});
		}
	}
	return roots;
}

} // namespace

unsigned ScopeRule::state_dependencies() const {
	unsigned dependencies = parent ? parent->state_dependencies() : 0;
	for (const auto& selector : start) dependencies |= selector->state_dependencies();
	for (const auto& selector : end) dependencies |= selector->state_dependencies();
	return dependencies;
}

SelectorMatch match_scoped_selector(const SelectorDOM& dom, SelectorNode node, const Selector& selector,
	const ScopeRule& scope, MatchMode mode) {
	for (const auto& root : find_scope_roots(dom, node, scope, mode)) {
		NestingMatchCache cache;
		auto flags = match_selector(dom, node, selector, {root.node, mode, false, &cache});
		if (flags == match_none) continue;
		// Candidate traversal defers dynamic roots and limits to the complete match.
		if (mode == MatchMode::candidate) flags |= match_deferred_pseudo_class;
		return {flags, root.distance};
	}
	return {};
}

} // namespace dockit::css
