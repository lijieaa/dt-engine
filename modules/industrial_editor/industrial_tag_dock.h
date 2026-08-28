#pragma once

#include "editor/docks/editor_dock.h"
#include "scene/gui/tree.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "core/object/ref_counted.h"

class IndustrialProject;

// Bottom-side dock for global tag browsing.
class IndustrialTagDock : public EditorDock {
	GDCLASS(IndustrialTagDock, EditorDock);

public:
	IndustrialTagDock();
	~IndustrialTagDock() override;

	void set_project(Ref<IndustrialProject> p_project);
	void refresh();

	void edit_selected();
	void delete_selected();
	void export_csv();
	void focus_new_tag();

private:
	Ref<IndustrialProject> project;
	Tree *tree = nullptr;
	LineEdit *search = nullptr;
	OptionButton *filter_device = nullptr;
	OptionButton *filter_type = nullptr;

	void _build_ui();
	void _populate_tree();
	void _on_search_text_changed(const String &p_text);
	void _on_filter_changed(int p_idx);

protected:
	void _notification(int p_what);
	static void _bind_methods();
};
