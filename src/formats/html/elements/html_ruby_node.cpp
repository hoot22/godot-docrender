#include "html_ruby_node.h"

#include "formats/html/layout/grid/litehtml_render_grid.h"

#include <array>
#include <utility>

using namespace litehtml;

namespace dockit {
namespace {
	class RubySegment final : public render_item_grid {
	protected:
		std::optional<pixel_t> find_layout_baseline(bool p_last) const override {
			if (!children().empty()) {
				const auto& base = *children().front();
				if (const auto baseline = base.layout_baseline(p_last)) {
					return content_offset_top() + base.calc_placement().y - base.content_offset_top() + *baseline;
				}
			}
			return std::nullopt;
		}

	public:
		using render_item_grid::render_item_grid;
		std::shared_ptr<render_item> clone() override { return std::make_shared<RubySegment>(src_el()); }
	};

	struct Segment {
		std::vector<element::ptr> base;
		std::vector<element::ptr> annotations;
	};

	void append_segment(const std::shared_ptr<render_item>& p_parent, const element::ptr& p_ruby, const Segment& p_segment) {
		const auto wrapper_node = std::make_shared<html_tag>(p_ruby, "display:inline-grid;grid-template-columns:auto");
		const auto wrapper = std::make_shared<RubySegment>(wrapper_node);
		wrapper->parent(p_parent);
		p_parent->add_child(wrapper);

		const bool under = p_ruby->css().is_ruby_under();
		const int annotation_count = static_cast<int>(p_segment.annotations.size());
		const auto append_row = [&](int p_row, const auto& p_children) {
			const auto row_node = std::make_shared<html_tag>(wrapper_node,
				"display:block;white-space:nowrap;text-align:center;grid-column:1;grid-row:" + std::to_string(p_row));
			const auto row = row_node->create_render_item(wrapper);
			for (const auto& child : p_children) child->append_render_items(row);
			wrapper->add_child(row);
		};
		append_row(under ? 1 : annotation_count + 1, p_segment.base);
		for (int i = 0; i < annotation_count; ++i) {
			append_row(under ? i + 2 : annotation_count - i, std::array{p_segment.annotations[i]});
		}
	}
}

std::shared_ptr<render_item> HTMLRubyNode::create_render_item(const std::shared_ptr<render_item>& p_parent) {
	if (css().skips_contents()) return html_tag::create_render_item(p_parent);
	auto children = std::exchange(m_children, {});
	auto result = html_tag::create_render_item(p_parent);
	m_children.swap(children);
	if (!result) return result;
	std::vector<Segment> segments;
	Segment next;
	for (const auto& child : m_children) {
		if (child->tag() == _rp_ && child->css().get_display() == display_none) continue;
		if (child->tag() == _rt_) {
			if (child->css().get_display() == display_none) continue;
			if (next.base.empty() && !segments.empty() && !segments.back().annotations.empty()) {
				segments.back().annotations.push_back(child);
			} else {
				next.annotations.push_back(child);
				segments.push_back(std::exchange(next, {}));
			}
		} else {
			next.base.push_back(child);
		}
	}
	const auto self = shared_from_this();
	for (const auto& segment : segments) append_segment(result, self, segment);
	for (const auto& child : next.base) child->append_render_items(result);
	return result;
}
}
