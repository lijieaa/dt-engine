#pragma once

#include "scene/gui/panel_container.h"

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class WidgetRecipe;
class OptionButton;
class VBoxContainer;

/// TagRecipe â€?recipe selector widget. Holds a C++ WidgetRecipe core,
/// populates an OptionButton from list_entries(), and applies the selected
/// entry via build_write_sequence() + tag_widget::write_tag().
/// C++ port of native Tag widget.
class TagRecipe : public PanelContainer {
	GDCLASS(TagRecipe, PanelContainer);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TagRecipe();
	~TagRecipe();

	void set_recipe_cfg(const Dictionary &p_cfg);
	Dictionary get_recipe_cfg() const { return recipe_cfg; }
	void set_entry_hint(const String &p_name) { entry_hint = p_name; }
	String get_entry_hint() const { return entry_hint; }

	/// Ordered write sequence for the named entry (Array of {tag,value,order}).
	Array build_sequence(const String &p_entry_name) const;

	/// Apply the recipe: build_write_sequence -> write_tag for each step.
	void apply_recipe(const String &p_entry_name);

	/// Rebuild the OptionButton from the core's entry list.
	void refresh_entries();

private:
	Dictionary recipe_cfg;
	String entry_hint = "R1";
	Ref<WidgetRecipe> recipe_core;
	OptionButton *option_button = nullptr;
	VBoxContainer *vb = nullptr;

	void _on_selected(int idx);
};