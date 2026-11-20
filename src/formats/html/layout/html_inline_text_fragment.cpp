#include "html_inline_text_fragment.h"

#include "html_lexical_breaks.h"
#include "html_line_break.h"
#include "html_punctuation_spacing.h"
#include "html_text_autospace.h"
#include "formats/html/backend/litehtml_container.h"

#include <litehtml/iterators.h>
#include <litehtml/render_inline.h>
#include <litehtml/render_inline_context.h>

#include <algorithm>
#include <cmath>

using namespace godot;
using namespace litehtml;

namespace dockit {
	HTMLInlineTextFragment::HTMLInlineTextFragment(const std::shared_ptr<render_item>& p_original,
		const std::shared_ptr<const String>& p_rendered, int64_t p_run_start, const HTMLTextCluster& p_cluster,
		bool p_lexical_break_before, bool p_hyphenation_fallback) :
		el_text(nullptr, p_original->src_el()->get_document(), false), original(p_original), rendered(p_rendered), run_start(p_run_start),
		start(p_cluster.start), end(p_cluster.end), lexical_break_before(p_lexical_break_before), hyphenation_fallback(p_hyphenation_fallback) {
		const auto& source = *original->src_el();
		parent(source.parent());
		css_w() = source.css();
		m_text = is_automatic_hyphen() ? "\xc2\xad" : rendered->substr(start - run_start, end - start).utf8().get_data();
		set_hyphenation_allowed(is_automatic_hyphen());
		auto metrics = css().get_font_metrics();
		auto* container = static_cast<LiteHTMLContainer*>(get_document()->container());
		const auto owner = parent();
		if (is_automatic_hyphen() && owner) {
			metrics = owner->css().get_font_metrics();
			const auto layout = container->shape_input(css().get_font(), String::utf8(owner->css().get_hyphenate_character().c_str()));
			if (layout.is_valid()) {
				metrics.ascent = layout.ascent;
				metrics.descent = layout.descent;
			}
		} else {
			const auto layout = container->shape_input(css().get_font(), *rendered);
			if (layout.is_valid()) {
				const auto extents = layout.metrics_in_range(start - run_start, end - run_start);
				metrics.ascent = extents.x;
				metrics.descent = extents.y;
			}
		}
		metrics.height = metrics.ascent + metrics.descent;
		css_w().set_font_metrics(metrics);
		m_size = { p_cluster.width, metrics.height };
	}

	void HTMLInlineTextFragment::draw(uint_ptr, pixel_t p_x, pixel_t p_y, const litehtml::position* p_clip,
		const std::shared_ptr<render_item>& p_item) {
		auto pos = p_item->pos();
		pos.x += p_x;
		pos.y = std::round(pos.y + p_y);
		const auto owner = parent();
		if (owner && owner->css().get_font() && pos.does_intersect(p_clip)) {
			auto* container = static_cast<LiteHTMLContainer*>(get_document()->container());
			if (is_automatic_hyphen()) {
				if (pos.width > 0) container->draw_automatic_hyphen(pos, *owner, p_item->get_text_justification());
			}
			else container->draw_text_fragment(*rendered, start - run_start, end - run_start, pos, *owner,
				p_item->get_text_justification(), p_item->get_punctuation_trim());
		}
	}
}

namespace litehtml {
	namespace {
		void build_text_fragments(render_item& p_parent, dockit::LiteHTMLContainer& p_container,
			const dockit::HTMLInlineWordBreaks& p_breaks) {
			using Fragment = dockit::HTMLInlineTextFragment;
			go_inside_inline inline_children;
			auto& children = p_parent.children();
			for (auto child = children.begin(); child != children.end();) {
				const auto current = child++;
				auto original = *current;
				if (const auto* fragment = dynamic_cast<const Fragment*>(original->src_el().get())) {
					original = fragment->original_renderer();
					while (child != children.end()) {
						const auto* next = dynamic_cast<const Fragment*>((*child)->src_el().get());
						if (!next || next->original_renderer() != original) break;
						child = children.erase(child);
					}
					*current = original;
					original->skip(false);
				}
				if (inline_children.select(original)) {
					build_text_fragments(*original, p_container, p_breaks);
					continue;
				}
				const auto* text = dynamic_cast<const el_text*>(original->src_el().get());
				if (!text || !text->css().get_font() || text->is_break() || text->is_space() ||
					text->is_other_space_separator() || is_explicit_wrap_point(*text)) continue;
				const auto& style = text->css();
				const auto owner = text->parent();
				const auto dictionary = p_breaks.lexical.find(original.get());
				const auto automatic = p_breaks.automatic_hyphens.find(original.get());
				const bool wraps = style.get_text_wrap_mode() == text_wrap_wrap;
				const bool has_dictionary_breaks = wraps && dictionary != p_breaks.lexical.end();
				const bool split_clusters = has_dictionary_breaks || (wraps && (style.get_line_break() == line_break_anywhere ||
					style.get_word_break() == word_break_break_all || style.get_effective_overflow_wrap() != overflow_wrap_normal));
				const bool has_hyphens = wraps && automatic != p_breaks.automatic_hyphens.end();
				const auto& rendered = text->get_rendered_text();
				const int autospace = owner && rendered.size() >= 2 ? owner->css().get_text_autospace() : 0;
				const bool non_ascii = std::any_of(rendered.begin(), rendered.end(), [](unsigned char p_byte) { return p_byte >= 0x80; });
				const bool can_autospace = autospace != 0 && ((autospace & text_autospace_punctuation) || non_ascii);
				const bool can_trim_punctuation = owner && owner->css().get_text_spacing_trim() != 0 && non_ascii;
				if (!split_clusters && !has_hyphens && !can_autospace && !can_trim_punctuation) continue;
				const auto automatic_at = [&](int64_t offset) -> const dockit::HTMLAutomaticHyphen* {
					if (!has_hyphens) return nullptr;
					const auto& cuts = automatic->second;
					const auto candidate = std::lower_bound(cuts.begin(), cuts.end(), offset,
						[](const dockit::HTMLAutomaticHyphen& entry, int64_t at) { return entry.offset < at; });
					return candidate != cuts.end() && candidate->offset == offset ? &*candidate : nullptr;
				};
				if (rendered.empty() || (rendered.size() < 2 && !has_hyphens)) continue;
				auto content = std::make_shared<const String>(String::utf8(rendered.c_str()));
				std::vector<int64_t> shaping_cuts;
				if (can_autospace || can_trim_punctuation) {
					for (int64_t offset = 1; offset < content->length(); ++offset) {
						int64_t before = offset - 1;
						while (before > 0 && is_punctuation_variation_selector((*content)[before])) --before;
						const auto next = (*content)[offset];
						const bool punctuation_boundary = can_trim_punctuation && !is_punctuation_variation_selector(next) &&
							(is_fullwidth_spacing_punctuation((*content)[before]) || is_fullwidth_spacing_punctuation(next));
						if (punctuation_boundary || (can_autospace && is_autospace_fragment_boundary(*owner, *content, offset))) {
							shaping_cuts.push_back(offset);
						}
					}
					if (!split_clusters && !has_hyphens && shaping_cuts.empty()) continue;
				}
				auto clusters = p_container.get_shaped_clusters(text->css().get_font(), *content);
				if (clusters.empty() ||
					content->length() != String::utf8(text->get_source_text().c_str()).length()) continue;
				shaping_cuts.erase(std::remove_if(shaping_cuts.begin(), shaping_cuts.end(), [&](int64_t p_offset) {
					const auto match = std::lower_bound(clusters.begin(), clusters.end(), p_offset,
						[](const dockit::HTMLTextCluster& p_cluster, int64_t p_position) { return p_cluster.start < p_position; });
					return match == clusters.end() || match->start != p_offset;
				}), shaping_cuts.end());
				if (!split_clusters && !has_hyphens && shaping_cuts.empty()) continue;
				if (clusters.size() == 1 && !has_dictionary_breaks && !automatic_at(clusters.front().end)) continue;
				bool first = true;
				const auto add = [&](const std::shared_ptr<const String>& p_run, int64_t p_run_start,
					const dockit::HTMLTextCluster& cluster, bool lexical, bool fallback) {
					auto node = std::make_shared<Fragment>(original, p_run, p_run_start, cluster, lexical, fallback);
					auto renderer = std::make_shared<render_item_inline>(std::move(node));
					renderer->parent(p_parent.shared_from_this());
					if (first) {
						original->skip(true);
						*current = std::move(renderer);
						first = false;
					}
					else children.insert(child, std::move(renderer));
				};
				const auto add_run = [&](const std::shared_ptr<const String>& p_run, int64_t p_start,
					std::vector<dockit::HTMLTextCluster> p_clusters) {
					size_t used = 0;
					for (auto cluster : p_clusters) {
						cluster.start += p_start;
						cluster.end += p_start;
						if (used == 0 || split_clusters || automatic_at(cluster.start)) p_clusters[used++] = cluster;
						else {
							p_clusters[used - 1].end = cluster.end;
							p_clusters[used - 1].width += cluster.width;
						}
					}
					if (used == 1 && !shaping_cuts.empty()) {
						p_clusters[0].width = text->get_document()->container()->text_width(p_run->utf8().get_data(), text->css().get_font());
					}
					for (size_t index = 0; index < used; ++index) {
						const auto& cluster = p_clusters[index];
						const bool lexical = has_dictionary_breaks &&
							std::binary_search(dictionary->second.begin(), dictionary->second.end(), cluster.start);
						add(p_run, p_start, cluster, lexical, false);
						if (const auto* hyphen = automatic_at(cluster.end)) {
							add(p_run, p_start, { cluster.end, cluster.end, 0 }, false, hyphen->fallback);
						}
					}
				};
				if (shaping_cuts.empty()) add_run(content, 0, std::move(clusters));
				else {
					// Shape separate runs where CSS changes or inserts an advance.
					shaping_cuts.push_back(content->length());
					int64_t start = 0;
					for (const auto end : shaping_cuts) {
						auto run = std::make_shared<const String>(content->substr(start, end - start));
						add_run(run, start, p_container.get_shaped_clusters(text->css().get_font(), *run));
						start = end;
					}
				}
			}
		}
	}

	void render_item_inline_context::prepare_text_fragments(const containing_block_context& self_size) {
		m_line_boxes.clear();
		if (auto* container = dynamic_cast<dockit::LiteHTMLContainer*>(src_el()->get_document()->container())) {
			const auto width = self_size.measuring_inline_size || self_size.grid_intrinsic_width ? 0 : self_size.render_width;
			const auto dictionary = dockit::prepare_inline_word_breaks(shared_from_this(), width);
			build_text_fragments(*this, *container, dictionary);
			prepare_autospace_replacements(shared_from_this(), width);
		}
	}

	void render_item_inline_context::merge_text_fragments() {
		using Fragment = dockit::HTMLInlineTextFragment;
		// Justified lines can have extra gaps between individual text items.
		if (css().get_text_align() == text_align_justify || css().get_text_align_last() == text_align_justify) return;
		for (auto& line : m_line_boxes) {
			render_item* first = nullptr;
			Fragment* combined = nullptr;
			for (const auto& item : line->items()) {
				auto& render = *item->get_el();
				auto* fragment = dynamic_cast<Fragment*>(render.src_el().get());
				if (fragment && fragment->is_automatic_hyphen() && render.pos().width <= 0) {
					render.skip(true);
					continue;
				}
				if (fragment && fragment->is_automatic_hyphen()) fragment = nullptr;
				if (!fragment || render.skip()) {
					first = nullptr;
					combined = nullptr;
					continue;
				}
				if (combined && combined->shares_shaping_run(*fragment) &&
					combined->source_end() == fragment->source_start() && first->pos().y == render.pos().y) {
					combined->extend_to(fragment->source_end());
					first->pos().width = render.pos().right() - first->pos().x;
					render.skip(true);
				}
				else {
					first = &render;
					combined = fragment;
				}
			}
		}
	}
}
