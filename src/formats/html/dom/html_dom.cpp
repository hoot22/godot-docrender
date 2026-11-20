#include "html_dom.h"
#include "html_shadow_root.h"
#include "html_document_fragment.h"

#include "formats/html/css/css_litehtml_dom.h"

#include "formats/html/elements/html_anchor_element.h"
#include "formats/html/elements/html_button_element.h"
#include "formats/html/elements/html_element_types.h"
#include "formats/html/elements/html_metadata_elements.h"
#include "formats/html/elements/html_stylesheet_node.h"
#include "formats/html/elements/html_details_element.h"
#include "formats/html/elements/html_dialog_element.h"
#include "formats/html/elements/html_dialog_node.h"
#include "formats/html/elements/html_gauge_elements.h"
#include "formats/html/elements/html_audio_element.h"
#include "formats/html/elements/html_video_element.h"
#include "formats/html/elements/html_heading_element.h"
#include "formats/html/elements/html_list_elements.h"
#include "formats/html/elements/html_form_elements.h"
#include "formats/html/elements/html_form_state.h"
#include "formats/html/elements/html_select_node.h"
#include "formats/html/elements/html_table_elements.h"
#include "formats/html/elements/html_image_element.h"
#include "formats/html/elements/html_canvas_element.h"
#include "formats/html/elements/html_input_element.h"
#include "formats/html/elements/html_template_element.h"
#include "formats/html/elements/html_template_node.h"
#include "formats/html/elements/html_textarea_element.h"
#include "formats/html/elements/html_input_node.h"
#include "formats/html/elements/html_node_copy_state.h"
#include "formats/html/elements/html_textarea_node.h"
#include "formats/html/backend/litehtml_document_adapter.h"

#include <litehtml/css_parser.h>
#include <litehtml/css_selector.h>
#include <litehtml/document.h>
#include <litehtml/el_cdata.h>
#include <litehtml/el_comment.h>
#include <litehtml/el_space.h>
#include <litehtml/html_tag.h>
#include <godot_cpp/templates/hash_set.hpp>

#include <algorithm>
#include <exception>
#include <iterator>
#include <string_view>
#include <utility>

using namespace godot;
using dockit::HTMLDOM;
using dockit::HTMLBoundary;
using DocumentAdapter = litehtml::dockit_editor;

namespace {
	bool is_valid_tag_name(std::string_view p_name) {
		const auto is_letter = [](unsigned char p_byte) {
			return (p_byte >= 'A' && p_byte <= 'Z') || (p_byte >= 'a' && p_byte <= 'z');
		};
		if (p_name.empty() || !is_letter(p_name.front())) {
			return false;
		}
		return std::all_of(p_name.begin(), p_name.end(), [&](unsigned char p_byte) {
			return is_letter(p_byte) || (p_byte >= '0' && p_byte <= '9') ||
				p_byte == '-' || p_byte == '_' || p_byte == '.' || p_byte == ':' || p_byte >= 0x80;
		});
	}

	bool is_void_element(const litehtml::element& p_element) {
		const std::string_view tag = p_element.get_tagName();
		static constexpr std::string_view void_tags[] = {
			"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param",
			"source", "track", "wbr", "basefont", "bgsound", "frame", "keygen",
		};
		return std::find(std::begin(void_tags), std::end(void_tags), tag) != std::end(void_tags);
	}

	bool can_contain_elements(const litehtml::element& p_element) {
		const std::string_view tag = p_element.get_tagName();
		return !is_void_element(p_element) && tag != "style" && tag != "script" &&
			tag != "title" && tag != "template" && tag != "textarea" && tag != "xmp" && tag != "iframe" &&
			tag != "noembed" && tag != "noframes" && tag != "plaintext";
	}

	bool has_raw_text(const litehtml::element& p_element) {
		const std::string_view tag = p_element.get_tagName();
		return tag == "style" || tag == "script" || tag == "xmp" || tag == "iframe" ||
			tag == "noembed" || tag == "noframes" || tag == "plaintext";
	}

	bool valid_character_data(const CharString& p_data) {
		return std::string_view(p_data.get_data(), p_data.length()).find('\0') == std::string_view::npos;
	}

	void sync_dom_children(const litehtml::element::ptr& p_node) {
		if (auto* textarea = dynamic_cast<dockit::HTMLTextAreaNode*>(p_node.get())) textarea->sync_default_value();
		dockit::forms::synchronize(p_node);
	}

	void append_escaped(std::string& r_html, std::string_view p_value, bool p_attribute) {
		for (const char byte : p_value) {
			switch (byte) {
			case '&': r_html += "&amp;"; break;
			case '<': r_html += "&lt;"; break;
			case '>': r_html += "&gt;"; break;
			case '"': r_html += p_attribute ? "&quot;" : "\""; break;
			default: r_html += byte; break;
			}
		}
	}

	const litehtml::element* tree_root(const litehtml::element& p_element) {
		const litehtml::element* root = &p_element;
		for (auto parent = root->parent(); parent; parent = parent->parent()) {
			root = parent.get();
		}
		return root;
	}

	const std::shared_ptr<const litehtml::string>& text_group(const litehtml::element& p_node) {
		static const std::shared_ptr<const litehtml::string> empty;
		const auto* text = dynamic_cast<const litehtml::el_text*>(&p_node);
		return text ? text->dom_text() : empty;
	}

	litehtml::elements_list node_parts(const litehtml::element::ptr& p_node) {
		const auto& group = text_group(*p_node);
		if (const auto parent = p_node->parent(); parent && group) {
			litehtml::elements_list parts;
			for (const auto& child : parent->children()) {
				if (text_group(*child) == group) parts.push_back(child);
				else if (!parts.empty()) break;
			}
			if (!parts.empty()) return parts;
		}
		return {p_node};
	}

	litehtml::element::ptr clone_character_data(const litehtml::element& p_source, const litehtml::document::ptr& p_document) {
		if (const auto* space = dynamic_cast<const litehtml::el_space*>(&p_source)) {
			return std::make_shared<litehtml::el_space>(space->get_source_text().c_str(), p_document);
		}
		if (const auto* text = dynamic_cast<const litehtml::el_text*>(&p_source)) {
			return std::make_shared<litehtml::el_text>(text->get_source_text().c_str(), p_document, text->is_whole_text_node());
		}
		litehtml::element::ptr copy;
		if (p_source.is_comment()) {
			copy = std::make_shared<litehtml::el_comment>(p_document);
		}
		else if (dynamic_cast<const litehtml::el_cdata*>(&p_source)) {
			copy = std::make_shared<litehtml::el_cdata>(p_document);
		}
		if (copy) {
			litehtml::string text;
			p_source.get_text(text);
			copy->set_data(text.c_str());
		}
		return copy;
	}

	template <typename Element>
	Ref<HTMLElement> instantiate_element_handle() {
		Ref<Element> handle;
		handle.instantiate();
		return handle;
	}

	Ref<HTMLElement> create_element_handle(const litehtml::element& p_element) {
		const auto p_tag = p_element.tag();
		if (p_element.is_html_namespace()) {
			switch (p_tag) {
				case litehtml::_html_: return instantiate_element_handle<HTMLHtmlElement>();
				case litehtml::_head_: return instantiate_element_handle<HTMLHeadElement>();
				case litehtml::_body_: return instantiate_element_handle<HTMLBodyElement>();
				case litehtml::_title_: return instantiate_element_handle<HTMLTitleElement>();
				case litehtml::_meta_: return instantiate_element_handle<HTMLMetaElement>();
				case litehtml::_base_: return instantiate_element_handle<HTMLBaseElement>();
				case litehtml::_script_: return instantiate_element_handle<HTMLScriptElement>();
				case litehtml::_link_: return instantiate_element_handle<HTMLLinkElement>();
				case litehtml::_style_: return instantiate_element_handle<HTMLStyleElement>();
				default: break;
			}
		}
		if (p_tag == litehtml::_id("menu")) return instantiate_element_handle<HTMLMenuElement>();
		if (p_tag == litehtml::_id("dialog")) return instantiate_element_handle<HTMLDialogElement>();
		switch (p_tag) {
			case litehtml::_a_: return instantiate_element_handle<HTMLAnchorElement>();
			case litehtml::_button_: return instantiate_element_handle<HTMLButtonElement>();
			case litehtml::_form_: return instantiate_element_handle<HTMLFormElement>();
			case litehtml::_label_: return instantiate_element_handle<HTMLLabelElement>();
			case litehtml::_fieldset_: return instantiate_element_handle<HTMLFieldSetElement>();
			case litehtml::_legend_: return instantiate_element_handle<HTMLLegendElement>();
			case litehtml::_select_: return instantiate_element_handle<HTMLSelectElement>();
			case litehtml::_option_: return instantiate_element_handle<HTMLOptionElement>();
			case litehtml::_optgroup_: return instantiate_element_handle<HTMLOptGroupElement>();
			case litehtml::_div_: return instantiate_element_handle<HTMLDivElement>();
			case litehtml::_p_: return instantiate_element_handle<HTMLParagraphElement>();
			case litehtml::_span_: return instantiate_element_handle<HTMLSpanElement>();
			case litehtml::_pre_: return instantiate_element_handle<HTMLPreElement>();
			case litehtml::_datalist_: return instantiate_element_handle<HTMLDataListElement>();
			case litehtml::_output_: return instantiate_element_handle<HTMLOutputElement>();
			case litehtml::_details_: return instantiate_element_handle<HTMLDetailsElement>();
			case litehtml::_meter_: return instantiate_element_handle<HTMLMeterElement>();
			case litehtml::_video_: return instantiate_element_handle<HTMLVideoElement>();
			case litehtml::_audio_: return instantiate_element_handle<HTMLAudioElement>();
			case litehtml::_progress_: return instantiate_element_handle<HTMLProgressElement>();
			case litehtml::_hr_: return instantiate_element_handle<HTMLHRElement>();
			case litehtml::_br_: return instantiate_element_handle<HTMLBRElement>();
			case litehtml::_blockquote_:
			case litehtml::_q_: return instantiate_element_handle<HTMLQuoteElement>();
			case litehtml::_dl_: return instantiate_element_handle<HTMLDListElement>();
			case litehtml::_section_: return instantiate_element_handle<HTMLSectionElement>();
			case litehtml::_article_: return instantiate_element_handle<HTMLArticleElement>();
			case litehtml::_ol_: return instantiate_element_handle<HTMLOListElement>();
			case litehtml::_ul_: return instantiate_element_handle<HTMLUListElement>();
			case litehtml::_li_: return instantiate_element_handle<HTMLLIElement>();
			case litehtml::_h1_:
			case litehtml::_h2_:
			case litehtml::_h3_:
			case litehtml::_h4_:
			case litehtml::_h5_:
			case litehtml::_h6_: return instantiate_element_handle<HTMLHeadingElement>();
			case litehtml::_img_: return instantiate_element_handle<HTMLImageElement>();
			case litehtml::_canvas_: return instantiate_element_handle<HTMLCanvasElement>();
			case litehtml::_input_: return instantiate_element_handle<HTMLInputElement>();
			case litehtml::_template_: return instantiate_element_handle<HTMLTemplateElement>();
			case litehtml::_textarea_: return instantiate_element_handle<HTMLTextAreaElement>();
			case litehtml::_table_: return instantiate_element_handle<HTMLTableElement>();
			case litehtml::_caption_: return instantiate_element_handle<HTMLTableCaptionElement>();
			case litehtml::_col_:
			case litehtml::_colgroup_: return instantiate_element_handle<HTMLTableColElement>();
			case litehtml::_thead_:
			case litehtml::_tbody_:
			case litehtml::_tfoot_: return instantiate_element_handle<HTMLTableSectionElement>();
			case litehtml::_tr_: return instantiate_element_handle<HTMLTableRowElement>();
			case litehtml::_td_:
			case litehtml::_th_: return instantiate_element_handle<HTMLTableCellElement>();
			default: return instantiate_element_handle<HTMLElement>();
		}
	}

} // namespace

HTMLNode::DetachedTree::DetachedTree(std::weak_ptr<HTMLDOM> p_owner, const litehtml::element::ptr& p_root) :
	owner(std::move(p_owner)), root(p_root) {}

HTMLNode::DetachedTree::~DetachedTree() {
	if (const auto dom = owner.lock(); root && dom) {
		dom->release_pointer_captures_in_subtree(*root);
		dom->detached_trees.erase(root.get());
		dom->forget_subtree_listeners(*root);
		const auto remove_entries = [&](const auto& p_visit, const litehtml::element& p_node) -> void {
			dom->node_handles.erase(&p_node);
			dom->forget_source_attributes(p_node);
			for (const auto& child : p_node.children()) p_visit(p_visit, *child);
		};
		remove_entries(remove_entries, *root);
	}
}

HTMLDOM::HTMLDOM(std::function<void()> p_on_change) :
	on_change(std::move(p_on_change)) {}

void HTMLDOM::set_document(litehtml::document::ptr p_document, ObjectID p_owner_id, uint64_t p_generation) {
	document = std::move(p_document);
	owner_id = p_owner_id;
	generation = p_generation;
	if (document) track_mutations(document->root());
	sync_open_dialogs();
	for (const auto& entry : detached_trees) {
		if (const auto tree = entry.value.lock()) {
			if (const auto handle = find_node_handle(*entry.key); handle.is_valid()) bind_handle(*handle.ptr(), tree->root);
			dockit::forms::synchronize(tree->root);
		}
	}
	initialize_shadow_roots();
}

void HTMLDOM::track_dialog(const std::shared_ptr<HTMLDialogNode>& p_dialog) {
	dialog_nodes.emplace_back(p_dialog);
}

void HTMLDOM::sync_open_dialogs() {
	if (!document) return;
	std::vector<std::shared_ptr<HTMLDialogNode>> opened;
	dialog_nodes.erase(std::remove_if(dialog_nodes.begin(), dialog_nodes.end(), [&](const auto& p_candidate) {
		const auto dialog = p_candidate.lock();
		if (!dialog) return true;
		if (!dialog->get_attr("open") || !is_element_connected(*dialog)) dialog->open_sequence = 0;
		else if (!dialog->open_sequence) opened.push_back(dialog);
		return false;
	}), dialog_nodes.end());
	if (opened.size() == 1) {
		opened.front()->open_sequence = ++last_dialog_sequence;
	} else if (!opened.empty()) {
		// A parsed or inserted subtree opens its dialogs in document order.
		visit_elements(document->root(), [&](const auto& p_element) {
			if (auto* dialog = dynamic_cast<HTMLDialogNode*>(p_element.get()); dialog && !dialog->open_sequence && dialog->get_attr("open"))
				dialog->open_sequence = ++last_dialog_sequence;
			return false;
		});
	}
}

std::shared_ptr<dockit::HTMLDialogNode> HTMLDOM::last_open_dialog() const {
	std::shared_ptr<HTMLDialogNode> result;
	for (const auto& candidate : dialog_nodes) {
		const auto dialog = candidate.lock();
		if (dialog && dialog->open_sequence && (!result || dialog->open_sequence > result->open_sequence)) result = dialog;
	}
	return result;
}

void HTMLDOM::normalize_details(const litehtml::element::ptr& p_tree, const std::vector<litehtml::element::ptr>& p_moved,
	bool p_prefer_moved, bool p_notify) {
	ChangeScope changes(*this);
	if (!p_tree) return;
	auto root = p_tree;
	while (const auto parent = root->parent()) root = parent;
	std::vector<litehtml::element::ptr> open;
	visit_elements(root, [&](const auto& p_element) {
		const char* name = p_element->get_attr("name");
		if (p_element->tag() == litehtml::_details_ && name && *name && p_element->get_attr("open")) open.push_back(p_element);
		return false;
	});
	if (!p_moved.empty()) {
		std::unordered_set<const litehtml::element*> moved_roots;
		for (const auto& node : p_moved) moved_roots.insert(node.get());
		const auto moved = [&](const auto& p_node) {
			for (auto current = p_node; current; current = current->parent()) {
				if (moved_roots.count(current.get())) return true;
			}
			return false;
		};
		std::stable_partition(open.begin(), open.end(), [&](const auto& p_node) { return moved(p_node) == p_prefer_moved; });
	}
	std::unordered_set<std::string> names;
	for (const auto& node : open) {
		if (names.insert(node->get_attr("name")).second) continue;
		const String old_value = String::utf8(node->get_attr("open", ""));
		DocumentAdapter::set_attribute(*node, "open", nullptr);
		set_source_attribute(node, "open", nullptr);
		publish_attribute(node, "open", old_value);
		if (p_notify) {
			const Ref<HTMLDetailsElement> details = make_element(node);
			if (details.is_valid()) details->queue_toggle(true);
		}
	}
}

void HTMLDOM::form_state_changed(const litehtml::element::ptr& p_node) {
	ChangeScope changes(*this);
	dockit::forms::synchronize(p_node);
	if (is_element_connected(*p_node)) notify_changed();
}

void HTMLDOM::clear() {
	shadow_roots.clear();
	pending_slot_changes.clear();
	slot_delivery_queued = false;
	design_mode = false;
	pending_changes.clear();
	pending_change_indices.clear();
	changed_styles = false;
	++lifetime;
	++observer_delivery_epoch;
	observer_delivery_queued = false;
	selectionchange_queued = false;
	++selectionchange_epoch;
	scroll_delivery_queued = false;
	++scroll_delivery_epoch;
	pending_scroll_targets.clear();
	scrollend_delivery_queued = false;
	++scrollend_delivery_epoch;
	pending_scrollend_targets.clear();
	for (ObjectID id : observers) {
		if (auto* observer = Object::cast_to<HTMLMutationObserver>(ObjectDB::get_instance(id))) observer->clear_owner(*this);
	}
	observers.clear();
	clear_event_listeners();
	for (ObjectID id : ranges) {
		if (auto* range = Object::cast_to<HTMLRange>(ObjectDB::get_instance(id))) {
			range->start = {};
			range->end = {};
		}
	}
	ranges.clear();
	if (selection.is_valid()) selection->range.unref();
	selection.unref();
	node_handles.clear();
	document_handle = ObjectID();
	// Releasing one root can destroy nested template fragments and unregister them.
	std::vector<std::shared_ptr<HTMLNode::DetachedTree>> trees;
	for (const auto& entry : detached_trees) {
		if (auto tree = entry.value.lock()) {
			trees.push_back(std::move(tree));
		}
	}
	detached_trees.clear();
	for (const auto& tree : trees) {
		tree->root.reset();
	}
	source_attributes.clear();
	dialog_nodes.clear();
	document.reset();
}

bool HTMLDOM::is_element(const litehtml::element& p_element) {
	const auto tag = p_element.tag();
	return tag != litehtml::empty_id && !litehtml::is_generated_tag(tag) && *p_element.get_tagName();
}

std::shared_ptr<HTMLNode::DetachedTree> HTMLDOM::get_detached_tree(const litehtml::element& p_element) const {
	const auto* tree = detached_trees.getptr(tree_root(p_element));
	return tree ? tree->lock() : nullptr;
}

std::shared_ptr<HTMLNode::DetachedTree> HTMLDOM::register_detached_tree(const litehtml::element::ptr& p_root) {
	track_mutations(p_root);
	auto tree = std::make_shared<HTMLNode::DetachedTree>(weak_from_this(), p_root);
	detached_trees.insert(p_root.get(), tree);
	return tree;
}

void HTMLDOM::update_detached_handles(const litehtml::element::ptr& p_root,
	const std::shared_ptr<HTMLNode::DetachedTree>& p_tree) {
	if (const auto handle = find_node_handle(*p_root); handle.is_valid()) handle->detached_tree = p_tree;
	if (p_tree) {
		if (auto* sheet_owner = dynamic_cast<HTMLStylesheetNode*>(p_root.get())) ++sheet_owner->source_version;
	}
	for (const auto& child : p_root->children()) update_detached_handles(child, p_tree);
}

void HTMLDOM::retain_detached_subtree(const litehtml::element::ptr& p_root) {
	update_detached_handles(p_root, register_detached_tree(p_root));
}

bool HTMLDOM::is_element_connected(const litehtml::element& p_element) const {
	const litehtml::element* root = tree_root(p_element);
	std::unordered_set<const litehtml::element*> visited;
	while (root && visited.insert(root).second) {
		if (document && document->root().get() == root) return true;
		const auto host = root->shadow_host.lock();
		if (!host) break;
		root = tree_root(*host);
	}
	return false;
}

Ref<HTMLNode> HTMLDOM::find_node_handle(const litehtml::element& p_element) const {
	if (const ObjectID* id = node_handles.getptr(&p_element)) {
		auto* existing = Object::cast_to<HTMLNode>(ObjectDB::get_instance(*id));
		if (existing && existing->element.lock().get() == &p_element) {
			return Ref<HTMLNode>(existing);
		}
	}
	return {};
}

Ref<HTMLElement> HTMLDOM::make_element(const litehtml::element::ptr& p_element) const {
	if (!p_element || !is_element(*p_element)) {
		return {};
	}
	if (const auto existing = find_node_handle(*p_element); existing.is_valid()) {
		return existing;
	}
	Ref<HTMLElement> handle;
	if (std::string(p_element->get_tagName()) == "slot") { Ref<HTMLSlotElement> slot; slot.instantiate(); handle = slot; }
	else handle = create_element_handle(*p_element);
	bind_handle(*handle.ptr(), p_element);
	return handle;
}

void HTMLDOM::bind_handle(HTMLNode& handle, const litehtml::element::ptr& p_node) const {
	if (!p_node->dom_mutation) track_mutations(p_node);
	handle.layout_id = owner_id;
	handle.parse_version = generation;
	handle.element = p_node;
	handle.detached_tree = get_detached_tree(*p_node);
	node_handles.insert(p_node.get(), ObjectID(handle.get_instance_id()));
}

Ref<HTMLNode> HTMLDOM::make_node(const litehtml::element::ptr& p_node) const {
	if (!p_node || litehtml::is_generated_tag(p_node->tag())) return {};
	if (is_element(*p_node)) return make_element(p_node);
	if (const auto existing = find_node_handle(*p_node); existing.is_valid()) return existing;
	Ref<HTMLNode> handle;
	if (p_node->is_comment()) {
		Ref<HTMLCommentNode> comment;
		comment.instantiate();
		handle = comment;
	} else if (p_node->is_text() || dynamic_cast<const litehtml::el_cdata*>(p_node.get())) {
		if (const auto* native = dynamic_cast<const litehtml::el_text*>(p_node.get())) {
			if (const auto first = native->dom_source(); first && first != p_node) return make_node(first);
		}
		Ref<HTMLTextNode> text;
		text.instantiate();
		handle = text;
	} else if (const auto tree = get_detached_tree(*p_node); tree && tree->root == p_node && tree->public_fragment) {
		Ref<HTMLDocumentFragment> fragment;
		fragment.instantiate();
		handle = fragment;
	}
	if (handle.is_valid()) bind_handle(*handle.ptr(), p_node);
	return handle;
}

Ref<HTMLDocumentNode> HTMLDOM::get_document_node() const {
	if (!document || !document->root()) return {};
	if (auto* handle = Object::cast_to<HTMLDocumentNode>(ObjectDB::get_instance(document_handle))) {
		return Ref<HTMLDocumentNode>(handle);
	}
	Ref<HTMLDocumentNode> handle;
	handle.instantiate();
	handle->element = document->root();
	handle->layout_id = owner_id;
	handle->parse_version = generation;
	document_handle = ObjectID(handle->get_instance_id());
	return handle;
}

TypedArray<HTMLElement> HTMLDOM::get_children(const litehtml::element& p_element) const {
	TypedArray<HTMLElement> children;
	for (const auto& child : p_element.children()) {
		if (const auto handle = make_element(child); handle.is_valid()) {
			children.append(handle);
		}
	}
	return children;
}

TypedArray<HTMLElement> HTMLDOM::get_elements(const String& p_value, Lookup p_lookup, const litehtml::element::ptr& p_scope) const {
	TypedArray<HTMLElement> result;
	if (!document) return result;
	const auto root = p_scope ? p_scope : document->root();
	const String tag = p_lookup == Lookup::TAG ? p_value.to_lower() : String();
	const auto tokens = p_lookup == Lookup::CLASS ? p_value.replace("\t", " ").replace("\r", " ")
		.replace("\n", " ").replace("\f", " ").split(" ", false) : PackedStringArray();
	if (p_lookup == Lookup::CLASS && tokens.is_empty()) return result;
	visit_elements(root, [&](const auto& node) {
		if (node == p_scope) return false;
		bool matches = false;
		if (p_lookup == Lookup::TAG) {
			matches = tag == "*" || tag == String::utf8(node->get_tagName());
		} else if (p_lookup == Lookup::NAME) {
			const char* name = get_element_attribute(*node, "name");
			matches = name && p_value == String::utf8(name);
		} else {
			const auto classes = make_element(node)->get_class_list();
			matches = true;
			for (const String& class_name : tokens) {
				if (!classes.has(class_name)) {
					matches = false;
					break;
				}
			}
		}
		if (matches) result.append(make_element(node));
		return false;
	});
	return result;
}

CharString HTMLDOM::normalize_tag_name(const String& p_tag_name) {
	CharString tag = p_tag_name.utf8();
	if (!is_valid_tag_name(std::string_view(tag.get_data(), tag.length()))) {
		return {};
	}
	for (char* byte = tag.ptrw(); *byte; ++byte) {
		if (*byte >= 'A' && *byte <= 'Z') {
			*byte += 'a' - 'A';
		}
	}
	return tag;
}

Ref<HTMLElement> HTMLDOM::create_element(const CharString& p_tag) {
	const auto node = document->create_element(p_tag.get_data(), {});
	if (node && (dockit::forms::is_listed(*node) || node->tag() == litehtml::_form_)) dockit::forms::synchronize(node);
	return make_detached_element(node);
}

Ref<HTMLDocumentFragment> HTMLDOM::create_document_fragment() {
	return create_document_fragment(document);
}

Ref<HTMLDocumentFragment> HTMLDOM::create_document_fragment(const litehtml::document::ptr& p_document) {
	Ref<HTMLDocumentFragment> fragment;
	fragment.instantiate();
	// An unnamed native tag can hold children without exposing an HTMLElement handle.
	fragment->detached_tree = register_detached_tree(std::make_shared<litehtml::html_tag>(p_document));
	fragment->detached_tree->public_fragment = true;
	bind_handle(*fragment.ptr(), fragment->detached_tree->root);
	return fragment;
}

litehtml::element::ptr HTMLDOM::create_template_node(const litehtml::document::ptr& p_document) {
	// Initial parsing uses a document that HTMLDOM has yet to install.
	const auto fragment = create_document_fragment(p_document);
	const auto node = std::make_shared<HTMLTemplateNode>(p_document, fragment);
	fragment->detached_tree->host = node;
	return node;
}

Ref<HTMLDocumentFragment> HTMLDOM::copy_fragment(const litehtml::element& p_root, bool p_deep) {
	if (!p_root.shadow_host.expired()) return {};
	const auto fragment = create_document_fragment();
	if (p_deep && !clone_children(*this, p_root, *fragment->detached_tree->root)) {
		return {};
	}
	dockit::forms::synchronize(fragment->detached_tree->root);
	return fragment;
}

Ref<HTMLDocumentFragment> HTMLDOM::parse_fragment(const String& p_html, const litehtml::element::ptr& p_context) {
	const auto source = p_html.utf8();
	if (!valid_character_data(source)) return {};
	const auto context = p_context ? p_context : document->create_element("body", {});
	const auto fragment = create_document_fragment();
	DocumentAdapter::replace_children(*fragment->detached_tree->root, DocumentAdapter::parse_fragment(*document, *context, source));
	dockit::forms::synchronize(fragment->detached_tree->root);
	return fragment;
}

Ref<HTMLDocumentFragment> HTMLDOM::import_fragment(const HTMLDocumentFragment& p_source) {
	if (const auto node = p_source.lock_element(); node && node->shadow_host.expired()) {
		const auto copy = create_document_fragment();
		if (clone_children(p_source.get_dom(), *node, *copy->detached_tree->root)) {
			dockit::forms::synchronize(copy->detached_tree->root);
			return copy;
		}
	}
	return {};
}

bool HTMLDOM::adopt_fragment(HTMLDocumentFragment& r_source) {
	ChangeScope changes(*this);
	const auto root = r_source.lock_element();
	if (!root || !r_source.detached_tree->host.expired()) return false;
	auto& source = r_source.get_dom();
	if (&source == this) return true;
	const auto copy = create_document_fragment();
	Adoption adoption;
	if (!clone_children(source, *root, *copy->detached_tree->root, &adoption)) return false;
	source.node_handles.erase(root.get());
	transfer_adoption(source, *root, adoption);
	copy->detached_tree->root->dom_mutation = root->dom_mutation;
	track_mutations(copy->detached_tree->root);
	copy->detached_tree->root->note_dom_change(litehtml::element::DOM_TREE);
	++r_source.adoption_version;
	bind_handle(r_source, copy->detached_tree->root);
	transfer_observers(source);
	dockit::forms::synchronize(r_source.detached_tree->root);
	return true;
}

Ref<HTMLNode> HTMLDOM::create_character_node(const String& p_data, bool p_comment) {
	const auto data = p_data.utf8();
	if (!valid_character_data(data)) return {};
	litehtml::elements_list parts;
	if (p_comment) {
		const auto node = std::make_shared<litehtml::el_comment>(document);
		node->set_data(data.get_data());
		parts.push_back(node);
	} else {
		parts = DocumentAdapter::create_text(document, data.get_data(), true);
		if (parts.empty()) parts.push_back(std::make_shared<litehtml::el_text>("", document));
	}
	const auto node = parts.front();
	const auto holder = std::make_shared<litehtml::html_tag>(document);
	const auto tree = register_detached_tree(holder);
	DocumentAdapter::replace_children(*holder, std::move(parts));
	return make_node(node);
}

String HTMLDOM::character_data(const litehtml::element& p_node) const {
	if (const auto& group = text_group(p_node)) return String::utf8(group->c_str());
	litehtml::string data;
	p_node.get_text(data);
	return String::utf8(data.c_str());
}

HTMLBoundary HTMLDOM::text_boundary(const litehtml::element::ptr& p_part, int64_t p_offset) const {
	for (auto node = p_part; node; node = node->parent()) {
		if (litehtml::is_generated_tag(node->tag())) return {};
	}
	if (const auto* text = dynamic_cast<const litehtml::el_text*>(p_part.get())) {
		return {make_node(p_part), p_offset + text->dom_offset()};
	}
	return {};
}

Error HTMLDOM::set_character_data(const litehtml::element::ptr& p_node, const String& p_data, bool p_notify) {
	return replace_character_data(p_node, p_data, 0, character_data(*p_node).length(), p_notify);
}

bool HTMLDOM::range_can_wrap(const litehtml::element::ptr& p_node) const {
	return p_node && can_contain_elements(*p_node);
}

bool HTMLDOM::can_insert_range_node(const litehtml::element::ptr& p_parent, const Ref<HTMLNode>& p_node) const {
	if (!p_parent || p_node.is_null() || p_node->get_node_type() == HTMLNode::DOCUMENT_NODE ||
		!dynamic_cast<litehtml::html_tag*>(p_parent.get()) || is_void_element(*p_parent) ||
		dynamic_cast<HTMLTemplateNode*>(p_parent.get())) return false;
	const auto native = p_node->lock_element();
	if (!native || is_host_including_ancestor(*native, p_parent) || (!native->parent() && is_element_connected(*native))) return false;
	if (can_contain_elements(*p_parent)) return true;
	if (is_element(*native)) return false;
	return p_node->get_node_type() != HTMLNode::DOCUMENT_FRAGMENT_NODE ||
		std::none_of(native->children().begin(), native->children().end(), [](const auto& node) { return is_element(*node); });
}

Ref<HTMLNode> HTMLDOM::split_range_text(const HTMLBoundary& p_boundary) {
	ChangeScope changes(*this);
	const auto text = p_boundary.node->lock_element();
	const auto parent = p_boundary.node->get_parent_node();
	if (!text || parent.is_null()) return {};
	const int64_t parent_offset = parent->get_child_nodes().find(p_boundary.node) + 1;
	const String data = character_data(*text);
	const auto suffix = create_character_node(data.substr(p_boundary.offset), false);
	struct SplitBoundary {
		Ref<HTMLRange> range;
		bool start;
		int64_t offset;
		bool in_tail;
	};
	std::vector<SplitBoundary> tracked;
	for (ObjectID id : ranges) {
		auto* current = Object::cast_to<HTMLRange>(ObjectDB::get_instance(id));
		if (!current || !current->is_valid()) continue;
		const auto capture = [&](const HTMLBoundary& point, bool p_start) {
			if (point.node == p_boundary.node && point.offset > p_boundary.offset) {
				tracked.push_back({Ref<HTMLRange>(current), p_start, point.offset - p_boundary.offset, true});
			} else if (point.node == parent && point.offset == parent_offset) {
				tracked.push_back({Ref<HTMLRange>(current), p_start, point.offset + 1, false});
			}
		};
		capture(current->start, true);
		capture(current->end, false);
	}
	if (suffix.is_null() || edit_nodes(text, TreeEdit::AFTER, Array::make(suffix), false) != OK ||
		replace_character_data(text, "", p_boundary.offset, data.length(), false) != OK) return {};
	bool selection_moved = false;
	for (const auto& item : tracked) {
		(item.start ? item.range->start : item.range->end) = {item.in_tail ? suffix : parent, item.offset};
		selection_moved |= selection.is_valid() && selection->range == item.range;
	}
	if (selection_moved) selection_changed();
	if (parent->is_connected()) notify_changed();
	return suffix;
}

Error HTMLDOM::replace_character_data(const litehtml::element::ptr& p_node, const String& p_data,
	int64_t p_offset, int64_t p_count, bool p_notify) {
	ChangeScope changes(*this);
	const String previous = character_data(*p_node);
	if (p_offset < 0 || p_offset > previous.length() || p_count < 0) return ERR_INVALID_PARAMETER;
	const int64_t count = std::min(p_count, previous.length() - p_offset);
	const String replacement = previous.substr(0, p_offset) + p_data + previous.substr(p_offset + count);
	const auto data = replacement.utf8();
	if (!valid_character_data(data)) return ERR_INVALID_PARAMETER;
	if (replacement == previous) return OK;
	const auto parent = p_node->parent();
	if (!parent) return ERR_INVALID_PARAMETER;
	litehtml::elements_list parts;
	if (p_node->is_comment()) {
		const auto comment = std::make_shared<litehtml::el_comment>(document);
		comment->set_data(data.get_data());
		parts.push_back(comment);
	} else {
		const bool split = parent->tag() != litehtml::_textarea_ && !has_raw_text(*parent);
		parts = DocumentAdapter::create_text(document, data.get_data(), split);
		if (parts.empty()) parts.push_back(std::make_shared<litehtml::el_text>("", document));
	}
	const auto first = parts.front();
	first->dom_mutation = p_node->dom_mutation;
	track_mutations(first);
	auto children = parent->children();
	const auto original = node_parts(p_node);
	const auto begin = std::find(children.begin(), children.end(), original.front());
	if (begin == children.end()) return ERR_INVALID_PARAMETER;
	first->note_dom_change(litehtml::element::DOM_TEXT);
	children.splice(begin, parts);
	children.erase(begin, std::next(begin, original.size()));
	const auto handle = observers.empty() ? find_node_handle(*p_node) : make_node(p_node);
	DocumentAdapter::replace_children(*parent, std::move(children));
	node_handles.erase(p_node.get());
	if (handle.is_valid()) bind_handle(*handle.ptr(), first);
	bool selection_moved = false;
	for (ObjectID id : ranges) {
		if (auto* range = Object::cast_to<HTMLRange>(ObjectDB::get_instance(id)); range && range->is_valid()) {
			for (auto* point : {&range->start, &range->end}) {
				if (point->node != handle || point->offset <= p_offset) continue;
				const int64_t offset = point->offset <= p_offset + count ? p_offset : point->offset + p_data.length() - count;
				selection_moved |= selection.is_valid() && selection->range.ptr() == range && point->offset != offset;
				point->offset = offset;
			}
		}
	}
	if (!observers.empty() && handle.is_valid()) {
		HTMLMutation record{HTMLMutation::CHARACTER_DATA, handle};
		record.old_value = previous;
		std::vector<Ref<HTMLNode>> ancestry;
		for (auto node = handle; node.is_valid(); node = node->get_parent_node()) ancestry.push_back(node);
		publish_mutation(record, ancestry);
	}
	if (selection_moved) selection_changed();
	sync_dom_children(parent);
	if (p_notify && is_element_connected(*parent)) notify_changed();
	return OK;
}

Error HTMLDOM::edit_nodes(const litehtml::element::ptr& p_target, TreeEdit p_edit, const Array& p_items, bool p_notify) {
	ChangeScope changes(*this);
	const bool sibling = p_edit == TreeEdit::BEFORE || p_edit == TreeEdit::AFTER || p_edit == TreeEdit::REPLACE;
	const auto parent = sibling ? p_target->parent() : p_target;
	if (!parent) return sibling ? OK : ERR_INVALID_PARAMETER;
	if (!dynamic_cast<litehtml::html_tag*>(parent.get()) || is_void_element(*parent) ||
		dynamic_cast<HTMLTemplateNode*>(parent.get())) return ERR_INVALID_PARAMETER;

	// Flatten by logical node. A repeated node takes its final position, as in a fragment.
	std::list<litehtml::elements_list> units;
	HashMap<const litehtml::element*, decltype(units)::iterator> indices;
	const auto collect = [&](litehtml::elements_list p_parts) {
		const auto* key = p_parts.front().get();
		if (const auto* known = indices.getptr(key)) {
			units.splice(units.end(), units, *known);
		} else {
			units.push_back(std::move(p_parts));
			indices.insert(key, std::prev(units.end()));
		}
	};
	for (const Variant& item : p_items) {
		if (item.get_type() == Variant::STRING) {
			const auto source = String(item).utf8();
			if (!valid_character_data(source)) return ERR_INVALID_PARAMETER;
			auto parts = DocumentAdapter::create_text(document, source.get_data(),
				parent->tag() != litehtml::_textarea_ && !has_raw_text(*parent));
			if (parts.empty()) parts.push_back(std::make_shared<litehtml::el_text>("", document));
			collect(std::move(parts));
			continue;
		}
		const Ref<HTMLNode> node = item;
		const auto native = node.is_valid() ? node->lock_element() : nullptr;
		if (!native || &node->get_dom() != this || node->get_node_type() == HTMLNode::DOCUMENT_NODE ||
			is_host_including_ancestor(*native, parent)) return ERR_INVALID_PARAMETER;
		if (node->get_node_type() == HTMLNode::DOCUMENT_FRAGMENT_NODE) {
			if (!native->shadow_host.expired()) return ERR_INVALID_PARAMETER;
			for (auto it = native->children().begin(); it != native->children().end();) {
				auto parts = node_parts(*it);
				std::advance(it, parts.size());
				collect(std::move(parts));
			}
		} else {
			// The document element and its implicit document parent cannot be moved here.
			if (!native->parent() && is_element_connected(*native)) return ERR_INVALID_PARAMETER;
			collect(node_parts(native));
		}
	}
	if (!can_contain_elements(*parent)) {
		for (const auto& parts : units) {
			if (is_element(*parts.front())) return ERR_INVALID_PARAMETER;
		}
	}

	HashSet<const litehtml::element*> moved;
	struct MovedNode {
		litehtml::element::ptr root;
		std::shared_ptr<HTMLNode::DetachedTree> tree;
	};
	std::vector<MovedNode> moved_nodes;
	std::vector<std::pair<litehtml::element::ptr, litehtml::elements_list>> parents;
	HashMap<const litehtml::element*, size_t> parent_indices;
	const auto add_parent = [&](const litehtml::element::ptr& p_parent) {
		if (p_parent && !parent_indices.has(p_parent.get())) {
			parent_indices.insert(p_parent.get(), parents.size());
			parents.emplace_back(p_parent, litehtml::elements_list());
		}
	};
	add_parent(parent);
	for (const auto& parts : units) {
		const auto& root = parts.front();
		add_parent(root->parent());
		moved_nodes.push_back({root, get_detached_tree(*root)});
		for (const auto& part : parts) moved.insert(part.get());
	}

	litehtml::element::ptr anchor;
	if (sibling) {
		const auto parts = node_parts(p_target);
		const auto after = p_edit == TreeEdit::BEFORE ? parts.front() : parts.back();
		bool ready = false;
		for (const auto& child : parent->children()) {
			if (p_edit == TreeEdit::BEFORE && child == after) ready = true;
			if (ready && !moved.has(child.get())) {
				anchor = child;
				break;
			}
			if (child == after) ready = true;
		}
	} else {
		for (const auto& child : parent->children()) {
			if (!moved.has(child.get()) && (child->tag() == litehtml::_after_ ||
				(p_edit == TreeEdit::PREPEND && !litehtml::is_generated_tag(child->tag())))) {
				anchor = child;
				break;
			}
		}
	}

	HashSet<const litehtml::element*> replaced;
	if (p_edit == TreeEdit::REPLACE) for (const auto& part : node_parts(p_target)) replaced.insert(part.get());
	litehtml::elements_list removed;
	for (auto& [owner, children] : parents) {
		for (const auto& child : owner->children()) {
			if (moved.has(child.get())) continue;
			if (owner == parent && (replaced.has(child.get()) || (p_edit == TreeEdit::REPLACE_CHILDREN &&
				!litehtml::is_generated_tag(child->tag())))) {
				removed.push_back(child);
			} else {
				children.push_back(child);
			}
		}
	}
	auto& children = parents.front().second;
	const auto position = anchor ? std::find(children.begin(), children.end(), anchor) : children.end();
	for (auto& parts : units) children.splice(position, parts);
	const auto changed = [](const auto& p_entry) { return p_entry.first->children() != p_entry.second; };
	parents.erase(std::remove_if(parents.begin(), parents.end(), [&](const auto& p_entry) { return !changed(p_entry); }), parents.end());
	if (parents.empty()) return OK;
	const bool connected = std::any_of(parents.begin(), parents.end(), [&](const auto& p_entry) {
		return is_element_connected(*p_entry.first);
	});
	std::vector<ChildChange> tracked;
	for (const auto& [owner, children] : parents) {
		auto change = capture_children(owner);
		if (change.target.is_valid()) tracked.push_back(std::move(change));
	}
	std::stable_sort(tracked.begin(), tracked.end(), [&](const ChildChange& p_first, const ChildChange& p_second) {
		const bool first_target = p_first.target->lock_element() == parent;
		const bool second_target = p_second.target->lock_element() == parent;
		if (first_target != second_target) return !first_target;
		return p_first.ancestry.size() > p_second.ancestry.size();
	});
	const auto target_tree = get_detached_tree(*parent);
	DocumentAdapter::replace_child_lists(parents);
	std::vector<litehtml::element::ptr> preferred;
	for (const auto& entry : moved_nodes) {
		release_detached_root(entry.tree, entry.root);
		update_detached_handles(entry.root, target_tree);
		preferred.push_back(entry.root);
	}
	while (!removed.empty()) {
		const auto first = removed.front();
		const auto& group = text_group(*first);
		if (group || first->is_text() || first->is_comment() || dynamic_cast<const litehtml::el_cdata*>(first.get())) {
			auto end = removed.begin();
			++end;
			if (group) while (end != removed.end() && text_group(**end) == group) ++end;
			litehtml::elements_list parts;
			parts.splice(parts.end(), removed, removed.begin(), end);
			const auto holder = std::make_shared<litehtml::html_tag>(document);
			DocumentAdapter::replace_children(*holder, std::move(parts));
			retain_detached_subtree(holder);
		} else {
			removed.pop_front();
			retain_detached_subtree(first);
		}
	}
	for (const auto& change : tracked) finish_children(change, preferred);
	for (const auto& [owner, previous] : parents) sync_dom_children(owner);
	normalize_details(parent, preferred);
	if (p_notify && connected) notify_changed();
	return OK;
}

Error HTMLDOM::normalize(const litehtml::element::ptr& p_root) {
	ChangeScope changes(*this);
	bool changed = false;
	bool selection_moved = false;
	const auto merge = [&](const auto& p_visit, const litehtml::element::ptr& p_parent) -> void {
		std::vector<Ref<HTMLTextNode>> run;
		const auto flush = [&]() {
			Ref<HTMLTextNode> first;
			String suffix;
			for (const auto& node : run) {
				const String part = node->get_data();
				if (first.is_null() && !part.is_empty()) {
					first = node;
				} else if (first.is_valid()) {
					suffix += part;
				} else {
					edit_nodes(node->lock_element(), TreeEdit::REPLACE, {}, false);
					changed = true;
				}
			}
			if (first.is_valid()) {
				int64_t offset = first->get_data().length();
				replace_character_data(first->lock_element(), suffix, offset, 0, false);
				const auto parent = first->get_parent_node();
				bool past_first = false;
				for (const auto& node : run) {
					if (past_first) {
						const int64_t index = parent->get_child_nodes().find(node);
						for (ObjectID id : ranges) {
							auto* range = Object::cast_to<HTMLRange>(ObjectDB::get_instance(id));
							if (!range || !range->is_valid()) continue;
							for (auto* point : {&range->start, &range->end}) {
								if (point->node == node || (point->node == parent && point->offset == index)) {
									*point = {first, offset + (point->node == node ? point->offset : 0)};
									selection_moved |= selection.is_valid() && selection->range.ptr() == range;
								}
							}
						}
						offset += node->get_data().length();
						edit_nodes(node->lock_element(), TreeEdit::REPLACE, {}, false);
						changed = true;
					}
					past_first |= node == first;
				}
			}
			run.clear();
		};
		const auto wrapper = make_node(p_parent);
		if (wrapper.is_null()) return;
		for (const Ref<HTMLNode>& child : wrapper->get_child_nodes()) {
			const Ref<HTMLTextNode> text = child;
			if (text.is_valid()) {
				run.push_back(text);
			} else {
				flush();
				if (child->get_node_type() == HTMLNode::ELEMENT_NODE) p_visit(p_visit, child->lock_element());
			}
		}
		flush();
	};
	merge(merge, p_root);
	if (selection_moved) selection_changed();
	if (changed && is_element_connected(*p_root)) notify_changed();
	return OK;
}

Ref<HTMLElement> HTMLDOM::make_detached_element(const litehtml::element::ptr& p_element) {
	Ref<HTMLElement> handle = make_element(p_element);
	if (handle.is_valid()) {
		handle->detached_tree = register_detached_tree(p_element);
	}
	return handle;
}

Ref<HTMLElement> HTMLDOM::copy_node(const HTMLDOM& p_source_dom, const litehtml::element& p_source, bool p_deep) {
	const auto copy = clone_subtree(p_source_dom, p_source, p_deep);
	dockit::forms::synchronize(copy);
	return make_detached_element(copy);
}

bool HTMLDOM::adopt_node(HTMLDOM& p_source_dom, const litehtml::element::ptr& p_source) {
	ChangeScope changes(*this);
	const auto source_tree = p_source_dom.get_detached_tree(*p_source);
	const auto parent = p_source->parent();
	if (!parent && !source_tree) {
		return false;
	}
	const bool source_connected = p_source_dom.is_element_connected(*p_source);
	const auto children_change = p_source_dom.capture_children(parent);
	Adoption adoption;
	const auto copy = clone_subtree(p_source_dom, *p_source, true, &adoption);
	if (!copy) {
		return false;
	}
	p_source_dom.release_pointer_captures_in_subtree(*p_source);
	// Stage the detached target before editing the source. Its owner cleans up on failure.
	const auto target_tree = register_detached_tree(copy);
	if (parent && !parent->removeChild(p_source)) {
		return false;
	}
	p_source_dom.release_detached_root(source_tree, p_source);
	transfer_adoption(p_source_dom, *p_source, adoption);
	p_source_dom.finish_children(children_change);
	dockit::forms::synchronize(copy, copy);
	dockit::forms::synchronize(parent);
	if (source_connected) {
		p_source_dom.notify_changed();
	}
	return true;
}

void HTMLDOM::transfer_adoption(HTMLDOM& p_source, const litehtml::element& p_root, const Adoption& p_adoption) {
	for (const auto& [source, target] : p_adoption.nodes) {
		target->dom_mutation = source->dom_mutation;
		track_node_mutations(target);
		target->note_dom_change(litehtml::element::DOM_TREE);
	}
	for (const auto& entry : p_adoption.handles) {
		p_source.node_handles.erase(entry.source);
		++entry.handle->adoption_version;
		bind_handle(*entry.handle.ptr(), entry.target);
	}
	for (const auto& entry : p_adoption.fragments) p_source.node_handles.erase(entry.source->element.lock().get());
	for (const auto& entry : p_adoption.handles) transfer_listeners(p_source, *entry.source, entry.target);
	p_source.forget_subtree(p_root);
	for (const auto& entry : p_adoption.fragments) {
		const auto root = entry.target->content->detached_tree->root;
		if (const auto source = entry.source->element.lock()) root->dom_mutation = source->dom_mutation;
		track_node_mutations(root);
		root->note_dom_change(litehtml::element::DOM_TREE);
		bind_handle(*entry.source.ptr(), entry.target->content->detached_tree->root);
		++entry.source->adoption_version;
		entry.target->content = entry.source;
	}
	transfer_observers(p_source);
}

litehtml::element::ptr HTMLDOM::clone_subtree(const HTMLDOM& p_source_dom, const litehtml::element& p_source, bool p_deep,
	Adoption* r_adoption) {
	if (!is_element(p_source)) {
		const auto copy = clone_character_data(p_source, document);
		if (copy && r_adoption) {
			r_adoption->nodes.emplace_back(p_source.shared_from_this(), copy);
			if (const auto handle = p_source_dom.find_node_handle(p_source); handle.is_valid()) r_adoption->handles.push_back({&p_source, copy, handle});
		}
		return copy;
	}
	const auto* source_attributes = p_source_dom.get_source_attributes(p_source);
	const litehtml::string_map attributes = source_attributes ? *source_attributes : litehtml::string_map();
	const auto copy = document->create_element(p_source.get_tagName(), attributes);
	if (!copy) {
		return {};
	}
	copy->set_namespace(p_source.get_namespace());
	if (p_deep) {
		const auto* source_template = dynamic_cast<const HTMLTemplateNode*>(&p_source);
		const auto& source = source_template ? *source_template->content->detached_tree->root : p_source;
		if (!clone_children(p_source_dom, source, *copy, r_adoption)) {
			forget_subtree(*copy);
			return {};
		}
	}
	const auto mode = r_adoption ? HTMLNodeCopyMode::ADOPT : HTMLNodeCopyMode::COPY;
	if (const auto* state = dynamic_cast<const HTMLNodeCopyState*>(&p_source); state && !state->copy_state_to(*copy, mode)) {
		forget_subtree(*copy);
		return {};
	}
	if (r_adoption) {
		r_adoption->nodes.emplace_back(p_source.shared_from_this(), copy);
		copy->css_w().copy_remembered_content_size(p_source.css());
		if (const auto handle = p_source_dom.find_node_handle(p_source); handle.is_valid()) {
			r_adoption->handles.push_back({ &p_source, copy, handle });
		}
		if (const auto* source_template = dynamic_cast<const HTMLTemplateNode*>(&p_source)) {
			r_adoption->fragments.push_back({ source_template->content, static_cast<HTMLTemplateNode*>(copy.get()) });
		}
	}
	return copy;
}

bool HTMLDOM::clone_children(const HTMLDOM& p_source_dom, const litehtml::element& p_source, litehtml::element& r_target,
	Adoption* r_adoption) {
	std::shared_ptr<const litehtml::string> previous_group;
	std::shared_ptr<const litehtml::string> copied_group;
	litehtml::element::ptr first_part;
	for (const auto& child : p_source.children()) {
		if (litehtml::is_generated_tag(child->tag())) {
			continue;
		}
		const auto copy = clone_subtree(p_source_dom, *child, true, r_adoption);
		if (!copy || !r_target.appendChild(copy)) {
			if (copy) {
				forget_subtree(*copy);
			}
			return false;
		}
		const auto& group = text_group(*child);
		if (group != previous_group) {
			copied_group = group ? std::make_shared<const litehtml::string>(*group) : nullptr;
			previous_group = group;
		}
		if (group) {
			const auto& source_text = static_cast<const litehtml::el_text&>(*child);
			auto& text = static_cast<litehtml::el_text&>(*copy);
			if (source_text.dom_offset() == 0) first_part = copy;
			text.set_dom_text(copied_group, first_part, source_text.dom_offset());
		}
	}
	return true;
}

bool HTMLDOM::is_host_including_ancestor(const litehtml::element& p_ancestor, litehtml::element::ptr p_node) const {
	while (p_node) {
		if (p_node.get() == &p_ancestor) {
			return true;
		}
		if (auto parent = p_node->parent()) {
			p_node = std::move(parent);
		} else {
			const auto tree = get_detached_tree(*p_node);
			p_node = tree ? tree->host.lock() : nullptr;
		}
	}
	return false;
}

void HTMLDOM::release_detached_root(const std::shared_ptr<HTMLNode::DetachedTree>& p_tree, const litehtml::element::ptr& p_node) {
	if (p_tree && p_tree->root == p_node) {
		detached_trees.erase(p_node.get());
		p_tree->root.reset();
	}
}

void HTMLDOM::forget_subtree(const litehtml::element& p_element) {
	release_pointer_captures_in_subtree(p_element);
	forget_subtree_listeners(p_element);
	if (const ObjectID* id = node_handles.getptr(&p_element)) {
		if (auto* handle = Object::cast_to<HTMLNode>(ObjectDB::get_instance(*id))) {
			handle->element.reset();
			handle->detached_tree.reset();
		}
		node_handles.erase(&p_element);
	}
	forget_source_attributes(p_element);
	for (const auto& child : p_element.children()) {
		forget_subtree(*child);
	}
	if (const auto* node = dynamic_cast<const HTMLTemplateNode*>(&p_element)) {
		const auto& tree = node->content->detached_tree;
		if (const auto root = tree->root) {
			forget_subtree(*root);
			release_detached_root(tree, root);
		}
	}
}

TypedArray<HTMLElement> HTMLDOM::select_elements(const String& p_selector, SelectorQuery p_query,
	const litehtml::element::ptr& p_scope) const {
	TypedArray<HTMLElement> matches;
	const CharString utf8 = p_selector.utf8();
	const auto tokens = litehtml::tokenize(std::string(utf8.get_data(), utf8.length()));
	ERR_FAIL_COND_V_MSG(!dockit::css::has_balanced_selector_tokens(tokens), matches, "Invalid HTML selector: " + p_selector);
	namespace syntax = dockit::css;
	const auto selectors = syntax::parse_selector_list(syntax::normalize(tokens, syntax::f_componentize),
		syntax::strict_mode | syntax::forbid_pseudo_elements,
		litehtml::selector_mode(document ? document->mode() : litehtml::no_quirks_mode));
	ERR_FAIL_COND_V_MSG(selectors.empty(), matches, "Invalid or unsupported HTML selector: " + p_selector);
	if (!document) {
		return matches;
	}
	const auto root = p_scope ? p_scope : document->root();
	const syntax::MatchContext context{root.get(), syntax::MatchMode::complete, p_scope && !is_element(*p_scope)};
	const auto& dom_view = litehtml::selector_dom_view();
	const auto collect = [&](const litehtml::element::ptr& p_element) {
		if (syntax::match_selector_list(dom_view, p_element.get(), selectors, context) == syntax::match_none) {
			return false;
		}
		matches.append(make_element(p_element));
		return p_query != SelectorQuery::ALL;
	};

	if (p_query == SelectorQuery::SELF || p_query == SelectorQuery::CLOSEST) {
		for (auto element = root; element; element = element->parent()) {
			if ((HTMLDOM::is_element(*element) && collect(element)) || p_query == SelectorQuery::SELF) {
				break;
			}
		}
	}
	else if (p_scope) {
		for (const auto& child : p_scope->children()) {
			if (HTMLDOM::visit_elements(child, collect)) {
				break;
			}
		}
	}
	else {
		HTMLDOM::visit_elements(root, collect);
	}
	return matches;
}

Ref<HTMLElement> HTMLDOM::query_selector(const String& p_selector, const litehtml::element::ptr& p_scope) const {
	const auto matches = select_elements(p_selector, SelectorQuery::FIRST, p_scope);
	return matches.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(matches[0]);
}

TypedArray<HTMLElement> HTMLDOM::query_selector_all(const String& p_selector, const litehtml::element::ptr& p_scope) const {
	return select_elements(p_selector, SelectorQuery::ALL, p_scope);
}

bool HTMLDOM::matches(const String& p_selector, const litehtml::element::ptr& p_scope) const {
	return !select_elements(p_selector, SelectorQuery::SELF, p_scope).is_empty();
}

Ref<HTMLElement> HTMLDOM::closest(const String& p_selector, const litehtml::element::ptr& p_scope) const {
	const auto matches = select_elements(p_selector, SelectorQuery::CLOSEST, p_scope);
	return matches.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(matches[0]);
}

Ref<HTMLElement> HTMLDOM::get_element_by_id(const String& p_id, const litehtml::element::ptr& p_root) const {
	if (!document || p_id.is_empty()) {
		return {};
	}
	const CharString id = p_id.utf8();
	litehtml::element::ptr match;
	HTMLDOM::visit_elements(p_root ? p_root : document->root(), [&](const litehtml::element::ptr& p_element) {
		const char* candidate = get_element_attribute(*p_element, "id");
		if (candidate && std::string_view(candidate) == std::string_view(id.get_data(), id.length())) {
			match = p_element;
		}
		return match != nullptr;
	});
	return make_element(match);
}

void HTMLDOM::remember_source_attributes(const litehtml::element::ptr& p_element, const litehtml::string_map& p_values) {
	source_attributes.insert(p_element.get(), SourceAttributes{ p_element, p_values });
}

const litehtml::string_map* HTMLDOM::get_source_attributes(const litehtml::element& p_element) const {
	const auto* source = source_attributes.getptr(&p_element);
	return source && !source->element.expired() ? &source->values : nullptr;
}

void HTMLDOM::set_source_attribute(const litehtml::element::ptr& p_element, const char* p_name, const char* p_value) {
	auto* source = source_attributes.getptr(p_element.get());
	if (p_value) {
		if (!source || source->element.expired()) {
			source_attributes.insert(p_element.get(), SourceAttributes{ p_element, {} });
			source = source_attributes.getptr(p_element.get());
		}
		source->values[p_name] = p_value;
	}
	else if (source) {
		source->values.erase(p_name);
	}
}

void HTMLDOM::forget_source_attributes(const litehtml::element& p_element) {
	source_attributes.erase(&p_element);
}

Dictionary HTMLDOM::get_element_attributes(const litehtml::element& p_element) const {
	Dictionary result;
	if (const auto* attributes = get_source_attributes(p_element)) {
		for (const auto& [name, value] : *attributes) {
			result[String::utf8(name.c_str())] = String::utf8(value.c_str());
		}
	}
	return result;
}

const char* HTMLDOM::get_element_attribute(const litehtml::element& p_element, const char* p_name) const {
	if (const auto* attributes = get_source_attributes(p_element)) {
		const auto found = attributes->find(p_name);
		if (found != attributes->end()) {
			return found->second.c_str();
		}
	}
	return p_element.get_attr(p_name);
}

Error HTMLDOM::edit_attribute(const litehtml::element::ptr& p_element, const char* p_name, const char* p_value) {
	ChangeScope changes(*this);
	const auto* source = get_source_attributes(*p_element);
	const char* previous = nullptr;
	if (source) {
		const auto found = source->find(p_name);
		if (found != source->end()) {
			previous = found->second.c_str();
		}
	}
	if ((!previous && !p_value) || (previous && p_value && std::string_view(previous) == p_value)) {
		return OK;
	}
	const Variant old_value = previous && !observers.empty() ? Variant(String::utf8(previous)) : Variant();
	if (!DocumentAdapter::set_attribute(*p_element, p_name, p_value)) {
		return ERR_UNAVAILABLE;
	}
	const bool open_presence_changed = std::string_view(p_name) == "open" && bool(previous) != bool(p_value);
	set_source_attribute(p_element, p_name, p_value);
	publish_attribute(p_element, p_name, old_value);
	if (p_element->tag() == litehtml::_details_) {
		if (open_presence_changed) {
			const Ref<HTMLDetailsElement> details = make_element(p_element);
			if (details.is_valid()) details->queue_toggle(bool(previous));
			if (p_value) normalize_details(p_element, {p_element}, true);
		} else if (std::string_view(p_name) == "name" && p_element->get_attr("open")) {
			normalize_details(p_element, {p_element});
		}
	}
	if (auto* input = dynamic_cast<HTMLInputNode*>(p_element.get())) {
		input->attribute_changed(p_name);
	} else if (auto* textarea = dynamic_cast<HTMLTextAreaNode*>(p_element.get())) {
		textarea->update_pseudos();
	}
	const std::string_view name(p_name);
	if (name == "multiple" && !p_value) {
		if (auto* select = dynamic_cast<dockit::HTMLSelectNode*>(p_element.get())) select->use_single_selection();
	}
	if (name == "selected") {
		if (auto* option = dynamic_cast<dockit::HTMLOptionNode*>(p_element.get())) option->attribute_changed();
	}
	if (name == "checked" || name == "type" || name == "name" || name == "form" || name == "id" || name == "disabled" || name == "selected" || name == "multiple" || name == "size") {
		dockit::forms::synchronize(p_element, p_element);
	} else if (name == "value" || name == "required" || name == "readonly" || name == "minlength" || name == "maxlength" ||
		name == "min" || name == "max" || name == "step" || name == "command" || name == "commandfor") {
		if (const auto* option = dynamic_cast<const dockit::HTMLOptionNode*>(p_element.get())) {
			if (auto* select = option->select()) dockit::forms::update_validity(*select);
		} else if (dockit::forms::is_listed(*p_element) && p_element->tag() != litehtml::_fieldset_) {
			dockit::forms::update_validity(*p_element);
		}
	}
	if (is_element_connected(*p_element)) {
		notify_changed();
	}
	return OK;
}

String HTMLDOM::serialize_children(const litehtml::element& p_element) const {
	return serialize(p_element, false);
}

String HTMLDOM::serialize_element(const litehtml::element& p_element) const {
	return serialize(p_element, true);
}

String HTMLDOM::serialize(const litehtml::element& p_element, bool p_outer) const {
	std::string html;
	const auto append_node = [&](const auto& p_visit, const litehtml::element& p_node, bool p_include_self,
		bool p_raw_text = false) -> void {
		if (litehtml::is_generated_tag(p_node.tag())) return;
		if (p_include_self && !is_element(p_node)) {
			std::string text;
			p_node.get_text(text);
			if (p_node.is_comment()) html += "<!--" + text + "-->";
			else if (p_raw_text) html += text;
			else append_escaped(html, text, false);
			return;
		}
		if (p_include_self) {
			html += '<';
			html += p_node.get_tagName();
			if (const auto* attributes = get_source_attributes(p_node)) {
				for (const auto& [name, value] : *attributes) {
					html += ' ';
					html += name;
					html += "=\"";
					append_escaped(html, value, true);
					html += '"';
				}
			}
			html += '>';
			if (is_void_element(p_node)) return;
		}
		const auto* node = dynamic_cast<const HTMLTemplateNode*>(&p_node);
		const auto& contents = node ? *node->content->detached_tree->root : p_node;
		for (const auto& child : contents.children()) p_visit(p_visit, *child, true, has_raw_text(p_node));
		if (p_include_self) {
			html += "</";
			html += p_node.get_tagName();
			html += '>';
		}
	};
	append_node(append_node, p_element, p_outer);
	return String::utf8(html.data(), html.size());
}

Error HTMLDOM::replace_children(const litehtml::element::ptr& p_element, const String& p_content, bool p_parse_html) {
	ChangeScope changes(*this);
	const auto* node = dynamic_cast<const HTMLTemplateNode*>(p_element.get());
	if (is_void_element(*p_element) || (node && !p_parse_html)) {
		return p_content.is_empty() ? OK : ERR_INVALID_PARAMETER;
	}
	const CharString content = p_content.utf8();
	if (std::string_view(content.get_data(), content.length()).find('\0') != std::string_view::npos) {
		return ERR_INVALID_PARAMETER;
	}
	litehtml::elements_list children;
	try {
		const bool split = p_element->tag() != litehtml::_textarea_ && !has_raw_text(*p_element);
		children = p_parse_html ? DocumentAdapter::parse_fragment(*document, *p_element, content) :
			DocumentAdapter::create_text(document, content.get_data(), split);
	}
	catch (const std::exception&) {
		return ERR_PARSE_ERROR;
	}
	const auto& target = node ? node->content->detached_tree->root : p_element;
	const auto children_change = capture_children(target);
	for (const auto& child : target->children()) {
		forget_subtree(*child);
	}
	DocumentAdapter::replace_children(*target, std::move(children));
	finish_children(children_change);
	if (auto* textarea = dynamic_cast<HTMLTextAreaNode*>(p_element.get())) {
		textarea->sync_default_value();
	}
	dockit::forms::synchronize(target, target);
	normalize_details(target, {target});
	if (!node && is_element_connected(*p_element)) {
		notify_changed();
	}
	return OK;
}

Error HTMLDOM::insert_child(const litehtml::element::ptr& p_parent, const litehtml::element::ptr& p_child,
	const litehtml::element::ptr& p_reference) {
	ChangeScope changes(*this);
	return place_child(p_parent, p_child, p_reference, false);
}

Error HTMLDOM::insert_fragment(const litehtml::element::ptr& p_parent, const HTMLDocumentFragment& p_fragment,
	const litehtml::element::ptr& p_reference) {
	ChangeScope changes(*this);
	const auto& tree = p_fragment.detached_tree;
	if (!tree || !tree->root || !tree->root->shadow_host.expired() || tree->owner.lock().get() != this || !can_contain_elements(*p_parent) ||
		(p_reference && p_reference->parent() != p_parent)) {
		return ERR_INVALID_PARAMETER;
	}
	const auto& root = tree->root;
	if (is_host_including_ancestor(*root, p_parent)) {
		return ERR_INVALID_PARAMETER;
	}

	const auto& children = root->children();
	if (children.empty()) {
		return OK;
	}
	const auto target_tree = get_detached_tree(*p_parent);
	const auto source_change = capture_children(root);
	const auto target_change = capture_children(p_parent);
	auto child = children.begin();
	const size_t count = children.size();
	if (!DocumentAdapter::insert_children(*root, *p_parent, p_reference)) {
		return ERR_INVALID_PARAMETER;
	}
	// Splice preserves the iterators to the moved children, now in the target list.
	std::vector<litehtml::element::ptr> moved;
	moved.reserve(count);
	for (size_t i = 0; i < count; ++i, ++child) {
		update_detached_handles(*child, target_tree);
		moved.push_back(*child);
	}
	finish_children(source_change);
	finish_children(target_change);
	normalize_details(p_parent, moved);
	dockit::forms::synchronize(p_parent, p_parent);
	if (!target_tree) {
		notify_changed();
	}
	return OK;
}

Error HTMLDOM::replace_child(const litehtml::element::ptr& p_parent, const litehtml::element::ptr& p_new_child,
	const litehtml::element::ptr& p_old_child) {
	ChangeScope changes(*this);
	return place_child(p_parent, p_new_child, p_old_child, true);
}

Error HTMLDOM::place_child(const litehtml::element::ptr& p_parent, const litehtml::element::ptr& p_child,
	const litehtml::element::ptr& p_reference, bool p_replace_reference, bool p_notify, bool* r_changed) {
	ChangeScope changes(*this);
	if (!can_contain_elements(*p_parent) || (p_replace_reference && !p_reference) ||
		(p_reference && p_reference->parent() != p_parent)) {
		return ERR_INVALID_PARAMETER;
	}
	if (is_host_including_ancestor(*p_child, p_parent)) {
		return ERR_INVALID_PARAMETER;
	}

	const auto source_tree = get_detached_tree(*p_child);
	const auto old_parent = p_child->parent();
	if (!source_tree && !p_child->parent()) {
		return ERR_INVALID_PARAMETER;
	}
	const auto target_tree = get_detached_tree(*p_parent);
	const auto source_change = old_parent != p_parent ? capture_children(old_parent) : ChildChange();
	const auto target_change = capture_children(p_parent);
	using EditResult = DocumentAdapter::child_edit_result;
	const auto result = DocumentAdapter::place_child(p_child, *p_parent, p_reference, p_replace_reference);
	if (result != EditResult::changed) {
		return result == EditResult::unchanged ? OK : ERR_INVALID_PARAMETER;
	}
	if (r_changed) *r_changed = true;
	if (source_tree != target_tree) {
		release_detached_root(source_tree, p_child);
		update_detached_handles(p_child, target_tree);
	}
	if (p_replace_reference) {
		retain_detached_subtree(p_reference);
		dockit::forms::synchronize(p_reference);
	}
	finish_children(source_change);
	finish_children(target_change, {p_child});
	dockit::forms::synchronize(p_parent, p_child);
	normalize_details(p_parent, {p_child});
	if (old_parent && tree_root(*old_parent) != tree_root(*p_parent)) dockit::forms::synchronize(old_parent);
	if (p_notify && (!source_tree || !target_tree)) {
		notify_changed();
	}
	return OK;
}

Error HTMLDOM::set_table_child(const litehtml::element::ptr& p_table, const litehtml::element::ptr& p_new_child,
	const litehtml::element::ptr& p_old_child, const litehtml::element::ptr& p_reference) {
	ChangeScope changes(*this);
	if ((p_old_child && p_old_child->parent() != p_table) || (p_reference && p_reference->parent() != p_table)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_new_child && !p_old_child) return OK;
	const bool connected = is_element_connected(*p_table) || (p_new_child && is_element_connected(*p_new_child));
	bool changed = false;
	// Insertion does all hierarchy validation before the old child is removed.
	if (p_new_child) {
		const Error error = place_child(p_table, p_new_child, p_reference, false, false, &changed);
		if (error != OK) return error;
	}
	if (p_old_child && p_new_child != p_old_child) {
		const Error error = detach_child(p_old_child, false);
		if (error != OK) return error;
		changed = true;
	}
	if (connected && changed) notify_changed();
	return OK;
}

Error HTMLDOM::remove_child(const litehtml::element::ptr& p_parent, const litehtml::element::ptr& p_child) {
	ChangeScope changes(*this);
	return p_child->parent() == p_parent ? detach_element(p_child) : ERR_INVALID_PARAMETER;
}

Error HTMLDOM::detach_element(const litehtml::element::ptr& p_element) {
	return detach_child(p_element, true);
}

Error HTMLDOM::detach_child(const litehtml::element::ptr& p_element, bool p_notify) {
	ChangeScope changes(*this);
	const auto parent = p_element->parent();
	const auto old_tree = get_detached_tree(*p_element);
	if (!parent) {
		return old_tree ? OK : ERR_INVALID_PARAMETER;
	}
	const auto children_change = capture_children(parent);
	if (!parent->removeChild(p_element)) {
		return ERR_INVALID_PARAMETER;
	}
	release_pointer_captures_in_subtree(*p_element);
	retain_detached_subtree(p_element);
	finish_children(children_change);
	dockit::forms::synchronize(parent);
	dockit::forms::synchronize(p_element);
	if (!old_tree && p_notify) {
		notify_changed();
	}
	return OK;
}

Error HTMLDOM::remove_element(const litehtml::element::ptr& p_element) {
	ChangeScope changes(*this);
	const auto parent = p_element->parent();
	const auto old_tree = get_detached_tree(*p_element);
	if (!parent) {
		return old_tree ? OK : ERR_INVALID_PARAMETER;
	}
	const auto children_change = capture_children(parent);
	if (!parent->removeChild(p_element)) {
		return ERR_INVALID_PARAMETER;
	}
	release_pointer_captures_in_subtree(*p_element);
	forget_subtree(*p_element);
	finish_children(children_change);
	dockit::forms::synchronize(parent);
	if (!old_tree) {
		notify_changed();
	}
	return OK;
}
