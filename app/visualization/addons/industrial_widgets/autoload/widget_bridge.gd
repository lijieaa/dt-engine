extends Node
## WidgetBridge — 统一数据桥（三态检测）。
##
## 三态优先级：
##   1. cpp_runtime — C++ IndustrialRuntime 模块已编译进引擎（自定义引擎 build）
##   2. native_module — WidgetFormat/WidgetTrend 等 C++ 类已通过 ClassDB 注册
##      （Ruling W5：所有 widget C++ 在 modules/industrial_editor/ 编译，无独立 DLL）
##      数据通道仍走 mock，但 C++ 格式/历史/报警核心可供元件直接调用。
##   3. mock — 纯 GDScript 模拟数据源（widget_bridge_mock.gd）
##
## 解析可移植性：本脚本不得直接引用 `IndustrialRuntime` / widget C++ 类的标识符
## （在无这些类的引擎上会解析失败）。检测一律走 ClassDB.class_exists(...)，
## 实例化走 ClassDB.instantiate(...)，绑定 C++ 静态 get_singleton() 经实例动态调用
## （见 _resolve_rt()，对应 Ruling W2）。

signal tag_changed(tag: String, value, quality: String, version: int, ts_ms: int)
signal write_result(rid: String, tag: String, ok: bool, error_text: String)
signal state_changed(new_state: int)
signal backend_detected(has_runtime: bool)

const RUNTIME_CLASS := "IndustrialRuntime"
const MOCK_SCRIPT := "res://addons/industrial_widgets/autoload/widget_bridge_mock.gd"

## 强制使用 mock 数据源（测试可置 true 后重调 _detect_backend()；_ready 后可重入）。
var use_mock: bool = false
## 检测到的桥接模式："" 未检测 / "cpp_runtime" / "native_module" / "mock"
var bridge_mode: String = ""
## 实际后端代理：IndustrialRuntime 实例（复用 Runtime autoload 单例）或 null
var _rt: Object = null
## 已探测到的 widget C++ 类清单（native_module 模式下非空；Ruling W5：类直接编译进引擎）
var native_classes: Array[String] = []
var _mock: Node = null
var _connected: bool = false
var _detected: bool = false

func _ready() -> void:
	_detect_backend()

## 三态检测：
##   1. cpp_runtime — C++ IndustrialRuntime 模块（自定义引擎 build）
##   2. native_module — widget C++ 类已编译进引擎（Ruling W5，类经 ClassDB 注册）
##      数据通道走 mock，C++ 格式/历史/报警核心供元件直接调用
##   3. mock — 纯 GDScript 模拟数据源
## 可在 _ready 之后重入（测试用）。
func _detect_backend() -> void:
	_detected = true
	if not use_mock:
		_rt = _resolve_rt()
		if _rt != null:
			bridge_mode = "cpp_runtime"
			_bind_rt_signals(_rt)
			_connected = true
			backend_detected.emit(true)
			return
		_rt = null
		# 档2：widget C++ 类已编译进引擎（无独立 DLL，直接探测各 Widget* 类）
		if _detect_native_classes():
			bridge_mode = "native_module"
			_setup_mock()  # 数据通道走 mock（C++ 核心类供元件直接调用）
			backend_detected.emit(true)
			return
	# 档3：纯 mock
	use_mock = true
	bridge_mode = "mock"
	_setup_mock()
	backend_detected.emit(false)

## 挂载 mock 数据源（幂等：_mock 已存在则跳过）。
func _setup_mock() -> void:
	if _mock != null:
		return
	if not ResourceLoader.exists(MOCK_SCRIPT):
		push_warning("WidgetBridge: mock 脚本缺失，bridge 空闲")
		return
	_mock = (load(MOCK_SCRIPT) as GDScript).new()
	add_child(_mock)  # 入树后其 _process 每帧自动 tick（模拟数据生成）
	_mock.set("bridge", self)
	_mock.tag_changed.connect(Callable(self, "_on_rt_tag_changed"))

## 探测编译进引擎的 widget C++ 类（Ruling W5：类各自经 ClassDB 注册，无 WidgetNative 门面）。
## 任一存在即认为 native_module 可用；记录全部已注册的 Widget* 类作能力清单。
func _detect_native_classes() -> bool:
	var candidates := [
		"WidgetFormat", "WidgetTrend", "WidgetAlarm",
		"WidgetMeter", "WidgetRecipe", "WidgetMacro",
	]
	native_classes = []
	for cls in candidates:
		if ClassDB.class_exists(cls):
			native_classes.append(cls)
	return not native_classes.is_empty()

## native_module 模式下是否已检测到 C++ widget 类。
func has_native() -> bool:
	if not _detected:
		_detect_backend()
	return bridge_mode == "native_module" and not native_classes.is_empty()

## 解析 C++ IndustrialRuntime 实例：优先复用 Runtime autoload 持有的单例
## （Ruling W2 —— 该类只 GDREGISTER_CLASS，Engine.get_singleton 恒为 null）。
func _resolve_rt() -> Object:
	if not ClassDB.class_exists(RUNTIME_CLASS):
		return null
	var inst := ClassDB.instantiate(RUNTIME_CLASS)
	if inst == null:
		return null
	# 绑定静态 get_singleton() → 复用 Runtime.gd 拥有的实例；无单例时此实例即单例。
	var rt: Object = inst.get_singleton()
	if rt == null:
		rt = inst
	return rt

func _bind_rt_signals(rt: Object) -> void:
	if rt.has_signal("tag_changed"):
		rt.connect("tag_changed", Callable(self, "_on_rt_tag_changed"))
	if rt.has_signal("write_result"):
		rt.connect("write_result", Callable(self, "_on_rt_write_result"))
	if rt.has_signal("connection_state_changed"):
		rt.connect("connection_state_changed", Callable(self, "_on_rt_state"))

func is_singleton_available() -> bool:
	return ClassDB.class_exists(RUNTIME_CLASS) or Engine.has_singleton(RUNTIME_CLASS)

func subscribe(tags: Array) -> bool:
	if tags.is_empty():
		return false
	if not _detected:
		_detect_backend()
	if _mock != null and (use_mock or _rt == null):
		return true
	if _rt != null:
		return bool(_rt.subscribe(tags))
	return false

func unsubscribe(tags: Array) -> bool:
	if tags.is_empty():
		return false
	if not _detected:
		_detect_backend()
	if _mock != null and (use_mock or _rt == null):
		return true
	if _rt != null:
		return bool(_rt.unsubscribe(tags))
	return false

func write_tag(tag: String, value, rid: String = "") -> bool:
	if not _detected:
		_detect_backend()
	if _mock != null and (use_mock or _rt == null):
		# mock 收到写请求即产出 write_result 回执（协议对齐）
		if _mock.has_method("submit_write"):
			return bool(_mock.submit_write(tag, value, rid))
		return true
	if _rt != null:
		return bool(_rt.write_tag(tag, value, rid))
	return false

func connect_backend(url: String) -> void:
	if _rt != null:
		_rt.connect_runtime(url)
		_connected = true

# ---------- 后端 → 桥信号中继 ----------

func _on_rt_tag_changed(tag, value, quality, version, ts_ms) -> void:
	tag_changed.emit(String(tag), value, String(quality), int(version), int(ts_ms))

func _on_rt_write_result(rid, tag, ok, error_text, verified = null) -> void:
	write_result.emit(String(rid), String(tag), bool(ok), String(error_text))

func _on_rt_state(new_state) -> void:
	state_changed.emit(int(new_state))

## 供 mock 源调用：收到写请求后产出 write_result 回执。
func _on_mock_write_request(tag, value, rid) -> void:
	write_result.emit(String(rid), String(tag), true, String())