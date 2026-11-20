#include "html_gauge_node.h"

#include "formats/html/backend/litehtml_container.h"

#include <litehtml/document.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <cmath>
#include <utility>

using namespace godot;
using namespace litehtml;

namespace dockit {
std::shared_ptr<render_item> HTMLGaugeNode::create_render_item(const std::shared_ptr<render_item>& p_parent) {
	auto children = std::exchange(m_children, {});
	auto item = html_tag::create_render_item(p_parent);
	m_children.swap(children);
	return item;
}

void HTMLGaugeNode::draw(uint_ptr p_hdc, pixel_t p_x, pixel_t p_y, const position* p_clip, const std::shared_ptr<render_item>& p_item) {
	html_tag::draw(p_hdc, p_x, p_y, p_clip, p_item);
	if (css().get_appearance() == appearance_none) return;
	const auto pos = p_item->pos();
	const Rect2 box(pos.x + p_x, pos.y + p_y, pos.width, pos.height);
	if (!box.has_area()) return;
	auto* container = static_cast<LiteHTMLContainer*>(get_document()->container());
	Rect2 track = box;
	track.size.y *= 0.45f;
	track.position.y = box.get_center().y - track.size.y * 0.5f;
	const float radius = track.size.y * 0.5f;
	const auto scheme = css().get_color_scheme();
	container->draw_input_chrome(box, track, html_system_color(system_color::gauge_border, scheme), radius);
	container->draw_input_chrome(box, track.grow(-1), html_system_color(system_color::gauge_track, scheme), std::max(0.0f, radius - 1));
	const auto reading = read_html_gauge(*this);
	Color fill = tag() == _progress_ ? Color::hex(0x0075ffff) : Color::hex(0x107c10ff);
	if (reading.zone == 1) fill = Color::hex(0xe2a500ff);
	if (reading.zone == 2) fill = Color::hex(0xc50000ff);
	const auto color = css().get_accent_color().to_srgb(css().get_color());
	if (tag() == _progress_ || css().get_accent_color().system != system_color::accent) {
		fill = Color(color.red / 255.0, color.green / 255.0, color.blue / 255.0, color.alpha / 255.0);
	}

	Rect2 amount = track;
	double start = 0;
	double fraction = reading.fraction;
	if (reading.indeterminate) {
		const double phase = std::fmod(container->sample_gauge_animation(), 2.0);
		fraction = 0.3;
		start = (phase <= 1 ? phase : 2 - phase) * (1 - fraction);
	}
	if (css().is_right_to_left()) start = 1 - start - fraction;
	amount.position.x += track.size.x * start;
	amount.size.x *= fraction;
	if (amount.has_area()) container->draw_input_chrome(box, amount, fill, radius);
}
}
