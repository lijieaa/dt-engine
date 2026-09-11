#include "tag_ascii_keypad.h"

#include "keypad_action_button.h"
#include "keypad_display_label.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "scene/gui/box_container.h"
#include "scene/gui/control.h"
#include "scene/gui/margin_container.h"

void TagAsciiKeypad::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_for", "initial"), &TagAsciiKeypad::open_for);
	ClassDB::bind_method(D_METHOD("open_for_options", "opts"), &TagAsciiKeypad::open_for_options);
	ClassDB::bind_method(D_METHOD("get_buffer"), &TagAsciiKeypad::get_buffer);
	ClassDB::bind_method(D_METHOD("_dispatch_compat_action", "request"), &TagAsciiKeypad::_dispatch_compat_action);

	ADD_SIGNAL(MethodInfo("value_confirmed", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("canceled"));
}

TagAsciiKeypad::TagAsciiKeypad() {
	set_custom_minimum_size(Size2(520, 320));
	_rebuild_ui();
	set_action_dispatcher(callable_mp(this, &TagAsciiKeypad::_dispatch_compat_action));
}

void TagAsciiKeypad::_rebuild_ui() {
	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 10);
	margin->add_theme_constant_override("margin_right", 10);
	margin->add_theme_constant_override("margin_top", 8);
	margin->add_theme_constant_override("margin_bottom", 8);
	add_child(margin);

	VBoxContainer *root = memnew(VBoxContainer);
	root->add_theme_constant_override("separation", 8);
	margin->add_child(root);

	KeypadDisplayLabel *display = memnew(KeypadDisplayLabel);
	display->set_bind_role("input_display");
	display->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	display->set_custom_minimum_size(Size2(0, 40));
	display->add_theme_font_size_override("font_size", 22);
	display->set_clip_text(true);
	root->add_child(display);

	const char *rows[] = {
		"1234567890",
		"qwertyuiop",
		"asdfghjkl",
		"zxcvbnm",
	};
	for (const char *row_chars : rows) {
		HBoxContainer *row = memnew(HBoxContainer);
		row->add_theme_constant_override("separation", 4);
		root->add_child(row);
		for (const char *character = row_chars; *character; ++character) {
			_add_char_button(row, String::chr((char32_t)*character));
		}
	}

	HBoxContainer *ops = memnew(HBoxContainer);
	ops->add_theme_constant_override("separation", 6);
	root->add_child(ops);
	_add_action_button(ops, "Shift", "toggle_shift");
	_add_action_button(ops, "Space", "insert_text", String(" "));
	_add_action_button(ops, "Backspace", "backspace", Variant(), true);
	_add_action_button(ops, "Clear", "clear");

	HBoxContainer *session_ops = memnew(HBoxContainer);
	session_ops->add_theme_constant_override("separation", 6);
	root->add_child(session_ops);
	_add_action_button(session_ops, "Confirm", "confirm");
	_add_action_button(session_ops, "Cancel", "cancel");
}

void TagAsciiKeypad::_add_char_button(Control *p_parent, const String &p_label) {
	_add_action_button(p_parent, p_label, "insert_text", p_label);
}

void TagAsciiKeypad::_add_action_button(Control *p_parent, const String &p_label, const String &p_action_id, const Variant &p_payload, bool p_repeat) {
	KeypadActionButton *button = memnew(KeypadActionButton);
	button->set_text(p_label);
	button->set_action_id(p_action_id);
	button->set_action_payload(p_payload);
	button->set_repeat_mode(p_repeat ? "repeat" : "none");
	button->set_custom_minimum_size(Size2(40, 40));
	button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_parent->add_child(button);
}

void TagAsciiKeypad::_dispatch_compat_action(const Dictionary &p_request) {
	if (compatibility_session.is_null()) {
		return;
	}

	const String action_id = p_request.get("action_id", String());
	const Variant payload = p_request.get("payload", Variant());
	if (action_id == "confirm") {
		emit_signal(SNAME("value_confirmed"), compatibility_session->get_buffer_text());
		return;
	}
	if (action_id == "cancel") {
		compatibility_session->cancel();
		emit_signal(SNAME("canceled"));
		return;
	}
	if (compatibility_session->dispatch_edit_action(action_id, payload)) {
		refresh_from_session();
	}
}

void TagAsciiKeypad::open_for(const String &p_initial) {
	Dictionary options;
	options["initial"] = p_initial;
	open_for_options(options);
}

void TagAsciiKeypad::open_for_options(const Dictionary &p_opts) {
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = p_opts.get("initial", String());

	compatibility_session.instantiate();
	compatibility_session->configure(descriptor);
	bind_session(compatibility_session);
	set_action_dispatcher(callable_mp(this, &TagAsciiKeypad::_dispatch_compat_action));
}

String TagAsciiKeypad::get_buffer() const {
	return compatibility_session.is_valid() ? compatibility_session->get_buffer_text() : String();
}
