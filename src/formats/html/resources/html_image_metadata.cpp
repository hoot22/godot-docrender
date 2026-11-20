#include "html_image_metadata.h"

#include <godot_cpp/classes/file_access.hpp>

#include <cstdint>
#include <cstring>
#include <string_view>

using namespace godot;

namespace dockit {
	namespace {
		struct MetadataView {
			FileAccess& file;
			uint64_t start;
			uint64_t size;
			bool little_endian = false;

			bool contains(uint64_t p_offset, uint64_t p_bytes) const {
				return p_offset <= size && p_bytes <= size - p_offset;
			}

			uint32_t number(uint64_t p_offset, uint32_t p_bytes) const {
				if (!contains(p_offset, p_bytes)) return 0;
				file.seek(start + p_offset);
				uint32_t result = 0;
				for (uint32_t index = 0; index < p_bytes; ++index) {
					const uint32_t shift = little_endian ? index : p_bytes - index - 1;
					result |= uint32_t(file.get_8()) << (8 * shift);
				}
				return result;
			}

			bool matches(uint64_t p_offset, std::string_view p_signature) const {
				if (!contains(p_offset, p_signature.size())) return false;
				file.seek(start + p_offset);
				const auto bytes = file.get_buffer(static_cast<int64_t>(p_signature.size()));
				return bytes.size() == static_cast<int64_t>(p_signature.size()) &&
					std::memcmp(bytes.ptr(), p_signature.data(), p_signature.size()) == 0;
			}
		};

		double square_dpi(double p_x, double p_y, double p_inches_per_unit) {
			return p_x > 0 && p_x == p_y && p_inches_per_unit > 0 ? p_x / p_inches_per_unit : 0;
		}

		HTMLImageMetadata exif_metadata(MetadataView p_tiff) {
			if (p_tiff.matches(0, std::string_view("Exif\0\0", 6))) {
				p_tiff.start += 6;
				p_tiff.size -= 6;
			}
			p_tiff.little_endian = p_tiff.matches(0, "II");
			if ((!p_tiff.little_endian && !p_tiff.matches(0, "MM")) || p_tiff.number(2, 2) != 42) return {};
			const uint64_t directory = p_tiff.number(4, 4);
			if (directory < 8) return {};
			const auto count = p_tiff.number(directory, 2);
			if (!p_tiff.contains(directory + 2, uint64_t(count) * 12)) return {};
			HTMLImageMetadata result;
			double x = 0, y = 0;
			uint32_t unit = 2; // TIFF defaults to inches.
			for (uint32_t index = 0; index < count; ++index) {
				const uint64_t entry = directory + 2 + uint64_t(index) * 12;
				const auto tag = p_tiff.number(entry, 2);
				if (tag != 0x0112 && tag != 0x011a && tag != 0x011b && tag != 0x0128) continue;
				const auto type = p_tiff.number(entry + 2, 2);
				if (p_tiff.number(entry + 4, 4) != 1) continue;
				if (tag == 0x0112 && type == 3) {
					const auto orientation = p_tiff.number(entry + 8, 2);
					if (orientation >= 1 && orientation <= 8) result.orientation = static_cast<int>(orientation);
				}
				else if (tag == 0x0128 && type == 3) {
					unit = p_tiff.number(entry + 8, 2);
				}
				else if ((tag == 0x011a || tag == 0x011b) && type == 5) {
					const uint64_t value = p_tiff.number(entry + 8, 4);
					const auto denominator = p_tiff.number(value + 4, 4);
					if (value < 8 || !denominator) continue;
					const double resolution = double(p_tiff.number(value, 4)) / denominator;
					(tag == 0x011a ? x : y) = resolution;
				}
			}
			result.dpi = square_dpi(x, y, unit == 2 ? 1.0 : unit == 3 ? 1.0 / 2.54 : 0.0);
			return result;
		}

		HTMLImageMetadata png_metadata(const MetadataView& p_png) {
			HTMLImageMetadata exif;
			double physical_dpi = 0;
			for (uint64_t chunk = 8; p_png.contains(chunk, 12);) {
				const uint64_t bytes = p_png.number(chunk, 4);
				if (!p_png.contains(chunk + 8, bytes + 4)) break;
				const MetadataView payload{ p_png.file, p_png.start + chunk + 8, bytes };
				if (p_png.matches(chunk + 4, "pHYs") && bytes == 9 && payload.number(8, 1) == 1) {
					physical_dpi = square_dpi(payload.number(0, 4), payload.number(4, 4), 1.0 / 0.0254);
				}
				else if (p_png.matches(chunk + 4, "eXIf")) {
					exif = exif_metadata(payload);
				}
				else if (p_png.matches(chunk + 4, "IDAT") || p_png.matches(chunk + 4, "IEND")) {
					break;
				}
				chunk += bytes + 12;
			}
			if (physical_dpi > 0) exif.dpi = physical_dpi;
			return exif;
		}

		HTMLImageMetadata jpeg_metadata(const MetadataView& p_jpeg) {
			HTMLImageMetadata exif;
			double jfif = 0;
			for (uint64_t marker = 2; p_jpeg.contains(marker, 2) && p_jpeg.number(marker++, 1) == 0xff;) {
				const auto kind = p_jpeg.number(marker, 1);
				if (kind == 0xff) continue;
				++marker;
				if (kind == 0xda || kind == 0xd9) break;
				if (kind == 0x01 || (kind >= 0xd0 && kind <= 0xd7)) continue;
				const uint64_t bytes = p_jpeg.number(marker, 2);
				if (bytes < 2 || !p_jpeg.contains(marker, bytes)) break;
				const MetadataView payload{ p_jpeg.file, p_jpeg.start + marker + 2, bytes - 2 };
				if (kind == 0xe1 && payload.matches(0, std::string_view("Exif\0\0", 6))) {
					const auto metadata = exif_metadata(payload);
					if (exif.dpi == 0) exif.dpi = metadata.dpi;
					if (exif.orientation == 1) exif.orientation = metadata.orientation;
				}
				else if (kind == 0xe0 && payload.size >= 14 && payload.matches(0, std::string_view("JFIF\0", 5))) {
					const auto units = payload.number(7, 1);
					jfif = square_dpi(payload.number(8, 2), payload.number(10, 2), units == 1 ? 1.0 : units == 2 ? 1.0 / 2.54 : 0.0);
				}
				marker += bytes;
			}
			if (exif.dpi == 0) exif.dpi = jfif;
			return exif;
		}

		HTMLImageMetadata webp_metadata(MetadataView p_webp) {
			p_webp.little_endian = true;
			const uint64_t riff_end = uint64_t(p_webp.number(4, 4)) + 8;
			if (riff_end < 12 || riff_end > p_webp.size) return {};
			p_webp.size = riff_end;
			for (uint64_t chunk = 12; p_webp.contains(chunk, 8);) {
				const uint64_t bytes = p_webp.number(chunk + 4, 4);
				const uint64_t aligned = bytes + (bytes % 2);
				if (!p_webp.contains(chunk + 8, aligned)) break;
				if (p_webp.matches(chunk, "EXIF")) return exif_metadata({ p_webp.file, p_webp.start + chunk + 8, bytes });
				if (p_webp.matches(chunk, "ALPH") || p_webp.matches(chunk, "VP8 ") ||
					p_webp.matches(chunk, "VP8L") || p_webp.matches(chunk, "ANMF")) break;
				chunk += aligned + 8;
			}
			return {};
		}
	}

	HTMLImageMetadata read_html_image_metadata(const String& p_path) {
		const String extension = p_path.get_extension().to_lower();
		if (extension != "png" && extension != "jpg" && extension != "jpeg" && extension != "webp") return {};
		const Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) return {};
		const MetadataView source{ *file.ptr(), 0, uint64_t(file->get_length()) };
		if (extension == "png" && source.matches(0, "\x89PNG\r\n\x1a\n")) return png_metadata(source);
		if ((extension == "jpg" || extension == "jpeg") && source.number(0, 2) == 0xffd8) return jpeg_metadata(source);
		if (extension == "webp" && source.matches(0, "RIFF") && source.matches(8, "WEBP")) return webp_metadata(source);
		return {};
	}
}
