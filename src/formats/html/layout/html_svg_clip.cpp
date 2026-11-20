#include "html_svg_clip.h"
#include "html_geometry.h"
#include "html_svg_text.h"
#include "html_svg_geometry.h"

#include "formats/html/css/css_svg_path.h"

#include <litehtml/document.h>
#include <litehtml/element.h>
#include <litehtml/html.h>

#include <algorithm>
#include <cmath>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		class SVGClipResolver {
			static constexpr size_t MAX_NESTED_SOURCES = 24; // At most two pending shader operands per source
			static constexpr size_t MAX_RESOLVED_SOURCES = 1024;
			const Transform2D& visual_matrix;
			std::vector<const element*> active_sources;
			size_t resolved_sources = 0;

			void append_clip(std::vector<HTMLClipPolygon>& r_clips, const element& p_owner,
				const Rect2& p_box, const Vector2& p_basis, const Transform2D& p_matrix) {
				const auto& path = p_owner.css().get_clip_path();
				if (!path) return;
				if (path->shape == css_clip_shape::reference) {
					if (auto clip = resolve(p_owner, path->url, path->base_url, p_box, p_basis, p_matrix)) {
						r_clips.push_back(std::move(*clip));
					}
				} else {
					r_clips.push_back(svg_basic_clip_polygon(*path, p_box, p_matrix, visual_matrix));
				}
			}

			std::optional<HTMLClipOutline> resolve_child(const element& p_child, const element& p_root,
				const Vector2& p_basis, const Transform2D& p_matrix) {
				if (p_child.css().get_display() == display_none ||
					(p_child.css().get_visibility() != visibility_visible && std::string_view(p_child.get_tagName()) != "text")) return {};
				const element* shape = &p_child;
				Transform2D use_matrix = p_matrix * svg::transform_attribute(p_child);
				Transform2D instance;
				if (std::string_view(p_child.get_tagName()) == "use") {
					shape = svg::resolve_fragment(p_root, p_child.get_attr("href", p_child.get_attr("xlink:href", "")));
					if (!shape || !svg::is_svg(*shape)) return {};
					const Vector2 offset(svg::length_attribute(p_child, "x", p_basis.x), svg::length_attribute(p_child, "y", p_basis.y));
					use_matrix *= Transform2D(0, offset);
					instance = svg::transform_attribute(*shape);
				}
				const Transform2D transform = use_matrix * instance;
				if (!transform.is_finite() || transform.determinant() == 0) return {};
				const Transform2D visual = visual_matrix * transform;
				const double scale = std::max(1.0, std::hypot(double(visual[0].length()), double(visual[1].length())));
				HTMLClipPolygon local;
				auto& outline = local.outlines.emplace_back();
				if (std::string_view(shape->get_tagName()) == "text") {
					outline = resolve_svg_text_outline(*shape, p_basis, 0.2 / scale);
				} else {
					const auto path = svg::shape_path(*shape, p_basis, shape->get_tagName());
					outline.even_odd = shape->css().uses_even_odd_clip_rule();
					for (const auto& source_contour : css::flatten_svg_path(path, 0.2 / scale)) {
						if (source_contour.size() < 3) continue;
						auto& contour = outline.contours.emplace_back();
						contour.reserve(source_contour.size());
						for (const auto& vertex : source_contour) {
							const Vector2 point(vertex.x, vertex.y);
							if (!point.is_finite()) return {};
							contour.push_back(point);
						}
					}
				}
				if (outline.contours.empty()) return {};
				auto mapped = local.transformed(transform);
				if (mapped.outlines.empty()) return {};
				auto& result = mapped.outlines.front();
				if (shape->css().get_clip_path()) append_clip(result.clips, *shape, local.bounds(), p_basis, transform);
				if (shape != &p_child && p_child.css().get_clip_path()) {
					append_clip(result.clips, p_child, local.transformed(instance).bounds(), p_basis, use_matrix);
				}
				return std::move(result);
			}

		public:
			explicit SVGClipResolver(const Transform2D& p_visual_matrix) : visual_matrix(p_visual_matrix) {}

			std::optional<HTMLClipPolygon> resolve(const element& p_target, std::string_view p_url,
				const std::string& p_css_base, const Rect2& p_box, const Vector2& p_basis, const Transform2D& p_matrix) {
				const auto fragment = p_url.find('#');
				if (fragment == std::string_view::npos || fragment + 1 == p_url.size() ||
					active_sources.size() >= MAX_NESTED_SOURCES || resolved_sources >= MAX_RESOLVED_SOURCES) return {};
				auto document = svg::reference_document(p_target, p_url, p_css_base);
				const auto root = document ? document->root() : nullptr;
				const auto* source = root ? svg::resolve_fragment(*root, p_url.substr(fragment)) : nullptr;
				if (!source || std::string_view(source->get_tagName()) != "clippath" || !svg::is_svg(*source) ||
					std::find(active_sources.begin(), active_sources.end(), source) != active_sources.end()) return {};
				++resolved_sources;
				active_sources.push_back(source);
				const bool normalized = std::string_view(source->get_attr("clippathunits", "")) == "objectBoundingBox";
				const Vector2 basis = normalized ? Vector2(1, 1) : p_basis;
				const Transform2D units = normalized ?
					Transform2D(Vector2(p_box.size.x, 0), Vector2(0, p_box.size.y), p_box.position) : Transform2D();
				const Transform2D matrix = p_matrix * units * svg::transform_attribute(*source);
				HTMLClipPolygon result;
				for (const auto& child : source->children()) {
					if (auto outline = resolve_child(*child, *root, basis, matrix)) {
						result.outlines.push_back(std::move(*outline));
					}
				}
				// A clip applied to the clipPath resource itself keeps the original target's reference box.
				append_clip(result.clips, *source, p_box, p_basis, p_matrix);
				active_sources.pop_back();
				return result;
			}
		};
	}

	std::optional<HTMLClipPolygon> resolve_svg_clip(const element& p_target, std::string_view p_url,
		const std::string& p_css_base, const Rect2& p_box, const Vector2& p_viewport, const Transform2D& p_visual_matrix) {
		return SVGClipResolver(p_visual_matrix).resolve(p_target, p_url, p_css_base, Rect2(Vector2(), p_box.size),
			p_viewport, Transform2D(0, p_box.position));
	}
}
