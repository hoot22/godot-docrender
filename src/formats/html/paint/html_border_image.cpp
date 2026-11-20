#include "html_border_image.h"

#include <algorithm>
#include <cmath>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		using Side = css_border_image_offsets::Side;

		double resolve_offset(const Side& p_side, double p_border, double p_size, double p_auto = 0) {
			if (const auto* factor = std::get_if<double>(&p_side)) return std::max(0.0, *factor * p_border);
			const auto& length = std::get<css_length>(p_side);
			return length.is_predefined() ? p_auto : std::max(0.0, static_cast<double>(length.calc_percent(p_size)));
		}

		struct Axis {
			double phase = 0;
			double tile = 0;
			double period = 0;
		};

		Axis tile_axis(border_image_tiling p_mode, double p_available, double p_tile) {
			if (p_mode == border_image_tiling::stretch) return { 0, p_available, p_available };
			if (p_tile <= 0 || !std::isfinite(p_tile)) return {};
			if (p_mode == border_image_tiling::repeat)
				return { std::fmod((p_available - p_tile) / 2, p_tile), p_tile, p_tile };
			if (p_mode == border_image_tiling::round) {
				const double size = p_available / std::max(1.0, std::round(p_available / p_tile));
				return { 0, size, size };
			}
			const double count = std::floor(p_available / p_tile);
			if (count < 1) return {};
			const double gap = (p_available - count * p_tile) / (count + 1);
			return { gap, p_tile, p_tile + gap };
		}

		double scale_slice(double p_destination, double p_source, double p_fallback_destination = 1,
			double p_fallback_source = 1) {
			if (p_destination > 0 && p_source > 0) return p_destination / p_source;
			if (p_fallback_destination > 0 && p_fallback_source > 0) return p_fallback_destination / p_fallback_source;
			return 1;
		}

		std::array<double, 4> border_widths(const borders& p_borders) {
			return { p_borders.top.width, p_borders.right.width, p_borders.bottom.width, p_borders.left.width };
		}
	}

	Rect2 get_border_image_area(const css_border_image& p_style, const Rect2& p_box, const borders& p_borders) {
		const auto widths = border_widths(p_borders);
		const double top = resolve_offset(p_style.outset.sides[0], widths[0], 0);
		const double right = resolve_offset(p_style.outset.sides[1], widths[1], 0);
		const double bottom = resolve_offset(p_style.outset.sides[2], widths[2], 0);
		const double left = resolve_offset(p_style.outset.sides[3], widths[3], 0);
		return Rect2(p_box.position - Vector2(left, top), p_box.size + Vector2(left + right, top + bottom));
	}

	std::vector<HTMLBorderImagePatch> make_border_image_patches(const css_border_image& p_style,
		const borders& p_borders, const Rect2& p_area, const Vector2& p_source_size, const Vector2& p_natural_size,
		const Vector2& p_texture_size, bool p_flip_x) {
		std::vector<HTMLBorderImagePatch> patches;
		const auto border = border_widths(p_borders);
		std::array<double, 4> slices;
		std::array<double, 4> widths;
		for (int side = 0; side < 4; ++side) {
			const int axis = side % 2 == 0 ? 1 : 0;
			const auto& slice = p_style.slice.sides[side];
			const double source = p_source_size[axis];
			const double amount = slice.percentage ? source * (slice.value / 100.0) : slice.value;
			slices[side] = std::min(source, amount);
			const double automatic = p_natural_size[axis] > 0 ? p_natural_size[axis] * slices[side] / source : border[side];
			widths[side] = resolve_offset(p_style.width.sides[side], border[side], p_area.size[axis], automatic);
		}
		const double horizontal = widths[3] + widths[1];
		const double vertical = widths[0] + widths[2];
		const double factor = std::min({ 1.0, horizontal > 0 ? p_area.size.x / horizontal : 1.0,
			vertical > 0 ? p_area.size.y / vertical : 1.0 });
		for (double& width : widths) width *= factor;

		const std::array<double, 4> source_x{ 0, slices[3], p_source_size.x - slices[1], p_source_size.x };
		const std::array<double, 4> source_y{ 0, slices[0], p_source_size.y - slices[2], p_source_size.y };
		const std::array<double, 4> target_x{ 0, widths[3], p_area.size.x - widths[1], p_area.size.x };
		const std::array<double, 4> target_y{ 0, widths[0], p_area.size.y - widths[2], p_area.size.y };
		const Vector2 texture_scale = p_texture_size / p_source_size;
		for (int y = 0; y < 3; ++y) {
			for (int x = 0; x < 3; ++x) {
				if (x == 1 && y == 1 && !p_style.slice.fill) continue;
				const Vector2 size(target_x[x + 1] - target_x[x], target_y[y + 1] - target_y[y]);
				const Vector2 source_size(source_x[x + 1] - source_x[x], source_y[y + 1] - source_y[y]);
				if (size.x <= 0 || size.y <= 0 || source_size.x <= 0 || source_size.y <= 0) continue;
				const Vector2 position = p_area.position + Vector2(target_x[x], target_y[y]);
				const double scale_x = y == 1 ? scale_slice(widths[0], slices[0], widths[2], slices[2]) : size.y / source_size.y;
				const double scale_y = x == 1 ? scale_slice(widths[3], slices[3], widths[1], slices[1]) : size.x / source_size.x;
				const auto a = tile_axis(x == 1 ? p_style.repeat.horizontal : border_image_tiling::stretch, size.x, source_size.x * scale_x);
				const auto b = tile_axis(y == 1 ? p_style.repeat.vertical : border_image_tiling::stretch, size.y, source_size.y * scale_y);
				if (a.tile <= 0 || b.tile <= 0) continue;
				const double source_left = p_flip_x ? p_source_size.x - source_x[x + 1] : source_x[x];
				patches.push_back({
					Rect2(Vector2(source_left, source_y[y]) * texture_scale, source_size * texture_scale),
					Rect2(position, size), position + Vector2(a.phase, b.phase), Vector2(a.tile, b.tile), Vector2(a.period, b.period),
				});
			}
		}
		return patches;
	}
}
