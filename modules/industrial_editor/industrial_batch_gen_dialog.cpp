#include "industrial_batch_gen_dialog.h"
#include "industrial_project.h"
#include "industrial_driver_schema.h"

#include "editor/editor_string_names.h"
#include "core/object/callable_mp.h"
#include "scene/gui/box_container.h"
#include "scene/gui/separator.h"

IndustrialBatchGenDialog::IndustrialBatchGenDialog() {
	_build_ui();
}

IndustrialBatchGenDialog::~IndustrialBatchGenDialog() {}

void IndustrialBatchGenDialog::_bind_methods() {}

void IndustrialBatchGenDialog::_notification(int p_what) {}

void IndustrialBatchGenDialog::_build_ui() {
	set_title(TTRC("Batch Generate Tags"));
	set_min_size(Size2(520, 420));

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(main);

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Base Address:")));
		lbl->set_custom_minimum_size(Size2(130, 0));
		row->add_child(lbl);
		base_address = memnew(LineEdit);
		base_address->set_text("DB0.DBW0");
		base_address->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		base_address->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialBatchGenDialog::_refresh_preview));
		row->add_child(base_address);
		main->add_child(row);
	}

	{
		HBoxContainer *row = memnew(HBoxContainer);
		row->add_theme_constant_override("separation", 14);

		Label *lbl_cnt = memnew(Label(TTRC("Count:")));
		lbl_cnt->set_custom_minimum_size(Size2(130, 0));
		row->add_child(lbl_cnt);
		count = memnew(SpinBox);
		count->set_min(1);
		count->set_max(1000);
		count->set_value(10);
		count->set_custom_minimum_size(Size2(90, 24));
		count->connect(SceneStringName(value_changed), callable_mp(this, &IndustrialBatchGenDialog::_refresh_preview));
		row->add_child(count);

		Label *lbl_step = memnew(Label(TTRC("Step:")));
		lbl_step->set_custom_minimum_size(Size2(60, 0));
		row->add_child(lbl_step);
		step = memnew(SpinBox);
		step->set_min(1);
		step->set_max(1000);
		step->set_value(2);
		step->set_custom_minimum_size(Size2(90, 24));
		step->connect(SceneStringName(value_changed), callable_mp(this, &IndustrialBatchGenDialog::_refresh_preview));
		row->add_child(step);

		main->add_child(row);
	}

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Name Pattern:")));
		lbl->set_custom_minimum_size(Size2(130, 0));
		row->add_child(lbl);
		name_pattern = memnew(LineEdit);
		name_pattern->set_text("Tag_@INDEX@");
		name_pattern->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		name_pattern->set_tooltip_text(TTRC("Use @INDEX@ as 0-based index placeholder."));
		name_pattern->connect(SceneStringName(text_changed), callable_mp(this, &IndustrialBatchGenDialog::_refresh_preview));
		row->add_child(name_pattern);
		main->add_child(row);
	}

	{
		HBoxContainer *row = memnew(HBoxContainer);
		Label *lbl = memnew(Label(TTRC("Data Type:")));
		lbl->set_custom_minimum_size(Size2(130, 0));
		row->add_child(lbl);
		data_type = memnew(OptionButton);
		StringList types = industrial_get_data_type_names();
		for (int i = 0; i < types.size(); i++) {
			data_type->add_item(types[i]);
		}
		data_type->select(TYPE_INT16);
		data_type->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		data_type->connect(SceneStringName(item_selected), callable_mp(this, &IndustrialBatchGenDialog::_refresh_preview));
		row->add_child(data_type);
		main->add_child(row);
	}

	{
		HBoxContainer *row = memnew(HBoxContainer);
		writable = memnew(CheckButton);
		writable->set_text(TTRC("Writable"));
		writable->set_pressed(true);
		row->add_child(writable);
		main->add_child(row);
	}

	main->add_child(memnew(HSeparator));

	preview_label = memnew(Label);
	preview_label->set_text(TTRC("Preview:"));
	main->add_child(preview_label);

	preview_container = memnew(VBoxContainer);
	preview_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preview_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main->add_child(preview_container);

	{
		HBoxContainer *actions = memnew(HBoxContainer);
		actions->add_spacer();
		btn_generate = memnew(Button);
		btn_generate->set_text(TTRC("Generate"));
		btn_generate->connect(SceneStringName(pressed), callable_mp(this, &IndustrialBatchGenDialog::_on_generate_pressed));
		actions->add_child(btn_generate);
		add_child(actions);
	}

	get_ok_button()->hide();
	_refresh_preview();
}

void IndustrialBatchGenDialog::set_project(Ref<IndustrialProject> p_project) {
	project = p_project;
}

void IndustrialBatchGenDialog::set_device_index(int p_device_index) {
	device_index = p_device_index;
}

void IndustrialBatchGenDialog::_refresh_preview() {
	if (!preview_container) return;

	while (preview_container->get_child_count() > 0) {
		preview_container->remove_child(preview_container->get_child(0));
	}

	String base = base_address ? base_address->get_text() : "";
	int cnt = count ? (int)count->get_value() : 1;
	int stp = step ? (int)step->get_value() : 1;
	String pattern = name_pattern ? name_pattern->get_text() : "Tag_@INDEX@";

	if (base.is_empty()) {
		preview_label->set_text(TTRC("Preview: (enter base address)"));
		return;
	}

	preview_label->set_text(vformat(TTRC("Preview (%d tags):"), cnt));

	int show = MIN(cnt, 10);
	for (int i = 0; i < show; i++) {
		int addr_idx = i * stp;
		String addr = base;
		if (addr_idx > 0) {
			addr += " + " + itos(addr_idx);
		}
		String tag_name = pattern.replace("@INDEX@", itos(i));

		Label *lbl = memnew(Label);
		lbl->set_text(vformat("  %s  %s  %s",
					addr,
					tag_name,
					industrial_get_data_type_name(data_type ? data_type->get_selected() : 0)));
		preview_container->add_child(lbl);
	}

	if (cnt > show) {
		Label *more = memnew(Label);
		more->set_text(vformat("  ... (%d more)", cnt - show));
		preview_container->add_child(more);
	}
}

void IndustrialBatchGenDialog::_generate_preview() {
	_refresh_preview();
}

void IndustrialBatchGenDialog::_on_generate_pressed() {
	if (project.is_null() || device_index < 0) {
		hide();
		return;
	}

	String base = base_address ? base_address->get_text() : "";
	int cnt = count ? (int)count->get_value() : 1;
	int stp = step ? (int)step->get_value() : 1;
	String pattern = name_pattern ? name_pattern->get_text() : "Tag_@INDEX@";
	int dtype = data_type ? data_type->get_selected() : 0;
	bool write = writable ? writable->is_pressed() : false;

	generated_tags.clear();

	for (int i = 0; i < cnt; i++) {
		int addr_idx = i * stp;
		String addr = base;
		if (addr_idx > 0) {
			addr += " + " + itos(addr_idx);
		}

		String tag_name = pattern.replace("@INDEX@", itos(i));

		IndustrialTagData tag;
		tag.name = tag_name;
		tag.address = addr;
		tag.data_type = dtype;
		tag.writable = write;
		tag.scale = 1.0;
		tag.unit = "";
		tag.scan_group = "";

		project->add_tag(device_index, tag);
		generated_tags.append(tag_name);
	}

	hide();
}
