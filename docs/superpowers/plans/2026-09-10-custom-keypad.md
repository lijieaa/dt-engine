# Configurable HMI Keypad Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the input controls' hardcoded keypad ownership with a TSCN-first, session-based keypad system that supports custom layouts, stable actions, and system, popup, fixed, and direct-window presentation modes.

**Architecture:** `TagNumInput` and `TagAsciiInput` create an `InputSession` through one scene-tree `InputSessionManager`. A `KeypadRegistry` resolves the requested keypad, the first storage backend instantiates a `PackedScene`, and `KeypadView` plus `KeypadActionButton` expose the scene contract. The existing numeric buffer, placement helpers, and native keypad classes remain the compatibility implementation for the built-in fallback while custom TSCN scenes use the same action and session protocol.

**Tech Stack:** Godot C++ module APIs in `modules/industrial_runtime`, Godot `Control`/`Node`/`PackedScene`/`RefCounted`, `String`/`HashMap`/`Vector`/`Dictionary`, doctest engine tests, and the existing Windows editor build.

**Spec:** `docs/superpowers/specs/2026-09-10-custom-keypad-design.md`

## Implementation status (2026-09-11)

| Task | Status |
|------|--------|
| 1 Action + InputSession | Done |
| 2 InputSessionManager | Done |
| 3 KeypadView + ActionButton | Done |
| 4 Backend + Registry + TSCN | Done |
| 5 KeypadHost + presentation modes | Done |
| 6 Builtin Num/Ascii as KeypadView | Done |
| 7 Inputs migrate to sessions | Done |
| 8 KeypadCommandService | Done |
| 9 TSCN fixture + E2E | Done (`smoke_proj/keypads/`) |
| 10 Docs / verify / policy | Done (this Task) |

Deferred integration boundary: screen/window navigation is a registered callback on `KeypadCommandService` (no built-in screen router).

---

- `modules/industrial_runtime` uses Godot cross-platform APIs only; do not add Win32, POSIX, Winsock, JNI, Objective-C, or browser-specific APIs.
- Do not add STL containers or STL headers; use Godot containers and strings.
- The implementation must build for Windows, LinuxBSD, macOS, Android, iOS, Web, and visionOS.
- Web and mobile presentation modes must use embedded Godot UI; do not create native child windows.
- Mouse and touch activation must follow the same session path.
- Opening popup or top-level keypad UI from an input event must use `call_deferred`.
- Do not edit `app/visualization/addons/industrial_widgets/`.
- Do not add competitor product names to product identifiers, comments, logs, tests, serialized action IDs, or UI strings.
- C++ UI literals use ASCII punctuation; runtime UI text uses `TTR`/`TTRC` where it is user-visible.
- Every new `.cpp` file must be listed in `modules/industrial_runtime/SCsub`.
- Existing uncommitted work in unrelated files must remain intact; stage only files belonging to the current task when committing.
- The stable serialized action protocol uses string action IDs and `Dictionary` payloads, never private numeric action codes.

## Current Baseline

The working tree already contains reusable popup work in:

- `modules/industrial_runtime/tag_keypad_buffer.h/.cpp`
- `modules/industrial_runtime/tag_keypad_placement.h/.cpp`
- `modules/industrial_runtime/tag_num_keypad.h/.cpp`
- `modules/industrial_runtime/tag_num_input.h/.cpp`
- `modules/industrial_runtime/tag_ascii_keypad.h/.cpp`
- `modules/industrial_runtime/tag_ascii_input.h/.cpp`

The plan must preserve their current numeric validation, min/max, caret editing, ASCII editing, placement, runtime write, and live-update suppression behavior while moving ownership to the new session layer.

## File Map

| File | Responsibility |
| --- | --- |
| `modules/industrial_runtime/keypad_action.h/.cpp` | Stable action IDs, action request normalization, and action category helpers |
| `modules/industrial_runtime/input_session.h/.cpp` | One editable numeric, ASCII, or password buffer and its validation/commit callbacks |
| `modules/industrial_runtime/input_session_manager.h/.cpp` | One active session, lifecycle, focus switching, and keypad binding |
| `modules/industrial_runtime/keypad_view.h/.cpp` | Native TSCN root contract, display-role binding, refresh, and error presentation |
| `modules/industrial_runtime/keypad_action_button.h/.cpp` | Serialized action button and mouse/touch long-press repeat |
| `modules/industrial_runtime/keypad_backend.h` | Backend request and resolved-definition types |
| `modules/industrial_runtime/keypad_registry.h/.cpp` | Keypad registration, defaults, and fallback resolution |
| `modules/industrial_runtime/tscn_keypad_backend.h/.cpp` | `PackedScene` loading, root validation, instantiation, and disposal |
| `modules/industrial_runtime/keypad_host.h/.cpp` | Embedded host for popup, fixed, system, and direct-window instances |
| `modules/industrial_runtime/keypad_command_service.h/.cpp` | Allowlisted tag commands and registered command callbacks |
| `modules/industrial_runtime/tag_keypad_placement.h/.cpp` | Shared Control-rect placement and viewport clamping |
| `modules/industrial_runtime/tag_num_keypad.h/.cpp` | Built-in numeric `KeypadView` compatibility implementation |
| `modules/industrial_runtime/tag_ascii_keypad.h/.cpp` | Built-in ASCII `KeypadView` compatibility implementation |
| `modules/industrial_runtime/tag_num_input.h/.cpp` | Numeric input properties and session target callbacks |
| `modules/industrial_runtime/tag_ascii_input.h/.cpp` | ASCII input properties and session target callbacks |
| `modules/industrial_runtime/tag_widget_util.h/.cpp` | Existing runtime bridge helpers used by command dispatch |
| `modules/industrial_runtime/register_types.cpp` | Register new runtime classes |
| `modules/industrial_runtime/SCsub` | Compile every new source |
| `modules/industrial_runtime/config.py` | Confirm new runtime classes remain enabled on every configured platform |
| `tests/scene/test_keypad_session.cpp` | Session, action, fallback, and command-service doctests |
| `tests/scene/test_keypad_scene.cpp` | Scene-tree and TSCN contract smoke tests |
| `smoke_proj/keypads/custom_numeric.tscn` | Minimal custom keypad fixture (native KeypadView + ActionButtons; no GDScript) |
| `smoke_proj/main.tscn` | Minimal main scene so smoke_proj can `--quit-after` cleanly |

---

### Task 1: Define the Stable Action and Session Contracts

**Files:**
- Create: `modules/industrial_runtime/keypad_action.h`
- Create: `modules/industrial_runtime/keypad_action.cpp`
- Create: `modules/industrial_runtime/input_session.h`
- Create: `modules/industrial_runtime/input_session.cpp`
- Modify: `modules/industrial_runtime/SCsub`
- Create: `tests/scene/test_keypad_session.cpp`

**Interfaces:**
- Produces `KeypadActionRequest`:

```cpp
struct KeypadActionRequest {
	String action_id;
	Variant payload;
	ObjectID source_id;
};
```

- Produces the stable action IDs:

```text
insert_text, decimal, toggle_sign, backspace, delete, clear,
move_left, move_right, move_home, move_end, increment, decrement,
confirm, cancel, focus_next, focus_previous, switch_keypad,
open_window, close_window, switch_screen, write_tag, set_tag,
toggle_tag, increment_tag, decrement_tag, call_command
```

- Produces `InputSession : RefCounted` with these methods:

```cpp
void configure(const Dictionary &p_descriptor);
void set_validation_callback(const Callable &p_callback);
void set_commit_callback(const Callable &p_callback);
int64_t get_session_id() const;
String get_input_mode() const;
String get_presentation_mode() const;
String get_keypad_id() const;
String get_buffer_text() const;
String get_display_text() const;
int get_caret_position() const;
bool is_modified() const;
bool is_active() const;
bool dispatch_edit_action(const String &p_action_id, const Variant &p_payload);
Dictionary validate_buffer() const;
Dictionary commit_buffer();
void cancel();
```

- The descriptor keys are fixed and documented in the header:

```text
session_id, input_mode, presentation_mode, keypad_id,
initial_value, previous_value, mask_display, format_config,
has_min, min_value, has_max, max_value, owner_id, anchor_id
```

- `InputSession` stores `Callable validation_callback` and `Callable commit_callback`.
  Validation returns `{"ok": bool, "error": String}`. Commit returns the same shape.
  A failed validation or commit leaves the session active and does not change the
  target control.

- Reuse `TagKeypadBuffer` for numeric editing and add an internal text-buffer path
  for ASCII and password modes. Password masking affects only `get_display_text()`;
  the committed value remains unmasked.

- [ ] **Step 1: Add the action constants and request normalization**

  Implement `keypad_action_id_is_input()`, `keypad_action_id_is_navigation()`,
  and `keypad_action_id_is_runtime_command()`. Reject empty IDs and payloads
  with the wrong shape before they reach a dispatcher.

- [ ] **Step 2: Add numeric, ASCII, and password session state**

  Preserve numeric sign, decimal, caret, delete, increment, decrement, and
  range metadata. ASCII accepts printable text and caret movement. Password
  mode shares ASCII editing but returns `*` characters from `get_display_text()`.

- [ ] **Step 3: Add callback-backed validation and commit**

  `commit_buffer()` must validate first, call the commit callback only after
  validation succeeds, and keep the status active when either operation fails.

- [ ] **Step 4: Write focused doctests before integration**

```cpp
TEST_CASE("[Keypad][Session] numeric edit actions") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "numeric";
	descriptor["initial_value"] = "12";
	session->configure(descriptor);
	CHECK(session->dispatch_edit_action("move_left", Variant()));
	CHECK(session->dispatch_edit_action("insert_text", "9"));
	CHECK(session->get_buffer_text() == "192");
	CHECK(session->dispatch_edit_action("backspace", Variant()));
	CHECK(session->get_buffer_text() == "12");
}

TEST_CASE("[Keypad][Session] failed commit keeps the session active") {
	Ref<InputSession> session = memnew(InputSession);
	Dictionary descriptor;
	descriptor["input_mode"] = "ascii";
	descriptor["initial_value"] = "abc";
	session->configure(descriptor);
	session->set_commit_callback(callable_mp_static(&return_write_failure));
	const Dictionary result = session->commit_buffer();
	CHECK_FALSE(result["ok"].operator bool());
	CHECK(session->is_active());
}
```

Define `return_write_failure(const String &)` immediately above the test as a
static helper returning `{"ok": false, "error": "write failed"}`. The test
file must contain executable assertions for numeric editing, ASCII editing,
password masking, min/max callback results, and no implicit write on cancel.

- [ ] **Step 5: Run the focused test target**

Run:

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --editor --test --test-case="[Keypad]"
```

Expected: the new session tests pass; the command exits with code 0.

- [ ] **Step 6: Commit only Task 1 files**

```powershell
git add modules/industrial_runtime/keypad_action.h modules/industrial_runtime/keypad_action.cpp modules/industrial_runtime/input_session.h modules/industrial_runtime/input_session.cpp modules/industrial_runtime/SCsub tests/scene/test_keypad_session.cpp
git commit -m "feat: add keypad action and input session contracts"
```

### Task 2: Add the Input Session Manager

**Files:**
- Create: `modules/industrial_runtime/input_session_manager.h`
- Create: `modules/industrial_runtime/input_session_manager.cpp`
- Modify: `modules/industrial_runtime/SCsub`
- Modify: `modules/industrial_runtime/register_types.cpp`
- Modify: `tests/scene/test_keypad_session.cpp`

**Interfaces:**
- `InputSessionManager : Node` is created once under the scene-tree root by:

```cpp
static InputSessionManager *get_or_create(Node *p_from);
```

- It exposes:

```cpp
int64_t begin_session(
		Object *p_owner,
		const Dictionary &p_descriptor,
		const Callable &p_validate,
		const Callable &p_commit,
		const Callable &p_cancel);
bool dispatch_action(const KeypadActionRequest &p_request);
bool dispatch_action(const String &p_action_id, const Variant &p_payload);
bool confirm_active();
void cancel_active();
void end_for_owner(ObjectID p_owner_id, const String &p_reason);
bool is_owner_active(ObjectID p_owner_id) const;
Ref<InputSession> get_active_session() const;
```

- Starting a new session follows the approved policy:
  - an unmodified old session is replaced immediately;
  - a modified old session is canceled without writing;
  - a new session receives a new monotonically increasing ID.

- The manager stores the active owner as `ObjectID`, checks `ObjectDB` before
  calling it, and closes safely when the owner exits the tree or the manager
  exits the tree.

- The manager must not close the session after a failed validation or write.
  It notifies the current keypad view after every edit, confirm failure, and
  cancellation.

- [ ] **Step 1: Implement root-scoped manager creation and cleanup**

  Add the manager as a child of the scene-tree root with a stable internal name.
  Do not use a process-global singleton that survives scene-tree teardown.

- [ ] **Step 2: Implement start, replace, cancel, and terminate lifecycle**

  Add explicit `end_for_owner`, screen-change termination, window-close
  termination, and runtime-stop termination hooks. Every implicit termination
  calls `InputSession::cancel()` and never calls the commit callback.

- [ ] **Step 3: Route all input actions**

  Route input action IDs to `InputSession::dispatch_edit_action()`. Route
  `confirm` to `confirm_active()`, `cancel` to `cancel_active()`, and focus
  actions to the registered navigation callback from Task 8.

- [ ] **Step 4: Add lifecycle tests**

  Add doctests for dirty-session replacement, clean-session replacement,
  cancellation without commit, owner exit, screen-change termination, and
  keeping the active view after a failed commit. Each test must create a
  manager and real `Ref<InputSession>` callbacks, then assert the active
  session ID, callback count, and final state instead of relying on log output.

- [ ] **Step 5: Run the focused tests and editor build**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --editor --test --test-case="[Keypad]"
```

Expected: manager lifecycle tests pass and the module links with no new
platform-specific dependencies.

- [ ] **Step 6: Commit**

```powershell
git add modules/industrial_runtime/input_session_manager.h modules/industrial_runtime/input_session_manager.cpp modules/industrial_runtime/SCsub modules/industrial_runtime/register_types.cpp tests/scene/test_keypad_session.cpp
git commit -m "feat: add input session manager lifecycle"
```

### Task 3: Implement the TSCN View and Action Button Contract

**Files:**
- Create: `modules/industrial_runtime/keypad_view.h`
- Create: `modules/industrial_runtime/keypad_view.cpp`
- Create: `modules/industrial_runtime/keypad_action_button.h`
- Create: `modules/industrial_runtime/keypad_action_button.cpp`
- Modify: `modules/industrial_runtime/SCsub`
- Modify: `modules/industrial_runtime/register_types.cpp`
- Create: `tests/scene/test_keypad_scene.cpp`

**Interfaces:**
- `KeypadView : Control` exposes:

```cpp
void bind_session(const Ref<InputSession> &p_session);
void unbind_session();
void refresh_from_session();
void show_error(const String &p_message);
void clear_error();
Size2 get_presentation_size() const;
void set_action_dispatcher(const Callable &p_dispatcher);
```

- The view scans descendants for metadata `keypad_bind_role` with these values:
  `input_display`, `previous_value`, `error`, and `range_hint`. A role is
  optional; absent labels do not affect editing.

- `KeypadActionButton : Button` exposes:

```cpp
void set_action_id(const String &p_id);
String get_action_id() const;
void set_action_payload(const Variant &p_payload);
Variant get_action_payload() const;
void set_repeat_mode(const String &p_mode);
String get_repeat_mode() const;
void set_repeat_delay_ms(int p_ms);
void set_repeat_interval_ms(int p_ms);
```

- The button emits `action_requested(Dictionary request)` with:

```text
action_id, payload, source_id, repeat
```

- `KeypadView` connects every descendant `KeypadActionButton` to the dispatcher
  callable. It never invokes arbitrary node methods from serialized payloads.

- Repeat is enabled only for `increment`, `decrement`, `move_left`,
  `move_right`, and `backspace`. `confirm`, `cancel`, `write_tag`, `set_tag`,
  `toggle_tag`, and `call_command` default to no repeat.

- [ ] **Step 1: Add display-role scanning and refresh**

  Use Godot `Node` traversal and `HashMap<StringName, Node *>`. Update only
  nodes that expose `set_text`; use `TTRC` for built-in error labels.

- [ ] **Step 2: Add action-button serialization**

  Bind all properties with `ClassDB`, store payload as `Variant`, and reject
  an empty action ID at press time with a debug diagnostic.

- [ ] **Step 3: Add touch and mouse repeat**

  Use Godot input events and a child `Timer`. Stop the timer on release,
  cancellation, focus loss, and tree exit. Do not use native OS timers.

- [ ] **Step 4: Add a scene-tree test**

  Create a `KeypadView`, add a `KeypadActionButton`, bind an input session,
  emit the button action, and assert that the dispatcher receives the stable
  ID and payload. Assert that display and error roles refresh.

- [ ] **Step 5: Build and run scene tests**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --editor --test --test-case="[Keypad][SceneTree]"
```

Expected: the view contract and action-button tests pass for a mock scene tree.

- [ ] **Step 6: Commit**

```powershell
git add modules/industrial_runtime/keypad_view.h modules/industrial_runtime/keypad_view.cpp modules/industrial_runtime/keypad_action_button.h modules/industrial_runtime/keypad_action_button.cpp modules/industrial_runtime/SCsub modules/industrial_runtime/register_types.cpp tests/scene/test_keypad_scene.cpp
git commit -m "feat: add TSCN keypad view and action button"
```

### Task 4: Add Backend and Registry Abstractions

**Files:**
- Create: `modules/industrial_runtime/keypad_backend.h`
- Create: `modules/industrial_runtime/keypad_registry.h`
- Create: `modules/industrial_runtime/keypad_registry.cpp`
- Create: `modules/industrial_runtime/tscn_keypad_backend.h`
- Create: `modules/industrial_runtime/tscn_keypad_backend.cpp`
- Modify: `modules/industrial_runtime/SCsub`
- Modify: `modules/industrial_runtime/register_types.cpp`
- Modify: `tests/scene/test_keypad_session.cpp`
- Modify: `tests/scene/test_keypad_scene.cpp`

**Interfaces:**
- `KeypadOpenRequest` contains:

```cpp
String keypad_id;
String input_mode;
String presentation_mode;
ObjectID owner_id;
ObjectID anchor_id;
int64_t session_id;
Ref<PackedScene> scene_override;
Dictionary session_context;
```

- `KeypadDefinition` contains:

```cpp
String id;
Ref<PackedScene> packed_scene;
String scene_path;
String backend_name;
bool builtin = false;
```

- `KeypadBackend : RefCounted` defines:

```cpp
virtual bool can_open(const KeypadDefinition &p_definition) const = 0;
virtual KeypadView *open(const KeypadOpenRequest &p_request, const KeypadDefinition &p_definition) = 0;
virtual void close(KeypadView *p_instance) = 0;
```

- `KeypadRegistry : RefCounted` exposes:

```cpp
void register_keypad(const String &p_id, const Ref<PackedScene> &p_scene);
void register_keypad_path(const String &p_id, const String &p_path);
void unregister_keypad(const String &p_id);
bool has_keypad(const String &p_id) const;
KeypadDefinition resolve(const KeypadOpenRequest &p_request, Node *p_owner) const;
void set_project_default_id(const String &p_id);
```

- Resolution order is:

```text
input keypad_scene_override
-> input keypad_id
-> owner scene root metadata "keypad_default_id"
-> nearest Window metadata "keypad_default_id"
-> project setting "industrial_runtime/keypad/default_id"
-> built-in mode default
```

The direct `PackedScene` override is an editor convenience that registers an
in-memory definition for the current request; it does not change the stable
action protocol.

- The TSCN backend loads `scene_path` through `ResourceLoader`, instantiates
  the scene, verifies the root is a `KeypadView`, and rejects invalid roots
  without crashing. A script may customize a `KeypadView` root, but a generic
  `Control` without the native class is not accepted in v1.

- [ ] **Step 1: Define the abstract backend types**

  Keep storage-specific fields inside `KeypadDefinition`; input controls only
  pass a logical ID, override scene, and presentation mode.

- [ ] **Step 2: Implement registry registration and fallback**

  Add one diagnostic per failed explicit definition, then continue through
  the fallback chain. Register the built-in numeric, ASCII, and password
  definitions as `builtin = true`.

- [ ] **Step 3: Implement `TscnKeypadBackend`**

  Support both a loaded `PackedScene` and a path. After instantiation,
  return `nullptr` for a scene without the required root contract and let the
  registry request the built-in fallback.

- [ ] **Step 4: Add fallback-resolution tests**

  Verify explicit scene, explicit ID, scene metadata, window metadata,
  project default, built-in mode default, invalid-scene fallback, and
  missing-ID behavior.

- [ ] **Step 5: Build and run fallback tests**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --editor --test --test-case="[Keypad]"
```

Expected: all resolution tests pass and an invalid custom scene does not abort
the scene tree.

- [ ] **Step 6: Commit**

```powershell
git add modules/industrial_runtime/keypad_backend.h modules/industrial_runtime/keypad_registry.h modules/industrial_runtime/keypad_registry.cpp modules/industrial_runtime/tscn_keypad_backend.h modules/industrial_runtime/tscn_keypad_backend.cpp modules/industrial_runtime/SCsub modules/industrial_runtime/register_types.cpp tests/scene/test_keypad_session.cpp tests/scene/test_keypad_scene.cpp
git commit -m "feat: add keypad registry and TSCN backend"
```

### Task 5: Add the Embedded Keypad Host and Four Presentation Modes

**Files:**
- Create: `modules/industrial_runtime/keypad_host.h`
- Create: `modules/industrial_runtime/keypad_host.cpp`
- Modify: `modules/industrial_runtime/input_session_manager.h`
- Modify: `modules/industrial_runtime/input_session_manager.cpp`
- Modify: `modules/industrial_runtime/tag_keypad_placement.h`
- Modify: `modules/industrial_runtime/tag_keypad_placement.cpp`
- Modify: `modules/industrial_runtime/SCsub`
- Modify: `tests/scene/test_keypad_scene.cpp`

**Interfaces:**
- `KeypadHost : Control` exposes:

```cpp
KeypadView *show_system(KeypadView *p_view, const KeypadOpenRequest &p_request);
KeypadView *show_popup(KeypadView *p_view, const KeypadOpenRequest &p_request);
KeypadView *show_fixed(KeypadView *p_view, const KeypadOpenRequest &p_request);
KeypadView *show_direct_window(KeypadView *p_view, const KeypadOpenRequest &p_request);
void hide_popup();
void hide_session_view();
void close_fixed();
```

- The host is a full-rect embedded `Control` mounted under the root window.
It is not a native child window. Direct-window mode uses a full-rect Godot
layer with the same `KeypadView` contract.

- `system` selects the built-in mode-specific view. `popup` creates or reuses
one view and places it with `tag_keypad_placement`. `fixed` keeps one view
alive while the screen is active. `direct_window` uses the embedded layer and
does not bypass the session manager.

- Extend placement helpers to accept `Size2`, viewport `Rect2`, anchor `Rect2`,
side, and alignment, and return a clamped `Rect2`:

```cpp
Rect2 tag_keypad_place_centered(const Size2 &, const Rect2 &);
Rect2 tag_keypad_place_screen_cell(const Size2 &, const Rect2 &, int p_cell);
Rect2 tag_keypad_place_relative(const Size2 &, const Rect2 &, const Rect2 &, const String &, const String &);
```

- All opening from `gui_input` is deferred. The host must remain functional
when subwindow embedding is unavailable, including Web and mobile targets.

- [ ] **Step 1: Create the embedded host**

  Configure anchors, mouse filtering, z-order, and visibility. Add explicit
  cleanup for popup and fixed instances on scene-tree exit.

- [ ] **Step 2: Connect manager resolution to host presentation**

  The manager resolves a definition, asks the backend for a `KeypadView`,
  binds the session, sets the action dispatcher, adds it to the host, and
  calls one host presentation method. A backend failure must try the built-in
  definition before closing.

- [ ] **Step 3: Migrate placement helpers from dialog geometry to Control geometry**

  Preserve center, 3x3 screen-cell, relative-to-control, side, alignment, and
  visible-area clamping. Keep the existing defaults: center, cell 4, bottom,
  center.

- [ ] **Step 4: Add presentation tests**

  Verify system fallback, popup center/grid/control placement, fixed reuse
  across two owners, direct-window embedding, deferred opening, and no native
  window creation.

- [ ] **Step 5: Build and run scene tests**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --editor --test --test-case="[Keypad][SceneTree]"
```

Expected: all four modes produce a visible embedded `KeypadView` and popup
placement stays inside the viewport.

- [ ] **Step 6: Commit**

```powershell
git add modules/industrial_runtime/keypad_host.h modules/industrial_runtime/keypad_host.cpp modules/industrial_runtime/input_session_manager.h modules/industrial_runtime/input_session_manager.cpp modules/industrial_runtime/tag_keypad_placement.h modules/industrial_runtime/tag_keypad_placement.cpp modules/industrial_runtime/SCsub tests/scene/test_keypad_scene.cpp
git commit -m "feat: add embedded keypad presentation host"
```

### Task 6: Refactor the Built-in Numeric and ASCII Keypads

**Files:**
- Modify: `modules/industrial_runtime/tag_num_keypad.h`
- Modify: `modules/industrial_runtime/tag_num_keypad.cpp`
- Modify: `modules/industrial_runtime/tag_ascii_keypad.h`
- Modify: `modules/industrial_runtime/tag_ascii_keypad.cpp`
- Modify: `modules/industrial_runtime/tag_keypad_buffer.h`
- Modify: `modules/industrial_runtime/tag_keypad_buffer.cpp`
- Modify: `tests/scene/test_keypad_scene.cpp`

**Interfaces:**
- Change both native keypad classes to derive from `KeypadView` while
  retaining compatibility methods:

```cpp
void open_for(const String &p_initial);
void open_for_options(const Dictionary &p_options);
String get_buffer() const;
```

- Replace direct button callbacks with `KeypadActionButton` instances whose
  action IDs are `insert_text`, `decimal`, `toggle_sign`, `backspace`,
  `delete`, `clear`, `move_left`, `move_right`, `increment`, `decrement`,
  `confirm`, and `cancel`.

- Keep numeric previous-value and range hints as display-role nodes. Keep ASCII
  QWERTY, space, shift, backspace, clear, confirm, and cancel behavior.

- `open_for` remains a compatibility entry point for code that instantiates a
  native keypad directly; input controls no longer own these objects.

- [ ] **Step 1: Make native keypads implement the `KeypadView` contract**

  Preserve current dimensions, touch target sizes, labels, and error behavior
  while replacing direct signal-to-method wiring with action dispatch.

- [ ] **Step 2: Register built-in definitions**

  Have the registry factory create numeric, ASCII, and password-compatible
  native views when no custom TSCN resolves. Password mode reuses the ASCII
  edit path with masked display.

- [ ] **Step 3: Exercise built-in action paths**

  Test digit, decimal, sign, caret movement, delete, increment, decrement,
  ASCII character insertion, shift, clear, confirm, and cancel through the
  same dispatcher used by TSCN scenes.

- [ ] **Step 4: Build and run keypad tests**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --editor --test --test-case="[Keypad]"
```

Expected: built-in keypads remain functional without direct input-control
ownership and no existing popup behavior regresses.

- [ ] **Step 5: Commit**

```powershell
git add modules/industrial_runtime/tag_num_keypad.h modules/industrial_runtime/tag_num_keypad.cpp modules/industrial_runtime/tag_ascii_keypad.h modules/industrial_runtime/tag_ascii_keypad.cpp modules/industrial_runtime/tag_keypad_buffer.h modules/industrial_runtime/tag_keypad_buffer.cpp tests/scene/test_keypad_scene.cpp
git commit -m "refactor: route built-in keypads through session actions"
```

### Task 7: Migrate Numeric and ASCII Inputs to Sessions

**Files:**
- Modify: `modules/industrial_runtime/tag_num_input.h`
- Modify: `modules/industrial_runtime/tag_num_input.cpp`
- Modify: `modules/industrial_runtime/tag_ascii_input.h`
- Modify: `modules/industrial_runtime/tag_ascii_input.cpp`
- Modify: `modules/industrial_runtime/register_types.cpp`
- Modify: `tests/scene/test_keypad_session.cpp`
- Modify: `tests/scene/test_keypad_scene.cpp`

**Interfaces:**
- Add these exported properties to both input controls:

```text
keypad_id: String
keypad_scene_override: PackedScene
presentation_mode: system | popup | fixed | direct_window
```

- Preserve the existing numeric properties:

```text
use_min, min_value, use_max, max_value,
show_limits_on_keypad, restart_on_out_of_range,
show_previous_value, hide_keypad_title,
keypad_anchor, keypad_screen_cell, keypad_side, keypad_align
```

- Add private callbacks with these exact signatures:

```cpp
Dictionary _validate_input_session_text(const String &p_text) const;
Dictionary _commit_input_session_text(const String &p_text);
void _cancel_input_session();
void _begin_input_session();
```

- Input events call `call_deferred(SNAME("_begin_input_session"))`, then
  `InputSessionManager::begin_session(...)`. Mouse and touch use the same
  method.

- Numeric commit validates format and range, calls `tag_widget::write_tag`,
  updates the line edit only after a successful commit, and returns
  `{"ok": false, "error": "write failed"}` on failure. ASCII follows the same
  write-failure rule without numeric parsing.

- Live tag callbacks check `InputSessionManager::is_owner_active(get_instance_id())`
  and do not overwrite the active buffer.

- [ ] **Step 1: Remove direct keypad ownership**

  Delete `_ensure_keypad`, keypad pointers, and signal wiring from both inputs.
  Keep compatibility methods only in the keypad classes, not in input controls.

- [ ] **Step 2: Add serialized keypad selection and presentation properties**

  Bind the properties with `ClassDB` and use enum hints for presentation mode,
  anchor, side, and alignment. The direct `PackedScene` override is optional
  in the serialized scene and defaults to null.

- [ ] **Step 3: Implement numeric validation and commit callbacks**

  Reuse `WidgetFormat::parse_input`, current min/max behavior, previous-value
  metadata, and existing write bridge. Do not close the session on validation
  or write failure.

- [ ] **Step 4: Implement ASCII validation and commit callbacks**

  Accept the current text, keep the session open on write failure, and preserve
  live-update suppression.

- [ ] **Step 5: Add input integration tests**

  Verify numeric and ASCII controls start sessions, select custom scenes,
  share a fixed keypad, switch between two inputs without implicit writes,
  cancel without changing the tag, and retain the view after a failed write.

- [ ] **Step 6: Build and run the integration tests**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --editor --test --test-case="[Keypad]"
```

Expected: existing popup workflows and new custom-scene workflows both pass.

- [ ] **Step 7: Commit**

```powershell
git add modules/industrial_runtime/tag_num_input.h modules/industrial_runtime/tag_num_input.cpp modules/industrial_runtime/tag_ascii_input.h modules/industrial_runtime/tag_ascii_input.cpp modules/industrial_runtime/register_types.cpp tests/scene/test_keypad_session.cpp tests/scene/test_keypad_scene.cpp
git commit -m "feat: migrate tag inputs to managed keypad sessions"
```

### Task 8: Add Allowlisted Runtime and Navigation Actions

**Files:**
- Create: `modules/industrial_runtime/keypad_command_service.h`
- Create: `modules/industrial_runtime/keypad_command_service.cpp`
- Modify: `modules/industrial_runtime/input_session_manager.h`
- Modify: `modules/industrial_runtime/input_session_manager.cpp`
- Modify: `modules/industrial_runtime/tag_widget_util.h`
- Modify: `modules/industrial_runtime/tag_widget_util.cpp`
- Modify: `modules/industrial_runtime/SCsub`
- Modify: `modules/industrial_runtime/register_types.cpp`
- Modify: `tests/scene/test_keypad_session.cpp`

**Interfaces:**
- `KeypadCommandService : RefCounted` exposes:

```cpp
bool dispatch(Node *p_owner, const String &p_action_id, const Dictionary &p_payload, String &r_error);
void register_command(const String &p_command_id, const Callable &p_handler);
void unregister_command(const String &p_command_id);
bool has_command(const String &p_command_id) const;
void set_navigation_handler(const Callable &p_handler);
```

- Implement built-in commands:

```text
write_tag     { tag_name, value }
set_tag       { tag_name, value }
toggle_tag    { tag_name }
increment_tag { tag_name, step }
decrement_tag { tag_name, step }
call_command  { command_id, args }
```

- Tag commands use `tag_widget::write_tag` and the existing runtime bridge.
For toggle and increment/decrement, read the current value through the
existing `IndustrialRuntimeHost` path before writing. A missing owner,
missing tag, or write failure returns an error and leaves the keypad visible.

- `call_command` accepts only IDs registered with `register_command`. It must
reject method paths, script text, object paths, and unregistered IDs.

- Navigation actions call the registered navigation handler with the original
action ID and payload. If no handler is registered, the action is rejected
with a debug diagnostic and no session is auto-confirmed.

- [ ] **Step 1: Implement tag command dispatch**

  Add exact payload validation and route every successful write through the
  existing cache/re-read path. Do not duplicate WebSocket or HTTP code.

- [ ] **Step 2: Implement registered command dispatch**

  Store handlers in `HashMap<StringName, Callable>` and verify the callable
  before invocation.

- [ ] **Step 3: Connect dispatcher routing**

  Extend `InputSessionManager::dispatch_action()` so input actions go to the
  active session, runtime actions go to `KeypadCommandService`, and navigation
  actions go to the registered navigation service.

- [ ] **Step 4: Add command and security tests**

  Test all five built-in command forms, invalid payloads, failed writes,
  registered command success, unregistered command rejection, arbitrary method
  path rejection, and navigation callback forwarding.

- [ ] **Step 5: Build and run**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --editor --test --test-case="[Keypad]"
```

Expected: command tests pass and no action can invoke an unregistered method or
script.

- [ ] **Step 6: Commit**

```powershell
git add modules/industrial_runtime/keypad_command_service.h modules/industrial_runtime/keypad_command_service.cpp modules/industrial_runtime/input_session_manager.h modules/industrial_runtime/input_session_manager.cpp modules/industrial_runtime/tag_widget_util.h modules/industrial_runtime/tag_widget_util.cpp modules/industrial_runtime/SCsub modules/industrial_runtime/register_types.cpp tests/scene/test_keypad_session.cpp
git commit -m "feat: add allowlisted keypad runtime commands"
```

### Task 9: Add the TSCN Fixture and End-to-End Scene Smoke Test

**Files:**
- Create: `smoke_proj/keypads/custom_numeric.tscn`
- Create: `smoke_proj/keypads/custom_numeric.gd`
- Modify: `tests/scene/test_keypad_scene.cpp`
- Modify: `smoke_proj/project.godot` only if the fixture needs an explicit module setting

**Interfaces:**
- The fixture root must be `KeypadView`.
- It must contain:
  - one node with `keypad_bind_role = "input_display"`;
  - one numeric `KeypadActionButton` with `action_id = "insert_text"` and payload `{"text": "7"}`;
  - one `clear` button;
  - one `backspace` button;
  - one `confirm` button;
  - one `cancel` button;
  - one node with `keypad_bind_role = "error"`.

- The fixture may use a small script only for scene setup or visual labels.
It must not perform tag writes, method lookup, or input-session ownership.

- [ ] **Step 1: Create the minimal TSCN fixture**

  Keep the scene small enough to load in a headless test and make every action
  button's serialized properties visible in the text resource.

- [ ] **Step 2: Load and register the fixture in the scene test**

  Use `ResourceLoader::load("res://keypads/custom_numeric.tscn")`, register it
  under `"custom_numeric"`, start a numeric session, and dispatch button presses.

- [ ] **Step 3: Assert end-to-end behavior**

  Verify scene load, display refresh, action routing, confirm callback,
  validation failure retention, cancellation, and reopening the same scene for
  a second input owner.

- [ ] **Step 4: Run the smoke test**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --path smoke_proj --editor --test --test-case="[Keypad]"
```

Expected: the custom TSCN is instantiated, buttons operate on the active
session, and the scene remains alive after a rejected confirm.

- [ ] **Step 5: Commit**

```powershell
git add smoke_proj/keypads/custom_numeric.tscn smoke_proj/keypads/custom_numeric.gd tests/scene/test_keypad_scene.cpp smoke_proj/project.godot
git commit -m "test: add custom keypad TSCN smoke fixture"
```

### Task 10: Finish Registration, Documentation, and Cross-Platform Verification

**Files:**
- Modify: `modules/industrial_runtime/SCsub`
- Modify: `modules/industrial_runtime/register_types.cpp`
- Modify: `modules/industrial_runtime/config.py` only if the class registration requires a documented module option
- Modify: `modules/industrial_runtime/README.md`
- Modify: `docs/superpowers/specs/2026-09-10-custom-keypad-design.md`
- Modify: `docs/superpowers/plans/2026-09-10-custom-keypad.md`

**Interfaces:**
- Register these classes at scene initialization:

```text
InputSession
InputSessionManager
KeypadView
KeypadActionButton
KeypadHost
TagNumKeypad
TagAsciiKeypad
TagNumInput
TagAsciiInput
```

- Add every new `.cpp` exactly once to `SCsub`.
- Document the user workflow in `modules/industrial_runtime/README.md`:

```text
1. Create a scene whose root is KeypadView.
2. Add KeypadActionButton nodes and assign stable action IDs.
3. Set keypad_bind_role metadata on optional display labels.
4. Assign keypad_id or keypad_scene_override on TagNumInput or TagAsciiInput.
5. Choose system, popup, fixed, or direct_window presentation.
```

- [x] **Step 1: Register all classes and verify source lists**

  Compare `register_types.cpp` and `SCsub` against the File Map. Add no
  platform-specific source branches.

- [x] **Step 2: Document fallback and failure behavior**

  Explain that invalid custom scenes fall back to the built-in mode default,
  dirty sessions cancel on replacement, and failed writes keep the keypad open.

- [x] **Step 3: Run the complete Windows editor verification**

```powershell
scons platform=windows target=editor tests=yes -j28
bin/godot.windows.editor.x86_64.exe --headless --test --test-case="*[Keypad]*"
bin/godot.windows.editor.x86_64.exe --headless --path smoke_proj --quit-after 1
git diff --check
```

Expected: build succeeds, all keypad tests pass, the smoke project exits
cleanly, and `git diff --check` reports no whitespace errors.

- [x] **Step 4: Run static policy checks**

```powershell
rg -n -i "ebpro|easybuilder|weintek" modules/industrial_runtime/keypad_* modules/industrial_runtime/input_session* tests/scene/test_keypad_session.cpp tests/scene/test_keypad_scene.cpp smoke_proj/keypads
rg -n "#include <(vector|unordered_map|map|set|string)" -g "keypad_*" -g "input_session*" modules/industrial_runtime
rg -n "Win32|windows\.h|winsock|JNI|Objective-C|pthread|unistd\.h" -g "keypad_*" -g "input_session*" modules/industrial_runtime
```

Expected: no product-code traces, STL includes, or platform-specific APIs are
introduced. The forbidden-string search is copied from the repository rule and
must be run only against the changed product paths; do not add those strings to
product files.

- [x] **Step 5: Verify all configured platforms**

Run the existing module configuration checks for Windows, LinuxBSD, macOS,
Android, iOS, Web, and visionOS. For Web and mobile, inspect the code path to
confirm that `KeypadHost` remains an embedded `Control` and no `Window` child
is created.

- [x] **Step 6: Update the spec status and finish the plan**

  Mark the TSCN backend and four presentation modes as implemented only after
  the preceding tests pass. Record any intentionally deferred screen/window
  navigation adapter as an explicit integration boundary, not as an
  untracked behavior.

- [x] **Step 7: Commit the final integration**

```powershell
git add modules/industrial_runtime/SCsub modules/industrial_runtime/register_types.cpp modules/industrial_runtime/config.py modules/industrial_runtime/README.md docs/superpowers/specs/2026-09-10-custom-keypad-design.md docs/superpowers/plans/2026-09-10-custom-keypad.md
git commit -m "docs: finish configurable keypad integration"
```

## Self-Review

### Spec Coverage

- `InputSession` and numeric/ASCII/password state: Task 1.
- One active session and lifecycle termination: Task 2.
- Stable action protocol and long-press policy: Tasks 1 and 3.
- Custom scene contract and display roles: Task 3.
- TSCN backend and fallback chain: Task 4.
- System, popup, fixed, and direct-window modes: Task 5.
- Built-in compatibility defaults: Task 6.
- Numeric and ASCII input integration: Task 7.
- Allowlisted tag and registered-command actions: Task 8.
- TSCN user workflow and end-to-end loading: Task 9.
- Registration, build, policy, and platform verification: Task 10.

### Placeholder Scan

The plan contains no unresolved implementation markers. The only deliberate
wording about a future boundary is the navigation adapter in Task 10, which is
specified as a registered callback with defined failure behavior.

### Type and Signature Check

- `InputSessionManager` owns `Ref<InputSession>` and receives `Callable`
  validation, commit, and cancel callbacks from input controls.
- `KeypadRegistry` returns `KeypadDefinition`; `TscnKeypadBackend` consumes
  that definition and returns a `Node *` whose root contract is `KeypadView`.
- `KeypadHost` owns the instantiated `KeypadView` lifetime and exposes the
  four presentation methods used by the manager.
- `KeypadActionButton` emits `Dictionary` requests; the manager and command
  service consume the same stable string IDs.
- Existing `TagKeypadBuffer` and placement helpers are reused rather than
  duplicated.

Plan complete and saved to `docs/superpowers/plans/2026-09-10-custom-keypad.md`. Two execution options:

1. Subagent-Driven (recommended) - Dispatch a fresh subagent per task and review between tasks.
2. Inline Execution - Execute tasks in this session with checkpoints.

Which approach?
