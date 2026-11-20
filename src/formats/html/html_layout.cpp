#include "html_layout.h"
#include "elements/html_audio_element.h"
#include "elements/html_dialog_element.h"
#include "elements/html_dialog_node.h"
#include "elements/html_video_node.h"
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

#include "elements/html_form_state.h"
#include "elements/html_input_node.h"
#include "elements/html_select_node.h"
#include "elements/html_stylesheet_node.h"
#include "elements/html_textarea_node.h"

#include "formats/html/paint/html_canvas_renderer.h"
#include "formats/html/elements/html_details_node.h"
#include "formats/html/dom/html_dom.h"
#include "formats/html/dom/html_document_fragment.h"
#include "formats/html/resources/html_image_sources.h"
#include "formats/html/resources/html_resource_paths.h"
#include "layout/html_geometry.h"
#include "formats/html/backend/litehtml_container.h"
#include "formats/html/backend/litehtml_document_adapter.h"
#include "formats/html/css/css_transition_state.h"
#include "formats/html/css/css_animation_state.h"
#include "formats/html/css/css_declarations.h"
#include "formats/html/css/css_media_query.h"
#include "formats/html/css/css_stylesheet_rules.h"
#include "formats/html/css/css_supports.h"
#include "formats/html/css/css_syntax_util.h"
#include "formats/html/css/html_cssom.h"
#include "formats/html/css/html_style_sheet.h"
#include "formats/html/markup/html_markup.h"

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_pan_gesture.hpp>
#include <godot_cpp/classes/interval_tweener.hpp>
#include <godot_cpp/classes/method_tweener.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <litehtml/css_parser.h>
#include <litehtml/html_tag.h>
#include <litehtml/el_image.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <unordered_set>

using namespace godot;
using dockit::HTMLInputNode;
using dockit::HTMLTextAreaNode;
using dockit::HTMLTextControlNode;
using dockit::HTMLCanvasRenderer;
using dockit::HTMLAnchor;
using dockit::HTMLDOM;
using dockit::HTMLGeometryIndex;
using dockit::HTMLHighlight;
using dockit::LiteHTMLContainer;
using DocumentAdapter = litehtml::dockit_editor;

namespace syntax = dockit::css;

namespace {
	litehtml::element::ptr find_html_ancestor(litehtml::element::ptr p_element) {
		while (p_element && !HTMLDOM::is_element(*p_element)) {
			p_element = p_element->parent();
		}
		return p_element;
	}

	const char* get_link_href(const litehtml::element& p_element) {
		const char* href = (p_element.tag() == litehtml::_a_ || p_element.tag() == litehtml::_area_) ?
			p_element.get_attr("href") : nullptr;
		return href && *href ? href : nullptr;
	}

	litehtml::element::ptr find_link_element(litehtml::element::ptr p_element) {
		for (auto element = std::move(p_element); element; element = element->parent()) {
			if (element->tag() == litehtml::_a_ || element->tag() == litehtml::_area_) {
				return get_link_href(*element) ? element : nullptr;
			}
		}
		return {};
	}

	std::vector<real_t> image_map_coordinates(const litehtml::element& p_area, const String& p_shape,
		const Vector2& p_intrinsic_size) {
		const String source = String::utf8(p_area.get_attr("coords", ""));
		if (source.is_empty()) return {};
		const PackedStringArray parts = source.split(",", false);
		std::vector<real_t> coordinates;
		coordinates.reserve(parts.size());
		for (int32_t i = 0; i < parts.size(); ++i) {
			String token = parts[i].strip_edges();
			const bool percent = token.ends_with("%");
			if (percent) token = token.substr(0, token.length() - 1).strip_edges();
			if (!token.is_valid_float()) return {};
			const real_t value = token.to_float();
			if (!std::isfinite(value)) return {};
			real_t basis = i % 2 == 0 ? p_intrinsic_size.x : p_intrinsic_size.y;
			if (p_shape == "circle" && i == 2) basis = p_intrinsic_size.length() / std::sqrt(2.0f);
			coordinates.push_back(percent ? value * basis / 100.0f : value);
		}
		return coordinates;
	}

	bool image_map_polygon_contains(const Vector2& p_point, const std::vector<real_t>& p_coordinates) {
		const std::size_t count = p_coordinates.size() / 2;
		if (count < 3 || p_coordinates.size() % 2 != 0) return false;
		bool inside = false;
		for (std::size_t i = 0, j = count - 1; i < count; j = i++) {
			const Vector2 a(p_coordinates[j * 2], p_coordinates[j * 2 + 1]);
			const Vector2 b(p_coordinates[i * 2], p_coordinates[i * 2 + 1]);
			const Vector2 edge = b - a;
			const Vector2 offset = p_point - a;
			const real_t cross = edge.x * offset.y - edge.y * offset.x;
			const real_t dot = edge.dot(offset);
			if (std::abs(cross) <= 1e-5f && dot >= -1e-5f && dot <= edge.length_squared() + 1e-5f) return true;
			if ((a.y > p_point.y) != (b.y > p_point.y) &&
				p_point.x < (b.x - a.x) * (p_point.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
		}
		return inside;
	}

	bool image_map_area_contains(const litehtml::element& p_area, const Vector2& p_point, const Vector2& p_intrinsic_size) {
		const String shape = String::utf8(p_area.get_attr("shape", "rect")).strip_edges().to_lower();
		if (shape == "default") return true;
		const bool circle = shape == "circle" || shape == "circ";
		const bool polygon = shape == "poly" || shape == "polygon";
		const String normalized_shape = circle ? "circle" : polygon ? "poly" : "rect";
		const std::vector<real_t> coordinates = image_map_coordinates(p_area, normalized_shape, p_intrinsic_size);
		if (circle) {
			if (coordinates.size() < 3 || coordinates[2] < 0) return false;
			const Vector2 delta = p_point - Vector2(coordinates[0], coordinates[1]);
			return delta.length_squared() <= coordinates[2] * coordinates[2];
		}
		if (polygon) return image_map_polygon_contains(p_point, coordinates);
		if (coordinates.size() < 4) return false;
		const real_t left = std::min(coordinates[0], coordinates[2]);
		const real_t right = std::max(coordinates[0], coordinates[2]);
		const real_t top = std::min(coordinates[1], coordinates[3]);
		const real_t bottom = std::max(coordinates[1], coordinates[3]);
		return p_point.x >= left && p_point.x <= right && p_point.y >= top && p_point.y <= bottom;
	}

	using RenderFragments = std::vector<const dockit::RenderGeometry*>;

	const dockit::RenderGeometry* scroll_target_renderer(const RenderFragments* p_renderers, bool p_allow_fallback = false) {
		if (p_renderers) {
			for (const auto* renderer : *p_renderers) {
				if (renderer->inverse && renderer->item->is_visible() &&
					(!renderer->fragments.empty() || (p_allow_fallback && renderer->anchor_fallback))) return renderer;
			}
		}
		return nullptr;
	}

	std::optional<Rect2> scroll_target_bounds(const RenderFragments& p_renderers,
		const dockit::HTMLPlaneTransform& p_from_document, bool p_allow_fallback) {
		std::optional<Rect2> bounds;
		const auto include = [&](const Rect2& p_rect) {
			if (p_rect.is_finite()) bounds = bounds ? bounds->merge(p_rect) : p_rect;
		};
		for (const auto* renderer : p_renderers) {
			if (!renderer->item->is_visible() || !renderer->inverse) continue;
			const auto matrix = p_from_document * renderer->matrix;
			for (const auto& fragment : renderer->fragments) include(matrix.xform(fragment.rect));
			if (p_allow_fallback && renderer->anchor_fallback) include(matrix.xform(*renderer->anchor_fallback));
		}
		return bounds;
	}

	void collect_anchors(const litehtml::element& p_element, const HTMLGeometryIndex& p_geometry,
		HashMap<String, HTMLAnchor>& r_anchors, const RenderFragments* p_placeholder = nullptr) {
		if (p_element.css().get_display() == litehtml::display_none) {
			return;
		}
		const auto* renderers = p_placeholder ? p_placeholder : p_geometry.find_element(&p_element);
		const char* id = p_element.get_attr("id");
		if (id && *id && p_element.css().get_visibility() == litehtml::visibility_visible) {
			const String name = String::utf8(id);
			if (renderers && !r_anchors.has(name)) {
				if (const auto bounds = scroll_target_bounds(*renderers, {}, true)) {
					r_anchors.insert(name, {*bounds, p_element.weak_from_this()});
				}
			}
		}
		if (p_element.css().hides_contents()) return;
		if (p_element.css().skips_contents()) p_placeholder = renderers;
		for (const auto& child : p_element.children()) {
			collect_anchors(*child, p_geometry, r_anchors, p_placeholder);
		}
	}

	const litehtml::element* auto_content_placeholder(const litehtml::element& p_element) {
		if (p_element.css().get_visibility() != litehtml::visibility_visible ||
			p_element.css().get_display() == litehtml::display_none) return nullptr;
		const litehtml::element* placeholder = nullptr;
		for (auto parent = p_element.parent(); parent; parent = parent->parent()) {
			const auto& css = parent->css();
			if (css.hides_contents() || css.get_display() == litehtml::display_none) return nullptr;
			if (css.skips_contents()) placeholder = parent.get();
		}
		return placeholder;
	}

} // namespace

namespace {
	litehtml::color_scheme system_preference() {
		static auto scheme = litehtml::color_scheme::light;
		static uint64_t next_poll = 0;
		const auto now = Time::get_singleton()->get_ticks_msec();
		if (now >= next_poll) {
			const auto* display = DisplayServer::get_singleton();
			scheme = display && display->is_dark_mode_supported() && display->is_dark_mode()
				? litehtml::color_scheme::dark : litehtml::color_scheme::light;
			next_poll = now + 250;
		}
		return scheme;
	}
}

struct HTMLLayout::State {
	enum class Update { NONE, LAYOUT, STYLE, HOVER_LAYOUT };
	struct AnimationEventTrack {
		std::weak_ptr<litehtml::element> target;
		String name;
		String pseudo;
		double time = 0.0;
		double duration = 0.0;
		double iteration_duration = 0.0;
		double delay = 0.0;
		dockit::css::PlaybackState playback_state = dockit::css::PlaybackState::idle;
		uint64_t iteration = 0;
		bool started = false;
		bool scripted = false;
	};
	struct ScrollMotion {
		std::weak_ptr<litehtml::element> element;
		Vector2 from;
		Vector2 to;
		Ref<Tween> tween;
		Callable changed;
	};
	struct SnapMemory {
		std::weak_ptr<litehtml::element> container;
		std::array<std::weak_ptr<const litehtml::element>, 2> areas;
		Ref<Tween> pan_timer;
		Vector2i pan_axes;
		uint64_t pan_id = 0;
		Callable changed;
	};

	// Render items are cleared before the document releases its fonts through the container.
	std::shared_ptr<HTMLDOM> dom;
	std::unique_ptr<LiteHTMLContainer> container;
	Ref<HTMLCanvasRenderer> canvas_renderer;
	HTMLGeometryIndex geometry;
	HashMap<String, HTMLAnchor> anchors;
	std::vector<std::weak_ptr<litehtml::element>> hovered_elements;
	std::map<uint64_t, ScrollMotion> scroll_motions;
	uint64_t next_scroll_motion = 0;
	std::map<const litehtml::element*, SnapMemory> nested_snap_memory;
	std::map<std::weak_ptr<litehtml::element>, Vector2, std::owner_less<>> skipped_scroll_offsets;
	std::weak_ptr<litehtml::element> content_focus;
	SnapMemory viewport_snap_memory;
	Vector2i viewport_size;
	Vector2i media_viewport_size;
	Vector2i viewport_unit_size;
	Vector2i viewport_scroll_offset;
	Vector2 viewport_gutters;
	std::array<litehtml::overflow, 2> viewport_overflow{litehtml::overflow_auto, litehtml::overflow_auto};
	Vector2i content_size;
	String imported_video_base_dir;
	Dictionary audio_streams;
	Dictionary video_sources;
	std::string master_styles;
	std::string user_styles;
	struct StylesheetIdentity {
		int64_t id = 0;
		Ref<HTMLStyleSheet> handle;
	};
	std::map<std::weak_ptr<litehtml::element>, StylesheetIdentity, std::owner_less<>> stylesheet_identities;
	int64_t extra_stylesheet_id = 0;
	Callable edit_callback;
	Callable element_scroll_callback;
	dockit::css::TransitionState transitions;
	dockit::css::AnimationState animations;
	std::map<std::pair<bool, uint64_t>, ObjectID> animation_handles;
	std::map<std::pair<bool, uint64_t>, AnimationEventTrack> animation_event_tracks;
	dockit::css::TransitionState::Snapshot animation_underlying;
	Callable transition_callback;
	Callable transition_frame;
	ObjectID transition_tree;
	double animation_time = 0.0;
	bool sampling_transitions = false;
	Callable content_update_callback;
	Callable resource_layout_callback;
	Callable content_selection_callback;
	String error;
	Error parse_error = OK;
	Update pending_update = Update::LAYOUT;
	bool implicit_empty_document = false;
	bool reflow_queued = false;
	bool input_paint_pending = false;
	bool hover_viewport_style_changed = false;
	// Kept through scrollbar convergence so viewport-unit changes rebuild the hover-updated render tree.
	bool hover_layout_active = false;
	unsigned selector_state_dependencies = 0;
	bool css_diagnostics_enabled = false;
	bool has_css_transitions = false;
	bool print_media = false;
	bool embedded_block = false;

	bool has_layout() const {
		return pending_update == Update::NONE && viewport_size.x > 0;
	}
};

struct HTMLLayout::CSSOMSheet {
	int64_t id;
	std::shared_ptr<litehtml::element> owner;
	std::string text;
	std::string base;
	std::string media;
	bool disabled = false;
};

bool HTMLLayout::refresh_cssom() {
	const auto& document = state->dom->get_document();
	if (!document) return false;
	if (state->pending_update == State::Update::NONE) return true;
	if (state->viewport_size.x > 0) {
		return layout_viewport(state->viewport_size, state->media_viewport_size, state->viewport_unit_size) == OK;
	}
	if (state->pending_update == State::Update::STYLE) {
		DocumentAdapter::restyle(document, *state->container, state->master_styles, state->user_styles);
		state->selector_state_dependencies = DocumentAdapter::selector_state_dependencies(*document);
		state->has_css_transitions = DocumentAdapter::has_transitions(*document);
		state->pending_update = State::Update::LAYOUT;
	}
	return true;
}

namespace {
	using namespace litehtml;

	const html_tag* cssom_tag(const element& element, const String& pseudo) {
		if (pseudo.is_empty()) return dynamic_cast<const html_tag*>(&element);
		const auto name = pseudo.trim_prefix("::").trim_prefix(":");
		if (name == "backdrop") {
			const auto* tag = dynamic_cast<const html_tag*>(&element);
			return tag ? tag->backdrop().get() : nullptr;
		}
		const auto tag = name == "before" ? __tag_before_ : name == "after" ? __tag_after_ :
			name == "marker" ? marker_tag() : empty_id;
		if (tag == empty_id) return nullptr;
		for (const auto& child : element.children()) {
			if (child->tag() == tag) return dynamic_cast<const html_tag*>(child.get());
		}
		return nullptr;
	}

	std::string transform_matrix_value(const HTMLGeometryIndex& geometry, const html_tag& target) {
		const auto& transforms = target.css().get_transforms();
		if (transforms.empty()) return "none";
		const auto matrix = geometry.css_transform_matrix(target, transforms);
		if (!matrix) return {};
		const bool flat = (*matrix)[0].z == 0.0 && (*matrix)[0].w == 0.0 &&
			(*matrix)[1].z == 0.0 && (*matrix)[1].w == 0.0 &&
			(*matrix)[2] == Vector4(0, 0, 1, 0) && (*matrix)[3].z == 0.0 && (*matrix)[3].w == 1.0;
		using Kind = css_transform_function::kind;
		css_transform_function value{flat ? Kind::matrix : Kind::matrix3d};
		if (flat) {
			value.matrix = {(*matrix)[0].x, (*matrix)[0].y, (*matrix)[1].x, (*matrix)[1].y, (*matrix)[3].x, (*matrix)[3].y};
		} else {
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) value.matrix[i * 4 + j] = (*matrix)[i][j];
			}
		}
		return dockit::css::serialize_value(_transform_, property_value(css_transform_list{value}, false));
	}

	String read_computed(const HTMLGeometryIndex& geometry, const html_tag& element, const std::string& name, const String& pseudo) {
		if (name.empty()) return {};
		const auto canonical = css_canonical_property(_id(name));
		const auto physical = css_map_logical_property(canonical, element.css().is_right_to_left());
		const auto id = physical == empty_id ? canonical : physical;
		if (pseudo == "::selection" || pseudo == ":selection") {
			const auto colors = element.get_selection_colors(element.css().get_color());
			const auto& color = id == _color_ ? colors.foreground : colors.background;
			if (!color || (id != _color_ && id != _background_color_)) return {};
			return String::utf8(dockit::css::serialize_value(id, property_value(color->to_srgb(), false)).c_str());
		}
		const bool placeholder = pseudo == "::placeholder" || pseudo == ":placeholder";
		const auto* target = placeholder ? &element : cssom_tag(element, pseudo);
		if (!target) return {};
		const auto* style = placeholder ? target->get_placeholder_style() : &target->css();
		if (!style) return {};
		std::string result;
		if (name.compare(0, 2, "--") == 0) {
			css_token_vector tokens;
			if (target->get_custom_property(id, tokens)) result = get_repr(tokens, 0, -1, true);
		} else {
			if (id == _transform_ && !placeholder) result = transform_matrix_value(geometry, *target);
			if (result.empty()) result = dockit::css::serialize_computed(id, *style);
			if (result.empty() && !placeholder) {
				const auto& value = target->get_cascaded_property(id);
				if (!value.is<inherit>() && !value.is<initial_value>()) result = dockit::css::serialize_value(id, value);
			}
		}
		return String::utf8(result.c_str());
	}
}

String HTMLLayout::get_computed_style_property(const Ref<HTMLElement>& p_element, const String& p_name, const String& p_pseudo) {
	if (!is_element_connected(p_element) || !refresh_cssom()) return {};
	const auto node = std::dynamic_pointer_cast<litehtml::html_tag>(p_element->lock_element());
	return node ? read_computed(state->geometry, *node, syntax::property_name(p_name), p_pseudo) : String();
}

Dictionary HTMLLayout::get_computed_style(const Ref<HTMLElement>& p_element, const String& p_pseudo) {
	Dictionary result;
	if (!is_element_connected(p_element) || !refresh_cssom()) return result;
	const auto node = std::dynamic_pointer_cast<litehtml::html_tag>(p_element->lock_element());
	if (!node) return result;
	const auto read = [&](const std::string& name) {
		const String value = read_computed(state->geometry, *node, name, p_pseudo);
		if (!value.is_empty()) result[String::utf8(name.c_str())] = value;
	};
	for (const auto& property : syntax::animated_properties()) read(litehtml::_s(property.name));
	for (const char* name : {"display", "position", "visibility", "overflow-x", "overflow-y", "box-sizing", "direction",
			"font-family", "font-weight", "font-style", "text-align", "line-height", "cursor", "z-index",
			"grid-template-columns", "grid-template-rows", "grid-auto-flow", "grid-auto-rows", "grid-auto-columns",
			"align-items", "align-self", "align-content", "justify-items", "justify-self", "justify-content"}) read(name);
	std::vector<litehtml::string_id> names;
	if (const auto* tag = cssom_tag(*node, p_pseudo)) tag->collect_property_names(names);
	for (auto parent = node->parent(); parent; parent = parent->parent()) {
		if (const auto* tag = dynamic_cast<const litehtml::html_tag*>(parent.get())) tag->collect_property_names(names);
	}
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	for (const auto name : names) {
		const auto& spelling = litehtml::_s(name);
		if (spelling.compare(0, 2, "__") != 0) read(spelling);
	}
	return result;
}

std::vector<HTMLLayout::CSSOMSheet> HTMLLayout::collect_stylesheets() {
	if (!refresh_cssom()) return {};
	static std::atomic<int64_t> next_id{1};
	for (auto it = state->stylesheet_identities.begin(); it != state->stylesheet_identities.end();) {
		if (it->first.expired()) it = state->stylesheet_identities.erase(it);
		else ++it;
	}
	std::vector<CSSOMSheet> result;
	for (const auto& sheet : DocumentAdapter::stylesheets(*state->dom->get_document())) {
		auto owner = sheet.owner.lock();
		if (!owner) continue;
		auto [entry, added] = state->stylesheet_identities.try_emplace(owner);
		if (added) entry->second.id = next_id++;
		result.push_back({entry->second.id, std::move(owner), sheet.text, sheet.baseurl, sheet.media, sheet.disabled});
	}
	if (state->extra_stylesheet_id == 0) state->extra_stylesheet_id = next_id++;
	result.push_back({state->extra_stylesheet_id, {}, state->user_styles, {}, {}});
	return result;
}

int64_t HTMLLayout::get_stylesheet_id(const HTMLElement& p_element) {
	const auto node = p_element.lock_element();
	if (node && state->dom->is_element_connected(*node)) {
		for (const auto& sheet : collect_stylesheets()) {
			if (sheet.owner == node) return sheet.id;
		}
	}
	return 0;
}

Ref<HTMLStyleSheet> HTMLLayout::get_stylesheet(const HTMLElement& p_element) {
	const auto node = std::dynamic_pointer_cast<dockit::HTMLStylesheetNode>(p_element.lock_element());
	const int64_t id = node ? get_stylesheet_id(p_element) : 0;
	if (id == 0) return {};
	auto& handle = state->stylesheet_identities.find(node)->second.handle;
	if (handle.is_valid() && handle->source_version == node->source_version) return handle;

	Ref<HTMLStyleSheet> sheet;
	sheet.instantiate();
	sheet->layout_id = ObjectID(get_instance_id());
	sheet->owner = node;
	sheet->parse_version = parse_version;
	sheet->source_version = node->source_version;
	sheet->id = id;
	handle = sheet;
	return sheet;
}

TypedArray<Dictionary> HTMLLayout::get_stylesheets() {
	TypedArray<Dictionary> result;
	for (const auto& sheet : collect_stylesheets()) {
		Dictionary snapshot;
		snapshot["id"] = sheet.id;
		snapshot["owner"] = sheet.owner ? wrap_hit_element(sheet.owner) : Ref<HTMLElement>();
		snapshot["source"] = !sheet.owner ? "stylesheet" : sheet.owner->tag() == litehtml::_link_ ? "link" : "style";
		snapshot["path"] = String::utf8(sheet.base.c_str());
		snapshot["media"] = String::utf8(sheet.media.c_str());
		snapshot["disabled"] = sheet.disabled;
		result.append(snapshot);
	}
	return result;
}

PackedStringArray HTMLLayout::get_css_rules(int64_t p_stylesheet) {
	PackedStringArray result;
	for (const auto& sheet : collect_stylesheets()) {
		if (sheet.id != p_stylesheet) continue;
		for (const auto& rule : litehtml::css_parser::parse_stylesheet(sheet.text, true)) {
			result.append(String::utf8(dockit::css::serialize_rule(*rule).c_str()));
		}
		break;
	}
	return result;
}

Error HTMLLayout::edit_css_rule(int64_t p_stylesheet, int64_t p_index, const String* p_rule) {
	auto sheets = collect_stylesheets();
	const auto sheet = std::find_if(sheets.begin(), sheets.end(), [&](const auto& item) { return item.id == p_stylesheet; });
	if (sheet == sheets.end()) return ERR_INVALID_PARAMETER;
	auto rules = litehtml::css_parser::parse_stylesheet(sheet->text, true);
	if (p_index < 0 || p_index > static_cast<int64_t>(rules.size()) ||
		(!p_rule && p_index == static_cast<int64_t>(rules.size()))) return ERR_INVALID_PARAMETER;
	if (p_rule) {
		const auto input = syntax::parse_cssom_rule(*p_rule);
		if (!input) return ERR_PARSE_ERROR;
		rules.insert(rules.begin() + p_index, input);
		bool accepted = false;
		const auto mode = state->dom->get_document()->mode();
		syntax::visit_stylesheet_rules(rules, true, [&](const syntax::StylesheetRule& rule) {
			bool valid = true;
			if (rule.kind == syntax::StylesheetRuleKind::style) {
				valid = !syntax::parse_selector_list(rule.syntax.prelude, syntax::strict_mode, litehtml::selector_mode(mode)).empty();
			} else if (rule.kind == syntax::StylesheetRuleKind::supports) {
				valid = litehtml::evaluate_css_supports(rule.syntax.prelude, mode).has_value();
			} else if (rule.kind == syntax::StylesheetRuleKind::keyframes) {
				valid = syntax::parse_keyframe_rule(rule.syntax.prelude, rule.syntax.block.value).has_value();
			}
			if (&rule.syntax == input.get()) accepted = valid;
			return valid;
		});
		if (!accepted) return ERR_PARSE_ERROR;
		// An insertion must not move existing imports past the allowed prefix.
		bool past_imports = false;
		for (const auto& rule : rules) {
			if (rule->type == syntax::raw_rule::at && syntax::detail::lowcase(rule->name) == "import") {
				if (past_imports) return ERR_INVALID_DATA;
			} else if (!(rule->type == syntax::raw_rule::at && rule->block.type != syntax::CURLY_BLOCK &&
				syntax::detail::lowcase(rule->name) == "layer")) {
				past_imports = true;
			}
		}
	} else {
		rules.erase(rules.begin() + p_index);
	}
	std::string updated;
	for (const auto& rule : rules) updated += dockit::css::serialize_rule(*rule) + '\n';
	if (sheet->owner) DocumentAdapter::edit_stylesheet(*state->dom->get_document(), sheet->owner, std::move(updated));
	else state->user_styles = std::move(updated);
	invalidate_styles();
	return OK;
}

Error HTMLLayout::insert_rule(int64_t p_stylesheet, const String& p_rule, int64_t p_index) {
	return edit_css_rule(p_stylesheet, p_index, &p_rule);
}

Error HTMLLayout::delete_rule(int64_t p_stylesheet, int64_t p_index) {
	return edit_css_rule(p_stylesheet, p_index, nullptr);
}

bool HTMLLayout::supports_css(const String& p_property_or_condition, const String& p_value) const {
	const String input = p_value.is_empty() ? p_property_or_condition : p_property_or_condition + String(": ") + p_value;
	const auto& doc = state->dom->get_document();
	return litehtml::evaluate_css_supports(litehtml::normalize(input.utf8().get_data(), syntax::f_componentize),
		doc ? doc->mode() : litehtml::no_quirks_mode, true).value_or(false);
}

bool HTMLLayout::supports_selector(const String& p_selector) const {
	const auto& doc = state->dom->get_document();
	return litehtml::supports_css_selector(litehtml::normalize(p_selector.utf8().get_data(), syntax::f_componentize),
		doc ? doc->mode() : litehtml::no_quirks_mode);
}

bool HTMLLayout::matches_media(const String& p_query) const {
	const auto& doc = state->dom->get_document();
	if (!doc || p_query.strip_edges().is_empty()) return false;
	litehtml::media_features features;
	doc->container()->get_media_features(features);
	return litehtml::parse_media_query_list(p_query.utf8().get_data(), doc).check(features);
}

namespace {
	std::optional<double> animation_number(const Variant& value) {
		if (value.get_type() != Variant::FLOAT && value.get_type() != Variant::INT) return std::nullopt;
		const double number = value;
		return std::isfinite(number) ? std::optional(number) : std::nullopt;
	}

	std::optional<litehtml::css_easing> animation_easing(const Variant& value) {
		if (value.get_type() != Variant::STRING && value.get_type() != Variant::STRING_NAME) return std::nullopt;
		const auto tokens = litehtml::normalize(String(value).utf8().get_data(), syntax::f_componentize | syntax::f_remove_whitespace);
		return tokens.size() == 1 ? litehtml::parse_css_easing(tokens.front()) : std::nullopt;
	}

	std::optional<litehtml::css_animation> animation_options(const Dictionary& options) {
		litehtml::css_animation result;
		const auto duration = animation_number(options.get("duration", 0.0));
		const auto delay = animation_number(options.get("delay", 0.0));
		const Variant iteration_input = options.get("iterations", 1.0);
		auto iterations = animation_number(iteration_input);
		if (iteration_input.get_type() == Variant::FLOAT && double(iteration_input) == std::numeric_limits<double>::infinity()) {
			iterations = double(iteration_input);
		}
		const auto easing = animation_easing(options.get("easing", "linear"));
		if (!duration || *duration < 0 || !delay || !iterations || *iterations < 0 || !easing) return std::nullopt;
		const std::array<String, 4> directions{"normal", "reverse", "alternate", "alternate-reverse"};
		const std::array<String, 4> fills{"none", "forwards", "backwards", "both"};
		const auto direction = std::find(directions.begin(), directions.end(), String(options.get("direction", "normal")));
		const auto fill = std::find(fills.begin(), fills.end(), String(options.get("fill", "none")));
		if (direction == directions.end() || fill == fills.end()) return std::nullopt;
		result.duration = *duration;
		result.delay = *delay;
		result.iterations = *iterations;
		result.easing = *easing;
		result.direction = static_cast<int>(direction - directions.begin());
		result.fill = static_cast<int>(fill - fills.begin());
		return result;
	}

	std::shared_ptr<const litehtml::css_keyframes> script_keyframes(const TypedArray<Dictionary>& keyframes) {
		const auto size = keyframes.size();
		if (size == 0) return {};
		std::vector<std::optional<double>> offsets(size);
		double previous = 0.0;
		for (int64_t index = 0; index < size; ++index) {
			const Dictionary frame = keyframes[index];
			if (!frame.has("offset")) continue;
			const auto offset = animation_number(frame["offset"]);
			if (!offset || *offset < previous || *offset > 1.0) return {};
			previous = *offset;
			offsets[index] = *offset;
		}
		if (!offsets.back()) offsets.back() = 1.0;
		if (!offsets.front()) offsets.front() = 0.0;
		for (size_t first = 0; first + 1 < offsets.size();) {
			size_t last = first + 1;
			while (!offsets[last]) ++last;
			for (size_t index = first + 1; index < last; ++index) {
				offsets[index] = *offsets[first] + (*offsets[last] - *offsets[first]) * double(index - first) / double(last - first);
			}
			first = last;
		}
		std::map<double, litehtml::style> frames;
		for (int64_t index = 0; index < size; ++index) {
			auto& parsed = frames[*offsets[index]];
			const Dictionary frame = keyframes[index];
			for (const Variant& key : frame.keys()) {
				if (key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) return {};
				const String name(key);
				if (name == "offset") continue;
				const Variant value = frame[key];
				if (name == "easing" && !animation_easing(value)) return {};
				if (value.get_type() != Variant::STRING && value.get_type() != Variant::STRING_NAME && !animation_number(value)) return {};
				const auto property = litehtml::_id(name == "easing" ? "animation-timing-function" : syntax::property_name(name));
				const auto tokens = litehtml::normalize(value.stringify().utf8().get_data(), syntax::f_componentize | syntax::f_remove_whitespace);
				const auto status = litehtml::parse_css_declaration(property, tokens, {}, false, nullptr, {}).status;
				if (status != litehtml::css_declaration_status::parsed && status != litehtml::css_declaration_status::deferred) return {};
				parsed.add_property(property, tokens);
			}
		}
		auto result = std::make_shared<litehtml::css_keyframes>();
		for (auto& [offset, frame] : frames) result->frames.emplace(offset, std::make_shared<litehtml::style>(std::move(frame)));
		return result;
	}
}

Ref<HTMLAnimation> HTMLLayout::animation_handle(uint64_t p_track, bool p_transition) {
	auto& handle_id = state->animation_handles[{p_transition, p_track}];
	if (auto* cached = Object::cast_to<HTMLAnimation>(ObjectDB::get_instance(handle_id))) return Ref<HTMLAnimation>(cached);
	Ref<HTMLAnimation> result;
	result.instantiate();
	result->owner = ObjectID(get_instance_id());
	result->version = parse_version;
	result->track = p_track;
	result->transition = p_transition;
	handle_id = result->get_instance_id();
	return result;
}

double HTMLLayout::animation_clock_time() const {
	return uses_owner_animation_clock ? state->animation_time : Time::get_singleton()->get_ticks_usec() / 1'000'000.0;
}

std::optional<syntax::PlaybackInfo> HTMLLayout::animation_info(uint64_t p_track, bool p_transition) const {
	const double now = animation_clock_time();
	auto info = p_transition ? state->transitions.info(p_track, now) : state->animations.info(p_track, now);
	if (info && !state->dom->is_element_connected(*info->target)) return std::nullopt;
	return info;
}

bool HTMLLayout::control_animation(uint64_t p_track, bool p_transition, syntax::PlaybackCommand p_command, double p_value) {
	if (!refresh_cssom() || !animation_info(p_track, p_transition)) return false;
	const double now = animation_clock_time();
	const bool updated = p_transition ? state->transitions.control(p_track, p_command, p_value, now) :
		state->animations.control(p_track, p_command, p_value, now);
	if (updated) sample_css_animations(true);
	return updated;
}

TypedArray<HTMLAnimation> HTMLLayout::get_animations(const Ref<HTMLElement>& p_element) {
	TypedArray<HTMLAnimation> result;
	if ((p_element.is_valid() && !is_element_connected(p_element)) || !refresh_cssom()) return result;
	const auto target = p_element.is_valid() ? p_element->lock_element() : nullptr;
	for (auto it = state->animation_handles.begin(); it != state->animation_handles.end();) {
		if (!ObjectDB::get_instance(it->second)) it = state->animation_handles.erase(it);
		else ++it;
	}
	for (const auto track : state->animations.list(target.get())) result.append(animation_handle(track, false));
	for (const auto track : state->transitions.list(target.get(), animation_clock_time())) result.append(animation_handle(track, true));
	return result;
}

Ref<HTMLAnimation> HTMLLayout::animate(const Ref<HTMLElement>& p_element, const TypedArray<Dictionary>& p_keyframes,
	const Dictionary& p_options) {
	if (!is_element_connected(p_element) || !refresh_cssom()) return {};
	const auto settings = animation_options(p_options);
	if (!settings) return {};
	const auto frames = script_keyframes(p_keyframes);
	const auto target = p_element->lock_element();
	if (!frames || !target) return {};
	const auto& document = state->dom->get_document();
	const auto track = state->animations.create(target, document, frames, *settings, animation_clock_time());
	if (!track) return {};
	const Ref<HTMLAnimation> result = animation_handle(track, false);
	if (state->animation_underlying.empty()) state->animation_underlying = syntax::TransitionState::capture(document->root());
	sample_css_animations(true);
	return result;
}

void HTMLLayout::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_animations", "element"), &HTMLLayout::get_animations, DEFVAL(Ref<HTMLElement>()));
	ClassDB::bind_method(D_METHOD("animate", "element", "keyframes", "options"), &HTMLLayout::animate, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_computed_style", "element", "pseudo"), &HTMLLayout::get_computed_style, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_computed_style_property", "element", "name", "pseudo"),
		&HTMLLayout::get_computed_style_property, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_stylesheets"), &HTMLLayout::get_stylesheets);
	ClassDB::bind_method(D_METHOD("get_css_rules", "stylesheet"), &HTMLLayout::get_css_rules);
	ClassDB::bind_method(D_METHOD("insert_rule", "stylesheet", "rule", "index"), &HTMLLayout::insert_rule, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("delete_rule", "stylesheet", "index"), &HTMLLayout::delete_rule);
	ClassDB::bind_method(D_METHOD("supports_css", "property_or_condition", "value"), &HTMLLayout::supports_css, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("supports_selector", "selector"), &HTMLLayout::supports_selector);
	ClassDB::bind_method(D_METHOD("matches_media", "query"), &HTMLLayout::matches_media);
	ClassDB::bind_method(D_METHOD("parse", "html", "base_dir", "stylesheet"), &HTMLLayout::parse,
		DEFVAL(String("res://")), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("layout", "viewport_size"), &HTMLLayout::layout);
	ClassDB::bind_method(D_METHOD("get_content_size"), &HTMLLayout::get_content_size);
	ClassDB::bind_method(D_METHOD("get_plain_text"), &HTMLLayout::get_plain_text);
	ClassDB::bind_method(D_METHOD("get_accessible_text"), &HTMLLayout::get_accessible_text);
	ClassDB::bind_method(D_METHOD("hit_test_text", "position"), &HTMLLayout::hit_test_text);
	ClassDB::bind_method(D_METHOD("get_selection_rects", "from", "to"), &HTMLLayout::get_selection_rects);
	ClassDB::bind_method(D_METHOD("draw", "canvas", "position", "clip"), &HTMLLayout::draw,
		DEFVAL(Vector2()), DEFVAL(Rect2()));
	ClassDB::bind_method(D_METHOD("get_link_at", "position"), &HTMLLayout::get_link_at);
	ClassDB::bind_method(D_METHOD("get_anchor_rect", "id"), &HTMLLayout::get_anchor_rect);
	ClassDB::bind_method(D_METHOD("create_element", "tag_name"), &HTMLLayout::create_element);
	ClassDB::bind_method(D_METHOD("create_document_fragment"), &HTMLLayout::create_document_fragment);
	ClassDB::bind_method(D_METHOD("get_document_node"), &HTMLLayout::get_document_node);
	ClassDB::bind_method(D_METHOD("create_range"), &HTMLLayout::create_range);
	ClassDB::bind_method(D_METHOD("get_selection"), &HTMLLayout::get_selection);
	ClassDB::bind_method(D_METHOD("get_document_element"), &HTMLLayout::get_document_element);
	ClassDB::bind_method(D_METHOD("get_head"), &HTMLLayout::get_head);
	ClassDB::bind_method(D_METHOD("get_body"), &HTMLLayout::get_body);
	ClassDB::bind_method(D_METHOD("get_base_uri"), &HTMLLayout::get_base_uri);
	ClassDB::bind_method(D_METHOD("get_title"), &HTMLLayout::get_title);
	ClassDB::bind_method(D_METHOD("set_title", "title"), &HTMLLayout::set_title);
	ClassDB::bind_method(D_METHOD("get_elements_by_tag_name", "name"), &HTMLLayout::get_elements_by_tag_name);
	ClassDB::bind_method(D_METHOD("get_elements_by_class_name", "names"), &HTMLLayout::get_elements_by_class_name);
	ClassDB::bind_method(D_METHOD("get_elements_by_name", "name"), &HTMLLayout::get_elements_by_name);
	ClassDB::bind_method(D_METHOD("create_text_node", "data"), &HTMLLayout::create_text_node);
	ClassDB::bind_method(D_METHOD("create_comment", "data"), &HTMLLayout::create_comment);
	ClassDB::bind_method(D_METHOD("parse_fragment", "html", "context"), &HTMLLayout::parse_fragment, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("import_fragment", "fragment"), &HTMLLayout::import_fragment);
	ClassDB::bind_method(D_METHOD("adopt_fragment", "fragment"), &HTMLLayout::adopt_fragment);
	ClassDB::bind_method(D_METHOD("import_node", "element", "deep"), &HTMLLayout::import_node, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("adopt_node", "element"), &HTMLLayout::adopt_node);
	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &HTMLLayout::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &HTMLLayout::query_selector_all);
	ClassDB::bind_method(D_METHOD("get_element_by_id", "id"), &HTMLLayout::get_element_by_id);
	ClassDB::bind_method(D_METHOD("get_element_at", "position"), &HTMLLayout::get_element_at);
	ClassDB::bind_method(D_METHOD("get_hovered_elements"), &HTMLLayout::get_hovered_elements);
	ClassDB::bind_method(D_METHOD("get_elements_at", "position"), &HTMLLayout::get_elements_at);
	ClassDB::bind_method(D_METHOD("get_render_error"), &HTMLLayout::get_render_error);
	ClassDB::bind_method(D_METHOD("set_css_diagnostics_enabled", "enabled"), &HTMLLayout::set_css_diagnostics_enabled);
	ClassDB::bind_method(D_METHOD("is_css_diagnostics_enabled"), &HTMLLayout::is_css_diagnostics_enabled);
	ClassDB::bind_method(D_METHOD("get_css_diagnostics"), &HTMLLayout::get_css_diagnostics);
	ClassDB::bind_method(D_METHOD("clear"), &HTMLLayout::clear);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "css_diagnostics_enabled"), "set_css_diagnostics_enabled", "is_css_diagnostics_enabled");
}

HTMLLayout::HTMLLayout() :
	state(std::make_unique<State>()) {
	state->dom = std::make_shared<HTMLDOM>([this] { invalidate_styles(); });
	set_dom_selection_change_callback({});
}

HTMLDOM& HTMLLayout::get_dom() const {
	return *state->dom;
}

HTMLLayout::~HTMLLayout() {
	clear();
}

Error HTMLLayout::parse(const String& p_html, const String& p_base_dir, const String& p_stylesheet) {
	return parse_source(p_html, p_base_dir, p_stylesheet, Dictionary());
}

Error HTMLLayout::parse_source(const String& p_html, const String& p_base_dir, const String& p_stylesheet,
	const Dictionary& p_textures, const String& p_source_path, bool p_fit_images, const String& p_default_stylesheet, const dockit::HTMLFontConfig& p_fonts,
	const Dictionary& p_local_stylesheets, const Dictionary& p_local_fonts, const Dictionary& p_image_dpi,
	const Dictionary& p_image_orientations, const Dictionary& p_svg_clip_sources, const Dictionary& p_audio_streams, const Dictionary& p_video_sources) {
	clear();
	const std::optional<String> base_dir = dockit::normalize_html_resource_path(p_base_dir);
	if (!base_dir) {
		state->error = "The base directory must use res:// or user://.";
		return state->parse_error = ERR_INVALID_PARAMETER;
	}
	state->audio_streams = p_audio_streams;
	state->video_sources = p_video_sources;
	try {
		state->container = std::make_unique<LiteHTMLContainer>(*state->dom, *base_dir, p_textures, p_fit_images, p_fonts, p_local_stylesheets,
			p_local_fonts, p_image_dpi, p_image_orientations, p_svg_clip_sources);
		state->container->set_remote_image_callback(callable_mp_static(&HTMLLayout::remote_image_completed).bind(get_instance_id(), parse_version));
		state->container->set_remote_stylesheet_callback(callable_mp_static(&HTMLLayout::remote_stylesheet_completed).bind(get_instance_id(), parse_version));
		state->container->set_document_source_path(p_source_path);
		state->container->set_css_diagnostics_enabled(state->css_diagnostics_enabled);
		state->container->set_preferred_color_scheme(system_preference());
		const DisplayServer* display = DisplayServer::get_singleton();
		state->container->set_accessibility_preferences(
			display && display->accessibility_should_reduce_animation() > 0,
			display && display->accessibility_should_increase_contrast() > 0);
		state->master_styles = litehtml::master_css;
		if (!p_default_stylesheet.is_empty()) {
			state->master_styles += '\n';
			state->master_styles += p_default_stylesheet.utf8().get_data();
		}
		const CharString stylesheet = p_stylesheet.utf8();
		state->user_styles.assign(stylesheet.get_data(), stylesheet.length());
		if (!p_html.strip_edges().is_empty() && !initialize_document(p_html.utf8())) {
			clear();
			state->error = "litehtml could not create the document.";
			return state->parse_error = ERR_PARSE_ERROR;
		}
		state->imported_video_base_dir = state->container->get_document_base().directory;
	}
	catch (const std::exception& p_error) {
		clear();
		state->error = "Could not parse HTML. " + String::utf8(p_error.what());
		return state->parse_error = ERR_PARSE_ERROR;
	}
	return OK;
}

bool HTMLLayout::initialize_document(const CharString& p_html) {
	state->dom->set_document(DocumentAdapter::parse(p_html, *state->container, state->master_styles, state->user_styles),
		ObjectID(get_instance_id()), parse_version);
	if (const auto document = state->dom->get_document()) {
		state->selector_state_dependencies = DocumentAdapter::selector_state_dependencies(*document);
		state->has_css_transitions = DocumentAdapter::has_transitions(*document);
	}
	return bool(state->dom->get_document());
}

bool HTMLLayout::ensure_document_for_editing() {
	if (!state->container || state->parse_error != OK) {
		return false;
	}
	if (state->dom->get_document()) {
		return true;
	}
	if (!initialize_document(String("<!doctype html>").utf8())) {
		return false;
	}
	state->implicit_empty_document = true;
	return true;
}

Error HTMLLayout::layout(const Vector2i& p_viewport_size) {
	return layout_viewport(p_viewport_size, p_viewport_size, p_viewport_size);
}

Error HTMLLayout::layout_embedded_block(const Vector2i& p_viewport_size) {
	state->embedded_block = true;
	return layout(p_viewport_size);
}

Error HTMLLayout::layout_viewport(const Vector2i& p_viewport_size, const Vector2i& p_media_size,
	const Vector2i& p_viewport_unit_size) {
	if (state->parse_error != OK) {
		return state->parse_error;
	}
	if (p_viewport_size.x <= 0 || p_viewport_size.y <= 0) {
		state->error = "The viewport width and height must be positive.";
		return ERR_INVALID_PARAMETER;
	}
	if (!state->container) {
		state->error = "Call parse() before laying out the document.";
		return ERR_UNCONFIGURED;
	}
	state->error = String();
	update_preferred_color_scheme();
	if (state->has_layout() && state->viewport_size == p_viewport_size &&
		state->media_viewport_size == p_media_size && state->viewport_unit_size == p_viewport_unit_size) {
		return OK;
	}
	using Update = State::Update;
	if (state->pending_update == Update::NONE) {
		state->pending_update = Update::LAYOUT;
	}
	if (state->dom->get_document() && !state->implicit_empty_document) {
		try {
			const Error result = reflow(p_viewport_size, p_media_size, p_viewport_unit_size);
			if (result != OK) {
				return result;
			}
		}
		catch (const std::exception& p_error) {
			state->anchors.clear();
			state->container->clear_recording();
			state->geometry.clear();
			state->error = "Could not lay out HTML. " + String::utf8(p_error.what());
			return ERR_INVALID_DATA;
		}
	}
	state->viewport_size = p_viewport_size;
	state->media_viewport_size = p_media_size;
	state->viewport_unit_size = p_viewport_unit_size;
	state->pending_update = Update::NONE;
	state->hover_viewport_style_changed = false;
	return OK;
}

Error HTMLLayout::reflow(const Vector2i& p_viewport_size, const Vector2i& p_media_size,
	const Vector2i& p_viewport_unit_size) {
	using Update = State::Update;
	const bool hover_layout_update = state->pending_update == Update::HOVER_LAYOUT;
	const bool viewport_units_changed = state->viewport_unit_size != p_viewport_unit_size;
	state->container->begin_image_layout();
	const auto& document = state->dom->get_document();
	const double transition_time = animation_clock_time();
	dockit::css::TransitionState::Snapshot before_transition;
	if (!state->sampling_transitions && state->viewport_size.x > 0) {
		before_transition = state->animation_underlying.empty() ?
			dockit::css::TransitionState::capture(document->root()) : std::move(state->animation_underlying);
	}
	bool transitions_committed = state->sampling_transitions;
	if (state->sampling_transitions) {
		state->transitions.sample(transition_time);
		state->animations.sample(transition_time);
	}
	state->container->set_transition_state(state->sampling_transitions && state->transitions.has_overrides() ? &state->transitions : nullptr);
	state->container->set_animation_state(state->sampling_transitions && state->animations.has_overrides() ? &state->animations : nullptr);
	state->container->set_viewport(p_viewport_size, p_media_size);
	DocumentAdapter::set_viewport_unit_size(*document, p_viewport_unit_size);
	litehtml::media_features image_media;
	document->container()->get_media_features(image_media);
	const dockit::HTMLImageSourceResolver resolve_image = [this](std::string_view p_source) {
		return state->container->resolve_image_source(p_source);
	};
	std::vector<litehtml::el_image*> images;
	bool image_hints_changed = false;
	auto& scroll_offsets = state->skipped_scroll_offsets;
	for (auto it = scroll_offsets.begin(); it != scroll_offsets.end();) {
		const auto element = it->first.lock();
		if (!element || !state->dom->is_element_connected(*element)) it = scroll_offsets.erase(it);
		else ++it;
	}
	HTMLDOM::visit_elements(document->root(), [&](const litehtml::element::ptr& p_element) {
		if (p_element->tag() == litehtml::_img_) {
			if (auto* image = dynamic_cast<litehtml::el_image*>(p_element.get())) {
				image_hints_changed |= image->select_source(dockit::select_html_image(*image, resolve_image, image_media));
				images.push_back(image);
			}
		}
		if (const auto* renderers = state->geometry.find_element(p_element.get())) {
			for (const auto* renderer : *renderers) {
				const auto& item = renderer->item;
				const Vector2 offset(item->get_scroll_left(), item->get_scroll_top());
				if (!item->scrollable_content_size()) continue;
				if (offset != Vector2()) scroll_offsets[p_element] = offset;
				else scroll_offsets.erase(p_element);
				break;
			}
		}
		return false;
	});
	const auto selected_content = selected_auto_content();
	state->anchors.clear();
	state->container->clear_recording();
	state->geometry.clear();
	if (state->pending_update == Update::STYLE || image_hints_changed || DocumentAdapter::media_changed(*document)) {
		state->pending_update = Update::STYLE;
		DocumentAdapter::restyle(document, *state->container, state->master_styles, state->user_styles);
		state->selector_state_dependencies = DocumentAdapter::selector_state_dependencies(*document);
		state->has_css_transitions = DocumentAdapter::has_transitions(*document);
		state->pending_update = Update::LAYOUT;
	}
	else if (!hover_layout_update || viewport_units_changed || state->sampling_transitions) {
		// Viewport units also change when all media queries keep matching.
		DocumentAdapter::compute_styles(*document);
		if (state->hover_layout_active) DocumentAdapter::rebuild_render_tree(*document);
	}
	Vector2 overflow;
	std::unordered_set<const litehtml::element*> revealed;
	std::set<std::pair<std::vector<bool>, std::vector<std::array<litehtml::pixel_t, 2>>>> container_states;
	for (;;) {
		if (uses_document_viewport) state->viewport_overflow = DocumentAdapter::propagate_viewport_overflow(*document);
		document->render(p_viewport_size.x);
		auto containers = DocumentAdapter::measure_containers(*document);
		if ((containers.rules_changed || containers.units_changed) &&
			container_states.emplace(std::move(containers.rule_matches), std::move(containers.dimensions)).second) {
			if (containers.rules_changed) {
				DocumentAdapter::restyle(document, *state->container, state->master_styles, state->user_styles);
			} else {
				DocumentAdapter::compute_styles(*document);
				DocumentAdapter::rebuild_render_tree(*document);
			}
			continue;
		}
		rebuild_document_geometry(p_viewport_size, false);
		const Vector2 original_size(document->width(), document->height());
		overflow = state->geometry.has_transforms() ? state->geometry.extend_overflow(original_size) : original_size;
		bool restored_scroll = false;
		for (const auto& [node, offset] : scroll_offsets) {
			const auto element = node.lock();
			if (const auto* renderers = element ? state->geometry.find_element(element.get()) : nullptr) {
				for (const auto* renderer : *renderers) {
					const auto& item = renderer->item;
					if (!item->scrollable_content_size()) continue;
					restored_scroll |= item->h_scroll(offset.x - item->get_scroll_left()) != 0;
					restored_scroll |= item->v_scroll(offset.y - item->get_scroll_top()) != 0;
					break;
				}
			}
		}
		if (state->geometry.has_sticky()) {
			rebuild_document_geometry(p_viewport_size);
		} else if (restored_scroll) {
			rebuild_document_geometry(p_viewport_size);
		}
		resnap_nested(p_viewport_size);
		if (!update_auto_content_visibility(p_viewport_size, selected_content, revealed)) {
			if (transitions_committed) break;
			transitions_committed = true;
			state->transitions.commit(document->root(), before_transition, transition_time,
				[this](const litehtml::element& element, const litehtml::css_transform_list& sample) {
					return state->geometry.can_interpolate_transform(element, sample);
				});
			state->animations.commit(document, transition_time);
			if (state->animations.has_overrides() || state->animations.active())
				state->animation_underlying = dockit::css::TransitionState::capture(document->root());
			if (!state->transitions.has_overrides() && !state->animations.has_overrides()) break;
			state->container->set_transition_state(&state->transitions);
			state->container->set_animation_state(&state->animations);
			container_states.clear();
			state->geometry.clear();
			DocumentAdapter::compute_styles(*document);
			continue;
		}
		state->geometry.clear();
		DocumentAdapter::compute_styles(*document);
		DocumentAdapter::rebuild_render_tree(*document);
	}
	const double width = std::ceil(overflow.x);
	const double height = std::ceil(overflow.y);
	constexpr double MAX_SIZE = std::numeric_limits<int32_t>::max();
	if (!std::isfinite(width) || !std::isfinite(height) || width > MAX_SIZE || height > MAX_SIZE) {
		state->geometry.clear();
		state->error = "The HTML content size is outside the supported range.";
		return ERR_INVALID_DATA;
	}
	const Vector2i size(static_cast<int32_t>(std::max(0.0, width)), static_cast<int32_t>(std::max(0.0, height)));
	state->content_size = size;
	DocumentAdapter::remember_content_sizes(*document);
	record_paint();
	DocumentAdapter::prune_unused_fonts(*document);
	for (auto* image : images) image->commit_source();
	update_transition_subscription();
	return OK;
}

bool HTMLLayout::update_auto_content_visibility(const Vector2i& p_viewport_size,
	const std::unordered_set<const litehtml::element*>& p_selected,
	std::unordered_set<const litehtml::element*>& r_revealed, bool p_test_only) {
	if (state->embedded_block || state->geometry.get_auto_content_boxes().empty()) return false;
	std::unordered_set<const litehtml::element*> focused;
	for (auto node = state->content_focus.lock(); node; node = node->parent()) focused.insert(node.get());
	bool changed = false;
	for (const auto* geometry : state->geometry.get_auto_content_boxes()) {
		const auto& node = geometry->item->src_el();
		auto& css = node->css_w();
		const bool was_skipped = css.skips_contents();
		const real_t margin = was_skipped ? 240 : 320;
		const Rect2 viewport(geometry->viewport_fixed_subtree ? Vector2() : Vector2(state->viewport_scroll_offset), p_viewport_size);
		const Rect2 nearby = viewport.grow(margin);
		const auto near_viewport = [&]() {
			if (!geometry->inverse) return false;
			std::vector<dockit::TransformedClip> clips;
			for (auto* clip = geometry->overflow; clip; clip = clip->parent) {
				if (clip->is_path) {
					clips.push_back(*clip);
					continue;
				}
				auto shape = clip->shape;
				const auto& overflow = clip->item->css();
				const real_t x = litehtml::is_scrollable_overflow(overflow.get_overflow_x()) ? margin : 0;
				const real_t y = litehtml::is_scrollable_overflow(overflow.get_overflow_y()) ? margin : 0;
				shape = shape.outset(shape.rect.grow_individual(x, y, x, y));
				clips.push_back({shape, clip->matrix, clip->axes});
			}
			const auto intersects = [&](Rect2 p_box) {
				p_box.size = p_box.size.max(Vector2(1, 1));
				if (!nearby.intersects(geometry->matrix.xform(p_box), true)) return false;
				if (clips.empty()) return true;
				const Rect2 visible = dockit::clipped_visual_bounds(p_box, geometry->matrix, clips);
				return visible.has_area() && nearby.intersects(visible, true);
			};
			for (const auto& fragment : geometry->fragments) {
				if (intersects(fragment.rect)) return true;
			}
			return geometry->anchor_fallback && intersects(*geometry->anchor_fallback);
		};
		const bool skip = !focused.count(node.get()) && !p_selected.count(node.get()) &&
			!r_revealed.count(node.get()) && !near_viewport();
		if (p_test_only) {
			if (skip != was_skipped) return true;
			continue;
		}
		if (css.set_auto_content_skipped(skip)) {
			changed = true;
			// A placeholder can move into range when another placeholder shrinks.
			// Keep freshly revealed content stable for the rest of this layout.
			if (!skip) r_revealed.insert(node.get());
		}
	}
	return changed;
}

std::unordered_set<const litehtml::element*> HTMLLayout::selected_auto_content() const {
	std::unordered_set<const litehtml::element*> selected;
	if (!state->container || !state->content_selection_callback.is_valid()) return selected;
	const auto& recorder = state->container->get_paint_recorder();
	const auto& ranges = recorder.get_auto_content_text();
	if (ranges.empty()) return selected;
	const Vector2i selection = state->content_selection_callback.call();
	if (selection.x >= selection.y) return selected;
	for (const auto& [element, range] : ranges) {
		if (recorder.has_selectable_text(std::max(range.x, selection.x), std::min(range.y, selection.y))) selected.insert(element);
	}
	return selected;
}

void HTMLLayout::request_auto_content_update() {
	if (!state->has_layout()) return;
	std::unordered_set<const litehtml::element*> revealed;
	if (update_auto_content_visibility(state->viewport_size, selected_auto_content(), revealed, true)) {
		state->pending_update = State::Update::LAYOUT;
		if (state->content_update_callback.is_valid()) state->content_update_callback.call();
		else queue_reflow();
	}
}

void HTMLLayout::set_content_visibility_callbacks(const Callable& p_update, const Callable& p_selection) {
	state->content_update_callback = p_update;
	state->content_selection_callback = p_selection;
}

void HTMLLayout::set_resource_layout_callback(const Callable& p_callback) {
	state->resource_layout_callback = p_callback;
}

void HTMLLayout::queue_resource_layout() {
	if (state->pending_update == State::Update::NONE) state->pending_update = State::Update::LAYOUT;
	if (state->resource_layout_callback.is_valid()) state->resource_layout_callback.call();
	else queue_reflow();
}

void HTMLLayout::set_print_media(bool p_enabled) {
	if (state->print_media == p_enabled) return;
	state->print_media = p_enabled;
	if (state->container) state->container->set_print_media(p_enabled);
	mark_styles_pending();
}

int HTMLLayout::pending_resource_count() const {
	return state->container ? state->container->pending_resource_count() : 0;
}

int HTMLLayout::get_failed_remote_resource_count() const {
	return state->container ? state->container->get_failed_remote_resource_count() : 0;
}

const dockit::HTMLPaintRecorder* HTMLLayout::get_paint_recorder() const {
	return state->container ? &state->container->get_paint_recorder() : nullptr;
}

void HTMLLayout::remote_stylesheet_completed(const String& p_url, const String& p_final_url, const String& p_text,
	uint64_t p_id, uint64_t p_version) {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_id)));
	if (!layout || layout->parse_version != p_version || !layout->state->container) return;
	if (!layout->state->container->receive_remote_stylesheet(p_url, p_final_url, p_text)) return;
	if (!p_final_url.is_empty()) layout->mark_styles_pending();
	layout->queue_resource_layout();
}

void HTMLLayout::remote_image_completed(const String& p_url, const Ref<Texture2D>& p_texture, uint64_t p_id, uint64_t p_version) {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_id)));
	if (!layout || layout->parse_version != p_version || !layout->state->container) return;
	auto& state = *layout->state;
	const auto update = state.container->receive_remote_image(p_url, p_texture);
	if (update == dockit::HTMLResources::ImageUpdate::none) {
		layout->queue_resource_layout();
		return;
	}
	if (update == dockit::HTMLResources::ImageUpdate::paint) {
		layout->repaint_input();
		return;
	}
	layout->queue_resource_layout();
}

void HTMLLayout::record_paint() {
	state->input_paint_pending = false;
	state->anchors.clear();
	collect_anchors(*state->dom->get_document()->root(), state->geometry, state->anchors);
	state->container->set_gauge_animation_time(uses_owner_animation_clock ? state->animation_time :
		Time::get_singleton()->get_ticks_msec() / 1000.0);
	state->container->record(*state->dom->get_document(), state->content_size, state->geometry, state->viewport_gutters);
	record_css_diagnostics();
}

Vector2i HTMLLayout::get_content_size() const {
	return state->content_size;
}

void HTMLLayout::draw(CanvasItem* p_canvas, const Vector2& p_position, const Rect2& p_clip) {
	draw_with_highlights(p_canvas, p_position, p_clip, get_node_selection_offsets(), nullptr, Color(0.2f, 0.45f, 0.85f, 0.4f));
}

String HTMLLayout::get_plain_text() const {
	return state->has_layout() ? state->container->get_paint_recorder().get_plain_text() : String();
}

bool HTMLLayout::has_current_layout() const {
	return state->has_layout();
}

String HTMLLayout::get_accessible_text() const {
	return state->has_layout() ? state->container->get_paint_recorder().get_accessible_text() : String();
}

int64_t HTMLLayout::hit_test_text(const Vector2& p_position) const {
	const auto modal = top_modal_node();
	return state->has_layout() ? state->container->get_paint_recorder().hit_test_text(p_position, modal.get()) : 0;
}

bool HTMLLayout::hit_test_selectable_text(const Vector2& p_position, bool* r_blank) const {
	const auto element = find_html_ancestor(hit_test_element(p_position));
	bool over_text = false;
	const bool selectable = element && state->container->get_paint_recorder().hit_test_selectable_text(
		p_position, *element, r_blank ? &over_text : nullptr);
	if (r_blank) *r_blank = state->has_layout() && !over_text && (!element || !element->is_replaced());
	return selectable;
}

dockit::HTMLSelectionPoint HTMLLayout::selection_point_at(const Vector2& p_position) const {
	const auto modal = top_modal_node();
	return state->has_layout() ? state->container->get_paint_recorder().selection_point_at(p_position,
		hit_test_element(p_position, nullptr, true), modal.get()) : dockit::HTMLSelectionPoint{};
}

String HTMLLayout::get_copyable_text(int64_t p_from, int64_t p_to) const {
	return state->has_layout() ? state->container->get_paint_recorder().get_copyable_text(p_from, p_to) : String();
}

TypedArray<Rect2> HTMLLayout::get_selection_rects(int64_t p_from, int64_t p_to) const {
	if (!state->has_layout()) {
		return {};
	}
	const int64_t length = state->container->get_paint_recorder().get_plain_text().length();
	const int64_t from = CLAMP(std::min(p_from, p_to), int64_t(0), length);
	const int64_t to = CLAMP(std::max(p_from, p_to), int64_t(0), length);
	return state->container->get_paint_recorder().get_selection_rects(from, to);
}

void HTMLLayout::draw_with_highlights(CanvasItem* p_canvas, const Vector2& p_position, const Rect2& p_clip,
	const Vector2i& p_selection, const StyleBox* p_selection_style, const Color& p_selection_color,
	const Vector2i& p_search, const StyleBox* p_search_style, const Vector2& p_viewport_origin) {
	if (!state->has_layout() || !p_canvas || !p_canvas->is_inside_tree()) {
		return;
	}
	if (state->input_paint_pending) record_paint();
	const auto& runs = state->container->get_paint_recorder().get_paint_runs();
	const auto& diagnostics = state->container->get_paint_recorder().get_diagnostic_runs();
	if (state->canvas_renderer.is_null()) {
		state->canvas_renderer.instantiate();
	}
	const HTMLHighlight selection{ p_selection.x, p_selection.y, p_selection_style, p_selection_color };
	const HTMLHighlight search{ p_search.x, p_search.y, p_search_style, Color(0, 0, 0, 0) };
	state->canvas_renderer->submit(*p_canvas, p_position, p_clip, runs, state->container->get_paint_recorder().get_paint_groups(),
		diagnostics, selection, search, p_viewport_origin);
}

void HTMLLayout::draw_selection_drag_image(CanvasItem* p_canvas, const Vector2i& p_selection,
	const Vector2& p_position, const Rect2& p_clip) {
	if (!state->has_layout() || !p_canvas || !p_canvas->is_inside_tree()) {
		return;
	}
	if (state->input_paint_pending) record_paint();
	const auto& runs = state->container->get_paint_recorder().get_paint_runs();
	if (state->canvas_renderer.is_null()) {
		state->canvas_renderer.instantiate();
	}
	static const std::vector<dockit::HTMLPaintGroup> no_groups;
	static const std::vector<dockit::HTMLPaintRun> no_diagnostics;
	const int64_t from = std::min(p_selection.x, p_selection.y);
	const int64_t to = std::max(p_selection.x, p_selection.y);
	const dockit::HTMLHighlight selection{ from, to, nullptr, Color(0, 0, 0, 0) };
	const dockit::HTMLHighlight search{};
	state->canvas_renderer->submit(*p_canvas, p_position, p_clip, runs, no_groups, no_diagnostics,
		selection, search, Vector2(), true);
}

String HTMLLayout::get_link_at(const Vector2& p_position) const {
	return find_link_at(p_position).uri;
}

Rect2 HTMLLayout::get_anchor_rect(const String& p_id) const {
	const auto* anchor = state->anchors.getptr(p_id);
	return anchor ? anchor->bounds : Rect2();
}

const HashMap<String, HTMLAnchor>& HTMLLayout::get_anchors() const {
	return state->anchors;
}

Ref<HTMLElement> HTMLLayout::create_element(const String& p_tag_name) {
	const CharString tag = HTMLDOM::normalize_tag_name(p_tag_name);
	if (tag.length() == 0) {
		return {};
	}
	try {
		if (ensure_document_for_editing()) {
			return state->dom->create_element(tag);
		}
	}
	catch (const std::exception&) {
		return {};
	}
	return {};
}

Ref<HTMLDocumentFragment> HTMLLayout::create_document_fragment() {
	try {
		if (ensure_document_for_editing()) {
			return state->dom->create_document_fragment();
		}
	}
	catch (const std::exception&) {
		return {};
	}
	return {};
}

Ref<HTMLDocumentNode> HTMLLayout::get_document_node() const {
	return state->dom->get_document_node();
}

Ref<HTMLRange> HTMLLayout::create_range() {
	return ensure_document_for_editing() ? state->dom->create_range() : Ref<HTMLRange>();
}

Ref<HTMLSelection> HTMLLayout::get_selection() const {
	return state->dom->get_selection();
}

void HTMLLayout::set_dom_selection_change_callback(const Callable& p_callback) {
	state->dom->set_selection_callback([this, p_callback] {
		if (state->canvas_renderer.is_valid()) state->canvas_renderer->queue_redraws();
		if (p_callback.is_valid()) p_callback.call();
		callable_mp(this, &HTMLLayout::request_auto_content_update).call_deferred();
	});
}

dockit::HTMLBoundary HTMLLayout::text_boundary_at(int64_t p_offset) const {
	if (!state->has_layout()) return {};
	const auto& recorder = state->container->get_paint_recorder();
	const auto& spans = recorder.get_text_index_spans();
	const auto at = std::lower_bound(spans.begin(), spans.end(), p_offset,
		[](const auto& p_span, int64_t p_index) { return p_span.to < p_index; });
	const auto mapped = [&](const auto& p_span, int64_t p_local) {
		const auto point = state->dom->text_boundary(p_span.source.lock(), p_local);
		return point.node.is_valid() && point.node->is_connected() ? point : dockit::HTMLBoundary();
	};
	if (at != spans.end() && p_offset >= at->from) {
		if (const auto point = mapped(*at, p_offset - at->from); point.node.is_valid()) return point;
	}
	for (auto it = at; it != spans.begin();) {
		--it;
		if (const auto point = mapped(*it, it->to - it->from); point.node.is_valid()) return point;
	}
	for (auto it = at; it != spans.end(); ++it) {
		if (const auto point = mapped(*it, 0); point.node.is_valid()) return point;
	}
	return {};
}

int64_t HTMLLayout::document_text_offset(const dockit::HTMLBoundary& p_boundary, bool p_forward) const {
	if (!state->has_layout() || p_boundary.node.is_null() || !p_boundary.node->is_connected()) return 0;
	const auto& recorder = state->container->get_paint_recorder();
	int64_t previous = 0;
	for (const auto& span : recorder.get_text_index_spans()) {
		const auto native = span.source.lock();
		if (!native) continue;
		const auto point = state->dom->text_boundary(native, 0);
		if (point.node.is_null() || !point.node->is_connected()) continue;
		if (p_boundary.node == point.node) {
			if (p_boundary.offset < point.offset) return p_forward ? span.from : previous;
			if (p_boundary.offset <= point.offset + span.to - span.from) return span.from + p_boundary.offset - point.offset;
		} else if (dockit::compare_html_boundaries(p_boundary, point) < 0) {
			return p_forward ? span.from : previous;
		}
		previous = span.to;
	}
	return previous;
}

TypedArray<Rect2> HTMLLayout::range_client_rects(const HTMLRange& p_range) const {
	if (p_range.is_collapsed() || !p_range.start.node->is_connected() || !p_range.end.node->is_connected()) return {};
	const auto from = document_text_offset(p_range.start);
	const auto to = document_text_offset(p_range.end, false);
	return from < to ? get_selection_rects(from, to) : TypedArray<Rect2>();
}

Vector2i HTMLLayout::get_node_selection_offsets() const {
	const auto selection = get_selection();
	const auto range = selection.is_valid() ? selection->get_range_at(0) : Ref<HTMLRange>();
	if (range.is_null()) return {};
	const auto from = document_text_offset(range->start);
	const auto to = range->is_collapsed() ? from : std::max(from, document_text_offset(range->end, false));
	return Vector2i(from, to);
}

bool HTMLLayout::set_node_selection_offsets(int64_t p_anchor, int64_t p_focus) {
	const auto selection = get_selection();
	if (selection.is_null()) return false;
	const auto anchor = text_boundary_at(p_anchor);
	const auto focus = text_boundary_at(p_focus);
	if (anchor.node.is_null() || focus.node.is_null()) {
		selection->remove_all_ranges();
		return false;
	}
	return selection->set_base_and_extent(anchor.node, anchor.offset, focus.node, focus.offset) == OK;
}

Ref<HTMLElement> HTMLLayout::get_document_element() const {
	const auto& document = state->dom->get_document();
	return document ? state->dom->make_element(document->root()) : Ref<HTMLElement>();
}

Ref<HTMLElement> HTMLLayout::get_head() const {
	return query_selector("html > head");
}

Ref<HTMLElement> HTMLLayout::get_body() const {
	return query_selector("html > body, html > frameset");
}

String HTMLLayout::get_base_uri() const {
	return state->container ? state->container->get_document_base().uri : String();
}

String HTMLLayout::resolve_link_href(const String& p_href, bool p_use_fallback) const {
	if (!state->container) return p_href;
	return dockit::resolve_html_link_href(p_href,
		p_use_fallback ? state->container->get_fallback_base() : state->container->get_document_base());
}

Ref<HTMLElement> HTMLLayout::get_title_element() const {
	const auto& document = state->dom->get_document();
	Ref<HTMLElement> title;
	if (document) {
		HTMLDOM::visit_elements(document->root(), [&](const auto& p_node) {
			if (p_node->tag() != litehtml::_title_ || !p_node->is_html_namespace()) return false;
			title = state->dom->make_element(p_node);
			return true;
		});
	}
	return title;
}

String HTMLLayout::get_title() const {
	const auto title = get_title_element();
	if (title.is_null()) return {};
	const String value = title->get_text_content().replace("\t", " ").replace("\r", " ").replace("\n", " ").replace("\f", " ");
	return String(" ").join(value.split(" ", false));
}

Error HTMLLayout::set_title(const String& p_title) {
	if (!ensure_document_for_editing()) return ERR_UNCONFIGURED;
	if (const auto title = get_title_element(); title.is_valid()) return title->set_text_content(p_title);
	auto head = get_head();
	const bool needs_head = head.is_null();
	if (needs_head) head = create_element("head");
	const auto title = create_element("title");
	if (title.is_null() || head.is_null()) return ERR_CANT_CREATE;
	const Error error = title->set_text_content(p_title);
	if (error != OK) return error;
	const Error inserted = head->append_child(title);
	return inserted == OK && needs_head ? get_document_element()->prepend(Array::make(head)) : inserted;
}

TypedArray<HTMLElement> HTMLLayout::get_elements_by_tag_name(const String& p_name) const {
	return state->dom->get_elements(p_name, HTMLDOM::Lookup::TAG);
}

TypedArray<HTMLElement> HTMLLayout::get_elements_by_class_name(const String& p_names) const {
	return state->dom->get_elements(p_names, HTMLDOM::Lookup::CLASS);
}

TypedArray<HTMLElement> HTMLLayout::get_elements_by_name(const String& p_name) const {
	return state->dom->get_elements(p_name, HTMLDOM::Lookup::NAME);
}

Ref<HTMLTextNode> HTMLLayout::create_text_node(const String& p_data) {
	return ensure_document_for_editing() ? Ref<HTMLTextNode>(state->dom->create_character_node(p_data, false)) : Ref<HTMLTextNode>();
}

Ref<HTMLCommentNode> HTMLLayout::create_comment(const String& p_data) {
	return ensure_document_for_editing() ? Ref<HTMLCommentNode>(state->dom->create_character_node(p_data, true)) : Ref<HTMLCommentNode>();
}

Ref<HTMLDocumentFragment> HTMLLayout::parse_fragment(const String& p_html, const Ref<HTMLElement>& p_context) {
	const auto context = p_context.is_valid() ? p_context->lock_element() : nullptr;
	if (p_context.is_valid() && (!context || p_context->get_owner() != this)) return {};
	try {
		if (ensure_document_for_editing()) return state->dom->parse_fragment(p_html, context);
	} catch (const std::exception&) {
		return {};
	}
	return {};
}

Ref<HTMLDocumentFragment> HTMLLayout::import_fragment(const Ref<HTMLDocumentFragment>& p_fragment) {
	try {
		if (p_fragment.is_valid() && ensure_document_for_editing()) return state->dom->import_fragment(*p_fragment.ptr());
	} catch (const std::exception&) {
		return {};
	}
	return {};
}

Ref<HTMLDocumentFragment> HTMLLayout::adopt_fragment(const Ref<HTMLDocumentFragment>& p_fragment) {
	try {
		if (p_fragment.is_valid() && ensure_document_for_editing() && state->dom->adopt_fragment(*p_fragment.ptr())) return p_fragment;
	} catch (const std::exception&) {
		return {};
	}
	return {};
}

Ref<HTMLElement> HTMLLayout::import_node(const Ref<HTMLElement>& p_element, bool p_deep) {
	const auto node = p_element.is_valid() ? p_element->lock_element() : nullptr;
	if (!node) {
		return {};
	}
	try {
		if (ensure_document_for_editing()) {
			return state->dom->copy_node(p_element->get_dom(), *node, p_deep);
		}
	}
	catch (const std::exception&) {
		return {};
	}
	return {};
}

Ref<HTMLElement> HTMLLayout::adopt_node(const Ref<HTMLElement>& p_element) {
	const auto node = p_element.is_valid() ? p_element->lock_element() : nullptr;
	if (!node) {
		return {};
	}
	auto& source = p_element->get_dom();
	const auto& source_document = source.get_document();
	if (!source_document || source_document->root() == node) {
		return {};
	}
	try {
		if (ensure_document_for_editing() && state->dom->adopt_node(source, node)) {
			return p_element;
		}
	}
	catch (const std::exception&) {
		return {};
	}
	return {};
}

void HTMLLayout::set_edit_callback(const Callable& p_callback) {
	state->edit_callback = p_callback;
}

void HTMLLayout::set_element_scroll_callback(const Callable& p_callback) {
	state->element_scroll_callback = p_callback;
}

void HTMLLayout::set_transition_callback(const Callable& p_callback) {
	state->transition_callback = p_callback;
}

void HTMLLayout::update_transition_subscription() {
	if (uses_owner_animation_clock) return;
	if (!has_active_css_animations()) {
		disconnect_transition_frames();
		return;
	}
	if (state->transition_frame.is_valid()) return;
	auto* tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
	if (!tree) return;
	state->transition_frame = callable_mp(this, &HTMLLayout::sample_transition_frame).bind(parse_version);
	state->transition_tree = tree->get_instance_id();
	tree->connect("process_frame", state->transition_frame);
}

void HTMLLayout::disconnect_transition_frames() {
	auto* tree = Object::cast_to<SceneTree>(ObjectDB::get_instance(state->transition_tree));
	if (tree && state->transition_frame.is_valid() && tree->is_connected("process_frame", state->transition_frame))
		tree->disconnect("process_frame", state->transition_frame);
	state->transition_frame = {};
	state->transition_tree = ObjectID();
}

void HTMLLayout::sample_transition_frame(uint64_t p_version) {
	if (parse_version == p_version) sample_css_animations();
}

bool HTMLLayout::has_active_css_animations() const {
	return state->error.is_empty() && (state->transitions.active() || state->animations.active() ||
		(state->container && state->container->has_gauge_animation()));
}

void HTMLLayout::advance_css_animations(double p_delta) {
	if (!has_active_css_animations() || !std::isfinite(p_delta) || p_delta <= 0.0) return;
	state->animation_time += p_delta;
	sample_css_animations();
}

void HTMLLayout::sample_css_animations(bool p_force) {
	if (!state->has_layout()) return;
	if (!p_force && !state->transitions.active() && !state->animations.active() && state->container->has_gauge_animation()) {
		repaint_input();
		return;
	}
	Ref<HTMLLayout> keep_alive(this);
	auto& current = *state;
	const auto previous_size = current.content_size;
	current.sampling_transitions = true;
	current.pending_update = State::Update::LAYOUT;
	const auto result = layout_viewport(current.viewport_size, current.media_viewport_size, current.viewport_unit_size);
	current.sampling_transitions = false;
	if (result != OK) {
		disconnect_transition_frames();
		return;
	}
	if (current.canvas_renderer.is_valid()) current.canvas_renderer->queue_redraws();
	dispatch_css_animation_events();
	if (current.transition_callback.is_valid()) current.transition_callback.call(current.content_size != previous_size);
}

void HTMLLayout::dispatch_css_animation_events() {
	using EventKey = std::pair<bool, uint64_t>;
	using PlaybackState = dockit::css::PlaybackState;
	const auto dom = state->dom;
	if (!dom) return;
	const double now = animation_clock_time();
	std::map<EventKey, State::AnimationEventTrack> current_tracks;
	const auto dispatch_event = [&](bool p_transition, const Ref<HTMLElement>& p_target, const String& p_name,
		const String& p_pseudo, const StringName& p_type, double p_elapsed) {
		if (p_target.is_null() || !p_target->is_valid()) return;
		Ref<HTMLEvent> event;
		if (p_transition) {
			Ref<HTMLTransitionEvent> transition_event;
			transition_event.instantiate();
			transition_event->set_property_name(p_name);
			transition_event->set_pseudo_element(p_pseudo);
			transition_event->set_elapsed_time(p_elapsed);
			event = transition_event;
		} else {
			Ref<HTMLAnimationEvent> animation_event;
			animation_event.instantiate();
			animation_event->set_animation_name(p_name);
			animation_event->set_pseudo_element(p_pseudo);
			animation_event->set_elapsed_time(p_elapsed);
			event = animation_event;
		}
		event->set_type(p_type);
		event->set_bubbles(true);
		event->set_composed(true);
		event->mark_trusted();
		dom->dispatch_event(p_target, event);
	};
	const auto collect = [&](bool p_transition, const std::vector<uint64_t>& p_ids) {
		for (uint64_t id : p_ids) {
			const auto info = p_transition ? state->transitions.info(id, now) : state->animations.info(id, now);
			if (!info || !dom->is_element_connected(*info->target) || (!p_transition && info->scripted)) continue;
			const EventKey key{p_transition, id};
			State::AnimationEventTrack track;
			track.target = info->target;
			track.name = String::utf8(info->name.c_str());
			track.pseudo = String::utf8(info->pseudo.c_str());
			track.time = info->time;
			track.duration = info->duration;
			track.iteration_duration = info->iteration_duration;
			track.delay = info->delay;
			track.playback_state = info->state;
			track.scripted = info->scripted;
			track.started = track.time >= track.delay;
			if (track.iteration_duration > 0.0 && std::isfinite(track.time)) {
				const double active_time = std::max(0.0, track.time - track.delay);
				track.iteration = uint64_t(std::floor(active_time / track.iteration_duration));
				if (std::isfinite(track.duration)) {
					const double total_iterations = track.duration / track.iteration_duration;
					const uint64_t last_iteration_boundary = total_iterations > 0.0 ?
						uint64_t(std::max(0.0, std::ceil(total_iterations) - 1.0)) : 0;
					track.iteration = std::min(track.iteration, last_iteration_boundary);
				}
			}
			const auto previous = state->animation_event_tracks.find(key);
			if (previous == state->animation_event_tracks.end()) {
				const Ref<HTMLElement> target = dom->make_element(info->target);
				if (p_transition) dispatch_event(true, target, track.name, track.pseudo, StringName("transitionrun"), 0.0);
				if (track.started) {
					dispatch_event(p_transition, target, track.name, track.pseudo,
						p_transition ? StringName("transitionstart") : StringName("animationstart"),
						p_transition ? 0.0 : std::max(0.0, -track.delay));
				}
				if (!p_transition && track.started && track.iteration_duration > 0.0) {
					for (uint64_t iteration = 0; iteration < track.iteration && iteration < 1000; ++iteration)
						dispatch_event(false, target, track.name, track.pseudo, StringName("animationiteration"), track.iteration_duration);
				}
				if (track.playback_state == PlaybackState::finished)
					dispatch_event(p_transition, target, track.name, track.pseudo,
						p_transition ? StringName("transitionend") : StringName("animationend"), track.duration);
			} else {
				const State::AnimationEventTrack& old = previous->second;
				const Ref<HTMLElement> target = dom->make_element(info->target);
				if (!old.started && track.started) {
					dispatch_event(p_transition, target, track.name, track.pseudo,
						p_transition ? StringName("transitionstart") : StringName("animationstart"),
						p_transition ? 0.0 : std::max(0.0, -track.delay));
				}
				if (!p_transition && track.started && track.iteration > old.iteration && track.iteration_duration > 0.0) {
					for (uint64_t iteration = old.iteration; iteration < track.iteration && iteration - old.iteration < 1000; ++iteration)
						dispatch_event(false, target, track.name, track.pseudo, StringName("animationiteration"), track.iteration_duration);
				}
				if (old.playback_state != PlaybackState::finished && track.playback_state == PlaybackState::finished)
					dispatch_event(p_transition, target, track.name, track.pseudo,
						p_transition ? StringName("transitionend") : StringName("animationend"), track.duration);
			}
			current_tracks.emplace(key, std::move(track));
		}
	};
	collect(false, state->animations.list(nullptr));
	collect(true, state->transitions.list(nullptr, now));
	for (const auto& [key, previous] : state->animation_event_tracks) {
		if (current_tracks.count(key)) continue;
		const bool transition = key.first;
		const auto info = transition ? state->transitions.info(key.second, now) : state->animations.info(key.second, now);
		const bool finished = info && info->state == PlaybackState::finished;
		if (const auto target_node = previous.target.lock()) {
			const Ref<HTMLElement> target = dom->make_element(target_node);
			dispatch_event(transition, target, previous.name, previous.pseudo,
				finished ? (transition ? StringName("transitionend") : StringName("animationend")) :
					(transition ? StringName("transitioncancel") : StringName("animationcancel")),
				finished ? previous.duration : std::clamp(previous.time - previous.delay, 0.0, previous.duration));
		}
	}
	state->animation_event_tracks = std::move(current_tracks);
}

void HTMLLayout::invalidate_styles() {
	if (const auto& document = state->dom->get_document()) {
		DocumentAdapter::prune_modals(*document);
		state->container->refresh_document_base(document->root());
	}
	state->dom->sync_open_dialogs();
	mark_styles_pending();
	state->implicit_empty_document = false;
	if (state->edit_callback.is_valid()) {
		state->edit_callback.call();
	}
	else {
		queue_reflow();
	}
}

void HTMLLayout::mark_styles_pending() {
	state->pending_update = State::Update::STYLE;
	state->hover_viewport_style_changed = false;
	state->anchors.clear();
}

void HTMLLayout::queue_reflow() {
	if (state->viewport_size != Vector2i() && !state->reflow_queued) {
		state->reflow_queued = true;
		callable_mp_static(&HTMLLayout::reflow_after_edit).call_deferred(get_instance_id(), parse_version);
	}
}

void HTMLLayout::reflow_after_edit(uint64_t p_id, uint64_t p_version) {
	auto* owner = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_id)));
	if (!owner || owner->parse_version != p_version) {
		return;
	}
	State& current = *owner->state;
	current.reflow_queued = false;
	if (!current.has_layout() && current.viewport_size != Vector2i()) {
		owner->layout_viewport(current.viewport_size, current.media_viewport_size, current.viewport_unit_size);
	}
	if (current.canvas_renderer.is_valid()) {
		current.canvas_renderer->queue_redraws();
	}
}

Ref<HTMLElement> HTMLLayout::query_selector(const String& p_selector) const {
	return state->dom->query_selector(p_selector);
}

TypedArray<HTMLElement> HTMLLayout::query_selector_all(const String& p_selector) const {
	return state->dom->query_selector_all(p_selector);
}

Ref<HTMLElement> HTMLLayout::get_element_by_id(const String& p_id) const {
	return state->dom->get_element_by_id(p_id);
}

TypedArray<Rect2> HTMLLayout::get_element_rects(const litehtml::element::ptr& p_element) const {
	TypedArray<Rect2> result;
	if (const auto* renderers = state->geometry.find_element(p_element.get()); state->has_layout() && renderers) {
		for (const auto* renderer : *renderers) {
			if (!renderer->item->skip()) {
				for (const Rect2& box : renderer->visual_boxes) {
					result.append(box);
				}
			}
		}
	}
	return result;
}

litehtml::element::ptr HTMLLayout::image_map_area_at(const litehtml::element::ptr& p_image, const Vector2& p_position) const {
	if (!state->has_layout() || !p_image || p_image->tag() != litehtml::_img_) return {};
	const String usemap = String::utf8(p_image->get_attr("usemap", "")).strip_edges();
	if (usemap.length() < 2 || usemap[0] != '#') return {};
	const String map_name = dockit::decode_html_fragment(usemap.substr(1));
	if (map_name.is_empty()) return {};
	const auto document = state->dom->get_document();
	auto* image = dynamic_cast<litehtml::el_image*>(p_image.get());
	const auto* renderers = state->geometry.find_element(p_image.get());
	if (!document || !image || !renderers) return {};

	const dockit::RenderGeometry* renderer = nullptr;
	for (const auto* candidate : *renderers) {
		if (candidate->item->is_visible() && candidate->inverse && candidate->contains_visible_point(p_position)) {
			renderer = candidate;
			break;
		}
	}
	if (!renderer) return {};

	litehtml::size natural_size{};
	image->get_content_size(natural_size, 0);
	const Vector2 intrinsic_size(natural_size.width, natural_size.height);
	const Rect2 content_box = dockit::to_rect(renderer->item->get_placement());
	if (!content_box.has_area()) return {};
	const Vector2 content_position = renderer->inverse->xform(p_position) - content_box.position;
	if (!content_box.has_point(renderer->inverse->xform(p_position))) return {};
	const Vector2 coordinate_size = intrinsic_size.x > 0 && intrinsic_size.y > 0 ? intrinsic_size : content_box.size;
	const Vector2 image_point = content_position * coordinate_size / content_box.size;

	litehtml::element::ptr map;
	HTMLDOM::visit_elements(document->root(), [&](const litehtml::element::ptr& p_element) {
		if (p_element->tag() != litehtml::_map_) return false;
		const String name = String::utf8(p_element->get_attr("name", ""));
		const String id = String::utf8(p_element->get_attr("id", ""));
		if (name != map_name && id != map_name) return false;
		map = p_element;
		return true;
	});
	if (!map) return {};

	litehtml::element::ptr area;
	HTMLDOM::visit_elements(map, [&](const litehtml::element::ptr& p_element) {
		if (p_element->tag() != litehtml::_area_ || !image_map_area_contains(*p_element, image_point, coordinate_size)) return false;
		area = p_element;
		return true;
	});
	return area;
}

const dockit::RenderGeometry* HTMLLayout::element_geometry(const HTMLElement& p_element) const {
	if (p_element.get_owner() != this || !state->has_layout()) return nullptr;
	const auto element = p_element.lock_element();
	const auto* boxes = element ? state->geometry.find_element(element.get()) : nullptr;
	if (!boxes) return nullptr;
	for (const auto* box : *boxes) {
		if (!box->item->skip() && (!box->fragments.empty() || box->anchor_fallback)) return box;
	}
	return nullptr;
}

namespace {
	Vector2 layout_content_origin(const litehtml::render_item& p_item) {
		const auto unscrolled = [](const litehtml::render_item& p_box) {
			const auto placement = p_box.calc_placement();
			return Vector2(placement.x + p_box.get_scroll_left(), placement.y + p_box.get_scroll_top());
		};
		Vector2 result = unscrolled(p_item);
		const litehtml::render_item* item = &p_item;
		for (auto parent = item->parent(); parent && !item->is_viewport_fixed(); parent = item->parent()) {
			item = parent.get();
			result += unscrolled(*item);
		}
		return result;
	}

	bool uses_viewport_dimensions(const litehtml::element& p_element) {
		return p_element.tag() == litehtml::_html_ || (p_element.tag() == litehtml::_body_ &&
			p_element.get_document()->mode() == litehtml::quirks_mode);
	}

	Vector2 padding_box_size(const litehtml::render_item& p_item) {
		const auto size = p_item.calc_placement();
		return {size.width + p_item.padding_left() + p_item.padding_right(),
			size.height + p_item.padding_top() + p_item.padding_bottom()};
	}
}

std::shared_ptr<litehtml::render_item> HTMLLayout::offset_parent_of(const litehtml::render_item& p_item) const {
	if (p_item.src_el()->tag() == litehtml::_body_ || p_item.is_root() || p_item.is_viewport_fixed()) return {};
	const bool static_position = p_item.css().get_position() == litehtml::element_position_static;
	for (auto parent = p_item.parent(); parent; parent = parent->parent()) {
		const auto node = parent->src_el();
		if (!HTMLDOM::is_element(*node)) continue;
		const auto tag = node->tag();
		if (tag == litehtml::_body_ || parent->establishes_positioned_containing_block() ||
			(static_position && (tag == litehtml::_table_ || tag == litehtml::_td_ || tag == litehtml::_th_))) return parent;
	}
	return {};
}

Vector2 HTMLLayout::element_client_size(const HTMLElement& p_element) const {
	const auto* box = element_geometry(p_element);
	if (!box) return {};
	const auto& item = *box->item;
	if (uses_viewport_dimensions(*item.src_el())) return Vector2(state->viewport_size);
	if (item.css().get_display() == litehtml::display_inline && !item.src_el()->is_replaced()) return {};
	return padding_box_size(item);
}

Vector2 HTMLLayout::element_client_offset(const HTMLElement& p_element) const {
	const auto* box = element_geometry(p_element);
	if (!box || uses_viewport_dimensions(*box->item->src_el()) ||
		(box->item->css().get_display() == litehtml::display_inline && !box->item->src_el()->is_replaced())) return {};
	return {box->item->border_left(), box->item->border_top()};
}

Ref<HTMLElement> HTMLLayout::element_offset_parent(const HTMLElement& p_element) const {
	const auto* box = element_geometry(p_element);
	const auto parent = box ? offset_parent_of(*box->item) : nullptr;
	return parent ? state->dom->make_element(parent->src_el()) : Ref<HTMLElement>();
}

Rect2 HTMLLayout::element_offset_rect(const HTMLElement& p_element) const {
	const auto* box = element_geometry(p_element);
	if (!box) return {};
	const auto parent = offset_parent_of(*box->item);
	const Vector2 origin = parent ? layout_content_origin(*parent) - Vector2(parent->padding_left(), parent->padding_top()) : Vector2();
	std::optional<Rect2> bounds;
	Vector2 first;
	for (const auto* geometry : *state->geometry.find_element(box->item->src_el().get())) {
		if (geometry->item->skip()) continue;
		const auto old = geometry->item->get_placement();
		const Vector2 correction = layout_content_origin(*geometry->item) - Vector2(old.x, old.y);
		for (const auto& fragment : geometry->fragments) {
			const Rect2 rect(fragment.rect.position + correction, fragment.rect.size);
			if (!bounds) first = rect.position;
			bounds = bounds ? bounds->merge(rect) : rect;
		}
	}
	return bounds ? Rect2(first - origin, bounds->size) : Rect2();
}

Vector2 HTMLLayout::element_scroll_size(const HTMLElement& p_element) const {
	const auto* box = element_geometry(p_element);
	if (!box) return {};
	if (uses_viewport_dimensions(*box->item->src_el())) return Vector2(state->viewport_size.max(state->content_size));
	const Vector2 client = element_client_size(p_element);
	const auto limit = box->item->scrollable_content_size();
	if (!limit) return client;
	const auto& size = box->item->pos();
	return client + Vector2(limit->width - size.width, limit->height - size.height).max(Vector2());
}

Vector2 HTMLLayout::element_scroll_offset(const HTMLElement& p_element) const {
	const auto* box = element_geometry(p_element);
	return box ? Vector2(box->item->get_scroll_left(), box->item->get_scroll_top()) : Vector2();
}

Error HTMLLayout::set_element_scroll_offset(const HTMLElement& p_element, const Vector2& p_offset) {
	const auto* box = element_geometry(p_element);
	if (!box || !p_offset.is_finite()) return ERR_INVALID_PARAMETER;
	const auto item = box->item;
	if (!item->scrollable_content_size() || item->is_root() || item->src_el()->is_replaced()) return OK;
	const Vector2 delta = p_offset - Vector2(item->get_scroll_left(), item->get_scroll_top());
	cancel_nested_scroll(*item->src_el());
	cancel_nested_snap(*item->src_el());
	const auto dx = litehtml::is_scrollable_overflow(item->css().get_overflow_x()) ? item->h_scroll(delta.x) : 0;
	const auto dy = litehtml::is_scrollable_overflow(item->css().get_overflow_y()) ? item->v_scroll(delta.y) : 0;
	if (dx || dy) {
		refresh_overflow_paint(true);
		if (state->element_scroll_callback.is_valid()) state->element_scroll_callback.call();
	}
	return OK;
}

Ref<HTMLElement> HTMLLayout::get_element_at(const Vector2& p_position) const {
	return wrap_hit_element(hit_test_element(p_position));
}

TypedArray<HTMLElement> HTMLLayout::get_elements_at(const Vector2& p_position) const {
	TypedArray<HTMLElement> result;
	const auto first = get_element_at(p_position);
	if (first.is_null()) return result;
	std::unordered_set<const HTMLElement*> visited{first.ptr()};
	result.append(first);
	state->container->get_paint_recorder().visit_hit_elements(p_position, false, [&](const auto& p_node) {
		if (!allows_modal_interaction(*p_node)) return false;
		const auto handle = wrap_hit_element(p_node);
		if (handle.is_valid() && visited.insert(handle.ptr()).second) result.append(handle);
		return false;
	});
	return result;
}

Ref<HTMLElement> HTMLLayout::wrap_hit_element(litehtml::element::ptr p_element) const {
	for (auto ancestor = p_element; ancestor; ancestor = ancestor->parent()) {
		if (litehtml::is_generated_tag(ancestor->tag())) {
			p_element = ancestor->parent();
		}
	}
	return state->dom->make_element(find_html_ancestor(std::move(p_element)));
}

litehtml::element::ptr HTMLLayout::hit_test_element(const Vector2& p_position, bool* r_scrollbar, bool p_text_selection) const {
	if (r_scrollbar) *r_scrollbar = false;
	const Rect2 bounds(Vector2(), Vector2(state->content_size.max(state->viewport_size)));
	if (!state->has_layout() || !state->dom->get_document() || !p_position.is_finite() || !bounds.has_point(p_position)) {
		return {};
	}
	const auto root = state->dom->get_document()->root_render();
	if (!root) {
		return {};
	}
	const auto resolve_image_map_target = [&](litehtml::element::ptr p_target) {
		if (p_target && p_target->tag() == litehtml::_img_) {
			if (const auto area = image_map_area_at(p_target, p_position); area && allows_modal_interaction(*area)) return area;
		}
		return p_target;
	};
	if (const auto bar = scrollbar_at(p_position)) {
		if (r_scrollbar) *r_scrollbar = true;
		return bar->element.lock();
	}
	if (top_modal_node()) {
		litehtml::element::ptr target;
		state->container->get_paint_recorder().visit_hit_elements(p_position, p_text_selection, [&](const auto& p_node) {
			if (!allows_modal_interaction(*p_node)) return false;
			target = p_node;
			return true;
		});
		return resolve_image_map_target(target);
	}
	if (state->geometry.uses_visual_hit_order()) {
		return resolve_image_map_target(state->container->get_paint_recorder().hit_test_element(p_position, p_text_selection));
	}
	const auto accepts_geometry = [&](const dockit::RenderGeometry* p_geometry) {
		return p_geometry && p_geometry->item->is_visible() && (p_text_selection || p_geometry->item->css().accepts_pointer_events()) &&
			p_geometry->contains_visible_point(p_position);
	};
	const auto accepts = [&](const std::shared_ptr<litehtml::render_item>& p_item) {
		// Some litehtml fallback paths offer another fragment of the same element.
		return accepts_geometry(state->geometry.find_renderer(p_item.get()));
	};
	struct SkippedItems {
		std::vector<litehtml::render_item*> items;
		~SkippedItems() {
			for (auto* item : items) {
				item->skip(false);
			}
		}
	};
	SkippedItems skipped;
	for (;;) {
		auto element = root->get_element_by_point(p_position.x, p_position.y, p_position.x, p_position.y, accepts);
		if (!element) {
			return {};
		}
		const auto* renderers = state->geometry.find_element(element.get());
		if (!renderers) {
			return {};
		}
		if (std::any_of(renderers->begin(), renderers->end(), accepts_geometry)) {
			return resolve_image_map_target(element);
		}

		// Exclude boxes rejected by the geometry index and retry in the same order.
		const size_t previous_size = skipped.items.size();
		for (const auto* renderer : *renderers) {
			auto* item = renderer->item.get();
			if (item->is_visible()) {
				skipped.items.push_back(item);
				item->skip(true);
			}
		}
		if (skipped.items.size() == previous_size) {
			return {};
		}
	}
}

Vector2 HTMLLayout::scroll_input_steps(const Ref<InputEvent>& p_event) {
	const Ref<InputEventPanGesture> pan = p_event;
	if (pan.is_valid()) {
		const Vector2 delta = pan->get_delta();
		return delta.is_finite() ? delta : Vector2();
	}
	const Ref<InputEventMouseButton> wheel = p_event;
	if (wheel.is_null() || !wheel->is_pressed()) return {};
	Vector2 delta;
	switch (wheel->get_button_index()) {
		case MOUSE_BUTTON_WHEEL_UP: delta.y = -1; break;
		case MOUSE_BUTTON_WHEEL_DOWN: delta.y = 1; break;
		case MOUSE_BUTTON_WHEEL_LEFT: delta.x = -1; break;
		case MOUSE_BUTTON_WHEEL_RIGHT: delta.x = 1; break;
		default: return {};
	}
	if (wheel->is_shift_pressed()) delta = Vector2(delta.y, delta.x);
	const float factor = wheel->get_factor();
	return std::isfinite(factor) ? delta * (factor > 0 ? factor : 1.0f) : Vector2();
}

bool HTMLLayout::forward_scroll_remainder(const Ref<InputEvent>& p_event, const Vector2& p_remaining) {
	if (p_remaining.is_zero_approx()) return false;
	// Godot transforms the GUI event for each parent. Carry only the unconsumed distance into that next dispatch.
	const Ref<InputEventPanGesture> pan = p_event;
	if (pan.is_valid()) {
		pan->set_delta(p_remaining);
	} else {
		const Ref<InputEventMouseButton> wheel = p_event;
		wheel->set_factor(std::max(std::abs(p_remaining.x), std::abs(p_remaining.y)));
	}
	return true;
}

dockit::HTMLScrollInput HTMLLayout::scroll_overflow_from_input(const Vector2& p_position, const Ref<InputEvent>& p_event,
	Node& p_owner, const Callable& p_changed) {
	constexpr real_t PIXELS_PER_STEP = 36;
	const Vector2 steps = scroll_input_steps(p_event);
	const Vector2 delta = steps * PIXELS_PER_STEP;
	const Ref<InputEventPanGesture> pan = p_event;
	const Vector2 consumed = scroll_overflow(p_position, delta, pan.is_valid() ? &p_owner : nullptr, p_changed);
	if (consumed == Vector2()) return dockit::HTMLScrollInput::ignored;
	return forward_scroll_remainder(p_event, (delta - consumed) / PIXELS_PER_STEP) ?
		dockit::HTMLScrollInput::forwarded : dockit::HTMLScrollInput::handled;
}

namespace {
	Vector2 block_overscroll(const litehtml::css_properties& p_css, const Vector2& p_remaining, bool p_viewport = false) {
		return {
			litehtml::blocks_scroll_chain(p_css.get_overscroll_x()) &&
				(p_viewport || litehtml::is_scrollable_overflow(p_css.get_overflow_x())) ? p_remaining.x : real_t(0),
			litehtml::blocks_scroll_chain(p_css.get_overscroll_y()) &&
				(p_viewport || litehtml::is_scrollable_overflow(p_css.get_overflow_y())) ? p_remaining.y : real_t(0),
		};
	}

	std::shared_ptr<litehtml::render_item> scroll_chain_parent(const litehtml::render_item& p_item) {
		if (p_item.css().get_position() == litehtml::element_position_fixed) return p_item.fixed_containing_block();
		auto parent = p_item.parent();
		if (p_item.css().get_position() == litehtml::element_position_absolute) {
			while (parent && !parent->is_root() && !parent->establishes_positioned_containing_block()) {
				parent = parent->parent();
			}
		}
		return parent;
	}

	std::shared_ptr<litehtml::render_item> nearest_scroll_container(const litehtml::render_item& p_item) {
		for (auto parent = scroll_chain_parent(p_item); parent; parent = scroll_chain_parent(*parent)) {
			if (parent->scrollable_content_size() && !parent->src_el()->is_replaced()) return parent;
		}
		return {};
	}

	std::vector<dockit::HTMLSnapArea> collect_snap_areas(const HTMLGeometryIndex& p_geometry,
		const litehtml::render_item* p_container, const dockit::HTMLPlaneTransform& p_from_document, const Vector2& p_origin) {
		std::vector<dockit::HTMLSnapArea> areas;
		std::unordered_set<const litehtml::element*> included;
		for (const auto* geometry : p_geometry.get_snap_areas()) {
			const auto& item = *geometry->item;
			if (!geometry->inverse || !item.is_visible() || item.is_root()) continue;
			if (!p_container && geometry->viewport_fixed_subtree) continue;
			if (nearest_scroll_container(item).get() != p_container) continue;
			const auto* element = item.src_el().get();
			if (!included.insert(element).second) continue;
			const auto* renderers = p_geometry.find_element(element);
			if (!renderers) continue;
			if (const auto bounds = scroll_target_bounds(*renderers, p_from_document, false)) {
				const auto area = dockit::html_scroll_margin_area(dockit::translate_rect(*bounds, p_origin), item.css());
				if (area.is_finite()) areas.push_back({area, &item.css(), element});
			}
		}
		return areas;
	}

	Vector2 item_scroll_position(const litehtml::render_item& p_item) {
		return {p_item.get_scroll_left(), p_item.get_scroll_top()};
	}

	Vector2 item_scroll_limit(const litehtml::render_item& p_item) {
		const auto size = p_item.scrollable_content_size();
		const auto box = p_item.get_placement();
		return size ? Vector2(size->width - box.width, size->height - box.height).max(Vector2()) : Vector2();
	}

	void prepare_snap_relayout(dockit::HTMLSnapRequest& r_request,
		const std::array<std::weak_ptr<const litehtml::element>, 2>& p_previous) {
		if (r_request.method != dockit::HTMLSnapMethod::relayout) return;
		for (int axis = 0; axis < 2; ++axis) r_request.preferred[axis] = p_previous[axis].lock().get();
	}

	void remember_snap_result(const dockit::HTMLSnapRequest& p_request, const dockit::HTMLSnapResult& p_result,
		std::array<std::weak_ptr<const litehtml::element>, 2>& r_memory) {
		for (int axis = 0; axis < 2; ++axis) {
			if (!p_request.axes[axis]) continue;
			r_memory[axis] = p_result.areas[axis] ? p_result.areas[axis]->weak_from_this() : std::weak_ptr<const litehtml::element>();
		}
	}
}

Vector2i HTMLLayout::viewport_snap_axes() const {
	const auto& document = state->dom->get_document();
	const auto root = document && state->has_layout() ? document->root() : nullptr;
	if (!root) return {};
	const auto& type = root->css().get_scroll_snap_type();
	return {type.x, type.y};
}

dockit::HTMLSnapResult HTMLLayout::snap_viewport(const Vector2& p_viewport, dockit::HTMLSnapRequest p_request) {
	if (viewport_snap_axes() == Vector2i()) {
		state->viewport_snap_memory = {};
		return {p_request.end.clamp(Vector2(), p_request.limit), {}};
	}
	const auto root = state->dom->get_document()->root();
	auto& memory = state->viewport_snap_memory.areas;
	prepare_snap_relayout(p_request, memory);
	const auto port = dockit::html_scroll_padding_area(Rect2(Vector2(), p_viewport), root->css());
	const auto areas = collect_snap_areas(state->geometry, nullptr, {}, {});
	const auto result = dockit::html_select_scroll_snap(root->css(), port, areas, p_request);
	remember_snap_result(p_request, result, memory);
	return result;
}

dockit::HTMLSnapResult HTMLLayout::snap_nested(const litehtml::render_item& p_container, dockit::HTMLSnapRequest p_request) {
	const auto element = p_container.src_el();
	const auto* geometry = state->geometry.find_renderer(&p_container);
	if (!p_container.css().get_scroll_snap_type().enabled() || !geometry || !geometry->inverse || !geometry->child_overflow) {
		state->nested_snap_memory.erase(element.get());
		return {p_request.end.clamp(Vector2(), p_request.limit), {}};
	}
	const auto& scrollport = geometry->child_overflow->shape.rect;
	p_request.axes.x = p_request.axes.x != 0 && litehtml::is_scrollable_overflow(p_container.css().get_overflow_x());
	p_request.axes.y = p_request.axes.y != 0 && litehtml::is_scrollable_overflow(p_container.css().get_overflow_y());
	p_request.limit = item_scroll_limit(p_container);
	auto& memory = state->nested_snap_memory[element.get()];
	memory.container = element;
	prepare_snap_relayout(p_request, memory.areas);
	const auto port = dockit::html_scroll_padding_area(Rect2(Vector2(), scrollport.size), p_container.css());
	const auto areas = collect_snap_areas(state->geometry, &p_container, *geometry->inverse,
		item_scroll_position(p_container) - scrollport.position);
	const auto result = dockit::html_select_scroll_snap(p_container.css(), port, areas, p_request);
	remember_snap_result(p_request, result, memory.areas);
	return result;
}

void HTMLLayout::resnap_nested(const Vector2i& p_viewport_size) {
	const auto containers = state->geometry.get_snap_containers();
	for (auto it = containers.rbegin(); it != containers.rend(); ++it) {
		auto& item = **it;
		const auto tracked = state->nested_snap_memory.find(item.src_el().get());
		if (tracked != state->nested_snap_memory.end() && tracked->second.pan_timer.is_valid()) continue;
		const auto animating = [&](const auto& p_motion) { return p_motion.second.element.lock() == item.src_el(); };
		if (std::any_of(state->scroll_motions.begin(), state->scroll_motions.end(), animating)) continue;
		const Vector2 current = item_scroll_position(item);
		const auto result = snap_nested(item, {current, current, item_scroll_limit(item), {1, 1}, dockit::HTMLSnapMethod::relayout});
		const Vector2 delta = result.offset - current;
		if (delta == Vector2()) continue;
		item.h_scroll(delta.x);
		item.v_scroll(delta.y);
		rebuild_document_geometry(p_viewport_size);
	}
	for (auto it = state->nested_snap_memory.begin(); it != state->nested_snap_memory.end();) {
		const auto live = it->second.container.lock();
		if (!live || !live->css().get_scroll_snap_type().enabled()) {
			if (it->second.pan_timer.is_valid()) it->second.pan_timer->kill();
			it = state->nested_snap_memory.erase(it);
		}
		else ++it;
	}
}

Vector2 HTMLLayout::viewport_contained_scroll(const Vector2& p_delta) const {
	const auto& document = state->dom->get_document();
	const auto root = document && state->has_layout() ? document->root() : nullptr;
	return root ? block_overscroll(root->css(), p_delta, true) : Vector2();
}

Vector2 HTMLLayout::scroll_overflow(const Vector2& p_position, const Vector2& p_delta, Node* p_pan_owner,
	const Callable& p_changed) {
	if (p_delta == Vector2() || !p_delta.is_finite() || !state->has_layout()) return {};
	flush_input_paint();

	std::shared_ptr<litehtml::render_item> target;
	const auto hit = hit_test_element(p_position);
	for (auto element = hit; element && !target; element = element->parent()) {
		const auto* renderers = state->geometry.find_element(element.get());
		if (!renderers) continue;
		for (const auto* renderer : *renderers) {
			if (renderer->item->is_visible() && renderer->contains_visible_point(p_position)) {
				target = renderer->item;
				break;
			}
		}
	}

	Vector2 remaining = p_delta;
	bool field_moved = false;
	if (auto* textarea = dynamic_cast<HTMLTextAreaNode*>(hit.get())) {
		const auto& style = textarea->css();
		const Vector2 field_delta(litehtml::is_interactive_overflow(style.get_overflow_x()) ? remaining.x : 0,
			litehtml::is_interactive_overflow(style.get_overflow_y()) ? remaining.y : 0);
		if (field_delta != Vector2()) {
			const Vector2 moved = textarea->editor.scroll_by(field_delta);
			field_moved = moved != Vector2();
			remaining -= moved;
		}
		remaining -= block_overscroll(style, remaining);
	}
	bool box_moved = false;
	for (auto item = target; item && remaining != Vector2(); item = scroll_chain_parent(*item)) {
		if (!item->scrollable_content_size() || item->src_el()->is_replaced()) continue;
		cancel_nested_scroll(*item->src_el());
		if (!p_pan_owner) cancel_nested_snap(*item->src_el());
		Vector2 delta(item->is_h_scrollable(remaining.x) ? remaining.x : 0,
			item->is_v_scrollable(remaining.y) ? remaining.y : 0);
		Vector2i stopped;
		if (!p_pan_owner && delta != Vector2() && item->css().get_scroll_snap_type().enabled()) {
			const Vector2 current = item_scroll_position(*item);
			const Vector2 limit = item_scroll_limit(*item);
			const auto snap = snap_nested(*item, {current, current + delta, limit,
				{delta.x != 0, delta.y != 0}, dockit::HTMLSnapMethod::direction});
			stopped = snap.stopped;
			for (int axis = 0; axis < 2; ++axis) {
				const bool at_boundary = delta[axis] > 0 ? snap.offset[axis] >= limit[axis] : snap.offset[axis] <= 0;
				if (snap.areas[axis] && !at_boundary) stopped[axis] = 1;
			}
			delta = snap.offset - current;
		}
		const Vector2 moved(item->h_scroll(delta.x), item->v_scroll(delta.y));
		if (p_pan_owner && moved != Vector2() && item->css().get_scroll_snap_type().enabled()) {
			queue_nested_snap(item->src_el(), {moved.x != 0, moved.y != 0}, *p_pan_owner, p_changed);
		}
		box_moved |= moved != Vector2();
		for (int axis = 0; axis < 2; ++axis) {
			if (stopped[axis] || std::abs(moved[axis]) >= std::abs(remaining[axis])) remaining[axis] = 0;
			else remaining[axis] -= moved[axis];
		}
		remaining -= block_overscroll(item->css(), remaining);
		if (moved != Vector2() && remaining != Vector2() && !state->geometry.get_snap_areas().empty()) rebuild_scroll_geometry();
	}
	if (field_moved || box_moved) refresh_overflow_paint(box_moved);
	return p_delta - remaining;
}

void HTMLLayout::refresh_overflow_paint(bool p_moved_boxes) {
	if (p_moved_boxes) rebuild_scroll_geometry();
	record_paint();
	if (state->canvas_renderer.is_valid()) state->canvas_renderer->queue_redraws();
	request_auto_content_update();
}

void HTMLLayout::rebuild_scroll_geometry() {
	rebuild_document_geometry(state->viewport_size);
}

void HTMLLayout::rebuild_document_geometry(const Vector2i& p_viewport_size, bool p_resolve_sticky) {
	const auto& document = state->dom->get_document();
	const auto& top = document->top_layer_render();
	const Rect2 viewport = !top.empty() || (p_resolve_sticky && state->geometry.has_sticky())
		? Rect2(state->viewport_scroll_offset, p_viewport_size) : Rect2();
	state->geometry.rebuild(document->root_render(), viewport, top);
}

Rect2 HTMLLayout::viewport_scroll_region(const Rect2& p_viewport) const {
	const auto& document = state->dom->get_document();
	const auto root = document ? document->root() : nullptr;
	return root ? dockit::html_scroll_padding_area(p_viewport, root->css()) : p_viewport;
}

bool HTMLLayout::viewport_uses_smooth_scroll() const {
	const auto& document = state->dom->get_document();
	const auto root = document && state->has_layout() ? document->root() : nullptr;
	return root && root->css().uses_smooth_scroll();
}

Ref<Tween> HTMLLayout::start_scroll_tween(Node& p_owner, const Callable& p_update) {
	Ref<Tween> tween = p_owner.create_tween();
	tween->set_ignore_time_scale();
	tween->set_trans(Tween::TRANS_CUBIC);
	tween->set_ease(Tween::EASE_OUT);
	tween->tween_method(p_update, 0.0, 1.0, 0.25);
	return tween;
}

Ref<Tween> HTMLLayout::start_snap_timer(Node& p_owner, const Callable& p_callback) {
	Ref<Tween> tween = p_owner.create_tween();
	tween->set_ignore_time_scale();
	tween->tween_interval(0.10);
	tween->tween_callback(p_callback);
	return tween;
}

void HTMLLayout::queue_nested_snap(const std::shared_ptr<litehtml::element>& p_element, const Vector2i& p_axes,
	Node& p_owner, const Callable& p_changed) {
	auto& memory = state->nested_snap_memory[p_element.get()];
	if (memory.pan_timer.is_valid()) memory.pan_timer->kill();
	memory.container = p_element;
	memory.pan_axes = memory.pan_axes.max(p_axes);
	memory.pan_id = ++state->next_scroll_motion;
	memory.changed = p_changed;
	memory.pan_timer = start_snap_timer(p_owner, callable_mp_static(&HTMLLayout::settle_nested_pan)
		.bind(get_instance_id(), memory.pan_id));
}

void HTMLLayout::cancel_nested_snap(const litehtml::element& p_element) {
	const auto found = state->nested_snap_memory.find(&p_element);
	if (found == state->nested_snap_memory.end() || found->second.pan_timer.is_null()) return;
	auto& memory = found->second;
	memory.pan_timer->kill();
	memory.pan_timer.unref();
	memory.pan_axes = {};
	memory.changed = {};
}

void HTMLLayout::settle_nested_pan(uint64_t p_layout_id, uint64_t p_motion_id) {
	auto* owner = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_layout_id)));
	if (!owner) return;
	for (auto& [key, memory] : owner->state->nested_snap_memory) {
		if (memory.pan_id != p_motion_id || memory.pan_timer.is_null()) continue;
		const auto element = memory.container.lock();
		const Vector2i axes = std::exchange(memory.pan_axes, Vector2i());
		const Callable changed = std::exchange(memory.changed, Callable());
		memory.pan_timer.unref();
		if (element && owner->settle_nested_snap(*element, axes) && changed.is_valid()) changed.call();
		return;
	}
}

void HTMLLayout::cancel_nested_scroll(const litehtml::element& p_element) {
	for (auto motion = state->scroll_motions.begin(); motion != state->scroll_motions.end();) {
		const auto element = motion->second.element.lock();
		if (!element || element.get() == &p_element) {
			motion->second.tween->kill();
			motion = state->scroll_motions.erase(motion);
		} else {
			++motion;
		}
	}
}

void HTMLLayout::cancel_scroll_activity() {
	for (const auto& [id, motion] : state->scroll_motions) motion.tween->kill();
	state->scroll_motions.clear();
	for (auto& [element, memory] : state->nested_snap_memory) {
		if (const auto container = memory.container.lock()) cancel_nested_snap(*container);
		else if (memory.pan_timer.is_valid()) {
			memory.pan_timer->kill();
			memory.pan_timer.unref();
		}
	}
}

void HTMLLayout::advance_nested_scroll(double p_fraction, uint64_t p_layout_id, uint64_t p_motion_id) {
	auto* owner = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_layout_id)));
	if (!owner) return;
	auto& state = *owner->state;
	const auto motion = state.scroll_motions.find(p_motion_id);
	if (motion == state.scroll_motions.end()) return;

	const auto& data = motion->second;
	const auto element = data.element.lock();
	bool moved = false;
	if (element && state.has_layout()) {
		if (const auto* renderers = state.geometry.find_element(element.get())) {
			for (const auto* renderer : *renderers) {
				auto& item = *renderer->item;
				if (!item.scrollable_content_size()) continue;
				const Vector2 next = data.from.lerp(data.to, p_fraction);
				const auto dx = item.h_scroll(next.x - item.get_scroll_left());
				const auto dy = item.v_scroll(next.y - item.get_scroll_top());
				moved = dx != 0 || dy != 0;
				break;
			}
		}
	}
	const Callable changed = data.changed;
	if (p_fraction >= 1.0) state.scroll_motions.erase(motion);
	if (moved) {
		owner->refresh_overflow_paint(true);
		if (changed.is_valid()) changed.call();
	}
}

std::optional<dockit::HTMLScrollTarget> HTMLLayout::scroll_anchor_into_view(const String& p_id, float p_align,
	Node& p_owner, const Callable& p_changed) {
	const auto* anchor = state->anchors.getptr(p_id);
	const auto element = anchor ? anchor->element.lock() : nullptr;
	return element ? scroll_element_into_view(*element, p_align, true, p_owner, p_changed) : std::nullopt;
}

std::optional<dockit::HTMLScrollTarget> HTMLLayout::scroll_element_into_view(const Ref<HTMLElement>& p_element,
	std::optional<float> p_align, Node& p_owner, const Callable& p_changed, const dockit::HTMLScrollOptions* p_options) {
	if (p_element.is_null() || p_element->get_owner() != this) return std::nullopt;
	const auto element = p_element->element.lock();
	return element ? scroll_element_into_view(*element, p_align, false, p_owner, p_changed, p_options) : std::nullopt;
}

std::optional<dockit::HTMLScrollTarget> HTMLLayout::scroll_element_into_view(const litehtml::element& p_element,
	std::optional<float> p_align, bool p_allow_fallback, Node& p_owner, const Callable& p_changed,
	const dockit::HTMLScrollOptions* p_options) {
	if (!state->has_layout() || !allows_modal_interaction(p_element)) return std::nullopt;
	const auto* renderers = state->geometry.find_element(&p_element);
	const auto* renderer = scroll_target_renderer(renderers, p_allow_fallback);
	if (!renderer) {
		const auto* placeholder = auto_content_placeholder(p_element);
		return placeholder ? scroll_element_into_view(*placeholder, p_align, true, p_owner, p_changed, p_options) : std::nullopt;
	}

	const bool viewport_fixed = renderer->viewport_fixed_subtree;
	const auto target_item = renderer->item;
	const auto closest_scroller = nearest_scroll_container(*target_item);
	bool moved = false;
	struct PlannedMotion {
		std::shared_ptr<litehtml::render_item> item;
		Vector2 from;
		Vector2 to;
	};
	std::vector<PlannedMotion> animated;
	// Each outer port needs the final geometry of the inner port, even when the inner trip is animated.
	for (auto item = target_item; item && !item->is_viewport_fixed();) {
		item = scroll_chain_parent(*item);
		if (!item || !item->scrollable_content_size() || item->src_el()->is_replaced()) continue;
		const auto* scroller = state->geometry.find_renderer(item.get());
		if (!scroller || !scroller->inverse || !scroller->child_overflow) continue;
		const auto bounds = scroll_target_bounds(*renderers, *scroller->inverse, p_allow_fallback);
		if (!bounds) continue;
		const auto& css = item->css();
		const Rect2 target = dockit::html_scroll_margin_area(*bounds, p_element.css());
		const Rect2 port = dockit::html_scroll_padding_area(scroller->child_overflow->shape.rect, css);
		Vector2 delta = dockit::html_scroll_delta(target, port, p_align, p_options);
		cancel_nested_scroll(*item->src_el());
		cancel_nested_snap(*item->src_el());
		const Vector2 from(item->get_scroll_left(), item->get_scroll_top());
		if (css.get_scroll_snap_type().enabled()) {
			const auto preferred = closest_scroller == item ? &p_element : nullptr;
			const dockit::HTMLSnapRequest request{from, from + delta, item_scroll_limit(*item), {1, 1},
				dockit::HTMLSnapMethod::destination, {preferred, preferred}};
			delta = snap_nested(*item, request).offset - from;
		}
		const auto dx = litehtml::is_scrollable_overflow(css.get_overflow_x()) ? item->h_scroll(delta.x) : 0;
		const auto dy = litehtml::is_scrollable_overflow(css.get_overflow_y()) ? item->v_scroll(delta.y) : 0;
		if (dx != 0 || dy != 0) {
			moved = true;
			const bool smooth = p_options ? p_options->smooth(css.uses_smooth_scroll()) : css.uses_smooth_scroll();
			if (smooth && p_owner.is_inside_tree()) {
				animated.push_back({item, from, Vector2(item->get_scroll_left(), item->get_scroll_top())});
			}
			rebuild_scroll_geometry();
			renderers = state->geometry.find_element(&p_element);
			if (!renderers) break;
		}
	}
	const auto bounds = renderers ? scroll_target_bounds(*renderers, {}, p_allow_fallback) : std::nullopt;
	for (const auto& motion : animated) {
		motion.item->h_scroll(motion.from.x - motion.to.x);
		motion.item->v_scroll(motion.from.y - motion.to.y);
		const uint64_t id = ++state->next_scroll_motion;
		const auto callback = callable_mp_static(&HTMLLayout::advance_nested_scroll).bind(get_instance_id(), id);
		state->scroll_motions.emplace(id, State::ScrollMotion{motion.item->src_el(), motion.from, motion.to,
			start_scroll_tween(p_owner, callback), p_changed});
	}
	if (moved) refresh_overflow_paint(!animated.empty());
	if (!bounds) return std::nullopt;
	return dockit::HTMLScrollTarget{dockit::html_scroll_margin_area(*bounds, p_element.css()), viewport_fixed, &p_element};
}

void HTMLLayout::set_viewport_scroll_offset(const Vector2i& p_offset) {
	if (state->viewport_scroll_offset == p_offset) return;
	state->viewport_scroll_offset = p_offset;
	if (state->has_layout() && (state->geometry.has_sticky() || top_modal_node())) refresh_overflow_paint(true);
	else request_auto_content_update();
}

void HTMLLayout::set_viewport_gutters(const Vector2& p_gutters) {
	if (state->viewport_gutters == p_gutters) return;
	state->viewport_gutters = p_gutters;
	state->input_paint_pending = true;
}

void HTMLLayout::draw_viewport_background(CanvasItem& p_canvas, const Vector2& p_position, const Rect2& p_clip,
	const Vector2& p_viewport_origin) {
	if (!state->has_layout() || !state->dom->get_document() || !p_canvas.is_inside_tree() || !p_clip.has_area()) return;
	if (state->input_paint_pending) record_paint();
	const auto& paint = state->container->get_paint_recorder();
	const auto& runs = paint.get_viewport_background();
	if (runs.empty()) return;
	if (state->canvas_renderer.is_null()) state->canvas_renderer.instantiate();
	state->canvas_renderer->submit(p_canvas, p_position, p_clip, runs, paint.get_viewport_background_groups(),
		{}, {}, {}, p_viewport_origin);
}

dockit::HTMLViewportStyle HTMLLayout::get_viewport_style() const {
	const auto& document = state->dom->get_document();
	const bool hover_styles_current = state->pending_update == State::Update::HOVER_LAYOUT;
	const auto root = document && (state->has_layout() || hover_styles_current) ? document->root() : nullptr;
	if (!root || root->css().get_display() == litehtml::display_none) return {};
	// Read the computed values, before viewport propagation changes the root box's used overflow.
	const dockit::css::ComputedStyle& css = root->css();
	return {dockit::html_scrollbar_appearance(css), state->viewport_overflow,
		{css.get_overflow_x(), css.get_overflow_y()}, css.get_scrollbar_gutter(), css.get_color_scheme()};
}

bool HTMLLayout::can_skip_hover_base_layout(const Vector2i& p_media_size) const {
	// The hover result and viewport inputs already agree; base layout would repeat the same reflow.
	return state->pending_update == State::Update::HOVER_LAYOUT && !state->hover_viewport_style_changed &&
		!state->sampling_transitions && state->viewport_size == p_media_size &&
		state->media_viewport_size == p_media_size && state->viewport_unit_size == p_media_size;
}

void HTMLLayout::complete_hover_layout() {
	state->hover_layout_active = false;
}

litehtml::color_scheme HTMLLayout::get_element_color_scheme(const Ref<HTMLElement>& p_element) const {
	if (p_element.is_valid() && p_element->get_owner() == this) {
		if (const auto element = p_element->lock_element()) return element->css().get_color_scheme();
	}
	return litehtml::color_scheme::light;
}

bool HTMLLayout::update_preferred_color_scheme() {
	if (!state->container) return false;
	const DisplayServer* display = DisplayServer::get_singleton();
	const bool changed = state->container->set_preferred_color_scheme(system_preference()) |
		state->container->set_accessibility_preferences(
			display && display->accessibility_should_reduce_animation() > 0,
			display && display->accessibility_should_increase_contrast() > 0);
	if (!changed) return false;
	mark_styles_pending();
	return true;
}

dockit::HTMLTextLayout HTMLLayout::shape_element_text(const Ref<HTMLElement>& p_element, const String& p_text) const {
	if (state->container && p_element.is_valid() && p_element->get_owner() == this) {
		if (const auto element = p_element->lock_element()) return state->container->shape_input(element->css().get_font(), p_text);
	}
	return {};
}
std::optional<dockit::HTMLScrollbarDrag> HTMLLayout::scrollbar_at(const Vector2& p_position) const {
	if (!state->has_layout() || !p_position.is_finite()) return {};
	const auto bar = state->container->get_paint_recorder().scrollbar_at(p_position);
	const auto owner = bar ? bar->element.lock() : nullptr;
	return owner && allows_modal_interaction(*owner) ? bar : std::nullopt;
}

void HTMLLayout::update_scrollbar_hover(const std::optional<Vector2>& p_position,
	const dockit::HTMLHitTestResult& p_hit_test) {
	if (!state->container) return;
	std::optional<dockit::HTMLScrollbarDrag> target;
	if (p_position && state->has_layout()) {
		target = scrollbar_at(*p_position);
		if (!target) {
			const auto hit_element = p_hit_test ? *p_hit_test : hit_test_element(*p_position);
			const Ref<HTMLElement> element = wrap_hit_element(hit_element);
			const auto input = text_control(element);
			if (const auto textarea = std::dynamic_pointer_cast<HTMLTextAreaNode>(input)) {
				if (const auto local = input_local_point(*element.ptr(), *p_position)) {
					const auto resize = textarea->resize_axes() != Vector2i() && textarea->get_resize_handle().has_point(*local);
					if (const auto axis = textarea->editor.scrollbar_at(*local); axis && !resize) {
						target = dockit::HTMLScrollbarDrag{textarea, *axis ? 1 : 0, 0};
					}
				}
			}
		}
	}
	if (state->container->get_paint_recorder().set_scrollbar_hover(target)) repaint_input();
}

void HTMLLayout::set_scrollbar_pressed(const std::optional<dockit::HTMLScrollbarDrag>& p_drag) {
	if (state->container && state->container->get_paint_recorder().set_scrollbar_pressed(p_drag)) repaint_input();
}

void HTMLLayout::drag_scrollbar(const dockit::HTMLScrollbarDrag& p_drag, const Vector2& p_position) {
	if (!state->has_layout() || !p_position.is_finite()) return;
	set_scrollbar_pressed(p_drag);
	const auto node = p_drag.element.lock();
	const auto* renderers = node ? state->geometry.find_element(node.get()) : nullptr;
	if (!renderers) return;
	cancel_nested_scroll(*node);
	cancel_nested_snap(*node);
	for (const auto* geometry : *renderers) {
		const auto bar = geometry->scrollbars()[p_drag.axis];
		if (bar.range <= 0) continue;
		const int axis = p_drag.axis;
		const real_t travel = bar.track.size[axis] - bar.thumb.size[axis];
		if (travel <= 0) continue;
		const real_t pointer = geometry->inverse->xform(p_position)[axis];
		if (!std::isfinite(pointer)) return;
		const real_t fraction = (pointer - bar.track.position[axis] - bar.thumb.size[axis] * p_drag.grab) / travel;
		const real_t target = std::clamp(fraction, real_t(0), real_t(1)) * bar.range;
		auto& item = *geometry->item;
		const real_t moved = axis == 0 ? item.h_scroll(target - item.get_scroll_left()) : item.v_scroll(target - item.get_scroll_top());
		if (moved != 0) refresh_overflow_paint(true);
		return;
	}
}

void HTMLLayout::finish_scrollbar_drag(const dockit::HTMLScrollbarDrag& p_drag) {
	set_scrollbar_pressed(std::nullopt);
	const auto node = p_drag.element.lock();
	if (!node) return;
	Vector2i axes;
	axes[p_drag.axis] = 1;
	settle_nested_snap(*node, axes);
}

bool HTMLLayout::settle_nested_snap(const litehtml::element& p_element, const Vector2i& p_axes) {
	const auto* renderers = state->has_layout() ? state->geometry.find_element(&p_element) : nullptr;
	if (!renderers) return false;
	for (const auto* renderer : *renderers) {
		auto& item = *renderer->item;
		if (!item.css().get_scroll_snap_type().enabled() || !item.scrollable_content_size()) continue;
		const Vector2 current = item_scroll_position(item);
		const auto snap = snap_nested(item, {current, current, item_scroll_limit(item), p_axes, dockit::HTMLSnapMethod::destination});
		const auto dx = item.h_scroll(snap.offset.x - current.x);
		const auto dy = item.v_scroll(snap.offset.y - current.y);
		if (dx == 0 && dy == 0) return false;
		refresh_overflow_paint(true);
		return true;
	}
	return false;
}

std::optional<Rect2> HTMLLayout::get_element_scroll_bounds(const Ref<HTMLElement>& p_element) const {
	if (!state->has_layout() || p_element.is_null() || p_element->get_owner() != this) {
		return std::nullopt;
	}
	const auto element = p_element->element.lock();
	const auto* renderers = state->geometry.find_element(element.get());
	std::optional<Rect2> bounds;
	if (renderers) {
		for (const auto* renderer : *renderers) {
			if (renderer->item->is_visible()) {
				for (const auto& fragment : renderer->fragments) {
					if (renderer->has_visible_area(fragment.rect)) {
						const Rect2 box = renderer->matrix.xform(fragment.rect);
						bounds = bounds ? bounds->merge(box) : box;
					}
				}
			}
		}
	}
	return bounds;
}

HTMLLayout::LinkHit HTMLLayout::find_link_at(const Vector2& p_position, Ref<HTMLElement>* r_element) const {
	bool scrollbar = false;
	auto element = hit_test_element(p_position, &scrollbar);
	if (r_element) {
		*r_element = scrollbar ? Ref<HTMLElement>() : wrap_hit_element(element);
	}
	if (scrollbar) return {};
	if (const auto link = find_link_element(std::move(element)); link && allows_modal_interaction(*link)) {
		return { String::utf8(link->get_attr("href")), reinterpret_cast<uintptr_t>(link.get()) };
	}
	return {};
}

String HTMLLayout::get_activatable_link_uri(const Ref<HTMLElement>& p_element) const {
	if (p_element.is_null() || p_element->get_owner() != this) return {};
	const auto link = p_element->lock_element();
	const char* href = link ? get_link_href(*link) : nullptr;
	return href && (link->tag() == litehtml::_area_ || get_element_scroll_bounds(p_element)) ? String::utf8(href) : String();
}

std::string_view HTMLLayout::get_cursor_at(const Vector2& p_position, bool& r_over_link, bool* r_over_selectable_text) const {
	const auto element = find_html_ancestor(hit_test_element(p_position));
	const auto link = find_link_element(element);
	r_over_link = link && allows_modal_interaction(*link);
	if (r_over_selectable_text) {
		*r_over_selectable_text = element && !r_over_link &&
			state->container->get_paint_recorder().hit_test_selectable_text(p_position, *element);
	}
	return element ? std::string_view(element->css().get_cursor()) : std::string_view();
}

Ref<HTMLElement> HTMLLayout::get_button_ancestor(const Ref<HTMLElement>& p_element) const {
	if (p_element.is_valid() && p_element->get_owner() == this) {
		const auto target = p_element->element.lock();
		bool allow_disclosure = true;
		for (auto element = target; element && allows_modal_interaction(*element); element = element->parent()) {
			if (DocumentAdapter::is_command_button(*element)) return state->dom->make_element(element);
			const auto disclosure = allow_disclosure ? dockit::details_activation_target(element) : nullptr;
			if (disclosure && (element != disclosure || element == target)) return state->dom->make_element(element);
			if ((element->tag() == litehtml::_a_ && element->get_attr("href")) ||
				dockit::forms::is_listed(*element)) allow_disclosure = false;
		}
	}
	return {};
}

bool HTMLLayout::is_enabled_button(const Ref<HTMLElement>& p_element) const {
	if (p_element.is_null() || p_element->get_owner() != this) {
		return false;
	}
	const auto element = p_element->element.lock();
	return element && (DocumentAdapter::is_command_button(*element) || dockit::details_activation_target(element)) && !dockit::forms::is_disabled(*element) &&
		get_element_scroll_bounds(p_element).has_value();
}

std::vector<dockit::HTMLSelectMenuRow> HTMLLayout::suggestion_menu_rows(const Ref<HTMLElement>& p_element) const {
	std::vector<dockit::HTMLSelectMenuRow> rows;
	if (!is_focusable(p_element) || !state->container) return rows;
	const auto input = std::dynamic_pointer_cast<HTMLInputNode>(p_element->lock_element());
	if (!input || !input->accepts_suggestions() || !input->is_editable() || input->editor.has_preedit()) return rows;
	const auto* id = input->get_attr("list");
	if (!id || !*id) return rows;
	const auto handle = state->dom->get_element_by_id(String::utf8(id));
	const auto list = handle.is_valid() ? handle->lock_element() : nullptr;
	if (!list || list->tag() != litehtml::_datalist_) return rows;
	const auto query = input->type() == HTMLInputNode::Type::COLOR ? String() : input->get_value().to_lower();
	std::set<String> seen;
	HTMLDOM::visit_elements(list, [&](const auto& p_node) {
		const auto option = std::dynamic_pointer_cast<dockit::HTMLOptionNode>(p_node);
		if (!option || dockit::forms::is_disabled(*option)) return false;
		const auto value = option->get_value();
		const auto label = option->get_label();
		if (value.is_empty() || (!query.is_empty() && value.to_lower().find(query) < 0 && label.to_lower().find(query) < 0)) return false;
		if (!input->is_suggestion_valid(value) || !seen.insert(value).second) return false;
		const auto text = label.is_empty() || label == value ? value : value + String("  ") + label;
		const auto color = input->css().get_color().to_srgb();
		rows.push_back({state->container->shape_input(input->css().get_font(), text),
			Color::from_rgba8(color.red, color.green, color.blue, color.alpha), {}, -1, true, value});
		return false;
	});
	return rows;
}

Ref<Texture2D> HTMLLayout::get_control_icon(dockit::HTMLControlIcon p_icon) const {
	return state->container ? state->container->get_control_icon(p_icon) : Ref<Texture2D>();
}

dockit::HTMLTextLayout HTMLLayout::shape_control_text(const Ref<HTMLElement>& p_element, const String& p_text,
	int p_size, bool p_bold) const {
	const auto node = is_element_connected(p_element) ? p_element->lock_element() : nullptr;
	if (!node || !state->container) return {};
	auto font = node->css().get_font();
	if (p_size > 0) {
		litehtml::font_description description;
		description.families = {{"Arial", false}, {"sans-serif", true}};
		description.size = p_size;
		description.weight = p_bold ? 700 : 400;
		litehtml::font_metrics metrics;
		font = node->get_document()->get_font(description, &metrics);
	}
	return state->container->shape_input(font, p_text);
}
std::vector<dockit::HTMLSelectMenuRow> HTMLLayout::select_menu_rows(const Ref<HTMLElement>& p_element) const {
	std::vector<dockit::HTMLSelectMenuRow> result;
	if (!is_focusable(p_element) || !state->container) return result;
	const auto select = std::dynamic_pointer_cast<dockit::HTMLSelectNode>(p_element->lock_element());
	if (!select) return result;
	const auto to_color = [](const litehtml::web_color& p_color) {
		return Color(p_color.red / 255.0f, p_color.green / 255.0f, p_color.blue / 255.0f, p_color.alpha / 255.0f);
	};
	const auto options = select->get_options();
	const int64_t selected = select->selected_index(options);
	const auto make_row = [&](const litehtml::element& p_node, const String& p_text, int64_t p_index, bool p_enabled) {
		const auto& style = p_node.css();
		const auto font = style.get_font() ? style.get_font() : select->css().get_font();
		const auto bg = style.get_bg().m_color.to_srgb(style.get_color());
		result.push_back({state->container->shape_input(font, p_text), to_color(style.get_color().to_srgb()),
			to_color(bg.alpha ? bg : select->css().get_bg().m_color.to_srgb(select->css().get_color())), p_index, p_enabled,
			{}, p_index >= 0 && p_index == selected});
	};
	litehtml::element::ptr previous_group;
	int64_t index = 0;
	for (const auto& option : options) {
		const int64_t option_index = index++;
		const auto parent = option->parent();
		const auto group = parent.get() != select.get() ? parent : nullptr;
		if (option->get_attr("hidden") || (group && group->get_attr("hidden"))) continue;
		if (group && group != previous_group) make_row(*group, String::utf8(group->get_attr("label", "")), -1, false);
		previous_group = group;
		make_row(*option, option->get_label(), option_index, !dockit::forms::is_disabled(*option));
		result.back().option = state->dom->make_element(option);
	}
	return result;
}

Vector2 HTMLLayout::get_image_intrinsic_size(const HTMLElement& p_element) const {
	if (!state->has_layout()) return {};
	const auto element = p_element.lock_element();
	auto* image = dynamic_cast<litehtml::el_image*>(element.get());
	if (!image || !state->dom->is_element_connected(*image)) return {};
	litehtml::size size{};
	image->get_content_size(size, 0);
	return Vector2(size.width, size.height);
}

String HTMLLayout::get_image_current_src(const HTMLElement& p_element) const {
	const auto element = p_element.lock_element();
	const auto* image = dynamic_cast<const litehtml::el_image*>(element.get());
	return image && state->dom->is_element_connected(*image) ? String::utf8(image->get_current_src().c_str()) : String();
}

namespace {
	std::optional<int64_t> explicit_tab_index(const litehtml::element& p_element) {
		const String value = String::utf8(p_element.get_attr("tabindex", "")).strip_edges();
		return value.is_valid_int() ? std::optional<int64_t>(value.to_int()) : std::nullopt;
	}

	bool can_focus_node(const litehtml::element& p_node) {
		const auto tag = p_node.tag();
		if (p_node.get_attr("hidden")) return false;
		if (dockit::forms::is_listed(p_node) && dockit::forms::is_disabled(p_node)) return false;
		if (tag == litehtml::_textarea_ || tag == litehtml::_select_) return true;
		if (dynamic_cast<const dockit::HTMLDialogNode*>(&p_node)) return p_node.get_attr("open") != nullptr;
		if (tag == litehtml::_input_) {
			const auto* input = dynamic_cast<const HTMLInputNode*>(&p_node);
			return input && input->type() != HTMLInputNode::Type::HIDDEN;
		}
		const auto* video = dynamic_cast<const dockit::HTMLVideoNode*>(&p_node);
		const String editable = String::utf8(p_node.get_attr("contenteditable", "")).strip_edges().to_lower();
		const bool editable_host = p_node.get_attr("contenteditable") &&
			(editable.is_empty() || editable == "true" || editable == "plaintext-only");
		return editable_host || explicit_tab_index(p_node).has_value() || DocumentAdapter::is_command_button(p_node) ||
			dockit::details_activation_target(std::const_pointer_cast<litehtml::element>(p_node.shared_from_this())) ||
			(tag == litehtml::_a_ && p_node.get_attr("href")) || (tag == litehtml::_audio_ && p_node.get_attr("controls")) ||
			(video && video->has_controls());
	}
}

void HTMLLayout::push_modal(const litehtml::element::ptr& p_element) {
	DocumentAdapter::push_modal(*state->dom->get_document(), p_element);
}

litehtml::element::ptr HTMLLayout::top_modal_node() const {
	const auto& document = state->dom->get_document();
	return document && !document->top_layer().empty() ? document->top_layer().back().lock() : nullptr;
}

Ref<HTMLDialogElement> HTMLLayout::top_modal() const {
	return state->dom->make_element(top_modal_node());
}

Ref<HTMLDialogElement> HTMLLayout::active_dialog() const {
	if (const auto modal = top_modal_node()) return state->dom->make_element(modal);
	return state->dom->make_element(state->dom->last_open_dialog());
}

bool HTMLLayout::is_outside_dialog(const HTMLDialogElement& p_dialog, const Vector2& p_point) const {
	const auto node = p_dialog.lock_element();
	if (!state->has_layout() || !node) return false;
	const auto* boxes = state->geometry.find_element(node.get());
	if (!boxes || boxes->empty()) return false;
	bool visible = false;
	for (const auto* box : *boxes) {
		if (!box->item->is_visible() || !box->inverse) continue;
		visible = true;
		// CSS pointer-events can suppress a hit on otherwise visible dialog content.
		if (box->contains_visible_point(p_point)) return false;
	}
	if (!visible) return false;
	for (auto hit = hit_test_element(p_point); hit; hit = hit->parent()) {
		if (hit == node) return false;
	}
	return true;
}

Vector2i HTMLLayout::modal_text_range() const {
	const auto modal = top_modal_node();
	return modal && state->has_layout() ? state->container->get_paint_recorder().text_range_in(*modal) : Vector2i();
}

bool HTMLLayout::allows_modal_interaction(const litehtml::element& p_element) const {
	const auto modal = top_modal_node();
	if (!modal) return true;
	if (p_element.tag() == litehtml::_id("-tag-backdrop")) return false;
	for (auto node = p_element.shared_from_this(); node; node = node->parent()) {
		if (node == modal) return true;
		if (const auto* dialog = dynamic_cast<const dockit::HTMLDialogNode*>(node.get());
			dialog && dialog->has_pseudo_class(litehtml::_id("modal"))) return false;
	}
	return false;
}

bool HTMLLayout::allows_modal_interaction(const Ref<HTMLElement>& p_element) const {
	if (p_element.is_null() || p_element->get_owner() != this) return false;
	const auto node = p_element->lock_element();
	return node && allows_modal_interaction(*node);
}

Ref<HTMLElement> HTMLLayout::dialog_focus_target(const Ref<HTMLDialogElement>& p_dialog) const {
	Ref<HTMLElement> first, autofocus;
	HTMLDOM::visit_elements(p_dialog->lock_element(), [&](const auto& p_node) {
		const auto candidate = state->dom->make_element(p_node);
		if (!is_focusable(candidate)) return false;
		if (p_node->get_attr("autofocus")) {
			autofocus = candidate;
			return true;
		}
		if (first.is_null() && candidate != p_dialog && explicit_tab_index(*p_node).value_or(0) >= 0) first = candidate;
		return false;
	});
	if (autofocus.is_valid()) return autofocus;
	if (first.is_valid()) return first;
	return is_focusable(p_dialog) ? Ref<HTMLElement>(p_dialog) : Ref<HTMLElement>();
}

bool HTMLLayout::is_focusable(const Ref<HTMLElement>& p_element) const {
	if (p_element.is_null() || p_element->get_owner() != this) return false;
	const auto element = p_element->element.lock();
	if (!element || !state->has_layout() || !allows_modal_interaction(*element)) return false;
	const bool design_mode_body = state->dom->is_design_mode_enabled() && element->tag() == litehtml::_body_;
	if (!can_focus_node(*element) && !p_element->is_content_editable() && !design_mode_body) return false;
	return design_mode_body || scroll_target_renderer(state->geometry.find_element(element.get())) || auto_content_placeholder(*element);
}

bool HTMLLayout::prepare_content_focus(const Ref<HTMLElement>& p_element) {
	if (p_element.is_null()) {
		state->content_focus.reset();
		request_auto_content_update();
		return true;
	}
	if (!is_focusable(p_element)) return false;
	const auto element = p_element->element.lock();
	const auto previous = std::exchange(state->content_focus, element);
	if (auto_content_placeholder(*element) || (element->css().skips_contents() && element->css().has_auto_content_visibility())) {
		state->pending_update = State::Update::LAYOUT;
		if (layout_viewport(state->viewport_size, state->media_viewport_size, state->viewport_unit_size) != OK ||
			!scroll_target_renderer(state->geometry.find_element(element.get()))) {
			state->content_focus = previous;
			return false;
		}
		if (state->content_update_callback.is_valid()) state->content_update_callback.call();
	}
	return true;
}

bool HTMLLayout::is_element_connected(const Ref<HTMLElement>& p_element) const {
	if (p_element.is_null() || p_element->get_owner() != this) {
		return false;
	}
	const auto element = p_element->element.lock();
	return element && state->dom->is_element_connected(*element);
}

Ref<HTMLElement> HTMLLayout::get_focus_ancestor(const Ref<HTMLElement>& p_element) const {
	if (!p_element.is_valid() || p_element->get_owner() != this) return {};
	for (auto node = p_element->element.lock(); node && allows_modal_interaction(*node); node = node->parent()) {
		if (!can_focus_node(*node)) continue;
		const auto handle = state->dom->make_element(node);
		if (get_element_scroll_bounds(handle)) return handle;
	}
	return {};
}

std::vector<Ref<HTMLElement>> HTMLLayout::get_tab_elements(bool& r_has_focusable_elements) const {
	struct TabElement {
		Ref<HTMLElement> element;
		int64_t index;
	};
	std::vector<TabElement> elements;
	r_has_focusable_elements = false;
	if (state->has_layout() && state->dom->get_document()) {
		HTMLDOM::visit_elements(state->dom->get_document()->root(), [&](const litehtml::element::ptr& p_element) {
			const bool design_mode_body = state->dom->is_design_mode_enabled() && p_element->tag() == litehtml::_body_;
			if (!can_focus_node(*p_element) && !design_mode_body) return false;
			const auto handle = state->dom->make_element(p_element);
			if (is_focusable(handle)) {
				r_has_focusable_elements = true;
				const int64_t index = explicit_tab_index(*p_element).value_or(0);
				if (const auto* radio = dynamic_cast<const HTMLInputNode*>(p_element.get()); radio && radio->type() == HTMLInputNode::Type::RADIO) {
					litehtml::element::ptr tab_stop;
					for (const auto& peer : dockit::forms::radio_group(*radio)) {
						if (!can_focus_node(*peer) || explicit_tab_index(*peer).value_or(0) < 0 ||
							!is_focusable(state->dom->make_element(peer))) continue;
						if (!tab_stop) tab_stop = peer;
						if (static_cast<const HTMLInputNode&>(*peer).is_checked()) {
							tab_stop = peer;
							break;
						}
					}
					if (tab_stop != p_element) return false;
				}
				if (index >= 0 && (!dynamic_cast<const dockit::HTMLDialogNode*>(p_element.get()) ||
						explicit_tab_index(*p_element))) elements.push_back({handle, index});
			}
			return false;
		});
	}
	std::stable_sort(elements.begin(), elements.end(), [](const TabElement& p_left, const TabElement& p_right) {
		if (p_left.index == 0 || p_right.index == 0) return p_left.index > p_right.index;
		return p_left.index < p_right.index;
	});
	std::vector<Ref<HTMLElement>> result;
	result.reserve(elements.size());
	for (auto& entry : elements) result.push_back(std::move(entry.element));
	return result;
}

std::shared_ptr<HTMLTextControlNode> HTMLLayout::text_control(const Ref<HTMLElement>& p_element) const {
	if (p_element.is_null() || p_element->get_owner() != this) return {};
	const auto element = p_element->element.lock();
	if (const auto* input = dynamic_cast<const HTMLInputNode*>(element.get()); input && input->is_button()) return {};
	return std::dynamic_pointer_cast<HTMLTextControlNode>(element);
}

std::optional<dockit::HTMLPlaneTransform> HTMLLayout::input_inverse(const HTMLElement& p_input) const {
	if (!state->has_layout() || p_input.get_owner() != this) return std::nullopt;
	const auto element = p_input.element.lock();
	if (!element || element->css().skips_contents()) return std::nullopt;
	const auto* renderers = state->geometry.find_element(element.get());
	if (!renderers || renderers->empty()) return std::nullopt;
	return renderers->front()->inverse;
}

std::optional<Vector2> HTMLLayout::input_local_point(const HTMLElement& p_input, const Vector2& p_point) const {
	const auto inverse = input_inverse(p_input);
	if (!inverse) return std::nullopt;
	const Vector2 local = inverse->xform(p_point);
	return local.is_finite() ? std::optional<Vector2>(local) : std::nullopt;
}

std::optional<Vector2> HTMLLayout::input_caret_point(const HTMLElement& p_input) const {
	const auto inverse = input_inverse(p_input);
	const auto node = std::dynamic_pointer_cast<HTMLTextControlNode>(p_input.lock_element());
	if (!inverse || !node) return std::nullopt;
	const auto caret = node->editor.get_caret_rect();
	if (!caret.has_area()) return std::nullopt;
	const auto viewport = node->editor.get_viewport();
	if (!viewport.has_area()) return std::nullopt;
	const Vector2 point = Vector2(caret.position.x, caret.get_end().y).clamp(viewport.position, viewport.get_end());
	const Vector2 visual = inverse->inverse().xform(point);
	return visual.is_finite() ? std::optional<Vector2>(visual) : std::nullopt;
}

void HTMLLayout::flush_input_paint() {
	if (state->has_layout() && state->input_paint_pending) record_paint();
}

std::optional<HTMLLayout::ResizeTarget> HTMLLayout::resize_grip_at(const Vector2& p_point) const {
	if (!state->has_layout() || !p_point.is_finite()) return {};
	if (const auto* grip = state->container->get_paint_recorder().resize_grip_at(p_point)) {
		auto& item = *grip->item;
		if (!allows_modal_interaction(*item.src_el())) return {};
		auto box = item.get_placement();
		if (item.css().get_box_sizing() == litehtml::box_sizing_border_box) {
			box += item.get_paddings();
			box += item.get_borders();
		}
		return ResizeTarget{state->dom->make_element(item.src_el()), *grip->inverse,
			Vector2(box.width, box.height), dockit::html_resize_axes(item.css())};
	}
	const auto element = get_element_at(p_point);
	const auto node = text_control(element);
	const auto* textarea = dynamic_cast<const HTMLTextAreaNode*>(node.get());
	if (!textarea) return {};
	const auto axes = textarea->resize_axes();
	const auto inverse = input_inverse(*element.ptr());
	if (!inverse || axes == Vector2i() || !textarea->get_resize_handle().has_point(inverse->xform(p_point))) return {};
	const Vector2 size = textarea->css().get_box_sizing() == litehtml::box_sizing_border_box ?
		textarea->get_border_size() : textarea->get_border_size() - textarea->get_border_extras();
	return ResizeTarget{element, *inverse, size, axes};
}

bool HTMLLayout::resize_element(const Ref<HTMLElement>& p_element, const Vector2& p_size, const Vector2i& p_axes) {
	if (!p_size.is_finite() || !is_element_connected(p_element) || !allows_modal_interaction(p_element)) return false;
	const auto node = p_element->lock_element();
	const auto* textarea = dynamic_cast<const HTMLTextAreaNode*>(node.get());
	const auto enabled = textarea ? textarea->resize_axes() : dockit::html_resize_axes(node->css());
	const Vector2i axes(p_axes.x && enabled.x, p_axes.y && enabled.y);
	if (axes == Vector2i()) return false;

	litehtml::raw_declaration::vector declarations;
	litehtml::raw_rule::vector rules;
	litehtml::css_parser(litehtml::normalize(node->get_attr("style", ""), litehtml::f_componentize))
		.consume_style_block_contents(declarations, rules);
	litehtml::string style;
	for (const auto& declaration : declarations) {
		const auto name = litehtml::lowcase(declaration.name);
		if ((axes.x && name == "width") || (axes.y && name == "height")) continue;
		style += declaration.name + ": " + litehtml::get_repr(declaration.value);
		if (declaration.important) style += " !important";
		style += "; ";
	}
	if (axes.x) style += "width: " + std::to_string(std::max<real_t>(0, p_size.x)) + "px; ";
	if (axes.y) style += "height: " + std::to_string(std::max<real_t>(0, p_size.y)) + "px;";
	return state->dom->edit_attribute(node, "style", style.c_str()) == OK;
}

void HTMLLayout::repaint_input() {
	state->input_paint_pending = true;
	if (state->canvas_renderer.is_valid()) state->canvas_renderer->queue_redraws();
}

void HTMLLayout::update_input(HTMLTextControlNode& p_input) {
	state->dom->flush_changes();
	const bool style_changed = p_input.update_pseudos();
	const bool validity_changed = dockit::forms::update_validity(p_input);
	if (!state->dom->is_element_connected(p_input)) return;
	if (style_changed || (validity_changed && (state->selector_state_dependencies & dockit::css::selector_depends_on_validity))) invalidate_styles();
	else repaint_input();
}

bool HTMLLayout::is_interaction_target_available(const Ref<HTMLElement>& p_element, bool p_after_layout) const {
	if (p_element.is_null() || p_element->get_owner() != this) return false;
	const auto node = p_element->lock_element();
	if (!node || !state->dom->is_element_connected(*node) || dockit::forms::is_disabled(*node)) return false;
	for (auto ancestor = node; ancestor; ancestor = ancestor->parent()) {
		if (ancestor->get_attr("hidden") || ancestor->get_attr("inert")) return false;
	}
	if (!p_after_layout) return true;
	return allows_modal_interaction(*node) && node->css().get_visibility() == litehtml::visibility_visible &&
		(p_element->get_bounding_rect().has_area() ||
			(node->tag() == litehtml::_area_ && !get_activatable_link_uri(p_element).is_empty()));
}

bool HTMLLayout::set_interaction_pseudo(const Ref<HTMLElement>& p_element, const char* p_name, bool p_enabled) {
	if (p_element.is_null() || p_element->get_owner() != this) {
		return false;
	}
	const auto element = p_element->element.lock();
	if (auto* input = dynamic_cast<HTMLTextControlNode*>(element.get()); input && std::string_view(p_name) == "focus") {
		input->set_focus_style(p_enabled);
	}
	return element && element->set_pseudo_class(litehtml::_id(p_name), p_enabled);
}

TypedArray<HTMLElement> HTMLLayout::get_hovered_elements() const {
	TypedArray<HTMLElement> result;
	if (!state) return result;
	const std::vector<std::weak_ptr<litehtml::element>> entries = state->hovered_elements;
	for (const auto& entry : entries) {
		if (const auto element = entry.lock()) {
			Ref<HTMLElement> handle = wrap_hit_element(element);
			if (handle.is_valid()) result.append(handle);
		}
	}
	return result;
}

dockit::HTMLHoverUpdate HTMLLayout::update_hover(const std::optional<Vector2>& p_position,
	const dockit::HTMLHitTestResult& p_hit_test) {
	const auto hit_element = p_position ? (p_hit_test ? *p_hit_test : hit_test_element(*p_position)) : nullptr;
	const auto target = find_html_ancestor(hit_element);
	std::vector<std::weak_ptr<litehtml::element>> next;
	for (auto element = target; element && allows_modal_interaction(*element); element = find_html_ancestor(element->parent())) {
		next.emplace_back(element);
	}
	auto& previous = state->hovered_elements;
	if (std::equal(previous.begin(), previous.end(), next.begin(), next.end(),
			[](const auto& p_left, const auto& p_right) { return p_left.lock() == p_right.lock(); })) {
		return dockit::HTMLHoverUpdate::unchanged;
	}

	std::unordered_set<const litehtml::element*> hovered;
	for (const auto& entry : next) {
		hovered.insert(entry.lock().get());
	}
	for (const auto& entry : previous) {
		if (const auto element = entry.lock(); element && !hovered.erase(element.get())) {
			DocumentAdapter::set_hover(*element, false);
		}
	}
	for (auto element = target; element && allows_modal_interaction(*element); element = find_html_ancestor(element->parent())) {
		if (hovered.count(element.get())) {
			DocumentAdapter::set_hover(*element, true);
		}
	}
	previous = std::move(next);
	if (!(state->selector_state_dependencies & dockit::css::selector_depends_on_hover)) {
		return dockit::HTMLHoverUpdate::state_changed;
	}
	const auto& document = state->dom->get_document();
	if (!state->has_layout() || !document) {
		mark_styles_pending();
		return dockit::HTMLHoverUpdate::layout;
	}
	if (state->has_css_transitions || state->transitions.active() || state->transitions.has_overrides() ||
		state->animations.active()) {
		// Let reflow capture the pre-hover values before recomputing transition targets.
		mark_styles_pending();
		return dockit::HTMLHoverUpdate::layout;
	}
	litehtml::position::vector redraw_boxes;
	bool layout_required = false;
	bool viewport_style_required = false;
	const bool styles_changed = document->root()->find_styles_changes(redraw_boxes, &layout_required,
		&viewport_style_required);
	if (!styles_changed) return dockit::HTMLHoverUpdate::state_changed;
	layout_required |= viewport_style_required;
	if (layout_required) {
		DocumentAdapter::rebuild_render_tree(*document);
		state->pending_update = State::Update::HOVER_LAYOUT;
		state->hover_viewport_style_changed = viewport_style_required;
		state->hover_layout_active = true;
		state->anchors.clear();
		return dockit::HTMLHoverUpdate::layout;
	}
	repaint_input();
	return dockit::HTMLHoverUpdate::paint_only;
}

String HTMLLayout::get_render_error() const {
	return state->error;
}

void HTMLLayout::set_css_diagnostics_enabled(bool p_enabled) {
	if (state->css_diagnostics_enabled == p_enabled) {
		return;
	}
	state->css_diagnostics_enabled = p_enabled;
	if (!state->container) {
		return;
	}
	state->container->set_css_diagnostics_enabled(p_enabled);
	if (p_enabled && state->dom->get_document()) {
		state->pending_update = State::Update::STYLE;
		state->hover_viewport_style_changed = false;
		if (!state->edit_callback.is_valid()) {
			queue_reflow();
		}
	}
	else if (!p_enabled && state->canvas_renderer.is_valid()) {
		state->canvas_renderer->queue_redraws();
	}
}

bool HTMLLayout::is_css_diagnostics_enabled() const {
	return state->css_diagnostics_enabled;
}

TypedArray<Dictionary> HTMLLayout::get_css_diagnostics() const {
	return state->container ? state->container->get_css_diagnostics() : TypedArray<Dictionary>();
}

TypedArray<Dictionary> HTMLLayout::get_css_diagnostics_for(const litehtml::element& p_element) const {
	TypedArray<Dictionary> result;
	if (state->container && state->pending_update != State::Update::STYLE) {
		if (const auto* entries = state->container->get_element_diagnostics(p_element)) {
			for (const auto index : *entries) {
				result.append(state->container->get_css_diagnostic(index).duplicate());
			}
		}
	}
	return result;
}

void HTMLLayout::record_css_diagnostics() {
	if (!state->css_diagnostics_enabled) {
		return;
	}
	HTMLDOM::visit_elements(state->dom->get_document()->root(), [&](const litehtml::element::ptr& p_element) {
		const auto* renderers = state->geometry.find_element(p_element.get());
		if (renderers && state->container->get_element_diagnostics(*p_element)) {
			for (const auto* renderer : *renderers) {
				if (!renderer->item->is_visible()) {
					continue;
				}
				for (const auto& fragment : renderer->fragments) {
					if (fragment.rect.has_area()) {
						state->container->record_css_diagnostic(*renderer, fragment.rect);
					}
				}
			}
		}
		return false;
	});
}

String HTMLLayout::get_css_diagnostic_tooltip(const Vector2& p_position) const {
	if (!state->css_diagnostics_enabled) {
		return {};
	}
	for (auto node = find_html_ancestor(hit_test_element(p_position)); node; node = find_html_ancestor(node->parent())) {
		const auto* entries = state->container->get_element_diagnostics(*node);
		if (!entries) {
			continue;
		}
		String element = String::utf8(node->get_tagName());
		if (const char* id = state->dom->get_element_attribute(*node, "id"); id && *id) {
			element += "#" + String::utf8(id);
		}
		String html = "<div class='element'>" + element.xml_escape() + "</div>";
		for (const auto index : *entries) {
			const Dictionary& entry = state->container->get_css_diagnostic(index);
			const String path = entry["path"];
			const String source = path.is_empty() ? String(entry["source"]) : path;
			const String kind = entry["kind"];
			const String property = entry["property"];
			const String value = entry["value"];
			html += "<div class='diagnostic'><div class='kind'>" + kind.xml_escape() + "</div>" +
				"<div>" + property.xml_escape() + ": " + value.xml_escape() + "</div>" +
				"<div class='source'>" + source.xml_escape() + "</div></div>";
		}
		return html;
	}
	return {};
}

TypedArray<HTMLVideoElement> HTMLLayout::video_elements() const {
	TypedArray<HTMLVideoElement> result;
	if (const auto& document = state->dom->get_document()) {
		HTMLDOM::visit_elements(document->root(), [&](const auto& node) {
			if (node->tag() == litehtml::_video_) result.append(state->dom->make_element(node));
			return false;
		});
	}
	return result;
}

PackedStringArray HTMLLayout::video_paths(const HTMLVideoElement& p_element) const {
	PackedStringArray result;
	const auto element = p_element.get_video_node();
	if (!element || p_element.get_owner() != this || !state->container) return result;
	const String& base_dir = state->container->get_document_base().directory;
	const auto append = [&](const char* p_src) {
		if (!p_src) return;
		const String value = String::utf8(p_src);
		String path = dockit::resolve_html_video_path(value, base_dir);
		if (path.is_empty() && (base_dir == state->imported_video_base_dir || value.begins_with("uid://"))) {
			path = dockit::resolve_html_video_path(state->video_sources.get(value, String()), base_dir);
		}
		if (!path.is_empty() && !result.has(path)) result.append(path);
	};
	if (const char* src = element->get_attr("src")) append(src);
	else for (const auto& child : element->children()) {
		if (child->tag() == litehtml::_source_) append(child->get_attr("src"));
	}
	return result;
}

Ref<Texture2D> HTMLLayout::video_poster(const HTMLVideoElement& p_element) {
	const auto node = p_element.get_video_node();
	return state->container && node ? state->container->video_poster(node->get_attr("poster")) : Ref<Texture2D>();
}

Ref<Texture2D> HTMLLayout::video_control_icon(dockit::HTMLControlIcon p_icon) {
	return state->container ? state->container->get_paint_recorder().get_control_icon(p_icon) : Ref<Texture2D>();
}

void HTMLLayout::video_dimensions_changed() {
	if (state->pending_update == State::Update::NONE) state->pending_update = State::Update::LAYOUT;
}

TypedArray<HTMLAudioElement> HTMLLayout::audio_elements() const {
	TypedArray<HTMLAudioElement> result;
	if (const auto& document = state->dom->get_document()) {
		HTMLDOM::visit_elements(document->root(), [&](const auto& node) {
			if (node->tag() == litehtml::_audio_) result.append(state->dom->make_element(node));
			return false;
		});
	}
	return result;
}

PackedStringArray HTMLLayout::audio_sources(const HTMLAudioElement& p_element) const {
	PackedStringArray result;
	if (p_element.get_owner() != this || !state->container) return result;
	const auto element = p_element.lock_element();
	if (!element) return result;
	const auto append_source = [&](const char* p_src) {
		if (!p_src) return;
		const String path = dockit::resolve_html_audio_path(String::utf8(p_src), state->container->get_document_base().directory);
		if (!path.is_empty()) result.append(path);
	};
	if (const char* src = element->get_attr("src")) append_source(src);
	else {
		for (const auto& child : element->children()) {
			if (child->tag() == litehtml::_source_) append_source(child->get_attr("src"));
		}
	}
	return result;
}

Ref<Resource> HTMLLayout::audio_resource(const String& p_path, bool p_fmod) const {
	const Ref<Resource> stored = state->audio_streams.get(p_path, Variant());
	if (stored.is_valid()) return stored;
	auto* loader = ResourceLoader::get_singleton();
	const StringName type = p_fmod ? "FmodAudioStream" : "AudioStream";
	if (loader->exists(p_path, type)) return loader->load(p_path, type);
	if (!p_path.begins_with("user://") || !FileAccess::file_exists(p_path)) return {};
	const auto format = p_path.get_extension().to_lower();
	if (format != "wav" && format != "ogg" && format != "mp3") return {};
	if (p_fmod) {
		if (!ClassDB::class_has_method(type, "load_from_file")) return {};
		return ClassDB::class_call_static(type, "load_from_file", p_path);
	}
	if (format == "wav") return AudioStreamWAV::load_from_file(p_path);
	if (format == "ogg") return AudioStreamOggVorbis::load_from_file(p_path);
	return AudioStreamMP3::load_from_file(p_path);
}

void HTMLLayout::clear() {
	state->audio_streams.clear();
	state->video_sources.clear();
	state->imported_video_base_dir = String();
	++parse_version;
	state->stylesheet_identities.clear();
	state->extra_stylesheet_id = 0;
	state->animation_handles.clear();
	state->animation_event_tracks.clear();
	disconnect_transition_frames();
	state->transitions = {};
	state->animations = {};
	state->animation_underlying.clear();
	state->animation_time = 0.0;
	cancel_scroll_activity();
	state->nested_snap_memory.clear();
	state->skipped_scroll_offsets.clear();
	state->content_focus.reset();
	state->viewport_snap_memory = {};
	state->canvas_renderer.unref();
	if (state->container) {
		state->container->clear_recording();
	}
	state->geometry.clear();
	state->dom->clear();
	state->hovered_elements.clear();
	state->selector_state_dependencies = 0;
	state->container.reset();
	state->anchors.clear();
	state->viewport_size = Vector2i();
	state->media_viewport_size = Vector2i();
	state->viewport_unit_size = Vector2i();
	state->viewport_scroll_offset = Vector2i();
	state->viewport_gutters = Vector2();
	state->viewport_overflow = {litehtml::overflow_auto, litehtml::overflow_auto};
	state->content_size = Vector2i();
	state->error = String();
	state->parse_error = OK;
	state->pending_update = State::Update::LAYOUT;
	state->hover_viewport_style_changed = false;
	state->hover_layout_active = false;
	state->has_css_transitions = false;
	state->implicit_empty_document = false;
	state->reflow_queued = false;
	state->embedded_block = false;
	state->master_styles.clear();
	state->user_styles.clear();
}
