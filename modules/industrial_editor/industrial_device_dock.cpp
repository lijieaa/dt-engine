#include "industrial_device_dock.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"
#include "industrial_device_fields.h"
#include "industrial_device_list_row.h"
#include "industrial_tag_list_row.h"
#include "modules/industrial_runtime/industrial_runtime_client.h"
#include "industrial_csv_io.h"

#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/templates/hash_set.h"
#include "core/input/input_event.h"
#include "scene/gui/box_container.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/dialogs.h"
#include "scene/main/http_request.h"

#include <cstring>

namespace {

int _hover_interface_group(const String &p_iface) {
	if (p_iface == "ethernet" || p_iface == "ethernet_ip") {
		return 0;
	}
	if (p_iface == "serial_rs232c" || p_iface == "serial_rs485" ||
			p_iface == "df1_fullduplex" || p_iface == "mpi" ||
			p_iface == "ppi" || p_iface == "usb") {
		return 1;
	}
	if (p_iface == "profibus" || p_iface == "ethercat" || p_iface == "can_j1939") {
		return 2;
	}
	if (p_iface == "bacnet_mstp" || p_iface == "bacnet_ip" ||
			p_iface == "iec_104" || p_iface == "iec_101" ||
			p_iface == "hsms" || p_iface == "secs_i") {
		return 3;
	}
	if (p_iface == "free") {
		return 4;
	}
	return 1;
}

String _hover_interface_type_label(const String &p_iface) {
	// Same 5-group labels as IndustrialNewDeviceDialog Interface Type dropdown.
	static const char *group_names[5] = { "Ethernet", "Serial", "Bus", "Industry", "Special" };
	const int group = _hover_interface_group(p_iface);
	return TTR(group_names[CLAMP(group, 0, 4)]);
}

bool _hover_should_show_interface_field(const String &p_key, int p_iface_group) {
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

String _hover_format_value(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL:
			return TTR("-");
		case Variant::BOOL:
			return (bool)p_value ? TTR("Yes") : TTR("No");
		default: {
			const String s = p_value;
			return s.is_empty() ? TTR("-") : s;
		}
	}
}

String _hover_driver_iface(int p_driver) {
	String iface = IndustrialRuntimeClient::get_driver_interface(p_driver);
	if (iface.is_empty()) {
		if (const DriverMeta *meta = industrial_get_driver_meta(p_driver)) {
			iface = String(meta->interface);
		}
	}
	return iface;
}

} // namespace

IndustrialDeviceDock::IndustrialDeviceDock() {
	_build_ui();
}

IndustrialDeviceDock::~IndustrialDeviceDock() {}

void IndustrialDeviceDock::_bind_methods() {
	ADD_SIGNAL(MethodInfo("device_selected", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("new_device_requested"));
	ADD_SIGNAL(MethodInfo("edit_device_requested", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("diagnose_requested"));
	ADD_SIGNAL(MethodInfo("tag_selected", PropertyInfo(Variant::INT, "device_index"), PropertyInfo(Variant::INT, "tag_index")));
	ADD_SIGNAL(MethodInfo("new_tag_requested", PropertyInfo(Variant::INT, "device_index")));
	ADD_SIGNAL(MethodInfo("edit_tag_requested", PropertyInfo(Variant::INT, "device_index"), PropertyInfo(Variant::INT, "tag_index")));
}

void IndustrialDeviceDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			_init_split_offset();
			_apply_action_icons();
			refresh();
			break;
		case NOTIFICATION_THEME_CHANGED:
			_apply_action_icons();
			break;
		case NOTIFICATION_TRANSLATION_CHANGED:
			_update_ui_text();
			_rebuild_device_type_filter();
			_rebuild_tag_type_filter();
			refresh();
			// Title refresh queues tab style via EditorDock::set_title 鈫?_tab_style_changed.
			break;
	}
}

void IndustrialDeviceDock::_init_split_offset() {
	if (!content_split) {
		return;
	}
	// Godot 4 SplitContainer: split_offset is an *offset from* the default
	// stretch layout (usually 50/50 when both children EXPAND), NOT an absolute
	// pixel position. A large value clamps the dragger to one edge so it feels
	// undraggable in a narrow left dock.
	content_split->set_split_offset(0);
}

void IndustrialDeviceDock::_build_ui() {
	set_name("DeviceManagement");
	set_layout_key("IndustrialDeviceManagement");
	set_default_slot(EditorDock::DOCK_SLOT_LEFT_UL);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(vbox);

	content_split = memnew(HSplitContainer);
	content_split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content_split->set_dragging_enabled(true);
	content_split->set_dragger_visibility(SplitContainer::DRAGGER_VISIBLE);
	content_split->add_theme_constant_override("minimum_grab_thickness", 12 * EDSCALE);
	content_split->add_theme_constant_override("autohide", 0);
	content_split->set_split_offset(0);
	vbox->add_child(content_split);

	// ---- Device panel (left) ----
	// Keep mins low so this dock can shrink like Scene (same left slot uses max of tab mins).
	VBoxContainer *device_panel = memnew(VBoxContainer);
	device_panel->set_custom_minimum_size(Size2(0, 0));
	device_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	device_panel->set_stretch_ratio(1.0);
	device_panel->set_clip_contents(true);
	content_split->add_child(device_panel);

	GridContainer *device_filter_grid = memnew(GridContainer);
	device_filter_grid->set_columns(2);
	device_filter_grid->add_theme_constant_override("h_separation", 4 * EDSCALE);
	device_filter_grid->add_theme_constant_override("v_separation", 4 * EDSCALE);
	device_filter_grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_panel->add_child(device_filter_grid);

	device_name_filter = memnew(LineEdit);
	device_name_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_name_filter->set_clear_button_enabled(true);
	device_name_filter->set_custom_minimum_size(Size2(0, 0));
	device_name_filter->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceDock::_on_device_filter_changed));
	device_filter_grid->add_child(device_name_filter);

	device_addr_filter = memnew(LineEdit);
	device_addr_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_addr_filter->set_clear_button_enabled(true);
	device_addr_filter->set_custom_minimum_size(Size2(0, 0));
	device_addr_filter->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceDock::_on_device_filter_changed));
	device_filter_grid->add_child(device_addr_filter);

	device_type_filter = memnew(OptionButton);
	// Critical: default fit_to_longest_item=true sizes to the longest of ~81
	// driver labels and locks the HSplit (no drag range left).
	device_type_filter->set_fit_to_longest_item(false);
	device_type_filter->set_clip_text(true);
	device_type_filter->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	device_type_filter->set_custom_minimum_size(Size2(0, 0));
	device_type_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_type_filter->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceDock::_on_device_option_changed));
	device_filter_grid->add_child(device_type_filter);

	device_enabled_filter = memnew(OptionButton);
	device_enabled_filter->set_fit_to_longest_item(false);
	device_enabled_filter->set_clip_text(true);
	device_enabled_filter->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	device_enabled_filter->set_custom_minimum_size(Size2(0, 0));
	device_enabled_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_enabled_filter->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceDock::_on_device_option_changed));
	device_filter_grid->add_child(device_enabled_filter);

	HBoxContainer *device_body = memnew(HBoxContainer);
	device_body->add_theme_constant_override("separation", 4 * EDSCALE);
	device_body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_body->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	device_panel->add_child(device_body);

	// Icon-only action rail (tooltip has label) so left editor column can shrink.
	VBoxContainer *device_actions = memnew(VBoxContainer);
	device_actions->set_custom_minimum_size(Size2(28 * EDSCALE, 0));
	device_actions->add_theme_constant_override("separation", 4 * EDSCALE);
	device_actions->set_clip_contents(true);
	device_body->add_child(device_actions);

	btn_new = memnew(Button);
	btn_new->set_flat(true);
	btn_new->set_focus_mode(Control::FOCUS_NONE);
	btn_new->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::focus_new_device));
	device_actions->add_child(btn_new);

	btn_duplicate = memnew(Button);
	btn_duplicate->set_flat(true);
	btn_duplicate->set_focus_mode(Control::FOCUS_NONE);
	btn_duplicate->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::duplicate_selected));
	device_actions->add_child(btn_duplicate);

	btn_delete = memnew(Button);
	btn_delete->set_flat(true);
	btn_delete->set_focus_mode(Control::FOCUS_NONE);
	btn_delete->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::delete_selected));
	device_actions->add_child(btn_delete);

	HSeparator *dev_sep = memnew(HSeparator);
	device_actions->add_child(dev_sep);

	btn_diagnose = memnew(Button);
	btn_diagnose->set_flat(true);
	btn_diagnose->set_focus_mode(Control::FOCUS_NONE);
	btn_diagnose->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::show_diagnose));
	device_actions->add_child(btn_diagnose);

	VBoxContainer *device_list_col = memnew(VBoxContainer);
	device_list_col->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_list_col->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	device_body->add_child(device_list_col);

	device_scroll = memnew(ScrollContainer);
	device_scroll->set_custom_minimum_size(Size2(0, 48 * EDSCALE));
	device_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	device_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	device_scroll->connect(SceneStringName(gui_input), callable_mp(this, &IndustrialDeviceDock::_on_device_list_gui_input));
	device_list_col->add_child(device_scroll);

	device_list = memnew(VBoxContainer);
	device_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	device_list->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	device_list->add_theme_constant_override("separation", 2 * EDSCALE);
	// Like SceneTreeEditor 鈫?Tree::nothing_selected: click empty list area clears selection.
	device_list->connect(SceneStringName(gui_input), callable_mp(this, &IndustrialDeviceDock::_on_device_list_gui_input));
	device_scroll->add_child(device_list);

	empty_state_label = memnew(Label);
	empty_state_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	empty_state_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	empty_state_label->set_visible(false);
	device_list_col->add_child(empty_state_label);

	btn_empty_create = memnew(Button);
	btn_empty_create->set_visible(false);
	btn_empty_create->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::focus_new_device));
	device_list_col->add_child(btn_empty_create);

	// ---- Tag panel (right) ----
	VBoxContainer *tag_panel = memnew(VBoxContainer);
	tag_panel->set_custom_minimum_size(Size2(0, 0));
	tag_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_panel->set_stretch_ratio(1.0);
	tag_panel->set_clip_contents(true);
	content_split->add_child(tag_panel);

	GridContainer *tag_filter_grid = memnew(GridContainer);
	tag_filter_grid->set_columns(2);
	tag_filter_grid->add_theme_constant_override("h_separation", 4 * EDSCALE);
	tag_filter_grid->add_theme_constant_override("v_separation", 4 * EDSCALE);
	tag_filter_grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_panel->add_child(tag_filter_grid);

	tag_name_filter = memnew(LineEdit);
	tag_name_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_name_filter->set_clear_button_enabled(true);
	tag_name_filter->set_custom_minimum_size(Size2(0, 0));
	tag_name_filter->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceDock::_on_tag_filter_changed));
	tag_filter_grid->add_child(tag_name_filter);

	tag_addr_filter = memnew(LineEdit);
	tag_addr_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_addr_filter->set_clear_button_enabled(true);
	tag_addr_filter->set_custom_minimum_size(Size2(0, 0));
	tag_addr_filter->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceDock::_on_tag_filter_changed));
	tag_filter_grid->add_child(tag_addr_filter);

	tag_type_filter = memnew(OptionButton);
	tag_type_filter->set_fit_to_longest_item(false);
	tag_type_filter->set_clip_text(true);
	tag_type_filter->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	tag_type_filter->set_custom_minimum_size(Size2(0, 0));
	tag_type_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_type_filter->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceDock::_on_tag_option_changed));
	tag_filter_grid->add_child(tag_type_filter);

	tag_rw_filter = memnew(OptionButton);
	tag_rw_filter->set_fit_to_longest_item(false);
	tag_rw_filter->set_clip_text(true);
	tag_rw_filter->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	tag_rw_filter->set_custom_minimum_size(Size2(0, 0));
	tag_rw_filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_rw_filter->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceDock::_on_tag_option_changed));
	tag_filter_grid->add_child(tag_rw_filter);

	tag_scope_label = memnew(Label);
	tag_scope_label->set_clip_text(true);
	tag_panel->add_child(tag_scope_label);

	HBoxContainer *tag_body = memnew(HBoxContainer);
	tag_body->add_theme_constant_override("separation", 4 * EDSCALE);
	tag_body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_body->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_panel->add_child(tag_body);

	VBoxContainer *tag_list_col = memnew(VBoxContainer);
	tag_list_col->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_list_col->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_body->add_child(tag_list_col);

	tag_scroll = memnew(ScrollContainer);
	tag_scroll->set_custom_minimum_size(Size2(0, 48 * EDSCALE));
	tag_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	tag_list_col->add_child(tag_scroll);

	tag_list = memnew(VBoxContainer);
	tag_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_list->add_theme_constant_override("separation", 2 * EDSCALE);
	tag_scroll->add_child(tag_list);

	tag_page_bar = memnew(HBoxContainer);
	tag_page_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_page_bar->add_theme_constant_override("separation", 5 * EDSCALE);
	tag_page_bar->set_visible(false);
	tag_list_col->add_child(tag_page_bar);

	tag_empty_state_label = memnew(Label);
	tag_empty_state_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	tag_empty_state_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	tag_empty_state_label->set_visible(false);
	tag_list_col->add_child(tag_empty_state_label);

	VBoxContainer *tag_actions = memnew(VBoxContainer);
	tag_actions->set_custom_minimum_size(Size2(28 * EDSCALE, 0));
	tag_actions->add_theme_constant_override("separation", 4 * EDSCALE);
	tag_actions->set_clip_contents(true);
	tag_body->add_child(tag_actions);

	btn_new_tag = memnew(Button);
	btn_new_tag->set_flat(true);
	btn_new_tag->set_focus_mode(Control::FOCUS_NONE);
	btn_new_tag->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::focus_new_tag));
	tag_actions->add_child(btn_new_tag);

	btn_delete_tag = memnew(Button);
	btn_delete_tag->set_flat(true);
	btn_delete_tag->set_focus_mode(Control::FOCUS_NONE);
	btn_delete_tag->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::delete_selected_tag));
	tag_actions->add_child(btn_delete_tag);

	btn_clear_tags = memnew(Button);
	btn_clear_tags->set_flat(true);
	btn_clear_tags->set_focus_mode(Control::FOCUS_NONE);
	btn_clear_tags->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::clear_device_tags));
	tag_actions->add_child(btn_clear_tags);

	HSeparator *tag_sep = memnew(HSeparator);
	tag_actions->add_child(tag_sep);

	btn_import_tags = memnew(Button);
	btn_import_tags->set_flat(true);
	btn_import_tags->set_focus_mode(Control::FOCUS_NONE);
	btn_import_tags->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::import_tags_csv));
	tag_actions->add_child(btn_import_tags);

	btn_export_tags = memnew(Button);
	btn_export_tags->set_flat(true);
	btn_export_tags->set_focus_mode(Control::FOCUS_NONE);
	btn_export_tags->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::export_tags_csv));
	tag_actions->add_child(btn_export_tags);

	_build_context_menu();
	_build_tag_context_menu();
	_rebuild_device_type_filter();
	_rebuild_tag_type_filter();
	_update_ui_text();
	_update_action_enabled();

	tag_import_dialog = memnew(EditorFileDialog);
	tag_import_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	tag_import_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	tag_import_dialog->clear_filters();
	tag_import_dialog->add_filter("*.csv", TTR("CSV Files"));
	tag_import_dialog->add_filter("*.xlsx", TTR("Excel Workbook"));
	tag_import_dialog->add_filter("*.xls", TTR("Excel 97-2003"));
	tag_import_dialog->connect("file_selected", callable_mp(this, &IndustrialDeviceDock::_on_tag_import_file_selected));
	add_child(tag_import_dialog);

	tag_export_dialog = memnew(EditorFileDialog);
	tag_export_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	tag_export_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	tag_export_dialog->clear_filters();
	tag_export_dialog->add_filter("*.csv", TTR("CSV Files"));
	tag_export_dialog->add_filter("*.xlsx", TTR("Excel Workbook"));
	tag_export_dialog->connect("file_selected", callable_mp(this, &IndustrialDeviceDock::_on_tag_export_file_selected));
	add_child(tag_export_dialog);

	tag_sheet_result_dialog = memnew(AcceptDialog);
	tag_sheet_result_dialog->set_title(TTR("Tag Import"));
	tag_sheet_result_dialog->set_autowrap(true);
	tag_sheet_result_dialog->set_min_size(Size2(420, 0) * EDSCALE);
	add_child(tag_sheet_result_dialog);

	tag_clear_confirm_dialog = memnew(ConfirmationDialog);
	tag_clear_confirm_dialog->set_title(TTR("Clear Tags"));
	tag_clear_confirm_dialog->set_autowrap(true);
	tag_clear_confirm_dialog->set_min_size(Size2(420, 0) * EDSCALE);
	tag_clear_confirm_dialog->connect(SceneStringName(confirmed), callable_mp(this, &IndustrialDeviceDock::_on_clear_tags_confirmed));
	add_child(tag_clear_confirm_dialog);

	tag_sheet_http = memnew(HTTPRequest);
	tag_sheet_http->set_body_size_limit(64 * 1024 * 1024); // large tag sheets (xlsx parse JSON)
	tag_sheet_http->set_timeout(120);
	tag_sheet_http->connect("request_completed", callable_mp(this, &IndustrialDeviceDock::_on_tag_sheet_http_completed));
	add_child(tag_sheet_http);
}

void IndustrialDeviceDock::_apply_action_icons() {
	if (btn_new) {
		btn_new->set_button_icon(get_editor_theme_icon(SNAME("Add")));
	}
	if (btn_duplicate) {
		btn_duplicate->set_button_icon(get_editor_theme_icon(SNAME("Duplicate")));
	}
	if (btn_delete) {
		btn_delete->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
	}
	if (btn_diagnose) {
		btn_diagnose->set_button_icon(get_editor_theme_icon(SNAME("Debug")));
	}
	if (btn_empty_create) {
		btn_empty_create->set_button_icon(get_editor_theme_icon(SNAME("Add")));
	}
	if (btn_new_tag) {
		btn_new_tag->set_button_icon(get_editor_theme_icon(SNAME("Add")));
	}
	if (btn_delete_tag) {
		btn_delete_tag->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
	}
	if (btn_clear_tags) {
		btn_clear_tags->set_button_icon(get_editor_theme_icon(SNAME("Clear")));
	}
	if (btn_import_tags) {
		btn_import_tags->set_button_icon(get_editor_theme_icon(SNAME("Load")));
	}
	if (btn_export_tags) {
		btn_export_tags->set_button_icon(get_editor_theme_icon(SNAME("Save")));
	}
}

void IndustrialDeviceDock::_update_action_enabled() {
	const bool has_device = selected_device_index >= 0;
	const bool has_tag = selected_tag_index >= 0;
	if (btn_duplicate) {
		btn_duplicate->set_disabled(!has_device);
	}
	if (btn_delete) {
		btn_delete->set_disabled(!has_device);
	}
	if (btn_diagnose) {
		btn_diagnose->set_disabled(!has_device);
	}
	if (btn_new_tag) {
		btn_new_tag->set_disabled(!has_device);
	}
	if (btn_delete_tag) {
		btn_delete_tag->set_disabled(!has_tag);
	}
	if (btn_clear_tags) {
		const bool has_any_tags = has_device && project.is_valid() &&
				project->get_tag_count_for_device(selected_device_index) > 0;
		btn_clear_tags->set_disabled(!has_any_tags);
	}
	if (btn_import_tags) {
		btn_import_tags->set_disabled(!has_device);
	}
	if (btn_export_tags) {
		btn_export_tags->set_disabled(!has_device);
	}
}

void IndustrialDeviceDock::_update_ui_text() {
	set_title(TTR("Device Management"));

	if (device_name_filter) {
		device_name_filter->set_placeholder(TTR("Name"));
	}
	if (device_addr_filter) {
		device_addr_filter->set_placeholder(TTR("IP / Address"));
	}
	if (device_enabled_filter) {
		const int selected = device_enabled_filter->get_selected();
		device_enabled_filter->clear();
		device_enabled_filter->add_item(TTR("All"), ENABLED_FILTER_ALL);
		device_enabled_filter->add_item(TTR("Enabled"), ENABLED_FILTER_ENABLED);
		device_enabled_filter->add_item(TTR("Disabled"), ENABLED_FILTER_DISABLED);
		device_enabled_filter->select(CLAMP(selected, 0, 2));
	}
	if (btn_new) {
		btn_new->set_text(String());
		btn_new->set_tooltip_text(TTR("New"));
	}
	if (btn_duplicate) {
		btn_duplicate->set_text(String());
		btn_duplicate->set_tooltip_text(TTR("Copy"));
	}
	if (btn_delete) {
		btn_delete->set_text(String());
		btn_delete->set_tooltip_text(TTR("Delete"));
	}
	if (btn_diagnose) {
		btn_diagnose->set_text(String());
		btn_diagnose->set_tooltip_text(TTR("Diagnose"));
	}
	if (btn_empty_create) {
		btn_empty_create->set_text(TTR("Create Device"));
	}
	if (empty_state_label) {
		empty_state_label->set_text(TTR("No devices yet."));
	}
	if (tag_name_filter) {
		tag_name_filter->set_placeholder(TTR("Name"));
	}
	if (tag_addr_filter) {
		tag_addr_filter->set_placeholder(TTR("Address"));
	}
	if (tag_type_filter) {
		tag_type_filter->set_tooltip_text(TTR("Data Type"));
	}
	if (tag_rw_filter) {
		const int selected = tag_rw_filter->get_selected();
		tag_rw_filter->clear();
		tag_rw_filter->add_item(TTR("All"), RW_FILTER_ALL);
		tag_rw_filter->add_item(TTR("Read-only"), RW_FILTER_READONLY);
		tag_rw_filter->add_item(TTR("Writable"), RW_FILTER_WRITABLE);
		tag_rw_filter->select(CLAMP(selected, 0, 2));
	}
	if (btn_new_tag) {
		btn_new_tag->set_text(String());
		btn_new_tag->set_tooltip_text(TTR("New"));
	}
	if (btn_delete_tag) {
		btn_delete_tag->set_text(String());
		btn_delete_tag->set_tooltip_text(TTR("Delete"));
	}
	if (btn_clear_tags) {
		btn_clear_tags->set_text(String());
		btn_clear_tags->set_tooltip_text(TTR("Clear"));
	}
	if (btn_import_tags) {
		btn_import_tags->set_text(String());
		btn_import_tags->set_tooltip_text(TTR("Import"));
	}
	if (btn_export_tags) {
		btn_export_tags->set_text(String());
		btn_export_tags->set_tooltip_text(TTR("Export"));
	}
	if (tag_empty_state_label && selected_device_index < 0) {
		tag_empty_state_label->set_text(TTR("Select a device to view its tags."));
	}
	if (tag_scope_label && selected_device_index < 0) {
		// ASCII hyphen only: Unicode dashes mojibake under MSVC without BOM.
		tag_scope_label->set_text(vformat(TTR("Current device: %s"), TTR("-")));
	}

	if (context_menu) {
		context_menu->clear();
		context_menu->add_item(TTR("Delete Device"), ACTION_DELETE);
		context_menu->add_item(TTR("Copy"), ACTION_DUPLICATE);
		context_menu->add_separator();
		context_menu->add_item(TTR("Diagnose"), ACTION_DIAGNOSE);
	}
	if (tag_context_menu) {
		tag_context_menu->clear();
		tag_context_menu->add_item(TTR("Delete Tag"), TAG_ACTION_DELETE);
		tag_context_menu->add_separator();
		tag_context_menu->add_item(TTR("New Tag"), TAG_ACTION_NEW);
	}
}

void IndustrialDeviceDock::_rebuild_device_type_filter() {
	if (!device_type_filter) {
		return;
	}
	const int prev_meta = device_type_filter->get_selected_id();
	device_type_filter->clear();
	device_type_filter->add_item(TTR("All Types"), -1);
	const int count = industrial_get_driver_count();
	for (int i = 0; i < count; i++) {
		device_type_filter->add_item(industrial_format_device_type_label(i), i);
	}
	const int idx = device_type_filter->get_item_index(prev_meta);
	device_type_filter->select(idx >= 0 ? idx : 0);
}

void IndustrialDeviceDock::_rebuild_tag_type_filter() {
	if (!tag_type_filter) {
		return;
	}

	String prev_key;
	if (tag_type_filter->get_selected() > 0) {
		prev_key = String(tag_type_filter->get_item_metadata(tag_type_filter->get_selected()));
	}

	tag_type_filter->clear();
	tag_type_filter->add_item(TTR("All"), 0);
	tag_type_filter->set_item_metadata(0, String());
	tag_type_filter->set_tooltip_text(TTR("Data Type"));

	HashSet<String> seen;
	if (project.is_valid() && selected_device_index >= 0 && selected_device_index < project->get_device_count()) {
		const IndustrialDeviceData &dev = project->get_device(selected_device_index);
		for (int i = 0; i < (int)dev.tags.size(); i++) {
			const IndustrialTagData &tag = dev.tags[i];
			const String key = _tag_data_type_key(tag);
			if (key.is_empty() || seen.has(key)) {
				continue;
			}
			seen.insert(key);
			const int idx = tag_type_filter->get_item_count();
			tag_type_filter->add_item(_tag_data_type_filter_label(tag), idx);
			tag_type_filter->set_item_metadata(idx, key);
		}
	}

	if (!prev_key.is_empty()) {
		for (int i = 0; i < tag_type_filter->get_item_count(); i++) {
			if (String(tag_type_filter->get_item_metadata(i)) == prev_key) {
				tag_type_filter->select(i);
				return;
			}
		}
	}
	tag_type_filter->select(0);
}

String IndustrialDeviceDock::_tag_data_type_key(const IndustrialTagData &p_tag) const {
	String fmt = p_tag.data_format;
	if (fmt.is_empty()) {
		fmt = industrial_get_data_type_name(p_tag.data_type);
	}
	if (fmt.is_empty()) {
		return String();
	}
	// Include R/W so filter options can show read/write per type occurrence.
	return vformat("%s|%d", fmt, p_tag.writable ? 1 : 0);
}

String IndustrialDeviceDock::_tag_data_type_filter_label(const IndustrialTagData &p_tag) const {
	String fmt_id = p_tag.data_format;
	String driver_key;
	if (project.is_valid() && selected_device_index >= 0 && selected_device_index < project->get_device_count()) {
		const IndustrialDeviceData &dev = project->get_device(selected_device_index);
		driver_key = !dev.driver_key.is_empty() ? dev.driver_key : IndustrialRuntimeClient::get_driver_key(dev.driver);
	}

	String type_label;
	if (!fmt_id.is_empty()) {
		type_label = industrial_format_data_type_label(fmt_id, driver_key);
	} else {
		type_label = industrial_get_data_type_name(p_tag.data_type);
	}
	if (type_label.is_empty()) {
		type_label = TTR("-");
	}
	return vformat("%s - %s", type_label, p_tag.writable ? TTR("Writable") : TTR("Read-only"));
}

void IndustrialDeviceDock::_build_context_menu() {
	context_menu = memnew(PopupMenu);
	context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &IndustrialDeviceDock::_on_context_menu_pressed));
	add_child(context_menu);
}

void IndustrialDeviceDock::_build_tag_context_menu() {
	tag_context_menu = memnew(PopupMenu);
	tag_context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &IndustrialDeviceDock::_on_tag_context_menu_pressed));
	add_child(tag_context_menu);
}

void IndustrialDeviceDock::set_project(Ref<IndustrialProject> p_project) {
	project = p_project;
	selected_device_index = -1;
	selected_tag_device_index = -1;
	selected_tag_index = -1;
	_rebuild_device_type_filter();
	refresh();
}

bool IndustrialDeviceDock::_device_matches_filters(int p_idx) const {
	if (project.is_null() || p_idx < 0 || p_idx >= project->get_device_count()) {
		return false;
	}
	const IndustrialDeviceData &dev = project->get_device(p_idx);

	if (device_name_filter) {
		const String q = device_name_filter->get_text().strip_edges().to_lower();
		if (!q.is_empty() && dev.name.to_lower().find(q) < 0) {
			return false;
		}
	}
	if (device_addr_filter) {
		const String q = device_addr_filter->get_text().strip_edges().to_lower();
		if (!q.is_empty()) {
			const bool hit = dev.ip.to_lower().find(q) >= 0 ||
					dev.remote_hmi_ip.to_lower().find(q) >= 0 ||
					dev.serial_port.to_lower().find(q) >= 0;
			if (!hit) {
				return false;
			}
		}
	}
	if (device_type_filter && device_type_filter->get_selected() > 0) {
		const int want = device_type_filter->get_selected_id();
		if (want >= 0 && want != dev.driver) {
			return false;
		}
	}
	if (device_enabled_filter) {
		const int mode = device_enabled_filter->get_selected_id();
		if (mode == ENABLED_FILTER_ENABLED && !dev.enabled) {
			return false;
		}
		if (mode == ENABLED_FILTER_DISABLED && dev.enabled) {
			return false;
		}
	}
	return true;
}

bool IndustrialDeviceDock::_tag_matches_filters(const IndustrialTagData &p_tag) const {
	if (tag_name_filter) {
		const String q = tag_name_filter->get_text().strip_edges().to_lower();
		if (!q.is_empty() && p_tag.name.to_lower().find(q) < 0) {
			return false;
		}
	}
	if (tag_addr_filter) {
		const String q = tag_addr_filter->get_text().strip_edges().to_lower();
		if (!q.is_empty()) {
			const bool hit = p_tag.address.to_lower().find(q) >= 0 || p_tag.symbol.to_lower().find(q) >= 0;
			if (!hit) {
				return false;
			}
		}
	}
	if (tag_type_filter && tag_type_filter->get_selected() > 0) {
		const String want = String(tag_type_filter->get_item_metadata(tag_type_filter->get_selected()));
		if (!want.is_empty() && _tag_data_type_key(p_tag) != want) {
			return false;
		}
	}
	if (tag_rw_filter) {
		const int mode = tag_rw_filter->get_selected_id();
		if (mode == RW_FILTER_READONLY && p_tag.writable) {
			return false;
		}
		if (mode == RW_FILTER_WRITABLE && !p_tag.writable) {
			return false;
		}
	}
	return true;
}

String IndustrialDeviceDock::_device_hover_tooltip(int p_idx) const {
	if (project.is_null() || p_idx < 0 || p_idx >= project->get_device_count()) {
		return String();
	}
	const IndustrialDeviceData &dev = project->get_device(p_idx);
	const int driver_idx = dev.driver >= 0 ? dev.driver : 0;
	const String iface = _hover_driver_iface(driver_idx);
	const int iface_group = _hover_interface_group(iface);

	PackedStringArray lines;
	// Mirror IndustrialNewDeviceDialog "Device Info" rows (same TTR keys).
	lines.push_back(vformat("%s: %s", TTR("Name"), dev.name.is_empty() ? TTR("-") : dev.name));
	if (!dev.description.is_empty()) {
		lines.push_back(vformat("%s: %s", TTR("Description"), dev.description));
	}
	lines.push_back(vformat("%s: %s", TTR("Interface Type"),
			iface.is_empty() ? TTR("-") : _hover_interface_type_label(iface)));
	lines.push_back(vformat("%s: %s", TTR("Device Type"), industrial_format_device_type_label(driver_idx)));
	lines.push_back(vformat("%s: %s", TTR("Enabled"), dev.enabled ? TTR("Yes") : TTR("No")));
	if (runtime_status_by_id.has(dev.id)) {
		const Dictionary st = runtime_status_by_id[dev.id];
		const String status = st.get("status", "");
		const String err = st.get("error", "");
		if (!status.is_empty()) {
			lines.push_back(vformat("%s: %s", TTR("Runtime Status"), status));
		}
		if (!err.is_empty()) {
			lines.push_back(vformat("%s: %s", TTR("Connection error"), err));
		}
	}

	// Interface / Protocol / Tuning 鈥?same field set & filters as New Device.
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

	Vector<IndustrialFieldDef> fields = industrial_get_driver_fields(driver_idx);
	Vector<IndustrialFieldDef> visible_fields = industrial_filter_driver_fields(fields, kHiddenKeys, kHiddenKeyCount);
	const Dictionary params = industrial_device_param_dict(dev);

	bool added_dynamic = false;
	for (int i = 0; i < visible_fields.size(); i++) {
		const IndustrialFieldDef &field = visible_fields[i];
		const IndustrialDeviceFieldGroup group = industrial_classify_device_field(field.key);
		if (group == IND_DEVICE_GROUP_COMMON) {
			continue;
		}
		if (group == IND_DEVICE_GROUP_INTERFACE && !_hover_should_show_interface_field(field.key, iface_group)) {
			continue;
		}

		Variant value;
		if (params.has(field.key)) {
			value = params[field.key];
		} else if (field.default_value.get_type() != Variant::NIL) {
			value = field.default_value;
		}

		if (!added_dynamic) {
			lines.push_back(String());
			added_dynamic = true;
		}
		lines.push_back(vformat("%s: %s", TTR(field.name), _hover_format_value(value)));
	}

	return String("\n").join(lines);
}

String IndustrialDeviceDock::_tag_hover_tooltip(int p_device_index, int p_tag_index) const {
	if (project.is_null() || p_device_index < 0 || p_device_index >= project->get_device_count()) {
		return String();
	}
	const IndustrialDeviceData &dev = project->get_device(p_device_index);
	if (p_tag_index < 0 || p_tag_index >= (int)dev.tags.size()) {
		return String();
	}
	const IndustrialTagData &tag = dev.tags[p_tag_index];
	PackedStringArray lines;
	lines.push_back(vformat("%s (%s)", tag.name, tag.writable ? TTR("Writable") : TTR("Read-only")));
	lines.push_back(vformat("%s: %s", TTR("Device"), dev.name));
	if (!tag.schema.is_empty()) {
		lines.push_back(vformat("%s: %s", TTR("Schema"), industrial_format_tag_schema(tag.schema)));
	}
	if (tag.schema != "symbolic" && !tag.address_mode.is_empty()) {
		String mode = tag.address_mode;
		if (mode == "bit") {
			mode = TTR("Bit");
		} else if (mode == "word") {
			mode = TTR("Word");
		}
		lines.push_back(vformat("%s: %s", TTR("Address Mode"), mode));
	}
	if (!tag.address_type.is_empty()) {
		lines.push_back(vformat("%s: %s", TTR("Address Type"), tag.address_type));
	}
	if (!tag.data_format.is_empty() || (tag.data_type >= 0 && tag.data_type < TYPE_MAX)) {
		String fmt_label;
		if (!tag.data_format.is_empty()) {
			String driver_key;
			if (project.is_valid() && p_device_index >= 0 && p_device_index < project->get_device_count()) {
				const IndustrialDeviceData &d = project->get_device(p_device_index);
				driver_key = !d.driver_key.is_empty() ? d.driver_key : IndustrialRuntimeClient::get_driver_key(d.driver);
			}
			fmt_label = industrial_format_data_type_label(tag.data_format, driver_key);
		} else {
			fmt_label = industrial_get_data_type_name(tag.data_type);
		}
		lines.push_back(vformat("%s: %s", TTR("Data Type"), fmt_label));
	}
	if (!tag.address.is_empty()) {
		lines.push_back(vformat("%s: %s", TTR("Address"), tag.address));
	}
	if (!tag.symbol.is_empty()) {
		lines.push_back(vformat("%s: %s", TTR("Symbol"), tag.symbol));
	}
	if (!tag.unit.is_empty()) {
		lines.push_back(vformat("%s: %s", TTR("Unit"), tag.unit));
	}
	if (!tag.description.is_empty()) {
		lines.push_back(String());
		lines.push_back(tag.description);
	}
	return String("\n").join(lines);
}

void IndustrialDeviceDock::refresh() {
	if (!device_list) {
		return;
	}
	const int selected_idx_before = selected_device_index;
	Control *prev_focus = nullptr;
	if (Viewport *vp = get_viewport()) {
		prev_focus = vp->gui_get_focus_owner();
	}

	_populate_device_list();

	if (selected_idx_before >= 0) {
		// Keep logical selection for properties even if filtered out of the list.
		selected_device_index = selected_idx_before;
	}
	_rebuild_tag_type_filter();
	_populate_tag_list();
	_update_action_enabled();

	if (prev_focus && is_ancestor_of(prev_focus) && prev_focus != device_scroll && prev_focus != tag_scroll) {
		prev_focus->grab_focus();
	}
}

void IndustrialDeviceDock::set_runtime_device_statuses(const Dictionary &p_by_id) {
	runtime_status_by_id = p_by_id;
	if (!device_list) {
		return;
	}
	for (int i = 0; i < device_list->get_child_count(); i++) {
		IndustrialDeviceListRow *row = Object::cast_to<IndustrialDeviceListRow>(device_list->get_child(i));
		if (!row || project.is_null()) {
			continue;
		}
		const int idx = row->get_device_index();
		if (idx < 0 || idx >= project->get_device_count()) {
			continue;
		}
		const String id = project->get_device(idx).id;
		if (runtime_status_by_id.has(id)) {
			const Dictionary st = runtime_status_by_id[id];
			row->set_live_status(st.get("status", ""), st.get("error", ""));
		} else {
			row->set_live_status(String(), String());
		}
	}
}

void IndustrialDeviceDock::clear_runtime_device_statuses() {
	runtime_status_by_id.clear();
	set_runtime_device_statuses(Dictionary());
}

void IndustrialDeviceDock::_populate_device_list() {
	ERR_FAIL_NULL(device_list);
	while (device_list->get_child_count() > 0) {
		Node *child = device_list->get_child(0);
		device_list->remove_child(child);
		child->queue_free();
	}

	if (project.is_null()) {
		if (device_scroll) {
			device_scroll->set_visible(false);
		}
		if (empty_state_label) {
			empty_state_label->set_visible(false);
		}
		if (btn_empty_create) {
			btn_empty_create->set_visible(false);
		}
		return;
	}

	Vector<int> matched;
	const int count = project->get_device_count();
	for (int i = 0; i < count; i++) {
		if (_device_matches_filters(i)) {
			matched.push_back(i);
		}
	}

	if (count == 0) {
		if (device_scroll) {
			device_scroll->set_visible(false);
		}
		if (empty_state_label) {
			empty_state_label->set_visible(true);
		}
		if (btn_empty_create) {
			btn_empty_create->set_visible(true);
		}
		selected_device_index = -1;
		selected_tag_device_index = -1;
		selected_tag_index = -1;
		return;
	}

	if (device_scroll) {
		device_scroll->set_visible(true);
	}
	if (empty_state_label) {
		empty_state_label->set_visible(false);
	}
	if (btn_empty_create) {
		btn_empty_create->set_visible(false);
	}

	for (int i = 0; i < matched.size(); i++) {
		const int dev_idx = matched[i];
		const IndustrialDeviceData &dev = project->get_device(dev_idx);

		String addr = dev.ip;
		if (addr.is_empty()) {
			addr = dev.serial_port;
		}
		if (addr.is_empty()) {
			addr = dev.remote_hmi_ip;
		}
		const String meta = vformat("%s - %s - %d %s",
				industrial_format_device_type_label(dev.driver),
				addr.is_empty() ? TTR("-") : addr,
				(int)dev.tags.size(),
				TTR("tags"));

		IndustrialDeviceListRow *row = memnew(IndustrialDeviceListRow);
		row->set_device_index(dev_idx);
		String vendor = industrial_get_driver_vendor(dev.driver);
		if (vendor.is_empty() && !dev.driver_key.is_empty()) {
			const int by_key = industrial_find_driver_index_by_key(dev.driver_key);
			if (by_key >= 0) {
				vendor = industrial_get_driver_vendor(by_key);
			}
		}
		row->populate(dev.name, meta, vendor, dev.enabled, _device_hover_tooltip(dev_idx));
		if (runtime_status_by_id.has(dev.id)) {
			const Dictionary st = runtime_status_by_id[dev.id];
			row->set_live_status(st.get("status", ""), st.get("error", ""));
		}
		row->set_selected(dev_idx == selected_device_index);
		row->connect(SNAME("row_selected"), callable_mp(this, &IndustrialDeviceDock::_on_device_row_selected).bind(dev_idx));
		row->connect(SNAME("row_context"), callable_mp(this, &IndustrialDeviceDock::_on_device_row_context).bind(dev_idx));
		device_list->add_child(row);
	}
}

void IndustrialDeviceDock::_rebuild_tag_page_bar(int p_page, int p_page_count, int p_total_items) {
	ERR_FAIL_NULL(tag_page_bar);
	while (tag_page_bar->get_child_count() > 0) {
		Node *child = tag_page_bar->get_child(0);
		tag_page_bar->remove_child(child);
		child->queue_free();
	}

	if (p_page_count <= 1 || p_total_items <= 0) {
		tag_page_bar->set_visible(false);
		return;
	}
	tag_page_bar->set_visible(true);

	// Asset Library-style window of page buttons.
	int from = p_page - (5 / EDSCALE);
	if (from < 1) {
		from = 1;
	}
	int to = from + (10 / EDSCALE);
	if (to > p_page_count) {
		to = p_page_count;
	}

	tag_page_bar->add_spacer();

	Button *first = memnew(Button);
	first->set_button_icon(get_editor_theme_icon(SNAME("BackStart")));
	first->set_tooltip_text(TTR("First", "Pagination"));
	first->set_theme_type_variation("PanelBackgroundButton");
	if (p_page != 1) {
		first->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_tag_page_pressed).bind(1));
	} else {
		first->set_disabled(true);
		first->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	}
	tag_page_bar->add_child(first);

	Button *prev = memnew(Button);
	prev->set_button_icon(get_editor_theme_icon(SNAME("Back")));
	prev->set_tooltip_text(TTR("Previous", "Pagination"));
	prev->set_theme_type_variation("PanelBackgroundButton");
	if (p_page > 1) {
		prev->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_tag_page_pressed).bind(p_page - 1));
	} else {
		prev->set_disabled(true);
		prev->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	}
	tag_page_bar->add_child(prev);

	tag_page_bar->add_child(memnew(VSeparator));

	for (int i = from; i <= to; i++) {
		Button *current = memnew(Button);
		current->set_text(vformat(" %d ", i));
		current->set_theme_type_variation("PanelBackgroundButton");
		if (i == p_page) {
			current->set_disabled(true);
			current->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		} else {
			current->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_tag_page_pressed).bind(i));
		}
		tag_page_bar->add_child(current);
	}

	tag_page_bar->add_child(memnew(VSeparator));

	Button *next = memnew(Button);
	next->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
	next->set_tooltip_text(TTR("Next", "Pagination"));
	next->set_theme_type_variation("PanelBackgroundButton");
	if (p_page < p_page_count) {
		next->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_tag_page_pressed).bind(p_page + 1));
	} else {
		next->set_disabled(true);
		next->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	}
	tag_page_bar->add_child(next);

	Button *last = memnew(Button);
	last->set_button_icon(get_editor_theme_icon(SNAME("ForwardEnd")));
	last->set_tooltip_text(TTR("Last", "Pagination"));
	last->set_theme_type_variation("PanelBackgroundButton");
	if (p_page != p_page_count) {
		last->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_tag_page_pressed).bind(p_page_count));
	} else {
		last->set_disabled(true);
		last->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	}
	tag_page_bar->add_child(last);

	tag_page_bar->add_spacer();
}

void IndustrialDeviceDock::_on_tag_page_pressed(int p_page) {
	if (p_page < 1 || p_page == tag_page) {
		return;
	}
	tag_page = p_page;
	_populate_tag_list();
}

void IndustrialDeviceDock::_populate_tag_list() {
	ERR_FAIL_NULL(tag_list);
	while (tag_list->get_child_count() > 0) {
		Node *child = tag_list->get_child(0);
		tag_list->remove_child(child);
		child->queue_free();
	}

	if (project.is_null() || selected_device_index < 0 || selected_device_index >= project->get_device_count()) {
		if (tag_scroll) {
			tag_scroll->set_visible(false);
		}
		if (tag_page_bar) {
			tag_page_bar->set_visible(false);
		}
		if (tag_scope_label) {
			tag_scope_label->set_text(vformat(TTR("Current device: %s"), TTR("-")));
		}
		if (tag_empty_state_label) {
			tag_empty_state_label->set_text(TTR("Select a device to view its tags."));
			tag_empty_state_label->set_visible(true);
		}
		return;
	}

	const IndustrialDeviceData &dev = project->get_device(selected_device_index);
	if (tag_scope_label) {
		tag_scope_label->set_text(vformat(TTR("Current device: %s"), dev.name));
	}

	Vector<int> matched;
	for (int i = 0; i < (int)dev.tags.size(); i++) {
		if (_tag_matches_filters(dev.tags[i])) {
			matched.push_back(i);
		}
	}

	if (tag_scroll) {
		tag_scroll->set_visible(!matched.is_empty());
	}
	if (tag_empty_state_label) {
		tag_empty_state_label->set_text(matched.is_empty() ? TTR("No tags for this device.") : String());
		tag_empty_state_label->set_visible(matched.is_empty());
	}

	if (matched.is_empty()) {
		if (tag_page_bar) {
			tag_page_bar->set_visible(false);
		}
		return;
	}

	const int page_count = MAX(1, (matched.size() + TAG_PAGE_LEN - 1) / TAG_PAGE_LEN);
	if (tag_page > page_count) {
		tag_page = page_count;
	}
	if (tag_page < 1) {
		tag_page = 1;
	}
	const int start = (tag_page - 1) * TAG_PAGE_LEN;
	const int end = MIN(start + TAG_PAGE_LEN, matched.size());

	for (int i = start; i < end; i++) {
		const int tag_idx = matched[i];
		const IndustrialTagData &tag = dev.tags[tag_idx];
		const String addr = !tag.address.is_empty() ? tag.address : tag.symbol;
		String fmt_label;
		if (!tag.data_format.is_empty()) {
			const String driver_key = !dev.driver_key.is_empty() ? dev.driver_key : IndustrialRuntimeClient::get_driver_key(dev.driver);
			fmt_label = industrial_format_data_type_label(tag.data_format, driver_key);
		} else {
			fmt_label = industrial_get_data_type_name(tag.data_type);
		}
		const String meta = vformat("%s - %s - %s",
				addr.is_empty() ? TTR("-") : addr,
				fmt_label.is_empty() ? TTR("-") : fmt_label,
				tag.writable ? TTR("Writable") : TTR("Read-only"));

		IndustrialTagListRow *row = memnew(IndustrialTagListRow);
		row->set_indices(selected_device_index, tag_idx);
		row->populate(tag.name, meta, _tag_hover_tooltip(selected_device_index, tag_idx));
		row->set_selected(selected_tag_device_index == selected_device_index && selected_tag_index == tag_idx);
		row->connect(SNAME("row_selected"), callable_mp(this, &IndustrialDeviceDock::_on_tag_row_selected).bind(selected_device_index, tag_idx));
		row->connect(SNAME("row_context"), callable_mp(this, &IndustrialDeviceDock::_on_tag_row_context).bind(selected_device_index, tag_idx));
		tag_list->add_child(row);
	}

	_rebuild_tag_page_bar(tag_page, page_count, matched.size());
}

void IndustrialDeviceDock::_on_device_row_selected(int p_index) {
	selected_device_index = p_index;
	selected_tag_device_index = -1;
	selected_tag_index = -1;

	if (device_list) {
		for (int i = 0; i < device_list->get_child_count(); i++) {
			if (IndustrialDeviceListRow *row = Object::cast_to<IndustrialDeviceListRow>(device_list->get_child(i))) {
				row->set_selected(row->get_device_index() == p_index);
			}
		}
	}

	emit_signal(SNAME("device_selected"), selected_device_index);
	_rebuild_tag_type_filter();
	tag_page = 1;
	_populate_tag_list();
	_update_action_enabled();
}

void IndustrialDeviceDock::_clear_device_selection() {
	if (selected_device_index < 0 && selected_tag_device_index < 0 && selected_tag_index < 0) {
		return;
	}
	selected_device_index = -1;
	selected_tag_device_index = -1;
	selected_tag_index = -1;

	if (device_list) {
		for (int i = 0; i < device_list->get_child_count(); i++) {
			if (IndustrialDeviceListRow *row = Object::cast_to<IndustrialDeviceListRow>(device_list->get_child(i))) {
				row->set_selected(false);
			}
		}
	}

	emit_signal(SNAME("device_selected"), -1);
	_rebuild_tag_type_filter();
	tag_page = 1;
	_populate_tag_list();
	_update_action_enabled();
}

void IndustrialDeviceDock::_on_device_list_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (!mb.is_valid() || !mb->is_pressed() || mb->get_button_index() != MouseButton::LEFT) {
		return;
	}
	// Rows accept the event when clicked; reaching here means empty area under/around rows
	// (same role as Tree emitting nothing_selected).
	_clear_device_selection();
}

void IndustrialDeviceDock::_on_device_row_context(int p_index, const Vector2 &p_local_pos) {
	_on_device_row_selected(p_index);
	if (!context_menu || !device_list) {
		return;
	}
	IndustrialDeviceListRow *row = nullptr;
	for (int i = 0; i < device_list->get_child_count(); i++) {
		IndustrialDeviceListRow *candidate = Object::cast_to<IndustrialDeviceListRow>(device_list->get_child(i));
		if (candidate && candidate->get_device_index() == p_index) {
			row = candidate;
			break;
		}
	}
	if (!row) {
		return;
	}
	context_menu->set_position(row->get_screen_position() + p_local_pos);
	context_menu->reset_size();
	context_menu->popup();
}

void IndustrialDeviceDock::_on_tag_row_selected(int p_device_index, int p_tag_index) {
	selected_tag_device_index = p_device_index;
	selected_tag_index = p_tag_index;

	if (tag_list) {
		for (int i = 0; i < tag_list->get_child_count(); i++) {
			if (IndustrialTagListRow *row = Object::cast_to<IndustrialTagListRow>(tag_list->get_child(i))) {
				row->set_selected(row->get_device_index() == p_device_index && row->get_tag_index() == p_tag_index);
			}
		}
	}

	_update_action_enabled();
	if (p_device_index >= 0 && p_tag_index >= 0) {
		emit_signal(SNAME("tag_selected"), p_device_index, p_tag_index);
	}
}

void IndustrialDeviceDock::_on_tag_row_context(int p_device_index, int p_tag_index, const Vector2 &p_local_pos) {
	_on_tag_row_selected(p_device_index, p_tag_index);
	if (!tag_context_menu || !tag_list) {
		return;
	}
	IndustrialTagListRow *row = nullptr;
	for (int i = 0; i < tag_list->get_child_count(); i++) {
		IndustrialTagListRow *candidate = Object::cast_to<IndustrialTagListRow>(tag_list->get_child(i));
		if (candidate && candidate->get_device_index() == p_device_index && candidate->get_tag_index() == p_tag_index) {
			row = candidate;
			break;
		}
	}
	if (!row) {
		return;
	}
	tag_context_menu->set_position(row->get_screen_position() + p_local_pos);
	tag_context_menu->reset_size();
	tag_context_menu->popup();
}

void IndustrialDeviceDock::_on_device_filter_changed(const String &) {
	refresh();
}

void IndustrialDeviceDock::_on_device_option_changed(int) {
	refresh();
}

void IndustrialDeviceDock::_on_tag_filter_changed(const String &) {
	tag_page = 1;
	_populate_tag_list();
}

void IndustrialDeviceDock::_on_tag_option_changed(int) {
	tag_page = 1;
	_populate_tag_list();
}

void IndustrialDeviceDock::_on_tag_context_menu_pressed(int p_id) {
	switch (p_id) {
		case TAG_ACTION_DELETE:
			delete_selected_tag();
			break;
		case TAG_ACTION_NEW:
			focus_new_tag();
			break;
	}
}

void IndustrialDeviceDock::_on_context_menu_pressed(int p_id) {
	switch (p_id) {
		case ACTION_DELETE:
			delete_selected();
			break;
		case ACTION_DUPLICATE:
			duplicate_selected();
			break;
		case ACTION_DIAGNOSE:
			show_diagnose();
			break;
	}
}

void IndustrialDeviceDock::delete_selected() {
	if (project.is_null()) {
		return;
	}
	int idx = get_selected_device_index();
	if (idx < 0) {
		return;
	}
	project->remove_device(idx);
	selected_device_index = -1;
	selected_tag_device_index = -1;
	selected_tag_index = -1;
	refresh();
	emit_signal(SNAME("device_selected"), -1);
}

void IndustrialDeviceDock::duplicate_selected() {
	if (project.is_null()) {
		return;
	}
	const int idx = get_selected_device_index();
	if (idx < 0) {
		return;
	}
	const int new_idx = project->duplicate_device(idx);
	if (new_idx < 0) {
		return;
	}
	selected_device_index = new_idx;
	selected_tag_device_index = -1;
	selected_tag_index = -1;
	refresh();
	emit_signal(SNAME("device_selected"), selected_device_index);
}

void IndustrialDeviceDock::show_diagnose() {
	if (get_selected_device_index() < 0) {
		return;
	}
	emit_signal(SNAME("diagnose_requested"));
}

void IndustrialDeviceDock::move_to_group() {
	// Scan groups removed; no-op for EditorNode::DEVICE_MOVE_GROUP.
}

void IndustrialDeviceDock::export_csv() {
	// Device CSV export not required for this dock milestone.
}

void IndustrialDeviceDock::focus_new_device() {
	emit_signal(SNAME("new_device_requested"));
}

void IndustrialDeviceDock::focus_edit_device() {
	const int idx = get_selected_device_index();
	if (idx >= 0) {
		emit_signal(SNAME("edit_device_requested"), idx);
	}
}

void IndustrialDeviceDock::focus_new_tag() {
	if (selected_device_index >= 0) {
		emit_signal(SNAME("new_tag_requested"), selected_device_index);
	}
}

void IndustrialDeviceDock::delete_selected_tag() {
	if (project.is_null()) {
		return;
	}
	const int device_index = get_selected_tag_device_index();
	const int tag_index = get_selected_tag_index();
	if (device_index >= 0 && tag_index >= 0) {
		project->remove_tag(device_index, tag_index);
		selected_tag_device_index = -1;
		selected_tag_index = -1;
		refresh();
		// Return Properties to the device form (Scheme A1).
		emit_signal(SNAME("tag_selected"), -1, -1);
		if (selected_device_index >= 0) {
			emit_signal(SNAME("device_selected"), selected_device_index);
		}
	}
}

void IndustrialDeviceDock::clear_device_tags() {
	const int di = get_selected_device_index();
	if (di < 0 || project.is_null() || !tag_clear_confirm_dialog) {
		return;
	}
	const int n = project->get_tag_count_for_device(di);
	if (n <= 0) {
		return;
	}
	const String name = project->get_device(di).name;
	tag_clear_confirm_dialog->set_text(
			vformat(TTR("Clear all tags on device \"%s\" (%d tags)?\nThis cannot be undone."), name, n));
	tag_clear_confirm_dialog->popup_centered();
}

void IndustrialDeviceDock::_on_clear_tags_confirmed() {
	const int di = get_selected_device_index();
	if (di < 0 || project.is_null()) {
		return;
	}
	if (!project->clear_tags(di)) {
		return;
	}
	selected_tag_device_index = -1;
	selected_tag_index = -1;
	tag_page = 1;
	refresh();
	emit_signal(SNAME("tag_selected"), -1, -1);
	emit_signal(SNAME("device_selected"), di);
}

void IndustrialDeviceDock::edit_selected_tag() {
	const int device_index = get_selected_tag_device_index();
	const int tag_index = get_selected_tag_index();
	if (device_index >= 0 && tag_index >= 0) {
		emit_signal(SNAME("tag_selected"), device_index, tag_index);
	}
}

void IndustrialDeviceDock::export_tags_csv() {
	if (get_selected_device_index() < 0 || project.is_null()) {
		return;
	}
	if (tag_export_dialog) {
		tag_export_dialog->set_current_file("tags.xlsx");
		tag_export_dialog->popup_file_dialog();
	}
}

void IndustrialDeviceDock::import_tags_csv() {
	if (get_selected_device_index() < 0 || project.is_null()) {
		return;
	}
	if (tag_import_dialog) {
		tag_import_dialog->popup_file_dialog();
	}
}

void IndustrialDeviceDock::_show_tag_sheet_result(int p_added, int p_updated, int p_failed, const String &p_detail) {
	if (!tag_sheet_result_dialog) {
		return;
	}
	tag_sheet_result_dialog->set_title(TTR("Tag Import"));
	const int applied = p_added + p_updated;
	String summary = vformat(TTR("Applied %d tags (added %d, updated %d, failed %d)"), applied, p_added, p_updated, p_failed);
	// Put failure detail first so it is visible even if the dialog is short.
	String text;
	if (!p_detail.is_empty() && p_failed > 0) {
		text = p_detail + "\n\n" + summary;
	} else if (!p_detail.is_empty()) {
		text = summary + "\n" + p_detail;
	} else {
		text = summary;
	}
	tag_sheet_result_dialog->set_text(text);
	tag_sheet_result_dialog->popup_centered();
}

void IndustrialDeviceDock::_on_tag_import_file_selected(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	if (ext == "csv") {
		_import_tags_csv_local(p_path);
	} else if (ext == "xlsx" || ext == "xlsm") {
		_import_tags_xlsx_http(p_path);
	} else if (ext == "xls") {
		if (tag_sheet_result_dialog) {
			tag_sheet_result_dialog->set_title(TTR("Tag Import"));
			tag_sheet_result_dialog->set_text(TTR("Legacy .xls is not supported. Please save as .xlsx and import again."));
			tag_sheet_result_dialog->popup_centered();
		}
	} else {
		_show_tag_sheet_result(0, 0, 1);
	}
}

void IndustrialDeviceDock::_on_tag_export_file_selected(const String &p_path) {
	String path = p_path;
	const String ext = path.get_extension().to_lower();
	if (ext.is_empty()) {
		path += ".xlsx";
	}
	const String use_ext = path.get_extension().to_lower();
	if (use_ext == "csv") {
		_export_tags_csv_local(path);
	} else {
		_export_tags_xlsx_http(path);
	}
}

void IndustrialDeviceDock::_import_tags_csv_local(const String &p_path) {
	const int di = get_selected_device_index();
	IndustrialTagImportResult result;
	const Error err = industrial_import_device_tags_csv(project, di, p_path, &result);
	if (err != OK) {
		_show_tag_sheet_result(0, 0, 1);
		return;
	}
	tag_page = 1;
	refresh();
	String detail;
	if (di >= 0 && project.is_valid()) {
		detail = vformat(TTR("Device tag count now: %d"), project->get_tag_count_for_device(di));
	}
	_show_tag_sheet_result(result.added, result.updated, result.failed, detail);
}

void IndustrialDeviceDock::_export_tags_csv_local(const String &p_path) {
	const int di = get_selected_device_index();
	const Error err = industrial_export_device_tags_csv(project, di, p_path);
	if (err != OK && tag_sheet_result_dialog) {
		tag_sheet_result_dialog->set_title(TTR("Tag Export"));
		tag_sheet_result_dialog->set_text(TTR("Export failed."));
		tag_sheet_result_dialog->popup_centered();
	}
}

void IndustrialDeviceDock::_import_tags_xlsx_http(const String &p_path) {
	const int di = get_selected_device_index();
	if (di < 0 || project.is_null() || !tag_sheet_http) {
		return;
	}
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		_show_tag_sheet_result(0, 0, 1,
				vformat(TTR("Cannot open file:\n%s\nClose Excel or other apps that may lock it, then try again."), p_path));
		return;
	}
	const PackedByteArray file_bytes = f->get_buffer(f->get_length());
	f->close();
	if (file_bytes.is_empty()) {
		_show_tag_sheet_result(0, 0, 1,
				vformat(TTR("File is empty or unreadable:\n%s"), p_path));
		return;
	}

	const IndustrialDeviceData &dev = project->get_device(di);
	Array existing;
	for (int i = 0; i < (int)dev.tags.size(); i++) {
		existing.push_back(dev.tags[i].name);
	}

	const String boundary = "----IndustrialTagSheetBoundary7MA4YWxkTrZu0gW";
	PackedByteArray body;
	auto append_str = [&](const String &s) {
		const CharString utf = s.utf8();
		const int from = body.size();
		body.resize(from + utf.length());
		memcpy(body.ptrw() + from, utf.get_data(), utf.length());
	};
	auto append_bytes = [&](const PackedByteArray &b) {
		const int from = body.size();
		body.resize(from + b.size());
		memcpy(body.ptrw() + from, b.ptr(), b.size());
	};

	append_str("--" + boundary + "\r\n");
	append_str("Content-Disposition: form-data; name=\"device_name\"\r\n\r\n");
	append_str(dev.name + "\r\n");
	append_str("--" + boundary + "\r\n");
	append_str("Content-Disposition: form-data; name=\"existing\"\r\n\r\n");
	append_str(JSON::stringify(existing) + "\r\n");
	append_str("--" + boundary + "\r\n");
	append_str("Content-Disposition: form-data; name=\"file\"; filename=\"" + p_path.get_file() + "\"\r\n");
	append_str("Content-Type: application/vnd.openxmlformats-officedocument.spreadsheetml.sheet\r\n\r\n");
	append_bytes(file_bytes);
	append_str("\r\n--" + boundary + "--\r\n");

	PackedStringArray headers;
	headers.push_back("Content-Type: multipart/form-data; boundary=" + boundary);
	tag_sheet_http_is_import = true;
	tag_sheet_pending_export_path.clear();
	const String url = IndustrialRuntimeClient::get_runtime_url() + "/api/v1/tags/sheet/import";
	const Error err = tag_sheet_http->request_raw(url, headers, HTTPClient::METHOD_POST, body);
	if (err != OK) {
		_show_tag_sheet_result(0, 0, 1,
				vformat(TTR("Could not start HTTP request to runtime (error %d).\nURL: %s"), (int)err, url));
	}
}

void IndustrialDeviceDock::_export_tags_xlsx_http(const String &p_path) {
	const int di = get_selected_device_index();
	if (di < 0 || project.is_null() || !tag_sheet_http) {
		return;
	}
	const IndustrialDeviceData &dev = project->get_device(di);
	Dictionary payload;
	payload["format"] = "xlsx";
	payload["device_name"] = dev.name;
	Array tags;
	Vector<Dictionary> rows = industrial_device_tags_to_sheet_dicts(project, di);
	for (int i = 0; i < rows.size(); i++) {
		tags.push_back(rows[i]);
	}
	payload["tags"] = tags;

	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	tag_sheet_http_is_import = false;
	tag_sheet_pending_export_path = p_path;
	const String url = IndustrialRuntimeClient::get_runtime_url() + "/api/v1/tags/sheet/export";
	const Error err = tag_sheet_http->request(url, headers, HTTPClient::METHOD_POST, JSON::stringify(payload));
	if (err != OK) {
		tag_sheet_pending_export_path.clear();
		if (tag_sheet_result_dialog) {
			tag_sheet_result_dialog->set_title(TTR("Tag Export"));
			tag_sheet_result_dialog->set_text(TTR("Export failed."));
			tag_sheet_result_dialog->popup_centered();
		}
	}
}

void IndustrialDeviceDock::_on_tag_sheet_http_completed(int p_result, int p_response_code, const PackedStringArray & /*p_headers*/, const PackedByteArray &p_body) {
	if (tag_sheet_http_is_import) {
		if (p_result != HTTPRequest::RESULT_SUCCESS || p_response_code != 200) {
			const String body = String::utf8((const char *)p_body.ptr(), p_body.size());
			print_line(vformat("industrial_editor: tag sheet import failed result=%d http=%d body=%s",
					p_result, p_response_code, body.substr(0, 500)));
			String detail = vformat(TTR("HTTP error: result=%d status=%d"), p_result, p_response_code);
			if (!body.is_empty()) {
				detail += "\n" + body.substr(0, 300);
			} else {
				detail += "\n" + TTR("Is the runtime reachable at industrial/runtime/url?");
			}
			_show_tag_sheet_result(0, 0, 1, detail);
			return;
		}
		const String text = String::utf8((const char *)p_body.ptr(), p_body.size());
		Variant parsed = JSON::parse_string(text);
		if (parsed.get_type() != Variant::DICTIONARY) {
			print_line(vformat("industrial_editor: tag sheet import bad JSON (%d bytes)", p_body.size()));
			_show_tag_sheet_result(0, 0, 1, TTR("Invalid JSON from runtime sheet import."));
			return;
		}
		Dictionary root = parsed;
		Dictionary summary = root.get("summary", Dictionary());
		Array tags = root.get("tags", Array());
		Array failures = root.get("failures", Array());
		IndustrialTagImportResult result;
		const int di = get_selected_device_index();
		if (di < 0 || project.is_null()) {
			const int fail_n = tags.size() > 0 ? (int)tags.size() : 1;
			_show_tag_sheet_result(0, 0, fail_n,
					TTR("No device selected. Select a device, then import again."));
			return;
		}
		project->begin_bulk_edit();
		for (int i = 0; i < tags.size(); i++) {
			if (tags[i].get_type() != Variant::DICTIONARY) {
				result.failed++;
				result.errors.push_back(vformat("row %d: not an object", i + 1));
				continue;
			}
			(void)industrial_apply_sheet_tag_dict(project, di, Dictionary(tags[i]), &result);
		}
		project->end_bulk_edit();
		// Use local apply counts (what landed in the project), not server-side
		// add/update classification against the "existing" name list.
		const int sheet_tags = tags.size();
		const int sheet_failed = summary.has("failed") ? (int)summary.get("failed", 0) : 0;
		const int sheet_total = summary.has("total_rows") ? (int)summary.get("total_rows", sheet_tags) : sheet_tags;
		String detail = vformat(TTR("Sheet data rows: %d. Parsed OK: %d. Parse failed: %d."),
				sheet_total, sheet_tags, sheet_failed);
		if (di >= 0 && project.is_valid()) {
			detail += "\n" + vformat(TTR("Device tag count now: %d"), project->get_tag_count_for_device(di));
		}
		if (result.added + result.updated != sheet_tags) {
			detail += "\n" + vformat(TTR("Apply mismatch: landed %d of %d parsed tags."),
					result.added + result.updated, sheet_tags);
		}
		if (sheet_failed > 0 && failures.size() > 0 && failures[0].get_type() == Variant::DICTIONARY) {
			Dictionary f0 = failures[0];
			detail += "\n" + vformat(TTR("First parse error (row %d): %s"), (int)f0.get("row", 0), String(f0.get("error", "")));
		}
		if (!result.errors.is_empty()) {
			detail += "\n" + vformat(TTR("First apply error: %s"), result.errors[0]);
		}
		tag_page = 1;
		refresh();
		_show_tag_sheet_result(result.added, result.updated, result.failed + sheet_failed, detail);
		return;
	}

	// Export path.
	const String path = tag_sheet_pending_export_path;
	tag_sheet_pending_export_path.clear();
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_response_code != 200 || path.is_empty()) {
		if (tag_sheet_result_dialog) {
			tag_sheet_result_dialog->set_title(TTR("Tag Export"));
			tag_sheet_result_dialog->set_text(TTR("Export failed. Is the runtime reachable?"));
			tag_sheet_result_dialog->popup_centered();
		}
		return;
	}
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_null()) {
		if (tag_sheet_result_dialog) {
			tag_sheet_result_dialog->set_title(TTR("Tag Export"));
			tag_sheet_result_dialog->set_text(TTR("Export failed."));
			tag_sheet_result_dialog->popup_centered();
		}
		return;
	}
	f->store_buffer(p_body);
	f->close();
}
