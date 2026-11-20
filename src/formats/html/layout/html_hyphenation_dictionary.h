#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace dockit {
	struct HyphenationPatternData {
		std::string_view locale;
		const uint8_t* bytes;
		size_t size;
	};

	const HyphenationPatternData* find_hyphenation_patterns(std::string_view p_locale);
	const HyphenationPatternData* hyphenation_patterns_for_language(std::string_view p_language);
	std::vector<int64_t> automatic_hyphenation_points(const HyphenationPatternData& p_patterns, std::u32string_view p_word);
}
