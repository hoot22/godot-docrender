#pragma once

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace dockit::input_time {
	enum class Kind { none, date, month, week, time, local };
	constexpr double day = 86400000.0;

	std::optional<double> parse(Kind p_kind, const godot::String& p_value);
	godot::String format(Kind p_kind, double p_number);
	godot::Dictionary parts(Kind p_kind, double p_number);
	std::optional<double> from_parts(Kind p_kind, const godot::Dictionary& p_parts);
	double scale(Kind p_kind);
	double step(Kind p_kind);
	double base(Kind p_kind);
}
