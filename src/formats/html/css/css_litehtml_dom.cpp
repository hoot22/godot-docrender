#include "css_litehtml_dom.h"
#include "formats/html/elements/html_form_state.h"

#include <litehtml/html_tag.h>

#include <algorithm>
#include <cctype>

namespace litehtml {

namespace syntax = dockit::css;
using syntax::SelectorNode;
using syntax::SelectorNodeKind;

namespace {

const element& as_element(SelectorNode node) {
	return *static_cast<const element*>(node);
}

bool is_rtl_codepoint(uint32_t p_codepoint) {
	return (p_codepoint >= 0x05D0 && p_codepoint <= 0x05EA) ||
		(p_codepoint >= 0x05EF && p_codepoint <= 0x05F2) ||
		(p_codepoint >= 0x0620 && p_codepoint <= 0x064A) ||
		(p_codepoint >= 0x066E && p_codepoint <= 0x066F) ||
		(p_codepoint >= 0x0671 && p_codepoint <= 0x06D3) || p_codepoint == 0x06D5 ||
		(p_codepoint >= 0x06E5 && p_codepoint <= 0x06E6) ||
		(p_codepoint >= 0x06EE && p_codepoint <= 0x06EF) ||
		(p_codepoint >= 0x06FA && p_codepoint <= 0x06FC) || p_codepoint == 0x06FF ||
		(p_codepoint >= 0x0710 && p_codepoint <= 0x072F) ||
		(p_codepoint >= 0x074D && p_codepoint <= 0x074F) ||
		(p_codepoint >= 0x0780 && p_codepoint <= 0x07A5) ||
		(p_codepoint >= 0x07CA && p_codepoint <= 0x07EA) ||
		(p_codepoint >= 0x0800 && p_codepoint <= 0x0815) ||
		(p_codepoint >= 0x0840 && p_codepoint <= 0x0858) ||
		(p_codepoint >= 0x08A0 && p_codepoint <= 0x08C9) ||
		(p_codepoint >= 0xFB1D && p_codepoint <= 0xFDFF) ||
		(p_codepoint >= 0xFE70 && p_codepoint <= 0xFEFF) ||
		(p_codepoint >= 0x10800 && p_codepoint <= 0x10FFF) ||
		(p_codepoint >= 0x1E800 && p_codepoint <= 0x1E95F) ||
		(p_codepoint >= 0x1EE00 && p_codepoint <= 0x1EEFF);
}

bool is_ltr_codepoint(uint32_t p_codepoint) {
	// Skip common punctuation and numbers so dir=auto follows the first recognized strong letter.
	return (p_codepoint >= 0x0041 && p_codepoint <= 0x005A) ||
		(p_codepoint >= 0x0061 && p_codepoint <= 0x007A) ||
		(p_codepoint >= 0x00C0 && p_codepoint <= 0x058F) ||
		(p_codepoint >= 0x0900 && p_codepoint <= 0x1E7FF) ||
		(p_codepoint >= 0x2C00 && p_codepoint <= 0xA7FF) ||
		(p_codepoint >= 0xAC00 && p_codepoint <= 0xD7AF) ||
		(p_codepoint >= 0xF900 && p_codepoint <= 0xFAFF);
}

std::string auto_direction(const element& p_element) {
	string text;
	p_element.get_text(text);
	for (size_t index = 0; index < text.size();) {
		const uint8_t lead = static_cast<uint8_t>(text[index]);
		uint32_t codepoint = lead;
		size_t length = 1;
		if ((lead & 0xE0) == 0xC0 && index + 1 < text.size()) {
			codepoint = lead & 0x1F;
			length = 2;
		}
		else if ((lead & 0xF0) == 0xE0 && index + 2 < text.size()) {
			codepoint = lead & 0x0F;
			length = 3;
		}
		else if ((lead & 0xF8) == 0xF0 && index + 3 < text.size()) {
			codepoint = lead & 0x07;
			length = 4;
		}
		for (size_t offset = 1; offset < length; ++offset) {
			codepoint = (codepoint << 6) | (static_cast<uint8_t>(text[index + offset]) & 0x3F);
		}
		if (is_rtl_codepoint(codepoint)) return "rtl";
		if (is_ltr_codepoint(codepoint)) return "ltr";
		index += length;
	}
	return "ltr";
}

} // namespace

std::string compute_auto_direction(const element& p_element) {
	return auto_direction(p_element);
}

struct selector_dom final : syntax::SelectorDOM {
	SelectorNode parent(SelectorNode node) const override {
		return as_element(node).parent().get();
	}

	void visit_children(SelectorNode node, bool reverse,
		const std::function<bool(SelectorNode)>& visit) const override {
		const auto& children = as_element(node).children();
		if (reverse) {
			for (auto child = children.rbegin(); child != children.rend(); ++child) {
				if (visit(child->get())) return;
			}
		} else {
			for (const auto& child : children) {
				if (visit(child.get())) return;
			}
		}
	}

	SelectorNodeKind kind(SelectorNode node) const override {
		const auto& source = as_element(node);
		const auto tag = source.tag();
		if (is_generated_tag(tag)) return SelectorNodeKind::generated;
		if (tag != empty_id) return SelectorNodeKind::element;
		if (source.is_text()) return source.is_space() ? SelectorNodeKind::whitespace : SelectorNodeKind::text;
		return SelectorNodeKind::other;
	}

	bool is_matchable(SelectorNode node) const override {
		return dynamic_cast<const html_tag*>(&as_element(node)) != nullptr;
	}

	std::string_view tag(SelectorNode node) const override { return _s(as_element(node).tag()); }
	bool same_tag(SelectorNode left, SelectorNode right) const override {
		return as_element(left).tag() == as_element(right).tag();
	}
	std::string_view id(SelectorNode node) const override { return _s(as_element(node).id()); }

	bool has_class(SelectorNode node, std::string_view name) const override {
		const auto* tag = dynamic_cast<const html_tag*>(&as_element(node));
		if (!tag) return false;
		const auto& classes = tag->str_classes();
		return std::find(classes.begin(), classes.end(), name) != classes.end();
	}

	std::optional<std::string_view> attribute(SelectorNode node, const std::string& name) const override {
		if (const auto* value = as_element(node).get_attr(name.c_str())) return std::string_view(value);
		return {};
	}

	bool matches_direction(SelectorNode node, std::string_view p_direction) const override {
		for (const element* current = &as_element(node); current;) {
			const char* attr = current->get_attr("dir");
			std::string direction = attr ? std::string(attr) : std::string();
			std::transform(direction.begin(), direction.end(), direction.begin(), [](unsigned char p_char) {
				return static_cast<char>(std::tolower(p_char));
			});
			if (direction.empty() && current->tag() == _id("bdi")) direction = "auto";
			if (direction == "ltr" || direction == "rtl") return direction == p_direction;
			if (direction == "auto") return auto_direction(*current) == p_direction;
			const auto parent = current->parent();
			current = parent.get();
		}
		return p_direction == "ltr";
	}

	bool has_pseudo_class(SelectorNode node, std::string_view name) const override {
		const auto* tag = dynamic_cast<const html_tag*>(&as_element(node));
		if (!tag) return false;
		if (name == "required" || name == "optional") {
			return dockit::forms::supports_required(*tag) && (name == "required") == bool(tag->get_attr("required"));
		}
		if (name == "read-only" || name == "read-write") {
			return kind(node) == SelectorNodeKind::element && (name == "read-write") == dockit::forms::is_read_write(*tag);
		}
		return std::any_of(tag->m_pseudo_classes.begin(), tag->m_pseudo_classes.end(),
			[&](string_id pseudo) { return _s(pseudo) == name; });
	}
};

const syntax::SelectorDOM& selector_dom_view() {
	static const selector_dom dom;
	return dom;
}

int selector_match_flags(unsigned result) {
	if (result == syntax::match_none) return select_no_match;
	int flags = select_match;
	if (result & syntax::match_deferred_pseudo_class) flags |= select_match_pseudo_class;
	if (result & syntax::match_before) flags |= select_match_with_before;
	if (result & syntax::match_after) flags |= select_match_with_after;
	if (result & syntax::match_marker) flags |= select_match_with_marker;
	if (result & syntax::match_placeholder) flags |= select_match_with_placeholder;
	if (result & syntax::match_selection) flags |= select_match_with_selection;
	if (result & syntax::match_backdrop) flags |= select_match_with_backdrop;
	return flags;
}

const char* get_element_language(const element& node) {
	const auto language = syntax::inherited_language(selector_dom_view(), &node);
	return language ? language->data() : nullptr;
}

bool matches_element_language(const element& node, const std::vector<std::string>& ranges) {
	return syntax::matches_language(selector_dom_view(), &node, ranges);
}

} // namespace litehtml
