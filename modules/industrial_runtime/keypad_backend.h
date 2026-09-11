#pragma once

#include "core/object/ref_counted.h"
#include "core/object/object_id.h"
#include "core/variant/dictionary.h"
#include "scene/resources/packed_scene.h"

class KeypadView;

struct KeypadOpenRequest {
	String keypad_id;
	String input_mode;
	String presentation_mode;
	ObjectID owner_id;
	ObjectID anchor_id;
	int64_t session_id = 0;
	Dictionary session_context;
	Ref<PackedScene> scene_override;
};

struct KeypadDefinition {
	String id;
	Ref<PackedScene> packed_scene;
	String scene_path;
	String backend_name;
	bool builtin = false;
};

class KeypadBackend : public RefCounted {
	GDCLASS(KeypadBackend, RefCounted);

public:
	virtual bool can_open(const KeypadDefinition &p_definition) const = 0;
	virtual KeypadView *open(const KeypadOpenRequest &p_request, const KeypadDefinition &p_definition) = 0;
	virtual void close(KeypadView *p_instance) = 0;
};
