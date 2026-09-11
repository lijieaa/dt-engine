#pragma once

#include "core/math/rect2.h"
#include "core/math/vector2.h"
#include "core/string/ustring.h"

Rect2 tag_keypad_place_centered(const Size2 &pad_size, const Rect2 &viewport);
Rect2 tag_keypad_place_screen_cell(const Size2 &pad_size, const Rect2 &viewport, int cell /*0..8*/);
Rect2 tag_keypad_place_relative(const Size2 &pad_size, const Rect2 &viewport, const Rect2 &anchor_control,
		const String &side /*top|bottom|left|right*/, const String &align /*start|center|end*/);
