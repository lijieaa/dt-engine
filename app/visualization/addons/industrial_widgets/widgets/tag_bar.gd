extends HSlider
## TagBar — Horizontal status bar. Ported from scripts/ui/TagBar.gd — Runtime → WidgetBridge.

@export var tag_name: String = ""
@export var bar_tint: Color = Color(0.25, 0.75, 1.0, 1.0)
@export var warn_tint: Color = Color(1.0, 0.7, 0.1, 1.0)
@export var danger_tint: Color = Color(0.97, 0.22, 0.22, 1.0)
@export var warn_pct: float = 80.0
@export var danger_pct: float = 95.0

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)

func _on_tag_changed(tag: String, v, quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name:
		return
	set_value_bound(v)
	tooltip_text = "%s = %s [%s]" % [tag_name, str(v), quality]

func set_value_bound(v) -> void:
	if typeof(v) in [TYPE_INT, TYPE_FLOAT]:
		var f: float = clamp(float(v), min_value, max_value)
		value = f
		var pct: float = 0.0 if max_value == min_value else (f - min_value) / (max_value - min_value) * 100.0
		_apply_fill(pct)

func _apply_fill(pct: float) -> void:
	var s: StyleBoxFlat = StyleBoxFlat.new()
	s.bg_color = bar_tint if pct < warn_pct else (warn_tint if pct < danger_pct else danger_tint)
	s.corner_radius_top_left = 6
	s.corner_radius_top_right = 6
	s.corner_radius_bottom_left = 6
	s.corner_radius_bottom_right = 6
	add_theme_stylebox_override("slider", s)
