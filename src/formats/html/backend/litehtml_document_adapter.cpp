#include "litehtml_document_adapter.h"
#include "formats/html/elements/html_stylesheet_node.h"
#include "formats/html/dom/html_dom.h"
#include "formats/html/elements/html_select_node.h"

#include "formats/html/css/css_selector_matcher.h"
#include "formats/html/css/css_shadows.h"
#include "formats/html/css/css_default_stylesheet.h"
#include "formats/html/css/css_litehtml_dom.h"
#include "litehtml_container.h"

#include "formats/html/elements/html_form_state.h"

#include <gumbo.h>
#include <godot_cpp/classes/xml_parser.hpp>
#include <litehtml/css_parser.h>
#include <litehtml/css_selector.h>
#include <litehtml/el_space.h>
#include <litehtml/html_tag.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace litehtml {
	using dockit::LiteHTMLContainer;

	namespace {
		elements_list::iterator child_insert_position(elements_list& p_children, const element::ptr& p_reference) {
			if (p_reference) {
				return std::find(p_children.begin(), p_children.end(), p_reference);
			}
			if (!p_children.empty() && p_children.back()->tag() == __tag_after_) {
				return std::prev(p_children.end());
			}
			return p_children.end();
		}
	}

	document::ptr dockit_editor::parse(const godot::CharString& p_html, LiteHTMLContainer& p_container,
		const string& p_default_styles, const string& p_user_styles) {
		auto doc = parse_tree(p_html, p_container);
		if (doc) {
			p_container.get_dom().normalize_details(doc->root(), {}, false, false);
			p_container.refresh_document_base(doc->root());
			restyle(doc, p_container, p_default_styles, p_user_styles);
		}
		return doc;
	}

	document::ptr dockit_editor::parse_tree(const godot::CharString& p_html, document_container& p_container) {
		auto doc = std::make_shared<document>(&p_container);
		const auto destroy = [](GumboOutput* p_output) { gumbo_destroy_output(&kGumboDefaultOptions, p_output); };
		// Godot has already decoded the source, regardless of the HTML meta charset.
		const std::unique_ptr<GumboOutput, decltype(destroy)> output(doc->parse_html(
			estring(string(p_html.get_data(), p_html.length()), encoding::utf_8)), destroy);
		if (!output || !output->document || !output->root) {
			return {};
		}
		switch (output->document->v.document.doc_type_quirks_mode) {
		case GUMBO_DOCTYPE_NO_QUIRKS: doc->m_mode = no_quirks_mode; break;
		case GUMBO_DOCTYPE_QUIRKS: doc->m_mode = quirks_mode; break;
		case GUMBO_DOCTYPE_LIMITED_QUIRKS: doc->m_mode = limited_quirks_mode; break;
		}
		// The mode determines how node creation normalizes class and ID selectors.
		elements_list roots;
		doc->create_node(output->root, roots, true, true);
		if (roots.empty()) {
			return {};
		}
		doc->m_root = std::move(roots.back());
		doc->m_root->set_pseudo_class(_root_, true);
		return doc;
	}

	document::ptr dockit_editor::parse_svg_clip_source(const godot::String& p_svg, LiteHTMLContainer& p_container) {
		using namespace godot;
		Ref<XMLParser> parser;
		parser.instantiate();
		const auto bytes = p_svg.to_utf8_buffer();
		if (parser->open_buffer(bytes) != OK) return {};
		auto doc = std::make_shared<document>(&p_container);
		doc->m_mode = no_quirks_mode;
		struct Frame {
			String name;
			Dictionary namespaces;
			element::ptr node;
		};
		std::vector<Frame> frames;
		Error status;
		while ((status = parser->read()) == OK) {
			switch (parser->get_node_type()) {
				case XMLParser::NODE_ELEMENT: {
					if (frames.size() >= 128 || (frames.empty() && doc->m_root)) return {};
					const String name = parser->get_node_name();
					const int64_t colon = name.find(":");
					const String local = colon >= 0 ? name.substr(colon + 1) : name;
					auto namespaces = frames.empty() ? Dictionary() : frames.back().namespaces;
					bool changed_namespaces = false;
					for (int i = 0; i < parser->get_attribute_count(); ++i) {
						const String attribute = parser->get_attribute_name(i);
						if (attribute != "xmlns" && !attribute.begins_with("xmlns:")) continue;
						if (!changed_namespaces) {
							namespaces = namespaces.duplicate();
							changed_namespaces = true;
						}
						namespaces[attribute == "xmlns" ? String() : attribute.substr(6)] = parser->get_attribute_value(i);
					}
					const String ns = namespaces.get(colon >= 0 ? name.substr(0, colon) : String(), String());
					const bool svg = ns == "http://www.w3.org/2000/svg";
					if (frames.empty() && (!svg || local != "svg")) return {};
					element::ptr node;
					if (svg && (frames.empty() || frames.back().node)) {
						string_map attributes;
						for (int i = 0; i < parser->get_attribute_count(); ++i) {
							String attr = parser->get_attribute_name(i);
							if (attr == "xmlns" || attr.begins_with("xmlns:")) continue;
							const int64_t separator = attr.find(":");
							if (separator >= 0 && String(namespaces.get(attr.substr(0, separator), String())) == "http://www.w3.org/1999/xlink") {
								attr = "xlink:" + attr.substr(separator + 1);
							}
							attributes[attr.utf8().get_data()] = parser->get_attribute_value(i).utf8().get_data();
						}
						node = doc->create_element(local.utf8().get_data(), attributes);
						if (!node) return {};
						if (frames.empty()) doc->m_root = node;
						else frames.back().node->appendChild(node);
					}
					if (!parser->is_empty()) frames.push_back({ name, std::move(namespaces), std::move(node) });
					break;
				}
				case XMLParser::NODE_ELEMENT_END:
					if (frames.empty() || frames.back().name != parser->get_node_name()) return {};
					frames.pop_back();
					break;
				case XMLParser::NODE_TEXT:
				case XMLParser::NODE_CDATA: {
					const String data = parser->get_node_type() == XMLParser::NODE_TEXT ?
						parser->get_node_data() : parser->get_node_name();
					if (frames.empty()) {
						if (!data.strip_edges().is_empty()) return {};
					} else if (const auto& node = frames.back().node; node &&
						is_one_of(node->tag(), _style_, _id("text"), _id("tspan"), _id("textpath"))) {
						for (const auto& text : create_text(doc, data.utf8().get_data(), false)) node->appendChild(text);
					}
					break;
				}
				default:
					break;
			}
		}
		if (status != ERR_FILE_EOF || !frames.empty() || !doc->m_root) return {};
		doc->m_root->set_pseudo_class(_root_, true);
		restyle(doc, p_container, master_css, {}, false);
		return doc;
	}

	void dockit_editor::remove_attribute(html_tag& p_element, const char* p_name) {
		const std::string_view name(p_name);
		if (name == "class" || name == "id") {
			p_element.set_attr(p_name, "");
		}
		p_element.m_attrs.erase(p_name);
	}

	elements_list dockit_editor::parse_fragment(document& p_document, const element& p_parent, const godot::CharString& p_html) {
		GumboOptions options = kGumboDefaultOptions;
		options.fragment_context = gumbo_tag_enum(p_parent.get_tagName());
		options.fragment_namespace = static_cast<GumboNamespaceEnum>(p_parent.get_namespace());
		if (options.fragment_context == GUMBO_TAG_UNKNOWN) {
			options.fragment_context = GUMBO_TAG_DIV;
		}
		const auto destroy = [&options](GumboOutput* p_output) { gumbo_destroy_output(&options, p_output); };
		const std::unique_ptr<GumboOutput, decltype(destroy)> output(
			gumbo_parse_with_options(&options, p_html.get_data(), p_html.length()), destroy);
		if (!output || !output->root) {
			throw std::runtime_error("Could not parse the HTML fragment.");
		}
		elements_list children;
		p_document.create_node(output->root, children, options.fragment_context != GUMBO_TAG_TEXTAREA, false);
		return children;
	}

	void dockit_editor::replace_children(element& p_parent, elements_list p_children) {
		for (const auto& child : p_parent.m_children) {
			child->parent(nullptr);
		}
		for (const auto& child : p_children) {
			child->parent(p_parent.shared_from_this());
		}
		p_parent.m_children.swap(p_children);
		if (p_parent.tag() == _style_) reset_stylesheet_source(p_parent);
	}

	void dockit_editor::replace_child_lists(std::vector<std::pair<element::ptr, elements_list>>& r_lists) {
		for (const auto& [parent, children] : r_lists) {
			for (const auto& old : parent->m_children) old->parent(nullptr);
		}
		for (auto& [parent, children] : r_lists) {
			for (const auto& child : children) child->parent(parent);
			parent->m_children.swap(children);
			if (parent->tag() == _style_) reset_stylesheet_source(*parent);
		}
	}

	dockit_editor::child_edit_result dockit_editor::place_child(const element::ptr& p_child, element& p_parent,
		const element::ptr& p_reference, bool p_replace_reference) {
		auto* to = dynamic_cast<html_tag*>(&p_parent);
		if (!to || (p_replace_reference && !p_reference)) {
			return child_edit_result::invalid;
		}
		auto& target_children = to->m_children;
		const auto position = child_insert_position(target_children, p_reference);
		if (p_reference && position == target_children.end()) {
			return child_edit_result::invalid;
		}

		const auto old_parent = p_child->parent();
		if (old_parent) {
			auto* from = dynamic_cast<html_tag*>(old_parent.get());
			if (!from) {
				return child_edit_result::invalid;
			}
			const auto child = std::find(from->m_children.begin(), from->m_children.end(), p_child);
			if (child == from->m_children.end()) {
				return child_edit_result::invalid;
			}
			if (from == to && (child == position || (!p_replace_reference && std::next(child) == position))) {
				return child_edit_result::unchanged;
			}
			target_children.splice(position, from->m_children, child);
		}
		else {
			target_children.insert(position, p_child);
		}
		p_child->parent(to->shared_from_this());
		if (p_replace_reference) {
			p_reference->parent(nullptr);
			target_children.erase(position);
		}
		if (old_parent && old_parent.get() != to && old_parent->tag() == _style_) reset_stylesheet_source(*old_parent);
		if (to->tag() == _style_) reset_stylesheet_source(*to);
		return child_edit_result::changed;
	}

	bool dockit_editor::insert_children(element& p_source, element& p_parent, const element::ptr& p_reference) {
		auto* to = dynamic_cast<html_tag*>(&p_parent);
		if (!to || &p_source == to) {
			return false;
		}
		auto& target_children = to->m_children;
		const auto position = child_insert_position(target_children, p_reference);
		if (p_reference && position == target_children.end()) {
			return false;
		}
		const auto parent = to->shared_from_this();
		for (const auto& child : p_source.m_children) {
			child->parent(parent);
		}
		target_children.splice(position, p_source.m_children);
		if (p_source.tag() == _style_) reset_stylesheet_source(p_source);
		if (to->tag() == _style_) reset_stylesheet_source(*to);
		return true;
	}

	void dockit_editor::reset_tree(element& p_element) {
		p_element.m_renders.clear();
		p_element.m_used_styles.clear();
		p_element.m_counter_values.clear();
		p_element.m_counter_containment_root = nullptr;
		p_element.m_children.remove_if([](const element::ptr& p_child) {
			const bool generated = is_generated_tag(p_child->tag());
			if (generated) {
				p_child->parent(nullptr);
			}
			return generated;
			});
		if (auto* tag = dynamic_cast<html_tag*>(&p_element)) {
			// Container queries match against the preceding computed styles until the cascade is resolved again.
			tag->m_style.clear(true);
			tag->m_auxiliary_pseudos.reset();
		}
		for (const auto& child : p_element.children()) {
			reset_tree(*child);
		}
	}

	bool dockit_editor::media_changed(document& p_document) {
		p_document.container()->get_media_features(p_document.m_media);
		return p_document.update_media_lists(p_document.m_media);
	}

	void dockit_editor::set_viewport_unit_size(document& p_document, const godot::Vector2i& p_size) {
		p_document.m_viewport_unit_size = {static_cast<pixel_t>(p_size.x), static_cast<pixel_t>(p_size.y)};
	}

	void dockit_editor::compute_styles(document& p_document) {
		p_document.m_uses_container_units = false;
		p_document.set_quote_depth(0);
		p_document.m_root->inherit_counters(nullptr);
		p_document.m_root->compute_styles();
	}

	void dockit_editor::remember_content_sizes(document& p_document) {
		const auto remember = [](const auto& p_visit, render_item& p_item) -> void {
			const auto& box = p_item.pos();
			p_item.src_el()->css_w().remember_content_size(box.width, box.height);
			for (const auto& child : p_item.children()) p_visit(p_visit, *child);
		};
		if (p_document.m_root_render) remember(remember, *p_document.m_root_render);
	}

	void dockit_editor::prune_unused_fonts(document& p_document) {
		if (p_document.m_fonts.size() <= p_document.m_font_prune_threshold) return;

		std::unordered_set<uint_ptr> used;
		const auto collect = [&](const element& node) {
			used.insert(node.css().get_font());
			const auto* tag = dynamic_cast<const html_tag*>(&node);
			if (const auto* placeholder = tag ? tag->get_placeholder_style() : nullptr)
				used.insert(placeholder->get_font());
		};
		const auto visit_nodes = [&](const auto& visit, const element& node) -> void {
			collect(node);
			for (const auto& child : node.children()) visit(visit, *child);
		};
		const auto visit_boxes = [&](const auto& visit, const render_item& box) -> void {
			collect(*box.src_el());
			for (const auto& child : box.children()) visit(visit, *child);
		};
		if (p_document.m_root) visit_nodes(visit_nodes, *p_document.m_root);
		if (p_document.m_root_render) visit_boxes(visit_boxes, *p_document.m_root_render);

		for (auto font = p_document.m_fonts.begin(); font != p_document.m_fonts.end();) {
			if (used.count(font->second.font)) {
				++font;
				continue;
			}
			p_document.container()->delete_font(font->second.font);
			font = p_document.m_fonts.erase(font);
		}
		p_document.m_font_prune_threshold = std::max<std::size_t>(256, p_document.m_fonts.size() + 128);
	}

	dockit_editor::ContainerUpdate dockit_editor::measure_containers(document& doc) {
		ContainerUpdate update;
		const bool has_queries = doc.m_styles.has_container_queries() || doc.m_master_css.has_container_queries();
		const bool uses_units = doc.m_uses_container_units;
		if (!has_queries && !uses_units) {
			doc.m_query_container_sizes.clear();
			return update;
		}
		decltype(doc.m_query_container_sizes) sizes;
		const auto capture_sizes = [&](const auto& visit, render_item& item) -> void {
			if (item.css().establishes_query_container_context() && !item.skip()) {
				const auto& box = item.pos();
				const std::array dimensions = {
					std::max<pixel_t>(0, box.width),
					item.css().get_container_type() == container_type::size ? std::max<pixel_t>(0, box.height) : 0.0f,
				};
				const bool inserted = sizes.try_emplace(item.src_el().get(), dimensions).second;
				if (uses_units && inserted) update.dimensions.push_back(dimensions);
			}
			for (const auto& child : item.children()) visit(visit, *child);
		};
		if (doc.m_root_render) capture_sizes(capture_sizes, *doc.m_root_render);
		update.units_changed = uses_units && sizes != doc.m_query_container_sizes;
		doc.m_query_container_sizes = std::move(sizes);
		if (!has_queries) return update;

		const auto compare = [&](const auto& visit, element& node) -> void {
			if (auto* tag = dynamic_cast<html_tag*>(&node)) {
				for (const auto& used : node.m_used_styles) {
					const auto& rule = *used->m_selector;
					if (!rule.m_container_query) continue;
					int proximity = 0;
					const bool matches = rule.is_media_valid() && tag->match_style_selector(rule, true, proximity) != select_no_match;
					update.rule_matches.push_back(matches);
					update.rules_changed |= matches != used->m_used;
				}
			}
			for (const auto& child : node.children()) visit(visit, *child);
		};
		compare(compare, *doc.m_root);
		return update;
	}

	std::array<overflow, 2> dockit_editor::propagate_viewport_overflow(document& p_document) {
		const auto& root = p_document.m_root;
		if (root->css().get_display() == display_none) return {overflow_auto, overflow_auto};

		element* source = root.get();
		const dockit::css::ComputedStyle& root_style = root->css();
		if (root->tag() == _html_ && !root_style.has_containment() &&
			root_style.get_overflow_x() == overflow_visible && root_style.get_overflow_y() == overflow_visible) {
			for (const auto& child : root->children()) {
				if (child->tag() == _body_ && child->css().get_display() != display_none) {
					if (!child->css().has_containment()) source = child.get();
					break;
				}
			}
		}

		const auto viewport_mode = [](overflow p_value) {
			if (p_value == overflow_clip) return overflow_hidden;
			return is_scrollable_overflow(p_value) ? p_value : overflow_auto;
		};
		auto& css = source->m_css;
		const dockit::css::ComputedStyle& computed = css;
		const std::array modes{viewport_mode(computed.get_overflow_x()), viewport_mode(computed.get_overflow_y())};
		css.propagate_overflow_to_viewport();
		return modes;
	}

	unsigned dockit_editor::selector_state_dependencies(const document& p_document) {
		unsigned dependencies = 0;
		for (const css* sheet : { &p_document.m_master_css, &p_document.m_styles }) {
			for (const auto& rule : sheet->selectors()) {
				dependencies |= rule->m_syntax->state_dependencies();
				if (rule->m_scope) dependencies |= rule->m_scope->state_dependencies();
			}
		}
		return dependencies;
	}

	bool dockit_editor::has_transitions(document& p_document) {
		for (const css* sheet : { &p_document.m_master_css, &p_document.m_styles }) {
			for (const auto& rule : sheet->selectors()) {
				if (!rule->m_style) continue;
				std::vector<string_id> properties;
				rule->m_style->collect_property_names(properties);
				if (std::any_of(properties.begin(), properties.end(), [](string_id p_property) {
					return _s(p_property).rfind("transition", 0) == 0;
				})) return true;
			}
		}
		const auto visit = [&](const auto& self, const element& p_element) -> bool {
			if (p_element.css().get_transitions()) return true;
			for (const auto& child : p_element.children()) {
				if (self(self, *child)) return true;
			}
			return false;
		};
		const auto root = p_document.root();
		return root && visit(visit, *root);
	}

	void dockit_editor::set_hover(element& p_element, bool p_hovered) {
		p_element.set_pseudo_class(_hover_, p_hovered);
	}

	bool dockit_editor::is_command_button(const element& p_element) {
		return dockit::forms::button_action(p_element) != dockit::forms::ButtonAction::NONE;
	}

	css_text::vector dockit_editor::stylesheets(const document& p_document) {
		auto sheets = p_document.m_css;
		for (auto& sheet : sheets) {
			const auto found = p_document.m_cssom_overrides.find(sheet.owner);
			if (found != p_document.m_cssom_overrides.end()) sheet.text = found->second.replacement;
		}
		return sheets;
	}

	void dockit_editor::edit_stylesheet(document& p_document, const element::ptr& p_owner, string p_text) {
		for (const auto& sheet : p_document.m_css) {
			if (sheet.owner.lock() == p_owner) {
				p_document.m_cssom_overrides[p_owner] = {sheet.text, sheet.baseurl, std::move(p_text)};
				return;
			}
		}
	}

	void dockit_editor::reset_stylesheet_source(element& p_owner) {
		auto* node = dynamic_cast<dockit::HTMLStylesheetNode*>(&p_owner);
		if (!node) return;
		if (const auto doc = node->get_document()) doc->m_cssom_overrides.erase(node->weak_from_this());
		++node->source_version;
		node->style_disabled = false;
		node->linked_request_path.clear();
		node->linked_path.clear();
		node->linked_text.clear();
	}

	void dockit_editor::apply_author_styles(const document::ptr& p_document, LiteHTMLContainer& p_container,
		const string& p_additional_styles) {
		auto& doc = *p_document;
		p_container.set_css_source_kind(css_source_kind::style_element);
		doc.append_document_stylesheets();
		if (!p_additional_styles.empty()) {
			p_container.set_css_source_kind(css_source_kind::stylesheet);
			doc.m_styles.parse_css_stylesheet(p_additional_styles, "", p_document);
		}
		p_container.set_css_source_kind(css_source_kind::none);
		doc.apply_document_author_styles();
	}

	void dockit_editor::restyle(const document::ptr& p_document, LiteHTMLContainer& p_container,
		const string& p_default_styles, const string& p_user_styles, bool p_build_render_tree) {
		p_container.reset_stylesheet_reads();
		p_container.begin_css_diagnostics();
		p_container.set_css_source_kind(css_source_kind::none);
		auto& doc = *p_document;
		const auto& root = doc.m_root;
		doc.m_root_render.reset();
		doc.m_top_layer_render.clear();
		doc.m_tabular_elements.clear();
		doc.m_fixed_boxes.clear();
		doc.m_size = {};
		reset_tree(*root);
		dockit::forms::synchronize(root);
		doc.m_css.clear();
		doc.m_media_lists.clear();
		doc.m_master_css = {};
		doc.m_styles = {};

		doc.container()->get_media_features(doc.m_media);
		doc.m_master_css.parse_css_stylesheet(p_default_styles, "", p_document);
		doc.m_master_css.sort_selectors(false);
		doc.update_media_lists(doc.m_media);
		root->apply_stylesheet(doc.m_master_css);
		root->parse_attributes();
		const auto apply_presentational_hints = [](const auto& p_visit, element& p_element, bool p_svg) -> void {
			const std::string_view name = p_element.get_tagName();
			const bool svg = p_svg || name == "svg";
			if (auto* tag = dynamic_cast<html_tag*>(&p_element)) {
				if (const auto* select = dynamic_cast<const dockit::HTMLSelectNode*>(tag); select && select->is_list_box()) {
					const auto rows = std::min<int64_t>(select->display_size(), 1000);
					tag->m_style.add_property(_height_, "calc(" + std::to_string(rows) + "lh + " + std::to_string(rows * 2 + 2) + "px)");
				}
				const std::string direction = lowcase(tag->get_attr("dir", ""));
				if (is_one_of(direction, "ltr", "rtl")) {
					tag->m_style.add_property(_id("direction"), direction);
				} else if (direction == "auto" || tag->tag() == _bdi_) {
					tag->m_style.add_property(_id("direction"), compute_auto_direction(*tag));
				}
				if (const char* rule = tag->get_attr("clip-rule")) {
					tag->m_style.add_property(_id("clip-rule"), rule);
				}
				if (const char* clip = svg ? tag->get_attr("clip-path") : nullptr) {
					tag->m_style.add_property(_id("clip-path"), clip);
				}
				if (svg) {
					if (const std::string_view space = tag->get_attr("xml:space", ""); is_one_of(space, "preserve", "default")) {
						tag->m_style.add_property(_white_space_, space == "preserve" ? "pre" : "normal");
					}
					for (const char* property : { "font", "font-family", "font-weight", "font-style", "font-stretch",
							"font-kerning", "font-feature-settings", "font-variation-settings", "letter-spacing",
							"word-spacing", "direction", "text-anchor", "white-space", "display", "visibility", "color",
							"mask-type", "color-interpolation" }) {
						if (const char* value = tag->get_attr(property)) tag->m_style.add_property(_id(property), value);
					}
					if (const char* size = tag->get_attr("font-size")) {
						auto tokens = normalize(size, f_componentize | f_remove_whitespace);
						if (tokens.size() == 1 && tokens.front().type == NUMBER) {
							tokens.front().type = DIMENSION;
							tokens.front().unit = "px";
						}
						tag->m_style.add_property(_font_size_, tokens);
					}
				}
				if (is_one_of(tag->tag(), _ol_, _li_)) {
					static const std::map<std::string_view, const char*> marker_types{
						{ "1", "decimal" }, { "a", "lower-alpha" }, { "A", "upper-alpha" },
						{ "i", "lower-roman" }, { "I", "upper-roman" },
					};
					const auto type = marker_types.find(tag->get_attr("type", ""));
					if (type != marker_types.end()) {
						tag->m_style.add_property(_list_style_type_, type->second);
					}
				}
			}
			for (const auto& child : p_element.children()) {
				p_visit(p_visit, *child, svg && name != "foreignobject");
			}
			};
		apply_presentational_hints(apply_presentational_hints, *root, false);
		apply_author_styles(p_document, p_container, p_user_styles);
		auto faces = doc.m_master_css.font_faces();
		const auto& author_faces = doc.m_styles.font_faces();
		faces.insert(faces.end(), author_faces.begin(), author_faces.end());
		auto feature_values = doc.m_master_css.font_feature_values();
		const auto& author_features = doc.m_styles.font_feature_values();
		feature_values.insert(feature_values.end(), author_features.begin(), author_features.end());
		if (p_build_render_tree && p_container.set_font_rules(std::move(faces), std::move(feature_values))) {
			for (const auto& entry : doc.m_fonts) doc.container()->delete_font(entry.second.font);
			doc.m_fonts.clear();
		}
		const auto prepare_shadow = [&](const auto& self, element& node) -> void {
			if (node.shadow_tree) {
				auto& root = *node.shadow_tree;
				reset_tree(root);
				root.apply_stylesheet(doc.m_master_css);
				root.parse_attributes();
				css local;
				string text = "slot { display: contents; }";
				const auto collect = [&](const auto& visit, const element& child) -> void {
					if (child.tag() == _style_) { string source; child.get_text(source); text += source; }
					for (const auto& next : child.children()) visit(visit, *next);
				};
				collect(collect, root);
				local.parse_css_stylesheet(text, "", p_document);
				local.sort_selectors(false);
				root.apply_stylesheet(local);
				self(self, root);
			}
			for (const auto& child : node.children()) self(self, *child);
		};
		prepare_shadow(prepare_shadow, *root);
		compute_styles(doc);
		if (p_build_render_tree) rebuild_render_tree(doc);
	}

	void dockit_editor::rebuild_render_tree(document& doc) {
		if (doc.m_root_render) {
			const auto clear_render_items = [](const auto& visit, element& node) -> void {
				node.m_renders.clear();
				if (node.shadow_tree) visit(visit, *node.shadow_tree);
				for (const auto& child : node.children()) visit(visit, *child);
			};
			clear_render_items(clear_render_items, *doc.m_root);
		}
		doc.m_root_render.reset();
		doc.m_top_layer_render.clear();
		doc.m_tabular_elements.clear();
		doc.m_fixed_boxes.clear();
		doc.m_root_render = doc.m_root->create_render_item(nullptr);
		for (const auto& entry : doc.m_top_layer) {
			const auto dialog = std::dynamic_pointer_cast<html_tag>(entry.lock());
			if (!dialog) continue;
			bool hidden = false;
			for (auto node = element::ptr(dialog); node; node = node->parent()) {
				hidden |= node->css().get_display() == display_none;
			}
			if (hidden) continue;
			// Each synthetic containing block is independent of the document's render ancestry.
			const auto viewport = std::make_shared<html_tag>(doc.m_root, "display:block; width:100%; height:100%; direction:ltr");
			auto holder = viewport->create_render_item(nullptr);
			if (const auto backdrop = dialog->backdrop(); backdrop && backdrop->css().get_display() != display_none) {
				if (auto box = backdrop->create_render_item(holder)) holder->add_child(box);
			}
			if (auto box = dialog->create_render_item(holder)) holder->add_child(box);
			doc.m_top_layer_render.push_back(std::move(holder));
		}
		doc.fix_tables_layout();
		if (doc.m_root_render) {
			trim_white_space(*doc.m_root_render);
			doc.m_root_render = doc.m_root_render->init();
		}
		for (auto& layer : doc.m_top_layer_render) {
			trim_white_space(*layer);
			layer = layer->init();
		}
	}

	void dockit_editor::push_modal(document& doc, const element::ptr& p_element) {
		p_element->set_pseudo_class(_id("modal"), true);
		doc.m_top_layer.emplace_back(p_element);
	}

	void dockit_editor::prune_modals(document& doc) {
		auto& layer = doc.m_top_layer;
		layer.erase(std::remove_if(layer.begin(), layer.end(), [&](const auto& candidate) {
			const auto element = candidate.lock();
			if (!element) return true;
			if (element->get_attr("open")) {
				for (auto node = element; node; node = node->parent()) {
					if (node == doc.m_root) return false;
				}
			}
			element->set_pseudo_class(_id("modal"), false);
			return true;
		}), layer.end());
	}

	bool dockit_editor::set_attribute(element& p_element, const char* p_name, const char* p_value) {
		auto* tag = dynamic_cast<html_tag*>(&p_element);
		if (!tag) {
			return false;
		}
		if (p_value) {
			tag->set_attr(p_name, p_value);
		}
		else {
			remove_attribute(*tag, p_name);
		}
		const std::string_view name(p_name);
		if ((p_element.tag() == _style_ && name == "type") ||
			(p_element.tag() == _link_ && is_one_of(name, "href", "rel", "type")))
			reset_stylesheet_source(p_element);
		return true;
	}

	elements_list dockit_editor::create_text(const document::ptr& p_document, const char* p_text, bool p_split) {
		elements_list children;
		if (*p_text && !p_split) {
			children.push_back(std::make_shared<el_text>(p_text, p_document));
		}
		else if (*p_text) {
			const std::string_view source(p_text);
			p_document->container()->split_text(p_text,
				[&](const char* p_word) { children.push_back(std::make_shared<el_text>(p_word, p_document, source == p_word)); },
				[&](const char* p_space) { children.push_back(std::make_shared<el_space>(p_space, p_document)); });
		}
		mark_text_node(children, p_text);
		return children;
	}

	void dockit_editor::mark_text_node(elements_list& r_parts, const char* p_text) {
		if (r_parts.size() <= 1) return;
		const auto group = std::make_shared<const string>(p_text);
		int64_t offset = 0;
		for (const auto& part : r_parts) {
			auto& text = static_cast<el_text&>(*part);
			text.set_dom_text(group, r_parts.front(), offset);
			for (const unsigned char byte : text.get_source_text()) {
				if ((byte & 0xc0) != 0x80) ++offset;
			}
		}
	}
}
