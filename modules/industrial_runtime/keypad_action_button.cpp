#include "keypad_action_button.h"

#include "keypad_action.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/input/input_event.h"
#include "core/string/ustring.h"
#include "scene/main/timer.h"

void KeypadActionButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action_id", "id"), &KeypadActionButton::set_action_id);
	ClassDB::bind_method(D_METHOD("get_action_id"), &KeypadActionButton::get_action_id);
	ClassDB::bind_method(D_METHOD("set_action_payload", "payload"), &KeypadActionButton::set_action_payload);
	ClassDB::bind_method(D_METHOD("get_action_payload"), &KeypadActionButton::get_action_payload);
	ClassDB::bind_method(D_METHOD("set_repeat_mode", "mode"), &KeypadActionButton::set_repeat_mode);
	ClassDB::bind_method(D_METHOD("get_repeat_mode"), &KeypadActionButton::get_repeat_mode);
	ClassDB::bind_method(D_METHOD("set_repeat_delay_ms", "ms"), &KeypadActionButton::set_repeat_delay_ms);
	ClassDB::bind_method(D_METHOD("get_repeat_delay_ms"), &KeypadActionButton::get_repeat_delay_ms);
	ClassDB::bind_method(D_METHOD("set_repeat_interval_ms", "ms"), &KeypadActionButton::set_repeat_interval_ms);
	ClassDB::bind_method(D_METHOD("get_repeat_interval_ms"), &KeypadActionButton::get_repeat_interval_ms);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "action_id", PROPERTY_HINT_ENUM, keypad_action_id_property_hint()), "set_action_id", "get_action_id");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "action_payload", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), "set_action_payload", "get_action_payload");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "repeat_mode", PROPERTY_HINT_ENUM, "none,repeat"), "set_repeat_mode", "get_repeat_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "repeat_delay_ms", PROPERTY_HINT_RANGE, "0,10000,1"), "set_repeat_delay_ms", "get_repeat_delay_ms");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "repeat_interval_ms", PROPERTY_HINT_RANGE, "1,10000,1"), "set_repeat_interval_ms", "get_repeat_interval_ms");

	ADD_SIGNAL(MethodInfo("action_requested", PropertyInfo(Variant::DICTIONARY, "request")));
}

KeypadActionButton::KeypadActionButton() {
	set_action_mode(ACTION_MODE_BUTTON_PRESS);

	repeat_timer = memnew(Timer);
	repeat_timer->set_one_shot(true);
	repeat_timer->connect(SNAME("timeout"), callable_mp(this, &KeypadActionButton::_on_repeat_timeout));
	add_child(repeat_timer);
}

void KeypadActionButton::set_action_id(const String &p_id) {
	action_id = p_id.strip_edges();
	action_payload = _normalize_payload_for_action(action_id, action_payload);
	notify_property_list_changed();
}

void KeypadActionButton::set_action_payload(const Variant &p_payload) {
	action_payload = _normalize_payload_for_action(action_id, p_payload);
}

bool KeypadActionButton::_action_requires_payload(const String &p_action_id) {
	return p_action_id == "insert_text" ||
			p_action_id == "increment" ||
			p_action_id == "decrement" ||
			keypad_action_id_is_runtime_command(p_action_id) ||
			p_action_id == "switch_keypad" ||
			p_action_id == "open_window" ||
			p_action_id == "switch_screen";
}

Variant KeypadActionButton::_normalize_payload_for_action(const String &p_action_id, const Variant &p_payload) const {
	if (!_action_requires_payload(p_action_id)) {
		return Variant();
	}
	if (p_action_id == "insert_text") {
		if (p_payload.get_type() == Variant::STRING) {
			return p_payload;
		}
		if (p_payload.get_type() == Variant::INT) {
			return String::num_int64((int64_t)p_payload);
		}
		if (p_payload.get_type() == Variant::FLOAT) {
			return String::num((double)p_payload);
		}
		return String("0");
	}
	if (p_action_id == "increment" || p_action_id == "decrement") {
		if (p_payload.get_type() == Variant::INT || p_payload.get_type() == Variant::FLOAT) {
			return p_payload;
		}
		if (p_payload.get_type() == Variant::STRING) {
			const String text = ((String)p_payload).strip_edges();
			if (text.is_valid_float()) {
				return text.to_float();
			}
		}
		return 1.0;
	}
	return p_payload;
}

void KeypadActionButton::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name != "action_payload") {
		return;
	}

	if (!_action_requires_payload(action_id)) {
		p_property.usage = PROPERTY_USAGE_NONE;
		return;
	}

	if (action_id == "insert_text") {
		// Dropdown of common keys; ENUM_SUGGESTION still allows typing custom chars.
		p_property.type = Variant::STRING;
		p_property.hint = PROPERTY_HINT_ENUM_SUGGESTION;
		p_property.hint_string = "0,1,2,3,4,5,6,7,8,9,.,-,+, ,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z";
		p_property.usage = PROPERTY_USAGE_DEFAULT;
		return;
	}

	if (action_id == "increment" || action_id == "decrement") {
		p_property.type = Variant::STRING;
		p_property.hint = PROPERTY_HINT_ENUM_SUGGESTION;
		p_property.hint_string = "0.01,0.1,1,10,100";
		p_property.usage = PROPERTY_USAGE_DEFAULT;
		return;
	}

	if (keypad_action_id_is_runtime_command(action_id) ||
			action_id == "switch_keypad" ||
			action_id == "open_window" ||
			action_id == "switch_screen") {
		p_property.type = Variant::DICTIONARY;
		p_property.hint = PROPERTY_HINT_NONE;
		p_property.usage = PROPERTY_USAGE_DEFAULT;
	}
}

void KeypadActionButton::set_repeat_mode(const String &p_mode) {
	repeat_mode = p_mode == "repeat" ? String("repeat") : String("none");
}

void KeypadActionButton::set_repeat_delay_ms(int p_ms) {
	repeat_delay_ms = MAX(p_ms, 0);
}

void KeypadActionButton::set_repeat_interval_ms(int p_ms) {
	repeat_interval_ms = MAX(p_ms, 1);
}

bool KeypadActionButton::_is_repeatable_action(const String &p_action_id) {
	return p_action_id == "increment" ||
			p_action_id == "decrement" ||
			p_action_id == "move_left" ||
			p_action_id == "move_right" ||
			p_action_id == "backspace";
}

void KeypadActionButton::_emit_action(bool p_repeat) {
	if (action_id.is_empty()) {
		WARN_PRINT("Keypad action button ignored an empty action id.");
		return;
	}

	Dictionary request;
	request["action_id"] = action_id;
	request["payload"] = action_payload;
	request["source_id"] = get_instance_id();
	request["repeat"] = p_repeat;
	emit_signal(SNAME("action_requested"), request);
}

void KeypadActionButton::_start_repeat() {
	if (repeat_timer == nullptr || repeat_mode != "repeat" || !_is_repeatable_action(action_id) || is_disabled()) {
		return;
	}

	repeat_timer->stop();
	repeat_timer->set_one_shot(true);
	repeat_timer->set_wait_time((double)repeat_delay_ms / 1000.0);
	repeat_timer->start();
}

void KeypadActionButton::_stop_repeat() {
	if (repeat_timer != nullptr) {
		repeat_timer->stop();
	}
}

void KeypadActionButton::_on_repeat_timeout() {
	if (repeat_timer == nullptr || is_disabled() || repeat_mode != "repeat" || !_is_repeatable_action(action_id)) {
		_stop_repeat();
		return;
	}

	_emit_action(true);
	repeat_timer->set_one_shot(false);
	repeat_timer->set_wait_time((double)repeat_interval_ms / 1000.0);
	repeat_timer->start();
}

void KeypadActionButton::pressed() {
	_emit_action(false);
	if (action_id == "cancel" || action_id == "confirm") {
		_stop_repeat();
	}
}

void KeypadActionButton::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mouse_button = p_event;
	if (mouse_button.is_valid() && mouse_button->get_button_index() == MouseButton::LEFT) {
		if (mouse_button->is_pressed()) {
			_start_repeat();
		} else {
			_stop_repeat();
		}
	}

	Ref<InputEventScreenTouch> screen_touch = p_event;
	if (screen_touch.is_valid()) {
		if (screen_touch->is_pressed()) {
			_start_repeat();
		} else {
			_stop_repeat();
		}
	}

	Button::gui_input(p_event);
}

void KeypadActionButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_MOUSE_EXIT:
		case NOTIFICATION_FOCUS_EXIT:
		case NOTIFICATION_EXIT_TREE:
			_stop_repeat();
			break;
		default:
			break;
	}
}
