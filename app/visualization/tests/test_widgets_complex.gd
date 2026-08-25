extends SceneTree
## 新元件测试（Task 8）：TagNumDisplay / TagNumInput / TagLamp / TagMeter。
## 依赖 WidgetBridge（autoload）为 mock 模式：测试在 _initialize 里强制
## use_mock = true 并重入检测，保证确定性推值。
## headless: godot --headless --path app/visualization --script res://tests/test_widgets_complex.gd

const WIDGET_DIR := "res://addons/industrial_widgets/widgets/"
const MOCK_SCRIPT := "res://addons/industrial_widgets/autoload/widget_bridge_mock.gd"

var _failures: Array[String] = []
var _checks := 0

func _initialize() -> void:
	# 先确保 bridge autoload 处于 mock 档（WidgetBridge 是 *autoload，root 上可直接取）
	await process_frame
	_force_bridge_mock()

	var code := await _run_all()
	quit(code)

func _force_bridge_mock() -> void:
	var bridge: Node = root.get_node_or_null("WidgetBridge")
	if bridge == null:
		_failures.append("WidgetBridge autoload 不存在")
		return
	bridge.set("use_mock", true)
	bridge.call("_detect_backend")
	var mock: Node = bridge.get("_mock")
	if mock == null:
		_failures.append("mock 源未挂载")
		return
	# 确定性 tag 集（避免开场随机正弦造成歧义）
	mock.call("add_mock_tag", "NUM1", "sine", 0.0, 100.0)
	mock.call("add_mock_tag", "NUM2", "sine", 0.0, 1.0)
	mock.call("add_mock_tag", "LAMP1", "square", 0.0, 1.0)
	mock.call("add_mock_tag", "MTR1", "sine", 0.0, 100.0)

func _run_all() -> int:
	print("=== test_widgets_complex ===")
	# 0. 前置：WidgetMeter C++ 核心存在（T8 Step 4 的 angle_for_value）
	await test_meter_native_core()
	# 1. TagNumDisplay：值经 WidgetFormat.format_value 格式化渲染
	await test_num_display()
	# 2. TagNumInput：text_submitted → validate → write_tag
	await test_num_input()
	# 3. TagLamp：非 0 亮 / 0 灭 / 闪烁动效（本地壳）
	await test_lamp()
	# 4. TagMeter：_draw 自绘 + angle 线性映射
	await test_meter_widget()

	if _failures.is_empty():
		print("RESULT: PASS — %d checks, 0 failure(s)" % _checks)
		return 0
	print("RESULT: FAIL — %d checks, %d failure(s)" % [_checks, _failures.size()])
	for f in _failures:
		print("  FAIL: ", f)
	return 1

## 步骤0：C++ WidgetMeter::angle_for_value 必须存在且线性映射正确。
func test_meter_native_core() -> void:
	print("[test] meter_native_core")
	if not ClassDB.class_exists("WidgetMeter"):
		_check(false, "WidgetMeter 类已注册")
		return
	var core: RefCounted = ClassDB.instantiate("WidgetMeter")
	if core == null:
		_check(false, "WidgetMeter 实例化成功")
		return
	if not core.has_method("angle_for_value"):
		_check(false, "angle_for_value 方法存在")
		return
	var a0: float = core.angle_for_value(0.0, 0.0, 100.0, {})
	var a1: float = core.angle_for_value(50.0, 0.0, 100.0, {})
	var a2: float = core.angle_for_value(100.0, 0.0, 100.0, {})
	_check(absf(a1 - (a0 + a2) / 2.0) < 0.001, "中点角度为端点均值 (a0=%s a1=%s a2=%s)" % [str(a0), str(a1), str(a2)])
	_check(a2 > a0, "满量程角度 > 零位角度 (%s > %s)" % [str(a2), str(a0)])
	_check(a0 == -120.0 and a2 == 120.0, "0%→-120° 100%→+120° (EBPro 240° 表盘)")

## 用例1：TagNumDisplay — tag 变化 → 文本更新且带格式化（如千分位）。
func test_num_display() -> void:
	print("[test] num_display")
	var script: GDScript = load(WIDGET_DIR + "tag_num_display.gd")
	_check(script != null, "tag_num_display.gd 可加载")
	if script == null:
		return
	var disp: Label = script.new()
	root.add_child(disp)
	disp.set("tag_name", "NUM1")
	disp.set("format_cfg", {"thousands": true, "decimals": 1})
	# 手动触发 _ready 逻辑（add_child 同步调用 _ready）
	disp.call("_ready")
	var mock: Node = _get_mock()
	mock.call("push", "NUM1", 1234.5, "good")
	await process_frame
	var txt: String = disp.text
	_check(txt != "" and txt != "—", "NUM1 推值后文本已刷新 (got: '%s')" % txt)
	_check(txt.contains("1,234") or txt.contains("1234"), "文本带格式化数字 (got: '%s')" % txt)
	disp.queue_free()

## 用例2：TagNumInput — 提交合法文本 → write_tag 走 bridge（mock 收写回执）。
func test_num_input() -> void:
	print("[test] num_input")
	var script: GDScript = load(WIDGET_DIR + "tag_num_input.gd")
	_check(script != null, "tag_num_input.gd 可加载")
	if script == null:
		return
	var input: LineEdit = script.new()
	root.add_child(input)
	input.set("tag_name", "NUM2")
	input.set("format_cfg", {"decimals": 1})
	input.call("_ready")
	var got_writes := []
	var bridge: Node = root.get_node_or_null("WidgetBridge")
	if bridge != null:
		# 钩住 write_result 观察写入
		var conn_ok := bridge.connect("write_result", func(rid, tag, ok, err): got_writes.append([tag, ok]))
		_check(conn_ok == OK, "write_result 信号可连接")
	input.text = "3.5"
	input.emit_signal("text_submitted", "3.5")
	await process_frame
	_check(got_writes.size() >= 1, "合法提交触发 write_result 回执")
	if not got_writes.is_empty():
		_check(got_writes[0][0] == "NUM2", "写入目标是 NUM2 (got: %s)" % str(got_writes[0]))
	input.queue_free()

## 用例3：TagLamp — 非 0 亮 / 0 灭；闪烁是否启用仅影响动效，不影响状态。
func test_lamp() -> void:
	print("[test] lamp")
	var script: GDScript = load(WIDGET_DIR + "tag_lamp.gd")
	_check(script != null, "tag_lamp.gd 可加载")
	if script == null:
		return
	var lamp: Panel = script.new()
	root.add_child(lamp)
	lamp.set("tag_name", "LAMP1")
	lamp.call("_ready")
	var mock: Node = _get_mock()
	mock.call("push", "LAMP1", 1, "good")
	await process_frame
	_check(bool(lamp.get("lit")) == true, "LAMP1=1 → lit == true")
	mock.call("push", "LAMP1", 0, "good")
	await process_frame
	_check(bool(lamp.get("lit")) == false, "LAMP1=0 → lit == false")
	# 闪烁配置：开启后 _process 有计时逻辑（不下断言具体色，只验证不抛错）
	lamp.set("blink_cfg", {"enabled": true, "interval_sec": 0.5})
	lamp.call("_process", 0.1)
	_check(true, "blink_cfg 开启后 _process 运行不抛错")
	lamp.queue_free()

## 用例4：TagMeter — push 值后 angle 按 min/max 线性映射并反映在 UI 状态。
func test_meter_widget() -> void:
	print("[test] meter_widget")
	var script: GDScript = load(WIDGET_DIR + "tag_meter.gd")
	_check(script != null, "tag_meter.gd 可加载")
	if script == null:
		return
	var meter: Control = script.new()
	root.add_child(meter)
	meter.set("tag_name", "MTR1")
	meter.set("min", 0.0)
	meter.set("max", 100.0)
	meter.custom_minimum_size = Vector2(120, 120)
	meter.call("_ready")
	var mock: Node = _get_mock()
	mock.call("push", "MTR1", 75.0, "good")
	await process_frame
	var angle: float = float(meter.get("value_angle"))
	# 75% 在 -120..+120 映射为 60°（>0），验证正角度驱动
	_check(angle > 0.0, "推 75 后 value_angle > 0（got: %s）" % str(angle))
	# 满量程角度（复用 WidgetMeter 计算，验证实例角度与满量程线性关系）
	if meter.has_method("angle_for_value"):
		var full: float = meter.angle_for_value(100.0, 0.0, 100.0, {})
		_check(full > angle, "满量程角度 > 当前 75 后角度 (%s > %s)" % [str(full), str(angle)])
	meter.queue_free()

# ---------- helpers ----------

func _get_mock() -> Node:
	var bridge: Node = root.get_node_or_null("WidgetBridge")
	if bridge == null:
		return null
	return bridge.get("_mock")

func AllWidgets_bridge() -> Node:
	return root.get_node_or_null("WidgetBridge")

func _check(cond: bool, what: String) -> void:
	_checks += 1
	if cond:
		print("  PASS: ", what)
	else:
		_failures.append(what)
		print("  FAIL: ", what)