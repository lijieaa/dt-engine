#pragma once

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

struct KeypadActionRequest {
	String action_id;
	Variant payload;
	ObjectID source_id;
};

bool keypad_action_id_is_input(const String &p_action_id);
bool keypad_action_id_is_navigation(const String &p_action_id);
bool keypad_action_id_is_runtime_command(const String &p_action_id);
bool keypad_action_id_is_known(const String &p_action_id);
