# Copyright © web-platform-tests contributors
# Adapted from Web Platform Tests. See LICENSE.md in this directory.
extends SceneTree

var _failures: int


func _initialize() -> void:
	_test_mutation_revisions()
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
	_test_node_navigation_and_data()
	_test_mixed_tree_edits()
	_test_fragment_import_and_adoption()
	_test_live_range_mutations()
	_test_range_contents()
	await _test_mutation_observer_delivery()
	if _failures == 0:
		print("Dockit DOM WPT ports passed.")
	quit(int(_failures != 0))


func _check(condition: bool, message: String) -> bool:
	if not condition:
		_failures += 1
		push_error("DOM WPT port: " + message)
	return condition


# DOM living ranges adjust for logical child insertion and removal.
# https://dom.spec.whatwg.org/#concept-node-insert
# https://dom.spec.whatwg.org/#concept-node-remove
func _test_live_range_mutations() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse('<main id="host"><b>A</b><span id="branch">甲🙂乙 文</span><i>Z</i></main>') == OK,
		"parse range mutation fixture")
	var host: HTMLElement = layout.get_element_by_id("host")
	var branch: HTMLElement = layout.get_element_by_id("branch")
	var text: HTMLTextNode = branch.get_first_child() as HTMLTextNode
	var range: HTMLRange = layout.create_range()
	if not _check(range != null and text != null, "create a live range and logical text handle"):
		return
	_check(range.set_start(host, 1) == OK and range.set_end(host, 2) == OK, "select one child by logical index")
	_check(host.prepend(["a new text node",]) == OK and range.get_start_offset() == 2 and range.get_end_offset() == 3,
		"insertion shifts later offsets by the number of DOM nodes, including split words")
	_check(host.insert_before(layout.create_element("u"), branch) == OK
		and range.get_start_offset() == 2 and range.get_end_offset() == 4,
		"an insertion on a boundary keeps that boundary before the new child")
	var selection: HTMLSelection = layout.get_selection()
	_check(selection.set_base_and_extent(text, 4, text, 1) == OK and selection.get_anchor_offset() == 4
		and selection.get_focus_offset() == 1 and selection.get_text() == "🙂乙 ",
		"backward selection preserves direction and uses Godot code point offsets")
	var selected_range: HTMLRange = selection.get_range_at(0)
	_check(text.set_data("替换") == OK and selected_range.is_collapsed() and selected_range.get_start_container() == text
		and selected_range.get_start_offset() == 0, "replacing all character data collapses offsets within the replaced interval")
	_check(branch.detach() == OK and selection.get_range_at(0) == selected_range
		and selection.get_anchor_node() == host and selection.get_anchor_offset() == 3,
		"removing a selected subtree updates both endpoints to the old child index")
	_check(host.append([branch,]) == OK and selection.get_anchor_node() == host,
		"reinserting the former subtree keeps the selection in the parent")

	_check(branch.replace_children(["ab", "🙂cd",]) == OK, "prepare adjacent logical text nodes")
	var first: HTMLTextNode = branch.get_first_child() as HTMLTextNode
	var last: HTMLTextNode = first.get_next_sibling() as HTMLTextNode
	_check(range.set_start(branch, 1) == OK and range.set_end(last, 2) == OK and branch.normalize() == OK
		and range.get_start_container() == first and range.get_start_offset() == 2
		and range.get_end_container() == first and range.get_end_offset() == 4 and range.get_text() == "🙂c",
		"normalization moves the inter-node boundary and the tail boundary into the merged text")
	var target: HTMLLayout = HTMLLayout.new()
	_check(target.parse("<p>Other layout</p>") == OK and target.adopt_node(branch) == branch
		and range.get_start_container() == host and range.is_collapsed(),
		"a source range relocates before a subtree is adopted into another layout")
	layout.clear()
	_check(not range.is_valid() and not selection.is_valid() and range.get_start_container() == null,
		"clearing the layout invalidates its ranges and selection")


# Adapt the cross-element content and text-split cases from the DOM Range suite.
# https://dom.spec.whatwg.org/#concept-range-extract
# https://dom.spec.whatwg.org/#concept-range-insert
func _test_range_contents() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse('<div id="host"><i id="left">abc</i><b id="middle">M</b><em id="right">xyz</em></div>') == OK,
		"parse range content fixture")
	var host: HTMLElement = layout.get_element_by_id("host")
	var left: HTMLTextNode = layout.get_element_by_id("left").get_first_child() as HTMLTextNode
	var right: HTMLTextNode = layout.get_element_by_id("right").get_first_child() as HTMLTextNode
	var middle: HTMLElement = layout.get_element_by_id("middle")
	var range: HTMLRange = layout.create_range()
	_check(range.set_start(left, 1) == OK and range.set_end(right, 2) == OK and range.get_text() == "bcMxy",
		"range text includes partial and full subtrees")
	_check(range.surround_contents(layout.create_element("strong")) == ERR_INVALID_PARAMETER and range.get_text() == "bcMxy",
		"surrounding partially selected elements fails before modifying the tree")
	var clone: HTMLDocumentFragment = range.clone_contents()
	_check(clone != null and clone.query_selector("i").get_text_content() == "bc"
		and clone.query_selector("em").get_text_content() == "xy" and clone.query_selector("b") != middle,
		"clone copies partial ancestor shells and fully contained nodes")
	var extracted: HTMLDocumentFragment = range.extract_contents()
	_check(extracted != null and extracted.is_equal_node(clone) and extracted.query_selector("b") == middle
		and not middle.is_connected() and left.data == "a" and right.data == "z"
		and range.get_start_container() == host and range.get_start_offset() == 1 and range.is_collapsed(),
		"extract retains complete node identities and collapses after the first partial ancestor")
	_check(range.insert_node(extracted) == OK and extracted.get_child_nodes().is_empty() and middle.is_connected(),
		"inserting the extracted fragment consumes it and reconnects its nodes")

	var text: HTMLTextNode = layout.create_text_node("abcdef")
	_check(host.replace_children([text,]) == OK and range.set_start(text, 2) == OK and range.collapse(true) == OK,
		"prepare an insertion in the middle of a text node")
	var tail: HTMLRange = layout.create_range()
	_check(tail.set_start(text, 4) == OK and tail.set_end(text, 6) == OK, "track an independent range in the text suffix")
	var mark: HTMLElement = layout.create_element("mark")
	_check(range.insert_node(mark) == OK and text.data == "ab" and tail.get_text() == "ef"
		and tail.get_start_container() != text and tail.get_start_offset() == 2,
		"splitting a text node transfers later points to the new suffix")
	var strong: HTMLElement = layout.create_element("strong")
	_check(tail.surround_contents(strong) == OK and strong.get_text_content() == "ef" and tail.get_text() == "ef",
		"surrounding a character range wraps the selected text")
	_check(tail.delete_contents() == OK and not strong.is_connected() and tail.is_collapsed(),
		"deleting range contents removes nodes and collapses the range")


func _test_mutation_observer_delivery() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse('<main id="host"><p id="a">initial</p></main>') == OK, "parse mutation observer fixture")
	var host: HTMLElement = layout.get_element_by_id("host")
	var a: HTMLElement = layout.get_element_by_id("a")
	var text: HTMLTextNode = a.get_first_child() as HTMLTextNode
	var observer: HTMLMutationObserver = HTMLMutationObserver.new()
	var batches: Array[Array] = []
	observer.mutations_delivered.connect(func(records: Array) -> void: batches.append(records))
	_check(observer.observe(host, {"subtree": true, "child_list": true, "attribute_filter": ["id",],
		"attribute_old_value": true, "character_data_old_value": true,}) == OK, "observe tree changes and old values")
	var fragment: HTMLDocumentFragment = layout.create_document_fragment()
	var empty: HTMLElement = layout.create_element("span")
	var fragment_observer: HTMLMutationObserver = HTMLMutationObserver.new()
	_check(fragment.replace_children([empty, "two words",]) == OK
		and fragment_observer.observe(fragment, {"child_list": true,}) == OK and host.append([fragment, "tail",]) == OK,
		"insert a mixed fragment in a single transaction")
	var removed_from_fragment: Array[Dictionary] = fragment_observer.take_records()
	_check(removed_from_fragment.size() == 1 and removed_from_fragment[0]["target"] == fragment
		and removed_from_fragment[0]["removed_nodes"].size() == 2,
		"consuming the fragment also reports its removed logical nodes")
	_check(a.set_attribute("class", "ignored") == OK and a.set_attribute("id", "updated") == OK
		and text.set_data("second") == OK, "mutate a filtered attribute and a descendant text node")
	_check(batches.is_empty(), "mutation callbacks are deferred")
	var records: Array[Dictionary] = observer.take_records()
	if _check(records.size() == 3, "mixed batch, filtered attribute, and text each produce one record"):
		var added: Array = records[0]["added_nodes"]
		_check(records[0]["type"] == "childList" and added.size() == 3 and added[0] == empty
			and records[0]["previous_sibling"] == a and records[0]["next_sibling"] == null,
			"fragment insertion reports logical nodes in order")
		_check(records[1]["attribute_name"] == "id" and records[1]["old_value"] == "a"
			and records[2]["target"] == text and records[2]["old_value"] == "initial",
			"attribute and character data records preserve real prior values")
	_check(host.append([host,]) == ERR_INVALID_PARAMETER and observer.take_records().is_empty(),
		"an invalid atomic edit emits no mutation")
	await process_frame
	await process_frame
	_check(batches.is_empty(), "take_records drains the scheduled callback")
	_check(a.detach() == OK and text.set_data("detached before delivery") == OK, "edit a just-detached subtree")
	await process_frame
	await process_frame
	_check(batches.size() == 1 and batches[0].size() == 2
		and batches[0][0]["removed_nodes"] == [a,] and batches[0][1]["target"] == text,
		"subtree observations follow removed descendants through the removal delivery")
	text.set_data("after delivery")
	await process_frame
	_check(batches.size() == 1, "a detached subtree stops reporting changes after delivery")
	_check(observer.observe(a, {"attributes": true,}) == OK, "observe the detached node directly")
	var destination: HTMLLayout = HTMLLayout.new()
	_check(destination.parse("<p>Destination</p>") == OK and destination.adopt_node(a) == a
		and a.set_attribute("title", "adopted") == OK, "adopt a directly observed node into a second layout")
	var adoption_records: Array[Dictionary] = observer.take_records()
	_check(adoption_records.size() == 1 and adoption_records[0]["target"] == a,
		"direct observations follow an adopted handle without duplicating records")
	observer.disconnect()

	var first: HTMLMutationObserver = HTMLMutationObserver.new()
	var second: HTMLMutationObserver = HTMLMutationObserver.new()
	var order: Array[String] = []
	first.mutations_delivered.connect(func(batch: Array) -> void:
		order.append("first:" + str(batch[0]["attribute_name"]))
		if batch[0]["attribute_name"] == "id":
			host.set_attribute("class", "reentrant")
	)
	second.mutations_delivered.connect(func(batch: Array) -> void:
		order.append("second:" + str(batch[0]["attribute_name"]))
		_check(batch.size() == 1, "a reentrant update cannot extend the current delivery snapshot")
	)
	_check(first.observe(host, {"attributes": true,}) == OK and second.observe(host, {"attributes": true,}) == OK,
		"register two observers on the same node")
	host.set_attribute("id", "changed")
	_check(order.is_empty(), "observe does not invoke callbacks during the edit")
	for _frame: int in range(4):
		await process_frame
	_check(order == ["first:id", "second:id", "first:class", "second:class",], "callback edits arrive in a separate delivery")
	host.set_attribute("title", "pending clear")
	layout.clear()
	for _frame: int in range(2):
		await process_frame
	_check(order.size() == 4 and first.take_records().is_empty(), "clear drops pending observer work from the old layout")


func _test_node_navigation_and_data() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	if not _check(layout.parse('<!doctype html><title>  Draft   title </title><main id="root">hello world<!-- note --><b name="item">tail</b></main>') == OK,
			"parse the document and character-node fixture"):
		return
	var document: HTMLDocumentNode = layout.get_document_node()
	var main: HTMLElement = layout.get_element_by_id("root")
	if not _check(document != null and main != null, "expose the document and its main element"):
		return
	_check(document.get_first_child() == layout.get_document_element() and main.get_root_node() == document,
			"the document and root use stable handles")
	_check(layout.get_head().get_parent() == layout.get_document_element() and layout.get_body().contains(main),
			"head and body are direct document-element children")
	_check(layout.get_title() == "Draft title" and layout.set_title("New title") == OK and layout.get_title() == "New title",
			"read a normalized title and replace it in the runtime DOM")
	var children: Array[HTMLNode] = main.get_child_nodes()
	if not _check(children.size() == 3, "native text parts appear as one logical DOM text node"):
		return
	var text: HTMLTextNode = children[0] as HTMLTextNode
	var comment: HTMLCommentNode = children[1] as HTMLCommentNode
	var element: HTMLElement = children[2] as HTMLElement
	if not _check(text != null and comment != null and element != null, "use typed text, comment, and element handles"):
		return
	_check(text.data == "hello world" and comment.data == " note " and element == main.get_first_element_child(),
			"typed nodes preserve source data")
	_check(text.get_next_sibling() == comment and text.get_parent_node() == main and document.contains(text),
			"node navigation includes comments and text")
	_check(text.compare_document_position(element) == HTMLNode.POSITION_FOLLOWING
			and main.compare_document_position(text) == (HTMLNode.POSITION_FOLLOWING | HTMLNode.POSITION_CONTAINED_BY),
			"document position flags distinguish following and contained nodes")
	_check(text.set_data("new words 中文") == OK and comment.set_data("edited") == OK
			and main.get_first_child() == text and main.get_text_content() == "new words 中文tail",
			"character edits preserve identity and update raw DOM text")
	_check(main.get_outer_html().contains("<!--edited-->") and layout.get_elements_by_name("item") == [element,],
			"serialize comments and query name attributes")
	element.css_class_name = "one two three"
	_check(element.replace_class("one", "three") and element.css_class_name == "three two"
			and main.get_elements_by_class_name("two\tthree") == [element,], "replace and query ordered class tokens")
	_check(element.set_dataset_value("questName", "forest") == OK and element.get_attribute("data-quest-name") == "forest"
			and element.get_attribute_names().has("data-quest-name"), "reflect dataset keys through source attributes")
	_check(element.toggle_attribute("hidden") and element.hidden and not element.toggle_attribute("hidden", false),
			"toggle a boolean attribute and read its reflected property")
	_check(layout.parse("<p>Replacement</p>") == OK and not text.is_valid() and not document.is_valid(),
			"reparsing invalidates text and document handles")


func _test_mixed_tree_edits() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	if not _check(layout.parse('<main id="root"><b id="a">A</b><i id="b">B</i></main>') == OK, "parse the mixed edit fixture"):
		return
	var main: HTMLElement = layout.get_element_by_id("root")
	var a: HTMLElement = layout.get_element_by_id("a")
	var b: HTMLElement = layout.get_element_by_id("b")
	var fragment: HTMLDocumentFragment = layout.create_document_fragment()
	var text: HTMLTextNode = layout.create_text_node("two words")
	if not _check(fragment != null and text != null, "create detached fragment and text handles"):
		return
	_check(fragment.append_child(text) == OK and fragment.get_first_child() == text and text.get_parent_node() == fragment,
			"a fragment can hold a logical text node")
	_check(main.prepend([fragment, main,]) == ERR_INVALID_PARAMETER and fragment.get_first_child() == text,
			"a failed batch does not consume its fragment")
	_check(a.after([b, " ", fragment, a,]) == OK and main.get_children() == [b, a,]
			and fragment.get_child_nodes().is_empty() and text.get_parent_node() == main,
			"one batch moves siblings and consumes a mixed fragment")
	_check(main.normalize() == OK and main.get_child_nodes().size() == 3 and text.is_valid() and text.get_parent_node() == null,
			"normalize merges neighboring text into the first node and detaches the rest")
	_check(text.set_data("detached again") == OK and text.data == "detached again", "detached text remains editable")
	_check(b.insert_adjacent_html("beforebegin", '<em id="new">&amp;</em>') == OK
			and main.get_first_element_child() == layout.get_element_by_id("new"), "parse adjacent markup in the parent context")
	_check(b.set_outer_html("<strong>next</strong>") == OK and b.is_valid() and not b.is_connected()
			and main.get_first_element_child().get_next_element_sibling().get_tag_name() == "strong", "outer HTML replaces and detaches")
	_check(main.set_inner_text("one\r\ntwo") == OK and main.get_inner_html() == "one<br>two", "set inner text with structural line breaks")
	_check(layout.layout(Vector2i(160, 100)) == OK and main.get_inner_text() == "one\ntwo", "read rendered structural line breaks")


func _test_fragment_import_and_adoption() -> void:
	var source: HTMLLayout = HTMLLayout.new()
	var target: HTMLLayout = HTMLLayout.new()
	_check(source.parse('<table id="table"></table>') == OK and target.parse('<div id="target"></div>') == OK, "parse fragment layouts")
	var fragment: HTMLDocumentFragment = source.parse_fragment("<tr><td>anchor text</td></tr>", source.get_element_by_id("table"))
	if not _check(fragment != null and fragment.query_selector("tbody > tr > td") != null, "parse fragment in a table context"):
		return
	var cell: HTMLElement = fragment.query_selector("td")
	var text: HTMLTextNode = cell.get_first_child() as HTMLTextNode
	var copy: HTMLDocumentFragment = target.import_fragment(fragment)
	_check(copy != null and copy.is_equal_node(fragment) and copy.query_selector("td") != cell, "import creates a separate fragment tree")
	_check(target.adopt_fragment(fragment) == fragment and fragment.query_selector("td") == cell, "adopt retains fragment and element identity")
	source.clear()
	_check(target.get_element_by_id("target").append([fragment,]) == OK and text.is_valid()
			and text.is_connected() and cell.get_first_child() == text and text.data == "anchor text",
			"adopted text identity survives source clear and fragment insertion")


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


func _test_mutation_revisions() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	_check(layout.parse('<main id="host"><span id="text">Old words</span><input id="field" value="old"><input id="a" type="radio" name="g" checked><input id="b" type="radio" name="g"><select id="select"><option id="one">One</option><option id="two">Two</option></select></main>') == OK,
		"parse the revision fixture without observers")
	var host: HTMLElement = layout.get_element_by_id("host")
	var text: HTMLElement = layout.get_element_by_id("text")
	var data: HTMLTextNode = text.get_first_child() as HTMLTextNode
	var field: HTMLInputElement = layout.get_element_by_id("field") as HTMLInputElement
	var root_revision: int = host.mutation_revision
	var revision: int = text.mutation_revision
	_check(revision == 1 and text.get_mutation_revision() == revision, "fresh nodes have a readable initial revision")
	text.set_attribute("title", "changed")
	_check(text.mutation_revision > revision and host.mutation_revision == root_revision,
		"attribute writes only change the target revision")
	revision = text.mutation_revision
	text.set_attribute("title", "changed")
	_check(text.mutation_revision == revision, "repeated attribute writes keep their revision")
	var parent_revision: int = text.mutation_revision
	revision = data.mutation_revision
	data.set_data("New words")
	_check(data.mutation_revision > revision and text.mutation_revision == parent_revision,
		"logical text revisions survive native text replacement")
	revision = data.mutation_revision
	data.set_data("New words")
	_check(data.mutation_revision == revision, "unchanged text does not increment revisions")
	revision = field.mutation_revision
	field.value = "edited"
	_check(field.mutation_revision > revision, "live input values increment revisions without observers")
	revision = field.mutation_revision
	field.value = "edited"
	field.set_selection_range(0, 2)
	_check(field.mutation_revision == revision, "selection and repeated live values do not increment revisions")
	var first: HTMLInputElement = layout.get_element_by_id("a") as HTMLInputElement
	var second: HTMLInputElement = layout.get_element_by_id("b") as HTMLInputElement
	var first_revision: int = first.mutation_revision
	revision = second.mutation_revision
	second.checked = true
	_check(first.mutation_revision > first_revision and second.mutation_revision > revision,
		"radio synchronization versions every changed control")
	var one: HTMLOptionElement = layout.get_element_by_id("one") as HTMLOptionElement
	var two: HTMLOptionElement = layout.get_element_by_id("two") as HTMLOptionElement
	first_revision = one.mutation_revision
	revision = two.mutation_revision
	two.selected = true
	_check(one.mutation_revision > first_revision and two.mutation_revision > revision,
		"select synchronization versions both effective option states")
	var fragment: HTMLDocumentFragment = layout.create_document_fragment()
	var fragment_revision: int = fragment.mutation_revision
	revision = text.mutation_revision
	fragment.append_child(text)
	_check(fragment.mutation_revision > fragment_revision and text.mutation_revision > revision and not text.is_connected(),
		"moving nodes versions the fragment and moved root")
	revision = text.mutation_revision
	text.set_attribute("title", "detached")
	_check(text.mutation_revision > revision, "detached nodes still count mutations")
	var clone: HTMLElement = text.clone_node(true)
	_check(clone != text and clone.mutation_revision == 1, "clones begin with independent revision state")
	var destination: HTMLLayout = HTMLLayout.new()
	destination.parse("<main></main>")
	revision = text.mutation_revision
	_check(destination.adopt_node(text) == text and text.mutation_revision > revision,
		"adoption preserves and advances the logical revision")
	revision = text.mutation_revision
	text.set_attribute("title", "adopted")
	_check(text.mutation_revision > revision, "adopted nodes continue counting in the new owner")
	var orphan: HTMLElement = layout.create_element("span")
	var orphan_id: int = orphan.get_instance_id()
	orphan.set_attribute("title", "retained")
	fragment.append_child(orphan)
	revision = orphan.mutation_revision
	orphan = null
	orphan = fragment.query_selector("span[title=retained]")
	_check(orphan != null and orphan.mutation_revision == revision, "revision state survives wrapper replacement")
	_check(orphan.get_instance_id() != orphan_id, "the wrapper lifecycle fixture actually replaced its handle")
	layout.parse("<p>Reparsed</p>")
	_check(field.mutation_revision == 0, "invalid handles return revision zero")
