// Portions adapted from litehtml.
// Copyright (c) 2013, Yuri Kobets (tordex). All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// See addons/dockit/LiteHTML-LICENSES/litehtml.txt for the license and disclaimer.

#ifndef LH_MASTER_CSS_H
#define LH_MASTER_CSS_H
namespace litehtml{ const char* const master_css = R"##(


html {
	display: block;
}

head {
	display: none
}

meta {
	display: none
}

title {
	display: none
}

link {
	display: none
}

style {
	display: none
}

script, template {
	display: none
}

map, area {
	display: none
}

svg defs, svg clippath, svg mask {
	display: none
}

body {
	display:block; 
	margin:8px; 
}

p {
	display: block;
	margin-block: 1em;
	margin-inline: 0;
}

b, strong {
	display:inline; 
	font-weight:bolder;
}

bdi, [dir="auto"] {
	unicode-bidi: isolate;
}

bdo {
	unicode-bidi: bidi-override;
}

i, em, cite, dfn, var, address { font-style: italic; }
address { display: block; }
small { font-size: smaller; }
abbr[title] { text-decoration: underline dotted; }

ins, u {
	text-decoration:underline
}

del, s, strike {
	text-decoration:line-through
}

center 
{
	text-align:center;
	display:block;
}

a:link
{
	text-decoration: underline;
	color: LinkText;
	cursor: pointer;
}

h1, h2, h3, h4, h5, h6, div {
	display: block;
}

h1 {
	font-weight:bold; 
	margin-top:0.67em; 
	margin-bottom:0.67em; 
	font-size: 2em;
}

h2 {
	font-weight:bold; 
	margin-top:0.83em; 
	margin-bottom:0.83em; 
	font-size: 1.5em;
}

h3 {
	font-weight:bold; 
	margin-top:1em; 
	margin-bottom:1em; 
	font-size:1.17em;
}

h4 {
	font-size: 1em;
	font-weight:bold; 
	margin-top:1.33em; 
	margin-bottom:1.33em
}

h5 {
	font-weight:bold; 
	margin-top:1.67em; 
	margin-bottom:1.67em;
	font-size:.83em;
}

h6 {
	font-weight:bold; 
	margin-top:2.33em; 
	margin-bottom:2.33em;
	font-size:.67em;
} 

br {
	display:inline-block;
}

br[clear="all"]
{
	clear:both;
}

br[clear="left"]
{
	clear:left;
}

br[clear="right"]
{
	clear:right;
}

span {
	display:inline
}

img {
	display: inline-block;
}

img[align="right"]
{
	float: right;
}

img[align="left"]
{
	float: left;
}

hr {
	display: block;
	margin-block: 0.5em;
	margin-inline: auto;
	overflow: hidden;
	color: gray;
	border: 1px inset currentColor
}


/***************** TABLES ********************/

table {
	display: table;
	border-collapse: separate;
	border-spacing: 2px;
	border-color: gray;
	font-size: medium;
	font-weight: normal;
	font-style: normal;
}

tbody, tfoot, thead {
	vertical-align: middle;
	border-color: inherit;
}
tbody { display: table-row-group; }
thead { display: table-header-group; }
tfoot { display: table-footer-group; }
col { display: table-column; }
colgroup { display: table-column-group; }

tr {
	display: table-row;
	vertical-align: inherit;
	border-color: inherit;
}

td, th {
	display: table-cell;
	vertical-align: inherit;
	border-width:1px;
	padding:1px;
}

th {
	font-weight: bold;
	text-align: center;
}

table[border] {
	border-style:solid;
}

table[border^="0"] {
	border-style:none;
}

table[border] td, table[border] th {
	border-style:solid;
	border-top-color:black;
	border-left-color:black;
	border-bottom-color:gray;
	border-right-color:gray;
}

table[border^="0"] td, table[border^="0"] th {
	border-style:none;
}

table[align=left] {
   float: left;
}

table[align=right] {
   float: right;
}

table[align=center] {
   margin-left: auto;
   margin-right: auto;
}

caption {
	display: table-caption;
	text-align: center;
}

td[nowrap], th[nowrap] {
	white-space:nowrap;
}

tt, code, kbd, samp {
	font-family: monospace
}

pre, xmp, plaintext, listing {
	display: block;
	font-family: monospace;
	white-space: pre;
	margin: 1em 0
}

/***************** LISTS ********************/

ul, menu, dir {
	display: block;
	list-style-type: disc;
	margin-block: 1em;
	margin-inline: 0;
	padding-inline-start: 40px
}

ol {
	display: block;
	list-style-type: decimal;
	margin-block: 1em;
	margin-inline: 0;
	padding-inline-start: 40px
}

li {
	display: list-item;
}

:is(ul, ol, menu, dir) :is(ul, menu, dir) {
	list-style-type: circle;
}

:is(ul, ol, menu, dir) :is(ul, ol, menu, dir) :is(ul, menu, dir) {
	list-style-type: square;
}

dd {
	display: block;
	margin-inline-start: 40px;
}

dl {
	display: block;
	margin-block: 1em;
	margin-inline: 0;
}

dt { display: block; }

:is(ul, ol, menu, dl, dir) :is(ul, ol, menu, dl, dir) {
	margin-block: 0;
}

blockquote {
	display: block;
	margin-top: 1em;
	margin-bottom: 1em;
	margin-inline: 40px;
}

/*********** FORM ELEMENTS ************/

form {
	display: block;
	margin-top: 0em;
}

option, optgroup, datalist {
	display: none;
}

fieldset {
	display: block;
	margin-inline: 2px;
	border: 2px groove #f0f0f0;
	min-inline-size: min-content;
	padding-block: 0.35em 0.625em;
	padding-inline: 0.75em;
}
legend { display: block; padding-inline: 2px; }
label { cursor: default; }

button, meter, progress, select,
input:is([type="reset" i], [type="button" i], [type="submit" i], [type="checkbox" i], [type="radio" i]) {
	user-select: none;
}

audio:not([controls]) { display: none !important; }
video {
  display: inline-block;
  object-fit: contain;
}

audio[controls] {
	display: inline-block; width: 300px; max-width: 100%; height: 54px; border-radius: 27px;
	font-family: Arial, sans-serif; font-size: 13px;
	background-color: light-dark(#f1f3f4, #303134); color: light-dark(#202124, #e8eaed);
}
audio:focus-visible { outline: 2px solid -dockit-focus-ring; }
progress, meter {
	display: inline-block;
	appearance: auto;
	box-sizing: border-box;
	width: 10em;
	height: 1em;
	vertical-align: -0.2em;
}
meter { width: 5em; }

input, textarea, select, button {
	appearance: auto;
	display: inline-block;
	font: 13.333px Arial, sans-serif;
	margin: 0;
	color: FieldText;
	line-height: normal;
	text-transform: none;
	text-indent: 0;
	text-shadow: none;
}

input:not(:is([type="checkbox" i], [type="radio" i], [type="range" i], [type="hidden" i],
	[type="button" i], [type="submit" i], [type="reset" i], [type="image" i], [type="file" i],
	[type="color" i], [type="date" i], [type="datetime-local" i], [type="month" i], [type="week" i], [type="time" i])) {
	width: 169px;
	height: 1lh;
	background-color: Field;
	border: 1px solid ButtonBorder;
	padding: 2px 3px;
	border-radius: 2px;
	cursor: text;
}

input:is([type="checkbox" i], [type="radio" i]) {
	width: 13px;
	height: 13px;
	box-sizing: border-box;
	margin: 3px 3px 3px 4px;
	border: 0 solid ButtonBorder;
	border-radius: 2px;
	background-color: transparent;
	cursor: default;
}
input[type="radio" i] {
	margin: 3px 3px 0 5px;
	border-radius: 50%;
}
input[type="range" i] { width: 129px; height: 16px; margin: 2px; cursor: default; }
input:is([type="date" i], [type="datetime-local" i], [type="month" i], [type="week" i], [type="time" i]) {
	width: 11em;
	height: 1lh;
	background-color: Field;
	border: 1px solid ButtonBorder;
	padding: 2px 3px;
	border-radius: 2px;
	cursor: text;
}
input[type="datetime-local" i] { width: 16em; }
input[type="time" i] { width: 8em; }
input[type="file" i] { width: 19em; height: calc(1lh + 9px); padding: 0; cursor: default; user-select: none; }
input[type="image" i] { cursor: pointer; user-select: none; }
input[type="color" i] {
	box-sizing: border-box;
	width: 50px;
	height: 27px;
	border: 1px solid ButtonBorder;
	border-radius: 2px;
	padding: 1px 2px;
	background-color: ButtonFace;
	cursor: default;
}
input[type="color" i][list] { width: 94px; }

button, input:is([type="button" i], [type="submit" i], [type="reset" i]) {
	box-sizing: border-box;
	padding: 2px 7px;
	background-color: ButtonFace;
	color: ButtonText;
	border: 1px solid ButtonBorder;
	border-radius: 2px;
	text-align: center;
	cursor: default;
}
input:is([type="button" i], [type="submit" i], [type="reset" i]) { min-height: 21px; }
:is(button, input[type="button" i], input[type="submit" i], input[type="reset" i]):hover {
	background-color: -dockit-button-hover;
	border-color: -dockit-border-hover;
}
:is(button, input[type="button" i], input[type="submit" i], input[type="reset" i]):active {
	background-color: -dockit-button-active;
	border-color: -dockit-border-active;
}
:is(button, input, select, textarea):disabled {
	color: -dockit-disabled-text;
	background-color: -dockit-disabled-face;
	border-color: -dockit-disabled-border;
	cursor: default;
}
input:is([type="checkbox" i], [type="radio" i]):disabled { background-color: transparent; }

select {
	box-sizing: border-box;
	height: 19px;
	padding-inline: 3px 1px;
	background-color: Field;
	border: 1px solid ButtonBorder;
	cursor: default;
}
select[multiple] { border-radius: 2px; }
textarea {
	font-family: monospace;
	background-color: Field;
	border: 1px solid ButtonBorder;
	padding: 2px;
	white-space: pre-wrap;
	overflow: auto;
	resize: both;
	cursor: text;
}
:is(button, textarea, input, select)[hidden] { display: none; }
:is(button, input, select, textarea, a[href], [tabindex]):focus-visible { outline: 2px solid -dockit-focus-ring; }
input[type="hidden" i] { display: none !important; }


article, aside, footer, header, hgroup, main, nav, search, section
{
	display: block;
}

details, summary { display: block; }
dialog:not([open]) { display: none; }
dialog {
	position: absolute;
	inset-inline: 0;
	width: fit-content;
	height: fit-content;
	margin: auto;
	border: solid;
	padding: 1em;
	background: Canvas;
	color: CanvasText;
}
dialog:modal {
	position: fixed;
	top: 50%;
	bottom: auto;
	height: auto;
	transform: translateY(-50%);
	max-width: calc(100% - 6px - 2em);
	max-height: calc(100% - 6px - 2em);
	overflow: auto;
}
dialog::backdrop { position: fixed; inset: 0; background: rgba(0, 0, 0, 0.1); }
details > summary:first-of-type { display: list-item; list-style: disclosure-closed inside; }
details:open > summary:first-of-type { list-style-type: disclosure-open; }
details:not(:has(> summary))::before { display: block; content: "▸ Details"; }
details:open:not(:has(> summary))::before { content: "▾ Details"; }
details > summary:first-of-type:focus-visible, details:not(:has(> summary)):focus-visible { outline: 2px solid -dockit-focus-ring; }

ruby { display: inline; }
ruby > rt { display: block; font-size: 50%; white-space: nowrap; }
rp { display: none; }

mark { background-color: yellow; color: black; }
q::before { content: open-quote; }
q::after { content: close-quote; }

sub {
	vertical-align: sub;
	font-size: smaller;
}

sup {
	vertical-align: super;
	font-size: smaller;
}

figure {
	display: block;
	margin-top: 1em;
	margin-bottom: 1em;
	margin-inline: 40px;
}

figcaption {
	display: block;
}

)##"; }
#endif  // LH_MASTER_CSS_H
