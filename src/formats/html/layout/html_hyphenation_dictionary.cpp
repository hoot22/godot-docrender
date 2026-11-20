#include "html_hyphenation_dictionary.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <utility>

namespace dockit {
	namespace {
		constexpr uint32_t HYB_MAGIC = 0x62ad7968;
		constexpr size_t MAX_WORD_LENGTH = 128;
		constexpr uint32_t ALPHABET_MASK = (1u << 11) - 1;
		constexpr uint32_t PATTERN_OFFSET_MASK = (1u << 20) - 1;

		struct Bytes {
			const uint8_t* data;
			size_t size;

			uint32_t u32(size_t p_offset) const {
				const auto* bytes = data + p_offset;
				return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
			}
			Bytes section(size_t p_start, size_t p_end) const { return { data + p_start, p_end - p_start }; }
		};

		uint32_t letter_code(Bytes p_alphabet, char32_t p_letter) {
			if (p_alphabet.size < 8) return 0;
			const auto version = p_alphabet.u32(0);
			if (version == 0 && p_alphabet.size >= 12) {
				const auto minimum = p_alphabet.u32(4);
				const auto end = p_alphabet.u32(8);
				if (p_letter < minimum || p_letter >= end || end - minimum > p_alphabet.size - 12) return 0;
				return p_alphabet.data[12 + p_letter - minimum];
			}
			if (version != 1 || p_alphabet.u32(4) > (p_alphabet.size - 8) / 4) return 0;
			const size_t count = p_alphabet.u32(4);
			size_t first = 0;
			size_t last = count;
			while (first < last) {
				const size_t middle = first + (last - first) / 2;
				if (p_alphabet.u32(8 + middle * 4) >> 11 < p_letter) first = middle + 1;
				else last = middle;
			}
			if (first == count) return 0;
			const auto packed = p_alphabet.u32(8 + first * 4);
			return (packed >> 11) == p_letter ? packed & ALPHABET_MASK : 0;
		}

		struct PatternTable {
			Bytes table;
			uint32_t count;
			uint32_t pool;
			uint32_t size;

			void apply(uint32_t p_pattern, int p_match_end, size_t p_word_length,
				std::array<uint8_t, MAX_WORD_LENGTH + 1>& r_weights) const {
				if (!p_pattern || p_pattern >= count) return;
				const auto entry = table.u32(16 + size_t(p_pattern) * 4);
				const auto offset = entry & PATTERN_OFFSET_MASK;
				const int length = entry >> 26;
				const int trailing = (entry >> 20) & 0x3f;
				if (offset > size || uint32_t(length) > size - offset) return;
				const int beginning = p_match_end + 1 - length - trailing;
				const int first = std::max(0, -beginning);
				const int past_last = std::min(length, static_cast<int>(p_word_length) + 1 - beginning);
				for (int index = first; index < past_last; ++index) {
					auto& weight = r_weights[beginning + index];
					weight = std::max(weight, table.data[pool + offset + index]);
				}
			}
		};

		char32_t canonical_letter(std::u32string_view p_text, size_t& r_position) {
			struct Composition { char32_t first; char32_t second; char32_t result; };
			static constexpr Composition spellings[] = {
				#include "unicode_hyphenation_compose.inc"
			};
			const auto first = p_text[r_position++];
			const auto find = [first](char32_t second) -> char32_t {
				const auto key = std::make_pair(first, second);
				const auto* match = std::lower_bound(std::begin(spellings), std::end(spellings), key,
					[](const Composition& entry, const std::pair<char32_t, char32_t>& sequence) {
						return std::make_pair(entry.first, entry.second) < sequence;
					});
				return match != std::end(spellings) && match->first == first && match->second == second ? match->result : 0;
			};
			if (r_position < p_text.size() && p_text[r_position] >= 0x80) {
				if (const auto combined = find(p_text[r_position])) {
					++r_position;
					return combined;
				}
			}
			const auto canonical = first >= 0x80 ? find(0) : 0;
			return canonical ? canonical : first;
		}

		std::string_view consume_subtag(std::string_view& r_tag) {
			const size_t separator = r_tag.find('-');
			const auto subtag = r_tag.substr(0, separator);
			r_tag = separator == std::string_view::npos ? std::string_view() : r_tag.substr(separator + 1);
			return subtag;
		}
	}

	const HyphenationPatternData* hyphenation_patterns_for_language(std::string_view p_language) {
		std::string lowercase(p_language);
		for (auto& character : lowercase) {
			if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
			else if (character == '_') character = '-';
		}
		std::string_view tag(lowercase);
		const auto language = consume_subtag(tag);
		std::string_view region;
		bool old_german_spelling = false;
		while (!tag.empty()) {
			const auto subtag = consume_subtag(tag);
			if (subtag.size() < 2) break;
			if (subtag.size() == 2) region = subtag;
			const bool script = subtag.size() == 4 && subtag.front() >= 'a' && subtag.front() <= 'z';
			if (script && subtag != (language == "uk" ? "cyrl" : "latn")) return nullptr;
			old_german_spelling |= subtag == "1901";
		}
		if (language == "en") {
			constexpr std::string_view american_regions[] = { "as", "ca", "gu", "mh", "mp", "pr", "um", "us", "vi" };
			return find_hyphenation_patterns(std::find(std::begin(american_regions), std::end(american_regions), region) !=
				std::end(american_regions) ? "en-us" : "en-gb");
		}
		if (language == "de") {
			if (!old_german_spelling) return find_hyphenation_patterns("de-1996");
			return find_hyphenation_patterns(region == "ch" || region == "li" ? "de-ch-1901" : "de-1901");
		}
		return find_hyphenation_patterns(language);
	}

	std::vector<int64_t> automatic_hyphenation_points(const HyphenationPatternData& p_patterns, std::u32string_view p_word) {
		const Bytes data{ p_patterns.bytes, p_patterns.size };
		const size_t suffix = p_patterns.locale.substr(0, 3) == "en-" || p_patterns.locale == "fr" ||
			p_patterns.locale == "ga" || p_patterns.locale == "pt" ? 3 : 2;
		if (p_word.size() < 2 + suffix || p_word.size() > MAX_WORD_LENGTH * 2 || data.size < 24 ||
			data.u32(0) != HYB_MAGIC || data.u32(4) != 0 || data.u32(20) != data.size) return {};
		const auto alphabet_start = data.u32(8);
		const auto trie_start = data.u32(12);
		const auto pattern_start = data.u32(16);
		if (alphabet_start < 24 || alphabet_start >= trie_start || trie_start >= pattern_start || pattern_start >= data.size) return {};
		const auto alphabet = data.section(alphabet_start, trie_start);
		const auto trie = data.section(trie_start, pattern_start);
		const auto patterns = data.section(pattern_start, data.size);
		if (trie.size < 24 || trie.u32(0) != 0 || patterns.size < 16 || patterns.u32(0) != 0) return {};
		const auto character_mask = trie.u32(4);
		const auto link_shift = trie.u32(8);
		const auto link_mask = trie.u32(12);
		const auto pattern_shift = trie.u32(16);
		const auto trie_count = trie.u32(20);
		const PatternTable table{ patterns, patterns.u32(4), patterns.u32(8), patterns.u32(12) };
		if (link_shift >= 32 || pattern_shift >= 32 || trie_count > (trie.size - 24) / 4 || table.pool < 16 ||
			table.pool > patterns.size || table.count > (table.pool - 16) / 4 || table.size > patterns.size - table.pool) return {};

		std::array<uint32_t, MAX_WORD_LENGTH + 2> letters{};
		std::array<int64_t, MAX_WORD_LENGTH + 1> source_offset;
		size_t word_length = 0;
		for (size_t position = 0; position < p_word.size();) {
			const auto code = letter_code(alphabet, canonical_letter(p_word, position));
			if (!code || code > character_mask || word_length == MAX_WORD_LENGTH) return {};
			letters[++word_length] = code;
			source_offset[word_length] = static_cast<int64_t>(position);
		}
		if (word_length < 2 + suffix) return {};

		// Liang's patterns contribute weights between characters. The greatest weight wins, with odd values denoting breaks.
		std::array<uint8_t, MAX_WORD_LENGTH + 1> weights{};
		for (size_t start = 0; start <= word_length; ++start) {
			size_t node = 0;
			for (size_t end = start; end < word_length + 2; ++end) {
				const auto code = letters[end];
				const size_t edge = node + code;
				if (edge >= trie_count) break;
				const auto transition = trie.u32(24 + edge * 4);
				if ((transition & character_mask) != code) break;
				node = (transition & link_mask) >> link_shift;
				if (!node || node >= trie_count) break;
				const auto pattern = trie.u32(24 + node * 4) >> pattern_shift;
				table.apply(pattern, static_cast<int>(end), word_length, weights);
			}
		}
		std::vector<int64_t> result;
		for (size_t index = 2; index <= word_length - suffix; ++index) {
			if (weights[index] & 1) result.push_back(source_offset[index]);
		}
		return result;
	}
}
