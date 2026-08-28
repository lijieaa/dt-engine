#include "industrial_tag_dock.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "editor/editor_string_names.h"
#include "core/object/callable_mp.h"
#include "scene/gui/box_container.h"

IndustrialTagDock::IndustrialTagDock() {
	_build_ui();
}

IndustrialTagDock::~IndustrialTagDock() {}

void IndustrialTagDock::_build_ui() {
	set_name("Tag Browser");
	set_title(TTR("Tag Browser"));
	set_default_slot(EditorDock::DOCK_SLOT_LEFT_BL);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(vbox);

	// Filter bar.
	HBoxContainer *filter_bar = memnew(HBoxContainer);
	filter_bar->add_theme_constant_override("separation", 4);
	vbox->add_child(filter_bar);

	search = memnew(LineEdit);
	search->set_placeholder(TTR("Search tags..."));
	search->set_custom_minimum_size(Size2(0, 24));
	search->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialTagDock::_on_search_text_changed));
	filter_bar->add_child(search);

	filter_device = memnew(OptionButton);
	filter_device->set_custom_minimum_size(Size2(120, 24));
	filter_device->add_item(TTR("All Devices"));
	filter_device->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialTagDock::_on_filter_changed));
	filter_bar->add_child(filter_device);

	filter_type = memnew(OptionButton);
	filter_type->set_custom_minimum_size(Size2(120, 24));
	filter_type->add_item(TTR("All Types"));
	StringList type_names = industrial_get_data_type_names();
	for (int i = 0; i < type_names.size(); i++) {
		filter_type->add_item(type_names[i]);
	}
	filter_type->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialTagDock::_on_filter_changed));
	filter_bar->add_child(filter_type);

	// Tag tree.
	tree = memnew(Tree);
	tree->set_custom_minimum_size(Size2(0, 200));
	tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_columns(5);
	tree->set_column_title(0, TTR("Device"));
	tree->set_column_title(1, TTR("Address"));
	tree->set_column_title(2, TTR("Name"));
	tree->set_column_title(3, TTR("Type"));
	tree->set_column_title(4, TTR("Writable"));
	tree->set_column_expand_ratio(0, 2);
	tree->set_column_expand_ratio(1, 2);
	tree->set_column_expand_ratio(2, 3);
	tree->set_column_expand_ratio(3, 1);
	tree->set_column_expand_ratio(4, 1);
	vbox->add_child(tree);
}

void IndustrialTagDock::_bind_methods() {
}

void IndustrialTagDock::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		refresh();
	}
}

void IndustrialTagDock::set_project(Ref<IndustrialProject> p_project) {
	project = p_project;
	refresh();
}

void IndustrialTagDock::refresh() {
	if (!tree) {
		return;
	}
	tree->clear();

	// Update device filter.
	if (filter_device && project.is_valid()) {
		while (filter_device->get_item_count() > 1) {
			filter_device->remove_item(filter_device->get_item_count() - 1);
		}
		int dev_count = project->get_device_count();
		for (int i = 0; i < dev_count; i++) {
			filter_device->add_item(project->get_device(i).name);
		}
	}

	_populate_tree();
}

void IndustrialTagDock::_populate_tree() {
	if (project.is_null()) {
		return;
	}

	int dev_count = project->get_device_count();
	for (int di = 0; di < dev_count; di++) {
		const auto &dev = project->get_device(di);

		TreeItem *dev_item = tree->create_item();
		dev_item->set_text(0, dev.name);
		// Godot 4 TreeItem::set_metadata(int column, Variant) — 用 Dictionary 把
		// 多个语义字段合并存到 column 0,避免每 column 只能存一个 metadata。
		Dictionary dev_meta;
		dev_meta["is_device"] = true;
		dev_meta["device_index"] = di;
		dev_item->set_metadata(0, dev_meta);

		int tag_count = (int)dev.tags.size();
		for (int ti = 0; ti < tag_count; ti++) {
			const auto &tag = dev.tags[ti];
			TreeItem *tag_item = tree->create_item(dev_item);
			tag_item->set_text(0, dev.name);
			tag_item->set_text(1, tag.address);
			tag_item->set_text(2, tag.name);
			tag_item->set_text(3, industrial_get_data_type_name(tag.data_type));
			tag_item->set_text(4, tag.writable ? TTR("Yes") : TTR("No"));
			Dictionary tag_meta;
			tag_meta["is_device"] = false;
			tag_meta["device_index"] = di;
			tag_meta["tag_index"] = ti;
			tag_item->set_metadata(0, tag_meta);
		}

		// Godot 4 用 set_collapsed(false) 替代旧 set_expanded(true)。
		dev_item->set_collapsed(false);
	}
}

void IndustrialTagDock::_on_search_text_changed(const String &p_text) {
	// TODO: Implement client-side filter.
}

void IndustrialTagDock::_on_filter_changed(int p_idx) {
	// TODO: Re-populate with filter.
}

void IndustrialTagDock::edit_selected() {
	// TODO: Implement edit tag dialog.
}

void IndustrialTagDock::delete_selected() {
	// TODO: Implement delete tag.
}

void IndustrialTagDock::export_csv() {
	// TODO: Implement CSV export.
}

void IndustrialTagDock::focus_new_tag() {
	// TODO: Show new tag dialog.
}
