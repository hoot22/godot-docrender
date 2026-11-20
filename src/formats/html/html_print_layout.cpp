#include "html_print_layout.h"

#include "html_document.h"
#include "html_layout.h"
#include "formats/html/backend/litehtml_container.h"
#include "formats/html/paint/html_paint_recorder.h"
#include "formats/pdf/writer/pdf_writer.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace godot;

namespace {
	constexpr double CSS_PX_TO_PT = 0.75;
	constexpr double DEFAULT_A4_WIDTH = 793.7007874015748;
	constexpr double DEFAULT_A4_HEIGHT = 1122.5196850393702;
	constexpr int64_t MAX_PRINT_PAGES = 10000;

	struct PageStyle {
		Vector2 size{DEFAULT_A4_WIDTH, DEFAULT_A4_HEIGHT};
		Vector4 margins{37.79527559055118, 37.79527559055118, 37.79527559055118, 37.79527559055118};
		bool has_size = false;
		std::array<bool, 4> has_margin_side{false, false, false, false};
		String name;
	};

	String trim_lower(const String& p_value) {
		return p_value.strip_edges().to_lower();
	}

	std::vector<String> split_css_values(const String& p_text) {
		std::vector<String> values;
		String current;
		for (int64_t i = 0; i < p_text.length(); ++i) {
			const char32_t character = p_text[i];
			if (character == ' ' || character == '\t' || character == '\r' || character == '\n') {
				if (!current.is_empty()) {
					values.push_back(current);
					current = String();
				}
			} else {
				current += String::chr(character);
			}
		}
		if (!current.is_empty()) values.push_back(current);
		return values;
	}

	double css_length(const String& p_value, double p_fallback) {
		const String value = trim_lower(p_value);
		if (value.is_empty()) return p_fallback;
		const char* units[] = {"mm", "cm", "in", "pt", "pc", "px"};
		const double scales[] = {96.0 / 25.4, 96.0 / 2.54, 96.0, 96.0 / 72.0, 16.0, 1.0};
		for (int i = 0; i < 6; ++i) {
			const String unit = String(units[i]);
			if (value.ends_with(unit)) {
				const double number = value.substr(0, value.length() - unit.length()).to_float();
				return std::isfinite(number) ? number * scales[i] : p_fallback;
			}
		}
		const double number = value.to_float();
		return std::isfinite(number) ? number : p_fallback;
	}

	PageStyle apply_size(PageStyle p_style, const String& p_value) {
		const std::vector<String> values = split_css_values(p_value);
		if (values.empty()) return p_style;
		const String first = trim_lower(values[0]);
		if (first == "a3") p_style.size = Vector2(1122.519685, 1587.401575);
		else if (first == "a4") p_style.size = Vector2(DEFAULT_A4_WIDTH, DEFAULT_A4_HEIGHT);
		else if (first == "a5") p_style.size = Vector2(559.370079, 793.700787);
		else if (first == "letter") p_style.size = Vector2(816.0, 1056.0);
		else if (first == "legal") p_style.size = Vector2(816.0, 1344.0);
		else if (values.size() >= 2) {
			p_style.size = Vector2(css_length(values[0], p_style.size.x), css_length(values[1], p_style.size.y));
		}
		if ((first == "landscape") || (values.size() >= 2 && trim_lower(values[1]) == "landscape")) {
			if (p_style.size.x < p_style.size.y) std::swap(p_style.size.x, p_style.size.y);
		} else if ((first == "portrait") || (values.size() >= 2 && trim_lower(values[1]) == "portrait")) {
			if (p_style.size.x > p_style.size.y) std::swap(p_style.size.x, p_style.size.y);
		}
		p_style.has_size = true;
		return p_style;
	}

	PageStyle apply_margins(PageStyle p_style, const String& p_value) {
		const std::vector<String> values = split_css_values(p_value);
		if (values.empty()) return p_style;
		std::vector<double> parsed;
		parsed.reserve(values.size());
		for (const String& value : values) parsed.push_back(css_length(value, 0.0));
		const double top = parsed[0];
		const double right = parsed.size() > 1 ? parsed[1] : top;
		const double bottom = parsed.size() > 2 ? parsed[2] : top;
		const double left = parsed.size() > 3 ? parsed[3] : right;
		p_style.margins = Vector4(left, top, right, bottom);
		p_style.has_margin_side.fill(true);
		return p_style;
	}

	void apply_page_declarations(PageStyle& r_style, const String& p_body) {
		const PackedStringArray declarations = p_body.split(";", false);
		for (int64_t i = 0; i < declarations.size(); ++i) {
			const int64_t colon = declarations[i].find(":");
			if (colon < 0) continue;
			const String name = trim_lower(declarations[i].substr(0, colon));
			const String value = declarations[i].substr(colon + 1).strip_edges();
			if (name == "size") r_style = apply_size(r_style, value);
			else if (name == "margin") r_style = apply_margins(r_style, value);
			else if (name == "margin-top" || name == "margin-right" || name == "margin-bottom" || name == "margin-left") {
				const double parsed = css_length(value, 0.0);
				if (name == "margin-left") { r_style.margins.x = parsed; r_style.has_margin_side[0] = true; }
				else if (name == "margin-top") { r_style.margins.y = parsed; r_style.has_margin_side[1] = true; }
				else if (name == "margin-right") { r_style.margins.z = parsed; r_style.has_margin_side[2] = true; }
				else { r_style.margins.w = parsed; r_style.has_margin_side[3] = true; }
			}
		}
	}

	void parse_page_rules(const String& p_css, PageStyle& r_default, PageStyle& r_first, PageStyle& r_left,
		PageStyle& r_right, std::vector<PageStyle>& r_named) {
		String lower = p_css.to_lower();
		int64_t cursor = 0;
		while ((cursor = lower.find("@page", cursor)) >= 0) {
			const int64_t name_start = cursor + 5;
			const int64_t open = lower.find("{", name_start);
			if (open < 0) break;
			const int64_t close = lower.find("}", open + 1);
			if (close < 0) break;
			const String selector = lower.substr(name_start, open - name_start).strip_edges();
			const String body = p_css.substr(open + 1, close - open - 1);
			if (selector.is_empty()) apply_page_declarations(r_default, body);
			else if (selector == ":first") apply_page_declarations(r_first, body);
			else if (selector == ":left") apply_page_declarations(r_left, body);
			else if (selector == ":right") apply_page_declarations(r_right, body);
			else {
				PageStyle named = r_default;
				named.name = selector;
				apply_page_declarations(named, body);
				r_named.push_back(named);
			}
			cursor = close + 1;
		}
	}

	bool is_forced_break_value(const String& p_value) {
		const String value = trim_lower(p_value).get_slice("!", 0).strip_edges();
		return value == "page" || value == "always" || value == "left" || value == "right";
	}

	std::pair<std::optional<bool>, std::optional<bool>> forced_break_declarations(const String& p_body) {
		std::optional<bool> before;
		std::optional<bool> after;
		const PackedStringArray declarations = p_body.split(";", false);
		for (int64_t i = 0; i < declarations.size(); ++i) {
			const int64_t colon = declarations[i].find(":");
			if (colon < 0) continue;
			const String name = trim_lower(declarations[i].substr(0, colon));
			if (name == "break-before" || name == "page-break-before") before = is_forced_break_value(declarations[i].substr(colon + 1));
			else if (name == "break-after" || name == "page-break-after") after = is_forced_break_value(declarations[i].substr(colon + 1));
		}
		return {before, after};
	}

	void add_element_break_positions(const TypedArray<HTMLElement>& p_elements, bool p_before, bool p_after,
		std::vector<double>& r_breaks) {
		for (int64_t i = 0; i < p_elements.size(); ++i) {
			const Ref<HTMLElement> element = p_elements[i];
			if (element.is_null()) continue;
			const TypedArray<Rect2> rects = element->get_client_rects();
			if (rects.is_empty()) continue;
			const Rect2 first = rects[0];
			const Rect2 last = rects[rects.size() - 1];
			if (p_before) r_breaks.push_back(first.position.y);
			if (p_after) r_breaks.push_back(last.get_end().y);
		}
	}

	void collect_break_rule_block(const String& p_css, HTMLLayout& p_layout, std::vector<double>& r_breaks) {
		int64_t segment_start = 0;
		int64_t cursor = 0;
		while (cursor < p_css.length()) {
			const int64_t open = p_css.find("{", cursor);
			if (open < 0) break;
			int depth = 1;
			int64_t close = open + 1;
			for (; close < p_css.length() && depth > 0; ++close) {
				if (p_css[close] == '{') ++depth;
				else if (p_css[close] == '}') --depth;
			}
			if (depth != 0) break;
			--close;
			const String selector = p_css.substr(segment_start, open - segment_start).strip_edges();
			const String body = p_css.substr(open + 1, close - open - 1);
			const String lowered = selector.to_lower();
			if (lowered.begins_with("@media")) {
				if (!lowered.contains("screen") || lowered.contains("print")) collect_break_rule_block(body, p_layout, r_breaks);
			} else if (lowered.begins_with("@supports")) {
				collect_break_rule_block(body, p_layout, r_breaks);
			} else if (!selector.begins_with("@")) {
				const auto declarations = forced_break_declarations(body);
				if (declarations.first.value_or(false) || declarations.second.value_or(false)) {
					for (const String& item : selector.split(",", false)) {
						const String trimmed = item.strip_edges();
						if (trimmed.is_empty()) continue;
						add_element_break_positions(p_layout.query_selector_all(trimmed), declarations.first.value_or(false),
							declarations.second.value_or(false), r_breaks);
					}
				}
			}
			cursor = close + 1;
			segment_start = cursor;
		}
	}

	void collect_embedded_css(const String& p_html, std::vector<String>& r_css) {
		const String lower = p_html.to_lower();
		int64_t cursor = 0;
		while ((cursor = lower.find("<style", cursor)) >= 0) {
			const int64_t open = lower.find(">", cursor + 6);
			if (open < 0) break;
			const int64_t close = lower.find("</style", open + 1);
			if (close < 0) break;
			r_css.push_back(p_html.substr(open + 1, close - open - 1));
			cursor = close + 7;
		}
	}

	void collect_page_break_positions(const String& p_html, const String& p_stylesheet, const Dictionary& p_local_stylesheets,
		HTMLLayout& p_layout, std::vector<double>& r_breaks) {
		r_breaks.clear();
		const TypedArray<HTMLElement> all = p_layout.query_selector_all("*");
		for (int64_t i = 0; i < all.size(); ++i) {
			const Ref<HTMLElement> element = all[i];
			if (element.is_null()) continue;
			const auto before = is_forced_break_value(element->get_style_property("break-before")) ||
				is_forced_break_value(element->get_style_property("page-break-before"));
			const auto after = is_forced_break_value(element->get_style_property("break-after")) ||
				is_forced_break_value(element->get_style_property("page-break-after"));
			if (before || after) {
				TypedArray<HTMLElement> one;
				one.append(element);
				add_element_break_positions(one, before, after, r_breaks);
			}
		}
		std::vector<String> sheets;
		collect_embedded_css(p_html, sheets);
		if (!p_stylesheet.is_empty()) sheets.push_back(p_stylesheet);
		for (const Variant& sheet : p_local_stylesheets.values()) {
			if (sheet.get_type() == Variant::STRING) sheets.push_back(sheet);
		}
		for (const String& sheet : sheets) collect_break_rule_block(sheet, p_layout, r_breaks);
		std::sort(r_breaks.begin(), r_breaks.end());
		r_breaks.erase(std::unique(r_breaks.begin(), r_breaks.end(), [](double p_a, double p_b) {
			return std::abs(p_a - p_b) < 0.5;
		}), r_breaks.end());
	}

	void apply_option_overrides(const Dictionary& p_options, PageStyle& r_style) {
		if (p_options.has("page_size")) {
			const Variant raw = p_options["page_size"];
			if (raw.get_type() == Variant::VECTOR2) {
				const Vector2 size = raw;
				if (size.is_finite() && size.x > 0 && size.y > 0) {
					r_style.size = size;
					r_style.has_size = true;
				}
			}
		}
		if (p_options.has("orientation")) {
			const String orientation = trim_lower(p_options["orientation"]);
			if (orientation == "landscape" && r_style.size.x < r_style.size.y) std::swap(r_style.size.x, r_style.size.y);
			else if (orientation == "portrait" && r_style.size.x > r_style.size.y) std::swap(r_style.size.x, r_style.size.y);
		}
		if (p_options.has("margin")) {
			const Variant value = p_options["margin"];
			if (value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT) {
				const double margin = value;
				r_style.margins = Vector4(margin, margin, margin, margin);
			} else if (value.get_type() == Variant::VECTOR4) {
				r_style.margins = value;
			} else if (value.get_type() == Variant::DICTIONARY) {
				const Dictionary margins = value;
				r_style.margins = Vector4(margins.get("left", r_style.margins.x), margins.get("top", r_style.margins.y),
					margins.get("right", r_style.margins.z), margins.get("bottom", r_style.margins.w));
			}
		}
	}

	Rect2 rect_for_pdf(const Rect2& p_rect, double p_source_y, double p_source_end, const Rect2& p_page_content,
		double p_page_height_pt) {
		const Rect2 page_source(0, p_source_y, p_page_content.size.x, p_source_end - p_source_y);
		const Rect2 clipped = p_rect.intersection(page_source);
		if (!clipped.has_area()) return {};
		const double x = (p_page_content.position.x + clipped.position.x) * CSS_PX_TO_PT;
		const double y_top = (p_page_content.position.y + clipped.position.y - page_source.position.y) * CSS_PX_TO_PT;
		return Rect2(x, p_page_height_pt - y_top - clipped.size.y * CSS_PX_TO_PT,
			clipped.size.x * CSS_PX_TO_PT, clipped.size.y * CSS_PX_TO_PT);
	}

	Color opaque_or_transparent(const Color& p_color) {
		return p_color;
	}
}

HTMLPrintLayout::~HTMLPrintLayout() {
	if (layout.is_valid()) layout->set_resource_layout_callback(Callable());
}

void HTMLPrintLayout::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_ready"), &HTMLPrintLayout::is_ready);
	ClassDB::bind_method(D_METHOD("get_page_count"), &HTMLPrintLayout::get_page_count);
	ClassDB::bind_method(D_METHOD("get_page_size", "page_index"), &HTMLPrintLayout::get_page_size);
	ClassDB::bind_method(D_METHOD("get_page_content_rect", "page_index"), &HTMLPrintLayout::get_page_content_rect);
	ClassDB::bind_method(D_METHOD("get_diagnostics"), &HTMLPrintLayout::get_diagnostics);
	ClassDB::bind_method(D_METHOD("get_error"), &HTMLPrintLayout::get_error);
	ClassDB::bind_method(D_METHOD("get_error_message"), &HTMLPrintLayout::get_error_message);
	ClassDB::bind_method(D_METHOD("to_pdf"), &HTMLPrintLayout::to_pdf);
	ClassDB::bind_method(D_METHOD("save_pdf", "path"), &HTMLPrintLayout::save_pdf);
	ADD_SIGNAL(MethodInfo("ready"));
}

void HTMLPrintLayout::prepare(const HTMLDocument& p_document, const Dictionary& p_options) {
	layout = p_document.create_layout();
	if (layout.is_null()) {
		layout_error = pending_error = ERR_CANT_CREATE;
		queue_ready_signal();
		return;
	}
	layout->set_print_media(true);
	layout->set_resource_layout_callback(callable_mp(this, &HTMLPrintLayout::on_resources_changed));
	PageStyle page_style;
	PageStyle first_style = page_style;
	PageStyle left_style = page_style;
	PageStyle right_style = page_style;
	std::vector<PageStyle> named_styles;
	parse_page_rules(p_document.text, page_style, first_style, left_style, right_style, named_styles);
	parse_page_rules(p_document.stylesheet, page_style, first_style, left_style, right_style, named_styles);
	for (const Variant& sheet : p_document.local_stylesheets.values()) {
		if (sheet.get_type() == Variant::STRING) parse_page_rules(sheet, page_style, first_style, left_style, right_style, named_styles);
	}
	if (!first_style.has_size) first_style.size = page_style.size;
	if (!first_style.has_margin_side[0]) first_style.margins.x = page_style.margins.x;
	if (!first_style.has_margin_side[1]) first_style.margins.y = page_style.margins.y;
	if (!first_style.has_margin_side[2]) first_style.margins.z = page_style.margins.z;
	if (!first_style.has_margin_side[3]) first_style.margins.w = page_style.margins.w;
	if (!left_style.has_size) left_style.size = page_style.size;
	if (!left_style.has_margin_side[0]) left_style.margins.x = page_style.margins.x;
	if (!left_style.has_margin_side[1]) left_style.margins.y = page_style.margins.y;
	if (!left_style.has_margin_side[2]) left_style.margins.z = page_style.margins.z;
	if (!left_style.has_margin_side[3]) left_style.margins.w = page_style.margins.w;
	if (!right_style.has_size) right_style.size = page_style.size;
	if (!right_style.has_margin_side[0]) right_style.margins.x = page_style.margins.x;
	if (!right_style.has_margin_side[1]) right_style.margins.y = page_style.margins.y;
	if (!right_style.has_margin_side[2]) right_style.margins.z = page_style.margins.z;
	if (!right_style.has_margin_side[3]) right_style.margins.w = page_style.margins.w;
	for (PageStyle& named : named_styles) {
		if (!named.has_size) named.size = page_style.size;
		if (!named.has_margin_side[0]) named.margins.x = page_style.margins.x;
		if (!named.has_margin_side[1]) named.margins.y = page_style.margins.y;
		if (!named.has_margin_side[2]) named.margins.z = page_style.margins.z;
		if (!named.has_margin_side[3]) named.margins.w = page_style.margins.w;
	}
	apply_option_overrides(p_options, page_style);
	apply_option_overrides(p_options, first_style);
	apply_option_overrides(p_options, left_style);
	apply_option_overrides(p_options, right_style);
	if (p_options.has("page_name")) {
		const String wanted = trim_lower(p_options["page_name"]);
		for (const PageStyle& named : named_styles) {
			if (trim_lower(named.name) == wanted) {
				page_style = named;
				apply_option_overrides(p_options, page_style);
				first_style = page_style;
				left_style = page_style;
				right_style = page_style;
				break;
			}
		}
	}
	if (p_options.has("page_size") && p_options["page_size"].get_type() == Variant::VECTOR2) {
		first_style.size = page_style.size;
		left_style.size = page_style.size;
		right_style.size = page_style.size;
	}
	if (p_options.has("orientation")) {
		first_style.size = page_style.size;
		left_style.size = page_style.size;
		right_style.size = page_style.size;
	}
	if (p_options.has("margin")) {
		first_style.margins = page_style.margins;
		left_style.margins = page_style.margins;
		right_style.margins = page_style.margins;
	}
	if (!named_styles.empty() && !p_options.has("page_name")) {
		add_diagnostic_once("Named @page rules require the print option page_name; element-level page assignment is not available.");
	}
	const auto valid_page_style = [](const PageStyle& p_style) {
		return std::isfinite(p_style.size.x) && std::isfinite(p_style.size.y) && std::isfinite(p_style.margins.x) &&
			std::isfinite(p_style.margins.y) && std::isfinite(p_style.margins.z) && std::isfinite(p_style.margins.w) &&
			p_style.size.x > p_style.margins.x + p_style.margins.z && p_style.size.y > p_style.margins.y + p_style.margins.w &&
			p_style.margins.x >= 0 && p_style.margins.y >= 0 && p_style.margins.z >= 0 && p_style.margins.w >= 0 &&
			p_style.size.x <= std::numeric_limits<int32_t>::max() && p_style.size.y <= std::numeric_limits<int32_t>::max();
	};
	if (!valid_page_style(page_style) || !valid_page_style(first_style) || !valid_page_style(left_style) || !valid_page_style(right_style)) {
		layout_error = pending_error = ERR_INVALID_PARAMETER;
		add_diagnostic_once("The selected page margins leave no printable area.");
		ready = true;
		queue_ready_signal();
		return;
	}
	const double base_content_width = page_style.size.x - page_style.margins.x - page_style.margins.z;
	if (std::abs(first_style.size.x - first_style.margins.x - first_style.margins.z - base_content_width) > 0.5 ||
		std::abs(left_style.size.x - left_style.margins.x - left_style.margins.z - base_content_width) > 0.5 ||
		std::abs(right_style.size.x - right_style.margins.x - right_style.margins.z - base_content_width) > 0.5) {
		add_diagnostic_once("Page-specific content widths do not trigger separate text reflow; all pages share one layout width.");
	}
	this->first_style = {first_style.size, first_style.margins};
	this->left_style = {left_style.size, left_style.margins};
	this->right_style = {right_style.size, right_style.margins};
	layout_width = int64_t(std::floor(page_style.size.x - page_style.margins.x - page_style.margins.z));
	if (layout_width <= 0 || layout->layout(Vector2i(int32_t(layout_width), int32_t(std::max<double>(1.0,
		double(page_style.size.y - page_style.margins.y - page_style.margins.w))))) != OK) {
		layout_error = pending_error = ERR_INVALID_DATA;
		add_diagnostic_once("HTML print layout failed; inspect the document resource and layout diagnostics.");
		ready = true;
		queue_ready_signal();
		return;
	}
	source_html = p_document.text;
	source_stylesheet = p_document.stylesheet;
	local_stylesheets = p_document.local_stylesheets.duplicate();
	collect_page_break_positions(source_html, source_stylesheet, local_stylesheets, *layout.ptr(), forced_page_breaks);
	rebuild_pages();
	if (layout->pending_resource_count() == 0) {
		ready = true;
		queue_ready_signal();
	}
}

void HTMLPrintLayout::rebuild_pages() {
	pages.clear();
	const double content_height = std::max(0.0, double(layout->get_content_size().y));
	double source_y = 0.0;
	size_t next_break = 0;
	for (int64_t index = 0; index < MAX_PRINT_PAGES; ++index) {
		const HTMLPrintPageStyle& selected = index == 0 ? first_style : (index % 2 == 1 ? left_style : right_style);
		const double width = selected.size.x - selected.margins.x - selected.margins.z;
		const double height = selected.size.y - selected.margins.y - selected.margins.w;
		if (width <= 0 || height <= 0) break;
		while (next_break < forced_page_breaks.size() && forced_page_breaks[next_break] <= source_y + 0.5) ++next_break;
		double source_end = std::min(content_height, source_y + height);
		if (next_break < forced_page_breaks.size() && forced_page_breaks[next_break] < source_end - 0.5) {
			source_end = forced_page_breaks[next_break++];
		}
		if (source_end <= source_y) source_end = std::min(content_height, source_y + height);
		pages.push_back({selected.size, Rect2(selected.margins.x, selected.margins.y, width, height), source_y, source_end});
		source_y = source_end;
		if (source_y >= content_height && index >= 0) break;
	}
	if (pages.empty()) {
		layout_error = pending_error = ERR_INVALID_PARAMETER;
		add_diagnostic_once("The selected page margins leave no printable area.");
	}
	if (pages.size() >= MAX_PRINT_PAGES && source_y < content_height) {
		layout_error = pending_error = ERR_OUT_OF_MEMORY;
		add_diagnostic_once("The document exceeds the 10,000 page print layout limit (content height: " + String::num_real(content_height) + " CSS px).");
	}
	if (layout->get_content_size().y > 0) {
		add_diagnostic_once("Pagination uses continuous layout slices; break-before/after and legacy page-break-before/after page values are honored. break-inside, column values, widows, and orphans are not applied.");
	}
}

void HTMLPrintLayout::on_resources_changed() {
	if (!layout.is_valid()) return;
	const int64_t height = std::max<int64_t>(1, pages.empty() ? 1 : int64_t(pages.front().content_rect.size.y));
	if (layout->layout(Vector2i(int32_t(layout_width), int32_t(height))) != OK) {
		layout_error = pending_error = ERR_INVALID_DATA;
		add_diagnostic_once("HTML print layout failed after a remote resource completed.");
		ready = true;
		queue_ready_signal();
		return;
	}
	collect_page_break_positions(source_html, source_stylesheet, local_stylesheets, *layout.ptr(), forced_page_breaks);
	rebuild_pages();
	if (layout->get_failed_remote_resource_count() > 0) {
		add_diagnostic_once("One or more remote print resources failed to load; the PDF uses the available fallback content.");
	}
	if (!ready && layout->pending_resource_count() == 0) {
		ready = true;
		queue_ready_signal();
	}
}

void HTMLPrintLayout::emit_ready_signal() {
	ready_signal_queued = false;
	emit_signal("ready");
}

void HTMLPrintLayout::queue_ready_signal() {
	if (ready_signal_queued) return;
	ready_signal_queued = true;
	callable_mp(this, &HTMLPrintLayout::emit_ready_signal).call_deferred();
}

void HTMLPrintLayout::add_diagnostic_once(const String& p_message) {
	if (!diagnostics.has(p_message)) diagnostics.append(p_message);
}

bool HTMLPrintLayout::is_ready() const { return ready; }
int64_t HTMLPrintLayout::get_page_count() const { return pages.size(); }

Vector2 HTMLPrintLayout::get_page_size(int64_t p_page_index) const {
	return p_page_index >= 0 && p_page_index < int64_t(pages.size()) ? pages[size_t(p_page_index)].size : Vector2();
}

Rect2 HTMLPrintLayout::get_page_content_rect(int64_t p_page_index) const {
	return p_page_index >= 0 && p_page_index < int64_t(pages.size()) ? pages[size_t(p_page_index)].content_rect : Rect2();
}

PackedStringArray HTMLPrintLayout::get_diagnostics() const { return diagnostics; }
int64_t HTMLPrintLayout::get_error() const { return pending_error; }

String HTMLPrintLayout::get_error_message() const {
	if (pending_error == OK) return String();
	if (pending_error == ERR_UNAVAILABLE) return "PDF export is unavailable because this build has no PDFium writer backend.";
	if (pending_error == ERR_INVALID_PARAMETER) return "The print page configuration is invalid.";
	if (pending_error == ERR_OUT_OF_MEMORY) return "The print layout exceeded its page limit.";
	if (pending_error == ERR_BUSY) return "The print layout is waiting for remote resources.";
	return "The HTML document could not be laid out for printing.";
}

PackedByteArray HTMLPrintLayout::to_pdf() {
	if (!ready) {
		pending_error = ERR_BUSY;
		return {};
	}
	if (layout_error != OK) {
		pending_error = layout_error;
		return {};
	}
	pending_error = OK;
	if (!PDFWriter::is_available()) {
		pending_error = ERR_UNAVAILABLE;
		add_diagnostic_once(get_error_message());
		return {};
	}
	Ref<PDFWriter> writer;
	writer.instantiate();
	if (writer->create_document() != OK) {
		pending_error = writer->get_last_error();
		return {};
	}
	const dockit::HTMLPaintRecorder* recorder = layout->get_paint_recorder();
	if (!recorder) {
		pending_error = ERR_UNCONFIGURED;
		return {};
	}
	int64_t standard_font = 0;
	const std::vector<dockit::HTMLPaintRun>& runs = recorder->get_paint_runs();
	const String plain_text = layout->get_plain_text();
	for (size_t page_index = 0; page_index < pages.size(); ++page_index) {
		const Page& page = pages[page_index];
		const Vector2 size_pt(page.size.x * CSS_PX_TO_PT, page.size.y * CSS_PX_TO_PT);
		const int64_t inserted = writer->insert_page(size_pt);
		if (inserted < 0) {
			pending_error = writer->get_last_error();
			return {};
		}
		const Rect2 content_pt(page.content_rect.position.x * CSS_PX_TO_PT,
			(page.size.y - page.content_rect.position.y - page.content_rect.size.y) * CSS_PX_TO_PT,
			page.content_rect.size.x * CSS_PX_TO_PT, page.content_rect.size.y * CSS_PX_TO_PT);
		writer->clip_page(inserted, content_pt);
		for (const dockit::HTMLPaintRun& run : runs) {
			if (run.material.is_valid()) {
				add_diagnostic_once("Some CSS effects use materials that PDF export cannot preserve as vector content.");
			}
			if (!run.clips.empty()) add_diagnostic_once("Element overflow clips are not reproduced in the exported PDF.");
			if (run.matrix != dockit::HTMLPlaneTransform()) add_diagnostic_once("CSS transforms are not reproduced in the exported PDF.");
			for (const dockit::HTMLPaintCommand& command : run.commands) {
				if (const auto* fill = std::get_if<dockit::HTMLFill>(&command)) {
					const Rect2 transformed = run.matrix.xform(fill->rect);
					const Rect2 pdf_rect = rect_for_pdf(transformed, page.source_y, page.source_end, page.content_rect, size_pt.y);
					if (!pdf_rect.has_area() || fill->color.a <= 0) continue;
					const int64_t object = writer->create_rectangle(pdf_rect);
					if (!object) continue;
					writer->set_object_fill_color(object, opaque_or_transparent(fill->color));
					writer->set_path_draw_mode(object, PDFWriter::FILL_WINDING, false);
					writer->insert_object(inserted, object);
				}
				else if (const auto* text = std::get_if<dockit::HTMLText>(&command)) {
					if (!text->layout.is_valid() || text->offset < 0 || text->offset >= plain_text.length()) continue;
					const int64_t text_length = std::max<int64_t>(0, std::min<int64_t>(text->length, plain_text.length() - text->offset));
					if (text_length == 0 || text->color.a <= 0) continue;
					const Vector2 transformed_top = run.matrix.xform(text->position);
					const double baseline_y = transformed_top.y + text->layout.line_ascent();
					if (baseline_y < page.source_y || baseline_y >= page.source_end) continue;
					const String value = plain_text.substr(text->offset, text_length);
					if (value.is_empty()) continue;
					if (!standard_font) standard_font = writer->load_standard_font("Helvetica");
					const float font_size_px = std::max(1.0f, text->layout.line_ascent() + text->layout.line_descent());
					const int64_t object = standard_font ? writer->create_text(standard_font, float(font_size_px * CSS_PX_TO_PT), value) : 0;
					if (!object) {
						add_diagnostic_once("Some text could not be encoded by the PDF standard font and was omitted.");
						continue;
					}
					const Vector2 transformed = run.matrix.xform(text->position + Vector2(0, text->layout.line_ascent()));
					const double local_x = page.content_rect.position.x + transformed.x;
					const double local_y = page.content_rect.position.y + transformed.y - page.source_y;
					const Vector2 origin(local_x * CSS_PX_TO_PT, size_pt.y - local_y * CSS_PX_TO_PT);
					writer->set_object_matrix(object, Transform2D(Vector2(1, 0), Vector2(0, 1), origin));
					writer->set_object_fill_color(object, text->color);
					writer->insert_object(inserted, object);
				}
				else if (const auto* image = std::get_if<dockit::HTMLImage>(&command)) {
					if (image->texture.is_null()) continue;
					const Rect2 transformed = run.matrix.xform(image->destination);
					const Rect2 pdf_rect = rect_for_pdf(transformed, page.source_y, page.source_end, page.content_rect, size_pt.y);
					if (!pdf_rect.has_area()) continue;
					Ref<Image> bitmap = image->texture->get_image();
					if (bitmap.is_null() || bitmap->is_empty()) {
						add_diagnostic_once("Some images could not be decoded for PDF export and were omitted.");
						continue;
					}
					const int64_t object = writer->create_image(bitmap);
					if (!object) continue;
					writer->set_object_matrix(object, Transform2D(Vector2(pdf_rect.size.x, 0), Vector2(0, pdf_rect.size.y),
						Vector2(pdf_rect.position.x, pdf_rect.position.y)));
					writer->insert_object(inserted, object);
				}
				else if (const auto* circle = std::get_if<dockit::HTMLCircle>(&command)) {
					const Vector2 center = run.matrix.xform(circle->center);
					if (center.y < page.source_y || center.y >= page.source_end) continue;
					const double y = page.content_rect.position.y + center.y - page.source_y;
					const double radius = circle->radius * CSS_PX_TO_PT;
					const Vector2 pdf_center((page.content_rect.position.x + center.x) * CSS_PX_TO_PT, size_pt.y - y * CSS_PX_TO_PT);
					const double k = 0.5522847498307936;
					const int64_t object = writer->create_path(Vector2(pdf_center.x + radius, pdf_center.y));
					if (!object) continue;
					writer->path_bezier_to(object, Vector2(pdf_center.x + radius, pdf_center.y + k * radius),
						Vector2(pdf_center.x + k * radius, pdf_center.y + radius), Vector2(pdf_center.x, pdf_center.y + radius));
					writer->path_bezier_to(object, Vector2(pdf_center.x - k * radius, pdf_center.y + radius),
						Vector2(pdf_center.x - radius, pdf_center.y + k * radius), Vector2(pdf_center.x - radius, pdf_center.y));
					writer->path_bezier_to(object, Vector2(pdf_center.x - radius, pdf_center.y - k * radius),
						Vector2(pdf_center.x - k * radius, pdf_center.y - radius), Vector2(pdf_center.x, pdf_center.y - radius));
					writer->path_bezier_to(object, Vector2(pdf_center.x + k * radius, pdf_center.y - radius),
						Vector2(pdf_center.x + radius, pdf_center.y - k * radius), Vector2(pdf_center.x + radius, pdf_center.y));
					writer->path_close(object);
					writer->set_path_draw_mode(object, circle->filled ? PDFWriter::FILL_WINDING : PDFWriter::FILL_NONE, !circle->filled);
					if (circle->filled) writer->set_object_fill_color(object, circle->color);
					else writer->set_object_stroke_color(object, circle->color);
					writer->set_object_stroke_width(object, 0.75f);
					writer->insert_object(inserted, object);
				}
			}
		}
		const Error page_error = writer->generate_page_content(inserted);
		if (page_error != OK) {
			pending_error = writer->get_last_error() == OK ? page_error : writer->get_last_error();
			return {};
		}
	}
	if (standard_font) writer->close_font(standard_font);
	PackedByteArray result = writer->save_to_buffer();
	if (result.is_empty()) pending_error = writer->get_last_error();
	return result;
}

int64_t HTMLPrintLayout::save_pdf(const String& p_path) {
	if (!ready) return ERR_BUSY;
	if (layout_error != OK) return pending_error = layout_error;
	const PackedByteArray pdf = to_pdf();
	if (pdf.is_empty()) return pending_error;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) return pending_error = FileAccess::get_open_error();
	file->store_buffer(pdf);
	return pending_error = file->get_error() == OK ? OK : file->get_error();
}
