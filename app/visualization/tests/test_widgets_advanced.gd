extends SceneTree
## 复杂元件测试（Task 10）：TagTrend / TagAlarmList / TagRecipe / TagMacroButton。
## 每个元件 _ready 订阅 WidgetBridge(mock 档) 的 tag_changed；_on_tag_changed 调 C++ 核心刷新 UI。
## headless: godot --headless --path app/visualization --script res://tests/test_widgets_advanced.gd

const WIDGET_DIR := "res://addons/industrial_widgets/widgets/"

var _failures: Array[String] = []
var _checks := 0

func _initialize() -> void:
	await process_frame
	# 强制 bridge 到 mock 档，保证确定性推值
	var bridge: Node = root.get_node_or_null("WidgetBridge")
	if bridge != null:
		bridge.set("use_mock", true)
		bridge.call("_detect_backend")
		var mock: Node = bridge.get("_mock")
		if mock != null:
			# 关闭自动 tick：测试全靠显式 push 驱动（避免 random 模式 tag 干扰断言）
			mock.set("tick_interval", 0)
			mock.call("add_mock_tag", "TREND1", "sine", 0.0, 100.0)
			mock.call("add_mock_tag", "ALM1", "random", 0.0, 100.0)
			mock.call("add_mock_tag", "RECIPE1", "sine", 0.0, 100.0)
			mock.call("add_mock_tag", "MACRO1", "sine", 0.0, 100.0)
	else:
		_failures.append("WidgetBridge autoload 不存在")

	var code := await _run_all()
	quit(code)

func _run_all() -> int:
	print("=== test_widgets_advanced ===")
	await test_trend()
	await test_alarm_list()
	await test_recipe()
	await test_macro_button()
	if _failures.is_empty():
		print("RESULT: PASS — %d checks, 0 failure(s)" % _checks)
		return 0
	print("RESULT: FAIL — %d checks, %d failure(s)" % [_checks, _failures.size()])
	for f in _failures:
		print("  FAIL: ", f)
	return 1

## 用例1：TagTrend — mock 推值 → C++ WidgetTrend 环形缓冲累积 → 可 query 画折线。
func test_trend() -> void:
	print("[test] trend")
	var script: GDScript = load(WIDGET_DIR + "tag_trend.gd")
	_check(script != null, "tag_trend.gd 可加载")
	if script == null:
		return
	var trend: Control = script.new()
	root.add_child(trend)
	trend.set("tag_name", "TREND1")
	trend.set("buffer_capacity", 64)
	trend.call("_ready")
	# mock 推 5 个值（确定性，不依赖正弦）
	var mock: Node = _get_mock()
	for i in range(5):
		mock.call("push", "TREND1", float(i * 10), "good")
	await process_frame
	# 内部 C++ WidgetTrend 已累积 5 样本
	var core: RefCounted = trend.get("_trend_core")
	_check(core != null, "内部 WidgetTrend 核心已创建")
	if core != null:
		var cnt: int = core.get("sample_count")
		_check(cnt == 5, "内部核心 sample_count == 5 (got %d)" % cnt)
		var w: Array = core.query_window(0, 999999)
		_check(w.size() == 5, "query_window 全窗 5 点")
	# 缓存画线点数非 0（_draw 消费 query_window）
	var pts: int = int(trend.get("_render_pts"))
	_check(pts >= 1, "_render_pts >= 1 (got %d)" % pts)
	trend.queue_free()

## 用例2：TagAlarmList — 监控 tag 列表，WidgetAlarm 规则求值 → active 项上色。
func test_alarm_list() -> void:
	print("[test] alarm_list")
	var script: GDScript = load(WIDGET_DIR + "tag_alarm_list.gd")
	_check(script != null, "tag_alarm_list.gd 可加载")
	if script == null:
		return
	var alm: ItemList = script.new()
	root.add_child(alm)
	alm.set("monitored_tags", ["ALM1"])
	alm.set("threshold", 50.0)
	alm.set("comparator", "gt")
	alm.set("debounce_ms", 0)
	alm.call("_ready")
	var mock: Node = _get_mock()
	mock.call("push", "ALM1", 20.0, "good")   # 低于阈值：非 active
	await process_frame
	var core: RefCounted = alm.get("_alarm_core")
	_check(core != null, "内部 WidgetAlarm 核心已创建")
	if core != null:
		_check((core.call("get_active_rules") as Array).size() == 0, "低值无 active 规则")
		_check(core.call("evaluate", "ALM1", 20.0) == false, "evaluate(20) == false")
	mock.call("push", "ALM1", 80.0, "good")   # 超过阈值：触发
	await process_frame
	if core != null:
		var act: Array = core.call("get_active_rules")
		_check(act.size() == 1, "高值 active 规则数 == 1 (got %d)" % act.size())
		_check(act.has("ALM1"), "active 规则含 ALM1")
		_check(core.call("evaluate", "ALM1", 80.0) == true, "evaluate(80) == true")
	# UI 侧 active 计数同步
	var ui_active: int = int(alm.get("_active_count"))
	_check(ui_active == 1, "UI active 计数 == 1 (got %d)" % ui_active)
	alm.queue_free()

## 用例3：TagRecipe — 配置配方 → 切换 → build_write_sequence → 批量 write_tag。
func test_recipe() -> void:
	print("[test] recipe")
	var script: GDScript = load(WIDGET_DIR + "tag_recipe.gd")
	_check(script != null, "tag_recipe.gd 可加载")
	if script == null:
		return
	var rec: Control = script.new()
	root.add_child(rec)
	var recipe_cfg := {
		"entries": [
			{"name": "R1", "writes": [
				{"tag": "RECIPE1", "value": 1},
				{"tag": "RECIPE1", "value": 2},
			]},
			{"name": "R2", "writes": [
				{"tag": "RECIPE1", "value": 3},
			]},
		]
	}
	rec.set("recipe_cfg", recipe_cfg)
	rec.call("_ready")
	var core: RefCounted = rec.get("_recipe_core")
	_check(core != null, "内部 WidgetRecipe 核心已创建")
	if core != null:
		var entries: Array = core.call("list_entries")
		_check(entries.size() == 2, "list_entries == 2")
	# 触发"切换配方 R1"写序列
	var seq: Array = rec.call("build_sequence", "R1")
	_check(seq.size() == 2, "R1 序列 2 步 (got %d)" % seq.size())
	# 写回 bridge（mock 收 write_result）
	var got_writes := []
	var bridge: Node = root.get_node_or_null("WidgetBridge")
	if bridge != null:
		bridge.connect("write_result", func(rid, tag, ok, err): got_writes.append([tag, ok]))
	rec.call("apply_recipe", "R1")
	await process_frame
	_check(got_writes.size() == 2, "apply_recipe(R1) 产生 2 次写 (got %d)" % got_writes.size())
	rec.queue_free()

## 用例4：TagMacroButton — pressed → WidgetMacro.execute → step_done 刷新文本/禁用态。
func test_macro_button() -> void:
	print("[test] macro_button")
	var script: GDScript = load(WIDGET_DIR + "tag_macro_button.gd")
	_check(script != null, "tag_macro_button.gd 可加载")
	if script == null:
		return
	var btn: Button = script.new()
	root.add_child(btn)
	var macro_cfg := {
		"steps": [
			{"tag": "MACRO1", "value": 7, "delay_ms": 1},
			{"tag": "MACRO1", "value": 8, "delay_ms": 1},
		]
	}
	btn.set("macro_cfg", macro_cfg)
	btn.call("_ready")
	var core: RefCounted = btn.get("_macro_core")
	_check(core != null, "内部 WidgetMacro 核心已创建")
	# pressed → 同步执行 2 步
	var got_steps := []
	if core != null and core.has_signal("step_done"):
		core.step_done.connect(func(idx, tag, v): got_steps.append([idx, tag, v]))
	btn.emit_signal("pressed")
	# 同步执行立即产出；等一帧让 UI 文本刷新
	await process_frame
	_check(got_steps.size() == 2, "pressed 后 2 个 step_done (got %d)" % got_steps.size())
	var running: bool = bool(btn.get("_busy"))
	_check(not running, "执行完成后 _busy == false")
	var txt: String = btn.text
	_check(txt.contains("2") or txt.contains("done") or txt.contains("完成"), "按钮文本反映完成态 (got: '%s')" % txt)
	btn.queue_free()

# ---------- helpers ----------

func _get_mock() -> Node:
	var bridge: Node = root.get_node_or_null("WidgetBridge")
	if bridge == null:
		return null
	return bridge.get("_mock")

func _check(cond: bool, what: String) -> void:
	_checks += 1
	if cond:
		print("  PASS: ", what)
	else:
		_failures.append(what)
		print("  FAIL: ", what)