#pragma once

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class Node;

/// Allowlisted tag / registered-command / navigation dispatch for keypad actions.
/// Plain C++ type (not Object) so unit tests can stack-allocate it.
class KeypadCommandService {
public:
	bool dispatch(Node *p_owner, const String &p_action_id, const Dictionary &p_payload, String &r_error);
	void register_command(const String &p_command_id, const Callable &p_handler);
	void unregister_command(const String &p_command_id);
	bool has_command(const String &p_command_id) const;
	void set_navigation_handler(const Callable &p_handler);

private:
	HashMap<StringName, Callable> commands;
	Callable navigation_handler;

	static bool _command_id_is_safe(const String &p_command_id);
	static bool _read_tag_value(Node *p_owner, const String &p_tag, Variant &r_value, String &r_error);
	bool _dispatch_write_tag(Node *p_owner, const Dictionary &p_payload, String &r_error) const;
	bool _dispatch_toggle_tag(Node *p_owner, const Dictionary &p_payload, String &r_error) const;
	bool _dispatch_step_tag(Node *p_owner, const Dictionary &p_payload, bool p_increment, String &r_error) const;
	bool _dispatch_call_command(const Dictionary &p_payload, String &r_error);
	bool _dispatch_navigation(const String &p_action_id, const Dictionary &p_payload, String &r_error) const;
};
