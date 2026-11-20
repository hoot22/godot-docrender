#pragma once

#include "css_cascade_order.h"

#include <map>

#include "css_background.h"
#include "css_clip_path.h"
#include "css_motion_path.h"
#include "formats/html/css/css_border_image.h"
#include "css_length.h"
#include "css_position.h"
#include "css_token_compat.h"
#include "css_gradient.h"
#include "css_string_id.h"
#include "formats/html/css/css_transform.h"
#include "css_transition.h"
#include "css_animation.h"
#include "css_grid_values.h"
#include "formats/html/css/css_shadows.h"
#include "formats/html/css/css_values.h"
#include "css_web_color.h"

namespace litehtml {
	struct invalid {}; // indicates "not found" condition in style::get_property
	struct inherit {}; // "inherit" was specified as the value of this property

	struct initial_value {};
	struct revert_value {};
	struct revert_layer_value {};

	// Layers use their normal cascade order. Special ranks sit outside the named layers.
	using dockit::css::css_default_layer;
	using dockit::css::css_unlayered;
	using dockit::css::css_inline_layer;

	struct property_value : variant<
		invalid,
		initial_value,
		revert_value,
		revert_layer_value,
		inherit,
		int,
		int_vector,
		css_length,
		css_aspect_ratio,
		css_contain_intrinsic_size,
		css_clip_margin,
		css_clip_path_ref,
		css_offset_path,
		css_offset_position,
		css_offset_rotation,
		css_scroll_snap_type,
		css_scroll_snap_align,
		object_fit,
		css_image_position,
		vector<css_axis_position>,
		css_image_resolution,
		std::optional<web_color>,
		std::optional<css_scrollbar_colors>,
		css_border_image_source,
		css_border_image_slice,
		css_border_image_offsets,
		css_border_image_repeat,
		css_image_orientation,
		length_vector,
		float,
		web_color,
		css_image_ptr,
		vector<image>,
		vector<background_repeat>,
		string,
		string_vector,
		size_vector,
		grid_line,
		css_alignment,
		css_transform_list,
		css_transform_origin,
		css_individual_transform,
		css_transition_times,
		css_transition_easings,
		css_animation_names,
		css_filter_list,
		grid_track_template,
		grid_area_template,
		grid_track_list,
		grid_auto_flow,
		std::vector<box_shadow>,
		css_font_families,
		css_font_style,
		css_font_features,
		css_font_alternates,
		css_font_variations,
		css_font_size_adjust,
		css_generated_content,
		css_list_marker_type,
		css_quotes,
		css_counter_changes,
		css_text_indent,
		css_hyphenation_chars,
		css_token_vector
	>
	{
		bool m_important = false;
		dockit::css::CascadePriority priority;
		string_id declaration_property = empty_id;

		struct deferred_declaration
		{
			string_id name;
			css_token_vector tokens;
			string baseurl;
			css_source source;
		};
		std::shared_ptr<const deferred_declaration> deferred;

		struct declaration_origin
		{
			string_id name;
			string value;
			css_source source;
		};
		std::shared_ptr<const declaration_origin> origin;

		property_value() {}
		template<class T> property_value(const T& val, bool important)
			: base(val), m_important(important) {}
	};

	using props_map = std::map<string_id, property_value>;
} // namespace litehtml
