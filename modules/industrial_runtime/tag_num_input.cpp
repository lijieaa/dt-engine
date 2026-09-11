#include "tag_num_input.h"

#include "input_session_manager.h"
#include "tag_widget_util.h"
#include "widget_format.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "scene/main/scene_tree.h"

void TagNumInput::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagNumInput::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagNumInput::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_format_cfg", "cfg"), &TagNumInput::set_format_cfg);
	ClassDB::bind_method(D_METHOD("get_format_cfg"), &TagNumInput::get_format_cfg);
	ClassDB::bind_method(D_METHOD("validate_input", "text"), &TagNumInput::validate_input);
	ClassDB::bind_method(D_METHOD("_validate_input_session_text", "text"), &TagNumInput::_validate_input_session_text);
	ClassDB::bind_method(D_METHOD("_commit_input_session_text", "text"), &TagNumInput::_commit_input_session_text);
	ClassDB::bind_method(D_METHOD("_cancel_input_session"), &TagNumInput::_cancel_input_session);
	ClassDB::bind_method(D_METHOD("_begin_input_session"), &TagNumInput::_begin_input_session);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagNumInput::_on_tag_changed);

	ClassDB::bind_method(D_METHOD("set_keypad_id", "id"), &TagNumInput::set_keypad_id);
	ClassDB::bind_method(D_METHOD("get_keypad_id"), &TagNumInput::get_keypad_id);
	ClassDB::bind_method(D_METHOD("set_keypad_scene_override", "scene"), &TagNumInput::set_keypad_scene_override);
	ClassDB::bind_method(D_METHOD("get_keypad_scene_override"), &TagNumInput::get_keypad_scene_override);
	ClassDB::bind_method(D_METHOD("set_presentation_mode", "mode"), &TagNumInput::set_presentation_mode);
	ClassDB::bind_method(D_METHOD("get_presentation_mode"), &TagNumInput::get_presentation_mode);

	ClassDB::bind_method(D_METHOD("set_use_min", "v"), &TagNumInput::set_use_min);
	ClassDB::bind_method(D_METHOD("get_use_min"), &TagNumInput::get_use_min);
	ClassDB::bind_method(D_METHOD("set_min_value", "v"), &TagNumInput::set_min_value);
	ClassDB::bind_method(D_METHOD("get_min_value"), &TagNumInput::get_min_value);
	ClassDB::bind_method(D_METHOD("set_use_max", "v"), &TagNumInput::set_use_max);
	ClassDB::bind_method(D_METHOD("get_use_max"), &TagNumInput::get_use_max);
	ClassDB::bind_method(D_METHOD("set_max_value", "v"), &TagNumInput::set_max_value);
	ClassDB::bind_method(D_METHOD("get_max_value"), &TagNumInput::get_max_value);
	ClassDB::bind_method(D_METHOD("set_show_limits_on_keypad", "v"), &TagNumInput::set_show_limits_on_keypad);
	ClassDB::bind_method(D_METHOD("get_show_limits_on_keypad"), &TagNumInput::get_show_limits_on_keypad);
	ClassDB::bind_method(D_METHOD("set_restart_on_out_of_range", "v"), &TagNumInput::set_restart_on_out_of_range);
	ClassDB::bind_method(D_METHOD("get_restart_on_out_of_range"), &TagNumInput::get_restart_on_out_of_range);
	ClassDB::bind_method(D_METHOD("set_show_previous_value", "v"), &TagNumInput::set_show_previous_value);
	ClassDB::bind_method(D_METHOD("get_show_previous_value"), &TagNumInput::get_show_previous_value);
	ClassDB::bind_method(D_METHOD("set_hide_keypad_title", "v"), &TagNumInput::set_hide_keypad_title);
	ClassDB::bind_method(D_METHOD("get_hide_keypad_title"), &TagNumInput::get_hide_keypad_title);
	ClassDB::bind_method(D_METHOD("set_keypad_anchor", "v"), &TagNumInput::set_keypad_anchor);
	ClassDB::bind_method(D_METHOD("get_keypad_anchor"), &TagNumInput::get_keypad_anchor);
	ClassDB::bind_method(D_METHOD("set_keypad_screen_cell", "v"), &TagNumInput::set_keypad_screen_cell);
	ClassDB::bind_method(D_METHOD("get_keypad_screen_cell"), &TagNumInput::get_keypad_screen_cell);
	ClassDB::bind_method(D_METHOD("set_keypad_side", "v"), &TagNumInput::set_keypad_side);
	ClassDB::bind_method(D_METHOD("get_keypad_side"), &TagNumInput::get_keypad_side);
	ClassDB::bind_method(D_METHOD("set_keypad_align", "v"), &TagNumInput::set_keypad_align);
	ClassDB::bind_method(D_METHOD("get_keypad_align"), &TagNumInput::get_keypad_align);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "format_cfg"), "set_format_cfg", "get_format_cfg");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "keypad_id"), "set_keypad_id", "get_keypad_id");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "keypad_scene_override", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_keypad_scene_override", "get_keypad_scene_override");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "presentation_mode", PROPERTY_HINT_ENUM, "system,popup,fixed,direct_window"), "set_presentation_mode", "get_presentation_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_min"), "set_use_min", "get_use_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_value"), "set_min_value", "get_min_value");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_max"), "set_use_max", "get_use_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_value"), "set_max_value", "get_max_value");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_limits_on_keypad"), "set_show_limits_on_keypad", "get_show_limits_on_keypad");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "restart_on_out_of_range"), "set_restart_on_out_of_range", "get_restart_on_out_of_range");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_previous_value"), "set_show_previous_value", "get_show_previous_value");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_keypad_title"), "set_hide_keypad_title", "get_hide_keypad_title");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "keypad_anchor", PROPERTY_HINT_ENUM, "center,screen,control"), "set_keypad_anchor", "get_keypad_anchor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "keypad_screen_cell", PROPERTY_HINT_RANGE, "0,8,1"), "set_keypad_screen_cell", "get_keypad_screen_cell");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "keypad_side", PROPERTY_HINT_ENUM, "top,bottom,left,right"), "set_keypad_side", "get_keypad_side");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "keypad_align", PROPERTY_HINT_ENUM, "start,center,end"), "set_keypad_align", "get_keypad_align");
}

void TagNumInput::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (!tag_name.is_empty()) {
		tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagNumInput::_on_tag_changed));
	}
}

Dictionary TagNumInput::validate_input(const String &p_text) {
	if (ClassDB::class_exists("WidgetFormat")) {
		const Variant result = WidgetFormat::parse_input(p_text, format_cfg);
		if (result.get_type() == Variant::DICTIONARY) {
			const Dictionary parsed = result;
			if (parsed.has("ok")) {
				return parsed;
			}
		}
	}

	const String value = p_text.strip_edges();
	if (value.is_empty() || value == "-" || value == "." || value == "-.") {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "input empty";
		return result;
	}

	Dictionary result;
	if (value.is_valid_int()) {
		result["ok"] = true;
		result["value"] = value.to_int();
		return result;
	}
	if (value.is_valid_float()) {
		result["ok"] = true;
		result["value"] = value.to_float();
		return result;
	}
	result["ok"] = false;
	result["error"] = "invalid number";
	return result;
}

void TagNumInput::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		set_editable(false);
		set_virtual_keyboard_enabled(false);
		set_focus_mode(FOCUS_ALL);
		if (get_placeholder().is_empty()) {
			set_placeholder("Tap to enter...");
		}
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagNumInput::_on_tag_changed));
		}
	}
}

void TagNumInput::gui_input(const Ref<InputEvent> &p_event) {
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

void TagNumInput::_begin_input_session() {
	InputSessionManager *manager = InputSessionManager::get_or_create(this);
	if (manager == nullptr) {
		return;
	}

	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["presentation_mode"] = presentation_mode;
	descriptor["keypad_id"] = keypad_id;
	descriptor["keypad_scene_override"] = keypad_scene_override;
	descriptor["initial_value"] = get_text();
	descriptor["previous_value"] = get_text();
	descriptor["format_config"] = format_cfg;
	descriptor["has_min"] = use_min;
	descriptor["min_value"] = min_value;
	descriptor["has_max"] = use_max;
	descriptor["max_value"] = max_value;
	descriptor["show_previous_value"] = show_previous_value;
	descriptor["show_limits_on_keypad"] = show_limits_on_keypad;
	descriptor["hide_keypad_title"] = hide_keypad_title;
	descriptor["anchor"] = keypad_anchor;
	descriptor["screen_cell"] = keypad_screen_cell;
	descriptor["side"] = keypad_side;
	descriptor["align"] = keypad_align;
	descriptor["anchor_control"] = this;
	manager->begin_session(
			this,
			descriptor,
			callable_mp(this, &TagNumInput::_validate_input_session_text),
			callable_mp(this, &TagNumInput::_commit_input_session_text),
			callable_mp(this, &TagNumInput::_cancel_input_session));
}

Dictionary TagNumInput::_validate_input_session_text(const String &p_text) const {
	const String entered = p_text.strip_edges().is_empty() ? String("0") : p_text.strip_edges();
	TagNumInput *self = const_cast<TagNumInput *>(this);
	Dictionary result = self->validate_input(entered);
	if (!(bool)result.get("ok", false)) {
		return result;
	}

	const Variant value = result.get("value", entered);
	const double numeric = tag_widget::to_float(value, 0.0f);
	if (use_min && numeric < min_value) {
		result["ok"] = false;
		result["error"] = "out of range";
		return result;
	}
	if (use_max && numeric > max_value) {
		result["ok"] = false;
		result["error"] = "out of range";
		return result;
	}
	return result;
}

Dictionary TagNumInput::_commit_input_session_text(const String &p_text) {
	Dictionary result = _validate_input_session_text(p_text);
	if (!(bool)result.get("ok", false)) {
		return result;
	}

	const Variant value = result.get("value", String("0"));
	if (!tag_name.is_empty() && !tag_widget::write_tag(this, tag_name, value)) {
		Dictionary failure;
		failure["ok"] = false;
		failure["error"] = "write failed";
		return failure;
	}

	set_text(value.operator String());
	revert_error_style();
	return result;
}

void TagNumInput::_cancel_input_session() {
	revert_error_style();
}

void TagNumInput::_on_tag_changed(const String &p_tag, const Variant &p_value, const String &p_quality, int p_version, int p_ts_ms) {
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

void TagNumInput::flash_error(const String &p_msg) {
	add_theme_color_override("font_color", Color(0.95f, 0.2f, 0.2f));
	set_tooltip_text(p_msg);
	if (is_inside_tree()) {
		Ref<SceneTreeTimer> timer = get_tree()->create_timer(1.0);
		timer->connect("timeout", callable_mp(this, &TagNumInput::revert_error_style));
	}
}

void TagNumInput::revert_error_style() {
	add_theme_color_override("font_color", Color(1, 1, 1));
	set_tooltip_text("");
}
