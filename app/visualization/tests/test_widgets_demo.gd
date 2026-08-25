extends SceneTree
## 演示画面集成测试（Task 12）。
## headless 加载 Screen2_WidgetsDemo.tscn，强制 bridge 到 mock 档，
## 推一组确定性值，断言：NumDisplay 格式化正确 / Lamp 翻转 / Trend 收样本 /
## Alarm 上色 active / Switch 切换触发写 / Recipe 写序列 / Macro 执行。
## headless: godot --headless --path app/visualization --script res://tests/test_widgets_demo.gd

const DEMO_SCENE := "res://scenes/Screen2_WidgetsDemo.tscn"
const DEMO_CTRL_SCRIPT := "res://addons/industrial_widgets/demo/demo_controller.gd"

var _failures: Array[String] = []
var _checks := 0

func _initialize() -> void:
	await process_frame
	# 前置：先强制 bridge 到 mock 并关闭自动 tick（保持确定性）
	var bridge: Node = root.get_node_or_null("WidgetBridge")
	if bridge != null:
		bridge.set("use_mock", true)
		bridge.call("_detect_backend")
		var mock: Node = bridge.get("_mock")
		if mock != null:
			mock.set("tick_interval", 0)
	var code := await _run_all()
	quit(code)

func _run_all() -> int:
	print("=== test_widgets_demo ===")
	if not ResourceLoader.exists(DEMO_SCENE):
		_check(false, "Screen2_WidgetsDemo.tscn 存在")
		print("RESULT: FAIL — 场景缺失")
		return 1
	var demo: Node = (load(DEMO_SCENE) as PackedScene).instantiate()
	root.add_child(demo)
	await process_frame
	# demo 控制器已挂载（内部把 mock tags 配置好）
	var ctrl: Node = demo.get_node_or_null("DemoController")
	_check(ctrl != null, "DemoController 存在")
	if ctrl == null:
		_check(false, "演示场景缺少 DemoController 脚本")
		return 1
	# 场景加载后立刻关闭 mock 自动 tick（Demo 控制器可能开了 0.5s tick）
	var bridge: Node = root.get_node_or_null("WidgetBridge")
	var mock: Node = bridge.get("_mock") if bridge != null else null
	if mock != null:
		mock.set("tick_interval", 0)
	await process_frame

	# ---- 断言 1：TagNumDisplay 格式化 ----
	var nmpath: String = String(ctrl.get("num_display_path"))
	var num_disp: Label = demo.get_node_or_null(nmpath)
	_check(num_disp != null, "TagNumDisplay 挂载 (path=%s)" % nmpath)
	if num_disp != null:
		num_disp.set("tag_name", "Pump01.Speed")
		num_disp.call("_ready")
		num_disp.set("format_cfg", {"decimals": 1, "thousands": true})
		mock.call("push", "Pump01.Speed", 1234.5, "good")
		await process_frame
		_check(num_disp.text.contains("1,234"), "NumDisplay 文本含千分位 (got '%s')" % num_disp.text)

	# ---- 断言 2：TagLamp 翻转 ----
	var lamppath: String = String(ctrl.get("lamp_path"))
	var lamp: Panel = demo.get_node_or_null(lamppath)
	_check(lamp != null, "TagLamp 挂载")
	if lamp != null:
		lamp.set("tag_name", "Valve02.Open")
		lamp.call("_ready")
		mock.call("push", "Valve02.Open", 0, "good")
		await process_frame
		var lit0: bool = bool(lamp.get("lit"))
		mock.call("push", "Valve02.Open", 1, "good")
		await process_frame
		_check(not lit0 and bool(lamp.get("lit")), "Lamp 0→1 翻转 (lit: %s→%s)" % [str(lit0), str(lamp.get("lit"))])

	# ---- 断言 3：TagTrend 收样本 ----
	var trpath: String = String(ctrl.get("trend_path"))
	var trend: Control = demo.get_node_or_null(trpath)
	_check(trend != null, "TagTrend 挂载")
	if trend != null:
		trend.set("tag_name", "Tank01.Level")
		trend.call("_ready")
		for i in range(4):
			mock.call("push", "Tank01.Level", float(i) * 10.0, "good")
		await process_frame
		var core: RefCounted = trend.get("_trend_core")
		_check(core != null and core.get("sample_count") >= 4, "Trend 收到 ≥4 样本 (got %s)" % str(core.get("sample_count") if core else "no-core"))

	# ---- 断言 4：TagAlarmList active 上色 ----
	var alpath: String = String(ctrl.get("alarm_path"))
	var alm: ItemList = demo.get_node_or_null(alpath)
	_check(alm != null, "TagAlarmList 挂载")
	if alm != null:
		alm.set("monitored_tags", ["Tank01.Level"])
		alm.set("threshold", 50.0)
		alm.set("comparator", "gt")
		alm.set("debounce_ms", 0)
		alm.call("_ready")
		mock.call("push", "Tank01.Level", 80.0, "good")
		await process_frame
		var core_a: RefCounted = alm.get("_alarm_core")
		var act: Array = core_a.call("get_active_rules") if core_a != null else []
		_check(act.size() == 1 and act.has("Tank01.Level"), "Alarm active 规则数==1 (got %s)" % str(act))
		_check(alm.get("_active_count") == 1, "AlarmList UI active 计数==1")

	# ---- 断言 5：TagSwitch 切换触发写事件 ----
	var swpath: String = String(ctrl.get("switch_path"))
	var sw: CheckBox = demo.get_node_or_null(swpath)
	_check(sw != null, "TagSwitch 挂载")
	if sw != null:
		var got_writes := []
		bridge.connect("write_result", func(rid, tag, ok, err): got_writes.append([tag, ok]))
		sw.set("tag_name", "Valve02.Open")
		sw.call("_ready")
		sw.set_pressed(true)
		sw.emit_signal("toggled", true)
		await process_frame
		_check(not got_writes.is_empty() and got_writes[0][0] == "Valve02.Open", "Switch 切换写 Valve02.Open (got %s)" % str(got_writes))

	# ---- 断言 6：TagRecipe apply 批量写 ----
	var recpath: String = String(ctrl.get("recipe_path"))
	var rec: Control = demo.get_node_or_null(recpath)
	_check(rec != null, "TagRecipe 挂载")
	if rec != null:
		var seq: Array = rec.call("build_sequence", "R1")
		_check(seq.size() >= 1, "Recipe R1 写序列 ≥1 (got %d)" % seq.size())

	# ---- 断言 7：TagMacroButton 执行 ----
	var mbpath: String = String(ctrl.get("macro_path"))
	var mbtn: Button = demo.get_node_or_null(mbpath)
	_check(mbtn != null, "TagMacroButton 挂载")
	if mbtn != null:
		var got_steps := []
		var mcore: RefCounted = mbtn.get("_macro_core")
		if mcore != null and mcore.has_signal("step_done"):
			mcore.step_done.connect(func(idx, tag, v): got_steps.append([idx, tag, v]))
		mbtn.emit_signal("pressed")
		await process_frame
		_check(not got_steps.is_empty(), "Macro pressed 后产出 step_done (got %d)" % got_steps.size())

	demo.queue_free()
	if _failures.is_empty():
		print("RESULT: PASS — %d checks, 0 failure(s)" % _checks)
		return 0
	print("RESULT: FAIL — %d checks, %d failure(s)" % [_checks, _failures.size()])
	for f in _failures:
		print("  FAIL: ", f)
	return 1

func _check(cond: bool, what: String) -> void:
	_checks += 1
	if cond:
		print("  PASS: ", what)
	else:
		_failures.append(what)
		print("  FAIL: ", what)