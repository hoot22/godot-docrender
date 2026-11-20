#pragma once

#include "html_audio_element.h"
#include <litehtml/html_tag.h>

namespace dockit {
class HTMLAudioNode final : public litehtml::html_tag {
	godot::Rect2 content_box;

public:
	using html_tag::html_tag;
	double volume = 1.0;
	double time = 0.0;
	double duration = 0.0;
	godot::String current_src;
	bool muted = false;
	bool mute_overridden = false;
	bool paused = true;
	HTMLAudioPart focus_part = HTMLAudioPart::play;

	void compute_styles(bool p_recursive) override;
	std::shared_ptr<litehtml::render_item> create_render_item(const std::shared_ptr<litehtml::render_item>& p_parent) override;
	void draw(litehtml::uint_ptr p_hdc, litehtml::pixel_t p_x, litehtml::pixel_t p_y,
		const litehtml::position* p_clip, const std::shared_ptr<litehtml::render_item>& p_item) override;
	HTMLAudioPart part_at(const godot::Vector2& p_point) const;
	godot::Rect2 part_rect(HTMLAudioPart p_part) const;
	bool has_controls() const;
	bool is_muted() const;
};
}
