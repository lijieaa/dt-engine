#include "keypad_command_service.h"

#include "keypad_action.h"
#include "tag_widget_util.h"

#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "core/variant/array.h"
#include "scene/main/node.h"

bool KeypadCommandService::_command_id_is_safe(const String &p_command_id) {
	if (p_command_id.is_empty()) {
		return false;
	}
	// Reject method paths, object paths, and script-like identifiers.
	if (p_command_id.contains(".") || p_command_id.contains("/") || p_command_id.contains(":") ||
			p_command_id.contains(" ") || p_command_id.contains("(") || p_command_id.contains(")")) {
		return false;
	}
	return true;
}

bool KeypadCommandService::_read_tag_value(Node *p_owner, const String &p_tag, Variant &r_value, String &r_error) {
	Node *bridge = tag_widget::get_bridge(p_owner);
	if (bridge == nullptr) {
		r_error = "runtime unavailable";
		return false;
	}
	if (bridge->has_method("has_tag") && !(bool)bridge->call("has_tag", p_tag)) {
		r_error = "tag not found";
		return false;
	}
	if (bridge->has_method("get_tag")) {
		const Dictionary entry = bridge->call("get_tag", p_tag);
		if (entry.is_empty() && bridge->has_method("has_tag")) {
			r_error = "tag not found";
			return false;
		}
		r_value = entry.get("value", Variant());
		return true;
	}
	r_error = "tag read unavailable";
	return false;
}

bool KeypadCommandService::_dispatch_write_tag(Node *p_owner, const Dictionary &p_payload, String &r_error) const {
	if (!p_payload.has("tag_name") || !p_payload.has("value")) {
		r_error = "invalid payload";
		return false;
	}
	const String tag = p_payload["tag_name"];
	if (tag.is_empty()) {
		r_error = "invalid payload";
		return false;
	}
	const Variant value = p_payload["value"];
	Node *bridge = tag_widget::get_bridge(p_owner);
	if (bridge != nullptr && bridge->has_method("write_tag")) {
		// Prefer the live bridge result. Do not silently fall back to HTTP when
		// the bridge explicitly rejects the write (tests and play-mode hosts).
		if (!(bool)bridge->call("write_tag", tag, value, String())) {
			r_error = "write failed";
			return false;
		}
		r_error = String();
		return true;
	}
	if (!tag_widget::write_tag(p_owner, tag, value)) {
		r_error = "write failed";
		return false;
	}
	r_error = String();
	return true;
}

bool KeypadCommandService::_dispatch_toggle_tag(Node *p_owner, const Dictionary &p_payload, String &r_error) const {
	if (!p_payload.has("tag_name")) {
		r_error = "invalid payload";
		return false;
	}
	const String tag = p_payload["tag_name"];
	if (tag.is_empty()) {
		r_error = "invalid payload";
		return false;
	}
	Variant current;
	if (!_read_tag_value(p_owner, tag, current, r_error)) {
		return false;
	}
	const bool next = !tag_widget::to_bool(current);
	Node *bridge = tag_widget::get_bridge(p_owner);
	if (bridge != nullptr && bridge->has_method("write_tag")) {
		if (!(bool)bridge->call("write_tag", tag, next, String())) {
			r_error = "write failed";
			return false;
		}
		r_error = String();
		return true;
	}
	if (!tag_widget::write_tag(p_owner, tag, next)) {
		r_error = "write failed";
		return false;
	}
	r_error = String();
	return true;
}

bool KeypadCommandService::_dispatch_step_tag(Node *p_owner, const Dictionary &p_payload, bool p_increment, String &r_error) const {
	if (!p_payload.has("tag_name") || !p_payload.has("step")) {
		r_error = "invalid payload";
		return false;
	}
	const String tag = p_payload["tag_name"];
	if (tag.is_empty()) {
		r_error = "invalid payload";
		return false;
	}
	const Variant step_v = p_payload["step"];
	if (step_v.get_type() != Variant::INT && step_v.get_type() != Variant::FLOAT) {
		r_error = "invalid step";
		return false;
	}
	Variant current;
	if (!_read_tag_value(p_owner, tag, current, r_error)) {
		return false;
	}
	const double step = step_v.operator double();
	const double next = tag_widget::to_float(current) + (p_increment ? step : -step);
	Node *bridge = tag_widget::get_bridge(p_owner);
	if (bridge != nullptr && bridge->has_method("write_tag")) {
		if (!(bool)bridge->call("write_tag", tag, next, String())) {
			r_error = "write failed";
			return false;
		}
		r_error = String();
		return true;
	}
	if (!tag_widget::write_tag(p_owner, tag, next)) {
		r_error = "write failed";
		return false;
	}
	r_error = String();
	return true;
}

bool KeypadCommandService::_dispatch_call_command(const Dictionary &p_payload, String &r_error) {
	if (!p_payload.has("command_id")) {
		r_error = "invalid payload";
		return false;
	}
	const String command_id = p_payload["command_id"];
	if (!_command_id_is_safe(command_id)) {
		r_error = "invalid command id";
		return false;
	}
	if (!has_command(command_id)) {
		r_error = "unregistered command";
		return false;
	}
	const Callable &handler = commands[StringName(command_id)];
	if (!handler.is_valid()) {
		r_error = "invalid command handler";
		return false;
	}
	const Variant args = p_payload.get("args", Array());
	Variant ret;
	Callable::CallError ce;
	const Variant *argptrs[1] = { &args };
	handler.callp(argptrs, 1, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		r_error = "command call failed";
		return false;
	}
	if (ret.get_type() == Variant::BOOL && !(bool)ret) {
		r_error = "command rejected";
		return false;
	}
	r_error = String();
	return true;
}

bool KeypadCommandService::_dispatch_navigation(const String &p_action_id, const Dictionary &p_payload, String &r_error) const {
	if (!navigation_handler.is_valid()) {
		r_error = "navigation handler not set";
		print_line(vformat("KeypadCommandService: navigation %s rejected (no handler)", p_action_id));
		return false;
	}
	Variant ret;
	Callable::CallError ce;
	Variant action_v = p_action_id;
	Variant payload_v = p_payload;
	const Variant *argptrs[2] = { &action_v, &payload_v };
	navigation_handler.callp(argptrs, 2, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		r_error = "navigation call failed";
		return false;
	}
	if (ret.get_type() == Variant::BOOL && !(bool)ret) {
		r_error = "navigation rejected";
		return false;
	}
	r_error = String();
	return true;
}

bool KeypadCommandService::dispatch(Node *p_owner, const String &p_action_id, const Dictionary &p_payload, String &r_error) {
	r_error = String();
	if (p_action_id.is_empty()) {
		r_error = "empty action";
		return false;
	}
	if (keypad_action_id_is_navigation(p_action_id)) {
		return _dispatch_navigation(p_action_id, p_payload, r_error);
	}
	if (p_owner == nullptr || !p_owner->is_inside_tree()) {
		if (keypad_action_id_is_runtime_command(p_action_id) && p_action_id != "call_command") {
			r_error = "owner unavailable";
			return false;
		}
	}
	if (p_action_id == "write_tag" || p_action_id == "set_tag") {
		return _dispatch_write_tag(p_owner, p_payload, r_error);
	}
	if (p_action_id == "toggle_tag") {
		return _dispatch_toggle_tag(p_owner, p_payload, r_error);
	}
	if (p_action_id == "increment_tag") {
		return _dispatch_step_tag(p_owner, p_payload, true, r_error);
	}
	if (p_action_id == "decrement_tag") {
		return _dispatch_step_tag(p_owner, p_payload, false, r_error);
	}
	if (p_action_id == "call_command") {
		return _dispatch_call_command(p_payload, r_error);
	}
	r_error = "unsupported action";
	return false;
}

void KeypadCommandService::register_command(const String &p_command_id, const Callable &p_handler) {
	if (!_command_id_is_safe(p_command_id) || !p_handler.is_valid()) {
		return;
	}
	commands[StringName(p_command_id)] = p_handler;
}

void KeypadCommandService::unregister_command(const String &p_command_id) {
	commands.erase(StringName(p_command_id));
}

bool KeypadCommandService::has_command(const String &p_command_id) const {
	return commands.has(StringName(p_command_id));
}

void KeypadCommandService::set_navigation_handler(const Callable &p_handler) {
	navigation_handler = p_handler;
}
