#include "industrial_new_tag_dialog.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"
#include "industrial_runtime_client.h"

#include "editor/editor_string_names.h"
#include "core/object/callable_mp.h"
#include "scene/gui/box_container.h"
#include "scene/gui/separator.h"

namespace {

String legacy_format_from_type(int p_type) {
	switch (p_type) {
		case TYPE_BOOL:
			return "bit";
		case TYPE_INT16:
			return "i16";
		case TYPE_INT32:
			return "i32";
		case TYPE_UINT16:
			return "u16";
		case TYPE_UINT32:
			return "u32";
		case TYPE_BYTE:
			return "u8";
		case TYPE_FLOAT:
		case TYPE_REAL32:
			return "f32";
		case TYPE_DOUBLE:
		case TYPE_REAL64:
			return "f64";
		case TYPE_BCD16:
			return "bcd16";
		case TYPE_BCD32:
			return "bcd32";
		default:
			return String();
	}
}

} // namespace

IndustrialNewTagDialog::IndustrialNewTagDialog() {
	_build_ui();
}

IndustrialNewTagDialog::~IndustrialNewTagDialog() {}

void IndustrialNewTagDialog::_bind_methods() {}

void IndustrialNewTagDialog::_notification(int p_what) {
	(void)p_what;
}

void IndustrialNewTagDialog::_build_ui() {
	set_title(TTRC("New Tag"));
	set_min_size(Size2(480, 420));

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
	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Description:")));
		lbl->set_custom_minimum_size(Size2(110, 0));
		row->add_child(lbl);
		tag_description = memnew(LineEdit);
		tag_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(tag_description);
		main->add_child(row);
	}

	absolute_section = memnew(VBoxContainer);
	main->add_child(absolute_section);

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl_mode = memnew(Label(TTRC("Address Mode:")));
		lbl_mode->set_custom_minimum_size(Size2(110, 0));
		row->add_child(lbl_mode);

		tag_address_mode = memnew(OptionButton);
		tag_address_mode->set_custom_minimum_size(Size2(120, 0));
		tag_address_mode->connect(SceneStringName(item_selected),
				callable_mp(this, &IndustrialNewTagDialog::_on_address_mode_changed));
		row->add_child(tag_address_mode);

		tag_address_type_label = memnew(Label(TTRC("Address Type:")));
		tag_address_type_label->set_custom_minimum_size(Size2(110, 0));
		row->add_child(tag_address_type_label);

		tag_address_type = memnew(OptionButton);
		tag_address_type->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		tag_address_type->connect(SceneStringName(item_selected),
				callable_mp(this, &IndustrialNewTagDialog::_on_address_type_changed));
		row->add_child(tag_address_type);
		absolute_section->add_child(row);
	}

	{
		HBoxContainer *row = memnew(HBoxContainer);
		tag_data_format_label = memnew(Label(TTRC("Data Format:")));
		tag_data_format_label->set_custom_minimum_size(Size2(110, 0));
		row->add_child(tag_data_format_label);

		tag_data_format = memnew(OptionButton);
		tag_data_format->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(tag_data_format);
		absolute_section->add_child(row);
	}

	{
		HBoxContainer *row = memnew(HBoxContainer);
		tag_offset_label = memnew(Label(TTRC("Address:")));
		tag_offset_label->set_custom_minimum_size(Size2(110, 0));
		row->add_child(tag_offset_label);

		tag_offset = memnew(LineEdit);
		tag_offset->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		tag_offset->set_placeholder(TTRC("Offset / numeric part only, e.g. 0"));
		row->add_child(tag_offset);
		absolute_section->add_child(row);
	}

	row_db_length = memnew(HBoxContainer);
	{
		tag_db_number_label = memnew(Label(TTRC("DB Number:")));
		tag_db_number_label->set_custom_minimum_size(Size2(110, 0));
		row_db_length->add_child(tag_db_number_label);

		tag_db_number = memnew(SpinBox);
		tag_db_number->set_min(0);
		tag_db_number->set_max(65535);
		tag_db_number->set_custom_minimum_size(Size2(120, 0));
		row_db_length->add_child(tag_db_number);

		tag_length_label = memnew(Label(TTRC("Length:")));
		tag_length_label->set_custom_minimum_size(Size2(110, 0));
		row_db_length->add_child(tag_length_label);

		tag_length = memnew(SpinBox);
		tag_length->set_min(0);
		tag_length->set_max(65535);
		tag_length->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row_db_length->add_child(tag_length);
	}
	absolute_section->add_child(row_db_length);

	symbolic_section = memnew(VBoxContainer);
	symbolic_section->set_visible(false);
	main->add_child(symbolic_section);

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("PLC Tag / Symbol:")));
		lbl->set_custom_minimum_size(Size2(110, 0));
		row->add_child(lbl);

		tag_symbol = memnew(LineEdit);
		tag_symbol->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		tag_symbol->set_placeholder(TTRC("Enter PLC symbolic tag path"));
		row->add_child(tag_symbol);
		symbolic_section->add_child(row);
	}
	{
		HBoxContainer *row = memnew(HBoxContainer);
		tag_symbolic_data_format_label = memnew(Label(TTRC("Data Format:")));
		tag_symbolic_data_format_label->set_custom_minimum_size(Size2(110, 0));
		row->add_child(tag_symbolic_data_format_label);

		tag_symbolic_data_format = memnew(OptionButton);
		tag_symbolic_data_format->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(tag_symbolic_data_format);
		symbolic_section->add_child(row);
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

void IndustrialNewTagDialog::_resolve_driver_key(String &r_key, int &r_catalog_idx) const {
	r_catalog_idx = device_index;
	if (project.is_valid() && device_index >= 0 && device_index < project->get_device_count()) {
		const IndustrialDeviceData &dev = project->get_device(device_index);
		r_catalog_idx = dev.driver;
		if (!dev.driver_key.is_empty()) {
			r_key = dev.driver_key;
			return;
		}
	}
	r_key = IndustrialRuntimeClient::get_driver_key(r_catalog_idx);
	static const char *legacy_keys[] = {
		"siemens_s7_1200_1500",
		"modbus_rtu_rs485",
		"modbus_tcp_ascii_ethernet",
		"bacnet_mstp_rs485",
		"secs_gem_rs232",
		"mitsubishi_fx5u_slmp_ethernet",
		"omron_cs_cj_cp_fins_ethernet",
	};
	if (r_key.is_empty() && device_index >= 0 && device_index < (int)(sizeof(legacy_keys) / sizeof(legacy_keys[0]))) {
		r_key = legacy_keys[device_index];
	}
	if (r_key.is_empty()) {
		r_key = "modbus_tcp";
	}
}

int IndustrialNewTagDialog::_legacy_type_from_data_format(const String &p_format) {
	if (p_format == "bit" || p_format == "bool") {
		return TYPE_BOOL;
	}
	if (p_format == "i16" || p_format == "int16") {
		return TYPE_INT16;
	}
	if (p_format == "i32" || p_format == "int32") {
		return TYPE_INT32;
	}
	if (p_format == "u16" || p_format == "uint16") {
		return TYPE_UINT16;
	}
	if (p_format == "u32" || p_format == "uint32") {
		return TYPE_UINT32;
	}
	if (p_format == "u8" || p_format == "byte") {
		return TYPE_BYTE;
	}
	if (p_format == "f32" || p_format == "float" || p_format == "real32") {
		return TYPE_FLOAT;
	}
	if (p_format == "f64" || p_format == "double" || p_format == "real64") {
		return TYPE_DOUBLE;
	}
	if (p_format == "bcd16") {
		return TYPE_BCD16;
	}
	if (p_format == "bcd32") {
		return TYPE_BCD32;
	}
	return -1;
}

void IndustrialNewTagDialog::_apply_layout_for_mode(bool p_symbolic) {
	if (absolute_section) {
		absolute_section->set_visible(!p_symbolic);
	}
	if (symbolic_section) {
		symbolic_section->set_visible(p_symbolic);
	}
}

void IndustrialNewTagDialog::_fill_data_formats(OptionButton *p_btn, PackedStringArray &p_ids, bool p_include_bit) {
	if (!p_btn) {
		return;
	}
	p_btn->clear();
	p_ids.clear();

	Dictionary cat = IndustrialRuntimeClient::get_tag_field_catalog(current_driver_key);
	Array formats = cat.get("data_formats", Array());
	for (int i = 0; i < formats.size(); i++) {
		Dictionary df = formats[i];
		String id = df.get("id", Variant(""));
		if (id.is_empty()) {
			continue;
		}
		if (!p_include_bit && id == "bit") {
			continue;
		}
		String msgid = df.get("label_msgid", Variant(""));
		String zh = df.get("label_zh", Variant(""));
		String display = msgid.is_empty() ? id : atr(msgid);
		if (zh.length() > 0) {
			display += "  (" + zh + ")";
		}
		const int idx = p_btn->get_item_count();
		p_btn->add_item(display, idx);
		p_btn->set_item_metadata(idx, id);
		p_ids.append(id);
	}
}

void IndustrialNewTagDialog::_fill_address_types() {
	if (!tag_address_type) {
		return;
	}
	tag_address_type->clear();
	address_type_ids.clear();

	PackedStringArray labels = IndustrialRuntimeClient::get_address_type_labels(current_driver_key);
	PackedStringArray ids = IndustrialRuntimeClient::get_address_type_ids(current_driver_key);
	address_type_ids = ids;
	for (int i = 0; i < labels.size(); i++) {
		tag_address_type->add_item(labels[i], i);
		Variant meta = i < ids.size() ? Variant(ids[i]) : Variant(String());
		tag_address_type->set_item_metadata(i, meta);
	}
	if (tag_address_type->get_item_count() > 0) {
		tag_address_type->select(0);
	}
}

Dictionary IndustrialNewTagDialog::_lookup_address_type(const String &p_id) const {
	if (p_id.is_empty()) {
		return Dictionary();
	}
	Dictionary cat = IndustrialRuntimeClient::get_tag_field_catalog(current_driver_key);
	Array arr = cat.get("address_types", Array());
	for (int i = 0; i < arr.size(); i++) {
		Dictionary at = arr[i];
		if (String(at.get("id", Variant(""))) == p_id) {
			return at;
		}
	}
	return Dictionary();
}

void IndustrialNewTagDialog::_select_option_by_metadata(OptionButton *p_btn, const String &p_id) const {
	if (!p_btn || p_id.is_empty()) {
		return;
	}
	for (int i = 0; i < p_btn->get_item_count(); i++) {
		if (String(p_btn->get_item_metadata(i)) == p_id) {
			p_btn->select(i);
			return;
		}
	}
}

void IndustrialNewTagDialog::_select_option_by_id(PackedStringArray &p_ids, OptionButton *p_btn, const String &p_id) const {
	if (!p_btn || p_id.is_empty()) {
		return;
	}
	for (int i = 0; i < p_ids.size(); i++) {
		if (p_ids[i] == p_id) {
			p_btn->select(i);
			return;
		}
	}
}

String _current_address_mode_id(const OptionButton *p_mode) {
	if (!p_mode || p_mode->get_item_count() == 0) {
		return "word";
	}
	const int sel = p_mode->get_selected();
	if (sel < 0) {
		return "word";
	}
	return String(p_mode->get_item_metadata(sel));
}

void IndustrialNewTagDialog::_fill_symbolic_data_format_dropdown(const String &p_preselect) {
	if (!tag_symbolic_data_format) {
		return;
	}
	tag_symbolic_data_format->clear();
	symbolic_data_format_ids.clear();

	tag_symbolic_data_format->add_item(TTRC("(Auto / infer from PLC)"), 0);
	tag_symbolic_data_format->set_item_metadata(0, String());

	Dictionary cat = IndustrialRuntimeClient::get_tag_field_catalog(current_driver_key);
	Array formats = cat.get("data_formats", Array());
	for (int i = 0; i < formats.size(); i++) {
		Dictionary df = formats[i];
		String id = df.get("id", Variant(""));
		if (id.is_empty() || id == "bit") {
			continue;
		}
		String msgid = df.get("label_msgid", Variant(""));
		String zh = df.get("label_zh", Variant(""));
		String display = msgid.is_empty() ? id : atr(msgid);
		if (zh.length() > 0) {
			display += "  (" + zh + ")";
		}
		const int idx = tag_symbolic_data_format->get_item_count();
		tag_symbolic_data_format->add_item(display, idx);
		tag_symbolic_data_format->set_item_metadata(idx, id);
		symbolic_data_format_ids.append(id);
	}

	if (!p_preselect.is_empty()) {
		_select_option_by_metadata(tag_symbolic_data_format, p_preselect);
	} else {
		tag_symbolic_data_format->select(0);
	}
}

void IndustrialNewTagDialog::_update_conditional_fields() {
	const String mode_id = _current_address_mode_id(tag_address_mode);
	const bool bit_mode = (mode_id == "bit");

	if (tag_data_format_label) {
		tag_data_format_label->set_visible(!bit_mode);
	}
	if (tag_data_format) {
		tag_data_format->set_visible(!bit_mode);
	}

	String at_id;
	if (tag_address_type) {
		const int sel = tag_address_type->get_selected();
		if (sel >= 0 && sel < address_type_ids.size()) {
			at_id = address_type_ids[sel];
		}
	}
	const Dictionary at = _lookup_address_type(at_id);
	const bool requires_db = at.get("requires_db", false);
	const bool has_length = at.get("has_length", false);

	if (row_db_length) {
		row_db_length->set_visible(requires_db || has_length);
	}
	if (tag_db_number_label) {
		tag_db_number_label->set_visible(requires_db);
	}
	if (tag_db_number) {
		tag_db_number->set_visible(requires_db);
	}
	if (tag_length_label) {
		tag_length_label->set_visible(has_length);
	}
	if (tag_length) {
		tag_length->set_visible(has_length);
	}

	if (!bit_mode && tag_data_format && !at.is_empty()) {
		const String default_fmt = at.get("default_data_format", Variant(""));
		if (!default_fmt.is_empty()) {
			_select_option_by_id(data_format_ids, tag_data_format, default_fmt);
		}
	}
}

void IndustrialNewTagDialog::_on_address_mode_changed(int /*p_idx*/) {
	_update_conditional_fields();
}

void IndustrialNewTagDialog::_on_address_type_changed(int /*p_idx*/) {
	_update_conditional_fields();
}

void IndustrialNewTagDialog::_populate_from_tag(const IndustrialTagData &p_tag) {
	IndustrialTagData tag_data = p_tag;

	Dictionary cat = IndustrialRuntimeClient::get_tag_field_catalog(current_driver_key);
	const bool symbolic = (String(cat.get("addressing_mode", Variant("absolute"))) == "symbolic") ||
			tag_data.schema == "symbolic" ||
			(!tag_data.symbol.is_empty() && tag_data.address_type.is_empty());

	if (!symbolic && tag_data.address_type.is_empty() && !tag_data.address.is_empty()) {
		String addr = tag_data.address.strip_edges();
		int split_at = addr.find(" ");
		if (split_at < 0) {
			split_at = addr.find(".");
		}
		if (split_at > 0) {
			const String maybe_type = addr.substr(0, split_at);
			if (!_lookup_address_type(maybe_type).is_empty()) {
				tag_data.address_type = maybe_type;
				tag_data.address = addr.substr(split_at + 1).strip_edges();
			}
		}
	}

	if (tag_name) {
		tag_name->set_text(tag_data.name);
	}
	if (tag_description) {
		tag_description->set_text(tag_data.description);
	}
	if (tag_writable) {
		tag_writable->set_pressed(tag_data.writable);
	}
	if (tag_scale) {
		tag_scale->set_value(tag_data.scale);
	}
	if (tag_unit) {
		tag_unit->set_text(tag_data.unit);
	}

	_apply_layout_for_mode(symbolic);

	if (symbolic) {
		if (tag_symbol) {
			tag_symbol->set_text(!tag_data.symbol.is_empty() ? tag_data.symbol : tag_data.address);
		}
		_fill_symbolic_data_format_dropdown(tag_data.data_format);
		return;
	}

	if (tag_address_mode) {
		if (!tag_data.address_mode.is_empty()) {
			_select_option_by_metadata(tag_address_mode, tag_data.address_mode);
		}
	}

	_fill_address_types();
	if (!tag_data.address_type.is_empty()) {
		_select_option_by_id(address_type_ids, tag_address_type, tag_data.address_type);
	}

	_fill_data_formats(tag_data_format, data_format_ids, true);
	if (!tag_data.data_format.is_empty()) {
		_select_option_by_id(data_format_ids, tag_data_format, tag_data.data_format);
	} else if (tag_data.data_type >= 0 && tag_data.data_type < TYPE_MAX) {
		const String legacy_fmt = legacy_format_from_type(tag_data.data_type);
		if (!legacy_fmt.is_empty()) {
			_select_option_by_id(data_format_ids, tag_data_format, legacy_fmt);
		}
	}

	if (tag_offset) {
		tag_offset->set_text(tag_data.address);
	}
	if (tag_db_number) {
		tag_db_number->set_value(tag_data.db_number);
	}
	if (tag_length) {
		tag_length->set_value(tag_data.length);
	}

	_update_conditional_fields();
}

void IndustrialNewTagDialog::set_project(Ref<IndustrialProject> p_project) {
	project = p_project;
	tag_index = -1;
	pending_tag_populate = false;
	set_title(TTRC("New Tag"));

	if (tag_name) {
		tag_name->set_text("");
	}
	if (tag_description) {
		tag_description->set_text("");
	}
	if (tag_offset) {
		tag_offset->set_text("0");
	}
	if (tag_symbol) {
		tag_symbol->set_text("");
	}
	if (tag_writable) {
		tag_writable->set_pressed(true);
	}
	if (tag_scale) {
		tag_scale->set_value(1.0);
	}
	if (tag_unit) {
		tag_unit->set_text("");
	}
	if (tag_db_number) {
		tag_db_number->set_value(0);
	}
	if (tag_length) {
		tag_length->set_value(0);
	}
}

void IndustrialNewTagDialog::set_device_index(int p_device_index) {
	if (p_device_index == device_index) {
		if (tag_address_type && tag_address_type->get_item_count() > 0) {
			return;
		}
	}
	device_index = p_device_index;
	_on_device_index_changed(p_device_index);
}

void IndustrialNewTagDialog::_on_device_index_changed(int p_idx) {
	int catalog_idx = p_idx;
	_resolve_driver_key(current_driver_key, catalog_idx);
	(void)catalog_idx;

	_on_tagfield_fetched_for_current(false);

	Callable cb = callable_mp(this, &IndustrialNewTagDialog::_on_tagfield_fetched_for_current);
	IndustrialRuntimeClient::fetch_tag_field_catalog(current_driver_key, this, cb);
}

void IndustrialNewTagDialog::_on_tagfield_fetched_for_current(bool /*p_success*/) {
	int catalog_idx = device_index;
	_resolve_driver_key(current_driver_key, catalog_idx);
	(void)catalog_idx;

	Dictionary cat = IndustrialRuntimeClient::get_tag_field_catalog(current_driver_key);
	const bool symbolic = (String(cat.get("addressing_mode", Variant("absolute"))) == "symbolic");

	_apply_layout_for_mode(symbolic);

	if (tag_address_mode) {
		tag_address_mode->clear();
		Array amodes = cat.get("address_modes_ui", Array());
		for (int i = 0; i < amodes.size(); i++) {
			Dictionary m = amodes[i];
			String id = m.get("id", Variant(""));
			String msgid = m.get("label_msgid", Variant(""));
			String zh = m.get("label_zh", Variant(""));
			String display = msgid.is_empty() ? id : atr(msgid);
			if (zh.length() > 0) {
				display += "  (" + zh + ")";
			}
			tag_address_mode->add_item(display, i);
			tag_address_mode->set_item_metadata(i, id);
		}
		if (tag_address_mode->get_item_count() > 0) {
			tag_address_mode->select(0);
		}
	}

	if (symbolic) {
		Dictionary sym = cat.get("symbolic_import", Dictionary());
		Array hints = sym.get("tag_name_hints", Array());
		String placeholder;
		for (int i = 0; i < hints.size(); i++) {
			if (placeholder.length() > 0) {
				placeholder += " / ";
			}
			placeholder += String(hints[i]);
		}
		if (tag_symbol) {
			tag_symbol->set_placeholder(placeholder.length() > 0
					? placeholder
					: TTRC("Enter PLC symbolic tag path"));
		}
		_fill_symbolic_data_format_dropdown(String());
	} else {
		_fill_address_types();
		_fill_data_formats(tag_data_format, data_format_ids, true);
		_update_conditional_fields();
	}

	if (pending_tag_populate) {
		_populate_from_tag(pending_tag_data);
		pending_tag_populate = false;
	}
}

void IndustrialNewTagDialog::edit_tag(int p_tag_index) {
	if (project.is_null() || device_index < 0) {
		return;
	}

	const auto &tag = project->get_device(device_index).tags[p_tag_index];
	tag_index = p_tag_index;
	set_title(TTRC("Edit Tag"));

	pending_tag_data = tag;
	pending_tag_populate = true;
	_populate_from_tag(tag);
}

void IndustrialNewTagDialog::_on_confirm() {
	if (project.is_null() || device_index < 0) {
		hide();
		return;
	}

	int catalog_idx = device_index;
	String driver_key;
	_resolve_driver_key(driver_key, catalog_idx);
	(void)catalog_idx;

	Dictionary cat = IndustrialRuntimeClient::get_tag_field_catalog(driver_key);
	const bool symbolic = (String(cat.get("addressing_mode", Variant("absolute"))) == "symbolic");

	IndustrialTagData tag;
	if (tag_name) {
		tag.name = tag_name->get_text().strip_edges();
	}
	if (tag_description) {
		tag.description = tag_description->get_text().strip_edges();
	}
	if (tag_writable) {
		tag.writable = tag_writable->is_pressed();
	}
	if (tag_scale) {
		tag.scale = tag_scale->get_value();
	}
	if (tag_unit) {
		tag.unit = tag_unit->get_text();
	}

	if (symbolic) {
		tag.schema = "symbolic";
		tag.symbol = tag_symbol ? tag_symbol->get_text().strip_edges() : String();
		tag.address.clear();
		tag.address_mode.clear();
		tag.address_type.clear();
		tag.db_number = 0;
		tag.length = 0;

		if (tag_symbolic_data_format) {
			const int sel = tag_symbolic_data_format->get_selected();
			if (sel >= 0) {
				const String fmt = String(tag_symbolic_data_format->get_item_metadata(sel));
				if (!fmt.is_empty()) {
					tag.data_format = fmt;
				}
			}
		}
	} else {
		tag.schema = "absolute";
		tag.symbol.clear();

		if (tag_address_mode) {
			const int sel = tag_address_mode->get_selected();
			if (sel >= 0) {
				tag.address_mode = String(tag_address_mode->get_item_metadata(sel));
			}
		}
		if (tag_address_type) {
			const int sel = tag_address_type->get_selected();
			if (sel >= 0 && sel < address_type_ids.size()) {
				tag.address_type = address_type_ids[sel];
			}
		}
		if (tag_offset) {
			tag.address = tag_offset->get_text().strip_edges();
		}

		const bool bit_mode = (tag.address_mode == "bit");
		if (bit_mode) {
			tag.data_format = "bit";
		} else if (tag_data_format) {
			const int sel = tag_data_format->get_selected();
			if (sel >= 0 && sel < data_format_ids.size()) {
				tag.data_format = data_format_ids[sel];
			}
		}

		const Dictionary at = _lookup_address_type(tag.address_type);
		if (at.get("requires_db", false) && tag_db_number) {
			tag.db_number = int(tag_db_number->get_value());
		}
		if (at.get("has_length", false) && tag_length) {
			tag.length = int(tag_length->get_value());
		}
	}

	if (!tag.data_format.is_empty()) {
		const int mapped = _legacy_type_from_data_format(tag.data_format);
		if (mapped >= 0) {
			tag.data_type = mapped;
		}
	} else if (tag.data_type < 0 || tag.data_type >= TYPE_MAX) {
		tag.data_type = TYPE_INT16;
	}

	if (tag_index < 0) {
		project->add_tag(device_index, tag);
	} else {
		project->update_tag(device_index, tag_index, tag);
	}

	hide();
}
