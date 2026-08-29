#include "industrial_device_dock.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "editor/editor_string_names.h"
#include "core/object/callable_mp.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "scene/gui/box_container.h"

IndustrialDeviceDock::IndustrialDeviceDock() {
	_build_ui();
}

IndustrialDeviceDock::~IndustrialDeviceDock() {}

void IndustrialDeviceDock::_bind_methods() {
	ADD_SIGNAL(MethodInfo("device_selected", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("new_device_requested"));
	ADD_SIGNAL(MethodInfo("edit_device_requested", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("tag_selected", PropertyInfo(Variant::INT, "device_index"), PropertyInfo(Variant::INT, "tag_index")));
	ADD_SIGNAL(MethodInfo("new_tag_requested", PropertyInfo(Variant::INT, "device_index")));
	ADD_SIGNAL(MethodInfo("edit_tag_requested", PropertyInfo(Variant::INT, "device_index"), PropertyInfo(Variant::INT, "tag_index")));
}

void IndustrialDeviceDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			refresh();
			break;
		case NOTIFICATION_TRANSLATION_CHANGED:
			_update_ui_text();
			refresh();
			update_tab_style();
			break;
	}
}

void IndustrialDeviceDock::_build_ui() {
	set_name("DeviceManagement");
	set_layout_key("IndustrialDeviceManagement");
	set_default_slot(EditorDock::DOCK_SLOT_LEFT_UL);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(vbox);

	// Device list | Tag list — keep mins low so the splitter can move in a
	// narrow left dock; users drag the center handle to reallocate width.
	content_split = memnew(HSplitContainer);
	content_split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content_split->set_split_offset(220);
	vbox->add_child(content_split);

	VBoxContainer *device_panel = memnew(VBoxContainer);
	device_panel->set_custom_minimum_size(Size2(140, 0));
	device_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	device_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	device_panel->set_stretch_ratio(1.0);
	content_split->add_child(device_panel);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->add_theme_constant_override("separation", 4);
	device_panel->add_child(toolbar);

	search = memnew(LineEdit);
	search->set_custom_minimum_size(Size2(0, 24));
	search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceDock::_on_search_text_changed));
	toolbar->add_child(search);

	view_mode_selector = memnew(OptionButton);
	view_mode_selector->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceDock::_on_view_mode_changed));
	toolbar->add_child(view_mode_selector);

	btn_new = memnew(Button);
	btn_new->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_button_pressed).bind(0));
	toolbar->add_child(btn_new);

	// Secondary action buttons (kept for discoverability).
	HBoxContainer *btn_bar = memnew(HBoxContainer);
	btn_bar->add_theme_constant_override("separation", 2);
	device_panel->add_child(btn_bar);

	btn_edit = memnew(Button);
	btn_edit->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_button_pressed).bind(1));
	btn_bar->add_child(btn_edit);

	btn_delete = memnew(Button);
	btn_delete->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_button_pressed).bind(2));
	btn_bar->add_child(btn_delete);

	btn_duplicate = memnew(Button);
	btn_duplicate->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::_on_button_pressed).bind(3));
	btn_bar->add_child(btn_duplicate);

	// Device tree — 4 columns. Height-only min so width can shrink with the split.
	tree = memnew(Tree);
	tree->set_custom_minimum_size(Size2(0, 120));
	tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_columns(4);
	tree->set_column_expand_ratio(0, 3);
	tree->set_column_expand_ratio(1, 3);
	tree->set_column_expand_ratio(2, 2);
	tree->set_column_expand_ratio(3, 1);
	tree->set_allow_rmb_select(true);
	tree->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceDock::_on_tree_selection_changed));
	tree->connect("item_mouse_selected", callable_mp(this, &IndustrialDeviceDock::_on_tree_item_mouse_selected));
	device_panel->add_child(tree);

	// Empty state label (shown when no devices exist).
	empty_state_label = memnew(Label);
	empty_state_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	empty_state_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	empty_state_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	empty_state_label->set_visible(false);
	device_panel->add_child(empty_state_label);

	// The tag list lives in the same dock and follows the selected device.
	VBoxContainer *tag_panel = memnew(VBoxContainer);
	tag_panel->set_custom_minimum_size(Size2(140, 0));
	tag_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_panel->set_stretch_ratio(1.2);
	content_split->add_child(tag_panel);

	HBoxContainer *tag_toolbar = memnew(HBoxContainer);
	tag_toolbar->add_theme_constant_override("separation", 4);
	tag_panel->add_child(tag_toolbar);

	tag_scope_label = memnew(Label);
	tag_scope_label->set_custom_minimum_size(Size2(64, 0));
	tag_toolbar->add_child(tag_scope_label);

	tag_search = memnew(LineEdit);
	tag_search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_search->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialDeviceDock::_on_tag_search_text_changed));
	tag_toolbar->add_child(tag_search);

	btn_new_tag = memnew(Button);
	btn_new_tag->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::focus_new_tag));
	tag_toolbar->add_child(btn_new_tag);

	btn_delete_tag = memnew(Button);
	btn_delete_tag->connect(SceneStringName(pressed), callable_mp(this, &IndustrialDeviceDock::delete_selected_tag));
	tag_toolbar->add_child(btn_delete_tag);

	tag_tree = memnew(Tree);
	tag_tree->set_custom_minimum_size(Size2(0, 120));
	tag_tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_tree->set_columns(5);
	tag_tree->set_column_expand_ratio(0, 3);
	tag_tree->set_column_expand_ratio(1, 3);
	tag_tree->set_column_expand_ratio(2, 2);
	tag_tree->set_column_expand_ratio(3, 1);
	tag_tree->set_column_expand_ratio(4, 1);
	tag_tree->set_allow_rmb_select(true);
	tag_tree->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialDeviceDock::_on_tag_tree_selection_changed));
	tag_tree->connect("item_mouse_selected", callable_mp(this, &IndustrialDeviceDock::_on_tag_tree_item_mouse_selected));
	tag_panel->add_child(tag_tree);

	tag_empty_state_label = memnew(Label);
	tag_empty_state_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	tag_empty_state_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tag_empty_state_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_empty_state_label->set_visible(false);
	tag_panel->add_child(tag_empty_state_label);

	_build_context_menu();
	_build_tag_context_menu();
	_update_ui_text();
}

void IndustrialDeviceDock::_update_ui_text() {
	set_title(TTR("Device Management"));

	if (search) {
		search->set_placeholder(TTR("Search devices..."));
	}
	if (view_mode_selector) {
		const int selected = view_mode_selector->get_selected();
		view_mode_selector->clear();
		view_mode_selector->add_item(TTR("Flat"));
		view_mode_selector->add_item(TTR("By Driver"));
		view_mode_selector->add_item(TTR("By Group"));
		view_mode_selector->select(CLAMP(selected, 0, 2));
	}
	if (btn_new) {
		btn_new->set_text(TTR("+ New"));
	}
	if (btn_edit) {
		btn_edit->set_text(TTR("Edit"));
	}
	if (btn_delete) {
		btn_delete->set_text(TTR("Delete"));
	}
	if (btn_duplicate) {
		btn_duplicate->set_text(TTR("Duplicate"));
	}
	if (tree) {
		tree->set_column_title(0, TTR("Device"));
		tree->set_column_title(1, TTR("Driver / Vendor"));
		tree->set_column_title(2, TTR("Status"));
		tree->set_column_title(3, TTR("Tags"));
	}
	if (empty_state_label) {
		empty_state_label->set_text(TTR("No devices yet. Click + to create one."));
	}
	if (tag_search) {
		tag_search->set_placeholder(TTR("Search tags..."));
	}
	if (btn_new_tag) {
		btn_new_tag->set_text(TTR("+ Tag"));
	}
	if (btn_delete_tag) {
		btn_delete_tag->set_text(TTR("Delete"));
	}
	if (tag_tree) {
		tag_tree->set_column_title(0, TTR("Name"));
		tag_tree->set_column_title(1, TTR("Address"));
		tag_tree->set_column_title(2, TTR("Type"));
		tag_tree->set_column_title(3, TTR("Writable"));
		tag_tree->set_column_title(4, TTR("Unit"));
	}
	if (tag_empty_state_label && selected_device_index < 0) {
		tag_empty_state_label->set_text(TTR("Select a device to view its tags."));
	}
	if (tag_scope_label && selected_device_index < 0) {
		tag_scope_label->set_text(TTR("Tags"));
	}

	if (context_menu) {
		context_menu->clear();
		context_menu->add_item(TTR("Edit Device"), ACTION_EDIT);
		context_menu->add_item(TTR("Delete Device"), ACTION_DELETE);
		context_menu->add_separator();
		context_menu->add_item(TTR("Duplicate"), ACTION_DUPLICATE);
		context_menu->add_item(TTR("Move to Group..."), ACTION_MOVE_GROUP);
		context_menu->add_separator();
		context_menu->add_item(TTR("Diagnose"), ACTION_DIAGNOSE);
	}
	if (tag_context_menu) {
		tag_context_menu->clear();
		tag_context_menu->add_item(TTR("Edit Tag"), TAG_ACTION_EDIT);
		tag_context_menu->add_item(TTR("Delete Tag"), TAG_ACTION_DELETE);
		tag_context_menu->add_separator();
		tag_context_menu->add_item(TTR("New Tag"), TAG_ACTION_NEW);
	}
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
	refresh();
}

void IndustrialDeviceDock::refresh() {
	if (!tree) {
		return;
	}
	// Remember the currently selected device so we can restore it after
	// rebuilding the tree (selected TreeItem* is invalidated by clear()).
	int selected_idx_before = get_selected_device_index();
	tree->clear();
	if (selected_idx_before >= 0) {
		selected_device_index = selected_idx_before;
	}
	if (project.is_null()) {
		if (empty_state_label) {
			empty_state_label->set_visible(false);
		}
		return;
	}

	int count = project->get_device_count();
	if (count == 0) {
		tree->set_visible(false);
		if (empty_state_label) {
			empty_state_label->set_visible(true);
		}
		selected_device_index = -1;
		_populate_tag_tree();
		return;
	}
	tree->set_visible(true);
	if (empty_state_label) {
		empty_state_label->set_visible(false);
	}

	TreeItem *root = tree->create_item();
	switch (view_mode) {
		case VIEW_FLAT:
			_populate_tree_flat(root);
			break;
		case VIEW_BY_DRIVER:
			_populate_tree_by_driver(root);
			break;
		case VIEW_BY_GROUP:
			_populate_tree_by_group(root);
			break;
	}

	// Restore the previous selection if it still exists.
	if (selected_idx_before >= 0) {
		TreeItem *found = _find_item_by_device_index(root, selected_idx_before);
		if (found) {
			found->select(0);
		}
	}
	_populate_tag_tree();
}

void IndustrialDeviceDock::_populate_tree_flat(TreeItem *p_root) {
	int count = project->get_device_count();
	for (int i = 0; i < count; i++) {
		const auto &dev = project->get_device(i);
		TreeItem *item = tree->create_item(p_root);

		item->set_text(0, dev.name);
		if (!dev.description.is_empty()) {
			item->set_tooltip_text(0, dev.description);
		}
		item->set_text(1, industrial_get_driver_name(dev.driver));
		item->set_text(2, dev.enabled ? TTR("Enabled") : TTR("Disabled"));
		item->set_text(3, itos(project->get_tag_count_for_device(i)));
		item->set_metadata(0, i);
	}
}

void IndustrialDeviceDock::_populate_tree_by_driver(TreeItem *p_root) {
	// Group device indices by driver index.
	HashMap<int, List<int>> driver_groups;
	int count = project->get_device_count();
	for (int i = 0; i < count; i++) {
		driver_groups[project->get_device(i).driver].push_back(i);
	}

	for (const KeyValue<int, List<int>> &kv : driver_groups) {
		int drv_idx = kv.key;
		const List<int> &indices = kv.value;

		TreeItem *group = tree->create_item(p_root);
		group->set_text(0, industrial_get_driver_name(drv_idx) + " (" + itos(indices.size()) + ")");
		group->set_metadata(0, -1);
		group->set_selectable(0, false);
		group->set_custom_color(0, Color(0.55f, 0.7f, 0.9f));

		for (const int &dev_idx : indices) {
			const auto &dev = project->get_device(dev_idx);
			TreeItem *item = tree->create_item(group);
			item->set_text(0, dev.name);
			if (!dev.description.is_empty()) {
				item->set_tooltip_text(0, dev.description);
			}
			item->set_text(2, dev.enabled ? TTR("Enabled") : TTR("Disabled"));
			item->set_text(3, itos(project->get_tag_count_for_device(dev_idx)));
			item->set_metadata(0, dev_idx);
		}
	}
}

void IndustrialDeviceDock::_populate_tree_by_group(TreeItem *p_root) {
	// Task 8 drops VIEW_BY_GROUP. Stub lists every device under Ungrouped.
	const int count = project->get_device_count();
	TreeItem *group = tree->create_item(p_root);
	group->set_text(0, TTR("Ungrouped") + " (" + itos(count) + ")");
	group->set_metadata(0, -1);
	group->set_selectable(0, false);
	group->set_custom_color(0, Color(0.55f, 0.7f, 0.9f));

	for (int i = 0; i < count; i++) {
		const auto &dev = project->get_device(i);
		TreeItem *item = tree->create_item(group);
		item->set_text(0, dev.name);
		if (!dev.description.is_empty()) {
			item->set_tooltip_text(0, dev.description);
		}
		item->set_text(1, industrial_get_driver_name(dev.driver));
		item->set_text(2, dev.enabled ? TTR("Enabled") : TTR("Disabled"));
		item->set_text(3, itos(project->get_tag_count_for_device(i)));
		item->set_metadata(0, i);
	}
}

TreeItem *IndustrialDeviceDock::_find_item_by_device_index(TreeItem *p_root, int p_idx) {
	if (!p_root) {
		return nullptr;
	}
	TreeItem *top = p_root->get_first_child();
	while (top) {
		// Leaf device row (flat view).
		if ((int)top->get_metadata(0) == p_idx && top->is_selectable(0)) {
			return top;
		}
		// Grouped view: search group children.
		TreeItem *child = top->get_first_child();
		while (child) {
			if ((int)child->get_metadata(0) == p_idx && child->is_selectable(0)) {
				return child;
			}
			child = child->get_next();
		}
		top = top->get_next();
	}
	return nullptr;
}

int IndustrialDeviceDock::get_selected_device_index() const {
	if (!tree || !tree->get_selected()) {
		return -1;
	}
	return tree->get_selected()->get_metadata(0);
}

void IndustrialDeviceDock::_on_tree_selection_changed() {
	// Emit device_selected so the right-side form (or other consumers) can
	// react to selection changes. -1 means nothing selected.
	selected_device_index = get_selected_device_index();
	emit_signal("device_selected", selected_device_index);
	_populate_tag_tree();
}

void IndustrialDeviceDock::_on_search_text_changed(const String &p_text) {
	if (!tree || project.is_null()) {
		return;
	}
	String search_lower = p_text.to_lower();
	TreeItem *root = tree->get_root();
	if (!root) {
		return;
	}

	int count = project->get_device_count();
	TreeItem *top = root->get_first_child();
	while (top) {
		TreeItem *first_child = top->get_first_child();
		if (first_child) {
			// Grouped view: filter children; hide group if none visible.
			bool any_visible = false;
			TreeItem *child = first_child;
			while (child) {
				bool item_visible = search_lower.is_empty();
				if (!item_visible) {
					int dev_idx = child->get_metadata(0);
					if (dev_idx >= 0 && dev_idx < count) {
						String name = project->get_device(dev_idx).name.to_lower();
						item_visible = name.find(search_lower) >= 0;
					}
				}
				child->set_visible(item_visible);
				if (item_visible) {
					any_visible = true;
				}
				child = child->get_next();
			}
			top->set_visible(any_visible);
		} else {
			// Flat view: device row directly under root.
			bool item_visible = search_lower.is_empty();
			if (!item_visible) {
				int dev_idx = top->get_metadata(0);
				if (dev_idx >= 0 && dev_idx < count) {
					String name = project->get_device(dev_idx).name.to_lower();
					item_visible = name.find(search_lower) >= 0;
				}
			}
			top->set_visible(item_visible);
		}
		top = top->get_next();
	}
}

void IndustrialDeviceDock::_populate_tag_tree() {
	if (!tag_tree) {
		return;
	}
	tag_tree->clear();
	if (project.is_null() || selected_device_index < 0 || selected_device_index >= project->get_device_count()) {
		if (tag_scope_label) {
			tag_scope_label->set_text(TTR("Tags"));
		}
		if (tag_empty_state_label) {
			tag_empty_state_label->set_text(TTR("Select a device to view its tags."));
			tag_empty_state_label->set_visible(true);
		}
		return;
	}

	const IndustrialDeviceData &dev = project->get_device(selected_device_index);
	if (tag_scope_label) {
		tag_scope_label->set_text(vformat(TTR("Tags: %s"), dev.name));
	}
	String filter = tag_search ? tag_search->get_text().to_lower() : String();
	int visible_count = 0;
	TreeItem *root = tag_tree->create_item();
	for (int i = 0; i < (int)dev.tags.size(); i++) {
		const IndustrialTagData &tag = dev.tags[i];
		if (!filter.is_empty() && tag.name.to_lower().find(filter) < 0 && tag.address.to_lower().find(filter) < 0) {
			continue;
		}
		TreeItem *item = tag_tree->create_item(root);
		item->set_text(0, tag.name);
		item->set_text(1, tag.address);
		item->set_text(2, industrial_get_data_type_name(tag.data_type));
		item->set_text(3, tag.writable ? TTR("Yes") : TTR("No"));
		item->set_text(4, tag.unit);
		Dictionary meta;
		meta["device_index"] = selected_device_index;
		meta["tag_index"] = i;
		item->set_metadata(0, meta);
		visible_count++;
	}
	if (tag_empty_state_label) {
		tag_empty_state_label->set_text(visible_count == 0 ? TTR("No tags for this device.") : String());
		tag_empty_state_label->set_visible(visible_count == 0);
	}
}

int IndustrialDeviceDock::get_selected_tag_device_index() const {
	if (!tag_tree || !tag_tree->get_selected()) {
		return -1;
	}
	Variant value = tag_tree->get_selected()->get_metadata(0);
	if (value.get_type() != Variant::DICTIONARY) {
		return -1;
	}
	return (int)((Dictionary)value).get("device_index", -1);
}

int IndustrialDeviceDock::get_selected_tag_index() const {
	if (!tag_tree || !tag_tree->get_selected()) {
		return -1;
	}
	Variant value = tag_tree->get_selected()->get_metadata(0);
	if (value.get_type() != Variant::DICTIONARY) {
		return -1;
	}
	return (int)((Dictionary)value).get("tag_index", -1);
}

void IndustrialDeviceDock::_on_tag_tree_selection_changed() {
	const int device_index = get_selected_tag_device_index();
	const int tag_index = get_selected_tag_index();
	if (device_index >= 0 && tag_index >= 0) {
		emit_signal(SNAME("tag_selected"), device_index, tag_index);
	}
}

void IndustrialDeviceDock::_on_tag_search_text_changed(const String &) {
	_populate_tag_tree();
}

void IndustrialDeviceDock::_on_tag_tree_item_mouse_selected(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT || !tag_context_menu || get_selected_tag_index() < 0) {
		return;
	}
	tag_context_menu->set_position(tag_tree->get_screen_position() + p_pos);
	tag_context_menu->reset_size();
	tag_context_menu->popup();
}

void IndustrialDeviceDock::_on_tag_context_menu_pressed(int p_id) {
	switch (p_id) {
		case TAG_ACTION_EDIT:
			edit_selected_tag();
			break;
		case TAG_ACTION_DELETE:
			delete_selected_tag();
			break;
		case TAG_ACTION_NEW:
			focus_new_tag();
			break;
	}
}

void IndustrialDeviceDock::_on_button_pressed(int p_action) {
	switch (p_action) {
		case 0: focus_new_device(); break;
		case 1: focus_edit_device(); break;
		case 2: delete_selected(); break;
		case 3: duplicate_selected(); break;
	}
}

void IndustrialDeviceDock::_on_view_mode_changed(int p_idx) {
	view_mode = (ViewMode)p_idx;
	refresh();
}

void IndustrialDeviceDock::_on_tree_item_mouse_selected(const Vector2 &p_pos, MouseButton p_button) {
	if (!context_menu || p_button != MouseButton::RIGHT) {
		return;
	}
	// Only show the menu when a device row (not a group header) is selected.
	if (get_selected_device_index() < 0) {
		return;
	}
	context_menu->set_position(tree->get_screen_position() + p_pos);
	context_menu->reset_size();
	context_menu->popup();
}

void IndustrialDeviceDock::_on_context_menu_pressed(int p_id) {
	switch (p_id) {
		case ACTION_EDIT: focus_edit_device(); break;
		case ACTION_DELETE: delete_selected(); break;
		case ACTION_DUPLICATE: duplicate_selected(); break;
		case ACTION_MOVE_GROUP: move_to_group(); break;
		case ACTION_DIAGNOSE: show_diagnose(); break;
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
	refresh();
}

void IndustrialDeviceDock::duplicate_selected() {
	if (project.is_null()) {
		return;
	}
	int idx = get_selected_device_index();
	if (idx < 0) {
		return;
	}
	project->duplicate_device(idx);
	refresh();
}

void IndustrialDeviceDock::show_diagnose() {
	// TODO: Implement diagnostics dialog.
}

void IndustrialDeviceDock::move_to_group() {
	// TODO: Implement move-to-group dialog.
}

void IndustrialDeviceDock::import_csv() {
	// TODO: Implement CSV import.
}

void IndustrialDeviceDock::export_csv() {
	// TODO: Implement CSV export.
}

void IndustrialDeviceDock::focus_new_device() {
	// The plugin owns the dialog. Emit a request so this dock remains a
	// reusable view of the project model instead of owning editor windows.
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

void IndustrialDeviceDock::edit_selected_tag() {
	const int device_index = get_selected_tag_device_index();
	const int tag_index = get_selected_tag_index();
	if (device_index >= 0 && tag_index >= 0) {
		emit_signal(SNAME("edit_tag_requested"), device_index, tag_index);
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
		refresh();
	}
}

void IndustrialDeviceDock::export_tags_csv() {
	// CSV export remains available through the project model; the editor menu
	// can provide a destination dialog when that workflow is enabled.
}
