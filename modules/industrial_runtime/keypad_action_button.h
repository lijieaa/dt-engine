#pragma once

#include "scene/gui/button.h"

#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class InputEvent;
class Timer;

class KeypadActionButton : public Button {
	GDCLASS(KeypadActionButton, Button);

	String action_id;
	Variant action_payload;
	String repeat_mode = "none";
	int repeat_delay_ms = 400;
	int repeat_interval_ms = 100;
	Timer *repeat_timer = nullptr;

	static bool _is_repeatable_action(const String &p_action_id);
	static bool _action_requires_payload(const String &p_action_id);
	void _emit_action(bool p_repeat);
	void _start_repeat();
	void _stop_repeat();
	void _on_repeat_timeout();
	Variant _normalize_payload_for_action(const String &p_action_id, const Variant &p_payload) const;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;
	virtual void pressed() override;
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	void _notification(int p_what);

public:
	KeypadActionButton();

	void set_action_id(const String &p_id);
	String get_action_id() const { return action_id; }
	void set_action_payload(const Variant &p_payload);
	Variant get_action_payload() const { return action_payload; }
	void set_repeat_mode(const String &p_mode);
	String get_repeat_mode() const { return repeat_mode; }
	void set_repeat_delay_ms(int p_ms);
	int get_repeat_delay_ms() const { return repeat_delay_ms; }
	void set_repeat_interval_ms(int p_ms);
	int get_repeat_interval_ms() const { return repeat_interval_ms; }
};
