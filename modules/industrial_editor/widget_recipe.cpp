#include "widget_recipe.h"

#include "core/object/class_db.h"

void WidgetRecipe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_version"), &WidgetRecipe::get_version);
}

String WidgetRecipe::get_version() const {
	return "0.1.0";
}
