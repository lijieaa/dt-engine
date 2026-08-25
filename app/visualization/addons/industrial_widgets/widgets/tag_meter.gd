extends Control
## TagMeter — 仪表盘元件（Task 8）。
## 经 WidgetBridge 订阅 tag；_draw() 自绘刻度弧 + 指针，角度由 C++
## WidgetMeter::angle_for_value 线性映射（EBPro 240° 表盘）。
## C++ WidgetMeter 不可用时退化为本地近似映射。

@export var tag_name: String = ""
@export var min: float = 0.0
@export var max: float = 100.0
@export var red_zone: float = 80.0          ## 超过该值进入红色区
@export var major_ticks: int = 5            ## 大刻度数
@export var minor_ticks_per_major: int = 4  ## 每个大刻度内的小刻度数
@export var arc_color: Color = Color(0.25, 0.55, 0.95)
@export var red_color: Color = Color(0.9, 0.2, 0.2)
@export var needle_color: Color = Color(0.95, 0.95, 0.98)
@export var face_color: Color = Color(0.09, 0.1, 0.13)
@export var tick_color: Color = Color(0.8, 0.82, 0.88)

## 当前显示值（供测试读取）
var current_value: float = 0.0
## 最近一次计算出的指针角度（度，-120..+120）
var value_angle: float = -120.0
var _quality: String = "stale"

const DEFAULT_ANGLE_START := -120.0
const DEFAULT_ANGLE_END := 120.0

func _ready() -> void:
	custom_minimum_size = Vector2(140, 140)
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)
	queue_redraw()

func _on_tag_changed(tag: String, v, quality: String, _ver: int, _ts: int) -> void:
	if tag != tag_name:
		return
	apply_external_value(v)
	_quality = quality

func apply_external_value(v) -> void:
	var f: float = 0.0
	match typeof(v):
		TYPE_INT: f = float(int(v))
		TYPE_FLOAT: f = float(v)
		TYPE_STRING: f = float(String(v).to_float())
	current_value = f
	value_angle = angle_for_value(f, min, max)
	queue_redraw()

## 调 C++ WidgetMeter::angle_for_value（线性映射）；原生类缺失时本地近似。
func angle_for_value(v: float, v_min: float, v_max: float, cfg: Dictionary = {}) -> float:
	if ClassDB.class_exists("WidgetMeter"):
		var core: RefCounted = ClassDB.instantiate("WidgetMeter")
		if core != null and core.has_method("angle_for_value"):
			return float(core.call("angle_for_value", v, v_min, v_max, cfg))
	return DEFAULT_ANGLE_START + (clampf(v, v_min, v_max) - v_min) / (v_max - v_min) * (DEFAULT_ANGLE_END - DEFAULT_ANGLE_START)

func _draw() -> void:
	var c := size / 2.0
	var radius: float = min(size.x, size.y) * 0.42
	# 表盘
	draw_circle(c, radius, face_color)
	draw_arc(c, radius, deg_to_rad(DEFAULT_ANGLE_START - 90.0), deg_to_rad(DEFAULT_ANGLE_END - 90.0), 96, arc_color, 2.5, true)
	_draw_ticks(c, radius)
	_draw_needle(c, radius)
	# 红区弧
	var red_ratio := clampf((red_zone - min) / (max - min), 0.0, 1.0)
	var red_start := DEFAULT_ANGLE_START + red_ratio * (DEFAULT_ANGLE_END - DEFAULT_ANGLE_START)
	draw_arc(c, radius * 0.92, deg_to_rad(red_start - 90.0), deg_to_rad(DEFAULT_ANGLE_END - 90.0), 48, red_color, 3.0, true)

func _draw_ticks(c: Vector2, radius: float) -> void:
	for i in range(major_ticks + 1):
		var t: float = float(i) / float(major_ticks)
		var ang := deg_to_rad(DEFAULT_ANGLE_START + t * (DEFAULT_ANGLE_END - DEFAULT_ANGLE_START) - 90.0)
		var p_in := c + Vector2(cos(ang), sin(ang)) * radius * 0.86
		var p_out := c + Vector2(cos(ang), sin(ang)) * radius * 0.97
		draw_line(p_in, p_out, tick_color, 2.0)

func _draw_needle(c: Vector2, radius: float) -> void:
	var ang := deg_to_rad(value_angle - 90.0)
	var tip := c + Vector2(cos(ang), sin(ang)) * radius * 0.78
	draw_line(c, tip, needle_color, 2.0)
	draw_circle(c, 5.0, needle_color)