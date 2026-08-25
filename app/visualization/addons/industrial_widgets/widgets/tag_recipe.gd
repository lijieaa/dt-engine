extends PanelContainer
## TagRecipe — 配方元件（Task 10）。
## 配置配方字典 → 内部 C++ WidgetRecipe 解析；切换配方 → build_write_sequence →
## 批量 WidgetBridge.write_tag（写回 mock/真实后端）。

@export var recipe_cfg: Dictionary = {}
@export var entry_hint: String = "R1"   # 默认激活配方（测试/编辑器可见）

var _recipe_core: RefCounted = null
var _option_button: OptionButton = null

func _ready() -> void:
	if ClassDB.class_exists("WidgetRecipe"):
		_recipe_core = ClassDB.instantiate("WidgetRecipe")
		if _recipe_core != null and _recipe_core.has_method("configure") and not recipe_cfg.is_empty():
			_recipe_core.call("configure", recipe_cfg)
	_build_ui()

func _build_ui() -> void:
	var vb := VBoxContainer.new()
	add_child(vb)
	_option_button = OptionButton.new()
	vb.add_child(_option_button)
	if _recipe_core != null:
		var entries: Array = _recipe_core.call("list_entries")
		for e in entries:
			_option_button.add_item(String(e))
		_option_button.item_selected.connect(_on_selected)
		if not entries.is_empty():
			_option_button.select(0)

## 供外部/测试：取某配方写序列（不经 UI）。
func build_sequence(entry_name: String) -> Array:
	if _recipe_core == null or not _recipe_core.has_method("build_write_sequence"):
		return []
	return _recipe_core.call("build_write_sequence", entry_name)

## 应用配方：build_write_sequence → 批量 write_tag。
func apply_recipe(entry_name: String) -> void:
	var seq: Array = build_sequence(entry_name)
	for step in seq:
		var d: Dictionary = step
		WidgetBridge.write_tag(String(d.tag), d.value, "recipe:" + entry_name)

func _on_selected(idx: int) -> void:
	var entries: Array = []
	if _recipe_core != null:
		entries = _recipe_core.call("list_entries")
	if idx >= 0 and idx < entries.size():
		apply_recipe(String(entries[idx]))