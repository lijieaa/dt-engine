#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/spin_box.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/templates/hash_map.h"

class IndustrialProject;
struct IndustrialDeviceData;

// Right-side dock: device property form with dynamic fields per driver schema.
class IndustrialDeviceForm : public VBoxContainer {
	GDCLASS(IndustrialDeviceForm, VBoxContainer);

public:
	IndustrialDeviceForm();
	~IndustrialDeviceForm() override;

	void set_project(Ref<IndustrialProject> p_project);
	void edit_device(int p_device_index);
	void clear_form();
	void set_read_only(bool p_read_only);

	// Populate/refresh the Driver/ScanGroup dropdowns on demand (called
	// from IndustrialEditorPlugin both on construction and after the
	// Go runtime /api/v1/drivers HTTP fetch finishes, as well as each
	// time the project changes so 81 backend drivers are visible even if
	// the form was built before the HTTP catalog settled).
	void refresh_driver_dropdown();
	void refresh_scangroup_dropdown();

private:
	Ref<IndustrialProject> project;
	int device_index = -1;
	bool read_only = false;

	// Basic info fields.
	LineEdit *field_name = nullptr;
	LineEdit *field_description = nullptr;
	OptionButton *field_driver = nullptr;
	OptionButton *field_scan_group = nullptr;
	CheckButton *field_enabled = nullptr;

	Label *label_common = nullptr;
	Label *label_interface = nullptr;
	Label *label_protocol = nullptr;
	Label *label_tuning = nullptr;

	// Dynamic connection params — §B–§D (+ common extras beyond Device Info).
	VBoxContainer *common_params_container = nullptr;
	VBoxContainer *interface_params_container = nullptr;
	VBoxContainer *protocol_params_container = nullptr;
	VBoxContainer *tuning_params_container = nullptr;
	Control *remote_hmi_row = nullptr;

	// Tag table container.
	VBoxContainer *tag_table_container = nullptr;

	// Selected tag detail form.
	VBoxContainer *tag_detail_container = nullptr;

	// UI sections.
	Label *label_device_info = nullptr;
	Label *label_conn_params = nullptr;
	Label *label_tags = nullptr;
	Label *label_selected_tag = nullptr;

	// Stored field widgets for dynamic params (key → widget).
	// Use HashMap instead of Dictionary because FieldWidget is a custom struct, not a Variant.
	struct FieldWidget {
		Control *widget = nullptr;
		String key;
		int data_type = 0; // 0=int, 1=float, 2=string, 3=bool, 4=choice
	};
	HashMap<String, FieldWidget> param_widgets;

	void _build_ui();
	void _populate_dynamic_params(int p_driver, const Dictionary &p_params);
	void _clear_dynamic_params();
	void _update_remote_hmi_visibility();
	bool _should_show_interface_field(const String &p_key) const;
	void _on_location_mode_changed(int p_idx);
	void _populate_tag_table();
	void _update_tag_detail(int p_tag_index);
	void _on_driver_changed(int p_idx);
	void _on_scan_group_changed(int p_idx);
	// Signal dispatch helpers.
	// NOTE: do NOT overload these — in DEBUG builds callable_mp resolves the
	// target by the STRINGIFIED method name via ClassDB::get_method(name),
	// so two overloads with the same identifier always resolve to the same
	// single MethodBind (the first one registered), producing the error
	// "Method expected 0 argument(s), but called with 1." whenever a
	// CheckButton's toggled(bool) or a SpinBox's value_changed(double) fires.
	// We therefore give each target a unique name and connect explicitly by
	// the required signature.
	void _on_field_changed_no_arg() { _do_field_changed(); }
	void _on_field_changed_bool(bool) { _do_field_changed(); }
	void _on_field_changed_double(double) { _do_field_changed(); }
	void _on_field_changed_int(int) { _do_field_changed(); }
	void _do_field_changed();
	void _on_tag_selected(int p_tag_index);
	void _add_tag_row();

public:
	// Tag table helpers.
	void add_empty_tag();
	void remove_tag(int p_tag_index);
	void add_tag_row_to_table(int p_tag_index);

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
