#include "widget_alarm.h"

#include "core/object/class_db.h"

void WidgetAlarm::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetAlarm::get_version);
}

String WidgetAlarm::get_version() const {
	return "0.1.0";
}