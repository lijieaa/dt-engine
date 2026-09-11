#include "tag_keypad_placement.h"

#include "core/math/math_funcs.h"

static Rect2 _clamp_in_viewport(Rect2 r, const Rect2 &viewport) {
	if (r.size.x > viewport.size.x) {
		r.size.x = viewport.size.x;
	}
	if (r.size.y > viewport.size.y) {
		r.size.y = viewport.size.y;
	}
	const real_t max_x = viewport.position.x + viewport.size.x - r.size.x;
	const real_t max_y = viewport.position.y + viewport.size.y - r.size.y;
	r.position.x = CLAMP(r.position.x, viewport.position.x, max_x);
	r.position.y = CLAMP(r.position.y, viewport.position.y, max_y);
	return r;
}

Rect2 tag_keypad_place_centered(const Size2 &pad_size, const Rect2 &viewport) {
	Rect2 r;
	r.size = pad_size;
	r.position = viewport.position + (viewport.size - pad_size) * 0.5f;
	return _clamp_in_viewport(r, viewport);
}

Rect2 tag_keypad_place_screen_cell(const Size2 &pad_size, const Rect2 &viewport, int cell) {
	cell = CLAMP(cell, 0, 8);
	const int col = cell % 3;
	const int row = cell / 3;
	const Size2 cell_size = viewport.size / 3.0f;
	const Point2 cell_origin = viewport.position + Point2(cell_size.x * (real_t)col, cell_size.y * (real_t)row);
	const Point2 cell_center = cell_origin + cell_size * 0.5f;
	Rect2 r;
	r.size = pad_size;
	r.position = cell_center - pad_size * 0.5f;
	return _clamp_in_viewport(r, viewport);
}

Rect2 tag_keypad_place_relative(const Size2 &pad_size, const Rect2 &viewport, const Rect2 &anchor_control,
		const String &side, const String &align) {
	Rect2 r;
	r.size = pad_size;

	const String s = side.to_lower();
	const String a = align.to_lower();
	const bool vertical_stack = (s == "top" || s == "bottom" || s.is_empty());

	if (s == "top") {
		r.position.y = anchor_control.position.y - pad_size.y;
	} else if (s == "left") {
		r.position.x = anchor_control.position.x - pad_size.x;
	} else if (s == "right") {
		r.position.x = anchor_control.position.x + anchor_control.size.x;
	} else {
		r.position.y = anchor_control.position.y + anchor_control.size.y;
	}

	if (vertical_stack) {
		if (a == "start") {
			r.position.x = anchor_control.position.x;
		} else if (a == "end") {
			r.position.x = anchor_control.position.x + anchor_control.size.x - pad_size.x;
		} else {
			r.position.x = anchor_control.position.x + (anchor_control.size.x - pad_size.x) * 0.5f;
		}
	} else {
		if (a == "start") {
			r.position.y = anchor_control.position.y;
		} else if (a == "end") {
			r.position.y = anchor_control.position.y + anchor_control.size.y - pad_size.y;
		} else {
			r.position.y = anchor_control.position.y + (anchor_control.size.y - pad_size.y) * 0.5f;
		}
	}

	return _clamp_in_viewport(r, viewport);
}
