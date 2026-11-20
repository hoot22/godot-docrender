#include "html_svg_text.h"
#include "html_svg_geometry.h"

#include "formats/html/backend/litehtml_container.h"
#include "formats/html/text/html_glyph_outline.h"
#include "formats/html/text/html_shaped_text.h"

#include <godot_cpp/classes/text_server_manager.hpp>

#include <litehtml/css_parser.h>
#include <litehtml/document.h>
#include <litehtml/element.h>
#include <litehtml/html.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		enum PositionKind { X, Y, DX, DY, ROTATE, POSITION_COUNT };
		using Position = std::array<std::optional<double>, POSITION_COUNT>;

		std::vector<double> position_list(const element& p_node, const char* p_name, double p_basis, bool p_rotation) {
			const char* value = p_node.get_attr(p_name);
			if (!value) return {};
			const auto tokens = litehtml::normalize(value, f_componentize | f_remove_whitespace);
			std::vector<double> values;
			bool may_separate = false;
			for (const auto& token : tokens) {
				if (token.ch == ',') {
					if (!may_separate) return {};
					may_separate = false;
					continue;
				}
				if (token.type == dockit::css::NUMBER) {
					if (!std::isfinite(token.n.number)) return {};
					values.push_back(token.n.number);
				} else {
					css_length length;
					if (p_rotation || !length.from_token(token, f_length_percentage | f_math)) return {};
					p_node.get_document()->cvt_units(length, p_node.css().get_font_metrics(), 0);
					const double number = length.calc_percent(p_basis);
					if (!std::isfinite(number)) return {};
					values.push_back(number);
				}
				may_separate = true;
			}
			return may_separate ? std::move(values) : std::vector<double>();
		}

		struct TextScope {
			std::array<std::vector<double>, POSITION_COUNT> positions;
			size_t offset = 0;

			TextScope(const element& p_node, const Vector2& p_basis) : positions{
				position_list(p_node, "x", p_basis.x, false), position_list(p_node, "y", p_basis.y, false),
				position_list(p_node, "dx", p_basis.x, false), position_list(p_node, "dy", p_basis.y, false),
				position_list(p_node, "rotate", 0, true)
			} {}
		};

		struct TextRun {
			const element* style;
			const element* path_owner;
			Position position;
			std::u32string content;
			HTMLTextLayout layout;
			float scale = 1.0f;

			double advance() const { return layout.is_valid() ? layout.width() * scale : 0; }
		};

		class TextRuns {
			const Vector2& basis;
			std::vector<TextScope> scopes;
			std::vector<TextRun> runs;
			bool collapsible_space = false;

			void append(const element& p_style, const String& p_text, const element* p_path_owner) {
				const bool collapses = p_style.css().collapses_white_space();
				for (int64_t i = 0; i < p_text.length(); ++i) {
					char32_t character = p_text[i];
					if (character == U'\t' || character == U'\r' || character == U'\n' || character == U'\f') character = U' ';
					const bool collapsible = collapses && character == U' ';
					if (collapsible && (runs.empty() || collapsible_space)) continue;
					collapsible_space = collapsible;

					Position position;
					for (auto& scope : scopes) {
						for (size_t kind = 0; kind < POSITION_COUNT; ++kind) {
							const auto& values = scope.positions[kind];
							if (scope.offset < values.size()) position[kind] = values[scope.offset];
							else if (kind == ROTATE && !values.empty()) position[kind] = values.back();
						}
						++scope.offset;
					}
					if (p_path_owner) position[Y].reset();
					const bool reposition = position[X] || position[Y] || position[DX] || position[DY];
					if (runs.empty() || reposition || runs.back().position[ROTATE] != position[ROTATE] ||
						runs.back().style != &p_style || runs.back().path_owner != p_path_owner) {
						runs.push_back({ &p_style, p_path_owner, position, {}, {} });
					}
					runs.back().content.push_back(character);
				}
			}

			void collect(const element& p_node, const element* p_path_owner) {
				if (p_node.css().get_display() == display_none) return;
				const bool path = std::string_view(p_node.get_tagName()) == "textpath";
				if (path) p_path_owner = &p_node;
				else scopes.emplace_back(p_node, basis);
				for (const auto& child : p_node.children()) {
					if (child->is_text()) {
						std::string text;
						child->get_text(text);
						append(p_node, String::utf8(text.data(), text.size()), p_path_owner);
					} else {
						const std::string_view tag = child->get_tagName();
						if (tag == "tspan" || (tag == "textpath" && !p_path_owner)) collect(*child, p_path_owner);
					}
				}
				if (!path) scopes.pop_back();
			}

		public:
			explicit TextRuns(const Vector2& p_basis) : basis(p_basis) {}

			std::vector<TextRun> build(const element& p_root) {
				collect(p_root, nullptr);
				if (collapsible_space) {
					runs.back().content.pop_back();
					if (runs.back().content.empty()) runs.pop_back();
				}
				return std::move(runs);
			}
		};

		struct TextPath {
			svg::PathMeasure measure;
			double offset;
		};

		TextPath resolve_text_path(const element& p_text_path, const Vector2& p_basis, double p_tolerance) {
			css::SVGPath path = css::parse_svg_geometry_path(p_text_path.get_attr("path", ""));
			Transform2D matrix;
			std::optional<double> declared_length;
			const bool has_inline_segments = std::any_of(path.begin(), path.end(),
				[](const css::SVGSubpath& subpath) { return !subpath.segments.empty(); });
			if (!has_inline_segments) {
				path.clear();
				const auto document = p_text_path.get_document();
				const auto root = document ? document->root() : nullptr;
				const auto* shape = root ? svg::resolve_fragment(*root,
					p_text_path.get_attr("href", p_text_path.get_attr("xlink:href", ""))) : nullptr;
				if (shape && svg::is_svg(*shape)) {
					path = svg::shape_path(*shape, p_basis, shape->get_tagName());
					matrix = svg::transform_attribute(*shape);
					const auto length = position_list(*shape, "pathlength", 0, true);
					if (length.size() == 1 && length.front() >= 0) declared_length = length.front();
				}
			}
			const bool reverse = std::string_view(p_text_path.get_attr("side", "")) == "right";
			svg::PathMeasure measure(path, matrix, p_tolerance, reverse);
			// Percentages are relative to the actual length. Other distances are calibrated by pathLength.
			const double length = measure.length();
			const double authored_length = declared_length.value_or(length);
			double offset = 0;
			const char* attribute = p_text_path.get_attr("startoffset");
			if (attribute && length > 0) {
				const auto tokens = litehtml::normalize(attribute, f_componentize | f_remove_whitespace);
				const bool zero_length = authored_length == 0;
				const auto value = position_list(p_text_path, "startoffset", zero_length ? length : authored_length, false);
				if (value.size() == 1) {
					offset = value.front();
					if (!zero_length) offset *= length / authored_length;
					else if (tokens.size() == 1 && tokens.front().type != dockit::css::PERCENTAGE && offset != 0) {
						offset = std::copysign(std::numeric_limits<double>::infinity(), offset);
					}
				}
			}
			return { std::move(measure), offset };
		}

		struct GlyphPlacement {
			Vector2 origin;
			const TextPath* path;
			int anchor;
			bool right_to_left;

			std::optional<Transform2D> cluster(double p_start, double p_width, double p_rotation) const {
				if (!path) return Transform2D(p_rotation, origin + Vector2(p_start, 0));
				const double half = p_width * 0.5;
				const double local_middle = origin.x + p_start + half;
				const double middle = path->offset + local_middle;
				const double length = path->measure.length();
				if (length <= 0 || !std::isfinite(middle)) return {};
				if (path->measure.is_closed()) {
					const double window = anchor == 1 ? -length * 0.5 :
						(right_to_left ? anchor == 0 : anchor == 2) ? -length : 0;
					if (local_middle < window || local_middle > window + length) return {};
				} else if (middle < 0 || middle > length) {
					return {};
				}
				const auto sample = path->measure.sample(middle);
				const Vector2 normal(-sample.tangent.y, sample.tangent.x);
				return Transform2D(sample.tangent.angle() + p_rotation,
					sample.point - sample.tangent * half + normal * origin.y);
			}
		};

		void append_glyphs(HTMLClipOutline& r_outline, const TextServer& p_server, const TextRun& p_run,
			const GlyphPlacement& p_placement, double p_tolerance) {
			if (!p_run.layout.is_valid() || p_run.style->css().get_visibility() != visibility_visible) return;
			const auto glyphs = p_server.shaped_text_get_glyphs(p_run.layout.get_rid());
			const double angle = p_run.position[ROTATE].value_or(0) * (std::acos(-1.0) / 180);
			double pen = 0;
			int64_t cluster_end = 0;
			double cluster_pen = 0;
			std::optional<Transform2D> transform;
			for (int64_t i = 0; i < glyphs.size(); ++i) {
				const Dictionary glyph = glyphs[i];
				const RID font = glyph["font_rid"];
				const int64_t index = glyph["index"];
				const int size = glyph["font_size"];
				const int repeat = glyph["repeat"];
				const double advance = glyph["advance"];
				const double spacing = p_run.layout.spacing ? p_run.layout.spacing->glyph_offset(i) : 0.0;
				if (i == cluster_end && p_placement.path) {
					const int64_t count = glyph["count"];
					cluster_end = i + std::clamp<int64_t>(count, 1, glyphs.size() - i);
					cluster_pen = pen;
					double width = 0;
					for (int64_t j = i; j < cluster_end; ++j) {
						const Dictionary member = glyphs[j];
						width += double(member["advance"]) * int(member["repeat"]);
					}
					transform = p_placement.cluster((pen + spacing) * p_run.scale, width * p_run.scale, angle);
				}
				if (font.is_valid() && index != 0 && size > 0 && (!p_placement.path || transform)) {
					const Vector2 offset = glyph["offset"];
					bool reverse = false;
					const auto path = read_glyph_outline(p_server, font, size, index, reverse);
					const auto contours = css::flatten_svg_path(path, p_tolerance / p_run.scale);
					for (int copy = 0; copy < repeat; ++copy) {
						const double glyph_advance = copy * advance;
						const auto matrix = p_placement.path ? *transform :
							*p_placement.cluster((pen + spacing + glyph_advance) * p_run.scale, 0, angle);
						const Vector2 cluster_offset(p_placement.path ? (pen - cluster_pen + glyph_advance) * p_run.scale : 0, 0);
						for (const auto& contour : contours) {
							if (contour.size() < 3) continue;
							std::vector<Vector2> points;
							points.reserve(contour.size());
							for (const auto& point : contour) {
								const Vector2 position = matrix.xform(cluster_offset + (offset + Vector2(point.x, point.y)) * p_run.scale);
								if (!position.is_finite()) break;
								points.push_back(position);
							}
							if (points.size() != contour.size()) continue;
							if (reverse) std::reverse(points.begin(), points.end());
							r_outline.contours.push_back(std::move(points));
						}
					}
				}
				pen += advance * repeat;
			}
		}
	}

	HTMLClipOutline resolve_svg_text_outline(const element& p_text, const Vector2& p_viewport, double p_tolerance) {
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		const auto document = p_text.get_document();
		auto* container = document ? dynamic_cast<LiteHTMLContainer*>(document->container()) : nullptr;
		HTMLClipOutline result;
		if (server.is_null() || !container) return result;

		auto runs = TextRuns(p_viewport).build(p_text);
		for (auto& run : runs) {
			const auto& style = run.style->css();
			run.layout = container->shape_text_for_outline(style.get_font(), String(run.content.c_str()), style.is_right_to_left(), run.scale);
		}
		Vector2 pen;
		const element* path_owner = nullptr;
		std::optional<TextPath> path;
		for (size_t first = 0; first < runs.size();) {
			const auto& start = runs[first];
			if (start.path_owner != path_owner) {
				if (path && path->measure.length() > 0) pen = path->measure.end();
				path_owner = start.path_owner;
				path.reset();
				if (path_owner) {
					path = resolve_text_path(*path_owner, p_viewport, p_tolerance);
					pen = Vector2();
				}
			}
			const auto& style = start.style->css();
			const double direction = style.is_right_to_left() ? -1.0 : 1.0;
			size_t end = first + 1;
			double advance = direction * start.advance();
			while (end < runs.size() && runs[end].path_owner == path_owner && !runs[end].position[X] && !runs[end].position[Y]) {
				advance += direction * runs[end].advance() + runs[end].position[DX].value_or(0);
				++end;
			}
			const int anchor = style.get_text_anchor();
			pen.x = start.position[X].value_or(pen.x) - advance * (anchor == 1 ? 0.5 : anchor == 2 ? 1.0 : 0.0);
			pen.y = start.position[Y].value_or(pen.y);
			for (size_t i = first; i < end; ++i) {
				const auto& run = runs[i];
				pen += Vector2(run.position[DX].value_or(0), run.position[DY].value_or(0));
				const Vector2 origin = pen + Vector2(direction < 0 ? -run.advance() : 0, 0);
				append_glyphs(result, *server.ptr(), run,
					{ origin, path ? &*path : nullptr, anchor, direction < 0 }, p_tolerance);
				pen.x += direction * run.advance();
			}
			first = end;
		}
		return result;
	}
}
