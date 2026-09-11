#pragma once

#include "keypad_backend.h"
#include "keypad_view.h"

#include "core/object/object_id.h"
#include "core/string/ustring.h"
#include "scene/gui/control.h"

class KeypadHost : public Control {
	GDCLASS(KeypadHost, Control);

	ObjectID session_view_id;
	ObjectID fixed_view_id;
	String session_presentation_mode;

	KeypadView *_get_view(ObjectID p_id) const;
	void _remove_view(ObjectID p_id);
	KeypadView *_adopt_session_view(KeypadView *p_view, const String &p_mode);
	void _configure_view(KeypadView *p_view);
	Rect2 _get_local_viewport() const;
	Rect2 _get_anchor_rect(const KeypadOpenRequest &p_request) const;
	void _place_centered(KeypadView *p_view);
	void _place_popup(KeypadView *p_view, const KeypadOpenRequest &p_request);
	void _place_direct(KeypadView *p_view);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	KeypadHost();

	KeypadView *show_system(KeypadView *p_view, const KeypadOpenRequest &p_request);
	KeypadView *show_popup(KeypadView *p_view, const KeypadOpenRequest &p_request);
	KeypadView *show_fixed(KeypadView *p_view, const KeypadOpenRequest &p_request);
	KeypadView *show_direct_window(KeypadView *p_view, const KeypadOpenRequest &p_request);
	void hide_popup();
	void hide_session_view();
	void close_fixed();
};
