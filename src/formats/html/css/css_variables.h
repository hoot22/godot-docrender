#pragma once

#include "css_token_compat.h"
#include "css_string_id.h"

#include <functional>
#include <map>
#include <optional>

namespace litehtml {
	class document_container;
	struct property_value;
	using css_custom_properties = std::map<string_id, const property_value*>;
	using css_computed_variables = std::map<string_id, std::optional<css_token_vector>>;
	enum class css_registered_syntax { number, integer, length, percentage, length_percentage, color, custom_ident };
	struct css_registered_property {
		std::vector<css_registered_syntax> syntax;
		bool universal = false;
		bool inherits = true;
		std::optional<css_token_vector> initial_value;
	};
	using css_registered_properties = std::map<string_id, css_registered_property>;
	enum class css_variable_syntax { none, valid, invalid };
	enum class css_wide_keyword { none, inherit, initial, unset, revert, revert_layer };
	using css_inherited_variable = std::function<bool(string_id, css_token_vector&)>;
	using css_custom_rollback = std::function<const property_value&(string_id, const property_value&, css_wide_keyword)>;
	css_variable_syntax inspect_css_variables(const css_token_vector& p_tokens);
	css_wide_keyword read_css_wide_keyword(const css_token_vector& p_tokens);
	bool matches_registered_syntax(const css_token_vector& p_tokens, const css_registered_property& p_registration,
		document_container* p_container);

	class css_variable_resolver {
		const css_inherited_variable& inherited;
		css_custom_properties declarations;
		const css_registered_properties& registered;
		document_container* container = nullptr;
		const css_custom_rollback& rollback;
		css_computed_variables computed;
		bool dependencies_changed = false;

		void invalidate_cycles();
		const std::optional<css_token_vector>& resolve(string_id p_name, int p_depth);
		bool substitute(const css_token_vector& p_tokens, size_t p_start, css_token_vector& r_output,
			size_t& r_budget, int p_depth);

	public:
		css_variable_resolver(const css_inherited_variable& p_inherited, css_custom_properties p_declarations,
			const css_registered_properties& p_registered, document_container* p_container, const css_custom_rollback& p_rollback);
		std::optional<css_token_vector> substitute(const css_token_vector& p_tokens);
		css_computed_variables take_computed();
	};
}
