extends ProgressBar
## TagGauge — Progress gauge. Ported from scripts/ui/TagGauge.gd — Runtime → WidgetBridge.

@export var tag_name: String = ""
@export var range_min: float = 0.0
@export var range_max: float = 100.0
@export var low_warn_pct: float = 15.0
@export var high_warn_pct: float = 85.0

func _ready() -> void:
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)

func _on_tag_changed(tag: String, v, quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name:
		return
	_set_raw(v)
	tooltip_text = "%s = %s [%s]" % [tag_name, str(v), quality]

func _set_raw(v) -> void:
	if typeof(v) == TYPE_INT or typeof(v) == TYPE_FLOAT:
		var fv: float = float(v)
		var span: float = max(0.0001, range_max - range_min)
		var pct: float = clamp((fv - range_min) / span, 0.0, 1.0) * 100.0
		value = pct
		_tint(pct)
		tooltip_text = "%s / [%s, %s]" % [str(fv), str(range_min), str(range_max)]

func _tint(pct: float) -> void:
	var theme_stylebox: StyleBoxFlat = StyleBoxFlat.new()
	if pct < low_warn_pct:
		theme_stylebox.bg_color = Color(1.0, 0.7, 0.1, 0.9)
	elif pct > high_warn_pct:
		theme_stylebox.bg_color = Color(0.98, 0.2, 0.25, 0.9)
	else:
		theme_stylebox.bg_color = Color(0.18, 0.78, 0.95, 0.95)
	theme_stylebox.corner_radius_top_left = 8
	theme_stylebox.corner_radius_top_right = 8
	theme_stylebox.corner_radius_bottom_left = 8
	theme_stylebox.corner_radius_bottom_right = 8
	add_theme_stylebox_override("fill", theme_stylebox)
