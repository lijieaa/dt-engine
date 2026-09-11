#pragma once

#include "core/string/ustring.h"

/// Pure numeric keypad buffer + caret (insertion index 0..text.length()).
struct TagKeypadBuffer {
	String text;
	int caret = 0;
};

TagKeypadBuffer tag_keypad_buffer_from_initial(const String &p_initial);
void tag_keypad_insert_digit(TagKeypadBuffer &b, char32_t d);
void tag_keypad_insert_dot(TagKeypadBuffer &b);
void tag_keypad_toggle_sign(TagKeypadBuffer &b);
void tag_keypad_backspace(TagKeypadBuffer &b);
void tag_keypad_delete(TagKeypadBuffer &b);
void tag_keypad_clear(TagKeypadBuffer &b);
void tag_keypad_move_left(TagKeypadBuffer &b);
void tag_keypad_move_right(TagKeypadBuffer &b);
bool tag_keypad_inc(TagKeypadBuffer &b, double step = 1.0);
bool tag_keypad_dec(TagKeypadBuffer &b, double step = 1.0);

/// Display text with ASCII caret marker `|` at insertion point.
String tag_keypad_display_with_caret(const TagKeypadBuffer &b);
