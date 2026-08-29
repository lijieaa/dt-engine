#include "industrial_new_device_dialog.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"
#include "industrial_device_fields.h"
#include "industrial_runtime_client.h"

#include "editor/editor_string_names.h"
#include "core/object/callable_mp.h"
#include "scene/gui/separator.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/scroll_container.h"
#include "core/templates/list.h"
#include "core/templates/hash_map.h"

IndustrialNewDeviceDialog::IndustrialNewDeviceDialog() {
	_build_ui();
}

IndustrialNewDeviceDialog::~IndustrialNewDeviceDialog() {}

void IndustrialNewDeviceDialog::_bind_methods() {}

void IndustrialNewDeviceDialog::_notification(int p_what) {}

static Label *_make_section_title(const String &p_text) {
	Label *lbl = memnew(Label);
	lbl->set_text(p_text);
	lbl->add_theme_font_size_override("font_size", 13);
	lbl->add_theme_color_override("font_color", Color(0.75f, 0.82f, 1.0f, 1.0f));
	return lbl;
}

void IndustrialNewDeviceDialog::_build_ui() {
	// Title (also a msgid).
	set_title(TTRC("New Device"));
	set_min_size(Size2(620, 520));

	get_ok_button()->set_text(TTRC("Create Device"));
	// The dialog is reused for every device creation. A one-shot connection
	// would make the first submission work and silently disable all later ones.
	get_ok_button()->connect(SceneStringName(pressed), callable_mp(this, &IndustrialNewDeviceDialog::_on_confirm_pressed));

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(main);

	// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
	// 鈶?Basic Info  (Name / Description / Driver/Protocol ONLY 鈥?	//   Scan Group  鈫?managed in Device Dock after creation.
	//   Enabled     鈫?EBPro +0xc44 defaults to ENABLED; toggled later.)
	// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
	main->add_child(_make_section_title(TTRC("Basic Info")));

	basic_section = memnew(VBoxContainer);
	basic_section->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	basic_section->add_theme_constant_override("separation", 4);
	main->add_child(basic_section);

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Name:")));
		lbl->set_custom_minimum_size(Size2(130, 0));
		row->add_child(lbl);
		dev_name = memnew(LineEdit);
		dev_name->set_placeholder(TTRC("device name, e.g. S7_Line01_CPU"));
		dev_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(dev_name);
		basic_section->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Description:")));
		lbl->set_custom_minimum_size(Size2(130, 0));
		row->add_child(lbl);
		dev_desc = memnew(LineEdit);
		dev_desc->set_placeholder(TTRC("(optional)"));
		dev_desc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(dev_desc);
		basic_section->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Interface Type:")));
		lbl->set_custom_minimum_size(Size2(130, 0));
		row->add_child(lbl);
		dev_interface_type = memnew(OptionButton);
		dev_interface_type->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		dev_interface_type->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialNewDeviceDialog::_on_interface_changed));
		row->add_child(dev_interface_type);
		basic_section->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Device Type:")));
		lbl->set_custom_minimum_size(Size2(130, 0));
		row->add_child(lbl);
		dev_device_type = memnew(OptionButton);
		dev_device_type->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		dev_device_type->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialNewDeviceDialog::_on_device_type_changed));
		row->add_child(dev_device_type);
		basic_section->add_child(row);
	}

	main->add_child(memnew(HSeparator));

	// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
	// 搂A鈥撀 EBPro-aligned connection groups (dynamic fields)
	// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
	params_section = memnew(VBoxContainer);
	params_section->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	params_section->add_theme_constant_override("separation", 6);

	params_section->add_child(_make_section_title(TTRC("Interface")));
	interface_params_container = memnew(VBoxContainer);
	interface_params_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	interface_params_container->add_theme_constant_override("separation", 2);
	params_section->add_child(interface_params_container);

	params_section->add_child(_make_section_title(TTRC("Protocol")));
	protocol_params_container = memnew(VBoxContainer);
	protocol_params_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	protocol_params_container->add_theme_constant_override("separation", 2);
	params_section->add_child(protocol_params_container);

	params_section->add_child(_make_section_title(TTRC("Tuning")));
	tuning_params_container = memnew(VBoxContainer);
	tuning_params_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tuning_params_container->add_theme_constant_override("separation", 2);
	params_section->add_child(tuning_params_container);

	main->add_child(params_section);

	// Populate driver dropdowns (fallback to legacy hardcoded list if the
	// runtime catalog isn't loaded yet; set_project() + plugin callback will
	// refresh once the async /api/v1/drivers fetch finishes).
	refresh_protocol_dropdowns();
}

void IndustrialNewDeviceDialog::set_project(Ref<IndustrialProject> p_project) {
	project = p_project;

	created_index = -1;

	if (dev_name) {
		dev_name->set_text("");
	}
	if (dev_desc) {
		dev_desc->set_text("");
	}
	refresh_protocol_dropdowns();
	_rebuild_params(0);
}

void IndustrialNewDeviceDialog::refresh_protocol_dropdowns() {
	if (dev_interface_type) {
		int saved_meta = -1;
		if (dev_interface_type->get_selected() >= 0) {
			saved_meta = (int)dev_interface_type->get_item_metadata(dev_interface_type->get_selected());
		}
		dev_interface_type->clear();
		_populate_interface_types();
		if (saved_meta >= 0) {
			for (int i = 0; i < dev_interface_type->get_item_count(); i++) {
				if ((int)dev_interface_type->get_item_metadata(i) == saved_meta) {
					dev_interface_type->select(i);
					break;
				}
			}
		}
		if (dev_interface_type->get_selected() < 0 && dev_interface_type->get_item_count() > 0) {
			dev_interface_type->select(0);
		}
	}
	if (dev_device_type) {
		int group = 0;
		if (dev_interface_type && dev_interface_type->get_selected() >= 0) {
			group = (int)dev_interface_type->get_item_metadata(dev_interface_type->get_selected());
		}
		int saved_meta = -1;
		if (dev_device_type->get_selected() >= 0) {
			saved_meta = (int)dev_device_type->get_item_metadata(dev_device_type->get_selected());
		}
		dev_device_type->clear();
		_populate_device_types(group);
		if (saved_meta >= 0) {
			for (int i = 0; i < dev_device_type->get_item_count(); i++) {
				if ((int)dev_device_type->get_item_metadata(i) == saved_meta) {
					dev_device_type->select(i);
					break;
				}
			}
		}
		if (dev_device_type->get_selected() < 0 && dev_device_type->get_item_count() > 0) {
			dev_device_type->select(0);
		}
		// Rebuild connection-params section when the list changes.
		int driver_idx = -1;
		if (dev_device_type->get_selected() >= 0) {
			driver_idx = (int)dev_device_type->get_item_metadata(dev_device_type->get_selected());
		}
		_rebuild_params(MAX(0, driver_idx));
		current_driver_key = _resolve_driver_key(MAX(0, driver_idx));
	}
}

// 鈹€鈹€ Interface-group helpers 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
// Maps each backend category_interface value to one of 5 UI groups so the
// first dropdown only shows 5 broad categories.  When the runtime catalog is
// not reachable, _populate_interface_types() falls back to a single "All"
// group containing the legacy 7 hardcoded drivers.

int IndustrialNewDeviceDialog::_interface_to_group(const String &p_iface) const {
	// Group 0 鈥?Ethernet family (PLCIF=1).
	if (p_iface == "ethernet" || p_iface == "ethernet_ip") return 0;
	// Group 1 鈥?Serial family (PLCIF=0).
	if (p_iface == "serial_rs232c" || p_iface == "serial_rs485" ||
			p_iface == "df1_fullduplex" || p_iface == "mpi" ||
			p_iface == "ppi" || p_iface == "usb") return 1;
	// Group 2 鈥?Bus / fieldbus.
	if (p_iface == "profibus" || p_iface == "ethercat" || p_iface == "can_j1939") return 2;
	// Group 3 鈥?Vertical industry protocols.
	if (p_iface == "bacnet_mstp" || p_iface == "bacnet_ip" ||
			p_iface == "iec_104" || p_iface == "iec_101" ||
			p_iface == "hsms" || p_iface == "secs_i") return 3;
	// Group 4 鈥?Free / special.
	if (p_iface == "free") return 4;
	// Default: drop into Serial group.
	return 1;
}

String IndustrialNewDeviceDialog::_interface_label(const String &p_iface) const {
	if (p_iface == "ethernet")      return TTRC("Ethernet");
	if (p_iface == "ethernet_ip")   return TTRC("EtherNet/IP");
	if (p_iface == "serial_rs232c") return TTRC("RS-232C");
	if (p_iface == "serial_rs485")  return TTRC("RS-485");
	if (p_iface == "mpi")           return TTRC("MPI");
	if (p_iface == "ppi")           return TTRC("PPI");
	if (p_iface == "profibus")      return TTRC("PROFIBUS DP");
	if (p_iface == "usb")           return TTRC("USB");
	if (p_iface == "df1_fullduplex") return TTRC("DF1 Full-Duplex");
	if (p_iface == "can_j1939")     return TTRC("CANbus J1939");
	if (p_iface == "bacnet_mstp")   return TTRC("BACnet MSTP");
	if (p_iface == "bacnet_ip")     return TTRC("BACnet/IP");
	if (p_iface == "iec_104")       return TTRC("IEC 104");
	if (p_iface == "iec_101")       return TTRC("IEC 101");
	if (p_iface == "hsms")          return TTRC("HSMS");
	if (p_iface == "secs_i")        return TTRC("SECS-I");
	if (p_iface == "ethercat")      return TTRC("EtherCAT");
	if (p_iface == "free")          return TTRC("Free Protocol");
	return p_iface;
}

String IndustrialNewDeviceDialog::_addressing_label(const String &p_mode) const {
	if (p_mode == "absolute") return TTRC("Absolute Addressing");
	if (p_mode == "symbolic") return TTRC("Symbolic Addressing");
	return p_mode;
}

void IndustrialNewDeviceDialog::_populate_interface_types() {
	if (!dev_interface_type) return;
	dev_interface_type->clear();

	Array catalog = IndustrialRuntimeClient::get_driver_catalog();
	if (catalog.is_empty()) {
		// Fallback: use the 81-entry hardcoded catalog's interface fields to
		// populate the same 5-group dropdown as the runtime path.
		bool group_has_driver[5] = { false, false, false, false, false };
		int total = industrial_get_driver_count();
		for (int i = 0; i < total; i++) {
			const DriverMeta *meta = industrial_get_driver_meta(i);
			if (meta) {
				group_has_driver[_interface_to_group(meta->interface)] = true;
			}
		}

		static const char *group_names[5] = {
			"Ethernet", "Serial", "Bus", "Industry", "Special"
		};
		int added = 0;
		for (int g = 0; g < 5; g++) {
			if (!group_has_driver[g]) continue;
			dev_interface_type->add_item(TTRC(group_names[g]));
			dev_interface_type->set_item_metadata(added, g);
			added++;
		}
		// Safety: if no group had drivers, fall back to a single "All".
		if (added == 0) {
			dev_interface_type->add_item(TTRC("All"));
			dev_interface_type->set_item_metadata(0, 0);
		}
		return;
	}

	// Find which groups actually have drivers so the dropdown only shows
	// non-empty categories.
	bool group_has_driver[5] = { false, false, false, false, false };
	for (int i = 0; i < catalog.size(); i++) {
		Dictionary d = catalog[i];
		String iface = d.get("category_interface", String());
		group_has_driver[_interface_to_group(iface)] = true;
	}

	static const char *group_names[5] = {
		"Ethernet", "Serial", "Bus", "Industry", "Special"
	};
	int added = 0;
	for (int g = 0; g < 5; g++) {
		if (!group_has_driver[g]) continue;
		dev_interface_type->add_item(TTRC(group_names[g]));
		dev_interface_type->set_item_metadata(added, g);
		added++;
	}
	// Safety: if no group had drivers, fall back to a single "All".
	if (added == 0) {
		dev_interface_type->add_item(TTRC("All"));
		dev_interface_type->set_item_metadata(0, 0);
	}
}

void IndustrialNewDeviceDialog::_populate_device_types(int p_group_idx) {
	if (!dev_device_type) return;
	dev_device_type->clear();

	Array catalog = IndustrialRuntimeClient::get_driver_catalog();
	if (catalog.is_empty()) {
		// Fallback: iterate the 81-entry hardcoded catalog, filter by the
		// selected interface group, and build the same 4-segment label as
		// the runtime path.
		int target_group = p_group_idx;
		if (target_group < 0 || target_group >= 5) target_group = 0;

		int total = industrial_get_driver_count();
		int added = 0;
		for (int i = 0; i < total; i++) {
			const DriverMeta *meta = industrial_get_driver_meta(i);
			if (!meta) continue;
			if (_interface_to_group(meta->interface) != target_group) continue;

			String vendor = meta->vendor;
			String disp_name = meta->display_name;
			String addr_mode = meta->addressing_mode;
			String iface = meta->interface;

			// Series part: strip the vendor prefix from display_name so the
			// 4-segment label isn't redundant (vendor is its own segment).
			String series = disp_name;
			if (vendor.length() > 0 && series.find(vendor) == 0) {
				series = series.substr(vendor.length());
				while (series.length() > 0 && (series[0] == ' ' || series[0] == '-')) {
					series = series.substr(1);
				}
			}

			String label = vformat("%s %s (%s) (%s)",
					vendor, series,
					_addressing_label(addr_mode),
					_interface_label(iface));

			dev_device_type->add_item(label);
			dev_device_type->set_item_metadata(added, i);
			added++;
		}
		return;
	}

	// When p_group_idx is out of range (e.g. fallback "All" group=0 in
	// _populate_interface_types() but runtime catalog exists with proper
	// groups), treat the first group as selected.
	int target_group = p_group_idx;
	if (target_group < 0 || target_group >= 5) target_group = 0;

	int added = 0;
	for (int i = 0; i < catalog.size(); i++) {
		Dictionary d = catalog[i];
		String iface = d.get("category_interface", String());
		if (_interface_to_group(iface) != target_group) continue;

		String vendor    = d.get("category_vendor", String());
		String disp_name = d.get("display_name", String());
		String mode      = d.get("addressing_mode", String());

		// Series part: strip the vendor prefix from display_name so the 4-segment
		// label isn't redundant (vendor is its own segment).
		String series = disp_name;
		if (vendor.length() > 0 && series.find(vendor) == 0) {
			series = series.substr(vendor.length());
			// Trim leading space / dash.
			while (series.length() > 0 && (series[0] == ' ' || series[0] == '-')) {
				series = series.substr(1);
			}
		}

		String label = vformat("%s %s (%s) (%s)",
				vendor, series,
				_addressing_label(mode),
				_interface_label(iface));

		dev_device_type->add_item(label);
		dev_device_type->set_item_metadata(added, i);
		added++;
	}
}

void IndustrialNewDeviceDialog::_on_interface_changed(int p_idx) {
	if (!dev_interface_type || p_idx < 0) return;
	int group = (int)dev_interface_type->get_item_metadata(p_idx);
	_populate_device_types(group);
	if (dev_device_type && dev_device_type->get_item_count() > 0) {
		dev_device_type->select(0);
		_on_device_type_changed(0);
	}
}

void IndustrialNewDeviceDialog::_on_device_type_changed(int p_idx) {
	if (!dev_device_type || p_idx < 0) {
		return;
	}
	int driver_idx = (int)dev_device_type->get_item_metadata(p_idx);
	_rebuild_params(driver_idx);
	current_driver_key = _resolve_driver_key(driver_idx);
}

void IndustrialNewDeviceDialog::_on_tagfield_fetched(bool /*p_success*/) {
	// Kept for catalog callbacks; initial tags UI removed.
}

// Resolve driver_idx → driver_key: prefer runtime catalog, else 81-entry fallback.
String IndustrialNewDeviceDialog::_resolve_driver_key(int p_driver_idx) const {
	String key = IndustrialRuntimeClient::get_driver_key(p_driver_idx);
	if (key.is_empty()) {
		key = industrial_get_driver_key(p_driver_idx);
	}
	return key;
}

void IndustrialNewDeviceDialog::_on_location_mode_changed(int /*p_idx*/) {
	_update_remote_hmi_visibility();
}

void IndustrialNewDeviceDialog::_update_remote_hmi_visibility() {
	if (!remote_hmi_row) {
		return;
	}
	bool show_remote = false;
	if (Control *widget = param_widgets.get("location_mode")) {
		if (OptionButton *ob = Object::cast_to<OptionButton>(widget)) {
			if (ob->get_selected() >= 0) {
				show_remote = ob->get_item_text(ob->get_selected()) == "Remote";
			}
		}
	}
	remote_hmi_row->set_visible(show_remote);
}

bool IndustrialNewDeviceDialog::_should_show_interface_field(const String &p_key, int p_iface_group) const {
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
	if (p_iface_group == 0) {
		return is_eth || (!is_eth && !is_serial);
	}
	if (p_iface_group == 1) {
		return is_serial || (!is_eth && !is_serial);
	}
	return true;
}

void IndustrialNewDeviceDialog::_rebuild_params(int p_driver_idx) {
	auto clear_container = [](VBoxContainer *p_container) {
		if (!p_container) {
			return;
		}
		while (p_container->get_child_count() > 0) {
			Node *child = p_container->get_child(0);
			p_container->remove_child(child);
			memdelete(child);
		}
	};

	clear_container(interface_params_container);
	clear_container(protocol_params_container);
	clear_container(tuning_params_container);
	param_widgets.clear();
	remote_hmi_row = nullptr;

	Vector<IndustrialFieldDef> fields = industrial_get_driver_fields(p_driver_idx);

	// Name/driver live in Basic Info; HMI-only EBPro extras stay out of this dialog.
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
	Vector<IndustrialFieldDef> visible_fields = industrial_filter_driver_fields(fields, kHiddenKeys, kHiddenKeyCount);

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

	int iface_group = 0;
	if (dev_interface_type && dev_interface_type->get_selected() >= 0) {
		iface_group = (int)dev_interface_type->get_item_metadata(dev_interface_type->get_selected());
	}

	for (int i = 0; i < visible_fields.size(); i++) {
		const IndustrialFieldDef &field = visible_fields[i];
		const IndustrialDeviceFieldGroup group = industrial_classify_device_field(field.key);
		if (group == IND_DEVICE_GROUP_INTERFACE && !_should_show_interface_field(field.key, iface_group)) {
			continue;
		}

		VBoxContainer *target = nullptr;
		switch (group) {
			case IND_DEVICE_GROUP_COMMON:
				// Basic-info / HMI extras are hidden via kHiddenKeys.
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

		const int before = target->get_child_count();
		industrial_add_param_row(target, field, Variant(), param_widgets, true);

		if (field.key == "location_mode") {
			if (Control *widget = param_widgets.get("location_mode")) {
				if (OptionButton *ob = Object::cast_to<OptionButton>(widget)) {
					ob->connect(SceneStringName(item_selected),
							callable_mp(this, &IndustrialNewDeviceDialog::_on_location_mode_changed));
				}
			}
		}
		if (field.key == "remote_hmi_ip" && target->get_child_count() > before) {
			remote_hmi_row = Object::cast_to<Control>(target->get_child(target->get_child_count() - 1));
		}
	}

	_update_remote_hmi_visibility();
}

void IndustrialNewDeviceDialog::_collect_params(Dictionary &r_params) {
	industrial_collect_param_widgets(param_widgets, r_params);
}

void IndustrialNewDeviceDialog::_on_confirm_pressed() {
	if (project.is_null()) {
		hide();
		return;
	}

	IndustrialDeviceData dev;
	if (dev_name) dev.name = dev_name->get_text();
	if (dev_desc) dev.description = dev_desc->get_text();
	if (dev_device_type && dev_device_type->get_selected() >= 0) {
		dev.driver = (int)dev_device_type->get_item_metadata(dev_device_type->get_selected());
	} else {
		dev.driver = 0;
	}
	// EBPro 搂2.1 +0xc44: new devices are ENABLED by default.  If user wants to
	// disable before first connect, they do so in Device Dock after creation.
	dev.enabled = true;

	// Collect UI params → flat fields + options.
	Dictionary params;
	_collect_params(params);

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
	int driver_idx = dev.driver;
	if (driver_idx < 0) {
		driver_idx = 0;
	}
	industrial_backfill_param_defaults(driver_idx, params, kHiddenKeys, kHiddenKeyCount);
	industrial_apply_param_dict_to_device(dev, params);
	industrial_sync_device_connection_params(dev);
	dev.driver_key = _resolve_driver_key(dev.driver);

	bool ok = project->add_device(dev);
	if (!ok) {
		created_index = -1;
		hide();
		return;
	}
	created_index = project->get_device_count() - 1;

	hide();
	// AcceptDialog's default ok-button wiring would normally emit "confirmed"
	// for us, but we call hide() directly above, which short-circuits the AcceptDialog's
	// accept() flow.  Explicitly fire "confirmed" here so the plugin can
	// refresh docks and, crucially, so TAG_NEW next click sees the device.
	call_deferred(SNAME("emit_signal"), SNAME("confirmed"));
}
