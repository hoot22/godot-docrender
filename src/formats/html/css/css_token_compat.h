#pragma once

#include "css_tokenizer.h"
#include "css_types.h"

namespace litehtml {

using dockit::css::css_token_type;
using dockit::css::css_number_type;
using dockit::css::css_hash_type;
using dockit::css::css_token;
using dockit::css::css_token_vector;
using dockit::css::css_tokenizer;
using dockit::css::get_repr;
using dockit::css::css_parse_error;
using dockit::css::tokenize;

inline constexpr auto WHITESPACE = dockit::css::WHITESPACE;
inline constexpr auto _EOF = dockit::css::_EOF;
inline constexpr auto LEFT_BRACE = dockit::css::LEFT_BRACE;
inline constexpr auto RIGHT_BRACE = dockit::css::RIGHT_BRACE;
inline constexpr auto LEFT_BRACKET = dockit::css::LEFT_BRACKET;
inline constexpr auto RIGHT_BRACKET = dockit::css::RIGHT_BRACKET;
inline constexpr auto LEFT_PAREN = dockit::css::LEFT_PAREN;
inline constexpr auto RIGHT_PAREN = dockit::css::RIGHT_PAREN;
inline constexpr auto COLON = dockit::css::COLON;
inline constexpr auto SEMICOLON = dockit::css::SEMICOLON;
inline constexpr auto COMMA = dockit::css::COMMA;
inline constexpr auto BANG = dockit::css::BANG;
inline constexpr auto DOT = dockit::css::DOT;
inline constexpr auto AMPERSAND = dockit::css::AMPERSAND;
inline constexpr auto IDENT = dockit::css::IDENT;
inline constexpr auto FUNCTION = dockit::css::FUNCTION;
inline constexpr auto AT_KEYWORD = dockit::css::AT_KEYWORD;
inline constexpr auto HASH = dockit::css::HASH;
inline constexpr auto STRING = dockit::css::STRING;
inline constexpr auto BAD_STRING = dockit::css::BAD_STRING;
inline constexpr auto URL = dockit::css::URL;
inline constexpr auto BAD_URL = dockit::css::BAD_URL;
inline constexpr auto NUMBER = dockit::css::NUMBER;
inline constexpr auto PERCENTAGE = dockit::css::PERCENTAGE;
inline constexpr auto DIMENSION = dockit::css::DIMENSION;
inline constexpr auto CDO = dockit::css::CDO;
inline constexpr auto CDC = dockit::css::CDC;
inline constexpr auto CV_FUNCTION = dockit::css::CV_FUNCTION;
inline constexpr auto CURLY_BLOCK = dockit::css::CURLY_BLOCK;
inline constexpr auto ROUND_BLOCK = dockit::css::ROUND_BLOCK;
inline constexpr auto SQUARE_BLOCK = dockit::css::SQUARE_BLOCK;
inline constexpr auto css_number_integer = dockit::css::css_number_integer;
inline constexpr auto css_number_number = dockit::css::css_number_number;
inline constexpr auto css_hash_unrestricted = dockit::css::css_hash_unrestricted;
inline constexpr auto css_hash_id = dockit::css::css_hash_id;

} // namespace litehtml
