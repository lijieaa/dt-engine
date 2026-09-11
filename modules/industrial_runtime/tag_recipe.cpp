#include "tag_recipe.h"

#include "tag_widget_util.h"
#include "widget_recipe.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "scene/gui/option_button.h"
#include "scene/gui/box_container.h"
#include "scene/main/node.h"
#include "scene/scene_string_names.h"

TagRecipe::TagRecipe() {
	recipe_core.instantiate();
}

TagRecipe::~TagRecipe() {}

void TagRecipe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_recipe_cfg", "cfg"), &TagRecipe::set_recipe_cfg);
	ClassDB::bind_method(D_METHOD("get_recipe_cfg"), &TagRecipe::get_recipe_cfg);
	ClassDB::bind_method(D_METHOD("set_entry_hint", "name"), &TagRecipe::set_entry_hint);
	ClassDB::bind_method(D_METHOD("get_entry_hint"), &TagRecipe::get_entry_hint);
	ClassDB::bind_method(D_METHOD("build_sequence", "entry_name"), &TagRecipe::build_sequence);
	ClassDB::bind_method(D_METHOD("apply_recipe", "entry_name"), &TagRecipe::apply_recipe);
	ClassDB::bind_method(D_METHOD("refresh_entries"), &TagRecipe::refresh_entries);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "recipe_cfg"), "set_recipe_cfg", "get_recipe_cfg");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "entry_hint"), "set_entry_hint", "get_entry_hint");
}

void TagRecipe::set_recipe_cfg(const Dictionary &p_cfg) {
	recipe_cfg = p_cfg;
	if (recipe_core.is_valid() && !recipe_cfg.is_empty()) {
		recipe_core->configure(recipe_cfg);
	}
	refresh_entries();
}

void TagRecipe::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (vb == nullptr) {
			vb = memnew(VBoxContainer);
			add_child(vb);
		}
		if (option_button == nullptr) {
			option_button = memnew(OptionButton);
			vb->add_child(option_button);
			option_button->connect(SceneStringName(item_selected), callable_mp(this, &TagRecipe::_on_selected));
		}
		if (!recipe_cfg.is_empty() && recipe_core.is_valid()) {
			recipe_core->configure(recipe_cfg);
		}
		refresh_entries();
	}
}

Array TagRecipe::build_sequence(const String &p_entry_name) const {
	if (recipe_core.is_null()) {
		return Array();
	}
	return recipe_core->build_write_sequence(p_entry_name);
}

void TagRecipe::apply_recipe(const String &p_entry_name) {
	const Array seq = build_sequence(p_entry_name);
	for (int i = 0; i < seq.size(); i++) {
		const Dictionary d = seq[i];
		if (!d.has("tag")) {
			continue;
		}
		const String tag = d["tag"].operator String();
		const Variant value = d.has("value") ? d["value"] : Variant();
		tag_widget::write_tag(this, tag, value, "recipe:" + p_entry_name);
	}
}

void TagRecipe::refresh_entries() {
	if (option_button == nullptr || recipe_core.is_null()) {
		return;
	}
	option_button->clear();
	const Array entries = recipe_core->list_entries();
	for (int i = 0; i < entries.size(); i++) {
		option_button->add_item(entries[i].operator String());
	}
	if (entries.size() > 0) {
		option_button->select(0);
	}
}

void TagRecipe::_on_selected(int idx) {
	if (recipe_core.is_null()) {
		return;
	}
	const Array entries = recipe_core->list_entries();
	if (idx >= 0 && idx < entries.size()) {
		apply_recipe(entries[idx].operator String());
	}
}