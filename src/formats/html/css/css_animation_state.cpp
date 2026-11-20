#include "css_animation_state.h"

#include <litehtml/document.h>
#include <litehtml/html.h>
#include <litehtml/html_tag.h>
#include <litehtml/style.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace dockit::css {
	using namespace litehtml;

	namespace {
		std::optional<css_easing> keyframe_easing(const style& frame, const html_tag& node) {
			const auto& declaration = frame.get_property(css_animation_property_names()[3]);
			if (declaration.is<css_transition_easings>()) return declaration.get<css_transition_easings>().front();
			if (declaration.is<initial_value>()) return css_easing{};
			if (declaration.is<inherit>()) {
				const auto parent = node.parent();
				const auto* animations = parent ? parent->css().get_animations() : nullptr;
				return animations ? animations->easings.front() : css_easing{};
			}
			return std::nullopt;
		}

		string_id animation_pseudo(const element& node) {
			return is_generated_tag(node.tag()) ? node.tag() : empty_id;
		}
	}

	double AnimationState::Track::duration() const {
		return animation.duration == 0 ? 0 : animation.duration * animation.iterations;
	}

	double AnimationState::Track::end() const {
		return std::max(0.0, animation.delay + duration());
	}

	void AnimationState::Track::resolve(const html_tag& node, const std::shared_ptr<document>& doc,
			const css_keyframes& keyframes) {
		stops.clear();
		const auto& supported = animated_properties();
		for (const auto& [offset, declarations] : keyframes.frames) {
			style frame = *declarations;
			frame.resolve_cascade(node, true);
			props_map properties;
			for (const auto& property : supported) {
				const auto& value = frame.get_property(property.name);
				if (!value.is<invalid>() || value.declaration_property != empty_id)
					properties.emplace(property.name, value);
			}
			if (properties.empty()) continue;
			css_properties computed;
			computed.compute(&node, doc, &properties);
			const auto easing = keyframe_easing(frame, node);
			for (std::size_t index = 0; index < supported.size(); ++index) {
				const auto& property = supported[index];
				if (properties.count(property.name)) stops[index].push_back({offset, property.read(computed), easing});
			}
		}
		for (auto& [index, series] : stops) {
			const auto& property = supported[index];
			if (series.front().offset != 0) series.insert(series.begin(), {0, property.read(node.css()), {}, true});
			if (series.back().offset != 1) series.push_back({1, property.read(node.css()), {}, true});
		}
	}

	void AnimationState::Track::sample(double now, Sample& output, bool& needs_frames) const {
		if (stops.empty() || playback.cancelled) return;
		const auto& settings = animation;
		const double time = playback.time(now, end(), settings.paused);
		const double local = time - settings.delay;
		const bool before = local < 0;
		const bool after = time >= settings.delay + duration();
		needs_frames |= playback.state(now, end(), settings.paused) == PlaybackState::running;
		if ((before && !(settings.fill & 2)) || (after && !(settings.fill & 1))) return;

		const double overall = before ? 0 : after ? settings.iterations : local / settings.duration;
		double iteration = std::isfinite(overall) ? std::floor(overall) : 0;
		double progress = std::isfinite(overall) ? overall - iteration : 0;
		if (after && progress == 0 && iteration > 0) {
			--iteration;
			progress = 1;
		}
		const bool odd = std::fmod(iteration, 2.0) != 0;
		const bool reversed = ((settings.direction & 1) != 0) != (settings.direction >= 2 && odd);
		if (reversed) progress = 1 - progress;
		const bool before_flag = reversed ? after : before;
		const auto& supported = animated_properties();
		for (const auto& [index, series] : stops) {
			auto upper = std::upper_bound(series.begin(), series.end(), progress,
				[](double offset, const Stop& stop) { return offset < stop.offset; });
			if (upper == series.end()) --upper;
			const auto& start = *std::prev(upper);
			const auto& end = *upper;
			const auto& easing = start.easing ? *start.easing : settings.easing;
			const auto amount = easing.sample((progress - start.offset) / (end.offset - start.offset), before_flag);
			const auto underlying = output.find(index);
			const auto& from = start.implicit && underlying != output.end() ? underlying->second : start.value;
			const auto& to = end.implicit && underlying != output.end() ? underlying->second : end.value;
			const auto& property = supported[index];
			auto value = can_interpolate_animated_value(property, from, to) ?
				interpolate_animated_value(property, from, to, amount, true) : amount < 0.5 ? from : to;
			output.insert_or_assign(index, std::move(value));
		}
	}

	void AnimationState::commit(const std::shared_ptr<document>& doc, double now) {
		decltype(tracks) updated;
		const auto visit = [&](const auto& visit, const element::ptr& node) -> void {
			if (!node || node->css().get_display() == display_none) return;
			const auto* settings = node->css().get_animations();
			const auto* subject = dynamic_cast<const html_tag*>(node.get());
			if (settings && subject) {
				const auto pseudo = animation_pseudo(*node);
				// Style rebuilds recreate generated boxes. Keep their clocks on the originating element.
				const auto owner = pseudo == empty_id ? node : node->parent();
				if (!owner) return;
				const auto old = tracks.find(owner);
				const std::vector<Track>* previous_tracks = nullptr;
				if (old != tracks.end()) {
					const auto found = old->second.find(pseudo);
					if (found != old->second.end()) previous_tracks = &found->second;
				}
				const auto old_count = previous_tracks ? previous_tracks->size() : 0;
				std::vector<bool> matched(old_count);
				std::vector<Track> resolved;
				// The last occurrence of a repeated name keeps the last matching player.
				for (std::size_t index = settings->names.size(); index-- > 0;) {
					const auto& name = settings->names[index];
					const auto* rule = name ? doc->find_keyframes(*name) : nullptr;
					if (!rule) continue;
					Track track;
					track.name = *name;
					track.animation = settings->at(index);
					track.playback.updated_at = now;
					for (std::size_t previous = old_count; previous-- > 0;) {
						const auto& prior = (*previous_tracks)[previous];
						if (matched[previous] || prior.scripted || prior.name != *name) continue;
						matched[previous] = true;
						track.playback = prior.playback;
						track.playback.elapsed = prior.playback.time(now, prior.end(), prior.animation.paused);
						track.playback.updated_at = now;
						break;
					}
					if (track.playback.id == 0) track.playback.id = next_id++;
					track.resolve(*subject, doc, *rule);
					resolved.push_back(std::move(track));
				}
				if (!resolved.empty()) {
					std::reverse(resolved.begin(), resolved.end());
					updated[owner].emplace(pseudo, std::move(resolved));
				}
			}
			for (const auto& child : node->children()) visit(visit, child);
		};
		visit(visit, doc->root());
		for (auto& [weak, targets] : tracks) {
			const auto previous = targets.find(empty_id);
			if (previous == targets.end() || std::none_of(previous->second.begin(), previous->second.end(),
				[](const Track& track) { return bool(track.scripted); })) continue;
			const auto subject = std::dynamic_pointer_cast<html_tag>(weak.lock());
			if (!subject) continue;
			auto root = std::static_pointer_cast<element>(subject);
			while (const auto parent = root->parent()) root = parent;
			if (root != doc->root()) continue;
			for (auto& track : previous->second) {
				if (!track.scripted) continue;
				track.resolve(*subject, doc, *track.scripted);
				updated[subject][empty_id].push_back(std::move(track));
			}
		}
		tracks.swap(updated);
		sample(now);
	}

	uint64_t AnimationState::create(const std::shared_ptr<element>& node, const std::shared_ptr<document>& doc,
			std::shared_ptr<const css_keyframes> frames, const css_animation& settings, double now) {
		const auto* subject = dynamic_cast<const html_tag*>(node.get());
		if (!subject) return 0;
		Track track;
		track.animation = settings;
		track.scripted = std::move(frames);
		track.playback.updated_at = now;
		track.resolve(*subject, doc, *track.scripted);
		if (track.stops.empty()) return 0;
		const auto id = track.playback.id = next_id++;
		tracks[node][empty_id].push_back(std::move(track));
		return id;
	}

	std::vector<uint64_t> AnimationState::list(const element* target) const {
		std::vector<uint64_t> result;
		for (const auto& [weak, pseudos] : tracks) {
			const auto subject = weak.lock();
			if (!subject || (target && target != subject.get())) continue;
			for (const auto& [pseudo, entries] : pseudos) {
				for (const auto& track : entries) {
					if (!track.playback.cancelled) result.push_back(track.playback.id);
				}
			}
		}
		return result;
	}

	std::optional<PlaybackInfo> AnimationState::info(uint64_t id, double now) const {
		for (const auto& [weak, pseudos] : tracks) {
			for (const auto& [pseudo, entries] : pseudos) {
				for (const auto& track : entries) {
					if (track.playback.id != id) continue;
					const auto target = weak.lock();
					if (!target) return std::nullopt;
					const char* pseudo_name = pseudo == __tag_before_ ? "::before" : pseudo == __tag_after_ ? "::after" :
						pseudo == marker_tag() ? "::marker" : "";
					return PlaybackInfo{target, track.name, pseudo_name, track.playback.time(now, track.end(), track.animation.paused),
						track.duration(), track.playback.state(now, track.end(), track.animation.paused), bool(track.scripted),
						track.animation.duration, track.animation.delay};
				}
			}
		}
		return std::nullopt;
	}

	bool AnimationState::control(uint64_t id, PlaybackCommand command, double value, double now) {
		for (auto& [weak, pseudos] : tracks) {
			if (weak.expired()) continue;
			for (auto& [pseudo, entries] : pseudos) {
				for (auto& track : entries) {
					if (track.playback.id == id) return track.playback.control(command, value, now, track.end(), track.animation.paused);
				}
			}
		}
		return false;
	}

	void AnimationState::sample(double now) {
		playing = false;
		overrides.clear();
		const auto& supported = animated_properties();
		for (auto it = tracks.begin(); it != tracks.end();) {
			const auto node = it->first.lock();
			if (!node) {
				it = tracks.erase(it);
				continue;
			}
			for (const auto& [pseudo, target_tracks] : it->second) {
				Sample sample;
				for (const auto& track : target_tracks) track.sample(now, sample, playing);
				props_map values;
				for (const auto& [index, value] : sample) {
					values.emplace(supported[index].name, std::visit(
						[](const auto& value) { return property_value(value, false); }, value));
				}
				if (!values.empty()) overrides[node.get()].emplace(pseudo, std::move(values));
			}
			++it;
		}
	}

	const property_value* AnimationState::find(const element* element, string_id name) const {
		const auto pseudo = animation_pseudo(*element);
		const auto owner = pseudo == empty_id ? nullptr : element->parent();
		const auto subject = overrides.find(pseudo == empty_id ? element : owner.get());
		if (subject == overrides.end()) return nullptr;
		const auto target = subject->second.find(pseudo);
		if (target == subject->second.end()) return nullptr;
		const auto value = target->second.find(name);
		return value == target->second.end() ? nullptr : &value->second;
	}
}
