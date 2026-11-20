#include "css_position_values.h"

#include <algorithm>

namespace litehtml {
	namespace {
		constexpr int POSITION_LENGTH = f_length_percentage | f_math;
		enum class Axis { any, horizontal, vertical };
		enum class Family { physical, logical, generic };

		struct Component {
			css_axis_position position;
			Axis axis = Axis::any;
			Family family = Family::physical;
			bool is_length = false;
			bool is_center = false;
		};

		std::optional<Component> parse_component(const css_token& p_token) {
			using Edge = css_position_edge;
			const auto keyword = p_token.ident();
			if (keyword == "center") return Component{ { css_length(50, css_units_percentage) }, Axis::any, Family::physical, false, true };

			if (keyword == "left") return Component{ {}, Axis::horizontal };
			if (keyword == "right") return Component{ { {}, Edge::far }, Axis::horizontal };
			if (keyword == "top") return Component{ {}, Axis::vertical };
			if (keyword == "bottom") return Component{ { {}, Edge::far }, Axis::vertical };
			if (keyword == "x-start") return Component{ { {}, Edge::flow_start }, Axis::horizontal };
			if (keyword == "x-end") return Component{ { {}, Edge::flow_end }, Axis::horizontal };
			if (keyword == "y-start") return Component{ { {}, Edge::flow_start }, Axis::vertical };
			if (keyword == "y-end") return Component{ { {}, Edge::flow_end }, Axis::vertical };

			if (keyword == "inline-start") return Component{ { {}, Edge::flow_start }, Axis::horizontal, Family::logical };
			if (keyword == "inline-end") return Component{ { {}, Edge::flow_end }, Axis::horizontal, Family::logical };
			if (keyword == "block-start") return Component{ { {}, Edge::flow_start }, Axis::vertical, Family::logical };
			if (keyword == "block-end") return Component{ { {}, Edge::flow_end }, Axis::vertical, Family::logical };
			if (keyword == "start") return Component{ { {}, Edge::flow_start }, Axis::any, Family::generic };
			if (keyword == "end") return Component{ { {}, Edge::flow_end }, Axis::any, Family::generic };

			Component component;
			component.is_length = true;
			if (!component.position.offset.from_token(p_token, POSITION_LENGTH)) return std::nullopt;
			return component;
		}

		std::optional<css_image_position> pair_components(const Component& p_first, const Component& p_second) {
			if (!p_first.is_center && !p_second.is_center && p_first.family != p_second.family) return std::nullopt;
			if (p_first.family == Family::generic || p_second.family == Family::generic) {
				// Unqualified start/end values are block then inline.
				return css_image_position{ p_second.position, p_first.position };
			}
			if (p_first.axis != Axis::any && p_first.axis == p_second.axis) return std::nullopt;
			const bool swapped = p_first.axis == Axis::vertical || p_second.axis == Axis::horizontal;
			if (swapped && (p_first.is_length || p_second.is_length)) return std::nullopt;
			return swapped ? css_image_position{ p_second.position, p_first.position } :
				css_image_position{ p_first.position, p_second.position };
		}

		std::optional<css_image_position> parse_components(const css_token_vector& p_tokens, int p_start, int p_count) {
			const auto first = parse_component(p_tokens[p_start]);
			if (!first) return std::nullopt;
			if (p_count == 1) {
				if (first->family == Family::generic) return std::nullopt;
				css_image_position result;
				(first->axis == Axis::vertical ? result.y : result.x) = first->position;
				return result;
			}
			if (p_count == 2) {
				const auto second = parse_component(p_tokens[p_start + 1]);
				return second ? pair_components(*first, *second) : std::nullopt;
			}
			const int end = p_start + p_count;
			int index = p_start;
			const auto read_edge = [&]() -> std::optional<Component> {
				if (index == end) return std::nullopt;
				auto value = parse_component(p_tokens[index++]);
				if (!value || value->is_length) return std::nullopt;
				if (p_count == 3 && (value->family != Family::physical ||
					value->position.edge == css_position_edge::flow_start || value->position.edge == css_position_edge::flow_end))
					return std::nullopt;

				css_length offset;
				if (index < end && !value->is_center && offset.from_token(p_tokens[index], POSITION_LENGTH)) {
					value->position.offset = offset;
					++index;
				}
				else if (p_count == 4) return std::nullopt;
				return value;
			};
			const auto a = read_edge();
			const auto b = read_edge();
			if (!a || !b || index != end) return std::nullopt;
			return pair_components(*a, *b);
		}
	}

	bool parse_image_position(const css_token_vector& p_tokens, int& r_index, css_image_position& r_position,
		bool p_accept_three) {
		const int remaining = static_cast<int>(p_tokens.size()) - r_index;
		for (int count = std::min(4, remaining); count > 0; --count) {
			if (count == 3 && !p_accept_three) continue;
			if (const auto position = parse_components(p_tokens, r_index, count)) {
				r_position = *position;
				r_index += count;
				return true;
			}
		}
		return false;
	}

	std::optional<css_axis_position> parse_axis_position(const css_token_vector& p_tokens, bool p_horizontal, bool p_logical) {
		if (p_tokens.empty() || p_tokens.size() > 2) return std::nullopt;
		const auto component = parse_component(p_tokens.front());
		if (!component) return std::nullopt;
		auto result = component->position;
		if (component->is_length && p_logical) result.edge = css_position_edge::flow_start;
		if (!component->is_center && !component->is_length) {
			if (p_logical ? component->family != Family::generic :
				(component->family != Family::physical || component->axis != (p_horizontal ? Axis::horizontal : Axis::vertical)))
				return std::nullopt;
		}
		if (p_tokens.size() == 2 && (component->is_length || component->is_center ||
			!result.offset.from_token(p_tokens[1], POSITION_LENGTH))) return std::nullopt;
		return result;
	}
}
