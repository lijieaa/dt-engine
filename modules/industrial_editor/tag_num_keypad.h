#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"

#include "core/string/ustring.h"
#include "core/variant/variant.h"

/// Modal numeric keypad for TagNumInput (industrial HMI style).
class TagNumKeypad : public AcceptDialog {
	GDCLASS(TagNumKeypad, AcceptDialog);

protected:
	static void _bind_methods();
	virtual void ok_pressed() override;

public:
	TagNumKeypad();

	void open_for(const String &p_initial);
	String get_buffer() const { return buffer; }

private:
	Label *display = nullptr;
	String buffer;

	void _rebuild_ui();
	void _refresh_display();
	void _on_digit(const String &p_digit);
	void _on_dot();
	void _on_sign();
	void _on_backspace();
	void _on_clear();
	void _add_key_button(class GridContainer *p_grid, const String &p_label, const Callable &p_cb);
};
