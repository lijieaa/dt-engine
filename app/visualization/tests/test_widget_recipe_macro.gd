extends SceneTree
## WidgetRecipe / WidgetMacro 测试（Task 9）。
## headless: godot --headless --path app/visualization --script res://tests/test_widget_recipe_macro.gd

var _failures: Array[String] = []
var _checks := 0

func _initialize() -> void:
	print("=== test_widget_recipe_macro ===")
	# 前置：类必须已注册（T3 骨架）
	if not ClassDB.class_exists("WidgetRecipe"):
		_check(false, "WidgetRecipe 类已注册")
		quit(1)
		return
	if not ClassDB.class_exists("WidgetMacro"):
		_check(false, "WidgetMacro 类已注册")
		quit(1)
		return
	await test_recipe()
	await test_macro()
	if _failures.is_empty():
		print("RESULT: PASS — %d checks, 0 failure(s)" % _checks)
		quit(0)
		return
	print("RESULT: FAIL — %d checks, %d failure(s)" % [_checks, _failures.size()])
	for f in _failures:
		print("  FAIL: ", f)
	quit(1)

## 用例1：Recipe — configure(Dictionary) 解析、列举、构造写序列。
func test_recipe() -> void:
	print("[test] recipe")
	var wr: RefCounted = ClassDB.instantiate("WidgetRecipe")
	if not wr.has_method("configure"):
		_check(false, "WidgetRecipe.configure 存在")
		return
	var cfg := {
		"entries": [
			{"name": "R1", "writes": [
				{"tag": "A", "value": 1},
				{"tag": "B", "value": 2},
			]},
			{"name": "R2", "writes": [
				{"tag": "C", "value": 3},
			]},
		]
	}
	var ok: bool = wr.call("configure", cfg)
	_check(ok, "configure 合法配方返回 true")
	var entries: Array = wr.call("list_entries")
	_check(entries.size() == 2, "list_entries 数量 == 2 (got %s)" % entries.size())
	if entries.size() == 2:
		_check(String(entries[0]) == "R1", "第一条入口名 R1")
	# 构造写序列
	var seq: Array = wr.call("build_write_sequence", "R1")
	_check(seq.size() == 2, "R1 写序列长度 == 2 (got %s)" % seq.size())
	if seq.size() == 2:
		var s0: Dictionary = seq[0]
		var s1: Dictionary = seq[1]
		_check(String(s0.tag) == "A" and s0.value == 1, "seq[0] = {A, 1}")
		_check(String(s1.tag) == "B" and s1.value == 2, "seq[1] = {B, 2}")
	# 不存在入口
	var bad: Array = wr.call("build_write_sequence", "NOPE")
	_check(bad.is_empty(), "不存在入口返回空序列")
	# 非法配置
	var bad_ok: bool = wr.call("configure", {"entries": "not-an-array"})
	_check(not bad_ok, "非法配置 configure 返回 false")

## 用例2：Macro — configure 步骤 + 执行写序列（写事件经 match_tag 观察）。
func test_macro() -> void:
	print("[test] macro")
	var wm: RefCounted = ClassDB.instantiate("WidgetMacro")
	if not wm.has_method("configure"):
		_check(false, "WidgetMacro.configure 存在")
		return
	var steps := [
		{"tag": "A", "value": 5, "delay_ms": 1},
		{"tag": "B", "value": 6, "delay_ms": 1},
	]
	var ok: bool = wm.call("configure", {"steps": steps})
	_check(ok, "configure 合法宏返回 true")
	# 步骤计数
	var raw_steps: Array = wm.call("get_steps")
	_check(raw_steps.size() == 2, "get_steps 数量 == 2")
	# 信号收集（注意：GDScript lambda 捕获 bool 是值拷贝，须用数组包装引用）
	var written := []
	var done_ok := [false]
	if wm.has_signal("step_done"):
		wm.step_done.connect(func(idx, tag, v): written.append([idx, tag, v]))
	if wm.has_signal("finished"):
		wm.finished.connect(func(okk): done_ok[0] = okk)
	# ---- 同步执行：exec_ok 且立即产出 step_done ----
	var exec_ok: bool = wm.call("execute", true)
	_check(exec_ok, "execute(true) 返回 true")
	_check(written.size() == 2, "同步执行产出 2 个 step_done (got %d)" % written.size())
	await process_frame
	if written.size() == 2:
		_check(written[0][0] == 0 and written[0][1] == "A" and written[0][2] == 5, "step0 = {A,5}")
		_check(written[1][0] == 1 and written[1][1] == "B" and written[1][2] == 6, "step1 = {B,6}")
	_check(done_ok[0], "同步执行后 finished 信号发出且 ok == true")
	# ---- 延迟执行：execute(false) 仅调度，tick_run() 才触发 ----
	written.clear()
	done_ok[0] = false
	var sched: bool = wm.call("execute", false)
	_check(sched, "execute(false) 返回 true（已调度）")
	_check(written.size() == 0, "execute(false) 不立即执行")
	_check(bool(wm.call("is_running")) == true, "is_running() == true（调度中）")
	wm.call("tick_run")
	await process_frame
	_check(written.size() == 2, "tick_run() 触发后执行 2 步")
	_check(done_ok[0], "延迟执行后 finished 发出")
	# 非法配置
	var bad_ok: bool = wm.call("configure", {"steps": "nope"})
	_check(not bad_ok, "非法宏 configure 返回 false")

func _check(cond: bool, what: String) -> void:
	_checks += 1
	if cond:
		print("  PASS: ", what)
	else:
		_failures.append(what)
		print("  FAIL: ", what)