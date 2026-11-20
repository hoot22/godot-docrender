// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace dockit::css::detail {

inline int value_index(std::string_view value, std::string_view keywords, int fallback = -1) {
	if (value.empty()) return fallback;
	for (int index = 0; !keywords.empty(); ++index) {
		const auto end = keywords.find(';');
		if (keywords.substr(0, end) == value) return index;
		if (end == std::string_view::npos) break;
		keywords.remove_prefix(end + 1);
	}
	return fallback;
}

inline std::string index_value(int index, std::string_view keywords) {
	const int original = index;
	while (index >= 0 && !keywords.empty()) {
		const auto end = keywords.find(';');
		if (end != 0 && index-- == 0) return std::string(keywords.substr(0, end));
		if (end == std::string_view::npos) break;
		keywords.remove_prefix(end + 1);
	}
	return std::to_string(original);
}

template<typename T, typename... Options>
bool is_one_of(const T& value, const Options&... options) {
	return (... || (value == options));
}

template<typename T>
const T& at(const std::vector<T>& values, int index) {
	static const T invalid{};
	const int size = static_cast<int>(values.size());
	if (index < 0) index += size;
	return index >= 0 && index < size ? values[index] : invalid;
}

template<typename T>
std::vector<T> slice(const std::vector<T>& values, int index) {
	return {values.begin() + index, values.end()};
}

template<typename Container>
void remove(Container& value, int index) {
	const int size = static_cast<int>(value.size());
	if (index < 0) index += size;
	if (index >= 0 && index < size) value.erase(value.begin() + index);
}

inline int lowcase(int ch) {
	return ch >= 'A' && ch <= 'Z' ? ch + 'a' - 'A' : ch;
}

inline std::string lowcase(std::string value) {
	for (auto& ch : value) ch = static_cast<char>(lowcase(ch));
	return value;
}

inline bool is_letter(int ch) {
	ch = lowcase(ch);
	return ch >= 'a' && ch <= 'z';
}

inline bool is_digit(int ch) {
	return ch >= '0' && ch <= '9';
}

inline bool is_hex_digit(int ch) {
	ch = lowcase(ch);
	return is_digit(ch) || (ch >= 'a' && ch <= 'f');
}

inline int digit_value(int ch) {
	return is_digit(ch) ? ch - '0' : lowcase(ch) - 'a' + 10;
}

inline bool is_surrogate(int ch) {
	return ch >= 0xD800 && ch < 0xE000;
}

} // namespace dockit::css::detail
