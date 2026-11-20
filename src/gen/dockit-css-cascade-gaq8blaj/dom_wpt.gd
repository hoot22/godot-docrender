# Copyright © web-platform-tests contributors
# Adapted from Web Platform Tests. See LICENSE.md in this directory.
extends SceneTree

var _failures: int


func _initialize() -> void:
	_test_fragment_insertion()
	_test_fragment_queries()
	_test_clone()
	_test_adopt()
	_test_template_instantiation()
	_test_template_inner_html()
	_test_nested_template_adoption()
	_test_form_dom_associations()
	_test_selector_snapshot()
	_test_relational_selectors()
	if _failures == 0:
		print("Dockit DOM WPT ports passed.")
	quit(int(_failures != 0))


func _check(condition: bool, message: String) -> bool:
	if not condition:
		_failures += 1
		push_error("DOM WPT port: " + message)
	return condition


# Port the fragment addition and removal cases to element children and check the resulting tree.
# https://github.com/web-platform-tests/wpt/blob/master/dom/nodes/MutationObserver-childList.html#L157-L173
# https://github.com/web-platform-tests/wpt/blob/master/dom/nodes/MutationObserver-childList.html#L210-L226
func _test_fragment_insertion() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	if not _check(layout.parse('<div id="target"><span id="reference"></span></div>') == OK,
			"parse fragment fixture"):
		return
	var target: HTMLElement = layout.get_element_by_id("target")
	var reference: HTMLElement = layout.get_element_by_id("reference")
	var fragment: HTMLDocumentFragment = layout.create_document_fragment()
	var first: HTMLElement = layout.create_element("b")
	var second: HTMLElement = layout.create_element("i")
	if not _check(target != null and reference != null and fragment != null and first != null
			and second != null, "create fragment fixture handles"):
		return
	_check(fragment.append_child(first) == OK, "put the first child in the fragment")
	_check(fragment.append_child(second) == OK, "put the second child in the fragment")
	_check(first.get_parent() == null and not first.is_connected(), "fragment child is outside the document")
	# Adapt insertBefore's wrong-parent reference case to batch insertion.
	# https://github.com/web-platform-tests/wpt/blob/master/dom/nodes/Node-insertBefore.html#L49-L56
	_check(target.insert_fragment_before(fragment, first) == ERR_INVALID_PARAMETER, "reject a foreign reference")
	_check(fragment.get_children() == [first, second] and target.get_children() == [reference],
			"failed insertion leaves both child lists intact")
	_check(target.insert_fragment_before(fragment, reference) == OK, "insert the fragment before a child")
	_check(target.get_children() == [first, second, reference], "insert the same elements in order before the reference")
	_check(fragment.get_children().is_empty(), "inserting before a reference empties the fragment")
	_check(first.get_parent() == target and second.is_connected(), "fragment children now belong to the document")
	_check(fragment.append_child(first) == OK, "move the first child back into the fragment")
	_check(fragment.append_child(second) == OK, "move the second child back into the fragment")
	_check(target.append_fragment(fragment) == OK, "append the reused fragment")
	_check(target.get_children() == [reference, first, second], "append the same elements after the existing child")
	_check(fragment.get_children().is_empty(), "appending empties the fragment")


func _test_fragment_queries() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = ('<!doctype html><main id="target"><div id="Row:1" class="row">'
			+ '<span id="deep" class="row"></span></div>'
			+ '<div id="Row:1" class="row last" style="display:none"></div></main>')
	if not _check(layout.parse(html) == OK, "parse fragment query fixture"):
		return
	var target: HTMLElement = layout.get_element_by_id("target")
	var fragment: HTMLDocumentFragment = layout.create_document_fragment()
	if not _check(target != null and fragment != null and target.get_children().size() == 2,
			"find and create fragment query roots"):
		return
	var direct: Array[HTMLElement] = target.get_children()
	var first: HTMLElement = direct[0]
	var last: HTMLElement = direct[1]
	var deep: HTMLElement = layout.get_element_by_id("deep")
	if not _check(deep != null and fragment.append_child(first) == OK and fragment.append_child(last) == OK,
			"move query fixture nodes into the fragment"):
		return
	var snapshot: Array[HTMLElement] = fragment.query_selector_all(".last, .row")
	_check(snapshot == [first, deep, last] and fragment.query_selector(".last, .row") == first,
			"selector lists use tree order and deduplicate matches")
	_check(fragment.get_element_by_id("Row:1") == first and fragment.get_element_by_id("deep") == deep
			and fragment.get_element_by_id("row:1") == null and fragment.get_element_by_id("") == null,
			"ID lookup uses the raw case-sensitive value and returns the first duplicate")
	_check(fragment.query_selector_all(":scope > .row") == direct and fragment.query_selector(":scope") == null
			and fragment.query_selector_all(":scope").is_empty(), "scope anchors the fragment without exposing its root")
	_check(fragment.query_selector_all("* > .row") == [deep] and fragment.query_selector_all(":not(.absent) > .row") == [deep],
			"generic ancestor selectors only match real elements")
	_check(fragment.query_selector_all(":is(:scope, .absent) > .row") == direct
			and fragment.query_selector_all(":scope:has(> .row) > .row") == direct,
			"the virtual root supports logical and relational scope selectors")
	_check(fragment.query_selector(":scope > .row + .row") == last
			and fragment.query_selector(":scope > .row:nth-child(2)") == last,
			"top-level siblings and structural selectors use the fragment's child list")
	_check(layout.get_element_by_id("Row:1") == null and fragment.query_selector("main .row") == null,
			"querying the fragment stays inside its own tree")
	_check(target.append_fragment(fragment) == OK and fragment.query_selector_all(".row").is_empty()
			and fragment.get_element_by_id("Row:1") == null and layout.get_element_by_id("Row:1") == first,
			"insertion updates both roots without changing the returned element handle")
	_check(snapshot == [first, deep, last], "previous query results remain snapshots after insertion")
	_check(layout.parse('<div id="Row:1" class="row"></div>') == OK
			and fragment.query_selector(".row") == null and fragment.query_selector_all(".row").is_empty()
			and fragment.get_element_by_id("Row:1") == null, "invalidated fragments never query the current document")


# Port current-value and checkedness retention for text inputs and checkboxes.
# https://github.com/web-platform-tests/wpt/blob/master/html/semantics/forms/the-input-element/clone.html#L29-L83
func _test_clone() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = '<form id="original"><input id="text" value="Default">' \
			+ '<input id="on" type="checkbox"><input id="off" type="checkbox" checked></form>'
	if not _check(layout.parse(html) == OK, "parse clone fixture"):
		return
	var original: HTMLElement = layout.get_element_by_id("original")
	var original_text: HTMLInputElement = layout.get_element_by_id("text") as HTMLInputElement
	var original_on: HTMLInputElement = layout.get_element_by_id("on") as HTMLInputElement
	var original_off: HTMLInputElement = layout.get_element_by_id("off") as HTMLInputElement
	if not _check(original != null and original_text != null and original_on != null and original_off != null,
			"find original form controls"):
		return
	original_text.value = "Edited"
	original_on.checked = true
	original_off.checked = false
	var shallow: HTMLElement = original.clone_node()
	var deep: HTMLElement = original.clone_node(true)
	if not _check(shallow != null and deep != null, "create both form clones"):
		return
	_check(shallow != original and shallow.get_attribute("id") == "original", "shallow clone copies attributes")
	_check(shallow.get_children().is_empty(), "shallow clone has no descendants")
	_check(deep != original and not deep.is_connected(), "deep clone starts detached with a new handle")
	var body: HTMLElement = layout.query_selector("body")
	if not _check(body != null and body.append_child(deep) == OK, "insert the deep clone"):
		return
	var cloned_text: HTMLInputElement = deep.query_selector("#text") as HTMLInputElement
	var cloned_on: HTMLInputElement = deep.query_selector("#on") as HTMLInputElement
	var cloned_off: HTMLInputElement = deep.query_selector("#off") as HTMLInputElement
	if not _check(cloned_text != null and cloned_on != null and cloned_off != null, "find cloned form controls"):
		return
	_check(deep.get_children() == [cloned_text, cloned_on, cloned_off], "deep clone preserves child order")
	_check(cloned_text != original_text and cloned_text.value == "Edited", "text clone retains its current value")
	_check(cloned_on.checked and not cloned_off.checked, "checkbox clones retain checked and unchecked states")
	_check(cloned_text.set_attribute("value", "New default") == OK, "edit the cloned default value")
	_check(cloned_text.value == "Edited" and original_text.get_attribute("value") == "Default",
			"clone stays dirty and its attribute edit is independent")


# Port same-document detachment and cross-document adoption while retaining child identity.
# https://github.com/web-platform-tests/wpt/blob/master/dom/nodes/Document-adoptNode.html#L9-L25
func _test_adopt() -> void:
	var source: HTMLLayout = HTMLLayout.new()
	var target: HTMLLayout = HTMLLayout.new()
	if not _check(source.parse('<section><div id="moving"><span id="child">Text</span></div></section>') == OK
			and target.parse("<main></main>") == OK, "parse adoption fixtures"):
		return
	var node: HTMLElement = source.get_element_by_id("moving")
	var child: HTMLElement = source.get_element_by_id("child")
	var old_parent: HTMLElement = source.query_selector("section")
	var new_parent: HTMLElement = target.query_selector("main")
	if not _check(node != null and child != null and old_parent != null and new_parent != null,
			"find adoption fixture handles"):
		return
	_check(node.get_parent() == old_parent and child.get_parent() == node, "check the initial tree")
	_check(source.adopt_node(node) == node, "same-layout adoption returns the element")
	_check(node.get_parent() == null and node.get_children() == [child], "same-layout adoption only detaches")
	_check(old_parent.append_child(node) == OK, "reattach before cross-layout adoption")
	_check(target.adopt_node(node) == node, "cross-layout adoption returns the same handle")
	_check(node.get_parent() == null and source.get_element_by_id("moving") == null,
			"cross-layout adoption detaches from the source")
	_check(node.get_children() == [child] and child.get_parent() == node, "adopted descendant keeps its identity")
	# The original handle and descendant now follow the target layout's lifetime.
	source.clear()
	_check(node.is_valid() and child.is_valid(), "clearing the source leaves adopted handles valid")
	_check(new_parent.append_child(node) == OK and target.get_element_by_id("child") == child,
			"existing descendant handle is returned by target queries")


func _test_template_instantiation() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = ('<!doctype html><style>#outside { width: 40px; }</style>'
			+ '<main id="target"><div id="outside">Outside</div></main>'
			+ '<template id="card">Before<!--marker--><p class="entry">Inside</p>After'
			+ '<style>#outside { width: 80px; }</style>'
			+ '<template id="nested"><b id="hidden">Nested</b></template></template>')
	var viewport: Vector2i = Vector2i(300, 200)
	if not _check(layout.parse(html) == OK and layout.layout(viewport) == OK, "lay out template fixture"):
		return
	var template: HTMLTemplateElement = layout.get_element_by_id("card") as HTMLTemplateElement
	var target: HTMLElement = layout.get_element_by_id("target")
	var outside: HTMLElement = layout.get_element_by_id("outside")
	if not _check(template != null and target != null and outside != null, "find the template host and target"):
		return
	var content: HTMLDocumentFragment = template.content
	if not _check(content != null and content.get_children().size() == 3, "parsed template exposes its fragment"):
		return
	var entry: HTMLElement = content.get_children()[0]
	var nested: HTMLTemplateElement = content.query_selector("template#nested") as HTMLTemplateElement
	_check(content.query_selector(".entry") == entry and content.get_element_by_id("nested") == nested
			and content.query_selector("#hidden") == null and content.get_element_by_id("hidden") == null,
			"the fragment can find a nested template element while its contents stay separate")
	_check(nested != null and nested.content.get_element_by_id("hidden") == nested.content.query_selector(":scope > b")
			and nested.content.query_selector("b") != null and nested.content.query_selector("template b") == null,
			"nested content has its own scope and its selectors cannot reach the template host")
	_check(content == template.get_content() and template.get_children().is_empty(), "template content keeps its identity")
	_check(entry.get_parent() == null and not entry.is_connected() and entry.get_client_rects().is_empty(),
			"content is detached and has no geometry")
	_check(layout.query_selector(".entry") == null and template.query_selector(".entry") == null
			and not template.matches(":has(.entry)") and layout.get_element_by_id("nested") == null,
			"host, document, and relational selectors exclude template content")
	_check(not template.get_bounding_rect().has_area() and is_equal_approx(outside.get_bounding_rect().size.x, 40.0),
			"default template and its stylesheet are inactive")
	var shallow: HTMLDocumentFragment = content.clone_node()
	var shallow_host: HTMLTemplateElement = template.clone_node() as HTMLTemplateElement
	var deep_host: HTMLTemplateElement = template.clone_node(true) as HTMLTemplateElement
	_check(shallow != null and shallow != content and shallow.get_children().is_empty(), "shallow fragment clone is empty")
	_check(shallow_host != null and shallow_host.content != content and shallow_host.get_inner_html().is_empty(),
			"shallow host clone has its own empty content")
	_check(deep_host != null and deep_host.content != content and deep_host.get_inner_html() == template.get_inner_html(),
			"deep host clone copies text, comments, elements, and nested content")
	var first: HTMLDocumentFragment = content.clone_node(true)
	var next: HTMLDocumentFragment = content.clone_node(true)
	if not _check(first != null and next != null and first.get_children().size() == 3
			and next.get_children().size() == 3, "clone the template twice"):
		return
	var first_entry: HTMLElement = first.get_children()[0]
	var next_entry: HTMLElement = next.get_children()[0]
	_check(first_entry != next_entry and first_entry != entry, "instances use independent element handles")
	_check(target.append_fragment(first) == OK and target.append_fragment(next) == OK, "insert both cloned fragments")
	_check(first.get_children().is_empty() and next.get_children().is_empty() and content.get_children()[0] == entry,
			"insertion consumes only the copies")
	_check(layout.layout(viewport) == OK and first_entry.get_bounding_rect().has_area()
			and next_entry.get_bounding_rect().has_area() and is_equal_approx(outside.get_bounding_rect().size.x, 80.0),
			"inserted content and styles now participate in layout")
	_check(target.query_selector_all(".entry") == [first_entry, next_entry] and layout.get_element_by_id("hidden") == null,
			"document queries include both instances and still exclude nested content")
	_check(target.get_inner_html().contains("Before<!--marker-->") and target.get_inner_html().contains("After"),
			"batch insertion preserves the fragment text and comments")
	_check(layout.parse("<p>Replacement</p>") == OK and not entry.is_valid() and content.clone_node() == null,
			"reparse invalidates outstanding template content handles")


func _test_template_inner_html() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	if not _check(layout.parse('<div id="target"></div>') == OK, "parse programmatic template fixture"):
		return
	var template: HTMLTemplateElement = layout.create_element("template") as HTMLTemplateElement
	var target: HTMLElement = layout.get_element_by_id("target")
	if not _check(template != null and target != null, "create a typed template element"):
		return
	var content: HTMLDocumentFragment = template.content
	if not _check(template.set_inner_html('<tr><td>Row</td></tr><template><em>Nested</em></template>') == OK
			and content.get_children().size() == 2, "parse markup in the template context"):
		return
	var row: HTMLElement = content.get_children()[0]
	var nested: HTMLTemplateElement = content.get_children()[1] as HTMLTemplateElement
	if not _check(row.get_tag_name() == "tr" and nested != null and nested.content.get_children().size() == 1,
			"template context preserves table rows and nested templates"):
		return
	var nested_content: HTMLDocumentFragment = nested.content
	var italic: HTMLElement = nested_content.get_children()[0]
	_check(template.set_inner_html("Replacement<!--note-->") == OK and template.content == content,
			"replacing the markup preserves the content object")
	_check(not row.is_valid() and not nested.is_valid() and not italic.is_valid() and nested_content.clone_node() == null,
			"replacing markup invalidates former nested fragments")
	var clone: HTMLDocumentFragment = content.clone_node(true)
	_check(clone != null and target.append_fragment(clone) == OK and target.get_inner_html() == "Replacement<!--note-->",
			"a fragment containing only text and comments can be instantiated")
	_check(template.get_inner_html() == "Replacement<!--note-->", "serializing the original template remains independent")


func _test_nested_template_adoption() -> void:
	var source: HTMLLayout = HTMLLayout.new()
	var target: HTMLLayout = HTMLLayout.new()
	var html: String = ('<main id="wrapper"><template id="outer"><template id="inner">'
			+ '<div><input value="Default"></div></template></template></main>')
	if not _check(source.parse(html) == OK and target.parse('<main id="target"></main>') == OK, "parse nested templates"):
		return
	var wrapper: HTMLElement = source.get_element_by_id("wrapper")
	var outer: HTMLTemplateElement = source.get_element_by_id("outer") as HTMLTemplateElement
	var parent: HTMLElement = target.get_element_by_id("target")
	if not _check(wrapper != null and outer != null and parent != null and outer.content.get_children().size() == 1,
			"find the outer template"):
		return
	var outer_content: HTMLDocumentFragment = outer.content
	var inner: HTMLTemplateElement = outer_content.get_children()[0] as HTMLTemplateElement
	if not _check(inner != null and inner.content.get_children().size() == 1, "find the nested template"):
		return
	var inner_content: HTMLDocumentFragment = inner.content
	var child: HTMLElement = inner_content.get_children()[0]
	var input: HTMLInputElement = child.query_selector("input") as HTMLInputElement
	if not _check(input != null, "find the nested text input"):
		return
	input.value = "Edited"
	input.set_selection_range(1, 3)
	_check(outer_content.append_child(outer) == ERR_INVALID_PARAMETER, "reject placing the host in its content")
	_check(inner_content.append_child(wrapper) == ERR_INVALID_PARAMETER, "reject an ancestor across two template hosts")
	_check(child.append_fragment(outer_content) == ERR_INVALID_PARAMETER, "reject inserting a fragment into its hosted descendant")
	_check(outer_content.get_children() == [inner] and inner_content.get_children() == [child]
			and wrapper.get_children() == [outer], "rejected insertions preserve both trees")
	var imported: HTMLTemplateElement = target.import_node(outer, true) as HTMLTemplateElement
	_check(imported != null and imported.content != outer_content and imported.get_inner_html() == outer.get_inner_html(),
			"cross-layout import copies nested contents independently")
	_check(target.adopt_node(wrapper) == wrapper and source.get_element_by_id("outer") == null,
			"adopt an ordinary ancestor of nested templates")
	_check(outer.content == outer_content and inner.content == inner_content and inner_content.get_children() == [child],
			"adoption preserves both fragment handles and their contents")
	_check(source.query_selector("body").append_fragment(inner_content) == ERR_INVALID_PARAMETER,
			"the adopted fragment now belongs to the target layout")
	source.clear()
	_check(outer_content.query_selector(":scope > template") == inner and outer_content.get_element_by_id("inner") == inner
			and inner_content.query_selector("input") == input and outer_content.query_selector("input") == null,
			"adopted fragments query their new tree and preserve the original nested handles")
	_check(input.is_valid() and input.value == "Edited" and input.get_selection_start() == 1
			and input.get_selection_end() == 3, "adopted input keeps its handle, value, and selection after clearing the source")
	_check(parent.append_child(wrapper) == OK and target.get_element_by_id("outer") == outer
			and target.get_element_by_id("inner") == null and not child.is_connected(),
			"reinsert the host while its nested content stays detached")
	_check(wrapper.remove() == OK and not child.is_valid() and not input.is_valid()
			and outer_content.clone_node() == null and inner_content.clone_node() == null,
			"removing the host invalidates both hosted fragment trees")


func _test_form_dom_associations() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var other: HTMLLayout = HTMLLayout.new()
	var html: String = ('<form id="first"><input id="field" name="item" value="original"></form>'
			+ '<form id="second"></form><div id="target"></div>'
			+ '<template><form id="draft"><label>Size<select name="size"><option value="a">A</option>'
			+ '<option value="b" selected>B</option></select></label></form></template>')
	if not _check(layout.parse(html) == OK and other.parse("<main></main>") == OK, "parse form association fixtures"):
		return
	var first: HTMLFormElement = layout.get_element_by_id("first") as HTMLFormElement
	var second: HTMLFormElement = layout.get_element_by_id("second") as HTMLFormElement
	var field: HTMLInputElement = layout.get_element_by_id("field") as HTMLInputElement
	var target: HTMLElement = layout.get_element_by_id("target")
	var template: HTMLTemplateElement = layout.query_selector("template") as HTMLTemplateElement
	if not _check(first != null and second != null and field != null and target != null and template != null,
			"retrieve typed form controls"):
		return
	_check(field.get_form() == first and field.set_attribute("form", "second") == OK and field.get_form() == second
			and first.get_controls().is_empty() and second.get_controls() == [field], "reassociate a connected form field")
	_check(second.set_attribute("id", "updated") == OK and field.get_form() == null, "changing the form ID clears the explicit association")
	_check(field.remove_attribute("form") == OK and field.get_form() == first, "removing the explicit association restores ancestor lookup")
	var fragment: HTMLDocumentFragment = template.content.clone_node(true)
	var form: HTMLFormElement = fragment.get_element_by_id("draft") as HTMLFormElement if fragment != null else null
	var select: HTMLSelectElement = fragment.query_selector("select") as HTMLSelectElement if fragment != null else null
	if not _check(form != null and select != null and select.value == "b", "clone the form and select inside a template"):
		return
	_check(select.get_form() == form and target.append_fragment(fragment) == OK and select.get_form() == form,
			"template insertion keeps form associations and handles")
	select.value = "a"
	var chosen: HTMLElement = select.query_selector("option:checked")
	var clone: HTMLSelectElement = select.clone_node(true) as HTMLSelectElement
	_check(chosen != null and clone != null and clone.value == "a" and clone.query_selector("option:checked") != null,
			"cloning preserves the current selection and its selector state")
	_check(other.adopt_node(form) == form and select.get_form() == form and select.query_selector("option:checked") == chosen,
			"adoption preserves option handles and effective state")
	layout.clear()
	_check(select.value == "a" and form.reset() == OK and select.value == "b", "the adopted form can restore the select default")


# https://github.com/web-platform-tests/wpt/blob/master/dom/nodes/ParentNode-querySelector-All.js
# Port verifyStaticList for both a document and an element root.
func _test_selector_snapshot() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	if not _check(layout.parse("<main><div></div></main>") == OK, "parse selector snapshot fixture"):
		return
	var root: HTMLElement = layout.query_selector("main")
	var added: HTMLElement = layout.create_element("div")
	if not _check(root != null and added != null, "create selector snapshot fixture handles"):
		return
	var before_document: Array[HTMLElement] = layout.query_selector_all("div")
	var before_element: Array[HTMLElement] = root.query_selector_all("div")
	if not _check(before_element.size() == 1 and before_document == before_element, "query the original match"):
		return
	_check(root.append_child(added) == OK, "add a matching element")
	_check(before_document.size() == 1 and before_element.size() == 1, "earlier results remain snapshots")
	var expected: Array[HTMLElement] = [before_element[0], added]
	_check(layout.query_selector_all("div") == expected and root.query_selector_all("div") == expected,
			"later queries return both elements in document order")
	_check(root.query_selector_all(":scope > div, div") == expected, "selector lists deduplicate in scope")


# Port relational selector queries, closest, and matches using the same selector expectations.
# https://github.com/web-platform-tests/wpt/blob/master/css/selectors/has-basic.html#L57-L74
func _test_relational_selectors() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """<!doctype html><main><div id="a" class="ancestor">
		<div id="b" class="parent ancestor"><div class="sibling"><div id="d" class="descendant"></div></div>
		<div class="target"></div></div>
		<div id="f" class="parent ancestor"><div class="target"></div></div>
		<div id="h" class="parent ancestor"><div class="target"></div>
		<div class="sibling"><div id="k" class="descendant"></div></div></div></div></main>"""
	if not _check(layout.parse(html) == OK, "parse relational selector fixture"):
		return
	var root: HTMLElement = layout.query_selector("main")
	var a: HTMLElement = layout.get_element_by_id("a")
	var b: HTMLElement = layout.get_element_by_id("b")
	var f: HTMLElement = layout.get_element_by_id("f")
	var h: HTMLElement = layout.get_element_by_id("h")
	var k: HTMLElement = layout.get_element_by_id("k")
	if not _check(root != null and a != null and b != null and f != null and h != null and k != null,
			"find relational selector fixture handles"):
		return
	_check(root.query_selector_all(":has(.target)") == [a, b, f, h], "match descendants in :has()")
	_check(root.query_selector_all(":has(> .target)") == [b, f, h], "match direct children in :has()")
	_check(root.query_selector(":has(+ #h)") == f, "match adjacent siblings in :has()")
	_check(root.query_selector_all(".parent:has(~ #h)") == [b, f], "match following siblings in :has()")
	_check(k.closest(".ancestor:has(.descendant)") == h, "closest returns the nearest matching ancestor")
	_check(h.matches(":has(.target ~ .sibling .descendant)"), "matches resolves a relative selector chain")
