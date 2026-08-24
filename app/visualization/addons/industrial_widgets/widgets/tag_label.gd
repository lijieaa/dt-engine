extends Label
## TagLabel — Tag display label. Ported from scripts/ui/TagLabel.gd — Runtime → WidgetBridge.

@export var tag_name: String = ""
@export var base_color: Color = Color(1, 1, 1, 1)
@export var good_color: Color = Color(0.0, 0.85, 0.45, 1)
@export var uncertain_color: Color = Color(1.0, 0.73, 0.12, 1)
@export var bad_color: Color = Color(0.95, 0.2, 0.2, 1)
@export var stale_color: Color = Color(0.65, 0.65, 0.65, 1)

func _ready() -> void:
	if text == "":
		text = "—"
	theme_type_variation = "HeaderMedium"
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)

func _on_tag_changed(tag: String, v, quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name:
		return
	text = str(v)
	set_quality(quality)

func set_quality(q: String) -> void:
	match q:
		"good":    add_theme_color_override("font_color", good_color)
		"uncertain": add_theme_color_override("font_color", uncertain_color)
		"bad":     add_theme_color_override("font_color", bad_color)
		"stale":   add_theme_color_override("font_color", stale_color)
		_:         add_theme_color_override("font_color", base_color)

func flash_written() -> void:
	var original := get_theme_color("font_color")
	add_theme_color_override("font_color", Color(0.2, 0.95, 1.0, 1.0))
	await get_tree().create_timer(0.12).timeout
	add_theme_color_override("font_color", original)
