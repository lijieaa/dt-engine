extends Node
## DemoController — 元件演示画面控制器（Task 12）。
## 职责：
##   1. 记录各元件节点路径（供集成测试读取）
##   2. 确保 mock 数据源注册好演示用 tags（Pump01.* / Tank01.* / Valve02.*）
##   3. 提供 apply 入口把元件 tag_name 与 mock tags 对齐

## 元件路径（与 Screen2_WidgetsDemo.tscn 节点树对应；测试经 get() 读取）
@export var label_path := "Root/HSplit/Grid/LabelPanel/VBox/TagLabel"
@export var num_display_path := "Root/HSplit/Grid/NumPanel/VBox/TagNumDisplay"
@export var num_input_path := "Root/HSplit/Grid/NumPanel/VBox/TagNumInput"
@export var lamp_path := "Root/HSplit/Grid/StatusPanel/VBox/Row/TagLamp"
@export var switch_path := "Root/HSplit/Grid/StatusPanel/VBox/Row/TagSwitch"
@export var meter_path := "Root/HSplit/Grid/MeterPanel/VBox/Row/TagMeter"
@export var bar_path := "Root/HSplit/Grid/MeterPanel/VBox/Row/TagBar"
@export var gauge_path := "Root/HSplit/Grid/MeterPanel/VBox/Row/TagGauge"
@export var trend_path := "Root/HSplit/Right/TrendPanel/VBox/TagTrend"
@export var alarm_path := "Root/HSplit/Right/AlarmPanel/VBox/TagAlarmList"
@export var recipe_path := "Root/HSplit/Right/RecipePanel/VBox/TagRecipe"
@export var macro_path := "Root/HSplit/Right/MacroPanel/VBox/TagMacroButton"

## 演示 tags：与 mock 源注册名对应
const DEMO_TAGS := [
	{"tag": "Pump01.Speed", "pattern": "sine", "min": 0.0, "max": 1800.0},
	{"tag": "Tank01.Level", "pattern": "sine", "min": 0.0, "max": 100.0},
	{"tag": "Valve02.Open", "pattern": "square", "min": 0.0, "max": 1.0},
]

func _ready() -> void:
	_setup_mock_tags()
	_hook_widgets()

func _setup_mock_tags() -> void:
	var bridge: Node = get_node_or_null("/root/WidgetBridge")
	if bridge == null:
		return
	bridge.set("use_mock", true)
	bridge.call("_detect_backend")
	var mock: Node = bridge.get("_mock")
	if mock == null:
		return
	# 补注册演示 tags（mock 已含 T1/T2/T3；演示补充专用 tag 名）
	for t in DEMO_TAGS:
		mock.call("add_mock_tag", t.tag, t.pattern, t.min, t.max)

## 把元件与演示 tag 绑定（默认 tag_name 已在场景里设好，此处做幂等同步）。
func _hook_widgets() -> void:
	var binds := {
		label_path: "Pump01.Speed",
		num_display_path: "Pump01.Speed",
		lamp_path: "Valve02.Open",
		switch_path: "Valve02.Open",
		meter_path: "Tank01.Level",
		bar_path: "Tank01.Level",
		gauge_path: "Tank01.Level",
		trend_path: "Tank01.Level",
	}
	for path in binds:
		var node: Node = get_node_or_null(String(path))
		if node != null and node.has_method("_ready"):
			node.set("tag_name", binds[path])
			node.call("_ready")