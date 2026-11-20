#include "el_anchor.h"
#include "document.h"
#include "document_container.h"

litehtml::el_anchor::el_anchor(const std::shared_ptr<litehtml::document>& doc) : html_tag(doc)
{
}

void litehtml::el_anchor::on_click()
{
	const char* href = get_attr("href");

	if(href)
	{
		get_document()->container()->on_anchor_click(href, shared_from_this());
	}
}

