#pragma once

#include "css_selectors.h"

#include <limits>
#include <map>
#include <cstdint>

namespace dockit::css {

// Layer ranks follow normal precedence. Special ranks represent the baseline, unlayered rules and inline declarations.
inline constexpr int css_default_layer = -1;
inline constexpr int css_unlayered = std::numeric_limits<int>::max() - 1;
inline constexpr int css_inline_layer = std::numeric_limits<int>::max();

struct CascadePriority {
	int layer = css_default_layer;
	selector_specificity specificity;
	int source_order = 0;
	int proximity = std::numeric_limits<int>::max();
	std::uint64_t declaration_order = 0;
};

bool takes_cascade_precedence(const CascadePriority& candidate, bool candidate_important,
	const CascadePriority& current, bool current_important);

class CascadeLayers {
	struct Layer {
		vector<size_t> children;
		std::map<string, size_t> named_children;
		int rank = css_unlayered;
	};
	vector<Layer> layers{ Layer{} };

	size_t append_child(size_t parent);

public:
	size_t register_layer(size_t parent, const vector<string>& names);
	void assign_ranks();
	int rank(size_t layer) const { return layers[layer].rank; }
	bool has_layers() const { return layers.size() > 1; }
};

} // namespace dockit::css
