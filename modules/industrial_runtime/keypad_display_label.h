#pragma once

#include "scene/gui/label.h"

#include "core/string/ustring.h"

class KeypadDisplayLabel : public Label {
	GDCLASS(KeypadDisplayLabel, Label);

	String bind_role = "none";

protected:
	static void _bind_methods();

public:
	void set_bind_role(const String &p_role);
	String get_bind_role() const { return bind_role; }

	static bool is_known_bind_role(const String &p_role);
};
