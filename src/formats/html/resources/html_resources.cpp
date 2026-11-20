#include "html_resources.h"
#include "formats/http_url.h"
#include "formats/remote_images.h"

#include "formats/html/markup/html_markup.h"
#include "html_resource_paths.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <litehtml/html.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>

using namespace godot;

namespace dockit {
	namespace {

		struct ImageFragment {
			std::array<int, 4> coordinates;
			bool percent = false;

			Rect2i region(const Vector2i& p_size) const {
				Vector2i origin;
				Vector2i end;
				for (int axis = 0; axis < 2; ++axis) {
					const int64_t start = coordinates[axis];
					const int64_t stop = start + coordinates[axis + 2];
					const int64_t limit = p_size[axis];
					origin[axis] = static_cast<int32_t>(std::min(limit, percent ? (start * limit) / 100 : start));
					end[axis] = static_cast<int32_t>(std::min(limit, percent ? (stop * limit + 99) / 100 : stop));
				}
				return Rect2i(origin, end - origin);
			}
		};

		std::optional<ImageFragment> parse_image_fragment(const String& p_fragment) {
			if (!p_fragment.begins_with("xywh=")) return std::nullopt;
			const CharString bytes = p_fragment.utf8();
			std::string_view value(bytes.get_data() + 5, bytes.length() - 5);
			ImageFragment fragment;
			if (value.rfind("percent:", 0) == 0) {
				fragment.percent = true;
				value.remove_prefix(8);
			}
			else if (value.rfind("pixel:", 0) == 0) {
				value.remove_prefix(6);
			}
			for (size_t index = 0; index < fragment.coordinates.size(); ++index) {
				const auto comma = value.find(',');
				const auto field = value.substr(0, comma);
				const bool last = index + 1 == fragment.coordinates.size();
				if (field.empty() || field.front() < '0' || field.front() > '9' ||
					last != (comma == std::string_view::npos)) return std::nullopt;
				const auto* end = field.data() + field.size();
				const auto parsed = std::from_chars(field.data(), end, fragment.coordinates[index]);
				if (parsed.ec != std::errc() || parsed.ptr != end) return std::nullopt;
				if (!last) value.remove_prefix(comma + 1);
			}
			if (fragment.coordinates[2] == 0 || fragment.coordinates[3] == 0) return std::nullopt;
			if (fragment.percent && std::any_of(fragment.coordinates.begin(), fragment.coordinates.end(),
				[](int p_coordinate) { return p_coordinate > 100; })) return std::nullopt;
			return fragment;
		}

		Ref<Texture2D> orient_texture(const Ref<Texture2D>& p_texture, litehtml::css_image_orientation p_orientation) {
			using orientation = litehtml::css_image_orientation;
			if (p_texture.is_null() || p_orientation == orientation::identity) return p_texture;
			const Ref<Image> source = p_texture->get_image();
			if (source.is_null() || source->is_empty()) return {};
			const Ref<Image> pixels = source->duplicate();
			if (pixels->is_compressed() && pixels->decompress() != OK) return {};
			const bool mipmaps = pixels->has_mipmaps();
			if (mipmaps) pixels->clear_mipmaps();
			Vector2i nominal_size(p_texture->get_size());
			switch (p_orientation) {
				case orientation::rotate_90:
				case orientation::transpose:
					pixels->rotate_90(CLOCKWISE);
					nominal_size = Vector2i(nominal_size.y, nominal_size.x);
					break;
				case orientation::rotate_270:
				case orientation::transverse:
					pixels->rotate_90(COUNTERCLOCKWISE);
					nominal_size = Vector2i(nominal_size.y, nominal_size.x);
					break;
				case orientation::rotate_180:
					pixels->rotate_180();
					break;
				default: break;
			}
			if (p_orientation == orientation::flip_x || p_orientation == orientation::transpose ||
				p_orientation == orientation::transverse) pixels->flip_x();
			else if (p_orientation == orientation::flip_y) pixels->flip_y();
			if (mipmaps) pixels->generate_mipmaps();
			const Ref<ImageTexture> texture = ImageTexture::create_from_image(pixels);
			if (texture.is_valid() && nominal_size != pixels->get_size()) texture->set_size_override(nominal_size);
			return texture;
		}

		Ref<Texture2D> crop_texture(const Ref<Texture2D>& p_texture, const ImageFragment& p_fragment) {
			if (p_texture.is_null()) return {};
			const auto nominal_size = Vector2i(p_texture->get_size());
			const Rect2i nominal_region = p_fragment.region(nominal_size);
			if (!nominal_region.has_area()) return {};
			if (nominal_region == Rect2i(Vector2i(), nominal_size)) return p_texture;
			const Ref<Image> pixels = p_texture->get_image();
			if (pixels.is_null() || pixels->is_empty()) return {};
			const Rect2i region = p_fragment.region(pixels->get_size());
			if (!region.has_area()) return {};
			const bool mipmaps = pixels->has_mipmaps();
			if (pixels->is_compressed() && pixels->decompress() != OK) return {};
			const Ref<Image> cropped = pixels->get_region(region);
			if (mipmaps) cropped->generate_mipmaps();
			return ImageTexture::create_from_image(cropped);
		}
	}

	bool is_html_stylesheet_link(const char* p_rel) {
		if (!p_rel) {
			return false;
		}
		constexpr std::string_view whitespace = " \t\n\r\f";
		const std::string_view rel(p_rel);
		bool stylesheet = false;
		for (auto start = rel.find_first_not_of(whitespace); start != std::string_view::npos;) {
			const auto end = rel.find_first_of(whitespace, start);
			const auto token = rel.substr(start, end - start);
			const auto is = [token](std::string_view p_name) {
				return token.size() == p_name.size() &&
					litehtml::t_strncasecmp(token.data(), p_name.data(), token.size()) == 0;
			};
			if (is("alternate")) {
				return false;
			}
			stylesheet |= is("stylesheet");
			start = rel.find_first_not_of(whitespace, end);
		}
		return stylesheet;
	}

	static std::optional<String> read_html_source_text(const String& p_path) {
		const Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			return std::nullopt;
		}
		const uint64_t length = file->get_length();
		if (length > MAX_HTML_SOURCE_BYTES) {
			return std::nullopt;
		}
		const PackedByteArray bytes = file->get_buffer(length);
		return static_cast<uint64_t>(bytes.size()) == length ? decode_html_source_text(bytes) : std::nullopt;
	}

	std::optional<String> read_html_stylesheet(const String& p_path) {
		return read_html_source_text(p_path);
	}

	std::optional<String> read_html_svg_source(const String& p_path) {
		return read_html_source_text(p_path);
	}

	HTMLResources::HTMLResources(const String& p_base_dir, const Dictionary& p_textures, const Dictionary& p_local_stylesheets,
		const Dictionary& p_local_fonts, const Dictionary& p_image_dpi, const Dictionary& p_image_orientations,
		const Dictionary& p_svg_clip_sources) :
		fallback_base(make_html_resource_base(p_base_dir)), document_base(fallback_base),
		textures(p_textures), local_stylesheets(p_local_stylesheets), local_fonts(p_local_fonts),
		image_dpi(p_image_dpi), image_orientations(p_image_orientations), svg_clip_sources(p_svg_clip_sources) {
	}

	void HTMLResources::set_source_path(const String& p_path) {
		if (is_http_url(p_path)) {
			fallback_base = { p_path, resolve_http_url(".", p_path) };
		}
		else {
			fallback_base = make_html_resource_base(fallback_base.directory, p_path);
		}
		document_base = fallback_base;
	}

	void HTMLResources::set_document_base_href(const char* p_href) {
		document_base = resolve_html_document_base(fallback_base, p_href);
	}

	std::string_view HTMLResources::stylesheet_path(std::string_view p_css_base) {
		const auto separator = p_css_base.find_last_of('\n');
		return separator == std::string_view::npos ? p_css_base : p_css_base.substr(separator + 1);
	}

	void HTMLResources::reset_stylesheet_reads() {
		stylesheet_reads = 0;
		pending_stylesheets.clear();
	}

	String HTMLResources::resolve_remote_resource(const String& p_url, std::string_view p_css_base) const {
		const auto path = stylesheet_path(p_css_base);
		const String base = path.empty() ? document_base.uri : String::utf8(path.data(), path.size());
		return resolve_http_url(p_url, base);
	}

	String HTMLResources::resolve_stylesheet_source(const String& p_url, std::string_view p_css_base) const {
		if (p_url.strip_edges().is_empty()) return {};
		const String remote = resolve_remote_resource(p_url, p_css_base);
		if (!remote.is_empty()) return http_request_url(remote);
		const String css_base = p_css_base.empty() ? String() : String::utf8(p_css_base.data(), p_css_base.size());
		return resolve_html_stylesheet_path(p_url, get_url_dir(css_base.utf8().get_data()));
	}

	void HTMLResources::set_remote_stylesheet_callback(const Callable& p_callback) {
		remote_stylesheet_callback = p_callback;
	}

	bool HTMLResources::receive_remote_stylesheet(const String& p_url, const String& p_final_url, const String& p_text) {
		if (!pending_stylesheets.erase(p_url)) return false;
		if (p_final_url.is_empty()) {
			++failed_remote_resources;
			return true;
		}
		remote_stylesheets.insert(p_url, {RemoteStylesheetStatus::ready, p_final_url, p_text});
		return true;
	}

	String HTMLResources::get_url_dir(const char* p_css_base) const {
		if (!p_css_base || !*p_css_base) {
			return document_base.directory;
		}
		const auto path = stylesheet_path(p_css_base);
		const String url = String::utf8(path.data(), path.size());
		return is_http_url(url) ? resolve_http_url(".", url) : url.get_base_dir();
	}

	String HTMLResources::resolve_svg_clip_source(std::string_view p_url, const char* p_css_base) const {
		return resolve_html_svg_clip_path(String::utf8(p_url.data(), p_url.size()), get_url_dir(p_css_base));
	}

	std::optional<String> HTMLResources::get_svg_clip_source(const String& p_path) const {
		const Variant stored = svg_clip_sources.get(p_path, Variant());
		return stored.get_type() == Variant::STRING ? std::optional<String>(stored) : read_html_svg_source(p_path);
	}

	void HTMLResources::import_css(std::string& r_text, const std::string& p_url, std::string& r_css_base) {
		r_text.clear();
		const String path = resolve_stylesheet_source(String::utf8(p_url.c_str()), r_css_base);
		const auto parents = String::utf8(r_css_base.c_str()).split("\n", false);
		// litehtml carries this base string into nested imports and declarations.
		if (path.is_empty() || parents.has(path) || parents.size() >= 16 || stylesheet_reads >= 128) {
			return;
		}
		++stylesheet_reads;
		String source = path;
		std::optional<String> file;
		if (is_http_url(path)) {
			const auto* cached = remote_stylesheets.getptr(path);
			const auto sheet = cached ? *cached : request_remote_stylesheet(path, remote_stylesheet_callback);
			if (sheet.status == RemoteStylesheetStatus::loading) pending_stylesheets.insert(path);
			if (sheet.status != RemoteStylesheetStatus::ready || parents.has(sheet.url)) return;
			if (!cached) remote_stylesheets.insert(path, sheet);
			source = sheet.url;
			file = sheet.text;
		} else {
			const Variant stored = local_stylesheets.get(path, Variant());
			file = stored.get_type() == Variant::STRING ? std::optional<String>(stored) : read_html_stylesheet(path);
		}
		if (!file) {
			return;
		}
		if (!r_css_base.empty()) {
			r_css_base += '\n';
		}
		r_css_base += source.utf8().get_data();
		r_text = file->utf8().get_data();
	}

	String HTMLResources::resolve_image_source(std::string_view p_source) const {
		const String source = String::utf8(p_source.data(), p_source.size()).strip_edges();
		if (is_remote_image_url(source)) return source;
		if (source.begins_with("res://") || source.begins_with("user://") || source.begins_with("uid://")) {
			const String path = resolve_html_image_path(source, document_base.directory);
			const int64_t fragment = source.find("#");
			return !path.is_empty() && fragment >= 0 ? path + source.substr(fragment) : path;
		}
		if (is_http_url(document_base.uri)) return resolve_http_url(source, document_base.uri);
		const String path = resolve_html_image_path(source, document_base.directory);
		const int64_t fragment = source.find("#");
		return !path.is_empty() && fragment >= 0 ? path + source.substr(fragment) : path;
	}

	bool HTMLResources::is_vector_image(const char* p_src, const char* p_css_base) const {
		const String path = resolve_html_image_path(String::utf8(p_src), get_url_dir(p_css_base));
		return path.get_extension().nocasecmp_to("svg") == 0;
	}

	const HTMLImageMetadata& HTMLResources::get_image_metadata(const String& p_path) {
		if (const auto* cached = image_metadata.getptr(p_path)) return *cached;
		auto metadata = read_html_image_metadata(p_path);
		const Variant stored_dpi = image_dpi.get(p_path, Variant());
		if (stored_dpi.get_type() == Variant::FLOAT || stored_dpi.get_type() == Variant::INT) {
			const double value = stored_dpi;
			if (value > 0 && std::isfinite(value)) metadata.dpi = value;
		}
		const Variant stored_orientation = image_orientations.get(p_path, Variant());
		if (stored_orientation.get_type() == Variant::INT) {
			const int64_t value = stored_orientation;
			if (value >= 1 && value <= 8) metadata.orientation = static_cast<int>(value);
		}
		return image_metadata.insert(p_path, metadata)->value;
	}

	double HTMLResources::get_image_dpi(const char* p_src, const char* p_css_base) {
		const String path = resolve_html_image_path(String::utf8(p_src), get_url_dir(p_css_base));
		return path.is_empty() ? 0 : get_image_metadata(path).dpi;
	}

	Ref<Texture2D> HTMLResources::load_texture(const char* p_src, const char* p_css_base,
		litehtml::css_image_orientation p_orientation, bool p_affects_layout) {
		using orientation = litehtml::css_image_orientation;
		const String original = String::utf8(p_src).strip_edges();
		if (original.is_empty() || original.begins_with("#")) return {};
		const String resolved = resolve_remote_resource(original, p_css_base ? p_css_base : "");
		const String& source = resolved.is_empty() ? original : resolved;
		const bool remote = !resolved.is_empty();
		const String path = remote ? remote_image_request_url(source) : resolve_html_image_path(source, get_url_dir(p_css_base));
		if (path.is_empty()) return {};
		Ref<Texture2D> remote_texture;
		if (remote) {
			if (const auto* cached = images.getptr(path)) remote_texture = *cached;
			else {
				const auto result = request_remote_image(path, remote_image_callback);
				if (result.status == RemoteImageStatus::loading) {
					(p_affects_layout && !recording_paint ? pending_layout_images : pending_paint_images).insert(path);
				}
				remote_texture = result.texture;
				if (remote_texture.is_null()) return {};
				images.insert(path, remote_texture);
			}
		}
		if (p_orientation == orientation::from_image)
			p_orientation = remote ? orientation::identity : static_cast<orientation>(get_image_metadata(path).orientation);
		else if (p_orientation == orientation::none)
			p_orientation = orientation::identity;

		const int64_t separator = source.find("#");
		const String variant_key = String::num_int64(static_cast<int>(p_orientation)) + ":" + path;
		const auto load_base = [&]() { return remote ? remote_texture : load_texture_file(path); };
		const auto load_oriented = [&]() {
			if (p_orientation == orientation::identity) return load_base();
			if (const auto* cached = images.getptr(variant_key)) return *cached;
			const auto texture = orient_texture(load_base(), p_orientation);
			images.insert(variant_key, texture);
			return texture;
		};
		if (separator < 0) return load_oriented();

		const String fragment = decode_html_fragment(source.substr(separator + 1));
		const auto selection = parse_image_fragment(fragment);
		if (remote && !selection) return load_oriented();
		// Include the path length so fragments cannot alias file names containing an escaped hash.
		const String key = String::num_int64(path.length()) + "/" + variant_key + "#" + fragment;
		if (const auto* cached = images.getptr(key)) return *cached;
		Ref<Texture2D> texture;
		if (selection) texture = crop_texture(load_oriented(), *selection);
		images.insert(key, texture);
		return texture;
	}

	void HTMLResources::set_remote_image_callback(const Callable& p_callback) {
		remote_image_callback = p_callback;
	}

	void HTMLResources::begin_image_layout() {
		pending_layout_images.clear();
	}

	int HTMLResources::pending_resource_count() const {
		return pending_layout_images.size() + pending_paint_images.size() + pending_stylesheets.size();
	}

	void HTMLResources::begin_image_paint() {
		pending_paint_images.clear();
		recording_paint = true;
	}

	void HTMLResources::end_image_paint() {
		recording_paint = false;
	}

	HTMLResources::ImageUpdate HTMLResources::receive_remote_image(const String& p_url, const Ref<Texture2D>& p_texture) {
		const bool layout = pending_layout_images.erase(p_url);
		const bool paint = pending_paint_images.erase(p_url);
		if (!layout && !paint) return ImageUpdate::none;
		if (p_texture.is_null()) {
			++failed_remote_resources;
			return ImageUpdate::none;
		}
		images.insert(p_url, p_texture);
		return layout ? ImageUpdate::layout : paint ? ImageUpdate::paint : ImageUpdate::none;
	}

	Ref<Texture2D> load_html_image_file(const String& p_path) {
		auto* loader = ResourceLoader::get_singleton();
		if (loader->exists(p_path, "Texture2D")) {
			const Ref<Texture2D> texture = loader->load(p_path, "Texture2D");
			if (texture.is_valid()) return texture;
		}
		const auto extension = p_path.get_extension().to_lower();
		if (!p_path.begins_with("user://") || !litehtml::is_one_of(extension, "png", "jpg", "jpeg", "webp", "svg") ||
			!FileAccess::file_exists(p_path)) return {};
		const auto image = Image::load_from_file(p_path);
		return image.is_valid() && !image->is_empty() ? ImageTexture::create_from_image(image) : Ref<ImageTexture>();
	}

	Ref<Texture2D> HTMLResources::load_texture_file(const String& p_path) {
		if (const auto* cached = images.getptr(p_path)) return *cached;
		Ref<Texture2D> texture = textures.get(p_path, Variant());
		if (texture.is_null()) texture = load_html_image_file(p_path);
		images.insert(p_path, texture);
		return texture;
	}

	Ref<FontFile> HTMLResources::load_font(const char* p_src, const char* p_css_base) {
		const String source = String::utf8(p_src);
		const String path = resolve_html_font_path(source, get_url_dir(p_css_base));
		if (path.is_empty()) return {};
		const int64_t separator = source.find("#");
		if (separator < 0) return load_font_file(path);
		const String name = decode_html_fragment(source.substr(separator + 1));
		if (name.is_empty() || name.length() > 63) return {};
		constexpr std::u32string_view excluded = U"[](){}<>/%";
		for (int64_t index = 0; index < name.length(); ++index) {
			const char32_t character = name[index];
			if (character < 33 || character > 126 || excluded.find(character) != std::u32string_view::npos) return {};
		}
		const String key = path + String("#") + name;
		if (const auto* cached = fonts.getptr(key)) return *cached;
		const Ref<FontFile> base = load_font_file(path);
		Ref<FontFile> selected;
		if (base.is_valid()) {
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			const auto data = base->get_data();
			const auto count = base->get_face_count();
			for (int64_t index = 0; index < count; ++index) {
				// Read names from a fresh face, so the text server never reuses metadata from another index.
				const RID probe = server->create_font();
				server->font_set_data(probe, data);
				server->font_set_face_index(probe, index);
				const Dictionary names = server->font_get_ot_name_strings(probe);
				server->free_rid(probe);
				bool matches = false;
				for (const Variant& language : names.keys()) {
					const Dictionary localized = names[language];
					const String postscript_name = localized.get("postscript_name", String());
					if (name == postscript_name) {
						matches = true;
						break;
					}
				}
				if (matches) {
					selected = base->duplicate();
					selected->set_face_index(0, index);
					break;
				}
			}
		}
		fonts.insert(key, selected);
		return selected;
	}

	Ref<FontFile> HTMLResources::load_font_file(const String& p_path) {
		if (const auto* cached = fonts.getptr(p_path)) return *cached;
		Ref<FontFile> font = local_fonts.get(p_path, Variant());
		ResourceLoader* loader = ResourceLoader::get_singleton();
		if (font.is_null() && loader->exists(p_path, "FontFile")) {
			font = loader->load(p_path, "FontFile");
		}
		const auto extension = p_path.get_extension().to_lower();
		if (font.is_null() && p_path.begins_with("user://") &&
			litehtml::is_one_of(extension, "ttf", "otf", "ttc", "otc", "woff", "woff2") && FileAccess::file_exists(p_path)) {
			font.instantiate();
			if (font->load_dynamic_font(p_path) != OK) font.unref();
		}
		if (font.is_valid() && !font->get_data().is_empty() && font->get_face_count() > 0) {
			// Let shaping try the CSS font stack before using implicit system fallbacks.
			font = font->duplicate();
			font->set_allow_system_fallback(false);
			font->set_fallbacks(TypedArray<Font>());
		}
		else {
			font.unref();
		}
		fonts.insert(p_path, font);
		return font;
	}
} // namespace dockit
