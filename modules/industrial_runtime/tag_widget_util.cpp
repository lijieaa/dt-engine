#include "tag_widget_util.h"

#include "industrial_runtime_client.h"
#include "industrial_runtime_host.h"

#include "core/object/class_db.h"
#include "core/math/math_funcs.h"
#include "core/string/print_string.h"
#include "core/string/ustring.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

namespace tag_widget {

Node *get_bridge(Node *p_self) {
	if (p_self == nullptr || !p_self->is_inside_tree()) {
		return nullptr;
	}
	// Prefer existing /root/Runtime; otherwise spawn C++ IndustrialRuntimeHost
	// in play mode (editor canvas uses IndustrialEditorPlugin tag push).
	return IndustrialRuntimeHost::ensure_at_root(p_self);
}

bool subscribe(Node *p_self, const Array &p_tags, const Callable &p_on_tag_changed) {
	Node *bridge = get_bridge(p_self);
	if (bridge == nullptr) {
		return false;
	}
	// Wire the value-change signal once (Godot dedups identical connections).
	if (bridge->has_signal("tag_changed")) {
		bridge->connect("tag_changed", p_on_tag_changed);
	}
	// Subscribe tags through the bridge.
	if (bridge->has_method("subscribe")) {
		const Variant ret = bridge->call("subscribe", p_tags);
		return ret.operator bool();
	}
	return true;
}

bool write_tag(Node *p_self, const String &p_tag, const Variant &p_value, const String &p_rid) {
	(void)p_rid;
	if (p_self == nullptr || !p_self->is_inside_tree() || p_tag.is_empty()) {
		return false;
	}
	Node *bridge = get_bridge(p_self);
	if (bridge != nullptr && bridge->has_method("write_tag")) {
		const Variant ret = bridge->call("write_tag", p_tag, p_value, p_rid);
		if (ret.operator bool()) {
			return true;
		}
	}
	// Editor canvas (no /root/Runtime) or WS send failed: HTTP write to Go runtime.
	const bool queued = IndustrialRuntimeClient::write_tag(p_tag, p_value, p_self);
	if (!queued) {
		print_line(vformat("tag_widget: write_tag failed tag=%s (no Runtime WS and HTTP queue failed)", p_tag));
	}
	return queued;
}

float to_float(const Variant &p_v, float p_default) {
	switch (p_v.get_type()) {
		case Variant::INT:
			return float(p_v.operator int64_t());
		case Variant::FLOAT:
			return p_v.operator double();
		case Variant::STRING:
			return p_v.operator String().to_float();
		case Variant::BOOL:
			return p_v.operator bool() ? 1.0f : 0.0f;
		default:
			return p_default;
	}
}

bool to_bool(const Variant &p_v) {
	switch (p_v.get_type()) {
		case Variant::BOOL:
			return p_v.operator bool();
		case Variant::INT:
			return p_v.operator int64_t() != 0;
		case Variant::FLOAT:
			return Math::abs(p_v.operator double()) > 0.0001;
		case Variant::STRING: {
			const String s = p_v.operator String();
			return !(s == "0" || s == "" || s == "false" || s == "False" || s == "OFF" || s == "off");
		}
		default:
			return false;
	}
}

} // namespace tag_widget
