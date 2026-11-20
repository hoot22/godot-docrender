#pragma once

#include "html_element.h"

class HTMLHeadingElement : public HTMLElement {
	GDCLASS(HTMLHeadingElement, HTMLElement)

protected:
	static void _bind_methods();

public:
	int get_level() const; // 按标签返回 1 到 6 的标题级别，失效时返回 0
};
