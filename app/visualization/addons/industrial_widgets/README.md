# Industrial Widgets 元件插件

EBPro 对齐的工业 HMI 元件集（Godot 4.8.dev 自定义引擎）。

## 安装

1. **引擎要求**：本插件依赖编译进引擎的 C++ widget 类
   （`WidgetFormat`/`WidgetTrend`/`WidgetAlarm`/`WidgetMeter`/`WidgetRecipe`/
   `WidgetMacro`），必须使用 `D:/dt-engine` 的 scons 产物
   `bin/godot.windows.editor.x86_64.exe`（见 [BUILD.md](BUILD.md)）。

2. **复制插件**：把 `addons/industrial_widgets/` 整个目录复制到目标工程
   `res://addons/` 下。

3. **注册 autoload**：在目标工程 `project.godot` 的 `[autoload]` 段添加：

   ```ini
   WidgetBridge="*res://addons/industrial_widgets/autoload/widget_bridge.gd"
   ```

4. **启用编辑器插件**：`项目设置 → 插件` → 勾选 `Industrial Widgets`。

## 数据桥（WidgetBridge 三态）

| 模式 | 触发条件 | 数据流 |
|---|---|---|
| `cpp_runtime` | `IndustrialRuntime` 类存在 | 真实 C++ Runtime（WS 连接） |
| `native_module` | 任一 `Widget*` C++ 类存在 | mock 数据 + C++ 核心能力 |
| `mock` | 两者皆无 | 纯 GDScript 模拟源 |

- 演示默认 mock 档；生产接入把 `use_mock` 置 false 并提供 `IndustrialRuntime`。
- `control` 测试可 `bridge.set("use_mock", true)` 后重调 `_detect_backend()`。

## 元件面板使用

编辑器右侧 dock「元件面板」：
- **点击**某个元件 → 在当前场景根放置实例（位置随鼠标）。
- 放置后可设置 `tag_name` 等导出属性，运行即订阅 mock 数据。

## 12 元件一览

| 分类 | 元件 | 基类 | 说明 |
|---|---|---|---|
| 显示 | TagLabel | Label | 标签显示值 + 质量着色 |
| 显示 | TagNumDisplay | Label | C++ 数字格式化（千分位/BCD/进制） |
| 输入 | TagNumInput | LineEdit | C++ 校验解析 → 写回 |
| 状态 | TagLamp | Panel | 非 0 亮 + 可选闪烁 |
| 状态 | TagSwitch | CheckBox | 开关写回 |
| 仪表 | TagMeter | Control | `_draw` 自绘 + C++ 角度核心 |
| 仪表 | TagBar | HSlider | 水平进度条 |
| 仪表 | TagGauge | ProgressBar | 圆弧仪表盘 |
| 趋势 | TagTrend | Control | C++ 环形缓冲曲线 |
| 报警 | TagAlarmList | ItemList | C++ 规则求值 + active 上色 |
| 配方 | TagRecipe | PanelContainer | 选配方 → 批量写 |
| 宏 | TagMacroButton | Button | 宏时序执行 + 进度文本 |

## 数据源配置

- **mock**：默认（`widget_bridge_mock.gd` 正弦/方波/随机）。
- **真实 WS**：`IndustrialRuntime` 编译入引擎时自动走 `cpp_runtime`；
  用 `WidgetBridge.connect_backend("ws://host:port")` 指定地址。

## 演示画面

`res://scenes/Screen2_WidgetsDemo.tscn` 放置全部 12 元件，运行即见 mock 数据。
集成测试：`godot --headless --path . --script res://tests/test_widgets_demo.gd`。