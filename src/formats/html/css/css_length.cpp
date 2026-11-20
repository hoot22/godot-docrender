// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include "css_length.h"
#include "css_math.h"
#include "css_syntax_util.h"
#include "css_token_compat.h"
#include <cmath>

namespace litehtml
{

using dockit::css::detail::index_value;
using dockit::css::detail::is_one_of;
using dockit::css::detail::lowcase;
using dockit::css::detail::value_index;

void css_length::set_expression(std::shared_ptr<const css_math_expression> p_expression)
{
	const auto unit = p_expression->is_number ? css_units_none :
		(p_expression->has_percentage ? css_units_percentage : css_units_px);
	set_value(0, unit);
	m_expression = std::move(p_expression);
}

pixel_t css_length::evaluate_expression(pixel_t basis) const
{
	return static_cast<pixel_t>(m_expression->evaluate(basis));
}

void css_length::set_intrinsic_size(css_intrinsic_size kind)
{
	if (!is_one_of(kind, css_intrinsic_size::fit_content_limit, css_intrinsic_size::stretch_limit)) predef(0);
	m_is_predefined = true;
	m_intrinsic_size = kind;
}

css_length css_length::intrinsic_size_limit() const
{
	auto limit = *this;
	limit.m_is_predefined = false;
	limit.m_intrinsic_size = css_intrinsic_size::none;
	return limit;
}

bool css_length::from_token(const css_token& token, int options, const string& keywords)
{
	if (token.type == CV_FUNCTION && (options & f_math))
	{
		if (auto expression = parse_css_math(token, options))
		{
			set_expression(std::move(expression));
			return true;
		}
		return false;
	}
	m_expression.reset();
	m_intrinsic_size = css_intrinsic_size::none;
	if (is_one_of(token.type, NUMBER, DIMENSION, PERCENTAGE) &&
		(!std::isfinite(token.n.number) || ((options & f_positive) && token.n.number < 0)))
		return false;

	if (token.type == IDENT)
	{
		int idx = value_index(lowcase(token.name), keywords);
		if (idx == -1) return false;
		m_predef = idx;
		m_is_predefined = true;
		return true;
	}
	else if (token.type == DIMENSION)
	{
		if (!(options & f_length)) return false;

		int idx = value_index(lowcase(token.unit), css_units_strings);
		// note: 1none and 1\% are invalid
		if (idx == -1 || idx == css_units_none || idx == css_units_percentage)
			return false;
		
		m_value = token.n.number;
		m_units = (css_units)idx;
		m_is_predefined = false;
		return true;
	}
	else if (token.type == PERCENTAGE)
	{
		if (!(options & f_percentage)) return false;

		m_value = token.n.number;
		m_units = css_units_percentage;
		m_is_predefined = false;
		return true;
	}
	else if (token.type == NUMBER)
	{
		// if token is a nonzero number and neither f_number nor f_integer is specified in the options
		if (!(options & (f_number | f_integer)) && token.n.number != 0)
			return false;
		// if token is a zero number and neither of f_number, f_integer or f_length are specified in the options
		if (!(options & (f_number | f_integer | f_length)) && token.n.number == 0)
			return false;
		if ((options & f_integer) && token.n.number_type != css_number_integer)
			return false;
		
		m_value = token.n.number;
		m_units = css_units_none;
		m_is_predefined = false;
		return true;
	}
	return false;
}

string css_length::to_string() const
{
	switch (m_intrinsic_size)
	{
	case css_intrinsic_size::min_content: return "min-content";
	case css_intrinsic_size::max_content: return "max-content";
	case css_intrinsic_size::fit_content: return "fit-content";
	case css_intrinsic_size::fit_content_limit: return "fit-content(" + intrinsic_size_limit().to_string() + ")";
	case css_intrinsic_size::stretch: return "stretch";
	case css_intrinsic_size::stretch_limit: return "stretch(" + intrinsic_size_limit().to_string() + ")";
	default: break;
	}
	if (m_expression) return m_expression->to_string();
	if(m_is_predefined)
	{
		return "def(" + std::to_string(m_predef) + ")";
	}
	return std::to_string(m_value) + "{" + index_value(m_units, css_units_strings) + "}";
}

css_length css_length::predef_value(int val)
{
	css_length len;
	len.predef(val);
	return len;
}

} // namespace litehtml
