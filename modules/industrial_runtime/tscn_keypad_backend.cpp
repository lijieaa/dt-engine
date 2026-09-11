#include "tscn_keypad_backend.h"

#include "input_session.h"
#include "keypad_view.h"

#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"

void TscnKeypadBackend::_bind_methods() {
}

bool TscnKeypadBackend::can_open(const KeypadDefinition &p_definition) const {
	return p_definition.backend_name == "tscn" &&
			(p_definition.packed_scene.is_valid() || !p_definition.scene_path.is_empty());
}

KeypadView *TscnKeypadBackend::open(const KeypadOpenRequest &p_request, const KeypadDefinition &p_definition) {
	if (!can_open(p_definition)) {
		return nullptr;
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
			WARN_PRINT(vformat("Unable to load keypad scene '%s'.", p_definition.scene_path));
			return nullptr;
		}
	}
	if (packed_scene.is_null() || !packed_scene->can_instantiate()) {
		return nullptr;
	}

	Node *instance = packed_scene->instantiate();
	if (instance == nullptr) {
		return nullptr;
	}

	KeypadView *view = Object::cast_to<KeypadView>(instance);
	if (view == nullptr) {
		WARN_PRINT("The keypad scene root must be a KeypadView.");
		memdelete(instance);
		return nullptr;
	}

	const Variant session_variant = p_request.session_context.get("session", Variant());
	if (session_variant.get_type() == Variant::OBJECT) {
		Ref<InputSession> session = session_variant;
		if (session.is_valid()) {
			view->bind_session(session);
		}
	}
	return view;
}

void TscnKeypadBackend::close(KeypadView *p_instance) {
	if (p_instance != nullptr) {
		memdelete(p_instance);
	}
}
