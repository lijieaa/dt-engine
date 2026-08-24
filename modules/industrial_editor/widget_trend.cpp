#include "widget_trend.h"

#include "core/object/class_db.h"

void WidgetTrend::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetTrend::get_version);
}

String WidgetTrend::get_version() const {
	return "0.1.0";
}
