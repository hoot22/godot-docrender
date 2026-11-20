#include "css_clip_path.h"
#include "css_position_values.h"
#include "css_stylesheet_rules.h"
#include "css_syntax_util.h"

namespace litehtml {
	using dockit::css::detail::is_one_of;
	using dockit::css::detail::lowcase;

	std::optional<css_shape_box> parse_css_shape_box(const css_token& token) {
		const auto keyword = token.ident();
		if (keyword == "margin-box") return css_shape_box::margin;
		if (is_one_of(keyword, "border-box", "stroke-box", "view-box")) return css_shape_box::border;
		if (keyword == "padding-box") return css_shape_box::padding;
		if (is_one_of(keyword, "content-box", "fill-box")) return css_shape_box::content;
		return std::nullopt;
	}

	namespace {
		constexpr int length_flags = f_length_percentage | f_math;
		bool parse_quad(const css_token_vector& tokens, size_t start, size_t end,
			std::array<css_length, 4>& result, int flags) {
			const size_t count = end - start;
			if (count < 1 || count > 4) return false;
			for (size_t i = 0; i < count; ++i)
				if (!result[i].from_token(tokens[start + i], flags)) return false;
			if (count < 2) result[1] = result[0];
			if (count < 3) result[2] = result[0];
			if (count < 4) result[3] = result[1];
			return true;
		}

		bool parse_radii(const css_token_vector& tokens, size_t start, css_border_radius& radii) {
			size_t slash = start;
			while (slash < tokens.size() && tokens[slash].ch != '/') ++slash;
			std::array<css_length, 4> x, y;
			if (!parse_quad(tokens, start, slash, x, length_flags | f_positive)) return false;
			if (slash == tokens.size()) y = x;
			else if (!parse_quad(tokens, slash + 1, tokens.size(), y, length_flags | f_positive)) return false;
			radii.top_left_x = x[0];
			radii.top_left_y = y[0];
			radii.top_right_x = x[1];
			radii.top_right_y = y[1];
			radii.bottom_right_x = x[2];
			radii.bottom_right_y = y[2];
			radii.bottom_left_x = x[3];
			radii.bottom_left_y = y[3];
			return true;
		}

		bool parse_rectangle(const css_token_vector& tokens, css_clip_path& result) {
			size_t round = 0;
			while (round < tokens.size() && tokens[round].ident() != "round") ++round;
			if (result.shape == css_clip_shape::inset) {
				if (!parse_quad(tokens, 0, round, result.dimensions, length_flags)) return false;
			} else {
				if (round != 4) return false;
				for (size_t i = 0; i < 4; ++i) {
					const int flags = length_flags | (result.shape == css_clip_shape::xywh && i >= 2 ? f_positive : 0);
					if (!result.dimensions[i].from_token(tokens[i], flags, result.shape == css_clip_shape::rect ? "auto" : ""))
						return false;
				}
			}
			return round == tokens.size() || parse_radii(tokens, round + 1, result.radii);
		}

		bool parse_ellipse(const css_token_vector& tokens, css_clip_path& result) {
			size_t at = 0;
			while (at < tokens.size() && tokens[at].ident() != "at") ++at;
			const size_t radius_count = result.shape == css_clip_shape::circle ? 1 : 2;
			if (at != 0 && at != radius_count) return false;
			for (size_t i = 0; i < radius_count; ++i) {
				if (at == 0) result.dimensions[i].predef(0);
				else if (!result.dimensions[i].from_token(tokens[i], length_flags | f_positive, "closest-side;farthest-side"))
					return false;
			}
			if (at == tokens.size()) return true;
			result.center_explicit = true;
			int index = static_cast<int>(at + 1);
			return parse_image_position(tokens, index, result.center) && index == static_cast<int>(tokens.size());
		}

		bool parse_polygon(const css_token_vector& tokens, css_clip_path& result) {
			if (tokens.empty()) return false;
			size_t index = 0;
			const auto fill = tokens.front().ident();
			if (is_one_of(fill, "nonzero", "evenodd")) {
				result.even_odd = fill == "evenodd";
				++index;
			}
			if (index < tokens.size() && tokens[index].ident() == "round") {
				++index;
				if (index == tokens.size() ||
					!result.polygon_radius.from_token(tokens[index++], f_length | f_math | f_positive)) return false;
			}
			if (index != 0 && (index == tokens.size() || tokens[index++].ch != ',')) return false;
			for (;;) {
				if (tokens.size() - index < 2) return false;
				std::array<css_length, 2> vertex;
				if (!vertex[0].from_token(tokens[index], length_flags) ||
					!vertex[1].from_token(tokens[index + 1], length_flags)) return false;
				result.vertices.push_back(std::move(vertex));
				index += 2;
				if (index == tokens.size()) return true;
				if (tokens[index++].ch != ',') return false;
			}
		}

		bool parse_path(const css_token_vector& tokens, css_clip_path& result) {
			size_t index = 0;
			if (tokens.size() == 3) {
				const auto fill = tokens.front().ident();
				if (!is_one_of(fill, "evenodd", "nonzero") || tokens[1].ch != ',') return false;
				result.even_odd = fill == "evenodd";
				index = 2;
			} else if (tokens.size() != 1) {
				return false;
			}
			if (tokens[index].type != STRING) return false;
			auto path = dockit::css::parse_svg_path(tokens[index].str);
			if (!path) return false;
			result.svg_path = std::make_shared<const dockit::css::SVGPath>(std::move(*path));
			return true;
		}

		bool parse_shape(const css_token& token, css_clip_path& result) {
			if (token.type != CV_FUNCTION) return false;
			const auto name = lowcase(token.name);
			if (name == "inset") result.shape = css_clip_shape::inset;
			else if (name == "rect") result.shape = css_clip_shape::rect;
			else if (name == "xywh") result.shape = css_clip_shape::xywh;
			else if (name == "circle") result.shape = css_clip_shape::circle;
			else if (name == "ellipse") result.shape = css_clip_shape::ellipse;
			else if (name == "polygon") {
				result.shape = css_clip_shape::polygon;
				return parse_polygon(token.value, result);
			}
			else if (name == "path") {
				result.shape = css_clip_shape::path;
				return parse_path(token.value, result);
			}
			else if (name == "shape") {
				result.shape = css_clip_shape::shape;
				result.shape_path = dockit::css::parse_shape_path(token.value);
				if (!result.shape_path) return false;
				result.even_odd = result.shape_path->even_odd;
				return true;
			}
			else return false;
			return is_one_of(result.shape, css_clip_shape::circle, css_clip_shape::ellipse) ?
				parse_ellipse(token.value, result) : parse_rectangle(token.value, result);
		}
	}

	std::optional<css_clip_path_ref> parse_css_clip_path(const css_token_vector& tokens, const std::string& base_url) {
		if (tokens.empty() || tokens.size() > 2) return std::nullopt;
		if (tokens.size() == 1 && tokens[0].ident() == "none") return css_clip_path_ref{};
		css_clip_path result;
		if (tokens.size() == 1 && dockit::css::parse_url_token(tokens.front(), result.url)) {
			result.shape = css_clip_shape::reference;
			result.base_url = base_url;
			return std::make_shared<const css_clip_path>(std::move(result));
		}
		bool has_box = false;
		for (const auto& token : tokens) {
			if (const auto box = parse_css_shape_box(token)) {
				if (has_box) return std::nullopt;
				result.box = *box;
				has_box = true;
			} else if (result.shape != css_clip_shape::box || !parse_shape(token, result)) {
				return std::nullopt;
			}
		}
		return std::make_shared<const css_clip_path>(std::move(result));
	}
}
