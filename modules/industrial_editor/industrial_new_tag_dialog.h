#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/spin_box.h"
#include "core/object/ref_counted.h"

class IndustrialProject;

// Dialog for creating or editing a single tag.
class IndustrialNewTagDialog : public AcceptDialog {
	GDCLASS(IndustrialNewTagDialog, AcceptDialog);

public:
	IndustrialNewTagDialog();
	~IndustrialNewTagDialog() override;

	void set_project(Ref<IndustrialProject> p_project);
	void set_device_index(int p_device_index);
	void edit_tag(int p_tag_index);

private:
	Ref<IndustrialProject> project;
	int device_index = -1;
	int tag_index = -1; // -1 for new, >=0 for edit

	LineEdit *tag_name = nullptr;
	LineEdit *tag_address = nullptr;   // 兼容保留（最终拼接 "ATid.address_number"），用于提交
	OptionButton *tag_address_mode = nullptr;   // "字 / 位" (address_modes_ui id=word|bit)
	OptionButton *tag_address_type = nullptr;   // 区域码下拉（S7=IB/IW... / Modbus=0x_Coil...）
	LineEdit *tag_address_number = nullptr;     // 地址号 / DB号·偏移 / Symbolic 时 PLC 标签名
	Label *tag_address_type_label = nullptr;    // 左上 "Address Type:"，symbolic 时改为 "Tag Symbol Name:"
	Label *tag_address_number_label = nullptr;  // 左下 "Address No."，symbolic 模式下隐藏整行

	OptionButton *tag_type = nullptr;
	CheckButton *tag_writable = nullptr;
	SpinBox *tag_scale = nullptr;
	LineEdit *tag_unit = nullptr;

	PackedStringArray address_type_ids;   // 跟 tag_address_type 的项一一对应，存 AT.id

	void _build_ui();
	void _on_confirm();
	void _on_device_index_changed(int p_idx);
	void _on_tagfield_fetched_for_current(bool p_success);  // HTTP async refresh

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
