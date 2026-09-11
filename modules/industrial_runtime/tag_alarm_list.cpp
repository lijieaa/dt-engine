#include "tag_alarm_list.h"

#include "tag_widget_util.h"
#include "widget_alarm.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"

TagAlarmList::TagAlarmList() {
	alarm_core.instantiate();
}

TagAlarmList::~TagAlarmList() {}

void TagAlarmList::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_monitored_tags", "tags"), &TagAlarmList::set_monitored_tags);
	ClassDB::bind_method(D_METHOD("get_monitored_tags"), &TagAlarmList::get_monitored_tags);
	ClassDB::bind_method(D_METHOD("set_threshold", "v"), &TagAlarmList::set_threshold);
	ClassDB::bind_method(D_METHOD("get_threshold"), &TagAlarmList::get_threshold);
	ClassDB::bind_method(D_METHOD("set_comparator", "op"), &TagAlarmList::set_comparator);
	ClassDB::bind_method(D_METHOD("get_comparator"), &TagAlarmList::get_comparator);
	ClassDB::bind_method(D_METHOD("set_debounce_ms", "ms"), &TagAlarmList::set_debounce_ms);
	ClassDB::bind_method(D_METHOD("get_debounce_ms"), &TagAlarmList::get_debounce_ms);
	ClassDB::bind_method(D_METHOD("set_active_color", "c"), &TagAlarmList::set_active_color);
	ClassDB::bind_method(D_METHOD("get_active_color"), &TagAlarmList::get_active_color);
	ClassDB::bind_method(D_METHOD("set_normal_color", "c"), &TagAlarmList::set_normal_color);
	ClassDB::bind_method(D_METHOD("get_normal_color"), &TagAlarmList::get_normal_color);
	ClassDB::bind_method(D_METHOD("get_active_count"), &TagAlarmList::get_active_count);
	ClassDB::bind_method(D_METHOD("apply_external_value", "tag", "value"), &TagAlarmList::apply_external_value);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagAlarmList::_on_tag_changed);
	ClassDB::bind_method(D_METHOD("refresh_rows"), &TagAlarmList::refresh_rows);
	ClassDB::bind_method(D_METHOD("reconfigure_rules"), &TagAlarmList::reconfigure_rules);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "monitored_tags"), "set_monitored_tags", "get_monitored_tags");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "threshold"), "set_threshold", "get_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "comparator"), "set_comparator", "get_comparator");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "debounce_ms"), "set_debounce_ms", "get_debounce_ms");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "active_color"), "set_active_color", "get_active_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "normal_color"), "set_normal_color", "get_normal_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "active_count"), "", "get_active_count");
}

void TagAlarmList::set_monitored_tags(const Array &p_tags) {
	monitored_tags = p_tags;
	reconfigure_rules();
	refresh_rows();
}

void TagAlarmList::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		clear();
		if (!monitored_tags.is_empty()) {
			tag_widget::subscribe(this, monitored_tags, callable_mp(this, &TagAlarmList::_on_tag_changed));
		}
		reconfigure_rules();
		refresh_rows();
	}
}

void TagAlarmList::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms) {
	if (!monitored_tags.has(tag)) {
		return;
	}
	apply_external_value(tag, v);
}

void TagAlarmList::reconfigure_rules() {
	if (alarm_core.is_null()) {
		alarm_core.instantiate();
	}
	if (alarm_core.is_null()) {
		return;
	}
	alarm_core->clear();
	for (int i = 0; i < monitored_tags.size(); i++) {
		const String t = monitored_tags[i].operator String();
		Dictionary rule;
		rule["type"] = comparator;
		rule["threshold"] = threshold;
		rule["debounce_ms"] = debounce_ms;
		alarm_core->add_rule(t, rule);
	}
}

void TagAlarmList::apply_external_value(const String &p_tag, const Variant &p_value) {
	if (alarm_core.is_null()) {
		return;
	}
	alarm_core->evaluate(p_tag, p_value);
	refresh_rows();
}

void TagAlarmList::refresh_rows() {
	clear();
	active_count = 0;

	Array active_rules;
	if (!alarm_core.is_null()) {
		active_rules = alarm_core->get_active_rules();
	}

	for (int i = 0; i < monitored_tags.size(); i++) {
		const String t = monitored_tags[i].operator String();
		const int idx = add_item(t);
		if (active_rules.has(t)) {
			set_item_custom_fg_color(idx, active_color);
			active_count++;
		} else {
			set_item_custom_fg_color(idx, normal_color);
		}
	}
}