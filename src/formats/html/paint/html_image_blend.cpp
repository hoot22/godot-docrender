#include "html_image_blend.h"

#include "formats/html/css/css_image_value.h"
#include "formats/html/resources/html_resources.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace godot;

namespace dockit {
	namespace {
		constexpr int MAX_TILE_SIDE = 2048;
		const Color TRANSPARENT(0, 0, 0, 0);

		Color premultiplied(const litehtml::web_color& p_color) {
			const float alpha = p_color.alpha / 255.0f;
			return Color(p_color.red * alpha / 255.0f, p_color.green * alpha / 255.0f, p_color.blue * alpha / 255.0f, alpha);
		}

		struct AxisSample {
			int left;
			int right;
			float fraction;
		};

		std::vector<AxisSample> raster_axis(int p_source, int p_target, float p_css_extent,
			litehtml::image_rendering p_mode, bool p_flip = false) {
			using litehtml::image_rendering;
			const bool nearest = p_mode == image_rendering::crisp_edges || p_mode == image_rendering::optimize_speed;
			const bool pixelated = p_mode == image_rendering::pixelated;
			const double copies = std::max(1.0, std::round(static_cast<double>(p_css_extent) / p_source));
			std::vector<AxisSample> samples;
			samples.reserve(p_target);
			for (int index = 0; index < p_target; ++index) {
				const int step = p_flip ? p_target - 1 - index : index;
				const double point = (step + 0.5) * p_source / p_target;
				if (nearest) {
					const int pixel = std::clamp(static_cast<int>(point), 0, p_source - 1);
					samples.push_back({ pixel, pixel, 0.0f });
					continue;
				}
				const double fractional = pixelated ? point * copies - 0.5 : point - 0.5;
				const double first = std::floor(fractional);
				const double left = pixelated ? std::floor(first / copies) : first;
				const double right = pixelated ? std::floor((first + 1.0) / copies) : first + 1.0;
				samples.push_back({ std::clamp(static_cast<int>(left), 0, p_source - 1),
					std::clamp(static_cast<int>(right), 0, p_source - 1), static_cast<float>(fractional - first) });
			}
			return samples;
		}

		struct GradientStop {
			float offset;
			float hint;
			Color color;
		};

		std::vector<GradientStop> make_stops(const litehtml::background_layer::gradient_base& p_gradient) {
			std::vector<GradientStop> stops;
			for (const auto& point : p_gradient.color_points) {
				if (!std::isfinite(point.offset)) return {};
				const float offset = stops.empty() ? point.offset : std::max(point.offset, stops.back().offset);
				stops.push_back({ offset, point.hint.value_or(0.0f), premultiplied(point.color) });
			}
			return stops;
		}

		Color sample_gradient(const std::vector<GradientStop>& p_stops, float p_position) {
			const auto right = std::upper_bound(p_stops.begin(), p_stops.end(), p_position,
				[](float p_value, const GradientStop& p_stop) { return p_value < p_stop.offset; });
			if (right == p_stops.begin()) return right->color;
			const auto left = right - 1;
			if (right == p_stops.end()) return left->color;
			const float span = right->offset - left->offset;
			float fraction = span > 0 ? std::clamp((p_position - left->offset) / span, 0.0f, 1.0f) : 0.0f;
			if (left->hint > 0.0f && left->hint < 1.0f) fraction = std::pow(fraction, std::log(0.5f) / std::log(left->hint));
			return left->color.lerp(right->color, fraction);
		}

		uint8_t byte_channel(float p_value) {
			return static_cast<uint8_t>(std::lround(std::clamp(p_value, 0.0f, 1.0f) * 255.0f));
		}
	}

	Ref<Texture2D> rasterize_cross_fade(const litehtml::background_layer& p_tile, const litehtml::css_cross_fade& p_mix,
		const litehtml::string& p_base, const litehtml::web_color& p_current, int p_direction,
		litehtml::image_rendering p_sampling, HTMLResources& p_resources) {
		using litehtml::image;
		if (p_mix.layers.empty()) return {};
		const Vector2 extent(p_tile.origin_box.width, p_tile.origin_box.height);
		if (!extent.is_finite() || extent.x <= 0 || extent.y <= 0) return {};
		const bool uniform = std::all_of(p_mix.layers.begin(), p_mix.layers.end(), [](const auto& p_layer) {
			return p_layer.value.type == image::type_solid || p_layer.value.type == image::type_none;
		});
		const double scale = std::min(1.0, static_cast<double>(MAX_TILE_SIDE) / std::max(extent.x, extent.y));
		const int width = uniform ? 1 : std::max(1, static_cast<int>(std::ceil(extent.x * scale)));
		const int height = uniform ? 1 : std::max(1, static_cast<int>(std::ceil(extent.y * scale)));
		const size_t count = static_cast<size_t>(width) * height;
		std::vector<Color> mixed(count, TRANSPARENT);
		const auto accumulate = [&](float p_weight, const auto& p_sample) {
			for (int y = 0; y < height; ++y)
				for (int x = 0; x < width; ++x)
					mixed[static_cast<size_t>(y) * width + x] += p_sample(x, y) * p_weight;
		};
		for (const auto& layer : p_mix.layers) {
			const auto& value = layer.value;
			const float weight = static_cast<float>(layer.weight);
			if (value.type == image::type_solid) {
				const Color color = premultiplied(value.fallback_color->to_srgb(p_current)) * weight;
				for (auto& pixel : mixed) pixel += color;
			}
			else if (value.type == image::type_url) {
				const auto texture = p_resources.load_texture(value.url.c_str(), p_base.c_str(), value.orientation);
				if (texture.is_null() || value.density <= 0 || !std::isfinite(value.density)) continue;
				const auto pixels = texture->get_image();
				if (pixels.is_null() || pixels->is_empty() || (pixels->is_compressed() && pixels->decompress() != OK)) continue;
				pixels->convert(Image::FORMAT_RGBA8);
				const auto data = pixels->get_data();
				const uint8_t* bytes = data.ptr();
				const auto pixel_size = pixels->get_size();
				const bool flip = value.direction && static_cast<int>(*value.direction) != p_direction;
				const auto horizontal = raster_axis(pixel_size.x, width, extent.x, p_sampling, flip);
				const auto vertical = raster_axis(pixel_size.y, height, extent.y, p_sampling);
				const auto read = [&](int p_x, int p_y) {
					const uint8_t* pixel = bytes + (static_cast<size_t>(p_y) * pixel_size.x + p_x) * 4;
					const float alpha = pixel[3] / 255.0f;
					return Color(pixel[0] * alpha / 255.0f, pixel[1] * alpha / 255.0f, pixel[2] * alpha / 255.0f, alpha);
				};
				accumulate(weight, [&](int p_x, int p_y) {
					const auto& x = horizontal[p_x];
					const auto& y = vertical[p_y];
					return read(x.left, y.left).lerp(read(x.right, y.left), x.fraction)
						.lerp(read(x.left, y.right).lerp(read(x.right, y.right), x.fraction), y.fraction);
				});
			}
			else if (value.type == image::type_gradient) {
				litehtml::background gradient;
				gradient.m_image.push_back(value);
				gradient.m_current_color = p_current;
				litehtml::background_layer tile;
				tile.origin_box = litehtml::position(0, 0, extent.x, extent.y);
				const auto paint = [&](const auto& p_gradient, const auto& p_progress) {
					if (!p_gradient) return;
					const auto stops = make_stops(*p_gradient);
					if (stops.empty()) return;
					accumulate(weight, [&](int p_x, int p_y) {
						const Vector2 point((p_x + 0.5f) * extent.x / width, (p_y + 0.5f) * extent.y / height);
						return sample_gradient(stops, p_progress(*p_gradient, point));
					});
				};
				if (value.m_gradient.is_linear()) {
					paint(gradient.get_linear_gradient_layer(0, tile), [](const auto& p_gradient, const Vector2& p_point) {
						const Vector2 start(p_gradient.start.x, p_gradient.start.y);
						const Vector2 ray = Vector2(p_gradient.end.x, p_gradient.end.y) - start;
						return (p_point - start).dot(ray) / std::max(ray.length_squared(), real_t(0.000001));
					});
				}
				else if (value.m_gradient.is_radial()) {
					paint(gradient.get_radial_gradient_layer(0, tile), [](const auto& p_gradient, const Vector2& p_point) {
						const Vector2 radius(std::max(p_gradient.radius.x, 0.000001f), std::max(p_gradient.radius.y, 0.000001f));
						return ((p_point - Vector2(p_gradient.position.x, p_gradient.position.y)) / radius).length();
					});
				}
				else if (value.m_gradient.is_conic()) {
					paint(gradient.get_conic_gradient_layer(0, tile), [](const auto& p_gradient, const Vector2& p_point) {
						const Vector2 delta = p_point - Vector2(p_gradient.position.x, p_gradient.position.y);
						const double phase = std::atan2(delta.x, -delta.y) / Math_TAU - p_gradient.angle / 360.0;
						return static_cast<float>(phase - std::floor(phase));
					});
				}
			}
		}
		PackedByteArray data;
		if (data.resize(static_cast<int64_t>(count * 4)) != OK) return {};
		uint8_t* bytes = data.ptrw();
		for (size_t index = 0; index < count; ++index) {
			const Color pixel = mixed[index];
			const float alpha = pixel.a;
			bytes[index * 4] = alpha > 0 ? byte_channel(pixel.r / alpha) : 0;
			bytes[index * 4 + 1] = alpha > 0 ? byte_channel(pixel.g / alpha) : 0;
			bytes[index * 4 + 2] = alpha > 0 ? byte_channel(pixel.b / alpha) : 0;
			bytes[index * 4 + 3] = byte_channel(alpha);
		}
		return ImageTexture::create_from_image(Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, data));
	}
}
