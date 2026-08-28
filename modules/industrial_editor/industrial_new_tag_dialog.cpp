#include "industrial_new_tag_dialog.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"
#include "industrial_runtime_client.h"

#include "editor/editor_string_names.h"
#include "core/object/callable_mp.h"
#include "scene/gui/box_container.h"
#include "scene/gui/separator.h"

IndustrialNewTagDialog::IndustrialNewTagDialog() {
	_build_ui();
}

IndustrialNewTagDialog::~IndustrialNewTagDialog() {}

void IndustrialNewTagDialog::_bind_methods() {}

void IndustrialNewTagDialog::_notification(int p_what) {}

void IndustrialNewTagDialog::_build_ui() {
	set_title(TTRC("New Tag"));
	set_min_size(Size2(420, 340));

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(main);

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Name:")));
		lbl->set_custom_minimum_size(Size2(110, 0));
		row->add_child(lbl);
		tag_name = memnew(LineEdit);
		tag_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(tag_name);
		main->add_child(row);
	}
	// ---- Address block -------------------------------------------------
	// Split into two rows, mimicking EBPro's layout:
	//   Row1: Address Mode (位/字) × Address Type (IB/IW/… 或 0x_Coil 等)
	//   Row2: [hidden in symbolic mode] Address Number
	// Symbolic drivers use a single field named "PLC Tag Symbol Name" and
	// hide address_type + address_mode dropdowns.
	{
		HBoxContainer *row_mode_type = memnew(HBoxContainer);
		{
			Label *lbl_mode = memnew(Label(TTRC("Address Mode:")));
			lbl_mode->set_custom_minimum_size(Size2(110, 0));
			row_mode_type->add_child(lbl_mode);

			tag_address_mode = memnew(OptionButton);
			tag_address_mode->set_custom_minimum_size(Size2(120, 0));
			tag_address_mode->add_item(TTRC("Word"), 0); // id=word  index 0
			tag_address_mode->set_item_metadata(0, String("word"));
			tag_address_mode->add_item(TTRC("Bit"), 1);  // id=bit   index 1
			tag_address_mode->set_item_metadata(1, String("bit"));
			row_mode_type->add_child(tag_address_mode);

			tag_address_type_label = memnew(Label(TTRC("Address Type:")));
			tag_address_type_label->set_custom_minimum_size(Size2(110, 0));
			row_mode_type->add_child(tag_address_type_label);

			tag_address_type = memnew(OptionButton);
			tag_address_type->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			row_mode_type->add_child(tag_address_type);
		}
		main->add_child(row_mode_type);

		HBoxContainer *row_number = memnew(HBoxContainer);
		{
			tag_address_number_label = memnew(Label(TTRC("Address No.:")));
			tag_address_number_label->set_custom_minimum_size(Size2(110, 0));
			row_number->add_child(tag_address_number_label);

			tag_address_number = memnew(LineEdit);
			tag_address_number->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			tag_address_number->set_placeholder(TTRC("e.g. 0, DB1, DB1.DBX0.1, or symbolic tag name"));
			row_number->add_child(tag_address_number);
		}
		main->add_child(row_number);

		// Legacy tag_address is now a computed/hidden field built from
		// address_mode+address_type+address_number at submit time.  Keep it
		// attached to the tree so existing API (tag_address->get_text())
		// still works for downstream callers that read `tag_info.address`.
		tag_address = memnew(LineEdit);
		tag_address->set_visible(false);
		main->add_child(tag_address);
	}

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Data Type:")));
		lbl->set_custom_minimum_size(Size2(110, 0));
		row->add_child(lbl);
		tag_type = memnew(OptionButton);
		StringList types = industrial_get_data_type_names();
		for (int i = 0; i < types.size(); i++) {
			tag_type->add_item(types[i]);
		}
		tag_type->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(tag_type);
		main->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		tag_writable = memnew(CheckButton);
		tag_writable->set_text(TTRC("Writable"));
		tag_writable->set_pressed(true);
		row->add_child(tag_writable);
		main->add_child(row);
	}

	main->add_child(memnew(HSeparator));

	// NOTE: Scan Group intentionally removed from the NEW TAG dialog per EBPro
	// UI.  It is still a supported field on IndustrialTagData and editable
	// afterwards in Tag Dock's side editor / bulk property panel.

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl_scale = memnew(Label(TTRC("Scale:")));
		lbl_scale->set_custom_minimum_size(Size2(110, 0));
		row->add_child(lbl_scale);
		tag_scale = memnew(SpinBox);
		tag_scale->set_value(1.0);
		tag_scale->set_min(-10000);
		tag_scale->set_max(10000);
		tag_scale->set_step(0.01);
		tag_scale->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(tag_scale);
		main->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl_unit = memnew(Label(TTRC("Unit:")));
		lbl_unit->set_custom_minimum_size(Size2(110, 0));
		row->add_child(lbl_unit);
		tag_unit = memnew(LineEdit);
		tag_unit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		tag_unit->set_placeholder(TTRC("e.g. degC, kPa"));
		row->add_child(tag_unit);
		main->add_child(row);
	}

	get_ok_button()->connect(SceneStringName(pressed), callable_mp(this, &IndustrialNewTagDialog::_on_confirm));
}

void IndustrialNewTagDialog::set_project(Ref<IndustrialProject> p_project) {
	project = p_project;
	tag_index = -1;
	set_title(TTRC("New Tag"));

	if (tag_name) tag_name->set_text("");
	if (tag_address) tag_address->set_text("DB0.DBW0");
	if (tag_address_number) {
		tag_address_number->set_text("");
		tag_address_number->set_placeholder(TTRC("e.g. 0, DB1, DB1.DBX0.1, or symbolic tag name"));
	}
	if (tag_type) tag_type->select(TYPE_INT16);
	if (tag_writable) tag_writable->set_pressed(true);
	if (tag_scale) tag_scale->set_value(1.0);
	if (tag_unit) tag_unit->set_text("");
}

void IndustrialNewTagDialog::set_device_index(int p_device_index) {
	if (p_device_index == device_index) {
		// Still force a refresh the first time the UI has never been filled.
		if (tag_address_type && tag_address_type->get_item_count() > 0) return;
	}
	device_index = p_device_index;
	_on_device_index_changed(p_device_index);
}

void IndustrialNewTagDialog::_on_device_index_changed(int p_idx) {
	// Resolve the driver_key for the selected device.
	// p_idx = project-device ordinal (0 = first device created).  The actual
	// driver-catalog index lives in IndustrialDeviceData::driver, so we first
	// resolve device_idx → catalog_idx through the project (fallback: use
	// p_idx itself for legacy / no-project paths).
	int catalog_idx = p_idx;
	if (project.is_valid() && p_idx >= 0 && p_idx < project->get_device_count()) {
		catalog_idx = project->get_device(p_idx).driver;
	}
	String driver_key = IndustrialRuntimeClient::get_driver_key(catalog_idx);
	// Legacy fallback (7 drivers).  Keep this so the dialog still displays
	// address types before the user has created a device or when the Go
	// runtime is not reachable.
	static const char *legacy_keys[] = {
		"siemens_s7_1200_1500",   // S7-1200/1500
		"modbus_rtu_rs485",       // Modbus RTU
		"modbus_tcp_ascii_ethernet", // Modbus TCP
		"bacnet_mstp_rs485",      // BACnet MSTP
		"secs_gem_rs232",         // SECS GEM
		"mitsubishi_fx5u_slmp_ethernet", // Mitsubishi FX
		"omron_cs_cj_cp_fins_ethernet",  // OMRON FINS
	};
	if (driver_key.is_empty() && p_idx >= 0 && p_idx < (int)(sizeof(legacy_keys)/sizeof(legacy_keys[0]))) {
		driver_key = legacy_keys[p_idx];
	}
	if (driver_key.is_empty()) {
		// Unknown driver — still need a non-empty dropdown, show modbus-like.
		driver_key = "modbus_tcp_ascii_ethernet";
	}

	// 1) Immediately show sync fallback result so the UI never flashes empty.
	_on_tagfield_fetched_for_current(false);

	// 2) Kick off async HTTP fetch (per-driver cache inside the client).
	Callable cb = callable_mp(this, &IndustrialNewTagDialog::_on_tagfield_fetched_for_current);
	IndustrialRuntimeClient::fetch_tag_field_catalog(driver_key, this, cb);
}

void IndustrialNewTagDialog::_on_tagfield_fetched_for_current(bool /*p_success*/) {
	// Re-derive driver_key the same way as _on_device_index_changed().
	int catalog_idx = device_index;
	if (project.is_valid() && device_index >= 0 && device_index < project->get_device_count()) {
		catalog_idx = project->get_device(device_index).driver;
	}
	String driver_key = IndustrialRuntimeClient::get_driver_key(catalog_idx);
	static const char *legacy_keys[] = {
		"siemens_s7_1200_1500", "modbus_rtu_rs485", "modbus_tcp_ascii_ethernet",
		"bacnet_mstp_rs485", "secs_gem_rs232", "mitsubishi_fx5u_slmp_ethernet",
		"omron_cs_cj_cp_fins_ethernet",
	};
	if (driver_key.is_empty() && device_index >= 0 && device_index < (int)(sizeof(legacy_keys)/sizeof(legacy_keys[0]))) {
		driver_key = legacy_keys[device_index];
	}
	if (driver_key.is_empty()) driver_key = "modbus_tcp_ascii_ethernet";

	Dictionary cat = IndustrialRuntimeClient::get_tag_field_catalog(driver_key);
	String addr_mode = cat.get("addressing_mode", Variant("absolute"));
	bool symbolic = (addr_mode == "symbolic");

	// ---- Fill address mode dropdown ----
	if (tag_address_mode) {
		tag_address_mode->clear();
		Array amodes = cat.get("address_modes_ui", Array());
		for (int i = 0; i < amodes.size(); i++) {
			Dictionary m = amodes[i];
			String id = m.get("id", Variant(""));
			String msgid = m.get("label_msgid", Variant(""));
			String zh = m.get("label_zh", Variant(""));
			String display = msgid.is_empty() ? id : atr(msgid);
			if (zh.length() > 0) display += "  (" + zh + ")";
			tag_address_mode->add_item(display, i);
			tag_address_mode->set_item_metadata(i, id);
		}
		if (tag_address_mode->get_item_count() > 0) tag_address_mode->select(0);
	}

	// ---- Fill address type dropdown (absolute mode) OR hide it (symbolic) ----
	if (tag_address_type) {
		tag_address_type->clear();
	}
	address_type_ids.clear();
	if (symbolic) {
		// Symbolic → repurpose address_type dropdown into "Symbol driver"
		// indicator with a single hint entry, and hide Address Mode +
		// Address Type combo; replace the Label with "Tag Symbol Name:".
		if (tag_address_type_label) {
			tag_address_type_label->set_text(TTRC("Tag Symbol Name:"));
		}
		if (tag_address_mode) {
			tag_address_mode->set_visible(false);
		}
		if (tag_address_type) {
			tag_address_type->set_visible(false);
		}
		if (tag_address_number_label) {
			tag_address_number_label->set_text(TTRC("PLC Tag:"));
		}
		// Hints line: take the first tag_name_hints and use it as placeholder.
		Dictionary sym = cat.get("symbolic_import", Dictionary());
		Array hints = sym.get("tag_name_hints", Array());
		String placeholder;
		for (int i = 0; i < hints.size(); i++) {
			if (placeholder.length() > 0) placeholder += " / ";
			placeholder += String(hints[i]);
		}
		if (tag_address_number) {
			tag_address_number->set_placeholder(placeholder.length() > 0
					? placeholder : TTRC("Enter PLC symbolic tag name"));
		}
		// Expose project file import ext info in type dropdown — as a single
		// info entry.  This matches EBPro's "(Symbolic)" hint in the list.
		if (tag_address_type) {
			String label = TTRC("(Symbolic driver — import PLC tag list)");
			tag_address_type->add_item(label);
			tag_address_type->set_item_metadata(0, String("symbolic"));
			tag_address_type->select(0);
			address_type_ids.append("symbolic");
		}
	} else {
		// Absolute → restore labels & visibility, then fill AT dropdown.
		if (tag_address_type_label) {
			tag_address_type_label->set_text(TTRC("Address Type:"));
		}
		if (tag_address_mode) {
			tag_address_mode->set_visible(true);
		}
		if (tag_address_type) {
			tag_address_type->set_visible(true);
		}
		if (tag_address_number_label) {
			tag_address_number_label->set_text(TTRC("Address No.:"));
		}
		if (tag_address_number) {
			tag_address_number->set_placeholder(TTRC("e.g. 0, DB1, DB1.DBX0.1"));
		}

		PackedStringArray labels = IndustrialRuntimeClient::get_address_type_labels(driver_key);
		PackedStringArray ids    = IndustrialRuntimeClient::get_address_type_ids(driver_key);
		address_type_ids = ids;
		if (tag_address_type) {
			for (int i = 0; i < labels.size(); i++) {
				tag_address_type->add_item(labels[i], i);
				Variant meta;
				if (i < ids.size()) meta = ids[i];
				else                 meta = String("");
				tag_address_type->set_item_metadata(i, meta);
			}
			if (tag_address_type->get_item_count() > 0) tag_address_type->select(0);
		}
	}
}

void IndustrialNewTagDialog::edit_tag(int p_tag_index) {
	if (project.is_null() || device_index < 0) return;

	const auto &tag = project->get_device(device_index).tags[p_tag_index];
	tag_index = p_tag_index;
	set_title(TTRC("Edit Tag"));

	if (tag_name) tag_name->set_text(tag.name);
	// On edit, split the legacy "DB1.DBW0" style address back into
	// (address_type + address_number) by consulting the current catalog.
	// If we cannot split we keep the raw string in tag_address_number so
	// users can always inspect it manually.
	if (tag_address_number) {
		tag_address_number->set_text(tag.address);
	}
	if (tag_address) tag_address->set_text(tag.address);
	if (tag_type && tag.data_type >= 0 && tag.data_type < TYPE_MAX) tag_type->select(tag.data_type);
	if (tag_writable) tag_writable->set_pressed(tag.writable);
	if (tag_scale) tag_scale->set_value(tag.scale);
	if (tag_unit) tag_unit->set_text(tag.unit);
}

void IndustrialNewTagDialog::_on_confirm() {
	if (project.is_null() || device_index < 0) {
		hide();
		return;
	}

	// Resolve driver + addressing mode.
	String driver_key = IndustrialRuntimeClient::get_driver_key(device_index);
	static const char *legacy_keys_confirm[] = {
		"siemens_s7_1200_1500", "modbus_rtu_rs485", "modbus_tcp_ascii_ethernet",
		"bacnet_mstp_rs485", "secs_gem_rs232", "mitsubishi_fx5u_slmp_ethernet",
		"omron_cs_cj_cp_fins_ethernet",
	};
	if (driver_key.is_empty() && device_index >= 0 && device_index < (int)(sizeof(legacy_keys_confirm)/sizeof(legacy_keys_confirm[0]))) {
		driver_key = legacy_keys_confirm[device_index];
	}
	if (driver_key.is_empty()) driver_key = "modbus_tcp_ascii_ethernet";
	Dictionary cat = IndustrialRuntimeClient::get_tag_field_catalog(driver_key);
	bool symbolic = (String(cat.get("addressing_mode", Variant("absolute"))) == "symbolic");

	// Build the canonical stored address.  For symbolic drivers this is just
	// the user-entered PLC tag symbol name; for absolute drivers we store
	// "ATid.address_number" so the runtime driver can parse it back.
	String composed_address;
	if (symbolic) {
		composed_address = tag_address_number ? tag_address_number->get_text().strip_edges() : String();
	} else {
		String at_id;
		if (tag_address_type) {
			int sel = tag_address_type->get_selected();
			if (sel >= 0 && sel < address_type_ids.size()) {
				at_id = address_type_ids[sel];
			}
		}
		String num = tag_address_number ? tag_address_number->get_text().strip_edges() : String();
		if (at_id.length() > 0 && num.length() > 0) {
			composed_address = at_id + "." + num;
		} else if (num.length() > 0) {
			composed_address = num;   // fall back to raw (hand-edited case)
		} else {
			composed_address = at_id;
		}
	}
	// Keep the hidden legacy LineEdit in sync so downstream code that reads
	// tag_address->get_text() (and any stored tag_info.address path) keeps
	// working without modification.
	if (tag_address) tag_address->set_text(composed_address);

	IndustrialTagData tag;
	if (tag_name) tag.name = tag_name->get_text();
	tag.address = composed_address;
	if (tag_type) tag.data_type = tag_type->get_selected();
	if (tag_writable) tag.writable = tag_writable->is_pressed();
	if (tag_scale) tag.scale = tag_scale->get_value();
	if (tag_unit) tag.unit = tag_unit->get_text();

	if (tag_index < 0) {
		project->add_tag(device_index, tag);
	} else {
		project->update_tag(device_index, tag_index, tag);
	}

	hide();
}
