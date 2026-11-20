#include "css_transition_state.h"

#include <litehtml/element.h>
#include <litehtml/html.h>
#include <litehtml/html_tag.h>

#include <algorithm>
#include <cmath>

namespace dockit::css {
	using namespace litehtml;

	namespace {
		template<class Visitor>
		void visit_styled(const element::ptr& node, const Visitor& visit) {
			if (!node || node->css().get_display() == display_none || is_generated_tag(node->tag())) return;
			if (dynamic_cast<const html_tag*>(node.get())) visit(node);
			if (node->css().hides_contents()) return;
			for (const auto& child : node->children()) visit_styled(child, visit);
		}
	}

	double TransitionState::Track::progress(double now) const {
		const double time = playback.time(now, end(), false);
		return time < delay ? 0 : duration == 0 || time >= delay + duration ? 1 : easing.sample((time - delay) / duration);
	}

	TransitionState::Value TransitionState::Track::sample(double now, const AnimatedProperty& property) const {
		const double time = playback.time(now, end(), false);
		if (time < delay) return from;
		if (time >= delay + duration) return to;
		const auto amount = progress(now);
		if (discrete) return amount < 0.5 ? from : to;
		return interpolate_animated_value(property, from, to, amount, allow_discrete);
	}

	TransitionState::Snapshot TransitionState::capture(const std::shared_ptr<element>& root) {
		Snapshot before;
		visit_styled(root, [&](const element::ptr& node) {
			if (node->get_render_item() || node->css().get_display() == display_contents)
				before.emplace(node, read_animated_values(node->css()));
		});
		return before;
	}

	void TransitionState::commit(const std::shared_ptr<element>& root, const Snapshot& before, double now,
			const TransformInterpolationCheck& can_interpolate_transform) {
		decltype(running) next;
		visit_styled(root, [&](const element::ptr& node) {
			const auto* styles = node->css().get_transitions();
			if (!styles || (!node->get_render_item() && node->css().get_display() != display_contents)) return;
			const auto old = before.find(node);
			if (old == before.end()) return;
			const auto previous = running.find(node);
			const auto& properties = animated_properties();
			const bool rtl = node->css().is_right_to_left();
			Running updated;
			for (std::size_t index = 0; index < properties.size(); ++index) {
				const auto& property = properties[index];
				const auto config = styles->match(property.name, rtl);
				if (!config) continue;
				const Track* current = nullptr;
				if (previous != running.end()) {
					const auto track = previous->second.find(index);
					if (track != previous->second.end()) current = &track->second;
				}
				auto to = property.read(node->css());
				if (current && equal_animated_value(property, current->to, to)) {
					updated.emplace(index, *current);
					continue;
				}
				const auto from = current && !current->playback.cancelled ? current->sample(now, property) : old->second[index];
				if (equal_animated_value(property, from, to) || config->duration + config->delay <= 0) continue;
				bool discrete = !can_interpolate_animated_value(property, from, to);
				if (std::holds_alternative<css_transform_list>(from)) {
					const auto sample = std::get<css_transform_list>(interpolate_animated_value(property, from, to, 0.5, false));
					discrete = !can_interpolate_transform(*node, sample);
				}
				if (discrete && !config->allow_discrete) continue;
				const bool reversing = current && !current->playback.cancelled && current->playback.time(now, current->end(), false) < current->end() &&
					equal_animated_value(property, current->reversing_start, to);
				const double factor = reversing ? std::clamp(std::abs(current->progress(now) * current->shortening +
					(1 - current->shortening)), 0.0, 1.0) : 1.0;
				const auto delay = config->delay < 0 ? config->delay * factor : config->delay;
				Playback playback;
				playback.id = next_id++;
				playback.updated_at = now;
				updated.emplace(index, Track{from, std::move(to), reversing ? current->to : from, delay,
					config->duration * factor, factor, config->easing, discrete, config->allow_discrete, playback});
			}
			if (!updated.empty())
				next.emplace(node, std::move(updated));
		});
		running.swap(next);
		sample(now);
	}

	void TransitionState::sample(double now) {
		overrides.clear();
		playing = false;
		const auto& properties = animated_properties();
		for (auto it = running.begin(); it != running.end();) {
			if (const auto node = it->first.lock()) {
				props_map values;
				for (const auto& [index, track] : it->second) {
					const auto status = track.playback.state(now, track.end(), false);
					playing |= status == PlaybackState::running;
					if (status == PlaybackState::idle || track.playback.time(now, track.end(), false) >= track.end()) continue;
					const auto& property = properties[index];
					const auto sample = track.sample(now, property);
					values.emplace(property.name, std::visit(
						[](const auto& value) { return property_value(value, false); }, sample));
				}
				if (!values.empty()) overrides.emplace(node.get(), std::move(values));
				++it;
				continue;
			}
			it = running.erase(it);
		}
	}

	std::vector<uint64_t> TransitionState::list(const element* target, double now) const {
		std::vector<uint64_t> result;
		for (const auto& [weak, entries] : running) {
			const auto subject = weak.lock();
			if (!subject || (target && target != subject.get())) continue;
			for (const auto& [index, track] : entries) {
				const auto status = track.playback.state(now, track.end(), false);
				if (status == PlaybackState::running || status == PlaybackState::paused) result.push_back(track.playback.id);
			}
		}
		return result;
	}

	std::optional<PlaybackInfo> TransitionState::info(uint64_t id, double now) const {
		for (const auto& [weak, entries] : running) {
			for (const auto& [index, track] : entries) {
				if (track.playback.id != id) continue;
				const auto target = weak.lock();
				if (!target) return std::nullopt;
				return PlaybackInfo{target, _s(animated_properties()[index].name), {},
					track.playback.time(now, track.end(), false), track.duration, track.playback.state(now, track.end(), false),
					false, 0.0, track.delay};
			}
		}
		return std::nullopt;
	}

	bool TransitionState::control(uint64_t id, PlaybackCommand command, double value, double now) {
		for (auto& [weak, entries] : running) {
			if (weak.expired()) continue;
			for (auto& [index, track] : entries) {
				if (track.playback.id == id) return track.playback.control(command, value, now, track.end(), false);
			}
		}
		return false;
	}

	const property_value* TransitionState::find(const element* element, string_id name) const {
		const auto found = overrides.find(element);
		if (found == overrides.end()) return nullptr;
		const auto value = found->second.find(name);
		return value == found->second.end() ? nullptr : &value->second;
	}
}
