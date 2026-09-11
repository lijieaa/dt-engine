#pragma once

#include "input_session.h"
#include "keypad_action_button.h"

#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/callable.h"
#include "scene/gui/control.h"

class KeypadView : public Control {
	GDCLASS(KeypadView, Control);

	Ref<InputSession> active_session;
	Callable action_dispatcher;
	Callable action_handler;
	HashMap<StringName, Node *> role_nodes;
	Vector<ObjectID> action_button_ids;
	String error_message;

	void _scan_scene();
	void _scan_node(Node *p_node);
	void _disconnect_action_buttons();
	void _set_role_text(const StringName &p_role, const String &p_text);
	void _set_role_visible(const StringName &p_role, bool p_visible);
	void _fit_presentation_size();
	void _on_action_requested(const Dictionary &p_request);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	KeypadView();

	void bind_session(const Ref<InputSession> &p_session);
	void unbind_session();
	void refresh_from_session();
	void show_error(const String &p_message);
	void clear_error();
	Size2 get_presentation_size() const;
	void set_action_dispatcher(const Callable &p_dispatcher);
};
