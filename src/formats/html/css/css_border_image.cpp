#include "css_border_image.h"

#include "css_image_value.h"
#include "css_value_parser.h"
#include <litehtml/html.h>
#include "formats/html/css/css_math.h"

using namespace litehtml;

namespace {
	template<class T>
	void expand_sides(std::array<T, 4>& r_sides, size_t count) {
		if (count < 4) r_sides[3] = r_sides[count > 1 ? 1 : 0];
		if (count < 3) r_sides[2] = r_sides[0];
		if (count < 2) r_sides[1] = r_sides[0];
	}

	std::optional<css_border_image_source> parse_source(const css_token& token, const string& baseurl,
		document_container* container) {
		if (const auto parsed = parse_css_image(token, container)) return css_border_image_source{ *parsed, baseurl };
		return std::nullopt;
	}

	std::optional<css_border_image_slice::Side> parse_slice_side(const css_token& token) {
		using Side = css_border_image_slice::Side;
		if (token.type == NUMBER || token.type == PERCENTAGE) {
			if (token.n.number < 0 || !std::isfinite(token.n.number)) return std::nullopt;
			return Side{ token.n.number, token.type == PERCENTAGE };
		}
		if (const auto number = evaluate_css_scalar_math(token, css_math_scalar::number))
			return Side{ std::max(0.0, *number), false };
		if (const auto percent = evaluate_css_scalar_math(token, css_math_scalar::percentage, 100.0))
			return Side{ std::max(0.0, *percent), true };
		return std::nullopt;
	}

	std::optional<css_border_image_slice> parse_slice(const css_token_vector& tokens, size_t& index) {
		css_border_image_slice result;
		size_t count = 0;
		// The optional fill keyword may precede or follow the complete list.
		if (tokens[index].ident() == "fill") {
			result.fill = true;
			++index;
		}
		while (index < tokens.size() && count < result.sides.size()) {
			const auto side = parse_slice_side(tokens[index]);
			if (!side) break;
			result.sides[count++] = *side;
			++index;
		}
		if (count == 0) return std::nullopt;
		if (!result.fill && index < tokens.size() && tokens[index].ident() == "fill") {
			result.fill = true;
			++index;
		}
		expand_sides(result.sides, count);
		return result;
	}

	std::optional<css_border_image_offsets> parse_offsets(const css_token_vector& tokens, size_t& index, bool width) {
		css_border_image_offsets result;
		size_t count = 0;
		while (index < tokens.size() && count < result.sides.size()) {
			const auto& token = tokens[index];
			css_length length;
			if (token.type == NUMBER) {
				if (token.n.number < 0 || !std::isfinite(token.n.number)) break;
				result.sides[count] = static_cast<double>(token.n.number);
			}
			else if (const auto number = evaluate_css_scalar_math(token, css_math_scalar::number)) {
				result.sides[count] = std::max(0.0, *number);
			}
			else if (width && token.ident() == "auto") {
				result.sides[count] = css_length::predef_value(0);
			}
			else if (length.from_token(token, (width ? f_length_percentage : f_length) | f_positive | f_math)) {
				result.sides[count] = std::move(length);
			}
			else break;
			++count;
			++index;
		}
		if (count == 0) return std::nullopt;
		expand_sides(result.sides, count);
		return result;
	}

	std::optional<border_image_tiling> parse_tiling(const css_token& token) {
		const auto ident = token.ident();
		if (ident == "stretch") return border_image_tiling::stretch;
		if (ident == "repeat") return border_image_tiling::repeat;
		if (ident == "round") return border_image_tiling::round;
		if (ident == "space") return border_image_tiling::space;
		return std::nullopt;
	}

	std::optional<css_border_image_repeat> parse_repeat(const css_token_vector& tokens, size_t& index) {
		const auto first = parse_tiling(tokens[index]);
		if (!first) return std::nullopt;
		++index;
		const auto second = index < tokens.size() ? parse_tiling(tokens[index]) : std::nullopt;
		if (second) ++index;
		return css_border_image_repeat{ *first, second.value_or(*first) };
	}
}

void css_value_parser::parse_border_image(string_id name, const css_token_vector& tokens, const string& baseurl,
	bool important, document_container* container) {
	const auto source_id = _id("border-image-source");
	const auto slice_id = _id("border-image-slice");
	const auto width_id = _id("border-image-width");
	const auto outset_id = _id("border-image-outset");
	const auto repeat_id = _id("border-image-repeat");
	size_t index = 0;

	if (name == source_id) {
		if (tokens.size() == 1)
			if (const auto source = parse_source(tokens[0], baseurl, container))
				add_parsed_property(name, property_value(*source, important));
		return;
	}
	if (name == slice_id) {
		if (const auto slice = parse_slice(tokens, index); slice && index == tokens.size())
			add_parsed_property(name, property_value(*slice, important));
		return;
	}
	if (name == width_id || name == outset_id) {
		if (const auto offsets = parse_offsets(tokens, index, name == width_id); offsets && index == tokens.size())
			add_parsed_property(name, property_value(*offsets, important));
		return;
	}
	if (name == repeat_id) {
		if (const auto repeat = parse_repeat(tokens, index); repeat && index == tokens.size())
			add_parsed_property(name, property_value(*repeat, important));
		return;
	}

	std::optional<css_border_image_source> source;
	std::optional<css_border_image_slice> slice;
	std::optional<css_border_image_offsets> width, outset;
	std::optional<css_border_image_repeat> repeat;
	const auto slash = [&]() { return index < tokens.size() && tokens[index].ch == '/'; };
	while (index < tokens.size()) {
		if (!source && (source = parse_source(tokens[index], baseurl, container))) {
			++index;
		}
		else if (!repeat && (repeat = parse_repeat(tokens, index))) {
		}
		else if (!slice && (slice = parse_slice(tokens, index))) {
			if (!slash()) continue;
			++index;
			width = parse_offsets(tokens, index, true);
			if (slash()) {
				++index;
				outset = parse_offsets(tokens, index, false);
				if (!outset) return;
			}
			else if (!width) return;
		}
		else return;
	}
	if (source) add_parsed_property(source_id, property_value(*source, important));
	if (slice) add_parsed_property(slice_id, property_value(*slice, important));
	if (width) add_parsed_property(width_id, property_value(*width, important));
	if (outset) add_parsed_property(outset_id, property_value(*outset, important));
	if (repeat) add_parsed_property(repeat_id, property_value(*repeat, important));
}
