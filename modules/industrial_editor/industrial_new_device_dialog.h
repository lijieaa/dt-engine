#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/button.h"
#include "scene/gui/box_container.h"
#include "scene/gui/separator.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "scene/gui/control.h"
#include "core/templates/hash_map.h"

class IndustrialProject;

// Single-page form for creating a device with optional initial tags.
// Layout (matches EBPro creation flow, see docs/specs 2026-08-20 §5):
//   ① Basic Info         (Name, Description, Driver/Protocol)
//   ② Connection Params  (dynamic per driver schema)
//   ③ Initial Tags       (editable rows with a Skip checkbox)
// Notes on omitted fields (per user request against EBPro UI):
//   - "Scan Group"  : an orchestration concept, assigned later in Device Dock.
//   - "Enabled after creation" : EBPro +0xc44 defaults to ENABLED for new devices;
//     toggling is done afterwards in the device list.
class IndustrialNewDeviceDialog : public AcceptDialog {
	GDCLASS(IndustrialNewDeviceDialog, AcceptDialog);

public:
	IndustrialNewDeviceDialog();
	~IndustrialNewDeviceDialog() override;

	void set_project(Ref<IndustrialProject> p_project);
	int get_created_device_index() const { return created_index; }

	// Re-populate Interface Type + Device Type dropdowns from the latest
	// runtime catalog.  Called (1) on open via set_project(), (2) from the
	// plugin when the async HTTP /api/v1/drivers fetch settles (fixes the
	// "协议改不见了" symptom where ctor-time populate only saw legacy 7).
	void refresh_protocol_dropdowns();

private:
	Ref<IndustrialProject> project;
	int created_index = -1;

	// ── ① Basic Info ──
	VBoxContainer *basic_section = nullptr;
	LineEdit *dev_name = nullptr;
	LineEdit *dev_desc = nullptr;
	OptionButton *dev_interface_type = nullptr;  // 一级：接口大类（Ethernet/Serial/Bus/Industry/Special）
	OptionButton *dev_device_type   = nullptr;  // 二级：按一级过滤的驱动条目（含 metadata=driver_idx）

	// ── §A–§D Connection groups (EBPro-aligned) ──
	VBoxContainer *params_section = nullptr;
	VBoxContainer *common_params_container = nullptr;
	VBoxContainer *interface_params_container = nullptr;
	VBoxContainer *protocol_params_container = nullptr;
	VBoxContainer *tuning_params_container = nullptr;
	HashMap<String, Control *> param_widgets;
	Control *remote_hmi_row = nullptr;

	// ── ③ Initial Tags ──
	VBoxContainer *tags_section = nullptr;
	CheckButton *skip_tags = nullptr;
	VBoxContainer *tag_rows_container = nullptr;
	Button *add_tag_btn = nullptr;

	Array temp_tags;

	// 当前选中的设备类型对应的 driver_key（联动：地址类型下拉框随它变化）。
	String current_driver_key;

	void _build_ui();
	void _populate_interface_types();
	void _populate_device_types(int p_group_idx);
	int  _interface_to_group(const String &p_iface) const;
	String _interface_label(const String &p_iface) const;
	String _addressing_label(const String &p_mode) const;
	String _resolve_driver_key(int p_driver_idx) const;
	void _rebuild_params(int p_driver_idx);
	void _update_remote_hmi_visibility();
	bool _should_show_interface_field(const String &p_key, int p_iface_group) const;
	void _on_location_mode_changed(int p_idx);
	void _on_interface_changed(int p_idx);
	void _on_device_type_changed(int p_idx);
	void _on_tagfield_fetched(bool p_success);
	void _collect_params(Dictionary &r_params);
	void _on_confirm_pressed();
	void _on_skip_tags_toggled(bool p_pressed);

	void _add_temp_tag();
	void _remove_temp_tag(int p_idx);
	void _populate_tag_rows();

	void _on_temp_tag_addr_value_changed(int p_idx, const String &p_text);
	void _on_temp_tag_addr_type_changed(int p_idx, int p_type_idx);
	void _on_temp_tag_name_changed(int p_idx, const String &p_text);
	void _on_temp_tag_type_changed(int p_idx, int p_type_idx);
	void _on_temp_tag_writable_changed(int p_idx, bool p_pressed);

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
