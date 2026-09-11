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

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "action_id"), "set_action_id", "get_action_id");
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
}

void KeypadActionButton::set_action_payload(const Variant &p_payload) {
	action_payload = p_payload;
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
