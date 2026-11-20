#include "html_select_node.h"
#include "html_control_render_item.h"

#include "html_form_state.h"
#include "formats/html/backend/litehtml_container.h"

#include <litehtml/document.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <utility>

using namespace godot;

namespace dockit {
namespace {
	String option_text(const litehtml::element& p_option) {
		litehtml::string text;
		const auto append_text = [&](const auto& p_visit, const litehtml::element& p_node) -> void {
			if (litehtml::is_generated_tag(p_node.tag()) || p_node.tag() == litehtml::_script_) return;
			if (p_node.is_text()) p_node.get_text(text);
			for (const auto& child : p_node.children()) p_visit(p_visit, *child);
		};
		append_text(append_text, p_option);
		std::string collapsed;
		bool space = false;
		for (const char ch : text) {
			if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f') {
				space = !collapsed.empty();
			} else {
				if (space) collapsed += ' ';
				space = false;
				collapsed += ch;
			}
		}
		return String::utf8(collapsed.c_str());
	}
}

HTMLSelectNode* HTMLOptionNode::select() const {
	auto node = parent();
	if (node && node->tag() == litehtml::_optgroup_) node = node->parent();
	return dynamic_cast<HTMLSelectNode*>(node.get());
}

String HTMLOptionNode::get_value() const {
	if (const char* value = get_attr("value")) return String::utf8(value);
	return option_text(*this);
}

String HTMLOptionNode::get_label() const {
	if (const char* label = get_attr("label"); label && *label) return String::utf8(label);
	return option_text(*this);
}

bool HTMLOptionNode::is_selected() const {
	if (const auto* owner = select()) {
		const auto options = owner->get_options();
		if (owner->is_multiple()) return owner->is_option_selected(*this);
		const auto index = owner->selected_index(options);
		return index >= 0 && options[index].get() == this;
	}
	return selected.value_or(get_attr("selected") != nullptr);
}

void HTMLOptionNode::assign_selected(bool p_selected, bool p_dirty) {
	const bool changed = is_selected() != p_selected;
	selected = p_selected;
	if (changed) {
		note_dom_change(DOM_STATE);
		if (auto* owner = select()) owner->note_dom_change(DOM_STATE);
	}
	dirty |= p_dirty;
}

void HTMLOptionNode::set_selected(bool p_selected) {
	if (auto* owner = select()) {
		const auto options = owner->get_options();
		if (p_selected && !owner->is_multiple()) {
			const auto item = std::find_if(options.begin(), options.end(), [this](const auto& p_option) { return p_option.get() == this; });
			owner->set_selected_index(options, std::distance(options.begin(), item));
			return;
		}
	}
	assign_selected(p_selected, true);
}

void HTMLOptionNode::attribute_changed() {
	if (!dirty) {
		selected.reset();
		if (get_attr("selected")) {
			if (auto* owner = select(); owner && !owner->is_multiple()) {
				for (const auto& option : owner->get_options()) {
					if (option.get() != this) option->assign_selected(false, false);
				}
			}
		}
	}
}

bool HTMLOptionNode::copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode) const {
	auto* target = dynamic_cast<HTMLOptionNode*>(&p_target);
	if (!target) return false;
	target->selected = is_selected();
	target->dirty = dirty;
	return true;
}

HTMLSelectNode::Options HTMLSelectNode::get_options() const {
	Options options;
	const auto add = [&](const litehtml::element::ptr& p_element) {
		if (auto option = std::dynamic_pointer_cast<HTMLOptionNode>(p_element)) options.push_back(std::move(option));
	};
	for (const auto& child : m_children) {
		if (child->tag() == litehtml::_optgroup_) {
			for (const auto& option : child->children()) add(option);
		} else add(child);
	}
	return options;
}

HTMLSelectNode::Options HTMLSelectNode::get_list_options() const {
	auto options = get_options();
	options.erase(std::remove_if(options.begin(), options.end(), [](const auto& option) {
		const auto parent = option->parent();
		return option->get_attr("hidden") || (parent && parent->tag() == litehtml::_optgroup_ && parent->get_attr("hidden"));
	}), options.end());
	return options;
}

void HTMLSelectNode::sync_mutation_revision() {
	const auto options = get_options();
	const auto index = selected_index(options);
	std::vector<SelectionSnapshot> next;
	std::unordered_map<const HTMLOptionNode*, bool> previous;
	for (const auto& item : selection_snapshot) {
		if (const auto option = item.option.lock()) previous.emplace(option.get(), item.selected);
	}
	String value;
	bool changed = false;
	for (size_t i = 0; i < options.size(); ++i) {
		const bool selected = is_multiple() ? is_option_selected(*options[i]) : static_cast<int64_t>(i) == index;
		next.push_back({options[i], selected});
		if (static_cast<int64_t>(i) == index) value = options[i]->get_value();
		const auto found = previous.find(options[i].get());
		if (has_selection_snapshot && found != previous.end() && found->second != selected) {
			options[i]->note_dom_change(DOM_STATE);
			changed = true;
		}
	}
	if (has_selection_snapshot && (changed || selected_value_snapshot != value)) note_dom_change(DOM_STATE);
	selection_snapshot = std::move(next);
	selected_value_snapshot = value;
	has_selection_snapshot = true;
}

void HTMLSelectNode::use_single_selection() {
	const auto options = get_options();
	const auto index = selected_index(options);
	for (size_t i = 0; i < options.size(); ++i) {
		if (index != static_cast<int64_t>(i) && is_option_selected(*options[i])) options[i]->assign_selected(false, false);
	}
	active_row = anchor_row = -1;
	scroll_row = 0;
}

int64_t HTMLSelectNode::get_size() const {
	std::string_view value(get_attr("size", ""));
	const auto first = value.find_first_not_of(" \t\r\n\f");
	if (first == std::string_view::npos) return 0;
	value.remove_prefix(first);
	if (value.front() == '+') value.remove_prefix(1);
	uint32_t parsed = 0;
	const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
	return result.ec == std::errc() ? parsed : 0;
}

int64_t HTMLSelectNode::display_size() const {
	const auto rows = get_size();
	return rows > 0 ? rows : is_multiple() ? 4 : 1;
}

bool HTMLSelectNode::is_option_selected(const HTMLOptionNode& p_option) const {
	return p_option.selected.value_or(p_option.get_attr("selected") != nullptr);
}

int64_t HTMLSelectNode::selected_index(const Options& p_options) const {
	int64_t fallback = -1;
	int64_t chosen = -1;
	for (size_t i = 0; i < p_options.size(); ++i) {
		const auto& option = *p_options[i];
		if (is_option_selected(option)) {
			if (is_multiple()) return i;
			chosen = i;
		}
		if (fallback < 0 && !forms::is_disabled(option)) fallback = i;
	}
	return chosen >= 0 || allow_empty || is_list_box() ? chosen : fallback;
}

void HTMLSelectNode::set_selected_index(const Options& p_options, int64_t p_index) {
	std::vector<bool> before;
	for (const auto& option : p_options) before.push_back(option->is_selected());
	allow_empty = p_index < 0 || p_index >= static_cast<int64_t>(p_options.size());
	bool changed = false;
	for (size_t i = 0; i < p_options.size(); ++i) {
		const bool selected = p_index == static_cast<int64_t>(i);
		p_options[i]->selected = selected;
		p_options[i]->dirty |= selected;
		if (before[i] != selected) {
			p_options[i]->note_dom_change(DOM_STATE);
			changed = true;
		}
	}
	if (changed) note_dom_change(DOM_STATE);
}

bool HTMLSelectNode::is_missing() const {
	const auto options = get_options();
	const auto index = selected_index(options);
	return index < 0 || (!is_list_box() && index == 0 && options.front()->parent().get() == this && options.front()->get_value().is_empty());
}

void HTMLSelectNode::reset() {
	const auto options = get_options();
	std::vector<bool> before;
	for (const auto& option : options) before.push_back(option->is_selected());
	allow_empty = false;
	active_row = anchor_row = -1;
	scroll_row = 0;
	for (const auto& option : get_options()) {
		option->selected.reset();
		option->dirty = false;
	}
	bool changed = false;
	for (size_t i = 0; i < options.size(); ++i) {
		if (before[i] != options[i]->is_selected()) {
			options[i]->note_dom_change(DOM_STATE);
			changed = true;
		}
	}
	if (changed) note_dom_change(DOM_STATE);
}

bool HTMLSelectNode::copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const {
	auto* target = dynamic_cast<HTMLSelectNode*>(&p_target);
	if (!target) return false;
	target->allow_empty = allow_empty;
	if (p_mode == HTMLNodeCopyMode::ADOPT) {
		target->scroll_row = scroll_row;
		target->active_row = active_row;
		target->anchor_row = anchor_row;
	}
	return true;
}

std::shared_ptr<litehtml::render_item> HTMLSelectNode::create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) {
	const auto display = css().get_display();
	if (display == litehtml::display_inline_block || display == litehtml::display_block) {
		auto* container = get_document()->container();
		litehtml::pixel_t width = 0;
		for (const auto& option : get_options()) {
			const auto text = option->get_label().utf8();
			width = std::max(width, container->text_width(text.get_data(), css().get_font()));
		}
		if (!is_list_box() && css().get_appearance() != litehtml::appearance_none) width += 18;
		auto item = std::make_shared<HTMLControlRenderItem>(shared_from_this(),
			litehtml::size(width, css().line_height().computed_value));
		item->parent(p_parent);
		return item;
	}
	struct RestoreChildren {
		litehtml::elements_list& target;
		litehtml::elements_list children;
		~RestoreChildren() { target.swap(children); }
	} restore{m_children, std::exchange(m_children, {})};
	return html_tag::create_render_item(p_parent);
}

int HTMLSelectNode::list_row_at(const Vector2& p_point) const {
	if (!list_box.has_point(p_point)) return -1;
	return scroll_row + static_cast<int>((p_point.y - list_box.position.y) / row_height);
}

bool HTMLSelectNode::scroll_list(int p_rows) {
	const int visible = std::max(1, static_cast<int>(list_box.size.y / row_height));
	const int next = std::clamp(scroll_row + p_rows, 0, std::max(0, static_cast<int>(get_list_options().size()) - visible));
	if (next == scroll_row) return false;
	scroll_row = next;
	return true;
}

bool HTMLSelectNode::select_list_row(int p_row, bool p_extend, bool p_toggle, bool p_move_only) {
	const auto options = get_list_options();
	if (p_row < 0 || p_row >= static_cast<int>(options.size()) || forms::is_disabled(*options[p_row])) return false;
	const int previous = active_row;
	active_row = p_row;
	const int visible = std::max(1, static_cast<int>(list_box.size.y / row_height));
	scroll_row = std::clamp(scroll_row, std::max(0, p_row - visible + 1), p_row);
	if (p_move_only) return false;
	bool changed = false;
	if (!p_extend || anchor_row < 0 || anchor_row >= static_cast<int>(options.size())) anchor_row = p_extend && previous >= 0 ? previous : p_row;
	const int first = std::min(anchor_row, p_row);
	const int last = std::max(anchor_row, p_row);
	if (is_multiple() && p_toggle && !p_extend) {
		options[p_row]->assign_selected(!options[p_row]->is_selected(), true);
		changed = true;
	} else {
		const auto all_options = get_options();
		const bool extend = is_multiple() && p_extend;
		std::unordered_set<const HTMLOptionNode*> selected;
		for (int i = extend ? first : p_row; i <= (extend ? last : p_row); ++i) {
			if (!forms::is_disabled(*options[i])) selected.insert(options[i].get());
		}
		for (const auto& option : all_options) {
			const bool next = selected.count(option.get()) != 0;
			if (option->is_selected() == next) continue;
			option->assign_selected(next, true);
			changed = true;
		}
	}
	allow_empty = true;
	return changed;
}

void HTMLSelectNode::draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
	const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) {
	html_tag::draw(p_hdc, p_x, p_y, p_clip, p_item);
	const auto pos = p_item->pos();
	const Rect2 box(pos.x + p_x, pos.y + p_y, pos.width, pos.height);
	if (!box.has_area()) return;
	auto* container = static_cast<LiteHTMLContainer*>(get_document()->container());
	const auto options = get_options();
	const auto index = selected_index(options);
	const auto paint = [&](const String& p_text, const Rect2& p_clip_box, litehtml::web_color p_color, const css::ComputedStyle& p_style) {
		auto line = container->shape_input(p_style.get_font(), p_text);
		if (!line.is_valid() || !p_clip_box.has_area()) return;
		const auto& font = p_style.get_font_metrics();
		const float ascent = font.has_metric_overrides ? font.ascent : line.line_ascent();
		const float descent = font.has_metric_overrides ? font.descent : line.line_descent();
		const Vector2 baseline(p_clip_box.position.x, p_clip_box.get_center().y + (ascent - descent) * 0.5f);
		container->draw_input(p_clip_box, line, baseline, p_color, {}, {}, {});
	};
	if (is_list_box()) {
		list_box = box;
		const auto list_options = get_list_options();
		const int count = static_cast<int>(list_options.size());
		row_height = std::max(1.0, static_cast<double>(css().line_height().computed_value) + 2.0);
		scroll_list(0);
		const int visible = std::min(count - scroll_row, static_cast<int>(std::ceil(box.size.y / row_height)));
		for (int i = 0; i < visible; ++i) {
			const int row = scroll_row + i;
			const auto& option = *list_options[row];
			const bool selected = option.is_selected();
			const Rect2 area(box.position.x, box.position.y + i * row_height, box.size.x, row_height);
			if (selected || row == active_row) {
				container->draw_input_chrome(box, area,
					html_system_color(selected ? litehtml::system_color::list_selected : litehtml::system_color::list_hover,
						css().get_color_scheme()));
			}
			const auto ink = forms::is_disabled(option) ?
				litehtml::system_color_value(litehtml::system_color::gray_text, option.css().get_color_scheme()) :
				selected && !option.has_author_property(litehtml::_color_) ? litehtml::web_color::white : option.css().get_color();
			paint(option.get_label(), area.intersection(box), ink, option.css());
		}
		return;
	}
	const bool arrow = css().get_appearance() != litehtml::appearance_none;
	const bool rtl = css().is_right_to_left();
	const Rect2 text_box(box.position + Vector2(arrow && rtl ? 16 : 0, 0), Vector2(box.size.x - (arrow ? 16 : 0), box.size.y));
	if (index >= 0) paint(options[index]->get_label(), text_box, css().get_color(), css());
	if (arrow) {
		const auto color = css().get_color().to_srgb();
		container->draw_input_icon(dropdown_open ? HTMLControlIcon::up : HTMLControlIcon::down, box,
			Rect2(rtl ? box.position.x : box.get_end().x - 16, box.get_center().y - 8, 16, 16),
			Color::from_rgba8(color.red, color.green, color.blue, color.alpha));
	}
}
}
