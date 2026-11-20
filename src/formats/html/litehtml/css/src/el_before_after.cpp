#include "html.h"
#include "el_before_after.h"
#include "document.h"
#include "document_container.h"
#include "el_image.h"
#include "el_space.h"
#include "el_text.h"
#include "formats/html/css/css_litehtml_dom.h"
#include "formats/html/backend/litehtml_document_adapter.h"

namespace litehtml
{
namespace
{
	const string_vector& quote_pairs(const element& node)
	{
		static const string_vector english{ "\xe2\x80\x9c", "\xe2\x80\x9d", "\xe2\x80\x98", "\xe2\x80\x99" };
		static const string_vector french{ "\xc2\xab\xc2\xa0", "\xc2\xa0\xc2\xbb", "\xe2\x80\xb9\xc2\xa0", "\xc2\xa0\xe2\x80\xba" };
		static const string_vector german{ "\xe2\x80\x9e", "\xe2\x80\x9c", "\xe2\x80\x9a", "\xe2\x80\x98" };
		static const string_vector japanese{ "\xe3\x80\x8c", "\xe3\x80\x8d", "\xe3\x80\x8e", "\xe3\x80\x8f" };
		static const string_vector none;
		static const string_vector fr{ "fr" }, de{ "de" }, ja{ "ja" };

		const css_quotes* quotes = &node.css().get_quotes();
		auto language = node.parent();
		while (quotes->kind == css_quotes_kind::match_parent && language)
		{
			quotes = &language->css().get_quotes();
			const auto ancestor = language->parent();
			if (!ancestor) break;
			language = ancestor;
		}
		if (quotes->kind == css_quotes_kind::pairs) return quotes->pairs;
		if (quotes->kind == css_quotes_kind::none) return none;
		const auto& language_node = language ? *language : node;
		if (matches_element_language(language_node, fr)) return french;
		if (matches_element_language(language_node, de)) return german;
		if (matches_element_language(language_node, ja)) return japanese;
		return english;
	}

}

el_before_after_base::el_before_after_base(const document::ptr& doc, string_id tag) : html_tag(doc)
{
	m_tag = tag;
}

el_marker::el_marker(const document::ptr& doc) : el_before_after_base(doc, marker_tag())
{
}

void el_marker::compute_styles(bool recursive)
{
	html_tag::compute_styles(false);
	const auto list_item = parent();
	css_w().set_display(display_none);
	if (!list_item || list_item->css().get_display() != display_list_item || list_item->css().get_content_image()) return;
	for (auto ancestor = list_item->parent(); ancestor; ancestor = ancestor->parent())
		if (ancestor->css().get_display() == display_none || ancestor->css().get_content_image()) return;

	const auto& content = css().get_content();
	css_generated_content fallback;
	bool list_marker = false;
	if (content.normal)
	{
		const auto& list_css = list_item->css();
		const auto& value = list_css.get_list_style_image();
		const auto& base = list_css.get_list_style_image_baseurl();
		auto* container = get_document()->container();
		if (value && (value->type != image::type_url ||
			container->is_image_available(value->url.c_str(), base.c_str(), value->orientation)))
		{
			list_marker = true;
			fallback.base_url = base;
			fallback.items.push_back({css_content_kind::image, {}, {}, {}, value});
			fallback.items.push_back({ css_content_kind::text, " ", {} });
		}
		else
		{
			const auto& marker = list_css.get_list_marker_type();
			if (marker.text)
				fallback.items.push_back({ css_content_kind::text, *marker.text, {} });
			else if (marker.counter.name != "none")
				fallback.items.push_back({ css_content_kind::marker_counter, "list-item", {}, marker.counter });
		}
	}
	const auto& effective = content.normal ? fallback : content;
	if (effective.items.empty()) return;
	css_w().prepare_marker(list_item->css().get_list_style_position() == list_style_position_outside);
	compute_content_alternative();
	resolve_content(effective, list_marker);
	if (recursive) compute_child_styles();
}

void el_before_after_base::compute_styles(bool recursive)
{
	html_tag::compute_styles(false);
	const auto& content = css().get_content();
	if (content.items.empty())
		css_w().set_display(display_none);

	if (css().get_display() == display_none) return;
	for (auto ancestor = parent(); ancestor; ancestor = ancestor->parent())
	{
		if (ancestor->css().get_display() == display_none || ancestor->css().is_replaced()) return;
	}
	handle_counter_properties();
	compute_content_alternative();
	if (content_image()) {
		if (!materialized_content.empty()) {
			clearRecursive();
			materialized_content.clear();
			materialized_images.clear();
		}
		return;
	}
	resolve_content(content);
	if (recursive) compute_child_styles();
}

void el_before_after_base::resolve_content(const css_generated_content& content, bool list_marker)
{
	using kind = css_content_kind;
	const auto doc = get_document();
	std::vector<ContentPart> parts;
	string text;
	for (const auto& item : content.items)
	{
		if (append_content_text(item, text)) continue;
		switch (item.kind)
		{
		case kind::image:
			if (!text.empty()) parts.emplace_back(std::move(text));
			text.clear();
			parts.emplace_back(item.image_value);
			break;
		case kind::open_quote:
		case kind::close_quote:
		case kind::no_open_quote:
		case kind::no_close_quote:
		{
			const bool open = item.kind == kind::open_quote || item.kind == kind::no_open_quote;
			const std::optional<size_t> depth = open ? std::optional<size_t>(doc->open_quote()) : doc->close_quote();
			if (depth && (item.kind == kind::open_quote || item.kind == kind::close_quote))
			{
				const auto& pairs = quote_pairs(*this);
				if (!pairs.empty())
					text += pairs[std::min(*depth, pairs.size() / 2 - 1) * 2 + (open ? 0 : 1)];
			}
			break;
		}
		default: break;
		}
	}
	if (!text.empty()) parts.emplace_back(std::move(text));

	if (std::equal(parts.begin(), parts.end(), materialized_content.begin(), materialized_content.end(),
		[](const ContentPart& first, const ContentPart& second) {
			if (const auto* text = std::get_if<string>(&first))
				return std::holds_alternative<string>(second) && *text == std::get<string>(second);
			return std::holds_alternative<css_image_ptr>(second);
		})) {
		size_t index = 0;
		for (const auto& part : parts) {
			if (const auto* value = std::get_if<css_image_ptr>(&part)) {
				if (const auto node = materialized_images[index++].lock())
					node->set_generated_image(**value, content.base_url, list_marker);
			}
		}
		return;
	}
	materialized_content = std::move(parts);
	materialized_images.clear();
	clearRecursive();
	for (const auto& part : materialized_content)
	{
		if (const auto* value = std::get_if<css_image_ptr>(&part)) add_image(**value, content.base_url, list_marker);
		else add_text(std::get<string>(part));
	}
}

void el_before_after_base::add_text(const string& text)
{
	for (const auto& run : dockit_editor::create_text(get_document(), text.c_str(), true))
		appendChild(run);
}

void el_before_after_base::add_image(const image& value, const string& base, bool list_marker)
{
	const auto node = std::make_shared<el_image>(get_document());
	node->set_generated_image(value, base, list_marker);
	node->set_attr("style", "display:inline-block");
	node->set_tagName("img");
	appendChild(node);
	materialized_images.emplace_back(node);
}
} // namespace litehtml
