#ifndef LH_TYPES_H
#define LH_TYPES_H

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <variant>
#include <optional>
#include <algorithm>
#include "formats/html/css/css_types.h"

namespace litehtml
{
	class document;
	class element;
	using elements_list = std::list<std::shared_ptr<element>>;

	struct rendered_width
	{
		pixel_t natural_width = 0; // The width required to render the element without additional word wrapping.
		pixel_t min_width	  = 0; // The minimum width the element can be rendered

		void merge(const rendered_width& other)
		{
			natural_width = std::max(natural_width, other.natural_width);
			min_width	  = std::max(min_width, other.min_width);
		}

		void reset()
		{
			natural_width = 0;
			min_width	  = 0;
		}
	};

	struct scroll_values
	{
		pixel_t	dx;
		pixel_t dy;
		position scroll_box;

		scroll_values()
		{
			dx = 0;
			dy = 0;
		}
	};

	struct font_item
	{
		uint_ptr		font;
		font_metrics	metrics;
	};

	using fonts_map = std::map<string, font_item>;

	enum draw_flag
	{
		draw_root,
		draw_block,
		draw_floats,
		draw_inlines,
		draw_positioned,
	};

	struct intrinsic_inline_sizes
	{
		std::optional<pixel_t> preferred;
		std::optional<pixel_t> minimum;
		std::optional<pixel_t> maximum;
		std::optional<rendered_width> content;
	};

	struct containing_block_context
	{
		enum cbc_value_type
		{
			cbc_value_type_absolute,	// width/height of containing block is defined as absolute value
			cbc_value_type_percentage,	// width/height of containing block is defined as percentage
			cbc_value_type_auto,		// width/height of containing block is defined as auto
			cbc_value_type_none,		// min/max width/height of containing block is defined as none
		};

		enum cbc_size_mode
		{
			size_mode_normal = 0x00,
			size_mode_exact_width = 0x01,
			size_mode_exact_height = 0x02,
			size_mode_grid_assigned_width = 0x08,
			size_mode_grid_stretch_height = 0x10,
			size_mode_grid_measure_height = 0x20,
			size_mode_grid_measure_width = 0x40,
			size_mode_grid_measure_baseline = 0x80,
			size_mode_intrinsic_inline_probe = 0x100,
			size_mode_content = 0x04,
		};

		struct typed_pixel
		{
			pixel_t			value;
			cbc_value_type	type;

			typed_pixel(const typed_pixel& v) = default;

			typed_pixel(pixel_t val, cbc_value_type tp)
			{
				value = val;
				type = tp;
			}

			operator pixel_t() const
			{
				return value;
			}

			typed_pixel& operator=(pixel_t val)
			{
				value = val;
				return *this;
			}

			typed_pixel& operator=(const typed_pixel& v) = default;
		};

		typed_pixel width;						// width of the containing block
		typed_pixel render_width;
		typed_pixel min_width;
		typed_pixel max_width;

		typed_pixel height;						// height of the containing block
		typed_pixel min_height;
		typed_pixel max_height;

		enum class content_baseline_edge { none, start, end };
		content_baseline_edge grid_content_edge = content_baseline_edge::none;
		pixel_t grid_baseline_padding = 0;
		bool grid_intrinsic_width = false;
		bool measuring_inline_size = false;
		bool intrinsic_inline_basis = false;
		bool ratio_width = false;
		bool ratio_height = false;
		intrinsic_inline_sizes intrinsic_inline;
		std::optional<pixel_t> positioned_height;
		// Used auto height for layout inside a size-contained box. It remains indefinite for percentages.
		std::optional<pixel_t> contained_auto_block_size;

		int context_idx;
		uint32_t size_mode;

		containing_block_context() :
				width(0, cbc_value_type_auto),
				render_width(0, cbc_value_type_auto),
				min_width(0, cbc_value_type_none),
				max_width(0, cbc_value_type_none),
				height(0, cbc_value_type_auto),
				min_height(0, cbc_value_type_none),
				max_height(0, cbc_value_type_none),
				context_idx(0),
				size_mode(size_mode_normal)
		{}

		containing_block_context new_width(pixel_t w, uint32_t _size_mode = size_mode_normal) const
		{
			containing_block_context ret = *this;
			ret.render_width = w - (ret.width - ret.render_width);
			ret.width = w;
			ret.intrinsic_inline = {};
			ret.positioned_height.reset();
			ret.grid_content_edge = content_baseline_edge::none;
			ret.grid_baseline_padding = 0;
			ret.size_mode = _size_mode;
			return ret;
		}

		containing_block_context new_width_height(pixel_t w, pixel_t h, uint32_t _size_mode = size_mode_normal) const
		{
			containing_block_context ret = *this;
			ret.render_width = w - (ret.width - ret.render_width);
			ret.width = w;
			ret.height = h;
			ret.intrinsic_inline = {};
			ret.positioned_height.reset();
			ret.grid_content_edge = content_baseline_edge::none;
			ret.grid_baseline_padding = 0;
			ret.size_mode = _size_mode;
			return ret;
		}
	};

	class render_item;

	struct floated_box
	{
		position		                pos;
		element_float	                float_side;
		element_clear	                clear_floats;
		std::shared_ptr<render_item>	el;
		int								context;
		pixel_t							min_width;

		floated_box() = default;
		floated_box(const floated_box& val) = default;
		floated_box& operator=(const floated_box& val) = default;
		floated_box(floated_box&& val) noexcept = default;
		floated_box& operator=(floated_box&& val) noexcept = default;
	};

	struct pixel_pixel_cache
	{
		pixel_t		hash;
		pixel_t		val;
		bool	is_valid;
		bool	is_default;

		pixel_pixel_cache()
		{
			hash		= 0;
			val			= 0;
			is_valid	= false;
			is_default	= false;
		}
		void invalidate()
		{
			is_valid	= false;
			is_default	= false;
		}
		void set_value(pixel_t vHash, pixel_t vVal)
		{
			hash		= vHash;
			val			= vVal;
			is_valid	= true;
		}
	};

	enum select_result
	{
		select_no_match				= 0x00,
		select_match				= 0x01,
		select_match_pseudo_class	= 0x02,
		select_match_with_before	= 0x10,
		select_match_with_after		= 0x20,
		select_match_with_marker	= 0x40,
		select_match_with_placeholder = 0x80,
		select_match_with_selection = 0x100,
		select_match_with_backdrop = 0x200,
		select_match_with_generated = select_match_with_before | select_match_with_after | select_match_with_marker,
		select_match_with_pseudo_element = select_match_with_generated | select_match_with_placeholder | select_match_with_selection | select_match_with_backdrop,
	};

	template<class T>
	class def_value
	{
		T		m_val;
		bool	m_is_default;
	public:
		def_value(T def_val)
		{
			m_is_default	= true;
			m_val			= def_val;
		}
		def_value(const def_value<T>& val)
		{
			m_is_default	= val.m_is_default;
			m_val			= val.m_val;
		}
		void reset(T def_val)
		{
			m_is_default	= true;
			m_val			= def_val;
		}
		bool is_default() const
		{
			return m_is_default;
		}
		T operator=(T new_val)
		{
			m_val			= new_val;
			m_is_default	= false;
			return m_val;
		}
		def_value<T>& operator=(const def_value<T>& val)
		{
			m_is_default	= val.m_is_default;
			m_val			= val.m_val;
			return *this;
		}
		operator T() const
		{
			return m_val;
		}
	};

	class baseline
	{
	public:
		enum _baseline_type
		{
			baseline_type_none,
			baseline_type_top,
			baseline_type_bottom,
		};

	public:
		baseline() : m_value(0), m_type(baseline_type_none) {}
		baseline(pixel_t _value, _baseline_type _type) : m_value(_value), m_type(_type) {}

		pixel_t value() const			{ return m_value; 	}
		void value(pixel_t _value) 		{ m_value = _value; }
		_baseline_type type() const		{ return m_type; 	}
		void type(_baseline_type _type)	{ m_type = _type; 	}

		operator pixel_t() const	{ return m_value; 	}
		baseline& operator=(pixel_t _value) { m_value = _value; return *this; }

		void set(pixel_t _value, _baseline_type _type)	{ m_value = _value; m_type =_type; }
		/**
		 * Get baseline offset from top of element with specified height
		 * @param height - element height
		 * @return baseline offset
		 */
		pixel_t get_offset_from_top(pixel_t height) const
		{
			if(m_type == baseline_type_top) return m_value;
			return height - m_value;
		}
		/**
		 * Get baseline offset from bottom of element with specified height
		 * @param height - element height
		 * @return baseline offset
		 */
		pixel_t get_offset_from_bottom(pixel_t height) const
		{
			if(m_type == baseline_type_bottom) return m_value;
			return height - m_value;
		}
		/**
		 * Calculate baseline by top and bottom positions of element aligned by baseline == 0
		 * @param top - top of the aligned element
		 * @param bottom - bottom of the aligned element
		 */
		void calc(pixel_t top, pixel_t bottom)
		{
			if(m_type == baseline_type_top)
				m_value = -top;
			else if(m_type == baseline_type_bottom)
				m_value = bottom;
		}
	private:
		pixel_t m_value;
		_baseline_type m_type;
	};

	enum render_type
	{
		render_all,
		render_no_fixed,
		render_fixed_only,
	};

	const char* const split_delims_spaces = " \t\r\n\f\v";

	// List of the Void Elements (can't have any contents)
	const char* const void_elements = "area;base;br;col;command;embed;hr;img;input;keygen;link;meta;param;source;track;wbr";


}

#endif  // LH_TYPES_H
