#include "tag_keypad_buffer.h"

#include "core/math/math_funcs.h"
#include "core/string/ustring.h"

static void _clamp_caret(TagKeypadBuffer &b) {
	if (b.caret < 0) {
		b.caret = 0;
	}
	const int len = b.text.length();
	if (b.caret > len) {
		b.caret = len;
	}
}

TagKeypadBuffer tag_keypad_buffer_from_initial(const String &p_initial) {
	TagKeypadBuffer b;
	b.text = p_initial.strip_edges();
	b.caret = b.text.length();
	return b;
}

void tag_keypad_insert_digit(TagKeypadBuffer &b, char32_t d) {
	_clamp_caret(b);
	const String digit = String::chr(d);
	// Leading-zero replacement when caret at end (industrial pad feel).
	if (b.caret == b.text.length()) {
		if (b.text == "0") {
			b.text = digit;
			b.caret = b.text.length();
			return;
		}
		if (b.text == "-0") {
			b.text = "-" + digit;
			b.caret = b.text.length();
			return;
		}
	}
	b.text = b.text.substr(0, b.caret) + digit + b.text.substr(b.caret);
	b.caret += 1;
}

void tag_keypad_insert_dot(TagKeypadBuffer &b) {
	_clamp_caret(b);
	if (b.text.contains(".")) {
		return;
	}
	if (b.text.is_empty() || b.text == "-") {
		b.text = (b.text == "-") ? String("-0.") : String("0.");
		b.caret = b.text.length();
		return;
	}
	b.text = b.text.substr(0, b.caret) + "." + b.text.substr(b.caret);
	b.caret += 1;
}

void tag_keypad_toggle_sign(TagKeypadBuffer &b) {
	_clamp_caret(b);
	if (b.text.begins_with("-")) {
		b.text = b.text.substr(1);
		if (b.caret > 0) {
			b.caret -= 1;
		}
	} else if (!b.text.is_empty()) {
		b.text = "-" + b.text;
		b.caret += 1;
	} else {
		b.text = "-";
		b.caret = 1;
	}
	_clamp_caret(b);
}

void tag_keypad_backspace(TagKeypadBuffer &b) {
	_clamp_caret(b);
	if (b.caret <= 0 || b.text.is_empty()) {
		return;
	}
	b.text = b.text.substr(0, b.caret - 1) + b.text.substr(b.caret);
	b.caret -= 1;
}

void tag_keypad_delete(TagKeypadBuffer &b) {
	_clamp_caret(b);
	if (b.caret >= b.text.length()) {
		return;
	}
	b.text = b.text.substr(0, b.caret) + b.text.substr(b.caret + 1);
}

void tag_keypad_clear(TagKeypadBuffer &b) {
	b.text = "";
	b.caret = 0;
}

void tag_keypad_move_left(TagKeypadBuffer &b) {
	_clamp_caret(b);
	if (b.caret > 0) {
		b.caret -= 1;
	}
}

void tag_keypad_move_right(TagKeypadBuffer &b) {
	_clamp_caret(b);
	if (b.caret < b.text.length()) {
		b.caret += 1;
	}
}

static bool _apply_step(TagKeypadBuffer &b, double step) {
	String s = b.text.strip_edges();
	// Display shows placeholder "0" when empty; treat the same for Inc/Dec.
	if (s.is_empty()) {
		s = "0";
	}
	if (s == "-" || s == "." || s == "-.") {
		return false;
	}
	if (!s.is_valid_float() && !s.is_valid_int()) {
		return false;
	}
	const double v = s.to_float() + step;
	// Prefer integer string when result is whole and input had no dot.
	if (!s.contains(".") && Math::is_equal_approx(v, Math::round(v))) {
		b.text = String::num_int64((int64_t)Math::round(v));
	} else {
		b.text = String::num(v);
	}
	b.caret = b.text.length();
	return true;
}

bool tag_keypad_inc(TagKeypadBuffer &b, double step) {
	return _apply_step(b, step);
}

bool tag_keypad_dec(TagKeypadBuffer &b, double step) {
	return _apply_step(b, -step);
}

String tag_keypad_display_with_caret(const TagKeypadBuffer &b) {
	TagKeypadBuffer tmp = b;
	_clamp_caret(tmp);
	const String shown = tmp.text.is_empty() ? String("0") : tmp.text;
	int caret = tmp.caret;
	if (tmp.text.is_empty()) {
		// Show caret after the placeholder 0.
		caret = 1;
	}
	if (caret < 0) {
		caret = 0;
	}
	if (caret > shown.length()) {
		caret = shown.length();
	}
	return shown.substr(0, caret) + "|" + shown.substr(caret);
}
