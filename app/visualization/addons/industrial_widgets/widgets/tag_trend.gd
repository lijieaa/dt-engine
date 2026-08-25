extends Control
## TagTrend — 趋势曲线元件（Task 10）。
## 每收到 tag_changed → 内部 C++ WidgetTrend.push_sample 累积；_draw() 用
## query_window 拉取窗口数据画折线。C++ WidgetTrend 不可用时退化为本地数组。

@export var tag_name: String = ""
@export var buffer_capacity: int = 4096
@export var line_color: Color = Color(0.25, 0.65, 0.98)
@export var grid_color: Color = Color(0.16, 0.18, 0.22)
@export var window_ms: int = 60000  # 显示最近 60 秒

var _trend_core: RefCounted = null
var _fallback: Array[Array] = []     # [[ts, v], ...] 无原生类时的本地缓冲
var _render_pts: int = 0

func _ready() -> void:
	custom_minimum_size = Vector2(220, 120)
	if ClassDB.class_exists("WidgetTrend"):
		_trend_core = ClassDB.instantiate("WidgetTrend")
		if _trend_core != null and _trend_core.has_method("resize"):
			_trend_core.call("resize", buffer_capacity)
	if tag_name != "":
		WidgetBridge.subscribe([tag_name])
		WidgetBridge.tag_changed.connect(_on_tag_changed)
	queue_redraw()

func _on_tag_changed(tag: String, v, _quality: String, _ver: int, ts_ms: int) -> void:
	if tag != tag_name:
		return
	var f: float = float(v) if typeof(v) in [TYPE_INT, TYPE_FLOAT] else float(String(v).to_float())
	var ts: int = ts_ms if ts_ms > 0 else Time.get_ticks_msec()
	if _trend_core != null:
		_trend_core.call("push_sample", ts, f)
	else:
		_fallback.append([ts, f])
		if _fallback.size() > buffer_capacity:
			_fallback.pop_front()
	_cache_render_points()
	queue_redraw()

func _cache_render_points() -> void:
	if _trend_core != null and _trend_core.has_method("query_window"):
		var from_ms: int = Time.get_ticks_msec() - window_ms
		var w: Array = _trend_core.call("query_window", from_ms, 1 << 62)
		_render_pts = w.size()
		_render_points = w
	else:
		_render_pts = _fallback.size()
		_render_points = _fallback

var _render_points: Array = []

func _draw() -> void:
	var r := Rect2(Vector2.ZERO, size)
	# 背景
	draw_rect(r, Color(0.05, 0.06, 0.08))
	# 网格（5 横线）
	for i in range(6):
		var y: float = size.y * float(i) / 5.0
		draw_line(Vector2(0, y), Vector2(size.x, y), grid_color, 1.0)
	if _render_points.size() < 2:
		return
	# 值域估计（用本地 min/max）
	var lo := INF
	var hi := -INF
	for p in _render_points:
		var v: float = float(p[1])
		lo = minf(lo, v)
		hi = maxf(hi, v)
	if hi - lo < 0.001:
		hi = lo + 1.0
	var ts0: int = int(_render_points[0][0])
	var ts1: int = int(_render_points[_render_points.size() - 1][0])
	if ts1 <= ts0:
		ts1 = ts0 + 1
	var prev: Vector2 = Vector2.ZERO
	var first := true
	for p in _render_points:
		var t: int = int(p[0])
		var v: float = float(p[1])
		var x: float = (float(t - ts0) / float(ts1 - ts0)) * size.x
		var y: float = size.y - (v - lo) / (hi - lo) * (size.y - 4.0) - 2.0
		var pt := Vector2(x, y)
		if not first:
			draw_line(prev, pt, line_color, 1.5)
		prev = pt
		first = false