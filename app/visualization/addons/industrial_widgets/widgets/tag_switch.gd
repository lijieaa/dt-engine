extends CheckBox
## TagSwitch — On/off toggle. Ported from scripts/ui/TagSwitch.gd — Runtime → WidgetBridge.

@export var tag_name: String = ""
@export var on_color: Color = Color(0.1, 0.9, 0.5, 1.0)
@export var off_color: Color = Color(0.6, 0.6, 0.65, 1.0)

func _ready() -> void:
	if tag_name != "":
		text = "Write " + tag_name
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)
	toggled.connect(_on_toggled)
	_refresh_tint()

func _on_tag_changed(tag: String, v, _quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name:
		return
	apply_external_value(v)

func _on_toggled(p: bool) -> void:
	_refresh_tint()
	if tag_name != "":
		WidgetBridge.write_tag(tag_name, p)

func apply_external_value(v) -> void:
	var on: bool = false
	match typeof(v):
		TYPE_BOOL: on = bool(v)
		TYPE_INT: on = int(v) != 0
		TYPE_FLOAT: on = float(v) != 0.0
		TYPE_STRING: on = not (String(v) in ["0", "", "false", "False", "OFF", "off"])
	set_pressed_no_signal(on)
	_refresh_tint()

func _refresh_tint() -> void:
	var c: Color = on_color if is_pressed() else off_color
	add_theme_color_override("font_color", c)
