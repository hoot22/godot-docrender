#pragma once

#include <litehtml/element.h>

#include <vector>

namespace dockit::forms {
	enum class ButtonAction { NONE, BUTTON, SUBMIT, RESET };

bool is_listed(const litehtml::element& p_node);
bool is_disabled(const litehtml::element& p_node);
bool supports_required(const litehtml::element& p_node);
bool is_read_write(const litehtml::element& p_node);
litehtml::element::ptr owner(const litehtml::element& p_node);
std::vector<litehtml::element::ptr> labels(const litehtml::element& p_control);
litehtml::element::ptr labeled_control(const litehtml::element& p_label);
ButtonAction button_action(const litehtml::element& p_node);
std::vector<litehtml::element::ptr> controls(const litehtml::element& p_form);
std::vector<litehtml::element::ptr> radio_group(const litehtml::element& p_radio);
std::vector<litehtml::element::ptr> datalist_options(const litehtml::element& p_control);
litehtml::element::ptr first_invalid(const litehtml::element& p_form);
std::vector<litehtml::element::ptr> invalid_controls(const litehtml::element& p_form);
bool update_validity(litehtml::element& p_control);
void synchronize(const litehtml::element::ptr& p_tree, const litehtml::element::ptr& p_preferred = {});
}
