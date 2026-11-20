#include "html_text_layout.h"
#include "html_shaped_text.h"

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>

#include <algorithm>

using namespace godot;

namespace dockit {
	class HTMLTextSlice {
		Ref<TextServer> server;
		Ref<TextLine> source;

	public:
		const RID rid;
		const int64_t start;
		const float ascent;
		const float descent;
		const float width;

		HTMLTextSlice(const Ref<TextServer>& p_server, const Ref<TextLine>& p_source, int64_t p_start, int64_t p_end) :
			server(p_server), source(p_source),
			rid(server->shaped_text_substr(source->get_rid(), p_start, p_end - p_start)), start(p_start),
			ascent(rid.is_valid() ? server->shaped_text_get_ascent(rid) : 0),
			descent(rid.is_valid() ? server->shaped_text_get_descent(rid) : 0),
			width(rid.is_valid() ? server->shaped_text_get_width(rid) : 0) {}

		~HTMLTextSlice() {
			if (rid.is_valid()) server->free_rid(rid);
		}
	};

	HTMLTextLayout::HTMLTextLayout(const Ref<TextLine>& p_line, float p_ascent, int64_t p_source_prefix, int64_t p_source_suffix) :
		line(p_line), source_prefix(p_source_prefix), source_suffix(p_source_suffix), ascent(p_ascent) {}

	RID HTMLTextLayout::get_rid() const {
		return slice ? slice->rid : line->get_rid();
	}

	int64_t HTMLTextLayout::text_start(const TextServer& p_server) const {
		return slice ? slice->start : p_server.shaped_text_get_range(get_rid()).x + source_prefix;
	}

	Vector2 HTMLTextLayout::size() const {
		Vector2 size = slice ? Vector2(slice->width, slice->ascent + slice->descent) : line->get_size();
		size.x = width();
		return size;
	}

	float HTMLTextLayout::width() const {
		return (spacing ? spacing->get_width() : native_width()) - inline_trim.x - inline_trim.y;
	}

	float HTMLTextLayout::native_width() const {
		return slice ? slice->width : line->get_line_width();
	}

	void HTMLTextLayout::trim_inline_advance(float p_start, float p_end) {
		inline_trim = Vector2(p_start, p_end);
	}

	float HTMLTextLayout::height() const {
		return slice ? slice->ascent + slice->descent : line->get_size().y;
	}

	float HTMLTextLayout::line_ascent() const {
		return slice ? slice->ascent : line->get_line_ascent();
	}

	float HTMLTextLayout::line_descent() const {
		return slice ? slice->descent : line->get_line_descent();
	}

	void HTMLTextLayout::set_run_metrics(std::vector<HTMLTextRunMetrics> p_runs) {
		if (p_runs.empty()) return;
		layout_metrics = std::make_shared<const LayoutMetrics>(LayoutMetrics{ascent, descent, std::move(p_runs)});
		for (const auto& run : layout_metrics->runs) {
			ascent = std::max(ascent, run.ascent);
			descent = std::max(descent, run.descent);
		}
	}

	Vector2 HTMLTextLayout::metrics_in_range(int64_t p_start, int64_t p_end) const {
		if (!layout_metrics) return Vector2(ascent, descent);
		const auto& runs = layout_metrics->runs;
		auto run = std::lower_bound(runs.begin(), runs.end(), p_start,
			[](const HTMLTextRunMetrics& item, int64_t start) { return item.end <= start; });
		Vector2 result(layout_metrics->ascent, layout_metrics->descent);
		for (; run != runs.end() && run->start < p_end; ++run) {
			result.x = std::max<real_t>(result.x, run->ascent);
			result.y = std::max<real_t>(result.y, run->descent);
		}
		return result;
	}

	float HTMLTextLayout::underline_position() const {
		return line->get_line_underline_position();
	}

	float HTMLTextLayout::underline_thickness() const {
		return line->get_line_underline_thickness();
	}

	std::vector<HTMLTextCluster> HTMLTextLayout::clusters(const String& p_text) const {
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		const auto length = p_text.length();
		if (!is_valid() || server.is_null() || length == 0) return {};
		if (length == 1) return { { 0, length, width() } };
		const auto boundaries = server->string_get_character_breaks(p_text);
		if (boundaries.size() < 2) return { { 0, length, width() } };
		std::vector<float> advances(length + 1);
		std::vector<int64_t> glyph_ends(length);
		const auto offset = text_start(*server.ptr());
		for (const Dictionary& glyph : server->shaped_text_get_glyphs(get_rid())) {
			const int64_t start = int64_t(glyph["start"]) - offset;
			const int64_t end = int64_t(glyph["end"]) - offset;
			if (start < 0 || start >= length || end <= start || end > length) continue;
			advances[end] += float(glyph["advance"]) * int32_t(glyph["repeat"]);
			glyph_ends[start] = std::max(glyph_ends[start], end);
		}
		std::vector<HTMLTextCluster> result;
		int64_t start = 0;
		int64_t farthest_glyph_end = 0;
		int64_t boundary = 0;
		float advance = 0;
		for (int64_t end = 1; end <= length; ++end) {
			farthest_glyph_end = std::max(farthest_glyph_end, glyph_ends[end - 1]);
			advance += advances[end];
			while (boundary < boundaries.size() && boundaries[boundary] < end) ++boundary;
			if (end == length || (boundary < boundaries.size() && boundaries[boundary] == end && farthest_glyph_end <= end)) {
				result.push_back({ start, end, advance });
				start = end;
				advance = 0;
			}
		}
		return result;
	}

	HTMLTextLayout HTMLTextLayout::subrange(int64_t p_start, int64_t p_end) const {
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		if (!is_valid() || server.is_null() || p_start < 0 || p_end <= p_start) return {};
		const auto range = server->shaped_text_get_range(get_rid());
		const int64_t prefix = slice ? 0 : source_prefix;
		const int64_t suffix = slice ? 0 : source_suffix;
		const int64_t source_length = int64_t(range.y) - range.x - prefix - suffix;
		if (p_end > source_length) return {};
		if (p_start == 0 && p_end == source_length) return *this;
		const int64_t shaped_start = range.x + prefix + p_start;
		const auto metrics = metrics_in_range(shaped_start, shaped_start + p_end - p_start);
		HTMLTextLayout result(line, metrics.x);
		result.descent = metrics.y;
		result.layout_metrics = layout_metrics;
		result.slice = std::make_shared<HTMLTextSlice>(server, line, shaped_start, shaped_start + p_end - p_start);
		return result.slice->rid.is_valid() ? result : HTMLTextLayout{};
	}

	PackedVector2Array HTMLTextLayout::selection(const TextServer& p_server, int64_t p_from, int64_t p_to) const {
		const auto start = text_start(p_server);
		auto ranges = p_server.shaped_text_get_selection(get_rid(), start + p_from, start + p_to);
		if (spacing) {
			for (int64_t index = 0; index < ranges.size(); ++index) {
				const Vector2 range = ranges[index];
				ranges.set(index, Vector2(spacing->from_native(range.x), spacing->from_native(range.y)));
			}
		}
		if (inline_trim != Vector2()) {
			const auto extent = width();
			for (int64_t index = 0; index < ranges.size(); ++index) {
				const Vector2 range = ranges[index];
				ranges.set(index, Vector2(std::clamp(range.x - inline_trim.x, real_t(0), real_t(extent)),
					std::clamp(range.y - inline_trim.x, real_t(0), real_t(extent))));
			}
		}
		return ranges;
	}

	int64_t HTMLTextLayout::hit_test(double p_x) const {
		const double adjusted = p_x + inline_trim.x;
		const double native = spacing ? spacing->to_native(adjusted) : adjusted;
		if (slice) {
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			return server->shaped_text_hit_test_position(slice->rid, native) - slice->start;
		}
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		const auto range = server.is_valid() ? server->shaped_text_get_range(line->get_rid()) : Vector2i();
		const int64_t source_length = std::max<int64_t>(0, int64_t(range.y) - range.x - source_prefix - source_suffix);
		return std::clamp<int64_t>(line->hit_test(native) - source_prefix, 0, source_length);
	}

	void HTMLTextLayout::draw(const TextServer* p_server, const RID& p_canvas, const Vector2& p_position,
		const Color& p_color) const {
		const Vector2 origin = p_position - Vector2(inline_trim.x, 0);
		if (p_server && (spacing || slice)) {
			const Vector2 baseline = origin + Vector2(0, line_ascent());
			if (spacing) spacing->draw(*p_server, p_canvas, baseline, p_color);
			else p_server->shaped_text_draw(slice->rid, p_canvas, baseline, -1, -1, p_color);
		}
		else if (!slice) line->draw(p_canvas, origin, p_color);
	}
}
