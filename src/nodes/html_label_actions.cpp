#include "html_label.h"

#include "formats/html/dom/html_dom.h"
#include "formats/html/dom/html_document_fragment.h"
#include "formats/html/dom/html_range.h"
#include "formats/html/dom/html_selection.h"
#include "formats/html/elements/html_form_elements.h"
#include "formats/html/elements/html_input_node.h"
#include "formats/html/events/html_event.h"
#include "formats/html/html_layout.h"
#include "formats/html/text/html_text_selection.h"
#include "nodes/html_selection_drag_preview.h"

#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/object_id.hpp>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

using namespace godot;

using dockit::HTMLInputNode;

namespace {
	std::string ascii_lower(std::string_view p_value) {
		std::string result(p_value);
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char p_byte) { return char(std::tolower(p_byte)); });
		return result;
	}

	bool safe_editing_href(std::string_view p_value) {
		const std::string value = ascii_lower(p_value);
		const auto first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) return true;
		const std::string_view trimmed(value.data() + first, value.size() - first);
		const auto begins = [&](std::string_view p_prefix) { return trimmed.size() >= p_prefix.size() && trimmed.compare(0, p_prefix.size(), p_prefix) == 0; };
		return !begins("javascript:") && !begins("data:") && !begins("vbscript:") && !begins("file:");
	}

	Ref<HTMLElement> find_draggable_ancestor(const Ref<HTMLElement>& p_element) {
		for (Ref<HTMLElement> current = p_element; current.is_valid(); current = current->get_parent()) {
			if (current->get_attribute("draggable").strip_edges().to_lower() == "true") return current;
		}
		return {};
	}

	std::string escape_html_attribute(std::string_view p_value) {
		std::string result;
		for (const char ch : p_value) {
			switch (ch) {
				case '&': result += "&amp;"; break;
				case '"': result += "&quot;"; break;
				case '<': result += "&lt;"; break;
				case '>': result += "&gt;"; break;
				default: result += ch;
			}
		}
		return result;
	}

	std::string sanitize_editing_style(std::string_view p_value) {
		static constexpr std::string_view safe_properties[] = {"color", "background-color", "font-family", "font-size",
			"font-weight", "font-style", "text-decoration", "text-align", "vertical-align", "white-space"};
		const std::string source(p_value);
		const std::string lower = ascii_lower(source);
		if (lower.find("url(") != std::string::npos || lower.find("expression") != std::string::npos ||
			lower.find("@import") != std::string::npos || lower.find("javascript:") != std::string::npos) return {};
		std::string result;
		for (size_t start = 0; start < source.size();) {
			const size_t end = source.find(';', start);
			const std::string_view declaration(source.data() + start,
				(end == std::string::npos ? source.size() : end) - start);
			const size_t colon = declaration.find(':');
			if (colon != std::string_view::npos) {
				const std::string name = ascii_lower(declaration.substr(0, colon));
				const auto first = name.find_first_not_of(" \t\r\n");
				const auto last = name.find_last_not_of(" \t\r\n");
				const std::string property = first == std::string::npos ? std::string() : name.substr(first, last - first + 1);
				const auto value_start = declaration.find_first_not_of(" \t\r\n", colon + 1);
				const std::string value = value_start == std::string_view::npos ? std::string() :
					std::string(declaration.substr(value_start));
				if (!value.empty() && std::find(std::begin(safe_properties), std::end(safe_properties), property) != std::end(safe_properties)) {
					if (!result.empty()) result += ' ';
					result += property + ": " + value + ';';
				}
			}
			if (end == std::string::npos) break;
			start = end + 1;
		}
		return result;
	}

	String sanitize_editing_html(const String& p_html) {
		const CharString encoded = p_html.utf8();
		const std::string_view source(encoded.get_data(), encoded.length());
		static constexpr std::string_view safe_tags[] = {"p", "div", "br", "h1", "h2", "h3", "h4", "h5", "h6",
			"blockquote", "pre", "ul", "ol", "li", "a", "strong", "b", "em", "i", "u", "s", "del", "sub", "sup", "code", "span"};
		static constexpr std::string_view blocked_content[] = {"script", "style", "iframe", "object", "embed", "form", "input", "textarea", "button"};
		std::string output;
		std::string suppressed_tag;
		for (size_t index = 0; index < source.size();) {
			if (source[index] != '<') {
				const size_t next = source.find('<', index);
				if (suppressed_tag.empty()) output.append(source.substr(index, (next == std::string_view::npos ? source.size() : next) - index));
				index = next == std::string_view::npos ? source.size() : next;
				continue;
			}
			bool quoted = false;
			char quote = 0;
			size_t end = index + 1;
			for (; end < source.size(); ++end) {
				const char ch = source[end];
				if (quoted) { if (ch == quote) quoted = false; }
				else if (ch == '\'' || ch == '"') { quoted = true; quote = ch; }
				else if (ch == '>') break;
			}
			if (end >= source.size()) { if (suppressed_tag.empty()) output += "&lt;"; ++index; continue; }
			std::string_view token = source.substr(index + 1, end - index - 1);
			index = end + 1;
			while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) token.remove_prefix(1);
			bool closing = !token.empty() && token.front() == '/';
			if (closing) token.remove_prefix(1);
			size_t name_end = 0;
			while (name_end < token.size() && (std::isalnum(static_cast<unsigned char>(token[name_end])) || token[name_end] == '-')) ++name_end;
			if (name_end == 0) continue;
			const std::string tag = ascii_lower(token.substr(0, name_end));
			if (!suppressed_tag.empty()) { if (closing && tag == suppressed_tag) suppressed_tag.clear(); continue; }
			if (!closing && std::find(std::begin(blocked_content), std::end(blocked_content), tag) != std::end(blocked_content)) {
				suppressed_tag = tag;
				continue;
			}
			if (std::find(std::begin(safe_tags), std::end(safe_tags), tag) == std::end(safe_tags)) continue;
			if (closing) { if (tag != "br") output += "</" + tag + ">"; continue; }
			output += '<' + tag;
			token.remove_prefix(name_end);
			while (!token.empty()) {
				while (!token.empty() && (std::isspace(static_cast<unsigned char>(token.front())) || token.front() == '/')) token.remove_prefix(1);
				if (token.empty()) break;
				size_t attr_end = 0;
				while (attr_end < token.size() && !std::isspace(static_cast<unsigned char>(token[attr_end])) && token[attr_end] != '=' && token[attr_end] != '/') ++attr_end;
				if (attr_end == 0) { token.remove_prefix(1); continue; }
				const std::string attr = ascii_lower(token.substr(0, attr_end));
				token.remove_prefix(attr_end);
				while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) token.remove_prefix(1);
				std::string value;
				if (!token.empty() && token.front() == '=') {
					token.remove_prefix(1);
					while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) token.remove_prefix(1);
					if (!token.empty() && (token.front() == '\'' || token.front() == '"')) {
						const char delimiter = token.front(); token.remove_prefix(1);
						const size_t value_end = token.find(delimiter);
						value = std::string(token.substr(0, value_end));
						if (value_end == std::string_view::npos) token = {};
						else token.remove_prefix(value_end + 1);
					} else {
					const size_t value_end = token.find_first_of(" \t\r\n/");
					value = std::string(token.substr(0, value_end));
					if (value_end == std::string_view::npos) token = {}; else token.remove_prefix(value_end);
				}
				}
				if (attr == "title" || (tag == "a" && attr == "href" && safe_editing_href(value))) {
					output += ' ' + attr + "=\"" + escape_html_attribute(value) + '"';
				} else if (attr == "style") {
					const std::string style = sanitize_editing_style(value);
					if (!style.empty()) output += " style=\"" + escape_html_attribute(style) + '"';
				}
			}
			output += tag == "br" ? "/>" : ">";
		}
		return String::utf8(output.c_str());
	}

	Control* make_html_drag_preview(const Ref<HTMLLayout>& p_layout, const Vector2i& p_selection,
		const TypedArray<Rect2>& p_selection_rects, const String& p_text) {
		Rect2 bounds;
		for (int64_t index = 0; index < p_selection_rects.size(); ++index) {
			const Rect2 rect = p_selection_rects[index];
			if (rect.has_area()) bounds = bounds.has_area() ? bounds.merge(rect) : rect;
		}
		if (p_layout.is_valid() && bounds.has_area()) {
			return memnew(HTMLSelectionDragPreview(p_layout, p_selection, bounds));
		}

		Label* preview = memnew(Label);
		preview->set_text(p_text);
		preview->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
		preview->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		return preview;
	}

	Control* make_html_element_drag_preview(const Ref<HTMLLayout>& p_layout, const Ref<HTMLElement>& p_element,
		const String& p_text) {
		const Rect2 bounds = p_element.is_valid() ? p_element->get_bounding_rect() : Rect2();
		if (p_layout.is_valid() && bounds.has_area()) {
			return memnew(HTMLSelectionDragPreview(p_layout, Vector2i(), bounds, true));
		}
		Label* preview = memnew(Label);
		preview->set_text(p_text);
		preview->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
		preview->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		return preview;
	}
}

HTMLSelectionDragPreview::HTMLSelectionDragPreview(const Ref<HTMLLayout>& p_layout, const Vector2i& p_selection,
	const Rect2& p_bounds, bool p_draw_element) : layout(p_layout), selection(p_selection), selection_bounds(p_bounds),
	draw_element(p_draw_element) {
	set_size(p_bounds.size);
	set_custom_minimum_size(p_bounds.size);
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	set_modulate(Color(1.0, 1.0, 1.0, 0.75));
}

void HTMLSelectionDragPreview::_draw() {
	if (layout.is_valid()) {
		const Rect2 clip(Vector2(), get_size());
		if (draw_element) {
			layout->draw_with_highlights(this, -selection_bounds.position, clip, Vector2i(), nullptr, Color(0, 0, 0, 0));
		} else {
			layout->draw_selection_drag_image(this, selection, -selection_bounds.position, clip);
		}
	}
}

bool HTMLLabelCanvas::dispatch_drag_event(const StringName& p_type, const Ref<HTMLElement>& p_target,
	const Ref<HTMLDataTransfer>& p_transfer, const Vector2& p_position, const Ref<HTMLElement>& p_related_target) {
	if (!label || label->layout.is_null() || p_target.is_null() || !p_target->is_valid()) return true;
	Ref<HTMLEvent> base_event = dockit::make_dom_event(p_type, true,
		p_type == StringName("dragstart") || p_type == StringName("drag") || p_type == StringName("dragenter") ||
		p_type == StringName("dragover") || p_type == StringName("drop"));
	Ref<HTMLDragEvent> event = base_event;
	if (event.is_null()) return true;
	if (p_transfer.is_valid()) event->set_data_transfer(p_transfer);
	event->set_position(p_position - label->get_visible_rect().position);
	event->set_buttons(mouse_buttons);
	if (p_related_target.is_valid()) event->set_related_target(p_related_target);
	label->layout->get_dom().dispatch_event(p_target, event);
	return !event->is_default_prevented();
}

void HTMLLabelCanvas::update_drop_target(const Ref<HTMLElement>& p_target, const String& p_text,
	const Vector2& p_position) const {
	if (drop_target == p_target && drop_text == p_text) return;
	if (drop_target.is_valid()) {
		Ref<HTMLDataTransfer> transfer = dockit::get_current_drag_transfer();
		if (transfer.is_null()) { transfer.instantiate(); transfer->set_text(drop_text); }
		transfer->set_mode(HTMLDataTransfer::MODE_READ_ONLY);
		const_cast<HTMLLabelCanvas*>(this)->dispatch_drag_event(StringName("dragleave"), drop_target, transfer,
			p_position, p_target);
		transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
	}
	drop_target = p_target;
	drop_text = p_text;
	drop_accepted = false;
	if (drop_target.is_valid()) {
		Ref<HTMLDataTransfer> transfer = dockit::get_current_drag_transfer();
		if (transfer.is_null()) { transfer.instantiate(); transfer->set_text(drop_text); }
		transfer->set_mode(HTMLDataTransfer::MODE_READ_ONLY);
		const_cast<HTMLLabelCanvas*>(this)->dispatch_drag_event(StringName("dragenter"), drop_target, transfer, p_position);
		transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
	}
}

void HTMLLabelCanvas::clear_drop_target(const Vector2& p_position) const {
	if (drop_target.is_valid()) {
		Ref<HTMLDataTransfer> transfer = dockit::get_current_drag_transfer();
		if (transfer.is_null()) { transfer.instantiate(); transfer->set_text(drop_text); }
		transfer->set_mode(HTMLDataTransfer::MODE_READ_ONLY);
		const_cast<HTMLLabelCanvas*>(this)->dispatch_drag_event(StringName("dragleave"), drop_target, transfer, p_position);
		transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
	}
	drop_target.unref();
	drop_text = String();
	drop_accepted = false;
}

Variant HTMLLabelCanvas::begin_selection_drag(const Vector2& p_position, bool p_force_native_drag) {
	const Variant inherited_data = Control::_get_drag_data(p_position);
	if (inherited_data.get_type() != Variant::NIL) return inherited_data;
	if (native_drag_started) return drag_text;
	if (!label || !label->drag_and_drop_selection_enabled || !selection_drag_attempt ||
		!label->selection_enabled || label->layout.is_null()) return {};
	const String selected_text = label->get_selected_text();
	if (selected_text.is_empty()) return {};

	Ref<HTMLDataTransfer> transfer;
	transfer.instantiate();
	transfer->set_text(selected_text);
	transfer->set_effect_allowed("copyMove");
	transfer->set_mode(HTMLDataTransfer::MODE_READ_WRITE);
	const Ref<HTMLElement> source = drag_source.is_valid() ? drag_source : label->layout->get_document_element();
	if (const Ref<HTMLSelection> selection = label->layout->get_selection(); selection.is_valid() && selection->get_range_count() > 0) {
		if (const Ref<HTMLRange> range = selection->get_range_at(0); range.is_valid()) {
			if (const Ref<HTMLDocumentFragment> fragment = range->clone_contents(); fragment.is_valid()) {
				if (const auto node = fragment->lock_element()) transfer->set_data("text/html", label->layout->get_dom().serialize_children(*node));
			}
		}
	}
	if (!dispatch_drag_event(StringName("dragstart"), source, transfer, p_position)) return {};
	const String drag_text = transfer->get_data("text/plain");
	if (transfer->get_types().is_empty()) return {};
	transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
	dockit::set_current_drag_transfer(transfer);
	this->drag_text = drag_text;
	drag_source = source;
	native_drag_started = true;
	drag_elapsed = 0.0;
	set_process(true);
	cancel_click();
	selecting = false;
	selection_dragged = false;
	PointerInfo pointer;
	pointer.position = p_position;
	pointer.target = source;
	pointer.buttons = 0;
	dispatch_pointer_event(StringName("pointercancel"), source, pointer, false);
	Control* preview = make_html_drag_preview(label->layout, label->get_selection_range(), label->get_selection_rects(), drag_text);
	const Variant payload = drag_text.is_empty() ? Variant(Dictionary()) : Variant(drag_text);
	if (p_force_native_drag) {
		force_drag(payload, preview);
	} else {
		set_drag_preview(preview);
	}
	return payload;
}

Variant HTMLLabelCanvas::begin_element_drag(const Vector2& p_position, bool p_force_native_drag) {
	const Variant inherited_data = Control::_get_drag_data(p_position);
	if (inherited_data.get_type() != Variant::NIL) return inherited_data;
	if (native_drag_started) return drag_text;
	if (!label || !drag_source.is_valid() || label->layout.is_null()) return {};

	Ref<HTMLDataTransfer> transfer;
	transfer.instantiate();
	transfer->set_effect_allowed("copyMove");
	transfer->set_mode(HTMLDataTransfer::MODE_READ_WRITE);
	String text = drag_source->get_text_content().strip_edges();
	if (text.is_empty()) text = drag_source->get_attribute("href");
	if (text.is_empty()) text = drag_source->get_attribute("alt");
	if (text.is_empty()) text = drag_source->get_attribute("src");
	if (!text.is_empty()) transfer->set_data("text/plain", text);
	const String html = drag_source->get_outer_html();
	if (!html.is_empty()) transfer->set_data("text/html", html);
	if (!dispatch_drag_event(StringName("dragstart"), drag_source, transfer, p_position) || transfer->get_types().is_empty()) {
		element_drag_attempt = false;
		drag_source.unref();
		return {};
	}

	drag_text = transfer->get_data("text/plain");
	transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
	dockit::set_current_drag_transfer(transfer);
	native_drag_started = true;
	element_drag_attempt = false;
	drag_elapsed = 0.0;
	set_process(true);
	cancel_click();
	selecting = false;
	selection_dragged = false;
	PointerInfo pointer;
	pointer.position = p_position;
	pointer.target = drag_source;
	pointer.buttons = 0;
	dispatch_pointer_event(StringName("pointercancel"), drag_source, pointer, false);
	Control* preview = make_html_element_drag_preview(label->layout, drag_source, drag_text);
	Variant payload = drag_text.is_empty() ? Variant(Dictionary()) : Variant(drag_text);
	if (p_force_native_drag) {
		force_drag(payload, preview);
	} else {
		set_drag_preview(preview);
	}
	return payload;
}

Variant HTMLLabelCanvas::_get_drag_data(const Vector2& p_at_position) {
	if (element_drag_attempt) return begin_element_drag(p_at_position, false);
	return begin_selection_drag(p_at_position, false);
}

bool HTMLLabelCanvas::_can_drop_data(const Vector2& p_at_position, const Variant& p_data) const {
	Ref<HTMLDataTransfer> transfer = dockit::get_current_drag_transfer();
	const bool string_payload = p_data.get_type() == Variant::STRING && !String(p_data).is_empty();
	const bool transfer_payload = transfer.is_valid() && !transfer->get_types().is_empty() &&
		(p_data.get_type() == Variant::DICTIONARY || string_payload);
	if (!label || label->layout.is_null() || (!string_payload && !transfer_payload)) {
		clear_drop_target(p_at_position);
		return false;
	}
	Ref<HTMLElement> target = label->layout->get_element_at(p_at_position);
	if (target.is_null()) target = label->layout->get_document_element();
	if (target.is_null()) {
		clear_drop_target(p_at_position);
		return false;
	}
	if (transfer.is_null()) { transfer.instantiate(); transfer->set_text(String(p_data)); }
	transfer->set_mode(HTMLDataTransfer::MODE_READ_ONLY);
	const String text = transfer->get_data("text/plain");
	update_drop_target(target, text, p_at_position);
	transfer->set_drop_effect("copy");
	transfer->set_mode(HTMLDataTransfer::MODE_READ_ONLY);
	const bool dragover_prevented = !const_cast<HTMLLabelCanvas*>(this)->dispatch_drag_event(
		StringName("dragover"), target, transfer, p_at_position);
	const bool drop_effect_disabled = transfer->get_drop_effect() == "none";
	transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
	const auto control = label->layout->text_control(target);
	const bool text_input = control && control->is_text_control() && control->is_editable();
	const bool accepts_text = !text.is_empty() && (text_input || target->is_content_editable());
	drop_accepted = !drop_effect_disabled && (dragover_prevented || accepts_text);
	return drop_accepted;
}

void HTMLLabelCanvas::_drop_data(const Vector2& p_at_position, const Variant& p_data) {
	Ref<HTMLDataTransfer> transfer = dockit::get_current_drag_transfer();
	const bool string_payload = p_data.get_type() == Variant::STRING && !String(p_data).is_empty();
	const bool transfer_payload = transfer.is_valid() && !transfer->get_types().is_empty() &&
		(p_data.get_type() == Variant::DICTIONARY || string_payload);
	if (!label || label->layout.is_null() || (!string_payload && !transfer_payload)) {
		clear_drop_target(p_at_position);
		return;
	}
	if (!_can_drop_data(p_at_position, p_data)) {
		clear_drop_target(p_at_position);
		return;
	}
	if (transfer.is_null()) { transfer.instantiate(); transfer->set_text(String(p_data)); }
	transfer->set_mode(HTMLDataTransfer::MODE_READ_ONLY);
	const String text = transfer->get_data("text/plain");
	const Ref<HTMLElement> target = drop_target;
	const bool drop_not_canceled = dispatch_drag_event(StringName("drop"), target, transfer, p_at_position);
	if (drop_not_canceled) {
		const auto control = label->layout->text_control(target);
		if (!text.is_empty() && control && control->is_text_control() && control->is_editable() && !control->editor.has_preedit() &&
			label->layout->is_element_connected(target) && label->focus_element(target)) {
			set_input_caret(target, p_at_position, false);
			if (label->dispatch_input_event(StringName("beforeinput"), target, StringName("insertFromDrop"), text, false)) {
				const auto* field = dynamic_cast<const HTMLInputNode*>(control.get());
				const String maxlength_value = field ? String(field->maxlength()) : control->get_attr("maxlength", "");
				const CharString maxlength = maxlength_value.utf8();
				if (control->editor.insert(text, maxlength.get_data())) {
					change_input(target, true, StringName("insertFromDrop"), text, false);
				}
			}
		}
		else if (!text.is_empty() && target.is_valid() && target->is_content_editable() && label->layout->is_element_connected(target) &&
			label->focus_element(target)) {
			const auto hit = label->layout->selection_point_at(p_at_position);
			label->set_selection_range(hit.offset, hit.offset);
			Ref<HTMLSelection> selection = label->layout->get_selection();
			Ref<HTMLRange> range = selection.is_valid() && selection->get_range_count() > 0 ? selection->get_range_at(0) : Ref<HTMLRange>();
			if (range.is_valid() && label->dispatch_input_event(StringName("beforeinput"), target,
				StringName("insertFromDrop"), text, false)) {
				const String html = transfer->get_data("text/html");
				bool inserted = false;
				if (!html.is_empty()) {
					const String clean_html = sanitize_editing_html(html);
					const Ref<HTMLDocumentFragment> fragment = label->layout->get_dom().parse_fragment(clean_html, target->lock_element());
					if (fragment.is_valid() && range->delete_contents() == OK && range->insert_node(fragment) == OK) inserted = true;
				}
				if (!inserted && range->delete_contents() == OK) {
					const Ref<HTMLTextNode> text_node = label->layout->create_text_node(text);
					inserted = text_node.is_valid() && range->insert_node(text_node) == OK;
				}
				if (inserted) {
					range->collapse(false);
					selection->collapse(range->get_end_container(), range->get_end_offset());
					label->dispatch_input_event(StringName("input"), target, StringName("insertFromDrop"), text, false);
					label->queue_interaction();
				}
			}
		}
	}
	transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
	clear_drop_target(p_at_position);
}

void HTMLLabelCanvas::_process(double p_delta) {
	if (!native_drag_started || !drag_source.is_valid()) {
		set_process(false);
		return;
	}
	drag_elapsed += p_delta;
	if (drag_elapsed < 0.35) return;
	drag_elapsed = 0.0;
	Ref<HTMLDataTransfer> transfer = dockit::get_current_drag_transfer();
	if (transfer.is_null()) {
		transfer.instantiate();
		transfer->set_text(drag_text);
	}
	transfer->set_mode(HTMLDataTransfer::MODE_PROTECTED);
	if (!dispatch_drag_event(StringName("drag"), drag_source, transfer, get_local_mouse_position())) {
		if (Viewport* viewport = get_viewport()) viewport->gui_cancel_drag();
	}
}

int HTMLLabelCanvas::click_detail_for(const Ref<HTMLElement>& p_target, const Vector2& p_position) const {
	if (click_count == 0 || p_target.is_null() || !p_target->is_valid() || last_click_target != p_target ||
		last_click_version != p_target->adoption_version) return 1;
	const uint64_t now = Time::get_singleton()->get_ticks_msec();
	if (now < last_click_msec || now - last_click_msec >= 500 ||
		p_position.distance_squared_to(last_click_position) >= 25.0) return 1;
	return click_count + 1;
}

void HTMLLabelCanvas::record_click(const Ref<HTMLElement>& p_target, const Vector2& p_position, int p_detail) {
	if (p_target.is_null() || !p_target->is_valid()) {
		reset_click_sequence();
		return;
	}
	click_count = p_detail;
	last_click_msec = Time::get_singleton()->get_ticks_msec();
	last_click_version = p_target->adoption_version;
	last_click_position = p_position;
	last_click_target = p_target;
}

void HTMLLabelCanvas::reset_click_sequence() {
	click_count = 0;
	last_click_msec = 0;
	last_click_version = 0;
	last_click_target.unref();
}

namespace {
	constexpr int DRAG_DEADZONE = 6;

	std::optional<bool> tab_direction(const InputEventKey& p_key) {
		if (p_key.is_action_pressed("ui_focus_prev", true, true)) return true;
		if (p_key.is_action_pressed("ui_focus_next", true, true)) return false;
		return std::nullopt;
	}
}

void HTMLLabelCanvas::accept_pointer_event(const PointerInfo& p_info) {
	if (p_info.accepts_event) accept_event();
}

bool HTMLLabelCanvas::handle_wheel_input(const PointerInfo& p_info, float p_pixels_per_step) {
	if (!label || label->layout.is_null() || p_info.source_event.is_null()) return true;
	auto& dom = label->layout->get_dom();
	const Ref<HTMLElement> target = label->layout->get_element_at(p_info.position);
	if (target.is_null() || !target->is_valid()) return true;
	const Vector2 steps = HTMLLayout::scroll_input_steps(p_info.source_event);
	if (steps.is_zero_approx()) return true;
	Ref<HTMLWheelEvent> event;
	event.instantiate();
	event->set_type(StringName("wheel"));
	event->set_bubbles(true);
	event->set_cancelable(true);
	event->mark_trusted();
	event->set_delta(steps * p_pixels_per_step);
	event->set_delta_mode(HTMLWheelEvent::DELTA_MODE_PIXEL);
	event->set_position(p_info.position - label->get_visible_rect().position);
	event->set_shift_pressed(p_info.shift);
	event->set_ctrl_pressed(p_info.ctrl);
	event->set_alt_pressed(p_info.alt);
	event->set_meta_pressed(p_info.meta);
	dom.dispatch_event(target, event);
	label->wheel_dispatched_in_canvas = true;
	const uint64_t sequence = ++label->wheel_dispatch_sequence;
	callable_mp(label, &HTMLLabel::clear_canvas_wheel_dispatch).call_deferred(sequence);
	return !event->is_default_prevented();
}

void HTMLLabelCanvas::queue_pointer_move_event(const StringName& p_type, const Ref<HTMLElement>& p_target,
	const PointerInfo& p_info) {
	auto pending = std::find_if(pending_move_events.begin(), pending_move_events.end(), [&](const PendingMoveEvent& p_event) {
		return !p_event.is_touch && p_event.type == p_type && p_event.pointer.pointer_id == p_info.pointer_id;
	});
	if (pending == pending_move_events.end()) {
		pending_move_events.push_back({p_type, p_target, p_info, {}, false});
	} else {
		pending->target = p_target;
		pending->pointer = p_info;
	}
	if (!movement_flush_queued) {
		movement_flush_queued = true;
		callable_mp(this, &HTMLLabelCanvas::flush_move_events).call_deferred();
	}
}

void HTMLLabelCanvas::queue_touch_move_event(const StringName& p_type, const TouchPointer& p_touch) {
	auto pending = std::find_if(pending_move_events.begin(), pending_move_events.end(), [&](const PendingMoveEvent& p_event) {
		return p_event.is_touch && p_event.type == p_type && p_event.touch.pointer_id == p_touch.pointer_id;
	});
	if (pending == pending_move_events.end()) {
		PendingMoveEvent event;
		event.type = p_type;
		event.touch = p_touch;
		event.is_touch = true;
		pending_move_events.push_back(event);
	} else {
		pending->touch = p_touch;
	}
	if (!movement_flush_queued) {
		movement_flush_queued = true;
		callable_mp(this, &HTMLLabelCanvas::flush_move_events).call_deferred();
	}
}

void HTMLLabelCanvas::flush_move_events() {
	movement_flush_queued = false;
	std::vector<PendingMoveEvent> pending = std::move(pending_move_events);
	pending_move_events.clear();
	const ObjectID self(get_instance_id());
	for (const PendingMoveEvent& event : pending) {
		if (!ObjectDB::get_instance(self)) return;
		if (event.is_touch) dispatch_touch_event(event.type, event.touch);
		else dispatch_pointer_event(event.type, event.target, event.pointer, false);
	}
}

HTMLLabelCanvas::PointerInfo HTMLLabelCanvas::make_touch_pointer_info(const TouchPointer& p_touch,
	const Vector2& p_position, bool p_pressed, bool p_canceled) const {
	PointerInfo info;
	info.position = p_position;
	info.button = MOUSE_BUTTON_LEFT;
	info.buttons = p_pressed ? 1 : 0;
	info.pressed = p_pressed;
	info.canceled = p_canceled;
	info.device = p_touch.device;
	info.touch_index = p_touch.index;
	info.pointer_id = p_touch.pointer_id;
	info.detail = p_touch.detail;
	info.kind = PointerKind::TOUCH;
	info.primary = p_touch.primary;
	info.accepts_event = false;
	info.target = p_touch.target;
	return info;
}

bool HTMLLabelCanvas::dispatch_touch_event(const StringName& p_type, const TouchPointer& p_changed) {
	if (!label || label->layout.is_null()) return true;
	Ref<HTMLElement> target = p_changed.target;
	if (target.is_null() || !target->is_valid()) target = label->layout->get_body();
	if (target.is_null() || !target->is_valid()) return true;
	Ref<HTMLTouchEvent> event;
	event.instantiate();
	event->set_type(p_type);
	event->set_bubbles(true);
	event->set_cancelable(p_type == StringName("touchstart") || p_type == StringName("touchmove"));
	event->set_composed(true);
	event->mark_trusted();
	TypedArray<HTMLTouchPoint> touches;
	TypedArray<HTMLTouchPoint> target_touches;
	for (const auto& [key, touch] : active_touch_pointers) {
		Ref<HTMLTouchPoint> point;
		point.instantiate();
		point->set_identifier(touch.pointer_id);
		point->set_target(touch.target);
		point->set_position(touch.last - label->get_visible_rect().position);
		touches.push_back(point);
		if (touch.target.ptr() == p_changed.target.ptr()) target_touches.push_back(point);
	}
	Ref<HTMLTouchPoint> changed;
	changed.instantiate();
	changed->set_identifier(p_changed.pointer_id);
	changed->set_target(p_changed.target);
	changed->set_position(p_changed.last - label->get_visible_rect().position);
	TypedArray<HTMLTouchPoint> changed_touches;
	changed_touches.push_back(changed);
	event->set_touches(touches);
	event->set_target_touches(target_touches);
	event->set_changed_touches(changed_touches);
	return label->layout->get_dom().dispatch_event(target, event);
}

Ref<InputEventMouseButton> HTMLLabelCanvas::make_touch_mouse_event(const PointerInfo& p_info) const {
	Ref<InputEventMouseButton> event;
	event.instantiate();
	event->set_button_index(MouseButton(MOUSE_BUTTON_LEFT));
	event->set_pressed(p_info.pressed);
	event->set_canceled(p_info.canceled);
	event->set_device(p_info.device);
	event->set_position(p_info.position);
	return event;
}

void HTMLLabelCanvas::handle_touch_press(const InputEventScreenTouch& p_touch) {
	flush_move_events();
	if (!label || label->layout.is_null()) return;
	const TouchKey key{ p_touch.get_device(), p_touch.get_index() };
	if (active_touch_pointers.find(key) != active_touch_pointers.end()) cancel_touch_pointer(key.first, key.second);
	auto id = touch_pointer_ids.find(key);
	if (id == touch_pointer_ids.end()) id = touch_pointer_ids.emplace(key, next_touch_pointer_id++).first;
	TouchPointer touch;
	touch.device = key.first;
	touch.index = key.second;
	touch.pointer_id = id->second;
	touch.start = p_touch.get_position();
	touch.last = p_touch.get_position();
	touch.target = label->layout->get_element_at(touch.start);
	touch.detail = click_detail_for(touch.target, touch.start - label->get_visible_rect().position);
	touch.primary = active_touch_pointers.empty();
	touch.compatibility_suppressed = !touch.primary;
	auto [active, inserted] = active_touch_pointers.emplace(key, touch);
	if (!inserted) return;
	remember_interaction_targets();
	PointerInfo info = make_touch_pointer_info(touch, touch.start, true, false);
	const bool pointer_allowed = dispatch_pointer_event(StringName("pointerdown"), touch.target, info, true);
	active = active_touch_pointers.find(key);
	if (active == active_touch_pointers.end()) return;
	const TouchPointer after_pointer = active->second;
	const bool touch_allowed = dispatch_touch_event(StringName("touchstart"), after_pointer);
	active = active_touch_pointers.find(key);
	if (active == active_touch_pointers.end()) return;
	if (!pointer_allowed || !touch_allowed) active->second.compatibility_suppressed = true;
	if (active->second.compatibility_suppressed) return;
	Ref<InputEventMouseButton> mouse = make_touch_mouse_event(info);
	info.source_event = mouse;
	info.mouse_down_prevented = !dispatch_pointer_event(StringName("mousedown"), touch.target, info, true);
	if (active_touch_pointers.find(key) == active_touch_pointers.end()) return;
	handle_pointer_press(info);
}

void HTMLLabelCanvas::handle_touch_drag(const InputEventScreenDrag& p_drag) {
	const TouchKey key{ p_drag.get_device(), p_drag.get_index() };
	auto active = active_touch_pointers.find(key);
	if (active == active_touch_pointers.end() || active->second.canceled) return;
	TouchPointer& touch = active->second;
	touch.last = p_drag.get_position();
	PointerInfo info = make_touch_pointer_info(touch, touch.last, true, false);
	info.detail = 0;
	if (!touch.dragged && (touch.last - touch.start).length_squared() > DRAG_DEADZONE * DRAG_DEADZONE) {
		touch.dragged = true;
		if (touch.primary) cancel_press();
	}
	if (label->viewport_touch_dragged) {
		cancel_touch_pointer(key.first, key.second);
		return;
	}
	const TouchPointer snapshot = touch;
	if (!snapshot.dragged && !snapshot.compatibility_suppressed) handle_pointer_move(info);
	else queue_pointer_move_event(StringName("pointermove"), snapshot.target, info);
	if (active_touch_pointers.find(key) != active_touch_pointers.end()) queue_touch_move_event(StringName("touchmove"), snapshot);
}

void HTMLLabelCanvas::handle_touch_release(const InputEventScreenTouch& p_touch) {
	flush_move_events();
	const TouchKey key{ p_touch.get_device(), p_touch.get_index() };
	auto active = active_touch_pointers.find(key);
	if (active == active_touch_pointers.end()) return;
	TouchPointer touch = active->second;
	active_touch_pointers.erase(active);
	if (p_touch.is_canceled()) {
		reset_click_sequence();
		PointerInfo canceled = make_touch_pointer_info(touch, p_touch.get_position(), false, true);
		dispatch_pointer_event(StringName("pointercancel"), touch.target, canceled, false);
		dispatch_touch_event(StringName("touchcancel"), touch);
		if (touch.primary) cancel_press();
		return;
	}
	touch.last = p_touch.get_position();
	PointerInfo info = make_touch_pointer_info(touch, touch.last, false, false);
	info.detail = click_detail_for(touch.target, touch.last - label->get_visible_rect().position);
	dispatch_pointer_event(StringName("pointerup"), touch.target, info, false);
	dispatch_touch_event(StringName("touchend"), touch);
	const Ref<HTMLElement> release_target = label->layout->get_element_at(touch.last);
	if (!touch.primary || touch.compatibility_suppressed || touch.dragged || release_target != touch.target) {
		reset_click_sequence();
		if (touch.primary) cancel_press();
		return;
	}
	Ref<InputEventMouseButton> mouse = make_touch_mouse_event(info);
	info.source_event = mouse;
	dispatch_pointer_event(StringName("mouseup"), release_target, info, false);
	handle_pointer_release(info);
}

void HTMLLabelCanvas::cancel_touch_pointer(int p_device, int p_index) {
	flush_move_events();
	const TouchKey key{ p_device, p_index };
	auto active = active_touch_pointers.find(key);
	if (active == active_touch_pointers.end()) return;
	const TouchPointer touch = active->second;
	active_touch_pointers.erase(active);
	PointerInfo info = make_touch_pointer_info(touch, touch.last, false, true);
	dispatch_pointer_event(StringName("pointercancel"), touch.target, info, false);
	dispatch_touch_event(StringName("touchcancel"), touch);
	if (touch.primary) cancel_press();
}

void HTMLLabelCanvas::cancel_all_touch_pointers() {
	while (!active_touch_pointers.empty()) {
		const TouchKey key = active_touch_pointers.begin()->first;
		cancel_touch_pointer(key.first, key.second);
	}
}

void HTMLLabelCanvas::handle_pointer_move(const PointerInfo& p_info) {
	if (!label || label->layout.is_null()) return;
	const Rect2 visible_rect = label->get_visible_rect();
	const Vector2 point = p_info.position;
	const Ref<HTMLElement> event_target = p_info.target.is_valid() ? p_info.target : label->layout->get_element_at(point);
	const auto queue_moves = [&]() {
		queue_pointer_move_event(StringName("pointermove"), event_target, p_info);
		if (p_info.kind == PointerKind::MOUSE) queue_pointer_move_event(StringName("mousemove"), event_target, p_info);
	};
	update_dialog_pointer(point, p_info.device, p_info.touch_index);
	if (native_drag_started) return;
	if (selection_drag_attempt || element_drag_attempt) {
		const Vector2 selection_press = pressed_position + visible_rect.position;
		if ((p_info.buttons & MOUSE_BUTTON_MASK_LEFT) &&
			point.distance_squared_to(selection_press) > DRAG_DEADZONE * DRAG_DEADZONE) {
			if (selection_drag_attempt) begin_selection_drag(point, true);
			else begin_element_drag(point, true);
		}
		queue_moves();
		return;
	}
	if (move_video_control(point) || move_audio_control(point)) {
		queue_moves();
		accept_pointer_event(p_info);
		return;
	}
	if (dragging_range.is_valid()) {
		const auto range = dragging_range;
		queue_moves();
		accept_pointer_event(p_info);
		move_range(range, point, false);
		return;
	}
	if (scrollbar_drag) {
		label->layout->drag_scrollbar(*scrollbar_drag, point);
		queue_moves();
		accept_pointer_event(p_info);
		return;
	}
	if (box_drag) {
		move_box_drag(point);
		queue_moves();
		accept_pointer_event(p_info);
		return;
	}
	if (dragging_input.is_valid()) {
		set_input_caret(dragging_input, point, true);
		label->request_hover_check();
		queue_moves();
		accept_pointer_event(p_info);
		return;
	}
	if (pressed_checkbox.is_valid() && (!visible_rect.has_point(point) ||
		label->layout->get_element_at(point) != pressed_checkbox)) {
		set_element_pseudo(pressed_checkbox, "active", false);
		pressed_checkbox.unref();
	}
	const bool moved = (point - visible_rect.position).distance_squared_to(pressed_position) > DRAG_DEADZONE * DRAG_DEADZONE;
	if (!visible_rect.has_point(point) || (selecting && moved)) {
		cancel_click();
	}
	else {
		if (moved) {
			pressed_blank = false;
			pressed_link = 0;
			pressed_element.unref();
		}
		if (pressed_button.is_valid() &&
			label->layout->get_button_ancestor(label->layout->get_element_at(point)) != pressed_button) {
			cancel_button_press();
		}
	}
	if (selecting && (moved || selection_dragged)) {
		selection_dragged = true;
		const Vector2 clamped = point.clamp(visible_rect.position, visible_rect.get_end());
		extend_document_selection(clamped);
		set_process_internal(true);
		queue_redraw();
		accept_pointer_event(p_info);
	}
	label->request_hover_check();
	queue_moves();
}

void HTMLLabelCanvas::handle_pointer_press(const PointerInfo& p_info) {
	flush_move_events();
	if (!label || label->layout.is_null()) return;
	const Ref<InputEventMouseButton> button = p_info.source_event;
	if (button.is_null()) return;
	if (!native_drag_started) {
		suppress_next_release = false;
		selection_drag_attempt = false;
		element_drag_attempt = false;
		drag_source.unref();
	}
	const Rect2 visible_rect = label->get_visible_rect();
	const Vector2 point = p_info.position;
	Ref<HTMLElement> element;
	const HTMLLayout::LinkHit hit = visible_rect.has_point(point)
		? label->layout->find_link_at(point, &element) : HTMLLayout::LinkHit{};
	const Ref<HTMLElement> form_target = hit.id == 0 ? get_form_click_target(element) : Ref<HTMLElement>();
	if (p_info.mouse_down_prevented) {
		pressed_position = point - visible_rect.position;
		pressed_link = hit.id;
		pressed_element = element;
		pressed_form_target = form_target;
		pressed_blank = false;
		selecting = false;
		selection_dragged = false;
		accept_pointer_event(p_info);
		return;
	}
	if (handle_video_button(*button.ptr(), point) || handle_audio_button(*button.ptr(), point)) {
		accept_pointer_event(p_info);
		return;
	}
	if (visible_rect.has_point(point) &&
		label->handle_listbox_mouse(label->layout->get_element_at(point), *button.ptr(), point)) {
		cancel_press();
		accept_pointer_event(p_info);
		return;
	}
	if (visible_rect.has_point(point)) {
		if (begin_resize_drag(point)) {
			accept_pointer_event(p_info);
			return;
		}
		if (const auto drag = label->layout->scrollbar_at(point)) {
			cancel_press();
			scrollbar_drag = drag;
			label->layout->drag_scrollbar(*drag, point);
			set_default_cursor_shape(CURSOR_ARROW);
			accept_pointer_event(p_info);
			return;
		}
	}
	if (element.is_null() && label->layout->top_modal().is_valid()) {
		cancel_press();
		accept_pointer_event(p_info);
		return;
	}
	const Ref<HTMLElement> target_button = label->layout->get_button_ancestor(element);
	if (handle_input_decoration(Ref<HTMLInputElement>(element), *button.ptr(), point, p_info.accepts_event)) return;
	const Ref<HTMLInputElement> target_input = element;
	if (target_input.is_valid() && target_input->get_input()->type() == HTMLInputNode::Type::RANGE &&
		label->layout->is_focusable(target_input)) {
		cancel_press();
		if (label->focus_element(target_input)) {
			dragging_range = target_input;
			remember_interaction_targets();
			accept_pointer_event(p_info);
			move_range(target_input, point, false);
		}
		return;
	}
	if (begin_textarea_scrollbar_drag(element, point)) {
		accept_pointer_event(p_info);
		return;
	}
	cancel_button_press();
	cancel_key_press();
	pressed_position = point - visible_rect.position;
	pressed_link = hit.id;
	pressed_element = element;
	pressed_form_target = form_target;
	pressed_blank = false;
	selection_drag_attempt = false;
	suppress_next_release = false;
	selecting = visible_rect.has_point(point) && label->layout->hit_test_selectable_text(point, &pressed_blank) &&
		label->selection_enabled;
	pressed_blank = pressed_blank && hit.id == 0 && target_button.is_null() && form_target.is_null();
	const Vector2i current_selection = label->get_selection_range();
	if (p_info.button == MOUSE_BUTTON_LEFT && label->drag_and_drop_selection_enabled &&
		current_selection.x != current_selection.y && selecting) {
		for (const Rect2& rect : label->get_selection_rects()) {
			if (!rect.has_point(point)) continue;
			selection_drag_attempt = true;
			drag_source = element.is_valid() ? element : label->layout->get_document_element();
			selecting = false;
			selection_dragged = false;
			cancel_click();
			accept_pointer_event(p_info);
			return;
		}
	}
	if (p_info.button == MOUSE_BUTTON_LEFT && !selection_drag_attempt) {
		const Ref<HTMLElement> draggable = find_draggable_ancestor(element);
		if (draggable.is_valid()) {
			element_drag_attempt = true;
			drag_source = draggable;
			selecting = false;
		}
	}
	if (selecting) {
		selection_origin = label->layout->selection_point_at(point);
		capture_selection_origin();
		selecting = !selection_origin.excluded;
	}
	selection_dragged = false;
	remember_interaction_targets();
	const Ref<HTMLElement> target_focus = label->layout->get_focus_ancestor(element);
	if (target_focus.is_valid()) label->grab_focus();
	set_focused_element(target_focus);
	const auto control = label->layout->text_control(element);
	if (control && (control->is_text_control() || (target_input.is_valid() && target_input->get_input()->is_toggle()))) {
		selecting = false;
		pressed_blank = false;
		pressed_element.unref();
		pressed_link = 0;
		if (target_focus == element) {
			if (control->is_text_control()) {
				dragging_input = element;
				remember_interaction_targets();
				set_input_caret(element, point, p_info.shift);
				if (control->editor.is_multiline()) set_process_internal(true);
			}
			else if (target_input.is_valid() && target_input->get_input()->is_toggle()) {
				pressed_checkbox = target_input;
				set_element_pseudo(pressed_checkbox, "active", true);
			}
		}
		accept_pointer_event(p_info);
		return;
	}
	if (label->layout->is_enabled_button(target_button)) {
		pressed_button = target_button;
		set_element_pseudo(pressed_button, "active", true);
	}
	if (selecting) {
		const Vector2i selection = dockit::html_selection_between(selection_origin, selection_origin);
		selection_anchor = selection.x;
		selection_caret = selection.y;
		label->capture_node_selection();
		if (target_button.is_null()) label->grab_focus();
		queue_redraw();
	}
	if (selecting || hit.id != 0 || target_button.is_valid() || form_target.is_valid() || label->layout->top_modal_node()) {
		accept_pointer_event(p_info);
	}
}

void HTMLLabelCanvas::handle_pointer_release(const PointerInfo& p_info) {
	flush_move_events();
	if (!label || label->layout.is_null()) return;
	reset_click_sequence();
	const Ref<InputEventMouseButton> button = p_info.source_event;
	if (button.is_null()) return;
	const Rect2 visible_rect = label->get_visible_rect();
	const Vector2 point = p_info.position;
	const Ref<HTMLElement> event_target = label->layout->get_element_at(point);
	if (handle_video_button(*button.ptr(), point) || handle_audio_button(*button.ptr(), point)) {
		accept_pointer_event(p_info);
		return;
	}
	if (p_info.button == MOUSE_BUTTON_LEFT && dragging_range.is_valid()) {
		const auto range = dragging_range;
		dragging_range.unref();
		accept_pointer_event(p_info);
		if (!p_info.canceled) move_range(range, point, true);
		else commit_input(range);
		return;
	}
	if (scrollbar_drag) {
		if (!p_info.canceled) {
			label->layout->drag_scrollbar(*scrollbar_drag, point);
			label->layout->finish_scrollbar_drag(*scrollbar_drag);
		}
		scrollbar_drag.reset();
		label->layout->set_scrollbar_pressed(std::nullopt);
		label->request_hover_check();
		accept_pointer_event(p_info);
		return;
	}
	if (box_drag) {
		if (!p_info.canceled) move_box_drag(point);
		flush_box_resize();
		box_drag.reset();
		label->layout->set_scrollbar_pressed(std::nullopt);
		label->request_hover_check();
		accept_pointer_event(p_info);
		return;
	}
	if (suppress_next_release || native_drag_started) {
		suppress_next_release = false;
		accept_pointer_event(p_info);
		return;
	}
	if (selection_drag_attempt) {
		selection_drag_attempt = false;
		if (!p_info.canceled) {
			for (const Rect2& rect : label->get_selection_rects()) {
				if (rect.has_point(point)) {
					label->clear_selection();
					break;
				}
			}
		}
		drag_source.unref();
		accept_pointer_event(p_info);
		return;
	}
	if (element_drag_attempt) {
		element_drag_attempt = false;
		drag_source.unref();
	}
	Ref<HTMLElement> element;
	const HTMLLayout::LinkHit hit = visible_rect.has_point(point)
		? label->layout->find_link_at(point, &element) : HTMLLayout::LinkHit{};
	const Ref<HTMLElement> target_button = label->layout->get_button_ancestor(element);
	const Ref<HTMLElement> form_target = hit.id == 0 ? get_form_click_target(element) : Ref<HTMLElement>();
	const Ref<HTMLInputElement> target_input = element;
	if (handle_input_decoration(Ref<HTMLInputElement>(element), *button.ptr(), point, p_info.accepts_event)) return;
	if (dragging_input.is_valid()) {
		if (!p_info.canceled && point != pressed_position + visible_rect.position) set_input_caret(dragging_input, point, true);
		dragging_input.unref();
		set_process_internal(false);
		accept_pointer_event(p_info);
		return;
	}
	if (pressed_checkbox.is_valid()) {
		const Ref<HTMLInputElement> checkbox = pressed_checkbox;
		set_element_pseudo(checkbox, "active", false);
		pressed_checkbox.unref();
		accept_pointer_event(p_info);
		if (!p_info.canceled && checkbox == target_input) {
			const Ref<HTMLElement> checkbox_target = checkbox;
			const Vector2 click_position = point - visible_rect.position;
			record_click(checkbox_target, click_position, p_info.detail);
			if (dispatch_pointer_event(StringName("click"), checkbox_target, p_info, true)) {
				toggle_checkbox(checkbox);
			}
			if (p_info.detail == 2) dispatch_pointer_event(StringName("dblclick"), checkbox_target, p_info, true);
		}
		return;
	}
	const bool clicked = p_info.button == MOUSE_BUTTON_LEFT && !selection_dragged && !p_info.canceled &&
		(point - visible_rect.position).distance_squared_to(pressed_position) <= DRAG_DEADZONE * DRAG_DEADZONE;
	const bool aux_clicked = p_info.button != MOUSE_BUTTON_LEFT && !selection_dragged && !p_info.canceled &&
		(point - visible_rect.position).distance_squared_to(pressed_position) <= DRAG_DEADZONE * DRAG_DEADZONE;
	const bool blank_clicked = clicked && pressed_blank && visible_rect.has_point(point);
	const bool link_clicked = clicked && pressed_link != 0 && pressed_link == hit.id;
	const bool element_clicked = clicked && element.is_valid() && pressed_element == element;
	const uint64_t click_version = element_clicked ? element->adoption_version : 0;
	const bool button_clicked = !selection_dragged && !p_info.canceled &&
		pressed_button == target_button && label->layout->is_enabled_button(target_button);
	const bool form_clicked = clicked && form_target.is_valid() && pressed_form_target == form_target;
	if (selecting && selection_dragged && !p_info.canceled) {
		extend_document_selection(point.clamp(visible_rect.position, visible_rect.get_end()));
		queue_redraw();
	}
	if (selecting || link_clicked || pressed_button.is_valid() || form_clicked || label->layout->top_modal_node()) {
		accept_pointer_event(p_info);
	}
	const bool was_dragging = selection_dragged;
	cancel_press();
	if (blank_clicked) label->clear_selection();
	if (was_dragging) {
		label->request_hover_check();
		callable_mp(label->layout.ptr(), &HTMLLayout::request_auto_content_update).call_deferred();
	}
	const ObjectID label_id(label->get_instance_id());
	// 可取消的 click 在对应默认动作之前派发。
	if (clicked) record_click(event_target, point - visible_rect.position, p_info.detail);
	const bool click_prevented = clicked && !dispatch_pointer_event(StringName("click"), event_target,
		p_info, true);
	if (aux_clicked) dispatch_pointer_event(StringName("auxclick"), event_target, p_info, true);
	if (link_clicked && !click_prevented && label->layout->is_interaction_target_available(element, true) &&
		element->adoption_version == click_version) {
		label->activate_link(hit.uri);
	}
	if (button_clicked && !click_prevented) {
		if (auto* target = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(label_id))) {
			const Ref<HTMLInputElement> image = target_button;
			if (image.is_valid() && image->get_input() && image->get_input()->type() == HTMLInputNode::Type::IMAGE) {
				const auto local = target->layout->input_local_point(*image.ptr(), point);
				target->activate_form_control(image, local ? image->get_input()->image_coordinates(*local) : Vector2i());
			}
			else target->activate_element(target_button);
		}
	}
	else if (form_clicked && !click_prevented) {
		if (auto* target = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(label_id))) target->activate_element(form_target);
	}
	if (element_clicked && label->layout->is_interaction_target_available(element, true) && element->adoption_version == click_version) {
		if (auto* target = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(label_id))) {
			target->emit_signal("element_clicked", element);
		}
	}
	if (clicked && p_info.detail == 2 && event_target.is_valid()) {
		dispatch_pointer_event(StringName("dblclick"), event_target, p_info, true);
	}
}

bool HTMLLabelCanvas::handle_key_default_action(const InputEventKey& p_key) {
	const bool modal = label->layout->top_modal().is_valid();
	if (const auto direction = tab_direction(p_key); direction && (!tab_elements.empty() || modal)) {
		label->close_select_menu();
		const bool backwards = *direction;
		if (tab_elements.empty()) {
			label->accept_event();
			focus_tab_endpoint(backwards);
			return true;
		}
		if (move_video_focus(backwards) || move_audio_focus(backwards)) {
			accept_event();
			return true;
		}
		auto button = std::find(tab_elements.begin(), tab_elements.end(), focused_element);
		if (button != tab_elements.end()) {
			if (backwards && button != tab_elements.begin()) {
				--button;
			}
			else if (!backwards && std::next(button) != tab_elements.end()) {
				++button;
			}
			else {
				const auto* next = backwards ? label->find_prev_valid_focus() : label->find_next_valid_focus();
				if (!modal && next && next != label) {
					return false;
				}
				button = backwards ? std::prev(tab_elements.end()) : tab_elements.begin();
			}
		}
		else {
			button = backwards ? std::prev(tab_elements.end()) : tab_elements.begin();
		}
		accept_event();
		set_focused_element(*button, true);
		set_audio_focus_edge(backwards);
		set_video_focus_edge(backwards);
		return true;
	}

	if (!modal || label->layout->allows_modal_interaction(focused_element)) {
		if (handle_video_key(p_key) || handle_audio_key(p_key) || label->handle_select_key(p_key) ||
			handle_input_key(p_key) || handle_editable_key(p_key)) return true;
	}
	if (label->handle_dialog_escape(p_key)) return true;
	if (modal && !label->layout->allows_modal_interaction(focused_element)) return false;
	const Key key = p_key.get_keycode() != KEY_NONE ? p_key.get_keycode() : p_key.get_physical_keycode();
	if ((key == KEY_ENTER || key == KEY_KP_ENTER) && p_key.is_pressed() && !p_key.is_echo() &&
		!p_key.is_alt_pressed() && !p_key.is_ctrl_pressed() && !p_key.is_meta_pressed()) {
		const String uri = label->layout->get_activatable_link_uri(focused_element);
		if (!uri.is_empty()) {
			label->accept_event();
			label->activate_link(uri);
			return true;
		}
	}
	if (key != KEY_ENTER && key != KEY_KP_ENTER && key != KEY_SPACE) {
		return false;
	}
	if (!label->layout->is_enabled_button(focused_element)) {
		cancel_key_press();
		return false;
	}
	if (p_key.is_alt_pressed() || p_key.is_ctrl_pressed() || p_key.is_meta_pressed()) {
		cancel_key_press();
		return false;
	}
	label->accept_event();
	cancel_click();
	if (p_key.is_echo()) {
		return true;
	}
	if (p_key.is_pressed() && activation_key == KEY_NONE && pressed_button.is_null()) {
		activation_key = key;
		set_element_pseudo(focused_element, "active", true);
		if (key != KEY_SPACE) {
			const Ref<HTMLElement> target = focused_element;
			label->activate_element(target);
		}
	}
	else if (!p_key.is_pressed() && activation_key == key) {
		const Ref<HTMLElement> target = focused_element;
		cancel_key_press();
		if (key == KEY_SPACE && !p_key.is_canceled()) {
			label->activate_element(target);
		}
	}
	return true;
}
