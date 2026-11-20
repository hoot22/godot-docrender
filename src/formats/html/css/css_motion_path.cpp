#include "css_motion_path.h"

#include "css_math.h"
#include "css_position_values.h"
#include "css_syntax_util.h"

#include <algorithm>
#include <cmath>

namespace litehtml {
	namespace {
		css_token_vector subrange(const css_token_vector& p_tokens, size_t p_start, size_t p_end) {
			return { p_tokens.begin() + p_start, p_tokens.begin() + p_end };
		}

		std::optional<css_offset_ray> parse_ray(const css_token_vector& p_tokens) {
			css_offset_ray result;
			std::optional<double> angle;
			bool size_set = false;
			for (int i = 0; i < static_cast<int>(p_tokens.size());) {
				const auto& token = p_tokens[i++];
				const auto keyword = token.ident();
				if (keyword == "at") {
					css_image_position position;
					if (result.origin || !parse_image_position(p_tokens, i, position)) return {};
					result.origin = position;
				} else if (keyword == "contain") {
					if (result.contain) return {};
					result.contain = true;
				} else if (const auto value = parse_css_angle(token)) {
					if (angle) return {};
					angle = value;
				} else {
					using Size = css_offset_ray::size;
					if (size_set) return {};
					if (keyword == "closest-side") result.extent = Size::closest_side;
					else if (keyword == "farthest-side") result.extent = Size::farthest_side;
					else if (keyword == "closest-corner") result.extent = Size::closest_corner;
					else if (keyword == "farthest-corner") result.extent = Size::farthest_corner;
					else if (keyword == "sides") result.extent = Size::sides;
					else return {};
					size_set = true;
				}
			}
			if (!angle) return {};
			result.angle = *angle;
			return result;
		}

		bool parse_distance_rotation(const css_token_vector& p_tokens, size_t p_start, size_t p_end, css_motion_path& r_result) {
			if (p_start == p_end) return true;
			css_length distance;
			if (distance.from_token(p_tokens[p_start], f_length_percentage | f_math)) {
				const auto rotation = p_start + 1 == p_end ? std::optional<css_offset_rotation>(css_offset_rotation{}) :
					parse_offset_rotation(subrange(p_tokens, p_start + 1, p_end));
				if (rotation) {
					r_result.distance = distance;
					r_result.rotation = *rotation;
					return true;
				}
			}
			if (const auto rotation = parse_offset_rotation(subrange(p_tokens, p_start, p_end))) {
				r_result.rotation = *rotation;
				return true;
			}
			if (p_end - p_start > 1 && distance.from_token(p_tokens[p_end - 1], f_length_percentage | f_math)) {
				if (const auto rotation = parse_offset_rotation(subrange(p_tokens, p_start, p_end - 1))) {
					r_result.distance = distance;
					r_result.rotation = *rotation;
					return true;
				}
			}
			return false;
		}
	}

	std::optional<css_offset_position> parse_offset_position(const css_token_vector& p_tokens, bool p_accept_normal) {
		if (p_tokens.size() == 1) {
			const auto keyword = p_tokens.front().ident();
			if (keyword == "auto") return css_offset_position{};
			if (p_accept_normal && keyword == "normal") return css_offset_position{ css_offset_position::mode::normal, {} };
		}
		css_offset_position position;
		position.kind = css_offset_position::mode::explicit_position;
		int index = 0;
		if (!parse_image_position(p_tokens, index, position.position) || index != static_cast<int>(p_tokens.size())) return {};
		return position;
	}

	std::optional<css_offset_rotation> parse_offset_rotation(const css_token_vector& p_tokens) {
		if (p_tokens.empty() || p_tokens.size() > 2) return {};
		std::optional<double> angle;
		std::string_view direction;
		for (const auto& token : p_tokens) {
			const auto keyword = token.ident();
			if ((keyword == "auto" || keyword == "reverse") && direction.empty()) {
				direction = keyword == "auto" ? "auto" : "reverse";
			} else {
				if (angle) return {};
				angle = parse_css_angle(token);
				if (!angle) return {};
			}
		}
		return css_offset_rotation{ !direction.empty(), angle.value_or(0) + (direction == "reverse" ? std::acos(-1.0) : 0) };
	}

	std::optional<css_offset_path> parse_offset_path(const css_token_vector& p_tokens, const std::string& p_base) {
		if (p_tokens.empty() || p_tokens.size() > 2) return {};
		if (p_tokens.size() == 1 && p_tokens.front().ident() == "none") return css_offset_path{};
		css_offset_path result;
		const css_token* path = nullptr;
		bool has_box = false;
		for (const auto& token : p_tokens) {
			if (const auto box = parse_css_shape_box(token)) {
				if (has_box || *box == css_shape_box::margin) return {};
				has_box = true;
				result.box = *box;
			} else {
				if (path) return {};
				path = &token;
			}
		}
		const auto function = path && path->type == CV_FUNCTION ? dockit::css::detail::lowcase(path->name) : std::string();
		if (function == "ray") {
			result.ray = parse_ray(path->value);
			return result.ray ? std::optional<css_offset_path>(result) : std::nullopt;
		}
		if (function == "path" && path->value.size() != 1) return {};
		const auto shape = parse_css_clip_path(path ? css_token_vector{*path} : p_tokens, p_base);
		if (!shape || !*shape) return {};
		result.shape = *shape;
		return result;
	}

	std::optional<css_motion_path> parse_motion_shorthand(const css_token_vector& p_tokens, const std::string& p_base) {
		const auto slash = std::find_if(p_tokens.begin(), p_tokens.end(), [](const css_token& token) { return token.ch == '/'; });
		const size_t end = slash - p_tokens.begin();
		if (end == 0) return {};
		css_motion_path defaults;
		if (slash != p_tokens.end()) {
			const auto anchor = parse_offset_position(subrange(p_tokens, end + 1, p_tokens.size()), false);
			if (!anchor) return {};
			defaults.anchor = *anchor;
		}
		for (size_t start = 0; start <= std::min<size_t>(4, end); ++start) {
			auto result = defaults;
			if (start != 0) {
				const auto position = parse_offset_position(subrange(p_tokens, 0, start), true);
				if (!position) continue;
				result.position = *position;
			}
			if (start == end) return result;
			for (size_t count = 1; count <= 2 && start + count <= end; ++count) {
				const auto path = parse_offset_path(subrange(p_tokens, start, start + count), p_base);
				if (!path || !parse_distance_rotation(p_tokens, start + count, end, result)) continue;
				result.path = *path;
				return result;
			}
		}
		return {};
	}
}
