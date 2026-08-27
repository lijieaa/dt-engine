# Godot 编辑器 UI 布局 — 现状分析

- 日期: 2026-08-27
- 状态: 分析文档（用户确认范围：仅分析 Godot 本身编辑器壳，不改代码）
- 范围: `editor/` 下编辑器主窗、dock 槽位、主屏、底栏与布局持久化
- 不在范围: 运行时 HMI（`app/visualization`）、工业扩展 dock、游戏内 Control 布局

## 1. 目标

记录本仓库所基于的 **Godot 编辑器原生 UI 布局模型**：控件树如何搭、dock 槽位如何划分、默认面板落在哪里、布局如何存取。作为后续工业扩展挂靠、冲突排查与布局相关改动的共同基线。

## 2. 产品边界

| 概念 | 说明 |
|------|------|
| 编辑器壳 UI | `EditorNode` 构建的 IDE 界面（本文件主题） |
| 运行时游戏/应用 UI | 用户工程内的 `Control` / 场景布局（不在本文件） |
| 主题与缩放 | `EditorThemeManager` + `EDSCALE`；影响尺寸与外观，不改变槽位拓扑 |

本仓库是 Godot 源码 + 工业模块；下文描述的是 **上游 Godot 编辑器布局机制**，以本树 `editor/` 代码为准。

## 3. 控件树骨架

入口在 `editor/editor_node.cpp`（约构造后半段：创建 `gui_base` → `main_vbox` → 分割树）。

```
gui_base (Panel, PRESET_FULL_RECT)
└─ main_vbox (VBoxContainer)
   ├─ title_bar (EditorTitleBar)
   │    菜单栏、主屏切换按钮、布局菜单等
   └─ main_vsplit (DockSplitContainer, vertical)   // name: DockVSplitMain
      ├─ main_hsplit (DockSplitContainer)          // name: DockHSplitMain
      │  ├─ left_l_vsplit   → LEFT_UL / LEFT_BL
      │  ├─ left_r_vsplit   → LEFT_UR / LEFT_BR
      │  ├─ center_vb
      │  │  └─ center_split (vertical)
      │  │       ├─ EditorMainScreen
      │  │       └─ EditorBottomPanel
      │  ├─ right_l_vsplit  → RIGHT_UL / RIGHT_BL
      │  └─ right_r_vsplit  → RIGHT_UR / RIGHT_BR
      └─ bottom_hsplit                            // name 随实现
           ├─ BOTTOM_L
           └─ BOTTOM_R
```

设计要点：

- **中央主屏固定**：不进入左右 dock 槽，始终在 `center_split` 上半。
- **四周槽位网格**：左右各两列（每列上下两槽），底栏另有左右底槽；空槽折叠。
- **分割器统一**：`DockSplitContainer` 管理偏移与显隐，由 `EditorDockManager` 持有引用。

Android 路径会多包一层 `base_vbox` / `main_box`（触摸操作面板），桌面路径直接是 `gui_base → main_vbox`。

## 4. Dock 槽位契约

定义见 `editor/docks/editor_dock.h`：`EditorDock::DockSlot`。

| 枚举 | 布局位置 | 拖放网格示意（代码中 Rect2i） |
|------|----------|------------------------------|
| `DOCK_SLOT_LEFT_UL` | 最左列 · 上 | `(0,0,1,3)` |
| `DOCK_SLOT_LEFT_BL` | 最左列 · 下 | `(0,3,1,3)` |
| `DOCK_SLOT_LEFT_UR` | 左内侧列 · 上 | `(1,0,1,3)` |
| `DOCK_SLOT_LEFT_BR` | 左内侧列 · 下 | `(1,3,1,3)` |
| `DOCK_SLOT_RIGHT_UL` | 右内侧列 · 上 | `(6,0,1,3)` |
| `DOCK_SLOT_RIGHT_BL` | 右内侧列 · 下 | `(6,3,1,3)` |
| `DOCK_SLOT_RIGHT_UR` | 最右列 · 上 | `(7,0,1,3)` |
| `DOCK_SLOT_RIGHT_BR` | 最右列 · 下 | `(7,3,1,3)` |
| `DOCK_SLOT_BOTTOM` | 中心底栏（兼容/默认底栏槽） | 由 `EditorBottomPanel` 注册 |
| `DOCK_SLOT_BOTTOM_L` | 底区 · 左 | `(0,6,4,2)` |
| `DOCK_SLOT_BOTTOM_R` | 底区 · 右 | `(4,6,4,2)` |
| `DOCK_SLOT_NONE` / `MAX` | 未放置 / 枚举上界 | — |

每个槽实例为 `DockTabContainer`（侧栏为 `SideDockTabContainer`，底侧为 `BottomSideDockTabContainer`）：

- 同一槽内多个 `EditorDock` **叠成 Tab**。
- 边距声明合法拖放目标（例如 `LEFT_UL` 右缘 → `LEFT_UR`，底缘 → `LEFT_BL`）。
- Dock 还可 **浮动**（`DOCK_LAYOUT_FLOATING`）、关闭、按快捷键打开。

`EditorDock` 自身继承 `MarginContainer`，携带 `title`、`layout_key`、`default_slot`、`available_layouts` 等元数据；布局读写可通过虚方法挂到 ConfigFile。

### 4.1 插件 API 映射

`EditorPlugin` 暴露的 `DOCK_SLOT_*`（见 `editor/plugins/editor_plugin.h`）与 `EditorDock::DockSlot` 对齐。  
`add_control_to_dock(slot, control)` 会为控件包一层 dock 并 `set_default_slot`，进入同一套槽位系统。

注意：插件枚举历史上侧重八侧槽 + `BOTTOM`；底栏左右槽（`BOTTOM_L` / `BOTTOM_R`）以 `EditorDock` / `EditorDockManager` 为准。

## 5. 默认布局（出厂）

`EditorNode` 在注册核心 dock 后写入 `default_layout`（ConfigFile）。  
配置键 `dock_N` 中 **N = DockSlot 枚举值 + 1**。

| Config 键 | 对应槽 | 默认内容 |
|-----------|--------|----------|
| `dock_3` | `LEFT_UR` | Scene Tree + Import（选中 tab 0 = Scene） |
| `dock_4` | `LEFT_BR` | FileSystem + History |
| `dock_5` | `RIGHT_UL` | Inspector + Signals + Groups |
| （底栏） | `BOTTOM` 等 | `EditorLog`（Output）；另有 Audio 等面板高度偏移 |

默认水平宽度：可见左右 dock 约 `280 * EDSCALE`；`main_hsplit` 初始 `set_split_offsets({ +scaled, -scaled })`。  
`LEFT_UL` / `LEFT_BL` / `RIGHT_*R` / `RIGHT_BL` 默认常为空（折叠），用户拖入后才展开。

各核心 dock 的 `set_default_slot`（源码）：

| Dock | 默认槽 |
|------|--------|
| SceneTreeDock | `LEFT_UR` |
| ImportDock | `LEFT_UR` |
| FileSystemDock | `LEFT_BR` |
| HistoryDock | `LEFT_BR` |
| InspectorDock | `RIGHT_UL` |
| SignalsDock | `RIGHT_UL` |
| GroupsDock | `RIGHT_UL` |
| EditorLog | `BOTTOM` |

大量编辑器插件（Shader、Theme、SpriteFrames、Find in Files、VCS 等）默认落在 `BOTTOM` 或 `RIGHT_UL`。

## 6. 中央主屏与底栏

### 6.1 EditorMainScreen

- 类型：`PanelContainer`（`editor/editor_main_screen.h`）。
- 内置主编辑器表：`EDITOR_2D` / `EDITOR_3D` / `EDITOR_SCRIPT` / `EDITOR_GAME` / `EDITOR_ASSETLIB`。
- 行为：同一时间一个主屏插件可见；切换按钮挂在 title bar 的 `HBoxContainer`。
- 布局：主屏选择可写入 ConfigFile（与 dock 布局同属编辑器布局存档）。

### 6.2 EditorBottomPanel

- 类型：继承 `DockTabContainer`，槽为 `DOCK_SLOT_BOTTOM`。
- 挂在 `center_split` **下方**（与主屏竖直分割），不是左右侧栏。
- 附带 toaster、进度指示等底栏附属控件。
- `center_split` 默认可隐藏拖条（`DRAGGER_HIDDEN`），展开底栏时再表现分割行为。
- 默认布局里用 `bottom_panel_offsets` 字典记录各底栏面板目标高度（如 `Output = -270`，`Audio = -450`）。

## 7. 布局生命周期

```
启动 EditorNode
  → 构建分割树与空槽
  → EditorDockManager 注册槽 / vsplit / main splits
  → add_dock(各核心 dock) + 插件 add_control_to_dock
  → 加载工程 .godot/editor/editor_layout.cfg（失败则用 default_layout）
  → 用户拖拽 Tab / 浮动 / 显隐 / 改分割偏移
  → 保存回 editor_layout.cfg；可选「命名布局」经 Editor → Editor Layout
```

职责划分：

| 组件 | 职责 |
|------|------|
| `EditorNode` | 搭树、注册默认 dock、定义 default_layout、触发存盘 |
| `EditorDockManager` | 槽注册、移动、显隐、浮动、序列化/反序列化 |
| `EditorDock` | 单个面板元数据与 open/close/floating |
| `DockTabContainer` | Tab UI、拖放命中 |

## 8. 主题与缩放（对布局的影响）

- 主题：`editor/themes/`（Modern / Classic 等），由 `EditorThemeManager` 驱动。
- 缩放：`EDSCALE`；默认 dock 宽度、分割偏移、最小尺寸均乘缩放。
- **不改变**槽位拓扑；只改变像素尺度与样式。

## 9. 设计特征与约束

1. **槽位网格，非自由嵌套停靠**  
   只能落入预定义槽；不能像部分 IDE 那样任意递归 split 出无限区域。

2. **中心固定、左右对称**  
   经典「场景树 ↔ 视口 ↔ 属性」三栏；左右各保留一列「备用」槽给高级用户。

3. **同槽 Tab 密度高**  
   右侧 `RIGHT_UL` 默认挤三个属性相关 dock；插件再往同一槽塞会加剧 Tab 竞争。

4. **底栏双轨**  
   中心底栏（`EditorBottomPanel`）+ 可选 `BOTTOM_L` / `BOTTOM_R`；工具型、日志型面板偏向底栏。

5. **配置驱动、键名敏感**  
   布局靠 ConfigFile；`layout_key` 或 dock 标题变更会导致旧布局项失效或错位。

6. **扩展点清晰**  
   新面板应：实现/包装为 `EditorDock`（或经 `EditorPlugin::add_control_to_dock`），选好 `default_slot`，并考虑与默认三槽（左上内侧、左下内侧、右上内侧）及底栏的 Tab 竞争。

## 10. 关键源码索引

| 路径 | 用途 |
|------|------|
| `editor/editor_node.cpp` | 主布局构建、默认 layout、核心 dock 注册 |
| `editor/docks/editor_dock.h` | `DockSlot` / `DockLayout` 契约 |
| `editor/docks/editor_dock_manager.*` | dock 移动、显隐、浮动、持久化 |
| `editor/docks/dock_tab_container.*` | 槽容器与拖放 |
| `editor/docks/scene_tree_dock.cpp` 等 | 各核心 dock 默认槽与内部 UI |
| `editor/editor_main_screen.*` | 中央主屏切换 |
| `editor/gui/editor_bottom_panel.*` | 中心底栏 |
| `editor/plugins/editor_plugin.*` | 插件 dock API |
| `editor/themes/` | 主题与编辑器样式 |

## 11. 明确非目标

- 不规定工业模块应占用哪些槽（另文分析）。
- 不提出改 Godot 槽位拓扑或重写 dock 系统的方案。
- 不覆盖运行时 `app/visualization` 画面壳布局。

## 12. 后续可选方向（非本文件承诺）

若需要跟进，可单独开规格/计划：

- 对照本模型做「工业扩展 dock 落位与冲突」分析。
- 为团队约定插件默认槽与命名布局策略。
- 仅当产品要求改变 IDE 壳时，再写改动设计与 `writing-plans`。
