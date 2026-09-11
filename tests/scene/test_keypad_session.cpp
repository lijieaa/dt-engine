#include "tests/test_macros.h"

TEST_FORCE_LINK(test_keypad_session)

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "modules/industrial_runtime/input_session.h"
#include "modules/industrial_runtime/input_session_manager.h"
#include "modules/industrial_runtime/keypad_action.h"
#include "modules/industrial_runtime/keypad_command_service.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

namespace KeypadCommandTest {

class RuntimeBridge : public Node {
	GDCLASS(RuntimeBridge, Node);

public:
	Dictionary values;
	bool write_allowed = true;
	int write_count = 0;
	String last_tag;
	Variant last_value;

	bool write_tag(const String &p_tag, const Variant &p_value, const String &) {
		if (!write_allowed) {
			return false;
		}
		write_count++;
		last_tag = p_tag;
		last_value = p_value;
		Dictionary entry;
		entry["tag"] = p_tag;
		entry["value"] = p_value;
		values[p_tag] = entry;
		return true;
	}

	Dictionary get_tag(const String &p_tag) const {
		const Variant *entry = values.getptr(p_tag);
		if (entry == nullptr) {
			return Dictionary();
		}
		return Dictionary(*entry);
	}

	bool has_tag(const String &p_tag) const {
		return values.has(p_tag);
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("write_tag", "tag", "value", "request_id"), &RuntimeBridge::write_tag);
		ClassDB::bind_method(D_METHOD("get_tag", "tag"), &RuntimeBridge::get_tag);
		ClassDB::bind_method(D_METHOD("has_tag", "tag"), &RuntimeBridge::has_tag);
	}
};

static RuntimeBridge *install_runtime_bridge() {
	static bool registered = false;
	if (!registered) {
		ClassDB::register_class<RuntimeBridge>();
		registered = true;
	}

	Window *root = SceneTree::get_singleton()->get_root();
	Node *existing = root->get_node_or_null(NodePath("Runtime"));
	if (existing != nullptr) {
		RuntimeBridge *bridge = Object::cast_to<RuntimeBridge>(existing);
		if (bridge != nullptr) {
			return bridge;
		}
		return nullptr;
	}

	RuntimeBridge *bridge = memnew(RuntimeBridge);
	bridge->set_name("Runtime");
	root->add_child(bridge);
	return bridge;
}

static int registered_command_count = 0;
static int navigation_count = 0;
static String last_navigation_action;
static Dictionary last_navigation_payload;

static bool registered_command_handler(const Variant &p_args) {
	(void)p_args;
	registered_command_count++;
	return true;
}

static bool navigation_handler(const String &p_action_id, const Dictionary &p_payload) {
	navigation_count++;
	last_navigation_action = p_action_id;
	last_navigation_payload = p_payload;
	return true;
}

} // namespace KeypadCommandTest

static Dictionary keypad_validation_failure(const String &) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = "out of range";
	return result;
}

static Dictionary keypad_commit_failure(const String &) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = "write failed";
	return result;
}

static int keypad_commit_count = 0;
static int keypad_cancel_count = 0;

static Dictionary keypad_counting_commit(const String &p_value) {
	keypad_commit_count++;
	Dictionary result;
	result["ok"] = true;
	result["value"] = p_value;
	return result;
}

static void keypad_counting_cancel() {
	keypad_cancel_count++;
}

TEST_SUITE("[Keypad][Session]") {

TEST_CASE("[Keypad][Session] classifies stable action ids") {
	CHECK(keypad_action_id_is_input("insert_text"));
	CHECK(keypad_action_id_is_input("confirm"));
	CHECK(keypad_action_id_is_navigation("switch_screen"));
	CHECK(keypad_action_id_is_runtime_command("write_tag"));
	CHECK_FALSE(keypad_action_id_is_input("write_tag"));
	CHECK_FALSE(keypad_action_id_is_runtime_command("not_registered"));
}

TEST_CASE("[Keypad][Session] edits numeric buffer with caret") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = "12";
	session->configure(descriptor);

	CHECK(session->dispatch_edit_action("move_left", Variant()));
	CHECK(session->dispatch_edit_action("insert_text", String("9")));
	CHECK(session->get_buffer_text() == "192");
	CHECK(session->dispatch_edit_action("backspace", Variant()));
	CHECK(session->get_buffer_text() == "12");
	CHECK(session->is_modified());
}

TEST_CASE("[Keypad][Session] masks password display without changing value") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "password";
	descriptor["initial_value"] = "secret";
	descriptor["mask_display"] = true;
	session->configure(descriptor);

	CHECK(session->get_buffer_text() == "secret");
	CHECK(session->get_display_text() == "******");
}

TEST_CASE("[Keypad][Session] edits ASCII text with caret and deletion") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "ab";
	session->configure(descriptor);

	CHECK(session->dispatch_edit_action("move_left", Variant()));
	CHECK(session->dispatch_edit_action("insert_text", String("X")));
	CHECK(session->get_buffer_text() == "aXb");
	CHECK(session->dispatch_edit_action("delete", Variant()));
	CHECK(session->get_buffer_text() == "aX");
	CHECK(session->dispatch_edit_action("backspace", Variant()));
	CHECK(session->get_buffer_text() == "a");
}

TEST_CASE("[Keypad][Session] validates numeric range before commit") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = "12";
	descriptor["has_min"] = true;
	descriptor["min_value"] = 20.0;
	session->configure(descriptor);

	const Dictionary result = session->validate_buffer();
	CHECK_FALSE((bool)result.get("ok", true));
	CHECK(result.get("error", String()) == "out of range");
	CHECK(session->is_active());
}

TEST_CASE("[Keypad][Session] uses custom out_of_range_message with placeholders") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = "150";
	descriptor["has_min"] = true;
	descriptor["min_value"] = 0.0;
	descriptor["has_max"] = true;
	descriptor["max_value"] = 100.0;
	descriptor["out_of_range_message"] = "Value must be {min}-{max}";
	session->configure(descriptor);

	const Dictionary result = session->validate_buffer();
	CHECK_FALSE((bool)result.get("ok", true));
	CHECK(result.get("error", String()) == "Value must be 0-100");
	CHECK(session->get_out_of_range_message() == "Value must be 0-100");
}

TEST_CASE("[Keypad][Session] accepts numeric values inside the configured range") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = "12";
	descriptor["has_min"] = true;
	descriptor["min_value"] = 10.0;
	descriptor["has_max"] = true;
	descriptor["max_value"] = 20.0;
	session->configure(descriptor);

	const Dictionary result = session->validate_buffer();
	CHECK((bool)result.get("ok", false));
	CHECK(result.get("value", String()) == "12");
}

TEST_CASE("[Keypad][Session] rejects non-printable ASCII input") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	session->configure(descriptor);

	CHECK_FALSE(session->dispatch_edit_action("insert_text", String::chr(1)));
	CHECK(session->get_buffer_text().is_empty());
}

TEST_CASE("[Keypad][Session] rejects non-numeric increment payload") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = "3";
	session->configure(descriptor);

	CHECK_FALSE(session->dispatch_edit_action("increment", String("bad")));
	CHECK(session->get_buffer_text() == "3");
}

TEST_CASE("[Keypad][Session] validation callback failure keeps session active") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	session->configure(descriptor);
	session->set_validation_callback(callable_mp_static(&keypad_validation_failure));

	const Dictionary result = session->commit_buffer();
	CHECK_FALSE((bool)result.get("ok", true));
	CHECK(session->is_active());
}

TEST_CASE("[Keypad][Session] commit callback failure keeps session active") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	session->configure(descriptor);
	session->set_commit_callback(callable_mp_static(&keypad_commit_failure));

	const Dictionary result = session->commit_buffer();
	CHECK_FALSE((bool)result.get("ok", true));
	CHECK(session->is_active());
}

TEST_CASE("[Keypad][Session] cancel does not invoke commit callback") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	session->configure(descriptor);
	keypad_commit_count = 0;
	session->set_commit_callback(callable_mp_static(&keypad_counting_commit));

	session->cancel();
	CHECK_FALSE(session->is_active());
	CHECK(keypad_commit_count == 0);
}

TEST_CASE("[Keypad][Session] rejects malformed action payloads") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	session->configure(descriptor);

	CHECK_FALSE(session->dispatch_edit_action("insert_text", Dictionary()));
	CHECK_FALSE(session->dispatch_edit_action("insert_text", Array()));
	CHECK_FALSE(session->dispatch_edit_action("increment", String("bad")));
	CHECK(session->get_buffer_text() == "abc");
}

TEST_CASE("[Keypad][Session] cancel ends editing without changing buffer value") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	session->configure(descriptor);

	CHECK(session->dispatch_edit_action("insert_text", String("x")));
	CHECK(session->get_buffer_text() == "abcx");
	session->cancel();
	CHECK_FALSE(session->is_active());
	CHECK(session->get_buffer_text() == "abcx");
}

TEST_CASE("[Keypad][SessionManager][SceneTree] clean session replacement assigns a new id") {
	InputSessionManager *manager = memnew(InputSessionManager);
	SceneTree::get_singleton()->get_root()->add_child(manager);
	Node *owner_a = memnew(Node);
	Node *owner_b = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner_a);
	SceneTree::get_singleton()->get_root()->add_child(owner_b);

	Dictionary first_descriptor;
	first_descriptor["input_mode"] = "ascii";
	first_descriptor["initial_value"] = "a";
	keypad_cancel_count = 0;
	const int64_t first_id = manager->begin_session(
			owner_a,
			first_descriptor,
			Callable(),
			Callable(),
			callable_mp_static(&keypad_counting_cancel));
	CHECK(first_id > 0);
	CHECK(manager->is_owner_active(owner_a->get_instance_id()));

	Dictionary second_descriptor;
	second_descriptor["input_mode"] = "ascii";
	second_descriptor["initial_value"] = "b";
	const int64_t second_id = manager->begin_session(owner_b, second_descriptor, Callable(), Callable(), Callable());
	CHECK(second_id > first_id);
	CHECK(manager->get_active_session()->get_session_id() == second_id);
	CHECK(manager->is_owner_active(owner_b->get_instance_id()));
	CHECK_FALSE(manager->is_owner_active(owner_a->get_instance_id()));
	CHECK(keypad_cancel_count == 1);

	memdelete(owner_a);
	memdelete(owner_b);
	memdelete(manager);
}

TEST_CASE("[Keypad][SessionManager][SceneTree] dirty replacement cancels without committing") {
	InputSessionManager *manager = memnew(InputSessionManager);
	SceneTree::get_singleton()->get_root()->add_child(manager);
	Node *owner_a = memnew(Node);
	Node *owner_b = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner_a);
	SceneTree::get_singleton()->get_root()->add_child(owner_b);

	keypad_commit_count = 0;
	keypad_cancel_count = 0;
	Dictionary first_descriptor;
	first_descriptor["input_mode"] = "ascii";
	first_descriptor["initial_value"] = "a";
	const int64_t first_id = manager->begin_session(
			owner_a,
			first_descriptor,
			Callable(),
			callable_mp_static(&keypad_counting_commit),
			callable_mp_static(&keypad_counting_cancel));
	CHECK(first_id > 0);
	CHECK(manager->dispatch_action("insert_text", String("x")));
	CHECK(manager->get_active_session()->is_modified());

	Dictionary second_descriptor;
	second_descriptor["input_mode"] = "ascii";
	second_descriptor["initial_value"] = "b";
	const int64_t second_id = manager->begin_session(owner_b, second_descriptor, Callable(), Callable(), Callable());
	CHECK(second_id > first_id);
	CHECK(keypad_commit_count == 0);
	CHECK(keypad_cancel_count == 1);
	CHECK(manager->get_active_session()->is_active());
	CHECK(manager->get_active_session()->get_buffer_text() == "b");

	memdelete(owner_a);
	memdelete(owner_b);
	memdelete(manager);
}

TEST_CASE("[Keypad][SessionManager][SceneTree] failed confirmation keeps the active session") {
	InputSessionManager *manager = memnew(InputSessionManager);
	SceneTree::get_singleton()->get_root()->add_child(manager);
	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);

	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	const int64_t session_id = manager->begin_session(owner, descriptor, Callable(), callable_mp_static(&keypad_commit_failure), Callable());
	CHECK(session_id > 0);
	CHECK_FALSE(manager->confirm_active());
	CHECK(manager->get_active_session()->is_active());
	CHECK(manager->get_active_session()->get_session_id() == session_id);

	memdelete(owner);
	memdelete(manager);
}

TEST_CASE("[Keypad][SessionManager][SceneTree] cancel_active invokes cancel without committing") {
	InputSessionManager *manager = memnew(InputSessionManager);
	SceneTree::get_singleton()->get_root()->add_child(manager);
	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);

	keypad_commit_count = 0;
	keypad_cancel_count = 0;
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	manager->begin_session(
			owner,
			descriptor,
			Callable(),
			callable_mp_static(&keypad_counting_commit),
			callable_mp_static(&keypad_counting_cancel));
	manager->cancel_active();

	CHECK(manager->get_active_session().is_null());
	CHECK(keypad_commit_count == 0);
	CHECK(keypad_cancel_count == 1);

	memdelete(owner);
	memdelete(manager);
}

TEST_CASE("[Keypad][SessionManager][SceneTree] owner leaving the tree terminates the session") {
	InputSessionManager *manager = memnew(InputSessionManager);
	SceneTree::get_singleton()->get_root()->add_child(manager);
	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);

	keypad_cancel_count = 0;
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	manager->begin_session(
			owner,
			descriptor,
			Callable(),
			Callable(),
			callable_mp_static(&keypad_counting_cancel));

	SceneTree::get_singleton()->get_root()->remove_child(owner);
	SceneTree::get_singleton()->process(0.0);

	CHECK(manager->get_active_session().is_null());
	CHECK(keypad_cancel_count == 1);

	memdelete(owner);
	memdelete(manager);
}

TEST_CASE("[Keypad][Command][SceneTree] allowlisted tag commands use the runtime bridge") {
	KeypadCommandTest::RuntimeBridge *bridge = KeypadCommandTest::install_runtime_bridge();
	REQUIRE(bridge != nullptr);

	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);
	KeypadCommandService service;

	Dictionary write_payload;
	write_payload["tag_name"] = "T1";
	write_payload["value"] = 12;
	String error;
	CHECK(service.dispatch(owner, "write_tag", write_payload, error));
	CHECK(error.is_empty());
	CHECK(bridge->last_tag == String("T1"));
	CHECK(bridge->last_value.operator int() == 12);

	Dictionary set_payload;
	set_payload["tag_name"] = "T2";
	set_payload["value"] = "ready";
	CHECK(service.dispatch(owner, "set_tag", set_payload, error));
	CHECK(bridge->last_tag == String("T2"));
	CHECK(bridge->last_value.operator String() == String("ready"));

	Dictionary toggle_payload;
	toggle_payload["tag_name"] = "T3";
	Dictionary initial_toggle;
	initial_toggle["tag"] = "T3";
	initial_toggle["value"] = false;
	bridge->values["T3"] = initial_toggle;
	CHECK(service.dispatch(owner, "toggle_tag", toggle_payload, error));
	CHECK(bridge->last_tag == "T3");
	CHECK((bool)bridge->last_value);

	Dictionary increment_payload;
	increment_payload["tag_name"] = "T4";
	increment_payload["step"] = 2;
	Dictionary initial_number;
	initial_number["tag"] = "T4";
	initial_number["value"] = 3;
	bridge->values["T4"] = initial_number;
	CHECK(service.dispatch(owner, "increment_tag", increment_payload, error));
	CHECK(bridge->last_tag == "T4");
	CHECK((double)bridge->last_value == 5.0);

	Dictionary decrement_payload;
	decrement_payload["tag_name"] = "T4";
	decrement_payload["step"] = 1;
	CHECK(service.dispatch(owner, "decrement_tag", decrement_payload, error));
	CHECK((double)bridge->last_value == 4.0);

	memdelete(owner);
}

TEST_CASE("[Keypad][Command][SceneTree] invalid payloads and write failures are rejected") {
	KeypadCommandTest::RuntimeBridge *bridge = KeypadCommandTest::install_runtime_bridge();
	REQUIRE(bridge != nullptr);

	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);
	KeypadCommandService service;
	String error;

	Dictionary missing_value;
	missing_value["tag_name"] = "T1";
	CHECK_FALSE(service.dispatch(owner, "write_tag", missing_value, error));
	CHECK_FALSE(error.is_empty());

	Dictionary invalid_step;
	invalid_step["tag_name"] = "T1";
	invalid_step["step"] = "fast";
	CHECK_FALSE(service.dispatch(owner, "increment_tag", invalid_step, error));

	Dictionary missing_tag;
	missing_tag["tag_name"] = "missing";
	CHECK_FALSE(service.dispatch(owner, "toggle_tag", missing_tag, error));

	bridge->write_allowed = false;
	Dictionary write_payload;
	write_payload["tag_name"] = "T1";
	write_payload["value"] = 1;
	CHECK_FALSE(service.dispatch(owner, "write_tag", write_payload, error));
	CHECK(error == "write failed");
	bridge->write_allowed = true;

	memdelete(owner);
}

TEST_CASE("[Keypad][Command][SceneTree] registered commands and navigation do not expose arbitrary calls") {
	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);
	KeypadCommandService service;
	String error;

	KeypadCommandTest::registered_command_count = 0;
	service.register_command("accept_recipe", callable_mp_static(&KeypadCommandTest::registered_command_handler));
	CHECK(service.has_command("accept_recipe"));

	Dictionary call_payload;
	call_payload["command_id"] = "accept_recipe";
	call_payload["args"] = Array();
	CHECK(service.dispatch(owner, "call_command", call_payload, error));
	CHECK(KeypadCommandTest::registered_command_count == 1);

	call_payload["command_id"] = "owner.call";
	CHECK_FALSE(service.dispatch(owner, "call_command", call_payload, error));
	call_payload["command_id"] = "unregistered";
	CHECK_FALSE(service.dispatch(owner, "call_command", call_payload, error));

	KeypadCommandTest::navigation_count = 0;
	service.set_navigation_handler(callable_mp_static(&KeypadCommandTest::navigation_handler));
	Dictionary navigation_payload;
	navigation_payload["screen"] = "next";
	CHECK(service.dispatch(owner, "switch_screen", navigation_payload, error));
	CHECK(KeypadCommandTest::navigation_count == 1);
	CHECK(KeypadCommandTest::last_navigation_action == "switch_screen");
	CHECK(KeypadCommandTest::last_navigation_payload == navigation_payload);

	service.set_navigation_handler(Callable());
	CHECK_FALSE(service.dispatch(owner, "switch_screen", navigation_payload, error));

	memdelete(owner);
}

TEST_CASE("[Keypad][SessionManager][Command][SceneTree] routes runtime and navigation actions") {
	InputSessionManager *manager = memnew(InputSessionManager);
	SceneTree::get_singleton()->get_root()->add_child(manager);
	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);

	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	const int64_t session_id = manager->begin_session(owner, descriptor, Callable(), Callable(), Callable());
	REQUIRE(session_id > 0);

	KeypadCommandService *service = manager->get_command_service();
	REQUIRE(service != nullptr);
	service->set_navigation_handler(callable_mp_static(&KeypadCommandTest::navigation_handler));

	Dictionary payload;
	payload["screen"] = "next";
	KeypadCommandTest::navigation_count = 0;
	CHECK(manager->dispatch_action("switch_screen", payload));
	CHECK(KeypadCommandTest::navigation_count == 1);
	CHECK(manager->get_active_session()->is_active());

	manager->cancel_active();
	memdelete(owner);
	memdelete(manager);
}

} // namespace
