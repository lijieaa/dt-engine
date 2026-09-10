#pragma once

#include "editor/docks/editor_dock.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/label.h"
#include "scene/gui/split_container.h"
#include "core/input/input_enums.h"
#include "core/object/ref_counted.h"

class IndustrialProject;
struct IndustrialTagData;
class ScrollContainer;
class VBoxContainer;
class HBoxContainer;
class IndustrialDeviceListRow;
class IndustrialTagListRow;
class EditorFileDialog;
class AcceptDialog;
class ConfirmationDialog;
class HTTPRequest;

// Left-side dock for device list management (spec §3 / §3.2.8 custom rows).
class IndustrialDeviceDock : public EditorDock {
	GDCLASS(IndustrialDeviceDock, EditorDock);

public:
	IndustrialDeviceDock();
	~IndustrialDeviceDock() override;

	void set_project(Ref<IndustrialProject> p_project);
	void refresh();
	/// Apply live runtime device statuses (id -> {status, error}). Empty clears.
	void set_runtime_device_statuses(const Dictionary &p_by_id);
	void clear_runtime_device_statuses();
	int get_selected_device_index() const { return selected_device_index; }

	void delete_selected();
	void duplicate_selected();
	void show_diagnose();
	void move_to_group();
	void export_csv();
	void focus_new_device();
	void focus_edit_device();
	void focus_new_tag();
	void edit_selected_tag();
	void delete_selected_tag();
	void clear_device_tags();
	void export_tags_csv();
	void import_tags_csv();
	int get_selected_tag_device_index() const { return selected_tag_device_index; }
	int get_selected_tag_index() const { return selected_tag_index; }

private:
	Ref<IndustrialProject> project;
	HSplitContainer *content_split = nullptr;

	LineEdit *device_name_filter = nullptr;
	LineEdit *device_addr_filter = nullptr;
	OptionButton *device_type_filter = nullptr;
	OptionButton *device_enabled_filter = nullptr;

	ScrollContainer *device_scroll = nullptr;
	VBoxContainer *device_list = nullptr;
	Button *btn_new = nullptr;
	Button *btn_duplicate = nullptr;
	Button *btn_delete = nullptr;
	Button *btn_diagnose = nullptr;
	Button *btn_empty_create = nullptr;
	Label *empty_state_label = nullptr;

	LineEdit *tag_name_filter = nullptr;
	LineEdit *tag_addr_filter = nullptr;
	OptionButton *tag_type_filter = nullptr;
	OptionButton *tag_rw_filter = nullptr;
	Label *tag_scope_label = nullptr;
	ScrollContainer *tag_scroll = nullptr;
	VBoxContainer *tag_list = nullptr;
	HBoxContainer *tag_page_bar = nullptr;
	int tag_page = 1;
	static constexpr int TAG_PAGE_LEN = 50;
	Button *btn_new_tag = nullptr;
	Button *btn_delete_tag = nullptr;
	Button *btn_clear_tags = nullptr;
	Button *btn_import_tags = nullptr;
	Button *btn_export_tags = nullptr;
	Label *tag_empty_state_label = nullptr;

	EditorFileDialog *tag_import_dialog = nullptr;
	EditorFileDialog *tag_export_dialog = nullptr;
	AcceptDialog *tag_sheet_result_dialog = nullptr;
	ConfirmationDialog *tag_clear_confirm_dialog = nullptr;
	HTTPRequest *tag_sheet_http = nullptr;
	String tag_sheet_pending_export_path;
	bool tag_sheet_http_is_import = false;

	PopupMenu *context_menu = nullptr;
	PopupMenu *tag_context_menu = nullptr;
	int selected_device_index = -1;
	int selected_tag_device_index = -1;
	int selected_tag_index = -1;
	Dictionary runtime_status_by_id; // device id -> Dictionary{status, error}

	enum ContextAction {
		ACTION_DELETE = 0,
		ACTION_DUPLICATE,
		ACTION_DIAGNOSE,
	};

	enum TagContextAction {
		TAG_ACTION_DELETE = 0,
		TAG_ACTION_NEW,
	};

	enum DeviceEnabledFilter {
		ENABLED_FILTER_ALL = 0,
		ENABLED_FILTER_ENABLED,
		ENABLED_FILTER_DISABLED,
	};

	enum TagRwFilter {
		RW_FILTER_ALL = 0,
		RW_FILTER_READONLY,
		RW_FILTER_WRITABLE,
	};

	void _build_ui();
	void _update_ui_text();
	void _apply_action_icons();
	void _update_action_enabled();
	void _rebuild_device_type_filter();
	void _rebuild_tag_type_filter();
	String _tag_data_type_key(const IndustrialTagData &p_tag) const;
	String _tag_data_type_filter_label(const IndustrialTagData &p_tag) const;
	void _build_context_menu();
	void _build_tag_context_menu();
	bool _device_matches_filters(int p_idx) const;
	bool _tag_matches_filters(const IndustrialTagData &p_tag) const;
	String _device_hover_tooltip(int p_idx) const;
	String _tag_hover_tooltip(int p_device_index, int p_tag_index) const;
	void _populate_device_list();
	void _populate_tag_list();
	void _rebuild_tag_page_bar(int p_page, int p_page_count, int p_total_items);
	void _on_tag_page_pressed(int p_page);
	void _on_device_row_selected(int p_index);
	void _on_device_row_context(int p_index, const Vector2 &p_local_pos);
	void _clear_device_selection();
	void _on_device_list_gui_input(const Ref<InputEvent> &p_event);
	void _on_tag_row_selected(int p_device_index, int p_tag_index);
	void _on_tag_row_context(int p_device_index, int p_tag_index, const Vector2 &p_local_pos);
	void _on_device_filter_changed(const String &p_text = String());
	void _on_device_option_changed(int p_idx);
	void _on_context_menu_pressed(int p_id);
	void _on_tag_filter_changed(const String &p_text = String());
	void _on_tag_option_changed(int p_idx);
	void _on_tag_context_menu_pressed(int p_id);
	void _init_split_offset();
	void _on_tag_import_file_selected(const String &p_path);
	void _on_tag_export_file_selected(const String &p_path);
	void _show_tag_sheet_result(int p_added, int p_updated, int p_failed, const String &p_detail = String());
	void _import_tags_csv_local(const String &p_path);
	void _import_tags_xlsx_http(const String &p_path);
	void _export_tags_csv_local(const String &p_path);
	void _export_tags_xlsx_http(const String &p_path);
	void _on_tag_sheet_http_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_clear_tags_confirmed();

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
