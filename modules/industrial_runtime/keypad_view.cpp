#include "keypad_view.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "scene/main/canvas_item.h"

void KeypadView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bind_session", "session"), &KeypadView::bind_session);
	ClassDB::bind_method(D_METHOD("unbind_session"), &KeypadView::unbind_session);
	ClassDB::bind_method(D_METHOD("refresh_from_session"), &KeypadView::refresh_from_session);
	ClassDB::bind_method(D_METHOD("show_error", "message"), &KeypadView::show_error);
	ClassDB::bind_method(D_METHOD("clear_error"), &KeypadView::clear_error);
	ClassDB::bind_method(D_METHOD("get_presentation_size"), &KeypadView::get_presentation_size);
	ClassDB::bind_method(D_METHOD("set_action_dispatcher", "dispatcher"), &KeypadView::set_action_dispatcher);
}

KeypadView::KeypadView() {
	action_handler = callable_mp(this, &KeypadView::_on_action_requested);
}

void KeypadView::bind_session(const Ref<InputSession> &p_session) {
	active_session = p_session;
	_scan_scene();
	clear_error();
	refresh_from_session();
}

void KeypadView::unbind_session() {
	active_session.unref();
	clear_error();
}

void KeypadView::set_action_dispatcher(const Callable &p_dispatcher) {
	action_dispatcher = p_dispatcher;
}

void KeypadView::_disconnect_action_buttons() {
	for (int i = 0; i < action_button_ids.size(); i++) {
		KeypadActionButton *button = ObjectDB::get_instance<KeypadActionButton>(action_button_ids[i]);
		if (button != nullptr && button->is_connected(SNAME("action_requested"), action_handler)) {
			button->disconnect(SNAME("action_requested"), action_handler);
		}
	}
	action_button_ids.clear();
}

void KeypadView::_scan_scene() {
	_disconnect_action_buttons();
	role_nodes.clear();
	for (int i = 0; i < get_child_count(); i++) {
		_scan_node(get_child(i));
	}
}

void KeypadView::_scan_node(Node *p_node) {
	if (p_node == nullptr) {
		return;
	}

	if (p_node->has_meta("keypad_bind_role")) {
		const String role = p_node->get_meta("keypad_bind_role");
		if (role == "input_display" || role == "previous_value" || role == "error" || role == "range_hint") {
			role_nodes[StringName(role)] = p_node;
		}
	}

	KeypadActionButton *button = Object::cast_to<KeypadActionButton>(p_node);
	if (button != nullptr) {
		if (!button->is_connected(SNAME("action_requested"), action_handler)) {
			button->connect(SNAME("action_requested"), action_handler);
		}
		action_button_ids.push_back(button->get_instance_id());
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_scan_node(p_node->get_child(i));
	}
}

void KeypadView::_set_role_text(const StringName &p_role, const String &p_text) {
	Node **node_ptr = role_nodes.getptr(p_role);
	if (node_ptr == nullptr || *node_ptr == nullptr || !(*node_ptr)->has_method(SNAME("set_text"))) {
		return;
	}
	(*node_ptr)->call(SNAME("set_text"), p_text);
}

void KeypadView::_set_role_visible(const StringName &p_role, bool p_visible) {
	Node **node_ptr = role_nodes.getptr(p_role);
	if (node_ptr == nullptr || *node_ptr == nullptr) {
		return;
	}
	CanvasItem *role_canvas = Object::cast_to<CanvasItem>(*node_ptr);
	if (role_canvas != nullptr) {
		role_canvas->set_visible(p_visible);
	}
}

void KeypadView::refresh_from_session() {
	if (active_session.is_null()) {
		return;
	}

	_set_role_text("input_display", active_session->get_display_text());
	_set_role_text("previous_value", active_session->get_previous_value());
	_set_role_text("range_hint", active_session->get_range_hint());
	_set_role_text("error", error_message);
	_set_role_visible("error", !error_message.is_empty());
}

void KeypadView::show_error(const String &p_message) {
	error_message = p_message;
	_set_role_text("error", error_message);
	_set_role_visible("error", !error_message.is_empty());
}

void KeypadView::clear_error() {
	error_message = String();
	_set_role_text("error", String());
	_set_role_visible("error", false);
}

Size2 KeypadView::get_presentation_size() const {
	return get_size().max(get_combined_minimum_size());
}

void KeypadView::_on_action_requested(const Dictionary &p_request) {
	if (action_dispatcher.is_valid()) {
		Array args;
		args.append(p_request);
		action_dispatcher.callv(args);
		refresh_from_session();
	}
}

void KeypadView::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		_scan_scene();
		refresh_from_session();
	}
}
