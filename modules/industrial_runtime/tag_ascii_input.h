#pragma once

#include "scene/gui/line_edit.h"

#include "core/input/input_event.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "scene/resources/packed_scene.h"

class TagAsciiInput : public LineEdit {
	GDCLASS(TagAsciiInput, LineEdit);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	TagAsciiInput() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }

	void set_keypad_id(const String &p_v) { keypad_id = p_v; }
	String get_keypad_id() const { return keypad_id; }
	void set_keypad_scene_override(const Ref<PackedScene> &p_v) { keypad_scene_override = p_v; }
	Ref<PackedScene> get_keypad_scene_override() const { return keypad_scene_override; }
	void set_presentation_mode(const String &p_v) { presentation_mode = p_v; }
	String get_presentation_mode() const { return presentation_mode; }

	void set_hide_keypad_title(bool p_v) { hide_keypad_title = p_v; }
	bool get_hide_keypad_title() const { return hide_keypad_title; }
	void set_keypad_anchor(const String &p_v) { keypad_anchor = p_v; }
	String get_keypad_anchor() const { return keypad_anchor; }
	void set_keypad_screen_cell(int p_v) { keypad_screen_cell = p_v; }
	int get_keypad_screen_cell() const { return keypad_screen_cell; }
	void set_keypad_side(const String &p_v) { keypad_side = p_v; }
	String get_keypad_side() const { return keypad_side; }
	void set_keypad_align(const String &p_v) { keypad_align = p_v; }
	String get_keypad_align() const { return keypad_align; }

	Dictionary _validate_input_session_text(const String &p_text) const;
	Dictionary _commit_input_session_text(const String &p_text);
	void _cancel_input_session();
	void _begin_input_session();
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	Ref<PackedScene> keypad_scene_override;
	String keypad_id;
	String presentation_mode = "system";
	bool hide_keypad_title = false;
	String keypad_anchor = "center";
	int keypad_screen_cell = 4;
	String keypad_side = "bottom";
	String keypad_align = "center";

	void flash_error(const String &p_msg);
	void revert_error_style();
};
