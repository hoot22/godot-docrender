#pragma once

#include <cmath>
#include <locale>
#include <optional>
#include <sstream>
#include <string_view>

namespace dockit {
	// HTML attributes accept a numeric prefix; form values require a complete number.
	inline std::optional<double> html_number(std::string_view input, bool p_entire) {
		if (!p_entire) {
			const size_t start = input.find_first_not_of(" \t\n\r\f");
			if (start == std::string_view::npos) return {};
			input.remove_prefix(start);
			if (input.front() == '+') {
				input.remove_prefix(1);
				if (!input.empty() && input.front() == '-') return {};
			}
		}
		if (input.empty()) return {};

		const auto consume_digits = [&](size_t position) {
			while (position < input.size() && input[position] >= '0' && input[position] <= '9') {
				++position;
			}
			return position;
		};
		const size_t first_digit = input.front() == '-' ? 1 : 0;
		size_t end = consume_digits(first_digit);
		bool has_digits = end != first_digit;
		if (end < input.size() && input[end] == '.') {
			const size_t decimals = end + 1;
			end = consume_digits(decimals);
			if (p_entire && end == decimals) return {};
			has_digits |= end != decimals;
		}
		if (!has_digits) return {};

		const size_t mantissa_end = end;
		if (end < input.size() && (input[end] == 'e' || input[end] == 'E')) {
			size_t exponent = end + 1;
			if (exponent < input.size() && (input[exponent] == '+' || input[exponent] == '-')) {
				++exponent;
			}
			const size_t exponent_end = consume_digits(exponent);
			if (exponent_end != exponent) end = exponent_end;
		}
		if (p_entire && end != input.size()) return {};

		// Floating-point from_chars is unavailable in some supported libc++ versions.
		std::istringstream stream{ std::string(input.substr(0, end)) };
		stream.imbue(std::locale::classic());
		double value = 0.0;
		if (!(stream >> value) || !std::isfinite(value)) return {};
		if (value == 0.0 && input.find_first_of("123456789", first_digit) < mantissa_end) return {};
		return value;
	}

	inline std::optional<double> html_number(const char* p_value, bool p_entire = false) {
		return p_value ? html_number(std::string_view(p_value), p_entire) : std::nullopt;
	}
}
