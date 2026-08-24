#include "widget_macro.h"

#include "core/object/class_db.h"

void WidgetMacro::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetMacro::get_version);
}

String WidgetMacro::get_version() const {
	return "0.1.0";
}
