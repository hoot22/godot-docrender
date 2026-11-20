// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LH_CSS_LENGTH_H
#define LH_CSS_LENGTH_H

#include "css_types.h"
#include "css_token_compat.h"

#include <memory>

namespace litehtml
{
	struct css_math_expression;

	struct css_container_units
	{
		const void* subject = nullptr;
		std::optional<pixel_t> (*read_axis)(const void*, bool block_axis) = nullptr;
	};

	struct css_length_context
	{
		const font_metrics* root_font = nullptr;
		const media_features* viewport = nullptr;
		css_container_units container_units = {};
	};

	// from_token options (flags)
	enum {
		f_length     = 1, // <length> (includes unitless zero)
		f_percentage = 2, // <percentage>
		f_length_percentage = f_length | f_percentage, // <length-percentage>
		f_number     = 4, // <number>
		f_integer    = 8, // <integer>
		f_math       = 32, // CSS length math functions
		f_positive   = 16 // ⩾0 (non-negative)
	};

	// <length> | <percentage> | <number> | auto | none | normal
	// https://developer.mozilla.org/en-US/docs/Web/CSS/length
	enum class css_intrinsic_size : uint8_t { none, min_content, max_content, fit_content, fit_content_limit, stretch, stretch_limit };

	class css_length
	{
		std::shared_ptr<const css_math_expression> m_expression;
		union
		{
			float	m_value;
			int		m_predef;
		};
		css_units	m_units;
		bool		m_is_predefined;
		css_intrinsic_size m_intrinsic_size = css_intrinsic_size::none;
	public:
		css_length();
		css_length(float val, css_units units = css_units_px);
		css_length&	operator=(float val);

		bool		is_predefined() const;
		void		predef(int val);
		int			predef() const;
		static css_length predef_value(int val = 0);
		void		set_value(float val, css_units units);
		float		val() const;
		css_units	units() const;
		const auto& expression() const { return m_expression; }
		void set_expression(std::shared_ptr<const css_math_expression> p_expression);
		css_intrinsic_size intrinsic_size() const { return m_intrinsic_size; }
		void set_intrinsic_size(css_intrinsic_size kind);
		css_length intrinsic_size_limit() const;
		pixel_t evaluate_expression(pixel_t basis) const;
		pixel_t		calc_percent(pixel_t width) const;
		bool		from_token(const css_token& token, int options, const string& predefined_keywords = "");
		string		to_string() const;
	};

	using length_vector = vector<css_length>;

	// css_length inlines

	inline css_length::css_length()
	{
		m_value			= 0;
		m_predef		= 0;
		m_units			= css_units_none;
		m_is_predefined	= false;
	}

	inline css_length::css_length(float val, css_units units)
	{
		m_value = val;
		m_units = units;
		m_is_predefined = false;
	}

	inline css_length&	css_length::operator=(float val)
	{
		m_expression.reset();
		m_intrinsic_size = css_intrinsic_size::none;
		m_value = val;
		m_units = css_units_px;
		m_is_predefined = false;
		return *this;
	}

	inline bool css_length::is_predefined() const
	{ 
		return m_is_predefined;					
	}

	inline void css_length::predef(int val)		
	{ 
		m_expression.reset();
		m_intrinsic_size = css_intrinsic_size::none;
		m_predef		= val; 
		m_is_predefined = true;	
	}

	inline int css_length::predef() const
	{ 
		if(m_is_predefined && m_intrinsic_size == css_intrinsic_size::none)
		{
			return m_predef; 
		}
		return 0;
	}

	inline void css_length::set_value(float val, css_units units)
	{ 
		m_expression.reset();
		m_intrinsic_size = css_intrinsic_size::none;
		m_value			= val; 
		m_is_predefined = false;	
		m_units			= units;
	}

	inline float css_length::val() const
	{
		if(!m_is_predefined)
		{
			return m_value;
		}
		return 0;
	}

	inline css_units css_length::units() const
	{
		return m_units;
	}

	inline pixel_t css_length::calc_percent(pixel_t width) const
	{
		if (m_intrinsic_size != css_intrinsic_size::none) return 0;
		if (m_expression) return evaluate_expression(width);
		if(!is_predefined())
		{
			if(units() == css_units_percentage)
			{
				return (pixel_t) (width * m_value / 100.0);
			} else
			{
				return (pixel_t) val();
			}
		}
		return 0;
	}
}

#endif  // LH_CSS_LENGTH_H
