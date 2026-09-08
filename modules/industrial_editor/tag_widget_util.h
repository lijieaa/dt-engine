#pragma once

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"

class Node;

/// Shared bridge helpers for the engine-native widget controls (Tag* classes).
///
/// In play mode, controls bind to a C++ IndustrialRuntimeHost at `/root/Runtime`
/// (auto-spawned; no GDScript autoload). In the editor, canvas updates come
/// from IndustrialEditorPlugin tag push instead.
namespace tag_widget {

/// Resolve `/root/Runtime` (C++ host or legacy Node), or nullptr in editor.
Node *get_bridge(Node *p_self);

/// Subscribe tags and connect `tag_changed` to `p_on_tag_changed`. Idempotent.
bool subscribe(Node *p_self, const Array &p_tags, const Callable &p_on_tag_changed);

/// Route a write through Runtime.write_tag(tag, value, rid).
bool write_tag(Node *p_self, const String &p_tag, const Variant &p_value, const String &p_rid = String());

/// Coerce a Variant to float, mirroring tag_meter.gd semantics:
/// INT -> (float)int, FLOAT -> float, STRING -> to_float(), BOOL -> 1.0/0.0.
float to_float(const Variant &p_v, float p_default = 0.0f);

/// Coerce a Variant to bool, mirroring tag_lamp/tag_switch.gd semantics:
/// BOOL -> itself, INT -> != 0, FLOAT -> |v| > 0.0001,
/// STRING -> not in {"0","","false","False","OFF","off"}.
bool to_bool(const Variant &p_v);

/// Build a single-tag subscription list. Mirrors the GDScript `[tag_name]`
/// array literal (Array::make is not available on this engine build).
inline Array make_tags(const String &p_tag) {
	Array tags;
	tags.append(p_tag);
	return tags;
}

} // namespace tag_widget