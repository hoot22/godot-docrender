#include "html_image_sources.h"

#include "formats/html/css/css_image_value.h"
#include "formats/html/css/css_math.h"
#include "formats/html/markup/html_number.h"

#include <litehtml/css_parser.h>
#include <litehtml/document.h>
#include <litehtml/element.h>
#include <litehtml/media_query.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace dockit {
	namespace {
		constexpr std::string_view HTML_WHITESPACE = " \t\n\r\f";

		bool parse_descriptor(std::string_view p_value, HTMLImageCandidate& r_candidate) {
			const size_t start = p_value.find_first_not_of(HTML_WHITESPACE);
			if (start == std::string_view::npos) return true;
			p_value = p_value.substr(start, p_value.find_last_not_of(HTML_WHITESPACE) - start + 1);
			const char unit = p_value.back();
			p_value.remove_suffix(1);
			if (p_value.empty()) return false;
			const char* end = p_value.data() + p_value.size();
			if (unit == 'w') {
				uint64_t width = 0;
				const auto result = std::from_chars(p_value.data(), end, width);
				if (result.ec != std::errc() || result.ptr != end || width == 0) return false;
				r_candidate.value = static_cast<double>(width);
				r_candidate.width_descriptor = true;
				return true;
			}
			if (unit != 'x') return false;
			const auto density = html_number(p_value, true);
			if (!density || *density <= 0) return false;
			r_candidate.value = *density;
			return true;
		}

		double source_size(const char* p_sizes, const litehtml::document::ptr& p_document,
			const litehtml::media_features& p_media) {
			using namespace litehtml;
			if (!*p_sizes) return p_media.width;
			const auto tokens = normalize(std::string(p_sizes), f_componentize);
			const auto initial_font = p_document->initial_font_metrics();
			for (auto entry : parse_comma_separated_list(tokens)) {
				while (!entry.empty() && entry.back().type == WHITESPACE) entry.pop_back();
				if (entry.empty()) continue;
				const auto value = normalize(css_token_vector{entry.back()}, f_remove_whitespace);
				css_length length;
				if (!length.from_token(value.front(), f_length | f_math | f_positive) || length.is_predefined()) continue;
				entry.pop_back();
				while (!entry.empty() && entry.back().type == WHITESPACE) entry.pop_back();
				if (!entry.empty() && !matches_media_condition(entry, p_document, p_media)) continue;
				const double pixels = p_document->to_pixels(length, initial_font, 0, {&initial_font, &p_media});
				if (std::isfinite(pixels) && pixels >= 0) return pixels;
			}
			return p_media.width;
		}

		HTMLImageSelection select_from_attributes(const litehtml::element& p_source, const char* p_fallback,
			const HTMLImageSourceResolver& p_resolve, const litehtml::media_features& p_media) {
			auto candidates = parse_html_srcset(p_source.get_attr("srcset", ""));
			const bool uses_width = std::any_of(candidates.begin(), candidates.end(),
				[](const HTMLImageCandidate& p_candidate) { return p_candidate.width_descriptor; });
			// A source-set 1x entry wins a tie with the appended fallback.
			if (!candidates.empty() && *p_fallback && !uses_width) candidates.push_back({p_fallback});

			const double output_density = p_media.resolution > 0 ? p_media.resolution / litehtml::CSS_DPI_PER_DPPX : 1.0;
			const double slot = uses_width ? source_size(p_source.get_attr("sizes", ""), p_source.get_document(), p_media) : 1.0;
			HTMLImageSelection selected;
			for (const auto& candidate : candidates) {
				const double density = candidate.width_descriptor ?
					(slot > 0 ? candidate.value / slot : std::numeric_limits<double>::infinity()) : candidate.value;
				if (selected.density) {
					const double previous = *selected.density;
					const bool enough = density >= output_density;
					if (previous >= output_density ? !enough || density >= previous : !enough && density <= previous) continue;
				}
				const auto resolved = p_resolve(candidate.url);
				if (!resolved.is_empty()) selected = {candidate.url, resolved.utf8().get_data(), density, {}};
			}
			if (selected.source.empty() && *p_fallback) {
				const auto resolved = p_resolve(p_fallback);
				if (!resolved.is_empty()) selected = {p_fallback, resolved.utf8().get_data(), {}, {}};
			}
			return selected;
		}
	}

	std::vector<HTMLImageCandidate> parse_html_srcset(std::string_view p_srcset) {
		std::vector<HTMLImageCandidate> result;
		size_t index = 0;
		while (index < p_srcset.size()) {
			index = p_srcset.find_first_not_of(" ,\t\n\r\f", index);
			if (index == std::string_view::npos) break;

			const size_t start = index;
			while (index < p_srcset.size() && HTML_WHITESPACE.find(p_srcset[index]) == std::string_view::npos) ++index;
			auto url = p_srcset.substr(start, index - start);
			if (url.back() == ',') {
				while (!url.empty() && url.back() == ',') url.remove_suffix(1);
				result.push_back({std::string(url)});
				continue;
			}

			// Commas within a URL or parenthesized descriptor do not end the candidate.
			const size_t descriptor = index;
			bool in_parens = false;
			for (; index < p_srcset.size(); ++index) {
				const char ch = p_srcset[index];
				if (ch == ',' && !in_parens) break;
				if (ch == '(') in_parens = true;
				else if (ch == ')') in_parens = false;
			}
			HTMLImageCandidate candidate{std::string(url)};
			if (parse_descriptor(p_srcset.substr(descriptor, index - descriptor), candidate)) result.push_back(std::move(candidate));
		}
		return result;
	}

	HTMLImageSelection select_html_image(const litehtml::element& p_image,
		const HTMLImageSourceResolver& p_resolve, const litehtml::media_features& p_media) {
		using namespace litehtml;
		const auto parent = p_image.parent();
		if (parent && parent->tag() == _picture_) {
			for (const auto& source : parent->children()) {
				if (source.get() == &p_image) break;
				if (source->tag() != _source_) continue;
				const char* srcset = source->get_attr("srcset");
				if (!srcset || !*srcset) continue;
				if (const char* type = source->get_attr("type"); type && !supports_image_mime(type)) continue;
				const char* media = source->get_attr("media", "");
				if (*media && !parse_media_query_list(media, p_image.get_document()).check(p_media)) continue;
				auto selection = select_from_attributes(*source, "", p_resolve, p_media);
				if (selection.source.empty()) continue;
				const char* width = source->get_attr("width");
				const char* height = source->get_attr("height");
				if (width || height) selection.dimensions = HTMLImageDimensions{width ? width : "", height ? height : ""};
				return selection;
			}
		}
		return select_from_attributes(p_image, p_image.get_attr("src", ""), p_resolve, p_media);
	}
}
