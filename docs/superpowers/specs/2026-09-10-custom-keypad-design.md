# Custom Keypad Architecture Design

Date: 2026-09-10
Status: design approved in chat; written spec pending user review
Scope: `modules/industrial_runtime` and the runtime-facing keypad scene contract

## 1. Goal

Replace the current C++-constructed numeric and ASCII keypad layouts with an
extensible keypad architecture. The first backend is TSCN-based so users can
design keypad screens in the Godot editor. The public runtime contract must
not depend on the storage format, allowing a later project-data, JSON, or
remote backend without changing input controls or action dispatch.

The behavior target is an industrial HMI keypad model:

- an input object opens or selects a keypad;
- one active input session owns the editable value;
- keypad buttons dispatch stable actions;
- confirmation validates and writes through the runtime;
- cancel closes the session without writing;
- keypad layout and visual appearance are user-configurable.

This design is informed by the reverse-engineering material in:

- `D:\driver-engine\docs\reverse-engineering\2026-08-19-ebpro-runtime-analysis.md`
- `D:\driver-engine\c\EasyBuilder Pro.exe.c`
- `D:\driver-engine\c\ebpro_*_decomp.c`
- `D:\driver-engine\c\com_e30_*.txt`

The reverse-engineering sample does not contain the complete HMI/GUI runtime
process. Therefore this document aligns observable HMI behavior and component
boundaries, but does not claim compatibility with undocumented internal action
numbers or private binary frame values.

## 2. Current Problem

`TagNumInput` and `TagAsciiInput` currently create keypad objects directly:

```text
TagNumInput::_ensure_keypad()
    -> memnew(TagNumKeypad)

TagNumKeypad::_rebuild_ui()
    -> creates every button and layout in C++
```

This makes input editing functional but prevents users from:

- changing the keypad layout;
- choosing a different keypad per input object;
- reusing one keypad scene across input types;
- keeping a fixed keypad visible on a screen;
- using the same button/action model for input and machine-control commands.

The existing popup keypad design in
`docs/superpowers/specs/2026-09-10-popup-keypad-design.md` is superseded for
architecture purposes. Its numeric buffer, validation, and placement behavior
remain reusable implementation input, but its custom-keypad and fixed-keyboard
items are no longer non-goals.

## 3. Design Principles

1. Input controls depend on an input-session interface, not on a keypad class.
2. Keypad storage is selected through a backend interface.
3. TSCN is the first backend, not the permanent storage contract.
4. Buttons dispatch stable string action IDs and structured payloads.
5. Runtime control actions go through allowlisted services, never arbitrary
   method invocation.
6. Only one input session is active at a time.
7. A visible fixed keypad may outlive individual input sessions.
8. Editing state is isolated from live tag updates until confirm, cancel, or
   session termination.
9. Missing or invalid custom keypads fall back deterministically.
10. Existing numeric and ASCII behavior is preserved through built-in default
    TSCN scenes or an equivalent compatibility path.

## 4. Architecture

```text
TagNumInput / TagAsciiInput
        |
        | begin_input_session()
        v
InputSessionManager
        |
        | resolve keypad and presentation
        v
KeypadRegistry
        |
        v
IKeypadBackend
        |
        +-- TscnKeypadBackend       first implementation
        +-- ProjectDataBackend      future
        +-- JsonKeypadBackend       future
        +-- RemoteKeypadBackend     future
        |
        v
KeypadHost
        |
        v
KeypadView / KeypadActionButton
        |
        v
KeypadActionDispatcher
        |
        +-- InputSessionManager
        +-- KeypadRegistry
        +-- Screen/Window navigation service
        +-- Runtime command service
```

### 4.1 InputSession

`InputSession` is the authoritative state for one editing operation. It is
independent of any scene and must be testable without creating a Window.

Required state:

```text
session_id
target_control
tag_name
input_mode             numeric | ascii | password
initial_value
buffer_text
caret_position
format_config
has_min / min_value
has_max / max_value
keypad_id
presentation_mode      system | popup | fixed | direct_window
modified
status                 active | confirming | completed | canceled | failed
```

Required behavior:

- create from the input control's current value;
- insert and remove text;
- move the caret;
- numeric sign and decimal editing;
- numeric increment and decrement;
- validate without writing;
- confirm through the input-control write callback;
- cancel and restore the non-session display;
- ignore live tag updates while active;
- expose a display-safe value for keypad labels;
- report validation and write errors without destroying the session.

The existing `TagKeypadBuffer` is a candidate implementation for numeric
editing. ASCII and password modes may use a shared text buffer with mode
constraints and display masking.

### 4.2 InputSessionManager

`InputSessionManager` owns at most one active input session.

Responsibilities:

- start a session for a `TagNumInput` or `TagAsciiInput`;
- replace or cancel the current session when another input is selected;
- route input actions to the active session;
- perform confirm/cancel lifecycle transitions;
- notify the active keypad view after every state change;
- prevent live subscription updates from overwriting active edits;
- coordinate focus-next and focus-previous;
- close the session on screen change, window close, node exit, or runtime stop.

Default replacement policy:

1. If the old session has not been modified, switch immediately.
2. If it has been modified, cancel it without writing, then start the new
   session.
3. Never auto-confirm an old session during focus changes.

An explicit future policy may require confirmation before switching, but it is
not the default in the first implementation.

### 4.3 KeypadRegistry

`KeypadRegistry` resolves a logical `keypad_id` to a backend and a keypad
definition.

The first public registration shape is conceptually:

```text
register_keypad(keypad_id, backend_name, definition)
unregister_keypad(keypad_id)
has_keypad(keypad_id)
resolve_keypad(keypad_id, input_mode, presentation_mode)
```

For the TSCN backend, `definition` contains a `PackedScene` reference or a
scene path. The registry must not expose that storage detail to input
controls.

Resolution priority:

```text
input object's keypad_id
    -> current screen default keypad_id
    -> current window default keypad_id
    -> project default keypad_id
    -> built-in default for input mode
```

An invalid explicit definition continues through the fallback chain and emits
one diagnostic. It must not crash the runtime.

### 4.4 Backend Interface

The backend interface is a runtime contract, not necessarily a public C++ ABI.
The first implementation may use Godot `Object`/`RefCounted` classes while
preserving these operations:

```text
can_resolve(keypad_id, input_mode, presentation_mode)
open(request, host) -> keypad instance
attach_session(instance, session)
show(instance, presentation request)
hide(instance)
close(instance)
```

`KeypadOpenRequest` contains:

```text
keypad_id
input_mode
presentation_mode
owner_control
anchor_control
session_id
session_context
```

The backend owns scene loading and instance lifetime. The session manager owns
editing state and commit semantics.

### 4.5 TscnKeypadBackend

The first backend:

1. resolve `keypad_id` to a scene path or `PackedScene`;
2. load and instantiate the scene;
3. verify that the root implements the keypad-view contract;
4. attach the active session;
5. place it according to the presentation mode;
6. remove, hide, or reuse the instance according to its lifetime.

The backend must support four presentation modes:

```text
system
popup
fixed
direct_window
```

`system` resolves to a built-in default TSCN and is not an operating-system
keyboard bridge in the first version.

## 5. Keypad Scene Contract

### 5.1 KeypadView

A custom keypad scene root implements `KeypadView`. It may be a native class
or a script-backed node as long as it exposes the same contract.

Required operations:

```text
bind_session(session)
unbind_session()
refresh_from_session()
show_error(message)
clear_error()
get_presentation_size()
```

Optional notifications:

```text
session_changed
session_confirmed
session_canceled
keypad_requested
```

The view owns visual layout, labels, colors, fonts, icons, and responsive
arrangement. It does not own Tag writes or arbitrary runtime commands.

### 5.2 KeypadActionButton

`KeypadActionButton` is a reusable button node for TSCN scenes.

Serialized properties:

```text
action_type
action_payload
repeat_mode          none | repeat
repeat_delay_ms
repeat_interval_ms
enabled_condition
```

The button emits a structured action request to the dispatcher. It does not
call an arbitrary target method.

Long-press repeat is allowed for editing/navigation actions such as
`increment`, `decrement`, `move_left`, `move_right`, and `backspace`.
`confirm`, `cancel`, `write_tag`, and command actions default to
`repeat_mode = none`.

### 5.3 Display Binding

A keypad scene may contain one or more display nodes. The first implementation
provides a simple binding contract:

```text
bind_role = "input_display"
bind_role = "previous_value"
bind_role = "error"
bind_role = "range_hint"
```

The view updates these nodes from the session. A custom scene may omit any
role; omitted visual elements do not change session behavior.

## 6. Action Protocol

Action IDs are stable strings. Payloads are dictionaries or typed scalar
values, so scene files do not depend on internal enum numbers.

### 6.1 Input Session Actions

```text
insert_text       { text }
decimal           {}
toggle_sign       {}
backspace         {}
delete            {}
clear             {}
move_left         {}
move_right        {}
move_home         {}
move_end          {}
increment         { step }
decrement         { step }
confirm           {}
cancel            {}
focus_next        {}
focus_previous    {}
```

`decimal`, `toggle_sign`, `increment`, and `decrement` are accepted only when
the active session input mode supports them. Invalid actions are ignored and
diagnosed at debug level.

### 6.2 Keypad and Screen Actions

```text
switch_keypad     { keypad_id, presentation? }
open_window       { window_id }
close_window      { window_id? }
switch_screen     { screen_id }
```

Changing screen or closing the owning window terminates the active input
session according to the lifecycle rules. It must not implicitly write an
unconfirmed buffer.

### 6.3 Runtime Control Actions

```text
write_tag         { tag_name, value }
set_tag           { tag_name, value }
toggle_tag        { tag_name }
increment_tag     { tag_name, step }
decrement_tag     { tag_name, step }
call_command      { command_id, args }
```

These actions go through an allowlisted runtime command service. The first
implementation must reject arbitrary method paths, script source, or
unregistered command IDs.

Runtime writes use the same validation, error reporting, and cache/re-read
path as existing widget writes. A failed action leaves the current screen
visible and reports an error to the view when one is available.

## 7. Presentation Modes

### 7.1 System

Resolve the built-in keypad scene for the active input mode:

```text
numeric  -> built-in numeric keypad
ascii    -> built-in ASCII keypad
password -> built-in password keypad
```

The built-in scene is the fallback when no custom `keypad_id` resolves.

### 7.2 Popup

Create or reuse a keypad instance in a top-level `KeypadHost`. Place it using:

- centered;
- screen 3x3 cell;
- relative to the anchor input control;
- clamped to the visible area.

The popup closes after successful confirmation or cancellation. It remains
open after validation or write failure so the user can correct the value.

### 7.3 Fixed

A fixed keypad is instantiated with the screen and remains visible while the
screen is active. It does not belong to one input control.

When an input is clicked:

1. the manager starts or switches the active session;
2. the fixed keypad binds to that session;
3. its buttons operate on the active session;
4. selecting another input switches the session without recreating the view.

If a screen has an explicitly configured fixed keypad, input objects default
to it instead of opening a second popup, unless the input explicitly requests
popup presentation.

### 7.4 Direct Window

A Direct Window is a configured window/scene used as the keypad presentation
layer. It may contain keypad buttons, value displays, status labels, and
ordinary control buttons together.

It still binds through `InputSessionManager`; Direct Window is not a bypass
around session validation or runtime writes.

## 8. Input Control Integration

`TagNumInput` and `TagAsciiInput` retain their public tag-binding and
format/range properties. Their keypad-specific responsibilities become:

```text
keypad_id
presentation_mode
keypad_anchor
keypad_screen_cell
keypad_side
keypad_align
```

They call the session manager on pointer/touch activation and provide:

- current display value;
- input mode;
- validation rules;
- confirm callback;
- cancel callback;
- live-update suppression while active.

They must not instantiate `TagNumKeypad` or `TagAsciiKeypad` directly after
the migration is complete.

The existing `TagNumKeypad` and `TagAsciiKeypad` implementations may be kept
temporarily as compatibility wrappers during migration, but the built-in
default behavior must use the same action/session contract as custom scenes.

## 9. Lifecycle and Error Handling

### 9.1 Session End Conditions

An active session ends on:

- successful confirm and successful write;
- cancel;
- owning input node exit;
- screen change;
- owning window close;
- runtime stop;
- explicit session replacement.

Unconfirmed values are never written during implicit termination.

### 9.2 Failure Rules

| Failure | Required behavior |
|---|---|
| keypad ID missing | continue fallback chain |
| scene load failure | continue fallback chain and diagnose |
| invalid scene root | continue fallback chain and diagnose |
| unsupported action | ignore and diagnose |
| validation failure | keep session and show error |
| runtime write failure | keep view/session available and show error |
| active-session conflict | cancel old session, start new session |
| live tag update during edit | ignore for active control |

Fallback order:

```text
explicit input keypad
 -> screen default
 -> window default
 -> project default
 -> built-in mode default
 -> fail closed with diagnostic
```

## 10. Persistence and Backend Boundary

The first persistence format is Godot TSCN:

- keypad scenes are ordinary project resources;
- action button properties are serialized by the scene;
- input controls serialize `keypad_id` and presentation properties;
- registry entries may initially be project settings or a runtime registration
  API, depending on the existing project-loading path.

The scene contract must not require callers to know whether a keypad came from
TSCN, project data, JSON, or a future remote source. Future backends should
produce the same `KeypadView` and action-dispatch interfaces.

The first implementation should avoid introducing a full project-level
keypad editor or a large built-in layout library. A user can create and save a
TSCN scene directly; the registry and input properties are the integration
surface.

## 11. Migration Plan

1. Extract or reuse the existing numeric buffer as the session editing core.
2. Add `InputSession` and `InputSessionManager`.
3. Add action request and dispatcher types.
4. Add `KeypadView` and `KeypadActionButton`.
5. Add `KeypadRegistry`, backend interface, and TSCN backend.
6. Convert current numeric and ASCII keypad layouts into built-in TSCN scenes
   or compatibility scenes that use the new action contract.
7. Change `TagNumInput` and `TagAsciiInput` to open sessions through the
   manager.
8. Add popup, fixed, and Direct Window presentation handling.
9. Add runtime control action routing with an allowlist.
10. Remove direct keypad construction after compatibility coverage is verified.

The migration must preserve:

- numeric validation and min/max behavior;
- previous-value and range hints;
- popup placement behavior;
- ASCII editing behavior;
- write-through-runtime behavior;
- live-update suppression while editing.

## 12. Testing Strategy

### 12.1 Unit-Level Tests

Test the session and action layers without a visible window:

- numeric digit, decimal, sign, caret, backspace, delete, clear;
- ASCII insertion and deletion;
- password masking output;
- increment/decrement;
- min/max validation;
- confirm and cancel state transitions;
- action-to-session routing;
- unsupported action rejection;
- long-press policy;
- session replacement without implicit write;
- fallback resolution order.

### 12.2 Scene-Level Tests

Load a small custom TSCN containing:

- an input display;
- numeric buttons;
- clear/backspace;
- confirm and cancel;
- a runtime command button.

Verify that:

- the scene loads through the TSCN backend;
- buttons dispatch actions to the active session;
- the same scene can be reopened for another input;
- visual nodes refresh after buffer changes;
- errors remain visible without destroying the scene.

### 12.3 Presentation Tests

Verify all four modes:

- system fallback opens the correct built-in mode;
- popup appears in center, grid, and control-relative placements;
- fixed keypad remains visible across input selection;
- Direct Window binds both display and action controls;
- screen/window changes terminate without writing unconfirmed data.

### 12.4 Integration Tests

Verify with the runtime bridge:

- successful confirm writes the tag;
- failed write preserves the session/view;
- successful write is followed by the normal cache/re-read update;
- live tag updates do not overwrite active edits;
- `set_tag`, `toggle_tag`, and increment/decrement commands use the same
  runtime write path.

## 13. Acceptance Criteria

The design is considered implemented when:

1. A user can create a keypad TSCN with arbitrary layout and appearance.
2. An input object can select that keypad by `keypad_id`.
3. Numeric and ASCII input use the same session and action architecture.
4. Built-in default keypads remain available as fallback.
5. System, popup, fixed, and Direct Window modes all use the same session
   contract.
6. All listed input, navigation, and runtime-control actions are routed
   through the dispatcher.
7. Runtime-control actions are allowlisted and do not invoke arbitrary
   methods.
8. Confirm, cancel, validation failure, write failure, and screen changes have
   deterministic behavior.
9. Existing numeric range and placement behavior remains covered by tests.
10. A second backend could be added without modifying input controls or the
    action protocol.

## 14. Explicit Non-Goals

The first implementation does not include:

- an operating-system IME or native soft-keyboard bridge;
- undocumented vendor-private action numbers or binary protocol compatibility;
- a visual keypad-layout editor separate from the Godot scene editor;
- arbitrary script execution from serialized keypad actions;
- automatic confirmation of dirty sessions during navigation;
- a large prebuilt library of vendor-specific keypad layouts.
