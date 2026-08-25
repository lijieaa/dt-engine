extends LineEdit
## TagNumInput — 数字输入元件（Task 8）。
## 提交文本 → C++ WidgetFormat.parse_input 校验/解析 → 合法则 WidgetBridge.write_tag 写回。
## 依赖 C++ WidgetFormat；无原生类时退化为直接写原始文本。

@export var tag_name: String = ""
## 解析配置（透传给 WidgetFormat.parse_input）：
##   base: int      进制（2/8/10/16，默认 10）
##   allow_decimals: bool
##   allow_negative: bool
@export var format_cfg: Dictionary = {}

func _ready() -> void:
	if placeholder_text == "":
		placeholder_text = "输入数值…"
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)
		text_submitted.connect(_on_submitted)

func _on_tag_changed(tag: String, v, _quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name:
		return
	# 外部值变化时刷新显示（不回写，避免环）
	text = str(v)

## 提交：先经 C++ 校验解析，合法才写回；失败置错误提示色。
func _on_submitted(new_text: String) -> void:
	var parsed := validate_input(new_text)
	if parsed.ok:
		WidgetBridge.write_tag(tag_name, parsed.value)
		_revert_error_style()
	else:
		_flash_error(parsed.error)

## 调用 C++ WidgetFormat.parse_input；无原生类时做简易校验。
func validate_input(text_in: String) -> Dictionary:
	var core: RefCounted = _native_format()
	if core != null and core.has_method("parse_input"):
		var r: Dictionary = core.call("parse_input", text_in, format_cfg)
		if r.has("ok"):
			return r
	# 兜底：仅拒绝空串
	var s := text_in.strip_edges()
	if s.is_empty():
		return {"ok": false, "value": null, "error": "输入为空"}
	return {"ok": true, "value": s}

func _native_format() -> RefCounted:
	if ClassDB.class_exists("WidgetFormat"):
		return ClassDB.instantiate("WidgetFormat")
	return null

func _flash_error(msg: String) -> void:
	add_theme_color_override("font_color", Color(0.95, 0.2, 0.2))
	tooltip_text = msg
	await get_tree().create_timer(1.0).timeout
	_revert_error_style()

func _revert_error_style() -> void:
	add_theme_color_override("font_color", Color(1, 1, 1))
	tooltip_text = ""