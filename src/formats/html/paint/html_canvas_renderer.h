#pragma once

#include "html_paint_run.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/rid.hpp>

namespace godot {
	class CanvasItem;
}

namespace dockit {
	class HTMLCanvasRenderer : public godot::RefCounted {
		GDCLASS(HTMLCanvasRenderer, godot::RefCounted)

		struct Surface {
			godot::RID viewport;
			godot::RID canvas;
		};

		struct FilterRegion {
			godot::Rect2 box;
			HTMLPlaneTransform uv_to_box;
			HTMLPlaneTransform box_to_uv;
		};

		struct CanvasDrawing {
			godot::Callable redraw;
			godot::Callable exiting;
			std::vector<godot::RID> items;
			std::vector<Surface> surfaces;
			std::vector<godot::Ref<godot::ShaderMaterial>> materials;
			int32_t run_count = 0;
			uint64_t redraw_id = 0;
			bool submitted = false;
			bool order_passes = false;
			godot::HashMap<godot::RID, std::size_t> pass_indices;
			std::vector<godot::RID> pass_parents;
			std::vector<std::pair<godot::RID, godot::RID>> texture_dependencies;

			void clear();
			godot::RID add_item(godot::RenderingServer& p_server, const godot::RID& p_parent, int32_t p_draw_index);
			Surface add_surface(godot::RenderingServer& p_server, const godot::RID& p_parent, const godot::Vector2i& p_size,
				const godot::Transform2D& p_mapping, bool p_hdr);
			void set_surface_parent(godot::RenderingServer& p_server, const godot::RID& p_surface, const godot::RID& p_parent);
			void order_surface_passes(godot::RenderingServer& p_server, const godot::RID& p_viewport, std::size_t p_start);
		};

		godot::HashMap<uint64_t, CanvasDrawing> drawings;
		godot::Ref<godot::ShaderMaterial> opacity_material;
		godot::Ref<godot::Shader> filter_shader;
		godot::Ref<godot::Shader> mask_shader;
		godot::Ref<godot::Shader> projection_shader;
		godot::Ref<godot::Shader> background_blend_shader;
		godot::Ref<godot::Shader> element_blend_shader;
		uint64_t next_redraw_id = 0;

		void on_canvas_redraw(uint64_t p_canvas_id);
		static void finish_canvas_redraw(uint64_t p_renderer_id, uint64_t p_canvas_id, uint64_t p_redraw_id);
		void detach_canvas(uint64_t p_canvas_id);
		Surface filter_surface(godot::RenderingServer& p_server, CanvasDrawing& p_drawing, Surface p_source,
			const godot::RID& p_parent, const godot::Vector2i& p_size, const godot::Vector2& p_scale,
			const std::vector<HTMLPaintFilter>& p_filters, const godot::Transform2D& p_matrix, bool p_hdr,
			const FilterRegion* p_region = nullptr);

	protected:
		static void _bind_methods() {}

	public:
		~HTMLCanvasRenderer() override;

		void submit(godot::CanvasItem& p_canvas, const godot::Vector2& p_position, const godot::Rect2& p_clip,
			const std::vector<HTMLPaintRun>& p_runs, const std::vector<HTMLPaintGroup>& p_groups, const std::vector<HTMLPaintRun>& p_diagnostics,
			const HTMLHighlight& p_selection, const HTMLHighlight& p_search, const godot::Vector2& p_viewport_origin,
			bool p_selection_only = false);
		void queue_redraws() const;
	};
}
