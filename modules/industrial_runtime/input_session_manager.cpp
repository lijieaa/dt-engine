#include "input_session_manager.h"

#include "keypad_action.h"
#include "tag_ascii_keypad.h"
#include "tag_num_keypad.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/array.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

bool InputSessionManager::_payload_is_empty(const Variant &p_payload) {
	if (p_payload.get_type() == Variant::NIL) {
		return true;
	}
	return p_payload.get_type() == Variant::DICTIONARY && ((Dictionary)p_payload).is_empty();
}

void InputSessionManager::_bind_methods() {
	ClassDB::bind_static_method("InputSessionManager", D_METHOD("get_or_create", "from"), &InputSessionManager::get_or_create);
	ClassDB::bind_method(D_METHOD("begin_session", "owner", "descriptor", "validate", "commit", "cancel"), &InputSessionManager::begin_session);
	ClassDB::bind_method(
			D_METHOD("dispatch_action", "action_id", "payload"),
			static_cast<bool (InputSessionManager::*)(const String &, const Variant &)>(&InputSessionManager::dispatch_action));
	ClassDB::bind_method(D_METHOD("confirm_active"), &InputSessionManager::confirm_active);
	ClassDB::bind_method(D_METHOD("cancel_active"), &InputSessionManager::cancel_active);
	ClassDB::bind_method(D_METHOD("end_for_owner", "owner_id", "reason"), &InputSessionManager::end_for_owner);
	ClassDB::bind_method(D_METHOD("end_for_screen"), &InputSessionManager::end_for_screen);
	ClassDB::bind_method(D_METHOD("end_for_window"), &InputSessionManager::end_for_window);
	ClassDB::bind_method(D_METHOD("end_for_runtime"), &InputSessionManager::end_for_runtime);
	ClassDB::bind_method(D_METHOD("is_owner_active", "owner_id"), &InputSessionManager::is_owner_active);
	ClassDB::bind_method(D_METHOD("get_active_session"), &InputSessionManager::get_active_session);
	ClassDB::bind_method(D_METHOD("get_keypad_registry"), &InputSessionManager::get_keypad_registry);
	ClassDB::bind_method(D_METHOD("get_keypad_host"), &InputSessionManager::get_keypad_host);
}

InputSessionManager::InputSessionManager() {
	keypad_registry.instantiate();
	tscn_backend.instantiate();
}

InputSessionManager *InputSessionManager::get_or_create(Node *p_from) {
	if (p_from == nullptr || !p_from->is_inside_tree()) {
		return nullptr;
	}

	SceneTree *tree = p_from->get_tree();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return nullptr;
	}

	Window *root = tree->get_root();
	Node *existing = root->get_node_or_null(NodePath("InputSessionManager"));
	if (existing != nullptr) {
		InputSessionManager *manager = Object::cast_to<InputSessionManager>(existing);
		if (manager != nullptr) {
			manager->_ensure_keypad_host();
		}
		return manager;
	}

	InputSessionManager *manager = memnew(InputSessionManager);
	manager->set_name("InputSessionManager");
	root->add_child(manager);
	manager->_ensure_keypad_host();
	return manager;
}

KeypadHost *InputSessionManager::get_keypad_host() const {
	if (!keypad_host_id.is_valid()) {
		return nullptr;
	}
	return ObjectDB::get_instance<KeypadHost>(keypad_host_id);
}

KeypadHost *InputSessionManager::_ensure_keypad_host() {
	KeypadHost *host = get_keypad_host();
	if (host != nullptr) {
		return host;
	}

	SceneTree *tree = get_tree();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return nullptr;
	}

	Node *root = tree->get_root();
	Node *existing = root->get_node_or_null(NodePath("KeypadHost"));
	host = Object::cast_to<KeypadHost>(existing);
	if (host == nullptr) {
		host = memnew(KeypadHost);
		host->set_name("KeypadHost");
		root->add_child(host);
	}
	keypad_host_id = host->get_instance_id();
	return host;
}

int64_t InputSessionManager::begin_session(
		Object *p_owner,
		const Dictionary &p_descriptor,
		const Callable &p_validate,
		const Callable &p_commit,
		const Callable &p_cancel) {
	if (p_owner == nullptr) {
		return 0;
	}

	Node *owner_node = Object::cast_to<Node>(p_owner);
	if (owner_node == nullptr || !owner_node->is_inside_tree()) {
		return 0;
	}

	if (active_session.is_valid()) {
		_clear_active(true);
	}

	Dictionary descriptor = p_descriptor;
	const int64_t session_id = next_session_id++;
	descriptor["session_id"] = session_id;

	Ref<InputSession> session;
	session.instantiate();
	session->configure(descriptor);
	session->set_validation_callback(p_validate);
	session->set_commit_callback(p_commit);

	active_session = session;
	active_owner_id = p_owner->get_instance_id();
	active_cancel_callback = p_cancel;
	active_descriptor = descriptor;
	_ensure_keypad_host();
	set_process(true);
	callable_mp(this, &InputSessionManager::_present_active_session).call_deferred();
	return session_id;
}

bool InputSessionManager::dispatch_action(const KeypadActionRequest &p_request) {
	return dispatch_action(p_request.action_id, p_request.payload);
}

bool InputSessionManager::dispatch_action(const String &p_action_id, const Variant &p_payload) {
	if (!active_session.is_valid() || !active_session->is_active() || !_active_owner_is_inside_tree()) {
		if (active_session.is_valid()) {
			_clear_active(true);
		}
		return false;
	}

	if (p_action_id == "confirm") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		return confirm_active();
	}
	if (p_action_id == "cancel") {
		if (!_payload_is_empty(p_payload)) {
			return false;
		}
		cancel_active();
		return true;
	}

	if (keypad_action_id_is_runtime_command(p_action_id) || keypad_action_id_is_navigation(p_action_id)) {
		Dictionary payload;
		if (p_payload.get_type() == Variant::DICTIONARY) {
			payload = p_payload;
		} else if (!_payload_is_empty(p_payload)) {
			return false;
		}
		Object *owner_obj = ObjectDB::get_instance(active_owner_id);
		Node *owner = Object::cast_to<Node>(owner_obj);
		String error;
		const bool ok = command_service.dispatch(owner, p_action_id, payload, error);
		if (!ok) {
			KeypadView *view = _get_active_view();
			if (view != nullptr) {
				view->show_error(error.is_empty() ? String("action failed") : error);
			}
		}
		// Failed runtime/navigation must leave the session active.
		return ok;
	}

	if (!keypad_action_id_is_input(p_action_id)) {
		return false;
	}
	const bool handled = active_session->dispatch_edit_action(p_action_id, p_payload);
	if (handled) {
		KeypadView *view = _get_active_view();
		if (view != nullptr) {
			view->clear_error();
			view->refresh_from_session();
		}
	}
	return handled;
}

bool InputSessionManager::confirm_active() {
	if (!active_session.is_valid() || !active_session->is_active() || !_active_owner_is_inside_tree()) {
		return false;
	}

	const Dictionary result = active_session->commit_buffer();
	if (!(bool)result.get("ok", false)) {
		KeypadView *view = _get_active_view();
		if (view != nullptr) {
			view->show_error(result.get("error", String("commit failed")));
			view->refresh_from_session();
		}
		return false;
	}

	_clear_active(false);
	return true;
}

void InputSessionManager::cancel_active() {
	if (!active_session.is_valid()) {
		return;
	}
	_clear_active(true);
}

void InputSessionManager::end_for_owner(ObjectID p_owner_id, const String &p_reason) {
	(void)p_reason;
	if (active_session.is_valid() && active_owner_id == p_owner_id) {
		_clear_active(true);
	}
}

void InputSessionManager::end_for_screen() {
	cancel_active();
	KeypadHost *host = get_keypad_host();
	if (host != nullptr) {
		host->close_fixed();
	}
}

void InputSessionManager::end_for_window() {
	cancel_active();
	KeypadHost *host = get_keypad_host();
	if (host != nullptr) {
		host->close_fixed();
	}
}

void InputSessionManager::end_for_runtime() {
	cancel_active();
	KeypadHost *host = get_keypad_host();
	if (host != nullptr) {
		host->close_fixed();
	}
}

bool InputSessionManager::is_owner_active(ObjectID p_owner_id) const {
	return active_session.is_valid() &&
			active_session->is_active() &&
			active_owner_id == p_owner_id &&
			_active_owner_is_inside_tree();
}

void InputSessionManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			if (active_session.is_valid() && !_active_owner_is_inside_tree()) {
				_clear_active(true);
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_clear_active(true);
			KeypadHost *host = get_keypad_host();
			if (host != nullptr) {
				host->queue_free();
			}
		} break;
		default:
			break;
	}
}

void InputSessionManager::_clear_active(bool p_invoke_cancel) {
	if (!active_session.is_valid()) {
		active_owner_id = ObjectID();
		active_cancel_callback = Callable();
		set_process(false);
		return;
	}

	Callable cancel_callback = active_cancel_callback;
	KeypadView *view = _get_active_view();
	if (view != nullptr) {
		view->unbind_session();
	}
	KeypadHost *host = get_keypad_host();
	if (host != nullptr) {
		host->hide_session_view();
	}
	active_session->cancel();
	active_session.unref();
	active_owner_id = ObjectID();
	active_view_id = ObjectID();
	active_cancel_callback = Callable();
	active_descriptor.clear();
	set_process(false);

	if (p_invoke_cancel) {
		_invoke_cancel_callback(cancel_callback);
	}
}

void InputSessionManager::_invoke_cancel_callback(const Callable &p_callback) const {
	if (!p_callback.is_valid()) {
		return;
	}

	const ObjectID callback_owner_id = p_callback.get_object_id();
	if (callback_owner_id.is_valid() && ObjectDB::get_instance(callback_owner_id) == nullptr) {
		return;
	}
	p_callback.call();
}

bool InputSessionManager::_active_owner_is_inside_tree() const {
	if (!active_owner_id.is_valid()) {
		return false;
	}
	Node *owner = ObjectDB::get_instance<Node>(active_owner_id);
	return owner != nullptr && owner->is_inside_tree();
}

KeypadView *InputSessionManager::_get_active_view() const {
	if (!active_view_id.is_valid()) {
		return nullptr;
	}
	return ObjectDB::get_instance<KeypadView>(active_view_id);
}

KeypadView *InputSessionManager::_make_builtin_view(const KeypadOpenRequest &p_request) const {
	KeypadView *view = nullptr;
	if (p_request.input_mode == "ascii" || p_request.input_mode == "password") {
		view = memnew(TagAsciiKeypad);
	} else {
		view = memnew(TagNumKeypad);
	}
	if (p_request.input_mode == "ascii") {
		view->set_custom_minimum_size(Size2(520, 320));
	} else {
		view->set_custom_minimum_size(Size2(280, 420));
	}
	return view;
}

bool InputSessionManager::_dispatch_view_request(const Dictionary &p_request) {
	const String action_id = p_request.get("action_id", String());
	if (action_id.is_empty()) {
		return false;
	}
	return dispatch_action(action_id, p_request.get("payload", Variant()));
}

void InputSessionManager::_present_active_session() {
	if (!active_session.is_valid() || !active_session->is_active()) {
		return;
	}
	if (!_active_owner_is_inside_tree()) {
		_clear_active(true);
		return;
	}

	KeypadHost *host = _ensure_keypad_host();
	if (host == nullptr || keypad_registry.is_null() || tscn_backend.is_null()) {
		return;
	}

	Node *owner = ObjectDB::get_instance<Node>(active_owner_id);
	if (owner == nullptr) {
		return;
	}

	KeypadOpenRequest request;
	request.keypad_id = active_session->get_keypad_id();
	request.input_mode = active_session->get_input_mode();
	request.presentation_mode = active_session->get_presentation_mode();
	request.owner_id = active_owner_id;
	request.anchor_id = owner->get_instance_id();
	request.session_id = active_session->get_session_id();
	request.session_context = active_descriptor;
	request.session_context["session"] = active_session;
	const Variant scene_override = active_descriptor.get("keypad_scene_override", Variant());
	if (scene_override.get_type() == Variant::OBJECT) {
		request.scene_override = Ref<PackedScene>(Object::cast_to<PackedScene>(scene_override.get_validated_object()));
	}

	const Variant anchor_variant = active_descriptor.get("anchor_control", Variant());
	if (anchor_variant.get_type() == Variant::OBJECT) {
		Control *anchor = Object::cast_to<Control>(anchor_variant.get_validated_object());
		if (anchor != nullptr) {
			request.anchor_id = anchor->get_instance_id();
		}
	}

	const KeypadDefinition definition = keypad_registry->resolve(request, owner);
	KeypadView *view = nullptr;
	if (tscn_backend->can_open(definition)) {
		view = tscn_backend->open(request, definition);
	}
	if (view == nullptr) {
		view = _make_builtin_view(request);
	}

	KeypadView *presented = nullptr;
	if (request.presentation_mode == "system") {
		presented = host->show_system(view, request);
	} else if (request.presentation_mode == "fixed") {
		presented = host->show_fixed(view, request);
	} else if (request.presentation_mode == "direct_window") {
		presented = host->show_direct_window(view, request);
	} else {
		presented = host->show_popup(view, request);
	}
	if (presented == nullptr) {
		return;
	}

	presented->bind_session(active_session);
	presented->set_action_dispatcher(callable_mp(this, &InputSessionManager::_dispatch_view_request));
	presented->refresh_from_session();
	active_view_id = presented->get_instance_id();
}
