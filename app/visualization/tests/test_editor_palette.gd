extends SceneTree
## 编辑器元件面板测试（Task 11）。
## WidgetPalette 的 add_widget 在当前场景根添加元件实例；editor_plugin 的注册
## 走编辑器生命周期，headless 下验证脚本可加载 + 类正确 + palette 内建分类表。
## headless: godot --headless --path app/visualization --script res://tests/test_editor_palette.gd

var _failures: Array[String] = []
var _checks := 0

func _initialize() -> void:
	print("=== test_editor_palette ===")
	await test_palette_script_loads()
	await test_add_widget_creates_instance()
	await test_palette_metadata()
	if _failures.is_empty():
		print("RESULT: PASS — %d checks, 0 failure(s)" % _checks)
		quit(0)
		return
	print("RESULT: FAIL — %d checks, %d failure(s)" % [_checks, _failures.size()])
	for f in _failures:
		print("  FAIL: ", f)
	quit(1)

## 用例1：editor_plugin.gd / widget_palette.gd 可加载，继承关系正确。
func test_palette_script_loads() -> void:
	print("[test] palette_script_loads")
	var ep_path := "res://addons/industrial_widgets/editor/editor_plugin.gd"
	var wp_path := "res://addons/industrial_widgets/editor/widget_palette.gd"
	var ep: GDScript = load(ep_path) if ResourceLoader.exists(ep_path) else null
	_check(ep != null, "editor_plugin.gd 存在且可加载")
	if ep != null:
		_check(ep.can_instantiate(), "editor_plugin.gd 可实例化")
		# 继承关系：EditorPlugin 基类名
		_check(String(ep.get_instance_base_type()) == "EditorPlugin", "基类 == EditorPlugin (got %s)" % ep.get_instance_base_type())
	var wp: GDScript = load(wp_path) if ResourceLoader.exists(wp_path) else null
	_check(wp != null, "widget_palette.gd 存在且可加载")
	if wp != null:
		_check(String(wp.get_instance_base_type()) == "VBoxContainer", "基类 == VBoxContainer (got %s)" % wp.get_instance_base_type())

## 用例2：palette.add_widget(path) 在当前场景根创建元件实例（注入 target_root 绕开编辑器）。
func test_add_widget_creates_instance() -> void:
	print("[test] add_widget_creates_instance")
	var wp_path := "res://addons/industrial_widgets/editor/widget_palette.gd"
	if not ResourceLoader.exists(wp_path):
		_check(false, "widget_palette.gd 存在（前用例已验）")
		return
	var palette: Control = (load(wp_path) as GDScript).new()
	root.add_child(palette)
	# headless 非编辑器：注入目标根
	palette.set("target_root", root)
	_check(palette.has_method("add_widget"), "add_widget 方法存在")
	# 放置 TagLabel
	var inst_label: Node = palette.call("add_widget", "res://addons/industrial_widgets/widgets/tag_label.gd", Vector2(10, 20))
	_check(inst_label != null, "add_widget(TagLabel) 返回实例")
	if inst_label != null:
		_check(inst_label is Label, "实例是 Label")
		# headless --script 模式下 is_inside_tree() 恒 false（root 特殊性），
		# 用 parent 关系验证已挂载（add_child 已同步生效）
		_check(inst_label.get_parent() == root, "实例已挂到目标根")
		if inst_label is Control:
			_check((inst_label as Control).position == Vector2(10, 20), "实例位置 == 目标位置")
	# 放置 TagLamp
	var inst_lamp: Node = palette.call("add_widget", "res://addons/industrial_widgets/widgets/tag_lamp.gd", Vector2(40, 50))
	_check(inst_lamp != null and inst_lamp is Panel, "add_widget(TagLamp) 返回 Panel 实例")
	# 无效路径
	var bad: Node = palette.call("add_widget", "res://addons/industrial_widgets/widgets/nope.gd")
	_check(bad == null, "无效路径 add_widget 返回 null")
	palette.queue_free()

## 用例3：palette 内建 12 元件分类表完整（显示/输入/状态/仪表/趋势/报警/配方/宏）。
func test_palette_metadata() -> void:
	print("[test] palette_metadata")
	var wp_path := "res://addons/industrial_widgets/editor/widget_palette.gd"
	if not ResourceLoader.exists(wp_path):
		return
	var palette: Control = (load(wp_path) as GDScript).new()
	root.add_child(palette)
	var groups: Dictionary = palette.get("widget_groups")
	_check(groups.size() >= 4, "widget_groups 至少 4 组 (got %d)" % groups.size())
	# 统计全部元件数量（应覆盖 12 个）
	var total := 0
	for k in groups:
		total += (groups[k] as Array).size()
	_check(total == 12, "12 个元件已登记 (got %d)" % total)
	# 关键元件存在
	var flat: Array = []
	for k in groups:
		flat.append_array(groups[k])
	var names: Array = []
	for spec in flat:
		names.append(String(spec.get("name", "")))
	_check(names.has("TagNumDisplay"), "含 TagNumDisplay")
	_check(names.has("TagMeter"), "含 TagMeter")
	_check(names.has("TagTrend"), "含 TagTrend")
	_check(names.has("TagAlarmList"), "含 TagAlarmList")
	_check(names.has("TagRecipe"), "含 TagRecipe")
	_check(names.has("TagMacroButton"), "含 TagMacroButton")
	palette.queue_free()

func _check(cond: bool, what: String) -> void:
	_checks += 1
	if cond:
		print("  PASS: ", what)
	else:
		_failures.append(what)
		print("  FAIL: ", what)