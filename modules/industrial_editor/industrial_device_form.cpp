#include "industrial_device_form.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "editor/editor_string_names.h"
#include "core/object/callable_mp.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/tree.h"
#include "scene/gui/separator.h"

IndustrialDeviceForm::IndustrialDeviceForm() {
	_build_ui();
}

IndustrialDeviceForm::~IndustrialDeviceForm() {}

void IndustrialDeviceForm::_bind_methods() {}

void IndustrialDeviceForm::_notification(int p_what) {}

void IndustrialDeviceForm::_build_ui() {
	set_name("Device Form");
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);

	// ── Device Info section ──
	label_device_info = memnew(Label);
	label_device_info->set_text(TTR("Device Info"));
	label_device_info->add_theme_font_size_override("font_size", 14);
	label_device_info->add_theme_color_override("font_color", Color(0.6, 0.8, 1.0));
	add_child(label_device_info);

	// Name row.
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label);
		lbl->set_text(TTR("Name:"));
		lbl->set_custom_minimum_size(Size2(80, 0));
		row->add_child(lbl);
		field_name = memnew(LineEdit);
		field_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		field_name->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_no_arg));
		row->add_child(field_name);
		add_child(row);
	}

	// Description row.
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label);
		lbl->set_text(TTR("Description:"));
		lbl->set_custom_minimum_size(Size2(80, 0));
		row->add_child(lbl);
		field_description = memnew(LineEdit);
		field_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		field_description->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_no_arg));
		row->add_child(field_description);
		add_child(row);
	}

	// Driver row.
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label);
		lbl->set_text(TTR("Driver:"));
		lbl->set_custom_minimum_size(Size2(80, 0));
		row->add_child(lbl);
		field_driver = memnew(OptionButton);
		field_driver->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		// NOTE: do NOT fill items here — the Go runtime catalog loads
		// asynchronously via HTTP immediately after plugin enter_tree, so at
		// ctor time we would only see the legacy 7-item fallback (which
		// looked like "协议改不见了" to users).  refresh_driver_dropdown()
		// is called from set_project() and from the plugin's metadata-ready
		// callback, so the dropdown always carries the authoritative
		// driver list (currently 81 entries).
		field_driver->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceForm::_on_driver_changed));
		row->add_child(field_driver);
		add_child(row);
		refresh_driver_dropdown();
	}

	// Scan group row.
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label);
		lbl->set_text(TTR("Scan Group:"));
		lbl->set_custom_minimum_size(Size2(80, 0));
		row->add_child(lbl);
		field_scan_group = memnew(OptionButton);
		field_scan_group->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		// Same as driver: populate through shared refresh helper so the
		// dropdown is re-derived on catalog ready.
		field_scan_group->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceForm::_on_scan_group_changed));
		row->add_child(field_scan_group);
		add_child(row);
		refresh_scangroup_dropdown();
	}

	// Enabled row.
	{
		HBoxContainer *row = memnew(HBoxContainer);
		field_enabled = memnew(CheckButton);
		field_enabled->set_text(TTR("Enabled"));
		field_enabled->connect(SceneStringName(toggled), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_bool));
		row->add_child(field_enabled);
		add_child(row);
	}

	add_child(memnew(HSeparator));

	// ── Connection Params section ──
	label_conn_params = memnew(Label);
	label_conn_params->set_text(TTR("Connection Parameters"));
	label_conn_params->add_theme_font_size_override("font_size", 14);
	label_conn_params->add_theme_color_override("font_color", Color(0.6, 0.8, 1.0));
	add_child(label_conn_params);

	params_container = memnew(VBoxContainer);
	params_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(params_container);

	add_child(memnew(HSeparator));

	// ── Tags section ──
	label_tags = memnew(Label);
	label_tags->set_text(TTR("Tags"));
	label_tags->add_theme_font_size_override("font_size", 14);
	label_tags->add_theme_color_override("font_color", Color(0.6, 0.8, 1.0));
	add_child(label_tags);

	// Tag table (embedded).
	tag_table_container = memnew(VBoxContainer);
	tag_table_container->set_custom_minimum_size(Size2(0, 150));
	tag_table_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(tag_table_container);

	// Add tag button.
	{
		HBoxContainer *btn_row = memnew(HBoxContainer);
		Button *add_btn = memnew(Button);
		add_btn->set_text(TTR("+ Add Tag"));
		add_btn->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceForm::add_empty_tag));
		btn_row->add_child(add_btn);

		Button *batch_btn = memnew(Button);
		batch_btn->set_text(TTR("Batch Generate..."));
		batch_btn->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceForm::_add_tag_row));
		btn_row->add_child(batch_btn);

		add_child(btn_row);
	}

	add_child(memnew(HSeparator));

	// ── Selected tag detail ──
	label_selected_tag = memnew(Label);
	label_selected_tag->set_text(TTR("Selected Tag"));
	label_selected_tag->add_theme_font_size_override("font_size", 14);
	label_selected_tag->add_theme_color_override("font_color", Color(0.6, 0.8, 1.0));
	label_selected_tag->hide();
	add_child(label_selected_tag);

	tag_detail_container = memnew(VBoxContainer);
	tag_detail_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_detail_container->hide();
	add_child(tag_detail_container);
}

void IndustrialDeviceForm::set_project(Ref<IndustrialProject> p_project) {
	project = p_project;
	// Re-derive dropdowns every time the project is set so:
	//   (a) if the form was constructed before the Go runtime catalog
	//       arrived, we pick up 81 drivers;
	//   (b) future scangroup refresh (derived from the project's devices)
	//       is guaranteed to reflect the latest data set.
	refresh_driver_dropdown();
	refresh_scangroup_dropdown();
	clear_form();
}

void IndustrialDeviceForm::refresh_driver_dropdown() {
	if (!field_driver) return;
	const int saved = field_driver->get_selected();
	field_driver->clear();
	StringList names = industrial_get_driver_names();
	for (int i = 0; i < names.size(); i++) {
		field_driver->add_item(names[i]);
	}
	if (field_driver->get_item_count() > 0) {
		if (saved >= 0 && saved < field_driver->get_item_count()) {
			field_driver->select(saved);
		} else if (device_index >= 0 && project.is_valid() && device_index < project->get_device_count()) {
			int d = project->get_device(device_index).driver;
			if (d >= 0 && d < field_driver->get_item_count()) {
				field_driver->select(d);
			} else {
				field_driver->select(0);
			}
		} else {
			field_driver->select(0);
		}
	}
}

void IndustrialDeviceForm::refresh_scangroup_dropdown() {
	if (!field_scan_group) return;
	const int sel_idx = field_scan_group->get_selected();
	const String saved = (sel_idx >= 0 && sel_idx < field_scan_group->get_item_count())
			? field_scan_group->get_item_text(sel_idx)
			: String();
	field_scan_group->clear();
	field_scan_group->add_item(TTR("(none)"));
	if (project.is_valid()) {
		// Collect existing scan groups across all devices; sort for determinism.
		Vector<String> groups;
		HashSet<String> seen;
		for (int i = 0; i < project->get_device_count(); i++) {
			const String &g = project->get_device(i).scan_group;
			if (g.length() == 0) continue;
			if (seen.has(g)) continue;
			seen.insert(g);
			groups.push_back(g);
		}
		groups.sort();
		for (int i = 0; i < groups.size(); i++) {
			field_scan_group->add_item(groups[i]);
		}
	}
	// Try to restore selection.
	for (int i = 0; i < field_scan_group->get_item_count(); i++) {
		if (field_scan_group->get_item_text(i) == saved) {
			field_scan_group->select(i);
			return;
		}
	}
	field_scan_group->select(0);
}

void IndustrialDeviceForm::set_read_only(bool p_read_only) {
	read_only = p_read_only;
	// Set all fields to read-only.
	if (field_name) field_name->set_editable(!p_read_only);
	if (field_description) field_description->set_editable(!p_read_only);
	if (field_driver) field_driver->set_disabled(p_read_only);
	if (field_scan_group) field_scan_group->set_disabled(p_read_only);
	if (field_enabled) field_enabled->set_disabled(p_read_only);
}

void IndustrialDeviceForm::clear_form() {
	device_index = -1;
	if (field_name) field_name->set_text("");
	if (field_description) field_description->set_text("");
	if (field_driver) field_driver->select(0);
	if (field_scan_group) field_scan_group->select(0);
	if (field_enabled) field_enabled->set_pressed(true);

	_clear_dynamic_params();
	// Clear tag table.
	while (tag_table_container && tag_table_container->get_child_count() > 0) {
		tag_table_container->remove_child(tag_table_container->get_child(0));
	}

	label_selected_tag->hide();
	tag_detail_container->hide();
}

void IndustrialDeviceForm::edit_device(int p_device_index) {
	device_index = p_device_index;
	if (project.is_null() || p_device_index < 0) {
		clear_form();
		return;
	}

	const auto &dev = project->get_device(p_device_index);

	// Populate basic fields.
	if (field_name) field_name->set_text(dev.name);
	if (field_description) field_description->set_text(dev.description);
	if (field_driver && dev.driver >= 0 && dev.driver < industrial_get_driver_count()) {
		field_driver->select(dev.driver);
	}

	// Update scan group options.
	if (field_scan_group) {
		// Remove old items except the first "(none)".
		while (field_scan_group->get_item_count() > 1) {
			field_scan_group->remove_item(field_scan_group->get_item_count() - 1);
		}
		int sg_count = project->get_scan_group_count();
		for (int i = 0; i < sg_count; i++) {
			field_scan_group->add_item(project->get_scan_group(i).name);
		}
		// Select the device's scan group.
		int sel_idx = 0;
		if (!dev.scan_group.is_empty()) {
			for (int i = 0; i < field_scan_group->get_item_count(); i++) {
				if (field_scan_group->get_item_text(i) == dev.scan_group) {
					sel_idx = i;
					break;
				}
			}
		}
		field_scan_group->select(sel_idx);
	}

	if (field_enabled) field_enabled->set_pressed(dev.enabled);

	// Populate dynamic params.
	_populate_dynamic_params(dev.driver, dev.connection_params);

	// Populate tag table.
	_populate_tag_table();
}

void IndustrialDeviceForm::_clear_dynamic_params() {
	if (params_container) {
		while (params_container->get_child_count() > 0) {
			params_container->remove_child(params_container->get_child(0));
		}
	}
	param_widgets.clear();
}

void IndustrialDeviceForm::_populate_dynamic_params(int p_driver, const Dictionary &p_params) {
	_clear_dynamic_params();

	Vector<IndustrialFieldDef> fields = industrial_get_driver_fields(p_driver);
	for (const auto &field : fields) {
		HBoxContainer *row = memnew(HBoxContainer);

		Label *lbl = memnew(Label);
		lbl->set_text(field.name + ":");
		lbl->set_custom_minimum_size(Size2(120, 0));
		row->add_child(lbl);

		FieldWidget fw;
		fw.key = field.key;
		fw.data_type = field.data_type;

		switch (field.data_type) {
			case 0: { // int
				SpinBox *sb = memnew(SpinBox);
				sb->set_min(field.min_value);
				sb->set_max(field.max_value > 0 ? field.max_value : 999999);
				sb->set_step(1);
				if (p_params.has(field.key)) {
					sb->set_value(p_params[field.key]);
				} else {
					sb->set_value(field.default_value);
				}
				sb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				sb->connect(SceneStringName(value_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_double));
				fw.widget = sb;
				row->add_child(sb);
			} break;
			case 1: { // float
				SpinBox *sb = memnew(SpinBox);
				sb->set_min(field.min_value);
				sb->set_max(field.max_value > 0 ? field.max_value : 999999.0);
				sb->set_step(0.01);
				if (p_params.has(field.key)) {
					sb->set_value(p_params[field.key]);
				} else {
					sb->set_value(field.default_value);
				}
				sb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				sb->connect(SceneStringName(value_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_double));
				fw.widget = sb;
				row->add_child(sb);
			} break;
			case 2: { // string
				LineEdit *le = memnew(LineEdit);
				if (p_params.has(field.key)) {
					le->set_text(p_params[field.key]);
				} else {
					le->set_text(field.default_value);
				}
				le->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				le->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_no_arg));
				fw.widget = le;
				row->add_child(le);
			} break;
			case 3: { // bool
				CheckButton *cb = memnew(CheckButton);
				if (p_params.has(field.key)) {
					cb->set_pressed(p_params[field.key]);
				} else {
					cb->set_pressed(field.default_value);
				}
				cb->connect(SceneStringName(toggled), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_bool));
				fw.widget = cb;
				row->add_child(cb);
			} break;
			case 4: { // choice
				OptionButton *ob = memnew(OptionButton);
				for (int ci = 0; ci < field.choices.size(); ci++) {
					ob->add_item(field.choices[ci]);
				}
				String current_val;
				if (p_params.has(field.key)) {
					current_val = p_params[field.key];
				} else {
					current_val = field.default_value;
				}
				for (int ci = 0; ci < field.choices.size(); ci++) {
					if (field.choices[ci] == current_val) {
						ob->select(ci);
						break;
					}
				}
				ob->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				ob->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_int));
				fw.widget = ob;
				row->add_child(ob);
			} break;
			default: {
				LineEdit *le = memnew(LineEdit);
				le->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				fw.widget = le;
				row->add_child(le);
			} break;
		}

		if (!field.tooltip.is_empty()) {
			lbl->set_tooltip_text(field.tooltip);
		}

		param_widgets[field.key] = fw;
		params_container->add_child(row);
	}
}

void IndustrialDeviceForm::_populate_tag_table() {
	if (!tag_table_container || project.is_null()) {
		return;
	}

	// Clear existing tag rows.
	while (tag_table_container->get_child_count() > 0) {
		tag_table_container->remove_child(tag_table_container->get_child(0));
	}

	if (device_index < 0) {
		return;
	}

	const auto &dev = project->get_device(device_index);
	int tag_count = (int)dev.tags.size();
	for (int i = 0; i < tag_count; i++) {
		add_tag_row_to_table(i);
	}
}

void IndustrialDeviceForm::add_tag_row_to_table(int p_tag_index) {
	if (!tag_table_container || project.is_null() || device_index < 0) {
		return;
	}

	const auto &tag = project->get_device(device_index).tags[p_tag_index];

	HBoxContainer *row = memnew(HBoxContainer);

	// Address.
	LineEdit *addr_edit = memnew(LineEdit);
	addr_edit->set_text(tag.address);
	addr_edit->set_custom_minimum_size(Size2(100, 24));
	addr_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	addr_edit->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_no_arg));
	row->add_child(addr_edit);

	// Name.
	LineEdit *name_edit = memnew(LineEdit);
	name_edit->set_text(tag.name);
	name_edit->set_custom_minimum_size(Size2(100, 24));
	name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	name_edit->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_no_arg));
	row->add_child(name_edit);

	// Data type.
	OptionButton *type_btn = memnew(OptionButton);
	StringList types = industrial_get_data_type_names();
	for (int t = 0; t < types.size(); t++) {
		type_btn->add_item(types[t]);
	}
	if (tag.data_type >= 0 && tag.data_type < TYPE_MAX) {
		type_btn->select(tag.data_type);
	}
	type_btn->set_custom_minimum_size(Size2(80, 24));
	type_btn->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_int));
	row->add_child(type_btn);

	// Writable.
	CheckButton *write_cb = memnew(CheckButton);
	write_cb->set_pressed(tag.writable);
	write_cb->connect(SceneStringName(toggled), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_bool));
	row->add_child(write_cb);

	// Delete button.
	Button *del_btn = memnew(Button);
	del_btn->set_text("✕");
	del_btn->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceForm::remove_tag).bind(p_tag_index));
	row->add_child(del_btn);

	tag_table_container->add_child(row);
}

void IndustrialDeviceForm::add_empty_tag() {
	if (project.is_null() || device_index < 0) {
		return;
	}

	IndustrialTagData new_tag;
	new_tag.name = "New Tag";
	new_tag.address = "DB0.DBW0";
	new_tag.data_type = TYPE_INT16;
	new_tag.writable = true;

	project->add_tag(device_index, new_tag);
	int new_idx = project->get_tag_count_for_device(device_index) - 1;
	add_tag_row_to_table(new_idx);
}

void IndustrialDeviceForm::remove_tag(int p_tag_index) {
	if (project.is_null() || device_index < 0) {
		return;
	}
	project->remove_tag(device_index, p_tag_index);
	_populate_tag_table();
}

void IndustrialDeviceForm::_on_driver_changed(int p_idx) {
	if (device_index < 0 || project.is_null()) {
		return;
	}
	// Get current connection params before changing driver.
		Dictionary current_params;
		for (const auto &kv : param_widgets) {
			const String &key = kv.key;
			const FieldWidget &fw = kv.value;
			if (fw.widget) {
				switch (fw.data_type) {
					case 0:
					case 1:
						current_params[key] = Object::cast_to<SpinBox>(fw.widget)->get_value();
						break;
					case 2:
						current_params[key] = Object::cast_to<LineEdit>(fw.widget)->get_text();
						break;
					case 3:
						current_params[key] = Object::cast_to<CheckButton>(fw.widget)->is_pressed();
						break;
					case 4:
						current_params[fw.key] = Object::cast_to<OptionButton>(fw.widget)->get_item_text(Object::cast_to<OptionButton>(fw.widget)->get_selected());
						break;
				}
			}
		}

	// Update device driver and repopulate params.
	IndustrialDeviceData dev = project->get_device(device_index);
	dev.driver = p_idx;
	project->update_device(device_index, dev);

	_populate_dynamic_params(p_idx, current_params);
}

void IndustrialDeviceForm::_on_scan_group_changed(int p_idx) {
	// No-op for now; scan group changes will be committed in _on_field_changed.
}

void IndustrialDeviceForm::_do_field_changed() {
	// Commit field values back to project.
	if (device_index < 0 || project.is_null()) {
		return;
	}

	IndustrialDeviceData dev = project->get_device(device_index);

	if (field_name) dev.name = field_name->get_text();
	if (field_description) dev.description = field_description->get_text();
	if (field_driver) dev.driver = field_driver->get_selected();
	if (field_enabled) dev.enabled = field_enabled->is_pressed();

	// Scan group.
	if (field_scan_group && field_scan_group->get_selected() > 0) {
		dev.scan_group = field_scan_group->get_item_text(field_scan_group->get_selected());
	} else {
		dev.scan_group = "";
	}

	// Connection params.
	for (auto &kv : param_widgets) {
		const String &key = kv.key;
		FieldWidget &fw = kv.value;
		if (fw.widget) {
			switch (fw.data_type) {
				case 0:
				case 1:
					dev.connection_params[key] = Object::cast_to<SpinBox>(fw.widget)->get_value();
					break;
				case 2:
					dev.connection_params[key] = Object::cast_to<LineEdit>(fw.widget)->get_text();
					break;
				case 3:
					dev.connection_params[key] = Object::cast_to<CheckButton>(fw.widget)->is_pressed();
					break;
				case 4:
					dev.connection_params[key] = Object::cast_to<OptionButton>(fw.widget)->get_item_text(Object::cast_to<OptionButton>(fw.widget)->get_selected());
					break;
			}
		}
	}

	project->update_device(device_index, dev);
}

void IndustrialDeviceForm::_add_tag_row() {
	add_empty_tag();
}

void IndustrialDeviceForm::_on_tag_selected(int p_tag_index) {
	_update_tag_detail(p_tag_index);
}

void IndustrialDeviceForm::_update_tag_detail(int p_tag_index) {
	if (device_index < 0 || project.is_null()) {
		label_selected_tag->hide();
		tag_detail_container->hide();
		return;
	}

	const auto &tags = project->get_device(device_index).tags;
	if (p_tag_index < 0 || p_tag_index >= (int)tags.size()) {
		label_selected_tag->hide();
		tag_detail_container->hide();
		return;
	}

	const auto &tag = tags[p_tag_index];
	label_selected_tag->show();
	tag_detail_container->show();

	// Clear existing tag detail fields.
	while (tag_detail_container->get_child_count() > 0) {
		tag_detail_container->remove_child(tag_detail_container->get_child(0));
	}

	// Add tag property fields.
	{
		HBoxContainer *row = memnew(HBoxContainer);
		row->add_child(memnew(Label(TTR("Name:"))));
		LineEdit *le = memnew(LineEdit);
		le->set_text(tag.name);
		le->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(le);
		tag_detail_container->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		row->add_child(memnew(Label(TTR("Address:"))));
		LineEdit *le = memnew(LineEdit);
		le->set_text(tag.address);
		le->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(le);
		tag_detail_container->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		row->add_child(memnew(Label(TTR("Type:"))));
		OptionButton *ob = memnew(OptionButton);
		StringList types = industrial_get_data_type_names();
		for (int i = 0; i < types.size(); i++) {
			ob->add_item(types[i]);
		}
		if (tag.data_type >= 0 && tag.data_type < TYPE_MAX) {
			ob->select(tag.data_type);
		}
		ob->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(ob);
		tag_detail_container->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		CheckButton *cb = memnew(CheckButton);
		cb->set_text(TTR("Writable"));
		cb->set_pressed(tag.writable);
		row->add_child(cb);
		tag_detail_container->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		row->add_child(memnew(Label(TTR("Scale:"))));
		SpinBox *sb = memnew(SpinBox);
		sb->set_value(tag.scale);
		sb->set_min(-10000);
		sb->set_max(10000);
		sb->set_step(0.01);
		sb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(sb);
		row->add_child(memnew(Label(TTR("Unit:"))));
		LineEdit *le = memnew(LineEdit);
		le->set_text(tag.unit);
		le->set_custom_minimum_size(Size2(60, 24));
		row->add_child(le);
		tag_detail_container->add_child(row);
	}
}
