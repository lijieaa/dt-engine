#pragma once

#include "editor/docks/editor_dock.h"
#include "scene/gui/tree.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/label.h"
#include "scene/gui/split_container.h"
#include "core/input/input_enums.h"
#include "core/object/ref_counted.h"

class IndustrialProject;

// Left-side dock for device list management.
class IndustrialDeviceDock : public EditorDock {
	GDCLASS(IndustrialDeviceDock, EditorDock);

public:
	IndustrialDeviceDock();
	~IndustrialDeviceDock() override;

	void set_project(Ref<IndustrialProject> p_project);
	void refresh();
	int get_selected_device_index() const;

	// Actions triggered by menu items.
	void delete_selected();
	void duplicate_selected();
	void show_diagnose();
	void move_to_group();
	void import_csv();
	void export_csv();
	void focus_new_device();
	void focus_edit_device();
	void focus_new_tag();
	void edit_selected_tag();
	void delete_selected_tag();
	void export_tags_csv();
	int get_selected_tag_device_index() const;
	int get_selected_tag_index() const;

private:
	Ref<IndustrialProject> project;
	HSplitContainer *content_split = nullptr;
	Tree *tree = nullptr; // Device list.
	Tree *tag_tree = nullptr;
	LineEdit *search = nullptr; // Device search.
	LineEdit *tag_search = nullptr;
	OptionButton *view_mode_selector = nullptr;
	Button *btn_new = nullptr;
	Button *btn_edit = nullptr;
	Button *btn_delete = nullptr;
	Button *btn_duplicate = nullptr;
	Button *btn_new_tag = nullptr;
	Button *btn_delete_tag = nullptr;
	PopupMenu *context_menu = nullptr;
	PopupMenu *tag_context_menu = nullptr;
	Label *empty_state_label = nullptr;
	Label *tag_empty_state_label = nullptr;
	Label *tag_scope_label = nullptr;
	int selected_device_index = -1;

	enum ViewMode {
		VIEW_FLAT,
		VIEW_BY_DRIVER,
		VIEW_BY_GROUP,
	};
	ViewMode view_mode = VIEW_FLAT;

	// Context menu action ids (must match _build_context_menu order).
	enum ContextAction {
		ACTION_EDIT = 0,
		ACTION_DELETE,
		ACTION_DUPLICATE,
		ACTION_MOVE_GROUP,
		ACTION_DIAGNOSE,
	};

	void _build_ui();
	void _update_ui_text();
	void _build_context_menu();
	void _build_tag_context_menu();
	void _populate_tree_flat(TreeItem *p_root);
	void _populate_tree_by_driver(TreeItem *p_root);
	void _populate_tree_by_group(TreeItem *p_root);
	TreeItem *_find_item_by_device_index(TreeItem *p_root, int p_idx);
	void _on_tree_selection_changed();
	void _on_search_text_changed(const String &p_text);
	void _on_button_pressed(int p_action);
	void _on_view_mode_changed(int p_idx);
	void _on_tree_item_mouse_selected(const Vector2 &p_pos, MouseButton p_button);
	void _on_context_menu_pressed(int p_id);
	void _on_tag_tree_selection_changed();
	void _on_tag_search_text_changed(const String &p_text);
	void _on_tag_tree_item_mouse_selected(const Vector2 &p_pos, MouseButton p_button);
	void _on_tag_context_menu_pressed(int p_id);
	void _populate_tag_tree();

	enum TagContextAction {
		TAG_ACTION_EDIT = 0,
		TAG_ACTION_DELETE,
		TAG_ACTION_NEW,
	};

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
