#include <litehtml/css_parser.h>
#include <litehtml/encodings.h>

namespace litehtml {

raw_rule::vector css_parser::parse_stylesheet(const string& input, bool top_level) {
	return dockit::css::css_parser::parse_stylesheet(decode(input, encoding::utf_8), top_level);
}

} // namespace litehtml
