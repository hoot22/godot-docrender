#include "css_cascade.h"

#include "css_declarations.h"

namespace dockit::css {

namespace {

using namespace litehtml;

string_id all_property() {
	static const auto name = _id("all");
	return name;
}

bool resets_property(string_id name) {
	static const auto direction = _id("direction");
	static const auto unicode_bidi = _id("unicode-bidi");
	return name != empty_id && name != all_property() && name != direction && name != unicode_bidi &&
		_s(name).compare(0, 2, "--") != 0;
}

bool is_rollback(const property_value& value) {
	return value.is<revert_value>() || value.is<revert_layer_value>();
}

const property_value* prefer(const property_value* candidate, const property_value* current) {
	if (!candidate) return current;
	return !current || takes_cascade_precedence(candidate->priority, candidate->m_important,
		current->priority, current->m_important) ? candidate : current;
}

} // namespace

void Cascade::add(Property name, Value value, CascadePriority priority) {
	priority.declaration_order = ++next_declaration;
	insert(name, std::move(value), priority);
}

void Cascade::insert(Property name, Value value, CascadePriority priority) {
	value.priority = priority;
	value.declaration_property = name;
	const auto wins = [&](const Value& current) {
		return takes_cascade_precedence(priority, value.m_important, current.priority, current.m_important);
	};
	if (priority.layer >= 0 && priority.layer != css_inline_layer) {
		auto [entry, inserted] = layer_candidates[name].try_emplace(priority.layer, value);
		if (!inserted && wins(entry->second)) entry->second = value;
	}
	auto [entry, inserted] = winners.try_emplace(name, std::move(value));
	if (!inserted && wins(entry->second)) entry->second = std::move(value);
}

void Cascade::merge(const Cascade& source, CascadePriority priority) {
	const auto offset = next_declaration;
	next_declaration += source.next_declaration;
	for (const auto& [name, value] : source.winners) {
		priority.declaration_order = offset + value.priority.declaration_order;
		insert(name, value, priority);
	}
}

void Cascade::snapshot_defaults() {
	defaults = winners;
}

void Cascade::clear(bool keep_variables) {
	winners.clear();
	defaults.clear();
	layer_candidates.clear();
	computed.clear();
	if (!keep_variables) variables.clear();
	diagnostics.clear();
	next_declaration = 0;
}

const Cascade::Value& Cascade::rollback(Property name, Property alias, int layer, bool layer_only, bool has_author_layers) const {
	const Value* best = nullptr;
	const std::array candidates{ name, alias, resets_property(name) ? all_property() : litehtml::empty_id };
	if (has_author_layers && layer_only) {
		for (auto property : candidates) {
			if (property == litehtml::empty_id) continue;
			if (const auto found = layer_candidates.find(property); found != layer_candidates.end()) {
				for (const auto& [rank, candidate] : found->second) {
					// The fallback boundary follows normal layer order, including for important winners.
					if (rank >= layer) break;
					best = prefer(&candidate, best);
				}
			}
		}
		if (best) return *best;
	}
	if (layer != css_default_layer) {
		for (auto property : candidates) {
			if (property == litehtml::empty_id) continue;
			if (const auto found = defaults.find(property); found != defaults.end()) best = prefer(&found->second, best);
		}
	}
	static const Value unset;
	return best ? *best : unset;
}

void Cascade::resolve(bool has_author_layers, bool inherited_rtl, const litehtml::css_inherited_variable& inherited,
	const css_registered_properties& registered, document_container* container, const ParseDeferred& parse_deferred) {
	using namespace litehtml;
	computed.clear();
	variables.clear();
	diagnostics.clear();

	const auto restore = [&](Property name, Property alias, const Value& entry) -> const Value& {
		const auto* effective = &entry;
		while (is_rollback(*effective)) {
			effective = &rollback(name, alias, effective->priority.layer, effective->is<revert_layer_value>(), has_author_layers);
		}
		return *effective;
	};

	css_custom_properties local_variables;
	for (const auto& [name, winner] : winners) {
		if (_s(name).compare(0, 2, "--") != 0) continue;
		const auto& value = restore(name, empty_id, winner);
		if (value.is<css_token_vector>() || value.is<initial_value>() || value.is<inherit>()) {
			local_variables.emplace(name, &value);
		}
	}

	const css_custom_rollback restore_custom = [&](Property name, const Value& value,
		css_wide_keyword keyword) -> const Value& {
		return restore(name, empty_id, rollback(name, empty_id, value.priority.layer,
			keyword == css_wide_keyword::revert_layer, has_author_layers));
	};
	css_variable_resolver resolver(inherited, std::move(local_variables), registered, container, restore_custom);
	std::map<const Value::deferred_declaration*, props_map> parsed;
	const auto evaluate = [&](const Value& value) -> const props_map& {
		const auto& declaration = *value.deferred;
		auto [entry, inserted] = parsed.try_emplace(&declaration);
		if (!inserted) return entry->second;
		if (const auto tokens = resolver.substitute(declaration.tokens)) {
			auto result = parse_deferred(declaration, *tokens, value.m_important);
			if (result.status == css_declaration_status::parsed) {
				entry->second = std::move(result.properties);
				return entry->second;
			}
		}
		if (auto diagnostic = check_css_declaration(css_declaration_status::invalid, declaration.name,
			declaration.tokens, declaration.source)) diagnostics.push_back(std::move(*diagnostic));
		return entry->second;
	};

	const auto resolve_property = [&](Property name, Property alias, const Value& winner) {
		const auto* effective = &restore(name, alias, winner);
		std::optional<Value> resolved;
		while (effective->deferred) {
			const auto& properties = evaluate(*effective);
			const auto found = properties.find(effective->declaration_property);
			if (found == properties.end() || !is_rollback(found->second)) {
				resolved = found == properties.end() ? Value{} : found->second;
				resolved->priority = effective->priority;
				resolved->declaration_property = effective->declaration_property;
				effective = &*resolved;
				break;
			}
			effective = &restore(name, alias, rollback(name, alias, effective->priority.layer,
				found->second.is<revert_layer_value>(), has_author_layers));
		}
		if (effective->origin) {
			diagnostics.push_back({css_diagnostic_kind::not_rendered, _s(effective->origin->name),
				effective->origin->value, effective->origin->source});
		}
		if (resolved) computed.emplace(name, std::move(*resolved));
		else if (effective != &winner || winner.declaration_property != name || alias != empty_id) computed.emplace(name, *effective);
	};

	// Direction can itself depend on variables, layers, or the parent's computed direction.
	const auto direction_name = _id("direction");
	if (const auto found = winners.find(direction_name); found != winners.end())
		resolve_property(direction_name, empty_id, found->second);
	const auto& direction = get(direction_name);
	const bool rtl = direction.is<int>() ? direction.get<int>() == static_cast<int>(css_direction::rtl) :
		inherited_rtl && !direction.is<initial_value>();
	// Keep the shorthand once per cascade. Compare it with named declarations as
	// a physical longhand, with one shared fallback for otherwise unmentioned properties.
	const auto all_name = all_property();
	const auto all = winners.find(all_name);
	if (all != winners.end()) resolve_property(all_name, empty_id, all->second);
	const auto with_reset = [&](Property name, const Value* named) {
		return all != winners.end() && resets_property(name) ? prefer(&all->second, named) : named;
	};

	std::map<Property, Property> logical_slots;
	for (const auto& [name, winner] : winners) {
		const auto physical = css_map_logical_property(name, rtl);
		if (physical != empty_id) logical_slots.emplace(physical, name);
	}
	for (const auto& [name, winner] : winners) {
		if (name != direction_name && name != all_name && (logical_slots.empty() ||
			(logical_slots.find(name) == logical_slots.end() && css_map_logical_property(name, rtl) == empty_id))) {
			resolve_property(name, empty_id, *with_reset(name, &winner));
		}
	}
	for (const auto& [physical, logical] : logical_slots) {
		const auto current = winners.find(physical);
		const auto& alias = winners.at(logical);
		const auto* winner = with_reset(physical, prefer(&alias, current == winners.end() ? nullptr : &current->second));
		resolve_property(physical, logical, *winner);
	}
	variables = resolver.take_computed();
}

const Cascade::Value& Cascade::get(Property name) const {
	if (const auto found = computed.find(name); found != computed.end()) return found->second;
	if (const auto found = winners.find(name); found != winners.end()) return found->second;
	if (const auto all = winners.find(all_property()); all != winners.end() && resets_property(name)) {
		if (const auto computed_all = computed.find(all_property()); computed_all != computed.end()) return computed_all->second;
		return all->second;
	}
	static const Value unset;
	return unset;
}

void Cascade::collect_property_names(std::vector<Property>& names) const {
	for (const auto& entry : winners) names.push_back(entry.first);
	for (const auto& entry : computed) names.push_back(entry.first);
	for (const auto& entry : variables) names.push_back(entry.first);
}

const css_token_vector* Cascade::get_custom_property(Property name, bool& local) const {
	const auto found = variables.find(name);
	local = found != variables.end();
	return local && found->second ? &*found->second : nullptr;
}

} // namespace dockit::css
