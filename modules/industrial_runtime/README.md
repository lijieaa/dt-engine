# Industrial Runtime — Godot 内建模块 & 可视化客户端

本目录（`D:\dt-engine`）是 Godot 引擎源码仓库。除了 Godot 本身外，还包含：

- **内建 C++ 模块**：`modules/industrial_runtime/` —— Tag 缓存、WebSocket 客户端、表达式求值、IndustrialRuntime 门面、TagBinding 节点。
- **可视化客户端 Godot 工程**：`app/visualization/` —— 一个工程化的 UI 壳，包括 Tag 浏览器面板、状态栏、多画面切换、以及若干示例 UI 组件（TagLabel / TagGauge / TagBar / TagSwitch）。

对应的 Go 侧服务端工业运行时位于并仓库 `D:\driver-engine`（同一级目录），WebSocket 端点为 `ws://127.0.0.1:8787/ws`，HTTP API 为 `http://127.0.0.1:8787/api/v1`。此工程就是 Go 运行时的可视化客户端。

---

## 1. 模块内容

```
modules/industrial_runtime/
├── config.py                # 模块构建配置（无三方依赖）
├── SCsub                    # SCons 源文件清单
├── register_types.h / .cpp  # 模块初始化，注册类
├── value_convert.h / .cpp   # Go 侧协议常量 + Variant ⇄ JSON 兼容转换
├── tag_cache.h / .cpp       # 客户端 Tag 缓存（hashmap，质量码/版本号/时间戳）
├── ws_client.h / .cpp       # 基于 WebSocketPeer 的协议客户端
├── expr_eval.h / .cpp       # 小型表达式求值器（$tag 引用 + 四则 + 函数）
├── industrial_runtime.h/cpp # 门面类 IndustrialRuntime（单例）
└── tag_binding.h / .cpp     # 把标签订阅、表达式和 UI 属性绑定在一起的节点
```

### 1.1 `IndustrialRuntime`（门面类）

GDScript 使用方式：

```gdscript
var rt = IndustrialRuntime.new()
rt.connect_runtime("ws://127.0.0.1:8787/ws")
rt.subscribe(["T1", "T3"], "my_view")           # rid 用于多订阅者（Panel/画面）区分
rt.write_tag("T1", 3.14, "my_write_req_1")       # 返回 request id
rt.unsubscribe(["T1"], "my_view")
```

重要信号：
- `connection_state_changed(state:int)` 0/1/2/3/4 = IDLE/CONNECTING/OPEN/CLOSING/CLOSED
- `connection_error(code:int, message:String)`
- `tag_changed(tag, value, quality, version, ts_ms)`
- `subscribe_result(rid, ok, error_text)`
- `write_result(rid, tag, ok, error_text, verified)`

### 1.2 `TagBinding` 节点

把 UI 控件放在 `scripts/ui/TagLabel.gd` 等节点下，添加一个 `TagBinding` 子节点，
配置 inspector：

```
tags                 = ["T1"]
expression           = "$T1"                      # 也支持 clamp($T1,0,100)、quality("T1")
target_property      = "text"                     # 写父节点 Label.text
fallback             = "—"
write_enable         = true                       # （可选）开启回写
write_source_signal  = "pressed"                  # 父节点信号名
write_source_property= "value"                    # 回写时取值的父节点属性
```

---

## 2. 客户端工程 `app/visualization/`

### 2.1 文件布局

```
app/visualization/
├── project.godot
├── icon.svg
├── cfg/
│   ├── runtime_endpoint.json         # WebSocket/HTTP 地址 & 重连参数
│   └── screens.json                  # 画面清单（MainController 读取）
├── scripts/
│   ├── Runtime.gd                    # 全局 autoload：重连、tag_directory REST、License、缓存计数
│   ├── MainController.gd             # 主场景 Controller：按钮加载画面
│   ├── panels/TagTreePanel.gd        # 左侧 Tag 浏览器（REST 目录+双击订阅+右键写入）
│   ├── panels/StatusBar.gd           # 底部连接状态 + License 横幅
│   └── ui/
│       ├── TagLabel.gd               # 带质量码配色的 Label
│       ├── TagGauge.gd               # 进度条样式仪表
│       ├── TagBar.gd                 # 横条仪表（Range 子类）
│       └── TagSwitch.gd              # CheckBox 回写控件
└── scenes/
    ├── Main.tscn                     # 主窗口（TopBar / 侧栏 / 画面区域 / 状态栏）
    └── Screen1_FactoryFloor.tscn     # 示例“工厂车间 01”画面
```

### 2.2 运行前先做的事

**注意：** Godot 原生编辑器二进制**不包含** `industrial_runtime` 模块。
你必须先在 `D:\dt-engine` 下执行 SCons 编译，产出内置本模块的 Godot 可执行文件，
再用这个自定义 Godot 编辑器去打开 `app/visualization/`，
否则类 `IndustrialRuntime`、`TagBinding`、`TagCache` 等无法加载。

```powershell
# 从开发机命令行（需已安装 Python 3.10+ 和对应 MSVC 编译器）：
cd D:\dt-engine
# 编辑器版本
scons platform=windows target=editor module_industrial_runtime_enabled=yes -j8
# 或者导出版本（用于打包可执行）
scons platform=windows target=template_release module_industrial_runtime_enabled=yes -j8
```

编译完成后，产物通常位于 `D:\dt-engine\bin\godot.windows.editor.x86_64.exe`。
然后：

```powershell
& .\bin\godot.windows.editor.x86_64.exe --path app\visualization --editor
```

### 2.3 和 Go 运行时联调步骤

1. 启动 Go 侧工业运行时：

   ```powershell
   cd D:\driver-engine
   go run ./cmd/runtime --config examples/project.json --addr 127.0.0.1:8787
   ```

   需要带 license 时加 `--license path/to.lic`；不带就跑 demo 模式（无 enforcer，license/status 返回 `invalid_signature`）。

2. 确认端口 OK，默认：
   - HTTP `http://127.0.0.1:8787/api/v1`
   - WS   `ws://127.0.0.1:8787/ws/v1`

   修改 `app/visualization/cfg/runtime_endpoint.json` 即可更换地址。

3. 启动自定义 Godot 编辑器打开工程后，点 F5 运行 `Main.tscn`。
   - 底部状态栏变绿色 = 已连接
   - 左侧 Tag 浏览器从 `/api/v1/tags` 拉到目录，双击订阅，右键写入
   - 主画面区域显示 `Screen1_FactoryFloor`（需给其中几个控件手工挂 TagBinding 节点并填 `tags = ["T1"]` 等，或替换为你的实际 Tag 名）

---

## 3. 协议 & 常量

协议版本、消息类型、质量码统一集中在：

- `D:\dt-engine\modules\industrial_runtime\value_convert.h`（客户端侧常量）
- `D:\driver-engine\internal\ws\protocol.go`（服务端侧常量）

版本不一致时 `ws_client.cpp` 会打 warning；
质量码 `good` / `uncertain` / `bad` / `stale` 在 UI 组件里映射为不同颜色。
