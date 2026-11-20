// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#include <litehtml/html.h>
#include <litehtml/borders.h>

litehtml::string litehtml::css_border::to_string() const
{
	return width.to_string() + "/" + index_value(style, border_style_strings) + "/" + color.to_string();
}
