#include "html_input_node.h"
#include "html_control_render_item.h"
#include "html_select_node.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "formats/html/backend/litehtml_container.h"
#include "formats/html/css/css_color.h"
#include "formats/html/css/css_parser.h"
#include "formats/html/html_ui_translations.h"
#include "formats/html/markup/html_number.h"

#include <godot_cpp/classes/reg_ex.hpp>
#include <godot_cpp/classes/reg_ex_match.hpp>
#include <litehtml/document.h>
#include <litehtml/el_image.h>
#include <litehtml/render_item.h>
#include <litehtml/render_image.h>

using namespace godot;

namespace dockit {
	HTMLInputNode::HTMLInputNode(const std::shared_ptr<litehtml::document>& p_document) : HTMLTextControlNode(p_document, false) {
		update_pseudos();
	}

	namespace {
		using Type = HTMLInputNode::Type;
		constexpr std::pair<const char*, Type> types[] = {
			{"text", Type::TEXT}, {"search", Type::SEARCH}, {"password", Type::PASSWORD}, {"email", Type::EMAIL},
			{"url", Type::URL}, {"tel", Type::TEL}, {"number", Type::NUMBER}, {"range", Type::RANGE},
			{"checkbox", Type::CHECKBOX}, {"radio", Type::RADIO}, {"hidden", Type::HIDDEN},
			{"button", Type::BUTTON}, {"submit", Type::SUBMIT}, {"reset", Type::RESET},
			{"date", Type::DATE}, {"month", Type::MONTH}, {"week", Type::WEEK}, {"time", Type::TIME},
			{"datetime-local", Type::LOCAL}, {"color", Type::COLOR}, {"file", Type::FILE}, {"image", Type::IMAGE},
		};

		struct NumberLimits {
			std::optional<double> min;
			std::optional<double> max;
			std::optional<double> step;
			double base;
		};

		NumberLimits number_limits(const HTMLInputNode& p_node) {
			const auto kind = p_node.time_kind();
			const auto parse = [kind](const char* p_value) -> std::optional<double> {
				if (kind == input_time::Kind::none) return html_number(p_value, true);
				return p_value ? input_time::parse(kind, String::utf8(p_value)) : std::nullopt;
			};
			auto min = parse(p_node.get_attr("min"));
			auto max = parse(p_node.get_attr("max"));
			auto step = html_number(p_node.get_attr("step"), true);
			if (std::string_view(p_node.get_attr("step", "")) == "any") step.reset();
			else step = (step && *step > 0 ? *step : input_time::step(kind)) * input_time::scale(kind);
			if (p_node.type() == Type::RANGE) {
				min = min.value_or(0);
				max = std::max(*min, max.value_or(100));
			}
			const double base = min.value_or(parse(p_node.get_attr("value")).value_or(input_time::base(kind)));
			return { min, max, step, base };
		}

		double constrain_number(double p_value, const NumberLimits& p_limits) {
			if (p_limits.min) p_value = std::max(*p_limits.min, p_value);
			if (p_limits.max) p_value = std::min(*p_limits.max, p_value);
			if (!p_limits.step) return p_value;
			const double steps = (p_value - p_limits.base) / *p_limits.step;
			if (!std::isfinite(steps)) return p_value;
			double candidate = std::fma(std::floor(steps + 0.5), *p_limits.step, p_limits.base);
			if (p_limits.max && candidate > *p_limits.max) {
				candidate = std::fma(std::floor((*p_limits.max - p_limits.base) / *p_limits.step), *p_limits.step, p_limits.base);
			}
			return std::isfinite(candidate) && (!p_limits.min || candidate >= *p_limits.min) ? candidate : p_value;
		}

		bool ascii_alnum(char32_t p_char) {
			return (p_char >= '0' && p_char <= '9') || (p_char >= 'a' && p_char <= 'z') || (p_char >= 'A' && p_char <= 'Z');
		}

		bool valid_email(const String& p_value) {
			const int64_t at = p_value.find("@");
			if (at <= 0 || at == p_value.length() - 1 || p_value.find("@", at + 1) >= 0) return false;
			const std::string_view special(".!#$%&'*+-/=?^_{}|~");
			for (int64_t i = 0; i < at; ++i) {
				const auto c = p_value[i];
				if (!ascii_alnum(c) && c != 0x60 && (c > 127 || special.find(static_cast<char>(c)) == std::string_view::npos)) return false;
			}
			for (const String& part : p_value.substr(at + 1).split(".", true)) {
				if (part.is_empty() || part.length() > 63 || !ascii_alnum(part[0]) || !ascii_alnum(part[part.length() - 1])) return false;
				for (int64_t i = 0; i < part.length(); ++i) if (!ascii_alnum(part[i]) && part[i] != '-') return false;
			}
			return true;
		}

		bool valid_absolute_url(const String& p_value) {
			const auto colon = p_value.find(":");
			if (colon <= 0) return false;
			const auto scheme = p_value.substr(0, colon).to_lower();
			if (scheme[0] < 'a' || scheme[0] > 'z') return false;
			for (int64_t i = 0; i < scheme.length(); ++i) {
				const auto c = scheme[i];
				if (!ascii_alnum(c) && c != '+' && c != '-' && c != '.') return false;
			}
			for (int64_t i = colon + 1; i < p_value.length(); ++i) if (p_value[i] <= ' ') return false;
			if (scheme == "http" || scheme == "https" || scheme == "ftp") {
				const auto rest = p_value.substr(colon + 1).lstrip("/");
				return !rest.is_empty() && rest[0] != '?' && rest[0] != '#';
			}
			return true;
		}

		std::optional<litehtml::web_color> parsed_color(const String& p_value) {
			const auto tokens = css::normalize(p_value.utf8().get_data(), css::f_componentize | css::f_remove_whitespace);
			litehtml::web_color color;
			if (tokens.size() == 1 && litehtml::parse_color(tokens.front(), color, nullptr) &&
				!color.needs_current_color()) return color;
			return {};
		}

		litehtml::web_color parse_color_value(const String& p_value) {
			return parsed_color(p_value).value_or(litehtml::web_color::black);
		}

		String sanitize_color(const HTMLInputNode& p_input, const String& p_value) {
			const bool alpha = p_input.get_attr("alpha");
			const bool p3 = String::utf8(p_input.get_attr("colorspace", "")).nocasecmp_to("display-p3") == 0;
			auto components = litehtml::css_color_components(parse_color_value(p_value),
				p3 ? litehtml::css_color_space::display_p3 : litehtml::css_color_space::srgb);
			if (!p3) {
				for (int i = 0; i < 3; ++i) components[i] = std::ceil(std::clamp(components[i], 0.0, 1.0) * 255.0 - 1e-9) / 255.0;
				if (!alpha) return "#" + godot::Color(components[0], components[1], components[2], 1).to_html(false).to_lower();
			}
			const String space = p3 ? "display-p3" : "srgb";
			String result = "color(" + space;
			for (int i = 0; i < 3; ++i) result += " " + String::num(components[i], 10);
			if (alpha && components[3] < 1.) result += " / " + String::num(components[3], 10);
			return result + ")";
		}
	}

	HTMLInputNode::Type HTMLInputNode::type() const {
		const String name = String::utf8(get_attr("type", "")).to_lower();
		for (const auto& [label, kind] : types) if (name == label) return kind;
		return Type::TEXT;
	}

	String HTMLInputNode::type_name() const {
		const auto current = type();
		for (const auto& [label, kind] : types) if (current == kind) return label;
		return "text";
	}

	bool HTMLInputNode::is_text_control() const {
		const auto kind = type();
		return kind == Type::TEXT || kind == Type::SEARCH || kind == Type::PASSWORD || kind == Type::EMAIL ||
			kind == Type::URL || kind == Type::TEL || kind == Type::NUMBER || time_kind() != input_time::Kind::none;
	}

	bool HTMLInputNode::is_button() const {
		const auto kind = type();
		return kind == Type::BUTTON || kind == Type::SUBMIT || kind == Type::RESET || kind == Type::IMAGE;
	}

	bool HTMLInputNode::supports_selection() const {
		const auto kind = type();
		return kind == Type::TEXT || kind == Type::SEARCH || kind == Type::TEL || kind == Type::URL || kind == Type::PASSWORD;
	}

	const char* HTMLInputNode::maxlength() const {
		return supports_selection() || type() == Type::EMAIL ? get_attr("maxlength", "") : "";
	}

	bool HTMLInputNode::supports_readonly() const { return is_text_control(); }

	bool HTMLInputNode::supports_required() const { return is_text_control() || is_toggle() || type() == Type::FILE; }

	bool HTMLInputNode::has_picker() const {
		return time_kind() != input_time::Kind::none || type() == Type::COLOR || type() == Type::FILE ||
			(accepts_suggestions() && get_attr("list"));
	}

	input_time::Kind HTMLInputNode::time_kind() const {
		using Kind = input_time::Kind;
		switch (type()) {
			case Type::DATE: return Kind::date;
			case Type::MONTH: return Kind::month;
			case Type::WEEK: return Kind::week;
			case Type::TIME: return Kind::time;
			case Type::LOCAL: return Kind::local;
			default: return Kind::none;
		}
	}

	double HTMLInputNode::step_scale() const { return number_limits(*this).step.value_or(input_time::scale(time_kind())); }
	double HTMLInputNode::step_base() const { return number_limits(*this).base; }

	bool HTMLInputNode::accepts_suggestions() const {
		return (is_text_control() && !is_password()) || type() == Type::COLOR || type() == Type::RANGE;
	}

	Color HTMLInputNode::color_preview(const String& p_value) {
		const auto value = parse_color_value(p_value).to_srgb();
		return Color::from_rgba8(value.red, value.green, value.blue, value.alpha);
	}

	bool HTMLInputNode::is_suggestion_valid(const String& p_value) const {
		switch (type()) {
		case Type::EMAIL: return valid_email(p_value.strip_edges());
		case Type::URL: return valid_absolute_url(p_value.strip_edges());
		case Type::NUMBER: return html_number(p_value.utf8().get_data(), true).has_value();
		case Type::COLOR: return parsed_color(p_value).has_value();
		default: return time_kind() == input_time::Kind::none || input_time::parse(time_kind(), p_value).has_value();
		}
	}

	Dictionary HTMLInputNode::validity() const {
		const Type kind = type();
		const auto actual = get_value();
		bool candidate = !forms::is_disabled(*this) && !is_button() && kind != Type::HIDDEN &&
			!(supports_readonly() && get_attr("readonly"));
		for (auto ancestor = parent(); candidate && ancestor; ancestor = ancestor->parent()) {
			candidate = ancestor->tag() != litehtml::_datalist_;
		}
		bool missing = false, bad_input = false, type_mismatch = false, pattern_mismatch = false;
		bool underflow = false, overflow = false, step_mismatch = false, too_short = false, too_long = false;
		if (candidate) {
			missing = supports_required() && get_attr("required") &&
				(is_toggle() ? !checked : kind == Type::FILE ? files.is_empty() : actual.is_empty());
			if (kind == Type::RADIO) {
				bool required = get_attr("required"), any_checked = checked;
				for (const auto& peer : forms::radio_group(*this)) {
					required |= peer->get_attr("required") != nullptr;
					any_checked |= static_cast<const HTMLInputNode&>(*peer).is_checked();
				}
				missing = required && !any_checked;
			}
			bad_input = (kind == Type::NUMBER || time_kind() != input_time::Kind::none) &&
				!editor.get_value().is_empty() && actual.is_empty();
			if (!actual.is_empty() && (kind == Type::EMAIL || kind == Type::URL)) {
				if (kind == Type::EMAIL && get_attr("multiple")) {
					for (const String& item : actual.split(",", true)) type_mismatch |= !valid_email(item.strip_edges());
				} else type_mismatch = !is_suggestion_valid(actual);
			}
			if (supports_selection() || kind == Type::EMAIL) {
				too_short = editor.has_length_violation(get_attr("minlength"), nullptr);
				too_long = editor.has_length_violation(nullptr, get_attr("maxlength"));
				if (const char* pattern = get_attr("pattern"); pattern && !actual.is_empty()) {
					Ref<RegEx> regex;
					regex.instantiate();
					if (regex->compile("\\A(?:" + String::utf8(pattern) + ")\\z", false) == OK) {
						if (kind == Type::EMAIL && get_attr("multiple")) {
							for (const String& item : actual.split(",", true)) pattern_mismatch |= regex->search(item.strip_edges()).is_null();
						} else pattern_mismatch = regex->search(actual).is_null();
					}
				}
			}
			const double value = value_as_number();
			if (std::isfinite(value)) {
				const auto limits = number_limits(*this);
				underflow = limits.min && value < *limits.min;
				overflow = limits.max && value > *limits.max;
				if (kind == Type::TIME && limits.min && limits.max && *limits.max < *limits.min) {
					underflow = overflow = underflow && overflow;
				}
				if (limits.step) {
					const double steps = (value - limits.base) / *limits.step;
					step_mismatch = !std::isfinite(steps) || std::abs(steps - std::round(steps)) > 1e-7;
				}
			}
		}
		Dictionary result;
		result["will_validate"] = candidate;
		result["value_missing"] = missing;
		result["bad_input"] = bad_input;
		result["type_mismatch"] = type_mismatch;
		result["pattern_mismatch"] = pattern_mismatch;
		result["range_underflow"] = underflow;
		result["range_overflow"] = overflow;
		result["step_mismatch"] = step_mismatch;
		result["too_short"] = too_short;
		result["too_long"] = too_long;
		result["valid"] = !(missing || bad_input || type_mismatch || pattern_mismatch || underflow || overflow ||
			step_mismatch || too_short || too_long);
		return result;
	}

	String HTMLInputNode::sanitize_value(const String& p_value) const {
		const auto kind = type();
		if (time_kind() != input_time::Kind::none) {
			const auto date = input_time::parse(time_kind(), p_value);
			return date ? input_time::format(time_kind(), *date) : String();
		}
		if (kind == Type::COLOR) return sanitize_color(*this, p_value);
		if (kind == Type::FILE) return {};
		if (kind == Type::NUMBER && !html_number(p_value.utf8().get_data(), true)) return {};
		if (kind == Type::RANGE) {
			const auto limits = number_limits(*this);
			const double average = *limits.min / 2 + *limits.max / 2;
			const double value = constrain_number(html_number(p_value.utf8().get_data(), true).value_or(average), limits);
			return String::num_real(value, false);
		}
		if (kind == Type::EMAIL || kind == Type::URL) return p_value.replace("\r", "").replace("\n", "").strip_edges();
		return p_value;
	}

	double HTMLInputNode::value_as_number() const {
		const double missing = std::numeric_limits<double>::quiet_NaN();
		if (time_kind() != input_time::Kind::none) return input_time::parse(time_kind(), get_value()).value_or(missing);
		if (type() != Type::NUMBER && type() != Type::RANGE) return missing;
		return html_number(get_value().utf8().get_data(), true).value_or(missing);
	}

	String HTMLInputNode::format_number(double p_value) const {
		return time_kind() == input_time::Kind::none ? String::num_real(p_value, false) :
			input_time::format(time_kind(), p_value);
	}

	bool HTMLInputNode::set_numeric_from_user(double p_value) {
		if (!std::isfinite(p_value)) return false;
		const auto value = format_number(constrain_number(p_value, number_limits(*this)));
		if (value.is_empty() || value == get_value()) return false;
		editor.set_selection(0, editor.get_value().length());
		return editor.insert(value);
	}

	Error HTMLInputNode::step_number(int64_t p_step, bool& r_changed, std::optional<bool> p_endpoint) {
		r_changed = false;
		if (type() != Type::NUMBER && type() != Type::RANGE && time_kind() == input_time::Kind::none) return ERR_UNAVAILABLE;
		const auto limits = number_limits(*this);
		if (!limits.step) {
			if (type() != Type::RANGE || !p_endpoint) return ERR_UNAVAILABLE;
		}
		if (p_endpoint) {
			const auto limit = *p_endpoint ? limits.max : limits.min;
			r_changed = limit && set_numeric_from_user(*limit);
			return OK;
		}
		if (!p_step || (limits.min && limits.max && *limits.max < *limits.min && type() != Type::TIME)) return OK;
		const auto current = value_as_number();
		const auto step = *limits.step;
		const double steps = ((std::isfinite(current) ? current : 0) - limits.base) / step;
		const double index = p_step < 0 ? std::ceil(steps - 1e-9) : std::floor(steps + 1e-9);
		double target = std::fma(index + p_step, step, limits.base);
		if (type() == Type::TIME) {
			target = std::fmod(target, input_time::day);
			if (target < 0) target += input_time::day;
		}
		if (type() == Type::TIME && limits.min && limits.max && *limits.max < *limits.min) {
			if (target < *limits.min && target > *limits.max) target = p_step > 0 ? *limits.min : *limits.max;
			const String next = format_number(target);
			if (!next.is_empty() && next != get_value()) {
				editor.set_selection(0, editor.get_value().length());
				r_changed = editor.insert(next);
			}
		} else if (!std::isfinite(current) || (p_step > 0 ? !limits.max || current <= *limits.max :
			!limits.min || current >= *limits.min)) {
			r_changed = set_numeric_from_user(target);
		}
		return OK;
	}

	bool HTMLInputNode::set_range_point(const Vector2& p_point) {
		if (!range_box.has_area()) return false;
		const auto limits = number_limits(*this);
		double progress = std::clamp<double>((p_point.x - range_box.position.x) / range_box.size.x, 0, 1);
		if (css().is_right_to_left()) progress = 1 - progress;
		return set_numeric_from_user((1 - progress) * *limits.min + progress * *limits.max);
	}

	void HTMLInputNode::set_attr(const char* p_name, const char* p_value) {
		html_tag::set_attr(p_name, p_value);
		attribute_changed(p_name);
	}

	void HTMLInputNode::attribute_changed(std::string_view p_name) {
		const Type current = type();
		if (p_name == "type" && current != previous_type) {
			const auto old_type = previous_type;
			previous_type = current;
			const auto reflected = [](Type p_type) {
				return p_type == Type::CHECKBOX || p_type == Type::RADIO || p_type == Type::HIDDEN ||
					p_type == Type::BUTTON || p_type == Type::RESET || p_type == Type::SUBMIT || p_type == Type::IMAGE;
			};
			if (!reflected(old_type) && old_type != Type::FILE && reflected(current) && !editor.get_value().is_empty()) {
				html_tag::set_attr("value", editor.get_value().utf8().get_data());
			}
			if (old_type == Type::FILE || current == Type::FILE) files.clear();
			if (reflected(old_type) || reflected(current) || old_type == Type::FILE || current == Type::FILE) {
				editor.reset_value(sanitize_value(String::utf8(get_attr("value", ""))));
			} else if (editor.has_dirty_value()) {
				editor.set_value(sanitize_value(editor.get_value()));
			}
		}
		if (p_name == "value" || p_name == "type" || p_name == "min" || p_name == "max" || p_name == "step" ||
			p_name == "alpha" || p_name == "colorspace") {
			editor.use_default(sanitize_value(String::utf8(get_attr("value", ""))));
		}
		if (current == Type::FILE && p_name == "multiple" && !get_attr("multiple") && files.size() > 1) files.resize(1);
		if ((p_name == "checked" || p_name == "type") && !dirty_checked) checked = get_attr("checked");
		update_pseudos();
	}

	bool HTMLInputNode::update_pseudos() {
		const bool disabled = forms::is_disabled(*this);
		bool changed = set_pseudo_class(litehtml::_id("disabled"), disabled);
		changed |= set_pseudo_class(litehtml::_id("enabled"), !disabled);
		const bool mixed = type() == Type::CHECKBOX && indeterminate;
		changed |= set_pseudo_class(litehtml::_id("checked"), is_toggle() && checked);
		changed |= set_pseudo_class(litehtml::_id("indeterminate"), mixed);
		changed |= set_pseudo_class(litehtml::_id("placeholder-shown"),
			time_kind() == input_time::Kind::none && is_text_control() && editor.get_value().is_empty() && get_attr("placeholder"));
		return changed;
	}

	String HTMLInputNode::get_value() const {
		if (is_toggle()) return String::utf8(get_attr("value", "on"));
		if (type() == Type::FILE) {
			if (files.is_empty()) return {};
			const Ref<HTMLInputFile> file = files[0];
			return "C:\\fakepath\\" + file->get_name();
		}
		if (type() == Type::HIDDEN || is_button()) return String::utf8(get_attr("value", ""));
		return sanitize_value(editor.get_value());
	}

	bool HTMLInputNode::set_checked(bool p_checked) {
		dirty_checked = true;
		bool changed = checked != p_checked;
		checked = p_checked;
		if (changed) note_dom_change(DOM_STATE);
		if (p_checked && type() == Type::RADIO) {
			for (const auto& peer : forms::radio_group(*this)) {
				auto* input = static_cast<HTMLInputNode*>(peer.get());
				if (input != this && input->checked) {
					input->clear_group_checked();
					changed = true;
				}
			}
		}
		return changed;
	}

	void HTMLInputNode::clear_group_checked() {
		if (checked) note_dom_change(DOM_STATE);
		checked = false;
		update_pseudos();
	}

	void HTMLInputNode::reset() {
		const bool old_checked = checked;
		const bool had_files = !files.is_empty();
		editor.reset_value(sanitize_value(String::utf8(get_attr("value", ""))));
		dirty_checked = false;
		checked = get_attr("checked") != nullptr;
		files.clear();
		if (old_checked != checked || had_files) note_dom_change(DOM_STATE);
	}

	bool HTMLInputNode::set_indeterminate(bool p_value) {
		if (indeterminate == p_value) return false;
		indeterminate = p_value;
		note_dom_change(DOM_STATE);
		return true;
	}

	bool HTMLInputNode::set_files(const TypedArray<HTMLInputFile>& p_files) {
		if (files == p_files) return false;
		files = p_files.duplicate();
		note_dom_change(DOM_STATE);
		return true;
	}

	bool HTMLInputNode::copy_state_to(litehtml::element& p_target, HTMLNodeCopyMode p_mode) const {
		auto* target = dynamic_cast<HTMLInputNode*>(&p_target);
		if (!target) {
			return false;
		}
		target->editor.copy_value_state(editor, p_mode == HTMLNodeCopyMode::ADOPT);
		target->checked = checked;
		target->dirty_checked = dirty_checked;
		target->indeterminate = indeterminate;
		if (p_mode == HTMLNodeCopyMode::ADOPT) target->files = get_files().duplicate();
		target->update_pseudos();
		return true;
	}

	void HTMLInputNode::draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
		const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) {
		html_tag::draw(p_hdc, p_x, p_y, p_clip, p_item);
		const auto pos = p_item->pos();
		auto* container = get_document()->container();
		if (type() == Type::IMAGE) {
			auto box = pos;
			box.x += p_x;
			box.y += p_y;
			const char* src = get_attr("src", "");
			image_available = *src && container->is_image_available(src, nullptr, css().get_image_orientation());
			if (image_available) {
				auto layer = litehtml::make_replaced_object_layer(*this, box, *p_item);
				layer.image_orientation = css().get_image_orientation();
				image_box = Rect2(box.x, box.y, box.width, box.height);
				container->draw_image(p_hdc, layer, src, "");
				return;
			}
		}
		paint_contents(*static_cast<LiteHTMLContainer*>(container), Rect2(pos.x + p_x, pos.y + p_y, pos.width, pos.height));
	}

	String HTMLInputNode::button_text() const {
		const auto kind = type();
		if (kind == Type::IMAGE) {
			if (const char* alt = get_attr("alt")) return String::utf8(alt);
			return html_ui_translate("Submit", "input.default_action");
		}
		if (const char* value = get_attr("value")) return String::utf8(value);
		if (kind == Type::RESET) return html_ui_translate("Reset", "input.default_action");
		if (kind == Type::SUBMIT) return html_ui_translate("Submit", "input.default_action");
		return String();
	}

	void HTMLInputNode::parse_attributes() {
		html_tag::parse_attributes();
		if (type() == Type::IMAGE) {
			if (const char* width = get_attr("width")) map_to_dimension_property(litehtml::_width_, width);
			if (const char* height = get_attr("height")) map_to_dimension_property(litehtml::_height_, height);
		} else if (supports_selection() || type() == Type::EMAIL) {
			if (const char* size = get_attr("size")) {
				const String width = String::utf8(size).strip_edges();
				if (width.is_valid_int() && width.to_int() > 0) {
					m_style.add_property(litehtml::_width_, css::normalize(
						(String::num_int64(std::min<int64_t>(width.to_int(), 100000)) + "ch").utf8().get_data()));
				}
			}
		}
	}

	bool HTMLInputNode::is_replaced() const { return type() == Type::IMAGE; }

	void HTMLInputNode::get_content_size(litehtml::size& r_size, litehtml::pixel_t) {
		if (type() != Type::IMAGE) return;
		auto* container = get_document()->container();
		if (const char* src = get_attr("src"); src && *src) {
			container->get_image_size(src, nullptr, r_size, css().get_image_orientation());
			if (r_size.width > 0 && r_size.height > 0) return;
		}
		if (css().get_font()) {
			const CharString text = button_text().utf8();
			r_size = {container->text_width(text.get_data(), css().get_font()), css().line_height().computed_value};
		}
	}

	std::shared_ptr<litehtml::render_item> HTMLInputNode::create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) {
		if (type() == Type::HIDDEN) return {};
		const auto display = css().get_display();
		if (type() == Type::COLOR && (display == litehtml::display_inline_block || display == litehtml::display_block)) {
			auto item = std::make_shared<HTMLControlRenderItem>(shared_from_this(), litehtml::size(get_attr("list") ? 88 : 44, 23),
				HTMLControlRenderItem::Baseline::swatch);
			item->parent(p_parent);
			return item;
		}
		if (type() == Type::IMAGE && display != litehtml::display_none) {
			auto item = std::make_shared<litehtml::render_item_image>(shared_from_this());
			item->parent(p_parent);
			return item;
		}
		if (display == litehtml::display_inline_block || display == litehtml::display_block) {
			litehtml::size natural(169, css().line_height().computed_value);
			if (is_button() && css().get_font()) {
				const CharString text = button_text().utf8();
				natural.width = get_document()->container()->text_width(text.get_data(), css().get_font());
			}
			else if (type() == Type::FILE) natural.width = 19 * css().get_font_size();
			else if (is_toggle()) natural = {11, 11};
			else if (type() == Type::RANGE) natural = {129, 16};
			auto item = std::make_shared<HTMLControlRenderItem>(shared_from_this(), natural);
			item->parent(p_parent);
			return item;
		}
		return html_tag::create_render_item(p_parent);
	}

	void HTMLInputNode::paint_contents(LiteHTMLContainer& p_container, const Rect2& p_box) {
		const auto kind = type();
		decoration_box = {};
		if (kind == Type::HIDDEN || !p_box.has_area()) return;
		const auto color = css().get_color();
		const auto baseline = [&](const Rect2& p_rect, const HTMLTextLayout& p_line) {
			return HTMLControlRenderItem::paint_text_baseline(p_rect.position.y, p_rect.size.y,
				css().get_font_metrics(), p_line.ascent);
		};
		if (is_button()) {
			const auto line = p_container.shape_input(css().get_font(), button_text());
			if (line.is_valid()) p_container.draw_input(p_box, line,
				Vector2(p_box.position.x + std::max<real_t>(0, (p_box.size.x - line.width()) / 2),
					baseline(p_box, line)), color, {}, {}, {});
			return;
		}
		if (kind == Type::RANGE) {
			range_box = p_box;
			if (css().get_appearance() == litehtml::appearance_none) return;
			const auto limits = number_limits(*this);
			const double span = *limits.max - *limits.min;
			double ratio = span > 0 ? (value_as_number() - *limits.min) / span : 0;
			if (!std::isfinite(ratio)) ratio = 0;
			if (css().is_right_to_left()) ratio = 1 - ratio;
			const auto accent = css().get_accent_color().to_srgb(color);
			const Color ink(accent.red / 255.0, accent.green / 255.0, accent.blue / 255.0, accent.alpha / 255.0);
			const real_t thumb = std::min<real_t>(p_box.size.x, std::min<real_t>(p_box.size.y, 14));
			const real_t offset = (p_box.size.x - thumb) * std::clamp(ratio, 0.0, 1.0);
			const Rect2 track(p_box.position.x, p_box.get_center().y - 4, p_box.size.x, 8);
			const auto scheme = css().get_color_scheme();
			p_container.draw_input_chrome(p_box, track, html_system_color(litehtml::system_color::gauge_border, scheme), 4);
			p_container.draw_input_chrome(p_box, track.grow(-1), html_system_color(litehtml::system_color::gauge_track, scheme), 3);
			Rect2 fill = track;
			fill.size.x = css().is_right_to_left() ? track.size.x - offset - thumb / 2 : offset + thumb / 2;
			if (css().is_right_to_left()) fill.position.x = track.get_end().x - fill.size.x;
			p_container.draw_input_chrome(p_box, fill, ink, 4);
			if (span > 0 && get_attr("list")) {
				const Color tick_color = html_system_color(litehtml::system_color::gauge_border, scheme);
				for (const auto& option : forms::datalist_options(*this)) {
					const auto* item = dynamic_cast<const HTMLOptionNode*>(option.get());
					if (!item) continue;
					const auto value = html_number(item->get_value().utf8().get_data(), true);
					if (!value || *value < *limits.min || *value > *limits.max) continue;
					double tick = (*value - *limits.min) / span;
					if (css().is_right_to_left()) tick = 1 - tick;
					const real_t x = p_box.position.x + thumb / 2 + tick * (p_box.size.x - thumb);
					p_container.draw_input_chrome(p_box, Rect2(x, track.get_end().y + 1, 1, 2), tick_color, 0);
				}
			}
			p_container.draw_input_chrome(p_box, Rect2(p_box.position.x + offset, p_box.get_center().y - thumb / 2, thumb, thumb), ink, thumb / 2);
			return;
		}
		if (kind == Type::CHECKBOX || kind == Type::RADIO) {
			if (css().get_appearance() == litehtml::appearance_none) return;
			const bool mixed = kind == Type::CHECKBOX && indeterminate;
			HTMLControlIcon icon;
			if (kind == Type::RADIO) {
				icon = checked ? HTMLControlIcon::radio_checked : HTMLControlIcon::radio_unchecked;
			} else {
				icon = mixed ? HTMLControlIcon::checkbox_indeterminate :
					checked ? HTMLControlIcon::checkbox_checked : HTMLControlIcon::checkbox_unchecked;
			}
			const bool use_accent = (checked || mixed) && !has_author_property(litehtml::_color_) && !forms::is_disabled(*this);
			const auto ink = use_accent ? css().get_accent_color().to_srgb(color) : color.to_srgb();
			const auto edge = std::min(p_box.size.x, p_box.size.y);
			const Vector2 size(edge, edge);
			p_container.draw_input_icon(icon, p_box, Rect2(p_box.get_center() - size * 0.5f, size),
				Color(ink.red / 255.0f, ink.green / 255.0f, ink.blue / 255.0f, ink.alpha / 255.0f));
			return;
		}

		const auto rgba = color.to_srgb();
		const Color ink(rgba.red / 255.0, rgba.green / 255.0, rgba.blue / 255.0, rgba.alpha / 255.0);
		const bool chrome = css().get_appearance() != litehtml::appearance_none;
		const bool enabled = !forms::is_disabled(*this);
		if (kind == Type::COLOR) {
			Rect2 swatch = p_box.grow_individual(-2, -4, -2, -4);
			if (chrome && get_attr("list")) {
				const bool rtl = css().is_right_to_left();
				swatch = p_box.grow_individual(rtl ? -24 : -8, -4, rtl ? -8 : -24, -4);
				p_container.draw_input_icon(HTMLControlIcon::down, p_box, Rect2(rtl ? p_box.position.x + 2 :
					p_box.get_end().x - 18, p_box.get_center().y - 8, 16, 16), ink);
			}
			if (!swatch.has_area()) return;
			const Color fill = preview_color ? *preview_color : color_preview(get_value());
			p_container.draw_input_chrome(p_box, swatch, Color::from_rgba8(119, 119, 119, enabled ? 255 : 120), 0);
			const Rect2 inner = swatch.grow(-1);
			if (!inner.has_area()) return;
			if (fill.a < 1.0f) p_container.draw_input_chrome(p_box, inner,
				html_system_color(litehtml::system_color::button_face, css().get_color_scheme()), 0);
			p_container.draw_input_chrome(p_box, inner, fill, 0);
			return;
		}
		if (kind == Type::FILE) {
			const String button = html_ui_translate(files.size() > 1 || get_attr("multiple") ? "Choose Files" : "Choose File",
				"file_input.button");
			String names;
			for (const Ref<HTMLInputFile>& file : files) {
				if (!names.is_empty()) names += ", ";
				names += file->get_name();
			}
			if (names.is_empty()) names = html_ui_translate("No file chosen", "file_input.status");
			const auto draw_label = [&](const String& p_text, const Rect2& p_rect) {
				const auto line = p_container.shape_input(css().get_font(), p_text);
				if (line.is_valid()) p_container.draw_input(p_rect, line,
					Vector2(p_rect.position.x, baseline(p_rect, line)),
					color, {}, {}, {});
			};
			const auto label = p_container.shape_input(css().get_font(), button);
			const float width = std::min<float>(p_box.size.x, label.width() + 16);
			const Rect2 button_box(p_box.position, Vector2(width, p_box.size.y));
			if (chrome) {
				const auto scheme = css().get_color_scheme();
				const Color face = html_system_color(litehtml::system_color::button_face, scheme);
				const Color border = html_system_color(enabled ? litehtml::system_color::button_border :
					litehtml::system_color::disabled_border, scheme);
				p_container.draw_input_chrome(p_box, button_box, border, 2);
				const Rect2 inner = button_box.grow(-2);
				if (inner.has_area()) p_container.draw_input_chrome(p_box, inner, face);
			}
			draw_label(button, Rect2(p_box.position + Vector2(8, 0), Vector2(width - 16, p_box.size.y)));
			draw_label(names, Rect2(p_box.position + Vector2(width + 6, 0), Vector2(p_box.size.x - width - 6, p_box.size.y)));
			return;
		}
		Rect2 edit_box = p_box;
		const auto time = time_kind();
		String placeholder = String::utf8(get_attr("placeholder", ""));
		if (time != input_time::Kind::none) {
			placeholder = time == input_time::Kind::date ? "yyyy-mm-dd" : time == input_time::Kind::month ? "yyyy-mm" :
				time == input_time::Kind::week ? "yyyy-Www" : time == input_time::Kind::time ? "hh:mm" : "yyyy-mm-dd hh:mm";
		}
		if (chrome && enabled && !get_attr("readonly") && (kind == Type::NUMBER || time != input_time::Kind::none ||
			(kind == Type::SEARCH && !editor.get_value().is_empty()))) {
			const float width = std::min<float>(18, p_box.size.x);
			const bool rtl = css().is_right_to_left();
			decoration_box = Rect2(rtl ? p_box.position.x : p_box.get_end().x - width, p_box.position.y, width, p_box.size.y);
			edit_box.size.x -= width;
			if (rtl) edit_box.position.x += width;
			const auto draw_icon = [&](HTMLControlIcon p_icon, Vector2 p_center, float p_size) {
				const Vector2 size(p_size, p_size);
				p_container.draw_input_icon(p_icon, p_box, Rect2(p_center - size * 0.5, size), ink);
			};
			if (kind == Type::NUMBER) {
				draw_icon(HTMLControlIcon::up, decoration_box.get_center() - Vector2(0, decoration_box.size.y / 4), 12);
				draw_icon(HTMLControlIcon::down, decoration_box.get_center() + Vector2(0, decoration_box.size.y / 4), 12);
			} else if (kind == Type::SEARCH) {
				draw_icon(HTMLControlIcon::close_small, decoration_box.get_center(), 14);
			} else {
				draw_icon(time == input_time::Kind::time ? HTMLControlIcon::clock : HTMLControlIcon::calendar,
					decoration_box.get_center(), 14);
			}
		}
		if (edit_box.has_area()) editor.paint_single(p_container, edit_box, *this, placeholder,
			focused && !preview_time, is_password(), preview_time ? &*preview_time : nullptr);
	}

	int HTMLInputNode::decoration_at(const Vector2& p_point) const {
		if (!decoration_box.has_point(p_point)) return 0;
		return type() == Type::NUMBER && p_point.y >= decoration_box.get_center().y ? -1 : 1;
	}

	Vector2i HTMLInputNode::image_coordinates(const Vector2& p_point) const {
		return image_available ? Vector2i((p_point - image_box.position).floor()) : Vector2i();
	}

	bool HTMLInputNode::edit_time_segment(int p_direction, bool p_step) {
		const auto kind = time_kind();
		const auto current = value_as_number();
		if (kind == input_time::Kind::none || !std::isfinite(current)) return false;
		const auto segments = [](const String& p_value) {
			std::vector<Vector2i> result;
			for (int begin = 0; begin < p_value.length(); ++begin) {
				if (p_value[begin] < '0' || p_value[begin] > '9') continue;
				int end = begin + 1;
				while (end < p_value.length() && p_value[end] >= '0' && p_value[end] <= '9') ++end;
				result.emplace_back(begin, end);
				begin = end - 1;
			}
			return result;
		};
		const auto fields = segments(editor.get_value());
		if (fields.empty()) return false;
		int index = 0;
		const auto position = editor.selection_start();
		while (index + 1 < fields.size() && fields[index].y < position) ++index;
		if (!p_step) {
			index = std::clamp(index + p_direction, 0, int(fields.size()) - 1);
			editor.set_selection(fields[index].x, fields[index].y);
			return true;
		}
		using Kind = input_time::Kind;
		auto parts = input_time::parts(kind, current);
		double target = current;
		if (kind == Kind::week) {
			if (index == 1) target += p_direction * input_time::day * 7;
			else {
				const String week = input_time::format(kind, current);
				const int64_t year = week.get_slice("-W", 0).to_int() + p_direction;
				const auto next = input_time::parse(kind, vformat("%04d-W%02d", year, week.get_slice("-W", 1).to_int()));
				const auto fallback = input_time::parse(kind, vformat("%04d-W52", year));
				if (!fallback) return false;
				target = next.value_or(*fallback);
			}
		} else if ((kind == Kind::date || kind == Kind::local) && index == 2) target += p_direction * input_time::day;
		else if (kind == Kind::time || (kind == Kind::local && index >= 3)) {
			constexpr double units[] = { 3600000, 60000, 1000, 1 };
			target += p_direction * units[std::clamp(index - (kind == Kind::local ? 3 : 0), 0, 3)];
			if (kind == Kind::time) target = std::fmod(target + input_time::day, input_time::day);
		} else {
			const int64_t year = parts["year"], month = parts["month"];
			const auto next_month = (year - 1) * 12 + month - 1 + p_direction * (index == 0 ? 12 : 1);
			if (next_month < 0 || next_month >= 275760 * 12LL) return false;
			parts["year"] = next_month / 12 + 1;
			parts["month"] = next_month % 12 + 1;
			std::optional<double> next;
			for (int day = int(parts["day"]); day >= 1 && !next; --day) {
				parts["day"] = day;
				next = input_time::from_parts(kind, parts);
			}
			if (!next) return false;
			target = *next;
		}
		const String value = input_time::format(kind, target);
		if (value.is_empty() || value == get_value()) return false;
		editor.set_selection(0, editor.get_value().length());
		if (!editor.insert(value)) return false;
		const auto updated = segments(value);
		index = std::min(index, int(updated.size()) - 1);
		editor.set_selection(updated[index].x, updated[index].y);
		return true;
	}
}
