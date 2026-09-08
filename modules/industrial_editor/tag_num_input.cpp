#include "tag_num_input.h"
#include "tag_num_keypad.h"

#include "tag_widget_util.h"
#include "widget_format.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/scene_string_names.h"

void TagNumInput::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag"), &TagNumInput::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TagNumInput::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_format_cfg", "cfg"), &TagNumInput::set_format_cfg);
	ClassDB::bind_method(D_METHOD("get_format_cfg"), &TagNumInput::get_format_cfg);
	ClassDB::bind_method(D_METHOD("validate_input", "text"), &TagNumInput::validate_input);
	ClassDB::bind_method(D_METHOD("_on_tag_changed", "tag", "v", "quality", "version", "ts_ms"), &TagNumInput::_on_tag_changed);
	ClassDB::bind_method(D_METHOD("_on_keypad_value_confirmed", "text"), &TagNumInput::_on_keypad_value_confirmed);
	ClassDB::bind_method(D_METHOD("_on_keypad_canceled"), &TagNumInput::_on_keypad_canceled);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "format_cfg"), "set_format_cfg", "get_format_cfg");
}

void TagNumInput::set_tag_name(const String &p_tag) {
	tag_name = p_tag;
	if (tag_name.is_empty()) {
		return;
	}
	tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagNumInput::_on_tag_changed));
}

Dictionary TagNumInput::validate_input(const String &p_text) {
	if (ClassDB::class_exists("WidgetFormat")) {
		const Variant r = WidgetFormat::parse_input(p_text, format_cfg);
		if (r.get_type() == Variant::DICTIONARY) {
			Dictionary d = r;
			if (d.has("ok")) {
				return d;
			}
		}
	}
	const String s = p_text.strip_edges();
	if (s.is_empty() || s == "-" || s == "." || s == "-.") {
		Dictionary d;
		d["ok"] = false;
		d["value"] = Variant();
		d["error"] = "input empty";
		return d;
	}
	Dictionary d;
	if (s.is_valid_int()) {
		d["ok"] = true;
		d["value"] = s.to_int();
		return d;
	}
	if (s.is_valid_float()) {
		d["ok"] = true;
		d["value"] = s.to_float();
		return d;
	}
	d["ok"] = false;
	d["value"] = Variant();
	d["error"] = "invalid number";
	return d;
}

void TagNumInput::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		set_editable(false);
		set_focus_mode(FOCUS_ALL);
		if (get_placeholder().is_empty()) {
			set_placeholder("Tap to enter...");
		}
		if (!tag_name.is_empty()) {
			tag_widget::subscribe(this, tag_widget::make_tags(tag_name), callable_mp(this, &TagNumInput::_on_tag_changed));
		}
	}
}

void TagNumInput::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		_open_keypad();
		accept_event();
		return;
	}
	Ref<InputEventScreenTouch> st = p_event;
	if (st.is_valid() && st->is_pressed()) {
		_open_keypad();
		accept_event();
		return;
	}
	LineEdit::gui_input(p_event);
}

void TagNumInput::_ensure_keypad() {
	if (keypad != nullptr) {
		return;
	}
	keypad = memnew(TagNumKeypad);
	add_child(keypad);
	keypad->connect(SNAME("value_confirmed"), callable_mp(this, &TagNumInput::_on_keypad_value_confirmed));
	keypad->connect(SNAME("canceled"), callable_mp(this, &TagNumInput::_on_keypad_canceled));
}

void TagNumInput::_open_keypad() {
	if (tag_name.is_empty()) {
		return;
	}
	_ensure_keypad();
	keypad_open = true;
	keypad->open_for(get_text());
}

void TagNumInput::_on_keypad_value_confirmed(const String &p_text) {
	keypad_open = false;
	_apply_input_text(p_text);
}

void TagNumInput::_on_keypad_canceled() {
	keypad_open = false;
}

void TagNumInput::_apply_input_text(const String &p_text) {
	if (tag_name.is_empty()) {
		return;
	}
	const Dictionary parsed = validate_input(p_text);
	if (!(parsed.has("ok") && parsed["ok"].operator bool())) {
		const String err = parsed.has("error") ? parsed["error"].operator String() : String("invalid");
		flash_error(err);
		print_line(vformat("TagNumInput: validate failed tag=%s text=%s err=%s", tag_name, p_text, err));
		return;
	}
	const Variant value = parsed["value"];
	set_text(value.operator String());
	const bool ok = tag_widget::write_tag(this, tag_name, value);
	if (!ok) {
		flash_error("write failed");
		print_line(vformat("TagNumInput: write_tag failed tag=%s value=%s (is Runtime/WS connected?)", tag_name, value));
		return;
	}
	print_line(vformat("TagNumInput: write_tag sent tag=%s value=%s", tag_name, value));
	revert_error_style();
}

void TagNumInput::_on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms) {
	(void)quality;
	(void)version;
	(void)ts_ms;
	if (tag != tag_name) {
		return;
	}
	if (keypad_open) {
		return;
	}
	set_text(v.operator String());
}

void TagNumInput::flash_error(const String &p_msg) {
	add_theme_color_override("font_color", Color(0.95f, 0.2f, 0.2f));
	set_tooltip_text(p_msg);
	if (is_inside_tree()) {
		Ref<SceneTreeTimer> t = get_tree()->create_timer(1.0);
		t->connect("timeout", callable_mp(this, &TagNumInput::revert_error_style));
	}
}

void TagNumInput::revert_error_style() {
	add_theme_color_override("font_color", Color(1, 1, 1));
	set_tooltip_text("");
}
