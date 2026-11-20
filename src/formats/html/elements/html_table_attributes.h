#pragma once

#include <algorithm>
#include <string_view>

namespace dockit {
	inline int read_table_span(std::string_view p_value, int p_minimum, int p_maximum) {
		const auto start = p_value.find_first_not_of(" \t\n\r\f");
		if (start == std::string_view::npos) return 1;
		p_value.remove_prefix(start);
		const bool negative = p_value.front() == '-';
		if (negative || p_value.front() == '+') p_value.remove_prefix(1);
		if (p_value.empty() || p_value.front() < '0' || p_value.front() > '9') return 1;
		int value = 0;
		for (const char digit : p_value) {
			if (digit < '0' || digit > '9') break;
			if (negative && digit != '0') return 1;
			value = value * 10 + digit - '0';
			if (value >= p_maximum) return p_maximum;
		}
		return std::max(value, p_minimum);
	}
}
