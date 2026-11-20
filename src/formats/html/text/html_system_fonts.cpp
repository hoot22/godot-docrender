#include "html_system_fonts.h"

#include <godot_cpp/classes/os.hpp>

#ifdef WINDOWS_ENABLED
#include <dwrite_3.h>

#include <memory>
#include <vector>
#endif

using namespace godot;

namespace dockit {
	namespace {
#ifdef WINDOWS_ENABLED
		struct ReleaseCom {
			void operator()(IUnknown* p_object) const { p_object->Release(); }
		};

		template <class T>
		using ComRef = std::unique_ptr<T, ReleaseCom>;

		bool matches_name(IDWriteFontFace3& p_face, DWRITE_INFORMATIONAL_STRING_ID p_id, const String& p_name) {
			IDWriteLocalizedStrings* raw_names = nullptr;
			BOOL exists = false;
			if (FAILED(p_face.GetInformationalStrings(p_id, &raw_names, &exists)) || !exists) return false;
			const ComRef<IDWriteLocalizedStrings> names(raw_names);
			UINT32 index = 0;
			BOOL english = false;
			if (FAILED(names->FindLocaleName(L"en-us", &index, &english))) return false;
			if (!english) index = 0;
			UINT32 length;
			if (FAILED(names->GetStringLength(index, &length))) return false;
			std::vector<wchar_t> name(length + 1);
			return SUCCEEDED(names->GetString(index, name.data(), length + 1)) &&
				p_name.nocasecmp_to(String::utf16(reinterpret_cast<const char16_t*>(name.data()), length)) == 0;
		}

		String font_file_path(IDWriteFontFaceReference& p_face) {
			IDWriteFontFile* raw_file = nullptr;
			if (FAILED(p_face.GetFontFile(&raw_file))) return {};
			const ComRef<IDWriteFontFile> file(raw_file);
			const void* key = nullptr;
			UINT32 key_size = 0;
			IDWriteFontFileLoader* raw_loader = nullptr;
			if (FAILED(file->GetReferenceKey(&key, &key_size)) || FAILED(file->GetLoader(&raw_loader))) return {};
			const ComRef<IDWriteFontFileLoader> loader(raw_loader);
			IDWriteLocalFontFileLoader* raw_local = nullptr;
			if (FAILED(loader->QueryInterface(IID_PPV_ARGS(&raw_local)))) return {};
			const ComRef<IDWriteLocalFontFileLoader> local(raw_local);
			UINT32 length;
			if (FAILED(local->GetFilePathLengthFromKey(key, key_size, &length))) return {};
			std::vector<wchar_t> path(length + 1);
			if (FAILED(local->GetFilePathFromKey(key, key_size, path.data(), length + 1))) return {};
			return String::utf16(reinterpret_cast<const char16_t*>(path.data()), length);
		}
#else
		bool matches_font_names(const FontFile& p_font, const String& p_name) {
			const Dictionary names = p_font.get_ot_name_strings();
			String full_name;
			for (const Variant& key : names.keys()) {
				const Dictionary localized = names[key];
				const String postscript_name = localized.get("postscript_name", String());
				if (postscript_name.nocasecmp_to(p_name) == 0) return true;
				const String current_name = localized.get("full_name", String());
				const String language = key;
				if (!current_name.is_empty() && (full_name.is_empty() || language == "en" || language == "en-us")) {
					full_name = current_name;
				}
			}
			return full_name.nocasecmp_to(p_name) == 0;
		}
#endif
	}

	Ref<FontFile> load_system_font_face(const String& p_name) {
		if (p_name.is_empty()) return {};
#ifdef WINDOWS_ENABLED
		IDWriteFactory3* raw_factory = nullptr;
		if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory3),
			reinterpret_cast<IUnknown**>(&raw_factory)))) return {};
		const ComRef<IDWriteFactory3> factory(raw_factory);
		IDWriteFontSet* raw_fonts = nullptr;
		if (FAILED(factory->GetSystemFontSet(&raw_fonts))) return {};
		const ComRef<IDWriteFontSet> fonts(raw_fonts);
		const Char16String name = p_name.utf16();
		for (const auto property : { DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, DWRITE_FONT_PROPERTY_ID_FULL_NAME }) {
			const DWRITE_FONT_PROPERTY filter{ property, reinterpret_cast<const wchar_t*>(name.get_data()), nullptr };
			IDWriteFontSet* raw_matches = nullptr;
			if (FAILED(fonts->GetMatchingFonts(&filter, 1, &raw_matches))) continue;
			const ComRef<IDWriteFontSet> matches(raw_matches);
			for (UINT32 index = 0; index < matches->GetFontCount(); ++index) {
				IDWriteFontFaceReference* raw_reference = nullptr;
				if (FAILED(matches->GetFontFaceReference(index, &raw_reference))) continue;
				const ComRef<IDWriteFontFaceReference> reference(raw_reference);
				if (reference->GetLocality() != DWRITE_LOCALITY_LOCAL) continue;
				IDWriteFontFace3* raw_face = nullptr;
				if (FAILED(reference->CreateFontFace(&raw_face))) continue;
				const ComRef<IDWriteFontFace3> face(raw_face);
				const auto name_id = property == DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME ?
					DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME : DWRITE_INFORMATIONAL_STRING_FULL_NAME;
				if (!matches_name(*face, name_id, p_name)) continue;
				const String path = font_file_path(*reference);
				if (path.is_empty()) continue;
				Ref<FontFile> font;
				font.instantiate();
				if (font->load_dynamic_font(path) != OK || reference->GetFontFaceIndex() >= font->get_face_count()) continue;
				font->set_face_index(0, reference->GetFontFaceIndex());
				return font;
			}
		}
#else
		const String path = OS::get_singleton()->get_system_font_path(p_name);
		if (path.is_empty()) return {};
		Ref<FontFile> font;
		font.instantiate();
		if (font->load_dynamic_font(path) != OK) return {};
		const auto data = font->get_data();
		const auto faces = font->get_face_count();
		for (int64_t index = 0; index < faces; ++index) {
			Ref<FontFile> face;
			face.instantiate();
			face->set_data(data);
			face->set_face_index(0, index);
			if (matches_font_names(*face.ptr(), p_name)) {
				return face;
			}
		}
#endif
		return {};
	}
}
