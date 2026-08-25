extends Label
## TagNumDisplay — 数字显示元件（Task 8）。
## 经 WidgetBridge 订阅 tag，tag 变化时用 C++ WidgetFormat.format_value 格式化刷新文本。
## 依赖 C++ WidgetFormat（modules/industrial_editor/widget_format.*）；无原生类时退化为 str(v)。

@export var tag_name: String = ""
## 格式化配置（透传给 WidgetFormat.format_value）：
##   thousands: bool   千分位
##   decimals: int     小数位
##   prefix/suffix: String
##   base: int         进制（2/8/10/16，默认 10）
##   bcd: bool         BCD 解码（默认 false）
@export var format_cfg: Dictionary = {}

func _ready() -> void:
	if text == "":
		text = "—"
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)

func _on_tag_changed(tag: String, v, quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name:
		return
	text = format_value(v)
	_tint_by_quality(quality)

## 用 C++ WidgetFormat 格式化；无原生类时退化显示原始值。
func format_value(v: Variant) -> String:
	var core: RefCounted = _native_format()
	if core != null and core.has_method("format_value"):
		return String(core.call("format_value", v, format_cfg))
	return str(v)

func _native_format() -> RefCounted:
	# 优先复用 WidgetBridge 记录的原生类清单（native_module 档）；否则实时探测。
	var bridge := WidgetBridge
	if bridge.has_method("has_native") and bridge.call("has_native"):
		var nc: Array = bridge.get("native_classes") if (
			bridge.get("native_classes") != null and not bridge.get("native_classes").is_empty()
		) else []
		if "WidgetFormat" in nc:
			return ClassDB.instantiate("WidgetFormat")
	if ClassDB.class_exists("WidgetFormat"):
		return ClassDB.instantiate("WidgetFormat")
	return null

func _tint_by_quality(q: String) -> void:
	match q:
		"good":      add_theme_color_override("font_color", Color(0.9, 0.95, 1.0))
		"uncertain": add_theme_color_override("font_color", Color(1.0, 0.73, 0.12))
		"bad":       add_theme_color_override("font_color", Color(0.95, 0.2, 0.2))
		"stale":     add_theme_color_override("font_color", Color(0.6, 0.6, 0.6))
		_:           add_theme_color_override("font_color", Color(1, 1, 1))