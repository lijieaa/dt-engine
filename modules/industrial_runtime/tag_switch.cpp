#include "tag_switch.h"

#include "tag_widget_util.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/callable.h"
#include "scene/scene_string_names.h"

void TagSwitch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagSwitch::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagSwitch::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_on_color", "c"), &TagSwitch::set_on_color);
	ClassDB::bind_method(D_METHOD("get_on_color"), &TagSwitch::get_on_color);
	ClassDB::bind_method(D_METHOD("set_off_color", "c"), &TagSwitch::set_off_color);
	ClassDB::bind_method(D_METHOD("get_off_color"), &TagSwitch::get_off_color);
	ClassDB::bind_method(D_METHOD("apply_external_value", "value"), &TagSwitch::apply_external_value);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagSwitch::_on_tag_changed);
	ClassDB::bind_method(D_METHOD("_on_toggled", "pressed"), &TagSwitch::_on_toggled);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "on_color"), "set_on_color", "get_on_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "off_color"), "set_off_color", "get_off_color");
}

void TagSwitch::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (!tag_name.is_empty()) {
		set_text("Write " + tag_name);
		tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagSwitch::_on_tag_changed));
	}
}

void TagSwitch::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagSwitch::_on_tag_changed));
		}
		connect(SceneStringName(toggled), callable_mp(this, &TagSwitch::_on_toggled));
		refresh_tint();
	}
}

void TagSwitch::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms) {
	if (tag == tag_name) {
		apply_external_value(v);
	}
}

void TagSwitch::apply_external_value(const Variant &p_value) {
	set_pressed_no_signal(tag_widget::to_bool(p_value));
	refresh_tint();
}

void TagSwitch::_on_toggled(bool p_pressed) {
	refresh_tint();
	if (!tag_name.is_empty()) {
		tag_widget::write_tag(this, tag_name, p_pressed);
	}
}

void TagSwitch::refresh_tint() {
	add_theme_color_override("font_color", is_pressed() ? on_color : off_color);
}