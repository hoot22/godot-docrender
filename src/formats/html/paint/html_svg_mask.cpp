#include "html_svg_mask.h"

#include "formats/html/backend/litehtml_container.h"
#include "formats/html/css/css_parser.h"
#include "formats/html/css/css_syntax_util.h"
#include "formats/html/layout/html_plane_transform.h"
#include "formats/html/layout/html_svg_geometry.h"
#include "formats/html/layout/html_svg_text.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <gumbo.h>
#include <litehtml/document.h>
#include <litehtml/element.h>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		const element& source_svg(const element& p_mask) {
			const element* root = &p_mask;
			for (auto parent = p_mask.parent(); parent; parent = parent->parent()) {
				const std::string_view tag(parent->get_tagName());
				if (tag == "foreignobject") break;
				if (tag == "svg") root = parent.get();
			}
			return *root;
		}

		void append_attribute(String& r_svg, const String& p_name, const String& p_value) {
			r_svg += " " + p_name + "=\"" + p_value.xml_escape(true) + "\"";
		}

		String svg_box(const Rect2& p_rect) {
			return String::num_real(p_rect.position.x) + " " + String::num_real(p_rect.position.y) + " " +
				String::num_real(p_rect.size.x) + " " + String::num_real(p_rect.size.y);
		}

		String outline_path(const HTMLClipOutline& p_outline) {
			String path;
			for (const auto& contour : p_outline.contours) {
				if (contour.size() < 3) continue;
				for (std::size_t index = 0; index < contour.size(); ++index) {
					const auto& point = contour[index];
					path += (index == 0 ? "M" : "L") + String::num_real(point.x) + " " + String::num_real(point.y);
				}
				path += "Z";
			}
			return path;
		}

		bool ignored_mask_style(std::string_view p_name) {
			return p_name == "display" || p_name == "opacity" || p_name == "filter" || p_name == "mask-type";
		}

		String mask_style(const std::string& p_style) {
			css::css_parser parser(css::normalize(p_style, css::f_componentize));
			css::raw_declaration::vector declarations;
			css::raw_rule::vector rules;
			parser.consume_style_block_contents(declarations, rules);
			String result;
			for (const auto& declaration : declarations) {
				if (ignored_mask_style(declaration.name)) continue;
				result += String::utf8(declaration.name.c_str()) + ":" +
					String::utf8(css::get_repr(declaration.value, 0, -1, false).c_str()) +
					(declaration.important ? "!important;" : ";");
			}
			return result;
		}

		bool xml_name(std::string_view p_name) {
			return !p_name.empty() && std::all_of(p_name.begin(), p_name.end(), [](char c) {
				return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
					c == '-' || c == '_' || c == ':';
			});
		}

		class MaskDocument {
			static constexpr std::size_t MAX_DEPTH = 128;
			static constexpr std::size_t MAX_NODES = 8192;
			const LiteHTMLContainer& container;
			const element& root;
			const element& source;
			const Vector2 basis;
			const bool normalized;
			std::size_t nodes = 0;
			bool wrote_source = false;
			String svg;

			void attributes(const element& p_node, bool p_mask, bool p_text, bool p_resolve_lengths) {
				if (const auto* attributes = container.get_source_attributes(p_node)) {
					for (const auto& [name, value] : *attributes) {
						if (!xml_name(name) || name == "xmlns" || name.compare(0, 6, "xmlns:") == 0) continue;
						const auto key = css::detail::lowcase(name);
						if (key == "color" || (p_text && key == "d")) continue;
						if (&p_node == &root && (key == "width" || key == "height" || key == "viewbox" ||
							key == "x" || key == "y" || key == "preserveaspectratio")) continue;
						if (p_mask && (ignored_mask_style(key) || key == "maskunits" || key == "maskcontentunits" ||
							key == "x" || key == "y" || key == "width" || key == "height")) continue;
						if ((key == "href" || key == "xlink:href") && (value.empty() || value[0] != '#')) continue;
						const String attribute = String::utf8(name.c_str());
						if (p_mask && key == "style") {
							append_attribute(svg, attribute, mask_style(value));
							continue;
						}
						const auto end = value.find_last_not_of(" \t\r\n\f");
						if (p_resolve_lengths && end != std::string::npos && value[end] == '%') {
							const auto base = length_basis(p_node, key);
							if (base) {
								append_attribute(svg, attribute, String::num_real(svg::length_attribute(p_node, key.c_str(), *base)));
								continue;
							}
						}
						append_attribute(svg, attribute, String::utf8(value.c_str()));
					}
				}
				if (p_mask) {
					// The referenced container inherits paint from its SVG ancestors.
					for (const char* name : { "fill", "fill-opacity", "fill-rule", "stroke", "stroke-width", "stroke-opacity",
							"stroke-linecap", "stroke-linejoin", "stroke-miterlimit", "stroke-dasharray", "stroke-dashoffset" }) {
						if (p_node.get_attr(name)) continue;
						for (auto parent = p_node.parent(); parent; parent = parent->parent()) {
							if (const char* value = parent->get_attr(name)) {
								append_attribute(svg, name, String::utf8(value));
								break;
							}
							if (parent.get() == &root) break;
						}
					}
				}
				const auto color = p_node.css().get_color().to_srgb();
				append_attribute(svg, "color", "#" + Color(color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f).to_html(false));
			}

			std::optional<double> length_basis(const element& p_node, std::string_view p_key) const {
				const std::string_view tag = p_node.get_tagName();
				if (tag == "lineargradient" || tag == "radialgradient" || tag == "pattern" || tag == "filter" ||
					tag.substr(0, 2) == "fe" || tag == "mask" || tag == "clippath" || tag == "svg") return {};
				if (p_key == "x" || p_key == "x1" || p_key == "x2" || p_key == "cx" || p_key == "rx" || p_key == "width")
					return basis.x;
				if (p_key == "y" || p_key == "y1" || p_key == "y2" || p_key == "cy" || p_key == "ry" || p_key == "height")
					return basis.y;
				if (p_key == "r") return std::hypot(basis.x, basis.y) / std::sqrt(2.0);
				return {};
			}

			void append(const element& p_node, std::size_t p_depth, bool p_resolve_lengths = false) {
				if (p_depth >= MAX_DEPTH || ++nodes > MAX_NODES) return;
				const auto* name = p_node.get_tagName();
				const std::string_view lower(name);
				if (!xml_name(lower) || lower == "script" || lower == "foreignobject" || lower == "image" ||
					p_node.is_comment() || is_generated_tag(p_node.tag())) return;
				const bool mask = &p_node == &source;
				const bool text = lower == "text";
				const GumboStringPiece tag_piece{ name, std::strlen(name) };
				const char* normalized_name = gumbo_normalize_svg_tagname(&tag_piece);
				const String tag = mask ? "g" : (text ? "path" : String::utf8(normalized_name ? normalized_name : name));
				svg += "<" + tag;
				attributes(p_node, mask, text, p_resolve_lengths);
				wrote_source |= mask;
				if (text) {
					const auto outline = resolve_svg_text_outline(p_node, basis, normalized ? 0.0001 : 0.1);
					const auto path = outline_path(outline);
					append_attribute(svg, "d", path);
				}
				svg += ">";
				if (lower == "style") {
					std::string text;
					p_node.get_text(text);
					svg += String::utf8(text.data(), text.size()).xml_escape();
				} else if (!text) {
					const bool resolve_lengths = mask || (p_resolve_lengths && lower != "svg");
					for (const auto& child : p_node.children()) append(*child, p_depth + 1, resolve_lengths);
				}
				svg += "</" + tag + ">";
			}

		public:
			MaskDocument(const LiteHTMLContainer& p_container, const element& p_mask, const Vector2& p_basis,
				bool p_normalized) : container(p_container), root(source_svg(p_mask)), source(p_mask),
				basis(p_basis), normalized(p_normalized) {}

			String build(const Rect2& p_view_box, const Vector2i& p_pixels) {
				svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" preserveAspectRatio=\"none\"";
				append_attribute(svg, "width", String::num_int64(p_pixels.x));
				append_attribute(svg, "height", String::num_int64(p_pixels.y));
				append_attribute(svg, "viewBox", svg_box(p_view_box));
				svg += "><defs>";
				append(root, 0);
				if (!wrote_source) return {};
				svg += "</defs><use";
				append_attribute(svg, "href", "#" + String::utf8(source.get_attr("id", "")));
				return svg + "/></svg>";
			}
		};
	}

	std::optional<HTMLSVGMask> rasterize_svg_mask(const element& p_target, std::string_view p_url,
		const std::string& p_css_base, const Rect2& p_target_box, const Rect2& p_ink, const Vector2& p_viewport,
		const HTMLPlaneTransform& p_matrix, css_mask_mode p_mode) {
		const auto fragment = p_url.find('#');
		if (fragment == std::string_view::npos) return {};
		const auto document = svg::reference_document(p_target, p_url, p_css_base);
		const auto root = document ? document->root() : nullptr;
		const auto* source = root ? svg::resolve_fragment(*root, p_url.substr(fragment)) : nullptr;
		if (!source || std::string_view(source->get_tagName()) != "mask" || !svg::is_svg(*source))
			return fragment == 0 ? std::optional<HTMLSVGMask>(HTMLSVGMask{}) : std::nullopt;
		auto* container = dynamic_cast<LiteHTMLContainer*>(document->container());
		if (!container || !p_target_box.has_area() || !p_ink.has_area()) return HTMLSVGMask{};

		const bool object_units = std::string_view(source->get_attr("maskunits", "")) != "userSpaceOnUse";
		const Vector2 region_basis = object_units ? p_target_box.size : p_viewport;
		// Unitless object-bounding-box values are fractions. Percentage values use the same basis.
		const auto region_length = [&](const char* name, double basis, double fallback) {
			return object_units ? svg::length_attribute(*source, name, 1.0, fallback) * basis :
				svg::length_attribute(*source, name, basis, fallback * basis);
		};
		const Rect2 region(p_target_box.position + Vector2(region_length("x", region_basis.x, -0.1), region_length("y", region_basis.y, -0.1)),
			Vector2(region_length("width", region_basis.x, 1.2), region_length("height", region_basis.y, 1.2)));
		const Rect2 box = region.has_area() && region.is_finite() ? region.intersection(p_ink) : Rect2();
		if (!box.has_area()) return HTMLSVGMask{};

		const bool normalized = std::string_view(source->get_attr("maskcontentunits", "")) == "objectBoundingBox";
		const Vector2 basis = normalized ? Vector2(1, 1) : p_viewport;
		const Vector2 units = normalized ? p_target_box.size : Vector2(1, 1);
		const Rect2 view_box((box.position - p_target_box.position) / units, box.size / units);
		const auto visual = p_matrix.affine_at(box.get_center());
		const Vector2 scale(std::max<real_t>(1.0f, visual[0].length()), std::max<real_t>(1.0f, visual[1].length()));
		const Vector2 requested = (box.size * scale).ceil();
		if (!requested.is_finite()) return HTMLSVGMask{};
		const float limit = 4096.0f / std::max(requested.x, requested.y);
		const Vector2i pixels((requested * std::min(1.0f, limit)).ceil().max(Vector2(1, 1)));
		const bool alpha = p_mode == css_mask_mode::alpha ||
			(p_mode == css_mask_mode::match_source && source->css().svg_mask_uses_alpha());
		const auto input = MaskDocument(*container, *source, basis, normalized).build(view_box, pixels);
		if (input.is_empty()) return HTMLSVGMask{};
		Ref<Image> image;
		image.instantiate();
		if (image->load_svg_from_string(input) != OK || image->is_empty()) return HTMLSVGMask{};
		return HTMLSVGMask{ ImageTexture::create_from_image(image), box, !alpha, source->css().svg_interpolates_linear_rgb() };
	}
}
