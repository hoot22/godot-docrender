#pragma once

#include "css_animated_property.h"
#include "css_playback.h"

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

namespace litehtml { class element; }

namespace dockit::css {
	class TransitionState {
		using ElementKey = std::weak_ptr<litehtml::element>;
		using Value = AnimatedValue;
		using Values = AnimatedValues;

		struct Track {
			Value from;
			Value to;
			Value reversing_start;
			double delay;
			double duration;
			double shortening = 1.0;
			litehtml::css_easing easing;
			bool discrete = false;
			bool allow_discrete = false;
			Playback playback;

			double end() const { return std::max(0.0, delay + duration); }
			double progress(double now) const;
			Value sample(double now, const AnimatedProperty& property) const;
		};
		using Running = std::map<std::size_t, Track>;
		std::map<ElementKey, Running, std::owner_less<>> running;
		std::unordered_map<const litehtml::element*, litehtml::props_map> overrides;
		uint64_t next_id = 1;
		bool playing = false;

	public:
		std::vector<uint64_t> list(const litehtml::element* target, double now) const;
		std::optional<PlaybackInfo> info(uint64_t id, double now) const;
		bool control(uint64_t id, PlaybackCommand command, double value, double now);
		using Snapshot = std::map<ElementKey, Values, std::owner_less<>>;
		using TransformInterpolationCheck = std::function<bool(const litehtml::element&, const litehtml::css_transform_list&)>;

		static Snapshot capture(const std::shared_ptr<litehtml::element>& root);
		void commit(const std::shared_ptr<litehtml::element>& root, const Snapshot& before, double now,
			const TransformInterpolationCheck& can_interpolate_transform);
		void sample(double now);
		bool active() const { return playing; }
		bool has_overrides() const { return !overrides.empty(); }
		const litehtml::property_value* find(const litehtml::element* element, litehtml::string_id name) const;
	};
}
