#pragma once

#include "formats/html/text/html_text_layout.h"
#include "formats/html/layout/html_geometry.h"
#include "formats/html/css/css_blend_mode.h"

#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/rid.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace godot {
	class RenderingServer;
	class StyleBox;
	class TextServer;
}

namespace litehtml { enum class css_filter_type; }

namespace dockit {
	void release_html_paint_shader();
	godot::Ref<godot::ShaderMaterial> make_html_paint_material(bool p_composite = false);

	struct HTMLFill {
		godot::Rect2 rect;
		godot::Color color;
	};

	struct HTMLText {
		HTMLTextLayout layout;
		godot::Vector2 position;
		godot::Color color;
		int64_t offset = -1;
		int64_t length = 0;
		const litehtml::render_item* renderer = nullptr;
		int64_t ellipsis = -1;
		bool selectable = true;
		std::optional<godot::Color> selection_foreground;
		std::optional<godot::Color> selection_background;
		const litehtml::element* selection_owner = nullptr;
	};

	struct HTMLImage {
		godot::Ref<godot::Texture2D> texture;
		godot::Rect2 destination;
		godot::Rect2 source;
		godot::Color modulate{1, 1, 1, 1};
	};

	enum class HTMLTextureFilter { inherited, nearest, linear, linear_mipmaps };

	struct HTMLTextureSampling {
		HTMLTextureFilter filter = HTMLTextureFilter::inherited;
		bool repeat = false;

		bool operator!=(const HTMLTextureSampling& p_other) const {
			return filter != p_other.filter || repeat != p_other.repeat;
		}
	};

	struct HTMLCircle {
		godot::Vector2 center;
		float radius;
		godot::Color color;
		bool filled;
	};

	using HTMLPaintCommand = std::variant<HTMLFill, HTMLText, HTMLImage, HTMLCircle>;

	struct HTMLHighlight {
		int64_t from = 0;
		int64_t to = 0;
		const godot::StyleBox* style = nullptr;
		godot::Color color;
	};

	struct HTMLPaintFilter {
		litehtml::css_filter_type type;
		float amount;
		godot::Vector2 offset;
		godot::Color color;
	};

	struct HTMLPaintRun {
		std::optional<godot::Rect2> clip;
		HTMLPlaneTransform matrix;
		std::vector<TransformedClip> clips;
		godot::Ref<godot::ShaderMaterial> material;
		std::vector<HTMLPaintCommand> commands;
		HTMLTextureSampling sampling;
		bool fixed_background = false;
		std::optional<godot::Vector3> depth_plane;

		void draw(
			godot::RenderingServer& p_server,
			const godot::RID& p_canvas,
			const godot::Vector2& p_offset,
			const godot::TextServer* p_text_server,
			const HTMLHighlight& p_search,
			const HTMLHighlight& p_selection,
			const godot::RID& p_ink_canvas = {},
			bool p_selection_only = false
		) const;
	};

	struct HTMLPaintMaskLayer {
		std::vector<HTMLPaintRun> runs;
		bool luminance = false;
		bool linear_rgb = false;
		int composite = 0;
	};

	struct HTMLBackdropArea {
		TransformedClip clip;
		godot::Ref<godot::ShaderMaterial> material;
		godot::Ref<godot::ShaderMaterial> input_material;
		std::optional<godot::Rect2> output_clip;
	};

	// Half-open ranges in paint order. Each group's descendants immediately follow it.
	struct HTMLPaintGroup {
		std::size_t first_run;
		std::size_t last_run;
		std::size_t after_group;
		float opacity;
		std::vector<HTMLPaintFilter> filters;
		godot::Transform2D matrix;
		std::optional<godot::Rect2> output_clip;
		godot::Ref<godot::ShaderMaterial> output_material;
		std::optional<godot::Vector3> depth_plane;
		std::size_t first_pointer = 0;
		std::size_t last_pointer = 0;
		bool sorts_depth = false;
		bool blends_background = false;
		litehtml::css_blend_mode blend_mode = litehtml::css_blend_mode::normal;
		std::optional<litehtml::css_blend_mode> element_blend_mode;
		bool captures_backdrop = false;
		bool can_draw_directly = false;
		std::vector<HTMLPaintMaskLayer> mask;
		std::vector<HTMLPaintFilter> backdrop_filters;
		std::vector<HTMLBackdropArea> backdrop_areas;
		bool backdrop_root = false;
		bool captures_filter_backdrop = false;
		bool needs_parent_backdrop = false;
	};

	godot::Rect2 html_group_ink_bounds(const std::vector<HTMLPaintRun>& p_runs,
		const std::vector<HTMLPaintGroup>& p_groups, std::size_t p_group);
}
