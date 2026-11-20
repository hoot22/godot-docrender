#include "html_text_editor.h"
#include "html_control_render_item.h"

#include "formats/html/backend/litehtml_container.h"

#include <godot_cpp/classes/text_server_manager.hpp>
#include <litehtml/document.h>
#include <litehtml/html_tag.h>
#include "formats/html/css/css_computed_style.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

using namespace godot;

namespace dockit {
	namespace {
		constexpr size_t MAX_UNDO_EDITS = 100;

		Ref<TextServer> text_server() {
			return TextServerManager::get_singleton()->get_primary_interface();
		}

		PackedInt32Array breaks(const String& p_value) {
			return text_server()->string_get_character_breaks(p_value);
		}

		int64_t nearest_boundary(const String& p_value, int64_t p_offset) {
			p_offset = std::clamp<int64_t>(p_offset, 0, p_value.length());
			int64_t previous = 0;
			const auto boundaries = breaks(p_value);
			for (const auto boundary : boundaries) {
				if (boundary >= p_offset) return boundary - p_offset < p_offset - previous ? boundary : previous;
				previous = boundary;
			}
			return p_offset;
		}

		int64_t step_boundary(const String& p_value, int64_t p_offset, int p_direction) {
			int64_t previous = 0;
			const auto boundaries = breaks(p_value);
			for (const auto boundary : boundaries) {
				if (p_direction < 0 && boundary >= p_offset) return previous;
				if (p_direction > 0 && boundary > p_offset) return boundary;
				previous = boundary;
			}
			return p_direction < 0 ? previous : p_value.length();
		}

		int64_t utf16_length(const String& p_value) {
			int64_t count = p_value.length();
			for (int64_t index = 0; index < p_value.length(); ++index) {
				count += p_value[index] > 0xffff;
			}
			return count;
		}

		std::optional<int64_t> length_limit(const char* p_attribute) {
			if (!p_attribute) return {};
			std::string_view text(p_attribute);
			const auto first = text.find_first_not_of(" \t\n\r\f");
			if (first == std::string_view::npos) return {};
			text.remove_prefix(first);
			const bool negative = text.front() == '-';
			if (negative || text.front() == '+') text.remove_prefix(1);
			const auto is_digit = [](char p_char) { return p_char >= '0' && p_char <= '9'; };
			if (text.empty() || !is_digit(text.front())) return {};
			int64_t limit = 0;
			for (char character : text) {
				if (!is_digit(character)) break;
				const int digit = character - '0';
				if (negative && digit != 0) return {};
				if (limit > (std::numeric_limits<int64_t>::max() - digit) / 10) return std::numeric_limits<int64_t>::max();
				limit = limit * 10 + digit;
			}
			return limit;
		}

		Vector2i utf16_selection(const String& p_value, const Vector2i& p_range) {
			const int64_t start = std::max(p_range.x, 0);
			const int64_t end = start + std::max(p_range.y, 0);
			int64_t units = 0;
			int64_t from = 0;
			int64_t to = 0;
			for (int64_t index = 0; index < p_value.length() && units < end; ++index) {
				units += p_value[index] > 0xffff ? 2 : 1;
				to = index + 1;
				if (units <= start) from = to;
			}
			return Vector2i(from, std::max<int64_t>(0, to - from));
		}
	}


	String HTMLTextEditor::normalize(const String& p_value) const {
		return is_multiline() ? p_value.replace("\r\n", "\n").replace("\r", "\n") :
			p_value.replace("\r", "").replace("\n", "");
	}

	void HTMLTextEditor::use_default(const String& p_value) {
		if (dirty_value) return;
		const String clean = normalize(p_value);
		const bool changed = value != clean;
		value = clean;
		set_selection(anchor, caret);
		if (changed && value_changed) value_changed();
	}

	bool HTMLTextEditor::set_value(const String& p_value) {
		dirty_value = true;
		const String clean = normalize(p_value);
		if (clean == value) return false;
		clear_history();
		value = clean;
		set_preedit({}, {});
		set_selection(value.length(), value.length());
		user_changed = false;
		last_edit_was_user = false;
		if (value_changed) value_changed();
		return true;
	}

	void HTMLTextEditor::copy_value_state(const HTMLTextEditor& p_source, bool p_preserve_selection) {
		clear_history();
		value = p_source.value;
		dirty_value = p_source.dirty_value;
		last_edit_was_user = p_preserve_selection && p_source.last_edit_was_user;
		set_selection(p_preserve_selection ? p_source.anchor : 0, p_preserve_selection ? p_source.caret : 0);
		if (p_preserve_selection) {
			history = p_source.history;
			history_position = p_source.history_position;
		}
	}

	void HTMLTextEditor::reset_value(const String& p_value) {
		const String clean = normalize(p_value);
		const bool changed = clean != value;
		++reset_version;
		clear_history();
		dirty_value = false;
		user_changed = false;
		last_edit_was_user = false;
		value = clean;
		set_preedit({}, {});
		set_selection(0, 0);
		horizontal_scroll = 0;
		scroll = {};
		if (changed && value_changed) value_changed();
	}

	int64_t HTMLTextEditor::selection_start() const { return std::min(anchor, caret); }
	int64_t HTMLTextEditor::selection_end() const { return std::max(anchor, caret); }

	bool HTMLTextEditor::set_selection(int64_t p_anchor, int64_t p_caret) {
		const int64_t start = nearest_boundary(value, p_anchor);
		const int64_t end = p_anchor == p_caret ? start : nearest_boundary(value, p_caret);
		if (start == anchor && end == caret) return false;
		finish_ime_edit();
		anchor = start;
		caret = end;
		preferred_x.reset();
		caret_line_hint.reset();
		reveal_caret = true;
		trailing_affinity = false;
		return true;
	}

	float HTMLTextEditor::caret_x(int64_t p_offset, float p_near, bool p_nearest) const {
		if (!shaped.is_valid() || showing_placeholder) return 0;
		const auto server = text_server();
		const Dictionary carets = server->shaped_text_get_carets(shaped.get_rid(), p_offset);
		const Rect2 lead = carets.get("leading_rect", Rect2());
		const Rect2 trail = carets.get("trailing_rect", Rect2());
		if (lead.size.y <= 0) return trail.position.x;
		if (trail.size.y <= 0) return lead.position.x;
		if (p_nearest) {
			return std::abs(trail.position.x - p_near) < std::abs(lead.position.x - p_near) ? trail.position.x : lead.position.x;
		}
		return trailing_affinity ? trail.position.x : lead.position.x;
	}

	void HTMLTextEditor::hit_single(float p_x, bool p_extend) {
		if (has_preedit()) return;
		const float x = p_x - text_origin.x;
		const int64_t hit = shaped.is_valid() && !showing_placeholder ? shaped.hit_test(x) : 0;
		set_selection(p_extend ? anchor : hit, hit);
		trailing_affinity = false;
		trailing_affinity = caret_x(caret, x, true) != caret_x(caret);
	}

	void HTMLTextEditor::move_single(int p_direction, bool p_extend) {
		if (has_preedit()) return;
		// Hit-test the shaper on either side of the current visual stop so RTL runs
		// use the same geometry for keys, pointer and drawing.
		int64_t next = caret;
		std::optional<float> visual_stop;
		if (!p_extend && anchor != caret) {
			const float a = caret_x(anchor);
			const float b = caret_x(caret);
			next = ((a < b) == (p_direction < 0)) ? anchor : caret;
		} else if (shaped.is_valid() && !showing_placeholder) {
			const float origin = caret_x(caret);
			const float edge = p_direction < 0 ? 0.0f : shaped.width();
			const float extent = std::abs(edge - origin);
			float previous = 0;
			for (float distance = 1; extent > 0; distance *= 2) {
				distance = std::min(distance, extent);
				next = nearest_boundary(value, shaped.hit_test(origin + distance * p_direction));
				if (next != caret) {
					while (distance - previous > 1.0f) {
						const float middle = previous + (distance - previous) * 0.5f;
						const auto candidate = nearest_boundary(value, shaped.hit_test(origin + middle * p_direction));
						if (candidate == caret) previous = middle;
						else {
							next = candidate;
							distance = middle;
						}
					}
					visual_stop = origin + distance * p_direction;
					break;
				}
				if (distance == extent) break;
				previous = distance;
			}
		} else {
			next = step_boundary(value, caret, p_direction);
		}
		set_selection(p_extend ? anchor : next, next);
		if (visual_stop) {
			trailing_affinity = false;
			trailing_affinity = caret_x(caret, *visual_stop, true) != caret_x(caret);
		}
	}

	bool HTMLTextEditor::erase(int p_direction) {
		if (has_preedit()) return false;
		finish_ime_edit();
		const auto before = selection();
		if (anchor == caret) set_selection(caret, step_boundary(value, caret, p_direction));
		return replace_selection({}, "", before, false);
	}

	bool HTMLTextEditor::insert(const String& p_text, const char* p_maxlength) {
		finish_ime_edit();
		return replace_selection(p_text, p_maxlength, selection(), false);
	}

	bool HTMLTextEditor::insert_character(char32_t p_character, const char* p_maxlength, uint64_t p_frame, bool p_ime_event) {
		// Godot sends each character in an IME result as a key event in the same input frame.
		auto batch = ime_edit && ime_edit->frame == p_frame ? ime_edit : std::nullopt;
		if (!batch && (p_ime_event || has_preedit())) batch = ImeEdit{p_frame, selection()};
		const bool changed = replace_selection(String::chr(p_character), p_maxlength,
			batch ? batch->before : selection(), batch && batch->recorded);
		if (batch) batch->recorded |= changed;
		ime_edit = batch;
		return changed;
	}

	bool HTMLTextEditor::replace_selection(const String& p_text, const char* p_maxlength, const Selection& p_before, bool p_merge) {
		String text = normalize(p_text);
		set_preedit({}, {});
		if (text.is_empty() && !p_text.is_empty()) return false;
		const int64_t from = selection_start();
		const int64_t to = selection_end();
		const String prefix = value.substr(0, from);
		const String suffix = value.substr(to);
		if (const auto max = length_limit(p_maxlength); max && !text.is_empty()) {
			const int64_t limit = *max - utf16_length(prefix) - utf16_length(suffix);
			if (utf16_length(text) > limit) {
				int64_t length = 0;
				const auto boundaries = breaks(text);
				int64_t units = 0;
				for (const auto boundary : boundaries) {
					for (int64_t index = length; index < boundary; ++index) units += text[index] > 0xffff ? 2 : 1;
					if (units > limit) break;
					length = boundary;
				}
				if (length == 0) return false;
				text = text.substr(0, length);
			}
		}
		const String removed = value.substr(from, to - from);
		if (removed == text) {
			set_selection(to, to);
			return false;
		}
		value = prefix + text + suffix;
		dirty_value = true;
		user_changed = true;
		last_edit_was_user = true;
		set_selection(from + text.length(), from + text.length());

		if (!p_merge || history_position == 0 || history_position != history.size()) {
			history.erase(history.begin() + history_position, history.end());
			if (history.size() == MAX_UNDO_EDITS) history.erase(history.begin());
			history.push_back({p_before, selection(), {}});
			history_position = history.size();
		}
		auto& edit = history.back();
		edit.after = selection();
		edit.replacements.push_back({from, removed, text});
		if (value_changed) value_changed();
		return true;
	}

	void HTMLTextEditor::clear_history() {
		history.clear();
		history_position = 0;
		finish_ime_edit();
	}

	void HTMLTextEditor::restore_selection(const Selection& p_selection) {
		anchor = p_selection.anchor;
		caret = p_selection.caret;
		trailing_affinity = p_selection.trailing_affinity;
		preferred_x.reset();
		caret_line_hint.reset();
		set_preedit({}, {});
		dirty_value = true;
		user_changed = true;
		last_edit_was_user = true;
	}

	bool HTMLTextEditor::undo() {
		finish_ime_edit();
		if (!can_undo()) return false;
		const auto& edit = history[--history_position];
		for (auto step = edit.replacements.rbegin(); step != edit.replacements.rend(); ++step) {
			value = value.substr(0, step->offset) + step->removed + value.substr(step->offset + step->inserted.length());
		}
		restore_selection(edit.before);
		if (value_changed) value_changed();
		return true;
	}

	bool HTMLTextEditor::redo() {
		finish_ime_edit();
		if (!can_redo()) return false;
		const auto& edit = history[history_position++];
		for (const auto& step : edit.replacements) {
			value = value.substr(0, step.offset) + step.inserted + value.substr(step.offset + step.removed.length());
		}
		restore_selection(edit.after);
		if (value_changed) value_changed();
		return true;
	}

	void HTMLTextEditor::finish_ime_edit() {
		ime_edit.reset();
	}

	bool HTMLTextEditor::cancel_preedit() {
		finish_ime_edit();
		if (!has_preedit()) return false;
		set_preedit({}, {});
		return true;
	}

	void HTMLTextEditor::update_preedit(const String& p_text, const Vector2i& p_selection, uint64_t p_frame) {
		if (!p_text.is_empty()) {
			finish_ime_edit();
		} else if (has_preedit()) {
			// Some backends clear the marked text before handing off the result.
			ime_edit = ImeEdit{p_frame, selection()};
		}
		set_preedit(p_text, p_selection);
	}

	void HTMLTextEditor::set_preedit(const String& p_text, const Vector2i& p_selection) {
		preedit = normalize(p_text);
		reveal_caret = true;
#ifdef _WIN32
		preedit_selection = utf16_selection(preedit, p_selection);
#else
		preedit_selection = p_selection;
#endif
	}

	bool HTMLTextEditor::take_user_change() {
		return std::exchange(user_changed, false);
	}

	bool HTMLTextEditor::has_length_violation(const char* p_minlength, const char* p_maxlength) const {
		if (!last_edit_was_user || value.is_empty()) return false;
		const auto min = length_limit(p_minlength);
		const auto max = length_limit(p_maxlength);
		if (!min && !max) return false;
		const int64_t length = utf16_length(value);
		return (min && length < *min) || (max && length > *max);
	}


	namespace {
		HTMLInputPaintColors input_paint_colors(const litehtml::html_tag& host) {
			const auto& color = host.css().get_color();
			const auto selection = host.get_selection_colors(color);
			return {selection.foreground, selection.background, host.css().get_caret_color().resolve(color)};
		}

		litehtml::web_color placeholder_ink(const litehtml::html_tag& host, const css::ComputedStyle* style) {
			auto color = (style ? style->get_color() : host.css().get_color()).to_srgb();
			const double opacity = (style ? style->get_opacity() : 1.0) * (host.has_placeholder_color() ? 1.0 : 0.5);
			color.alpha = static_cast<litehtml::byte>(std::lround(color.alpha * opacity));
			return color;
		}
	}

	void HTMLTextEditor::paint_single(LiteHTMLContainer& p_container, const Rect2& p_box,
		const litehtml::html_tag& p_host, const String& p_placeholder, bool p_focused, bool p_password, const String* p_preview) {
		const auto& p_style = p_host.css();
		viewport = p_box;
		const auto color = p_style.get_color();
		const auto from = selection_start();
		const auto to = selection_end();
		showing_placeholder = !p_preview && value.is_empty() && preedit.is_empty();
		const String display = p_preview ? *p_preview : showing_placeholder ? p_placeholder :
			has_preedit() ? value.substr(0, from) + preedit + value.substr(to) : value;
		const auto* placeholder_style = showing_placeholder ? p_host.get_placeholder_style() : nullptr;
		const auto& text_style = placeholder_style ? *placeholder_style : p_style;
		const String masked = p_password && !showing_placeholder ? String::chr(0x2022).repeat(display.length()) : display;
		shaped = p_container.shape_input(text_style.get_font(), masked);
		const int64_t display_caret = has_preedit() ?
			from + std::clamp<int64_t>(preedit_selection.x, 0, preedit.length()) : caret;
		const float x = caret_x(display_caret);
		const float width = shaped.is_valid() ? shaped.width() : 0.0f;
		const float max_scroll = std::max(0.0f, width - p_box.size.x + 1.0f);
		if (p_focused) horizontal_scroll = std::clamp(horizontal_scroll, x - p_box.size.x + 1.0f, x);
		horizontal_scroll = std::clamp(horizontal_scroll, 0.0f, max_scroll);
		float align = 0;
		if (width < p_box.size.x) {
			if (p_style.get_text_align() == litehtml::text_align_center) align = (p_box.size.x - width) * 0.5f;
			if (p_style.get_text_align() == litehtml::text_align_right)
				align = p_box.size.x - width;
		}
		const auto& metrics = text_style.get_font_metrics();
		const bool use_shaped_metrics = shaped.is_valid() && !metrics.has_metric_overrides;
		const float ascent = use_shaped_metrics ? shaped.line_ascent() : metrics.ascent;
		const float descent = use_shaped_metrics ? shaped.line_descent() : metrics.descent;
		const float baseline = HTMLControlRenderItem::paint_text_baseline(p_box.position.y, p_box.size.y,
			metrics, shaped.is_valid() ? shaped.ascent : metrics.ascent);
		text_origin = Vector2(p_box.position.x + align - (p_preview ? 0.0f : horizontal_scroll), baseline);
		caret_rect = Rect2(text_origin.x + x, baseline - ascent, 1.0f, ascent + descent);
		const auto ink = showing_placeholder ? placeholder_ink(p_host, placeholder_style) : color;
		const auto server = text_server();
		PackedVector2Array selected;
		PackedVector2Array composition;
		if (shaped.is_valid() && p_focused && !showing_placeholder) {
			if (has_preedit()) {
				composition = shaped.selection(*server.ptr(), from, from + preedit.length());
				const auto selected_from = std::clamp<int64_t>(preedit_selection.x, 0, preedit.length());
				const auto selected_to = std::clamp<int64_t>(selected_from + std::max(0, preedit_selection.y), selected_from, preedit.length());
				selected = shaped.selection(*server.ptr(), from + selected_from, from + selected_to);
			} else if (from != to) {
				selected = shaped.selection(*server.ptr(), from, to);
			}
		}
		p_container.draw_input(p_box, shaped, text_origin, ink, selected, composition,
			p_focused && caret_visible ? caret_rect : Rect2(), ascent, descent, input_paint_colors(p_host));
	}

	namespace {
		constexpr float RESIZE_SIZE = 14;
	}

	float HTMLTextEditor::line_caret_x(const Line& p_line, int64_t p_offset, std::optional<float> p_near) const {
		if (!p_line.shaped.is_valid()) return p_line.baseline.x;
		const auto server = text_server();
		const auto rid = p_line.shaped.get_rid();
		const int64_t offset = p_line.shaped.text_start(*server.ptr()) + std::clamp(p_offset, p_line.start, p_line.end) - p_line.start;
		const Dictionary carets = server->shaped_text_get_carets(rid, offset);
		const Rect2 leading = carets.get("leading_rect", Rect2());
		const Rect2 trailing = carets.get("trailing_rect", Rect2());
		if (leading.size.y <= 0) return p_line.baseline.x + trailing.position.x;
		if (trailing.size.y <= 0) return p_line.baseline.x + leading.position.x;
		const bool use_trailing = p_near ?
			std::abs(p_line.baseline.x + trailing.position.x - *p_near) < std::abs(p_line.baseline.x + leading.position.x - *p_near) :
			trailing_affinity;
		return p_line.baseline.x + (use_trailing ? trailing.position.x : leading.position.x);
	}

	size_t HTMLTextEditor::line_at_offset(int64_t p_offset) const {
		if (caret_line_hint && *caret_line_hint < lines.size()) {
			const auto& line = lines[*caret_line_hint];
			if (p_offset >= line.start && p_offset <= line.end) return *caret_line_hint;
		}
		const auto next = std::upper_bound(lines.begin(), lines.end(), p_offset,
			[](int64_t p_index, const Line& p_line) { return p_index < p_line.start; });
		return next == lines.begin() ? 0 : size_t(next - lines.begin() - 1);
	}

	size_t HTMLTextEditor::line_at_y(float p_y) const {
		const auto next = std::upper_bound(lines.begin(), lines.end(), p_y,
			[](float p_coord, const Line& p_line) { return p_coord < p_line.bottom; });
		return std::min<size_t>(next - lines.begin(), lines.size() - 1);
	}

	void HTMLTextEditor::hit_caret(const Vector2& p_point, bool p_extend) {
		if (!is_multiline()) {
			hit_single(p_point.x, p_extend);
			return;
		}
		if (has_preedit() || lines.empty()) return;
		const Vector2 point = p_point - viewport.position + scroll;
		const auto index = line_at_y(point.y);
		const auto& line = lines[index];
		const auto offset = showing_placeholder ? 0 : line.start + (line.shaped.is_valid() ?
			line.shaped.hit_test(point.x - line.baseline.x) : 0);
		const auto hit = std::clamp<int64_t>(offset, 0, value.length());
		set_selection(p_extend ? anchor : hit, hit);
		caret_line_hint = index;
		trailing_affinity = false;
		trailing_affinity = line_caret_x(line, caret, point.x) != line_caret_x(line, caret);
		reveal_caret = true;
	}

	void HTMLTextEditor::move_caret(int p_direction, bool p_extend) {
		if (!is_multiline()) {
			move_single(p_direction, p_extend);
			return;
		}
		if (has_preedit()) return;
		if (!p_extend && anchor != caret) {
			const auto target = p_direction < 0 ? selection_start() : selection_end();
			set_selection(target, target);
			return;
		}
		int64_t next = caret;
		if (!lines.empty() && !showing_placeholder) {
			const size_t index = line_at_offset(caret);
			const auto& line = lines[index];
			const float start_x = line_caret_x(line, caret);
			const float width = line.shaped.is_valid() ? line.shaped.width() : 0;
			const float edge = line.baseline.x + (p_direction < 0 ? 0 : width);
			if (line.shaped.is_valid() && std::abs(edge - start_x) > 0.5f) {
				const float extent = std::abs(edge - start_x);
				float low = 0;
				float high = std::min(1.0f, extent);
				auto probe = [&](float p_distance) {
					return nearest_boundary(value, line.start +
						line.shaped.hit_test(start_x + p_direction * p_distance - line.baseline.x));
				};
				while ((next = probe(high)) == caret && high < extent) {
					low = high;
					high = std::min(high * 2, extent);
				}
				if (next != caret) {
					while (high - low > 1) {
						const float mid = (high + low) * 0.5f;
						const auto hit = probe(mid);
						if (hit == caret) low = mid;
						else { high = mid; next = hit; }
					}
					set_selection(p_extend ? anchor : next, next);
					caret_line_hint = index;
					trailing_affinity = line_caret_x(line, caret, start_x + high * p_direction) != line_caret_x(line, caret);
					return;
				}
			}
			if (p_direction < 0 && index > 0) next = lines[index - 1].end;
			else if (p_direction > 0 && index + 1 < lines.size()) next = lines[index + 1].start;
			else return;
			set_selection(p_extend ? anchor : next, next);
			caret_line_hint = size_t(int64_t(index) + p_direction);
			reveal_caret = true;
			return;
		}
		next = step_boundary(value, caret, p_direction);
		set_selection(p_extend ? anchor : next, next);
	}

	void HTMLTextEditor::move_vertical(int p_direction, bool p_extend) {
		if (has_preedit() || showing_placeholder || lines.empty()) return;
		const auto index = line_at_offset(caret);
		const auto target = size_t(std::clamp<int64_t>(int64_t(index) + p_direction, 0, lines.size() - 1));
		if (index == target) return;
		const float x = preferred_x.value_or(line_caret_x(lines[index], caret));
		const auto& line = lines[target];
		const auto offset = line.start + (line.shaped.is_valid() ? line.shaped.hit_test(x - line.baseline.x) : 0);
		set_selection(p_extend ? anchor : offset, offset);
		preferred_x = x;
		caret_line_hint = target;
		trailing_affinity = false;
		trailing_affinity = line_caret_x(line, caret, x) != line_caret_x(line, caret);
		reveal_caret = true;
	}

	void HTMLTextEditor::move_to_edge(bool p_end, bool p_document, bool p_extend) {
		int64_t target = p_end ? value.length() : 0;
		std::optional<size_t> index;
		if (is_multiline() && !p_document && !showing_placeholder && !lines.empty()) {
			index = line_at_offset(caret);
			const auto& line = lines[*index];
			target = p_end ? line.end : line.start;
		}
		set_selection(p_extend ? anchor : target, target);
		caret_line_hint = index;
	}

	void HTMLTextEditor::layout_lines(LiteHTMLContainer& p_container, const css::ComputedStyle& p_style,
		const css::ComputedStyle& p_text_style, const String& p_text, float p_width, float p_line_height) {
		lines.clear();
		full_size = {};
		const auto server = text_server();
		const auto& font = p_text_style.get_font_metrics();
		const auto add_line = [&](int64_t p_start, int64_t p_end, const HTMLTextLayout& p_layout) {
			const float width = p_layout.is_valid() ? p_layout.width() : 0.0f;
			const bool use_shaped_metrics = p_layout.is_valid() && !font.has_metric_overrides;
			const float ascent = use_shaped_metrics ? p_layout.line_ascent() : font.ascent;
			const float descent = use_shaped_metrics ? p_layout.line_descent() : font.descent;
			float x = 0;
			if (width < p_width) {
				if (p_style.get_text_align() == litehtml::text_align_center) x = (p_width - width) * 0.5f;
				if (p_style.get_text_align() == litehtml::text_align_right) x = p_width - width;
			}
			const float baseline = full_size.y + (p_line_height + ascent - descent) * 0.5f;
			full_size.y += p_line_height;
			full_size.x = std::max(full_size.x, width);
			lines.push_back({p_start, p_end, p_layout, Vector2(x, baseline), float(full_size.y)});
		};
		for (int64_t start = 0; start <= p_text.length();) {
			const auto newline = p_text.find("\n", start);
			const auto end = newline < 0 ? p_text.length() : newline;
			const auto paragraph = p_container.shape_input(p_text_style.get_font(), p_text.substr(start, end - start));
			if (paragraph.is_valid() && p_style.get_text_wrap_mode() == litehtml::text_wrap_wrap && paragraph.width() > p_width) {
				const auto ranges = server->shaped_text_get_line_breaks(paragraph.get_rid(), std::max(1.0f, p_width), 0,
					TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE);
				for (int64_t index = 0; index + 1 < ranges.size(); index += 2) {
					const int64_t from = ranges[index];
					const int64_t to = ranges[index + 1];
					add_line(start + from, start + to, paragraph.subrange(from, to));
				}
				if (ranges.size() < 2) add_line(start, end, paragraph);
			} else {
				add_line(start, end, paragraph);
			}
			if (newline < 0) break;
			start = end + 1;
		}
	}

	Vector2 HTMLTextEditor::scroll_by(const Vector2& p_delta) {
		const Vector2 old = scroll;
		scroll = (scroll + p_delta).clamp(Vector2(), scroll_limit);
		if (scroll != old) reveal_caret = false;
		return scroll - old;
	}

	Rect2 HTMLTextEditor::scrollbar_thumb(const Rect2& p_track, bool p_vertical) const {
		if (!p_track.has_area()) return {};
		const int axis = p_vertical ? 1 : 0;
		const float extent = p_track.size[axis];
		const float size = std::clamp(extent * viewport.size[axis] / std::max(1.0f, float(full_size[axis])),
			std::min(14.0f, extent), extent);
		Rect2 thumb = p_track;
		thumb.size[axis] = size;
		if (scroll_limit[axis] > 0) thumb.position[axis] += (extent - size) * scroll[axis] / scroll_limit[axis];
		return thumb;
	}

	std::optional<bool> HTMLTextEditor::scrollbar_at(const Vector2& p_point) const {
		if (vertical_track.has_point(p_point)) return true;
		if (horizontal_track.has_point(p_point)) return false;
		return std::nullopt;
	}

	float HTMLTextEditor::scrollbar_grab_offset(bool p_vertical, const Vector2& p_point) const {
		const Rect2 thumb = scrollbar_thumb(p_vertical ? vertical_track : horizontal_track, p_vertical);
		const int axis = p_vertical ? 1 : 0;
		return thumb.has_point(p_point) ? p_point[axis] - thumb.position[axis] : thumb.size[axis] * 0.5f;
	}

	void HTMLTextEditor::drag_scrollbar(bool p_vertical, const Vector2& p_point, float p_grab_offset) {
		const Rect2& track = p_vertical ? vertical_track : horizontal_track;
		const Rect2 thumb = scrollbar_thumb(track, p_vertical);
		const int axis = p_vertical ? 1 : 0;
		const float travel = track.size[axis] - thumb.size[axis];
		if (travel <= 0) return;
		Vector2 delta;
		delta[axis] = ((p_point[axis] - p_grab_offset - track.position[axis]) / travel) * scroll_limit[axis] - scroll[axis];
		(void)scroll_by(delta);
	}

	void HTMLTextEditor::paint_multiline(LiteHTMLContainer& p_container, const Rect2& p_box,
		const litehtml::html_tag& p_host, const String& p_placeholder, bool p_focused, bool p_resize_handle) {
		const auto& p_style = p_host.css();
		const int64_t from = selection_start();
		const int64_t to = selection_end();
		const bool composing = has_preedit();
		const int64_t preedit_start = std::clamp<int64_t>(preedit_selection.x, 0, preedit.length());
		const int64_t preedit_end = std::clamp<int64_t>(preedit_start + std::max(0, preedit_selection.y), preedit_start, preedit.length());
		showing_placeholder = value.is_empty() && !composing;
		const String display = showing_placeholder ? normalize(p_placeholder) :
			composing ? value.substr(0, from) + preedit + value.substr(to) : value;
		const auto* placeholder_style = showing_placeholder ? p_host.get_placeholder_style() : nullptr;
		const css::ComputedStyle& text_style = placeholder_style ? *placeholder_style : p_style;
		const float height = std::max(1.0f, text_style.line_height().computed_value);
		const auto overflow_x = p_style.get_overflow_x();
		const auto overflow_y = p_style.get_overflow_y();
		const auto wrap = p_style.get_text_wrap_mode();
		const auto align = p_style.get_text_align();
		const auto scrollbar = html_scrollbar_appearance(p_style);
		const float thickness = scrollbar.thickness();
		const bool show_scrollbars = thickness > 0;
		const float bar_width = std::min(thickness, p_box.size.x);
		const float bar_height = std::min(thickness, p_box.size.y);
		if (layout_size != p_box.size || layout_font != text_style.get_font() || laid_out_text != display ||
			layout_line_height != height || layout_wrap != wrap || layout_overflow_x != overflow_x || layout_overflow_y != overflow_y ||
			layout_scrollbar_width != thickness || layout_align != align || layout_resize != p_resize_handle) {
			laid_out_text = display;
			layout_size = p_box.size;
			layout_font = text_style.get_font();
			layout_line_height = height;
			layout_wrap = wrap;
			layout_overflow_x = overflow_x;
			layout_overflow_y = overflow_y;
			layout_scrollbar_width = thickness;
			layout_align = align;
			layout_resize = p_resize_handle;
			caret_line_hint.reset();
			horizontal_scrollbar = show_scrollbars && overflow_x == litehtml::overflow_scroll;
			vertical_scrollbar = show_scrollbars && overflow_y == litehtml::overflow_scroll;
			for (int pass = 0; pass < 3; ++pass) {
				const float width = p_box.size.x - (vertical_scrollbar ? bar_width : 0);
				const float box_height = p_box.size.y - (horizontal_scrollbar ? bar_height : 0);
				layout_lines(p_container, p_style, text_style, display, width, height);
				const bool need_vertical = vertical_scrollbar || (show_scrollbars && overflow_y == litehtml::overflow_auto && full_size.y > box_height);
				const bool need_horizontal = horizontal_scrollbar || (show_scrollbars && overflow_x == litehtml::overflow_auto && full_size.x > width);
				if (need_vertical == vertical_scrollbar && need_horizontal == horizontal_scrollbar) break;
				vertical_scrollbar = need_vertical;
				horizontal_scrollbar = need_horizontal;
			}
		}

		viewport = Rect2(p_box.position, Vector2(p_box.size.x - (vertical_scrollbar ? bar_width : 0),
			p_box.size.y - (horizontal_scrollbar ? bar_height : 0)));
		const float vertical_corner = std::max(horizontal_scrollbar ? bar_height : 0.0f, p_resize_handle ? RESIZE_SIZE : 0.0f);
		const float horizontal_corner = std::max(vertical_scrollbar ? bar_width : 0.0f, p_resize_handle ? RESIZE_SIZE : 0.0f);
		vertical_track = vertical_scrollbar ? Rect2(viewport.get_end().x, p_box.position.y, bar_width,
			std::max(0.0f, p_box.size.y - vertical_corner)) : Rect2();
		horizontal_track = horizontal_scrollbar ? Rect2(p_box.position.x, viewport.get_end().y,
			std::max(0.0f, p_box.size.x - horizontal_corner), bar_height) : Rect2();
		scroll_limit = (full_size - viewport.size).max(Vector2());
		if (multiline && !litehtml::is_scrollable_overflow(overflow_x)) scroll_limit.x = 0;
		if (multiline && !litehtml::is_scrollable_overflow(overflow_y)) scroll_limit.y = 0;
		scroll = scroll.clamp(Vector2(), scroll_limit);

		const int64_t display_caret = composing ? from + preedit_start : caret;
		const size_t caret_index = line_at_offset(display_caret);
		const auto& current = lines[caret_index];
		const float x = line_caret_x(current, display_caret);
		if (p_focused && reveal_caret) {
			if (x < scroll.x) scroll.x = x;
			else if (x + 1 > scroll.x + viewport.size.x) scroll.x = x + 1 - viewport.size.x;
			const float top = current.bottom - height;
			if (top < scroll.y) scroll.y = top;
			else if (current.bottom > scroll.y + viewport.size.y) scroll.y = current.bottom - viewport.size.y;
			scroll = scroll.clamp(Vector2(), scroll_limit);
			reveal_caret = false;
		}
		const Vector2 origin = viewport.position - scroll;
		caret_rect = Rect2(origin + Vector2(x, current.bottom - height), Vector2(1, height));
		const auto color = p_style.get_color();
		const auto ink = showing_placeholder ? placeholder_ink(p_host, placeholder_style) : color;
		const auto server = text_server();
		const float last_y = scroll.y + viewport.size.y;
		const auto colors = input_paint_colors(p_host);
		for (size_t index = line_at_y(scroll.y); index < lines.size(); ++index) {
			const auto& line = lines[index];
			if (line.bottom - height >= last_y) break;
			const auto geometry = [&](int64_t p_begin, int64_t p_end) {
				const auto begin = std::max(line.start, p_begin);
				const auto end = std::min(line.end, p_end);
				return begin < end && line.shaped.is_valid() ?
					line.shaped.selection(*server.ptr(), begin - line.start, end - line.start) : PackedVector2Array{};
			};
			const float ascent = line.baseline.y - (line.bottom - height);
			PackedVector2Array selected;
			if (p_focused && !showing_placeholder) {
				selected = composing ? geometry(from + preedit_start, from + preedit_end) : geometry(from, to);
				if (!composing && line.end < to && line.end >= from && line.end < value.length() && value[line.end] == '\n') {
					const float end_x = line_caret_x(line, line.end) - line.baseline.x;
					selected.append(Vector2(end_x, end_x + std::max(2.0f, height * 0.25f)));
				}
			}
			p_container.draw_input(viewport, line.shaped, origin + line.baseline, ink, selected,
				composing ? geometry(from, from + preedit.length()) : PackedVector2Array{},
				p_focused && caret_visible && index == caret_index ? caret_rect : Rect2(), ascent, height - ascent, colors);
		}
		for (const bool axis : {false, true}) {
			const Rect2& track = axis ? vertical_track : horizontal_track;
			if (track.has_area()) p_container.draw_input_scrollbar(p_box, track, scrollbar_thumb(track, axis), axis, scrollbar);
		}
	}
}
