#include "css_color.h"
#include "formats/html/css/css_math.h"

#include <litehtml/css_parser.h>
#include <litehtml/html.h>
#include "css_web_color.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace litehtml {
	namespace {
		constexpr double PI = 3.14159265358979323846;
		constexpr unsigned COMPONENTS = 0b0111;
		constexpr unsigned ALPHA = 0b1000;
		constexpr unsigned MAX_COLOR_DEPTH = 64;

		using Components = std::array<double, 4>;
		using Channels = std::array<double, 3>;
		using Matrix = std::array<Channels, 3>;

		struct ColorSample {
			css_color_space space;
			Components values;
			unsigned missing = 0;
			bool legacy_srgb = false;
		};

		constexpr ColorSample BLACK{ css_color_space::srgb, { 0.0, 0.0, 0.0, 1.0 }, 0, true };
		constexpr ColorSample WHITE{ css_color_space::srgb, { 1.0, 1.0, 1.0, 1.0 }, 0, true };

		enum class HueInterpolation { shorter, longer, increasing, decreasing };

		struct MixItem {
			web_color color;
			double weight = -1.0;
		};

		struct ColorMix {
			css_color_space space = css_color_space::oklab;
			HueInterpolation hue = HueInterpolation::shorter;
			std::vector<MixItem> items;
			double alpha_multiplier = 1.0;
		};

		struct ColorFunction {
			css_color_space space;
			bool rgb;
			Channels scales;
			Channels units;
			css_math_input_names names;
		};

		struct RelativeChannel {
			int source = -1;
			bool missing = false;
			double constant = 0.0;
			css_math_node_ptr calculation;
		};

		struct RelativeColor {
			web_color origin;
			ColorFunction function;
			std::array<RelativeChannel, 4> channels;
		};

		struct LightDarkColor {
			web_color light;
			web_color dark;

			const web_color& select(color_scheme scheme) const {
				return scheme == color_scheme::dark ? dark : light;
			}
		};

		struct ContrastColor {
			web_color background;
		};

		bool parse_component(const css_token& token, double percent_scale, double& value,
			css_math_scalar scalar = css_math_scalar::number_or_percentage) {
			if (scalar == css_math_scalar::number_or_percentage && token.ident() == "none") {
				value = 0.0;
				return true;
			}
			if (token.type == CV_FUNCTION) {
				const auto result = evaluate_css_scalar_math(token, scalar, percent_scale);
				if (!result) return false;
				value = *result;
				return true;
			}
			if ((token.type != NUMBER && token.type != PERCENTAGE) ||
				(scalar == css_math_scalar::number && token.type != NUMBER) ||
				(scalar == css_math_scalar::percentage && token.type != PERCENTAGE)) {
				return false;
			}
			value = double(token.n.number) * (token.type == PERCENTAGE ? percent_scale / 100.0 : 1.0);
			return std::isfinite(value);
		}

		bool parse_hue(const css_token& token, double& degrees) {
			if (token.type == CV_FUNCTION) {
				const auto result = evaluate_css_scalar_math(token, css_math_scalar::number_or_angle);
				if (!result) return false;
				degrees = *result;
				return true;
			}
			if (token.type != DIMENSION) {
				return token.type != PERCENTAGE && parse_component(token, 1.0, degrees);
			}
			const auto unit = lowcase(token.unit);
			const double value = token.n.number;
			if (unit == "deg") degrees = value;
			else if (unit == "grad") degrees = value * 0.9;
			else if (unit == "rad") degrees = value * (180.0 / PI);
			else if (unit == "turn") degrees = value * 360.0;
			else return false;
			return std::isfinite(degrees);
		}

		double normalize_hue(double hue) {
			if (!std::isfinite(hue)) return 0.0;
			const double degrees = std::fmod(hue, 360.0);
			return degrees < 0.0 ? degrees + 360.0 : degrees;
		}

		bool parse_legacy_components(const css_token_vector& values, css_color_space space, Components& components) {
			const auto args = parse_comma_separated_list(values);
			if ((args.size() != 3 && args.size() != 4) ||
				std::any_of(args.begin(), args.end(), [](const auto& item) { return item.size() != 1; })) return false;
			const auto& first = args[0].front();
			double first_percentage = 0.0;
			const bool hsl = space == css_color_space::hsl;
			const bool percentages = hsl || parse_component(first, 255.0, first_percentage, css_math_scalar::percentage);
			const auto scalar = percentages ? css_math_scalar::percentage : css_math_scalar::number;
			const double scale = hsl ? 100.0 : 255.0;
			if (hsl) {
				if (first.ident() == "none" || !parse_hue(first, components[0])) return false;
			} else if (percentages) {
				components[0] = first_percentage;
			} else if (!parse_component(first, scale, components[0], scalar)) {
				return false;
			}
			if (!parse_component(args[1].front(), scale, components[1], scalar) ||
				!parse_component(args[2].front(), scale, components[2], scalar)) return false;
			return args.size() == 3 || (args[3].front().ident() != "none" && parse_component(args[3].front(), 1.0, components[3]));
		}

		double to_linear(double channel) {
			const double magnitude = std::abs(channel);
			return magnitude <= 0.04045 ? channel / 12.92 :
				std::copysign(std::pow((magnitude + 0.055) / 1.055, 2.4), channel);
		}

		double to_srgb_channel(double channel) {
			const double magnitude = std::abs(channel);
			return magnitude <= 0.0031308 ? channel * 12.92 :
				std::copysign(1.055 * std::pow(magnitude, 1.0 / 2.4) - 0.055, channel);
		}

		Channels to_oklab(const Channels& rgb) {
			// CSS Color 4 conversion from linear sRGB through LMS to Oklab.
			const auto [r, g, b] = rgb;
			const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
			const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
			const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
			return {
				0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
				1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
				0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
			};
		}

		Channels oklab_to_linear(const Channels& lab) {
			const auto [lightness, a, b] = lab;
			const auto cube = [](double value) { return value * value * value; };
			const double l = cube(lightness + 0.3963377774 * a + 0.2158037573 * b);
			const double m = cube(lightness - 0.1055613458 * a - 0.0638541728 * b);
			const double s = cube(lightness - 0.0894841775 * a - 1.2914855480 * b);
			return {
				4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
				-1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
				-0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
			};
		}

		Channels polar_to_lab(const Channels& lch) {
			const auto [lightness, chroma, hue] = lch;
			const double angle = hue * (PI / 180.0);
			return { lightness, chroma * std::cos(angle), chroma * std::sin(angle) };
		}

		Channels hsl_to_srgb(const Channels& hsl) {
			const auto [hue, saturation, lightness] = hsl;
			const double amplitude = saturation * std::min(lightness, 1.0 - lightness);
			const auto channel = [&](double offset) {
				const double k = std::fmod(offset + hue / 30.0, 12.0);
				return lightness - amplitude * std::max(-1.0, std::min({ k - 3.0, 9.0 - k, 1.0 }));
			};
			return { channel(0.0), channel(8.0), channel(4.0) };
		}

		Channels apply_matrix(const Matrix& matrix, const Channels& channels) {
			const auto [x, y, z] = channels;
			Channels result{};
			for (size_t row = 0; row < result.size(); ++row) {
				const auto [a, b, c] = matrix[row];
				result[row] = a * x + b * y + c * z;
			}
			return result;
		}

		// Reference matrices and transfer curves from CSS Color 4.
		// https://www.w3.org/TR/css-color-4/#color-conversion-code
		struct RGBSpace {
			Matrix to_xyz;
			Matrix from_xyz;
			double gamma;
			double linear_threshold;
			double linear_scale;
		};

		constexpr RGBSpace SRGB {
			{{ { 506752.0 / 1228815, 87881.0 / 245763, 12673.0 / 70218 },
			   { 87098.0 / 409605, 175762.0 / 245763, 12673.0 / 175545 },
			   { 7918.0 / 409605, 87881.0 / 737289, 1001167.0 / 1053270 } }},
			{{ { 12831.0 / 3959, -329.0 / 214, -1974.0 / 3959 },
			   { -851781.0 / 878810, 1648619.0 / 878810, 36519.0 / 878810 },
			   { 705.0 / 12673, -2585.0 / 12673, 705.0 / 667 } }},
			1.0, 0.0, 1.0,
		};

		constexpr RGBSpace DISPLAY_P3 {
			{{ { 608311.0 / 1250200, 189793.0 / 714400, 198249.0 / 1000160 },
			   { 35783.0 / 156275, 247089.0 / 357200, 198249.0 / 2500400 },
			   { 0.0, 32229.0 / 714400, 5220557.0 / 5000800 } }},
			{{ { 446124.0 / 178915, -333277.0 / 357830, -72051.0 / 178915 },
			   { -14852.0 / 17905, 63121.0 / 35810, 423.0 / 17905 },
			   { 11844.0 / 330415, -50337.0 / 660830, 316169.0 / 330415 } }},
			1.0, 0.0, 1.0,
		};

		constexpr RGBSpace A98_RGB {
			{{ { 573536.0 / 994567, 263643.0 / 1420810, 187206.0 / 994567 },
			   { 591459.0 / 1989134, 6239551.0 / 9945670, 374412.0 / 4972835 },
			   { 53769.0 / 1989134, 351524.0 / 4972835, 4929758.0 / 4972835 } }},
			{{ { 1829569.0 / 896150, -506331.0 / 896150, -308931.0 / 896150 },
			   { -851781.0 / 878810, 1648619.0 / 878810, 36519.0 / 878810 },
			   { 16779.0 / 1248040, -147721.0 / 1248040, 1266979.0 / 1248040 } }},
			563.0 / 256, 0.0, 1.0,
		};

		constexpr RGBSpace PROPHOTO_RGB {
			{{ { 0.7977666449006423, 0.13518129740053308, 0.03134773412839220 },
			   { 0.2880748288194013, 0.71183523424187300, 0.00008993693872564 },
			   { 0.0, 0.0, 0.82510460251046020 } }},
			{{ { 1.3457868816471583, -0.25557208737979464, -0.05110186497554526 },
			   { -0.5446307051249019, 1.50824774284514680, 0.02052744743642139 },
			   { 0.0, 0.0, 1.21196754563894520 } }},
			1.8, 1.0 / 512, 16.0,
		};

		constexpr RGBSpace REC2020 {
			{{ { 63426534.0 / 99577255, 20160776.0 / 139408157, 47086771.0 / 278816314 },
			   { 26158966.0 / 99577255, 472592308.0 / 697040785, 8267143.0 / 139408157 },
			   { 0.0, 19567812.0 / 697040785, 295819943.0 / 278816314 } }},
			{{ { 30757411.0 / 17917100, -6372589.0 / 17917100, -4539589.0 / 17917100 },
			   { -19765991.0 / 29648200, 47925759.0 / 29648200, 467509.0 / 29648200 },
			   { 792561.0 / 44930125, -1921689.0 / 44930125, 42328811.0 / 44930125 } }},
			2.4, 0.0, 1.0,
		};

		constexpr Matrix D65_TO_D50 {{
			{ 1.0479297925449969, 0.022946870601609652, -0.05019226628920524 },
			{ 0.02962780877005599, 0.9904344267538799, -0.017073799063418826 },
			{ -0.009243040646204504, 0.015055191490298152, 0.7518742814281371 },
		}};

		constexpr Matrix D50_TO_D65 {{
			{ 0.955473421488075, -0.02309845494876471, 0.06325924320057072 },
			{ -0.0283697093338637, 1.0099953980813041, 0.021041441191917323 },
			{ 0.012314014864481998, -0.020507649298898964, 1.330365926242124 },
		}};

		constexpr Channels D50_WHITE{ 0.3457 / 0.3585, 1.0, (1.0 - 0.3457 - 0.3585) / 0.3585 };

		Channels lab_to_xyz(const Channels& lab) {
			const auto [lightness, a, b] = lab;
			const double y = (lightness + 16.0) / 116.0;
			Channels xyz{ y + a / 500.0, y, y - b / 200.0 };
			for (size_t i = 0; i < xyz.size(); ++i) {
				const double value = xyz[i];
				const double linear = value > 6.0 / 29 ? value * value * value :
					(116.0 * value - 16.0) / (24389.0 / 27);
				xyz[i] = linear * D50_WHITE[i];
			}
			return xyz;
		}

		Channels xyz_to_lab(Channels xyz) {
			for (size_t i = 0; i < xyz.size(); ++i) {
				const double value = xyz[i] / D50_WHITE[i];
				xyz[i] = value > 216.0 / 24389 ? std::cbrt(value) :
					((24389.0 / 27) * value + 16.0) / 116.0;
			}
			return { 116.0 * xyz[1] - 16.0, 500.0 * (xyz[0] - xyz[1]), 200.0 * (xyz[1] - xyz[2]) };
		}

		const RGBSpace* predefined_rgb(css_color_space space) {
			switch (space) {
				case css_color_space::srgb:
				case css_color_space::srgb_linear: return &SRGB;
				case css_color_space::display_p3:
				case css_color_space::display_p3_linear: return &DISPLAY_P3;
				case css_color_space::a98_rgb: return &A98_RGB;
				case css_color_space::prophoto_rgb: return &PROPHOTO_RGB;
				case css_color_space::rec2020: return &REC2020;
				default: return nullptr;
			}
		}

		bool uses_srgb_transfer(css_color_space space) {
			return space == css_color_space::srgb || space == css_color_space::display_p3;
		}

		Channels rgb_to_xyz(css_color_space space, const RGBSpace& rgb, Channels channels) {
			for (double& channel : channels) {
				if (uses_srgb_transfer(space)) {
					channel = to_linear(channel);
				} else if (std::abs(channel) < rgb.linear_threshold * rgb.linear_scale) {
					channel /= rgb.linear_scale;
				} else if (rgb.gamma != 1.0) {
					channel = std::copysign(std::pow(std::abs(channel), rgb.gamma), channel);
				}
			}
			channels = apply_matrix(rgb.to_xyz, channels);
			return space == css_color_space::prophoto_rgb ? apply_matrix(D50_TO_D65, channels) : channels;
		}

		Channels xyz_to_rgb(css_color_space space, const RGBSpace& rgb, Channels channels) {
			if (space == css_color_space::prophoto_rgb) channels = apply_matrix(D65_TO_D50, channels);
			channels = apply_matrix(rgb.from_xyz, channels);
			for (double& channel : channels) {
				if (uses_srgb_transfer(space)) {
					channel = to_srgb_channel(channel);
				} else if (std::abs(channel) < rgb.linear_threshold) {
					channel *= rgb.linear_scale;
				} else if (rgb.gamma != 1.0) {
					channel = std::copysign(std::pow(std::abs(channel), 1.0 / rgb.gamma), channel);
				}
			}
			return channels;
		}

		double hue_of_rgb(const Channels& rgb, double maximum, double range) {
			if (range == 0.0) return 0.0;
			const auto [r, g, b] = rgb;
			if (maximum == r) return normalize_hue((g - b) / range * 60.0);
			if (maximum == g) return ((b - r) / range + 2.0) * 60.0;
			return ((r - g) / range + 4.0) * 60.0;
		}

		Channels rgb_to_hsl_or_hwb(const Channels& rgb, css_color_space space) {
			const auto [minimum, maximum] = std::minmax_element(rgb.begin(), rgb.end());
			const double range = *maximum - *minimum;
			double hue = hue_of_rgb(rgb, *maximum, range);
			if (space == css_color_space::hwb) return { hue, *minimum, 1.0 - *maximum };

			const double lightness = (*maximum + *minimum) / 2.0;
			const double bound = std::min(lightness, 1.0 - lightness);
			const double saturation = std::abs(bound) > 0.00001 ? range / (2.0 * bound) : 0.0;
			if (saturation < 0.0) hue = normalize_hue(hue + 180.0);
			return { hue, std::abs(saturation), lightness };
		}

		Channels hwb_to_srgb(const Channels& channels) {
			const auto [hue, white, black] = channels;
			if (white + black >= 1.0) {
				const double gray = white / (white + black);
				return { gray, gray, gray };
			}
			auto rgb = hsl_to_srgb({ hue, 1.0, 0.5 });
			for (double& channel : rgb) channel = channel * (1.0 - white - black) + white;
			return rgb;
		}

		bool is_ok_space(css_color_space space) {
			return space == css_color_space::oklab || space == css_color_space::oklch;
		}

		bool is_cie_lab_space(css_color_space space) {
			return space == css_color_space::lab || space == css_color_space::lch;
		}

		bool is_srgb_polar(css_color_space space) {
			return space == css_color_space::hsl || space == css_color_space::hwb;
		}

		bool is_lch_space(css_color_space space) {
			return space == css_color_space::lch || space == css_color_space::oklch;
		}

		int hue_index(css_color_space space) {
			if (is_srgb_polar(space)) return 0;
			return is_lch_space(space) ? 2 : -1;
		}

		Channels to_xyz_d65(css_color_space space, Channels channels) {
			if (const auto* rgb = predefined_rgb(space)) return rgb_to_xyz(space, *rgb, channels);
			if (space == css_color_space::xyz_d65) return channels;
			if (space == css_color_space::xyz_d50) return apply_matrix(D50_TO_D65, channels);
			if (is_lch_space(space)) channels = polar_to_lab(channels);
			if (is_cie_lab_space(space)) return apply_matrix(D50_TO_D65, lab_to_xyz(channels));
			if (is_ok_space(space)) return apply_matrix(SRGB.to_xyz, oklab_to_linear(channels));
			channels = space == css_color_space::hsl ? hsl_to_srgb(channels) : hwb_to_srgb(channels);
			return rgb_to_xyz(css_color_space::srgb, SRGB, channels);
		}

		Channels from_xyz_d65(css_color_space space, const Channels& channels) {
			if (const auto* rgb = predefined_rgb(space)) return xyz_to_rgb(space, *rgb, channels);
			if (space == css_color_space::xyz_d65) return channels;
			if (space == css_color_space::xyz_d50) return apply_matrix(D65_TO_D50, channels);
			if (is_cie_lab_space(space)) return xyz_to_lab(apply_matrix(D65_TO_D50, channels));
			if (is_ok_space(space)) return to_oklab(apply_matrix(SRGB.from_xyz, channels));
			return rgb_to_hsl_or_hwb(xyz_to_rgb(css_color_space::srgb, SRGB, channels), space);
		}

		enum class Component { red, green, blue, lightness, colorfulness, hue, a, b, whiteness, blackness };

		std::array<Component, 3> component_categories(css_color_space space) {
			using C = Component;
			switch (space) {
				case css_color_space::lab:
				case css_color_space::oklab: return { C::lightness, C::a, C::b };
				case css_color_space::lch:
				case css_color_space::oklch: return { C::lightness, C::colorfulness, C::hue };
				case css_color_space::hsl: return { C::hue, C::colorfulness, C::lightness };
				case css_color_space::hwb: return { C::hue, C::whiteness, C::blackness };
				default: return { C::red, C::green, C::blue };
			}
		}

		unsigned carry_missing(css_color_space from, css_color_space to, unsigned missing) {
			if ((missing & COMPONENTS) == COMPONENTS || missing == 0) return missing;
			const auto source = component_categories(from);
			const auto target = component_categories(to);
			unsigned carried = missing & ALPHA;
			unsigned source_set = COMPONENTS;
			unsigned target_set = COMPONENTS;
			for (unsigned i = 0; i < source.size(); ++i) {
				const auto match = std::find(target.begin(), target.end(), source[i]);
				if (match == target.end()) continue;
				const unsigned target_bit = 1u << (match - target.begin());
				if (missing & (1u << i)) carried |= target_bit;
				source_set &= ~(1u << i);
				target_set &= ~target_bit;
			}
			if ((missing & source_set) == source_set) carried |= target_set;
			return carried;
		}

		ColorSample convert(ColorSample sample, css_color_space target, bool for_interpolation = false) {
			if (sample.space == target) return sample;

			const auto source = sample.space;
			Channels channels{ sample.values[0], sample.values[1], sample.values[2] };
			if ((is_ok_space(source) && is_ok_space(target)) ||
				(is_cie_lab_space(source) && is_cie_lab_space(target))) {
				if (is_lch_space(source)) channels = polar_to_lab(channels);
			} else if (is_srgb_polar(source) && (is_srgb_polar(target) || target == css_color_space::srgb)) {
				channels = source == css_color_space::hsl ? hsl_to_srgb(channels) : hwb_to_srgb(channels);
				if (is_srgb_polar(target)) channels = rgb_to_hsl_or_hwb(channels, target);
			} else if (source == css_color_space::srgb && is_srgb_polar(target)) {
				channels = rgb_to_hsl_or_hwb(channels, target);
			} else if (const auto* rgb = predefined_rgb(source); rgb && rgb == predefined_rgb(target)) {
				for (double& channel : channels) {
					channel = uses_srgb_transfer(source) ? to_linear(channel) : to_srgb_channel(channel);
				}
			} else {
				channels = from_xyz_d65(target, to_xyz_d65(source, channels));
			}

			sample.missing = for_interpolation ? carry_missing(source, target, sample.missing) : 0;
			if (is_lch_space(target)) {
				const double chroma = std::hypot(channels[1], channels[2]);
				const double hue = normalize_hue(std::atan2(channels[2], channels[1]) * (180.0 / PI));
				channels = { channels[0], chroma, hue };
				const double threshold = target == css_color_space::lch ? 0.0015 : 0.000004;
				if (for_interpolation && chroma <= threshold) sample.missing |= 4;
			} else if (for_interpolation && target == css_color_space::hsl && channels[1] <= 0.00001) {
				sample.missing |= 1;
			} else if (for_interpolation && target == css_color_space::hwb && channels[1] + channels[2] >= 0.99999) {
				sample.missing |= 1;
			}
			sample.space = target;
			for (unsigned i = 0; i < channels.size(); ++i) {
				sample.values[i] = sample.missing & (1u << i) ? 0.0 : channels[i];
			}
			return sample;
		}

		double mix_hue(double first, double second, double progress, HueInterpolation method) {
			const double difference = second - first;
			switch (method) {
				case HueInterpolation::shorter:
					if (difference > 180.0) first += 360.0;
					if (difference < -180.0) second += 360.0;
					break;
				case HueInterpolation::longer:
					if (0.0 < difference && difference < 180.0) first += 360.0;
					if (-180.0 < difference && difference <= 0.0) second += 360.0;
					break;
				case HueInterpolation::increasing:
					if (second < first) second += 360.0;
					break;
				case HueInterpolation::decreasing:
					if (first < second) first += 360.0;
					break;
			}
			return normalize_hue(first + (second - first) * progress);
		}

		ColorSample interpolate(const ColorMix& mix, ColorSample first, ColorSample second, double progress) {
			first = convert(first, mix.space, true);
			second = convert(second, mix.space, true);
			ColorSample result{ mix.space, {}, first.missing & second.missing };
			for (unsigned i = 0; i < result.values.size(); ++i) {
				const unsigned bit = 1u << i;
				if (first.missing & bit) first.values[i] = second.values[i];
				if (second.missing & bit) second.values[i] = first.values[i];
			}

			const double first_weight = 1.0 - progress;
			const double second_weight = progress;
			const double first_alpha = first.values[3] * first_weight;
			const double second_alpha = second.values[3] * second_weight;
			const double alpha = first_alpha + second_alpha;
			for (unsigned i = 0; i < 3; ++i) {
				if (result.missing & (1u << i)) continue;
				if (static_cast<int>(i) == hue_index(mix.space)) {
					result.values[i] = mix_hue(first.values[i], second.values[i], second_weight, mix.hue);
				} else if (result.missing & ALPHA) {
					result.values[i] = first.values[i] * first_weight + second.values[i] * second_weight;
				} else if (alpha > 0.0) {
					result.values[i] = (first.values[i] * first_alpha + second.values[i] * second_alpha) / alpha;
				}
			}
			result.values[3] = alpha;
			return result;
		}

		std::optional<css_color_space> parse_color_space(const string& name) {
			using Space = css_color_space;
			static constexpr std::pair<std::string_view, Space> spaces[] = {
				{ "srgb", Space::srgb }, { "srgb-linear", Space::srgb_linear },
				{ "display-p3", Space::display_p3 }, { "display-p3-linear", Space::display_p3_linear },
				{ "a98-rgb", Space::a98_rgb }, { "prophoto-rgb", Space::prophoto_rgb }, { "rec2020", Space::rec2020 },
				{ "xyz", Space::xyz_d65 }, { "xyz-d65", Space::xyz_d65 }, { "xyz-d50", Space::xyz_d50 },
				{ "lab", Space::lab }, { "lch", Space::lch }, { "oklab", Space::oklab }, { "oklch", Space::oklch },
				{ "hsl", Space::hsl }, { "hwb", Space::hwb },
			};
			for (const auto& [spelling, space] : spaces) {
				if (name == spelling) return space;
			}
			return std::nullopt;
		}

		std::optional<ColorFunction> color_function(const string& name, const css_token& space_token) {
			using Space = css_color_space;
			const bool rgb = name == "rgb" || name == "rgba";
			const bool hsl = name == "hsl" || name == "hsla";
			const bool predefined = name == "color";
			const auto space = rgb ? std::optional(Space::srgb) : hsl ? std::optional(Space::hsl) :
				parse_color_space(predefined ? space_token.ident() : name);
			if (!space) return {};
			const bool lab = is_cie_lab_space(*space);
			const bool ok = is_ok_space(*space);
			const bool xyz = *space == Space::xyz_d50 || *space == Space::xyz_d65;
			if (predefined ? !predefined_rgb(*space) && !xyz : !rgb && !hsl && !lab && !ok && *space != Space::hwb) return {};
			ColorFunction result{*space, rgb, {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}, {"r", "g", "b", "alpha"}};
			if (xyz) result.names = {"x", "y", "z", "alpha"};
			if (rgb) result.scales = result.units = {255.0, 255.0, 255.0};
			if (lab) result.scales = {100.0, *space == Space::lab ? 125.0 : 150.0, 125.0};
			if (ok) result.scales = {1.0, 0.4, 0.4};
			if (lab || ok) result.names = is_lch_space(*space) ? css_math_input_names{"l", "c", "h", "alpha"} :
				css_math_input_names{"l", "a", "b", "alpha"};
			if (is_srgb_polar(*space)) {
				result.scales = result.units = {1.0, 100.0, 100.0};
				result.names = hsl ? css_math_input_names{"h", "s", "l", "alpha"} : css_math_input_names{"h", "w", "b", "alpha"};
			}
			return result;
		}

		bool parse_relative_channel(const css_token& token, const ColorFunction& function, unsigned index,
			RelativeChannel& channel) {
			const auto keyword = token.ident();
			if (keyword == "none") {
				channel.missing = true;
				return true;
			}
			if (!keyword.empty()) {
				const auto match = std::find(function.names.begin(), function.names.end(), keyword);
				if (match == function.names.end()) return false;
				channel.source = static_cast<int>(match - function.names.begin());
				return true;
			}
			const bool hue = static_cast<int>(index) == hue_index(function.space);
			if (token.type == CV_FUNCTION) {
				channel.calculation = parse_css_scalar_math(token,
					hue ? css_math_scalar::number_or_angle : css_math_scalar::number_or_percentage, &function.names);
				return bool(channel.calculation);
			}
			return hue ? parse_hue(token, channel.constant) :
				parse_component(token, index < 3 ? function.scales[index] : 1.0, channel.constant);
		}

		bool parse_mix_space(const css_token_vector& tokens, ColorMix& mix) {
			if ((tokens.size() != 2 && tokens.size() != 4) || tokens[0].ident() != "in") return false;
			const auto space = parse_color_space(tokens[1].ident());
			if (!space) return false;
			mix.space = *space;
			if (tokens.size() == 2) return true;
			if (hue_index(mix.space) < 0 || tokens[3].ident() != "hue") return false;
			const auto method = tokens[2].ident();
			if (method == "shorter") mix.hue = HueInterpolation::shorter;
			else if (method == "longer") mix.hue = HueInterpolation::longer;
			else if (method == "increasing") mix.hue = HueInterpolation::increasing;
			else if (method == "decreasing") mix.hue = HueInterpolation::decreasing;
			else return false;
			return true;
		}

		bool parse_mix_item(const css_token_vector& tokens, MixItem& item,
			document_container* container, unsigned depth) {
			if (tokens.empty() || tokens.size() > 2) return false;
			size_t color_index = 0;
			if (tokens.size() == 2) {
				const auto parse_weight = [](const css_token& weight) -> std::optional<double> {
					if (weight.type != PERCENTAGE) {
						const auto computed = evaluate_css_scalar_math(weight, css_math_scalar::percentage, 100.0);
						return computed ? std::optional<double>(std::clamp(*computed, 0.0, 100.0)) : std::nullopt;
					}
					const double value = weight.n.number;
					if (!std::isfinite(value) || value < 0.0 || value > 100.0) return std::nullopt;
					return value;
				};
				auto weight = parse_weight(tokens[0]);
				if (weight) color_index = 1;
				else weight = parse_weight(tokens[1]);
				if (!weight) return false;
				item.weight = *weight;
			}
			return parse_color_at_depth(tokens[color_index], item.color, container, depth);
		}

		Components display_srgb(const ColorSample& sample) {
			Components values;
			const double white_lightness = is_ok_space(sample.space) ? 1.0 : 100.0;
			if ((is_ok_space(sample.space) || is_cie_lab_space(sample.space)) &&
				(sample.values[0] <= 0.0 || sample.values[0] >= white_lightness)) {
				const double gray = sample.values[0] <= 0.0 ? 0.0 : 1.0;
				values = {gray, gray, gray, sample.values[3]};
			} else {
				values = convert(sample, css_color_space::srgb).values;
			}
			for (double& value : values) value = std::isnan(value) ? 0.0 : std::clamp(value, 0.0, 1.0);
			return values;
		}

		byte to_byte(double value) {
			return static_cast<byte>(std::lround(value * 255.0));
		}
	}

	struct css_color_data {
		const std::variant<ColorSample, ColorMix, RelativeColor, LightDarkColor, ContrastColor> value;
		bool uses_scheme = false;
	};

	namespace {
		const ColorSample* get_sample(const web_color& color) {
			return color.css_data ? std::get_if<ColorSample>(&color.css_data->value) : nullptr;
		}

		bool uses_scheme(const web_color& color) {
			return color.system != system_color::none || (color.css_data && color.css_data->uses_scheme);
		}

		ColorSample evaluate(const web_color& color, const ColorSample& current, color_scheme scheme = color_scheme::light);

		ColorSample evaluate_contrast(const ColorSample& input) {
			// Compare opaque sRGB colors with the maximum relative luminance contrast.
			const auto srgb = display_srgb(input);
			const double luminance = 0.2126 * to_linear(srgb[0]) + 0.7152 * to_linear(srgb[1]) + 0.0722 * to_linear(srgb[2]);
			const double black_contrast = (luminance + 0.05) / 0.05;
			const double white_contrast = 1.05 / (luminance + 0.05);
			return black_contrast > white_contrast ? BLACK : WHITE;
		}

		ColorSample evaluate_mix(const ColorMix& mix, const ColorSample& current, color_scheme scheme = color_scheme::light) {
			const auto& first = mix.items.front();
			auto result = convert(evaluate(first.color, current, scheme), mix.space, true);
			double total = first.weight;
			for (size_t i = 1; i < mix.items.size(); ++i) {
				const auto& item = mix.items[i];
				total += item.weight;
				const double progress = total > 0.0 ? item.weight / total : 0.5;
				result = interpolate(mix, result, evaluate(item.color, current, scheme), progress);
			}
			result.values[3] *= mix.alpha_multiplier;
			return result;
		}

		ColorSample evaluate_relative(const RelativeColor& relative, const ColorSample& current, color_scheme scheme) {
			const auto& function = relative.function;
			const auto origin = convert(evaluate(relative.origin, current, scheme), function.space, true);
			Components inputs = origin.values;
			for (size_t i = 0; i < inputs.size(); ++i) {
				if (origin.missing & (1u << i)) inputs[i] = 0.0;
				else if (i < 3) inputs[i] *= function.units[i];
			}
			ColorSample result{function.space, {}};
			const int hue = hue_index(function.space);
			for (size_t i = 0; i < relative.channels.size(); ++i) {
				const auto& channel = relative.channels[i];
				if (channel.missing || (channel.source >= 0 && (origin.missing & (1u << channel.source)))) {
					result.missing |= 1u << i;
					continue;
				}
				double value = channel.source >= 0 ? inputs[channel.source] : channel.constant;
				if (channel.calculation) {
					value = evaluate_css_scalar_math(*channel.calculation,
						static_cast<int>(i) == hue ? css_math_scalar::number_or_angle : css_math_scalar::number_or_percentage,
						i < 3 ? function.scales[i] : 1.0, &inputs);
				}
				if (i == 3) result.values[i] = std::clamp(value, 0.0, 1.0);
				else if (static_cast<int>(i) == hue) result.values[i] = normalize_hue(value);
				else {
					constexpr double limit = std::numeric_limits<float>::max();
					result.values[i] = std::clamp(value / function.units[i], -limit, limit);
				}
			}
			return result;
		}

		ColorSample evaluate(const web_color& color, const ColorSample& current, color_scheme scheme) {
			if (color.is_current_color) return current;
			scheme = color.computed_scheme.value_or(scheme);
			if (color.system != system_color::none) return evaluate(system_color_value(color.system, scheme), current, scheme);
			if (const auto* sample = get_sample(color)) return *sample;
			if (color.css_data) {
				const auto& value = color.css_data->value;
				if (const auto* mix = std::get_if<ColorMix>(&value)) return evaluate_mix(*mix, current, scheme);
				if (const auto* relative = std::get_if<RelativeColor>(&value)) return evaluate_relative(*relative, current, scheme);
				if (const auto* conditional = std::get_if<LightDarkColor>(&value)) return evaluate(conditional->select(scheme), current, scheme);
				return evaluate_contrast(evaluate(std::get<ContrastColor>(value).background, current, scheme));
			}
			return { css_color_space::srgb, { color.red / 255.0, color.green / 255.0, color.blue / 255.0, color.alpha / 255.0 }, 0, true };
		}

		web_color store_sample(ColorSample value) {
			web_color color;
			color.css_data = std::make_shared<css_color_data>(css_color_data{ value });
			return color;
		}
	}

	web_color make_css_color(css_color_space space, Components components, unsigned missing) {
		return store_sample({space, components, missing});
	}

	std::array<double, 4> css_color_components(const web_color& color, css_color_space space) {
		return convert(evaluate(color, BLACK), space).values;
	}

	bool equal_css_colors(const web_color& first, const web_color& second) {
		const auto a = convert(evaluate(first, BLACK), css_color_space::srgb, true);
		const auto b = convert(evaluate(second, BLACK), css_color_space::srgb, true);
		return a.missing == b.missing && std::equal(a.values.begin(), a.values.end(), b.values.begin(),
			[](double left, double right) { return std::abs(left - right) < 1e-7; });
	}

	web_color interpolate_css_color(const web_color& from, const web_color& to, double progress) {
		const auto first = evaluate(from, BLACK);
		const auto second = evaluate(to, BLACK);
		ColorMix mix;
		const bool legacy = first.legacy_srgb && second.legacy_srgb;
		if (legacy) mix.space = css_color_space::srgb;
		auto value = interpolate(mix, first, second, progress);
		value.legacy_srgb = legacy;
		return store_sample(value);
	}

	bool web_color::needs_current_color() const {
		return is_current_color || system != system_color::none || (css_data && !get_sample(*this));
	}

	web_color web_color::compute_for_scheme(color_scheme used_scheme) const {
		if (!css_data || !css_data->uses_scheme || computed_scheme) return *this;
		if (const auto* conditional = std::get_if<LightDarkColor>(&css_data->value)) {
			const auto& selected = conditional->select(used_scheme);
			if (selected.system != system_color::none) return system_color_value(selected.system, used_scheme);
			return selected.compute_for_scheme(used_scheme);
		}
		// Preserve currentColor inside inherited expressions while fixing their scheme.
		auto computed = *this;
		computed.computed_scheme = used_scheme;
		return computed;
	}

	web_color web_color::resolve(const web_color& current) const {
		if (!needs_current_color()) return *this;
		if (is_current_color) return current;
		if (system != system_color::none) return system_color_value(system, current.scheme);
		const auto result = evaluate(*this, evaluate(current, BLACK, current.scheme), current.scheme);
		return store_sample(result);
	}

	web_color web_color::to_srgb(const web_color& current) const {
		if (!css_data && !needs_current_color()) return *this;
		const auto sample = evaluate(*this, needs_current_color() ? evaluate(current, BLACK, current.scheme) : BLACK, current.scheme);
		const auto srgb = display_srgb(sample);
		return web_color(to_byte(srgb[0]), to_byte(srgb[1]), to_byte(srgb[2]), to_byte(srgb[3]));
	}

	bool parse_absolute_color(const css_token& token, web_color& color) {
		if (token.type != CV_FUNCTION) return false;
		const auto name = lowcase(token.name);
		const auto& values = token.value;
		const bool predefined = name == "color";
		const size_t offset = predefined ? 1 : 0;
		if (values.empty()) return false;
		const auto function = color_function(name, values.front());
		if (!function) return false;

		using Space = css_color_space;
		const auto space = function->space;
		const bool rgb = function->rgb;
		const bool hsl = space == Space::hsl;
		const auto& scales = function->scales;
		const int hue = hue_index(space);
		Components components{ 0.0, 0.0, 0.0, 1.0 };
		unsigned missing = 0;
		const bool legacy = (rgb || hsl) && std::any_of(values.begin(), values.end(),
			[](const css_token& value) { return value.type == ','; });
		if (legacy) {
			if (!parse_legacy_components(values, space, components)) return false;
		} else {
			if (values.size() != offset + 3 &&
				(values.size() != offset + 5 || values[offset + 3].type != '/')) return false;
			for (int i = 0; i < 3; ++i) {
				const auto& value = values[offset + i];
				if ((offset + i != 0 && !value.preceded_by_space) ||
					!(i == hue ? parse_hue(value, components[i]) : parse_component(value, scales[i], components[i]))) return false;
				if (value.ident() == "none") missing |= 1u << i;
			}
			if (values.size() == offset + 5) {
				if (!parse_component(values.back(), 1.0, components[3])) return false;
				if (values.back().ident() == "none") missing |= ALPHA;
			}
		}
		for (int i = 0; i < 3; ++i) {
			if (i != hue && !std::isfinite(components[i])) {
				// Keep unbounded calculations representable during color-space conversion.
				components[i] = std::copysign(double(std::numeric_limits<float>::max()), components[i]);
			}
		}
		if (hue >= 0) components[hue] = normalize_hue(components[hue]);
		if (is_cie_lab_space(space) || is_ok_space(space)) components[0] = std::clamp(components[0], 0.0, scales[0]);
		if (is_lch_space(space)) components[1] = std::max(components[1], 0.0);
		for (size_t i = 0; i < 3; ++i) components[i] /= function->units[i];
		if (hsl) components[1] = std::max(0.0, components[1]);
		if (rgb) {
			for (size_t i = 0; i < 3; ++i) components[i] = std::clamp(components[i], 0.0, 1.0);
		}
		components[3] = std::clamp(components[3], 0.0, 1.0);
		color = store_sample({space, components, missing, !predefined && (rgb || hsl || space == Space::hwb)});
		return true;
	}

	bool parse_relative_color(const css_token& token, web_color& color, document_container* container, unsigned depth) {
		if (token.type != CV_FUNCTION || depth >= MAX_COLOR_DEPTH) return false;
		const auto& values = token.value;
		const auto name = lowcase(token.name);
		const size_t offset = name == "color" ? 3 : 2;
		if ((values.size() != offset + 3 && (values.size() != offset + 5 || values[offset + 3].type != '/')) ||
			values.front().ident() != "from" || !values[1].preceded_by_space ||
			(offset == 3 && !values[2].preceded_by_space)) return false;
		const auto function = color_function(name, values[2]);
		if (!function) return false;
		RelativeColor relative{{}, *function, {}};
		if (!parse_color_at_depth(values[1], relative.origin, container, depth + 1)) return false;
		for (size_t i = 0; i < 3; ++i) {
			const auto& channel = values[offset + i];
			if (!channel.preceded_by_space || !parse_relative_channel(channel, *function, i, relative.channels[i])) return false;
		}
		if (values.size() == offset + 5) {
			if (!parse_relative_channel(values.back(), *function, 3, relative.channels[3])) return false;
		} else {
			relative.channels[3].source = 3;
		}
		if (relative.origin.needs_current_color()) {
			const bool depends_on_scheme = uses_scheme(relative.origin);
			color = web_color();
			color.css_data = std::make_shared<css_color_data>(css_color_data{std::move(relative), depends_on_scheme});
		} else {
			color = store_sample(evaluate_relative(relative, BLACK, color_scheme::light));
		}
		return true;
	}

	bool parse_color_mix(const css_token& token, web_color& color, document_container* container, unsigned depth) {
		if (token.type != CV_FUNCTION || lowcase(token.name) != "color-mix" || depth >= MAX_COLOR_DEPTH) return false;
		const auto arguments = parse_comma_separated_list(token.value);
		ColorMix mix;
		size_t first = 0;
		if (!arguments.front().empty() && arguments.front().front().ident() == "in") {
			if (!parse_mix_space(arguments.front(), mix)) return false;
			first = 1;
		}
		if (first == arguments.size()) return false;

		mix.items.resize(arguments.size() - first);
		double specified_weight = 0.0;
		size_t omitted = 0;
		for (size_t i = 0; i < mix.items.size(); ++i) {
			auto& item = mix.items[i];
			if (!parse_mix_item(arguments[first + i], item, container, depth + 1)) return false;
			if (item.weight < 0.0) ++omitted;
			else specified_weight += item.weight;
		}
		const double remainder = std::max(0.0, 100.0 - specified_weight);
		const double total = omitted == 0 ? specified_weight : specified_weight + remainder;
		if (total == 0.0) {
			color = web_color::transparent;
			return true;
		}
		if (omitted != 0) {
			const double share = remainder / static_cast<double>(omitted);
			for (auto& item : mix.items) {
				if (item.weight < 0.0) item.weight = share;
			}
		}
		mix.alpha_multiplier = std::min(total / 100.0, 1.0);
		if (std::any_of(mix.items.begin(), mix.items.end(),
			[](const MixItem& item) { return item.color.needs_current_color(); })) {
			const bool depends_on_scheme = std::any_of(mix.items.begin(), mix.items.end(),
				[](const MixItem& item) { return uses_scheme(item.color); });
			color = web_color();
			color.css_data = std::make_shared<css_color_data>(css_color_data{std::move(mix), depends_on_scheme});
		} else {
			const auto result = evaluate_mix(mix, BLACK);
			color = make_css_color(result.space, result.values, result.missing);
		}
		return true;
	}

	bool parse_adaptive_color(const css_token& token, web_color& color, document_container* container, unsigned depth) {
		if (token.type != CV_FUNCTION || depth >= MAX_COLOR_DEPTH) return false;
		const auto& values = token.value;
		if (equal_i(token.name, "light-dark")) {
			LightDarkColor pair;
			if (values.size() != 3 || values[1].type != ',' ||
				!parse_color_at_depth(values[0], pair.light, container, depth + 1) ||
				!parse_color_at_depth(values[2], pair.dark, container, depth + 1)) return false;
			color = web_color();
			color.css_data = std::make_shared<css_color_data>(css_color_data{std::move(pair), true});
			return true;
		}
		if (!equal_i(token.name, "contrast-color")) return false;
		ContrastColor contrast;
		if (values.size() != 1 || !parse_color_at_depth(values[0], contrast.background, container, depth + 1)) return false;
		if (contrast.background.needs_current_color()) {
			const bool depends_on_scheme = uses_scheme(contrast.background);
			color = web_color();
			color.css_data = std::make_shared<css_color_data>(css_color_data{std::move(contrast), depends_on_scheme});
		} else {
			color = store_sample(evaluate_contrast(evaluate(contrast.background, BLACK)));
		}
		return true;
	}
}
