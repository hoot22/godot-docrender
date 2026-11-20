#include "html_text_autospace.h"

#include "html_line_break.h"
#include "formats/html/css/css_litehtml_dom.h"

#include <litehtml/document_container.h>
#include <litehtml/el_space.h>
#include <litehtml/iterators.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <string_view>

namespace litehtml {
	namespace {
		enum class TextClass { other, ideograph, letter, digit, mark, other_mark, punctuation, separator };
		enum class Space { none, interscript, narrow_no_break, no_break };

		TextClass classify(char32_t p_character) {
			struct Range { char32_t first; char32_t last; TextClass kind; };
			static constexpr Range classes[] = {
				#include "unicode_text_autospace.inc"
			};
			const auto* match = std::lower_bound(std::begin(classes), std::end(classes), p_character,
				[](const Range& p_range, char32_t p_value) { return p_range.last < p_value; });
			return match != std::end(classes) && match->first <= p_character ? match->kind : TextClass::other;
		}

		char32_t decode_edge(std::string_view p_text, bool p_back) {
			while (!p_text.empty()) {
				size_t start = p_back ? p_text.size() - 1 : 0;
				while (start > 0 && (static_cast<unsigned char>(p_text[start]) & 0xc0) == 0x80) --start;
				const auto first = static_cast<unsigned char>(p_text[start]);
				if (first < 128) return first;
				const int count = first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
				if (p_text.size() - start < static_cast<size_t>(count)) return 0;
				const auto character = godot::String::utf8(p_text.data() + start, count)[0];
				if (!p_back || start == 0 || !is_one_of(classify(character), TextClass::mark, TextClass::other_mark)) {
					return character;
				}
				p_text = p_text.substr(0, start);
			}
			return 0;
		}

		bool is_french(const element& p_owner) {
			const char* language = get_element_language(p_owner);
			if (!language) return false;
			const std::string_view value(language);
			return (value.size() == 2 || (value.size() > 2 && value[2] == '-')) &&
				(value[0] == 'f' || value[0] == 'F') && (value[1] == 'r' || value[1] == 'R');
		}

		bool punctuation_space(Space p_space) {
			return p_space == Space::narrow_no_break || p_space == Space::no_break;
		}

		Space spacing_at(const element& p_owner, char32_t p_left, char32_t p_right) {
			if (!p_left || !p_right) return Space::none;
			const int flags = p_owner.css().get_text_autospace();
			if (flags == 0 || (p_left < 128 && p_right < 128 && !(flags & text_autospace_punctuation))) {
				return Space::none;
			}
			const auto left = classify(p_left);
			const auto right = classify(p_right);
			if (left == TextClass::separator || right == TextClass::separator) return Space::none;
			if ((flags & text_autospace_punctuation) && is_french(p_owner)) {
				if (p_left == U'«' || p_right == U'»') return Space::no_break;
				if (!is_one_of(p_left, U':', U';', U'!', U'?')) {
					if (p_right == U':') return Space::no_break;
					if (is_one_of(p_right, U';', U'!', U'?')) return Space::narrow_no_break;
				}
			}
			if ((left == TextClass::ideograph) == (right == TextClass::ideograph)) return Space::none;
			const auto other = left == TextClass::ideograph ? right : left;
			if ((is_one_of(other, TextClass::letter, TextClass::mark) && (flags & text_autospace_alpha)) ||
				(other == TextClass::digit && (flags & text_autospace_numeric))) return Space::interscript;
			return Space::none;
		}

		Space spacing_between(const element& p_owner, const element& p_before, const element& p_after) {
			const auto* before = dynamic_cast<const el_text*>(&p_before);
			const auto* after = dynamic_cast<const el_text*>(&p_after);
			if (!before || !after || before->is_space() || after->is_space() || after->is_other_space_separator() ||
				before->is_other_space_separator() || p_owner.css().get_text_autospace() == 0) return Space::none;
			const auto space = spacing_at(p_owner, decode_edge(before->get_rendered_text(), true), decode_edge(after->get_rendered_text(), false));
			return space == Space::none || is_grapheme_boundary(p_before, p_after) ? space : Space::none;
		}

		pixel_t measure_space(const element& p_owner, Space p_space) {
			const auto& metrics = p_owner.css().get_font_metrics();
			switch (p_space) {
				case Space::interscript:
					return metrics.ic_width / 8;
				case Space::narrow_no_break:
				case Space::no_break: {
					const auto font = p_owner.css().get_font();
					if (!font) return 0;
					const char* space = p_space == Space::narrow_no_break ? "\xe2\x80\xaf" : "\xc2\xa0";
					return p_owner.get_document()->container()->text_width(space, font);
				}
				case Space::none:
					return 0;
			}
			return 0;
		}
	}

	std::shared_ptr<element> inline_boundary_owner(const element& p_before, const element& p_after) {
		const auto right_parent = p_after.parent();
		for (auto left = p_before.parent(); left; left = left->parent()) {
			for (auto right = right_parent; right; right = right->parent()) {
				if (left == right) return left;
			}
		}
		return {};
	}

	bool separates_autospace(const render_item& p_inline, bool p_start, pixel_t p_basis) {
		const auto& style = p_inline.css();
		const auto& padding = style.get_padding();
		const auto& margins = style.get_margins();
		const auto& borders = style.get_borders();
		const auto& border = p_start ? borders.left : borders.right;
		return (p_start ? padding.left : padding.right).calc_percent(p_basis) != 0 ||
			(p_start ? margins.left : margins.right).calc_percent(p_basis) != 0 ||
			(!is_one_of(border.style, border_style_none, border_style_hidden) && border.width.calc_percent(p_basis) != 0);
	}

	bool is_autospace_fragment_boundary(const element& p_owner, const godot::String& p_text, int64_t p_offset) {
		if (p_offset <= 0 || p_offset >= p_text.length()) return false;
		int64_t before = p_offset - 1;
		while (before > 0 && is_one_of(classify(p_text[before]), TextClass::mark, TextClass::other_mark)) --before;
		return spacing_at(p_owner, p_text[before], p_text[p_offset]) != Space::none;
	}

	pixel_t text_autospace_gap(const element& p_before, const element& p_after) {
		const auto owner = inline_boundary_owner(p_before, p_after);
		return owner ? measure_space(*owner, spacing_between(*owner, p_before, p_after)) : 0;
	}

	bool has_non_breaking_autospace(const element& p_before, const element& p_after) {
		const auto* before = dynamic_cast<const el_space*>(&p_before);
		const auto* after = dynamic_cast<const el_space*>(&p_after);
		if ((before && before->has_non_breaking_autospace()) || (after && after->has_non_breaking_autospace())) return true;
		const auto owner = inline_boundary_owner(p_before, p_after);
		return owner && (owner->css().get_text_autospace() & text_autospace_punctuation) &&
			punctuation_space(spacing_between(*owner, p_before, p_after));
	}

	void prepare_autospace_replacements(const std::shared_ptr<render_item>& p_root, pixel_t p_basis) {
		go_inside_inline descend;
		inline_selector select;
		elements_iterator iterator(true, &descend, &select);
		const element* before = nullptr;
		std::vector<el_space*> spaces;
		iterator.process(p_root, [&](const std::shared_ptr<render_item>& p_item, iterator_item_type p_type) {
			if (p_type != iterator_item_type_child) {
				if (separates_autospace(*p_item, p_type == iterator_item_type_start_parent, p_basis)) {
					before = nullptr;
					spaces.clear();
				}
				return;
			}
			auto& source = *p_item->src_el();
			if (auto* space = dynamic_cast<el_space*>(&source)) {
				space->set_autospace_width({}, false);
				if (space->is_space() && space->css().get_white_space_collapse() == white_space_discard) return;
				if (before && space->get_source_text() == " ") {
					spaces.push_back(space);
					return;
				}
			}
			if (before && !spaces.empty()) {
				const auto owner = inline_boundary_owner(*before, source);
				const bool collapsible = spaces.size() == 1 || std::all_of(spaces.begin(), spaces.end(),
					[](const el_space* p_space) { return p_space->is_white_space(); });
				if (owner && collapsible && (owner->css().get_text_autospace() & text_autospace_replace)) {
					const auto space = spacing_between(*owner, *before, source);
					if (space != Space::none) {
						const auto word_spacing = spaces.front()->css().get_font_metrics().word_spacing;
						// NBSP already carries the boundary owner's word spacing when measured.
						const auto preserved = word_spacing - (space == Space::no_break ?
							owner->css().get_font_metrics().word_spacing : 0);
						const auto width = std::max(pixel_t(0), measure_space(*owner, space) + preserved);
						for (auto* original : spaces) original->set_autospace_width(width, punctuation_space(space));
					}
				}
			}
			spaces.clear();
			before = &source;
		});
	}
}
