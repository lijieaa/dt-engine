extends ItemList
## TagAlarmList — 报警列表元件（Task 10）。
## 监控一个或多个 tag；值超过阈值（comparator 规则）→ 内部 C++ WidgetAlarm 规则
## active → 该项上色（红）指示。调 evaluate 驱动规则求值（含去抖）。

## 注意：用普通 Array（非 Array[String]）——动态配置/测试用 Object.set() 赋值时
## typed array 会类型转换失败，规则注册会静默丢失。
@export var monitored_tags: Array = []
@export var threshold: float = 100.0
@export var comparator: String = "gt"        # gt/lt/ge/le/eq/ne（映射到 C++ WidgetAlarm 的 "type" 键）
@export var debounce_ms: int = 0
@export var active_color: Color = Color(0.95, 0.2, 0.2)
@export var normal_color: Color = Color(0.85, 0.87, 0.92)

var _alarm_core: RefCounted = null
var _active_count: int = 0

func _ready() -> void:
	clear()
	if ClassDB.class_exists("WidgetAlarm"):
		_alarm_core = ClassDB.instantiate("WidgetAlarm")
		for t in monitored_tags:
			if _alarm_core != null and _alarm_core.has_method("add_rule"):
				# 注意：C++ WidgetAlarm 规则字典的类型键是 "type"（非 "comparator"）
				_alarm_core.call("add_rule", t, {
					"type": comparator,
					"threshold": threshold,
					"debounce_ms": debounce_ms,
				})
	if monitored_tags.is_empty():
		return
	WidgetBridge.subscribe(monitored_tags)
	WidgetBridge.tag_changed.connect(_on_tag_changed)
	_refresh_rows()

func _on_tag_changed(tag: String, v, _quality: String, _ver: int, _ts: int) -> void:
	if not monitored_tags.has(tag):
		return
	if _alarm_core != null and _alarm_core.has_method("evaluate"):
		_alarm_core.call("evaluate", tag, v)
	_refresh_rows()

## 用核心 active 规则驱动 UI 行态。
func _refresh_rows() -> void:
	clear()
	var active_rules: Array = []
	if _alarm_core != null and _alarm_core.has_method("get_active_rules"):
		active_rules = _alarm_core.call("get_active_rules")
	for t in monitored_tags:
		var idx := add_item(t)
		if active_rules.has(t):
			set_item_custom_fg_color(idx, active_color)
		else:
			set_item_custom_fg_color(idx, normal_color)
	_active_count = active_rules.size()