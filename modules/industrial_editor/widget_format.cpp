#include "widget_format.h"

#include "core/object/class_db.h"

void WidgetFormat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetFormat::get_version);
}

String WidgetFormat::get_version() const {
	return "0.1.0";
}
