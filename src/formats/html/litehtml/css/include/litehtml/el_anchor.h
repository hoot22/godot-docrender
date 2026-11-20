#ifndef LH_EL_ANCHOR_H
#define LH_EL_ANCHOR_H

#include "html_tag.h"

namespace litehtml
{
	class el_anchor : public html_tag
	{
	public:
		explicit el_anchor(const std::shared_ptr<litehtml::document>& doc);

		void	on_click() override;
	};
}

#endif  // LH_EL_ANCHOR_H
