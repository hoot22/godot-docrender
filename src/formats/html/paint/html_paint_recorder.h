#pragma once

#include "html_paint_run.h"
#include "formats/html/text/html_shaped_text.h"
#include "formats/html/text/html_text_selection.h"
#include "html_border_image.h"
#include "formats/html/css/css_shadows.h"

#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <litehtml/background.h>
#include <litehtml/borders.h>

#include <array>
#include <functional>
#include <unordered_map>

namespace litehtml {
	struct css_underline;
	class render_item_inline_context;
	class html_tag;
	struct list_marker;
}

namespace dockit {
	enum class HTMLControlIcon { play, pause, volume_up, volume_off, resize, fullscreen, fullscreen_exit,
		down, up, left, right, radio_checked, calendar, clock, colorize, unfold_more, radio_unchecked,
		checkbox_checked, checkbox_unchecked, checkbox_indeterminate, close_small, count };

	struct HTMLInputPaintColors {
		std::optional<litehtml::web_color> selection_foreground;
		std::optional<litehtml::web_color> selection_background;
		std::optional<litehtml::web_color> caret;
	};
	struct HTMLTextIndexSpan {
		std::weak_ptr<litehtml::element> source;
		int64_t from;
		int64_t to;
	};

	class HTMLPaintRecorder {
		struct Clip : TransformedClip {
			std::optional<godot::Rect2> bounds;
			bool has_precise_clip;
		};

		struct TextRun {
			HTMLTextLayout layout;
			godot::Rect2 rect;
			godot::Rect2 visible_rect;
			int64_t offset;
			int64_t length;
			HTMLPlaneTransform matrix;
			HTMLPlaneTransform inverse;
			std::vector<TransformedClip> clips;
			int8_t ellipsis_direction = 0;
			const litehtml::element* selection_owner = nullptr;
			bool selectable = false;
		};

		struct ElidedSource {
			const litehtml::element* element;
			int64_t from;
			int64_t to;
		};
		struct Ellipsis {
			HTMLTextLayout layout;
			godot::Vector2 origin;
			TransformedClip clip;
			std::vector<ElidedSource> sources;
			bool rtl;
		};
		struct OverflowPaint {
			std::optional<TransformedClip> clip;
			bool hide = false;
		};

		using Fill = HTMLFill;
		using Text = HTMLText;
		using Image = HTMLImage;
		using Circle = HTMLCircle;
		using Command = HTMLPaintCommand;

		using TextShaper = std::function<HTMLTextLayout(const litehtml::element&, const godot::String&)>;

		using OverflowTextShaper = std::function<HTMLTextLayout(litehtml::uint_ptr, const godot::String&,
			int64_t, int64_t, const litehtml::render_item*)>;

		std::array<godot::Ref<godot::Texture2D>, static_cast<size_t>(HTMLControlIcon::count)> control_icon_textures;
		std::vector<HTMLPaintRun> paint_runs;
		std::vector<HTMLPaintRun> viewport_background_runs;
		std::vector<HTMLPaintGroup> viewport_background_groups;
		struct OpenPaintGroup {
			std::vector<HTMLPaintRun> outlines;
			std::optional<std::size_t> isolated_group;
			std::optional<std::vector<Clip>> enclosing_clips;
			const TransformedClip* path_clip = nullptr;
			const RenderGeometry* mask_geometry = nullptr;
			bool is_stacking_context = false;
			bool is_blend_boundary = false;
		};
		std::vector<OpenPaintGroup> open_groups;
		std::vector<HTMLPaintGroup> paint_groups;
		int filter_depth = 0;
		bool separate_run = false;
		bool has_depth_scene = false;
		std::vector<HTMLPaintRun> diagnostic_runs;
		std::vector<Clip> clips;
		godot::Ref<godot::ShaderMaterial> clip_material;
		HTMLPlaneTransform clip_material_matrix;
		const HTMLGeometryIndex* recording_geometry = nullptr;
		const RenderGeometry* active_geometry = nullptr;
		void fill_rounded_color(const godot::Rect2& p_rect, const godot::Color& p_color, float p_radius);
		std::optional<HTMLScrollbarDrag> scrollbar_hovered;
		std::optional<HTMLScrollbarDrag> scrollbar_pressed;
		void paint_scrollbar(const godot::Rect2& p_track, const godot::Rect2& p_thumb, int p_axis,
			const HTMLScrollbarAppearance& p_appearance);
		bool active_decoration = false;
		struct PointerTarget {
			static constexpr int element = -1;
			static constexpr int resize = -2;
			const RenderGeometry* geometry;
			int axis = element;
			godot::Rect2 bounds;
		};
		std::vector<PointerTarget> pointer_targets;
		void visit_pointer_targets(const godot::Vector2& p_point, const std::function<bool(const PointerTarget&)>& p_visitor) const;
		const PointerTarget* overflow_control_at(const godot::Vector2& p_point) const;
		godot::Rect2 overflow_control_bounds;
		godot::String plain_text;
		godot::String accessible_text;
		std::vector<godot::Vector2i> excluded_selection;
		std::unordered_map<const litehtml::element*, godot::Vector2i> selection_scopes;
		std::vector<int64_t> selection_scope_starts;
		std::optional<bool> separator_selectable;
		std::vector<TextRun> text_runs;
		std::vector<HTMLTextIndexSpan> text_index_spans;
		std::vector<std::pair<const litehtml::element*, godot::Vector2i>> auto_content_text;
		std::vector<Ellipsis> ellipses;
		std::unordered_map<const litehtml::render_item*, std::vector<int64_t>> ellipses_by_context;
		std::unordered_map<const litehtml::render_item*, OverflowPaint> overflow_paint;
		std::unordered_map<const litehtml::element*, std::pair<int64_t, int64_t>> elided_text_offsets;
		bool text_overflow_clip_active = false;
		std::size_t horizon_clip_count = 0;
		godot::HashMap<godot::Vector2, std::vector<Text*>> painted_text;
		godot::HashMap<const litehtml::render_item*, std::vector<Text*>> painted_text_by_renderer;
		godot::HashMap<const litehtml::element*, std::vector<std::pair<Text*, int64_t>>> painted_text_by_source;

		void prepare_text_overflow(const litehtml::render_item& p_root, const OverflowTextShaper& p_shaper);
		void prepare_inline_overflow(const litehtml::render_item_inline_context& p_context, const OverflowTextShaper& p_shaper);
		bool has_elided_ancestor(const litehtml::render_item& p_item) const;
		struct AccessibleTextIndex {
			godot::String& text;
			godot::String separator;
			void append(const godot::String& p_value);
			void space();
		};
		void index_text(const std::shared_ptr<litehtml::element>& p_element, const TextShaper& p_shape_text,
			godot::String& r_separator, AccessibleTextIndex* p_accessible, bool p_unlaid = false, bool p_selectable = true);
		int64_t index_source_text(const godot::String& p_text, godot::String& r_separator, bool p_selectable,
			AccessibleTextIndex* p_accessible);
		void exclude_from_selection(int64_t p_from, int64_t p_to);
		godot::Ref<godot::ShaderMaterial> create_material(const RoundedRect* p_shape = nullptr, bool p_composite = false,
			const TransformedClip* p_path = nullptr, bool p_include_clips = true);
		void add_command(Command p_command, godot::Ref<godot::ShaderMaterial> p_material = {}, HTMLTextureSampling p_sampling = {},
			bool p_fixed_background = false);
		void add_indexed_text(Text p_text, const litehtml::element* p_source, const litehtml::web_color& p_color);
		void add_command_to(std::vector<HTMLPaintRun>& r_runs, Command p_command, godot::Ref<godot::ShaderMaterial> p_material = {},
			HTMLTextureSampling p_sampling = {}, bool p_fixed_background = false);
		void add_image(Image p_image, const RoundedRect* p_shape = nullptr, const godot::Vector2i& p_repeat = {},
			godot::Ref<godot::ShaderMaterial> p_material = {}, bool p_fixed_background = false);
		void add_borders(std::vector<HTMLPaintRun>& r_runs, const litehtml::borders& p_borders, const godot::Rect2& p_rect);
		void record_outline();
		void record_mask(std::size_t p_group, const RenderGeometry& p_geometry);
		HTMLPaintGroup& start_isolated_group(float p_opacity);
		void fill_rect(const godot::Rect2& p_rect, const godot::Color& p_color);
		void draw_decoration(const godot::Rect2& p_rect, litehtml::text_decoration_style p_style, const godot::Color& p_color);
		void draw_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::gradient_base& p_gradient,
			int p_type, const godot::Vector2& p_start, const godot::Vector2& p_parameters);
		void push_clip(const TransformedClip& p_clip);
		void push_clip(const RoundedRect& p_shape, const HTMLPlaneTransform& p_matrix) { push_clip({ p_shape, p_matrix }); }
		bool push_content_clip(const godot::Rect2& p_box);

	public:
		void clear();
		void begin(const HTMLGeometryIndex& p_geometry, const litehtml::render_item* p_root, const OverflowTextShaper& p_shaper);
		void record_viewport_background(const litehtml::render_item& p_root, const std::function<void()>& p_draw);
		void draw_text_overflow(const litehtml::render_item& p_context);
		void draw_scrollbars(const litehtml::render_item& p_item);
		godot::Ref<godot::Texture2D> get_control_icon(HTMLControlIcon p_icon);
		void draw_input_icon(HTMLControlIcon p_icon, const godot::Rect2& p_clip, const godot::Rect2& p_rect, const godot::Color& p_color);
		void draw_resize_grip(const godot::Rect2& p_clip, const godot::Rect2& p_grip);
		void draw_input_scrollbar(const godot::Rect2& p_clip, const godot::Rect2& p_track, const godot::Rect2& p_thumb,
			int p_axis, const HTMLScrollbarAppearance& p_appearance);
		bool set_scrollbar_hover(const std::optional<HTMLScrollbarDrag>& p_target);
		bool set_scrollbar_pressed(const std::optional<HTMLScrollbarDrag>& p_target);
		std::optional<HTMLScrollbarDrag> scrollbar_at(const godot::Vector2& p_point) const;
		const RenderGeometry* resize_grip_at(const godot::Vector2& p_point) const;
		void finish(const std::shared_ptr<litehtml::element>& p_root, const TextShaper& p_shape_text);

		const std::vector<HTMLPaintRun>& get_paint_runs() const;
		const std::vector<HTMLPaintRun>& get_viewport_background() const { return viewport_background_runs; }
		const std::vector<HTMLPaintGroup>& get_viewport_background_groups() const { return viewport_background_groups; }
		const std::vector<HTMLPaintGroup>& get_paint_groups() const { return paint_groups; }
		const std::vector<HTMLPaintRun>& get_diagnostic_runs() const { return diagnostic_runs; }
		std::shared_ptr<litehtml::element> hit_test_element(const godot::Vector2& p_point, bool p_text_selection = false) const;
		void visit_hit_elements(const godot::Vector2& p_point, bool p_text_selection,
			const std::function<bool(const std::shared_ptr<litehtml::element>&)>& p_visit) const;
		const godot::String& get_plain_text() const;
		const auto& get_text_index_spans() const { return text_index_spans; }
		const godot::String& get_accessible_text() const { return accessible_text; }
		godot::String get_copyable_text(int64_t p_from, int64_t p_to) const;
		bool has_selectable_text(int64_t p_from, int64_t p_to) const;
		HTMLSelectionPoint selection_point_at(const godot::Vector2& p_point, std::shared_ptr<litehtml::element> p_target,
			const litehtml::element* p_scope = nullptr) const;
		const auto& get_auto_content_text() const { return auto_content_text; }
		int64_t hit_test_text(const godot::Vector2& p_point, const litehtml::element* p_scope = nullptr) const;
		godot::Vector2i text_range_in(const litehtml::element& p_scope) const;
		bool hit_test_selectable_text(const godot::Vector2& p_point, const litehtml::element& p_target, bool* r_text_hit = nullptr) const;
		godot::TypedArray<godot::Rect2> get_selection_rects(int64_t p_from, int64_t p_to) const;

		void clear_diagnostics() { diagnostic_runs.clear(); }
		void record_css_diagnostic(const RenderGeometry& p_geometry, const godot::Rect2& p_box);
		void draw_text(const HTMLShapedText& p_shaped, int64_t p_length, litehtml::web_color p_color,
			const litehtml::position& p_position, const std::vector<litehtml::box_shadow>& p_shadows,
			const litehtml::css_underline& p_underline);
		void draw_input(const godot::Rect2& p_box, const HTMLTextLayout& p_text, const godot::Vector2& p_origin,
			const litehtml::web_color& p_color, const godot::PackedVector2Array& p_selection,
			const godot::PackedVector2Array& p_preedit, const godot::Rect2& p_caret, float p_ascent, float p_descent, const HTMLInputPaintColors& p_colors);
		void draw_input_chrome(const godot::Rect2& p_clip, const godot::Rect2& p_rect, const godot::Color& p_color, float p_radius = 0);
		void draw_image_alt(const godot::Rect2& p_box, const std::vector<HTMLTextLayout>& p_lines,
			float p_line_height, litehtml::web_color p_color, litehtml::text_align p_align);
		void draw_list_marker(const litehtml::list_marker& p_marker, const godot::Ref<godot::Texture2D>& p_texture);
		litehtml::image_rendering get_image_sampling() const; // 获取当前图层的 CSS 采样方式
		void draw_image(const litehtml::background_layer& p_layer, const godot::Ref<godot::Texture2D>& p_texture);
		void draw_solid_fill(const litehtml::background_layer& p_layer, const litehtml::web_color& p_color);
		void draw_box_shadows(const litehtml::html_tag& p_element, const litehtml::position& p_box,
			const litehtml::borders& p_borders, bool p_inset, const std::vector<litehtml::box_shadow>& p_shadows);
		void draw_border_image(const godot::Ref<godot::Texture2D>& p_texture,
			const std::vector<HTMLBorderImagePatch>& p_patches, const godot::Rect2& p_visible, bool p_flip_x);
		void draw_borders(const litehtml::borders& p_borders, const litehtml::position& p_position);
		void draw_linear_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::linear_gradient& p_gradient);
		void draw_radial_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::radial_gradient& p_gradient);
		void draw_conic_gradient(const litehtml::background_layer& p_layer, const litehtml::background_layer::conic_gradient& p_gradient);

		void set_clip(const litehtml::position& p_position, const litehtml::border_radiuses& p_radii);
		void set_overflow_clip(const litehtml::render_item& p_item);
		void set_render_item_clip(const litehtml::position& p_position, const litehtml::border_radiuses& p_radii,
			const litehtml::render_item& p_item);
		bool begin_render_item(const litehtml::render_item& p_item, const litehtml::position*& r_clip, bool p_decoration);
		void end_render_item();
		void begin_paint_group(const litehtml::render_item& p_item);
		void end_paint_group();
		void begin_background_blend(std::optional<litehtml::css_blend_mode> p_layer);
		void del_clip();
	};
}
