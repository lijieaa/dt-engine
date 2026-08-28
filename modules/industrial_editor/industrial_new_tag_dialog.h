#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/box_container.h"
#include "core/object/ref_counted.h"

#include "industrial_project.h"

// Dialog for creating or editing a single tag.
class IndustrialNewTagDialog : public AcceptDialog {
	GDCLASS(IndustrialNewTagDialog, AcceptDialog);

public:
	IndustrialNewTagDialog();
	~IndustrialNewTagDialog() override;

	void set_project(Ref<IndustrialProject> p_project);
	void set_device_index(int p_device_index);
	void edit_tag(int p_tag_index);

private:
	Ref<IndustrialProject> project;
	int device_index = -1;
	int tag_index = -1; // -1 for new, >=0 for edit

	LineEdit *tag_name = nullptr;
	LineEdit *tag_description = nullptr;

	// Absolute schema controls.
	VBoxContainer *absolute_section = nullptr;
	OptionButton *tag_address_mode = nullptr;
	OptionButton *tag_address_type = nullptr;
	OptionButton *tag_data_format = nullptr;
	LineEdit *tag_offset = nullptr;
	SpinBox *tag_db_number = nullptr;
	SpinBox *tag_length = nullptr;
	Label *tag_data_format_label = nullptr;
	Label *tag_address_type_label = nullptr;
	Label *tag_offset_label = nullptr;
	HBoxContainer *row_db_length = nullptr;
	Label *tag_db_number_label = nullptr;
	Label *tag_length_label = nullptr;

	// Symbolic schema controls.
	VBoxContainer *symbolic_section = nullptr;
	LineEdit *tag_symbol = nullptr;
	OptionButton *tag_symbolic_data_format = nullptr;
	Label *tag_symbolic_data_format_label = nullptr;

	CheckButton *tag_writable = nullptr;
	SpinBox *tag_scale = nullptr;
	LineEdit *tag_unit = nullptr;

	String current_driver_key;
	PackedStringArray address_type_ids;
	PackedStringArray data_format_ids;
	PackedStringArray symbolic_data_format_ids;
	bool pending_tag_populate = false;
	IndustrialTagData pending_tag_data;

	void _build_ui();
	void _on_confirm();
	void _on_device_index_changed(int p_idx);
	void _on_tagfield_fetched_for_current(bool p_success);
	void _on_address_mode_changed(int p_idx);
	void _on_address_type_changed(int p_idx);

	void _resolve_driver_key(String &r_key, int &r_catalog_idx) const;
	void _apply_layout_for_mode(bool p_symbolic);
	void _fill_address_types();
	void _fill_data_formats(OptionButton *p_btn, PackedStringArray &p_ids, bool p_include_bit);
	void _fill_symbolic_data_format_dropdown(const String &p_preselect = String());
	void _update_conditional_fields();
	Dictionary _lookup_address_type(const String &p_id) const;
	void _select_option_by_metadata(OptionButton *p_btn, const String &p_id) const;
	void _select_option_by_id(PackedStringArray &p_ids, OptionButton *p_btn, const String &p_id) const;
	void _populate_from_tag(const IndustrialTagData &p_tag);
	static int _legacy_type_from_data_format(const String &p_format);

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
