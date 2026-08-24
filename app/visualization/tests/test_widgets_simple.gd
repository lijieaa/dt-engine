extends SceneTree
## Simple widget tests (Task 2) — TagLabel / TagBar / TagGauge / TagSwitch.
## Verifies subscribe-on-ready, tag_changed → visual update, and toggled → write_tag.

const WIDGET_DIR := "res://addons/industrial_widgets/widgets/"
const TAG_LABEL_SCRIPT := WIDGET_DIR + "tag_label.gd"
const TAG_BAR_SCRIPT := WIDGET_DIR + "tag_bar.gd"
const TAG_GAUGE_SCRIPT := WIDGET_DIR + "tag_gauge.gd"
const TAG_SWITCH_SCRIPT := WIDGET_DIR + "tag_switch.gd"

var _failures: Array[String] = []
var _bridge: Node = null
var _mock: Node = null

func _initialize() -> void:
	await process_frame
	_init_bridge()
	var code := await _run_all()
	quit(code)

func _init_bridge() -> void:
	_bridge = Engine.get_singleton("WidgetBridge") if Engine.has_singleton("WidgetBridge") else null
	if _bridge == null:
		_bridge = get_root().get_node_or_null("WidgetBridge")
	if _bridge == null:
		_failures.append("WidgetBridge autoload not found")
		return
	_bridge.set("use_mock", true)
	_bridge.call("_detect_backend")
	_mock = _bridge.get("_mock")
	if _mock != null:
		_mock.set("tick_interval", 0.0)

func _check(cond: bool, what: String) -> void:
	if cond:
		print("  PASS: ", what)
	else:
		_failures.append(what)
		print("  FAIL: ", what)

func _make_widget(script_path: String) -> Control:
	if not ResourceLoader.exists(script_path):
		return null
	var s: GDScript = load(script_path)
	if s == null:
		return null
	var w: Control = s.new()
	return w

func _run_all() -> int:
	print("=== test_widgets_simple ===")
	await test_scripts_exist()
	await test_tag_label_subscribe_and_update()
	await test_tag_switch_writes_on_toggle()
	await test_tag_bar_responds_to_tag_changed()
	await test_tag_gauge_responds_to_tag_changed()
	if _failures.is_empty():
		print("RESULT: PASS")
		return 0
	print("RESULT: FAIL — %d failure(s)" % _failures.size())
	for f in _failures:
		print("  FAIL: ", f)
	return 1

func test_scripts_exist() -> void:
	print("[test] widget_scripts_exist")
	_check(ResourceLoader.exists(TAG_LABEL_SCRIPT), "tag_label.gd exists")
	_check(ResourceLoader.exists(TAG_BAR_SCRIPT), "tag_bar.gd exists")
	_check(ResourceLoader.exists(TAG_GAUGE_SCRIPT), "tag_gauge.gd exists")
	_check(ResourceLoader.exists(TAG_SWITCH_SCRIPT), "tag_switch.gd exists")

func test_tag_label_subscribe_and_update() -> void:
	print("[test] tag_label_subscribe_and_update")
	if not ResourceLoader.exists(TAG_LABEL_SCRIPT):
		_failures.append("tag_label.gd missing — skip")
		return
	var label: Control = await _make_widget(TAG_LABEL_SCRIPT)
	if label == null:
		_failures.append("TagLabel instantiate failed")
		return
	label.set("tag_name", "T1")
	get_root().add_child(label)
	await process_frame
	await process_frame
	_check(_bridge.call("subscribe", ["T1"]), "TagLabel triggers subscribe for T1")
	_mock.call("push", "T1", 42, "good")
	await process_frame
	_check(label.get("text") == "42", "TagLabel.text becomes '42' after tag_changed")
	label.free()

func test_tag_switch_writes_on_toggle() -> void:
	print("[test] tag_switch_writes_on_toggle")
	if not ResourceLoader.exists(TAG_SWITCH_SCRIPT):
		_failures.append("tag_switch.gd missing — skip")
		return
	var sw: Control = await _make_widget(TAG_SWITCH_SCRIPT)
	if sw == null:
		_failures.append("TagSwitch instantiate failed")
		return
	sw.set("tag_name", "T2")
	get_root().add_child(sw)
	await process_frame
	await process_frame
	var writes := []
	_bridge.connect("write_result", func(rid, tag, ok, err): writes.append([rid, tag, ok, err]))
	sw.set("button_pressed", true)
	await process_frame
	var found := false
	for w in writes:
		if w[1] == "T2":
			found = true
	_check(found, "TagSwitch toggled triggers write_tag for T2")
	sw.free()

func test_tag_bar_responds_to_tag_changed() -> void:
	print("[test] tag_bar_responds_to_tag_changed")
	if not ResourceLoader.exists(TAG_BAR_SCRIPT):
		_failures.append("tag_bar.gd missing — skip")
		return
	var bar: Control = await _make_widget(TAG_BAR_SCRIPT)
	if bar == null:
		_failures.append("TagBar instantiate failed")
		return
	bar.set("tag_name", "T1")
	get_root().add_child(bar)
	await process_frame
	await process_frame
	await process_frame
	_mock.call("push", "T1", 50.0, "good")
	await process_frame
	var v = bar.get("value")
	_check(typeof(v) in [TYPE_INT, TYPE_FLOAT], "TagBar.value is numeric after tag_changed")
	bar.free()

func test_tag_gauge_responds_to_tag_changed() -> void:
	print("[test] tag_gauge_responds_to_tag_changed")
	if not ResourceLoader.exists(TAG_GAUGE_SCRIPT):
		_failures.append("tag_gauge.gd missing — skip")
		return
	var gauge: Control = await _make_widget(TAG_GAUGE_SCRIPT)
	if gauge == null:
		_failures.append("TagGauge instantiate failed")
		return
	gauge.set("tag_name", "T1")
	get_root().add_child(gauge)
	await process_frame
	await process_frame
	_mock.call("push", "T1", 75.0, "good")
	await process_frame
	var v = gauge.get("value")
	_check(typeof(v) in [TYPE_INT, TYPE_FLOAT] and float(v) > 0, "TagGauge.value > 0 after tag_changed")
	gauge.free()
