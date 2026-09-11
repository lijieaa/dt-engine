#pragma once

#include "input_session.h"
#include "keypad_view.h"

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

class Control;

class TagAsciiKeypad : public KeypadView {
	GDCLASS(TagAsciiKeypad, KeypadView);

	Ref<InputSession> compatibility_session;

	void _rebuild_ui();
	void _dispatch_compat_action(const Dictionary &p_request);
	void _add_char_button(Control *p_parent, const String &p_label);
	void _add_action_button(Control *p_parent, const String &p_label, const String &p_action_id, const Variant &p_payload = Variant(), bool p_repeat = false);

protected:
	static void _bind_methods();

public:
	TagAsciiKeypad();

	void open_for(const String &p_initial);
	void open_for_options(const Dictionary &p_opts);
	String get_buffer() const;
};
