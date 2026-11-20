#include "css_image_value.h"

#include "formats/html/css/css_math.h"

#include <litehtml/css_parser.h>
#include <litehtml/document.h>
#include <litehtml/document_container.h>
#include <litehtml/html.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace litehtml {
	struct css_image_set {
		struct Option {
			image value;
			std::optional<double> density;
		};
		std::vector<Option> options;
	};

	struct css_light_dark_image {
		image light;
		image dark;
	};

	bool supports_image_mime(const string& p_type) {
		const string mime = lowcase(trim(p_type.substr(0, p_type.find(';'))));
		return is_one_of(mime, "image/png", "image/jpeg", "image/webp", "image/svg+xml");
	}

	namespace {
		std::optional<image> parse_image(const css_token& p_token, document_container* p_container, bool p_allow_set, int p_depth);

		std::optional<image> parse_light_dark_image(const css_token& p_token, document_container* p_container, bool p_allow_set, int p_depth) {
			const auto& tokens = p_token.value;
			if (tokens.size() != 3 || tokens[1].type != ',') return std::nullopt;
			const auto parse_branch = [&](const css_token& token) -> std::optional<image> {
				if (token.ident() != "none") return parse_image(token, p_container, p_allow_set, p_depth + 1);
				image value;
				value.type = image::type_solid;
				value.fallback_color = web_color::transparent;
				return value;
			};
			auto light = parse_branch(tokens[0]);
			auto dark = parse_branch(tokens[2]);
			if (!light || !dark) return std::nullopt;
			image result;
			result.type = image::type_light_dark;
			result.m_light_dark = std::make_shared<css_light_dark_image>(css_light_dark_image{std::move(*light), std::move(*dark)});
			return result;
		}

		std::optional<image> parse_image_function(const css_token& p_token, document_container* p_container) {
			const auto& tokens = p_token.value;
			image value;
			size_t index = 0;
			if (!tokens.empty()) {
				const auto tag = tokens.front().ident();
				if (tag == "ltr" || tag == "rtl") {
					value.direction = tag == "ltr" ? css_direction::ltr : css_direction::rtl;
					++index;
				}
			}
			if (index == tokens.size()) return std::nullopt;

			const auto& source = tokens[index];
			if (source.type == STRING || parse_url(source, value.url)) {
				if (source.type == STRING) value.url = trim(source.str);
				value.type = image::type_url;
				if (++index == tokens.size()) return value;
				if (tokens[index].type != ',') return std::nullopt;
			}
			if (tokens[index].type == ',') ++index;
			web_color color;
			if (index + 1 != tokens.size() || !parse_color(tokens[index], color, p_container)) return std::nullopt;
			value.fallback_color = std::move(color);
			if (value.type == image::type_none) value.type = image::type_solid;
			return value;
		}

		std::optional<image> parse_basic_image(const css_token& p_token, document_container* p_container) {
			if (p_token.type == CV_FUNCTION && equal_i(p_token.name, "image"))
				return parse_image_function(p_token, p_container);
			image value;
			if (parse_url(p_token, value.url)) {
				value.type = image::type_url;
			}
			else if (parse_gradient(p_token, value.m_gradient, p_container)) {
				value.type = image::type_gradient;
			}
			else {
				return std::nullopt;
			}
			return value;
		}

		std::optional<image> parse_image_set(const css_token& p_token, document_container* p_container, int p_depth) {
			auto candidates = std::make_shared<css_image_set>();
			const auto options = parse_comma_separated_list(p_token.value);
			if (options.empty()) return std::nullopt;
			for (const auto& tokens : options) {
				if (tokens.empty() || tokens.size() > 3) return std::nullopt;
				css_image_set::Option candidate;
				if (tokens.front().type == STRING) {
					candidate.value.type = image::type_url;
					candidate.value.url = trim(tokens.front().str);
				}
				else if (auto value = parse_image(tokens.front(), p_container, false, p_depth + 1)) {
					candidate.value = std::move(*value);
				}
				else {
					return std::nullopt;
				}

				bool has_density = false;
				bool has_type = false;
				bool supported_type = true;
				for (size_t index = 1; index < tokens.size(); ++index) {
					const auto& descriptor = tokens[index];
					if (descriptor.type == CV_FUNCTION && equal_i(descriptor.name, "type")) {
						if (has_type || descriptor.value.size() != 1 || descriptor.value.front().type != STRING)
							return std::nullopt;
						has_type = true;
						supported_type = supports_image_mime(descriptor.value.front().str);
					}
					else {
						const auto density = parse_css_resolution(descriptor);
						if (has_density || !density) return std::nullopt;
						has_density = true;
						candidate.density = *density;
					}
				}
				if (supported_type) candidates->options.push_back(std::move(candidate));
			}
			image result;
			result.type = image::type_set;
			result.m_set = std::move(candidates);
			return result;
		}

		std::optional<double> percentage(const css_token& p_token) {
			if (p_token.type == PERCENTAGE && p_token.n.number >= 0 && p_token.n.number <= 100)
				return p_token.n.number / 100.0;
			if (auto value = evaluate_css_scalar_math(p_token, css_math_scalar::percentage))
				return std::clamp(*value, 0.0, 1.0);
			return std::nullopt;
		}

		std::optional<image> parse_cross_fade(const css_token& p_token, document_container* p_container, bool p_allow_set, int p_depth) {
			auto mixture = std::make_shared<css_cross_fade>();
			double explicit_total = 0;
			int omitted = 0;
			for (const auto& item : parse_comma_separated_list(p_token.value)) {
				if (item.empty() || item.size() > 2) return std::nullopt;
				const css_token* source = &item.front();
				std::optional<double> weight;
				if (item.size() == 2) {
					weight = percentage(item.back());
					if (!weight) {
						weight = percentage(item.front());
						source = &item.back();
					}
					if (!weight) return std::nullopt;
				}
				image value;
				web_color color;
				if (parse_color(*source, color, p_container)) {
					value.type = image::type_solid;
					value.fallback_color = std::move(color);
				}
				else if (auto parsed = parse_image(*source, p_container, p_allow_set, p_depth + 1)) {
					value = std::move(*parsed);
				}
				else {
					return std::nullopt;
				}
				if (weight) explicit_total += *weight;
				else ++omitted;
				mixture->layers.push_back({ std::move(value), weight.value_or(-1.0) });
			}
			const double share = omitted ? std::max(0.0, 1.0 - explicit_total) / omitted : 0.0;
			const double divisor = std::max(1.0, explicit_total);
			for (auto& layer : mixture->layers)
				layer.weight = layer.weight < 0 ? share : layer.weight / divisor;
			image result;
			result.type = image::type_cross_fade;
			result.m_cross_fade = std::move(mixture);
			return result;
		}

		std::optional<image> parse_image(const css_token& p_token, document_container* p_container, bool p_allow_set, int p_depth) {
			if (p_depth >= 32) return std::nullopt;
			if (p_token.type == CV_FUNCTION) {
				if (equal_i(p_token.name, "image-set"))
					return p_allow_set ? parse_image_set(p_token, p_container, p_depth) : std::nullopt;
				if (equal_i(p_token.name, "cross-fade"))
					return parse_cross_fade(p_token, p_container, p_allow_set, p_depth);
				if (equal_i(p_token.name, "light-dark"))
					return parse_light_dark_image(p_token, p_container, p_allow_set, p_depth);
			}
			return parse_basic_image(p_token, p_container);
		}

		void select_css_image(image& r_image, double p_output_density) {
			const css_image_set::Option* smallest_sufficient = nullptr;
			const css_image_set::Option* largest_below = nullptr;
			for (const auto& option : r_image.m_set->options) {
				const double density = option.density.value_or(1.0);
				if (density >= p_output_density) {
					if (!smallest_sufficient || density < smallest_sufficient->density.value_or(1.0))
						smallest_sufficient = &option;
				}
				else if (!largest_below || density > largest_below->density.value_or(1.0)) {
					largest_below = &option;
				}
			}
			const auto* selected = smallest_sufficient ? smallest_sufficient : largest_below;
			image value;
			if (selected) {
				value = selected->value;
				value.from_image_set = true;
				value.candidate_density = selected->density;
			}
			r_image = std::move(value);
		}
	}

	std::optional<image> parse_css_image(const css_token& p_token, document_container* p_container) {
		return parse_image(p_token, p_container, true, 0);
	}

	double resolve_image_dppx(const string& p_url, const string& p_base, document_container& p_container,
		const css_image_resolution& p_resolution, bool p_from_image_set, std::optional<double> p_candidate_density) {
		double density = p_resolution.density.value_or(1.0);
		if (p_resolution.from_image || !p_resolution.density) {
			if (p_candidate_density) {
				density = *p_candidate_density;
			}
			else if (p_resolution.from_image || p_from_image_set) {
				const double dpi = p_container.get_image_dpi(p_url.c_str(), p_base.c_str());
				if (dpi > 0) density = dpi / CSS_DPI_PER_DPPX;
			}
		}
		if (p_resolution.snap && density > 0 && std::isfinite(density)) {
			media_features output;
			p_container.get_media_features(output);
			const double output_density = output.resolution > 0 ? output.resolution / CSS_DPI_PER_DPPX : 1.0;
			const double ratio = density / output_density;
			if (ratio >= 1.0) {
				density = std::round(ratio) * output_density;
			}
			else if (const double enlargement = 1.0 / ratio; std::isfinite(enlargement)) {
				// Compare the adjacent whole-pixel enlargements in resolution units.
				const double lower = output_density / std::ceil(enlargement);
				const double upper = output_density / std::floor(enlargement);
				density = density - lower < upper - density ? lower : upper;
			}
		}
		return density;
	}

	void compute_css_image(image& r_image, const string& p_base, document& p_doc, const font_metrics& p_font,
		const css_image_resolution& p_resolution, css_image_orientation p_orientation, color_scheme p_scheme, css_length_context p_units) {
		auto* container = p_doc.container();
		while (r_image.type == image::type_set || r_image.type == image::type_light_dark) {
			if (r_image.type == image::type_set) {
				media_features output;
				container->get_media_features(output);
				select_css_image(r_image, output.resolution > 0 ? output.resolution / CSS_DPI_PER_DPPX : 1.0);
			} else {
				auto selected = p_scheme == color_scheme::dark ? r_image.m_light_dark->dark : r_image.m_light_dark->light;
				if (r_image.from_image_set) {
					selected.from_image_set = true;
					selected.candidate_density = r_image.candidate_density;
				}
				r_image = std::move(selected);
			}
		}
		if (r_image.fallback_color) r_image.fallback_color = r_image.fallback_color->compute_for_scheme(p_scheme);
		switch (r_image.type) {
			case image::type_url:
				r_image.orientation = p_orientation;
				r_image.density = resolve_image_dppx(r_image.url, p_base, *container, p_resolution,
					r_image.from_image_set, r_image.candidate_density);
				if (r_image.fallback_color) {
					size natural;
					container->get_image_size(r_image.url.c_str(), p_base.c_str(), natural, p_orientation);
					if (natural.width <= 0 || natural.height <= 0) r_image.type = image::type_solid;
				}
				else if (!r_image.url.empty()) {
					container->load_image(r_image.url.c_str(), p_base.c_str(), true, p_orientation);
				}
				break;
			case image::type_gradient: {
				auto& gradient = r_image.m_gradient;
				for (auto* length : { &gradient.radial_radius_x, &gradient.radial_radius_y, &gradient.position_x, &gradient.position_y })
					p_doc.cvt_units(*length, p_font, 0, p_units);
				for (auto& stop : gradient.m_colors) {
					stop.color = stop.color.compute_for_scheme(p_scheme);
					if (stop.length) p_doc.cvt_units(*stop.length, p_font, 0, p_units);
				}
				break;
			}
			case image::type_cross_fade: {
				auto mixture = std::make_shared<css_cross_fade>();
				for (const auto& layer : r_image.m_cross_fade->layers) {
					if (layer.weight <= 0) continue;
					auto value = layer.value;
					compute_css_image(value, p_base, p_doc, p_font, p_resolution, p_orientation, p_scheme, p_units);
					if (value.type == image::type_cross_fade) {
						for (const auto& nested : value.m_cross_fade->layers)
							mixture->layers.push_back({ nested.value, nested.weight * layer.weight });
					}
					else {
						mixture->layers.push_back({ std::move(value), layer.weight });
					}
				}
				r_image.m_cross_fade = std::move(mixture);
				break;
			}
			default: break;
		}
	}

	size get_css_image_natural_size(const image& p_image, const string& p_base, document_container& p_container) {
		if (p_image.type == image::type_url) {
			size natural;
			p_container.get_css_image_size(p_image.url.c_str(), p_base.c_str(), p_image.density, natural, p_image.orientation);
			return natural;
		}
		if (p_image.type != image::type_cross_fade) return {};
		double width = 0, height = 0, total = 0;
		for (const auto& layer : p_image.m_cross_fade->layers) {
			if (layer.weight <= 0) continue;
			const auto dimensions = get_css_image_natural_size(layer.value, p_base, p_container);
			if (dimensions.width <= 0 || dimensions.height <= 0) continue;
			width += dimensions.width * layer.weight;
			height += dimensions.height * layer.weight;
			total += layer.weight;
		}
		return total > 0 ? size(static_cast<pixel_t>(width / total), static_cast<pixel_t>(height / total)) : size{};
	}
}
