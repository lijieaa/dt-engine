#include "tag_num_keypad.h"

#include "keypad_action_button.h"
#include "keypad_display_label.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "scene/gui/box_container.h"
#include "scene/gui/control.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/margin_container.h"

void TagNumKeypad::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_for", "initial"), &TagNumKeypad::open_for);
	ClassDB::bind_method(D_METHOD("open_for_options", "opts"), &TagNumKeypad::open_for_options);
	ClassDB::bind_method(D_METHOD("get_buffer"), &TagNumKeypad::get_buffer);
	ClassDB::bind_method(D_METHOD("_dispatch_compat_action", "request"), &TagNumKeypad::_dispatch_compat_action);

	ADD_SIGNAL(MethodInfo("value_confirmed", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("canceled"));
}

TagNumKeypad::TagNumKeypad() {
	set_custom_minimum_size(Size2(280, 420));
	_rebuild_ui();
	set_action_dispatcher(callable_mp(this, &TagNumKeypad::_dispatch_compat_action));
}

void TagNumKeypad::_rebuild_ui() {
	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 12);
	margin->add_theme_constant_override("margin_right", 12);
	margin->add_theme_constant_override("margin_top", 8);
	margin->add_theme_constant_override("margin_bottom", 8);
	add_child(margin);

	VBoxContainer *root = memnew(VBoxContainer);
	root->add_theme_constant_override("separation", 10);
	margin->add_child(root);

	KeypadDisplayLabel *display = memnew(KeypadDisplayLabel);
	display->set_bind_role("input_display");
	display->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	display->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	display->set_custom_minimum_size(Size2(0, 48));
	display->add_theme_font_size_override("font_size", 28);
	display->set_clip_text(true);
	root->add_child(display);

	KeypadDisplayLabel *previous = memnew(KeypadDisplayLabel);
	previous->set_bind_role("previous_value");
	previous->set_visible(false);
	root->add_child(previous);

	KeypadDisplayLabel *range = memnew(KeypadDisplayLabel);
	range->set_bind_role("range_hint");
	range->set_visible(false);
	root->add_child(range);

	KeypadDisplayLabel *error = memnew(KeypadDisplayLabel);
	error->set_bind_role("error");
	error->set_visible(false);
	root->add_child(error);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(3);
	grid->add_theme_constant_override("h_separation", 8);
	grid->add_theme_constant_override("v_separation", 8);
	root->add_child(grid);

	const char *digits[] = { "7", "8", "9", "4", "5", "6", "1", "2", "3" };
	for (const char *digit : digits) {
		_add_key_button(grid, String(digit), "insert_text", String(digit));
	}
	_add_key_button(grid, "-", "toggle_sign");
	_add_key_button(grid, "0", "insert_text", String("0"));
	_add_key_button(grid, ".", "decimal");

	HBoxContainer *ops = memnew(HBoxContainer);
	ops->add_theme_constant_override("separation", 8);
	root->add_child(ops);
	_add_ops_button(ops, "Backspace", "backspace", Variant(), true);
	_add_ops_button(ops, "Clear", "clear");

	HBoxContainer *edit_ops = memnew(HBoxContainer);
	edit_ops->add_theme_constant_override("separation", 8);
	root->add_child(edit_ops);
	_add_ops_button(edit_ops, "Del", "delete");
	_add_ops_button(edit_ops, "<", "move_left", Variant(), true);
	_add_ops_button(edit_ops, ">", "move_right", Variant(), true);
	_add_ops_button(edit_ops, "+1", "increment", 1.0, true);
	_add_ops_button(edit_ops, "-1", "decrement", 1.0, true);

	HBoxContainer *session_ops = memnew(HBoxContainer);
	session_ops->add_theme_constant_override("separation", 8);
	root->add_child(session_ops);
	_add_ops_button(session_ops, "Confirm", "confirm");
	_add_ops_button(session_ops, "Cancel", "cancel");
}

void TagNumKeypad::_add_key_button(Control *p_parent, const String &p_label, const String &p_action_id, const Variant &p_payload, bool p_repeat) {
	KeypadActionButton *button = memnew(KeypadActionButton);
	button->set_text(p_label);
	button->set_action_id(p_action_id);
	button->set_action_payload(p_payload);
	button->set_repeat_mode(p_repeat ? "repeat" : "none");
	button->set_custom_minimum_size(Size2(72, 48));
	button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_parent->add_child(button);
}

void TagNumKeypad::_add_ops_button(Control *p_parent, const String &p_label, const String &p_action_id, const Variant &p_payload, bool p_repeat) {
	KeypadActionButton *button = memnew(KeypadActionButton);
	button->set_text(p_label);
	button->set_action_id(p_action_id);
	button->set_action_payload(p_payload);
	button->set_repeat_mode(p_repeat ? "repeat" : "none");
	button->set_custom_minimum_size(Size2(0, 40));
	button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_parent->add_child(button);
}

void TagNumKeypad::_dispatch_compat_action(const Dictionary &p_request) {
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

void TagNumKeypad::open_for(const String &p_initial) {
	Dictionary options;
	options["initial"] = p_initial;
	open_for_options(options);
}

void TagNumKeypad::open_for_options(const Dictionary &p_opts) {
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = p_opts.get("initial", String());
	descriptor["previous_value"] = p_opts.get("previous", descriptor["initial_value"]);
	descriptor["has_min"] = p_opts.get("has_min", false);
	descriptor["has_max"] = p_opts.get("has_max", false);
	descriptor["min_value"] = p_opts.get("min", 0.0);
	descriptor["max_value"] = p_opts.get("max", 100.0);

	compatibility_session.instantiate();
	compatibility_session->configure(descriptor);
	bind_session(compatibility_session);
	set_action_dispatcher(callable_mp(this, &TagNumKeypad::_dispatch_compat_action));
}

String TagNumKeypad::get_buffer() const {
	return compatibility_session.is_valid() ? compatibility_session->get_buffer_text() : String();
}
