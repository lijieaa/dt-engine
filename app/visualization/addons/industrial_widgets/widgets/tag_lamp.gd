extends Panel
## TagLamp — 指示灯元件（Task 8）。
## tag 值非 0 → 亮 on_color；0 → 灭 off_color。可选闪烁动效（GDScript UI 壳计时）。
## 核心状态真值判断保持简单（本地）；如后续需要复杂报警时序，可迁移 C++ WidgetAlarm。

@export var tag_name: String = ""
@export var on_color: Color = Color(0.95, 0.15, 0.15)   ## EBPro 默认报警红
@export var off_color: Color = Color(0.15, 0.15, 0.18)
@export var base_bg: Color = Color(0.07, 0.08, 0.1)
## 闪烁配置：{enabled: bool, interval_sec: float}
@export var blink_cfg: Dictionary = {"enabled": false, "interval_sec": 0.5}

## 当前灯态（供测试/外部读取）
var lit: bool = false
var _blink_phase := 0.0
var _target_color: Color = Color()

func _ready() -> void:
	custom_minimum_size = Vector2(28, 28)
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)
	_refresh()

func _process(delta: float) -> void:
	if not lit:
		return
	var enabled: bool = bool(blink_cfg.get("enabled", false))
	if not enabled:
		return
	_blink_phase += delta
	var interval: float = float(blink_cfg.get("interval_sec", 0.5))
	if interval <= 0.0:
		interval = 0.5
	if _blink_phase >= interval:
		_blink_phase = 0.0
		_toggle_blink()

func _on_tag_changed(tag: String, v, _quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name:
		return
	apply_external_value(v)

func apply_external_value(v) -> void:
	var on: bool = false
	match typeof(v):
		TYPE_BOOL: on = bool(v)
		TYPE_INT: on = int(v) != 0
		TYPE_FLOAT: on = absf(float(v)) > 0.0001
		TYPE_STRING: on = not (String(v) in ["0", "", "false", "False", "OFF", "off"])
	lit = on
	_refresh()

func _refresh() -> void:
	if lit:
		_target_color = on_color
		add_theme_color_override("panel", on_color)
		_blink_phase = 0.0
	else:
		_target_color = off_color
		add_theme_color_override("panel", off_color)

func _toggle_blink() -> void:
	if lit:
		var cur := get_theme_color("panel")
		add_theme_color_override("panel", base_bg if cur == on_color else on_color)