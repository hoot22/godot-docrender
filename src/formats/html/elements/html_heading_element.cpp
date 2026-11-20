#include "html_heading_element.h"

using namespace godot;

void HTMLHeadingElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_level"), &HTMLHeadingElement::get_level);
}

int HTMLHeadingElement::get_level() const {
	const String tag = get_tag_name();
	return tag.length() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6' ? int(tag[1] - '0') : 0;
}
