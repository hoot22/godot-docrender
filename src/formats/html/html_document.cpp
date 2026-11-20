#include "html_document.h"
#include "html_print_layout.h"

using namespace godot;

void HTMLDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &HTMLDocument::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &HTMLDocument::get_text);
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &HTMLDocument::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &HTMLDocument::get_base_dir);
	ClassDB::bind_method(D_METHOD("_set_source_path", "path"), &HTMLDocument::set_source_path);
	ClassDB::bind_method(D_METHOD("_get_source_path"), &HTMLDocument::get_source_path);
	ClassDB::bind_method(D_METHOD("set_stylesheet", "stylesheet"), &HTMLDocument::set_stylesheet);
	ClassDB::bind_method(D_METHOD("get_stylesheet"), &HTMLDocument::get_stylesheet);
	ClassDB::bind_method(D_METHOD("_set_textures", "textures"), &HTMLDocument::set_textures);
	ClassDB::bind_method(D_METHOD("_get_textures"), &HTMLDocument::get_textures);
	ClassDB::bind_method(D_METHOD("_set_video_sources", "sources"), &HTMLDocument::set_video_sources);
	ClassDB::bind_method(D_METHOD("_get_video_sources"), &HTMLDocument::get_video_sources);
	ClassDB::bind_method(D_METHOD("_set_audio_streams", "streams"), &HTMLDocument::set_audio_streams);
	ClassDB::bind_method(D_METHOD("_get_audio_streams"), &HTMLDocument::get_audio_streams);
	ClassDB::bind_method(D_METHOD("_set_local_stylesheets", "stylesheets"), &HTMLDocument::set_local_stylesheets);
	ClassDB::bind_method(D_METHOD("_get_local_stylesheets"), &HTMLDocument::get_local_stylesheets);
	ClassDB::bind_method(D_METHOD("_set_local_fonts", "fonts"), &HTMLDocument::set_local_fonts);
	ClassDB::bind_method(D_METHOD("_get_local_fonts"), &HTMLDocument::get_local_fonts);
	ClassDB::bind_method(D_METHOD("_set_image_dpi", "dpi"), &HTMLDocument::set_image_dpi);
	ClassDB::bind_method(D_METHOD("_get_image_dpi"), &HTMLDocument::get_image_dpi);
	ClassDB::bind_method(D_METHOD("_set_image_orientations", "orientations"), &HTMLDocument::set_image_orientations);
	ClassDB::bind_method(D_METHOD("_get_image_orientations"), &HTMLDocument::get_image_orientations);
	ClassDB::bind_method(D_METHOD("_set_svg_clip_sources", "sources"), &HTMLDocument::set_svg_clip_sources);
	ClassDB::bind_method(D_METHOD("_get_svg_clip_sources"), &HTMLDocument::get_svg_clip_sources);
	ClassDB::bind_method(D_METHOD("create_layout"), &HTMLDocument::create_layout);
	ClassDB::bind_method(D_METHOD("create_print_layout", "options"), &HTMLDocument::create_print_layout, DEFVAL(Dictionary()));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_dir", PROPERTY_HINT_DIR), "set_base_dir", "get_base_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "_source_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_source_path", "_get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stylesheet", PROPERTY_HINT_MULTILINE_TEXT), "set_stylesheet", "get_stylesheet");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_video_sources", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_video_sources", "_get_video_sources");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_audio_streams", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_audio_streams", "_get_audio_streams");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_textures", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "_set_textures", "_get_textures");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_local_stylesheets", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_local_stylesheets", "_get_local_stylesheets");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_local_fonts", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_local_fonts", "_get_local_fonts");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_image_dpi", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_image_dpi", "_get_image_dpi");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_image_orientations", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_image_orientations", "_get_image_orientations");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_svg_clip_sources", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_svg_clip_sources", "_get_svg_clip_sources");
}

void HTMLDocument::set_text(const String& p_text) {
	if (text != p_text) {
		text = p_text;
		emit_changed();
	}
}

String HTMLDocument::get_text() const {
	return text;
}

void HTMLDocument::set_base_dir(const String& p_dir) {
	if (base_dir != p_dir) {
		base_dir = p_dir;
		emit_changed();
	}
}

String HTMLDocument::get_base_dir() const {
	return base_dir;
}

void HTMLDocument::set_source_path(const String& p_path) {
	if (source_path != p_path) {
		source_path = p_path;
		emit_changed();
	}
}

String HTMLDocument::get_source_path() const { return source_path; }

void HTMLDocument::set_stylesheet(const String& p_stylesheet) {
	if (stylesheet != p_stylesheet) {
		stylesheet = p_stylesheet;
		emit_changed();
	}
}

String HTMLDocument::get_stylesheet() const {
	return stylesheet;
}

void HTMLDocument::set_video_sources(const Dictionary& p_sources) {
	video_sources = p_sources.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_video_sources() const {
	return video_sources.duplicate();
}

void HTMLDocument::set_audio_streams(const Dictionary& p_audio) {
	audio_streams = p_audio.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_audio_streams() const {
	return audio_streams.duplicate();
}

void HTMLDocument::set_textures(const Dictionary& p_textures) {
	textures = p_textures.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_textures() const {
	return textures.duplicate();
}

void HTMLDocument::set_local_stylesheets(const Dictionary& p_stylesheets) {
	local_stylesheets = p_stylesheets.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_local_stylesheets() const {
	return local_stylesheets.duplicate();
}

void HTMLDocument::set_local_fonts(const Dictionary& p_fonts) {
	local_fonts = p_fonts.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_local_fonts() const {
	return local_fonts.duplicate();
}

void HTMLDocument::set_image_dpi(const Dictionary& p_dpi) {
	image_dpi = p_dpi.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_image_dpi() const {
	return image_dpi.duplicate();
}

void HTMLDocument::set_image_orientations(const Dictionary& p_orientations) {
	image_orientations = p_orientations.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_image_orientations() const {
	return image_orientations.duplicate();
}

void HTMLDocument::set_svg_clip_sources(const Dictionary& p_sources) {
	svg_clip_sources = p_sources.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_svg_clip_sources() const {
	return svg_clip_sources.duplicate();
}

Ref<HTMLLayout> HTMLDocument::create_layout() const {
	Ref<HTMLLayout> result;
	result.instantiate();
	parse_into(*result.ptr());
	return result;
}

Ref<HTMLPrintLayout> HTMLDocument::create_print_layout(const Dictionary& p_options) const {
	Ref<HTMLPrintLayout> result;
	result.instantiate();
	result->prepare(*this, p_options);
	return result;
}

void HTMLDocument::parse_into(HTMLLayout& p_layout, const String& p_default_stylesheet) const {
	p_layout.parse_source(text, base_dir, stylesheet, textures, source_path, false, p_default_stylesheet, {}, local_stylesheets,
		local_fonts, image_dpi, image_orientations, svg_clip_sources, audio_streams, video_sources);
}
