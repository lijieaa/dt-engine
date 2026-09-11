#pragma once

#include "input_session.h"
#include "keypad_action.h"
#include "keypad_command_service.h"
#include "keypad_host.h"
#include "keypad_registry.h"
#include "tscn_keypad_backend.h"

#include "core/object/object.h"
#include "core/variant/callable.h"
#include "scene/main/node.h"

class InputSessionManager : public Node {
	GDCLASS(InputSessionManager, Node);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	InputSessionManager();

	static InputSessionManager *get_or_create(Node *p_from);

	int64_t begin_session(
			Object *p_owner,
			const Dictionary &p_descriptor,
			const Callable &p_validate,
			const Callable &p_commit,
			const Callable &p_cancel);
	bool dispatch_action(const KeypadActionRequest &p_request);
	bool dispatch_action(const String &p_action_id, const Variant &p_payload);
	bool confirm_active();
	void cancel_active();
	void end_for_owner(ObjectID p_owner_id, const String &p_reason);
	void end_for_screen();
	void end_for_window();
	void end_for_runtime();
	bool is_owner_active(ObjectID p_owner_id) const;
	Ref<InputSession> get_active_session() const { return active_session; }
	KeypadRegistry *get_keypad_registry() const { return keypad_registry.ptr(); }
	KeypadHost *get_keypad_host() const;
	KeypadCommandService *get_command_service() { return &command_service; }

private:
	Ref<InputSession> active_session;
	ObjectID active_owner_id;
	ObjectID active_view_id;
	Callable active_cancel_callback;
	Dictionary active_descriptor;
	Ref<KeypadRegistry> keypad_registry;
	Ref<TscnKeypadBackend> tscn_backend;
	KeypadCommandService command_service;
	ObjectID keypad_host_id;
	int64_t next_session_id = 1;

	void _clear_active(bool p_invoke_cancel);
	void _invoke_cancel_callback(const Callable &p_callback) const;
	bool _active_owner_is_inside_tree() const;
	void _present_active_session();
	bool _dispatch_view_request(const Dictionary &p_request);
	KeypadHost *_ensure_keypad_host();
	KeypadView *_get_active_view() const;
	KeypadView *_make_builtin_view(const KeypadOpenRequest &p_request) const;
	static bool _payload_is_empty(const Variant &p_payload);
};
