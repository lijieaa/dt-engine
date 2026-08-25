extends Button
## TagMacroButton — 宏执行按钮元件（Task 10）。
## pressed → 内部 C++ WidgetMacro.execute → step_done 收集 → 按钮文本显示
## "执行中…" → 完成后刷新 "完成 (N)" 并恢复可用。写动作经 step tag 逐条
## WidgetBridge.write_tag（宿主循环或此处同步写）。

@export var macro_cfg: Dictionary = {}
@export var idle_text: String = "执行宏"
@export var running_text: String = "执行中…"

var _macro_core: RefCounted = null
var _busy: bool = false
var _done_count: int = 0

func _ready() -> void:
	if ClassDB.class_exists("WidgetMacro"):
		_macro_core = ClassDB.instantiate("WidgetMacro")
		if _macro_core != null and _macro_core.has_method("configure") and not macro_cfg.is_empty():
			_macro_core.call("configure", macro_cfg)
		if _macro_core != null and _macro_core.has_signal("step_done"):
			_macro_core.step_done.connect(_on_step_done)
		if _macro_core != null and _macro_core.has_signal("finished"):
			_macro_core.finished.connect(_on_finished)
	text = idle_text
	var _unused := _busy  # 保持导出状态可读

func pressed_cb() -> void:
	if _busy:
		return
	if _macro_core == null or not _macro_core.has_method("execute"):
		return
	_busy = true
	_done_count = 0
	text = running_text
	_macro_core.call("execute", true)

## 兼容编辑器/场景里 pressed 信号连接（Button.pressed → pressed_cb）。
func _notification(what: int) -> void:
	if what == NOTIFICATION_READY:
		if not pressed.is_connected(pressed_cb):
			pressed.connect(pressed_cb)

func _on_step_done(idx: int, tag: String, v) -> void:
	_done_count += 1
	# 每个步骤执行一次写（写回 bridge；同步 mock 收 write_result）
	WidgetBridge.write_tag(tag, v, "macro:step%d" % idx)
	text = "%s (%d/%d)" % [running_text, _done_count, _total_steps()]

func _on_finished(ok: bool) -> void:
	_busy = false
	if ok:
		text = "完成 (%d)" % _done_count
	else:
		text = "失败"

func _total_steps() -> int:
	if _macro_core != null and _macro_core.has_method("get_steps"):
		return (_macro_core.call("get_steps") as Array).size()
	return 0