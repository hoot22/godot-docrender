#ifndef DOCKIT_LITEHTML_CONTAINER_H
#define DOCKIT_LITEHTML_CONTAINER_H

#include "formats/html/paint/html_paint_recorder.h"
#include "formats/html/resources/html_resources.h"
#include "formats/html/css/css_values.h"
#include "formats/html/text/litehtml_fonts.h"

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <litehtml/document_container.h>

#include <vector>
#include <utility>

namespace dockit {
	namespace css { class AnimationState; class TransitionState; }
	class HTMLDOM;
	struct HTMLSVGClipDocument;

	class LiteHTMLContainer final : public litehtml::document_container {
	public:
		LiteHTMLContainer(
			HTMLDOM& p_dom,
			const godot::String& p_base_dir,
			const godot::Dictionary& p_textures,
			bool p_fit_images,
			const HTMLFontConfig& p_fonts,
			const godot::Dictionary& p_local_stylesheets,
			const godot::Dictionary& p_local_fonts,
			const godot::Dictionary& p_image_dpi,
			const godot::Dictionary& p_image_orientations,
			const godot::Dictionary& p_svg_clip_sources,
			bool p_svg_clip_only = false
		);

		bool set_font_rules(std::vector<litehtml::css_font_face> p_faces,
			std::vector<litehtml::css_font_feature_values_rule> p_feature_values);		// 更新文档的字体声明和命名特性并报告是否变化
		void set_viewport(const godot::Vector2i& p_size, const godot::Vector2i& p_media_size); // 更新排版和媒体查询各自使用的视口尺寸
		void set_print_media(bool p_enabled) { print_media = p_enabled; }
		bool set_preferred_color_scheme(litehtml::color_scheme p_scheme);
		bool set_accessibility_preferences(bool p_reduce_motion, bool p_increase_contrast);
		std::shared_ptr<litehtml::document> get_svg_clip_document(std::string_view p_url, const char* p_base); // 读取并缓存独立 SVG 裁剪文档
		const litehtml::string_map* get_source_attributes(const litehtml::element& p_element) const; // 读取保留原始拼写的属性
		godot::String resolve_image_source(std::string_view p_source) const {
			return resources.resolve_image_source(p_source);
		}
		void set_remote_image_callback(const godot::Callable& p_callback) { resources.set_remote_image_callback(p_callback); }
		void set_remote_stylesheet_callback(const godot::Callable& p_callback) { resources.set_remote_stylesheet_callback(p_callback); }
		bool receive_remote_stylesheet(const godot::String& p_url, const godot::String& p_final_url, const godot::String& p_text) {
			return resources.receive_remote_stylesheet(p_url, p_final_url, p_text);
		}
		void begin_image_layout() { resources.begin_image_layout(); }
		int pending_resource_count() const { return resources.pending_resource_count(); }
		int get_failed_remote_resource_count() const { return resources.get_failed_remote_resource_count(); }
		HTMLResources::ImageUpdate receive_remote_image(const godot::String& p_url, const godot::Ref<godot::Texture2D>& p_texture) {
			return resources.receive_remote_image(p_url, p_texture);
		}
		godot::Ref<godot::Texture2D> video_poster(const char* p_source) {
			return p_source ? resources.load_texture(p_source, nullptr) : godot::Ref<godot::Texture2D>();
		}
		void record(
			litehtml::document& p_document,
			const godot::Vector2i& p_content_size,
			const HTMLGeometryIndex& p_geometry,
			const godot::Vector2& p_viewport_gutters
		);																			// 记录已排版文档的绘制命令并建立可选择文本的索引
		HTMLTextLayout shape_input(litehtml::uint_ptr p_font, const godot::String& p_text) {
			return p_font && !p_text.is_empty() ? fonts.shape_text(p_font, p_text) : HTMLTextLayout{};
		}
		HTMLTextLayout shape_text_for_outline(litehtml::uint_ptr p_font, const godot::String& p_text,
			bool p_right_to_left, float& r_scale) {
			return p_font ? fonts.shape_text_for_outline(p_font, p_text, p_right_to_left, r_scale) : HTMLTextLayout{};
		}
		void draw_input(const godot::Rect2& p_box, const HTMLTextLayout& p_text, const godot::Vector2& p_origin,
			const litehtml::web_color& p_color, const godot::PackedVector2Array& p_selection,
			const godot::PackedVector2Array& p_preedit, const godot::Rect2& p_caret, float p_ascent = 0, float p_descent = 0, const HTMLInputPaintColors& p_colors = {}) {
			paint.draw_input(p_box, p_text, p_origin, p_color, p_selection, p_preedit, p_caret, p_ascent, p_descent, p_colors);
		}
		void draw_input_chrome(const godot::Rect2& p_clip, const godot::Rect2& p_rect, const godot::Color& p_color,
			float p_radius = 0) {
			paint.draw_input_chrome(p_clip, p_rect, p_color, p_radius);
		}
		void clear_recording() { paint.clear(); }
		void draw_input_icon(HTMLControlIcon p_icon, const godot::Rect2& p_clip, const godot::Rect2& p_rect, const godot::Color& p_color) {
			paint.draw_input_icon(p_icon, p_clip, p_rect, p_color);
		}
		void draw_resize_grip(const godot::Rect2& p_clip, const godot::Rect2& p_grip) {
			paint.draw_resize_grip(p_clip, p_grip);
		}
		godot::Ref<godot::Texture2D> get_control_icon(HTMLControlIcon p_icon) { return paint.get_control_icon(p_icon); }
		void draw_input_scrollbar(const godot::Rect2& p_clip, const godot::Rect2& p_track, const godot::Rect2& p_thumb,
			int p_axis, const HTMLScrollbarAppearance& p_appearance) {
			paint.draw_input_scrollbar(p_clip, p_track, p_thumb, p_axis, p_appearance);
		}
		HTMLPaintRecorder& get_paint_recorder() { return paint; }
		const HTMLPaintRecorder& get_paint_recorder() const { return paint; }
		void record_css_diagnostic(const RenderGeometry& p_geometry, const godot::Rect2& p_box) {
			paint.record_css_diagnostic(p_geometry, p_box);
		}
		void reset_stylesheet_reads();												// 重置本轮外部样式表的读取计数
		void set_document_source_path(const godot::String& p_path) { resources.set_source_path(p_path); }
		void refresh_document_base(const litehtml::element::ptr& p_root);
		const HTMLResourceBase& get_document_base() const { return resources.get_document_base(); }
		const HTMLResourceBase& get_fallback_base() const { return resources.get_fallback_base(); }
		void set_css_diagnostics_enabled(bool p_enabled);							// 设置是否收集 CSS 声明诊断
		void set_transition_state(const css::TransitionState* p_state) { transition_state = p_state; }
		void set_animation_state(const css::AnimationState* p_state) { animation_state = p_state; }
		const litehtml::property_value* transition_override(const litehtml::element* p_element, litehtml::string_id p_name) const override;
		const litehtml::property_value* animation_override(const litehtml::element* p_element, litehtml::string_id p_name) const override;
		void begin_css_diagnostics();												// 清空上一轮样式诊断
		void set_css_source_kind(litehtml::css_source_kind p_kind);					// 设置当前样式表来源
		godot::TypedArray<godot::Dictionary> get_css_diagnostics() const;			// 返回诊断快照
		const std::vector<int64_t>* get_element_diagnostics(
			const litehtml::element& p_element
		) const;
		const godot::Dictionary& get_css_diagnostic(
			int64_t p_index
		) const;
		std::vector<HTMLTextCluster> get_shaped_clusters(litehtml::uint_ptr p_font, const godot::String& p_text);
		std::vector<size_t> get_bidi_visual_order(const char* p_text, litehtml::uint_ptr p_font, bool p_right_to_left,
			const std::vector<std::pair<int64_t, int64_t>>& p_ranges) override {
			return fonts.get_bidi_visual_order(godot::String::utf8(p_text), p_font, p_right_to_left, p_ranges);
		}
		void draw_text_fragment(const godot::String& p_text, int64_t p_start, int64_t p_end, const litehtml::position& p_position,
			const litehtml::element& p_element, litehtml::text_justify_adjustment p_justification, litehtml::punctuation_trim p_trim);
		void draw_automatic_hyphen(const litehtml::position& p_position, const litehtml::element& p_element,
			litehtml::text_justify_adjustment p_justification);
		HTMLDOM& get_dom() { return dom; }
		void set_gauge_animation_time(double p_time) { gauge_animation_time = p_time; gauge_animation_active = false; }
		double sample_gauge_animation() { gauge_animation_active = true; return gauge_animation_time; }
		bool has_gauge_animation() const { return gauge_animation_active; }

	private:
		double gauge_animation_time = 0.0;
		bool gauge_animation_active = false;
		HTMLDOM& dom;
		HTMLResources resources;
		bool css_diagnostics_enabled = false;
		bool print_media = false;
		bool prefers_reduced_motion = false;
		bool prefers_contrast_more = false;
		const css::TransitionState* transition_state = nullptr;
		const css::AnimationState* animation_state = nullptr;
		litehtml::css_source_kind css_source_kind = litehtml::css_source_kind::none;
		std::vector<godot::Dictionary> css_diagnostics;
		godot::HashMap<godot::String, int64_t> css_diagnostic_keys;
		godot::HashMap<const litehtml::element*, std::vector<int64_t>> element_diagnostics;
		bool fit_images;
		bool svg_clip_only;
		bool has_base_element = false;
		LiteHTMLFonts fonts;
		godot::Vector2i viewport_size;
		godot::Vector2i media_viewport_size;
		litehtml::color_scheme preferred_color_scheme = litehtml::color_scheme::light;
		godot::HashMap<godot::String, std::shared_ptr<HTMLSVGClipDocument>> svg_clip_documents;
		HTMLPaintRecorder paint;

		litehtml::css_source get_css_source(const litehtml::string& p_base, bool p_inline) const override;
		bool is_image_available(const char* p_source, const char* p_base, litehtml::css_image_orientation p_orientation) override;
		std::shared_ptr<HTMLImageAltText> shape_image_alt(const char* p_alt, const litehtml::html_tag& p_image) override;
		void draw_image_alt(HTMLImageAltText& p_text, const litehtml::position& p_box, const litehtml::html_tag& p_image) override;
		void report_css_diagnostic(const litehtml::css_diagnostic& p_diagnostic, const litehtml::html_tag* p_element) override;
		litehtml::uint_ptr create_font(const litehtml::font_description& p_description, const litehtml::document*,
			litehtml::font_metrics* r_metrics) override;
		void delete_font(litehtml::uint_ptr p_font) override;
		litehtml::pixel_t text_width(const char* p_text, litehtml::uint_ptr p_font) override;
		litehtml::pixel_t measure_text(const char* p_text, litehtml::uint_ptr p_font, litehtml::font_metrics& r_metrics) override;
		litehtml::pixel_t trimmed_punctuation_width(const char* p_text, litehtml::uint_ptr p_font, litehtml::punctuation_trim p_trim) override;
		litehtml::text_justify_info get_text_justify_info(const char* p_text, litehtml::uint_ptr p_font,
			litehtml::text_justify p_method) override;
		void draw_text(litehtml::uint_ptr, const char* p_text, litehtml::uint_ptr p_font,
			litehtml::web_color p_color, const litehtml::position& p_position) override;
		void draw_styled_text(litehtml::uint_ptr, const char* p_text, litehtml::uint_ptr p_font,
			litehtml::web_color p_color, const litehtml::position& p_position, const litehtml::element& p_element,
			litehtml::text_justify_adjustment p_justification, litehtml::punctuation_trim p_trim) override;
		void record_text(const char* p_text, litehtml::uint_ptr p_font, litehtml::web_color p_color,
			const litehtml::position& p_position, const std::vector<litehtml::box_shadow>& p_shadows,
			const litehtml::css_underline& p_underline = {}, litehtml::text_justify_adjustment p_justification = {},
			litehtml::punctuation_trim p_trim = litehtml::punctuation_trim::none);
		litehtml::pixel_t pt_to_px(float p_points) const override;
		litehtml::pixel_t get_default_font_size() const override;
		const char* get_default_font_name() const override;
		litehtml::pixel_t get_default_fixed_font_size() const override;
		void draw_list_marker(litehtml::uint_ptr, const litehtml::list_marker& p_marker) override;
		void load_image(const char* p_src, const char* p_css_base, bool, litehtml::css_image_orientation p_orientation) override;
		void get_image_size(const char* p_src, const char* p_css_base, litehtml::size& r_size,
			litehtml::css_image_orientation p_orientation) override;
		double get_image_dpi(const char* p_src, const char* p_css_base) override;
		void get_css_image_size(const char* p_src, const char* p_css_base, double p_density, litehtml::size& r_size,
			litehtml::css_image_orientation p_orientation) override;
		void draw_image(litehtml::uint_ptr, const litehtml::background_layer& p_layer, const std::string& p_url,
			const std::string& p_css_base) override;
		void draw_cross_fade(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::css_cross_fade& p_mix, const std::string& p_css_base, const litehtml::web_color& p_color, int p_direction) override;
		void draw_solid_fill(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::web_color& p_color) override;
		void draw_box_shadows(const litehtml::html_tag& p_element, const litehtml::position& p_box,
			const litehtml::borders& p_borders, bool p_inset) override;
		bool draw_border_image(const litehtml::html_tag& p_element, const litehtml::borders& p_borders,
			const litehtml::position& p_box, const litehtml::position* p_fragment, bool p_first, bool p_last) override;
		void draw_borders(litehtml::uint_ptr, const litehtml::borders& p_borders,
			const litehtml::position& p_position, bool) override;
		void transform_text(litehtml::string& r_text, litehtml::text_transform p_transform) override;
		void split_text(const char* p_text, const std::function<void(const char*)>& p_word,
			const std::function<void(const char*)>& p_space) override;
		void set_clip(const litehtml::position& p_position, const litehtml::border_radiuses&) override;
		void set_overflow_clip(const litehtml::render_item& p_item) override;
		void set_render_item_clip(const litehtml::position& p_position, const litehtml::border_radiuses& p_radii,
			const litehtml::render_item& p_item) override;
		bool begin_render_item(const litehtml::render_item& p_item, const litehtml::position*& r_clip, bool p_decoration) override;
		void end_render_item() override;
		void draw_scrollbars(const litehtml::render_item& p_item) override;
		void draw_text_overflow(const litehtml::render_item& p_item) override;
		void begin_paint_group(const litehtml::render_item& p_item) override;
		void end_paint_group() override;
		void begin_background_blend() override;
		void begin_background_blend_layer(litehtml::css_blend_mode p_mode) override;
		void del_clip() override;
		void get_viewport(litehtml::position& r_viewport) const override;
		void get_media_features(litehtml::media_features& r_media) const override;
		void get_language(litehtml::string& r_language, litehtml::string& r_culture) const override;

		void import_css(litehtml::string& r_text, const litehtml::string& p_url, litehtml::string& r_css_base) override;
		void set_base_url(const char*) override {} // 文档基准在读取样式表前按 DOM 顺序计算
		void set_caption(const char*) override {}
		void link(const std::shared_ptr<litehtml::document>& p_doc, const litehtml::element::ptr& p_element) override;
		void on_anchor_click(const char*, const litehtml::element::ptr&) override {}
		void on_mouse_event(const litehtml::element::ptr&, litehtml::mouse_event) override {}
		void set_cursor(const char*) override {}
		litehtml::element::ptr create_element(const char* p_tag, const litehtml::string_map&,
			const std::shared_ptr<litehtml::document>& p_document) override;

		void draw_linear_gradient(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::background_layer::linear_gradient& p_gradient) override;
		void draw_radial_gradient(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::background_layer::radial_gradient& p_gradient) override;
		void draw_conic_gradient(litehtml::uint_ptr, const litehtml::background_layer& p_layer,
			const litehtml::background_layer::conic_gradient& p_gradient) override;
	};
} // namespace dockit

#endif
