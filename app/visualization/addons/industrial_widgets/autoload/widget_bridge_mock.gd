extends Node
## WidgetBridgeMock — 内置模拟数据源（Task 1）。
##
## 职责：
##   * 定时生成正弦/方波/随机值，经 `tag_changed` emit（模拟后端 tag_update）。
##   * 暴露 `push(tag, v, q)` 供单测确定性驱动（不依赖真实 timer）。
##   * 收到写请求后请求 bridge 发 `write_result` 回执（协议对齐）。
##   * 演示 tag：T1 正弦 / T2 方波 / T3 随机（pattern: sine|square|random）。

signal tag_changed(tag: String, value, quality: String, version: int, ts_ms: int)

const DEFAULT_TAGS := [
	{"tag": "T1", "pattern": "sine", "min": 0.0, "max": 100.0},
	{"tag": "T2", "pattern": "square", "min": 0.0, "max": 1.0},
	{"tag": "T3", "pattern": "random", "min": 0.0, "max": 100.0},
]

## 模拟 tick 间隔（秒）；0 表示关闭自动 tick（仅 push 驱动）。
var tick_interval := 0.5
var quality: String = "good"

var bridge: Node = null
var _tags: Array = []
var _tick_acc := 0.0
var _time := 0.0
var _version := 0

func _ready() -> void:
	for t in DEFAULT_TAGS:
		add_mock_tag(t.tag, t.pattern, t.min, t.max)

func _process(delta: float) -> void:
	_time += delta
	if tick_interval <= 0.0:
		return
	_tick_acc += delta
	if _tick_acc >= tick_interval:
		_tick_acc = 0.0
		_tick()

## 注册一个模拟 tag。pattern：sine / square / random。
func add_mock_tag(name: String, pattern: String, min_val := 0.0, max_val := 1.0) -> void:
	_tags.append({
		"name": name, "pattern": pattern,
		"min": float(min_val), "max": float(max_val),
	})

## 确定性推值：emit tag_changed，供单测使用（无 timer 依赖）。
func push(tag: String, v, q: String = "good") -> void:
	_version += 1
	var ts := Time.get_ticks_msec()
	tag_changed.emit(tag, v, q, _version, ts)

## 收到 bridge 的写请求 → 立即 emit write_result 回执。
func submit_write(tag: String, value, rid: String) -> bool:
	if bridge != null and bridge.has_method("_on_mock_write_request"):
		bridge._on_mock_write_request(tag, value, rid)
	else:
		push_warning("WidgetBridgeMock: bridge 未绑定，写回执跳过")
	return true

func has_mock_tag(name: String) -> bool:
	for t in _tags:
		if t.name == name:
			return true
	return false

# ---------- 内部 ----------

func _tick() -> void:
	for t in _tags:
		var v := _value_for(t)
		push(t.name, v, quality)

func _value_for(t: Dictionary) -> Variant:
	var lo := float(t.min)
	var hi := float(t.max)
	var mid := (lo + hi) / 2.0
	match String(t.pattern):
		"sine":
			var amp := (hi - lo) / 2.0
			return mid + amp * sin(_time * 2.0)
		"square":
			return hi if fmod(_time, 1.0) < 0.5 else lo
		"random":
			return randf_range(lo, hi)
	return mid