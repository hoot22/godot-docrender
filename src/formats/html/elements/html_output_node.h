#pragma once

#include "html_node_copy_state.h"

#include <godot_cpp/variant/string.hpp>
#include <litehtml/html_tag.h>

#include <optional>

namespace dockit {
	class HTMLOutputNode : public litehtml::html_tag, public HTMLNodeCopyState {
	public:
		using litehtml::html_tag::html_tag;
		std::optional<godot::String> default_override;
		bool copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const override;
	};
}
