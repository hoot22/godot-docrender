#include "html_label.h"

#include "formats/html/elements/html_input_node.h"
#include "formats/html/html_ui_translations.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>

using namespace godot;
using dockit::HTMLInputNode;
namespace calendar = dockit::input_time;

namespace {
	PackedStringArray file_filters(const String& p_accept) {
		PackedStringArray filters;
		for (const String& item : p_accept.split(",", false)) {
			const String type = item.strip_edges().to_lower();
			if (type.contains(";") || type.contains("\n") || type.contains("\r")) continue;
			if (type.begins_with(".") && type.length() > 1 && !type.contains("/") && !type.contains("\\") &&
				!type.contains("*") && !type.contains("?")) {
				filters.append("*" + type + " ; " + type);
			} else if (type == "image/*") {
				filters.append("*.png,*.jpg,*.jpeg,*.webp,*.gif,*.bmp,*.svg,*.avif,*.heic ; " +
					dockit::html_ui_translate("Images", "file_filter.category") + " ; image/*");
			} else if (type == "audio/*") {
				filters.append("*.mp3,*.wav,*.ogg,*.flac,*.aac,*.m4a,*.opus ; " +
					dockit::html_ui_translate("Audio", "file_filter.category") + " ; audio/*");
			} else if (type == "video/*") {
				filters.append("*.mp4,*.webm,*.ogv,*.mov,*.mkv,*.avi ; " +
					dockit::html_ui_translate("Video", "file_filter.category") + " ; video/*");
			} else if (type.contains("/")) {
				if (type == "text/plain") {
					filters.append("*.txt,*.text ; " + dockit::html_ui_translate("Plain text", "file_filter.category") + " ; text/plain");
					continue;
				}
				if (type == "image/jpeg") {
					filters.append("*.jpg,*.jpeg,*.jpe ; " + dockit::html_ui_translate("JPEG", "file_filter.category") + " ; image/jpeg");
					continue;
				}
				if (type == "audio/mpeg") {
					filters.append("*.mp3,*.mp2 ; " + dockit::html_ui_translate("MPEG audio", "file_filter.category") + " ; audio/mpeg");
					continue;
				}
				const auto slash = type.find("/");
				const auto extension = type.substr(slash + 1).get_slice("+", 0);
				if (!extension.is_empty()) filters.append("*." + extension + " ; " + type + " ; " + type);
			}
		}
		return filters;
	}
}

bool HTMLLabel::input_picker_valid() const {
	const auto input = picker_input.is_valid() ? picker_input->get_input() : nullptr;
	return input && picker_input->adoption_version == picker_adoption && input->type_name() == picker_type &&
		input->editor.get_reset_version() == picker_reset && is_inside_tree() && is_visible_in_tree() &&
		!source_dirty && layout->is_element_connected(picker_input) && layout->allows_modal_interaction(picker_input) &&
		!dockit::forms::is_disabled(*input) && !(input->supports_readonly() && input->get_attr("readonly"));
}

bool HTMLLabel::is_time_picker_open() const {
	return time_picker.kind != calendar::Kind::none;
}

bool HTMLLabel::is_canvas_picker_open() const {
	return picker_type == "color" || is_time_picker_open();
}

bool HTMLLabel::place_input_picker_canvas(Control& p_surface, const Vector2& p_size) {
	const auto anchor = layout->get_element_scroll_bounds(picker_input);
	if (!input_picker_valid() || !anchor || !get_visible_rect().intersects(*anchor)) {
		close_input_picker();
		return false;
	}
	const Transform2D inverse = get_global_transform_with_canvas().affine_inverse();
	const Rect2 owner_box = inverse.xform(canvas->get_global_transform_with_canvas().xform(*anchor));
	const Rect2 limits = inverse.xform(get_viewport()->get_visible_rect());
	if (!limits.has_area()) {
		close_input_picker();
		return false;
	}
	const auto scale = std::min<real_t>({ 1, limits.size.x / p_size.x, limits.size.y / p_size.y });
	const Vector2 visible_size = p_size * scale;
	Vector2 position(owner_box.position.x, owner_box.get_end().y);
	if (position.y + visible_size.y > limits.get_end().y &&
		owner_box.position.y - limits.position.y > limits.get_end().y - position.y) {
		position.y = owner_box.position.y - visible_size.y;
	}
	position = position.clamp(limits.position, limits.position.max(limits.get_end() - visible_size));
	p_surface.set_position(position.floor());
	p_surface.set_size(p_size);
	p_surface.set_scale(Vector2(scale, scale));
	return true;
}

void HTMLLabel::close_input_picker() {
	const bool closes_color = picker_type == "color";
	const bool closes_clock = is_time_picker_open();
	if (closes_color) {
		stop_color_eyedropper();
		if (const auto node = picker_input->get_input()) node->set_color_preview(std::nullopt);
		if (color_picker_canvas) color_picker_canvas->hide();
		color_picker = {};
		layout->repaint_input();
	}
	if (closes_clock) {
		if (const auto node = picker_input->get_input()) node->set_time_preview(std::nullopt);
		if (time_picker_canvas) time_picker_canvas->hide();
		time_picker = {};
		layout->repaint_input();
	}
	++picker_sequence;
	picker_input.unref();
	picker_type = String();
	if ((closes_color || closes_clock) && canvas) canvas->update_focus_mode();
	if (auto* dialog = std::exchange(file_dialog, nullptr)) {
		dialog->hide();
		dialog->queue_free();
	}
}

void HTMLLabel::on_input_picker_closed(uint64_t p_sequence) {
	if (p_sequence == picker_sequence) close_input_picker();
}

Error HTMLLabel::open_input_picker(const Ref<HTMLInputElement>& p_element) {
	if (!is_inside_tree() || !is_visible_in_tree() || source_dirty || !layout->is_element_connected(p_element) ||
		!layout->is_focusable(p_element) || !layout->allows_modal_interaction(p_element)) return ERR_INVALID_PARAMETER;
	const auto node = p_element->get_input();
	if (!node || !node->has_picker() || (node->supports_readonly() && node->get_attr("readonly"))) return ERR_UNAVAILABLE;
	if (picker_type == "file" && input_picker_valid()) return picker_input == p_element ? OK : ERR_BUSY;
	const auto kind = node->type();
	const auto date_kind = node->time_kind();
	const ObjectID id(get_instance_id());
	if (!focus_element(p_element) || !ObjectDB::get_instance(id)) return ERR_UNAVAILABLE;
	if (!layout->is_element_connected(p_element) || p_element->get_input() != node || node->type() != kind) return ERR_UNAVAILABLE;
	if (kind != HTMLInputNode::Type::FILE && kind != HTMLInputNode::Type::COLOR && date_kind == calendar::Kind::none) {
		return open_suggestion_menu(p_element) ? OK : ERR_UNAVAILABLE;
	}
	close_select_menu();
	canvas->close_input_menu();
	close_input_picker();
	picker_input = p_element;
	picker_type = node->type_name();
	picker_adoption = p_element->adoption_version;
	picker_reset = node->editor.get_reset_version();
	if (kind == HTMLInputNode::Type::FILE) return open_file_picker(picker_sequence);
	if (kind == HTMLInputNode::Type::COLOR) return show_color_picker();
	return show_clock_picker();
}

Error HTMLLabel::open_file_picker(uint64_t p_sequence) {
	const auto input = picker_input->get_input();
	auto* display = DisplayServer::get_singleton();
	if (display->get_name() == "headless") {
		close_input_picker();
		return ERR_UNAVAILABLE;
	}
	const bool multiple = input->get_attr("multiple");
	const auto filters = file_filters(String::utf8(input->get_attr("accept", "")));
	if (!get_window()->is_embedded() && display->has_feature(DisplayServer::FEATURE_NATIVE_DIALOG_FILE)) {
		const auto mode = multiple ? DisplayServer::FILE_DIALOG_MODE_OPEN_FILES : DisplayServer::FILE_DIALOG_MODE_OPEN_FILE;
		const auto title = dockit::html_ui_translate("Choose File", "file_dialog.title");
		const auto result = display->file_dialog_show(title, "", "", false, mode, filters,
			callable_mp(this, &HTMLLabel::native_files_selected).bind(p_sequence), get_window()->get_window_id());
		if (result == OK) return OK;
	}
	file_dialog = memnew(FileDialog);
	file_dialog->set_name("_HTMLFilePicker");
	file_dialog->set_title(dockit::html_ui_translate("Choose File", "file_dialog.title"));
	file_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
	file_dialog->set_file_mode(multiple ? FileDialog::FILE_MODE_OPEN_FILES : FileDialog::FILE_MODE_OPEN_FILE);
	file_dialog->set_filters(filters);
	file_dialog->connect("files_selected", callable_mp(this, &HTMLLabel::files_selected).bind(p_sequence));
	file_dialog->connect("file_selected", callable_mp(this, &HTMLLabel::file_selected).bind(p_sequence));
	file_dialog->connect("canceled", callable_mp(this, &HTMLLabel::on_input_picker_closed).bind(p_sequence));
	add_child(file_dialog, false, INTERNAL_MODE_BACK);
	file_dialog->popup_centered_ratio(0.68);
	return OK;
}

void HTMLLabel::native_files_selected(bool p_accepted, const PackedStringArray& p_paths, int, uint64_t p_sequence) {
	if (p_accepted) files_selected(p_paths, p_sequence);
	else on_input_picker_closed(p_sequence);
}

void HTMLLabel::file_selected(const String& p_path, uint64_t p_sequence) {
	files_selected(PackedStringArray(Array::make(p_path)), p_sequence);
}

void HTMLLabel::files_selected(const PackedStringArray& p_paths, uint64_t p_sequence) {
	if (p_sequence != picker_sequence) return;
	if (!input_picker_valid() || p_paths.is_empty()) {
		close_input_picker();
		return;
	}
	const auto element = picker_input;
	const auto input = element->get_input();
	TypedArray<HTMLInputFile> selected;
	const auto old_files = input->get_files();
	for (const String& path : p_paths) {
		const auto file = HTMLInputFile::open(path);
		if (file.is_null()) {
			close_input_picker();
			return;
		}
		selected.append(file);
		if (!input->get_attr("multiple")) break;
	}
	close_input_picker();
	if (old_files.size() == selected.size()) {
		bool same = true;
		for (int64_t i = 0; i < selected.size(); ++i) {
			const Ref<HTMLInputFile> old_file = old_files[i], new_file = selected[i];
			same &= old_file->file->get_path_absolute() == new_file->file->get_path_absolute();
		}
		if (same) return;
	}
	if (!input->set_files(selected)) return;
	const ObjectID id(get_instance_id());
	const auto reset = input->editor.get_reset_version();
	canvas->change_input(element, false);
	if (auto* label = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(id)); label &&
		label->layout->is_element_connected(element) && element->get_input() == input &&
		input->type() == HTMLInputNode::Type::FILE && input->editor.get_reset_version() == reset) {
		label->emit_signal("input_committed", element);
	}
}

void HTMLLabel::commit_time_picker() {
	if (!input_picker_valid()) {
		close_input_picker();
		return;
	}
	const auto kind = picker_input->get_input()->time_kind();
	const auto value = calendar::from_parts(kind, picker_date);
	if (value) commit_picker_value(calendar::format(kind, *value));
}

void HTMLLabel::commit_picker_value(const String& p_value) {
	if (!input_picker_valid()) {
		close_input_picker();
		return;
	}
	const auto element = picker_input;
	const auto input = element->get_input();
	close_input_picker();
	const String clean = input->sanitize_value(p_value);
	if (clean == input->get_value()) return;
	input->editor.set_selection(0, input->editor.get_value().length());
	if (!input->editor.insert(clean)) return;
	const ObjectID id(get_instance_id());
	canvas->change_input(element, false);
	if (auto* label = Object::cast_to<HTMLLabel>(ObjectDB::get_instance(id))) label->canvas->commit_input(element);
}
