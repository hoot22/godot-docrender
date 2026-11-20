#include "html_form_state.h"

#include "html_input_node.h"
#include "html_select_node.h"
#include "html_textarea_node.h"
#include "formats/html/dom/html_dom.h"

#include <litehtml/document.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace dockit::forms {
using namespace litehtml;

namespace {
	element::ptr root_of(const element& p_node) {
		auto root = std::const_pointer_cast<element>(p_node.shared_from_this());
		while (const auto parent = root->parent()) root = parent;
		return root;
	}

	element::ptr by_id(const element::ptr& p_root, const char* p_id) {
		if (!p_id || !*p_id) return {};
		element::ptr found;
		HTMLDOM::visit_elements(p_root, [&](const element::ptr& p_node) {
			if (std::string_view(p_node->get_attr("id", "")) == p_id) found = p_node;
			return bool(found);
		});
		return found;
	}

	bool labelable(const element& p_node) {
		if (const auto* input = dynamic_cast<const HTMLInputNode*>(&p_node)) {
			return input->type() != HTMLInputNode::Type::HIDDEN;
		}
		const auto tag = p_node.tag();
		return tag == _textarea_ || tag == _select_ || tag == _button_ || tag == _progress_ || tag == _meter_ || tag == _output_;
	}

	bool is_submit_button(const element& p_node) {
		const auto type = godot::String::utf8(p_node.get_attr("type", "")).to_lower();
		const auto parent = p_node.parent();
		return type == "submit" || (type != "reset" && type != "button" && !p_node.get_attr("command") &&
			!p_node.get_attr("commandfor") && (!parent || parent->tag() != _select_));
	}

	bool is_validation_candidate(const element& p_node) {
		if (!supports_required(p_node) && !(p_node.tag() == _button_ && is_submit_button(p_node))) return false;
		if (is_disabled(p_node)) return false;
		if (const auto* editor = dynamic_cast<const HTMLTextControlNode*>(&p_node);
			editor && editor->is_text_control() && p_node.get_attr("readonly")) return false;
		for (auto parent = p_node.parent(); parent; parent = parent->parent()) {
			if (parent->tag() == _datalist_) return false;
		}
		return true;
	}

	bool radio_value_missing(const std::vector<element::ptr>& p_group) {
		bool required = false;
		for (const auto& peer : p_group) {
			if (static_cast<const HTMLInputNode&>(*peer).is_checked()) return false;
			required |= peer->get_attr("required") != nullptr;
		}
		return required;
	}

	std::optional<bool> control_validity(const element& p_node, std::optional<bool> p_radio_missing = {}) {
		if (const auto* input = dynamic_cast<const HTMLInputNode*>(&p_node)) {
			if (input->type() == HTMLInputNode::Type::RADIO && p_radio_missing) {
				return is_validation_candidate(p_node) ? std::optional<bool>(!*p_radio_missing) : std::nullopt;
			}
			const auto validity = input->validity();
			return bool(validity["will_validate"]) ? std::optional<bool>(bool(validity["valid"])) : std::nullopt;
		}
		if (!is_validation_candidate(p_node)) return {};
		const bool required = p_node.get_attr("required");
		if (const auto* text = dynamic_cast<const HTMLTextControlNode*>(&p_node)) {
			return !(required && text->editor.get_value().is_empty()) &&
				!text->editor.has_length_violation(p_node.get_attr("minlength"), p_node.get_attr("maxlength"));
		}
		if (const auto* select = dynamic_cast<const HTMLSelectNode*>(&p_node)) return !required || !select->is_missing();
		return true;
	}

	bool set_validity(element& p_node, std::optional<bool> p_validity) {
		static const auto valid = _id("valid");
		static const auto invalid = _id("invalid");
		bool changed = p_node.set_pseudo_class(valid, p_validity.value_or(false));
		changed |= p_node.set_pseudo_class(invalid, !p_validity.value_or(true));
		return changed;
	}

	template<class Visit>
	void visit_validation_containers(const element& p_control, const Visit& p_visit) {
		if (const auto form = owner(p_control)) p_visit(form);
		for (auto parent = p_control.parent(); parent; parent = parent->parent()) {
			if (parent->tag() == _fieldset_) p_visit(parent);
		}
	}
}

bool is_listed(const element& p_node) {
	const auto tag = p_node.tag();
	return tag == _input_ || tag == _textarea_ || tag == _button_ || tag == _select_ || tag == _fieldset_ || tag == _output_;
}

bool supports_required(const element& p_node) {
	if (const auto* input = dynamic_cast<const HTMLInputNode*>(&p_node)) return input->supports_required();
	return p_node.tag() == _select_ || p_node.tag() == _textarea_;
}

bool is_read_write(const element& p_node) {
	const auto* editor = dynamic_cast<const HTMLTextControlNode*>(&p_node);
	return editor && editor->is_editable();
}

bool is_disabled(const element& p_node) {
	if (p_node.get_attr("disabled")) return true;
	if (p_node.tag() == _option_) {
		const auto group = p_node.parent();
		return group && group->tag() == _optgroup_ && group->get_attr("disabled");
	}
	const element* branch = &p_node;
	for (auto parent = p_node.parent(); parent; branch = parent.get(), parent = parent->parent()) {
		if (parent->tag() != _fieldset_ || !parent->get_attr("disabled")) continue;
		const auto& children = parent->children();
		const auto legend = std::find_if(children.begin(), children.end(), [](const element::ptr& p_child) {
			return p_child->tag() == _legend_;
		});
		if (legend == children.end() || legend->get() != branch) return true;
	}
	return false;
}

element::ptr owner(const element& p_node) {
	if (p_node.tag() == _legend_) {
		const auto parent = p_node.parent();
		return parent && parent->tag() == _fieldset_ ? owner(*parent) : element::ptr();
	}
	if (!is_listed(p_node)) return {};
	if (const auto* form = p_node.get_attr("form")) {
		const auto root = root_of(p_node);
		const auto doc = p_node.get_document();
		if (doc && root == doc->root()) {
			const auto match = by_id(root, form);
			return match && match->tag() == _form_ ? match : element::ptr();
		}
	}
	for (auto parent = p_node.parent(); parent; parent = parent->parent()) {
		if (parent->tag() == _form_) return parent;
	}
	return {};
}

element::ptr labeled_control(const element& p_label) {
	if (const auto* id = p_label.get_attr("for")) {
		const auto candidate = by_id(root_of(p_label), id);
		return candidate && labelable(*candidate) ? candidate : element::ptr();
	}
	element::ptr result;
	HTMLDOM::visit_elements(std::const_pointer_cast<element>(p_label.shared_from_this()), [&](const element::ptr& p_node) {
		if (labelable(*p_node)) result = p_node;
		return bool(result);
	});
	return result;
}

std::vector<element::ptr> labels(const element& p_control) {
	std::vector<element::ptr> result;
	HTMLDOM::visit_elements(root_of(p_control), [&](const element::ptr& p_node) {
		if (p_node->tag() == _label_ && labeled_control(*p_node).get() == &p_control) result.push_back(p_node);
		return false;
	});
	return result;
}

ButtonAction button_action(const element& p_node) {
	if (const auto* input = dynamic_cast<const HTMLInputNode*>(&p_node); input && input->is_button()) {
		if (input->type() == HTMLInputNode::Type::BUTTON) return ButtonAction::BUTTON;
		if (!owner(p_node)) return ButtonAction::NONE;
		return input->type() == HTMLInputNode::Type::RESET ? ButtonAction::RESET : ButtonAction::SUBMIT;
	}
	if (p_node.tag() != _button_) return ButtonAction::NONE;
	const auto type = godot::String::utf8(p_node.get_attr("type", "")).to_lower();
	if (type == "button") return ButtonAction::BUTTON;
	if (!owner(p_node)) return ButtonAction::NONE;
	return type == "reset" ? ButtonAction::RESET : ButtonAction::SUBMIT;
}

std::vector<element::ptr> controls(const element& p_form) {
	std::vector<element::ptr> result;
	HTMLDOM::visit_elements(root_of(p_form), [&](const element::ptr& p_node) {
		if (is_listed(*p_node) && owner(*p_node).get() == &p_form) result.push_back(p_node);
		return false;
	});
	return result;
}

std::vector<element::ptr> datalist_options(const element& p_control) {
	std::vector<element::ptr> options;
	const auto* id = p_control.get_attr("list");
	const auto doc = p_control.get_document();
	if (!id || !*id || !doc || root_of(p_control) != doc->root()) return options;
	const auto list = by_id(doc->root(), id);
	if (!list || list->tag() != _datalist_) return options;
	HTMLDOM::visit_elements(list, [&](const element::ptr& p_node) {
		if (p_node->tag() == _option_ && !is_disabled(*p_node)) options.push_back(p_node);
		return false;
	});
	return options;
}

std::vector<element::ptr> radio_group(const element& p_radio) {
	const std::string_view name(p_radio.get_attr("name", ""));
	if (name.empty()) return {std::const_pointer_cast<element>(p_radio.shared_from_this())};
	const auto form = owner(p_radio);
	std::vector<element::ptr> result;
	HTMLDOM::visit_elements(root_of(p_radio), [&](const element::ptr& p_node) {
		const auto* input = dynamic_cast<const HTMLInputNode*>(p_node.get());
		if (input && input->type() == HTMLInputNode::Type::RADIO && name == input->get_attr("name", "") && owner(*input) == form) {
			result.push_back(p_node);
		}
		return false;
	});
	return result;
}

element::ptr first_invalid(const element& p_form) {
	for (const auto& node : controls(p_form)) {
		if (!control_validity(*node).value_or(true)) return node;
	}
	return {};
}

std::vector<element::ptr> invalid_controls(const element& p_form) {
	std::vector<element::ptr> result;
	for (const auto& node : controls(p_form)) {
		if (!control_validity(*node).value_or(true)) result.push_back(node);
	}
	return result;
}

bool update_validity(element& p_control) {
	std::unordered_map<const element*, element::ptr> containers;
	bool changed = false;
	const auto update_control = [&](element& p_node, std::optional<bool> p_radio_missing = {}) {
		if (!set_validity(p_node, control_validity(p_node, p_radio_missing))) return;
		changed = true;
		visit_validation_containers(p_node, [&](const auto& p_container) { containers.emplace(p_container.get(), p_container); });
	};
	if (const auto* input = dynamic_cast<const HTMLInputNode*>(&p_control); input && input->type() == HTMLInputNode::Type::RADIO) {
		const auto group = radio_group(p_control);
		const bool missing = radio_value_missing(group);
		for (const auto& peer : group) update_control(*peer, missing);
	} else {
		update_control(p_control);
	}
	for (const auto& [_, container] : containers) {
		if (container->tag() == _form_) {
			changed |= set_validity(*container, !first_invalid(*container));
		} else {
			bool valid = true;
			HTMLDOM::visit_elements(container, [&](const element::ptr& p_node) {
				valid = control_validity(*p_node).value_or(true);
				return !valid;
			});
			changed |= set_validity(*container, valid);
		}
	}
	return changed;
}

void synchronize(const element::ptr& p_tree, const element::ptr& p_preferred) {
	std::unordered_set<const element*> preferred;
	if (p_preferred) HTMLDOM::visit_elements(p_preferred, [&](const element::ptr& p_node) {
		if (const auto* radio = dynamic_cast<const HTMLInputNode*>(p_node.get()); radio && radio->type() == HTMLInputNode::Type::RADIO && radio->is_checked()) {
			preferred.insert(radio);
		}
		return false;
	});
	std::unordered_set<const element*> handled_radios;
	std::unordered_set<const element*> missing_radios;
	const auto reconcile = [&](const element::ptr& p_node) {
		auto* radio = dynamic_cast<HTMLInputNode*>(p_node.get());
		if (!radio || radio->type() != HTMLInputNode::Type::RADIO || handled_radios.count(radio)) return false;
		const auto peers = radio_group(*radio);
		HTMLInputNode* checked = nullptr;
		for (const auto& peer : peers) {
			handled_radios.insert(peer.get());
			auto* candidate = static_cast<HTMLInputNode*>(peer.get());
			if (candidate->is_checked() && (!checked || preferred.count(candidate) || !preferred.count(checked))) checked = candidate;
		}
		for (const auto& peer : peers) {
			auto* candidate = static_cast<HTMLInputNode*>(peer.get());
			if (candidate != checked) candidate->clear_group_checked();
		}
		if (radio_value_missing(peers)) for (const auto& peer : peers) missing_radios.insert(peer.get());
		return false;
	};
	if (!p_tree) return;
	const auto root = root_of(*p_tree);
	// Checked nodes moved into a group take precedence over its old selection.
	HTMLDOM::visit_elements(root, reconcile);
	std::vector<element::ptr> containers;
	std::unordered_set<const element*> invalid_containers;
	HTMLDOM::visit_elements(root, [&](const element::ptr& p_node) {
		if (auto* input = dynamic_cast<HTMLTextControlNode*>(p_node.get())) input->update_pseudos();
		else if ((is_listed(*p_node) && p_node->tag() != _output_) || p_node->tag() == _option_ || p_node->tag() == _optgroup_) {
			const bool disabled = is_disabled(*p_node);
			p_node->set_pseudo_class(_id("disabled"), disabled);
			p_node->set_pseudo_class(_id("enabled"), !disabled);
		}
		if (auto* select = dynamic_cast<HTMLSelectNode*>(p_node.get())) {
			select->sync_mutation_revision();
			const auto options = select->get_options();
			const auto selected = select->selected_index(options);
			for (size_t i = 0; i < options.size(); ++i) options[i]->set_pseudo_class(_id("checked"),
				select->is_multiple() ? select->is_option_selected(*options[i]) : static_cast<int64_t>(i) == selected);
		}
		if (const auto* option = dynamic_cast<const HTMLOptionNode*>(p_node.get()); option && !option->select()) {
			p_node->set_pseudo_class(_id("checked"), option->is_selected());
		}
		if (p_node->tag() == _form_ || p_node->tag() == _fieldset_) containers.push_back(p_node);
		else if (is_listed(*p_node)) {
			const auto validity = control_validity(*p_node, missing_radios.count(p_node.get()) != 0);
			set_validity(*p_node, validity);
			if (!validity.value_or(true)) {
				visit_validation_containers(*p_node, [&](const auto& p_container) { invalid_containers.insert(p_container.get()); });
			}
		}
		return false;
	});
	for (const auto& container : containers) set_validity(*container, !invalid_containers.count(container.get()));
}
}
