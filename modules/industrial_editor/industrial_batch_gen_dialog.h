#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/button.h"
#include "core/object/ref_counted.h"
#include "core/variant/array.h"

class IndustrialProject;

// Dialog for batch-generating tags from an address range.
class IndustrialBatchGenDialog : public AcceptDialog {
	GDCLASS(IndustrialBatchGenDialog, AcceptDialog);

public:
	IndustrialBatchGenDialog();
	~IndustrialBatchGenDialog() override;

	void set_project(Ref<IndustrialProject> p_project);
	void set_device_index(int p_device_index);

	// Returns generated tags.
	Array get_generated_tags() const { return generated_tags; }

private:
	Ref<IndustrialProject> project;
	int device_index = -1;

	LineEdit *base_address = nullptr;
	SpinBox *count = nullptr;
	SpinBox *step = nullptr;
	LineEdit *name_pattern = nullptr; // e.g. "Tag_@INDEX@"
	OptionButton *data_type = nullptr;
	CheckButton *writable = nullptr;
	Label *preview_label = nullptr;
	VBoxContainer *preview_container = nullptr;
	Button *btn_generate = nullptr;

	Array generated_tags;

	void _build_ui();
	void _generate_preview();
	void _on_generate_pressed();
	void _refresh_preview();

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
