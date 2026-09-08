#pragma once

#include "scene/gui/line_edit.h"

#include "core/input/input_event.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class TagNumKeypad;

/// TagNumInput - shows live tag value; click opens a modal numeric keypad.
/// Confirm writes via bridge; live updates are ignored while the keypad is open.
class TagNumInput : public LineEdit {
	GDCLASS(TagNumInput, LineEdit);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	TagNumInput() = default;

	void set_tag_name(const String &p_tag);
	String get_tag_name() const { return tag_name; }
	void set_format_cfg(const Dictionary &p_cfg) { format_cfg = p_cfg; }
	Dictionary get_format_cfg() const { return format_cfg; }

	Dictionary validate_input(const String &p_text);
	void _on_tag_changed(const String &tag, const Variant &v, const String &quality, int version, int ts_ms);

private:
	String tag_name;
	Dictionary format_cfg;
	TagNumKeypad *keypad = nullptr;
	bool keypad_open = false;

	void _ensure_keypad();
	void _open_keypad();
	void _on_keypad_value_confirmed(const String &p_text);
	void _on_keypad_canceled();
	void _apply_input_text(const String &p_text);
	void flash_error(const String &p_msg);
	void revert_error_style();
};
