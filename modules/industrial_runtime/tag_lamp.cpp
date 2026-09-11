#include "tag_lamp.h"

TagLamp::TagLamp() {
	blink_cfg["enabled"] = false;
	blink_cfg["interval_sec"] = 0.5;
}

#include "tag_widget_util.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/callable.h"

void TagLamp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagLamp::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagLamp::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_on_color", "c"), &TagLamp::set_on_color);
	ClassDB::bind_method(D_METHOD("get_on_color"), &TagLamp::get_on_color);
	ClassDB::bind_method(D_METHOD("set_off_color", "c"), &TagLamp::set_off_color);
	ClassDB::bind_method(D_METHOD("get_off_color"), &TagLamp::get_off_color);
	ClassDB::bind_method(D_METHOD("set_base_bg", "c"), &TagLamp::set_base_bg);
	ClassDB::bind_method(D_METHOD("get_base_bg"), &TagLamp::get_base_bg);
	ClassDB::bind_method(D_METHOD("set_blink_cfg", "cfg"), &TagLamp::set_blink_cfg);
	ClassDB::bind_method(D_METHOD("get_blink_cfg"), &TagLamp::get_blink_cfg);
	ClassDB::bind_method(D_METHOD("is_lit"), &TagLamp::is_lit);
	ClassDB::bind_method(D_METHOD("apply_external_value", "value"), &TagLamp::apply_external_value);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagLamp::_on_tag_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "on_color"), "set_on_color", "get_on_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "off_color"), "set_off_color", "get_off_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "base_bg"), "set_base_bg", "get_base_bg");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "blink_cfg"), "set_blink_cfg", "get_blink_cfg");
}

void TagLamp::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (!tag_name.is_empty()) {
		tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagLamp::_on_tag_changed));
	}
}

void TagLamp::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		set_custom_minimum_size(Vector2(28, 28));
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagLamp::_on_tag_changed));
		}
		refresh();
	} else if (p_what == NOTIFICATION_PROCESS) {
		_process(get_process_delta_time());
	}
}

void TagLamp::_process(double p_delta) {
	if (!lit || !bool(blink_cfg.get("enabled", false))) {
		return;
	}
	double interval = float(blink_cfg.get("interval_sec", 0.5));
	if (interval <= 0.0) {
		interval = 0.5;
	}
	blink_phase += p_delta;
	if (blink_phase >= interval) {
		blink_phase = 0.0;
		toggle_blink();
	}
}

void TagLamp::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms) {
	if (tag == tag_name) {
		apply_external_value(v);
	}
}

void TagLamp::apply_external_value(const Variant &p_value) {
	lit = tag_widget::to_bool(p_value);
	refresh();
}

void TagLamp::refresh() {
	target_color = lit ? on_color : off_color;
	add_theme_color_override("panel", target_color);
	blink_phase = 0.0;
}

void TagLamp::toggle_blink() {
	if (lit) {
		const Color current = get_theme_color("panel");
		add_theme_color_override("panel", current == on_color ? base_bg : on_color);
	}
}