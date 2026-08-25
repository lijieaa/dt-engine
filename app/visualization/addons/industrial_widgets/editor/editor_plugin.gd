@tool
extends EditorPlugin
## Industrial Widgets — 编辑器插件入口（Task 11）。
## _enter_tree 把 WidgetPalette dock 加到编辑器右栏；_exit_tree 注销。

var palette: Control

func _enter_tree() -> void:
	palette = preload("res://addons/industrial_widgets/editor/widget_palette.gd").new()
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, palette)

func _exit_tree() -> void:
	if palette != null:
		remove_control_from_docks(palette)
		palette.free()
		palette = null