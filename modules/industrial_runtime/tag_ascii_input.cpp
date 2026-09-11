#include "tag_ascii_input.h"

#include "input_session_manager.h"
#include "tag_widget_util.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "scene/main/scene_tree.h"

void TagAsciiInput::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagAsciiInput::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagAsciiInput::get_tag_name);
	ClassDB::bind_method(D_METHOD("_validate_input_session_text", "text"), &TagAsciiInput::_validate_input_session_text);
	ClassDB::bind_method(D_METHOD("_commit_input_session_text", "text"), &TagAsciiInput::_commit_input_session_text);
	ClassDB::bind_method(D_METHOD("_cancel_input_session"), &TagAsciiInput::_cancel_input_session);
	ClassDB::bind_method(D_METHOD("_begin_input_session"), &TagAsciiInput::_begin_input_session);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagAsciiInput::_on_tag_changed);

	ClassDB::bind_method(D_METHOD("set_keypad_id", "id"), &TagAsciiInput::set_keypad_id);
	ClassDB::bind_method(D_METHOD("get_keypad_id"), &TagAsciiInput::get_keypad_id);
	ClassDB::bind_method(D_METHOD("set_keypad_scene_override", "scene"), &TagAsciiInput::set_keypad_scene_override);
	ClassDB::bind_method(D_METHOD("get_keypad_scene_override"), &TagAsciiInput::get_keypad_scene_override);
	ClassDB::bind_method(D_METHOD("set_presentation_mode", "mode"), &TagAsciiInput::set_presentation_mode);
	ClassDB::bind_method(D_METHOD("get_presentation_mode"), &TagAsciiInput::get_presentation_mode);
	ClassDB::bind_method(D_METHOD("set_hide_keypad_title", "v"), &TagAsciiInput::set_hide_keypad_title);
	ClassDB::bind_method(D_METHOD("get_hide_keypad_title"), &TagAsciiInput::get_hide_keypad_title);
	ClassDB::bind_method(D_METHOD("set_keypad_anchor", "v"), &TagAsciiInput::set_keypad_anchor);
	ClassDB::bind_method(D_METHOD("get_keypad_anchor"), &TagAsciiInput::get_keypad_anchor);
	ClassDB::bind_method(D_METHOD("set_keypad_screen_cell", "v"), &TagAsciiInput::set_keypad_screen_cell);
	ClassDB::bind_method(D_METHOD("get_keypad_screen_cell"), &TagAsciiInput::get_keypad_screen_cell);
	ClassDB::bind_method(D_METHOD("set_keypad_side", "v"), &TagAsciiInput::set_keypad_side);
	ClassDB::bind_method(D_METHOD("get_keypad_side"), &TagAsciiInput::get_keypad_side);
	ClassDB::bind_method(D_METHOD("set_keypad_align", "v"), &TagAsciiInput::set_keypad_align);
	ClassDB::bind_method(D_METHOD("get_keypad_align"), &TagAsciiInput::get_keypad_align);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "keypad_id"), "set_keypad_id", "get_keypad_id");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "keypad_scene_override", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_keypad_scene_override", "get_keypad_scene_override");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "presentation_mode", PROPERTY_HINT_ENUM, "system,popup,fixed,direct_window"), "set_presentation_mode", "get_presentation_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_keypad_title"), "set_hide_keypad_title", "get_hide_keypad_title");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "keypad_anchor", PROPERTY_HINT_ENUM, "center,screen,control"), "set_keypad_anchor", "get_keypad_anchor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "keypad_screen_cell", PROPERTY_HINT_RANGE, "0,8,1"), "set_keypad_screen_cell", "get_keypad_screen_cell");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "keypad_side", PROPERTY_HINT_ENUM, "top,bottom,left,right"), "set_keypad_side", "get_keypad_side");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "keypad_align", PROPERTY_HINT_ENUM, "start,center,end"), "set_keypad_align", "get_keypad_align");
}

void TagAsciiInput::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (!tag_name.is_empty()) {
		tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagAsciiInput::_on_tag_changed));
	}
}

void TagAsciiInput::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		set_editable(false);
		set_virtual_keyboard_enabled(false);
		set_focus_mode(FOCUS_ALL);
		if (get_placeholder().is_empty()) {
			set_placeholder("Tap to enter...");
		}
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagAsciiInput::_on_tag_changed));
		}
	}
}

void TagAsciiInput::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mouse_button = p_event;
	if (mouse_button.is_valid() && mouse_button->is_pressed() && mouse_button->get_button_index() == MouseButton::LEFT) {
		call_deferred(SNAME("_begin_input_session"));
		accept_event();
		return;
	}

	Ref<InputEventScreenTouch> screen_touch = p_event;
	if (screen_touch.is_valid() && screen_touch->is_pressed()) {
		call_deferred(SNAME("_begin_input_session"));
		accept_event();
		return;
	}
	LineEdit::gui_input(p_event);
}

void TagAsciiInput::_begin_input_session() {
	InputSessionManager *manager = InputSessionManager::get_or_create(this);
	if (manager == nullptr) {
		return;
	}

	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["presentation_mode"] = presentation_mode;
	descriptor["keypad_id"] = keypad_id;
	descriptor["keypad_scene_override"] = keypad_scene_override;
	descriptor["initial_value"] = get_text();
	descriptor["previous_value"] = get_text();
	descriptor["anchor"] = keypad_anchor;
	descriptor["screen_cell"] = keypad_screen_cell;
	descriptor["side"] = keypad_side;
	descriptor["align"] = keypad_align;
	descriptor["anchor_control"] = this;
	manager->begin_session(
			this,
			descriptor,
			callable_mp(this, &TagAsciiInput::_validate_input_session_text),
			callable_mp(this, &TagAsciiInput::_commit_input_session_text),
			callable_mp(this, &TagAsciiInput::_cancel_input_session));
}

Dictionary TagAsciiInput::_validate_input_session_text(const String &p_text) const {
	Dictionary result;
	result["ok"] = true;
	result["value"] = p_text;
	return result;
}

Dictionary TagAsciiInput::_commit_input_session_text(const String &p_text) {
	if (!tag_name.is_empty() && !tag_widget::write_tag(this, tag_name, p_text)) {
		Dictionary failure;
		failure["ok"] = false;
		failure["error"] = "write failed";
		return failure;
	}
	set_text(p_text);
	revert_error_style();

	Dictionary result;
	result["ok"] = true;
	result["value"] = p_text;
	return result;
}

void TagAsciiInput::_cancel_input_session() {
	revert_error_style();
}

void TagAsciiInput::_on_tag_changed(const String &p_tag, const Variant &p_value, const String &p_quality, int p_version, int p_ts_ms) {
	(void)p_quality;
	(void)p_version;
	(void)p_ts_ms;
	if (p_tag != tag_name) {
		return;
	}

	InputSessionManager *manager = InputSessionManager::get_or_create(this);
	if (manager != nullptr && manager->is_owner_active(get_instance_id())) {
		return;
	}
	set_text(p_value.operator String());
}

void TagAsciiInput::flash_error(const String &p_msg) {
	add_theme_color_override("font_color", Color(0.95f, 0.2f, 0.2f));
	set_tooltip_text(p_msg);
	if (is_inside_tree()) {
		Ref<SceneTreeTimer> timer = get_tree()->create_timer(1.0);
		timer->connect("timeout", callable_mp(this, &TagAsciiInput::revert_error_style));
	}
}

void TagAsciiInput::revert_error_style() {
	add_theme_color_override("font_color", Color(1, 1, 1));
	set_tooltip_text("");
}
