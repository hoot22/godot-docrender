#pragma once

#include <godot_cpp/variant/projection.hpp>

#include <optional>

namespace dockit {
	// Returns no matrix if either endpoint cannot be decomposed.
	std::optional<godot::Projection> interpolate_transform_matrices(const godot::Projection& p_from,
		const godot::Projection& p_to, double p_progress);
}
