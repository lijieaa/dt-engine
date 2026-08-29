#include "industrial_device_form.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"
#include "industrial_device_fields.h"

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

	// ── §B–§D connection groups (Common HMI extras hidden) ──
	label_conn_params = memnew(Label);
	label_conn_params->set_text(TTR("Connection"));
	label_conn_params->add_theme_font_size_override("font_size", 14);
	label_conn_params->add_theme_color_override("font_color", Color(0.6, 0.8, 1.0));
	add_child(label_conn_params);

	label_interface = memnew(Label);
	label_interface->set_text(TTR("Interface"));
	label_interface->add_theme_font_size_override("font_size", 12);
	add_child(label_interface);
	interface_params_container = memnew(VBoxContainer);
	interface_params_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(interface_params_container);

	label_protocol = memnew(Label);
	label_protocol->set_text(TTR("Protocol"));
	label_protocol->add_theme_font_size_override("font_size", 12);
	add_child(label_protocol);
	protocol_params_container = memnew(VBoxContainer);
	protocol_params_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(protocol_params_container);

	label_tuning = memnew(Label);
	label_tuning->set_text(TTR("Tuning"));
	label_tuning->add_theme_font_size_override("font_size", 12);
	add_child(label_tuning);
	tuning_params_container = memnew(VBoxContainer);
	tuning_params_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(tuning_params_container);
}

void IndustrialDeviceForm::set_project(Ref<IndustrialProject> p_project) {
	project = p_project;
	// Re-derive the driver dropdown every time the project is set so
	// that if the form was constructed before the Go runtime catalog
	// arrived, we pick up 81 drivers.
	refresh_driver_dropdown();
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

void IndustrialDeviceForm::set_read_only(bool p_read_only) {
	read_only = p_read_only;
	// Set all fields to read-only.
	if (field_name) field_name->set_editable(!p_read_only);
	if (field_description) field_description->set_editable(!p_read_only);
	if (field_driver) field_driver->set_disabled(p_read_only);
	if (field_enabled) field_enabled->set_disabled(p_read_only);
}

void IndustrialDeviceForm::clear_form() {
	device_index = -1;
	if (field_name) field_name->set_text("");
	if (field_description) field_description->set_text("");
	if (field_driver) field_driver->select(0);
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

	if (field_enabled) field_enabled->set_pressed(dev.enabled);

	// Populate dynamic params from flat + options.
	_populate_dynamic_params(dev.driver, industrial_device_param_dict(dev));

	// Populate tag table.
	_populate_tag_table();
}

void IndustrialDeviceForm::_clear_dynamic_params() {
	auto clear_container = [](VBoxContainer *p_container) {
		if (!p_container) {
			return;
		}
		while (p_container->get_child_count() > 0) {
			p_container->remove_child(p_container->get_child(0));
		}
	};
	clear_container(interface_params_container);
	clear_container(protocol_params_container);
	clear_container(tuning_params_container);
	param_widgets.clear();
	remote_hmi_row = nullptr;
}

void IndustrialDeviceForm::_on_location_mode_changed(int /*p_idx*/) {
	_update_remote_hmi_visibility();
}

void IndustrialDeviceForm::_update_remote_hmi_visibility() {
	if (!remote_hmi_row) {
		return;
	}
	bool show_remote = false;
	if (param_widgets.has("location_mode")) {
		const FieldWidget &fw = param_widgets["location_mode"];
		if (OptionButton *ob = Object::cast_to<OptionButton>(fw.widget)) {
			if (ob->get_selected() >= 0) {
				show_remote = ob->get_item_text(ob->get_selected()) == "Remote";
			}
		}
	}
	remote_hmi_row->set_visible(show_remote);
}

bool IndustrialDeviceForm::_should_show_interface_field(const String &p_key) const {
	static const char *kEth[] = { "ip", "port", "use_udp", "interface_type" };
	static const char *kSerial[] = {
		"serial_port", "baud_rate", "data_bits", "parity", "stop_bits",
		"flow_control", "station_no", "broadcast_station_no", "use_station_variable",
	};
	auto in_list = [](const String &p_key, const char *const *p_keys, int p_count) {
		for (int i = 0; i < p_count; i++) {
			if (p_key == p_keys[i]) {
				return true;
			}
		}
		return false;
	};
	const bool is_eth = in_list(p_key, kEth, sizeof(kEth) / sizeof(kEth[0]));
	const bool is_serial = in_list(p_key, kSerial, sizeof(kSerial) / sizeof(kSerial[0]));

	int driver_idx = field_driver ? field_driver->get_selected() : 0;
	const DriverMeta *meta = industrial_get_driver_meta(driver_idx);
	if (!meta) {
		return true;
	}
	const String iface = meta->interface;
	const bool eth_driver = iface == "ethernet" || iface == "ethernet_ip";
	const bool serial_driver = iface == "serial_rs232c" || iface == "serial_rs485" ||
			iface == "df1_fullduplex" || iface == "mpi" || iface == "ppi" || iface == "usb";
	if (eth_driver) {
		return is_eth || (!is_eth && !is_serial);
	}
	if (serial_driver) {
		return is_serial || (!is_eth && !is_serial);
	}
	return true;
}

void IndustrialDeviceForm::_populate_dynamic_params(int p_driver, const Dictionary &p_params) {
	_clear_dynamic_params();

	static const char *kHiddenKeys[] = {
		"name",
		"dev_type",
		"location_mode",
		"remote_hmi_ip",
		"interface_type",
		"supports_simulator",
		"enabled",
	};
	static const int kHiddenKeyCount = sizeof(kHiddenKeys) / sizeof(kHiddenKeys[0]);
	Vector<IndustrialFieldDef> visible_fields = industrial_filter_driver_fields(
			industrial_get_driver_fields(p_driver), kHiddenKeys, kHiddenKeyCount);

	int ip_idx = -1;
	int port_idx = -1;
	for (int i = 0; i < visible_fields.size(); i++) {
		if (visible_fields[i].key == "ip") {
			ip_idx = i;
		}
		if (visible_fields[i].key == "port") {
			port_idx = i;
		}
	}
	if (ip_idx >= 0 && port_idx >= 0 && port_idx > ip_idx + 1) {
		IndustrialFieldDef port_field = visible_fields[port_idx];
		visible_fields.remove_at(port_idx);
		visible_fields.insert(ip_idx + 1, port_field);
	}

	HashMap<String, Control *> widgets;
	for (int i = 0; i < visible_fields.size(); i++) {
		const IndustrialFieldDef &field = visible_fields[i];
		const IndustrialDeviceFieldGroup group = industrial_classify_device_field(field.key);
		if (group == IND_DEVICE_GROUP_INTERFACE && !_should_show_interface_field(field.key)) {
			continue;
		}

		VBoxContainer *target = nullptr;
		switch (group) {
			case IND_DEVICE_GROUP_COMMON:
				continue;
			case IND_DEVICE_GROUP_INTERFACE:
				target = interface_params_container;
				break;
			case IND_DEVICE_GROUP_TUNING:
				target = tuning_params_container;
				break;
			default:
				target = protocol_params_container;
				break;
		}
		if (!target) {
			continue;
		}

		const Variant current = p_params.has(field.key) ? p_params[field.key] : Variant();
		const int before = target->get_child_count();
		industrial_add_param_row(target, field, current, widgets, true);

		if (!widgets.has(field.key)) {
			continue;
		}

		FieldWidget fw;
		fw.key = field.key;
		fw.data_type = field.data_type;
		fw.widget = widgets[field.key];

		if (Control *w = fw.widget) {
			switch (field.data_type) {
				case 0:
				case 1:
					if (SpinBox *sb = Object::cast_to<SpinBox>(w)) {
						sb->connect(SceneStringName(value_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_double));
					}
					break;
				case 2:
					if (LineEdit *le = Object::cast_to<LineEdit>(w)) {
						le->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_no_arg));
					}
					break;
				case 3:
					if (CheckButton *cb = Object::cast_to<CheckButton>(w)) {
						cb->connect(SceneStringName(toggled), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_bool));
					}
					break;
				case 4:
					if (OptionButton *ob = Object::cast_to<OptionButton>(w)) {
						ob->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceForm::_on_field_changed_int));
						if (field.key == "location_mode") {
							ob->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceForm::_on_location_mode_changed));
						}
					}
					break;
				default:
					break;
			}
		}

		param_widgets[field.key] = fw;
		if (field.key == "remote_hmi_ip" && target->get_child_count() > before) {
			remote_hmi_row = Object::cast_to<Control>(target->get_child(target->get_child_count() - 1));
		}
	}

	_update_remote_hmi_visibility();
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
	// Get current param values before changing driver.
	Dictionary current_params;
	for (const KeyValue<String, FieldWidget> &kv : param_widgets) {
		const FieldWidget &fw = kv.value;
		if (fw.widget) {
			current_params[fw.key] = industrial_read_param_widget(fw.widget, fw.data_type);
		}
	}

	// Update device driver and repopulate params.
	IndustrialDeviceData dev = project->get_device(device_index);
	dev.driver = p_idx;
	project->update_device(device_index, dev);

	_populate_dynamic_params(p_idx, current_params);
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

	// Dynamic params → flat fields + options.
	Dictionary params;
	for (const KeyValue<String, FieldWidget> &kv : param_widgets) {
		const FieldWidget &fw = kv.value;
		if (fw.widget) {
			params[fw.key] = industrial_read_param_widget(fw.widget, fw.data_type);
		}
	}
	industrial_apply_param_dict_to_device(dev, params);
	industrial_sync_device_connection_params(dev);
	if (field_driver) {
		dev.driver_key = industrial_get_driver_key(field_driver->get_selected());
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
	if (!label_selected_tag || !tag_detail_container) {
		return;
	}
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
