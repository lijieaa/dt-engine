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

// Single-page form for creating a device (no inline initial tags).
// Layout:
//   ① Basic Info         (Name, Description, Interface Type, Device Type/Protocol)
//   ② §A–§D Connection groups  (Interface / Protocol / Tuning; Common extras hidden)
class IndustrialNewDeviceDialog : public AcceptDialog {
	GDCLASS(IndustrialNewDeviceDialog, AcceptDialog);

public:
	IndustrialNewDeviceDialog();
	~IndustrialNewDeviceDialog() override;

	void set_project(Ref<IndustrialProject> p_project);
	int get_created_device_index() const { return created_index; }

	void refresh_protocol_dropdowns();

private:
	Ref<IndustrialProject> project;
	int created_index = -1;

	VBoxContainer *basic_section = nullptr;
	LineEdit *dev_name = nullptr;
	LineEdit *dev_desc = nullptr;
	OptionButton *dev_interface_type = nullptr;
	OptionButton *dev_device_type = nullptr;

	VBoxContainer *params_section = nullptr;
	VBoxContainer *interface_params_container = nullptr;
	VBoxContainer *protocol_params_container = nullptr;
	VBoxContainer *tuning_params_container = nullptr;
	HashMap<String, Control *> param_widgets;
	Control *remote_hmi_row = nullptr;

	String current_driver_key;

	void _build_ui();
	void _populate_interface_types();
	void _populate_device_types(int p_group_idx);
	int _interface_to_group(const String &p_iface) const;
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

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
