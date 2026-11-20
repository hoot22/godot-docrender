#include "html_mutation_observer.h"

#include "html_dom.h"

#include <algorithm>

using namespace godot;

void HTMLMutationObserver::_bind_methods() {
	ClassDB::bind_method(D_METHOD("observe", "target", "options"), &HTMLMutationObserver::observe);
	ClassDB::bind_method(D_METHOD("disconnect"), &HTMLMutationObserver::disconnect);
	ClassDB::bind_method(D_METHOD("take_records"), &HTMLMutationObserver::take_records);
	ADD_SIGNAL(MethodInfo("mutations_delivered", PropertyInfo(Variant::ARRAY, "records")));
}

Error HTMLMutationObserver::observe(const Ref<HTMLNode>& p_target, const Dictionary& p_options) {
	if (p_target.is_null() || !p_target->is_valid()) return ERR_INVALID_PARAMETER;
	auto& dom = p_target->get_dom();
	Registration options;
	options.owner = dom.shared_from_this();
	options.target = p_target;
	options.child_list = p_options.get("child_list", false);
	options.subtree = p_options.get("subtree", false);
	options.attribute_old_value = p_options.get("attribute_old_value", false);
	options.character_data_old_value = p_options.get("character_data_old_value", false);
	options.has_attribute_filter = p_options.has("attribute_filter");
	options.attributes = p_options.get("attributes", p_options.has("attribute_old_value") || options.has_attribute_filter);
	options.character_data = p_options.get("character_data", p_options.has("character_data_old_value"));
	if (options.has_attribute_filter) {
		const Variant filter = p_options["attribute_filter"];
		if (filter.get_type() == Variant::PACKED_STRING_ARRAY) {
			options.attribute_filter = filter;
		} else if (filter.get_type() == Variant::ARRAY) {
			const Array values = filter;
			for (const Variant& value : values) {
				if (value.get_type() != Variant::STRING) return ERR_INVALID_PARAMETER;
				options.attribute_filter.append(value);
			}
		} else {
			return ERR_INVALID_PARAMETER;
		}
	}
	if ((!options.child_list && !options.attributes && !options.character_data) ||
		(!options.attributes && (options.attribute_old_value || options.has_attribute_filter)) ||
		(!options.character_data && options.character_data_old_value)) return ERR_INVALID_PARAMETER;
	const auto existing = std::find_if(registrations.begin(), registrations.end(), [&](const Registration& p_registration) {
		return p_registration.target == p_target;
	});
	if (existing == registrations.end()) registrations.push_back(std::move(options));
	else *existing = std::move(options);
	transient.erase(std::remove_if(transient.begin(), transient.end(), [&](const Transient& p_item) {
		return p_item.registration.target == p_target;
	}), transient.end());
	dom.track_observer(ObjectID(get_instance_id()));
	return OK;
}

void HTMLMutationObserver::disconnect() {
	++delivery_version;
	registrations.clear();
	transient.clear();
	pending.clear();
}

TypedArray<Dictionary> HTMLMutationObserver::take_records() {
	TypedArray<Dictionary> records;
	for (const auto& item : pending) records.append(item.record);
	pending.clear();
	return records;
}

TypedArray<Dictionary> HTMLMutationObserver::take_owner(dockit::HTMLDOM& p_owner) {
	TypedArray<Dictionary> records;
	pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const Pending& p_item) {
		if (p_item.owner.lock().get() != &p_owner) return false;
		records.append(p_item.record);
		return true;
	}), pending.end());
	transient.erase(std::remove_if(transient.begin(), transient.end(), [&](const Transient& p_item) {
		return p_item.registration.owner.lock().get() == &p_owner;
	}), transient.end());
	return records;
}

void HTMLMutationObserver::clear_owner(const dockit::HTMLDOM& p_owner) {
	registrations.erase(std::remove_if(registrations.begin(), registrations.end(), [&](const Registration& p_item) {
		return p_item.owner.lock().get() == &p_owner;
	}), registrations.end());
	transient.erase(std::remove_if(transient.begin(), transient.end(), [&](const Transient& p_item) {
		return p_item.registration.owner.lock().get() == &p_owner;
	}), transient.end());
	pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const Pending& p_item) {
		return p_item.owner.lock().get() == &p_owner;
	}), pending.end());
}

void HTMLMutationObserver::enqueue(dockit::HTMLDOM& p_owner, const dockit::HTMLMutation& p_change,
	const std::vector<Ref<HTMLNode>>& p_ancestry) {
	using Mutation = dockit::HTMLMutation;
	bool observed = false;
	bool old_value = false;
	std::vector<Transient> extra;
	const auto match = [&](const Registration& p_registration, const Ref<HTMLNode>& p_root) {
		if (p_registration.owner.lock().get() != &p_owner ||
			(p_root != p_change.target && (!p_registration.subtree ||
				std::find(p_ancestry.begin(), p_ancestry.end(), p_root) == p_ancestry.end()))) return;
		switch (p_change.kind) {
			case Mutation::CHILD_LIST:
				observed |= p_registration.child_list;
				if (p_registration.subtree) {
					for (const Ref<HTMLNode>& root : p_change.removed) extra.push_back({p_registration, root});
				}
				break;
			case Mutation::ATTRIBUTES:
				if (p_registration.attributes && (!p_registration.has_attribute_filter ||
					p_registration.attribute_filter.has(p_change.attribute))) {
					observed = true;
					old_value |= p_registration.attribute_old_value;
				}
				break;
			case Mutation::CHARACTER_DATA:
				if (p_registration.character_data) {
					observed = true;
					old_value |= p_registration.character_data_old_value;
				}
				break;
		}
	};
	for (const auto& entry : registrations) match(entry, entry.target);
	for (const auto& entry : transient) match(entry.registration, entry.root);
	for (auto& entry : extra) {
		const auto same = [&](const Transient& p_item) {
			return p_item.root == entry.root && p_item.registration.target == entry.registration.target;
		};
		if (std::none_of(transient.begin(), transient.end(), same)) transient.push_back(std::move(entry));
	}
	if (observed) {
		Dictionary record;
		record["type"] = p_change.kind == Mutation::CHILD_LIST ? "childList" :
			(p_change.kind == Mutation::ATTRIBUTES ? "attributes" : "characterData");
		record["target"] = p_change.target;
		record["added_nodes"] = p_change.added.duplicate();
		record["removed_nodes"] = p_change.removed.duplicate();
		record["previous_sibling"] = p_change.previous;
		record["next_sibling"] = p_change.next;
		record["attribute_name"] = p_change.kind == Mutation::ATTRIBUTES ? Variant(p_change.attribute) : Variant();
		record["old_value"] = old_value ? p_change.old_value : Variant();
		pending.push_back({p_owner.shared_from_this(), record});
	}
	if (observed || !extra.empty()) p_owner.queue_observer_delivery();
}
