#pragma once

#include "css_computed_style.h"
#include "css_font_description.h"
#include "css_property_value.h"
#include "css_units.h"

namespace dockit::css {

	// The host supplies the display scale and resource queries for the current document.
	class StyleHost {
	public:
		virtual ~StyleHost() = default;
		virtual pixel_t default_font_size() const = 0;
		virtual pixel_t default_fixed_font_size() const { return default_font_size(); }
		virtual font_metrics initial_font_metrics() const = 0;
		virtual pixel_t points_to_pixels(float points) const = 0;
		virtual css_container_units container_units() const = 0;
		virtual uint_ptr get_font(const font_description& description, font_metrics* metrics) const = 0;
		virtual void compute_image(image& value, const string& base, const font_metrics& metrics,
			const css_image_resolution& resolution, css_image_orientation orientation, color_scheme scheme) const = 0;
	};

	struct ComputeContext {
		const void* declarations;
		const property_value& (*read_declaration)(const void*, string_id);
		const ComputedStyle* parent;
		const StyleHost& host;
		const media_features& viewport;
		const font_metrics* root_font;
		bool is_root;
		bool is_replaced;
		const ComputedStyle* box_parent = nullptr;
		bool suppress_contents_display = false;
		bool allow_content_replacement = false;
		bool replacement_ignores_size_hints = false;

		pixel_t to_pixels(const css_length& length, const font_metrics& metrics,
			pixel_t basis, css_length_context overrides = {}) const;
		void cvt_units(css_length& length, const font_metrics& metrics, pixel_t basis) const;

	private:
		UnitContext unit_context(const font_metrics& metrics, css_length_context overrides = {}) const;
	};

}
