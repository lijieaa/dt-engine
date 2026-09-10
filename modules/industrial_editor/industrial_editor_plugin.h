#pragma once

#include "editor/plugins/editor_plugin.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "editor/docks/editor_dock.h"

class IndustrialDeviceDock;
class IndustrialDeviceForm;
class IndustrialTagForm;
class IndustrialProject;
class IndustrialNewDeviceDialog;
class IndustrialNewTagDialog;
class IndustrialBatchGenDialog;

// EditorPlugin subclass that wires up:
// 1. Connection to EditorNode.industrial_menu_requested signal.
// 2. Registration of device and tag docks.
// 3. Menu callback dispatch to docks/dialogs.
class IndustrialEditorPlugin : public EditorPlugin {
	GDCLASS(IndustrialEditorPlugin, EditorPlugin);

public:
	IndustrialEditorPlugin();
	~IndustrialEditorPlugin() override;

	String get_plugin_name() const override { return "Industrial Editor"; }
	bool has_main_screen() const override { return false; }
	const Ref<Texture2D> get_plugin_icon() const override { return Ref<Texture2D>(); }

	// Godot 4 用 _notification(NOTIFICATION_ENTER_TREE/EXIT_TREE) 接收 enter/exit
	// 事件,基类没有 virtual _enter_tree/_exit_tree(改用 GDVIRTUAL0 hook)。
	void _notification(int p_what);

	// Called by EditorNode when device/tag menu items are clicked.
	void on_industrial_menu_requested(int p_option);

private:
	IndustrialDeviceDock *device_dock = nullptr;
	IndustrialDeviceForm *device_form = nullptr;
	IndustrialTagForm *tag_form = nullptr;
	EditorDock *device_form_dock = nullptr;
	Ref<IndustrialProject> project;

	IndustrialNewDeviceDialog *new_device_dialog = nullptr;
	IndustrialNewTagDialog *new_tag_dialog = nullptr;
	IndustrialBatchGenDialog *batch_gen_dialog = nullptr;

	static void _bind_methods();
	void _show_new_device_dialog();
	void _show_edit_device_dialog();
	void _show_new_tag_dialog();
	void _show_batch_generate_dialog();
	void _show_tag_browser();
	void _import_devices_csv();
	void _export_devices_csv();
	void _on_dialog_confirmed_refresh();
	void _on_project_changed();
	void _on_device_selected(int p_device_index);
	void _on_tag_selected(int p_device_index, int p_tag_index);
	void _on_edit_device_requested(int p_device_index);
	void _on_new_tag_requested(int p_device_index);
	void _on_edit_tag_requested(int p_device_index, int p_tag_index);
	void _show_properties_device(int p_device_index);
	void _show_properties_tag(int p_device_index, int p_tag_index);
	void _load_project_data();
	void _save_project_data();
	void _publish_project_to_runtime();
	String _get_project_data_path() const;

	// Invoked once by IndustrialRuntimeClient after the first async HTTP
	// pair (GET /api/v1/drivers + /s7/address-catalog) settles.  Refills all
	// protocol dropdowns so 81 backend drivers are visible even if the UI
	// widgets were constructed before the catalog arrived.
	void _on_runtime_metadata_ready(const Array &p_info);

	// E2E smoke test: called from NOTIFICATION_ENTER_TREE when env
	// INDUSTRIAL_E2E=1.  Seeds the empty project with 4 devices and drives
	// NewTagDialog::set_device_index 4 times so we can assert the backend
	// receives 4 distinct GET /tag-field-catalog requests (one per family,
	// one symbolic).  After 15s the process calls EditorNode::save_layout()
	// and then requests a quit so the build wrapper can exit non-blockingly.
	void _e2e_seed_and_switch_devices();
	// Callback invoked by a short SceneTree timer: drives Nth switch / quit.
	void _e2e_tick(int p_step);

	// One-shot diagnostic (env INDUSTRIAL_DIAG_PROTOCOL=1): opens
	// NewDeviceDialog after catalog settle and dumps Interface Type +
	// Device Type dropdown contents so we can diagnose "协议不见了".
	void _diag_protocol_dump();
};
