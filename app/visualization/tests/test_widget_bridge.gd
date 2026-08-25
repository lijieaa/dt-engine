extends SceneTree
## WidgetBridge 单测（Task 1 + Task 7 三态检测）。
## 通过 res:// 路径动态加载（不依赖编辑器缓存/类标识符），headless --script 运行。
## 用例对应 plan Task 1 Step 2 + Task 7：
##   1. test_subscribe_calls_runtime_when_available
##   2. test_write_tag_emits_write_result
##   3. test_mock_mode_emits_tag_changed
##   4. test_three_mode_detection (T7：cpp_runtime / native_module / mock)

const BRIDGE_SCRIPT := "res://addons/industrial_widgets/autoload/widget_bridge.gd"

var _failures: Array[String] = []

func _initialize() -> void:
	# 等待一帧让 autoload 完成 _ready()（Runtime 连接 C++ WS；若是测试直接 new 的脚本 _ready 已同步跑完）
	await process_frame
	var code := await _run_all()
	quit(code)

func _run_all() -> int:
	print("=== test_widget_bridge ===")
	await test_subscribe_calls_runtime_when_available()
	await test_write_tag_emits_write_result()
	await test_mock_mode_emits_tag_changed()
	await test_three_mode_detection()
	if _failures.is_empty():
		print("RESULT: PASS")
		return 0
	print("RESULT: FAIL — %d failure(s)" % _failures.size())
	for f in _failures:
		print("  FAIL: ", f)
	return 1

func _check(cond: bool, what: String) -> void:
	if cond:
		print("  PASS: ", what)
	else:
		_failures.append(what)
		print("  FAIL: ", what)

## 直接实例化一个 WidgetBridge（不经 autoload），保证确定性。失败时返回 null。
func _make_bridge() -> Node:
	if not ResourceLoader.exists(BRIDGE_SCRIPT):
		_failures.append("widget_bridge.gd 不存在")
		return null
	var s: GDScript = load(BRIDGE_SCRIPT)
	if s == null:
		_failures.append("widget_bridge.gd 加载失败")
		return null
	var bridge: Node = s.new()
	root.add_child(bridge)
	await process_frame
	return bridge

func _make_mock(bridge: Node) -> Node:
	## 挂载 mock 源并按公开语义切换 bridge 到 mock 模式：
	##   use_mock = true → _detect_backend()（controller 批准的设计）。
	if not ResourceLoader.exists("res://addons/industrial_widgets/autoload/widget_bridge_mock.gd"):
		_failures.append("widget_bridge_mock.gd 不存在")
		return null
	var ms: GDScript = load("res://addons/industrial_widgets/autoload/widget_bridge_mock.gd")
	if ms == null:
		_failures.append("widget_bridge_mock.gd 加载失败")
		return null
	bridge.set("use_mock", true)
	bridge.call("_detect_backend")
	var mock: Node = bridge.get("_mock")
	if mock == null:
		return null
	bridge.set("_rt", null)
	# 不在此 push：信号是同步的，须等调用方 connect 后再推送（见各用例）。
	await process_frame
	return mock

## 用例1：当 C++/进程内 Runtime 存在时，subscribe() 转发到其后端（有真实后端则返回
## 《连接是否就绪》的结果；无后端时返回 false 但 bridge 已可加载调用）。
func test_subscribe_calls_runtime_when_available() -> void:
	print("[test] subscribe_calls_runtime_when_available")
	# 语义断言（不依赖节点实例）：脚本可加载、subscribe() 接口存在
	var s: GDScript = load(BRIDGE_SCRIPT)
	_check(ResourceLoader.exists(BRIDGE_SCRIPT), "bridge 脚本可加载")
	_check(s != null, "bridge GDScript 非空")
	var bridge: Node = await _make_bridge()
	if bridge == null:
		return
	_check(bridge.has_method("subscribe"), "bridge.subscribe 接口存在")
	var has_rt: bool = ClassDB.class_exists("IndustrialRuntime") or Engine.has_singleton("IndustrialRuntime")
	var called: bool = bridge.call("subscribe", ["T1"])
	if has_rt:
		# 有真实后端：返回「连接就绪与否」；API 路径已走到
		_check(typeof(called) == TYPE_BOOL, "subscribe 返回 bool")
	else:
		_check(not called, "无后端时 subscribe 返回 false")
	bridge.free()

## 用例2：write_tag 触发 write_result 回执（bridge 内桥接真实后端信号；mock 源直发）。
func test_write_tag_emits_write_result() -> void:
	print("[test] write_tag_emits_write_result")
	var bridge: Node = await _make_bridge()
	if bridge == null:
		return
	_make_mock(bridge)
	var got := []
	bridge.connect("write_result", func(rid, tag, ok, err): got.append([rid, tag, ok, err]))
	bridge.call("write_tag", "T1", 5, "rid1")
	await process_frame
	_check(got.size() == 1, "write_tag 触发 write_result 回调")
	if not got.is_empty():
		var r: Array = got[0]
		_check(r[0] == "rid1", "write_result 携带 rid")
		_check(r[1] == "T1", "write_result 携带 tag")
	bridge.free()

## 用例3：use_mock=true 时 tag_changed 收到 mock 推送。
func test_mock_mode_emits_tag_changed() -> void:
	print("[test] mock_mode_emits_tag_changed")
	var bridge: Node = await _make_bridge()
	if bridge == null:
		return
	var mock: Node = await _make_mock(bridge)
	if mock == null:
		return
	var got := []
	bridge.connect("tag_changed", func(tag, v, q, ver, ts): got.append([tag, v, q]))
	# connect 之后再推送（信号同步，先连后推）
	mock.call("push", "T1", 42, "good")
	mock.call("push", "T2", true, "good")
	await process_frame
	_check(got.size() >= 1, "mock 推送后收到 tag_changed")
	var found_t1 := false
	var found_t2 := false
	for ev in got:
		if ev[0] == "T1" and ev[1] == 42 and ev[2] == "good":
			found_t1 = true
		if ev[0] == "T2" and ev[1] == true:
			found_t2 = true
	_check(found_t1, "收到 T1=42/good")
	_check(found_t2, "收到 T2=true")
	bridge.free()

## 用例4（T7）：三态检测 — cpp_runtime / native_module / mock。
## 本引擎为自定义 build（IndustrialRuntime + widget C++ 类编译在内），因此期望：
##   - 有 IndustrialRuntime → cpp_runtime
##   - 否则有 Widget* 类 → native_module
##   - 都没有 → mock（用 use_mock 强制验证 mock 档逻辑独立可用）
func test_three_mode_detection() -> void:
	print("[test] three_mode_detection")
	var bridge: Node = await _make_bridge()
	if bridge == null:
		return
	var has_rt: bool = ClassDB.class_exists("IndustrialRuntime")
	var has_widget_native: bool = (
		ClassDB.class_exists("WidgetFormat")
		or ClassDB.class_exists("WidgetTrend")
		or ClassDB.class_exists("WidgetAlarm")
	)
	var mode: String = bridge.get("bridge_mode")
	if has_rt:
		_check(mode == "cpp_runtime", "bridge_mode == cpp_runtime (IndustrialRuntime 存在)")
	elif has_widget_native:
		_check(mode == "native_module", "bridge_mode == native_module (widget C++ 类存在)")
	else:
		_check(mode == "mock", "bridge_mode == mock (无 C++ 后端)")
	# native_classes 清单与 has_native() 一致性
	if mode == "native_module":
		var nc: Array = bridge.get("native_classes")
		_check(not nc.is_empty(), "native_classes 非空")
		_check(nc.has("WidgetFormat"), "native_classes 包含 WidgetFormat")
		_check(bridge.call("has_native") == true, "has_native() == true")
	else:
		_check(bridge.call("has_native") == false, "has_native() == false（非 native_module 档）")
	# 强制 mock 档：use_mock = true 后重入检测，模式必须切到 mock
	bridge.set("use_mock", true)
	bridge.call("_detect_backend")
	_check(bridge.get("bridge_mode") == "mock", "use_mock=true 后 bridge_mode == mock")
	bridge.free()