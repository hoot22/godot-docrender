#include "html_paint_recorder.h"

#include "formats/html/layout/html_inline_text_fragment.h"
#include "formats/html/layout/html_line_break.h"

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <litehtml/render_inline_context.h>

#include <algorithm>
#include <cmath>

using namespace godot;
using namespace litehtml;

namespace dockit {
	namespace {
		struct LinePiece {
			const render_item* renderer;
			const element* source;
			int64_t from;
			int64_t to;
			real_t start;
			real_t end;
			bool atomic;
		};

		std::vector<Vector2> glyph_bounds(const TextServer& p_server, const HTMLTextLayout& p_layout,
			const std::vector<HTMLTextCluster>& p_clusters) {
			std::vector<Vector2> result(p_clusters.size(), Vector2(p_layout.width(), 0));
			const int64_t origin = p_layout.text_start(p_server);
			const auto glyphs = p_server.shaped_text_get_glyphs(p_layout.get_rid());
			const auto from_native = [&](float p_offset) {
				const float adjusted = p_layout.spacing ? p_layout.spacing->from_native(p_offset) : p_offset;
				return std::clamp(adjusted - p_layout.trim_start(), 0.0f, p_layout.width());
			};
			float cursor = 0;
			for (int64_t i = 0; i < glyphs.size();) {
				const Dictionary info = glyphs[i];
				const int64_t count = std::clamp<int64_t>(int64_t(info["count"]), 1, glyphs.size() - i);
				const auto cluster = std::upper_bound(p_clusters.begin(), p_clusters.end(), int64_t(info["start"]) - origin,
					[](int64_t p_start, const HTMLTextCluster& p_candidate) { return p_start < p_candidate.end; });
				const float start = cursor;
				const int64_t limit = i + count;
				for (; i < limit; ++i) {
					const Dictionary glyph = glyphs[i];
					cursor += float(glyph["advance"]) * int32_t(glyph["repeat"]);
				}
				if (cluster != p_clusters.end() && int64_t(info["start"]) >= origin + cluster->start && cursor > start) {
					auto& bounds = result[cluster - p_clusters.begin()];
					bounds.x = std::min(bounds.x, real_t(from_native(start)));
					bounds.y = std::max(bounds.y, real_t(from_native(cursor)));
				}
			}
			return result;
		}

		template<class Shape>
		void read_inline_pieces(const line_box& p_line, const Rect2& p_viewport, real_t p_sign,
			const HTMLGeometryIndex& p_index, const RenderGeometry& p_context, const TextServer& p_server,
			const Shape& p_shaper, std::vector<LinePiece>& r_pieces) {
			for (const auto& item : p_line.items()) {
				const auto& box = *item->get_el();
				if (item->get_type() != line_box_item::type_text_part || box.skip() || item->width() <= 0) continue;
				const auto* geometry = p_index.find_renderer(&box);
				if (!geometry || !geometry->inverse) continue;

				const auto to_context = *p_context.inverse * geometry->matrix;
				const auto visual_range = [&](const Rect2& p_rect) {
					const auto projected = to_context.xform(p_rect);
					return Vector2(p_sign * (p_sign > 0 ? projected.position.x : projected.get_end().x),
						p_sign * (p_sign > 0 ? projected.get_end().x : projected.position.x));
				};
				const position placement = box.get_placement();
				const auto* text = dynamic_cast<const el_text*>(box.src_el().get());
				if (!text) {
					if (geometry->fragments.empty()) continue;
					const Vector2 range = visual_range(geometry->fragments.front().rect);
					r_pieces.push_back({ &box, nullptr, 0, 0, range.x, range.y, true });
					continue;
				}

				const auto parent = text->parent();
				if (!parent || !parent->css().get_font() || text->is_break()) continue;
				const auto* fragment = dynamic_cast<const HTMLInlineTextFragment*>(text);
				const element* source = fragment ? fragment->original_renderer()->src_el().get() : text;
				const int64_t source_start = fragment ? fragment->source_start() : 0;
				const bool slice = fragment && !fragment->is_automatic_hyphen();
				const String run = slice ? fragment->shaping_text() : String::utf8(is_soft_hyphen(*text) ?
					parent->css().get_hyphenate_character().c_str() : text->get_rendered_text().c_str());
				const int64_t start = slice ? fragment->shaping_start() : 0;
				const int64_t end = slice ? fragment->shaping_end() : -1;
				const String value = slice ? run.substr(start, end - start) : run;
				if (value.is_empty()) continue;
				const auto shaped = p_shaper(parent->css().get_font(), run, start, end, &box);
				if (!shaped.is_valid() || shaped.width() <= 0) continue;
				const real_t top = std::round(placement.y) + shaped.ascent - shaped.line_ascent();
				const Rect2 text_rect(placement.x, top, shaped.width(), shaped.height());
				const Vector2 extent = visual_range(text_rect);
				const real_t view_start = p_sign * (p_sign > 0 ? p_viewport.position.x : p_viewport.get_end().x);
				const real_t view_end = view_start + p_viewport.size.x;
				// Characters wholly outside the viewport cannot contribute to the retained part.
				if (extent.x >= view_end || extent.y <= view_start) {
					r_pieces.push_back({ &box, source, source_start, source_start + value.length(), extent.x, extent.y, false });
					continue;
				}
				const auto clusters = shaped.clusters(value);
				const auto bounds = glyph_bounds(p_server, shaped, clusters);
				for (size_t i = 0; i < clusters.size(); ++i) {
					const auto& range = bounds[i];
					if (range.y <= range.x) continue;
					const auto& cluster = clusters[i];
					const Vector2 projected = visual_range(Rect2(placement.x + range.x, top, range.y - range.x, shaped.height()));
					r_pieces.push_back({ &box, source, source_start + cluster.start, source_start + cluster.end,
						projected.x, projected.y, false });
				}
			}
		}
	}

	void HTMLPaintRecorder::prepare_text_overflow(const render_item& p_root, const OverflowTextShaper& p_shaper) {
		if (auto* lines = dynamic_cast<const render_item_inline_context*>(&p_root)) {
			prepare_inline_overflow(*lines, p_shaper);
		}
		for (const auto& child : p_root.children()) prepare_text_overflow(*child, p_shaper);
	}

	void HTMLPaintRecorder::prepare_inline_overflow(const render_item_inline_context& p_context, const OverflowTextShaper& p_shaper) {
		const bool anonymous = p_context.src_el()->tag() == empty_id;
		const auto parent = anonymous ? p_context.parent() : nullptr;
		const render_item& owner = parent ? *parent : static_cast<const render_item&>(p_context);
		const auto& style = owner.css();
		if (!style.has_text_overflow_ellipsis() || style.get_overflow_x() == overflow_visible ||
			!style.get_font() || !owner.is_visible()) return;

		const auto* geometry = recording_geometry->find_renderer(&p_context);
		if (!geometry || !geometry->inverse) return;
		const auto ellipsis = p_shaper(style.get_font(), String::chr(0x2026), 0, -1, nullptr);
		const auto server = TextServerManager::get_singleton()->get_primary_interface();
		if (!ellipsis.is_valid() || ellipsis.width() <= 0 || server.is_null()) return;

		const bool rtl = style.is_right_to_left();
		const real_t sign = rtl ? -1 : 1;
		const Rect2 viewport = to_rect(owner.get_placement());
		const auto axes = style.get_overflow_y() == overflow_visible ? ClipAxes::x : ClipAxes::both;
		const Rect2 context_box = to_rect(p_context.get_placement());
		const auto add_source = [&](Ellipsis& r_mark, const element* p_source, int64_t p_from, int64_t p_to) {
			if (!p_source || p_from >= p_to) return;
			r_mark.sources.push_back({ p_source, p_from, p_to });
			elided_text_offsets.try_emplace(p_source, -1, -1);
		};
		const auto add_subtree = [&](auto&& self, Ellipsis& r_mark, const element& p_source) -> void {
			if (const auto* text = dynamic_cast<const el_text*>(&p_source)) {
				add_source(r_mark, text, 0, String::utf8(text->get_source_text().c_str()).length());
			}
			else if (p_source.css().get_display() != display_none) {
				for (const auto& child : p_source.children()) self(self, r_mark, *child);
			}
		};

		for (const auto& line : p_context.line_boxes()) {
			Rect2 line_clip = viewport;
			const real_t right = std::min(viewport.get_end().x, context_box.position.x + line->line_right());
			line_clip.size.x = right - viewport.position.x;
			if (line_clip.size.x <= 0 || (axes == ClipAxes::both && line_clip.size.y <= 0)) continue;
			std::vector<LinePiece> pieces;
			read_inline_pieces(*line, line_clip, sign, *recording_geometry, *geometry, *server.ptr(), p_shaper, pieces);
			const real_t start = rtl ? -line_clip.get_end().x : line_clip.position.x;
			const real_t end = start + line_clip.size.x;
			if (std::none_of(pieces.begin(), pieces.end(), [&](const LinePiece& p_piece) { return p_piece.end > end + 0.01; })) continue;
			std::stable_sort(pieces.begin(), pieces.end(), [](const LinePiece& p_left, const LinePiece& p_right) {
				return p_left.start < p_right.start;
			});
			const auto first = std::find_if(pieces.begin(), pieces.end(), [&](const LinePiece& p_piece) { return p_piece.end > start; });
			// Preserve the first character or atomic inline even when nothing else fits.
			if (first == pieces.end() || first->end >= end) continue;
			real_t cut = first->end;
			for (auto piece = std::next(first); piece != pieces.end() && piece->end <= end - ellipsis.width(); ++piece) {
				cut = std::max(cut, piece->end);
			}

			const real_t baseline = context_box.position.y + line->bottom() - line->baseline() - p_context.get_scroll_top();
			const real_t top = std::round(baseline - ellipsis.ascent) + ellipsis.ascent - ellipsis.line_ascent();
			Ellipsis mark{ ellipsis, Vector2(rtl ? -cut - ellipsis.width() : cut, top),
				{ RoundedRect(line_clip, {}), geometry->matrix, axes }, {}, rtl };
			Rect2 retained = line_clip;
			if (rtl) {
				retained.position.x = -cut;
				retained.size.x = line_clip.get_end().x - retained.position.x;
			}
			else retained.size.x = cut - retained.position.x;
			for (const auto& piece : pieces) {
				if (piece.end <= cut + 0.01) continue;
				auto& paint = overflow_paint[piece.renderer];
				if (piece.atomic) {
					paint.hide = true;
					add_subtree(add_subtree, mark, *piece.renderer->src_el());
				}
				else {
					paint.clip = TransformedClip{ RoundedRect(retained, {}), geometry->matrix, axes };
					add_source(mark, piece.source, piece.from, piece.to);
				}
			}
			ellipses_by_context[&p_context].push_back(static_cast<int64_t>(ellipses.size()));
			ellipses.push_back(std::move(mark));
		}
	}

	bool HTMLPaintRecorder::has_elided_ancestor(const render_item& p_item) const {
		if (overflow_paint.empty()) return false;
		for (auto node = &p_item; node; ) {
			const auto paint = overflow_paint.find(node);
			if (paint != overflow_paint.end() && paint->second.hide) return true;
			const auto parent = node->parent();
			node = parent.get();
		}
		return false;
	}

}
