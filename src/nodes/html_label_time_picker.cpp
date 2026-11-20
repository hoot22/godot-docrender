#include "html_label.h"

#include "formats/html/css/css_litehtml_dom.h"
#include "formats/html/elements/html_input_node.h"
#include "formats/html/html_ui_translations.h"
#include "formats/html/paint/html_paint_recorder.h"

#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;
using dockit::HTMLControlIcon;
namespace calendar = dockit::input_time;

namespace {
	constexpr int row_count = 7;
	constexpr int row_height = 36;
	constexpr int column_width = 56;
	constexpr int days_per_row = 7;
	constexpr int calendar_rows = 6;
	constexpr int calendar_top = 68;
	constexpr int day_size = 32;
	constexpr int day_height = 29;
	constexpr int calendar_inset = 13;
	constexpr int week_number_width = 32;
	constexpr int months_per_row = 4;

	int wrap(int p_value, int p_modulus) {
		return (p_value % p_modulus + p_modulus) % p_modulus;
	}

	Rect2 clock_cell(int p_x, int p_column, int p_row) {
		return { real_t(p_x + 4 + p_column * column_width), real_t(4 + p_row * row_height), 52, row_height };
	}

	Rect2 calendar_day_cell(int p_index, int p_week_offset) {
		return { real_t(calendar_inset + p_week_offset + (p_index % days_per_row) * day_size),
			real_t(calendar_top + (p_index / days_per_row) * day_height), day_size, day_height };
	}

	Rect2 calendar_week_cell(int p_index) {
		return { calendar_inset, real_t(calendar_top + p_index * day_height), week_number_width, day_height };
	}

	Rect2 calendar_month_cell(int p_index, int p_width) {
		const real_t width = real_t(p_width - 2 * calendar_inset) / months_per_row;
		return { calendar_inset + (p_index % months_per_row) * width, real_t(39 + (p_index / months_per_row) * 32), width, 32 };
	}

	Rect2 previous_month(int p_width) { return { real_t(p_width - 73), 11, 28, 24 }; }
	Rect2 next_month(int p_width) { return { real_t(p_width - 41), 11, 28, 24 }; }
	Rect2 today_button(int p_width, int p_height) { return { real_t(p_width - 100), real_t(p_height - 33), 88, 20 }; }
	Rect2 clear_button(int p_height) { return { 12, real_t(p_height - 33), 44, 20 }; }

	const Rect2 calendar_title(12, 11, 156, 24);
	constexpr int title_id = 42;
	constexpr int previous_id = 43;
	constexpr int next_id = 44;
	constexpr int today_id = 45;
	constexpr int clear_id = 46;

	const char* month_names[] = { "January", "February", "March", "April", "May", "June", "July", "August",
		"September", "October", "November", "December" };
	const char* weekdays[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

	String localized_month(int p_index) {
		return dockit::html_ui_translate(String::utf8(month_names[p_index]), "time_picker.month");
	}

	std::optional<double> first_of_month(Vector2i p_month) {
		return calendar::parse(calendar::Kind::date, vformat("%04d-%02d-01", p_month.x, p_month.y));
	}

	std::optional<double> month_start(Vector2i p_month, bool p_sunday_first) {
		const auto first = first_of_month(p_month);
		if (!first) return {};
		const int64_t day_index = int64_t(*first / calendar::day);
		const int offset = wrap(int(day_index % 7) + (p_sunday_first ? 4 : 3), 7);
		return *first - offset * calendar::day;
	}

	struct CalendarBounds {
		calendar::Kind kind;
		std::optional<double> minimum;
		std::optional<double> maximum;

		CalendarBounds(calendar::Kind p_kind, const HTMLInputElement& p_input) : kind(p_kind),
			minimum(calendar::parse(kind, p_input.get_attribute("min"))),
			maximum(calendar::parse(kind, p_input.get_attribute("max"))) {
		}

		std::optional<double> value(double p_day) const {
			if (kind == calendar::Kind::week) return calendar::parse(kind, calendar::format(kind, p_day));
			if (kind == calendar::Kind::month) {
				const auto date = calendar::format(calendar::Kind::date, p_day);
				return calendar::parse(kind, date.get_slice("-", 0) + "-" + date.get_slice("-", 1));
			}
			return calendar::format(calendar::Kind::date, p_day).is_empty() ? std::nullopt : std::optional<double>(p_day);
		}

		bool allows(std::optional<double> p_candidate) const {
			const double range = kind == calendar::Kind::local ? calendar::day - 1 : 0;
			return p_candidate && (!minimum || *p_candidate + range >= *minimum) && (!maximum || *p_candidate <= *maximum);
		}
	};

	bool uses_twelve_hours(const String& p_locale) {
		if (p_locale.contains("-hc-h23") || p_locale.contains("-hc-h24")) return false;
		if (p_locale.contains("-hc-h11") || p_locale.contains("-hc-h12")) return true;
		const String language = p_locale.get_slice("-", 0);
		const String region = p_locale.get_slice("-", 1);
		if (language == "en") {
			return region.is_empty() || region == "us" || region == "ca" || region == "au" ||
				region == "nz" || region == "ph" || region == "in";
		}
		if (language == "zh") return region == "tw" || region == "hant";
		return language == "ar" || language == "hi" || language == "bn" || language == "ko";
	}

	// The representable steps in each column are subdivisions of its period.
	std::pair<int, int> clock_granularity(double p_step, double p_base, int p_unit, int p_units, bool p_fraction) {
		const double period = double(p_unit) * p_units;
		const int fallback = p_fraction ? 100 : 1;
		if (!std::isfinite(p_step) || p_step <= 0) return { fallback, 0 };
		if (std::fmod(p_step, period) == 0) p_step = period;
		if (std::fmod(period, p_step) != 0 || std::fmod(p_step, p_unit) != 0) return { fallback, 0 };
		int granularity = int(p_step / p_unit);
		if (p_fraction && granularity < 100) {
			for (int candidate : {100, 125, 200, 250, 500, 1000}) {
				if (candidate % granularity != 0) continue;
				granularity = candidate;
				break;
			}
		}
		const auto offset = int(std::fmod(std::floor(p_base / p_unit), p_units));
		return { granularity, wrap(offset, granularity) };
	}
}

int HTMLLabel::TimePickerState::calendar_width() const {
	if (!has_calendar()) return 0;
	if (kind == calendar::Kind::month) return 232;
	return kind == calendar::Kind::week ? 250 + week_number_width : 250;
}

int HTMLLabel::TimePickerState::calendar_height() const {
	return kind == calendar::Kind::month ? 182 : 282;
}

int HTMLLabel::TimePickerState::Column::row_index(int p_row) const {
	if (p_row < 0 || p_row >= row_count || values.empty()) return -1;
	const int index = first + p_row;
	if (loops()) return wrap(index, int(values.size()));
	return index < int(values.size()) ? index : -1;
}

void HTMLLabel::TimePickerState::Column::scroll(int p_rows) {
	first = loops() ? wrap(first + p_rows, int(values.size())) :
		std::clamp(first + p_rows, 0, std::max(0, int(values.size()) - row_count));
}

void HTMLLabel::TimePickerState::Column::reveal() {
	const int count = int(values.size());
	const int offset = loops() ? wrap(selected - first, count) : selected - first;
	if (offset < 0 || offset >= row_count) {
		first = loops() ? selected : std::clamp(selected, 0, std::max(0, count - row_count));
	}
}

Error HTMLLabel::show_clock_picker() {
	const auto input = picker_input->get_input();
	time_picker.kind = input->time_kind();
	time_picker.initial_value = input->get_value();
	const auto value = calendar::parse(time_picker.kind, time_picker.initial_value);
	picker_date = value ? calendar::parts(time_picker.kind, *value) : Time::get_singleton()->get_datetime_dict_from_system();
	if (!value && time_picker.has_calendar()) {
		const CalendarBounds limits(time_picker.kind, *picker_input.ptr());
		if (auto initial_date = calendar::from_parts(time_picker.kind, picker_date)) {
			if (limits.minimum && *initial_date < *limits.minimum) initial_date = limits.minimum;
			if (limits.maximum && *initial_date > *limits.maximum) initial_date = limits.maximum;
			picker_date = calendar::parts(time_picker.kind, *initial_date);
		}
	}
	String locale = TranslationServer::get_singleton()->get_locale();
	if (locale.is_empty()) locale = OS::get_singleton()->get_locale();
	locale = locale.replace("_", "-").to_lower();
	time_picker.use_meridiem = time_picker.has_clock() && uses_twelve_hours(locale);
	time_picker.sunday_first = time_picker.kind != calendar::Kind::week &&
		(locale == "en" || locale.begins_with("en-us") || locale.begins_with("en-ca") || locale.begins_with("ja"));
	time_picker.year_view = time_picker.kind == calendar::Kind::month;
	time_picker.initial = picker_date.duplicate();
	time_picker.calendar_month = Vector2i(int(picker_date["year"]), int(picker_date["month"]));
	if (time_picker.has_clock()) configure_clock_columns();
	if (time_picker.has_calendar()) time_picker.active_column = -1;
	if (!time_picker_canvas) {
		time_picker_canvas = memnew(Control);
		time_picker_canvas->set_name("_HTMLTimePicker");
		time_picker_canvas->set_mouse_filter(MOUSE_FILTER_STOP);
		time_picker_canvas->set_z_index(2);
		time_picker_canvas->hide();
		add_child(time_picker_canvas, false, INTERNAL_MODE_BACK);
		time_picker_canvas->connect("draw", callable_mp(this, &HTMLLabel::draw_clock_picker));
	}
	update_clock_picker();
	if (!is_time_picker_open()) return ERR_UNAVAILABLE;
	set_process_input(true);
	return OK;
}

void HTMLLabel::configure_clock_columns() {
	using Unit = TimePickerState::Unit;
	auto& state = time_picker;
	const auto input = picker_input->get_input();
	const double step = input->step_scale();
	const double base = input->step_base();
	const int hour = int(picker_date["hour"]);
	const int minute = int(picker_date["minute"]);
	const int second = int(picker_date.get("second", 0));
	const int ms = int(picker_date.get("millisecond", 0));
	state.columns.clear();
	const auto add = [&](Unit p_unit, int p_value, int p_scale, int p_count) {
		auto [granularity, offset] = clock_granularity(step, base, p_scale, p_count, p_unit == Unit::millisecond);
		TimePickerState::Column column{ p_unit,{} };
		for (int value = offset; value < p_count; value += granularity) column.values.push_back(value);
		// Show the current value exactly, including a value entered manually between picker steps.
		const auto selected = std::lower_bound(column.values.begin(), column.values.end(), p_value);
		column.selected = int(selected - column.values.begin());
		if (selected == column.values.end() || *selected != p_value) column.values.insert(selected, p_value);
		column.first = column.loops() ? column.selected :
			std::min(column.selected, std::max(0, int(column.values.size()) - row_count));
		state.columns.push_back(std::move(column));
		};
	add(Unit::hour, state.use_meridiem ? hour % 12 : hour, 3600000, state.use_meridiem ? 12 : 24);
	add(Unit::minute, minute, 60000, 60);
	const bool show_ms = std::fmod(step, 1000.0) != 0 || ms != 0 || std::fmod(base, 1000.0) != 0;
	if (show_ms || second || std::fmod(step, 60000.0) != 0 || std::fmod(base, 60000.0) != 0) add(Unit::second, second, 1000, 60);
	if (show_ms) add(Unit::millisecond, ms, 1, 1000);
	if (state.use_meridiem) state.columns.push_back({ Unit::meridiem,{ hour / 12, 1 - hour / 12 }, 0, 0 });
	state.active_column = std::min(state.active_column, int(state.columns.size()) - 1);
}

void HTMLLabel::update_clock_picker() {
	const int clock_width = time_picker.has_clock() ? 4 + column_width * int(time_picker.columns.size()) : 0;
	const Vector2 extent(time_picker.calendar_width() + clock_width,
		time_picker.has_calendar() ? time_picker.calendar_height() : 260);
	if (!place_input_picker_canvas(*time_picker_canvas, extent)) return;
	time_picker_canvas->show();
	time_picker_canvas->queue_redraw();
}

void HTMLLabel::draw_clock_picker() {
	if (!input_picker_valid()) return;
	auto& panel = *time_picker_canvas;
	const auto& state = time_picker;
	const bool dark = layout->get_element_color_scheme(picker_input) == litehtml::color_scheme::dark;
	const Color background = dark ? Color::html("#3b3b3b") : Color(1, 1, 1);
	const Color border = dark ? Color::html("#858585") : Color::html("#bfbfbf");
	const Color ink = dark ? Color(1, 1, 1) : Color::html("#101010");
	const Color selected = dark ? Color::html("#99c8ff") : Color::html("#0075ff");
	const Color selected_ink = dark ? background : Color(1, 1, 1);
	const Color hover = dark ? Color::html("#d1e6ff") : Color(0.0f, 0.46f, 1.0f, 0.3f);
	const Color hover_ink = dark ? background : ink;
	const Rect2 bounds(Vector2(), panel.get_size());
	panel.draw_rect(bounds, background);
	panel.draw_rect(bounds.grow(-0.5), border, false, 1.0f);
	const auto text_server = TextServerManager::get_singleton()->get_primary_interface();
	const auto text = [&](const String& p_text, const Rect2& p_rect, const Color& p_ink, int p_size, bool p_bold = false,
		bool p_left = false) {
			const auto line = layout->shape_control_text(picker_input, p_text, p_size, p_bold);
			if (!line.is_valid() || text_server.is_null()) return;
			const Vector2 pos(p_left ? p_rect.position.x : p_rect.get_center().x - line.width() * 0.5f,
				p_rect.get_center().y + (line.line_ascent() - line.line_descent()) * 0.5f);
			text_server->shaped_text_draw(line.get_rid(), panel.get_canvas_item(), pos, -1, -1, p_ink);
		};
	const auto icon = [&](HTMLControlIcon p_icon, const Rect2& p_rect) {
		const auto image = layout->get_control_icon(p_icon);
		if (image.is_valid()) panel.draw_texture_rect(image, Rect2(p_rect.get_center() - Vector2(8, 8), Vector2(16, 16)), false, ink);
		};
	const auto highlight = [&](Color p_fill, bool p_border) {
		Ref<StyleBoxFlat> box;
		box.instantiate();
		box->set_bg_color(p_fill);
		box->set_corner_radius_all(2);
		if (p_border) {
			box->set_border_width_all(2);
			box->set_border_color(ink);
		}
		return box;
		};
	const auto selected_style = highlight(selected, dark);
	const auto focused_style = dark ? selected_style : highlight(selected, true);
	const auto hover_style = highlight(hover, false);
	const auto cell = [&](const Rect2& p_rect, bool p_on, bool p_hover, bool p_focus) {
		if (p_on || p_hover) panel.draw_style_box(p_on ? (p_focus ? focused_style : selected_style) : hover_style, p_rect);
		return p_on ? selected_ink : p_hover ? hover_ink : ink;
		};
	const int offset_x = state.calendar_width();
	using Unit = TimePickerState::Unit;
	for (int col = 0; col < int(state.columns.size()); ++col) {
		const auto& column = state.columns[col];
		for (int row = 0; row < row_count; ++row) {
			const int index = column.row_index(row);
			if (index < 0) break;
			const Rect2 rect = clock_cell(offset_x, col, row);
			const bool on = index == column.selected;
			const bool over = state.hovered_cell == Vector2i(col, row);
			const int value = column.values[index];
			const auto label = column.unit == Unit::meridiem ? dockit::html_ui_translate(value ? "PM" : "AM", "time_picker.meridiem") :
				vformat(column.unit == Unit::millisecond ? "%03d" : "%02d",
					column.unit == Unit::hour && state.use_meridiem && value == 0 ? 12 : value);
			text(label, rect, cell(rect, on, over, on && state.active_column == col), 14, on);
		}
	}
	if (!state.has_calendar()) return;
	if (state.has_clock()) panel.draw_line(Vector2(offset_x, 1), Vector2(offset_x, bounds.size.y - 1), border, 1);
	const Rect2 previous = previous_month(offset_x);
	const Rect2 next = next_month(offset_x);
	if (state.calendar_hover == previous_id) panel.draw_rect(previous, hover);
	if (state.calendar_hover == next_id) panel.draw_rect(next, hover);
	icon(HTMLControlIcon::left, previous);
	icon(HTMLControlIcon::right, next);
	const Vector2i month = state.calendar_month;
	String caption;
	if (state.year_view) {
		caption = String::num_int64(month.x);
	} else {
		Dictionary values;
		values["month"] = localized_month(month.y - 1);
		values["year"] = month.x;
		caption = dockit::html_ui_translate("{month} {year}", "time_picker.month_year").format(values);
	}
	text(caption, calendar_title, ink, 12, true, true);
	if (state.kind != calendar::Kind::month) icon(HTMLControlIcon::down, Rect2(145, 11, 23, 24));
	const CalendarBounds limits(state.kind, *picker_input.ptr());
	const auto chosen = calendar::from_parts(state.kind, picker_date);
	const auto today = calendar::from_parts(calendar::Kind::date, Time::get_singleton()->get_datetime_dict_from_system());
	if (state.year_view) {
		for (int i = 0; i < 12; ++i) {
			const Rect2 rect = calendar_month_cell(i, offset_x);
			const auto day = first_of_month({ month.x, i + 1 });
			const bool enabled = state.kind != calendar::Kind::month || (day && limits.allows(limits.value(*day)));
			const bool on = enabled && int(picker_date["year"]) == month.x && int(picker_date["month"]) == i + 1;
			auto color = cell(rect, on, enabled && state.calendar_hover == i, on && state.active_column < 0);
			if (!enabled) color.a *= 0.3f;
			text(localized_month(i).substr(0, 3), rect, color, 12, on);
		}
	}
	else if (const auto start = month_start(month, state.sunday_first)) {
		const bool week = state.kind == calendar::Kind::week;
		const int week_offset = week ? week_number_width : 0;
		if (week) {
			text(dockit::html_ui_translate("Week", "time_picker"), Rect2(calendar_inset, 39, week_number_width, day_height), ink, 12);
			const real_t x = calendar_inset + week_offset;
			panel.draw_line(Vector2(x, 39), Vector2(x, calendar_top + calendar_rows * day_height), border, 1);
		}
		for (int i = 0; i < days_per_row; ++i) {
			const int index = (i + (state.sunday_first ? 6 : 0)) % days_per_row;
			const Rect2 header(calendar_inset + week_offset + i * day_size, 39, day_size, day_height);
			text(dockit::html_ui_translate(String::utf8(weekdays[index]), "time_picker.weekday"), header, ink, 12);
		}
		if (week) {
			for (int row = 0; row < calendar_rows; ++row) {
				const double monday = *start + row * days_per_row * calendar::day;
				const auto value = calendar::format(calendar::Kind::week, monday);
				if (value.is_empty()) continue;
				const bool enabled = limits.allows(monday);
				const bool on = enabled && chosen && *chosen == monday;
				auto color = cell(calendar_week_cell(row), on, enabled && state.calendar_hover == row, false);
				if (!enabled) color.a *= 0.3f;
				text(value.get_slice("W", 1), calendar_week_cell(row), color, 12, on);
			}
		}
		const auto chosen_day = calendar::from_parts(calendar::Kind::date, picker_date);
		for (int i = 0; i < days_per_row * calendar_rows; ++i) {
			const double date = *start + i * calendar::day;
			const auto parts = calendar::parts(calendar::Kind::date, date);
			if (parts.is_empty()) continue;
			const Rect2 rect = calendar_day_cell(i, week_offset);
			const int item = week ? i / days_per_row : i;
			const double monday = *start + item * days_per_row * calendar::day;
			const bool enabled = limits.allows(week ? monday : date);
			const bool on = enabled && (week ? (chosen && *chosen == monday) :
				(chosen_day && *chosen_day == date));
			const bool over = enabled && state.calendar_hover == item;
			auto color = cell(rect, on, over, on && !week && state.active_column < 0);
			if (enabled && today && *today == date && !on) panel.draw_rect(rect.grow(-0.5), border, false, 1);
			if (!enabled) color.a *= 0.3f;
			else if (!on && !over && int(parts["month"]) != month.y) color.a *= 0.55f;
			text(String::num_int64(int(parts["day"])), rect, color, 12, on);
		}
	}
	const Color action = dark ? selected : Color::html("#0078d4");
	const bool can_clear = !picker_input->has_attribute("required");
	const bool can_select_today = today && limits.allows(limits.value(*today));
	const auto today_rect = today_button(offset_x, state.calendar_height());
	const auto clear_rect = clear_button(state.calendar_height());
	if (can_clear) {
		if (state.calendar_hover == clear_id) panel.draw_rect(clear_rect, hover);
		text(dockit::html_ui_translate("Clear", "time_picker"), clear_rect, action, 12);
	}
	if (can_select_today && state.calendar_hover == today_id) panel.draw_rect(today_rect, hover);
	const String today_message = state.kind == calendar::Kind::month ? "This month" :
		state.kind == calendar::Kind::week ? "This week" : "Today";
	const String today_label = dockit::html_ui_translate(today_message, "time_picker");
	text(today_label, today_rect, can_select_today ? action : Color(ink, 0.3f), 12);
}

void HTMLLabel::preview_clock_value() {
	auto& state = time_picker;
	const int day_half = state.use_meridiem ? state.columns.back().values[state.columns.back().selected] : 0;
	constexpr const char* names[] = { "hour", "minute", "second", "millisecond" };
	for (const auto& column : state.columns) {
		if (column.unit == TimePickerState::meridiem) continue;
		const int value = column.values[column.selected];
		picker_date[names[column.unit]] = column.unit == TimePickerState::hour ? value + day_half * 12 : value;
	}
	const auto input = picker_input->get_input();
	const auto value = calendar::from_parts(input->time_kind(), picker_date);
	if (value) {
		const String preview = calendar::format(input->time_kind(), *value);
		state.changed = preview != state.initial_value;
		input->set_time_preview(state.changed ? std::optional<String>(preview) : std::nullopt);
		layout->repaint_input();
	}
	time_picker_canvas->queue_redraw();
}

int HTMLLabel::clock_calendar_hit(const Vector2& p_point) const {
	const auto& state = time_picker;
	const int width = state.calendar_width();
	const int height = state.calendar_height();
	if (previous_month(width).has_point(p_point)) return previous_id;
	if (next_month(width).has_point(p_point)) return next_id;
	if (state.kind != calendar::Kind::month && calendar_title.has_point(p_point)) return title_id;
	if (today_button(width, height).has_point(p_point)) return today_id;
	if (!picker_input->has_attribute("required") && clear_button(height).has_point(p_point)) return clear_id;
	if (state.year_view) {
		for (int i = 0; i < 12; ++i) {
			if (calendar_month_cell(i, width).has_point(p_point)) return i;
		}
	}
	else if (state.kind == calendar::Kind::week) {
		const Rect2 grid(calendar_inset, calendar_top, week_number_width + days_per_row * day_size, calendar_rows * day_height);
		if (grid.has_point(p_point)) return int((p_point.y - calendar_top) / day_height);
	}
	else {
		for (int i = 0; i < days_per_row * calendar_rows; ++i) {
			if (calendar_day_cell(i, 0).has_point(p_point)) return i;
		}
	}
	return -1;
}

void HTMLLabel::shift_clock_calendar(int p_months) {
	const Vector2i month = time_picker.calendar_month;
	const int index = (month.x - 1) * 12 + month.y - 1 + p_months;
	if (index < 0 || index >= 275760 * 12) return;
	time_picker.calendar_month = Vector2i(index / 12 + 1, index % 12 + 1);
	time_picker_canvas->queue_redraw();
}

bool HTMLLabel::set_clock_calendar_date(double p_date) {
	const auto kind = time_picker.kind;
	const CalendarBounds bounds(kind, *picker_input.ptr());
	const auto candidate = bounds.value(p_date);
	if (!bounds.allows(candidate)) return false;
	const bool month_or_week = kind == calendar::Kind::month || kind == calendar::Kind::week;
	const auto parts = month_or_week ? calendar::parts(kind, *candidate) : calendar::parts(calendar::Kind::date, p_date);
	if (parts.is_empty()) return false;
	for (const char* name : { "year", "month", "day" }) picker_date[name] = parts[name];
	// The visible month follows the clicked date even when its ISO week starts in the previous month.
	const auto view = month_or_week ? calendar::parts(calendar::Kind::date, p_date) : parts;
	time_picker.calendar_month = Vector2i(int(view["year"]), int(view["month"]));
	time_picker.year_view = kind == calendar::Kind::month;
	preview_clock_value();
	return true;
}

void HTMLLabel::activate_clock_calendar(int p_item) {
	auto& state = time_picker;
	if (p_item < 0) return;
	state.active_column = -1;
	if (p_item == clear_id) {
		commit_picker_value("");
		return;
	}
	if (p_item == title_id) {
		state.year_view = !state.year_view;
	}
	else if (p_item == previous_id || p_item == next_id) {
		shift_clock_calendar((p_item == previous_id ? -1 : 1) * (state.year_view ? 12 : 1));
	}
	else {
		std::optional<double> day;
		if (p_item == today_id) {
			day = calendar::from_parts(calendar::Kind::date, Time::get_singleton()->get_datetime_dict_from_system());
		}
		else if (state.year_view) {
			if (state.kind == calendar::Kind::month) day = first_of_month({ state.calendar_month.x, p_item + 1 });
			else {
				state.calendar_month.y = p_item + 1;
				state.year_view = false;
			}
		}
		else if (const auto first = month_start(state.calendar_month, state.sunday_first)) {
			const int offset = state.kind == calendar::Kind::week ? p_item * days_per_row : p_item;
			day = *first + offset * calendar::day;
		}
		if (day && set_clock_calendar_date(*day) && !state.has_clock()) {
			commit_time_picker();
			return;
		}
	}
	time_picker_canvas->queue_redraw();
}

bool HTMLLabel::handle_clock_picker_input(const Ref<InputEvent>& p_event) {
	if (!input_picker_valid()) {
		close_input_picker();
		return false;
	}
	auto& state = time_picker;
	const Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (!key->is_pressed()) return true;
		const Key code = key->get_keycode() == KEY_NONE ? key->get_physical_keycode() : key->get_keycode();
		const int first = state.has_calendar() ? -1 : 0;
		const int last = int(state.columns.size()) - 1;
		if (code == KEY_ESCAPE) {
			if (state.changed) {
				picker_date = state.initial.duplicate();
				state.calendar_month = Vector2i(int(picker_date["year"]), int(picker_date["month"]));
				state.year_view = state.kind == calendar::Kind::month;
				state.changed = false;
				if (state.has_clock()) configure_clock_columns();
				picker_input->get_input()->set_time_preview(std::nullopt);
				layout->repaint_input();
			}
			else close_input_picker();
		}
		else if (code == KEY_ENTER || code == KEY_KP_ENTER) {
			commit_time_picker();
			return true;
		}
		else if (code == KEY_TAB) {
			const int next = state.active_column + (key->is_shift_pressed() ? -1 : 1);
			state.active_column = next < first ? last : next > last ? first : next;
		}
		else if (state.active_column >= 0) {
			if (code == KEY_LEFT || code == KEY_RIGHT) {
				state.active_column = std::clamp(state.active_column + (code == KEY_RIGHT ? 1 : -1), first, last);
			}
			else if (code == KEY_UP || code == KEY_DOWN || code == KEY_HOME || code == KEY_END) {
				auto& column = state.columns[state.active_column];
				const int count = int(column.values.size());
				const int next = column.selected + (code == KEY_UP ? -1 : 1);
				column.selected = code == KEY_HOME ? 0 : code == KEY_END ? count - 1 :
					column.loops() ? wrap(next, count) : std::clamp(next, 0, count - 1);
				column.reveal();
				preview_clock_value();
			}
		}
		else if (code == KEY_PAGEUP || code == KEY_PAGEDOWN) {
			shift_clock_calendar((code == KEY_PAGEUP ? -1 : 1) * (state.year_view ? 12 : 1));
		}
		else if (code == KEY_LEFT || code == KEY_RIGHT || code == KEY_UP || code == KEY_DOWN) {
			const int sign = code == KEY_LEFT || code == KEY_UP ? -1 : 1;
			const bool vertical = code == KEY_UP || code == KEY_DOWN;
			if (state.kind == calendar::Kind::month) {
				if (const auto current = calendar::from_parts(calendar::Kind::month, picker_date)) {
					const auto value = calendar::format(calendar::Kind::month, *current + sign * (vertical ? months_per_row : 1));
					if (const auto day = calendar::parse(calendar::Kind::date, value + String("-01"))) set_clock_calendar_date(*day);
				}
			}
			else {
				const bool week = state.kind == calendar::Kind::week;
				const auto current = calendar::from_parts(week ? calendar::Kind::week : calendar::Kind::date, picker_date);
				if (current) set_clock_calendar_date(*current + sign * (week || vertical ? days_per_row : 1) * calendar::day);
			}
		}
		time_picker_canvas->queue_redraw();
		return true;
	}
	const Ref<InputEventMouse> mouse = p_event;
	if (mouse.is_null()) return false;
	const auto point = time_picker_canvas->get_global_transform_with_canvas().affine_inverse().xform(mouse->get_position());
	const bool inside = Rect2(Vector2(), time_picker_canvas->get_size()).has_point(point);
	const auto local = get_global_transform_with_canvas().affine_inverse().xform(mouse->get_position());
	const int offset = state.calendar_width();
	Vector2i cell(-1, -1);
	int calendar_item = -1;
	if (inside && point.x < offset) calendar_item = clock_calendar_hit(point);
	else if (inside && point.x >= offset + 4 && point.y >= 4 && point.y < 256) {
		cell = Vector2i(int((point.x - offset - 4) / column_width), int((point.y - 4) / row_height));
		if (cell.x >= int(state.columns.size()) || !clock_cell(offset, cell.x, cell.y).has_point(point) ||
			state.columns[cell.x].row_index(cell.y) < 0) cell = Vector2i(-1, -1);
	}
	const Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid()) {
		state.hovered_cell = cell;
		state.calendar_hover = calendar_item;
		if (state.pressed_cell.x >= 0) {
			const int steps = int((state.press_position.y - point.y) / row_height);
			if (steps) {
				state.columns[state.pressed_cell.x].scroll(steps);
				state.press_position.y -= steps * row_height;
				state.dragged_column = true;
			}
		}
		time_picker_canvas->queue_redraw();
		return inside || state.pressed_cell.x >= 0;
	}
	const Ref<InputEventMouseButton> button = p_event;
	if (button.is_null()) return inside;
	if (button->is_pressed() && (button->get_button_index() == MOUSE_BUTTON_WHEEL_UP ||
		button->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) && inside) {
		const int delta = button->get_button_index() == MOUSE_BUTTON_WHEEL_UP ? -1 : 1;
		if (cell.x >= 0) state.columns[cell.x].scroll(delta * std::max(1, int(button->get_factor())));
		else if (point.x < offset) shift_clock_calendar(delta * (state.year_view ? 12 : 1));
		time_picker_canvas->queue_redraw();
		return true;
	}
	if (button->get_button_index() != MOUSE_BUTTON_LEFT) return inside;
	if (button->is_pressed()) {
		if (!inside) {
			const bool anchor = get_element_at(local) == picker_input;
			if (state.changed) commit_time_picker();
			else close_input_picker();
			return anchor;
		}
		state.pressed_cell = cell;
		state.pressed_calendar = calendar_item;
		state.press_position = point;
		state.dragged_column = false;
		return true;
	}
	const auto pressed = std::exchange(state.pressed_cell, Vector2i(-1, -1));
	const int pressed_calendar = std::exchange(state.pressed_calendar, -1);
	if (!button->is_canceled() && inside) {
		if (cell.x >= 0 && pressed == cell && !state.dragged_column) {
			state.active_column = cell.x;
			auto& column = state.columns[cell.x];
			column.selected = column.row_index(cell.y);
			preview_clock_value();
		}
		else if (calendar_item >= 0 && pressed_calendar == calendar_item) {
			activate_clock_calendar(calendar_item);
		}
	}
	return inside || pressed.x >= 0;
}
