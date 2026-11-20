#include "css_animated_property.h"

#include "css_computed_style.h"
#include "css_math.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dockit::css {
	using namespace litehtml;

	namespace {
		using Kind = css_transform_function::kind;

		template <auto Getter>
		AnimatedValue read_computed(const ComputedStyle& css) {
			return (css.*Getter)();
		}

		bool equal_length(const css_length& a, const css_length& b) {
			if (a.is_predefined() != b.is_predefined() || a.intrinsic_size() != b.intrinsic_size()) return false;
			if (a.is_predefined()) return a.predef() == b.predef();
			if (a.expression() || b.expression()) return a.expression() == b.expression() || a.to_string() == b.to_string();
			return (a.val() == 0 && b.val() == 0) || (a.units() == b.units() && a.val() == b.val());
		}

		bool same_length_type(const AnimatedProperty& property, const css_length& a, const css_length& b) {
			return !property.distinguish_number || (a.units() == css_units_none) == (b.units() == css_units_none);
		}

		bool same_indent_flags(const css_text_indent& a, const css_text_indent& b) {
			return a.hanging == b.hanging && a.each_line == b.each_line;
		}

		bool length_is_numeric(const css_length& value) {
			return !value.is_predefined() && value.intrinsic_size() == css_intrinsic_size::none;
		}

		bool equal_function(const css_transform_function& a, const css_transform_function& b) {
			return a.type == b.type && equal_length(a.x, b.x) && equal_length(a.y, b.y) && equal_length(a.z, b.z) &&
				a.first == b.first && a.second == b.second && a.third == b.third && a.axis == b.axis &&
				a.matrix == b.matrix && a.blend == b.blend;
		}

		css_transform_function identity(const css_transform_function& model) {
			css_transform_function result{model.type};
			result.axis = model.axis;
			if (model.type == Kind::scale) result.first = result.second = 1;
			if (model.type == Kind::perspective) result.z = css_length::predef_value();
			if (model.type == Kind::matrix) result.matrix[0] = result.matrix[3] = 1;
			if (model.type == Kind::matrix3d) result.matrix[0] = result.matrix[5] = result.matrix[10] = result.matrix[15] = 1;
			return result;
		}

		css_transform_function blend_matrices(css_transform_list from, css_transform_list to, double progress,
				bool allow_discrete) {
			css_transform_function result{Kind::blend};
			result.blend = std::make_shared<css_transform_blend>(
				css_transform_blend{std::move(from), std::move(to), progress, allow_discrete});
			return result;
		}

		css_transform_list interpolate_transforms(const css_transform_list& a, const css_transform_list& b,
				double progress, bool allow_discrete) {
			css_transform_list result;
			result.reserve(std::max(a.size(), b.size()));
			const auto lerp = [progress](double from, double to) { return from + (to - from) * progress; };
			for (std::size_t index = 0; index < std::max(a.size(), b.size()); ++index) {
				if ((index < a.size() && a[index].type == Kind::blend) ||
						(index < b.size() && b[index].type == Kind::blend) ||
						(index < a.size() && index < b.size() && a[index].type != b[index].type)) {
					result.push_back(blend_matrices(
						{a.begin() + std::min(index, a.size()), a.end()},
						{b.begin() + std::min(index, b.size()), b.end()}, progress, allow_discrete));
					break;
				}
				auto first = index < a.size() ? a[index] : identity(b[index]);
				const auto second = index < b.size() ? b[index] : identity(a[index]);
				if (equal_function(first, second)) {
					result.push_back(std::move(first));
					continue;
				}
				if (first.type == Kind::matrix || first.type == Kind::matrix3d || first.type == Kind::perspective ||
						(first.type == Kind::rotate && first.axis != second.axis && first.first != 0 && second.first != 0)) {
					result.push_back(blend_matrices({std::move(first)}, {second}, progress, allow_discrete));
					continue;
				}
				if (first.type == Kind::translate) {
					first.x = interpolate_css_length(first.x, second.x, progress);
					first.y = interpolate_css_length(first.y, second.y, progress);
					first.z = interpolate_css_length(first.z, second.z, progress);
				} else {
					if (first.type == Kind::rotate && first.first == 0) first.axis = second.axis;
					first.first = lerp(first.first, second.first);
					first.second = lerp(first.second, second.second);
					first.third = lerp(first.third, second.third);
				}
				result.push_back(std::move(first));
			}
			return result;
		}

	}

	const std::vector<AnimatedProperty>& animated_properties() {
		using Range = AnimatedRange;
		static const std::vector<AnimatedProperty> properties{
			{_id("opacity"), [](const ComputedStyle& css) -> AnimatedValue { return static_cast<float>(css.get_opacity()); }, Range::unit_interval},
			{_color_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_color(); }},
			{_background_color_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_bg().m_color.resolve(css.get_color()); }},
			{_border_top_color_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().top.color.resolve(css.get_color()); }},
			{_border_right_color_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().right.color.resolve(css.get_color()); }},
			{_border_bottom_color_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().bottom.color.resolve(css.get_color()); }},
			{_border_left_color_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().left.color.resolve(css.get_color()); }},
			{_width_, read_computed<&ComputedStyle::get_width>},
			{_height_, read_computed<&ComputedStyle::get_height>},
			{_min_width_, read_computed<&ComputedStyle::get_min_width>},
			{_min_height_, read_computed<&ComputedStyle::get_min_height>},
			{_max_width_, read_computed<&ComputedStyle::get_max_width>},
			{_max_height_, read_computed<&ComputedStyle::get_max_height>},
			{_row_gap_, read_computed<&ComputedStyle::get_row_gap>},
			{_column_gap_, read_computed<&ComputedStyle::get_column_gap>},
			{_flex_basis_, read_computed<&ComputedStyle::get_flex_basis>},
			{_flex_grow_, read_computed<&ComputedStyle::get_flex_grow>},
			{_flex_shrink_, read_computed<&ComputedStyle::get_flex_shrink>},
			{__litehtml_border_spacing_x_, read_computed<&ComputedStyle::get_border_spacing_x>},
			{__litehtml_border_spacing_y_, read_computed<&ComputedStyle::get_border_spacing_y>},
			{_margin_top_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_margins().top; }, Range::unrestricted},
			{_margin_right_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_margins().right; }, Range::unrestricted},
			{_margin_bottom_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_margins().bottom; }, Range::unrestricted},
			{_margin_left_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_margins().left; }, Range::unrestricted},
			{_padding_top_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_padding().top; }},
			{_padding_right_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_padding().right; }},
			{_padding_bottom_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_padding().bottom; }},
			{_padding_left_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_padding().left; }},
			{_top_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_offsets().top; }, Range::unrestricted},
			{_right_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_offsets().right; }, Range::unrestricted},
			{_bottom_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_offsets().bottom; }, Range::unrestricted},
			{_left_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_offsets().left; }, Range::unrestricted},
			{_border_top_width_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().top.width; }},
			{_border_right_width_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().right.width; }},
			{_border_bottom_width_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().bottom.width; }},
			{_border_left_width_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().left.width; }},
			{_border_top_left_radius_x_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().radius.top_left_x; }},
			{_border_top_left_radius_y_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().radius.top_left_y; }},
			{_border_top_right_radius_x_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().radius.top_right_x; }},
			{_border_top_right_radius_y_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().radius.top_right_y; }},
			{_border_bottom_right_radius_x_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().radius.bottom_right_x; }},
			{_border_bottom_right_radius_y_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().radius.bottom_right_y; }},
			{_border_bottom_left_radius_x_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().radius.bottom_left_x; }},
			{_border_bottom_left_radius_y_, [](const ComputedStyle& css) -> AnimatedValue { return css.get_borders().radius.bottom_left_y; }},
			{_id("scroll-padding-top"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_scroll_padding()[0]; }},
			{_id("scroll-padding-right"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_scroll_padding()[1]; }},
			{_id("scroll-padding-bottom"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_scroll_padding()[2]; }},
			{_id("scroll-padding-left"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_scroll_padding()[3]; }},
			{_id("scroll-margin-top"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_scroll_margin()[0]; }, Range::unrestricted},
			{_id("scroll-margin-right"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_scroll_margin()[1]; }, Range::unrestricted},
			{_id("scroll-margin-bottom"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_scroll_margin()[2]; }, Range::unrestricted},
			{_id("scroll-margin-left"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_scroll_margin()[3]; }, Range::unrestricted},
			{_id("outline-width"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_outline().width; }},
			{_id("outline-offset"), [](const ComputedStyle& css) -> AnimatedValue { return css.get_outline().offset; }, Range::unrestricted},
			{_font_size_, [](const ComputedStyle& css) -> AnimatedValue { return css_length(css.get_font_size()); }},
			{_id("font-size-adjust"), read_computed<&ComputedStyle::get_font_size_adjust>},
			{_font_weight_, [](const ComputedStyle& css) -> AnimatedValue {
				return css_length(css.get_font_weight(), css_units_none);
			}, Range::font_weight},
			{_id("font-width"), read_computed<&ComputedStyle::get_font_width>},
			{_line_height_, [](const ComputedStyle& css) -> AnimatedValue {
				return css.line_height().css_value;
			}, Range::non_negative, true},
			{_id("letter-spacing"), read_computed<&ComputedStyle::get_letter_spacing>, Range::unrestricted},
			{_id("word-spacing"), read_computed<&ComputedStyle::get_word_spacing>, Range::unrestricted},
			{_text_indent_, read_computed<&ComputedStyle::get_text_indent>, Range::unrestricted},
			{_id("tab-size"), read_computed<&ComputedStyle::get_tab_size>, Range::non_negative, true},
			{_text_decoration_thickness_, read_computed<&ComputedStyle::get_text_decoration_thickness>},
			{_id("text-underline-offset"), read_computed<&ComputedStyle::get_text_underline_offset>, Range::unrestricted},
			{_text_decoration_color_, [](const ComputedStyle& css) -> AnimatedValue {
				return css.get_text_decoration_color().resolve(css.get_color());
			}},
			{_text_emphasis_color_, [](const ComputedStyle& css) -> AnimatedValue {
				return css.get_text_emphasis_color().resolve(css.get_color());
			}},
			{_transform_, read_computed<&ComputedStyle::get_transforms>},
		};
		return properties;
	}

	AnimatedValues read_animated_values(const ComputedStyle& css) {
		const auto& properties = animated_properties();
		AnimatedValues result;
		result.reserve(properties.size());
		for (const auto& property : properties) result.push_back(property.read(css));
		return result;
	}

	bool equal_animated_value(const AnimatedProperty& property, const AnimatedValue& a, const AnimatedValue& b) {
		if (a.index() != b.index()) return false;
		if (const auto* value = std::get_if<float>(&a)) return *value == std::get<float>(b);
		if (const auto* value = std::get_if<css_font_size_adjust>(&a)) return *value == std::get<css_font_size_adjust>(b);
		if (const auto* value = std::get_if<web_color>(&a)) return equal_css_colors(*value, std::get<web_color>(b));
		if (const auto* value = std::get_if<css_length>(&a)) {
			const auto& other = std::get<css_length>(b);
			return equal_length(*value, other) && (value->is_predefined() || same_length_type(property, *value, other));
		}
		if (const auto* value = std::get_if<css_text_indent>(&a)) {
			const auto& other = std::get<css_text_indent>(b);
			return same_indent_flags(*value, other) && equal_length(value->length, other.length);
		}
		const auto& first = std::get<css_transform_list>(a);
		const auto& second = std::get<css_transform_list>(b);
		return first.size() == second.size() && std::equal(first.begin(), first.end(), second.begin(), equal_function);
	}

	bool can_interpolate_animated_value(const AnimatedProperty& property, const AnimatedValue& a, const AnimatedValue& b) {
		if (a.index() != b.index()) return false;
		if (const auto* from = std::get_if<css_font_size_adjust>(&a)) {
			const auto& to = std::get<css_font_size_adjust>(b);
			return from->mode == css_font_size_adjust::Mode::number && to.mode == from->mode && from->metric == to.metric;
		}
		if (const auto* from = std::get_if<css_length>(&a)) {
			const auto& to = std::get<css_length>(b);
			return length_is_numeric(*from) && length_is_numeric(to) && same_length_type(property, *from, to);
		}
		if (const auto* from = std::get_if<css_text_indent>(&a)) return same_indent_flags(*from, std::get<css_text_indent>(b));
		return true;
	}

	AnimatedValue interpolate_animated_value(const AnimatedProperty& property, const AnimatedValue& a,
			const AnimatedValue& b, double progress, bool allow_discrete) {
		if (progress == 0) return a;
		if (progress == 1) return b;
		if (const auto* from = std::get_if<css_font_size_adjust>(&a)) {
			const auto& to = std::get<css_font_size_adjust>(b);
			if (from->mode != css_font_size_adjust::Mode::number || to.mode != from->mode || from->metric != to.metric) {
				return allow_discrete && progress >= 0.5 ? b : a;
			}
			auto result = *from;
			const double sample = from->value + (to.value - from->value) * progress;
			result.value = std::clamp(sample, 0.0, std::numeric_limits<double>::max());
			return result;
		}
		if (const auto* value = std::get_if<float>(&a)) {
			const double sample = *value + (double(std::get<float>(b)) - *value) * progress;
			const double upper = property.range == AnimatedRange::unit_interval ? 1.0 : std::numeric_limits<float>::max();
			return static_cast<float>(std::clamp(sample, 0.0, upper));
		}
		if (const auto* value = std::get_if<web_color>(&a)) return interpolate_css_color(*value, std::get<web_color>(b), progress);
		if (const auto* value = std::get_if<css_text_indent>(&a)) {
			auto result = *value;
			result.length = interpolate_css_length(value->length, std::get<css_text_indent>(b).length, progress, false);
			return result;
		}
		if (property.range == AnimatedRange::font_weight) {
			const double from = std::get<css_length>(a).val();
			const double to = std::get<css_length>(b).val();
			return css_length(static_cast<float>(std::clamp(from + (to - from) * progress, 1.0, 1000.0)), css_units_none);
		}
		if (const auto* value = std::get_if<css_length>(&a)) return interpolate_css_length(*value, std::get<css_length>(b),
			progress, property.range != AnimatedRange::unrestricted);
		return interpolate_transforms(std::get<css_transform_list>(a), std::get<css_transform_list>(b), progress, allow_discrete);
	}
}
