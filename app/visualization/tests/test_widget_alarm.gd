extends SceneTree
## WidgetAlarm C++ alarm condition evaluation + debounce test (Task 6).
## Uses the method API from the Task 6 overrides:
##   add_rule / evaluate / get_active_rules / enable_rule / disable_rule / remove_rule / clear
## Runs headless: godot --headless --script res://tests/test_widget_alarm.gd

func _initialize() -> void:
	print("=== test_widget_alarm ===")
	var failures := []
	var checks := 0

	# Instantiate the C++ class directly (Task 6 override 3: no WidgetNative facade).
	# Note: ClassDB.instantiate("WidgetAlarm") returns the native RefCounted
	# instance itself; calling .new() on it is a parse error in GDScript
	# ("Nonexistent function 'new' in base 'WidgetAlarm'"), so we use the
	# returned instance directly — same convention as test_native_load.gd.
	if not ClassDB.class_exists("WidgetAlarm"):
		printerr("FATAL: WidgetAlarm class not registered in ClassDB")
		quit(1)
		return
	var WidgetAlarm = ClassDB.instantiate("WidgetAlarm")
	if WidgetAlarm == null:
		printerr("FATAL: WidgetAlarm instantiation returned null")
		quit(1)
		return
	var alarm: Variant = WidgetAlarm

	# ---------- add_rule + evaluate: gt / lt / eq / ne / gte / lte ----------
	alarm.add_rule("HI", {"type": "gt", "threshold": 100.0, "debounce_ms": 0})
	checks += 1
	_expect(failures, "evaluate(HI, 99.0) false below threshold", alarm.evaluate("HI", 99.0), false)

	checks += 1
	_expect(failures, "evaluate(HI, 101.0) true above threshold", alarm.evaluate("HI", 101.0), true)

	checks += 1
	_expect(failures, "get_active_rules() contains HI while triggered", alarm.get_active_rules().has("HI"), true)

	checks += 1
	_expect(failures, "evaluate(HI, 100.0) false at threshold (strict gt)", alarm.evaluate("HI", 100.0), false)

	checks += 1
	_expect(failures, "get_active_rules() cleared when condition no longer holds", alarm.get_active_rules().has("HI"), false)

	alarm.add_rule("LOW", {"type": "lt", "threshold": 20, "debounce_ms": 0})
	checks += 1
	_expect(failures, "evaluate(LOW, 5) true below threshold", alarm.evaluate("LOW", 5), true)
	_expect(failures, "get_active_rules() contains LOW while triggered", alarm.get_active_rules().has("LOW"), true)
	_expect(failures, "evaluate(LOW, 20) false at threshold (strict lt)", alarm.evaluate("LOW", 20), false)

	alarm.add_rule("EQ", {"type": "eq", "threshold": 42, "debounce_ms": 0})
	checks += 1
	_expect(failures, "evaluate(EQ, 42) true when equal", alarm.evaluate("EQ", 42), true)
	_expect(failures, "evaluate(EQ, 43) false when not equal", alarm.evaluate("EQ", 43), false)

	alarm.add_rule("NE", {"type": "ne", "threshold": 0, "debounce_ms": 0})
	checks += 1
	_expect(failures, "evaluate(NE, 1) true when not equal", alarm.evaluate("NE", 1), true)
	_expect(failures, "evaluate(NE, 0) false when equal", alarm.evaluate("NE", 0), false)

	alarm.add_rule("GE", {"type": "gte", "threshold": 10.0, "debounce_ms": 0})
	checks += 1
	_expect(failures, "evaluate(GE, 10.0) true at threshold (gte inclusive)", alarm.evaluate("GE", 10.0), true)
	_expect(failures, "evaluate(GE, 9.9) false below threshold", alarm.evaluate("GE", 9.9), false)

	alarm.add_rule("LE", {"type": "lte", "threshold": 10.0, "debounce_ms": 0})
	checks += 1
	_expect(failures, "evaluate(LE, 10.0) true at threshold (lte inclusive)", alarm.evaluate("LE", 10.0), true)
	_expect(failures, "evaluate(LE, 10.1) false above threshold", alarm.evaluate("LE", 10.1), false)

	# ---------- enable_rule / disable_rule ----------
	# Re-trigger HI first so we have something to disable.
	checks += 1
	_expect(failures, "evaluate(HI, 200.0) true (pre-disable)", alarm.evaluate("HI", 200.0), true)
	alarm.disable_rule("HI")
	_expect(failures, "evaluate(HI, 200.0) false while disabled", alarm.evaluate("HI", 200.0), false)
	_expect(failures, "get_active_rules() excludes HI while disabled", alarm.get_active_rules().has("HI"), false)

	checks += 1
	alarm.enable_rule("HI")
	_expect(failures, "evaluate(HI, 200.0) true again after re-enable", alarm.evaluate("HI", 200.0), true)

	# ---------- remove_rule ----------
	checks += 1
	alarm.remove_rule("NE")
	# evaluate() returns a bool; an unknown rule simply evaluates to false.
	_expect(failures, "evaluate(NE, 1) false (unknown rule) after remove", alarm.evaluate("NE", 1), false)
	_expect(failures, "removed rule no longer appears in active_rules", alarm.get_active_rules().has("NE"), false)

	# ---------- clear ----------
	checks += 1
	alarm.clear()
	_expect(failures, "evaluate(after clear) false for all rules", alarm.evaluate("HI", 200.0), false)
	_expect(failures, "get_active_rules() empty after clear", alarm.get_active_rules().is_empty(), true)

	# ---------- debounce ----------
	# debounce_ms = 0 triggers immediately.
	alarm.add_rule("D0", {"type": "gt", "threshold": 0.0, "debounce_ms": 0})
	checks += 1
	_expect(failures, "debounce_ms=0 triggers immediately", alarm.evaluate("D0", 5.0), true)

	# Real debounce window: first evaluation is pending; after the window
	# elapses the alarm becomes active. We busy-wait on the engine's own
	# monotonic clock (same source the C++ debounce uses) so the test is
	# deterministic — frame-based create_timer() elapsed can land short of
	# the window in headless --script mode.
	alarm.add_rule("D3", {"type": "gt", "threshold": 0.0, "debounce_ms": 200})
	checks += 1
	_expect(failures, "debounced rule pending (not yet triggered)", alarm.evaluate("D3", 5.0), false)

	var tstart: int = Time.get_ticks_msec()
	while Time.get_ticks_msec() - tstart < 400:
		OS.delay_msec(10)

	checks += 1
	_expect(failures, "debounced rule triggers after debounce_ms elapsed", alarm.evaluate("D3", 5.0), true)
	_expect(failures, "debounced rule now in active_rules", alarm.get_active_rules().has("D3"), true)

	# Debounce resets when the condition clears; a new pending window starts.
	alarm.evaluate("D3", -1.0)  # condition no longer holds -> reset
	_expect(failures, "debounced rule clears when condition releases", alarm.evaluate("D3", -1.0), false)

	print("RESULT: %s — %d checks, %d failure(s)"
		% ["PASS" if failures.is_empty() else "FAIL", checks, failures.size()])
	for f in failures:
		print("  FAIL: ", f)
	quit(0 if failures.is_empty() else 1)


## Append to failures when actual != expected (String-aware compare).
func _expect(failures: Array, label: String, actual, expected) -> void:
	if actual != expected:
		failures.append("%s expected '%s' (%s), got '%s' (%s)" % [
			label, str(expected), typeof(expected), str(actual), typeof(actual),
		])
	else:
		print("  PASS: ", label, " -> ", actual)