#include "widget_meter.h"

#include "core/object/class_db.h"

void WidgetMeter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetMeter::get_version);
}

String WidgetMeter::get_version() const {
	return "0.1.0";
}
