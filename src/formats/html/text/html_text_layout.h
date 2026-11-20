#pragma once

#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

#include <memory>
#include <vector>

namespace godot {
	class TextServer;
}

namespace dockit {
	class HTMLTextSpacing;
	class HTMLTextSlice;

	struct HTMLTextCluster {
		int64_t start;
		int64_t end;
		float width;
	};

	struct HTMLTextRunMetrics {
		int64_t start;
		int64_t end;
		float ascent;
		float descent;
	};

	class HTMLTextLayout {
		struct LayoutMetrics {
			float ascent;
			float descent;
			std::vector<HTMLTextRunMetrics> runs;
		};
		godot::Ref<godot::TextLine> line;
		std::shared_ptr<const HTMLTextSlice> slice;
		std::shared_ptr<const LayoutMetrics> layout_metrics;
		godot::Vector2 inline_trim;
		int64_t source_prefix = 0;
		int64_t source_suffix = 0;

	public:
		float ascent = 0;
		float descent = 0;
		std::shared_ptr<const HTMLTextSpacing> spacing;

		HTMLTextLayout() = default;
		explicit HTMLTextLayout(const godot::Ref<godot::TextLine>& p_line, float p_ascent = 0,
			int64_t p_source_prefix = 0, int64_t p_source_suffix = 0);
		bool is_valid() const { return line.is_valid(); }
		bool is_subrange() const { return bool(slice); }
		godot::RID get_rid() const;
		int64_t text_start(const godot::TextServer& p_server) const;
		godot::Vector2 size() const;
		float width() const;
		float native_width() const;
		float trim_start() const { return inline_trim.x; }
		void trim_inline_advance(float p_start, float p_end);
		float height() const;
		float line_ascent() const;
		float line_descent() const;
		void set_run_metrics(std::vector<HTMLTextRunMetrics> p_runs);
		godot::Vector2 metrics_in_range(int64_t p_start, int64_t p_end) const;
		float underline_position() const;
		float underline_thickness() const;
		std::vector<HTMLTextCluster> clusters(const godot::String& p_text) const;
		HTMLTextLayout subrange(int64_t p_start, int64_t p_end) const;
		godot::PackedVector2Array selection(const godot::TextServer& p_server, int64_t p_from, int64_t p_to) const;
		int64_t hit_test(double p_x) const;
		void draw(const godot::TextServer* p_server, const godot::RID& p_canvas, const godot::Vector2& p_position,
			const godot::Color& p_color) const;
	};
}
