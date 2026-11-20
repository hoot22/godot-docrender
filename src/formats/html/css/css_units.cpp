#include "css_units.h"
#include "css_math.h"
#include "css_syntax_util.h"

namespace dockit::css {

using detail::is_one_of;

pixel_t length_to_pixels(const css_length& val, const font_metrics& metrics, pixel_t size,
	const UnitContext& context)
{
	if (val.is_predefined())
		return 0;
	if (const auto& expression = val.expression())
		return static_cast<pixel_t>(compute_css_math(*expression, [&](const css_length& length) {
			return length_to_pixels(length, metrics, 0, context);
		})->evaluate(size));

	const auto& viewport = context.viewport;
	const auto& root = context.root_font;
	const float amount = val.val();
	switch (val.units())
	{
	case css_units_percentage: return val.calc_percent(size);
	case css_units_em: return amount * metrics.font_size;
	case css_units_ex: return amount * metrics.x_height;
	case css_units_cap: return amount * metrics.cap_height;
	case css_units_ch: return amount * metrics.ch_width;
	case css_units_ic: return amount * metrics.ic_width;
	case css_units_lh: return amount * metrics.line_height;
	case css_units_rem: return amount * root.font_size;
	case css_units_rex: return amount * root.x_height;
	case css_units_rcap: return amount * root.cap_height;
	case css_units_rch: return amount * root.ch_width;
	case css_units_ric: return amount * root.ic_width;
	case css_units_rlh: return amount * root.line_height;

	// https://drafts.csswg.org/css-values-4/#absolute-lengths
	case css_units_pt: return context.points_to_pixels(context.host, amount);
	case css_units_pc: return context.points_to_pixels(context.host, amount * 12);
	case css_units_in: return context.points_to_pixels(context.host, amount * 72);
	case css_units_cm: return context.points_to_pixels(context.host, amount * 72 / 2.54f);
	case css_units_mm: return context.points_to_pixels(context.host, amount * 72 / 25.4f);
	case css_units_q: return context.points_to_pixels(context.host, amount * 72 / 101.6f);

	// The embedded viewport has one size and uses horizontal writing mode.
	case css_units_vw: case css_units_vi:
	case css_units_svw: case css_units_svi:
	case css_units_lvw: case css_units_lvi:
	case css_units_dvw: case css_units_dvi:
		return static_cast<pixel_t>(viewport.width * (amount / 100.0));
	case css_units_vh: case css_units_vb:
	case css_units_svh: case css_units_svb:
	case css_units_lvh: case css_units_lvb:
	case css_units_dvh: case css_units_dvb:
		return static_cast<pixel_t>(viewport.height * (amount / 100.0));
	case css_units_vmin: case css_units_svmin: case css_units_lvmin: case css_units_dvmin:
		return static_cast<pixel_t>(std::min(viewport.width, viewport.height) * (amount / 100.0));
	case css_units_vmax: case css_units_svmax: case css_units_lvmax: case css_units_dvmax:
		return static_cast<pixel_t>(std::max(viewport.width, viewport.height) * (amount / 100.0));

	case css_units_cqw: case css_units_cqi:
	case css_units_cqh: case css_units_cqb:
	case css_units_cqmin: case css_units_cqmax: {
		const auto dimension = [&](bool block_axis) {
			const auto& resolver = context.container_units;
			const auto axis = resolver.read_axis ? resolver.read_axis(resolver.subject, block_axis) : std::nullopt;
			return axis.value_or(block_axis ? viewport.height : viewport.width);
		};
		const auto units = val.units();
		pixel_t basis;
		if (is_one_of(units, css_units_cqw, css_units_cqi)) basis = dimension(false);
		else if (is_one_of(units, css_units_cqh, css_units_cqb)) basis = dimension(true);
		else {
			const auto inline_size = dimension(false);
			const auto block_size = dimension(true);
			basis = units == css_units_cqmin ? std::min(inline_size, block_size) : std::max(inline_size, block_size);
		}
		return static_cast<pixel_t>(basis * (amount / 100.0));
	}
	default: return amount;
	}
}

void compute_length(css_length& val, const font_metrics& metrics, pixel_t size, const UnitContext& context)
{
	const auto intrinsic = val.intrinsic_size();
	if (is_one_of(intrinsic, css_intrinsic_size::fit_content_limit, css_intrinsic_size::stretch_limit))
	{
		auto limit = val.intrinsic_size_limit();
		compute_length(limit, metrics, size, context);
		limit.set_intrinsic_size(intrinsic);
		val = std::move(limit);
		return;
	}
	if (const auto& expression = val.expression())
	{
		auto computed = compute_css_math(*expression, [&](const css_length& length) {
			return length_to_pixels(length, metrics, 0, context);
		});
		if (computed->has_percentage)
			val.set_expression(std::move(computed));
		else
			val.set_value(static_cast<float>(computed->evaluate(0)), css_units_px);
		return;
	}
	if (!val.is_predefined() && val.units() != css_units_percentage)
		val.set_value(length_to_pixels(val, metrics, size, context), css_units_px);
}

}
