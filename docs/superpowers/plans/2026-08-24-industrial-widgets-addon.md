# Industrial Widgets Addon Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建一个可移植的 Godot 插件 `addons/industrial_widgets/`，对齐实现 EBPro 画面元件（12 个控件），通过 WebSocket 绑定 Go runtime 的 PLC 数据，并用 GDExtension C++ 承载核心逻辑实现防反编译。

**Architecture:** 标准 Godot 插件壳（`plugin.cfg` + EditorPlugin 元件面板）+ `WidgetBridge` autoload 双轨数据桥（优先复用 C++ `IndustrialRuntime` singleton，标准 Godot 工程无此单例时用 GDExtension DLL 兜底）。核心算法（格式化/报警求值/环形缓冲/配方/宏时序/防篡改辅助）全部在独立 GDExtension `industrial_widgets_native` 中，GDScript 只做 UI 编排。

**Tech Stack:** Godot 4.3+ GDScript、GDExtension C++ (godot-cpp)、WebSocket JSON (协议 v1: subscribe/tag_update/write/write_result)、SConstruct/CMake 构建。

**Spec:** `docs/superpowers/specs/2026-08-24-industrial-widgets-addon-design.md`

## Global Constraints

- 插件主目录：`app/visualization/addons/industrial_widgets/`（需先创建 `addons/`）
- GDExtension 根目录：`app/visualization/addons/industrial_widgets/native/`
- 数据层接口统一（所有元件只依赖）：`tag_changed(tag: String, value, quality: String, version: int, ts_ms: int)` + `write_tag(tag: String, value) -> bool` + `subscribe(tags: Array)`
- WS 协议 v1 消息（Go 侧 `internal/ws/protocol.go`）：`subscribe`/`tag_update`/`write`/`write_result`/`unsubscribe`，JSON envelope `{version, type, request_id, tags, tag, value, values, ok, error}`
- 质量码：`good` / `uncertain` / `bad` / `stale`
- 元件清单 12 个（对应 spec §5）：TagLabel、TagNumDisplay、TagNumInput、TagLamp、TagSwitch、TagMeter、TagTrend、TagAlarmList、TagRecipe、TagMacroButton、TagBar、TagGauge
- **防反编译**：所有"有价值算法"必须在 C++ GDExtension，GDScript 只做 UI 壳（spec §3/§5.1）
- 授权/写权限/身份/设备授权全部在 Go Runtime（spec §2 D5），客户端不做最终判定
- v1 加固档位：DLL strip 去符号 + 核心字符串不落明文 + `.pck` 加密属于宿主工程发布配置（不在插件内）

---

### Task 1: 插件骨架 + WidgetBridge 数据桥（模式A 双轨）

**Files:**
- Create: `app/visualization/addons/industrial_widgets/plugin.cfg`
- Create: `app/visualization/addons/industrial_widgets/autoload/widget_bridge.gd`
- Create: `app/visualization/addons/industrial_widgets/autoload/widget_bridge_mock.gd`
- Modify: `app/visualization/project.godot`（注册 autoload `WidgetBridge`）
- Test: `app/visualization/tests/test_widget_bridge.gd`（GDScript 单测场景）

**Interfaces:**
- Produces:
  - `WidgetBridge` (autoload): `signal tag_changed(tag, value, quality, version, ts_ms)`、`signal write_result(rid, tag, ok, error_text)`、`signal state_changed(new_state)`、`func subscribe(tags: Array) -> bool`、`func unsubscribe(tags: Array) -> bool`、`func write_tag(tag: String, value, rid: String = "") -> bool`、`func connect_backend(url: String)`、`var use_mock: bool`
  - `WidgetBridgeMock`: 内置模拟数据生成器（`func _process(delta)` 定时生成正弦/方波/随机值并 emit `tag_changed`）
  - `static func WidgetBridge.is_singleton_available() -> bool`：检测 `Engine.has_singleton("IndustrialRuntime")` 或 `ClassDB.class_exists("IndustrialRuntime")`

- [ ] **Step 1: 创建插件骨架目录和 plugin.cfg**

```bash
mkdir -p app/visualization/addons/industrial_widgets/{autoload,widgets,editor,native}
```

```ini
[plugin]
name="Industrial Widgets"
description="EBPro-aligned industrial UI widgets with GDExtension core"
author="dt-engine"
version="0.1.0"
script="editor/editor_plugin.gd"
```

- [ ] **Step 2: 写失败测试 test_widget_bridge.gd**

写一个 GDScript 单测场景（`extends SceneTree` 或使用 `gdUnit4` 若项目已有；默认用 `SceneTree` 脚本）：
- `test_subscribe_calls_runtime_when_available`：当 mock `IndustrialRuntime` 存在时，`WidgetBridge.subscribe(["T1"])` 调用其 `subscribe`
- `test_write_tag_emits_write_result`：`WidgetBridge.write_tag("T1", 5, "rid1")` 触发回调
- `test_mock_mode_emits_tag_changed`：`use_mock = true` 时连接到 `tag_changed`，跑若干帧后收到事件

- [ ] **Step 3: 运行测试验证失败**

```bash
cd "D:/dt-engine" && ./bin/godot.windows.editor.x86_64.exe --headless --path app/visualization --script res://tests/test_widget_bridge.gd
```
Expected: FAIL（`WidgetBridge` 不存在，无法加载）

- [ ] **Step 4: 实现 WidgetBridge（模式A 双轨）**

`autoload/widget_bridge.gd`：

```gdscript
extends Node
## WidgetBridge — 统一数据桥。
## 模式A：优先复用 C++ IndustrialRuntime singleton（自定义引擎）。
## 兜底：标准 Godot 用 GDExtension DLL 或 mock（见 widget_bridge_native.gd，Task 7 接入）。

signal tag_changed(tag: String, value, quality: String, version: int, ts_ms: int)
signal write_result(rid: String, tag: String, ok: bool, error_text: String)
signal state_changed(new_state: int)
signal backend_detected(has_runtime: bool)

var use_mock: bool = false
var _rt: Object = null          # IndustrialRuntime singleton 或 null
var _mock: Node = null
var _connected: bool = false

func _ready() -> void:
	_detect_backend()

func _detect_backend() -> void:
	if ClassDB.class_exists("IndustrialRuntime"):
		_rt = Engine.get_singleton("IndustrialRuntime")
		if _rt == null:
			_rt = (IndustrialRuntime as Variant).new()  # fallback to fresh instance
		if _rt:
			_rt.connect("tag_changed", Callable(self, "_on_rt_tag_changed"))
			_rt.connect("write_result", Callable(self, "_on_rt_write_result"))
			_rt.connect("connection_state_changed", Callable(self, "_on_rt_state"))
			backend_detected.emit(true)
			return
	use_mock = true
	_mock = preload("res://addons/industrial_widgets/autoload/widget_bridge_mock.gd").new()
	add_child(_mock)
	_mock.tag_changed.connect(_on_rt_tag_changed)
	backend_detected.emit(false)

func subscribe(tags: Array) -> bool:
	if not _connected: return false
	if _rt: return Boolean(_rt.subscribe(tags))
	return false

func unsubscribe(tags: Array) -> bool:
	if _rt: return Boolean(_rt.unsubscribe(tags))
	return false

func write_tag(tag: String, value, rid: String = "") -> bool:
	if _rt: return Boolean(_rt.write_tag(tag, value, rid))
	return false

func connect_backend(url: String) -> void:
	if _rt:
		_rt.connect_runtime(url)
		_connected = true
```

`autoload/widget_bridge_mock.gd`（模拟数据源，`_process` 中 emit `tag_changed`，支持 `add_mock_tag(name, pattern, min, max)`）。

**注意**：`WidgetBridgeMock` 必须让测试能手动驱动 `emit tag_changed`（`func push(tag, v, q)` 公开方法），便于单测不依赖真实 timer。

- [ ] **Step 5: 运行测试验证通过**

再次运行 Step 3 命令。Expected: PASS（mock 模式下收到 `tag_changed`；`backend_detected` 在标准引擎上 emit false 走 mock）

- [ ] **Step 6: 注册 autoload**

修改 `app/visualization/project.godot` 的 `[autoload]` 段：
```ini
[autoload]
Runtime="*res://scripts/Runtime.gd"
WidgetBridge="*res://addons/industrial_widgets/autoload/widget_bridge.gd"
```

- [ ] **Step 7: Commit**

```bash
git add app/visualization/addons/industrial_widgets app/visualization/project.godot app/visualization/tests
git commit -m "feat(widgets): plugin skeleton + WidgetBridge dual-mode data bridge"
```

---

### Task 2: 移植已有 4 元件（TagLabel / TagBar / TagGauge / TagSwitch）为插件元件

**Files:**
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_label.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_bar.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_gauge.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_switch.gd` + `.tscn`
- Test: `app/visualization/tests/test_widgets_simple.gd`

**Interfaces:**
- Consumes: `WidgetBridge`（Task 1）`tag_changed` / `write_tag` / `subscribe`
- Produces: `TagLabel`(extends Label)、`TagBar`(extends HSlider)、`TagGauge`(extends ProgressBar)、`TagSwitch`(extends CheckBox) —— 各自 `@export var tag_name: String`，`_ready()` 里 `WidgetBridge.subscribe([tag_name])` + 连接 `tag_changed`，按质量码刷新颜色/值

- [ ] **Step 1: 从现有实现复制到 addons 目录**

现有源（项目根 `app/visualization/scripts/ui/`）：
- `TagLabel.gd`、`TagBar.gd`、`TagGauge.gd`、`TagSwitch.gd`

复制到 `addons/industrial_widgets/widgets/`，并把内部 `Runtime.subscribe` / `Runtime.connect("tag_changed")` / `Runtime.write_tag` 全部替换为 `WidgetBridge.subscribe` / `WidgetBridge.connect("tag_changed")` / `WidgetBridge.write_tag`。

- [ ] **Step 2: 写失败测试**

`test_widgets_simple.gd`（SceneTree 脚本）验证：
- `TagLabel.set_tag_name("T1")` 后 `_ready()` 调用 `WidgetBridge.subscribe`（可断言 `WidgetBridge` 收到订阅列表）
- mock 推 `tag_changed("T1", 42, "good", 1, 123)` 时 `TagLabel.text` 变为 "42"
- `TagSwitch.toggled` 触发 `WidgetBridge.write_tag("T1", true)`

- [ ] **Step 3: 运行测试验证失败**

同上 headless 命令。Expected: FAIL（元件未加载或仍引用旧 Runtime）

- [ ] **Step 4: 实现 4 个元件脚本（替换 Runtime → WidgetBridge）**

示例（tag_label.gd 核心逻辑，其余类似）：

```gdscript
extends Label
class_name TagLabel
@export var tag_name: String = ""
@export var base_color := Color(1, 1, 1, 1)

func _ready() -> void:
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)

func _on_tag_changed(tag: String, v, quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name: return
	text = str(v)
```

- [ ] **Step 5: 运行测试验证通过**

- [ ] **Step 6: Commit**

```bash
git add app/visualization/addons/industrial_widgets/widgets app/visualization/tests
git commit -m "feat(widgets): port TagLabel/Bar/Gauge/Switch to PluginBridge"
```

---

### Task 3: GDExtension 骨架 + WidgetNative 注册（C++ 核心）

**Files:**
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/SConstruct`
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/src/register_types.cpp`
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/src/register_types.h`
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/src/industrial_widgets_native.h/.cpp`（入口类）
- Create: `app/visualization/addons/industrial_widgets/industrial_widgets.gdextension`
- Test: `app/visualization/tests/test_native_load.gd`

**Interfaces:**
- Consumes: godot-cpp（构建期拉取，见 Step 1）
- Produces:
  - `WidgetNative` 类（GDExtension 注册）：`static func create() -> WidgetNative`、`func get_version() -> String`、内部暴露子核心 `WidgetFormat` / `WidgetAlarm` / `WidgetTrend` 等（由后续 Task 挂方法）
  - `.gdextension` 文件：`[configuration] entry_symbol="industrial_widgets_native_entry"`；`[libraries] windows.debug.x86_64 = "res://addons/industrial_widgets/native/bin/libindustrial_widgets_native.dll"`

- [ ] **Step 1: 拉取 godot-cpp 并配置构建**

```bash
cd app/visualization/addons/industrial_widgets/native
git clone --depth 1 --branch 4.3 https://github.com/godotengine/godot-cpp.git gdextension/godot-cpp
```

SConstruct（参照 `tests/compatibility_test/SConstruct` 架构，用 C++ 而非 C）：

```python
#!/usr/bin/env python
import os
env = Environment()
env.Append(CPPPATH=["gdextension/src", "gdextension/godot-cpp/godot-headers"])
env.Append(CXXFLAGS=["-O2", "-fPIC", "-std=c++17"])
# Windows 用 MSVC；这里示例用跨平台 SCons（用 godot-cpp 官方构建脚本更佳）
```

**推荐**：不用手写 SConstruct，而是用 godot-cpp 自带构建方式：
```bash
cd gdextension/godot-cpp && scons platform=windows target=template_release
```
然后把我们的 `src/*.cpp` 与 `libgodot-cpp.windows.*.lib` 一起链接成 `industrial_widgets_native.dll`。具体链接命令见 Godot GDExtension 官方模板 `godot-cpp/demo` 的 SConstruct 复制修改。

- [ ] **Step 2: 写失败测试 test_native_load.gd**

```gdscript
# headless 单测：验证 .gdextension 可加载且 WidgetNative 类存在
func _initialize() -> void:  # 或 _run
	var has: bool = ClassDB.class_exists("WidgetNative")
	assert(has, "WidgetNative class should exist after gdextension load")
	var w = (WidgetNative as Variant).new()
	assert(w.get_version() == "0.1.0")
```

- [ ] **Step 3: 运行测试验证失败**

```bash
cd "D:/dt-engine" && ./bin/godot.windows.editor.x86_64.exe --headless --path app/visualization --script res://tests/test_native_load.gd
```
Expected: FAIL（`WidgetNative` 未知）

- [ ] **Step 4: 实现 GDExtension 注册（最小可用）**

`register_types.cpp`（核心模板）：
```cpp
#include "register_types.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include "industrial_widgets_native.h"

using namespace godot;

void initialize_industrial_widgets_native_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
	ClassDB::register_class<WidgetNative>();
}
void uninitialize_industrial_widgets_native_module(ModuleInitializationLevel p_level) {}

extern "C" {
GDExtensionBool GDE_EXPORT industrial_widgets_native_entry(
	GDExtensionInterfaceGetProcAddress p_get_proc_address,
	GDExtensionClassLibraryPtr p_library,
	GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init(p_get_proc_address, p_library, r_initialization);
	init.register_initializer(initialize_industrial_widgets_native_module);
	init.register_terminator(uninitialize_industrial_widgets_native_module);
	return init.init();
}
}
```

`industrial_widgets_native.h` 最小版：`class WidgetNative : public RefCounted { GDCLASS(WidgetNative, RefCounted); static void _bind_methods(); String get_version() const; ... }`，`get_version()` 返回 `"0.1.0"`。**字符串用 `String::utf8` 或拼接避免明文问题由 Task 9 处理**。

`industrial_widgets.gdextension`：
```ini
[configuration]
entry_symbol = "industrial_widgets_native_entry"
compatibility_minimum = 4.3

[libraries]
windows.debug.x86_64 = "res://addons/industrial_widgets/native/bin/libindustrial_widgets_native.dll"
windows.release.x86_64 = "res://addons/industrial_widgets/native/bin/libindustrial_widgets_native.dll"
```

- [ ] **Step 5: 构建 DLL**

```bash
cd app/visualization/addons/industrial_widgets/native/gdextension
scons platform=windows target=template_release -j8
# 产出 libindustrial_widgets_native.dll，复制到 native/bin/
```

- [ ] **Step 6: 运行测试验证通过**

- [ ] **Step 7: Commit**

```bash
git add app/visualization/addons/industrial_widgets/native app/visualization/addons/industrial_widgets/industrial_widgets.gdextension app/visualization/tests
git commit -m "feat(native): GDExtension skeleton with WidgetNative registration"
```

---

### Task 4: C++ WidgetFormat（数字格式化核心）

**Files:**
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/src/widget_format.h/.cpp`
- Modify: `app/visualization/addons/industrial_widgets/native/gdextension/src/register_types.cpp`（注册 WidgetFormat）
- Test: `app/visualization/tests/test_widget_format.gd`（或 C++ 单测，本计划采用 GDScript 调 C++ 验证）

**Interfaces:**
- Produces（GDScript 可见静态方法，经 `WidgetNative` 暴露）：
  - `WidgetFormat.format_value(raw: Variant, format_cfg: Dictionary) -> String`
    - `format_cfg` 键：`decimals:int`、`thousands:bool`、`prefix:String`、`suffix:String`、`bcd:bool`、`signed:bool`
    - BCD 解码：`raw` 按 BCD 解析（每字节高 4 位十位、低 4 位个位）
  - `WidgetFormat.parse_input(text: String, format_cfg: Dictionary) -> Dictionary` → `{ok, value, error}`
- 语义：数字格式化（EBPro Numeric Display 对齐）；核心算法 C++ 实现

- [ ] **Step 1: 写失败测试**

```gdscript
# test_widget_format.gd（headless）
var wf = (WidgetNative as Variant).new().get_format()
assert(wf.format_value(1234.567, {"decimals":2,"thousands":true}) == "1,234.57")
assert(wf.format_value(1234.567, {"decimals":2,"thousands":false}) == "1234.57")
assert(wf.format_value(0x12, {"bcd":true}) == "12")  # BCD 0x12 → "12"
assert(wf.parse_input("abc", {"decimals":0}).ok == false)
assert(wf.parse_input("-3.5", {"decimals":1}).ok == true)
```

- [ ] **Step 2: 运行测试验证失败**

Expected: FAIL（`get_format` 不存在，编译报错或运行异常）

- [ ] **Step 3: 实现 WidgetFormat**

`widget_format.h`：
```cpp
class WidgetFormat : public RefCounted {
	GDCLASS(WidgetFormat, RefCounted);
public:
	String format_value(const Variant &raw, const Dictionary &cfg);
	Dictionary parse_input(const String &text, const Dictionary &cfg);
protected:
	static void _bind_methods();
private:
	static Variant decode_bcd(const Variant &raw);
	static String add_thousands(const String &num);
	static Variant to_decimal(const String &text, bool &ok);
};
```
`widget_format.cpp`：实现千分位、BCD 解码、parse_input 校验（空串/非数字→ error）。

- [ ] **Step 4: 注册 WidgetFormat 到 register_types.cpp**

```cpp
#include "widget_format.h"
...
ClassDB::register_class<WidgetFormat>();
```
并给 `WidgetNative` 增加 `Ref<WidgetFormat> get_format()`。

- [ ] **Step 5: 构建 DLL + 运行测试**

```bash
cd app/visualization/addons/industrial_widgets/native/gdextension && scons platform=windows target=template_release -j8
cd "D:/dt-engine" && ./bin/godot.windows.editor.x86_64.exe --headless --path app/visualization --script res://tests/test_widget_format.gd
```
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add app/visualization/addons/industrial_widgets/native app/visualization/tests
git commit -m "feat(native): WidgetFormat numeric formatting core (thousands/BCD/decimal)"
```

---

### Task 5: C++ WidgetTrend（环形缓冲历史曲线核心）

**Files:**
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/src/widget_trend.h/.cpp`
- Modify: `app/visualization/addons/industrial_widgets/native/gdextension/src/register_types.cpp`（注册 WidgetTrend）
- Test: `app/visualization/tests/test_widget_trend.gd`

**Interfaces:**
- Produces:
  - `WidgetTrend`：`func push_sample(ts_ms: int, value: float) -> void`、`func query_window(from_ms: int, to_ms: int) -> Array`（返回 `[[ts, value], ...]`）、`func resize(capacity: int) -> void`、`var sample_count: int`、`var min_value/max_value: float`（自动统计窗口极值）、`func clear() -> void`
  - 内部环形缓冲：`std::vector<std::pair<int64_t,double>>` + `head` 索引；容量默认 4096，超容覆盖最旧

- [ ] **Step 1: 写失败测试**

```gdscript
var wt = (WidgetNative as Variant).new().get_trend()
wt.resize(10)
for i in range(5): wt.push_sample(i * 100, float(i))
assert(wt.sample_count == 5)
var w = wt.query_window(100, 400)
assert(w.size() == 4)     # ts 100..400
assert(wt.min_value == 0.0 and wt.max_value == 4.0)
wt.push_sample(...)  # 超过10后最旧被丢弃
```

- [ ] **Step 2: 运行测试验证失败**

- [ ] **Step 3: 实现 WidgetTrend**

环形缓冲：`push_sample` 写入 `(ts, value)` 到 `(head++) % capacity`；`query_window` 线性扫描返回窗口内点（升序）；`min/max` 遍历窗口。

- [ ] **Step 4: 注册 + 构建 + 测试**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(native): WidgetTrend ring-buffer history core"
```

---

### Task 6: C++ WidgetAlarm（报警条件求值 + 确认状态机核心）

**Files:**
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/src/widget_alarm.h/.cpp`
- Modify: `app/visualization/addons/industrial_widgets/native/gdextension/src/register_types.cpp`
- Test: `app/visualization/tests/test_widget_alarm.gd`

**Interfaces:**
- Produces:
  - `WidgetAlarm`：`func add_rule(rule_id: String, config: Dictionary) -> void`（config 键：`comparator:"gt|lt|ge|le|eq|ne"`、`threshold:float`、`debounce_ms:int`）
  - `func update(tag: String, value: Variant, ts_ms: int) -> Dictionary`（返回 `{active:bool, triggered:bool, rule_id, value, ts}`）
  - `func acknowledge(rule_id: String) -> void`（确认状态机：active+acknowledged）
  - `func get_active_rules() -> Array`
  - **去抖**：状态改变后须持续 `debounce_ms` 才翻转 active（防抖）
- 语义：EBPro 报警显示对齐

- [ ] **Step 1: 写失败测试**

```gdscript
var wa = (WidgetNative as Variant).new().get_alarm()
wa.add_rule("HI", {"comparator":"gt", "threshold":100.0, "debounce_ms":0})
assert(wa.update("T", 99.0, 0).active == false)
assert(wa.update("T", 101.0, 1).active == true)
wa.acknowledge("HI")
assert(wa.get_active_rules()[0].acknowledged == true)
# 去抖：debounce 3
var wa2 = ...; wa2.add_rule("HI", {"comparator":"gt", "threshold":0.0, "debounce_ms":3000})
wa2.update("T", 5.0, 1000)  # 触发但未达去抖
assert(wa2.update("T", 6.0, 1000).active == false)
wa2.update("T", 7.0, 5000)  # 超过去抖窗
assert(wa2.update("T", 7.0, 5000).active == true)
```

- [ ] **Step 2: 运行测试验证失败**

- [ ] **Step 3: 实现 WidgetAlarm**

状态机：每规则记录 `last_value / triggered_since_ms / active / acknowledged`；`active` 翻转算去抖，返回 `triggered`（边沿）供 UI 闪烁。

- [ ] **Step 4: 注册 + 构建 + 测试**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(native): WidgetAlarm condition eval + debounce + ack state machine"
```

---

### Task 7: WidgetBridge 接入 WidgetNative（C++ 兜底 + mock 分离）

**Files:**
- Modify: `app/visualization/addons/industrial_widgets/autoload/widget_bridge.gd`

**Interfaces:**
- Consumes: `WidgetNative`（Task 3）
- Produces: `WidgetBridge` 增加 `var native: Object = null`（WidgetNative 实例相）、`func has_native() -> bool`、`var bridge_mode: String`（`"cpp_runtime"` / `"native_dll"` / `"mock"`）

- [ ] **Step 1: 写失败测试**——`test_widget_bridge.gd` 增补：
  - 标准引擎（无 IndustrialRuntime 类）时 `WidgetBridge.bridge_mode == "native_dll"` 且 `has_native() == true`（依赖 Task 3 已把 DLL 构建好）
  - `WidgetBridge.subscribe` 在 native 模式下走 C++（可断言不抛错）

- [ ] **Step 2: 运行测试验证失败**

- [ ] **Step 3: 实现**

`_detect_backend()` 改为三态：
1. `ClassDB.class_exists("IndustrialRuntime")` → `bridge_mode = "cpp_runtime"`，复用现有逻辑
2. 否则 `ClassDB.class_exists("WidgetNative")` → `bridge_mode = "native_dll"`，`native = WidgetNative.new()`，用 C++ WebSocket（Task 8 补充真实连接）
3. 都没有 → `mock`，挂 Mock 源

- [ ] **Step 4: 运行测试验证通过**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(widgets): bridge three-mode detection (cpp_runtime/native_dll/mock)"
```

---

### Task 8: 新元件上层 UI（TagNumDisplay / TagNumInput / TagLamp / TagMeter）

**Files:**
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_num_display.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_num_input.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_lamp.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_meter.gd` + `.tscn`
- Test: `app/visualization/tests/test_widgets_complex.gd`

**Interfaces:**
- Consumes: `WidgetBridge`（Task 1）、`WidgetNative.get_format()`（Task 4）
- Produces:
  - `TagNumDisplay`(extends Label)：`@export var format_cfg: Dictionary`，`_on_tag_changed` 调 `WidgetBridge.native.get_format().format_value(v, format_cfg)` 刷新 text
  - `TagNumInput`(extends LineEdit)：`@export var format_cfg`、`text_submitted` → `WidgetBridge.native.get_format().parse_input(...)` 校验 → `WidgetBridge.write_tag(tag, value)`
  - `TagLamp`(extends Panel)：`@export var on_color/off_color/blink_cfg`；值非 0 亮 on_color + 可选闪烁（blink 用 `_process` 计时，但**状态机在 C++ Task 6 扩展或本地简单实现**——核心状态判断放 C++ `WidgetLamp` 若需；本计划把闪烁时序放 GDScript UI 壳，状态真值判断仍简单）
  - `TagMeter`(extends Control)：`@export var min/max/red_zone`；`_draw()` 自绘圆弧+指针；**刻度/指针角度计算调 C++** `WidgetMeter::angle_for_value`（并入 Task 4 WidgetFormat 或独立小类，见 Task 8 Step 4）

- [ ] **Step 1: 写失败测试**——验证 4 个新元件的 `tag_changed` 驱动行为（mock 推值→text/颜色/角度变化）

- [ ] **Step 2: 运行测试验证失败**

- [ ] **Step 3: 实现 4 个元件脚本 + 场景**

（各元件遵循统一模式：`_ready()` 订阅 + 连接 `WidgetBridge.tag_changed`；`_on_tag_changed` 调 C++ 核心刷新 UI）

- [ ] **Step 4: C++ 补充 WidgetMeter::angle_for_value（若 Task 4 未含）**

在原生侧加 `WidgetNative::get_meter()` → `Ref<WidgetMeter>`，`float angle_for_value(float v, float min_v, float max_v)`，供 TagMeter._draw 调用（指针角度 = 值的线性映射）。

- [ ] **Step 5: 构建 DLL + 运行测试**

- [ ] **Step 6: Commit**

```bash
git commit -m "feat(widgets): TagNumDisplay/Input/Lamp/Meter with native formatting+drawing core"
```

---

### Task 9: C++ WidgetRecipe + WidgetMacro（配方/宏时序核心）+ 字符串防明文

**Files:**
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/src/widget_recipe.h/.cpp`
- Create: `app/visualization/addons/industrial_widgets/native/gdextension/src/widget_macro.h/.cpp`
- Modify: `app/visualization/addons/industrial_widgets/native/gdextension/src/register_types.cpp`
- Test: `app/visualization/tests/test_widget_recipe_macro.gd`

**Interfaces:**
- Produces:
  - `WidgetRecipe`：`func parse(recipe_json: String) -> bool`、`func list_entries() -> Array`、`func build_write_sequence(entry_name: String) -> Array of Dictionary {tag, value, order}`、`func switch_to(entry_name: String, immediate: bool) -> Array`（写回序列）
  - `WidgetMacro`：`func parse(macro_json: String) -> bool`、`func execute(async: bool) -> void`、`signal step_done(idx: int, tag: String, value)`、`signal finished(ok: bool)`；`func set_delay_ms(step_idx: int, delay: int) -> void`；内部按 `[{tag, value, delay_ms}]` 时序执行，`async` 用 `SceneTreeTimer` 逐条写
  - **字符串防明文**：错误消息/密钥等不直接 `"..."`——用 `String::utf8` + 运行时 XOR 或拼接（见 Step 3 模式）

- [ ] **Step 1: 写失败测试**

```gdscript
var wr = (WidgetNative as Variant).new().get_recipe()
assert(wr.parse("{\"entries\":[{\"name\":\"R1\",\"writes\":[{\"tag\":\"A\",\"value\":1},{\"tag\":\"B\",\"value\":2}]}]}"))
assert(wr.list_entries().size() == 1)
var seq = wr.build_write_sequence("R1")
assert(seq.size() == 2 and seq[0].tag == "A")
var wm = ...; assert(wm.parse("{\"steps\":[{\"tag\":\"A\",\"value\":5,\"delay_ms\":100}]}"))
wm.execute(true)
# async: 等 1.5 帧后断言 write 被调用（mock 记录写事件）
```

- [ ] **Step 2: 运行测试验证失败**

- [ ] **Step 3: 实现 WidgetRecipe / WidgetMacro**

JSON 解析用 Godot `JSON::parse_string`（GDScript 侧传入已解析的 `Dictionary` 也可——本计划让 C++ 接收 `Variant(Dictionary)` 减少解析依赖，`parse(json: String)` 内部 `JSON::parse_string` 或直接暴露 `configure(cfg: Dictionary)`）。**实现选用 `configure(Dictionary)` 更简单且避开字符串明文需求**；`parse(json: String)` 保留做兼容。

防明文模式（示例）：
```cpp
// 不写 "invalid recipe"; 用 XOR 常量
static const char kErr[] = {'i','n','v','a','l','i','d','\0'}; // 若需隐藏，用异或表；v1 至少避免直接字面量
```

- [ ] **Step 4: 注册 + 构建 + 测试**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(native): WidgetRecipe/WidgetMacro write sequencing + string obfuscation start"
```

---

### Task 10: 复杂元件 UI（TagTrend / TagAlarmList / TagRecipe / TagMacroButton）

**Files:**
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_trend.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_alarm_list.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_recipe.gd` + `.tscn`
- Create: `app/visualization/addons/industrial_widgets/widgets/tag_macro_button.gd` + `.tscn`
- Test: `app/visualization/tests/test_widgets_advanced.gd`

**Interfaces:**
- Consumes: `WidgetTrend`（Task 5）、`WidgetAlarm`（Task 6）、`WidgetRecipe/WidgetMacro`（Task 9）
- Produces:
  - `TagTrend`(extends Control)：每收到 `tag_changed` 调 `WidgetTrend.push_sample`，`_draw()` 用 `query_window` 画折线
  - `TagAlarmList`(extends ItemList)：监控若干个 tag，调 `WidgetAlarm.update`，active 项上色 + 闪烁
  - `TagRecipe`(extends PanelContainer)：可选配方列表（OptionButton + 表格），切换 → `WidgetRecipe.build_write_sequence` → 批量 `WidgetBridge.write_tag`
  - `TagMacroButton`(extends Button)：pressed → `WidgetMacro.execute`，step 完成后刷新按钮文本/禁用态

- [ ] **Step 1: 写失败测试**——mock 推值序列驱动 4 个复杂元件状态变化

- [ ] **Step 2: 运行测试验证失败**

- [ ] **Step 3: 实现 4 个元件**

（每个元件 `_ready()` 订阅 tag；`_on_tag_changed` 调对应 C++ 核心 → 刷新 UI；写操作经 `WidgetBridge.write_tag`）

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(widgets): TagTrend/AlarmList/Recipe/MacroButton UI shells"
```

---

### Task 11: 编辑器元件面板（WidgetPalette dock + EditorPlugin）

**Files:**
- Create: `app/visualization/addons/industrial_widgets/editor/editor_plugin.gd`
- Create: `app/visualization/addons/industrial_widgets/editor/widget_palette.gd`
- Modify: `app/visualization/addons/industrial_widgets/plugin.cfg`

**Interfaces:**
- Consumes: 12 个元件类（Task 2/8/10）
- Produces:
  - `WidgetPalette`(extends VBoxContainer)：分组树（显示/输入/状态/仪表/趋势/报警/配方/宏），每项 `item_activated` 触发 `add_widget(widget_script_path, position)`；支持拖拽 `_get_drag_data`
  - `EditorPlugin`：`_enter_tree` 把 `WidgetPalette` 加到 `EditorInterface.get_editor_main_screen()` 的右侧 dock（或 `add_control_to_dock(DOCK_SLOT_RIGHT_UL, palette)`）；`_handles(obj)` 返回 target 是 Control

- [ ] **Step 1: 写失败测试**——在测试工程（或 headless 编辑器脚本）里 `load("res://addons/industrial_widgets/editor/editor_plugin.gd").new()` 且注册后 `EditorInterface` 能拿到 palette dock；`palette.add_widget(...)` 在当前场景根添加元件实例

- [ ] **Step 2: 运行测试验证失败**

（注意：EditorPlugin 测试需在编辑器上下文；用 `--editor --quit-after 1` 或专门测试场景验证）

- [ ] **Step 3: 实现 EditorPlugin + WidgetPalette**

```gdscript
# editor_plugin.gd
@tool
extends EditorPlugin
var palette: Control
func _enter_tree() -> void:
	palette = preload("res://addons/industrial_widgets/editor/widget_palette.gd").new()
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, palette)
func _exit_tree() -> void:
	if palette: remove_control_from_docks(palette); palette.free()
```

`widget_palette.gd`：`Tree` 分组 + `Button`/`Tree item_activated` → `_add_widget(meta_path)` 在 `get_current_scene()` root 添加实例，`widget.position = (get_viewport mouse pos)`。

- [ ] **Step 4: 运行测试验证通过**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(editor): widget palette dock with click+drag placement"
```

---

### Task 12: 演示画面 + 集成测试（全 12 元件 + mock runtime）

**Files:**
- Create: `app/visualization/scenes/Screen2_WidgetsDemo.tscn`（全元件演示画面）
- Modify: `app/visualization/scripts/MainController.gd`（加第二个按钮 + 画面切换）
- Modify: `app/visualization/tests/test_widgets_demo.gd`（集成：mock 数据源驱动全画面）

**Interfaces:**
- Consumes: 全部元件 + WidgetBridge（mock 模式）
- Produces: 演示画面 + 集成测试通过准则

- [ ] **Step 1: 创建 Screen2_WidgetsDemo.tscn**——放置全部 12 元件（TagLabel/NumDisplay/NumInput/Lamp/Switch/Meter/Trend/AlarmList/Recipe/MacroButton/Bar/Gauge），tag 名用 mock 已注册的名字（如 `Pump01.Speed`、`Tank01.Level`、`Valve02.OpenCmd`…）

- [ ] **Step 2: MainController 增加画面切换按钮**（`BtnDemo`，`_load_demo()` 与 `_load_factory()` 平行）

- [ ] **Step 3: 写集成测试**——headless 加载 Screen2，mock 推一组值，断言：NumDisplay 文本格式化正确；Lamp 颜色翻转；Trend 收到样本；Alarm 一条 active 上色；Switch 切换触发写事件

- [ ] **Step 4: 运行测试验证通过**（`--script res://tests/test_widgets_demo.gd`）

- [ ] **Step 5: 手动验证指引**

```bash
cd "D:/dt-engine" && ./bin/godot.windows.editor.x86_64.exe --path app/visualization --editor
# 打开工程 → 运行 → 切到「元件演示」画面 → 全部元件应显示 mock 数据
```

- [ ] **Step 6: Commit**

```bash
git commit -m "feat(demo): widgets demo screen + integration test with mock runtime"
```

---

### Task 13: 防反编译加固收尾（strip + 字符串脱敏确认 + 文档）

**Files:**
- Modify: `app/visualization/addons/industrial_widgets/native/gdextension/SConstruct`（发布构建加 strip/去符号）
- Create: `app/visualization/addons/industrial_widgets/native/BUILD.md`（构建 + 加固说明）
- Create: `app/visualization/addons/industrial_widgets/README.md`（用户安装/使用说明）

**Interfaces:**
- 无新接口；验证现有 DLL 已 strip

- [ ] **Step 1: 发布构建加 strip**

SConstruct 或构建脚本末尾加：
```python
if env["PLATFORM"] == "win32" and env.get("RELEASE"):
    env.Append(LINKFLAGS=["/DEBUG:NONE"])   # 或 MSVC 不需要 strip，Release 默认去调试
```
Windows Release 构建默认无调试符号（PDB 不生成）；**额外核对**：`dumpbin /headers` 确认无 `COFF debug directory` / 无导出非入口符号（`dumpbin /exports libindustrial_widgets_native.dll` 只应有 `industrial_widgets_native_entry`）。

- [ ] **Step 2: 字符串明文复查**

```bash
grep -rn "\"[A-Za-z][^\"]*\"" app/visualization/addons/industrial_widgets/native/gdextension/src/ | grep -v "get_version\|class_\|register\|//"
```
确认无密钥/协议关键串明文（仅错误消息等展示性字面量保留，若需隐藏用异或）。

- [ ] **Step 3: 写 BUILD.md**

内容：godot-cpp 拉取、SConstruct 增量构建命令、Release strip 要点、Windows `dumpbin` 校验步骤。

- [ ] **Step 4: 写 README.md**

内容：插件安装（复制 `addons/industrial_widgets/` 到目标工程 `addons/`）、`project.godot` 注册 autoload `WidgetBridge`、元件面板使用（点击/拖拽）、数据源配置（WS URL / mock 开关）、12 元件一表。

- [ ] **Step 5: 全量回归**

```bash
cd "D:/dt-engine" && ./bin/godot.windows.editor.x86_64.exe --headless --path app/visualization --script res://tests/test_widget_bridge.gd
# ... 每个 test_*.gd 依次跑，全部 PASS
```

- [ ] **Step 6: Commit**

```bash
git add app/visualization/addons/industrial_widgets
git commit -m "build(native): release strip + hardening docs (BUILD/README)"
```