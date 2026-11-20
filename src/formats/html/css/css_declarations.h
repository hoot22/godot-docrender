#pragma once

#include "css_property_value.h"

#include <array>

namespace litehtml {
	enum class css_edge_group { margin, padding, inset, scroll_padding, scroll_margin, border_width, border_style,
		border_color, radius_x, radius_y, count };

	struct css_flow_edges {
		std::array<string_id, 4> physical;
		std::array<string_id, 4> logical;
		bool corners = false;

		size_t map_direction(size_t side, bool rtl) const {
			if (!rtl) return side;
			if (corners) return side ^ 1;
			return side % 2 == 1 ? 4 - side : side;
		}
	};

	const css_flow_edges& css_edge_properties(css_edge_group group);
	string_id css_map_logical_property(string_id name, bool rtl);
	string_id css_canonical_property(string_id name);
	const vector<string_id>& css_longhands(string_id name);

	enum class css_declaration_status { unknown, invalid, deferred, parsed };

	struct css_parsed_declaration {
		css_declaration_status status;
		props_map properties;
	};

	css_parsed_declaration parse_css_declaration(string_id name, const css_token_vector& tokens,
		const string& baseurl, bool important, document_container* container, const css_source& source);
	css_parsed_declaration reparse_css_declaration(const property_value::deferred_declaration& declaration,
		const css_token_vector& tokens, bool important, document_container* container);
	bool supports_css_declaration(const css_token_vector& tokens);
	std::optional<css_diagnostic> check_css_declaration(css_declaration_status status, string_id name,
		const css_token_vector& tokens, const css_source& source);
}
