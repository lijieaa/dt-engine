#include "keypad_action.h"

bool keypad_action_id_is_input(const String &p_action_id) {
	return p_action_id == "insert_text" ||
			p_action_id == "decimal" ||
			p_action_id == "toggle_sign" ||
			p_action_id == "backspace" ||
			p_action_id == "delete" ||
			p_action_id == "clear" ||
			p_action_id == "move_left" ||
			p_action_id == "move_right" ||
			p_action_id == "move_home" ||
			p_action_id == "move_end" ||
			p_action_id == "increment" ||
			p_action_id == "decrement" ||
			p_action_id == "toggle_shift" ||
			p_action_id == "confirm" ||
			p_action_id == "cancel";
}

bool keypad_action_id_is_navigation(const String &p_action_id) {
	return p_action_id == "focus_next" ||
			p_action_id == "focus_previous" ||
			p_action_id == "switch_keypad" ||
			p_action_id == "open_window" ||
			p_action_id == "close_window" ||
			p_action_id == "switch_screen";
}

bool keypad_action_id_is_runtime_command(const String &p_action_id) {
	return p_action_id == "write_tag" ||
			p_action_id == "set_tag" ||
			p_action_id == "toggle_tag" ||
			p_action_id == "increment_tag" ||
			p_action_id == "decrement_tag" ||
			p_action_id == "call_command";
}

bool keypad_action_id_is_known(const String &p_action_id) {
	return keypad_action_id_is_input(p_action_id) ||
			keypad_action_id_is_navigation(p_action_id) ||
			keypad_action_id_is_runtime_command(p_action_id);
}
