#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace litehtml { class element; }

namespace dockit::css {
	enum class PlaybackCommand { seek, play, pause, cancel, finish, reverse };
	enum class PlaybackState { idle, running, paused, finished };

	struct Playback {
		uint64_t id = 0;
		double elapsed = 0;
		double updated_at = 0;
		double rate = 1;
		std::optional<bool> paused;
		bool cancelled = false;

		double time(double now, double end, bool css_paused) const {
			if (cancelled) return 0;
			if (paused.value_or(css_paused)) return elapsed;
			const double current = elapsed + (now - updated_at) * rate;
			return rate > 0 ? std::min(current, std::max(elapsed, end)) : std::max(current, std::min(elapsed, 0.0));
		}

		PlaybackState state(double now, double end, bool css_paused) const {
			if (cancelled) return PlaybackState::idle;
			if (paused.value_or(css_paused)) return PlaybackState::paused;
			const double current = time(now, end, css_paused);
			return (rate > 0 ? current >= end : current <= 0) ? PlaybackState::finished : PlaybackState::running;
		}

		bool control(PlaybackCommand command, double value, double now, double end, bool css_paused) {
			double current = time(now, end, css_paused);
			switch (command) {
				case PlaybackCommand::seek:
					if (!std::isfinite(value)) return false;
					current = value;
					if (cancelled) paused = true;
					cancelled = false;
					break;
				case PlaybackCommand::play:
				case PlaybackCommand::reverse: {
					const double direction = command == PlaybackCommand::reverse ? -rate : rate;
					if (direction < 0 && (cancelled || current <= 0 || current > end) && !std::isfinite(end)) return false;
					rate = direction;
					if (cancelled || (rate > 0 ? current >= end : current <= 0 || current > end)) current = rate > 0 ? 0 : end;
					paused = false;
					cancelled = false;
					break;
				}
				case PlaybackCommand::pause:
					if (cancelled && rate < 0 && !std::isfinite(end)) return false;
					if (cancelled) current = rate < 0 ? end : 0;
					cancelled = false;
					paused = true;
					break;
				case PlaybackCommand::cancel:
					cancelled = true;
					current = 0;
					break;
				case PlaybackCommand::finish:
					if (rate > 0 && !std::isfinite(end)) return false;
					current = rate > 0 ? end : 0;
					paused = false;
					cancelled = false;
					break;
			}
			elapsed = current;
			updated_at = now;
			return true;
		}
	};

	struct PlaybackInfo {
		std::shared_ptr<litehtml::element> target;
		std::string name;
		std::string pseudo;
		double time = 0;
		double duration = 0;
		PlaybackState state = PlaybackState::idle;
		bool scripted = false;
		double iteration_duration = 0.0;
		double delay = 0.0;
	};
}
