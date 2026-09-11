#include "keypad_host.h"

#include "tag_keypad_placement.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "scene/main/canvas_item.h"

void KeypadHost::_bind_methods() {
	ClassDB::bind_method(D_METHOD("hide_popup"), &KeypadHost::hide_popup);
	ClassDB::bind_method(D_METHOD("hide_session_view"), &KeypadHost::hide_session_view);
	ClassDB::bind_method(D_METHOD("close_fixed"), &KeypadHost::close_fixed);
}

KeypadHost::KeypadHost() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	set_z_index(1000);
	set_anchors_and_offsets_preset(PRESET_FULL_RECT);
}

KeypadView *KeypadHost::_get_view(ObjectID p_id) const {
	if (!p_id.is_valid()) {
		return nullptr;
	}
	return ObjectDB::get_instance<KeypadView>(p_id);
}

void KeypadHost::_remove_view(ObjectID p_id) {
	KeypadView *view = _get_view(p_id);
	if (view == nullptr) {
		return;
	}
	if (view->get_parent() == this) {
		remove_child(view);
	}
	memdelete(view);
}

void KeypadHost::_configure_view(KeypadView *p_view) {
	if (p_view == nullptr) {
		return;
	}
	if (p_view->get_parent() == nullptr) {
		add_child(p_view);
	}
	p_view->set_visible(true);
	p_view->set_mouse_filter(MOUSE_FILTER_STOP);
	p_view->set_z_index(1);
}

KeypadView *KeypadHost::_adopt_session_view(KeypadView *p_view, const String &p_mode) {
	if (p_view == nullptr) {
		return nullptr;
	}

	hide_session_view();
	_configure_view(p_view);
	session_view_id = p_view->get_instance_id();
	session_presentation_mode = p_mode;
	return p_view;
}

Rect2 KeypadHost::_get_local_viewport() const {
	Rect2 viewport(Point2(), get_size());
	if (viewport.size.x <= 0.0f || viewport.size.y <= 0.0f) {
		const Rect2 viewport_rect = get_viewport_rect();
		viewport.size = viewport_rect.size;
	}
	return viewport;
}

Rect2 KeypadHost::_get_anchor_rect(const KeypadOpenRequest &p_request) const {
	const Variant anchor_variant = p_request.session_context.get("anchor_control", Variant());
	if (anchor_variant.get_type() != Variant::OBJECT) {
		return Rect2();
	}

	Control *anchor = Object::cast_to<Control>(anchor_variant.get_validated_object());
	if (anchor == nullptr) {
		return Rect2();
	}

	Rect2 anchor_rect = anchor->get_global_rect();
	anchor_rect.position -= get_global_position();
	return anchor_rect;
}

void KeypadHost::_place_centered(KeypadView *p_view) {
	if (p_view == nullptr) {
		return;
	}
	const Rect2 viewport = _get_local_viewport();
	const Size2 view_size = p_view->get_presentation_size();
	p_view->set_size(view_size);
	const Rect2 placed = tag_keypad_place_centered(view_size, viewport);
	p_view->set_position(placed.position);
}

void KeypadHost::_place_popup(KeypadView *p_view, const KeypadOpenRequest &p_request) {
	if (p_view == nullptr) {
		return;
	}

	const Rect2 viewport = _get_local_viewport();
	const Size2 view_size = p_view->get_presentation_size();
	p_view->set_size(view_size);

	const String anchor_mode = p_request.session_context.get("anchor", String("center"));
	Rect2 placed;
	if (anchor_mode == "screen") {
		const int screen_cell = (int)p_request.session_context.get("screen_cell", 4);
		placed = tag_keypad_place_screen_cell(view_size, viewport, screen_cell);
	} else if (anchor_mode == "control") {
		const Rect2 anchor_rect = _get_anchor_rect(p_request);
		if (anchor_rect.size != Size2()) {
			const String side = p_request.session_context.get("side", String("bottom"));
			const String align = p_request.session_context.get("align", String("center"));
			placed = tag_keypad_place_relative(view_size, viewport, anchor_rect, side, align);
		} else {
			placed = tag_keypad_place_centered(view_size, viewport);
		}
	} else {
		placed = tag_keypad_place_centered(view_size, viewport);
	}
	p_view->set_position(placed.position);
}

void KeypadHost::_place_direct(KeypadView *p_view) {
	if (p_view == nullptr) {
		return;
	}
	p_view->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	p_view->set_visible(true);
}

KeypadView *KeypadHost::show_system(KeypadView *p_view, const KeypadOpenRequest &p_request) {
	KeypadView *view = _adopt_session_view(p_view, "system");
	_place_centered(view);
	(void)p_request;
	return view;
}

KeypadView *KeypadHost::show_popup(KeypadView *p_view, const KeypadOpenRequest &p_request) {
	KeypadView *view = _adopt_session_view(p_view, "popup");
	_place_popup(view, p_request);
	return view;
}

KeypadView *KeypadHost::show_fixed(KeypadView *p_view, const KeypadOpenRequest &p_request) {
	if (session_view_id.is_valid() && session_view_id != fixed_view_id) {
		hide_session_view();
	}

	KeypadView *fixed_view = _get_view(fixed_view_id);
	if (fixed_view != nullptr) {
		if (p_view != nullptr && p_view != fixed_view && p_view->get_parent() == nullptr) {
			memdelete(p_view);
		}
		fixed_view->set_visible(true);
		_place_centered(fixed_view);
		(void)p_request;
		return fixed_view;
	}

	if (p_view == nullptr) {
		return nullptr;
	}
	_configure_view(p_view);
	fixed_view_id = p_view->get_instance_id();
	_place_centered(p_view);
	(void)p_request;
	return p_view;
}

KeypadView *KeypadHost::show_direct_window(KeypadView *p_view, const KeypadOpenRequest &p_request) {
	KeypadView *view = _adopt_session_view(p_view, "direct_window");
	_place_direct(view);
	(void)p_request;
	return view;
}

void KeypadHost::hide_popup() {
	if (session_presentation_mode == "popup") {
		hide_session_view();
	}
}

void KeypadHost::hide_session_view() {
	KeypadView *view = _get_view(session_view_id);
	if (view == nullptr) {
		session_view_id = ObjectID();
		session_presentation_mode = String();
		return;
	}

	// A fixed view is screen-scoped and survives session replacement.
	if (session_view_id == fixed_view_id) {
		view->set_visible(true);
		session_view_id = ObjectID();
		session_presentation_mode = String();
		return;
	}

	_remove_view(session_view_id);
	session_view_id = ObjectID();
	session_presentation_mode = String();
}

void KeypadHost::close_fixed() {
	if (session_view_id == fixed_view_id) {
		session_view_id = ObjectID();
		session_presentation_mode = String();
	}
	_remove_view(fixed_view_id);
	fixed_view_id = ObjectID();
}

void KeypadHost::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		hide_session_view();
		close_fixed();
	}
}
