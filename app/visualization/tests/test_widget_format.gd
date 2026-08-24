extends SceneTree
## WidgetFormat C++ numeric formatting core test (Task 4).
## Runs headless: godot --headless --script res://tests/test_widget_format.gd

func _initialize() -> void:
	print("=== test_widget_format ===")
	var failures := []
	var checks := 0

	# Instantiate via ClassDB (class is compiled into the engine binary).
	if not ClassDB.class_exists("WidgetFormat"):
		printerr("FATAL: WidgetFormat class not registered in ClassDB")
		quit(1)
		return
	var wf: RefCounted = ClassDB.instantiate("WidgetFormat")
	if wf == null:
		printerr("FATAL: WidgetFormat instantiation returned null")
		quit(1)
		return

	checks += 1
	var r1 = WidgetFormat.format_value(1234.567, {"decimals": 2, "thousands": true})
	expect(failures, "format_value(1234.567, {decimals:2, thousands:true})", r1, "1,234.57")

	checks += 1
	var r2 = WidgetFormat.format_value(1234.567, {"decimals": 2, "thousands": false})
	expect(failures, "format_value(1234.567, {decimals:2, thousands:false})", r2, "1234.57")

	checks += 1
	var r3 = WidgetFormat.format_value(0x12, {"bcd": true})
	expect(failures, "format_value(0x12, {bcd:true})", r3, "12")

	checks += 1
	var r4 = WidgetFormat.format_value(0x1234, {"bcd": true})
	expect(failures, "format_value(0x1234, {bcd:true})", r4, "1234")

	checks += 1
	var r5 = WidgetFormat.format_value(-1234, {"decimals": 0, "thousands": true})
	expect(failures, "format_value(-1234, {thousands:true})", r5, "-1,234")

	checks += 1
	var r6 = WidgetFormat.format_value(0, {"bcd": true})
	expect(failures, "format_value(0, {bcd:true})", r6, "0")

	checks += 1
	var r7 = WidgetFormat.format_value(42, {"prefix": "PV=", "suffix": " ℃"})
	expect(failures, "format_value(42, {prefix, suffix})", r7, "PV=42 ℃")

	# parse_input: invalid string
	checks += 1
	var p1 = WidgetFormat.parse_input("abc", {"decimals": 0})
	if p1.ok:
		failures.append("parse_input('abc', {decimals:0}).ok expected false, got true")

	# parse_input: empty string
	checks += 1
	var p2 = WidgetFormat.parse_input("", {"decimals": 0})
	if p2.ok or (p2.error.is_empty()):
		failures.append("parse_input('', {decimals:0}) expected ok=false with error message")

	# parse_input: valid negative float with decimals
	checks += 1
	var p3 = WidgetFormat.parse_input("-3.5", {"decimals": 1})
	if not p3.ok:
		failures.append("parse_input('-3.5', {decimals:1}).ok expected true, got false")
	else:
		var v3: float = p3.value
		if absf(v3 - -3.5) > 0.0001:
			failures.append("parse_input('-3.5', {decimals:1}).value expected -3.5, got %s" % str(v3))

	# parse_input: valid negative int with decimals 0
	checks += 1
	var p4 = WidgetFormat.parse_input("-254", {"decimals": 0})
	if not p4.ok:
		failures.append("parse_input('-254', {decimals:0}).ok expected true, got false")
	elif p4.value != -254:
		failures.append("parse_input('-254', {decimals:0}).value expected -254, got %s" % str(p4.value))

	# parse_input: float rejected when decimals == 0
	checks += 1
	var p5 = WidgetFormat.parse_input("3.5", {"decimals": 0})
	if p5.ok:
		failures.append("parse_input('3.5', {decimals:0}) expected ok=false (int expected), got true")

	print("RESULT: %s — %d checks, %d failure(s)"
		% ["PASS" if failures.is_empty() else "FAIL", checks, failures.size()])
	for f in failures:
		print("  FAIL: ", f)
	quit(0 if failures.is_empty() else 1)


## Append to failures when actual != expected (String-aware compare).
func expect(failures: Array, label: String, actual, expected) -> void:
	if actual != expected:
		failures.append("%s expected '%s' (%s), got '%s' (%s)" % [
			label, str(expected), typeof(expected), str(actual), typeof(actual),
		])
	else:
		print("  PASS: ", label, " -> ", actual)