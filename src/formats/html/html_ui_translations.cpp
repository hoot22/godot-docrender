#include "html_ui_translations.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/translation.hpp>
#include <godot_cpp/classes/translation_domain.hpp>
#include <godot_cpp/classes/translation_server.hpp>

namespace dockit {
	namespace {
		constexpr const char* translation_domain_name = "dockit.html";
		constexpr const char* translation_path = "res://addons/dockit/translations/html.zh_Hans.po";
		godot::Ref<godot::Translation> translation;
		godot::Ref<godot::TranslationDomain> domain;
	}

	void initialize_html_ui_translations() {
		if (translation.is_valid()) return;
		auto* loader = godot::ResourceLoader::get_singleton();
		auto* server = godot::TranslationServer::get_singleton();
		if (!loader || !server || !loader->exists(translation_path, "Translation")) return;
		translation = loader->load(translation_path, "Translation");
		if (translation.is_null()) return;
		domain = server->get_or_add_domain(translation_domain_name);
		if (domain.is_valid()) domain->add_translation(translation);
	}

	void shutdown_html_ui_translations() {
		if (domain.is_valid() && translation.is_valid()) domain->remove_translation(translation);
		domain.unref();
		translation.unref();
	}

	godot::String html_ui_translate(const godot::String& p_message, const godot::StringName& p_context) {
		return domain.is_valid() ? godot::String(domain->translate(godot::StringName(p_message), p_context)) : p_message;
	}
}
