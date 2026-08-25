# Industrial Widgets — 构建与加固说明

本项目是 Godot 4.8.dev 自定义引擎（`D:/dt-engine`）之上的工业元件插件。
C++ 核心类（WidgetFormat / WidgetTrend / WidgetAlarm / WidgetMeter /
WidgetRecipe / WidgetMacro）直接编译进引擎二进制（`modules/industrial_editor/`），
**没有独立的 GDExtension DLL**（Ruling W5）。

## 1. 目录结构

```
modules/industrial_editor/          ← C++ 核心（随引擎 scons 编译）
  widget_format.h/.cpp              数字格式化 + 文本解析
  widget_trend.h/.cpp               环形缓冲历史曲线
  widget_alarm.h/.cpp               报警条件求值 + 去抖 + 确认
  widget_meter.h/.cpp               表盘角度线性映射
  widget_recipe.h/.cpp              配方写序列
  widget_macro.h/.cpp               宏时序执行
  widget_obfuscate.h                字符串 XOR 防明文辅助

app/visualization/addons/industrial_widgets/
  plugin.cfg                        EditorPlugin 入口
  autoload/widget_bridge.gd         三态数据桥（cpp_runtime/native_module/mock）
  autoload/widget_bridge_mock.gd    模拟数据源
  widgets/                          12 个 GDScript 元件
  editor/                           元件面板 dock
  demo/                             演示画面控制器
```

## 2. 构建引擎

```bash
cd "D:/dt-engine"
PYTHONPATH= "C:/Users/jay/AppData/Local/Programs/Python/Python39/Scripts/scons.exe" \
  platform=windows target=editor accesskit=no d3d12=no -j12
```

- 增量构建通常 <2 分钟（只重编改动文件 + 链接）。
- `target=release` 用于发布/性能验证：

```bash
PYTHONPATH= "C:/Users/jay/AppData/Local/Programs/Python/Python39/Scripts/scons.exe" \
  platform=windows target=release accesskit=no d3d12=no -j12
```

## 3. C++ 编码规范（Ruling W6 强制）

所有 widget C++ 必须遵守
`docs/specs/2026-08-21-godot-cpp-module-cross-platform-standard.md`：

- **容器**：`std::vector` → `Vector<T>`（`core/templates/vector.h`），
  `std::map` → `HashMap<K,V>`（`core/templates/hash_map.h`），
  `std::string` → `String`，`std::pair` → `Pair<T,U>`
- **HashMap 迭代**：`.key` / `.value`（不是 `.first` / `.second`）
- **拼串**：`vformat()`，**错误消息**：`TTRC()` / `ETR()`（本地化上下文）
- **禁止 STL 头**：`#include <vector>` / `<map>` / `<string>` / `<utility>` 一律不得出现
- **Vector 写元素**：`vec.set(i, v)`（Godot 4.8 无 `.write[i]`）

自检命令：

```bash
grep -rn "std::" modules/industrial_editor/widget_*.[ch]pp   # 应无输出
```

## 4. 字符串防明文

- 展示性错误消息走 `TTRC()`（编译期提取 + 翻译上下文）。
- 密钥/协议关键串用 `widget_obfuscate.h` 的 XOR 解码（运行期解码，二进制静态
  扫描不可见）。
- 复查：

```bash
grep -rn "\"[A-Za-z][^\"]\{4,\}\"" modules/industrial_editor/widget_*.cpp \
  | grep -v "get_version\|D_METHOD\|//\|class_"   # 应只剩 TTRC/允许项
```

## 5. Release 加固要点（Windows）

- Ruling W5：无独立 DLL → 无需 dumpbin 导出白名单。
- Release 构建默认不带调试符号（无 PDB 生成）。
- 如需进一步剥离：`target=release` 时引擎自身已 `-O2` + 去符号，
  保持默认即可。

## 6. 测试

```bash
cd "D:/dt-engine"
BIN="bin/godot.windows.editor.x86_64.exe"
for t in test_widget_bridge test_widgets_simple test_native_load \
         test_widget_format test_widget_trend test_widget_alarm \
         test_widgets_complex test_widget_recipe_macro \
         test_widgets_advanced test_editor_palette test_widgets_demo; do
  "$BIN" --headless --path app/visualization --script "res://tests/$t.gd"
done
```

全部应输出 `RESULT: PASS`。