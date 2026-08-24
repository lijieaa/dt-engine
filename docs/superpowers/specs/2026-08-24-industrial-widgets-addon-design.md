# Industrial Widgets Addon — 设计文档

- 日期: 2026-08-24
- 状态: 已与用户逐节确认（设计基线 / 元件清单 / 架构）
- 范围: `D:\dt-engine\app\visualization` 同级的插件工程

## 1. 目标

在 Godot 中「对齐实现 EBPro 画面元件」：做成一款**可移植的 Godot 插件（addon）**，
提供一组可在**任意 Godot 工程**中调用的工业 UI 元件（数字显示/输入、位灯、开关、
仪表、趋势图、报警列表、配方、宏按钮等），通过 WebSocket 绑定采集 Go runtime 的
PLC 数据。核心设计约束：**防止项目被反编译**（提高逆向门槛，服务端授权为最终防线）。

## 2. 关键决策（用户已确认）

| # | 决策 | 内容 |
|---|------|------|
| D1 | 包装形态 | 标准 Godot 插件 `addons/industrial_widgets/`（plugin.cfg + EditorPlugin + 元件面板 dock） |
| D2 | 数据层模式 | **模式A**：优先复用 C++ `IndustrialRuntime` singleton；标准 Godot 工程无该单例时由 GDExtension DLL 兜底 |
| D3 | C++ 承载 | **独立 GDExtension `industrial_widgets_native`（DLL 随插件分发）**，不扩展现有 `modules/industrial_runtime/`（避免每次改动重编整个引擎） |
| D4 | 加固档位 | v1 做**基础档**：strip 去符号 + 核心字符串不进明文 + `.pck` 资源加密 + 核心逻辑全部下沉 C++ |
| D5 | 授权边界 | **遵循 PRD §14/§15**：License 校验 / 写权限 / 身份校验 / 设备授权全部在 Go Runtime；客户端仅提高逆向门槛，不承担最终授权 |

## 3. 防反编译防御层次（由外到内）

```
第1层 资源层   .pck 加密 + 脚本二进制（发布版开启 binary_format + 加密导出）
第2层 代码层   GDScript 只做 UI 编排/绑定声明，不承载核心算法
第3层 核心层   GDExtension C++ DLL
        ├─ 去符号 (strip)：不导出 symbol
        ├─ 核心协议/算法/密钥派生在 C++，GDScript 拿不到
        ├─ 关键字符串运行时拼接/加密常量，不落明文
        └─ 核心逻辑(格式化/校验/闪烁/自绘/缓冲/报警/配方/宏)全在 DLL
第4层 服务端   真正的授权/写权限/校验在 Go Runtime（不可篡改绕过）
```

## 4. 数据层双轨架构

```
┌─ addons/industrial_widgets/（可移植插件壳）──────────────┐
│  plugin.cfg + EditorPlugin(元件面板) + *.gd 元件类       │ UI 编排/绑定声明
└──────────────────────────────────────────────────────────┘
        │ 统一 bridge 接口: tag_changed / write_tag / subscribe
        ▼
WidgetBridge (autoload, GDScript 壳)
        │
        ├─ 优先 → C++ IndustrialRuntime（自定义引擎编译进来）
        │         核心通讯/缓存/表达式全在 C++，GDScript 只转发信号
        └─ 兜底 → WidgetNative (GDExtension DLL)
                  标准 Godot 工程加载 DLL 提供核心算法；
                  未检测到 runtime 时该桥也提供内置 mock 模式
```

- 所有元件只依赖桥层统一信号 `tag_changed(tag, value, quality, version, ts_ms)` + `write_tag()`，不感知底层。
- 未安装插件工程 → 自动提示"未检测到 Runtime，使用内置模拟数据"。

## 5. 元件清单（对应 EBPro 画面元件家族）

| # | 元件 | Godot 基类 | 数据/行为 | 核心逻辑位置 | EBPro 对应 |
|---|------|-----------|----------|------------|-----------|
| 1 | TagLabel | Label | 文本显示 + 质量配色 | GDScript（薄，移植） | 数字显示(文本) |
| 2 | TagNumDisplay | Label | 整数位/小数位/千分位/单位/BCD 格式化 | **C++** `WidgetFormat.format_value` | 数字显示(Numeric Display) |
| 3 | TagNumInput | LineEdit/SpinBox | 键盘输入 + 写回 + 范围校验 | **C++**（校验/写回序列化） | 数字输入(Numeric Input) |
| 4 | TagLamp | Panel/Control | 位状态灯：ON/OFF 颜色、闪烁、状态文本 | **C++**（闪烁定时/状态机） | 位状态灯(Lamp) |
| 5 | TagSwitch | CheckBox/Button | 位写回开关 | GDScript（已有，移植） | 开关(Switch) |
| 6 | TagMeter | Control 自绘 | 弧形仪表盘指针 + 刻度 + 红绿区 | **C++**（自绘 `_draw`） | 仪表(Meter/Gauge) |
| 7 | TagTrend | Control 自绘 | 历史曲线：环形缓冲、滚动、缩放 | **C++**（环形缓冲+缩放数学） | 趋势图(Trend) |
| 8 | TagAlarmList | ItemList/Tree | 报警列表：条件求值、确认、历史 | **C++**（报警条件/去抖/确认状态机） | 报警显示(Alarm) |
| 9 | TagRecipe | 容器+表格 | 配方表：切换配方→批量写回 | **C++**（配方解析/写回编排） | 配方(Recipe) |
| 10 | TagMacroButton | Button | 宏按钮：一组写操作序列 | **C++**（宏序列解析/时序执行） | 宏(Macro) |
| 11 | TagBar | HSlider/Range | 横条状态 | GDScript（薄，移植） | 状态条 |
| 12 | TagGauge | ProgressBar | 进度式仪表 | GDScript（薄，移植） | 仪表(简化) |

> 1/5/11/12 已有移植；2/3/4/6/7/8/9/10 新写。**有价值算法全部在 C++**。

### 5.1 C++ / GDScript 职责切分

**C++（GDExtension `WidgetNative` / `WidgetFormat` / `WidgetEngine`）**
- `WidgetFormat::format_value(tag_meta, raw_value)` → 数字格式化（BCD/小数位/单位/千分位/字节序）
- `WidgetInput::validate(raw, meta)` + `serialize_write(raw, meta)` → 输入校验与写回序列化
- `WidgetLamp::state_machine(value, blink_cfg)` → 位灯状态机（颜色/闪烁/去抖）
- `WidgetMeter::draw(draw_ctx, value, range)` → 圆弧仪表自绘（刻度/指针/红绿区）
- `WidgetTrend::push_sample(ts, value)` + `query_window(from, to)` → 环形缓冲（滑动窗口）
- `WidgetAlarm::evaluate(tag, value, rule)` → 报警条件求值 + 确认状态机 + 去抖
- `WidgetRecipe::parse(recipe_json)` + `build_write_sequence(entry)` → 配方写回编排
- `WidgetMacro::parse(macro_json)` + `execute(timeline)` → 宏时序执行器
- `WidgetCrypto`（可选 v1）: 核心密钥派生 + 工程完整性辅助校验

**GDScript（插件内，仅壳）**
- 各元件节点封装：属性 export（tag_name/颜色/范围/格式等）+ 信号转发
- WidgetBinding：把元件属性映射到 `WidgetBridge` 订阅，调用 C++ 核心
- 元件面板 dock：分组树 + 点击/拖拽创建实例
- 事件编排（如按钮 pressed → 写回、输入确认 → 校验+写回）——逻辑校验仍调 C++

### 5.2 新元件数据流示例（TagNumDisplay）

```
Go Runtime → WS tag_update → WidgetNative (C++) 缓存+格式化
  → WidgetBridge.tag_changed → TagNumDisplay._on_tag_changed
  → WidgetNative.WidgetFormat.format_value(...) → Label.text = 结果
```

## 6. 元件面板（编辑器交互）

- 停靠 dock「工业元件」：左侧树分组（显示/输入/状态/仪表/趋势/报警/配方/宏）
- **点击**：向当前画面（ScreenHost 或选中 Control）添加元件实例，自动挂 `WidgetBinding`（tag 名后填）
- **拖拽**：从面板拖到画面任意 Control 落位（Godot EditorPlugin `_get_drag_data` 机制）
- 新实例自动配置默认「订阅 + 表达式=$Tag」骨架
- 每个元件自动带 `WidgetBinding` 子节点（复用表达式 `$tag` / `clamp()` 等）

## 7. 数据流 / 错误处理

- 订阅失败（tag 不存在）→ 元件显示兜底 `—` + 质量码配色（stale/uncertain/bad）
- 写入失败 → `write_result(rid, ok=false, error)` → 元件恢复原值 + 状态栏提示
- 断线重连：WidgetBridge 复用现有 `backoff_ms` 指数退避；重连后自动重订阅全部激活 tag
- mock 模式：`WidgetBridge.enable_mock = true` 时内置模拟数据生成器（正弦/方波/随机），无 PLC 可预览

## 8. 测试策略

- **C++ 层**: 每元件核心逻辑用 Godot 单测框架（`tests/`）或 gtest 风格测试
  - WidgetFormat: BCD/小数位/单位/千分位边界
  - WidgetAlarm: 条件求值/去抖/确认状态机
  - WidgetTrend: 环形缓冲滑动窗口正确性
  - WidgetRecipe/WidgetMacro: 写回序列/时序
- **GDScript 层**: 每个元件 mock 数据源（`WidgetBridge.enable_mock`）驱动 `tag_changed`，断言 UI 状态变化（闪烁/颜色/文本/指针）
- **集成**: `app/visualization` 演示画面 + mock Go runtime（`--mock` 模式）跑通全部 12 元件

## 9. 交付物结构

```
app/visualization/addons/industrial_widgets/
├── plugin.cfg
├── editor/
│   ├── WidgetPalette.gd          # 元件面板 dock（分组树+拖拽）
│   └── editor_plugin.gd           # EditorPlugin 注册
├── autoload/
│   └── WidgetBridge.gd            # 双轨数据桥（IndustrialRuntime 优先 / DLL 兜底 / mock）
├── widgets/
│   ├── tag_label.gd / .tscn        # 1
│   ├── tag_num_display.gd / .tscn  # 2
│   ├── tag_num_input.gd / .tscn    # 3
│   ├── tag_lamp.gd / .tscn         # 4
│   ├── tag_switch.gd / .tscn       # 5
│   ├── tag_meter.gd / .tscn        # 6
│   ├── tag_trend.gd / .tscn        # 7
│   ├── tag_alarm_list.gd / .tscn   # 8
│   ├── tag_recipe.gd / .tscn       # 9
│   ├── tag_macro_button.gd / .tscn # 10
│   ├── tag_bar.gd / .tscn          # 11
│   └── tag_gauge.gd / .tscn        # 12
├── native/                         # GDExtension C++ 源码
│   ├── CMakeLists.txt
│   ├── BUILD.md
│   ├── src/register_types.cpp
│   ├── src/widget_format.h/.cpp
│   ├── src/widget_alarm.h/.cpp
│   ├── src/widget_trend.h/.cpp
│   ├── src/widget_meter.h/.cpp
│   ├── src/widget_recipe.h/.cpp
│   ├── src/widget_macro.h/.cpp
│   └── src/widget_crypto.h/.cpp (可选)
└── industrial_widgets.gdextension
```

（那条 `.pck 加密` 属于宿主工程的发布配置，不在插件目录内。）