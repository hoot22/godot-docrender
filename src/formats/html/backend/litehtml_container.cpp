#include "formats/html/elements/html_input_node.h"
#include "formats/html/elements/html_select_node.h"
#include "formats/html/elements/html_textarea_node.h"
#include "litehtml_container.h"
#include "formats/html/elements/html_gauge_node.h"
#include "formats/html/elements/html_audio_node.h"
#include "formats/html/elements/html_canvas_node.h"
#include "formats/html/elements/html_video_node.h"
#include "formats/html/elements/html_output_node.h"
#include "formats/html/elements/html_ruby_node.h"
#include "formats/html/elements/html_details_node.h"
#include "formats/html/elements/html_dialog_node.h"
#include "formats/html/elements/html_stylesheet_node.h"
#include "formats/http_url.h"
#include "formats/html/css/css_animation_state.h"
#include "formats/html/css/css_transition_state.h"

#include "formats/html/dom/html_dom.h"

#include "formats/html/text/html_image_alt_text.h"
#include "formats/html/paint/html_image_blend.h"
#include "formats/html/css/css_image_value.h"
#include "litehtml_document_adapter.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <litehtml/document.h>
#include <litehtml/el_image.h>
#include <litehtml/html_tag.h>
#include <litehtml/render_image.h>

#include <algorithm>
#include <string_view>
#include <utility>

using namespace godot;
using namespace litehtml;

namespace dockit {

	const property_value* LiteHTMLContainer::transition_override(const element* p_element, string_id p_name) const {
		return transition_state ? transition_state->find(p_element, p_name) : nullptr;
	}
	const property_value* LiteHTMLContainer::animation_override(const element* p_element, string_id p_name) const {
		return animation_state ? animation_state->find(p_element, p_name) : nullptr;
	}
	namespace {
		constexpr float CSS_DPI = 96.0f;

		class RawTextElement final : public html_tag {
		public:
			using html_tag::html_tag;
			void parse_attributes() override {}
			void compute_styles(bool) override {
				css_w().set_display(display_none);
			}
		};

		class ImageSourceElement final : public html_tag {
		public:
			using html_tag::html_tag;

			void compute_styles(bool) override {
				css_w().set_display(display_none);
			}
		};

		class FittedImageRenderItem : public render_item_image {
		protected:
			rendered_width _render(pixel_t p_x, pixel_t p_y, const containing_block_context& p_block,
				formatting_context* p_context, bool p_second_pass) override {
				const rendered_width rendered = render_item_image::_render(p_x, p_y, p_block, p_context, p_second_pass);
				if (m_pos.width <= 0.0f || m_pos.height <= 0.0f || static_cast<const el_image&>(*src_el()).get_alt_text()) {
					return rendered;
				}
				position viewport;
				src_el()->get_document()->container()->get_viewport(viewport);
				const pixel_t max_width = std::max(0.0f, std::min<pixel_t>(p_block.width, viewport.width) - content_offset_width());
				const pixel_t max_height = std::max(0.0f, viewport.height - content_offset_height());
				const pixel_t scale = std::min({ 1.0f, max_width / m_pos.width, max_height / m_pos.height });
				m_pos.width *= scale;
				m_pos.height *= scale;
				src_el()->css_w().line_height_w().computed_value = height();
				const pixel_t fitted_width = width();
				return { fitted_width, fitted_width };
			}

		public:
			using render_item_image::render_item_image;

			std::shared_ptr<render_item> clone() override {
				return std::make_shared<FittedImageRenderItem>(src_el());
			}
		};

		class FittedImageElement : public el_image {
		public:
			using el_image::el_image;

			std::shared_ptr<render_item> create_render_item(const std::shared_ptr<render_item>& p_parent) override {
				auto item = std::make_shared<FittedImageRenderItem>(shared_from_this());
				item->parent(p_parent);
				return item;
			}
		};

	} // namespace

	struct HTMLSVGClipDocument {
		std::shared_ptr<HTMLDOM> dom = std::make_shared<HTMLDOM>(nullptr);
		LiteHTMLContainer container;
		document::ptr source;

		explicit HTMLSVGClipDocument(const String& p_base_dir) :
			container(*dom, p_base_dir, {}, false, {}, {}, {}, {}, {}, {}, true) {}
	};

	LiteHTMLContainer::LiteHTMLContainer(HTMLDOM& p_dom, const String& p_base_dir, const Dictionary& p_textures, bool p_fit_images,
		const HTMLFontConfig& p_fonts, const Dictionary& p_local_stylesheets, const Dictionary& p_local_fonts,
		const Dictionary& p_image_dpi, const Dictionary& p_image_orientations, const Dictionary& p_svg_clip_sources,
		bool p_svg_clip_only) :
		dom(p_dom), resources(p_base_dir, p_textures, p_local_stylesheets, p_local_fonts, p_image_dpi, p_image_orientations, p_svg_clip_sources),
		fit_images(p_fit_images), svg_clip_only(p_svg_clip_only), fonts(p_fonts, resources) {
	}

	bool LiteHTMLContainer::set_font_rules(std::vector<litehtml::css_font_face> p_faces,
		std::vector<litehtml::css_font_feature_values_rule> p_feature_values) {
		return fonts.set_font_rules(std::move(p_faces), std::move(p_feature_values));
	}

	litehtml::element::ptr LiteHTMLContainer::create_element(const char* p_tag, const litehtml::string_map& p_attributes,
		const std::shared_ptr<litehtml::document>& p_document) {
		if (std::string_view(p_tag) == "canvas") {
			auto canvas = std::make_shared<HTMLCanvasNode>(p_document);
			for (const auto& [name, value] : p_attributes) canvas->set_attr(name.c_str(), value.c_str());
			if (!p_attributes.empty()) dom.remember_source_attributes(canvas, p_attributes);
			return canvas;
		}
		if (!p_attributes.empty()) {
			// The inner call selects litehtml's normal element type. The outer call applies the attributes.
			const auto element = p_document->create_element(p_tag, {});
			if (element) {
				dom.remember_source_attributes(element, p_attributes);
			}
			return element;
		}
		const std::string_view tag(p_tag);
		if (tag == "base") has_base_element = true;
		if (tag == "ruby") return std::make_shared<HTMLRubyNode>(p_document);
		if (tag == "details") return std::make_shared<HTMLDetailsNode>(p_document);
		if (tag == "dialog") {
			const auto dialog = std::make_shared<HTMLDialogNode>(p_document);
			dom.track_dialog(dialog);
			return dialog;
		}
		if (tag == "output") return std::make_shared<HTMLOutputNode>(p_document);
		if (tag == "template") return dom.create_template_node(p_document);
		if (tag == "video") return std::make_shared<HTMLVideoNode>(p_document);
		if (tag == "audio") return std::make_shared<HTMLAudioNode>(p_document);
		if (tag == "meter" || tag == "progress") return std::make_shared<HTMLGaugeNode>(p_document);
		if (tag == "select") return std::make_shared<HTMLSelectNode>(p_document);
		if (tag == "option") return std::make_shared<HTMLOptionNode>(p_document);
		if (tag == "textarea") return std::make_shared<HTMLTextAreaNode>(p_document);
		if (tag == "input") {
			return std::make_shared<HTMLInputNode>(p_document);
		}
		if (tag == "script") return std::make_shared<RawTextElement>(p_document);
		if (tag == "style" || tag == "link") return std::make_shared<HTMLStylesheetNode>(p_document);
		if (tag == "source") return std::make_shared<ImageSourceElement>(p_document);
		return fit_images && tag == "img" ? std::make_shared<FittedImageElement>(p_document) : nullptr;
	}

	void LiteHTMLContainer::reset_stylesheet_reads() {
		resources.reset_stylesheet_reads();
	}

	void LiteHTMLContainer::refresh_document_base(const litehtml::element::ptr& p_root) {
		if (!has_base_element) return;
		const char* href = nullptr;
		HTMLDOM::visit_elements(p_root, [&](const auto& p_node) {
			if (p_node->tag() == litehtml::_base_ && p_node->is_html_namespace()) href = p_node->get_attr("href");
			return href != nullptr;
		});
		resources.set_document_base_href(href);
	}

	void LiteHTMLContainer::set_css_diagnostics_enabled(bool p_enabled) {
		css_diagnostics_enabled = p_enabled;
		begin_css_diagnostics();
	}

	void LiteHTMLContainer::begin_css_diagnostics() {
		css_diagnostics.clear();
		css_diagnostic_keys.clear();
		element_diagnostics.clear();
		paint.clear_diagnostics();
	}

	void LiteHTMLContainer::set_css_source_kind(litehtml::css_source_kind p_kind) {
		css_source_kind = p_kind;
	}

	TypedArray<Dictionary> LiteHTMLContainer::get_css_diagnostics() const {
		TypedArray<Dictionary> result;
		for (const Dictionary& entry : css_diagnostics) {
			result.append(entry.duplicate());
		}
		return result;
	}

	const std::vector<int64_t>* LiteHTMLContainer::get_element_diagnostics(const element& p_element) const {
		return element_diagnostics.getptr(&p_element);
	}

	const Dictionary& LiteHTMLContainer::get_css_diagnostic(int64_t p_index) const {
		return css_diagnostics[p_index];
	}

	css_source LiteHTMLContainer::get_css_source(const litehtml::string& p_base, bool p_inline) const {
		if (!css_diagnostics_enabled) {
			return {};
		}
		if (p_inline) {
			return { css_source_kind::inline_style, {} };
		}
		if (css_source_kind == css_source_kind::none || p_base.empty()) {
			return { css_source_kind, {} };
		}
		return { css_source_kind::local_stylesheet, std::string(HTMLResources::stylesheet_path(p_base)) };
	}

	void LiteHTMLContainer::report_css_diagnostic(const css_diagnostic& p_diagnostic, const html_tag* p_element) {
		if (!css_diagnostics_enabled) {
			return;
		}
		static constexpr const char* kinds[] = { "unknown_property", "unsupported_value", "not_rendered" };
		static constexpr const char* sources[] = { "", "inline_style", "style_element", "stylesheet", "local_stylesheet" };
		const String kind = kinds[static_cast<int>(p_diagnostic.kind)];
		const String property = String::utf8(p_diagnostic.property.c_str());
		const String value = String::utf8(p_diagnostic.value.c_str());
		const String path = String::utf8(p_diagnostic.source.path.c_str());
		const String source = p_diagnostic.source.kind == css_source_kind::local_stylesheet && is_http_url(path)
			? "remote_stylesheet" : sources[static_cast<int>(p_diagnostic.source.kind)];
		const String key = vformat("%s/%s/%d/%d/%s%s%s", kind, source, property.length(), path.length(), property, path, value);
		const int64_t* existing = css_diagnostic_keys.getptr(key);
		const int64_t index = existing ? *existing : static_cast<int64_t>(css_diagnostics.size());
		if (!existing) {
			css_diagnostic_keys.insert(key, index);
			Dictionary entry;
			entry["kind"] = kind;
			entry["property"] = property;
			entry["value"] = value;
			entry["source"] = source;
			entry["path"] = path;
			css_diagnostics.push_back(entry);
		}
		if (p_element) {
			auto& entries = element_diagnostics[p_element];
			if (std::find(entries.begin(), entries.end(), index) == entries.end()) {
				entries.push_back(index);
			}
		}
	}

	void LiteHTMLContainer::set_viewport(const Vector2i& p_size, const Vector2i& p_media_size) {
		if (viewport_size != p_size || media_viewport_size != p_media_size) svg_clip_documents.clear();
		viewport_size = p_size;
		media_viewport_size = p_media_size;
	}

	bool LiteHTMLContainer::set_preferred_color_scheme(color_scheme p_scheme) {
		return std::exchange(preferred_color_scheme, p_scheme) != p_scheme;
	}

	bool LiteHTMLContainer::set_accessibility_preferences(bool p_reduce_motion, bool p_increase_contrast) {
		const bool changed = prefers_reduced_motion != p_reduce_motion || prefers_contrast_more != p_increase_contrast;
		prefers_reduced_motion = p_reduce_motion;
		prefers_contrast_more = p_increase_contrast;
		return changed;
	}

	document::ptr LiteHTMLContainer::get_svg_clip_document(std::string_view p_url, const char* p_base) {
		if (svg_clip_only) return {};
		const String path = resources.resolve_svg_clip_source(p_url, p_base);
		if (path.is_empty()) return {};
		if (const auto* cached = svg_clip_documents.getptr(path)) return *cached ? (*cached)->source : nullptr;
		const auto text = resources.get_svg_clip_source(path);
		if (!text) {
			svg_clip_documents.insert(path, {});
			return {};
		}
		auto clip = std::make_shared<HTMLSVGClipDocument>(path.get_base_dir());
		clip->container.set_viewport(viewport_size, media_viewport_size);
		clip->source = dockit_editor::parse_svg_clip_source(*text, clip->container);
		const auto source = clip->source;
		svg_clip_documents.insert(path, source ? std::move(clip) : nullptr);
		return source;
	}

	const litehtml::string_map* LiteHTMLContainer::get_source_attributes(const litehtml::element& p_element) const {
		return dom.get_source_attributes(p_element);
	}

	void LiteHTMLContainer::record(litehtml::document& p_document, const Vector2i& p_content_size,
		const HTMLGeometryIndex& p_geometry, const Vector2& p_viewport_gutters) {
		resources.begin_image_paint();
		const litehtml::position bounds(0, 0, std::max(p_content_size.x, viewport_size.x),
			std::max(p_content_size.y, viewport_size.y));
		paint.begin(p_geometry, p_document.root_render().get(), [this](uint_ptr p_font, const String& p_text,
			int64_t p_start, int64_t p_end, const render_item* p_item) {
			if (!p_item) return fonts.shape_text(p_font, p_text);
			return fonts.shape_styled_text(p_font, p_text, p_item->get_text_justification(), p_item->get_placement().width,
				p_start, p_end, p_item->get_punctuation_trim()).layout;
		});
		p_document.draw(0, 0, 0, &bounds);
		const auto& root_render = p_document.root_render();
		if (p_viewport_gutters != Vector2() && root_render && root_render->is_visible()) {
			const auto& root = p_document.root();
			if (const auto* background = root->get_background()) {
				auto canvas = bounds;
				canvas.x -= p_viewport_gutters.x;
				canvas.width += p_viewport_gutters.x + p_viewport_gutters.y;
				canvas.round();
				paint.record_viewport_background(*root_render, [&] {
					background->draw_layers(0, root_render->pos(), root.get(), root_render, *this, [&](background_layer& layer) {
						layer.clip_box = layer.border_box = canvas;
						layer.border_radius = {};
					});
				});
			}
		}
		paint.finish(p_document.root(), [this](const element& p_element, const String& p_text) {
			const auto font = p_element.parent()->css().get_font();
			return font ? fonts.shape_text(font, p_text) : HTMLTextLayout{};
		});
		resources.end_image_paint();
	}

	uint_ptr LiteHTMLContainer::create_font(const font_description& p_description, const litehtml::document*,
		font_metrics* r_metrics) {
		return fonts.create_font(p_description, r_metrics);
	}

	void LiteHTMLContainer::delete_font(uint_ptr p_font) {
		fonts.delete_font(p_font);
	}

	pixel_t LiteHTMLContainer::text_width(const char* p_text, uint_ptr p_font) {
		return fonts.text_width(p_text, p_font);
	}

	pixel_t LiteHTMLContainer::measure_text(const char* p_text, uint_ptr p_font, font_metrics& r_metrics) {
		return fonts.measure_text(String::utf8(p_text), p_font, r_metrics);
	}

	pixel_t LiteHTMLContainer::trimmed_punctuation_width(const char* p_text, uint_ptr p_font, punctuation_trim p_trim) {
		return fonts.trimmed_punctuation_width(p_text, p_font, p_trim);
	}

	text_justify_info LiteHTMLContainer::get_text_justify_info(const char* p_text, uint_ptr p_font, text_justify p_method) {
		const String value = String::utf8(p_text);
		const auto layout = fonts.shape_text(p_font, value);
		return HTMLTextSpacing::get_justification_info(layout, value, p_method);
	}

	void LiteHTMLContainer::draw_text(uint_ptr, const char* p_text, uint_ptr p_font, web_color p_color,
		const litehtml::position& p_position) {
		static const std::vector<box_shadow> no_shadows;
		record_text(p_text, p_font, p_color, p_position, no_shadows);
	}

	void LiteHTMLContainer::draw_styled_text(uint_ptr, const char* p_text, uint_ptr p_font, web_color p_color,
		const litehtml::position& p_position, const element& p_element, text_justify_adjustment p_justification, punctuation_trim p_trim) {
		record_text(p_text, p_font, p_color, p_position, p_element.css().get_text_shadows(), p_element.css().get_underline(),
			p_justification, p_trim);
	}

	void LiteHTMLContainer::record_text(const char* p_text, uint_ptr p_font, web_color p_color,
		const litehtml::position& p_position, const std::vector<box_shadow>& p_shadows, const css_underline& p_underline,
		text_justify_adjustment p_justification, punctuation_trim p_trim) {
		const String value = String::utf8(p_text);
		paint.draw_text(fonts.shape_styled_text(p_font, value, p_justification, p_position.width, 0, -1, p_trim), value.length(), p_color, p_position,
			p_shadows, p_underline);
	}

	std::vector<HTMLTextCluster> LiteHTMLContainer::get_shaped_clusters(uint_ptr p_font, const String& p_text) {
		return fonts.shape_text(p_font, p_text).clusters(p_text);
	}

	void LiteHTMLContainer::draw_text_fragment(const String& p_text, int64_t p_start, int64_t p_end,
		const litehtml::position& p_position, const element& p_element, text_justify_adjustment p_justification, punctuation_trim p_trim) {
		const auto& style = p_element.css();
		paint.draw_text(fonts.shape_styled_text(style.get_font(), p_text, p_justification, p_position.width, p_start, p_end, p_trim),
			p_end - p_start, style.get_color(), p_position, style.get_text_shadows(), style.get_underline());
	}

	void LiteHTMLContainer::draw_automatic_hyphen(const litehtml::position& p_position, const element& p_element,
		text_justify_adjustment p_justification) {
		const auto& style = p_element.css();
		const auto text = String::utf8(style.get_hyphenate_character().c_str());
		paint.draw_text(fonts.shape_styled_text(style.get_font(), text, p_justification, p_position.width),
			0, style.get_color(), p_position, style.get_text_shadows(), style.get_underline());
	}

	pixel_t LiteHTMLContainer::pt_to_px(float p_points) const {
		return p_points * (CSS_DPI / 72.0f);
	}

	pixel_t LiteHTMLContainer::get_default_font_size() const {
		return LiteHTMLFonts::DEFAULT_SIZE;
	}

	const char* LiteHTMLContainer::get_default_font_name() const {
		return fonts.default_family();
	}

	pixel_t LiteHTMLContainer::get_default_fixed_font_size() const {
		return fonts.default_fixed_size();
	}

	void LiteHTMLContainer::draw_list_marker(uint_ptr, const list_marker& p_marker) {
		Ref<Texture2D> texture;
		if (!p_marker.image.empty()) {
			texture = resources.load_texture(p_marker.image.c_str(), p_marker.baseurl);
		}
		paint.draw_list_marker(p_marker, texture);
	}

	void LiteHTMLContainer::import_css(litehtml::string& r_text, const litehtml::string& p_url, litehtml::string& r_css_base) {
		if (svg_clip_only) r_text.clear();
		else resources.import_css(r_text, p_url, r_css_base);
	}

	void LiteHTMLContainer::link(const std::shared_ptr<litehtml::document>& p_doc, const litehtml::element::ptr& p_element) {
		auto* node = dynamic_cast<HTMLStylesheetNode*>(p_element.get());
		const char* href = p_element->get_attr("href");
		if (!node || !href || !is_html_stylesheet_link(p_element->get_attr("rel"))) {
			return;
		}
		const char* type = p_element->get_attr("type", "");
		if (*type && String::utf8(type).get_slice(";", 0).strip_edges().nocasecmp_to("text/css") != 0) {
			return;
		}
		const auto path = resources.resolve_stylesheet_source(String::utf8(href)).utf8();
		if (p_element->get_attr("disabled")) {
			if (node->linked_path.empty()) return;
			if (node->linked_request_path == path.get_data()) {
				p_doc->add_stylesheet(node->linked_text.c_str(), node->linked_path.c_str(), p_element->get_attr("media"),
					p_element->parent().get(), p_element, true);
			} else {
				litehtml::dockit_editor::reset_stylesheet_source(*p_element);
			}
			return;
		}
		litehtml::string css;
		litehtml::string base;
		import_css(css, href, base);
		if (!css.empty() || !base.empty()) {
			if (!node->linked_path.empty() && (node->linked_request_path != path.get_data() ||
				base != node->linked_path || css != node->linked_text)) {
				litehtml::dockit_editor::reset_stylesheet_source(*p_element);
			}
			p_doc->add_stylesheet(css.c_str(), base.c_str(), p_element->get_attr("media"),
				p_element->parent().get(), p_element);
			node->linked_request_path = path.get_data();
			node->linked_path = std::move(base);
			node->linked_text = std::move(css);
		} else if (!node->linked_path.empty()) {
			litehtml::dockit_editor::reset_stylesheet_source(*p_element);
		}
	}

	void LiteHTMLContainer::load_image(const char* p_src, const char* p_css_base, bool, css_image_orientation p_orientation) {
		// Style computation preloads images even when none of their dimensions enter layout.
		if (!svg_clip_only) resources.load_texture(p_src, p_css_base, p_orientation, false);
	}

	bool LiteHTMLContainer::is_image_available(const char* p_source, const char* p_base, css_image_orientation p_orientation) {
		return !svg_clip_only && resources.load_texture(p_source, p_base, p_orientation).is_valid();
	}

	std::shared_ptr<HTMLImageAltText> LiteHTMLContainer::shape_image_alt(const char* p_alt, const html_tag& p_image) {
		const auto& style = p_image.css();
		if (!style.get_font()) return {};
		std::string text;
		bool space = false;
		for (const char ch : std::string_view(p_alt)) {
			if (std::string_view(" \t\r\n\f").find(ch) != std::string_view::npos) {
				space = !text.empty();
				continue;
			}
			if (space) text += ' ';
			space = false;
			text += ch;
		}
		if (text.empty()) return {};
		auto shaped = fonts.shape_text(style.get_font(), String::utf8(text.c_str()));
		if (!shaped.is_valid()) return {};
		return std::make_shared<HTMLImageAltText>(std::move(shaped), style.line_height().computed_value,
			style.get_text_wrap_mode() == text_wrap_wrap);
	}

	void LiteHTMLContainer::draw_image_alt(HTMLImageAltText& p_text, const position& p_box, const html_tag& p_image) {
		const auto& style = p_image.css();
		paint.draw_image_alt(Rect2(p_box.x, p_box.y, p_box.width, p_box.height), p_text.layout(p_box.width),
			p_text.get_line_height(), style.get_color(), style.get_text_align());
	}

	void LiteHTMLContainer::get_image_size(const char* p_src, const char* p_css_base, litehtml::size& r_size,
		css_image_orientation p_orientation) {
		const Ref<Texture2D> texture = svg_clip_only ? Ref<Texture2D>() : resources.load_texture(p_src, p_css_base, p_orientation);
		const Vector2 size = texture.is_valid() ? texture->get_size() : Vector2();
		r_size = litehtml::size(size.x, size.y);
	}

	void LiteHTMLContainer::get_css_image_size(const char* p_src, const char* p_css_base, double p_density, litehtml::size& r_size,
		css_image_orientation p_orientation) {
		if (p_density == 1.0) {
			get_image_size(p_src, p_css_base, r_size, p_orientation);
			return;
		}
		const bool vector_image = resources.is_vector_image(p_src, p_css_base);
		document_container::get_css_image_size(p_src, p_css_base, vector_image ? 1.0 : p_density, r_size, p_orientation);
	}

	double LiteHTMLContainer::get_image_dpi(const char* p_src, const char* p_css_base) {
		return resources.get_image_dpi(p_src, p_css_base);
	}

	void LiteHTMLContainer::draw_image(uint_ptr, const background_layer& p_layer, const std::string& p_url, const std::string& p_css_base) {
		paint.draw_image(p_layer, resources.load_texture(p_url.c_str(), p_css_base.c_str(), p_layer.image_orientation));
	}

	void LiteHTMLContainer::draw_cross_fade(uint_ptr, const background_layer& p_layer, const css_cross_fade& p_mix,
		const std::string& p_css_base, const web_color& p_color, int p_direction) {
		paint.draw_image(p_layer, rasterize_cross_fade(p_layer, p_mix, p_css_base, p_color, p_direction,
			paint.get_image_sampling(), resources));
	}

	void LiteHTMLContainer::draw_solid_fill(uint_ptr, const background_layer& p_layer, const web_color& p_color) {
		paint.draw_solid_fill(p_layer, p_color);
	}

	void LiteHTMLContainer::draw_linear_gradient(uint_ptr, const background_layer& p_layer, const background_layer::linear_gradient& p_gradient) {
		paint.draw_linear_gradient(p_layer, p_gradient);
	}

	void LiteHTMLContainer::draw_radial_gradient(uint_ptr, const background_layer& p_layer, const background_layer::radial_gradient& p_gradient) {
		paint.draw_radial_gradient(p_layer, p_gradient);
	}

	void LiteHTMLContainer::draw_conic_gradient(uint_ptr, const background_layer& p_layer, const background_layer::conic_gradient& p_gradient) {
		paint.draw_conic_gradient(p_layer, p_gradient);
	}

	void LiteHTMLContainer::draw_box_shadows(const html_tag& p_element, const litehtml::position& p_box,
		const borders& p_borders, bool p_inset) {
		const auto& shadows = p_element.css().get_box_shadows();
		if (!shadows.empty()) {
			paint.draw_box_shadows(p_element, p_box, p_borders, p_inset, shadows);
		}
	}

	bool LiteHTMLContainer::draw_border_image(const html_tag& p_element, const borders& p_borders,
		const litehtml::position& p_box, const litehtml::position* p_fragment, bool p_first, bool p_last) {
		const auto& computed = p_element.css();
		const auto& style = computed.get_border_image();
		const auto& value = style.source.value;
		const auto& base = style.source.baseurl;
		const Rect2 area = get_border_image_area(style, to_rect(p_box), p_borders);
		if (!area.is_finite() || !area.has_area()) return false;
		const auto dimensions = get_css_image_natural_size(value, base, *this);
		const Vector2 natural(dimensions.width, dimensions.height);
		Vector2 source_size;
		Ref<Texture2D> texture;
		const bool flip = value.type == image::type_url && value.direction &&
			static_cast<int>(*value.direction) != computed.get_bg().m_direction;
		if (value.type == image::type_url) {
			texture = resources.load_texture(value.url.c_str(), base.c_str(), value.orientation);
			if (texture.is_valid()) source_size = texture->get_size();
		}
		else {
			source_size = natural.x > 0 && natural.y > 0 ? natural : area.size;
			background_layer tile;
			tile.origin_box = litehtml::position(0, 0, source_size.x, source_size.y);
			const auto draw = [&](const css_cross_fade& mix) {
				return rasterize_cross_fade(tile, mix, base, computed.get_color(), computed.get_bg().m_direction,
					paint.get_image_sampling(), resources);
			};
			texture = value.type == image::type_cross_fade ? draw(*value.m_cross_fade) : draw({ { { value, 1.0 } } });
		}
		if (texture.is_null() || source_size.x <= 0 || source_size.y <= 0) return false;
		Rect2 visible = area;
		if (p_fragment) {
			const float left = p_first ? area.position.x : p_fragment->left();
			const float right = p_last ? area.get_end().x : p_fragment->right();
			visible = area.intersection(Rect2(left, area.position.y, right - left, area.size.y));
		}
		paint.draw_border_image(texture, make_border_image_patches(style, p_borders, area, source_size, natural,
			texture->get_size(), flip), visible, flip);
		return true;
	}

	void LiteHTMLContainer::draw_borders(uint_ptr, const borders& p_borders, const litehtml::position& p_position, bool) {
		paint.draw_borders(p_borders, p_position);
	}

	void LiteHTMLContainer::set_clip(const litehtml::position& p_position, const border_radiuses& p_radii) {
		paint.set_clip(p_position, p_radii);
	}

	void LiteHTMLContainer::set_overflow_clip(const render_item& p_item) {
		paint.set_overflow_clip(p_item);
	}

	void LiteHTMLContainer::set_render_item_clip(const litehtml::position& p_position, const border_radiuses& p_radii,
		const render_item& p_item) {
		paint.set_render_item_clip(p_position, p_radii, p_item);
	}

	bool LiteHTMLContainer::begin_render_item(const render_item& p_item, const litehtml::position*& r_clip, bool p_decoration) {
		return paint.begin_render_item(p_item, r_clip, p_decoration);
	}

	void LiteHTMLContainer::end_render_item() {
		paint.end_render_item();
	}

	void LiteHTMLContainer::draw_scrollbars(const render_item& p_item) {
		paint.draw_scrollbars(p_item);
	}

	void LiteHTMLContainer::draw_text_overflow(const render_item& p_item) {
		paint.draw_text_overflow(p_item);
	}

	void LiteHTMLContainer::begin_paint_group(const render_item& p_item) {
		paint.begin_paint_group(p_item);
	}

	void LiteHTMLContainer::end_paint_group() {
		paint.end_paint_group();
	}

	void LiteHTMLContainer::begin_background_blend() {
		paint.begin_background_blend(std::nullopt);
	}

	void LiteHTMLContainer::begin_background_blend_layer(css_blend_mode p_mode) {
		paint.begin_background_blend(p_mode);
	}

	void LiteHTMLContainer::del_clip() {
		paint.del_clip();
	}

	void LiteHTMLContainer::get_viewport(litehtml::position& r_viewport) const {
		r_viewport = litehtml::position(0, 0, viewport_size.x, viewport_size.y);
	}

	void LiteHTMLContainer::get_media_features(media_features& r_media) const {
		r_media.type = print_media ? media_type_print : media_type_screen;
		r_media.width = r_media.device_width = media_viewport_size.x;
		r_media.height = r_media.device_height = media_viewport_size.y;
		r_media.color = 8;
		r_media.resolution = CSS_DPI;
		r_media.preferred_color_scheme = preferred_color_scheme;
		r_media.prefers_reduced_motion = prefers_reduced_motion;
		r_media.prefers_contrast_more = prefers_contrast_more;
		const DisplayServer* display = DisplayServer::get_singleton();
		const bool has_mouse = display && display->has_feature(DisplayServer::FEATURE_MOUSE);
		const bool has_touch = display && display->has_feature(DisplayServer::FEATURE_TOUCHSCREEN);
		r_media.hover = has_mouse;
		r_media.any_hover = has_mouse;
		r_media.primary_pointer = has_mouse ? _id("fine") : has_touch ? _id("coarse") : _id("none");
		r_media.any_pointer_fine = has_mouse;
		r_media.any_pointer_coarse = has_touch;
	}

	void LiteHTMLContainer::get_language(litehtml::string& r_language, litehtml::string& r_culture) const {
		const String locale = TranslationServer::get_singleton()->get_locale();
		r_language = locale.get_slice("_", 0).utf8().get_data();
		r_culture = locale.get_slice("_", 1).utf8().get_data();
	}
} // namespace dockit
