#include "css_cascade_order.h"

namespace dockit::css {

bool takes_cascade_precedence(const CascadePriority& candidate, bool candidate_important,
	const CascadePriority& current, bool current_important) {
	if ((candidate.layer == css_default_layer) != (current.layer == css_default_layer)) {
		return current.layer == css_default_layer;
	}
	if (candidate_important != current_important) {
		return candidate_important;
	}
	if (candidate.layer == current.layer) {
		if (!(candidate.specificity == current.specificity)) return candidate.specificity > current.specificity;
		if (candidate.proximity != current.proximity) return candidate.proximity < current.proximity;
		if (candidate.source_order != current.source_order) return candidate.source_order > current.source_order;
		return candidate.declaration_order >= current.declaration_order;
	}
	if (current.layer == css_inline_layer) return false;
	if (candidate.layer == css_inline_layer) return true;
	return candidate_important ? candidate.layer < current.layer : candidate.layer > current.layer;
}

size_t CascadeLayers::append_child(size_t parent) {
	const size_t child = layers.size();
	layers[parent].children.push_back(child);
	layers.emplace_back();
	return child;
}

size_t CascadeLayers::register_layer(size_t parent, const vector<string>& names) {
	if (names.empty()) return append_child(parent);
	for (const auto& name : names) {
		const auto [entry, inserted] = layers[parent].named_children.try_emplace(name, layers.size());
		const auto child = entry->second;
		if (inserted) append_child(parent);
		parent = child;
	}
	return parent;
}

void CascadeLayers::assign_ranks() {
	// Reverse cascade order visits each parent's implicit sublayer before its children.
	auto pending = layers[0].children;
	int rank = static_cast<int>(layers.size()) - 1;
	while (!pending.empty()) {
		auto& layer = layers[pending.back()];
		pending.pop_back();
		layer.rank = --rank;
		pending.insert(pending.end(), layer.children.begin(), layer.children.end());
	}
}

} // namespace dockit::css
