#include "keypad_display_label.h"

#include "core/object/class_db.h"
#include "core/string/ustring.h"

void KeypadDisplayLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bind_role", "role"), &KeypadDisplayLabel::set_bind_role);
	ClassDB::bind_method(D_METHOD("get_bind_role"), &KeypadDisplayLabel::get_bind_role);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bind_role", PROPERTY_HINT_ENUM, "none,input_display,previous_value,min_value,max_value,range_hint,error"), "set_bind_role", "get_bind_role");
}

void KeypadDisplayLabel::set_bind_role(const String &p_role) {
	const String role = p_role.strip_edges();
	if (role.is_empty() || role == "none") {
		bind_role = "none";
		return;
	}
	if (!is_known_bind_role(role)) {
		WARN_PRINT(vformat("KeypadDisplayLabel ignored unknown bind_role '%s'.", role));
		bind_role = "none";
		return;
	}
	bind_role = role;
}

bool KeypadDisplayLabel::is_known_bind_role(const String &p_role) {
	return p_role == "input_display" ||
			p_role == "previous_value" ||
			p_role == "min_value" ||
			p_role == "max_value" ||
			p_role == "range_hint" ||
			p_role == "error";
}
