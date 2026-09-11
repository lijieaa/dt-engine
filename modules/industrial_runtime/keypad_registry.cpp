#include "keypad_registry.h"

#include "keypad_view.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

void KeypadRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_keypad", "id", "scene"), &KeypadRegistry::register_keypad);
	ClassDB::bind_method(D_METHOD("register_keypad_path", "id", "path"), &KeypadRegistry::register_keypad_path);
	ClassDB::bind_method(D_METHOD("unregister_keypad", "id"), &KeypadRegistry::unregister_keypad);
	ClassDB::bind_method(D_METHOD("has_keypad", "id"), &KeypadRegistry::has_keypad);
	ClassDB::bind_method(D_METHOD("set_project_default_id", "id"), &KeypadRegistry::set_project_default_id);
}

KeypadRegistry::KeypadRegistry() {
	definitions["builtin_numeric"] = _make_builtin_definition("numeric");
	definitions["builtin_ascii"] = _make_builtin_definition("ascii");
	definitions["builtin_password"] = _make_builtin_definition("password");
}

String KeypadRegistry::_get_builtin_id(const String &p_input_mode) {
	if (p_input_mode == "password") {
		return "builtin_password";
	}
	if (p_input_mode == "ascii") {
		return "builtin_ascii";
	}
	return "builtin_numeric";
}

KeypadDefinition KeypadRegistry::_make_builtin_definition(const String &p_input_mode) {
	KeypadDefinition definition;
	definition.id = _get_builtin_id(p_input_mode);
	definition.backend_name = "builtin";
	definition.builtin = true;
	return definition;
}

bool KeypadRegistry::_definition_is_valid(const KeypadDefinition &p_definition) {
	if (p_definition.builtin) {
		return true;
	}
	if (p_definition.backend_name != "tscn") {
		return false;
	}

	Ref<PackedScene> packed_scene = p_definition.packed_scene;
	if (packed_scene.is_null() && !p_definition.scene_path.is_empty()) {
		Error error = OK;
		packed_scene = ResourceLoader::load(
				p_definition.scene_path,
				"PackedScene",
				ResourceLoader::CACHE_MODE_REUSE,
				&error);
		if (error != OK) {
			return false;
		}
	}
	if (packed_scene.is_null() || !packed_scene->can_instantiate()) {
		return false;
	}

	Node *instance = packed_scene->instantiate();
	if (instance == nullptr) {
		return false;
	}
	const bool valid_root = Object::cast_to<KeypadView>(instance) != nullptr;
	memdelete(instance);
	return valid_root;
}

const KeypadDefinition *KeypadRegistry::_find_valid(const String &p_id, bool p_explicit) const {
	if (p_id.is_empty()) {
		return nullptr;
	}

	const KeypadDefinition *definition = definitions.getptr(p_id);
	if (definition == nullptr) {
		if (p_explicit) {
			WARN_PRINT(vformat("Keypad definition '%s' was not found; using fallback.", p_id));
		}
		return nullptr;
	}
	if (!_definition_is_valid(*definition)) {
		if (p_explicit) {
			WARN_PRINT(vformat("Keypad definition '%s' is invalid; using fallback.", p_id));
		}
		return nullptr;
	}
	return definition;
}

String KeypadRegistry::_get_scene_default_id(Node *p_owner) {
	if (p_owner == nullptr) {
		return String();
	}

	Node *scene_root = p_owner->get_owner();
	if (scene_root == nullptr && p_owner->is_inside_tree() && p_owner->get_tree() != nullptr) {
		scene_root = p_owner->get_tree()->get_current_scene();
	}
	if (scene_root != nullptr && scene_root->has_meta("keypad_default_id")) {
		return scene_root->get_meta("keypad_default_id");
	}
	return String();
}

String KeypadRegistry::_get_window_default_id(Node *p_owner) {
	for (Node *current = p_owner; current != nullptr; current = current->get_parent()) {
		Window *window = Object::cast_to<Window>(current);
		if (window != nullptr && window->has_meta("keypad_default_id")) {
			return window->get_meta("keypad_default_id");
		}
	}
	return String();
}

void KeypadRegistry::register_keypad(const String &p_id, const Ref<PackedScene> &p_scene) {
	const String id = p_id.strip_edges();
	if (id.is_empty() || p_scene.is_null()) {
		return;
	}

	KeypadDefinition definition;
	definition.id = id;
	definition.packed_scene = p_scene;
	definition.backend_name = "tscn";
	definitions[id] = definition;
}

void KeypadRegistry::register_keypad_path(const String &p_id, const String &p_path) {
	const String id = p_id.strip_edges();
	const String path = p_path.strip_edges();
	if (id.is_empty() || path.is_empty()) {
		return;
	}

	KeypadDefinition definition;
	definition.id = id;
	definition.scene_path = path;
	definition.backend_name = "tscn";
	definitions[id] = definition;
}

void KeypadRegistry::unregister_keypad(const String &p_id) {
	if (p_id.is_empty()) {
		return;
	}

	const KeypadDefinition *definition = definitions.getptr(p_id);
	if (definition != nullptr && !definition->builtin) {
		definitions.erase(p_id);
	}
}

bool KeypadRegistry::has_keypad(const String &p_id) const {
	return definitions.has(p_id);
}

KeypadDefinition KeypadRegistry::resolve(const KeypadOpenRequest &p_request, Node *p_owner) const {
	if (p_request.scene_override.is_valid()) {
		KeypadDefinition override_definition;
		override_definition.id = p_request.keypad_id.is_empty() ? String("scene_override") : p_request.keypad_id;
		override_definition.packed_scene = p_request.scene_override;
		override_definition.backend_name = "tscn";
		if (_definition_is_valid(override_definition)) {
			return override_definition;
		}
		WARN_PRINT("The keypad scene override is invalid; using fallback.");
	}

	const KeypadDefinition *definition = _find_valid(p_request.keypad_id, true);
	if (definition != nullptr) {
		return *definition;
	}

	const String scene_default_id = _get_scene_default_id(p_owner);
	definition = _find_valid(scene_default_id, false);
	if (definition != nullptr) {
		return *definition;
	}

	const String window_default_id = _get_window_default_id(p_owner);
	definition = _find_valid(window_default_id, false);
	if (definition != nullptr) {
		return *definition;
	}

	const String configured_default_id = !project_default_id.is_empty() ?
			project_default_id :
			String(ProjectSettings::get_singleton()->get_setting("industrial_runtime/keypad/default_id", String()));
	definition = _find_valid(configured_default_id, false);
	if (definition != nullptr) {
		return *definition;
	}

	return _make_builtin_definition(p_request.input_mode);
}

void KeypadRegistry::set_project_default_id(const String &p_id) {
	project_default_id = p_id.strip_edges();
}
