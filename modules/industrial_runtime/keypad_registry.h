#pragma once

#include "keypad_backend.h"

#include "core/templates/hash_map.h"
#include "scene/main/node.h"

class KeypadRegistry : public RefCounted {
	GDCLASS(KeypadRegistry, RefCounted);

	HashMap<String, KeypadDefinition> definitions;
	String project_default_id;

	static String _get_builtin_id(const String &p_input_mode);
	static bool _definition_is_valid(const KeypadDefinition &p_definition);
	static String _get_scene_default_id(Node *p_owner);
	static String _get_window_default_id(Node *p_owner);
	const KeypadDefinition *_find_valid(const String &p_id, bool p_explicit) const;
	static KeypadDefinition _make_builtin_definition(const String &p_input_mode);

protected:
	static void _bind_methods();

public:
	KeypadRegistry();

	void register_keypad(const String &p_id, const Ref<PackedScene> &p_scene);
	void register_keypad_path(const String &p_id, const String &p_path);
	void unregister_keypad(const String &p_id);
	bool has_keypad(const String &p_id) const;
	KeypadDefinition resolve(const KeypadOpenRequest &p_request, Node *p_owner) const;
	void set_project_default_id(const String &p_id);
};
