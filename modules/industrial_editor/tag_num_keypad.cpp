#include "tag_num_keypad.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/margin_container.h"
#include "scene/scene_string_names.h"

void TagNumKeypad::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_for", "initial"), &TagNumKeypad::open_for);
	ClassDB::bind_method(D_METHOD("get_buffer"), &TagNumKeypad::get_buffer);
	ClassDB::bind_method(D_METHOD("_on_digit", "digit"), &TagNumKeypad::_on_digit);
	ClassDB::bind_method(D_METHOD("_on_dot"), &TagNumKeypad::_on_dot);
	ClassDB::bind_method(D_METHOD("_on_sign"), &TagNumKeypad::_on_sign);
	ClassDB::bind_method(D_METHOD("_on_backspace"), &TagNumKeypad::_on_backspace);
	ClassDB::bind_method(D_METHOD("_on_clear"), &TagNumKeypad::_on_clear);

	ADD_SIGNAL(MethodInfo("value_confirmed", PropertyInfo(Variant::STRING, "text")));
}

void TagNumKeypad::ok_pressed() {
	// Fired from AcceptDialog::_ok_pressed before/around confirmed; carry the
	// typed buffer so TagNumInput can write even if the dialog is already hiding.
	emit_signal(SNAME("value_confirmed"), buffer);
}

TagNumKeypad::TagNumKeypad() {
	// ASCII-only UI strings (export templates have no editor TTR).
	set_title("Enter Value");
	set_flag(Window::FLAG_POPUP, true);
	set_exclusive(true);
	set_min_size(Size2(280, 360));
	add_cancel_button("Cancel");
	_rebuild_ui();
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

	display = memnew(Label);
	display->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	display->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	display->set_custom_minimum_size(Size2(0, 48));
	display->add_theme_font_size_override("font_size", 28);
	display->set_clip_text(true);
	root->add_child(display);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(3);
	grid->add_theme_constant_override("h_separation", 8);
	grid->add_theme_constant_override("v_separation", 8);
	root->add_child(grid);

	const char *digits[] = { "7", "8", "9", "4", "5", "6", "1", "2", "3" };
	for (const char *d : digits) {
		_add_key_button(grid, String(d), callable_mp(this, &TagNumKeypad::_on_digit).bind(String(d)));
	}
	_add_key_button(grid, "-", callable_mp(this, &TagNumKeypad::_on_sign));
	_add_key_button(grid, "0", callable_mp(this, &TagNumKeypad::_on_digit).bind(String("0")));
	_add_key_button(grid, ".", callable_mp(this, &TagNumKeypad::_on_dot));

	HBoxContainer *ops = memnew(HBoxContainer);
	ops->add_theme_constant_override("separation", 8);
	root->add_child(ops);

	Button *bs = memnew(Button);
	bs->set_text("Backspace");
	bs->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	bs->set_custom_minimum_size(Size2(0, 40));
	bs->connect(SceneStringName(pressed), callable_mp(this, &TagNumKeypad::_on_backspace));
	ops->add_child(bs);

	Button *clr = memnew(Button);
	clr->set_text("Clear");
	clr->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	clr->set_custom_minimum_size(Size2(0, 40));
	clr->connect(SceneStringName(pressed), callable_mp(this, &TagNumKeypad::_on_clear));
	ops->add_child(clr);

	_refresh_display();
}

void TagNumKeypad::_add_key_button(GridContainer *p_grid, const String &p_label, const Callable &p_cb) {
	Button *b = memnew(Button);
	b->set_text(p_label);
	b->set_custom_minimum_size(Size2(72, 48));
	b->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	b->connect(SceneStringName(pressed), p_cb);
	p_grid->add_child(b);
}

void TagNumKeypad::open_for(const String &p_initial) {
	buffer = p_initial.strip_edges();
	_refresh_display();
	popup_centered();
}

void TagNumKeypad::_refresh_display() {
	if (display) {
		display->set_text(buffer.is_empty() ? String("0") : buffer);
	}
}

void TagNumKeypad::_on_digit(const String &p_digit) {
	if (buffer == "0") {
		buffer = p_digit;
	} else if (buffer == "-0") {
		buffer = "-" + p_digit;
	} else {
		buffer += p_digit;
	}
	_refresh_display();
}

void TagNumKeypad::_on_dot() {
	if (buffer.contains(".")) {
		return;
	}
	if (buffer.is_empty() || buffer == "-") {
		buffer += "0.";
	} else {
		buffer += ".";
	}
	_refresh_display();
}

void TagNumKeypad::_on_sign() {
	if (buffer.begins_with("-")) {
		buffer = buffer.substr(1);
	} else if (!buffer.is_empty()) {
		buffer = "-" + buffer;
	} else {
		buffer = "-";
	}
	_refresh_display();
}

void TagNumKeypad::_on_backspace() {
	if (!buffer.is_empty()) {
		buffer = buffer.substr(0, buffer.length() - 1);
	}
	_refresh_display();
}

void TagNumKeypad::_on_clear() {
	buffer = "";
	_refresh_display();
}
