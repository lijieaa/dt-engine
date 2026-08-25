@tool
extends VBoxContainer
## WidgetPalette — 编辑器元件面板（Task 11）。
## 左列分组树：显示/输入/状态/仪表/趋势/报警/配方/宏，点击项在当前场景根放置元件。
## 供 EditorPlugin 挂入右侧 dock；测试可注入 target_root 绕开编辑环境下限。

const WIDGET_DIR := "res://addons/industrial_widgets/widgets/"

## 当前场景根（编辑器内由 _get_scene_root 解析；测试注入自定义 root）。
var target_root: Node = null

## 12 元件分类元数据：{组名: [ {name, script, desc}, ... ] }
var widget_groups := {
	"显示": [
		{"name": "TagLabel", "script": WIDGET_DIR + "tag_label.gd", "desc": "标签显示值"},
		{"name": "TagNumDisplay", "script": WIDGET_DIR + "tag_num_display.gd", "desc": "数字格式化显示"},
	],
	"输入": [
		{"name": "TagNumInput", "script": WIDGET_DIR + "tag_num_input.gd", "desc": "数字输入写回"},
	],
	"状态": [
		{"name": "TagLamp", "script": WIDGET_DIR + "tag_lamp.gd", "desc": "指示灯（闪烁可选）"},
		{"name": "TagSwitch", "script": WIDGET_DIR + "tag_switch.gd", "desc": "开关写回"},
	],
	"仪表": [
		{"name": "TagMeter", "script": WIDGET_DIR + "tag_meter.gd", "desc": "表盘（C++ 角度核心）"},
		{"name": "TagBar", "script": WIDGET_DIR + "tag_bar.gd", "desc": "进度条"},
	],
	"趋势": [
		{"name": "TagTrend", "script": WIDGET_DIR + "tag_trend.gd", "desc": "历史曲线"},
	],
	"报警": [
		{"name": "TagAlarmList", "script": WIDGET_DIR + "tag_alarm_list.gd", "desc": "报警列表"},
	],
	"配方": [
		{"name": "TagRecipe", "script": WIDGET_DIR + "tag_recipe.gd", "desc": "配方切换"},
	],
	"宏": [
		{"name": "TagMacroButton", "script": WIDGET_DIR + "tag_macro_button.gd", "desc": "宏执行按钮"},
	],
	"曲线": [
		{"name": "TagGauge", "script": WIDGET_DIR + "tag_gauge.gd", "desc": "仪表盘（圆弧）"},
	],
}

var _tree: Tree = null

func _ready() -> void:
	custom_minimum_size = Vector2(240, 0)
	var title := Label.new()
	title.text = "元件面板"
	title.add_theme_font_size_override("font_size", 15)
	add_child(title)

	_tree = Tree.new()
	_tree.custom_minimum_size = Vector2(220, 360)
	add_child(_tree)
	_tree.item_activated.connect(_on_item_activated)

	var root_item := _tree.create_item()
	root_item.set_text(0, "Industrial Widgets")
	for group_name in widget_groups.keys():
		var group_item := _tree.create_item(root_item)
		group_item.set_text(0, group_name)
		group_item.set_metadata(0, "__group__")
		for spec in widget_groups[group_name]:
			var child := _tree.create_item(group_item)
			child.set_text(0, spec.name)
			child.set_metadata(0, spec.script)

## 编辑器内解析当前场景根。
func _get_scene_root() -> Node:
	if target_root != null:
		return target_root
	if Engine.is_editor_hint() and EditorInterface.get_edited_scene_root() != null:
		return EditorInterface.get_edited_scene_root()
	return get_tree().current_scene if get_tree() != null else null

## 放置元件：加载脚本 → 实例化 → 加入目标根 → 设位置。
## 返回实例（失败返回 null）。position 为 null 时放置默认 (20,20)。
func add_widget(script_path: String, position: Vector2 = Vector2(20, 20)) -> Node:
	if not ResourceLoader.exists(script_path):
		push_error("WidgetPalette: 脚本不存在 " + script_path)
		return null
	var s: GDScript = load(script_path)
	if s == null or not s.can_instantiate():
		push_error("WidgetPalette: 无法实例化 " + script_path)
		return null
	var target := _get_scene_root()
	if target == null:
		push_error("WidgetPalette: 无目标场景根")
		return null
	var inst: Node = s.new()
	target.add_child(inst)
	if inst is Control:
		(inst as Control).position = position
	return inst

func _on_item_activated() -> void:
	if _tree == null:
		return
	var item := _tree.get_selected()
	if item == null:
		return
	var script_path: Variant = item.get_metadata(0)
	if typeof(script_path) != TYPE_STRING or String(script_path) == "__group__":
		return
	var pos := Vector2(20, 20)
	if get_viewport() != null and get_viewport().get_mouse_position() != Vector2.ZERO:
		pos = get_viewport().get_mouse_position()
	add_widget(String(script_path), pos)