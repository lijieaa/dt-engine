#include "tests/test_macros.h"

TEST_FORCE_LINK(test_keypad_scene)

#include "modules/industrial_runtime/input_session.h"
#include "modules/industrial_runtime/keypad_action_button.h"
#include "modules/industrial_runtime/keypad_display_label.h"
#include "modules/industrial_runtime/keypad_host.h"
#include "modules/industrial_runtime/input_session_manager.h"
#include "modules/industrial_runtime/keypad_registry.h"
#include "modules/industrial_runtime/keypad_view.h"
#include "modules/industrial_runtime/tag_keypad_placement.h"
#include "modules/industrial_runtime/tag_ascii_keypad.h"
#include "modules/industrial_runtime/tag_num_keypad.h"
#include "modules/industrial_runtime/tscn_keypad_backend.h"

#include "core/object/callable_mp.h"
#include "core/io/resource_loader.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_utils.h"

static Dictionary scene_last_action;
static int scene_action_count = 0;

static bool keypad_scene_dispatch(const Dictionary &p_request) {
	scene_last_action = p_request;
	scene_action_count++;
	return true;
}

static KeypadActionButton *find_keypad_action_button(Node *p_node, const String &p_action_id, const Variant &p_payload) {
	if (p_node == nullptr) {
		return nullptr;
	}

	KeypadActionButton *button = Object::cast_to<KeypadActionButton>(p_node);
	if (button != nullptr && button->get_action_id() == p_action_id && button->get_action_payload() == p_payload) {
		return button;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		KeypadActionButton *result = find_keypad_action_button(p_node->get_child(i), p_action_id, p_payload);
		if (result != nullptr) {
			return result;
		}
	}
	return nullptr;
}

TEST_SUITE("[Keypad][SceneTree]") {

TEST_CASE("[Keypad][SceneTree] view refreshes roles and forwards button actions") {
	KeypadView *view = memnew(KeypadView);
	KeypadDisplayLabel *display = memnew(KeypadDisplayLabel);
	display->set_bind_role("input_display");
	view->add_child(display);

	KeypadDisplayLabel *previous = memnew(KeypadDisplayLabel);
	previous->set_bind_role("previous_value");
	previous->set_visible(false);
	view->add_child(previous);

	KeypadDisplayLabel *range = memnew(KeypadDisplayLabel);
	range->set_bind_role("range_hint");
	range->set_visible(false);
	view->add_child(range);

	KeypadDisplayLabel *min_label = memnew(KeypadDisplayLabel);
	min_label->set_bind_role("min_value");
	min_label->set_visible(false);
	view->add_child(min_label);

	KeypadDisplayLabel *max_label = memnew(KeypadDisplayLabel);
	max_label->set_bind_role("max_value");
	max_label->set_visible(false);
	view->add_child(max_label);

	KeypadDisplayLabel *error = memnew(KeypadDisplayLabel);
	error->set_bind_role("error");
	error->set_visible(false);
	view->add_child(error);

	KeypadActionButton *button = memnew(KeypadActionButton);
	button->set_action_id("insert_text");
	button->set_action_payload("9");
	view->add_child(button);

	SceneTree::get_singleton()->get_root()->add_child(view);

	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = "12";
	descriptor["previous_value"] = "10";
	descriptor["has_min"] = true;
	descriptor["min_value"] = 0.0;
	descriptor["has_max"] = true;
	descriptor["max_value"] = 99.0;
	session->configure(descriptor);

	scene_last_action.clear();
	scene_action_count = 0;
	view->set_action_dispatcher(callable_mp_static(&keypad_scene_dispatch));
	view->bind_session(session);

	CHECK(display->get_text() == "12");
	CHECK(previous->get_text() == "10");
	CHECK(range->get_text() == "0 - 99");
	CHECK(min_label->get_text() == "0");
	CHECK(max_label->get_text() == "99");
	CHECK(previous->is_visible());
	CHECK(range->is_visible());
	CHECK(min_label->is_visible());
	CHECK(max_label->is_visible());

	view->show_error("invalid value");
	CHECK(error->is_visible());
	CHECK(error->get_text() == "invalid value");
	view->clear_error();
	CHECK(error->is_visible());
	CHECK(error->get_text().is_empty());

	button->press();
	CHECK(scene_action_count == 1);
	CHECK(scene_last_action.get("action_id", String()) == "insert_text");
	CHECK(scene_last_action.get("payload", Variant()) == "9");
	CHECK((int64_t)scene_last_action.get("source_id", int64_t(0)) == (int64_t)button->get_instance_id());
	CHECK_FALSE((bool)scene_last_action.get("repeat", true));

	memdelete(view);
}

TEST_CASE("[Keypad][SceneTree] view still accepts legacy metadata bind roles") {
	KeypadView *view = memnew(KeypadView);
	Label *display = memnew(Label);
	display->set_meta("keypad_bind_role", "input_display");
	view->add_child(display);
	SceneTree::get_singleton()->get_root()->add_child(view);

	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = "5";
	session->configure(descriptor);
	view->bind_session(session);
	CHECK(display->get_text() == "5");

	memdelete(view);
}

TEST_CASE("[Keypad][SceneTree] action button rejects empty action ids") {
	KeypadActionButton *button = memnew(KeypadActionButton);
	button->set_action_payload("ignored");
	button->press();
	CHECK(button->get_action_id().is_empty());
	memdelete(button);
}

TEST_CASE("[Keypad][SceneTree] TSCN backend opens only a KeypadView root") {
	Ref<PackedScene> packed_scene = memnew(PackedScene);
	KeypadView *source = memnew(KeypadView);
	CHECK(packed_scene->pack(source) == OK);
	memdelete(source);

	TscnKeypadBackend backend;
	KeypadOpenRequest request;
	request.input_mode = "numeric";
	request.presentation_mode = "popup";
	KeypadDefinition definition;
	definition.id = "test_numeric";
	definition.packed_scene = packed_scene;
	definition.backend_name = "tscn";

	KeypadView *opened = backend.open(request, definition);
	CHECK(opened != nullptr);
	backend.close(opened);

	Ref<PackedScene> invalid_scene = memnew(PackedScene);
	Control *invalid_root = memnew(Control);
	CHECK(invalid_scene->pack(invalid_root) == OK);
	memdelete(invalid_root);
	definition.packed_scene = invalid_scene;
	CHECK(backend.open(request, definition) == nullptr);
}

TEST_CASE("[Keypad][SceneTree] registry resolves explicit and contextual defaults") {
	KeypadRegistry registry;
	Ref<PackedScene> explicit_scene = memnew(PackedScene);
	KeypadView *scene_root = memnew(KeypadView);
	CHECK(explicit_scene->pack(scene_root) == OK);
	memdelete(scene_root);
	registry.register_keypad("custom_numeric", explicit_scene);

	KeypadOpenRequest request;
	request.input_mode = "numeric";
	request.keypad_id = "custom_numeric";
	CHECK(registry.resolve(request, nullptr).id == "custom_numeric");

	Node *screen_root = memnew(Node);
	Node *owner = memnew(Node);
	screen_root->add_child(owner);
	owner->set_owner(screen_root);
	SceneTree::get_singleton()->get_root()->add_child(screen_root);
	screen_root->set_meta("keypad_default_id", "custom_numeric");

	request.keypad_id = String();
	CHECK(registry.resolve(request, owner).id == "custom_numeric");

	Window *window = SceneTree::get_singleton()->get_root();
	window->set_meta("keypad_default_id", "custom_numeric");
	screen_root->remove_meta("keypad_default_id");
	CHECK(registry.resolve(request, owner).id == "custom_numeric");
	window->remove_meta("keypad_default_id");

	registry.set_project_default_id("custom_numeric");
	CHECK(registry.resolve(request, nullptr).id == "custom_numeric");

	memdelete(screen_root);
}

TEST_CASE("[Keypad][SceneTree] registry falls back from an invalid override to mode default") {
	KeypadRegistry registry;
	Ref<PackedScene> invalid_scene = memnew(PackedScene);
	Control *invalid_root = memnew(Control);
	CHECK(invalid_scene->pack(invalid_root) == OK);
	memdelete(invalid_root);

	KeypadOpenRequest request;
	request.input_mode = "password";
	request.scene_override = invalid_scene;
	const KeypadDefinition result = registry.resolve(request, nullptr);
	CHECK(result.builtin);
	CHECK(result.id == "builtin_password");
}

TEST_CASE("[Keypad][SceneTree] host presents embedded views in every mode") {
	KeypadHost *host = memnew(KeypadHost);
	host->set_size(Size2(320, 240));
	SceneTree::get_singleton()->get_root()->add_child(host);

	KeypadOpenRequest request;
	request.input_mode = "numeric";
	request.presentation_mode = "system";
	request.session_context["anchor"] = "center";

	KeypadView *system_view = memnew(KeypadView);
	system_view->set_custom_minimum_size(Size2(100, 60));
	CHECK(host->show_system(system_view, request) == system_view);
	CHECK(system_view->get_parent() == host);
	CHECK(system_view->is_visible());

	request.presentation_mode = "popup";
	request.session_context["anchor"] = "screen";
	request.session_context["screen_cell"] = 8;
	KeypadView *popup_view = memnew(KeypadView);
	popup_view->set_custom_minimum_size(Size2(120, 80));
	CHECK(host->show_popup(popup_view, request) == popup_view);
	CHECK(popup_view->get_parent() == host);
	CHECK(popup_view->is_visible());
	CHECK(host->get_rect().encloses(popup_view->get_rect()));

	request.presentation_mode = "direct_window";
	KeypadView *direct_view = memnew(KeypadView);
	direct_view->set_custom_minimum_size(Size2(90, 50));
	CHECK(host->show_direct_window(direct_view, request) == direct_view);
	CHECK(direct_view->get_parent() == host);
	CHECK(direct_view->is_visible());

	request.presentation_mode = "fixed";
	KeypadView *fixed_view = memnew(KeypadView);
	fixed_view->set_custom_minimum_size(Size2(80, 40));
	KeypadView *fixed_result = host->show_fixed(fixed_view, request);
	CHECK(fixed_result == fixed_view);
	CHECK(host->get_child_count() == 1);
	KeypadView *replacement = memnew(KeypadView);
	CHECK(host->show_fixed(replacement, request) == fixed_view);
	CHECK(host->get_child_count() == 1);

	host->hide_session_view();
	host->hide_popup();
	host->close_fixed();
	memdelete(host);
}

TEST_CASE("[Keypad][SceneTree] popup placement uses control anchor and remains clamped") {
	KeypadHost *host = memnew(KeypadHost);
	SceneTree::get_singleton()->get_root()->add_child(host);
	host->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT, Control::PRESET_MODE_KEEP_SIZE);
	host->set_size(Size2(320, 240));

	Control *anchor = memnew(Control);
	anchor->set_position(Point2(280, 210));
	anchor->set_size(Size2(30, 20));
	SceneTree::get_singleton()->get_root()->add_child(anchor);

	KeypadOpenRequest request;
	request.input_mode = "numeric";
	request.presentation_mode = "popup";
	request.session_context["anchor"] = "control";
	request.session_context["anchor_control"] = anchor;
	request.session_context["side"] = "bottom";
	request.session_context["align"] = "center";

	KeypadView *view = memnew(KeypadView);
	view->set_custom_minimum_size(Size2(120, 80));
	CHECK(host->show_popup(view, request) == view);
	CHECK(host->get_rect().encloses(view->get_rect()));
	CHECK(view->get_position().x <= 200);
	CHECK(view->get_position().y <= 160);

	memdelete(host);
	memdelete(anchor);
}

TEST_CASE("[Keypad][SceneTree] manager opens a custom keypad after deferred presentation") {
	InputSessionManager *manager = InputSessionManager::get_or_create(SceneTree::get_singleton()->get_root());
	REQUIRE(manager != nullptr);
	KeypadRegistry *registry = manager->get_keypad_registry();
	REQUIRE(registry != nullptr);

	Ref<PackedScene> packed_scene = memnew(PackedScene);
	KeypadView *source = memnew(KeypadView);
	source->set_custom_minimum_size(Size2(100, 60));
	CHECK(packed_scene->pack(source) == OK);
	memdelete(source);
	registry->register_keypad("deferred_numeric", packed_scene);

	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);

	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["keypad_id"] = "deferred_numeric";
	descriptor["presentation_mode"] = "popup";
	descriptor["initial_value"] = "42";
	descriptor["anchor"] = "center";

	const int64_t session_id = manager->begin_session(owner, descriptor, Callable(), Callable(), Callable());
	CHECK(session_id > 0);
	CHECK(manager->get_keypad_host()->get_child_count() == 0);

	SceneTree::get_singleton()->process(0);
	KeypadHost *host = manager->get_keypad_host();
	REQUIRE(host != nullptr);
	REQUIRE(host->get_child_count() == 1);
	KeypadView *view = Object::cast_to<KeypadView>(host->get_child(0));
	REQUIRE(view != nullptr);
	CHECK(view->is_visible());

	manager->cancel_active();
	CHECK(host->get_child_count() == 0);
	memdelete(owner);
}

TEST_CASE("[Keypad][SceneTree] builtin fallback views expose session action buttons") {
	InputSessionManager *manager = memnew(InputSessionManager);
	SceneTree::get_singleton()->get_root()->add_child(manager);
	Node *numeric_owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(numeric_owner);

	Dictionary numeric_descriptor;
	numeric_descriptor["input_mode"] = "numeric";
	numeric_descriptor["initial_value"] = "12";
	const int64_t numeric_session_id = manager->begin_session(numeric_owner, numeric_descriptor, Callable(), Callable(), Callable());
	REQUIRE(numeric_session_id > 0);
	SceneTree::get_singleton()->process(0.0);

	KeypadHost *host = manager->get_keypad_host();
	REQUIRE(host != nullptr);
	REQUIRE(host->get_child_count() == 1);
	TagNumKeypad *numeric_view = Object::cast_to<TagNumKeypad>(host->get_child(0));
	REQUIRE(numeric_view != nullptr);
	if (numeric_view == nullptr) {
		memdelete(numeric_owner);
		memdelete(manager);
		return;
	}
	KeypadActionButton *numeric_seven = find_keypad_action_button(numeric_view, "insert_text", "7");
	REQUIRE(numeric_seven != nullptr);
	if (numeric_seven == nullptr) {
		manager->cancel_active();
		memdelete(numeric_owner);
		memdelete(manager);
		return;
	}
	numeric_seven->press();
	CHECK(manager->get_active_session()->get_buffer_text() == "127");

	manager->cancel_active();
	memdelete(numeric_owner);

	Node *ascii_owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(ascii_owner);
	Dictionary ascii_descriptor;
	ascii_descriptor["input_mode"] = "ascii";
	ascii_descriptor["initial_value"] = "a";
	const int64_t ascii_session_id = manager->begin_session(ascii_owner, ascii_descriptor, Callable(), Callable(), Callable());
	REQUIRE(ascii_session_id > numeric_session_id);
	SceneTree::get_singleton()->process(0.0);

	REQUIRE(host->get_child_count() == 1);
	TagAsciiKeypad *ascii_view = Object::cast_to<TagAsciiKeypad>(host->get_child(0));
	REQUIRE(ascii_view != nullptr);
	KeypadActionButton *ascii_q = find_keypad_action_button(ascii_view, "insert_text", "q");
	REQUIRE(ascii_q != nullptr);
	ascii_q->press();
	CHECK(manager->get_active_session()->get_buffer_text() == "aq");

	manager->cancel_active();
	memdelete(ascii_owner);
	memdelete(manager);
}

static Dictionary fixture_commit_ok(const String &) {
	Dictionary result;
	result["ok"] = true;
	result["error"] = String();
	return result;
}

static Dictionary fixture_commit_fail(const String &) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = "rejected";
	return result;
}

TEST_CASE("[Keypad][SceneTree] custom TSCN fixture drives an end-to-end session") {
	// --test bypasses ProjectSettings::setup(--path); point res:// at smoke_proj for this case.
	const String smoke_root = TestUtils::get_executable_dir().path_join("../smoke_proj").simplify_path();
	String &resource_path = TestProjectSettingsInternalsAccessor::resource_path();
	const String previous_resource_path = resource_path;
	resource_path = smoke_root;

	const String path = "res://keypads/custom_numeric.tscn";
	if (!ResourceLoader::exists(path)) {
		resource_path = previous_resource_path;
		MESSAGE("smoke_proj/keypads/custom_numeric.tscn missing; skipping E2E fixture test");
		return;
	}

	Ref<PackedScene> packed = ResourceLoader::load(path);
	resource_path = previous_resource_path;
	REQUIRE(packed.is_valid());

	InputSessionManager *manager = InputSessionManager::get_or_create(SceneTree::get_singleton()->get_root());
	REQUIRE(manager != nullptr);
	KeypadRegistry *registry = manager->get_keypad_registry();
	REQUIRE(registry != nullptr);
	registry->register_keypad("custom_numeric", packed);

	Node *owner = memnew(Node);
	SceneTree::get_singleton()->get_root()->add_child(owner);

	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["keypad_id"] = "custom_numeric";
	descriptor["presentation_mode"] = "popup";
	descriptor["initial_value"] = "1";
	const int64_t session_id = manager->begin_session(
			owner,
			descriptor,
			Callable(),
			callable_mp_static(&fixture_commit_ok),
			Callable());
	REQUIRE(session_id > 0);
	SceneTree::get_singleton()->process(0.0);

	KeypadHost *host = manager->get_keypad_host();
	REQUIRE(host != nullptr);
	REQUIRE(host->get_child_count() == 1);
	KeypadView *view = Object::cast_to<KeypadView>(host->get_child(0));
	REQUIRE(view != nullptr);

	KeypadActionButton *insert7 = find_keypad_action_button(view, "insert_text", "7");
	REQUIRE(insert7 != nullptr);
	insert7->press();
	CHECK(manager->get_active_session()->get_buffer_text() == "17");

	KeypadActionButton *clear = find_keypad_action_button(view, "clear", Variant());
	REQUIRE(clear != nullptr);
	clear->press();
	const String cleared = manager->get_active_session()->get_buffer_text();
	CHECK(cleared.is_empty());

	insert7->press();
	CHECK(manager->get_active_session()->get_buffer_text() == "7");

	KeypadActionButton *confirm = find_keypad_action_button(view, "confirm", Variant());
	REQUIRE(confirm != nullptr);
	confirm->press();
	CHECK(manager->get_active_session().is_null());

	const int64_t session_id2 = manager->begin_session(
			owner,
			descriptor,
			Callable(),
			callable_mp_static(&fixture_commit_fail),
			Callable());
	REQUIRE(session_id2 > session_id);
	SceneTree::get_singleton()->process(0.0);
	REQUIRE(host->get_child_count() == 1);
	view = Object::cast_to<KeypadView>(host->get_child(0));
	REQUIRE(view != nullptr);
	confirm = find_keypad_action_button(view, "confirm", Variant());
	REQUIRE(confirm != nullptr);
	confirm->press();
	CHECK(manager->get_active_session().is_valid());
	CHECK(manager->get_active_session()->is_active());

	KeypadActionButton *cancel = find_keypad_action_button(view, "cancel", Variant());
	REQUIRE(cancel != nullptr);
	cancel->press();
	CHECK(manager->get_active_session().is_null());

	memdelete(owner);
}

} // namespace
